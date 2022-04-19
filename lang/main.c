/******************************************************************************
 * Copyright (c) 2022 Eric Chai <electromatter@gmail.com>                     *
 *                                                                            *
 * Permission to use, copy, modify, and/or distribute this software for any   *
 * purpose with or without fee is hereby granted, provided that the above     *
 * copyright notice and this permission notice appear in all copies.          *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES   *
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF           *
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR    *
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES     *
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER            *
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING     *
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.      *
 ******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>


void die(const char *why)
{
    fflush(stdout);
    fprintf(stderr, "die: %s\n", why);
    fflush(stderr);
    exit(1);
}


int lisp_istokenchar(int ch) {
    switch (ch) {
    case '!': case '$': case '%': case '&':
    case '*': case '+': case '-': case '/':
    case ':': case '<': case '=': case '>':
    case '?': case '@': case '^': case '_':
    case '~':
        return 1;
    default:
        if (ch >= '0' && ch <= '9') {
            return 1;
        }
        if (ch >= 'a' && ch <= 'z') {
            return 1;
        }
        if (ch >= 'A' && ch <= 'Z') {
            return 1;
        }
        return 0;
    }
}

int lisp_read(void)
{
    int ch;
    int depth;
    int expect;

    depth = 0;
    expect = 0;
    while (1) {
        ch = getc(stdin);

        switch (ch) {
        case EOF:
            if (expect != 0) {
                die("EXPECTED FORM GOT END OF FILE");
            }
            if (depth != 0) {
                die("EXPECTED ) GOT END OF FILE");
            }
            return 0;

        case ' ': case '\t': case '\v': case '\f': case '\r': case '\n':
            break;

        case ';':
            while (ch != EOF && ch != '\n') {
                ch = getc(stdin);
            }
            break;

        case '(':
            expect = 0;
            depth += 1;
            break;

        case ')':
            if (expect != 0) {
                die("EXPECTED FORM GOT END OF LIST");
            }
            if (depth == 0) {
                die("EXTRA CLOSING PARENTHESIS");
            }
            depth -= 1;
            if (depth == 0) {
                return 1;
            }
            break;

        case '\'':
            expect = 1;
            break;

        case '`':
            expect = 1;
            break;

        case ',':
            ch = getc(stdin);
            if (ch != '@') {
                ungetc(ch, stdin);
            }
            expect = 1;
            break;

        case '"':
            expect = 0;
            while (1) {
                ch = getc(stdin);
                if (ch == '"') {
                    break;
                }
                if (ch == '\\') {
                    ch = getc(stdin);
                }
                if (ch == EOF) {
                    die("EXPECTED \" GOT END OF FILE");
                }
            }
            if (depth == 0) {
                return 1;
            }
            break;

        default:
            expect = 0;
            if (!lisp_istokenchar(ch)) {
                fprintf(stderr, "INVALID CHARACTER: %c\n", ch);
                die("INVALID CHARACTER");
            }
            while (lisp_istokenchar(ch)) {
                ch = getc(stdin);
            }
            ungetc(ch, stdin);
            if (depth == 0) {
                return 1;
            }
            break;
        }
    }
}


int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    while (lisp_read()) {
        printf("TOP-LEVEL\n");
    }

    printf("Hello, world!\n");
    return 0;
}
