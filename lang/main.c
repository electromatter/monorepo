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

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define GC_MIN_BATCH    (100)
#define GC_MAX_BATCH    (10000)


void die(char *format, ...)
{
    va_list args;
    va_start(args, format);
    fflush(stdout);
    fputs("die: ", stderr);
    vfprintf(stderr, format, args);
    putc('\n', stderr);
    fflush(stderr);
    exit(1);
    va_end(args);
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
    int mark;
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


struct lisp_global {
    union object *objs;
    union object *markhead;
    union object **sweephead;
    union object nil;
    unsigned int nyoung;
    unsigned int nold;
};


struct lisp_global *makeglobal(void) {
    struct lisp_global *g;
    g = malloc(sizeof(*g));
    if (g == NULL) {
        die("OUT OF MEMORY");
    }
    g->objs = NULL;
    g->markhead = NULL;
    g->sweephead = NULL;
    g->nil.head.tag = TAG_NIL;
    g->nil.head.mark = 0;
    g->nil.head.next = NULL;
    g->nil.head.back = NULL;
    g->nyoung = 0;
    g->nold = 0;
    return g;
}


void sweep1(struct lisp_global *g) {
    union object *obj;
    obj = *g->sweephead;
    if (obj == NULL) {
        g->sweephead = NULL;
        return;
    }
    if (obj->head.mark) {
        obj->head.mark = 0;
        g->sweephead = &obj->head.next;
        g->nold += 1;
    } else {
        *g->sweephead = obj->head.next;
        free(obj);
    }
}


void mark(struct lisp_global *g, union object *obj) {
    if (obj->head.mark != 0) {
        return;
    }
    obj->head.mark = 1;
    obj->head.back = g->markhead;
    g->markhead = obj;
}


void mark1(struct lisp_global *g) {
    union object *obj;
    obj = g->markhead;
    if (obj == NULL) {
        return;
    }
    g->markhead = obj->head.back;
    switch (obj->head.tag) {
    case TAG_NIL:
        break;
    case TAG_CONS:
        mark(g, obj->cons.car);
        mark(g, obj->cons.cdr);
        break;
    case TAG_STRING:
        break;
    case TAG_SYMBOL:
        mark(g, obj->symbol.name);
        break;
    case TAG_FIXNUM:
        break;
    default:
        die("INVALID TAG %p", (void *)obj);
    }
}


void markroots(struct lisp_global *g) {
    (void)g;
}


void collect(struct lisp_global *g, int full) {
    unsigned int i;

    if (!full && (g->nyoung < g->nold || g->nyoung < GC_MIN_BATCH)) {
        return;
    }

    if (g->markhead == NULL && g->sweephead == NULL) {
        markroots(g);
    }

    for (i = 0; full || (i < GC_MAX_BATCH); i++) {
        if (g->markhead != NULL) {
            mark1(g);
            if (g->markhead == NULL) {
                g->sweephead = &g->objs;
                g->nold = 0;
            }
        } else if (g->sweephead != NULL) {
            sweep1(g);
        } else {
            g->nyoung = 0;
            return;
        }
    }
}


union object *makeobj(struct lisp_global *g, int tag, size_t size) {
    union object *ret;
    collect(g, 0);
    ret = malloc(size);
    if (ret == NULL) {
        die("OUT OF MEMORY");
    }
    ret->head.tag = tag;
    ret->head.mark = 0;
    ret->head.next = g->objs;
    ret->head.back = NULL;
    g->objs = ret;
    g->nyoung += 1;
    return ret;
}


union object *makenil(struct lisp_global *g) {
    return &g->nil;
}


union object *makecons(struct lisp_global *g, union object *car, union object *cdr) {
    union object *ret = NULL;
    ret = makeobj(g, TAG_CONS, sizeof(ret->cons));
    ret->cons.car = car;
    ret->cons.cdr = cdr;
    return ret;
}


union object *makestring(struct lisp_global *g, unsigned char *value, unsigned int length) {
    union object *ret = NULL;
    ret = makeobj(g, TAG_STRING, sizeof(ret->string) + length);
    memcpy(ret + 1, value, length);
    ret->string.value = (unsigned char *)(ret + 1);
    ret->string.length = length;
    return ret;
}


union object *makesymbol(struct lisp_global *g, unsigned char *name, unsigned int length) {
    union object *ret = NULL;
    ret = makeobj(g, TAG_SYMBOL, sizeof(ret->symbol));
    ret->symbol.name = makestring(g, name, length);
    return ret;
}


union object *makefixnum(struct lisp_global *g, int value) {
    union object *ret = NULL;
    ret = makeobj(g, TAG_FIXNUM, sizeof(ret->fixnum));
    ret->fixnum.value = value;
    return ret;
}


union object *cintern(struct lisp_global *g, char *name) {
    if (strcmp(name, "nil") == 0) {
        return makenil(g);
    }
    return makesymbol(g, (unsigned char *)name, strlen(name));
}


union object *parsetoken(struct lisp_global *g, unsigned char *maybeint, unsigned int length) {
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
            die("FIXNUM OVERFLOW");
        }

        memcpy(buffer, maybeint, length);
        buffer[length] = 0;

        errno = 0;
        value = strtol(buffer, NULL, 10);
        if (value > INT_MAX || value == LONG_MIN || errno != 0) {
            die("FIXNUM OVERFLOW");
        }

        return makefixnum(g, value);
    }

    if (length == 3 && memcmp(maybeint, "nil", 3) == 0) {
        return makenil(g);
    }

    return makesymbol(g, maybeint, length);
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


