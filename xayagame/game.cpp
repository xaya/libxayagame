// Copyright (C) 2018-2026 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "game.hpp"

#include "perftimer.hpp"

#include <jsonrpccpp/common/errors.h>
#include <jsonrpccpp/common/exception.h>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <sstream>
#include <thread>

/**
 * Timeout for WaitForChange (i.e. return after this time even if there
 * has not been any change).  Having a timeout in the first place avoids
 * collecting more and more blocked threads in the worst case.
 */
DEFINE_int32 (xaya_waitforchange_timeout_ms, 5'000,
              "timeout for waitforchange calls");

/**
 * The maximum accepted staleness of ZMQ.  If no block updates have been
 * received in that time frame, we assume the connection is broken, and
 * try to reconnect.
 */
DEFINE_int32 (xaya_zmq_staleness_ms, 120'000,
              "ZeroMQ staleness before reconnection attempt");

/**
 * If set to non-zero, this indicates the interval (in milliseconds)
 * at which a running Game should probe its connection to Xaya Core.
 */
DEFINE_int32 (xaya_connection_check_ms, 0,
              "if non-zero, interval between connection checks");

/**
 * If set to true, crash (CHECK-fail) when a block detach happens beyond
 * pruning depth instead of resetting and syncing from scratch.
 */
DEFINE_bool (xaya_crash_without_undo, false,
             "if true, crash instead of syncing from scratch for a reorg"
             " beyond pruning depth");

namespace xaya
{

/* ************************************************************************** */

/**
 * Helper class that runs a background thread.  At regular intervals,
 * it calls Game::ProbeAndFixConnection.
 */
class Game::ConnectionCheckerThread
{

private:

  /** The Game instance to call the check on.  */
  Game& game;

  /** The interval for checks.  */
  const std::chrono::milliseconds intv;

  /** Mutex for this instance.  */
  std::mutex mut;

  /** Condition variable to wait on / signal a stop request.  */
  std::condition_variable cv;

  /** The actual thread running.  */
  std::unique_ptr<std::thread> runner;

  /** Set to true if the thread should stop.  */
  bool shouldStop;

  /**
   * Runs the thread's main loop.
   */
  void
  Run ()
  {
    std::unique_lock<std::mutex> lock(mut);
    while (!shouldStop)
      {
        cv.wait_for (lock, intv);
        game.ProbeAndFixConnection ();
      }
  }

public:

  explicit ConnectionCheckerThread (Game& g)
    : game(g), intv(FLAGS_xaya_connection_check_ms), shouldStop(false)
  {
    runner = std::make_unique<std::thread> ([this] () { Run (); });
  }

