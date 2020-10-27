)";

} // anonymous namespace

void
InternalSetupGameChannelsSchema (SQLiteDatabase& db)
{
  LOG (INFO) << "Setting up the database schema for game channels...";
  db.Execute (SCHEMA_SQL);
}

} // namespace xaya
