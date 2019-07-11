// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/* Template implementation code for protoutils.hpp.  */

#include <xayautil/base64.hpp>

#include <glog/logging.h>

namespace xaya
{

template <typename Proto>
  std::string
  ProtoToBase64 (const Proto& msg)
{
  std::string serialised;
  CHECK (msg.SerializeToString (&serialised));
  return EncodeBase64 (serialised);
}

template <typename Proto>
  bool
  ProtoFromBase64 (const std::string& str, Proto& msg)
{
  std::string bytes;
  if (!DecodeBase64 (str, bytes))
    {
      LOG (ERROR) << "Invalid base64: " << str;
      return false;
    }

  if (!msg.ParseFromString (bytes))
    {
      LOG (ERROR) << "Failed to parse protocol buffer from decoded string";
      return false;
    }

  return true;
}

} // namespace xaya
