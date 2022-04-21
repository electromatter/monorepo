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
#define HASH_DEPTH      (16)
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
    TAG_FIXNUM,
    TAG_CHAR,
    TAG_CONS,
    TAG_STRING,
    TAG_SYMBOL,
    TAG_VECTOR,
    TAG_HASHTBL
};


const char *stag(int tag) {
    switch (tag) {
    case TAG_FIXNUM:
        return "FIXNUM";
    case TAG_CHAR:
        return "CHAR";
    case TAG_CONS:
        return "CONS";
    case TAG_STRING:
        return "STRING";
    case TAG_SYMBOL:
        return "SYMBOL";
    case TAG_VECTOR:
        return "VECTOR";
    case TAG_HASHTBL:
        return "HASHTBL";
    default:
        return "INVALID";
    }
}


typedef struct lispval {
    int tag;
    int fix;
    union object *ptr;
} lispval_t;


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
    int length;
    unsigned char *name;
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


union object {
    struct obhead head;
    struct cons cons;
    struct string string;
    struct symbol symbol;
    struct vector vector;
    struct hashtbl hashtbl;
};


struct lisp_global {
    union object *objs;
    union object *mark;
    union object **sweep;
    lispval_t nil;
    lispval_t t;
    lispval_t eof;
    unsigned int nyoung;
    unsigned int nold;
};


int lisp_tag(lispval_t x) {
    return x.tag;
}


int lisp_eq(lispval_t x, lispval_t y) {
    return x.tag == y.tag && x.fix == y.fix && x.ptr == y.ptr;
}


lispval_t makesymbol(struct lisp_global *g, const unsigned char *s, int n);


