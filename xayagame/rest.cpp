// Copyright (C) 2019-2026 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rest.hpp"

#include <microhttpd.h>

#include <zlib.h>

#include <glog/logging.h>

#include <algorithm>
#include <limits>

namespace xaya
{

namespace
{

/** Buffer size for IO with zlib's buffer-based gzip interface.  */
constexpr size_t GZIP_BUF_SIZE = 4'096;

/** Default timeout for the REST client.  */
constexpr auto REST_CLIENT_DEFAULT_TIMEOUT = std::chrono::seconds (10);

/**
 * Base helper for processing data through a zlib stream.  It handles feeding
 * input in chunks and collecting the produced output, while derived classes
 * initialise the stream and perform the actual deflate or inflate step.
 */
class ZlibStream
{

protected:

  /** Result of one step of a zlib stream operation.  */
  enum class StepResult
  {
    CONTINUE,
    END,
    INCOMPLETE,
    ERROR,
  };

  /** The underlying zlib stream struct.  */
  z_stream stream{};

  virtual ~ZlibStream () = default;

  /**
   * Returns a human-readable error message for a zlib return code.
   */
  std::string
  GetError (const int rc) const
  {
    if (stream.msg != nullptr)
      return stream.msg;
    return "zlib error code " + std::to_string (rc);
  }

  /**
   * Performs one operation step of the underlying stream.  This is passed
   * whether more input remains to be fed, and an error string to fill in
   * on failure.
   */
  virtual StepResult Step (bool moreInput, std::string& error) = 0;

  /**
   * Processes the given input through the stream, calling Step for each
   * chunk of output.
   */
  bool
  Process (const std::string& input, std::string& output,
           std::string& error)
  {
    const size_t maxChunk = std::numeric_limits<uInt>::max ();
    size_t offset = 0;
    char buf[GZIP_BUF_SIZE];

    while (true)
      {
        if (stream.avail_in == 0 && offset < input.size ())
          {
            const size_t n = std::min (input.size () - offset, maxChunk);
            stream.next_in = reinterpret_cast<Bytef*> (
                const_cast<char*> (input.data () + offset));
            stream.avail_in = static_cast<uInt> (n);
            offset += n;
          }

        stream.next_out = reinterpret_cast<Bytef*> (buf);
        stream.avail_out = sizeof (buf);

        const bool moreInput = (offset < input.size ());
        const auto res = Step (moreInput, error);
        output.append (buf, sizeof (buf) - stream.avail_out);

        switch (res)
          {
          case StepResult::CONTINUE:
            break;

          case StepResult::END:
            return true;

          case StepResult::INCOMPLETE:
          case StepResult::ERROR:
            return false;
          }
      }
  }

};

/**
 * Helper that deflates data into a gzip stream.
 */
class GzipDeflater : public ZlibStream
{

private:

  /** Whether the stream has already been ended.  */
  bool ended = false;

  StepResult
  Step (const bool moreInput, std::string& error) override
  {
    const bool noMoreInput = (!moreInput && stream.avail_in == 0);
    const int rc
        = deflate (&stream, noMoreInput ? Z_FINISH : Z_NO_FLUSH);
    if (rc == Z_OK)
      return StepResult::CONTINUE;
    if (rc == Z_STREAM_END)
      return StepResult::END;

    error = "gzip compression failed: " + GetError (rc);
    return StepResult::ERROR;
  }

public:

  GzipDeflater ()
  {
    const int rc = deflateInit2 (&stream, 9, Z_DEFLATED, MAX_WBITS + 16, 8,
                                 Z_DEFAULT_STRATEGY);
    if (rc != Z_OK)
      throw std::runtime_error (
          "deflateInit2 failed: " + GetError (rc));
  }

  ~GzipDeflater ()
  {
    if (!ended)
      deflateEnd (&stream);
  }

  GzipDeflater (const GzipDeflater&) = delete;
  void operator= (const GzipDeflater&) = delete;

  /**
   * Compresses the given input into a gzip stream, throwing on errors.
   */
  std::string
  Compress (const std::string& input)
  {
    std::string output;
    std::string error;
    if (!Process (input, output, error))
      throw std::runtime_error (error);

    const int rc = deflateEnd (&stream);
    ended = true;
    if (rc != Z_OK)
      throw std::runtime_error (
          "deflateEnd failed with error code " + std::to_string (rc));

    return output;
  }

};

/**
 * Helper that inflates data from a gzip stream.
 */
class GzipInflater : public ZlibStream
{

private:

  /** Whether the stream was initialised successfully.  */
  bool initialised = false;

  /** Error message from a failed initialisation.  */
  std::string initError;

