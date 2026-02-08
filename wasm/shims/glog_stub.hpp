/*
 * Stub replacement for glog (Google Logging).
 * Replaces LOG, CHECK, VLOG macros with minimal implementations.
 * Supports the streaming operator (CHECK(x) << "message").
 */

#ifndef WASM_SHIMS_GLOG_STUB_HPP
#define WASM_SHIMS_GLOG_STUB_HPP

#include <cstdlib>
#include <ostream>

// Severity levels (matching glog)
#define INFO 0
#define WARNING 1
#define ERROR 2
#define FATAL 3

// Log message class that optionally aborts on destruction.
// Supports operator<< for streaming (discards output).
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

// LOG macro
#define LOG(severity) LOG_##severity
#define LOG_INFO LogMessage(false)
#define LOG_WARNING LogMessage(false)
#define LOG_ERROR LogMessage(false)
#define LOG_FATAL LogMessage(true)

// LOG_IF macro
#define LOG_IF(severity, cond) if (!(cond)) {} else LOG(severity)

// VLOG macro - discard
#define VLOG(level) LogMessage(false)

// CHECK macros - abort on failure, support streaming
#define CHECK(cond) LogMessage(!(cond))
#define CHECK_EQ(a, b) LogMessage(!((a) == (b)))
#define CHECK_NE(a, b) LogMessage(!((a) != (b)))
#define CHECK_LT(a, b) LogMessage(!((a) < (b)))
#define CHECK_LE(a, b) LogMessage(!((a) <= (b)))
#define CHECK_GT(a, b) LogMessage(!((a) > (b)))
#define CHECK_GE(a, b) LogMessage(!((a) >= (b)))

#endif // WASM_SHIMS_GLOG_STUB_HPP
