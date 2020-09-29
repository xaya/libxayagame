// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAGAME_REST_HPP
#define XAYAGAME_REST_HPP

#include "defaultmain.hpp"

#include <json/json.h>

#include <stdexcept>
#include <string>

struct MHD_Daemon;

namespace xaya
{

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

  /**
   * Request handler function for MHD.
   */
  static int RequestCallback (void* data, struct MHD_Connection* conn,
                              const char* url, const char* method,
                              const char* version,
                              const char* upload, size_t* uploadSize,
                              void** connData);

  friend class RestApiTests;

protected:

  struct SuccessResult;
  class HttpError;

  /**
   * Internal request handler function.  It should return the value
   * we want to send on success, or throw an HttpError instance.
   */
  virtual SuccessResult Process (const std::string& url) = 0;

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

  SuccessResult () = default;

public:

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

} // namespace xaya

#endif // XAYAGAME_REST_HPP