  StepResult
  Step (const bool moreInput, std::string& error) override
  {
    const int rc = inflate (&stream, Z_NO_FLUSH);
    const bool inputExhausted = (!moreInput && stream.avail_in == 0);

    if (rc == Z_STREAM_END)
      {
        /* Handle concatenated gzip members.  */
        if (!inputExhausted)
          {
            const int resetRc = inflateReset (&stream);
            if (resetRc != Z_OK)
              {
                error = "gzip error: " + GetError (resetRc);
                return StepResult::ERROR;
              }
            return StepResult::CONTINUE;
          }
        return StepResult::END;
      }

    if (rc == Z_OK)
      {
        /* If no more input is available and the output buffer is not
           full, then the stream ended before its end marker.  */
        if (inputExhausted && stream.avail_out > 0)
          {
            error = "incomplete gzip stream";
            return StepResult::INCOMPLETE;
          }
        return StepResult::CONTINUE;
      }

    if (rc == Z_BUF_ERROR && inputExhausted)
      {
        error = "incomplete gzip stream";
        return StepResult::INCOMPLETE;
      }

    error = "gzip error: " + GetError (rc);
    error += " (code " + std::to_string (rc) + ")";
    return StepResult::ERROR;
  }

public:

  GzipInflater ()
  {
    const int rc = inflateInit2 (&stream, MAX_WBITS + 16);
    if (rc == Z_OK)
      initialised = true;
    else
      initError = "gzip error: " + GetError (rc);
  }

  ~GzipInflater ()
  {
    if (initialised)
      inflateEnd (&stream);
  }

  GzipInflater (const GzipInflater&) = delete;
  void operator= (const GzipInflater&) = delete;

  /**
   * Uncompresses the given gzip data, returning false with an error message
   * on failure.
   */
  bool
  Uncompress (const std::string& input, std::string& output,
              std::string& error)
  {
    if (!initialised)
      {
        error = initError;
        return false;
      }

    if (!Process (input, output, error))
      return false;

    const int rc = inflateEnd (&stream);
    initialised = false;
    if (rc != Z_OK)
      {
        error = "gzip error: zlib error code " + std::to_string (rc);
        return false;
      }

    return true;
  }

};

} // anonymous namespace

/* ************************************************************************** */

RestApi::SuccessResult::SuccessResult (const Json::Value& val)
  : type("application/json")
{
  Json::StreamWriterBuilder wbuilder;
  wbuilder["commentStyle"] = "None";
  wbuilder["indentation"] = "";
  wbuilder["enableYAMLCompatibility"] = false;
  wbuilder["dropNullPlaceholders"] = false;
  wbuilder["useSpecialFloats"] = false;

  payload = Json::writeString (wbuilder, val);
}

RestApi::SuccessResult
RestApi::SuccessResult::Gzip () const
{
  SuccessResult res;
  res.type = type + "+gzip";
  res.payload = GzipDeflater ().Compress (payload);

  return res;
}

/* ************************************************************************** */

RestApi::~RestApi ()
{
  if (daemon != nullptr)
    Stop ();
}

namespace
{

/**
 * Encapsulated MHD response.
 */
class Response
{

private:

  /** Underlying MHD response struct.  */
  struct MHD_Response* resp = nullptr;

  /** HTTP status code to use.  */
  int code;

public:

  Response () = default;

  ~Response ()
  {
    if (resp != nullptr)
      MHD_destroy_response (resp);
  }

  Response (const Response&) = delete;
  void operator= (const Response&) = delete;

  /**
   * Constructs the response from given text and with the given content type.
   */
  void
  Set (const int c, const std::string& type, const std::string& body)
  {
    CHECK (resp == nullptr);

    code = c;

    void* data = const_cast<void*> (static_cast<const void*> (body.data ()));
    resp = MHD_create_response_from_buffer (body.size (), data,
                                            MHD_RESPMEM_MUST_COPY);
    CHECK (resp != nullptr);

    CHECK_EQ (MHD_add_response_header (resp, "Content-Type", type.c_str ()),
              MHD_YES);
  }

  /**
   * Enqueues the response for sending by MHD.
   */
  auto
  Queue (struct MHD_Connection* conn)
  {
    CHECK (resp != nullptr);
    return MHD_queue_response (conn, code, resp);
  }

};

} // anonymous namespace

/**
 * Helper class that we just use to define the callback function in
 * a way that gives access to internals of RestApi, but without having
 * to declare it in the header (which would make the header depend on
 * libmicrohttpd).
 */
class RestApi::Callbacks
{

public:

