#!/bin/bash

set -ue

# Environmental variables used to compile the bootstrap
: ${CC:=gcc}
: ${CFLAGS:=-Os -Wall -Wextra -Wno-unused -pedantic -std=c99}

# First compile the bootstrap
${CC} ${CFLAGS} -o cc0 ./cc0.c || exit 1

# Then use the bootstrap to compile the compiler
timeout 1 sh -c ./cc0 < cc1.c > cc1 || { echo "cc0 failed"; exit 1; }
chmod +x cc1

# Then compile the compiler with itself
timeout 1 sh -c ./cc1 < cc1.c > cc2 || { echo "cc1 failed"; exit 1; }
chmod +x cc2

# And check our work
diff <(xxd cc1) <(xxd cc2) || :
cmp cc1 cc2 || { echo "output mismatch"; exit 1; }

exit 0