struct lisp_global *makeglobal(void) {
    struct lisp_global *g;
    g = (struct lisp_global *)malloc(sizeof(*g));
    if (g == NULL) {
        die("OUT OF MEMORY");
    }
    g->objs = NULL;
    g->mark = NULL;
    g->sweep = NULL;
    g->nil = makesymbol(g, (unsigned char *)"nil", 3);
    g->t = makesymbol(g, (unsigned char *)"t", 1);
    g->eof = makesymbol(g, (unsigned char *)"+eof+", 5);
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
    union object *o;
    if (lisp_tag(x) < TAG_CONS) {
        return;
    }
    o = x.ptr;
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
    case TAG_CONS:
        mark(g, obj->cons.car);
        mark(g, obj->cons.cdr);
        break;
    case TAG_STRING:
        break;
    case TAG_SYMBOL:
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
        die("INVALID TAG %s at %p", stag(obj->head.tag), (void *)obj);
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


lispval_t makeobj(struct lisp_global *g, int tag, size_t size) {
    lispval_t ret;
    /* collect(g, 0); */
    ret.tag = tag;
    ret.fix = 0;
    ret.ptr = (union object *)malloc(size);
    if (ret.ptr == NULL) {
        die("OUT OF MEMORY");
    }
    ret.ptr->head.tag = tag;
    ret.ptr->head.mark = 0;
    ret.ptr->head.next = g->objs;
    ret.ptr->head.back = NULL;
    g->objs = ret.ptr;
    g->nyoung += 1;
    return ret;
}


lispval_t makecons(struct lisp_global *g, lispval_t a, lispval_t d) {
    lispval_t ret;
    ret = makeobj(g, TAG_CONS, sizeof(ret.ptr->cons));
    ret.ptr->cons.car = a;
    ret.ptr->cons.cdr = d;
    return ret;
}


void expect_tag(lispval_t x, int tag) {
    if (lisp_tag(x) != tag) {
        die("TYPE ERROR EXPECTED %s", stag(tag));
    }
}


void rplaca(struct lisp_global *g, lispval_t x, lispval_t a) {
    (void) g;
    expect_tag(x, TAG_CONS);
    x.ptr->cons.car = a;
}


void rplacd(struct lisp_global *g, lispval_t x, lispval_t d) {
    (void) g;
    expect_tag(x, TAG_CONS);
    x.ptr->cons.cdr = d;
}


lispval_t car(struct lisp_global *g, lispval_t x) {
    (void) g;
    expect_tag(x, TAG_CONS);
    return x.ptr->cons.car;
}


lispval_t cdr(struct lisp_global *g, lispval_t x) {
    (void) g;
    expect_tag(x, TAG_CONS);
    return x.ptr->cons.cdr;
}


lispval_t makestring(struct lisp_global *g, int cap) {
    lispval_t ret;
    if (cap < VEC_MIN_SIZE) {
        cap = VEC_MIN_SIZE;
    }
    ret = makeobj(g, TAG_STRING, sizeof(ret.ptr->string) + cap);
    memset(&ret.ptr->string + 1, 0, cap);
    ret.ptr->string.extend = g->nil;
    ret.ptr->string.value = (unsigned char *)(&ret.ptr->string + 1);
    ret.ptr->string.capacity = cap;
    ret.ptr->string.length = 0;
    return ret;
}


int lisp_strcap(struct lisp_global *g, lispval_t x) {
    expect_tag(x, TAG_STRING);
    if (!lisp_eq(x.ptr->string.extend, g->nil)) {
        x = x.ptr->string.extend;
    }
    return x.ptr->string.capacity;
}


int lisp_strlen(struct lisp_global *g, lispval_t x) {
    (void) g;
    expect_tag(x, TAG_STRING);
    return x.ptr->string.length;
}


void lisp_strextend(struct lisp_global *g, lispval_t x) {
    lispval_t e;
    int i;
    int cap;
    expect_tag(x, TAG_STRING);
    cap = lisp_strcap(g, x);
    if (cap > INT_MAX / 2) {
        die("STRING TOO LARGE");
    }
    e = makestring(g, cap * 2);
    for (i = 0; i < cap; i++) {
        e.ptr->string.value[i] = x.ptr->string.value[i];
    }
    x.ptr->string.extend = e;
    x.ptr->string.value = e.ptr->string.value;
}


void lisp_strpush(struct lisp_global *g, lispval_t x, int ch) {
    int cap;
    expect_tag(x, TAG_STRING);
    cap = lisp_strcap(g, x);
    if (x.ptr->string.length == cap) {
        lisp_strextend(g, x);
    }
    x.ptr->string.value[x.ptr->string.length] = ch;
    x.ptr->string.length += 1;
}


unsigned char *lisp_str(struct lisp_global *g, lispval_t x) {
    (void) g;
    expect_tag(x, TAG_STRING);
    return x.ptr->string.value;
}


lispval_t makesymbol(struct lisp_global *g, const unsigned char *s, int n) {
    lispval_t ret;
    unsigned char *str;
    ret = makeobj(g, TAG_SYMBOL, sizeof(ret.ptr->symbol) + n);
    str = (unsigned char *)(&ret.ptr->symbol + 1);
    ret.ptr->symbol.name = str;
    memcpy(str, s, n);
    ret.ptr->symbol.length = n;
    return ret;
}


lispval_t makevector(struct lisp_global *g, int cap) {
    lispval_t ret;
    int i;
    if (cap < VEC_MIN_SIZE) {
        cap = VEC_MIN_SIZE;
    }
    ret = makeobj(g, TAG_VECTOR, sizeof(ret.ptr->vector) + cap * sizeof(lispval_t));
    ret.ptr->vector.length = 0;
    ret.ptr->vector.capacity = cap;
    ret.ptr->vector.extend = g->nil;
    ret.ptr->vector.slots = (lispval_t *)(&ret.ptr->vector + 1);
    for (i = 0; i < cap; i++) {
        ret.ptr->vector.slots[i] = g->nil;
    }
    return ret;
}


int lisp_veclen(struct lisp_global *g, lispval_t v) {
    (void) g;
    expect_tag(v, TAG_VECTOR);
    return v.ptr->vector.length;
}


int lisp_veccap(struct lisp_global *g, lispval_t v) {
    (void) g;
    expect_tag(v, TAG_VECTOR);
    if (!lisp_eq(v.ptr->vector.extend, g->nil)) {
        v = v.ptr->vector.extend;
    }
    return v.ptr->vector.capacity;
}


lispval_t lisp_vecelt(struct lisp_global *g, lispval_t v, int i) {
    int cap;
    expect_tag(v, TAG_VECTOR);
    cap = lisp_veccap(g, v);
    if (i < 0 || i > cap) {
        die("OUT OF BOUNDS");
    }
    return v.ptr->vector.slots[i];
}


void lisp_vecset(struct lisp_global *g, lispval_t v, int i, lispval_t a) {
    int cap;
    expect_tag(v, TAG_VECTOR);
    cap = lisp_veccap(g, v);
    if (i < 0 || i > cap) {
        die("OUT OF BOUNDS");
    }
    v.ptr->vector.slots[i] = a;
}


void lisp_vecextend(struct lisp_global *g, lispval_t v) {
    lispval_t e;
    int i;
    int cap;
    cap = lisp_veccap(g, v);
    if (cap > INT_MAX / 2) {
        die("VECTOR TOO LARGE");
    }
    e = makevector(g, cap * 2);
    for (i = 0; i < cap; i++) {
        e.ptr->vector.slots[i] = v.ptr->vector.slots[i];
    }
    v.ptr->vector.extend = e;
    v.ptr->vector.slots = e.ptr->vector.slots;
}


void lisp_vecpush(struct lisp_global *g, lispval_t v, lispval_t a) {
    int len;
    int cap;
    expect_tag(v, TAG_VECTOR);
    len = v.ptr->vector.length;
    cap = lisp_veccap(g, v);
    if (len == cap) {
        lisp_vecextend(g, v);
    }
    v.ptr->vector.slots[len] = a;
    v.ptr->vector.length += 1;
}


lispval_t makehashtbl(struct lisp_global *g) {
    lispval_t ret;
    ret = makeobj(g, TAG_HASHTBL, sizeof(ret.ptr->hashtbl));
    ret.ptr->hashtbl.keyv = g->nil;
    ret.ptr->hashtbl.valv = g->nil;
    return ret;
}


unsigned int fnv1a(const unsigned char *s, int length) {
    unsigned int hash = 2166136261;
    int i;
    for (i = 0; i < length; i++) {
        hash = 6777619 * (hash ^ s[i]);
    }
    return hash;
}


int lisp_symlen(struct lisp_global *g, lispval_t x) {
    (void)g;
    expect_tag(x, TAG_SYMBOL);
    return x.ptr->symbol.length;
}


const unsigned char *lisp_cname(struct lisp_global *g, lispval_t x) {
    (void)g;
    expect_tag(x, TAG_SYMBOL);
    return x.ptr->symbol.name;
}


lispval_t makechar(struct lisp_global *g, int value) {
    lispval_t ret;
    (void)g;
    ret.tag = TAG_CHAR;
    ret.fix = value & 0xff;
    ret.ptr = 0;
    return ret;
}


int lisp_char(struct lisp_global *g, lispval_t x) {
    (void)g;
    expect_tag(x, TAG_CHAR);
    return x.fix;
}


lispval_t makefixnum(struct lisp_global *g, int value) {
    lispval_t ret;
    (void)g;
    ret.tag = TAG_FIXNUM;
    ret.fix = value;
    ret.ptr = 0;
    return ret;
}


int lisp_fixnum(struct lisp_global *g, lispval_t x) {
    (void)g;
    expect_tag(x, TAG_FIXNUM);
    return x.fix;
}


lispval_t cintern(struct lisp_global *g, const char *name) {
    if (strcmp(name, "nil") == 0) {
        return g->nil;
    }
    if (strcmp(name, "t") == 0) {
        return g->t;
    }
    if (strcmp(name, "+eof+") == 0) {
        return g->eof;
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

    if (length == 1 && memcmp(s, "t", 1) == 0) {
        return g->t;
    }

    return makesymbol(g, s, length);
}


unsigned int
lisp_hash(struct lisp_global *g, lispval_t k, int depth) {
    const unsigned char *s;
    int i;
    int length;
    unsigned hash = 257;

    if (depth > HASH_DEPTH) {
        return hash;
    }

    switch (lisp_tag(k)) {
    case TAG_FIXNUM:
        hash = lisp_fixnum(g, k);
        break;

    case TAG_CHAR:
        hash = 17 * lisp_char(g, k);
        break;

    case TAG_CONS:
        hash = lisp_hash(g, car(g, k), depth + 1);
        hash ^= lisp_hash(g, cdr(g, k), depth + 1);
        hash *= 13;
        break;
    case TAG_STRING:
        s = lisp_str(g, k);
        length = lisp_strlen(g, k);
        hash = fnv1a(s, length);
        break;

    case TAG_SYMBOL:
        s = lisp_cname(g, k);
        length = lisp_symlen(g, k);
        hash = 7 * fnv1a(s, length);
        break;

    case TAG_VECTOR:
        length = lisp_veclen(g, k);
        for (i = 0; i < length; i++) {
            hash ^= lisp_hash(g, lisp_vecelt(g, k, i), depth + 1);
            hash *= 65537;
        }
        break;

    /*
    case TAG_HASHTBL:
        break;
    */

    default:
        die("INVALID TAG %s at %p", stag(lisp_tag(k)), k.ptr);
    }

    if (hash == 0) {
        hash = 1;
    }

    return hash;
}


#if 0
lispval_t
lisp_eql(struct lisp_global *g, lispval_t x, lispval_t y) {
}


lispval_t
lisp_hashget(struct lisp_global *g, lispval_t h, lispval_t k, lispval_t d) {
}


lispval_t
lisp_hashset(struct lisp_global *g, lispval_t h, lispval_t k) {
}


lispval_t
lisp_hashdel(struct lisp_global *g, lispval_t h, lispval_t k, lispval_t d) {
}
#endif


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
            return cintern(g, "+eof+");

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
                    break;
                } else if (ch == '.') {
                    rplacd(g, tail, lisp_read(g, 1));
                    ch = munch_whitespace(stdin);
                    if (ch != ')') {
                        die("EXPECTED ) AFTER CDR VALUE");
                    }
                    break;
                } else {
                    rplacd(g, tail, makecons(g, g->nil, g->nil));
                    tail = cdr(g, tail);
                }
            }
            return obj;

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

        case '#':
            ch = getc(stdin);
            if (ch == '(') {
                obj = makevector(g, 0);
                while (1) {
                    ch = munch_whitespace(stdin);
                    if (ch == ')') {
                        break;
                    }
                    ungetc(ch, stdin);
                    lisp_vecpush(g, obj, lisp_read(g, 1));
                }
            } else if (ch == '\\') {
                ch = getc(stdin);
                obj = makechar(g, ch);
                ch = munch_whitespace(stdin);
                if (lisp_istokenchar(ch)) {
                    die("INVALID CHARACTER %c", ch);
                }
                ungetc(ch, stdin);
            } else {
                die("UNKNOWN DISPATCHING MACRO CHARACTER %c", ch);
            }
            return obj;


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


