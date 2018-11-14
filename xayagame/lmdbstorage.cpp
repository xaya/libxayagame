// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "lmdbstorage.hpp"

#include <glog/logging.h>

namespace xaya
{

namespace
{

/** Single-character key for "current block hash".  */
constexpr char KEY_CURRENT_HASH = 'h';
/** Single-character key for "current game state".  */
constexpr char KEY_CURRENT_STATE = 's';
/**
 * Key prefix character for undo data (followed by hash bytes in big-endian
 * byte order as returned from uint256::GetBlob).
 */
constexpr char KEY_PREFIX_UNDO = 'u';
/**
 * Single-character key for the number of resizes made.  This number is not
 * really needed, but we do keep it around so that we commit actual writes
 * after each resize to make sure the new size is persisted.  The number is
 * stored as big-endian, using UNDO_HEIGHT_BYTES bytes.
 */
constexpr char KEY_NUM_RESIZES = 'r';

/**
 * Number of bytes that encode the height for stored undo data, preceding
 * the actual undo data in the database value.  These bytes encode the height
 * in big-endian order.
 */
constexpr size_t UNDO_HEIGHT_BYTES = 4;

} // anonymous namespace

LMDBStorage::LMDBStorage (const std::string& dir)
  : directory(dir)
{
  LOG (INFO)
      << "Using LMDB version " << mdb_version (nullptr, nullptr, nullptr);

  CheckOk (mdb_env_create (&env));
  CHECK (env != nullptr);
}

LMDBStorage::~LMDBStorage ()
{
  if (env != nullptr)
    {
      mdb_env_close (env);
      LOG (INFO) << "Closed LMDB environment";
    }

  /* This function should only be called when no transaction is open.
     Close the environment first just in case, so if the check fails, it
     is already "properly" closed.  That doesn't really matter in practice,
     but the check for being a nullptr can be done afterwards just fine.  */
  CHECK (startedTxn == nullptr);

  /* When needsResize is set, it should always be reset soon after by the
     following RollbackTransaction call.  It should never be left true until
     the object gets destructed, at the very least.  */
  CHECK (!needsResize);
}

void
LMDBStorage::CheckOk (const int code) const
{
  if (code == 0)
    return;

  /* If the map is full, throw a RetryWithNewTransaction exception.  We also
     set a flag that tells us to resize the map in the following
     RollbackTransaction call that will be made when the stack unwinds.  There
     we do the actual resizing, so that we are sure there is no currently
     open transaction.  */
  if (code == MDB_MAP_FULL)
    {
      LOG (WARNING) << "The LMDB map needs to be resized";
      CHECK (!needsResize)
          << "We got another MDB_MAP_FULL error while waiting for the resize";
      needsResize = true;
      throw StorageInterface::RetryWithNewTransaction ("LMDB needs resize");
    }

  LOG (FATAL) << "LMDB error: " << mdb_strerror (code);
}

void
LMDBStorage::Initialise ()
{
  LOG (INFO) << "Opening LMDB database at " << directory;
  CheckOk (mdb_env_open (env, directory.c_str (), 0, 0644));

  MDB_envinfo stat;
  CheckOk (mdb_env_info (env, &stat));
  LOG (INFO)
      << "LMDB has currently a map size of "
      << (stat.me_mapsize >> 20) << " MiB";
}

void
LMDBStorage::Clear ()
{
  CHECK (startedTxn == nullptr);
  LOG (INFO) << "Emptying the entire LMDB database to clear the storage";

  BeginTransaction ();
  try
    {
      CheckOk (mdb_drop (startedTxn, dbi, 0));
      CommitTransaction ();
    }
  catch (...)
    {
      RollbackTransaction ();
      throw;
    }

  CHECK (startedTxn == nullptr);
}

namespace
{

/**
 * Sets an MDB_val to point to a single-byte value with the given char.
 *
 * Note that the byte is passed *by reference*, and make sure that the
 *  reference remains valid as long as data is needed after the call!
 */
void
SingleByteValue (const char& byte, MDB_val& data)
{
  data.mv_size = 1;
  data.mv_data = const_cast<void*> (static_cast<const void*> (&byte));
}

/**
 * Sets an MDB_val to the bytes contained in a std::string.
 */
void
StringToValue (const std::string& str, MDB_val& data)
{
  data.mv_size = str.size ();
  data.mv_data = const_cast<void*> (static_cast<const void*> (&str[0]));
}

/**
 * Returns the key at which undo data is stored as string.
 */
std::string
KeyForUndoData (const uint256& hash)
{
  std::string key(1, KEY_PREFIX_UNDO);
  key.insert (1, reinterpret_cast<const char*> (hash.GetBlob ()),
              uint256::NUM_BYTES);

  return key;
}

/**
 * Retrieves the data from a MDB_val as std::string.  Strips a given
 * number of bytes from the start, which is used for undo data.
 */
std::string
ValueToString (const MDB_val& data, const size_t stripBytes)
{
  CHECK (data.mv_size >= stripBytes);
  const char* bytes = static_cast<const char*> (data.mv_data);

  return std::string (bytes + stripBytes, data.mv_size - stripBytes);
}

/**
 * Encodes a given number as big-endian bytes.
 */
void
EncodeUnsigned (unsigned num, unsigned char* bytes)
{
  for (size_t i = 0; i < UNDO_HEIGHT_BYTES; ++i)
    {
      bytes[UNDO_HEIGHT_BYTES - i - 1] = (num & 0xFF);
      num >>= 8;
    }
  CHECK_EQ (num, 0);
}

/**
 * Decodes big-endian bytes as unsigned integer.
 */
unsigned
DecodeUnsigned (const unsigned char* bytes)
{
  unsigned num = 0;
  for (size_t i = 0; i < UNDO_HEIGHT_BYTES; ++i)
    {
      num <<= 8;
      num |= bytes[i];
    }
  return num;
}

} // anonymous namespace

/**
 * Utility class that manages a read-only transaction using RAII mechanics.
 */
class LMDBStorage::ReadTransaction
{

private:

