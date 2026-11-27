// Copyright (C) 2023-2026 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "coprocessor.hpp"

namespace xaya
{

void
CoprocessorBatch::Add (const std::string& name, Coprocessor& p)
{
  /* Call BeginTransaction before adding it; in case that throws,
     we don't add it but leave the existing batch alone in a
     consistent state.  */
  if (activeTransaction)
    p.BeginTransaction ();

  CHECK (processors.emplace (name, &p).second)
      << "We already had a processor of name '" << name << "'";
  LOG (INFO) << "Added coprocessor '" << name << "'";
}

void
CoprocessorBatch::BeginTransaction ()
{
  CHECK (!activeTransaction) << "There is already an active transaction";

  std::set<std::string> started;
  try
    {
      for (auto& p : processors)
        {
          p.second->BeginTransaction ();
          started.insert (p.first);
        }
    }
  catch (...)
    {
      /* On error, abort all the transactions already started.  */
      for (auto& p : processors)
        if (started.find (p.first) != started.end ())
          p.second->AbortTransaction ();
      throw;
    }

  /* If all were successful, mark as having an active transaction.  */
  activeTransaction = true;
}

void
CoprocessorBatch::CommitTransaction ()
{
  CHECK (activeTransaction) << "There is no active transaction";

  activeTransaction = false;

  std::set<std::string> committed;
  try
    {
      for (auto& p : processors)
        {
          p.second->CommitTransaction ();
          committed.insert (p.first);
        }
    }
  catch (...)
    {
      /* If one of the commits throws, we want to abort all the others.  */
      for (auto& p : processors)
        if (committed.find (p.first) == committed.end ())
          p.second->AbortTransaction ();
      throw;
    }
}

void
CoprocessorBatch::AbortTransaction ()
{
  if (!activeTransaction)
    return;

  activeTransaction = false;

  for (auto& p : processors)
    p.second->AbortTransaction ();
}

void
CoprocessorBatch::Clear ()
{
  CHECK (!activeTransaction) << "There is an active transaction";
  for (auto& p : processors)
    p.second->Clear ();
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
