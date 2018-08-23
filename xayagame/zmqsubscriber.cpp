// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zmqsubscriber.hpp"

#include <glog/logging.h>

#include <sstream>

namespace xaya
{
namespace internal
{

ZmqSubscriber::~ZmqSubscriber ()
{
  if (IsRunning ())
    Stop ();
  CHECK (socket == nullptr);
}

void
ZmqSubscriber::SetEndpoint (const std::string& address)
{
  CHECK (!IsRunning ());
  addr = address;
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
  for (unsigned parts = 1; ; ++parts)
    {
      zmq::message_t msg;
      try
        {
          bool gotMessage = false;
          while (!gotMessage)
            {
              gotMessage = socket->recv (&msg);

              /* Check if a shutdown is requested.  */
              std::lock_guard<std::mutex> lock(mut);
              if (shouldStop)
                return false;
            }
        }
      catch (const zmq::error_t& exc)
        {
          /* See if the error is because the socket was closed.  In that case,
             we just want to shut down the listener thread.  */
          if (exc.num () == ETERM)
            return false;
          throw;
        }

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

      int more;
      size_t moreSize = sizeof (more);
      socket->getsockopt (ZMQ_RCVMORE, &more, &moreSize);
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
  rbuilder["rejectDupKeys"] = true;

  std::string topic;
  std::string payload;
  uint32_t seq;
  while (self->ReceiveMultiparts (topic, payload, seq))
    {
      VLOG (1) << "Received " << topic << " with sequence number " << seq;
      VLOG (2) << "Payload:\n" << payload;

      std::string gameId;
      bool isAttach;
      if (CheckTopicPrefix (topic, "game-block-attach json ", gameId))
        isAttach = true;
      else if (CheckTopicPrefix (topic, "game-block-detach json ", gameId))
        isAttach = false;
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
          << "Error parsing notification JSON: " << parseErrs;

      for (auto i = range.first; i != range.second; ++i)
        if (isAttach)
          i->second->BlockAttach (gameId, data, seqMismatch);
        else
          i->second->BlockDetach (gameId, data, seqMismatch);
    }
}

void
ZmqSubscriber::Start ()
{
  CHECK (IsEndpointSet ());
  LOG (INFO) << "Starting ZMQ subscriber at address: " << addr;

  CHECK (!IsRunning ());
  socket = std::make_unique<zmq::socket_t> (ctx, ZMQ_SUB);
  for (const auto& entry : listeners)
    for (const std::string cmd : {"game-block-attach", "game-block-detach"})
      {
        const std::string topic = cmd + " json " + entry.first;
        socket->setsockopt (ZMQ_SUBSCRIBE, topic.data (), topic.size ());
      }
  const int timeout = 100;
  socket->setsockopt (ZMQ_RCVTIMEO, &timeout, sizeof (timeout));
  socket->connect (addr.c_str ());

  /* Reset last-seen sequence numbers for a fresh start.  */
  lastSeq.clear ();

  shouldStop = false;
  worker = std::make_unique<std::thread> (&ZmqSubscriber::Listen, this);
}

void
ZmqSubscriber::Stop ()
{
  CHECK (IsRunning ());
  LOG (INFO) << "Stopping ZMQ subscriber at address " << addr;

  {
    std::lock_guard<std::mutex> lock(mut);
    shouldStop = true;
  }

  worker->join ();
  worker.reset ();
  socket.reset ();
}

} // namespace internal
} // namespace xaya
