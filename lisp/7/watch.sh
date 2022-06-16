#!/bin/bash
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

eval "$(sed -e '/\\$/N;/[^\t]/s/#.*$//g;' << 'EOF' | awk -v "self=$0" '
BEGIN {
	n = 0
}

recipe && /^[\t]/ {
	cmd = $0
	gsub(/'\''/, "'\'\\\\\'\''", cmd)
	print "echo '\''" cmd "'\'' >&2"
	print $0 " || return 1"
}

/^[^\t]/ {
	if (recipe) {
		print "}"
	}

	split($0, parts, /:/)
	split(parts[1], objects, /[ \t]/)
	split(parts[2], sources, /[ \t]/)

	object = objects[1]

	recipe = "recipe" n
	n += 1

	recipes[object] = recipe

	svar = ""
	for (i in sources) {
		source = sources[i]
		if (!source) {
			continue;
		}
		svar = svar " " source
		depends[object][i] = source
		print sources[i] " " object |& "tsort"
	}

	print recipe "() {"
	print "object=" object
	print "source=(" svar " )"
	print "echo \"${object}: ${source[@]}\" >&2"
}

END {
	if (recipe) {
		print "}"
	}

	close("tsort", "to")
	norder = 0
	while (("tsort" |& getline) > 0) {
		order[norder++] = $0
	}

	print "build() {"
	for (i in order) {
		object = order[i]
		recipe = recipes[object]
		if (recipe) {
			test = "[ \"$0\" -ot " object " ]"
			for (i in depends[object]) {
				source = depends[object][i]
				test = test " && [ " source " -ot " object " ]"
			}
			print test " || " recipe " || { echo " object " \"recipe failed\" >&2; return 1; }"
		} else {
			print "[ -e " object " ] || { echo " object " \"does not exist\" >&2; return 1; }"
		}
	}
	print "echo \"build successful!\" >&2"
	print "}"

	svar = ""
	rvar = ""
	print "query() {"
	for (i in order) {
		object = order[i]
		recipe = recipes[object]
		if (recipe) {
			test = ":"
			for (j in depends[object]) {
				source = depends[object][j]
				svar = svar " " source
				test = test " && [ " source " -ot " object " ]"
			}
			print test " || return 1"
		} else {
			rvar = rvar " " object
		}
	}
	print "}"
	print "SOURCES=(" svar " )"
	print "ROOTS=( \"$0\"" rvar " )"
}
'

# Build the C interpreter
u.o: u.c
	gcc -Wall -Wextra -pedantic -std=c99 -O1 -g -o "${object}" "${source}"

# Use the C interpreter to build the first generation
u.o.o: u.u u.o
	./"${source[1]}" > "${object}" < "${source}"
	chmod +x "${object}"

# Build a second generation and compare
u.o.o.o: u.u u.o.o
	./"${source[1]}" > "${object}" < "${source}"
	cmp "${object}" "${source[1]}"
EOF
)"

newest() {
	find "${ROOTS[@]}" -printf '%Ts\n' | sort -n -r | head -n1
}

main() {
	trap ':' TERM INT

	(
		trap '-' TERM
		stamp="$(newest)"
		while [ "${stamp}" -ge "$(newest)" ]; do
			sleep 1
		done
		kill -TERM 0
	) & watch=$!

	(
		trap '-' TERM
		sleep 1
		kill -INT 0
	) &

	build 2>&1 | less +F

	if ! kill -0 "${watch}" 2> /dev/null; then
		kill -TERM 0
		wait
		stty sane
		exec "$0"
	fi

	kill -TERM 0
	wait
	stty sane
}

main "$@"
