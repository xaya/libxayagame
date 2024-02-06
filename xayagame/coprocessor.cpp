// Copyright (C) 2023 The Xaya developers
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

CoprocessorBatch::Block::~Block ()
{
  if (!committed)
    for (auto& entry : blocks)
      if (started.count (entry.first) > 0)
        entry.second->Abort ();
}

void
CoprocessorBatch::Block::Begin ()
{
  for (auto& entry : blocks)
    {
      entry.second->Begin ();
      started.insert (entry.first);
    }
}

void
CoprocessorBatch::Block::Commit ()
{
  CHECK (!committed) << "CoprocessorBatch::Block is already committed";
  committed = true;
  for (auto& entry : blocks)
    {
      CHECK_EQ (started.count (entry.first), 1)
          << "Commit() called without Begin()";
      entry.second->Commit ();
    }
}

} // namespace xaya