  /**
   * LMDBStorage instance this is part of.  This is used to call CheckOk
   * on that instance.
   */
  const LMDBStorage& storage;

  /** The underlying LMDB transaction handle.  */
  MDB_txn* txn = nullptr;

  /**
   * Set to true if we "own" txn and have to abort it in the destructor.  This
   * is the case if no active parentTxn was passed to the constructor, so that
   * we created a fresh read-only tx.
   */
  bool ownTx;

  /** The opened database identifier.  */
  MDB_dbi dbi;

public:

  /**
   * Constructs a read transaction for the given LMDBStorage.  If the instance
   * has a currently open transaction, then that one is used for reading to
   * ensure that already-modified state is seen.
   */
  explicit ReadTransaction (const LMDBStorage& s)
    : storage(s)
  {
    if (storage.startedTxn == nullptr)
      {
        ownTx = true;
        VLOG (1) << "Starting a new read-only LMDB transaction";
        storage.CheckOk (mdb_txn_begin (storage.env, nullptr,
                                        MDB_RDONLY, &txn));
      }
    else
      {
        ownTx = false;
        VLOG (1) << "Reusing the parent transaction for reading";
        txn = storage.startedTxn;
      }

    CHECK (txn != nullptr);
    storage.CheckOk (mdb_dbi_open (txn, nullptr, 0, &dbi));
  }

  ~ReadTransaction ()
  {
    if (ownTx)
      {
        VLOG (1) << "Aborting the read-only LMDB transaction";
        CHECK (txn != nullptr);
        mdb_txn_abort (txn);
      }
  }