  ~ConnectionCheckerThread ()
  {
    {
      std::lock_guard<std::mutex> lock(mut);
      shouldStop = true;
      cv.notify_all ();
    }

    runner->join ();
    runner.reset ();
  }

};

/* ************************************************************************** */

Game::Game (const std::string& id)
  : gameId(id), state(State::DISCONNECTED), genesisHeight(-1)
{
  transactionManager.SetCoprocessor (coproc);
  targetBlock.SetNull ();
  genesisHash.SetNull ();
  zmq.AddListener (gameId, this);
}

Game::~Game () = default;

std::string
Game::StateToString (const State s)
{
  switch (s)
    {
    case State::UNKNOWN:
      return "unknown";
    case State::PREGENESIS:
      return "pregenesis";
    case State::OUT_OF_SYNC:
      return "out-of-sync";
    case State::CATCHING_UP:
      return "catching-up";
    case State::AT_TARGET:
      return "at-target";
    case State::UP_TO_DATE:
      return "up-to-date";
    case State::DISCONNECTED:
      return "disconnected";
    }

  std::ostringstream out;
  out << "invalid-" << static_cast<int> (s);
  return out.str ();
}

bool
Game::UpdateStateForAttach (const uint256& parent, const uint256& hash,
                            const Json::Value& blockData)
{
  uint256 currentHash;
  CHECK (storage->GetCurrentBlockHash (currentHash));
  if (currentHash != parent)
    {
      LOG (WARNING)
          << "Game state hash " << currentHash.ToHex ()
          << " does not match attached block's parent " << parent.ToHex ();
      return false;
    }

  const GameStateData oldState = storage->GetCurrentGameState ();

  CHECK (blockData.isObject ());
  const auto& blockHeader = blockData["block"];
  CHECK (blockHeader.isObject ());
  const unsigned height = blockHeader["height"].asUInt ();

  {
    internal::ActiveTransaction tx(transactionManager);

    CoprocessorBatch::Block coprocBlk(coproc, blockHeader,
                                      Coprocessor::Op::FORWARD);
    coprocBlk.Start ();

    UndoData undo;
    PerformanceTimer timer;
    const GameStateData newState
        = rules->ProcessForward (oldState, blockData, undo, &coprocBlk);
    timer.Stop ();
    LOG (INFO)
        << "Processing block " << height << " forward took " << timer;

    storage->AddUndoData (hash, height, undo);
    storage->SetCurrentGameStateWithHeight (hash, height, newState);

    coprocBlk.Finish ();
    tx.Commit ();
    rules->GameStateUpdated (newState, blockHeader);
  }

  LOG (INFO)
      << "Current game state is at height " << height
      << " (block " << hash.ToHex () << ")";
  NotifyStateChange ();

  return true;
}

bool
Game::UpdateStateForDetach (const uint256& parent, const uint256& hash,
                            const Json::Value& blockData)
{
  uint256 currentHash;
  CHECK (storage->GetCurrentBlockHash (currentHash));
  if (currentHash != hash)
    {
      LOG (WARNING)
          << "Game state hash " << currentHash.ToHex ()
          << " does not match detached block's hash " << hash.ToHex ();
      return false;
    }

  UndoData undo;
  if (!storage->GetUndoData (hash, undo))
    {
      LOG (ERROR)
          << "Failed to retrieve undo data for block " << hash.ToHex ()
          << ".  Need to resync from scratch.";
      transactionManager.TryAbortTransaction ();
      CHECK (!FLAGS_xaya_crash_without_undo)
          << "Block " << hash.ToHex ()
          << " is being detached without undo data";
      storage->Clear ();
      return false;
    }

  const GameStateData newState = storage->GetCurrentGameState ();

  {
    internal::ActiveTransaction tx(transactionManager);

    CHECK (blockData.isObject ());
    const auto& blockHeader = blockData["block"];
    CHECK (blockHeader.isObject ());
    const unsigned height = blockHeader["height"].asUInt ();
    CHECK_GT (height, 0);

    /* Note that here (unlike GameStateUpdated below), we want to pass the block
       that is being undone, not the new best block (its parent).  */
    CoprocessorBatch::Block coprocBlk(coproc, blockHeader,
                                      Coprocessor::Op::BACKWARD);
    coprocBlk.Start ();

    PerformanceTimer timer;
    const GameStateData oldState
        = rules->ProcessBackwards (newState, blockData, undo, &coprocBlk);
    timer.Stop ();
    LOG (INFO)
        << "Undoing block " << height << " took " << timer;

    storage->SetCurrentGameStateWithHeight (parent, height - 1, oldState);
    storage->ReleaseUndoData (hash);

    /* The new state's block data is not directly known, but we can conclude
       some information about it.  */
    Json::Value stateBlockHeader(Json::objectValue);
    stateBlockHeader["height"] = static_cast<Json::Int64> (height - 1);
    stateBlockHeader["hash"] = parent.ToHex ();

    coprocBlk.Finish ();
    tx.Commit ();
    rules->GameStateUpdated (oldState, stateBlockHeader);
  }

  LOG (INFO)
      << "Detached " << hash.ToHex () << ", restored state for block "
      << parent.ToHex ();
  NotifyStateChange ();

  return true;
}

bool
Game::IsReqtokenRelevant (const Json::Value& data) const
{
  std::string msgReqToken;
  if (data.isMember ("reqtoken"))
    msgReqToken = data["reqtoken"].asString ();

  if (state == State::CATCHING_UP)
    return msgReqToken == reqToken;

  return msgReqToken.empty ();
}

void
Game::BlockAttach (const std::string& id, const Json::Value& data,
                   const bool seqMismatch)
{
  CHECK_EQ (id, gameId);
  VLOG (2) << "Attached:\n" << data;

  uint256 parent;
  CHECK (parent.FromHex (data["block"]["parent"].asString ()));
  uint256 hash;
  CHECK (hash.FromHex (data["block"]["hash"].asString ()));
  VLOG (1) << "Attaching block " << hash.ToHex ();

  std::lock_guard<std::mutex> lock(mut);

  /* If we are at the desired sync target, do nothing.  */
  if (state == State::AT_TARGET)
    {
      VLOG (1) << "Ignoring attach, we are at sync target";
      return;
    }

  /* If we missed notifications, always reinitialise the state to make sure
     that all is again consistent.  */
  if (seqMismatch)
    {
      LOG (WARNING) << "Missed ZMQ notifications, reinitialising state";
      ReinitialiseState ();
      if (pruningQueue != nullptr)
        pruningQueue->Reset ();
      NotifyInstanceStateChanged ();
      return;
    }

  /* Ignore notifications that are not relevant at the moment.  */
  if (!IsReqtokenRelevant (data))
    {
      VLOG (1) << "Ignoring irrelevant attach notification";
      return;
    }

  const unsigned height = data["block"]["height"].asUInt ();

  bool needReinit = false;
  try
    {
      /* Handle the notification depending on the current state.  */
      switch (state)
        {
        case State::PREGENESIS:
          CHECK_GE (genesisHeight, 0);
          /* Check if we have reached the game's genesis height.  If we have,
             reinitialise which will store the initial game state.  */
          if (height >= static_cast<unsigned> (genesisHeight))
            needReinit = true;
          break;

        case State::CATCHING_UP:
          if (!UpdateStateForAttach (parent, hash, data))
            needReinit = true;

          /* If we are now at the last catching-up's target block hash,
             reinitialise the state as well.  This will check the current best
             tip and set the state to UP_TO_DATE or request more updates.  */
          if (hash == catchingUpTarget)
            needReinit = true;

          break;

        case State::UP_TO_DATE:
          if (!UpdateStateForAttach (parent, hash, data))
            needReinit = true;
          break;

        case State::UNKNOWN:
        case State::DISCONNECTED:
        case State::OUT_OF_SYNC:
        default:
          LOG (FATAL) << "Unexpected state: " << StateToString (state);
          break;
        }

      /* Attach the block in the pruning queue.  This is done after updating the
         state so that a potential pruning with nBlocks=0 can take place.  */
      if (pruningQueue != nullptr)
        pruningQueue->AttachBlock (hash, height);
    }
  catch (const StorageInterface::RetryWithNewTransaction& exc)
    {
      LOG (WARNING) << "Storage update failed, retrying: " << exc.what ();
      needReinit = true;
    }

  /* If UpdateStateForAttach failed to attach the block, then hash might
     not actually correspond to the current game state.  But in that case,
     needReinit is true, and then ReinitialiseState() below will ensure that
     we fix everything "from scratch", no matter what we do now.  */
  if (hash == targetBlock)
    state = State::AT_TARGET;

  if (needReinit)
    ReinitialiseState ();

  LOG_IF (INFO, state == State::AT_TARGET)
      << "Reached target block " << targetBlock.ToHex ()
      << ", pausing sync for now";

  if (state == State::UP_TO_DATE && pending != nullptr)
    {
      pending->ProcessAttachedBlock (storage->GetCurrentGameState (), data);
      NotifyPendingStateChange ();
    }

  NotifyInstanceStateChanged ();
}

void
Game::BlockDetach (const std::string& id, const Json::Value& data,
                   const bool seqMismatch)
{
  CHECK_EQ (id, gameId);
  VLOG (2) << "Detached:\n" << data;

  uint256 parent;
  CHECK (parent.FromHex (data["block"]["parent"].asString ()));
  uint256 hash;
  CHECK (hash.FromHex (data["block"]["hash"].asString ()));
  VLOG (1) << "Detaching block " << hash.ToHex ();

  std::lock_guard<std::mutex> lock(mut);

  /* If we are at the desired sync target, do nothing.  */
  if (state == State::AT_TARGET)
    {
      VLOG (1) << "Ignoring detach, we are at sync target";
      return;
    }

  /* If we missed notifications, always reinitialise the state to make sure
     that all is again consistent.  */
  if (seqMismatch)
    {
      LOG (WARNING) << "Missed ZMQ notifications, reinitialising state";
      ReinitialiseState ();
      if (pruningQueue != nullptr)
        pruningQueue->Reset ();
      NotifyInstanceStateChanged ();
      return;
    }

  /* Ignore notifications that are not relevant at the moment.  */
  if (!IsReqtokenRelevant (data))
    {
      VLOG (1) << "Ignoring irrelevant detach notification";
      return;
    }

  bool needReinit = false;
  try
    {
      /* Handle the notification depending on the current state.  */
      switch (state)
        {
        case State::PREGENESIS:
          /* Detaches are irrelevant (and unlikely).  */
          break;

        case State::CATCHING_UP:
          if (!UpdateStateForDetach (parent, hash, data))
            needReinit = true;

          /* We may reach a catching-up target also when detaching blocks.  This
             happens, for instance, when a block was declared invalid and a
             couple of blocks was just detached.  If a ZMQ message is missed
             at the same time (*or if this was the very first detach
             notification*!), then the client is catching-up while only
             detaching.  */
          if (parent == catchingUpTarget)
            needReinit = true;

          break;

        case State::UP_TO_DATE:
          if (!UpdateStateForDetach (parent, hash, data))
            needReinit = true;
          break;

        case State::UNKNOWN:
        case State::DISCONNECTED:
        case State::OUT_OF_SYNC:
        default:
          LOG (FATAL) << "Unexpected state: " << StateToString (state);
          break;
        }

      /* Detach the block in the pruning queue as well.  */
      if (pruningQueue != nullptr)
        pruningQueue->DetachBlock ();
    }
  catch (const StorageInterface::RetryWithNewTransaction& exc)
    {
      LOG (WARNING) << "Storage update failed, retrying: " << exc.what ();
      needReinit = true;
    }

  /* If UpdateStateForDetach failed to attach the block, then parent might
     not actually correspond to the current game state.  But in that case,
     needReinit is true, and then ReinitialiseState() below will ensure that
     we fix everything "from scratch", no matter what we do now.  */
  if (parent == targetBlock)
    state = State::AT_TARGET;

  if (needReinit)
    ReinitialiseState ();

  LOG_IF (INFO, state == State::AT_TARGET)
      << "Reached target block " << targetBlock.ToHex ()
      << ", pausing sync for now";

  if (state == State::UP_TO_DATE && pending != nullptr)
    {
      /* The height passed to the PendingMoveProcessor should be the "confirmed"
         height for processing moves, which means that it is one less than the
         currently detached height.  */
      const unsigned height = data["block"]["height"].asUInt ();
      CHECK_GT (height, 0);

      pending->ProcessDetachedBlock (storage->GetCurrentGameState (), data);
      NotifyPendingStateChange ();
    }

  NotifyInstanceStateChanged ();
}

void
Game::PendingMove (const std::string& id, const Json::Value& data)
{
  CHECK_EQ (id, gameId);

  std::lock_guard<std::mutex> lock(mut);
  if (state == State::UP_TO_DATE)
    {
      uint256 hash;
      CHECK (storage->GetCurrentBlockHash (hash));

      CHECK (pending != nullptr);
      pending->ProcessTx (storage->GetCurrentGameState (), data);
      NotifyPendingStateChange ();
    }
  else
    {
      VLOG (1) << "Ignoring pending move while not up-to-date";
      VLOG (2) << "Full data: " << data;
    }
}

void
Game::HasStopped ()
{
  std::lock_guard<std::mutex> lock(mut);
  state = State::DISCONNECTED;
  LOG (INFO) << "ZMQ subscriber has stopped listening";
  NotifyInstanceStateChanged ();
}

void
Game::ConnectRpcClient (const XayaRpcProvider& rpc)
{
  std::lock_guard<std::mutex> lock(mut);
  CHECK (chain == Chain::UNKNOWN) << "RPC client is already connected";

  rpcProvider = &rpc;

  const Json::Value info = (**rpcProvider).getblockchaininfo ();
  const std::string chainStr = info["chain"].asString ();
  chain = ChainFromString (chainStr);
  CHECK (chain != Chain::UNKNOWN)
      << "Unexpected chain type returned by Xaya Core: " << chainStr;

  LOG (INFO) << "Connected to RPC daemon with chain " << ChainToString (chain);
  if (rules != nullptr)
    rules->InitialiseGameContext (chain, gameId, rpcProvider);
  if (pending != nullptr)
    pending->InitialiseGameContext (chain, gameId, rpcProvider);
}

unsigned
Game::GetXayaVersion () const
{
  std::lock_guard<std::mutex> lock(mut);
  CHECK (rpcProvider != nullptr && *rpcProvider);

  const auto info = (**rpcProvider).getnetworkinfo ();
  CHECK (info.isObject ());
  const auto& version = info["version"];
  CHECK (version.isUInt ());

  return version.asUInt ();
}

Chain
Game::GetChain () const
{
  std::lock_guard<std::mutex> lock(mut);
  CHECK (chain != Chain::UNKNOWN);
  return chain;
}

namespace
{

/**
 * Hash-to-height translation function that calls Xaya Core by RPC.
 */
unsigned
GetHeightForBlockHash (XayaRpcClient& rpc, const uint256& hash)
{
  const Json::Value data = rpc.getblockheader (hash.ToHex ());
  CHECK (data.isMember ("height"));
  const int height = data["height"].asInt ();
  CHECK_GE (height, 0);
  return height;
}

} // anonymous namespace

void
Game::SetStorage (StorageInterface& s)
{
  std::lock_guard<std::mutex> lock(mut);
  CHECK (!mainLoop.IsRunning ());
  CHECK (pruningQueue == nullptr);

  storage = std::make_unique<internal::StorageWithCachedHeight> (s,
      [this] (const uint256& hash) {
        CHECK (rpcProvider != nullptr && *rpcProvider);
        return GetHeightForBlockHash (**rpcProvider, hash);
      });

  LOG (INFO) << "Storage has been added to Game, initialising it now";
  storage->Initialise ();

  if (chain == Chain::REGTEST)
    {
      LOG (INFO) << "Enabling height-cache cross-checks for regtest mode";
      storage->EnableCrossChecks ();
    }

  transactionManager.SetStorage (*storage);
}

void
Game::SetGameLogic (GameLogic& gl)
{
  std::lock_guard<std::mutex> lock(mut);
  CHECK (!mainLoop.IsRunning ());
  rules = &gl;
  if (chain != Chain::UNKNOWN)
    rules->InitialiseGameContext (chain, gameId, rpcProvider);
}

void
Game::SetPendingMoveProcessor (PendingMoveProcessor& p)
{
  std::lock_guard<std::mutex> lock(mut);
  CHECK (!mainLoop.IsRunning ());
  pending = &p;
  if (chain != Chain::UNKNOWN)
    pending->InitialiseGameContext (chain, gameId, rpcProvider);
}

void
Game::EnablePruning (const unsigned nBlocks)
{
  LOG (INFO) << "Enabling pruning with " << nBlocks << " blocks to keep";

  std::lock_guard<std::mutex> lock(mut);
  CHECK (storage != nullptr);

  if (pruningQueue == nullptr)
    pruningQueue = std::make_unique<internal::PruningQueue> (*storage,
                                                             transactionManager,
                                                             nBlocks);
  else
    pruningQueue->SetDesiredSize (nBlocks);
}

void
Game::SetTargetBlock (const uint256& blk)
{
  LOG (INFO) << "Setting desired target block to " << blk.ToHex ();

  std::lock_guard<std::mutex> lock(mut);
  targetBlock = blk;

  if (state != State::DISCONNECTED)
    {
      ReinitialiseState ();
      NotifyInstanceStateChanged ();
    }
}

void
Game::AddCoprocessor (const std::string& name, Coprocessor& p)
{
  coproc.Add (name, p);
}

bool
Game::DetectZmqEndpoint ()
{
  Json::Value notifications;

  {
    std::lock_guard<std::mutex> lock(mut);
    CHECK (rpcProvider != nullptr && *rpcProvider)
        << "RPC client is not yet set up";
    notifications = (**rpcProvider).getzmqnotifications ();
  }
  VLOG (1) << "Configured ZMQ notifications:\n" << notifications;

  bool foundBlocks = false;
  for (const auto& val : notifications)
    {
      const auto& typeVal = val["type"];
      if (!typeVal.isString ())
        continue;

      const auto& addrVal = val["address"];
      CHECK (addrVal.isString ());
      const std::string address = addrVal.asString ();
      CHECK (!address.empty ());

      const std::string type = typeVal.asString ();
      if (type == "pubgameblocks")
        {
          LOG (INFO) << "Detected ZMQ blocks endpoint: " << address;
          zmq.SetEndpoint (address);
          foundBlocks = true;
          continue;
        }
      if (type == "pubgamepending")
        {
          LOG (INFO) << "Detected ZMQ pending endpoint: " << address;
          zmq.SetEndpointForPending (address);
          continue;
        }
    }

  if (foundBlocks)
    return true;

  LOG (WARNING) << "No -zmqpubgameblocks notifier seems to be set up";
  return false;
}

Json::Value
Game::UnlockedGetInstanceStateJson (uint256& hash, unsigned& height) const
{
  Json::Value res(Json::objectValue);
  res["gameid"] = gameId;
  res["chain"] = ChainToString (chain);
  res["state"] = StateToString (state);

  /* Getting the height for the hash value might throw, if we revert
     back to Xaya RPC and that is down.  We want to handle this case
     gracefully, so we can detect and recover from a temporarily
     down Xaya Core.  This code might be called in that case e.g.
     to actually check the GSP state, so it should not crash.  */
  try
    {
      if (!storage->GetCurrentBlockHashWithHeight (hash, height))
        {
          hash.SetNull ();
          return res;
        }
    }
  catch (const std::exception& exc)
    {
      LOG (ERROR) << "Exception getting block hash and height: " << exc.what ();
      hash.SetNull ();
      return res;
    }

  CHECK (!hash.IsNull ());
  res["blockhash"] = hash.ToHex ();
  res["height"] = height;

  return res;
}

void
Game::NotifyInstanceStateChanged () const
{
  uint256 hash;
  unsigned height;
  const auto state = UnlockedGetInstanceStateJson (hash, height);
  rules->InstanceStateChanged (state);
}

Json::Value
Game::GetCustomStateData (
    const std::string& jsonField,
    const ExtractJsonFromStateWithLock& cb) const
{
  std::unique_lock<std::mutex> lock(mut);

  uint256 hash;
  unsigned height;
  auto res = UnlockedGetInstanceStateJson (hash, height);

  if (hash.IsNull ())
    return res;

  const GameStateData gameState = storage->GetCurrentGameState ();
  res[jsonField] = cb (gameState, hash, height, std::move (lock));

  return res;
}

Json::Value
Game::GetCustomStateData (
    const std::string& jsonField,
    const ExtractJsonFromStateWithBlock& cb) const
{
  return GetCustomStateData (jsonField,
    [&cb] (const GameStateData& state, const uint256& hash,
           const unsigned height,
           std::unique_lock<std::mutex> lock)
    {
      lock.unlock ();
      return cb (state, hash, height);
    });
}

Json::Value
Game::GetCustomStateData (
    const std::string& jsonField,
    const ExtractJsonFromState& cb) const
{
  return GetCustomStateData (jsonField,
    [&cb] (const GameStateData& state, const uint256& hash,
           const unsigned height)
    {
      return cb (state);
    });
}

Json::Value
Game::GetCurrentJsonState () const
{
  return GetCustomStateData ("gamestate",
      [this] (const GameStateData& state,
              const uint256& hash, const unsigned height,
              std::unique_lock<std::mutex> lock)
        {
          /* We keep the lock for the callback here, since e.g. SQLiteGame
             requires the state to be locked during GameStateToJson.  This
             method is not meant for performance critical tasks anyway, in
             which case specific data should be extracted using
             GetCustomStateData instead.  */
          return rules->GameStateToJson (state);
        });
}

Json::Value
Game::GetNullJsonState () const
{
  Json::Value res = GetCustomStateData ("data",
      [] (const GameStateData& state)
        {
          return Json::Value ();
        });
  res.removeMember ("data");
  return res;
}

Json::Value
Game::GetPendingJsonState () const
{
  std::unique_lock<std::mutex> lock(mut);
  return UnlockedPendingJsonState ();
}

Json::Value
Game::UnlockedPendingJsonState () const
{
  if (!zmq.IsPendingEnabled ())
    throw jsonrpc::JsonRpcException (jsonrpc::Errors::ERROR_RPC_INTERNAL_ERROR,
                                     "pending moves are not tracked");
  CHECK (pending != nullptr);

  Json::Value res(Json::objectValue);
  res["version"] = pendingStateVersion;
  res["gameid"] = gameId;
  res["chain"] = ChainToString (chain);
  res["state"] = StateToString (state);

  uint256 hash;
  unsigned height;
  if (storage->GetCurrentBlockHashWithHeight (hash, height))
    {
      res["blockhash"] = hash.ToHex ();
      res["height"] = height;
    }

  res["pending"] = pending->ToJson ();

  return res;
}

bool
Game::IsHealthy () const
{
  return state == State::UP_TO_DATE;
}

void
Game::NotifyStateChange () const
{
  /* Callers are expected to already hold the mut lock here (as that is the
     typical case when they make changes to the state anyway).  */
  VLOG (1) << "Notifying waiting threads about state change...";
  cvStateChanged.notify_all ();
}

void
Game::NotifyPendingStateChange ()
{
  /* Callers are expected to already hold the mut lock here (as that is the
     typical case when they make changes to the state anyway).  */
  CHECK_GT (pendingStateVersion, WAITFORCHANGE_ALWAYS_BLOCK);
  ++pendingStateVersion;
  VLOG (1)
      << "Notifying waiting threads about change of pending state,"
      << " new version: " << pendingStateVersion;
  cvPendingStateChanged.notify_all ();
}

void
Game::WaitForChange (const uint256& oldBlock, uint256& newBlock) const
{
  std::unique_lock<std::mutex> lock(mut);

  if (!oldBlock.IsNull () && storage->GetCurrentBlockHash (newBlock)
          && newBlock != oldBlock)
    {
      VLOG (1)
          << "Current block is different from old block,"
             " immediate return from WaitForChange";
      return;
    }

  if (zmq.IsRunning ())
    {
      VLOG (1) << "Waiting for state change on condition variable...";
      cvStateChanged.wait_for (lock,
          std::chrono::milliseconds (FLAGS_xaya_waitforchange_timeout_ms));
      VLOG (1) << "Potential state change detected in WaitForChange";
    }
  else
    LOG (WARNING)
        << "WaitForChange called with no active ZMQ listener,"
           " returning immediately";

  if (!storage->GetCurrentBlockHash (newBlock))
    newBlock.SetNull ();
}

Json::Value
Game::WaitForPendingChange (const int oldVersion) const
{
  std::unique_lock<std::mutex> lock(mut);

  if (oldVersion != WAITFORCHANGE_ALWAYS_BLOCK
        && oldVersion != pendingStateVersion)
    {
      VLOG (1)
          << "Known version differs from current one,"
             " returning immediately from WaitForPendingState";
      return UnlockedPendingJsonState ();
    }

  if (zmq.IsRunning () && zmq.IsPendingEnabled ())
    {
      VLOG (1) << "Waiting for pending state change on condition variable...";
      cvPendingStateChanged.wait_for (lock,
          std::chrono::milliseconds (FLAGS_xaya_waitforchange_timeout_ms));
      VLOG (1) << "Potential state change detected in WaitForPendingChange";
    }
  else
    LOG (WARNING)
        << "WaitForPendingChange called with no ZMQ listener on pending moves,"
           " returning immediately";

  return UnlockedPendingJsonState ();
}

void
Game::TrackGame ()
{
  std::lock_guard<std::mutex> lock(mut);
  CHECK (rpcProvider != nullptr && *rpcProvider)
      << "RPC client is not yet set up";
  (**rpcProvider).trackedgames ("add", gameId);
  LOG (INFO) << "Added " << gameId << " to tracked games";
}

void
Game::UntrackGame ()
{
  std::lock_guard<std::mutex> lock(mut);
  CHECK (rpcProvider != nullptr && *rpcProvider)
      << "RPC client is not yet set up";
  (**rpcProvider).trackedgames ("remove", gameId);
  LOG (INFO) << "Removed " << gameId << " from tracked games";
}

void
Game::ConnectToZmq ()
{
  if (pending == nullptr)
    {
      LOG (WARNING)
          << "No PendingMoveProcessor has been set, disabling pending moves"
             " in the ZMQ subscriber";
      zmq.SetEndpointForPending ("");
    }

  TrackGame ();
  zmq.Start ();

  std::lock_guard<std::mutex> lock(mut);
  ReinitialiseState ();
  NotifyInstanceStateChanged ();
}

void
Game::Start ()
{
  ConnectToZmq ();

  if (FLAGS_xaya_connection_check_ms > 0)
    connectionChecker = std::make_unique<ConnectionCheckerThread> (*this);
}

void
Game::Stop ()
{
  connectionChecker.reset ();

  zmq.Stop ();
  UntrackGame ();
  CHECK (state == State::DISCONNECTED);

  /* Make sure to wake up all listeners waiting for a state update (as there
     won't be one anymore).  */
  NotifyStateChange ();
  NotifyPendingStateChange ();

  /* Give the RPC server some more time to return still active calls.  */
  std::this_thread::sleep_for (std::chrono::milliseconds (100));
}

void
Game::Run ()
{
  CHECK (storage != nullptr && rules != nullptr)
      << "Storage and GameLogic must be set before starting the main loop";

  internal::MainLoop::Functor startAction = [this] () { Start (); };
  internal::MainLoop::Functor stopAction = [this] () { Stop (); };

  mainLoop.Run (startAction, stopAction);
}

void
Game::SyncFromCurrentState (const Json::Value& blockchainInfo,
                            const uint256& currentHash)
{
  CHECK (state == State::OUT_OF_SYNC);

  /* If we are at the desired target, then nothing is to be done.  */
  if (currentHash == targetBlock)
    {
      LOG (INFO) << "Game state matches sync target";
      state = State::AT_TARGET;
      transactionManager.SetBatchSize (1);
      return;
    }

  if (targetBlock.IsNull ())
    {
      const std::string bestHash = blockchainInfo["bestblockhash"].asString ();
      uint256 daemonBestHash;
      CHECK (daemonBestHash.FromHex (bestHash));
      if (daemonBestHash == currentHash)
        {
          LOG (INFO) << "Game state matches current tip, we are up-to-date";
          state = State::UP_TO_DATE;
          transactionManager.SetBatchSize (1);
          return;
        }
    }

  LOG (INFO)
      << "Game state does not match current tip or target,"
         " requesting updates from "
      << currentHash.ToHex ();
  /* At this point, mut is locked.  This means that even if game_sendupdates
     pushes ZMQ notifications before returning from the RPC, the ZMQ thread
     will only be able to get them processed by BlockAttach and BlockDetach
     once game_sendupdates and the code here are done.  This ensures that
     we won't ignore ZMQ messages that we just requested simply because we
     are not yet aware of the associated reqtoken.  */
  Json::Value upd;
  {
    Json::Value params;
    params["fromblock"] = currentHash.ToHex ();
    params["gameid"] = gameId;
    if (!targetBlock.IsNull ())
      params["toblock"] = targetBlock.ToHex ();
    upd = (**rpcProvider).CallMethod ("game_sendupdates", params);
    if (!upd.isObject ())
      throw jsonrpc::JsonRpcException (
          jsonrpc::Errors::ERROR_CLIENT_INVALID_RESPONSE,
          upd.toStyledString ());
  }

  /* If an error is returned, such as when Xaya X is not yet synced to
     our "fromblock", reset the ZMQ connection so it gets restored and
     the sync retried later.  */
  const auto errValue = upd["error"];
  if (errValue.isBool () && errValue.asBool ())
    {
      LOG (ERROR)
          << "Game blocks update request returned error,"
          << " resetting ZMQ connection...";
      zmq.RequestStop ();
      return;
    }

  LOG (INFO)
      << "Retrieving " << upd["steps"]["detach"].asInt () << " detach and "
      << upd["steps"]["attach"].asInt () << " attach steps with reqtoken = "
      << upd["reqtoken"].asString ()
      << ", leading to block " << upd["toblock"].asString ();

  state = State::CATCHING_UP;
  transactionManager.SetBatchSize (transactionBatchSize);

  CHECK (catchingUpTarget.FromHex (upd["toblock"].asString ()));
  reqToken = upd["reqtoken"].asString ();
}

void
Game::ReinitialiseState ()
{
  state = State::UNKNOWN;
  LOG (INFO) << "Reinitialising game state";

  const Json::Value data = (**rpcProvider).getblockchaininfo ();

  uint256 currentHash;
  if (storage->GetCurrentBlockHash (currentHash))
    {
      LOG (INFO) << "We have a current game state, syncing from there";
      state = State::OUT_OF_SYNC;
      SyncFromCurrentState (data, currentHash);
      return;
    }

  /* We do not have a current state in the storage.  This means that we have
     to reset to the initial state.  */

  if (genesisHeight < 0)
    {
      CHECK_EQ (genesisHeight, -1);

      /* GetInitialState may be expensive, and it may do things like update
         some external game state (setting it to the initial one) as we do
         e.g. with SQLiteGame.  Hence we should avoid calling it often, and
         cache the genesis height once known.  That way, we call the function
         exactly twice, independent of how many blocks / reinitialisations
         we process in the mean time.  */

      std::string genesisHashHex;
      unsigned genesisHeightFromGame;
      rules->GetInitialState (genesisHeightFromGame, genesisHashHex, nullptr);
      genesisHeight = genesisHeightFromGame;
      LOG (INFO) << "Got genesis height from game: " << genesisHeight;
    }
  CHECK_GE (genesisHeight, 0);

  /* If the current block height in the daemon is not yet the game's genesis
     height, simply wait for the genesis hash to be attached.  */
  if (data["blocks"].asUInt () < static_cast<unsigned> (genesisHeight))
    {
      LOG (INFO)
          << "Block height " << data["blocks"].asInt ()
          << " is before the genesis height " << genesisHeight;
      state = State::PREGENESIS;
      return;
    }

  /* Otherwise, we can store the initial state and start to sync from there.
     Note that we need to clear the storage *before* we call GetInitialState
     again here, because the latter will update the external state for
     the initial game state with SQLiteGame (and potentially other
     storage modules in the future).  */

  transactionManager.TryAbortTransaction ();
  storage->Clear ();

  const std::string blockHashHex = (**rpcProvider).getblockhash (genesisHeight);
  uint256 blockHash;
  CHECK (blockHash.FromHex (blockHashHex));

  Json::Value stateBlockHeader(Json::objectValue);
  stateBlockHeader["height"] = static_cast<Json::Int64> (genesisHeight);
  stateBlockHeader["hash"] = blockHash.ToHex ();

  std::string genesisHashHex;
  unsigned genesisHeightDummy;
  GameStateData genesisData;
  try
    {
      /* The coprocessor batch is started before we call GetInitialState, which
         will be able to access coprocessors from the Context.  We manage the
         Coprocessor transaction manually here, since it is detached
         from the storage update later (which uses the transactionManager).  */
      CoprocessorBatch::Block coprocBlk(coproc, stateBlockHeader,
                                        Coprocessor::Op::INITIALISATION);
      coproc.BeginTransaction ();
      coprocBlk.Start ();
      genesisData = rules->GetInitialState (genesisHeightDummy, genesisHashHex,
                                            &coprocBlk);
      coprocBlk.Finish ();
      coproc.CommitTransaction ();
    }
  catch (...)
    {
      coproc.AbortTransaction ();
      throw;
    }
  CHECK_EQ (genesisHeight, genesisHeightDummy);

  if (genesisHashHex.empty ())
    {
      LOG (WARNING)
          << "Game did not specify genesis hash, retrieved "
          << blockHash.ToHex ();
      genesisHash = blockHash;
    }
  else
    {
      CHECK (genesisHash.FromHex (genesisHashHex));
      CHECK (blockHash == genesisHash)
        << "The game's genesis block hash and height do not match";
    }

  while (true)
    try
      {
        internal::ActiveTransaction tx(transactionManager);

        storage->SetCurrentGameStateWithHeight (genesisHash, genesisHeight,
                                                genesisData);

        tx.Commit ();
        rules->GameStateUpdated (genesisData, stateBlockHeader);

        break;
      }
    catch (const StorageInterface::RetryWithNewTransaction& exc)
      {
        LOG (WARNING) << "Storage update failed, retrying: " << exc.what ();
      }

  LOG (INFO)
      << "We are at the genesis height, stored initial game state for block "
      << genesisHash.ToHex ();
  NotifyStateChange ();

  state = State::OUT_OF_SYNC;
  SyncFromCurrentState (data, genesisHash);
}

void
Game::ProbeAndFixConnection ()
{
  VLOG (1) << "Probing game connection to Xaya...";

  if (state == State::DISCONNECTED)
    {
      LOG (INFO) << "Attempting to re-establish the Xaya connection...";
      try
        {
          CHECK (DetectZmqEndpoint ())
              << "ZMQ endpoints not configured in Xaya";
          ConnectToZmq ();
        }
      catch (const std::exception& exc)
        {
          LOG_FIRST_N (ERROR, 10) << "Exception caught: " << exc.what ();
          zmq.RequestStop ();
          return;
        }
    }

  const auto maxStaleness
      = std::chrono::milliseconds (FLAGS_xaya_zmq_staleness_ms);
  /* If we haven't received an update in this timeframe, we try to
     trigger one (even if no actual blocks have been mined since) by
     requesting a game_sendupdates just for that purpose.  With this being
     half the maximum allowed staleness, we (except for edge cases) ensure
     that we will ping & process the ping before attempting a reconnect
     in case the connection is still working fine.  */
  const auto pingStaleness = maxStaleness / 2;
  const auto staleness = zmq.GetBlockStaleness<std::chrono::milliseconds> ();

  if (staleness < pingStaleness)
    return;

  if (staleness > maxStaleness)
    {
      LOG (ERROR) << "ZMQ connection is stale, disconnecting...";
      zmq.RequestStop ();
      return;
    }

  try
    {
      LOG (WARNING) << "ZMQ connection seems stale, requesting a block";
      /* Request some updates to be sent, so we get out of staleness
         in case the ZMQ connection still works.  We want to request an
         update that is as cheap as possible.  So try to request just the
         last block (or thereabouts).  */
      XayaRpcClient& rpcClient = **rpcProvider;
      const auto data = rpcClient.getblockchaininfo ();
      const std::string fromHash
          = rpcClient.getblockhash (data["blocks"].asInt () - 1);
      rpcClient.game_sendupdates (fromHash, gameId);
    }
  catch (const std::exception& exc)
    {
      LOG_FIRST_N (ERROR, 10) << "Exception caught: " << exc.what ();
      zmq.RequestStop ();
    }
}

/* ************************************************************************** */

} // namespace xaya