  /**
   * Request handler function for MHD.
   */
  static auto Request (void* data, struct MHD_Connection* conn,
                       const char* url, const char* method,
                       const char* version,
                       const char* upload, size_t* uploadSize,
                       void** connData);

};

auto
RestApi::Callbacks::Request (void* data, struct MHD_Connection* conn,
                             const char* url, const char* method,
                             const char* version,
                             const char* upload, size_t* uploadSize,
                             void** connData)
{
  /* We use "auto" as return type here as well as in Response::Queue
     so that it both works with old libmicrohttpd (returning int)
     and newer versions (returning MHD_Result).  */

  LOG (INFO) << "REST server: " << method << " request to " << url;

  Response resp;
  try
    {
      if (std::string (method) != "GET")
        throw HttpError (MHD_HTTP_METHOD_NOT_ALLOWED, "only GET is supported");

      RestApi* self = static_cast<RestApi*> (data);
      const auto res = self->Process (url);
      resp.Set (MHD_HTTP_OK, res.GetType (), res.GetPayload ());
    }
  catch (const HttpError& exc)
    {
      const int code = exc.GetStatusCode ();
      const std::string& msg = exc.what ();
      LOG (WARNING) << "Returning HTTP error " << code << ": " << msg;
      resp.Set (code, "text/plain", msg);
    }
  catch (const std::exception& exc)
    {
      LOG (ERROR)
          << "Uncaught exception in REST request handler: " << exc.what ();
      resp.Set (MHD_HTTP_INTERNAL_SERVER_ERROR, "text/plain",
                "internal server error");
    }
  catch (...)
    {
      LOG (ERROR) << "Uncaught unknown exception in REST request handler";
      resp.Set (MHD_HTTP_INTERNAL_SERVER_ERROR, "text/plain",
                "internal server error");
    }

  return resp.Queue (conn);
}

void
RestApi::Start ()
{
  CHECK (daemon == nullptr);
  daemon = MHD_start_daemon (MHD_USE_INTERNAL_POLLING_THREAD, port,
                             nullptr, nullptr,
                             &Callbacks::Request, this, MHD_OPTION_END);
  CHECK (daemon != nullptr) << "Failed to start microhttpd daemon";
}

void
RestApi::Stop ()
{
  CHECK (daemon != nullptr);
  MHD_stop_daemon (daemon);
  daemon = nullptr;
}

bool
RestApi::MatchEndpoint (const std::string& path, const std::string& endpoint,
                        std::string& remainder)
{
  if (path.substr (0, endpoint.size ()) != endpoint)
    return false;

  CHECK_GE (path.size (), endpoint.size ());
  remainder = path.substr (endpoint.size ());
  return true;
}

bool
RestApi::HandleState (const std::string& url, const Game& game,
                      SuccessResult& result)
{
  std::string remainder;
  if (!MatchEndpoint (url, "/state", remainder) || remainder != "")
    return false;

  result = SuccessResult (game.GetNullJsonState ());
  return true;
}

bool
RestApi::HandleHealthz (const std::string& url, const Game& game,
                        SuccessResult& result)
{
  std::string remainder;
  if (!MatchEndpoint (url, "/healthz", remainder) || remainder != "")
    return false;

  if (!game.IsHealthy ())
    throw HttpError (MHD_HTTP_INTERNAL_SERVER_ERROR, "not ok");

  result = SuccessResult ("text/plain", "ok");
  return true;
}

/* ************************************************************************** */

RestClient::RestClient (const std::string& url)
  : endpoint(url)
{
  CHECK_EQ (curl_global_init (CURL_GLOBAL_ALL), 0);
  SetTimeout (REST_CLIENT_DEFAULT_TIMEOUT);
}

namespace
{

/**
 * Sets a given option on the cURL handle.
 */
template <typename T>
  void
  SetCurlOption (CURL* handle, CURLoption option, const T& param)
{
  CHECK_EQ (curl_easy_setopt (handle, option, param), CURLE_OK);
}

} // anonymous namespace

RestClient::Request::Request (const RestClient& c)
  : client(c)
{
  handle = curl_easy_init ();
  CHECK (handle != nullptr);

  /* Let cURL store error messages into our error string.  */
  errBuffer.resize (CURL_ERROR_SIZE);
  SetCurlOption (handle, CURLOPT_ERRORBUFFER, errBuffer.data ());

  if (client.tlsVerification)
    {
      /* Enforce TLS verification.  */
      SetCurlOption (handle, CURLOPT_SSL_VERIFYPEER, 1L);
      SetCurlOption (handle, CURLOPT_SSL_VERIFYHOST, 2L);

      /* Set a CAINFO path if we have an explicit one.  */
      if (client.caFile.empty ())
        {
          LOG_FIRST_N (WARNING, 1) << "Using default cURL CA bundle";
        }
      else
        {
          LOG_FIRST_N (INFO, 1) << "Using CA bundle from " << client.caFile;
          SetCurlOption (handle, CURLOPT_CAINFO, client.caFile.c_str ());
          SetCurlOption (handle, CURLOPT_CAPATH, nullptr);
        }
    }
  else
    {
      LOG_FIRST_N (WARNING, 1) << "TLS verification is disabled";
      SetCurlOption (handle, CURLOPT_SSL_VERIFYPEER, 0L);
      SetCurlOption (handle, CURLOPT_SSL_VERIFYHOST, 0L);
    }

  /* Install our write callback.  */
  SetCurlOption (handle, CURLOPT_WRITEFUNCTION, &WriteCallback);
  SetCurlOption (handle, CURLOPT_WRITEDATA, this);

  /* Make sure we time out instead of waiting indefinitely.  */
  SetCurlOption (handle, CURLOPT_TIMEOUT_MS, client.timeout.count ());
}

size_t
RestClient::Request::WriteCallback (const char* ptr, const size_t sz,
                                    const size_t n, Request* self)
{
  CHECK_EQ (sz, 1);
  self->data.append (ptr, n);
  return n;
}

RestClient::Request::~Request ()
{
  curl_easy_cleanup (handle);
}

std::string
RestClient::Request::UrlEncode (const std::string& str) const
{
  char* ptr = curl_easy_escape (handle, str.data (), str.size ());
  CHECK (ptr != nullptr);
  std::string res(ptr);
  curl_free (ptr);
  return res;
}

bool
RestClient::Request::Send (const std::string& path)
{
  const std::string url = client.endpoint + path;
  VLOG (1) << "Requesting data from " << url << "...";

  data.clear ();
  SetCurlOption (handle, CURLOPT_URL, url.c_str ());

  if (curl_easy_perform (handle) != CURLE_OK)
    {
      LOG (WARNING)
          << "cURL request for " << url << " failed: " << errBuffer.c_str ();
      error << "cURL error: " << errBuffer.c_str ();
      return false;
    }

  long code;
  CHECK_EQ (curl_easy_getinfo (handle, CURLINFO_RESPONSE_CODE, &code),
            CURLE_OK);

  if (code != 200)
    {
      LOG (WARNING)
          << "cURL request for " << url << " returned status: " << code;
      error << "HTTP response status: " << code;
      return false;
    }

  const char* curlType;
  CHECK_EQ (curl_easy_getinfo (handle, CURLINFO_CONTENT_TYPE, &curlType),
            CURLE_OK);
  if (curlType == nullptr)
    {
      LOG (WARNING) << "No content-type received from " << url;
      error << "no content-type sent by server";
      return false;
    }
  type = curlType;

  VLOG (1) << "Request successful, received data of type " << type;
  VLOG (2) << "Return data:\n" << data;

  return ProcessData ();
}

bool
RestClient::Request::ProcessData ()
{
  if (!ProcessGzip ())
    return false;

  if (!ProcessJson ())
    return false;

  return true;
}

bool
RestClient::Request::ProcessGzip ()
{
  const std::string suffix = "+gzip";
  if (type.size () < suffix.size ())
    return true;
  if (type.substr (type.size () - suffix.size (), suffix.size ()) != suffix)
    return true;
  type = type.substr (0, type.size () - suffix.size ());

  std::string uncompressed;
  std::string err;
  if (!GzipInflater ().Uncompress (data, uncompressed, err))
    {
      error << err;
      return false;
    }

  data = std::move (uncompressed);
  return true;
}

bool
RestClient::Request::ProcessJson ()
{
  if (type != "application/json")
    return true;

  Json::CharReaderBuilder rbuilder;
  rbuilder["allowComments"] = false;
  rbuilder["strictRoot"] = true;
  rbuilder["allowDroppedNullPlaceholders"] = false;
  rbuilder["allowNumericKeys"] = false;
  rbuilder["allowSingleQuotes"] = false;
  rbuilder["failIfExtra"] = true;
  rbuilder["rejectDupKeys"] = true;
  rbuilder["allowSpecialFloats"] = false;

  std::string parseErrs;
  std::istringstream in(data);
  try
    {
      if (!Json::parseFromStream (rbuilder, in, &jsonData, &parseErrs))
        {
          LOG (WARNING)
              << "Failed to parse response data as JSON: " << parseErrs
              << "\n" << data;
          error << "JSON parser: " << parseErrs;
          return false;
        }
    }
  catch (const Json::Exception& exc)
    {
      LOG (WARNING)
          << "JSON parser threw: " << exc.what ()
          << "\n" << data;
      error << "JSON parser: " << exc.what ();
      return false;
    }

  return true;
}

/* ************************************************************************** */

} // namespace xaya
