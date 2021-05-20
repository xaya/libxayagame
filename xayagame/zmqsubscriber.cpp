// Copyright (C) 2018-2021 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zmqsubscriber.hpp"

#include <glog/logging.h>

#include <chrono>
#include <sstream>

namespace xaya
{
namespace internal
{

ZmqSubscriber::~ZmqSubscriber ()
{
  if (IsRunning ())
    Stop ();
  CHECK (sockets.empty ());
}

void
ZmqSubscriber::SetEndpoint (const std::string& address)
{
  CHECK (!IsRunning ());
  addrBlocks = address;
}

void
ZmqSubscriber::SetEndpointForPending (const std::string& address)
{
  CHECK (!IsRunning ());
  addrPending = address;
}

void
ZmqSubscriber::AddListener (const std::string& gameId, ZmqListener* listener)
{
  CHECK (!IsRunning ());
  listeners.emplace (gameId, listener);
}

bool
ZmqSubscriber::ReceiveMultiparts (std::string& topic, std::string& payload,
                                  uint32_t& seq)
{
  CHECK (!sockets.empty ());

  std::vector<zmq::pollitem_t> pollItems;
  for (const auto& s : sockets)
    {
      pollItems.emplace_back ();
      pollItems.back ().socket = static_cast<void*> (*s);
      pollItems.back ().events = ZMQ_POLLIN;
    }

  /* Wait until we can receive messages from any of our sockets.  */
  int rcPoll;
  do
    {
      constexpr auto TIMEOUT = std::chrono::milliseconds (100);
      rcPoll = zmq::poll (pollItems, TIMEOUT);

      /* In case of an error, zmq::poll throws instead of returning
         negative values.  */
      CHECK_GE (rcPoll, 0);

      /* Stop the thread if requested to, no need to read the messages anymore
         if there are ones available.  */
      if (shouldStop)
        return false;
    }
  while (rcPoll == 0);

  /* Find the socket that is available (or one of them).  */
  zmq::socket_t* socket = nullptr;
  for (size_t i = 0; i < pollItems.size (); ++i)
    if (pollItems[i].revents & ZMQ_POLLIN)
      {
        socket = sockets[i].get ();
        break;
      }
  CHECK (socket != nullptr);

  /* Read all message parts from the socket.  */
  for (unsigned parts = 1; ; ++parts)
    {
      zmq::message_t msg;
      CHECK (socket->recv (msg));

      switch (parts)
        {
        case 1:
          {
            const char* data = static_cast<const char*> (msg.data ());
            topic = std::string (data, data + msg.size ());
            break;
          }

        case 2:
          {
            const char* data = static_cast<const char*> (msg.data ());
            payload = std::string (data, data + msg.size ());
            break;
          }

        case 3:
          {
            const unsigned char* data
                = static_cast<const unsigned char*> (msg.data ());
            CHECK_EQ (msg.size (), 4)
                << "ZMQ sequence number should have size 4";
            seq = 0;
            for (int i = 0; i < 4; ++i)
              {
                seq <<= 8;
                seq |= data[3 - i];
              }
            break;
          }

        default:
          LOG (FATAL)
              << "Unexpected number of parts while receiving ZMQ message";
        }

      const int more = socket->get (zmq::sockopt::rcvmore);
      if (!more)
        {
          CHECK_EQ (parts, 3) << "Expected exactly three message parts in ZMQ";
          return true;
        }
      CHECK_LT (parts, 3) << "Expected exactly three message parts in ZMQ";
    }
}

namespace
{

/**
 * Checks if the topic string starts with the given prefix.  If it does,
 * then the remaining part is further returned in "suffix".
 */
bool
CheckTopicPrefix (const std::string& topic, const std::string& prefix,
                  std::string& suffix)
{
  if (!topic.compare (0, prefix.size (), prefix) == 0)
    return false;

  suffix = topic.substr (prefix.size ());
  return true;
}

} // anonymous namespace

void
ZmqSubscriber::Listen (ZmqSubscriber* self)
{
  if (self->noListeningForTesting)
    return;

  Json::CharReaderBuilder rbuilder;
  rbuilder["allowComments"] = false;
  rbuilder["strictRoot"] = true;
  rbuilder["failIfExtra"] = true;
  /* Xaya Core's univalue accepts duplicate keys, so it may forward moves to
     us that contain duplicate keys.  We need to handle them gracefully when
     parsing.  With our options, JsonCpp will accept them, and dedup by
     keeping only the last value.  */
  rbuilder["rejectDupKeys"] = false;

  std::string topic;
  std::string payload;
  uint32_t seq;
  while (self->ReceiveMultiparts (topic, payload, seq))
    {
      VLOG (1) << "Received " << topic << " with sequence number " << seq;
      VLOG (2) << "Payload:\n" << payload;

      enum class TopicType
      {
        UNKNOWN,
        ATTACH,
        DETACH,
        PENDING,
      };

      std::string gameId;
      TopicType type = TopicType::UNKNOWN;
      if (CheckTopicPrefix (topic, "game-block-attach json ", gameId))
        type = TopicType::ATTACH;
      else if (CheckTopicPrefix (topic, "game-block-detach json ", gameId))
        type = TopicType::DETACH;
      else if (CheckTopicPrefix (topic, "game-pending-move json ", gameId))
        type = TopicType::PENDING;
      else
        LOG (FATAL) << "Unexpected topic of ZMQ notification: " << topic;

      auto mit = self->lastSeq.find (topic);
      bool seqMismatch;
      if (mit == self->lastSeq.end ())
        {
          self->lastSeq.emplace (topic, seq);
          seqMismatch = true;
        }
      else
        {
          seqMismatch = (seq != mit->second + 1);
          mit->second = seq;
        }

      const auto range = self->listeners.equal_range (gameId);
      if (range.first == self->listeners.end ())
        continue;

      Json::Value data;
      std::string parseErrs;
      std::istringstream in(payload);
      CHECK (Json::parseFromStream (rbuilder, in, &data, &parseErrs))
          << "Error parsing notification JSON: " << parseErrs
          << "\n" << payload;

      for (auto i = range.first; i != range.second; ++i)
        switch (type)
          {
          case TopicType::ATTACH:
            i->second->BlockAttach (gameId, data, seqMismatch);
            break;
          case TopicType::DETACH:
            i->second->BlockDetach (gameId, data, seqMismatch);
            break;
          case TopicType::PENDING:
            i->second->PendingMove (gameId, data);
            break;
          default:
            LOG (FATAL) << "Invalid topic type: " << static_cast<int> (type);
          }
    }
}

void
ZmqSubscriber::Start ()
{
  CHECK (!addrBlocks.empty ()) << "ZMQ endpoint is not yet set";

  CHECK (!IsRunning ());
  CHECK (sockets.empty ());

  LOG (INFO) << "Starting ZMQ subscriber for blocks: " << addrBlocks;
  auto socket = std::make_unique<zmq::socket_t> (ctx, ZMQ_SUB);
  socket->connect (addrBlocks.c_str ());
  zmq::socket_t* const socketBlocks = socket.get ();
  sockets.push_back (std::move (socket));
  for (const auto& entry : listeners)
    for (const std::string cmd : {"game-block-attach", "game-block-detach"})
      {
        const std::string topic = cmd + " json " + entry.first;
        socketBlocks->set (zmq::sockopt::subscribe, topic);
      }

  if (!addrPending.empty ())
    {
      LOG (INFO) << "Receiving pending moves from: " << addrPending;

      zmq::socket_t* socketPending = nullptr;
      if (addrPending == addrBlocks)
        socketPending = socketBlocks;
      else
        {
          socket = std::make_unique<zmq::socket_t> (ctx, ZMQ_SUB);
          socket->connect (addrPending.c_str ());
          socketPending = socket.get ();
          sockets.push_back (std::move (socket));
        }

      for (const auto& entry : listeners)
        {
          const std::string topic = "game-pending-move json " + entry.first;
          socketPending->set (zmq::sockopt::subscribe, topic);
        }
    }
  else
    LOG (INFO) << "Not subscribing to pending moves";

  /* Reset last-seen sequence numbers for a fresh start.  */
  lastSeq.clear ();

  shouldStop = false;
  worker = std::make_unique<std::thread> (&ZmqSubscriber::Listen, this);
}

void
ZmqSubscriber::Stop ()
{
  CHECK (IsRunning ());
  LOG (INFO) << "Stopping ZMQ subscriber at address " << addrBlocks;

  shouldStop = true;

  worker->join ();
  worker.reset ();
  sockets.clear ();
}

} // namespace internal
} // namespace xaya
