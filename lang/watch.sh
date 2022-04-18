#!/bin/sh
################################################################################
# Copyright (c) 2022 Eric Chai <electromatter@gmail.com>                       #
#                                                                              #
# Permission to use, copy, modify, and/or distribute this software for any     #
# purpose with or without fee is hereby granted, provided that the above       #
# copyright notice and this permission notice appear in all copies.            #
#                                                                              #
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES     #
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF             #
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR      #
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES       #
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER              #
# IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING       #
# OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.        #
################################################################################

set -ue

: "${PAGER:="less +F"}"  # less with follow scrolling
: "${SIGNAL:=10}"        # SIGUSR1
: "${TIMEOUT:=1}"        # build timeout
: "${DELAY:=1}"          # rebuild poll delay

# Let the pager handle INT and SIGNAL.
trap ':' INT "${SIGNAL}"

while :; do
    {
        # Start timestamp
        TIMESTAMP="$(./build.sh -q)"

        # Run build.sh with a timeout with the pager.
        (time timeout "${TIMEOUT}" ./build.sh "$@") 2>&1
        printf "; build exited ($?)\n"

        # Only page output from the build.
        exec > /dev/null 2>&1

        # Poll for changes after the build finishes.
        while :; do
            sleep "${DELAY}" || :
            if [ "$(./build.sh -q)" -ne "${TIMESTAMP}" ]; then
                kill "-${SIGNAL}" 0
            fi
        done &
    } | ${PAGER} && STATUS=$? || STATUS=$?

    # Exit if the pager failed wasn't killed by our signal.
    if [ "${STATUS}" -ne $(( 128 + SIGNAL )) ]; then
        # Kill the polling loop if it's still around.
        kill -"${SIGNAL}" 0

        # Clean up the terminal state just in case.
        stty sane

        # Pass along the status code.
        exit "${STATUS}"
    fi
done
