// Copyright (C) 2022 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config.h"

#include "xayagame/sqliteintro.hpp"
#include "xayagame/sqlitestorage.hpp"

#include <xayautil/hash.hpp>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <cstdlib>
#include <iostream>

namespace
{

DEFINE_string (db, "",
               "File of the SQLite database to open");
DEFINE_string (table, "",
               "If set, only the given table is dumped");
DEFINE_bool (sha256, false, "If true, hash the output with SHA-256");

/**
 * Runs the main dumping on the given output stream.
 */
template <typename Out>
  void
  Run (Out& s, const xaya::SQLiteDatabase& db)
{
  if (FLAGS_table.empty ())
    WriteAllTables (s, db);
  else
    WriteTableContent (s, db, FLAGS_table);
}

} // anonymous namespace

int
main (int argc, char** argv)
{
  google::InitGoogleLogging (argv[0]);

  gflags::SetUsageMessage ("Dumps and hashes SQLite databases");
  gflags::SetVersionString (PACKAGE_VERSION);
  gflags::ParseCommandLineFlags (&argc, &argv, true);

  if (FLAGS_db.empty ())
    {
      std::cerr << "Error: --db must be set" << std::endl;
      return EXIT_FAILURE;
    }

  xaya::SQLiteDatabase db(FLAGS_db, SQLITE_OPEN_READONLY);

  if (FLAGS_sha256)
    {
      xaya::SHA256 hasher;
      Run (hasher, db);
      std::cout << hasher.Finalise ().ToHex () << std::endl;
    }
  else
    Run (std::cout, db);

  return EXIT_SUCCESS;
}
