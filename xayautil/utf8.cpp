// Copyright (C) 2026 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "utf8.hpp"

#include <cstdint>

namespace xaya
{

namespace
{

/**
 * Decodes the next codepoint from a UTF-8 encoded byte sequence, starting
 * at the given offset.  On success, the offset is advanced past the decoded
 * sequence and true is returned.  If the bytes starting at the offset are
 * not valid UTF-8 (as per RFC 3629), false is returned.
 */
bool
DecodeCodepoint (const std::string& data, size_t& offset)
{
  if (offset >= data.size ())
    return false;

  const uint8_t cur = static_cast<uint8_t> (data[offset]);

  /* Special case for ASCII characters.  */
  if (cur < 0x80)
    {
      offset += 1;
      return true;
    }

  if (cur < 0xC0)
    return false;

  /* Process the sequence-start character.  */
  uint8_t numBytes;
  uint8_t state;
  uint32_t cp;
  if (cur < 0xE0)
    {
      numBytes = 2;
      cp = static_cast<uint32_t> (cur & 0x1F) << 6;
      state = 6;
    }
  else if (cur < 0xF0)
    {
      numBytes = 3;
      cp = static_cast<uint32_t> (cur & 0x0F) << 12;
      state = 12;
    }
  else if (cur < 0xF8)
    {
      numBytes = 4;
      cp = static_cast<uint32_t> (cur & 0x07) << 18;
      state = 18;
    }
  else
    return false;
  offset += 1;

  /* Process the following bytes of this sequence.  */
  while (state > 0)
    {
      if (offset >= data.size ())
        return false;

      const uint8_t next = static_cast<uint8_t> (data[offset]);
      offset += 1;

      if ((next & 0xC0) != 0x80)
        return false;

      state -= 6;
      cp |= static_cast<uint32_t> (next & 0x3F) << state;
    }

  /* Verify that the character we decoded matches the number of bytes
     we had, to prevent overlong sequences.  */
  if (numBytes == 2)
    {
      if (cp < 0x80 || cp >= 0x800)
        return false;
    }
  else if (numBytes == 3)
    {
      if (cp < 0x800 || cp >= 0x10000)
        return false;
    }
  else
    {
      if (cp < 0x10000 || cp >= 0x110000)
        return false;
    }

  /* Prevent characters reserved for UTF-16 surrogate pairs.  */
  if (cp >= 0xD800 && cp <= 0xDFFF)
    return false;

  return true;
}

} // anonymous namespace

bool
IsValidUtf8 (const std::string& data)
{
  size_t offset = 0;
  while (offset < data.size ())
    if (!DecodeCodepoint (data, offset))
      return false;

  return offset == data.size ();
}

} // namespace xaya
