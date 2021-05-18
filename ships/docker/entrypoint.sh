#!/bin/sh -e

exclude="waitforchange,waitforpendingchange"

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
      --game_rpc_port=8600 \
      "$@"
    ;;

  charon-client)
    shift
    exec /usr/local/bin/charon-client \
      --waitforchange --waitforpendingchange \
      --port=8600 \
      --methods_json_spec="/usr/local/share/channel-gsp-rpc.json" \
      --methods_exclude="${exclude}" \
      --cafile="/usr/local/share/letsencrypt.pem" \
      "$@"
    ;;

  charon-server)
    shift
    exec /usr/local/bin/charon-server \
      --waitforchange --waitforpendingchange \
      --methods_json_spec="/usr/local/share/channel-gsp-rpc.json" \
      --methods_exclude="${exclude}" \
      --cafile="/usr/local/share/letsencrypt.pem" \
      "$@"
    ;;

  *)
    echo "Provide \"shipsd\", \"ships-channel\", \"charon-client\" or \"charon-server\" as command"
    exit 1
esac
