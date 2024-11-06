// Copyright (C) 2023-2024 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coprocessor.hpp"

namespace xaya
{

void
CoprocessorBatch::Add (const std::string& name, Coprocessor& p)
{
  CHECK (processors.emplace (name, &p).second)
      << "We already had a processor of name '" << name << "'";
  LOG (INFO) << "Added coprocessor '" << name << "'";

  if (activeTransaction)
    p.BeginTransaction ();
}

void
CoprocessorBatch::BeginTransaction ()
{
  CHECK (!activeTransaction) << "There is already an active transaction";
  activeTransaction = true;

  for (auto& p : processors)
    p.second->BeginTransaction ();
}

void
CoprocessorBatch::CommitTransaction ()
{
  CHECK (activeTransaction) << "There is no active transaction";
  activeTransaction = false;

  for (auto& p : processors)
    p.second->CommitTransaction ();
}

void
CoprocessorBatch::AbortTransaction ()
{
  CHECK (activeTransaction) << "There is no active transaction";
  activeTransaction = false;

  for (auto& p : processors)
    p.second->AbortTransaction ();
}

Coprocessor::Block::Block (const Json::Value& d, const Op o)
  : blockData(d), op(o)
{
  CHECK (blockData.isObject ()) << "Invalid block data:\n" << blockData;
  CHECK (hash.FromHex (blockData["hash"].asString ()))
      << "Invalid block data:\n" << blockData;

  const auto& heightVal = blockData["height"];
  CHECK (heightVal.isUInt64 ()) << "Invalid block data:\n" << blockData;
  height = heightVal.asUInt64 ();
}

CoprocessorBatch::Block::Block (CoprocessorBatch& batch,
                                const Json::Value& blockData,
                                const Coprocessor::Op op)
{
  for (auto& entry : batch.processors)
    blocks.emplace (entry.first, entry.second->ForBlock (blockData, op));
  CHECK_EQ (blocks.size (), batch.processors.size ())
      << "Duplicate coprocessor names while constructing batch";
}

void
CoprocessorBatch::Block::Start ()
{
  for (auto& entry : blocks)
    entry.second->Start ();
}

void
CoprocessorBatch::Block::Finish ()
{
  for (auto& entry : blocks)
    entry.second->Finish ();
}

} // namespace xaya
