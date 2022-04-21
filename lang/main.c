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


#define VEC_MIN_SIZE    (8)
#define GC_MIN_BATCH    (100)
#define GC_MAX_BATCH    (10000)


void die(const char *format, ...)
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
    TAG_VECTOR,
    TAG_HASHTBL,
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
    lispval_t extend;
    unsigned char *value;
    int length;
    int capacity;
};


struct symbol {
    struct obhead head;
    lispval_t name;
};


struct vector {
    struct obhead head;
    int length;
    int capacity;
    lispval_t extend;
    lispval_t *slots;
};


struct hashtbl {
    struct obhead head;
    lispval_t keyv;
    lispval_t valv;
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
    struct vector vector;
    struct hashtbl hashtbl;
    struct fixnum fixnum;
};


struct lisp_global {
    union object *objs;
    union object *mark;
    union object **sweep;
    union object onil;
    lispval_t nil;
    unsigned int nyoung;
    unsigned int nold;
};


struct lisp_global *makeglobal(void) {
    struct lisp_global *g;
    g = (struct lisp_global *)malloc(sizeof(*g));
    if (g == NULL) {
        die("OUT OF MEMORY");
    }
    g->objs = NULL;
    g->mark = NULL;
    g->sweep = NULL;
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
    obj = *g->sweep;
    if (obj == NULL) {
        g->sweep = NULL;
        return;
    }
    if (obj->head.mark) {
        obj->head.mark = 0;
        g->sweep = &obj->head.next;
        g->nold += 1;
    } else {
        *g->sweep = obj->head.next;
        free(obj);
    }
}


void mark(struct lisp_global *g, lispval_t x) {
    union object *o = (union object *)x;
    if (o->head.mark != TAG_NIL) {
        return;
    }
    o->head.mark = 1;
    o->head.back = g->mark;
    g->mark = o;
}


