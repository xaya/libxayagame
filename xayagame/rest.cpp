// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rest.hpp"

#include <microhttpd.h>

#include <zlib.h>

#include <glog/logging.h>

#include <unistd.h>

#include <thread>

namespace xaya
{

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
     We use a local pipe to get around this.  */

  int fd[2];
  CHECK_EQ (pipe (fd), 0);

  std::thread writer([&] ()
    {
      gzFile gz = gzdopen (fd[1], "wb9");

      const char* buf = payload.data ();
      size_t len = payload.size ();
      while (len > 0)
        {
          const int n = gzwrite (gz, buf, len);
          CHECK_GT (n, 0) << "gzwrite failed";
          CHECK_LE (n, len);
          buf += n;
          len -= n;
        }

      CHECK_EQ (gzclose (gz), Z_OK);
    });

  static constexpr size_t BUF_SIZE = 4'096;
  char buf[BUF_SIZE];
  while (true)
    {
      const int n = read (fd[0], buf, BUF_SIZE);
      CHECK_GE (n, 0);
      if (n == 0)
        break;

      res.payload += std::string (buf, n);
    }

  writer.join ();
  CHECK_EQ (close (fd[0]), 0);

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
  int
  Queue (struct MHD_Connection* conn)
  {
    CHECK (resp != nullptr);
    return MHD_queue_response (conn, code, resp);
  }

};

} // anonymous namespace

int
RestApi::RequestCallback (void* data, struct MHD_Connection* conn,
                          const char* url, const char* method,
                          const char* version,
                          const char* upload, size_t* uploadSize,
                          void** connData)
{
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
                             &RequestCallback, this, MHD_OPTION_END);
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

/* ************************************************************************** */

} // namespace xaya
