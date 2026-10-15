# Builds a minimal Docker image with the Xayaships binaries (shipsd and
# ships-channel) and their runtime libraries.
#
# It is based on the libxayagame image directly, and uses its cpld helper
# to collect the runtime libraries:  locally built libraries are copied,
# while libraries owned by a Debian package are installed via apt in the
# final runtime image.

FROM xaya/libxayagame AS build

# Collect the binaries and required runtime libraries.  cpld copies the
# locally built libraries and records the Debian packages needed for the
# rest in "packages.txt".
WORKDIR /jail
RUN mkdir bin \
  && cp /usr/local/bin/shipsd /usr/local/bin/ships-channel bin/ \
  && touch packages.txt \
  && for b in bin/*; do cpld "$b" lib64 packages.txt; done \
  && sort -u -o packages.txt packages.txt

COPY ships/docker/entrypoint.sh bin/

# Build the final, minimal runtime image based on Debian, installing the
# packages reported by cpld for the libraries we did not copy ourselves.
FROM debian:13-slim
COPY --from=build /jail /usr/local/
RUN apt -y update \
  && apt -y install --no-install-recommends $(cat /usr/local/packages.txt) \
  && rm -f /usr/local/packages.txt \
  && rm -rf /var/lib/apt/lists/*
ENV LD_LIBRARY_PATH "/usr/local/lib64"
ENV XAYAGAME_DIR "/xayagame"
LABEL description="Xayaships GSP"
VOLUME ["/log"]
ENV GLOG_log_dir "/log"
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
