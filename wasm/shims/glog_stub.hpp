/*
 * Stub replacement for glog (Google Logging).
 * Replaces LOG, CHECK, VLOG macros with minimal implementations.
 * Supports the streaming operator (CHECK(x) << "message").
 *
 * IMPORTANT: We intentionally do NOT define severity level constants
 * (INFO=0, WARNING=1, ERROR=2, FATAL=3) as macros.  Doing so would
 * conflict with protobuf's internal logging which uses token pasting
 * (LOGLEVEL_##LEVEL) and expects FATAL to be a literal token, not
 * the number 3.  Our LOG() macro uses token pasting (LOG_##severity)
 * which works without the numeric defines.
 */

#ifndef WASM_SHIMS_GLOG_STUB_HPP
#define WASM_SHIMS_GLOG_STUB_HPP

#include <cstdlib>
#include <ostream>

/* Log message class that optionally aborts on destruction.
   Supports operator<< for streaming (discards output). */
class LogMessage {
  bool fatal_;
public:
  explicit LogMessage(bool fatal = false) : fatal_(fatal) {}
  ~LogMessage() { if (fatal_) std::abort(); }
  template<typename T>
  LogMessage& operator<<(const T&) { return *this; }
  typedef std::basic_ostream<char> ostream_type;
  LogMessage& operator<<(ostream_type& (*)(ostream_type&)) { return *this; }
};

/* LOG(severity) - expands to LOG_INFO, LOG_WARNING, LOG_ERROR, or LOG_FATAL
   via token pasting.  No numeric severity defines needed. */
#define LOG(severity) LOG_##severity
#define LOG_INFO LogMessage(false)
#define LOG_WARNING LogMessage(false)
#define LOG_ERROR LogMessage(false)
#define LOG_FATAL LogMessage(true)

/* LOG_IF(severity, condition) - log only when condition is true. */
#define LOG_IF(severity, cond) if (!(cond)) {} else LOG(severity)

/* LOG_FIRST_N(severity, n) - in the stub, just log unconditionally
   (the "first N" throttling is a no-op since output is discarded). */
#define LOG_FIRST_N(severity, n) LOG(severity)

/* VLOG(level) - verbose logging, always discarded. */
#define VLOG(level) LogMessage(false)

/* CHECK macros - abort on failure, support streaming. */
#define CHECK(cond) LogMessage(!(cond))
#define CHECK_EQ(a, b) LogMessage(!((a) == (b)))
#define CHECK_NE(a, b) LogMessage(!((a) != (b)))
#define CHECK_LT(a, b) LogMessage(!((a) < (b)))
#define CHECK_LE(a, b) LogMessage(!((a) <= (b)))
#define CHECK_GT(a, b) LogMessage(!((a) > (b)))
#define CHECK_GE(a, b) LogMessage(!((a) >= (b)))

#endif // WASM_SHIMS_GLOG_STUB_HPP
