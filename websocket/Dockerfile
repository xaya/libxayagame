# Builds a Docker image that has the GSP websocket server installed
# and ready to run.

FROM alpine
RUN apk add --no-cache \
  python3 \
  py3-pip

ENV VIRTUAL_ENV=/opt/venv
RUN python3 -m venv "${VIRTUAL_ENV}"
ENV PATH="${VIRTUAL_ENV}/bin:${PATH}"

COPY websocket/requirements.txt /tmp/requirements.txt
RUN pip install --no-cache-dir -r /tmp/requirements.txt

COPY websocket/gsp-websocket-server.py /usr/local/bin/

RUN addgroup -S runner && adduser -S runner -G runner
USER runner

LABEL description="Image with the GSP websocket server script"
ENTRYPOINT ["/usr/local/bin/gsp-websocket-server.py"]
