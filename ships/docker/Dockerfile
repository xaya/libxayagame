# Builds a Docker image that contains the Xaya ships binaries as well as
# Charon binaries for running the Xayaships GSP client/server for
# light mode.

# Note:  This image is based on xaya/charon, which itself is based on
# xaya/libxayagame, not on the libxayagame image directly!

FROM xaya/charon:ships-on-eth AS build

# Collect the binaries and required libraries.  Also add in the Charon
# binaries.
WORKDIR /jail
RUN mkdir bin && cp /usr/local/bin/ships* /usr/local/bin/charon-* bin/
RUN for b in bin/*; do cpld $b lib64; done

# Add extra files needed (like the channel GSP method spec loaded with the
# Charon binaries).
RUN mkdir share \
  && cp /usr/local/share/channel-gsp-rpc.json share/ \
  && cp /usr/local/share/charon/letsencrypt.pem share/
COPY ships/docker/entrypoint.sh bin/

# Build the final image.
FROM alpine
COPY --from=build /jail /usr/local/
ENV LD_LIBRARY_PATH "/usr/local/lib64"
ENV XAYAGAME_DIR "/xayagame"
LABEL description="Xayaships GSP with Charon"
VOLUME ["/log"]
ENV GLOG_log_dir "/log"
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
