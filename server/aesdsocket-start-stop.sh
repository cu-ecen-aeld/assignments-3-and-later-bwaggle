#!/bin/bash

DAEMON_NAME="aesdsocket"
DAEMON_PATH="/usr/bin/$DAEMON_NAME"
DAEMON_USER="bwaggle"  # Replace "your_username" with the username of the user who owns the daemon

start() {
    if start-stop-daemon --start --oknodo --background --chuid "$DAEMON_USER" --exec "$DAEMON_PATH"; then
        echo "Daemon $DAEMON_NAME started."
    else
        echo "Failed to start daemon $DAEMON_NAME."
    fi
}

stop() {
    if start-stop-daemon --stop --oknodo --retry 5 --quiet --exec "$DAEMON_PATH"; then
        echo "Daemon $DAEMON_NAME stopped."
    else
        echo "Failed to stop daemon $DAEMON_NAME."
    fi
}

case "$1" in
    start)
        start
        ;;
    stop)
        stop
        ;;
    restart)
        stop
        sleep 1
        start
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac

exit 0
