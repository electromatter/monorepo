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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>


void die(char *why)
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


enum tag {
    TAG_NIL,
    TAG_CONS,
    TAG_STRING,
    TAG_SYMBOL,
    TAG_FIXNUM
};


struct obhead {
    int tag;
    union object *next;
    union object *back;
};


struct cons {
    struct obhead head;
    union object *car;
    union object *cdr;
};


struct string {
    struct obhead head;
    unsigned char *value;
    unsigned int length;
};


struct symbol {
    struct obhead head;
    union object *name;
};


struct fixnum {
    struct obhead head;
    int value;
};


union object {
    struct obhead head;
    struct cons cons;
    struct string string;
    struct symbol symbol;
    struct fixnum fixnum;
};


void free_object(union object *object) {
    if (object == NULL) {
        return;
    }
    switch (object->head.tag) {
    case TAG_NIL:
        break;
    case TAG_CONS:
        free(object);
        break;
    case TAG_STRING:
        free(object->string.value);
        free(object);
        break;
    case TAG_SYMBOL:
        free(object);
        break;
    case TAG_FIXNUM:
        free(object);
        break;
    default:
        die("INVALID TAG");
    }
}


union object *makenil(void) {
    static union object ob = {{TAG_NIL}};
    return &ob;
}


union object *makecons(union object *car, union object *cdr) {
    struct cons *ret;
    ret = malloc(sizeof(*ret));
    if (ret == NULL) {
        die("OUT OF MEMORY");
    }
    ret->head.tag = TAG_CONS;
    ret->car = car;
    ret->cdr = cdr;
    return (union object *)ret;
}


union object *makestring(unsigned char *value, unsigned int length) {
    unsigned char *buf;
    struct string *ret;
    ret = malloc(sizeof(*ret));
    if (ret == NULL) {
        die("OUT OF MEMORY");
    }
    buf = malloc(length);
    if (buf == NULL) {
        die("OUT OF MEMORY");
    }
    memcpy(buf, value, length);
    ret->head.tag = TAG_STRING;
    ret->value = buf;
    ret->length = length;
    return (union object *)ret;
}


union object *makesymbol(unsigned char *name, unsigned int length) {
    struct symbol *ret;
    if (length == 3 && memcmp(name, "nil", 3) == 0) {
        return makenil();
    }
    ret = malloc(sizeof(*ret));
    if (ret == NULL) {
        die("OUT OF MEMORY");
    }
    ret->head.tag = TAG_SYMBOL;
    ret->name = makestring(name, length);
    return (union object *)ret;
}


union object *makefixnum(int value) {
    struct fixnum *ret;
    ret = malloc(sizeof(*ret));
    if (ret == NULL) {
        die("OUT OF MEMORY");
    }
    ret->head.tag = TAG_FIXNUM;
    ret->value = value;
    return (union object *)ret;
}


union object *parsetoken(unsigned char *maybeint, unsigned int length) {
    long int value;
    unsigned int i = 0;
    char buffer[32];
    if ((maybeint[i] == '-' || maybeint[i] == '+') && i < length) {
        i += 1;
    }
    while ((maybeint[i] >= '0' && maybeint[i] <= '9') && i < length) {
        i += 1;
    }
    if (i == length) {
        if (i >= sizeof(buffer)) {
            die("INTEGER TOO BIG");
        }
        memcpy(buffer, maybeint, length);
        buffer[length] = 0;
        errno = 0;
        value = strtol(buffer, NULL, 10);
        if (value > INT_MAX || value == LONG_MIN || errno != 0) {
            die("FIXNUM OVERFLOW");
        }
        return makefixnum(value);
    }
    return makesymbol(maybeint, length);
}


int munch_whitespace(FILE *file)
{
    int ch;
    while (1) {
        ch = getc(file);
        switch (ch) {
        case ' ': case '\t': case '\v': case '\f': case '\r': case '\n':
            break;

        case ';':
            while (ch != EOF && ch != '\n') {
                ch = getc(stdin);
            }
            break;

        default:
            return ch;
        }
    }
}


union object *lisp_read(int expect)
{
    int ch;
    unsigned int length;
    static unsigned char scratch[4098];  /* DEFECT */
    union object *obj, *tail;

