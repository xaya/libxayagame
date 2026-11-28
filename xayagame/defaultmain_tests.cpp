// Copyright (C) 2026 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "defaultmain.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace xaya
{
namespace
{

class TestComponent : public GameComponent
{

public:

  bool startCalled = false;
  bool stopCalled = false;
  bool throwOnStart = false;
  bool throwOnStop = false;

  std::string startError = "start failure";
  std::string stopError = "stop failure";

  void
  Start () override
  {
    startCalled = true;

    if (throwOnStart)
      throw std::runtime_error (startError);
  }

  void
  Stop () override
  {
    stopCalled = true;

    if (throwOnStop)
      throw std::runtime_error (stopError);
  }

};

class GameComponentRunnerTests : public testing::Test
{

protected:

  std::vector<std::unique_ptr<GameComponent>> components;

  TestComponent&
  Add ()
  {
    auto component = std::make_unique<TestComponent> ();
    auto* res = component.get ();
    components.push_back (std::move (component));
    return *res;
  }

};

TEST_F (GameComponentRunnerTests, StartStopsPreviouslyStartedOnFailure)
{
  auto& first = Add ();
  auto& second = Add ();
  second.throwOnStart = true;

  {
    internal::GameComponentRunner runner(components);
    EXPECT_THROW (runner.Start (), std::runtime_error);
  }

  EXPECT_TRUE (first.startCalled);
  EXPECT_TRUE (first.stopCalled);
  EXPECT_TRUE (second.startCalled);
  EXPECT_FALSE (second.stopCalled);
}

TEST_F (GameComponentRunnerTests, StartPreservesOriginalException)
{
  auto& first = Add ();
  auto& second = Add ();
  first.throwOnStop = true;
  second.throwOnStart = true;
  second.startError = "original startup failure";

  {
    internal::GameComponentRunner runner(components);

    try
      {
        runner.Start ();
        FAIL () << "Expected startup failure";
      }
    catch (const std::runtime_error& exc)
      {
        EXPECT_STREQ (exc.what (), "original startup failure");
      }
  }

  EXPECT_TRUE (first.stopCalled);
  EXPECT_FALSE (second.stopCalled);
}

TEST_F (GameComponentRunnerTests, StopContinuesAfterFailure)
{
  auto& first = Add ();
  auto& second = Add ();
  auto& third = Add ();
  second.throwOnStop = true;

  internal::GameComponentRunner runner(components);
  runner.Start ();

  EXPECT_THROW (runner.Stop (), std::runtime_error);

  EXPECT_TRUE (first.stopCalled);
  EXPECT_TRUE (second.stopCalled);
  EXPECT_TRUE (third.stopCalled);
}

TEST_F (GameComponentRunnerTests, DestructorStopsStartedComponents)
{
  auto& first = Add ();

  {
    internal::GameComponentRunner runner(components);
    runner.Start ();
  }

  EXPECT_TRUE (first.startCalled);
  EXPECT_TRUE (first.stopCalled);
}

} // anonymous namespace
} // namespace xaya
