// Copyright (C) 2019-2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_REST_HPP
#define XAYAGAME_REST_HPP

#include "defaultmain.hpp"
#include "game.hpp"

#include <curl/curl.h>

#include <json/json.h>

#include <stdexcept>
#include <string>

struct MHD_Daemon;

namespace xaya
{

/* ************************************************************************** */

/**
 * HTTP server providing a (read-only) REST API for a GSP.
 */
class RestApi : public GameComponent
{

private:

  /** The port to listen on.  */
  const int port;

  /** The underlying MDH daemon.  */
  struct MHD_Daemon* daemon = nullptr;

  class Callbacks;
  friend class RestTests;

protected:

  struct SuccessResult;
  class HttpError;

  /**
   * Internal request handler function.  It should return the value
   * we want to send on success, or throw an HttpError instance.
   */
  virtual SuccessResult Process (const std::string& url) = 0;

  /**
   * Default handler for the /state endpoint (essentially the same as default
   * getnullstate).  Returns true if it matched and the output result has
   * been set, and false if the endpoint did not match (nothing happens
   * to result).
   */
  bool HandleState (const std::string& url, const Game& game,
                    SuccessResult& result);

  /**
   * Default handler for the /healthz endpoint (similar to HandleState).
   * This returns HTTP code 200 if the Game instance considers itself
   * healthy (up-to-date and all fine), and HTTP code 500 if not.
   */
  bool HandleHealthz (const std::string& url, const Game& game,
                      SuccessResult& result);

  /**
   * Utility method for matching a full path against a particular API endpoint.
   * Returns true if the path starts with the given endpoint string, and in
   * that case sets "remainder" to the remaining part of the string.
   */
  static bool MatchEndpoint (const std::string& path,
                             const std::string& endpoint,
                             std::string& remainder);

public:

  explicit RestApi (const int p)
    : port(p)
  {}

  virtual ~RestApi ();

  RestApi () = delete;
  RestApi (const RestApi&) = delete;
  void operator= (const RestApi&) = delete;

  /**
   * Starts the REST server.  Processing of requests is done in a separate
   * thread, so this method returns immediately.
   */
  void Start () override;

  /**
   * Shuts down the REST server.
   */
  void Stop () override;

};

/**
 * A success return value, with content-type and payload.
 */
class RestApi::SuccessResult
{

private:

  /** The content type to return.  */
  std::string type;

  /** The raw payload data.  */
  std::string payload;

public:

  SuccessResult () = default;

  explicit SuccessResult (const std::string& t, const std::string& p)
    : type(t), payload(p)
  {}

  explicit SuccessResult (const Json::Value& val);

  SuccessResult (SuccessResult&&) = default;
  SuccessResult (const SuccessResult&) = default;

  SuccessResult& operator= (const SuccessResult&) = default;
  SuccessResult& operator= (SuccessResult&&) = default;

  /**
   * Compresses the existing result with gzip format and turns it into
   * a new result.
   */
  SuccessResult Gzip () const;

  const std::string&
  GetType () const
  {
    return type;
  }

  const std::string&
  GetPayload () const
  {
    return payload;
  }

};

/**
 * Exception class thrown for returning HTTP errors to the client
 * in the Process function.
 */
class RestApi::HttpError : public std::runtime_error
{

private:

  /** The HTTP status code to return.  */
  const int httpCode;

public:

  explicit HttpError (const int c, const std::string& msg)
    : std::runtime_error(msg), httpCode(c)
  {}

  /**
   * Returns the status code.
   */
  int
  GetStatusCode () const
  {
    return httpCode;
  }

};

/* ************************************************************************** */

/**
 * REST client handler.  This holds basic data for doing requests, like
 * the TLS certificates or base endpoint to use.
 */
class RestClient
{

private:

  /** The base API endpoint.  */
  std::string endpoint;

  /** If set, the CA file to use for TLS verification.  */
  std::string caFile;

public:

  class Request;

  /**
   * Constructs a client with the given endpoint.  Also initialises the cURL
   * library internally.
   */
  explicit RestClient (const std::string& url);

  RestClient (const RestClient&) = delete;
  void operator= (const RestClient&) = delete;

  /**
   * Sets the CA file to use.
   */
  void
  SetCaFile (const std::string& f)
  {
    caFile = f;
  }

};

/**
 * Utility class to send a request to a REST API.  It mostly is just a wrapper
 * around cURL easy for doing requests, but can also handle some processing
 * of the received data (e.g. gzip-decompression and JSON parsing).
 */
class RestClient::Request
{

private:

  /** The client this belongs to.  */
  const RestClient& client;

  /** The underlying cURL handle.  */
  CURL* handle;

  /** Error buffer.  */
  std::string errBuffer;

  /** Actual error message (from cURL or us).  */
  std::ostringstream error;

  /** Content type of the response.  */
  std::string type;

  /** Buffer into which the response data is saved.  */
  std::string data;

  /** Parsed JSON value, if the response is application/json.  */
  Json::Value jsonData;

  /**
   * Performs any post-request processing of the raw payload data.  Returns
   * false if something went wrong, e.g. the data claims to be JSON but
   * fails to parse.
   */
  bool ProcessData ();

  /**
   * Tries to uncompress the data if the content-type indicates +gzip.
   */
  bool ProcessGzip ();

  /**
   * Tries to parse data if the content-type is JSON.  Returns true if the
   * data is not JSON or parsing was fine, and false if the data claims to
   * be JSON but failed to parse.
   */
  bool ProcessJson ();

  /**
   * cURL write callback that saves the bytes into our data string.
   */
  static size_t WriteCallback (const char* ptr, size_t sz, size_t n,
                               Request* self);

public:

  explicit Request (const RestClient& c);
  ~Request ();

  Request () = delete;
  Request (const Request&) = delete;
  void operator= (const Request&) = delete;

  /**
   * Performs URL encoding of a string.
   */
  std::string UrlEncode (const std::string& str) const;

  /**
   * Start a request to the given path (relative to the client's endpoint).
   * Returns true on success and false if something went wrong.
   *
   * This transparently handles processing of the received data, for instance
   * gzip decompression if the content-type indicates it, or parsing of
   * the data as JSON if the content-type is application/json.
   */
  bool Send (const std::string& path);

  /**
   * Returns the raw payload data in case of success.
   */
  const std::string&
  GetData () const
  {
    return data;
  }

  /**
   * If this is JSON (application/json), returns the payload data as
   * JSON value.
   */
  const Json::Value&
  GetJson () const
  {
    return jsonData;
  }

  /**
   * Returns the content type of the response.
   */
  const std::string&
  GetType () const
  {
    return type;
  }

  /**
   * Returns the error message in case of an error.
   */
  std::string
  GetError () const
  {
    return error.str ();
  }

};

/* ************************************************************************** */

} // namespace xaya

#endif // XAYAGAME_REST_HPP
