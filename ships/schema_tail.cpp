)";

} // anonymous namespace

void
SetupShipsSchema (xaya::SQLiteDatabase& db)
{
  LOG (INFO) << "Setting up the database schema for xayaships...";
  db.Execute (SCHEMA_SQL);
}

} // namespace ships
