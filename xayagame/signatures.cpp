// Copyright (C) 2019 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "signatures.hpp"

#include <glog/logging.h>

#include <json/json.h>

namespace xaya
{

std::string
VerifyMessage (XayaRpcClient& rpc,
               const std::string& msg, const std::string& sgn)
{
  const Json::Value res = rpc.verifymessage ("", msg, sgn);
  CHECK (res.isObject ());

  if (!res["valid"].asBool ())
    return "invalid";

  return res["address"].asString ();
}

} // namespace xaya
