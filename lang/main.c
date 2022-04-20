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


struct lispval;
typedef struct lispval *lispval_t;


struct obhead {
    int tag;
    int mark;
    union object *next;
    union object *back;
};


struct cons {
    struct obhead head;
    lispval_t car;
    lispval_t cdr;
};


struct string {
    struct obhead head;
    unsigned char *value;
    unsigned int length;
};


struct symbol {
    struct obhead head;
    lispval_t name;
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
    union object onil;
    lispval_t nil;
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
    g->onil.head.tag = TAG_NIL;
    g->onil.head.mark = 0;
    g->onil.head.next = NULL;
    g->onil.head.back = NULL;
    g->nil = (lispval_t)&g->onil;
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


void mark(struct lisp_global *g, lispval_t x) {
    union object *o = (union object *)x;
    if (o->head.mark != TAG_NIL) {
        return;
    }
    o->head.mark = 1;
    o->head.back = g->markhead;
    g->markhead = o;
}


void mark1(struct lisp_global *g) {
    union object *obj;
    obj = g->markhead;
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

    if (g->sweephead == NULL) {
        if (full == 0 && (g->nyoung < g->nold || g->nyoung < GC_MIN_BATCH)) {
            return;
        }

        g->sweephead = &g->objs;
        g->nold = 0;
        g->nyoung = 0;
        markroots(g);
    }

    for (i = 0; (full != 0 || i < GC_MAX_BATCH) && g->sweephead == NULL; i++) {
        if (g->markhead != NULL) {
            mark1(g);
        } else {
            sweep1(g);
        }
    }
}


void freeglobal(struct lisp_global *g) {
    collect(g, 1);
    free(g);
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


lispval_t makecons(struct lisp_global *g, lispval_t a, lispval_t d) {
    union object *ret = NULL;
    ret = makeobj(g, TAG_CONS, sizeof(ret->cons));
    ret->cons.car = a;
    ret->cons.cdr = d;
    return (lispval_t)ret;
}


void rplaca(struct lisp_global *g, lispval_t c, lispval_t x) {
    union object *o;
    (void) g;
    o = (void *)c;
    if (o->head.tag != TAG_CONS) {
        die("EXPECTED CONS");
    }
    o->cons.car = x;
}


void rplacd(struct lisp_global *g, lispval_t c, lispval_t x) {
    union object *o;
    (void) g;
    o = (void *)c;
    if (o->head.tag != TAG_CONS) {
        die("EXPECTED CONS");
    }
    o->cons.cdr = x;
}


lispval_t car(struct lisp_global *g, lispval_t c) {
    union object *o;
    (void) g;
    o = (void *)c;
    if (o->head.tag != TAG_CONS) {
        die("EXPECTED CONS");
    }
    return o->cons.car;
}


lispval_t cdr(struct lisp_global *g, lispval_t c) {
    union object *o;
    (void) g;
    o = (void *)c;
    if (o->head.tag != TAG_CONS) {
        die("EXPECTED CONS");
    }
    return o->cons.cdr;
}


lispval_t makestring(struct lisp_global *g, unsigned char *value, int length) {
    union object *ret = NULL;
    ret = makeobj(g, TAG_STRING, sizeof(ret->string) + length);
    memset(ret + 1, 0, length);
    if (value != NULL) {
        memcpy(ret + 1, value, length);
    }
    ret->string.value = (unsigned char *)(ret + 1);
    ret->string.length = length;
    return (lispval_t)ret;
}


int lisp_strlen(struct lisp_global *g, lispval_t x) {
    union object *o;
    (void) g;
    o = (void *)x;
    if (o->head.tag != TAG_STRING) {
        die("EXPECTED STRING");
    }
    return o->string.length;
}


unsigned char *lisp_str(struct lisp_global *g, lispval_t x) {
    union object *o;
    (void) g;
    o = (void *)x;
    if (o->head.tag != TAG_STRING) {
        die("EXPECTED STRING");
    }
    return o->string.value;
}


lispval_t makesymbol(struct lisp_global *g, unsigned char *name, int length) {
    union object *ret = NULL;
    ret = makeobj(g, TAG_SYMBOL, sizeof(ret->symbol));
    ret->symbol.name = makestring(g, name, length);
    return (lispval_t)ret;
}


lispval_t lisp_name(struct lisp_global *g, lispval_t x) {
    union object *o;
    (void) g;
    o = (void *)x;
    if (o->head.tag != TAG_SYMBOL) {
        die("EXPECTED SYMBOL");
    }
    return o->symbol.name;
}


lispval_t makefixnum(struct lisp_global *g, int value) {
    union object *ret = NULL;
    ret = makeobj(g, TAG_FIXNUM, sizeof(ret->fixnum));
    ret->fixnum.value = value;
    return (lispval_t)ret;
}


int lisp_fixnum(struct lisp_global *g, lispval_t x) {
    union object *o;
    (void) g;
    o = (void *)x;
    if (o->head.tag != TAG_FIXNUM) {
        die("EXPECTED FIXNUM");
    }
    return o->fixnum.value;
}


lispval_t cintern(struct lisp_global *g, char *name) {
    if (strcmp(name, "nil") == 0) {
        return g->nil;
    }
    return makesymbol(g, (unsigned char *)name, strlen(name));
}


lispval_t parsetoken(struct lisp_global *g, unsigned char *maybeint, int length) {
    long int value;
    int i = 0;
    char buffer[32];

    if ((maybeint[i] == '-' || maybeint[i] == '+') && i < length) {
        i += 1;
    }

    while ((maybeint[i] >= '0' && maybeint[i] <= '9') && i < length) {
        i += 1;
    }

    if (i == length) {
        if (i >= (int)sizeof(buffer)) {
            die("FIXNUM OVERFLOW");
        }

        memcpy(buffer, maybeint, length);
        buffer[length] = 0;

        errno = 0;
        value = strtol(buffer, NULL, 10);
        if (value > INT_MAX || value < INT_MIN || errno != 0) {
            die("FIXNUM OVERFLOW");
        }

        return makefixnum(g, value);
    }

    if (length == 3 && memcmp(maybeint, "nil", 3) == 0) {
        return g->nil;
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


lispval_t lisp_read(struct lisp_global *g, int expect)
{
    int ch;
    int length;
    static unsigned char scratch[4098];  /* DEFECT */
    lispval_t obj, tail;

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
                return g->nil;
            }

            obj = makecons(g, g->nil, g->nil);
            tail = obj;

            while (1) {
                ungetc(ch, stdin);
                rplaca(g, tail, lisp_read(g, 1));

                ch = munch_whitespace(stdin);
                if (ch == ')') {
                    return obj;
                } else if (ch == '.') {
                    rplacd(g, tail, lisp_read(g, 1));
                    ch = munch_whitespace(stdin);
                    if (ch != ')') {
                        die("EXPECTED ) AFTER CDR VALUE");
                    }
                    return obj;
                } else {
                    rplacd(g, tail, makecons(g, g->nil, g->nil));
                    tail = cdr(g, tail);
                }
            }

        case ')':
            die("EXTRA CLOSING PARENTHESIS");
            break;

        case '\'':
            return makecons(g, cintern(g, "quote"), makecons(g, lisp_read(g, 1), g->nil));

        case '`':
            return makecons(g, cintern(g, "quasiquote"), makecons(g, lisp_read(g, 1), g->nil));

        case ',':
            ch = getc(stdin);
            if (ch == '@') {
                obj = cintern(g, "unquote-splicing");
            } else {
                obj = cintern(g, "unquote");
                ungetc(ch, stdin);
            }
            return makecons(g, obj, makecons(g, lisp_read(g, 1), g->nil));

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
                if (length >= (int)sizeof(scratch)) {
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
                if (length >= (int)sizeof(scratch)) {
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


int lisp_tag(lispval_t x) {
    return ((union object *)x)->head.tag;
}


void lisp_write(struct lisp_global *g, lispval_t x) {
    lispval_t a, d;
    unsigned char *s;
    int i;
    int length;
    int ch;
    (void)g;

    switch (lisp_tag(x)) {
    case TAG_NIL:
        fputs("nil", stdout);
        break;

    case TAG_CONS:
        putc('(', stdout);
        while (1) {
            a = car(g, x);
            d = cdr(g, x);
            lisp_write(g, a);
            if (lisp_tag(d) == TAG_NIL) {
                break;
            } else if (d == NULL || lisp_tag(d) != TAG_CONS) {
                fputs(" . ", stdout);
                lisp_write(g, d);
                break;
            } else {
                putc(' ', stdout);
                x = d;
            }
        }
        putc(')', stdout);
        break;

    case TAG_STRING:
        putc('"', stdout);
        length = lisp_strlen(g, x);
        s = lisp_str(g, x);
        i = 0;
        while (i < length) {
            ch = s[i];
            if (ch == '\"' || ch == '\\') {
                putc('\\', stdout);
            }
            putc(ch, stdout);
            i += 1;
        }
        putc('"', stdout);
        break;

    case TAG_SYMBOL:
        x = lisp_name(g, x);
        length = lisp_strlen(g, x);
        s = lisp_str(g, x);
        i = 0;
        while (i < length) {
            putc(s[i], stdout);
            i += 1;
        }
        break;

    case TAG_FIXNUM:
        printf("%d", lisp_fixnum(g, x));
        break;

    default:
        die("INVALID TAG");
    }
}


void repl(struct lisp_global *g) {
    lispval_t x;
    while (1) {
        x = lisp_read(g, 0);
        if (x == NULL) {
            break;
        }
        lisp_write(g, x);
        printf("\n");
        fflush(stdout);
    }
}


int main(int argc, char **argv) {
    struct lisp_global *g;
    (void)argc;
    (void)argv;

    g = makeglobal();
    repl(g);
    freeglobal(g);
    return 0;
}
