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
 * Number of bytes that encode the height for stored undo data, preceding
 * the actual undo data in the database value.  These bytes encode the height
 * in big-endian order.
 */
constexpr size_t UNDO_HEIGHT_BYTES = 4;

/**
 * Checks that the error code is zero.  If it is not, LOG(FATAL)'s with the
 * LMDB translation of the error code to a string.
 */
void
CheckOk (const int code)
{
  if (code == 0)
    return;

  LOG (FATAL) << "LMDB error: " << mdb_strerror (code);
}

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
  mdb_env_close (env);

  /* This function should only be called when no transaction is open.
     Close the environment first just in case, so if the check fails, it
     is already "properly" closed.  That doesn't really matter in practice,
     but the check for being a nullptr can be done afterwards just fine.  */
  CHECK (startedTxn == nullptr);
}

void
LMDBStorage::Initialise ()
{
  LOG (INFO) << "Opening LMDB database at " << directory;
  CheckOk (mdb_env_open (env, directory.c_str (), 0, 0644));
}

void
LMDBStorage::Clear ()
{
  CHECK (startedTxn == nullptr);
  BeginTransaction ();

  LOG (INFO) << "Emptying the entire LMDB database to clear the storage";
  CheckOk (mdb_drop (startedTxn, dbi, 0));

  CommitTransaction ();
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
 * Utility class that manages a read-only transaction using RAII mechanics.
 */
class ReadTransaction
{

private:

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
   * Constructs a read transaction in the given environment.  The passed in
   * transaction is used as parent if not null; this ensures that state changed
   * in it is seen already by the reader.
   */
  explicit ReadTransaction (MDB_env* env, MDB_txn* parentTxn)
  {
    if (parentTxn == nullptr)
      {
        ownTx = true;
        VLOG (1) << "Starting a new read-only LMDB transaction";
        CheckOk (mdb_txn_begin (env, parentTxn, MDB_RDONLY, &txn));
      }
    else
      {
        ownTx = false;
        VLOG (1) << "Reusing the parent transaction for reading";
        txn = parentTxn;
      }

    CHECK (txn != nullptr);
    CheckOk (mdb_dbi_open (txn, nullptr, 0, &dbi));
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
    LOG (FATAL) << "LMDB error while reading: " << mdb_strerror (code);
  }

};

} // anonymous namespace

bool
LMDBStorage::GetCurrentBlockHash (uint256& hash) const
{
  ReadTransaction tx(env, startedTxn);

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
  ReadTransaction tx(env, startedTxn);

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
  ReadTransaction tx(env, startedTxn);

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
  char* bytes = static_cast<char*> (data.mv_data);
  std::copy (undo.begin (), undo.end (), bytes + UNDO_HEIGHT_BYTES);

  unsigned h = height;
  for (size_t i = 0; i < UNDO_HEIGHT_BYTES; ++i)
    {
      bytes[UNDO_HEIGHT_BYTES - i - 1] = (h & 0xFF);
      h >>= 8;
    }
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
    LOG (WARNING)
        << "Attempted to delete non-existant undo data for hash "
        << hash.ToHex ();
  else
    LOG (FATAL) << "LMDB error deleting undo data: " << mdb_strerror (code);
}

namespace
{

/**
 * Utility class that manages an LMDB cursor using RAII.
 */
class LMDBCursor
{

private:

  /** The underlying cursor handle.  */
  MDB_cursor* cursor = nullptr;

public:

  explicit LMDBCursor (MDB_txn* txn, MDB_dbi dbi)
  {
    CHECK (txn != nullptr);
    CheckOk (mdb_cursor_open (txn, dbi, &cursor));
    CHECK (cursor != nullptr);
  }

  ~LMDBCursor ()
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

    LOG (FATAL) << "LMDB error with cursor seek: " << mdb_strerror (code);
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

    LOG (FATAL) << "LMDB error with cursor next: " << mdb_strerror (code);
  }


  /**
   * Deletes the element the cursor currently points to.
   */
  void
  Delete ()
  {
    CHECK (cursor != nullptr);
    CheckOk (mdb_cursor_del (cursor, 0));
  }

};

} // anonymous namespace

void
LMDBStorage::PruneUndoData (unsigned height)
{
  LMDBCursor cursor(startedTxn, dbi);

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
      unsigned h = 0;
      for (size_t i = 0; i < UNDO_HEIGHT_BYTES; ++i)
        {
          h <<= 8;
          h |= bytes[i];
        }

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
  CHECK (startedTxn != nullptr);
  VLOG (1) << "Committing the current LMDB transaction";
  CheckOk (mdb_txn_commit (startedTxn));
  startedTxn = nullptr;
}

void
LMDBStorage::RollbackTransaction ()
{
  CHECK (startedTxn != nullptr);
  VLOG (1) << "Aborting the current LMDB transaction";
  mdb_txn_abort (startedTxn);
  startedTxn = nullptr;
}

} // namespace xaya
