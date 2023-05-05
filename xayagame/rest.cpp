// Copyright (C) 2019-2023 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rest.hpp"

#include "xayautil/cryptorand.hpp"

#include <microhttpd.h>

#include <zlib.h>

#include <glog/logging.h>

#include <experimental/filesystem>

#include <cstdio>
#include <cstring>

namespace xaya
{

namespace
{

namespace fs = std::experimental::filesystem;

/** Buffer size for IO with temporary files (to use zlib's gzip interface).  */
constexpr size_t TEMP_BUF_SIZE = 4'096;

/**
 * RAII helper that generates the name for a temporary file and removes the
 * file again on destruction.
 */
class TempFileName
{

private:

  /** The name of the temporary file.  */
  std::string name;

public:

  TempFileName ()
  {
    const uint256 val = CryptoRand ().Get<uint256> ();

    std::ostringstream suffix;
    suffix << "xaya" << val.ToHex ().substr (0, 8);

    name = (fs::temp_directory_path () / suffix.str ()).string ();
    LOG (INFO) << "Filename: " << name;
  }

  ~TempFileName ()
  {
    /* Just optimistically try to delete the file, and ignore any
       potential errors (e.g. because the file hasn't actually been
       created by the calling code).  */
    std::remove (name.c_str ());
  }

  const std::string&
  GetName () const
  {
    return name;
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

  /* Unfortunately, it seems zlib supports the gzip format mainly through
     a wrapper around files / file descriptors.  Thus we can't just encode
     a buffer in gzip format, but have to serve it through such a descriptor.
     We use a temporary file for this.  */
  TempFileName file;

  {
    gzFile gz = gzopen (file.GetName ().c_str (), "wb9");
    CHECK (gz != nullptr);

    const char* data = payload.data ();
    size_t len = payload.size ();
    while (len > 0)
      {
        const int n = gzwrite (gz, data, len);
        CHECK_GT (n, 0) << "gzwrite failed";
        CHECK_LE (n, len);
        data += n;
        len -= n;
      }

    CHECK_EQ (gzclose (gz), Z_OK);
  }

  {
    FILE* f = std::fopen (file.GetName ().c_str (), "rb");
    CHECK (f != nullptr)
        << "Failed to open " << file.GetName () << " for reading: "
        << std::strerror (errno);

    while (true)
      {
        char buf[TEMP_BUF_SIZE];
        const int n = std::fread (buf, 1, TEMP_BUF_SIZE, f);
        CHECK_GE (n, 0);
        res.payload.append (buf, n);

        if (std::feof (f))
          break;
        CHECK_EQ (n, TEMP_BUF_SIZE);
      }

    CHECK_EQ (std::fclose (f), 0);
  }

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

  /* As before, we use a temporary for zlib's gzip interface.  */
  TempFileName file;

  {
    FILE* f = std::fopen (file.GetName ().c_str (), "wb");
    CHECK (f != nullptr)
        << "Failed to open " << file.GetName () << " for writing: "
        << std::strerror (errno);

    const int n = std::fwrite (data.data (), 1, data.size (), f);
    CHECK_EQ (n, data.size ());

    CHECK_EQ (std::fclose (f), 0);
  }

  {
    gzFile gz = gzopen (file.GetName ().c_str (), "rb");
    CHECK (gz != nullptr);

    std::string uncompressed;
    char buf[TEMP_BUF_SIZE];
    while (true)
      {
        const int n = gzread (gz, buf, TEMP_BUF_SIZE);
        if (n == -1)
          {
            int err;
            error << "gzip error: " << gzerror (gz, &err);
            error << " (code " << err << ")";

            gzclose (gz);
            return false;
          }
        CHECK_GE (n, 0);

        uncompressed.append (buf, n);
        if (n == 0)
          break;
      }

    const int rc = gzclose (gz);
    if (rc == Z_BUF_ERROR)
      {
        error << "incomplete gzip stream";
        return false;
      }
    CHECK_EQ (rc, Z_OK);

    data = std::move (uncompressed);
  }

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
