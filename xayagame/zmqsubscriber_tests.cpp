// Copyright (C) 2018-2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zmqsubscriber.hpp"

#include <zmq.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <thread>
#include <vector>

namespace xaya
{
namespace internal
{

namespace
{

using testing::_;
using testing::InSequence;

constexpr const char IPC_ENDPOINT[] = "ipc:///tmp/xayagame_zmqsubscriber_tests";
constexpr const char IPC_ENDPOINT_PENDING[]
    = "ipc:///tmp/xayagame_zmqsubscriber_tests_pending";

constexpr const char GAME_ID[] = "test-game";
constexpr const char OTHER_GAME_ID[] = "other-game";

class MockZmqListener : public ZmqListener
{

public:

  MockZmqListener ()
  {
    /* By default, expect no calls to be made.  The calls that we expect
       should explicitly be specified in the individual tests.  */
    EXPECT_CALL (*this, BlockAttach (_, _, _)).Times (0);
    EXPECT_CALL (*this, BlockDetach (_, _, _)).Times (0);
    EXPECT_CALL (*this, PendingMove (_, _)).Times (0);
  }

  MOCK_METHOD3 (BlockAttach, void (const std::string& gameId,
                                   const Json::Value& data, bool seqMismatch));
  MOCK_METHOD3 (BlockDetach, void (const std::string& gameId,
                                   const Json::Value& data, bool seqMismatch));
  MOCK_METHOD2 (PendingMove, void (const std::string& gameId,
                                   const Json::Value& data));

};

} // anonymous namespace

class BasicZmqSubscriberTests : public testing::Test
{

protected:

  /* ZMQ context and socket used for sending test notifications.  */
  zmq::context_t zmqCtx;
  zmq::socket_t zmqSocket;

  MockZmqListener mockListener;

  BasicZmqSubscriberTests ()
    : zmqCtx(), zmqSocket(zmqCtx, ZMQ_PUB)
  {
    zmqSocket.bind (IPC_ENDPOINT);

    /* It seems that sending on the test socket does not work after fork()
       in a death test.  For that reason, we have to send messages and
       also start the listeners *before* fork, so that the death test
       runs already in a multi-threaded environment.  */
    testing::FLAGS_gtest_death_test_style = "threadsafe";
  }

  /**
   * Sleep for "some time" to avoid the "slow joiner syndrome".
   */
  static void
  SleepSome ()
  {
    std::this_thread::sleep_for (std::chrono::milliseconds (10));
  }

  /**
   * Sends a multipart message consisting of the given strings on the given
   * socket.
   */
  static void
  SendMultipart (zmq::socket_t& sock, const std::vector<std::string>& parts)
  {
    for (size_t i = 0; i < parts.size (); ++i)
      {
        zmq::message_t msg(parts[i].begin (), parts[i].end ());
        zmq::send_flags flags;
        if (i + 1 < parts.size ())
          flags = zmq::send_flags::sndmore;
        else
          flags = zmq::send_flags::none;
        ASSERT_TRUE (sock.send (std::move (msg), flags));
      }
  }

  void
  SendMultipart (const std::vector<std::string>& parts)
  {
    SendMultipart (zmqSocket, parts);
  }

  /**
   * Sends a message with the given topic, JSON payload and sequence number
   * on the given socket.
   */
  static void
  SendMessage (zmq::socket_t& sock, const std::string& topic,
               const Json::Value& payload, const uint32_t seq)
  {
    std::ostringstream payloadStr;
    payloadStr << payload;

    const std::string seqData(reinterpret_cast<const char*> (&seq),
                              sizeof (seq));

    SendMultipart (sock, {topic, payloadStr.str (), seqData});
  }

  static void
  DisableListening (ZmqSubscriber& zmq)
  {
    zmq.noListeningForTesting = true;
  }

  static bool
  ReceiveMultiparts (ZmqSubscriber& zmq, std::string& topic,
                     std::string& payload, uint32_t& seq)
  {
    return zmq.ReceiveMultiparts (topic, payload, seq);
  }