  /**
   * Reads data for the given key.  Returns false if the key is not found.
   */
  bool
  ReadData (const MDB_val& key, MDB_val& data) const
  {
    CHECK (txn != nullptr);

    const int code = mdb_get (txn, dbi, const_cast<MDB_val*> (&key), &data);
    if (code == 0)
      return true;
    if (code == MDB_NOTFOUND)
      return false;

    storage.CheckOk (code);
    LOG (FATAL) << "CheckOk should have failed with code " << code;
  }

};

bool
LMDBStorage::GetCurrentBlockHash (uint256& hash) const
{
  ReadTransaction tx(*this);

  MDB_val key;
  SingleByteValue (KEY_CURRENT_HASH, key);

  MDB_val data;
  if (!tx.ReadData (key, data))
    return false;

  CHECK_EQ (data.mv_size, uint256::NUM_BYTES)
      << "Invalid data for current block hash in LMDB";
  hash.FromBlob (static_cast<const unsigned char*> (data.mv_data));

  return true;
}

GameStateData
LMDBStorage::GetCurrentGameState () const
{
  ReadTransaction tx(*this);

  MDB_val key;
  SingleByteValue (KEY_CURRENT_STATE, key);

  MDB_val data;
  CHECK (tx.ReadData (key, data));

  return ValueToString (data, 0);
}

void
LMDBStorage::SetCurrentGameState (const uint256& hash,
                                  const GameStateData& state)
{
  CHECK (startedTxn != nullptr);

  /* Store the current block hash.  */
  MDB_val key;
  SingleByteValue (KEY_CURRENT_HASH, key);

  MDB_val data;
  data.mv_size = uint256::NUM_BYTES;
  data.mv_data = const_cast<void*> (static_cast<const void*> (hash.GetBlob ()));

  CheckOk (mdb_put (startedTxn, dbi, &key, &data, 0));

  /* Store the current game state.  */
  SingleByteValue (KEY_CURRENT_STATE, key);
  StringToValue (state, data);
  CheckOk (mdb_put (startedTxn, dbi, &key, &data, 0));
}

bool
LMDBStorage::GetUndoData (const uint256& hash, UndoData& undo) const
{
  ReadTransaction tx(*this);

  MDB_val key;
  const std::string strKey = KeyForUndoData (hash);
  StringToValue (strKey, key);

  MDB_val data;
  if (!tx.ReadData (key, data))
    return false;

  undo = ValueToString (data, UNDO_HEIGHT_BYTES);
  return true;
}

void
LMDBStorage::AddUndoData (const uint256& hash,
                          const unsigned height, const UndoData& undo)
{
  CHECK (startedTxn != nullptr);

  MDB_val key;
  const std::string strKey = KeyForUndoData (hash);
  StringToValue (strKey, key);

  MDB_val data;
  data.mv_size = UNDO_HEIGHT_BYTES + undo.size ();
  data.mv_data = nullptr;
  CheckOk (mdb_put (startedTxn, dbi, &key, &data, MDB_RESERVE));

  CHECK (data.mv_data != nullptr);
  unsigned char* bytes = static_cast<unsigned char*> (data.mv_data);
  std::copy (undo.begin (), undo.end (), bytes + UNDO_HEIGHT_BYTES);
  EncodeUnsigned (height, bytes);
}

void
LMDBStorage::ReleaseUndoData (const uint256& hash)
{
  CHECK (startedTxn != nullptr);

  MDB_val key;
  const std::string strKey = KeyForUndoData (hash);
  StringToValue (strKey, key);

  const int code = mdb_del (startedTxn, dbi, &key, nullptr);
  if (code == 0)
    return;
  if (code == MDB_NOTFOUND)
    {
      LOG (WARNING)
          << "Attempted to delete non-existant undo data for hash "
          << hash.ToHex ();
      return;
    }
  CheckOk (code);
}

/**
 * Utility class that manages an LMDB cursor using RAII.
 */
class LMDBStorage::Cursor
{

private:

  /** The underlying LMDBStorage instance.  */
  LMDBStorage& storage;

  /** The underlying cursor handle.  */
  MDB_cursor* cursor = nullptr;

public:

  explicit Cursor (LMDBStorage& s, MDB_txn* txn, MDB_dbi dbi)
    : storage(s)
  {
    CHECK (txn != nullptr);
    storage.CheckOk (mdb_cursor_open (txn, dbi, &cursor));
    CHECK (cursor != nullptr);
  }

  ~Cursor ()
  {
    CHECK (cursor != nullptr);
    mdb_cursor_close (cursor);
  }

  /**
   * Positions the cursor at the given key (or the next larger one).  The key
   * is updated to the actual key looked up, as well as the data.  Returns
   * false if no larger element can be found.
   */
  bool
  Seek (MDB_val& key, MDB_val& data)
  {
    CHECK (cursor != nullptr);
    const int code = mdb_cursor_get (cursor, &key, &data, MDB_SET_RANGE);
    if (code == 0)
      return true;
    if (code == MDB_NOTFOUND)
      return false;

    storage.CheckOk (code);
    LOG (FATAL) << "CheckOk should have failed with code " << code;
  }

  /**
   * Steps to the next element and returns its key and data.  Returns
   * false if there is none.
   */
  bool
  Next (MDB_val& key, MDB_val& data)
  {
    CHECK (cursor != nullptr);
    const int code = mdb_cursor_get (cursor, &key, &data, MDB_NEXT);
    if (code == 0)
      return true;
    if (code == MDB_NOTFOUND)
      return false;

    storage.CheckOk (code);
    LOG (FATAL) << "CheckOk should have failed with code " << code;
  }


