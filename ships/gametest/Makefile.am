AM_TESTS_ENVIRONMENT = \
  PYTHONPATH=$(top_srcdir):$(top_srcdir)/ships:$(PYTHONPATH)

TEST_LIBRARY = shipstest.py

REGTESTS = \
  channel_management.py \
  disputes.py \
  force_close.py \
  getchannel.py \
  multi_moves.py \
  pending.py \
  reorg.py

EXTRA_DIST = $(REGTESTS) $(TEST_LIBRARY)
TESTS = $(REGTESTS)