    while (1) {
        ch = munch_whitespace(stdin);

        switch (ch) {
        case EOF:
            if (expect) {
                die("EXPECTED VALUE GOT END OF FILE");
            }
            return NULL;

        case '(':
            ch = munch_whitespace(stdin);
            if (ch == ')') {
                return makenil();
            }

            obj = makecons(NULL, makenil());
            tail = obj;

            while (1) {
                ungetc(ch, stdin);
                tail->cons.car = lisp_read(1);

                ch = munch_whitespace(stdin);
                if (ch == ')') {
                    return obj;
                } else if (ch == '.') {
                    tail->cons.cdr = lisp_read(1);
                    ch = munch_whitespace(stdin);
                    if (ch != ')') {
                        die("EXPECTED ) AFTER CDR VALUE");
                    }
                    return obj;
                } else {
                    tail->cons.cdr = makecons(NULL, makenil());
                    tail = tail->cons.cdr;
                }
            }

        case ')':
            die("EXTRA CLOSING PARENTHESIS");
            break;

        case '\'':
            return makecons(
                makesymbol((unsigned char *)"quote", 5),
                makecons(lisp_read(1), makenil())
            );

        case '`':
            return makecons(
                makesymbol((unsigned char *)"quasiquote", 10),
                makecons(lisp_read(1), makenil())
            );

        case ',':
            ch = getc(stdin);
            if (ch == '@') {
                obj = makesymbol((unsigned char *)"unquote-splicing", 16);
            } else {
                obj = makesymbol((unsigned char *)"unquote", 7);
                ungetc(ch, stdin);
            }
            return makecons(obj, makecons(lisp_read(1), makenil()));

        case '"':
            length = 0;
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
                if (length >= sizeof(scratch)) {
                    die("TOKEN TOO LONG");
                }
                scratch[length] = ch;
                length += 1;
            }
            return makestring(scratch, length);

        default:
            if (!lisp_istokenchar(ch)) {
                fprintf(stderr, "INVALID CHARACTER: %c\n", ch);
                die("INVALID CHARACTER");
            }
            length = 0;
            do {
                if (length >= sizeof(scratch)) {
                    die("TOKEN TOO LONG");
                }
                scratch[length] = ch;
                length += 1;
                ch = getc(stdin);
            } while (lisp_istokenchar(ch));
            ungetc(ch, stdin);
            return parsetoken(scratch, length);
        }
    }
}


int lisp_isnil(union object *object) {
    if (object == NULL) {
        die("INVALID VALUE");
    }
    if (object->head.tag == TAG_NIL) {
        return 1;
    }
    return 0;
}


void lisp_write(union object *object) {
    unsigned int i;
    int ch;
    if (object == NULL) {
        die("INVALID VALUE");
    }
    switch (object->head.tag) {
    case TAG_NIL:
        printf("nil");
        break;
    case TAG_CONS:
        printf("(");
        while (1) {
            lisp_write(object->cons.car);
            if (lisp_isnil(object->cons.cdr)) {
                break;
            } else if (object->cons.cdr == NULL || object->cons.cdr->head.tag != TAG_CONS) {
                printf(" . ");
                lisp_write(object->cons.cdr);
                break;
            } else {
                printf(" ");
                object = object->cons.cdr;
            }
        }
        printf(")");
        break;
    case TAG_STRING:
        printf("\"");
        i = 0;
        while (i < object->string.length) {
            ch = object->string.value[i];
            if (ch == '\"' || ch == '\\') {
                printf("\\");
            }
            printf("%c", ch);
            i += 1;
        }
        printf("\"");
        break;
    case TAG_SYMBOL:
        if (
            object->symbol.name == NULL
            || object->symbol.name->head.tag != TAG_STRING
        ) {
            die("INVALID SYMBOL");
        }
        object = object->symbol.name;
        i = 0;
        while (i < object->string.length) {
            printf("%c", object->string.value[i]);
            i += 1;
        }
        break;
    case TAG_FIXNUM:
        printf("%u", object->fixnum.value);
        break;
    default:
        die("INVALID TAG");
    }
}


int main(int argc, char **argv)
{
    union object *value;
    (void)argc;
    (void)argv;

    while (1) {
        value = lisp_read(0);
        if (value == NULL) {
            break;
        }
        lisp_write(value);
        printf("\n");
        fflush(stdout);
    }

    printf("Hello, world!\n");
    return 0;
}
