/*
 * Minimal stub for jsoncpp's Json::Value.
 * Only provides enough to compile the C++ game code;
 * ToJson() methods return empty values (display is handled by React).
 */

#ifndef WASM_SHIMS_JSON_STUB_HPP
#define WASM_SHIMS_JSON_STUB_HPP

#include <string>
#include <cstdint>

namespace Json
{

// Value type enum matching jsoncpp's ValueType
enum ValueType
{
  nullValue = 0,
  intValue,
  uintValue,
  realValue,
  stringValue,
  booleanValue,
  arrayValue,
  objectValue
};

using Int64 = int64_t;
using UInt64 = uint64_t;

class Value
{
public:
  Value () = default;
  Value (ValueType) {}
  Value (const std::string&) {}
  Value (int) {}
  Value (unsigned) {}
  Value (int64_t) {}
  Value (uint64_t) {}
  Value (bool) {}
  Value (const char*) {}
  Value (double) {}

  // Array/object operations (no-op)
  Value& operator[] (const std::string&) { static Value v; return v; }
  Value& operator[] (const char*) { static Value v; return v; }
  Value& operator[] (int) { static Value v; return v; }
  Value& operator[] (unsigned) { static Value v; return v; }

  Value& append (const Value&) { return *this; }

  bool isNull () const { return true; }
  bool isObject () const { return false; }
  bool isArray () const { return false; }
  bool isString () const { return false; }
  bool isUInt () const { return false; }
  bool isInt () const { return false; }
  std::string asString () const { return ""; }
  int asInt () const { return 0; }
  unsigned asUInt () const { return 0; }
  int64_t asInt64 () const { return 0; }
  uint64_t asUInt64 () const { return 0; }
  bool asBool () const { return false; }
  double asDouble () const { return 0.0; }

  unsigned size () const { return 0; }
  bool empty () const { return true; }
  bool isMember (const std::string&) const { return false; }

  // Iteration support (no-op)
  const Value* begin () const { return nullptr; }
  const Value* end () const { return nullptr; }

  // Comparison
  bool operator== (const Value&) const { return true; }
  bool operator!= (const Value&) const { return false; }
};

// StreamWriterBuilder stub
class StreamWriterBuilder
{
public:
  StreamWriterBuilder () = default;
  Value& operator[] (const std::string&) { static Value v; return v; }
};

// writeString stub
inline std::string writeString (const StreamWriterBuilder&, const Value&)
{
  return "{}";
}

} // namespace Json

#endif // WASM_SHIMS_JSON_STUB_HPP
