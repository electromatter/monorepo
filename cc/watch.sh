#!/bin/bash

trap : TERM # INT

cd "$(dirname "$0")"

# Create event pipe
if ! [ -p ~/.post ]; then
	mkfifo ~/.post
fi

# Child process to wait for events
(
	trap : INT

	# Wait for timeout
	sleep 1
	kill -INT 0

	# Wait for someone to post an event
	: < ~/.post
	kill -TERM 0
) &

# Run build with pager
"$@" 2>&1 | less -Rc; rc=$?
stty sane

# Kill event process
kill -TERM 0
wait

# Pager exited from our TERM
if [ ${rc} -ne 15 ] && [ ${rc} -ne 143 ]; then
	exit 0
fi

# Build again
exec "$0" "$@"
