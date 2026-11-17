# Websocket Notification Server

This directory contains a simple Websocket server which can connect to
a GSP built with libxayagame.  It uses the `waitforchange` RPC method
to poll the GSP for updates, and pushes notifications to all connected
clients whenever a new best block is found.

Install the dependencies from `requirements.txt` before running the server:

```
pip3 install -r requirements.txt
```
