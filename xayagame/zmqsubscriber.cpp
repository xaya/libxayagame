// Copyright (C) 2018 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zmqsubscriber.hpp"

#include <glog/logging.h>

namespace xaya
{
namespace internal
{

ZmqSubscriber::ZmqSubscriber (const std::string& id)
  : gameId(id)
{}

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

bool
ZmqSubscriber::ReceiveMultiparts (std::string& topic, std::string& payload,
                                  uint32_t& seq)
{
  for (int parts = 1; ; ++parts)
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
          throw exc;
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

void
ZmqSubscriber::Listen (ZmqSubscriber* self)
{
  std::string topic;
  std::string payload;
  uint32_t seq;
  while (self->ReceiveMultiparts (topic, payload, seq))
    {
      LOG (INFO) << "Received:\n" << topic << "\n" << payload << "\n" << seq;
    }
}

void
ZmqSubscriber::Start ()
{
  CHECK (IsEndpointSet ());
  LOG (INFO) << "Starting ZMQ subscriber at address: " << addr;

  CHECK (!IsRunning ());
  socket = std::make_unique<zmq::socket_t> (ctx, ZMQ_SUB);
  for (const std::string cmd : {"game-block-attach", "game-block-detach"})
    {
      const std::string topic = cmd + " json " + gameId;
      socket->setsockopt (ZMQ_SUBSCRIBE, topic.data (), topic.size ());
    }
  const int timeout = 1000;
  socket->setsockopt (ZMQ_RCVTIMEO, &timeout, sizeof (timeout));
  socket->connect (addr.c_str ());

  shouldStop = false;
  listener = std::make_unique<std::thread> (&ZmqSubscriber::Listen, this);
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

  listener->join ();
  listener.reset ();
  socket.reset ();
}

} // namespace internal
} // namespace xaya
