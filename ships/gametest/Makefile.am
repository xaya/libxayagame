AM_TESTS_ENVIRONMENT = \
  PYTHONPATH=$(top_srcdir):$(top_srcdir)/ships

TEST_LIBRARY = shipstest.py

REGTESTS = \
  channel_management.py \
  disputes.py \
  reorg.py \
  winner_statement.py

EXTRA_DIST = $(REGTESTS) $(TEST_LIBRARY)
TESTS = $(REGTESTS)
