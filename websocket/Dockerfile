# Builds a Docker image that has the GSP websocket server installed
# and ready to run.

FROM alpine
RUN apk add --no-cache \
  python3 \
  py3-jsonrpclib \
  py3-pip

COPY websocket/gsp-websocket-server.py /usr/local/bin/

RUN addgroup -S runner && adduser -S runner -G runner
USER runner

RUN pip3 install websocket-server

LABEL description="Image with the GSP websocket server script"
ENTRYPOINT ["/usr/local/bin/gsp-websocket-server.py"]