  /**
   * Signals the ZmqSubscriber to stop.  This is an "alternative implementation"
   * of ZmqSubscriber::Stop that allows to join a custom thread (not the
   * worker) before destructing the socket.  It is used to test stopping
   * while ReceiveMultiparts is running on a test thread.
   */
  static void
  StopWithCustomThread (ZmqSubscriber& zmq, std::thread& threadToWaitFor)
  {
    zmq.shouldStop = true;

    threadToWaitFor.join ();
    zmq.sockets.clear ();
  }

};

namespace
{

/* ************************************************************************** */

TEST_F (BasicZmqSubscriberTests, SetEndpointWhenRunning)
{
  EXPECT_DEATH (
    {
      ZmqSubscriber zmq;
      zmq.SetEndpoint (IPC_ENDPOINT);
      zmq.Start ();
      zmq.SetEndpoint ("foo");
    }, "!IsRunning");

  EXPECT_DEATH (
    {
      ZmqSubscriber zmq;
      zmq.SetEndpoint (IPC_ENDPOINT);
      zmq.Start ();
      zmq.SetEndpointForPending ("foo");
    }, "!IsRunning");
}

TEST_F (BasicZmqSubscriberTests, AddListenerWhenRunning)
{
  EXPECT_DEATH (
    {
      ZmqSubscriber zmq;
      zmq.SetEndpoint (IPC_ENDPOINT);
      zmq.Start ();
      zmq.AddListener (OTHER_GAME_ID, &mockListener);
    }, "!IsRunning");
}

TEST_F (BasicZmqSubscriberTests, StartWithoutEndpoint)
{
  EXPECT_DEATH (
    {
      ZmqSubscriber zmq;
      zmq.Start ();
    }, "ZMQ endpoint is not yet set");
}

TEST_F (BasicZmqSubscriberTests, StartedTwice)
{
  EXPECT_DEATH (
    {
      ZmqSubscriber zmq;
      zmq.SetEndpoint (IPC_ENDPOINT);
      zmq.Start ();
      zmq.Start ();
    }, "!IsRunning");
}

TEST_F (BasicZmqSubscriberTests, StopWithoutStart)
{
  EXPECT_DEATH (
    {
      ZmqSubscriber zmq;
      zmq.Stop ();
    }, "IsRunning");
}

/* ************************************************************************** */

class ReceiveMultipartsTest : public BasicZmqSubscriberTests
{

protected:

  ZmqSubscriber zmq;
  const std::string subscribedTopic;

  std::string topic;
  std::string payload;
  uint32_t seq;

  ReceiveMultipartsTest ()
    : subscribedTopic(std::string ("game-block-attach json ") + GAME_ID)
  {
    zmq.SetEndpoint (IPC_ENDPOINT);
    DisableListening (zmq);

    /* We need a dummy listener so that we actually subscribe to the
       notifications.  Since we turn off listening, it won't be called.  */
    zmq.AddListener (GAME_ID, &mockListener);

    zmq.Start ();
    SleepSome ();
  }

};

TEST_F (ReceiveMultipartsTest, Works)
{
  SendMultipart ({subscribedTopic, "payload", std::string ({42, 1, 0, 5})});

  ASSERT_TRUE (ReceiveMultiparts (zmq, topic, payload, seq));
  EXPECT_EQ (topic, subscribedTopic);
  EXPECT_EQ (payload, "payload");
  EXPECT_EQ (seq, 42 + 0x05000100);
}

TEST_F (ReceiveMultipartsTest, StoppingInBetween)
{
  std::thread listener([this] ()
    {
      EXPECT_FALSE (ReceiveMultiparts (zmq, topic, payload, seq));
    });
  SleepSome ();
  StopWithCustomThread (zmq, listener);
}

TEST_F (ReceiveMultipartsTest, NonSubscribedTopicIgnored)
{
  SendMultipart ({"other topic", "stuff", "not four bytes"});
  SendMultipart ({subscribedTopic, "payload", "1234"});

  ASSERT_TRUE (ReceiveMultiparts (zmq, topic, payload, seq));
  EXPECT_EQ (topic, subscribedTopic);
  EXPECT_EQ (payload, "payload");
}

TEST_F (ReceiveMultipartsTest, InvalidSeqNumber)
{
  SendMultipart ({subscribedTopic, "payload", "not four bytes"});

  EXPECT_DEATH (
      ReceiveMultiparts (zmq, topic, payload, seq),
      "ZMQ sequence number should have size 4");
}

TEST_F (ReceiveMultipartsTest, TooFewParts)
{
  SendMultipart ({subscribedTopic, "payload"});

  EXPECT_DEATH (
      ReceiveMultiparts (zmq, topic, payload, seq),
      "Expected exactly three message parts");
}

TEST_F (ReceiveMultipartsTest, TooManyParts)
{
  SendMultipart ({subscribedTopic, "payload", "1234", "foo"});

  EXPECT_DEATH (
      ReceiveMultiparts (zmq, topic, payload, seq),
      "Expected exactly three message parts");
}

/* ************************************************************************** */

class ZmqSubscriberTests : public BasicZmqSubscriberTests
{

protected:

  ZmqSubscriber zmq;

