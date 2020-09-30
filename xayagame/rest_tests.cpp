// Copyright (C) 2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rest.hpp"

#include "testutils.hpp"

#include <microhttpd.h>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include <unordered_map>

namespace xaya
{
namespace
{

/* ************************************************************************** */

/** Port for our test REST server.  */
constexpr int REST_PORT = 18'042;

/** URL for the test REST client.  */
constexpr const char* REST_URL = "http://localhost:18042";

/**
 * Test REST server, where we can just add in specific endpoints with
 * hardcoded results for them.
 */
class TestRestServer : public RestApi
{

private:

  /** Map of endpoint paths to the results to return for them.  */
  std::unordered_map<std::string, SuccessResult> results;

protected:

  SuccessResult
  Process (const std::string& url) override
  {
    const auto mit = results.find (url);
    if (mit == results.end ())
      throw HttpError (MHD_HTTP_NOT_FOUND, "invalid API endpoint");

    return mit->second;
  }

public:

  TestRestServer ()
    : RestApi(REST_PORT)
  {
    Start ();
  }

  ~TestRestServer ()
  {
    Stop ();
  }

  /**
   * Adds a hardcoded result.
   */
  void
  AddResult (const std::string& path, const SuccessResult& res)
  {
    CHECK (results.emplace (path, res).second)
        << "Duplicate endpoint: " << path;
  }

};

/**
 * REST client used in our tests.
 */
class TestRestClient : public RestClient
{

public:

  TestRestClient ()
    : RestClient(REST_URL)
  {}

};

} // anonymous namespace

class RestTests : public testing::Test
{

protected:

  TestRestServer srv;
  TestRestClient client;

  using SuccessResult = RestApi::SuccessResult;

  static bool
  MatchEndpoint (const std::string& path, const std::string& endpoint,
                 std::string& remainder)
  {
    return RestApi::MatchEndpoint (path, endpoint, remainder);
  }

};

namespace
{

/* ************************************************************************** */

TEST_F (RestTests, MatchEndpoint)
{
  std::string remainder;
  EXPECT_FALSE (MatchEndpoint ("", "/foo", remainder));
  EXPECT_FALSE (MatchEndpoint ("/bar", "/foo", remainder));

  ASSERT_TRUE (MatchEndpoint ("", "", remainder));
  EXPECT_EQ (remainder, "");

  ASSERT_TRUE (MatchEndpoint ("/foo", "/foo", remainder));
  EXPECT_EQ (remainder, "");

  ASSERT_TRUE (MatchEndpoint ("/foo/bla", "/foo/", remainder));
  EXPECT_EQ (remainder, "bla");
}

TEST_F (RestTests, RawPayload)
{
  srv.AddResult ("/foo", SuccessResult ("text/plain", "foo"));
  srv.AddResult ("/bar", SuccessResult ("text/plain", "bar"));

  RestClient::Request req1(client);
  ASSERT_TRUE (req1.Send ("/foo"));
  EXPECT_EQ (req1.GetType (), "text/plain");
  EXPECT_EQ (req1.GetData (), "foo");

  RestClient::Request req2(client);
  ASSERT_TRUE (req2.Send ("/bar"));
  EXPECT_EQ (req2.GetType (), "text/plain");
  EXPECT_EQ (req2.GetData (), "bar");
}

TEST_F (RestTests, InvalidEndpoint)
{
  RestClient::Request req(client);
  EXPECT_FALSE (req.Send ("/invalid"));
}

TEST_F (RestTests, InvalidUrl)
{
  srv.AddResult ("/", SuccessResult ("text/plain", "success"));

  srv.Stop ();
  RestClient::Request req1(client);
  EXPECT_FALSE (req1.Send ("/"));
  srv.Start ();

  RestClient::Request req2(client);
  EXPECT_TRUE (req2.Send ("/"));
}

TEST_F (RestTests, Compression)
{
  const std::string rawData(1 << 20, 'x');
  const SuccessResult rawResult("text/plain", rawData);
  srv.AddResult ("/data", rawResult);
  srv.AddResult ("/data.gz", rawResult.Gzip ());

  RestClient::Request req1(client);
  ASSERT_TRUE (req1.Send ("/data"));
  EXPECT_EQ (req1.GetType (), "text/plain");
  EXPECT_EQ (req1.GetData (), rawData);

  RestClient::Request req2(client);
  ASSERT_TRUE (req2.Send ("/data.gz"));
  EXPECT_EQ (req2.GetType (), "text/plain");
  EXPECT_EQ (req2.GetData (), rawData);
}

TEST_F (RestTests, Json)
{
  const auto value = ParseJson (R"({
    "foo": "bar",
    "array": [1, 2, null]
  })");
  const SuccessResult result(value);
  srv.AddResult ("/data.json", result);
  srv.AddResult ("/data.json.gz", result.Gzip ());

  RestClient::Request req1(client);
  ASSERT_TRUE (req1.Send ("/data.json"));
  EXPECT_EQ (req1.GetType (), "application/json");
  EXPECT_EQ (req1.GetJson (), value);

  RestClient::Request req2(client);
  ASSERT_TRUE (req2.Send ("/data.json.gz"));
  EXPECT_EQ (req2.GetType (), "application/json");
  EXPECT_EQ (req2.GetJson (), value);
}

TEST_F (RestTests, InvalidJson)
{
  srv.AddResult ("/not.json", SuccessResult ("application/json", "invalid"));

  RestClient::Request req(client);
  EXPECT_FALSE (req.Send ("/not.json"));
}

/* ************************************************************************** */

} // anonymous namespace
} // namespace xaya
