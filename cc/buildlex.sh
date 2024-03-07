#!/bin/bash

set -ue

: ${CC:=./cc2}

${CC} < lex.c > lex
chmod +x lex

./lex < cc3.l