  ZmqSubscriberTests ()
  {
    zmq.SetEndpoint (IPC_ENDPOINT);
    zmq.AddListener (GAME_ID, &mockListener);
    zmq.Start ();
    SleepSome ();
  }

  ~ZmqSubscriberTests ()
  {
    /* Wait so that the worker thread can process all sent messages before
       the mock object verifies expectations.  */
    SleepSome ();
  }

  void
  SendAttach (const std::string& gameId, const Json::Value& payload,
              const uint32_t seq)
  {
    SendMessage (zmqSocket, "game-block-attach json " + gameId, payload, seq);
  }

  void
  SendDetach (const std::string& gameId, const Json::Value& payload,
              const uint32_t seq)
  {
    SendMessage (zmqSocket, "game-block-detach json " + gameId, payload, seq);
  }

};

TEST_F (ZmqSubscriberTests, ListenerCalled)
{
  Json::Value payload1;
  payload1["test"] = 42;
  Json::Value payload2;
  payload2["test"] = 5;

  {
    InSequence dummy;
    EXPECT_CALL (mockListener, BlockAttach (GAME_ID, payload1, _));
    EXPECT_CALL (mockListener, BlockDetach (GAME_ID, payload2, _));
  }

  SendAttach (GAME_ID, payload1, 1);
  SendDetach (GAME_ID, payload2, 1);
}

TEST_F (ZmqSubscriberTests, JsonKeysDeduped)
{
  Json::Value payload;
  std::istringstream in(R"(
    {
      "test": 42,
      "nested":
        {
          "field": "last"
        }
    }
  )");
  in >> payload;
  EXPECT_CALL (mockListener, BlockAttach (GAME_ID, payload, _));

  const std::string topic = std::string ("game-block-attach json ") + GAME_ID;
  SendMultipart ({topic, R"(
    {
      "test": 1,
      "nested":
        {
          "field": "first",
          "field": "last"
        },
      "test": 42
    }
  )", "1234"});
}

TEST_F (ZmqSubscriberTests, SequenceNumber)
{
  Json::Value payload;
  payload["foo"] = "dummy";

  {
    InSequence dummy;
    EXPECT_CALL (mockListener, BlockAttach (GAME_ID, _, true));
    EXPECT_CALL (mockListener, BlockDetach (GAME_ID, _, true));
    EXPECT_CALL (mockListener, BlockAttach (GAME_ID, _, false));
    EXPECT_CALL (mockListener, BlockDetach (GAME_ID, _, false));
    EXPECT_CALL (mockListener, BlockAttach (GAME_ID, _, true));
    EXPECT_CALL (mockListener, BlockDetach (GAME_ID, _, true));
  }

  SendAttach (GAME_ID, payload, 1);
  SendDetach (GAME_ID, payload, 1);
  SendAttach (GAME_ID, payload, 2);
  SendDetach (GAME_ID, payload, 2);
  SendAttach (GAME_ID, payload, 5);
  SendDetach (GAME_ID, payload, 5);
}

TEST_F (ZmqSubscriberTests, IgnoredGame)
{
  Json::Value payload;
  payload["foo"] = "dummy";

  EXPECT_CALL (mockListener, BlockAttach (GAME_ID, _, _));

  SendAttach (OTHER_GAME_ID, payload, 1);
  SendAttach (GAME_ID, payload, 1);
  SendAttach (OTHER_GAME_ID, payload, 2);
}

TEST_F (ZmqSubscriberTests, MultipleListeners)
{
  Json::Value payload1;
  payload1["foo"] = 42;
  Json::Value payload2;
  payload2["foo"] = 5;

  MockZmqListener gameListener;
  MockZmqListener otherListener;

  zmq.Stop ();
  zmq.AddListener (GAME_ID, &gameListener);
  zmq.AddListener (OTHER_GAME_ID, &otherListener);
  zmq.Start ();
  SleepSome ();

  {
    InSequence dummy;
    EXPECT_CALL (mockListener, BlockAttach (GAME_ID, payload1, true));
    EXPECT_CALL (mockListener, BlockAttach (GAME_ID, payload1, false));
  }

  {
    InSequence dummy;
    EXPECT_CALL (gameListener, BlockAttach (GAME_ID, payload1, true));
    EXPECT_CALL (gameListener, BlockAttach (GAME_ID, payload1, false));
  }

  {
    InSequence dummy;
    EXPECT_CALL (otherListener, BlockAttach (OTHER_GAME_ID, payload2, true));
    EXPECT_CALL (otherListener, BlockAttach (OTHER_GAME_ID, payload2, false));
  }

  SendAttach (OTHER_GAME_ID, payload2, 1);
  SendAttach (GAME_ID, payload1, 1);
  SendAttach (GAME_ID, payload1, 2);
  SendAttach (OTHER_GAME_ID, payload2, 2);

  /* Give the worker time before the mocks are destructed.  */
  SleepSome ();
}

TEST_F (ZmqSubscriberTests, InvalidJson)
{
  const std::string topic = std::string ("game-block-attach json ") + GAME_ID;
  EXPECT_DEATH (
    {
      SendMultipart ({topic, "{} // Junk", "1234"});
      SleepSome ();
    }, "Error parsing");
}

/* ************************************************************************** */

class ZmqSubscriberPendingTests : public BasicZmqSubscriberTests
{

protected:

  /** Second socket bound to an alternate endpoint.  */
  zmq::socket_t zmqSocketPending;

  ZmqSubscriber zmq;

  ZmqSubscriberPendingTests ()
    : zmqSocketPending(zmqCtx, ZMQ_PUB)
  {
    zmqSocketPending.bind (IPC_ENDPOINT_PENDING);

    zmq.SetEndpoint (IPC_ENDPOINT);
    zmq.AddListener (GAME_ID, &mockListener);
  }

  ~ZmqSubscriberPendingTests ()
  {
    /* Wait so that the worker thread can process all sent messages before
       the mock object verifies expectations.  */
    SleepSome ();
  }

  /**
   * Runs a test where just pending messages are sent and received on the
   * given socket.
   */
  void
  TestJustPending (zmq::socket_t& sock)
  {
    Json::Value payload1;
    payload1["test"] = 42;
    Json::Value payload2;
    payload2["test"] = 5;

    {
      InSequence dummy;
      EXPECT_CALL (mockListener, PendingMove (GAME_ID, payload1));
      EXPECT_CALL (mockListener, PendingMove (GAME_ID, payload2));
    }

    SendPending (sock, GAME_ID, payload1);
    SendPending (sock, GAME_ID, payload2);
  }

  /**
   * Sends a pending move notification on the given socket.
   */
  static void
  SendPending (zmq::socket_t& sock, const std::string& gameId,
               const Json::Value& payload)
  {
    SendMessage (sock, "game-pending-move json " + gameId, payload, 42);
  }

};

TEST_F (ZmqSubscriberPendingTests, BasicOneSocket)
{
  zmq.SetEndpointForPending (IPC_ENDPOINT);
  zmq.Start ();
  SleepSome ();

  TestJustPending (zmqSocket);
}

TEST_F (ZmqSubscriberPendingTests, BasicTwoSockets)
{
  zmq.SetEndpointForPending (IPC_ENDPOINT_PENDING);
  zmq.Start ();
  SleepSome ();

  TestJustPending (zmqSocketPending);
}

TEST_F (ZmqSubscriberPendingTests, MixedOneSocket)
{
  zmq.SetEndpointForPending (IPC_ENDPOINT);
  zmq.Start ();
  SleepSome ();

  const std::string gameId = GAME_ID;

  Json::Value payload;
  payload["foo"] = "bar";

  /* When we use just one socket for both types of notifications, then
     they should be received in exactly the order in which they are sent
     (as we just read from that one socket sequentially).  */

  {
    InSequence dummy;
    EXPECT_CALL (mockListener, BlockDetach (gameId, payload, _));
    EXPECT_CALL (mockListener, PendingMove (gameId, payload));
    EXPECT_CALL (mockListener, BlockAttach (gameId, payload, _));
  }

  SendMessage (zmqSocket, "game-block-detach json " + gameId, payload, 1);
  SendPending (zmqSocket, gameId, payload);
  SendMessage (zmqSocket, "game-block-attach json " + gameId, payload, 2);
}

TEST_F (ZmqSubscriberPendingTests, MixedTwoSockets)
{
  zmq.SetEndpointForPending (IPC_ENDPOINT_PENDING);
  zmq.Start ();
  SleepSome ();

  const std::string gameId = GAME_ID;

  Json::Value payload;
  payload["foo"] = "bar";

  /* If we use two sockets and mix the messages from them, it is not defined
     in what order we will receive block messages vs pending messages.  */

  {
    InSequence dummy;
    EXPECT_CALL (mockListener, BlockDetach (gameId, payload, _));
    EXPECT_CALL (mockListener, BlockAttach (gameId, payload, _));
  }
  EXPECT_CALL (mockListener, PendingMove (gameId, payload));

  SendMessage (zmqSocket, "game-block-detach json " + gameId, payload, 1);
  SendPending (zmqSocketPending, gameId, payload);
  SendMessage (zmqSocket, "game-block-attach json " + gameId, payload, 2);
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace internal
} // namespace xaya