  /**
   * Deletes the element the cursor currently points to.
   */
  void
  Delete ()
  {
    CHECK (cursor != nullptr);
    storage.CheckOk (mdb_cursor_del (cursor, 0));
  }

};

void
LMDBStorage::PruneUndoData (unsigned height)
{
  Cursor cursor(*this, startedTxn, dbi);

  MDB_val key;
  SingleByteValue (KEY_PREFIX_UNDO, key);

  MDB_val data;
  bool hasNext = cursor.Seek (key, data);
  while (hasNext)
    {
      const char* keyData = static_cast<const char*> (key.mv_data);
      if (key.mv_size < 1 || keyData[0] != KEY_PREFIX_UNDO)
        break;

      CHECK (data.mv_size >= UNDO_HEIGHT_BYTES)
          << "Invalid data stored in LMDB database for undo entry";
      const unsigned char* bytes
          = static_cast<const unsigned char*> (data.mv_data);
      const unsigned h = DecodeUnsigned (bytes);

      if (h <= height)
        {
          VLOG (1) << "Found undo entry for height " << h << ", pruning";
          cursor.Delete ();
        }

      hasNext = cursor.Next (key, data);
    }
}

void
LMDBStorage::BeginTransaction ()
{
  CHECK (!needsResize);
  CHECK (startedTxn == nullptr);

  VLOG (1) << "Starting a new LMDB transaction";
  CheckOk (mdb_txn_begin (env, nullptr, 0, &startedTxn));
  CHECK (startedTxn != nullptr);

  VLOG (1) << "Opening the unnamed database";
  CheckOk (mdb_dbi_open (startedTxn, nullptr, 0, &dbi));
}

void
LMDBStorage::CommitTransaction ()
{
  CHECK (!needsResize);
  CHECK (startedTxn != nullptr);
  VLOG (1) << "Committing the current LMDB transaction";

  try
    {
      CheckOk (mdb_txn_commit (startedTxn));
      startedTxn = nullptr;
    }
  catch (...)
    {
      /* Even if mdb_txn_commit fails, the txn data is freed.  Thus we have
         to make sure that it is set to null anyway, and that it won't be
         passed to mdb_txn_abort anymore by cleanup handling.  */
      LOG (WARNING) << "mdb_txn_commit failed, setting txn handle to null";
      startedTxn = nullptr;
      throw;
    }
}

void
LMDBStorage::RollbackTransaction ()
{
  /* If mdb_txn_commit failed, we may end up in a situation in which the
     txn handle is already freed and set to null, but the cleanup handling
     still calls RollbackTransaction.  In that case, simply ignore the request
     and return now without doing anything.  */
  if (startedTxn == nullptr)
    LOG (WARNING)
        << "RollbackTransaction called without a currently-active"
           " transaction.  This is ok if mdb_txn_commit just failed.";
  else
    {
      VLOG (1) << "Aborting the current LMDB transaction";
      mdb_txn_abort (startedTxn);
      startedTxn = nullptr;
    }
  CHECK (startedTxn == nullptr);

  if (needsResize)
    {
      needsResize = false;
      Resize ();
    }

  CHECK (startedTxn == nullptr);
  CHECK (!needsResize);
}

void
LMDBStorage::Resize ()
{
  CHECK (startedTxn == nullptr);

  MDB_envinfo stat;
  CheckOk (mdb_env_info (env, &stat));
  const size_t newSize = (stat.me_mapsize << 1);

  LOG (INFO)
      << "Resizing LMDB map from " << (stat.me_mapsize >> 20) << " MiB to "
      << (newSize >> 20) << " MiB";
  needsResize = false;

  mdb_dbi_close (env, dbi);
  CheckOk (mdb_env_set_mapsize (env, newSize));

  CheckOk (mdb_env_info (env, &stat));
  LOG (INFO) << "New size: " << stat.me_mapsize;

  /* The LMDB map size is only persisted in the environment once a write
     transaction has been committed.  To satisfy this requirement immediately,
     we keep a counter of how many resizes have been made in the database.
     Increment that now.  */
  BeginTransaction ();
  try
    {
      MDB_val key;
      SingleByteValue (KEY_NUM_RESIZES, key);

      unsigned numResizes = 0;
      {
        ReadTransaction tx(*this);

        MDB_val data;
        if (tx.ReadData (key, data))
          {
            CHECK_EQ (data.mv_size, UNDO_HEIGHT_BYTES);
            const unsigned char* bytes
                = static_cast<const unsigned char*> (data.mv_data);
            numResizes = DecodeUnsigned (bytes);
          }
      }
      ++numResizes;
      LOG (INFO) << "This is resize number " << numResizes;

      MDB_val data;
      data.mv_size = UNDO_HEIGHT_BYTES;
      data.mv_data = nullptr;
      CheckOk (mdb_put (startedTxn, dbi, &key, &data, MDB_RESERVE));

      CHECK (data.mv_data != nullptr);
      unsigned char* bytes = static_cast<unsigned char*> (data.mv_data);
      EncodeUnsigned (numResizes, bytes);

      CommitTransaction ();
    }
  catch (...)
    {
      RollbackTransaction ();
      throw;
    }
}

} // namespace xaya
