#!/bin/sh -e

case $1 in
  shipsd)
    shift
    exec /usr/local/bin/shipsd \
      --datadir="${XAYAGAME_DIR}" \
      --enable_pruning=1000 \
      --game_rpc_port=8600 \
      "$@"
    ;;

  ships-channel)
    shift
    exec /usr/local/bin/ships-channel \
      --rpc_port=8600 \
      "$@"
    ;;

  *)
    echo "Provide \"shipsd\" or \"ships-channel\" as command"
    exit 1
esac