void mark1(struct lisp_global *g) {
    union object *obj;
    int i;
    obj = g->mark;
    g->mark = obj->head.back;
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
    case TAG_VECTOR:
        mark(g, obj->vector.extend);
        for (i = 0; i < obj->vector.capacity; i++) {
            mark(g, obj->vector.slots[i]);
        }
        break;
    case TAG_HASHTBL:
        mark(g, obj->hashtbl.keyv);
        mark(g, obj->hashtbl.valv);
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

    if (g->sweep == NULL) {
        if (full == 0 && (g->nyoung < g->nold || g->nyoung < GC_MIN_BATCH)) {
            return;
        }

        g->sweep = &g->objs;
        g->nold = 0;
        g->nyoung = 0;

        markroots(g);
    }

    for (i = 0; (full != 0 || i < GC_MAX_BATCH) && g->sweep != NULL; i++) {
        if (g->mark != NULL) {
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
    /* collect(g, 0); */
    ret = (union object *)malloc(size);
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
    o = (union object *)c;
    if (o->head.tag != TAG_CONS) {
        die("EXPECTED CONS");
    }
    o->cons.car = x;
}


void rplacd(struct lisp_global *g, lispval_t c, lispval_t x) {
    union object *o;
    (void) g;
    o = (union object *)c;
    if (o->head.tag != TAG_CONS) {
        die("EXPECTED CONS");
    }
    o->cons.cdr = x;
}


lispval_t car(struct lisp_global *g, lispval_t c) {
    union object *o;
    (void) g;
    o = (union object *)c;
    if (o->head.tag != TAG_CONS) {
        die("EXPECTED CONS");
    }
    return o->cons.car;
}


lispval_t cdr(struct lisp_global *g, lispval_t c) {
    union object *o;
    (void) g;
    o = (union object *)c;
    if (o->head.tag != TAG_CONS) {
        die("EXPECTED CONS");
    }
    return o->cons.cdr;
}


lispval_t makestring(struct lisp_global *g, int cap) {
    union object *ret = NULL;
    if (cap < VEC_MIN_SIZE) {
        cap = VEC_MIN_SIZE;
    }
    ret = makeobj(g, TAG_STRING, sizeof(ret->string) + cap);
    memset(&ret->string + 1, 0, cap);
    ret->string.extend = g->nil;
    ret->string.value = (unsigned char *)(&ret->string + 1);
    ret->string.capacity = cap;
    ret->string.length = 0;
    return (lispval_t)ret;
}


int lisp_strcap(struct lisp_global *g, lispval_t x) {
    union object *o;
    o = (union object *)x;
    if (o->head.tag != TAG_STRING) {
        die("EXPECTED STRING");
    }
    if (o->string.extend != g->nil) {
        o = (union object *)o->string.extend;
    }
    return o->string.capacity;
}


int lisp_strlen(struct lisp_global *g, lispval_t x) {
    union object *o;
    (void) g;
    o = (union object *)x;
    if (o->head.tag != TAG_STRING) {
        die("EXPECTED STRING");
    }
    return o->string.length;
}


void lisp_strextend(struct lisp_global *g, lispval_t x) {
    union object *o;
    union object *e;
    int i;
    int cap;
    o = (union object *)x;
    if (o->head.tag != TAG_STRING) {
        die("EXPECTED STRING");
    }
    cap = lisp_strcap(g, x);
    if (cap > INT_MAX / 2) {
        die("STRING TOO LARGE");
    }
    e = (union object *)makestring(g, cap * 2);
    for (i = 0; i < cap; i++) {
        e->string.value[i] = o->string.value[i];
    }
    o->string.extend = (lispval_t)e;
    o->string.value = e->string.value;
}


void lisp_strpush(struct lisp_global *g, lispval_t x, int ch) {
    union object *o;
    int cap;
    o = (union object *)x;
    if (o->head.tag != TAG_STRING) {
        die("EXPECTED STRING");
    }
    cap = lisp_strcap(g, x);
    if (o->string.length == cap) {
        lisp_strextend(g, x);
    }
    o->string.value[o->string.length] = ch;
    o->string.length += 1;
}


unsigned char *lisp_str(struct lisp_global *g, lispval_t x) {
    union object *o;
    (void) g;
    o = (union object *)x;
    if (o->head.tag != TAG_STRING) {
        die("EXPECTED STRING");
    }
    return o->string.value;
}


lispval_t makesymbol(struct lisp_global *g, const unsigned char *s, int n) {
    union object *ret = NULL;
    union object *str = NULL;
    ret = makeobj(g, TAG_SYMBOL, sizeof(ret->symbol));
    str = (union object *)makestring(g, n);
    ret->symbol.name = (lispval_t)str;
    memcpy(str->string.value, s, n);
    str->string.length = n;
    return (lispval_t)ret;
}


lispval_t makevector(struct lisp_global *g, int cap) {
    union object *ret = NULL;
    int i;
    if (cap < VEC_MIN_SIZE) {
        cap = VEC_MIN_SIZE;
    }
    ret = makeobj(g, TAG_VECTOR, sizeof(ret->vector) + cap * sizeof(lispval_t));
    ret->vector.length = 0;
    ret->vector.capacity = cap;
    ret->vector.extend = g->nil;
    ret->vector.slots = (lispval_t *)(&ret->vector + 1);
    for (i = 0; i < cap; i++)
        ret->vector.slots[i] = g->nil;
    return (lispval_t)ret;
}


int lisp_veclen(struct lisp_global *g, lispval_t v) {
    union object *o;
    (void) g;
    o = (union object *)v;
    if (o->head.tag != TAG_VECTOR) {
        die("EXPECTED VECTOR");
    }
    return o->vector.length;
}


int lisp_veccap(struct lisp_global *g, lispval_t v) {
    union object *o;
    (void) g;
    o = (union object *)v;
    if (o->head.tag != TAG_VECTOR) {
        die("EXPECTED VECTOR");
    }
    if (o->vector.extend != g->nil) {
        o = (union object *)o->vector.extend;
    }
    return o->vector.capacity;
}


lispval_t lisp_vecelt(struct lisp_global *g, lispval_t v, int i) {
    union object *o;
    int cap;
    o = (union object *)v;
    if (o->head.tag != TAG_VECTOR) {
        die("EXPECTED VECTOR");
    }
    cap = lisp_veccap(g, v);
    if (i < 0 || i > cap) {
        die("OUT OF BOUNDS");
    }
    return o->vector.slots[i];
}


void lisp_vecset(struct lisp_global *g, lispval_t v, int i, lispval_t a) {
    union object *o;
    int cap;
    o = (union object *)v;
    if (o->head.tag != TAG_VECTOR) {
        die("EXPECTED VECTOR");
    }
    cap = lisp_veccap(g, v);
    if (i < 0 || i > cap) {
        die("OUT OF BOUNDS");
    }
    o->vector.slots[i] = a;
}


void lisp_vecextend(struct lisp_global *g, lispval_t v) {
    union object *o;
    union object *e;
    int i;
    int cap;
    o = (union object *)v;
    cap = lisp_veccap(g, v);
    if (cap > INT_MAX / 2) {
        die("VECTOR TOO LARGE");
    }
    e = (union object *)makevector(g, cap * 2);
    for (i = 0; i < cap; i++) {
        e->vector.slots[i] = o->vector.slots[i];
    }
    o->vector.extend = (lispval_t)e;
    o->vector.slots = e->vector.slots;
}


void lisp_vecpush(struct lisp_global *g, lispval_t v, lispval_t a) {
    union object *o;
    int len;
    int cap;
    o = (union object *)v;
    if (o->head.tag != TAG_VECTOR) {
        die("EXPECTED VECTOR");
    }
    len = o->vector.length;
    cap = lisp_veccap(g, v);
    if (len == cap) {
        lisp_vecextend(g, v);
    }
    o->vector.slots[len] = a;
    o->vector.length += 1;
}


lispval_t makehashtbl(struct lisp_global *g) {
    union object *ret = NULL;
    ret = makeobj(g, TAG_HASHTBL, sizeof(ret->fixnum));
    ret->hashtbl.keyv = g->nil;
    ret->hashtbl.valv = g->nil;
    return (lispval_t)ret;
}


/*
hashget
hashput
hashdel
hash
compiler
interpreter
*/


lispval_t lisp_name(struct lisp_global *g, lispval_t x) {
    union object *o;
    (void) g;
    o = (union object *)x;
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
    o = (union object *)x;
    if (o->head.tag != TAG_FIXNUM) {
        die("EXPECTED FIXNUM");
    }
    return o->fixnum.value;
}


lispval_t cintern(struct lisp_global *g, const char *name) {
    if (strcmp(name, "nil") == 0) {
        return g->nil;
    }
    return makesymbol(g, (const unsigned char *)name, strlen(name));
}


lispval_t parsetoken(struct lisp_global *g, lispval_t str) {
    long int value;
    int i = 0;
    int length;
    unsigned char *s;
    char buffer[32];

    length = lisp_strlen(g, str);
    s = lisp_str(g, str);

    if (i < length && (s[i] == '-' || s[i] == '+')) {
        i += 1;
    }

    while (i < length && (s[i] >= '0' && s[i] <= '9')) {
        i += 1;
    }

    if (i == length) {
        if (i >= (int)sizeof(buffer)) {
            die("FIXNUM OVERFLOW");
        }

        memcpy(buffer, s, length);
        buffer[length] = 0;

        errno = 0;
        value = strtol(buffer, NULL, 10);
        if (value > INT_MAX || value < INT_MIN || errno != 0) {
            die("FIXNUM OVERFLOW");
        }

        return makefixnum(g, value);
    }

    if (length == 3 && memcmp(s, "nil", 3) == 0) {
        return g->nil;
    }

    return makesymbol(g, s, length);
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
    lispval_t obj;
    lispval_t tail;
    lispval_t token;

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
            token = makestring(g, 0);
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
                lisp_strpush(g, token, ch);
            }
            return token;

        default:
            if (!lisp_istokenchar(ch)) {
                die("INVALID CHARACTER %c", ch);
            }
            token = makestring(g, 0);
            do {
                lisp_strpush(g, token, ch);
                ch = getc(stdin);
            } while (lisp_istokenchar(ch));
            ungetc(ch, stdin);
            return parsetoken(g, token);
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

    case TAG_VECTOR:
        length = lisp_veclen(g, x);
        putc('#', stdout);
        putc('(', stdout);
        for (i = 0; i < length; i++) {
            a = lisp_vecelt(g, x, i);
            lisp_write(g, a);
            if (i != length) {
                putc(' ', stdout);
            }
        }
        putc(')', stdout);
        break;

    case TAG_HASHTBL:
        fputs("#<HASHTBL>", stdout);
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