void lisp_write(struct lisp_global *g, lispval_t x) {
    lispval_t a, d;
    const unsigned char *s;
    int i;
    int length;
    int ch;

    switch (lisp_tag(x)) {
    case TAG_FIXNUM:
        printf("%d", lisp_fixnum(g, x));
        break;

    case TAG_CHAR:
        printf("#\\%c", x.fix);
        break;

    case TAG_CONS:
        putc('(', stdout);
        while (1) {
            a = car(g, x);
            d = cdr(g, x);
            lisp_write(g, a);
            if (lisp_eq(d, g->nil)) {
                break;
            } else if (lisp_tag(d) != TAG_CONS) {
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
        length = lisp_symlen(g, x);
        s = lisp_cname(g, x);
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
            if (i > 0) {
                putc(' ', stdout);
            }
            a = lisp_vecelt(g, x, i);
            lisp_write(g, a);
        }
        putc(')', stdout);
        break;

    case TAG_HASHTBL:
        fputs("#<HASHTBL>", stdout);
        break;

    default:
        die("INVALID TAG %s at %p", stag(lisp_tag(x)), x.ptr);
    }
}


void repl(struct lisp_global *g) {
    lispval_t x;
    while (1) {
        x = lisp_read(g, 0);
        if (lisp_eq(x, g->eof)) {
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