union object *lisp_read(struct lisp_global *g, int expect)
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
                return makenil(g);
            }

            obj = makecons(g, makenil(g), makenil(g));
            tail = obj;

            while (1) {
                ungetc(ch, stdin);
                tail->cons.car = lisp_read(g, 1);

                ch = munch_whitespace(stdin);
                if (ch == ')') {
                    return obj;
                } else if (ch == '.') {
                    tail->cons.cdr = lisp_read(g, 1);
                    ch = munch_whitespace(stdin);
                    if (ch != ')') {
                        die("EXPECTED ) AFTER CDR VALUE");
                    }
                    return obj;
                } else {
                    tail->cons.cdr = makecons(g, makenil(g), makenil(g));
                    tail = tail->cons.cdr;
                }
            }

        case ')':
            die("EXTRA CLOSING PARENTHESIS");
            break;

        case '\'':
            return makecons(
                g,
                cintern(g, "quote"),
                makecons(g, lisp_read(g, 1), makenil(g))
            );

        case '`':
            return makecons(
                g,
                cintern(g, "quasiquote"),
                makecons(g, lisp_read(g, 1), makenil(g))
            );

        case ',':
            ch = getc(stdin);
            if (ch == '@') {
                obj = cintern(g, "unquote-splicing");
            } else {
                obj = cintern(g, "unquote");
                ungetc(ch, stdin);
            }
            return makecons(g, obj, makecons(g, lisp_read(g, 1), makenil(g)));

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
            return makestring(g, scratch, length);

        default:
            if (!lisp_istokenchar(ch)) {
                die("INVALID CHARACTER %c", ch);
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
            return parsetoken(g, scratch, length);
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


void lisp_write(struct lisp_global *g, union object *object) {
    unsigned int i;
    int ch;

    (void)g;

    if (object == NULL) {
        die("INVALID VALUE");
    }

    switch (object->head.tag) {
    case TAG_NIL:
        fputs("nil", stdout);
        break;

    case TAG_CONS:
        putc('(', stdout);
        while (1) {
            lisp_write(g, object->cons.car);
            if (lisp_isnil(object->cons.cdr)) {
                break;
            } else if (object->cons.cdr == NULL || object->cons.cdr->head.tag != TAG_CONS) {
                fputs(" . ", stdout);
                lisp_write(g, object->cons.cdr);
                break;
            } else {
                putc(' ', stdout);
                object = object->cons.cdr;
            }
        }
        putc(')', stdout);
        break;

    case TAG_STRING:
        putc('"', stdout);
        i = 0;
        while (i < object->string.length) {
            ch = object->string.value[i];
            if (ch == '\"' || ch == '\\') {
                putc('\\', stdout);
            }
            putc(ch, stdout);
            i += 1;
        }
        putc('"', stdout);
        break;

    case TAG_SYMBOL:
        object = object->symbol.name;
        i = 0;
        while (i < object->string.length) {
            putc(object->string.value[i], stdout);
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
    struct lisp_global *g;
    union object *value;
    (void)argc;
    (void)argv;

    g = makeglobal();
    while (1) {
        value = lisp_read(g, 0);
        if (value == NULL) {
            break;
        }
        lisp_write(g, value);
        printf("\n");
        fflush(stdout);
    }
    collect(g, 1);

    printf("Hello, world!\n");
    return 0;
}
