// Copyright (C) 2019-2020 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "base64.hpp"

#include <openssl/evp.h>

#include <glog/logging.h>

#include <cstddef>
#include <sstream>
#include <vector>

namespace xaya
{

std::string
EncodeBase64 (const std::string& data)
{
  /* We need an upper bound on the length of the generated data, so that we
     can reserve a buffer large enough.  The output will be four bytes for
     every three in the input, plus newlines every 64 bytes, plus one NUL at
     the end.  By doubling the input data plus some extra bytes in the case
     of very short input, we are certainly above that.  */
  const size_t bufSize = 3 + 2 * data.size ();

  std::vector<unsigned char> encoded(bufSize, 0);
  const int n
      = EVP_EncodeBlock (encoded.data (),
                         reinterpret_cast<const unsigned char*> (data.data ()),
                         data.size ());
  CHECK_LE (n + 1, bufSize);

  /* Strip out all newline characters from the generated string.  */
  std::ostringstream res;
  for (int i = 0; i < n; ++i)
    if (encoded[i] != '\n')
      res << encoded[i];

  return res.str ();
}

bool
DecodeBase64 (const std::string& encoded, std::string& data)
{
  if (encoded.size () % 4 != 0)
    {
      LOG (ERROR) << "Base64 data has invalid length " << encoded.size ();
      return false;
    }

  /* EVP_DecodeBlock is quite lenient with respect to padding
     characters.  We want strict rules here, namely only accept 0-3
     padding characters at the very end of the input string.  */
  size_t padding = 0;
  for (const char c : encoded)
    {
      if (c != '=')
        {
          if (padding > 0)
            {
              LOG (ERROR) << "Padding in the middle of base64 data";
              return false;
            }
          continue;
        }

      ++padding;
    }
  if (padding >= 4)
    {
      LOG (ERROR) << "Too many padding characters in base64 data";
      return false;
    }

  /* The output data will have a length of three bytes for every four input
     characters.  */
  const size_t outputSize = encoded.size () / 4 * 3;
  data.resize (outputSize);

  const unsigned char* in
      = reinterpret_cast<const unsigned char*> (encoded.data ());
  unsigned char* out = reinterpret_cast<unsigned char*> (&data[0]);
  const int n = EVP_DecodeBlock (out, in, encoded.size ());
  if (n == -1)
    {
      LOG (ERROR) << "OpenSSL base64 decode returned error";
      return false;
    }
  CHECK_EQ (n, outputSize);

  /* Strip the zero padding bytes off the end.  There will be as many padding
     bytes as there were "=" characters at the end of encoded.  */
  if (n == 0)
    CHECK_EQ (padding, 0);
  else
    {
      CHECK_LT (padding, data.size ());
      data.resize (data.size () - padding);
    }

  return true;
}

} // namespace xaya
