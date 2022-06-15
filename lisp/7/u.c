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
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#define TNUM  0
#define TSYM  1
#define TCONS 2
struct num { int tag; int n; };
struct symbol { int tag; unsigned int n; unsigned char *s; };
struct cons { int tag; union obj *a, *d; };
union obj { int tag; struct num n; struct symbol s; struct cons c; };
static struct cons nilcons = {TCONS, (void *)&nilcons, (void *)&nilcons};
static union obj *nil = (void *)&nilcons;
union obj *parse(unsigned char *s, unsigned int n) {
    static char buf[32]; union obj *o; unsigned int i; long d;
    if (n == 0) abort();
    for (i = (s[0] == '+' || s[0] == '-'); i < n; i++)
        if (s[i] < '0' || s[i] > '9') break;
    if (i == n) {
        if (n >= sizeof(buf)) abort();
        for (i = 0; i < n; i++) buf[i] = s[i];
        buf[n] = 0;
        errno = 0; d = strtol(buf, NULL, 10);
        if (errno != 0 || d < INT_MIN || d > INT_MAX) abort();
        if ((o = malloc(sizeof(*o))) == NULL) abort();
        o->tag = TNUM; o->n.n = d;
        return o;
    } else {
        if ((o = malloc(sizeof(*o) + n)) == NULL) abort();
        o->tag = TSYM; o->s.n = n; o->s.s = (void *)(o + 1);
        for (i = 0; i < n; i++) o->s.s[i] = s[i];
        return o;
    }
}
union obj *cons(union obj *a, union obj *d) {
    union obj *o;
    if ((o = malloc(sizeof(*o))) == NULL) abort();
    if (a == NULL || d == NULL) abort();
    o->tag = TCONS; o->c.a = a; o->c.d = d;
    return o;
}
int skip_space(void) {
    int ch;
again:
    switch (ch = getc(stdin)) {
    case ';': do ch = getc(stdin); while (ch != '\n' && ch != EOF); goto again;
    case ' ': case '\t': case '\v': case '\r': case '\n': case '\f': goto again;
    default: return ch;
    }
}
union obj *read(void) {
    static unsigned char buf[256];
    union obj head, *end, *item; int ch; unsigned int n;
    switch (ch = skip_space()) {
    case EOF: return NULL;
    case '(':
        head.c.d = nil; end = &head;
        while (1) {
            if ((ch = skip_space()) == ')') return head.c.d;
            if (ch == '.') {
                if (head.c.d == nil) abort();
                if ((item = read()) == NULL) abort();
                end->c.d = item;
                if (skip_space() != ')') abort();
                return head.c.d;
            }
            ungetc(ch, stdin);
            if ((item = read()) == NULL) abort();
            end->c.d = cons(item, nil); end = end->c.d;
        }
    case '.': case ')': abort();
    default:
        n = 0;
        for (; ch >= '!' && ch <= '~' && ch != '(' && ch != ')'; ch = getchar())
            if (n < sizeof(buf)) buf[n++] = ch; else abort();
        ungetc(ch, stdin);
        return parse(buf, n);
    }
}
void printn(union obj *o) {
    if (o == nil) { fprintf(stdout, "nil"); return; }
    switch (o->tag) {
    default: abort();
    case TNUM: fprintf(stdout, "%d", o->n.n); break;
    case TSYM: fprintf(stdout, "%.*s", o->s.n, o->s.s); break;
    case TCONS:
        putc('(', stdout);
        while (1) {
            printn(o->c.a);
            if (o->c.d == nil) break; else putc(' ', stdout);
            if (o->c.d->tag != TCONS) {
                putc('.', stdout); putc(' ', stdout); printn(o->c.d);
                break;
            } else o = o->c.d;
        }
        putc(')', stdout);
        break;
    }
}
int symeql(union obj *o, char *s) {
    unsigned int i;
    if (o->tag != TSYM) return 0;
    for (i = 0; i < o->s.n; i++) if (o->s.s[i] != s[i]) return 0;
    return 1;
}
int eq(union obj *a, union obj *b) {
    unsigned int i;
    if (a->tag != b->tag) return 0;
    if (a->tag == TNUM) return a->n.n == b->n.n;
    if (a->tag != TSYM) return 0;
    if (a->s.n != b->s.n) return 0;
    for (i = 0; i < a->s.n; i++) if (a->s.s[i] != b->s.s[i]) return 0;
    return 1;
}
union obj *car(union obj *o) {
    if (o->tag != TCONS) abort();
    return o->c.a;
}
union obj *cdr(union obj *o) {
    if (o->tag != TCONS) abort();
    return o->c.d;
}
struct lab { struct lab *l; union obj *n; struct blob *b; unsigned int p; };
struct fix { struct fix *f; struct lab *l; struct blob *b; unsigned int p, o; };
struct blob {
    unsigned int n, z, v, o, a;
    struct lab *l;
    struct fix *f;
    unsigned char *s;
    struct blob *b;
};
struct blob *blobs = NULL;
struct blob *mkblob(void) {
    struct blob *b;
    if ((b = malloc(sizeof(*b))) == NULL) abort();
    b->n = b->z = b->v = 0; b->o = 0; b->l = NULL; b->f = NULL; b->s = NULL;
    b->a = 1;
    b->b = blobs;
    blobs = b;
    return b;
}
void emitb(struct blob *b, int x) {
    unsigned int z; unsigned char *t;
    if (b->n == b->z) {
        z = b->z + (b->z >> 3) + 1024;
        if (z < b->z || (t = realloc(b->s, z)) == NULL) abort();
        b->s = t; b->z = z;
    }
    b->s[b->n++] = x;
}
void imm(struct blob *b, union obj *x) {
    x = car(cdr(cdr(x)));
    if (x->tag != TNUM) abort();
    emitb(b, x->n.n & 0xff);
    emitb(b, (x->n.n >> 8) & 0xff);
    emitb(b, (x->n.n >> 16) & 0xff);
    emitb(b, (x->n.n >> 24) & 0xff);
}
void rop(struct blob *b, int op, int r, int m) {
    emitb(b, 0x48 + ((r & 8) >> 1) + ((m >> 3) & 1));
    emitb(b, op);
    emitb(b, 0xc0 + ((r & 7) << 3) + (m & 7));
}
void mop(struct blob *b, int op, int r, int m) {
    emitb(b, 0x48 + ((r & 8) >> 1) + ((m >> 3) & 1));
    emitb(b, op);
    emitb(b, 0x00 + ((r & 7) << 3) + (m & 7));
}
int reg(union obj *o) {
    if (symeql(o, "rax")) return 0;
    else if (symeql(o, "rcx")) return 1;
    else if (symeql(o, "rdx")) return 2;
    else if (symeql(o, "rbx")) return 3;
    else if (symeql(o, "rsp")) return 4;
    else if (symeql(o, "rbp")) return 5;
    else if (symeql(o, "rsi")) return 6;
    else if (symeql(o, "rdi")) return 7;
    else if (symeql(o, "r8")) return 8;
    else if (symeql(o, "r9")) return 9;
    else if (symeql(o, "r10")) return 10;
    else if (symeql(o, "r11")) return 11;
    else if (symeql(o, "r12")) return 12;
    else if (symeql(o, "r13")) return 13;
    else if (symeql(o, "r14")) return 14;
    else if (symeql(o, "r15")) return 15;
    else abort();
}
int ra(union obj *o) { return reg(car(cdr(o))); }
int rb(union obj *o) { return reg(car(cdr(cdr(o)))); }
struct lab *lab(struct blob *b, union obj *x) {
    struct lab *l;
    struct blob *t;
    for (t = blobs; t != NULL; t = t->b)
        for (l = t->l; l != NULL; l = l->l)
            if (eq(l->n, x))
                return l;
    if ((l = malloc(sizeof(*l))) == NULL) abort();
    l->l = b->l; l->n = x; l->b = NULL;
    b->l = l;
    return l;
}
void setlab(struct blob *b, union obj *e) {
    struct lab *l = lab(b, car(cdr(e)));
    if (l->b != NULL) abort();
    l->b = b; l->p = b->n;
}
void rel(struct blob *b, struct lab *l) {
    struct fix *f;
    if ((f = malloc(sizeof(*f))) == NULL) abort();
    f->f = b->f; f->l = l; f->b = b; f->p = b->n; f->o = 4;
    b->f = f;
    emitb(b, 0); emitb(b, 0); emitb(b, 0); emitb(b, 0);
}
void jmp(struct blob *b, int op, union obj *e) {
    emitb(b, op); rel(b, lab(b, car(cdr(e))));
}
void jcc(struct blob *b, int cc, union obj *e) {
    emitb(b, 0x0f); emitb(b, 0x80 + cc); rel(b, lab(b, car(cdr(e))));
}
void fixrel(struct fix *f) {
    unsigned long x;
    if (f->l->b == NULL) abort();
    x = f->l->b->v; x += f->l->p;
    if (f->b != NULL) { x -= f->b->v; x -= f->p; x -= f->o; }
    if ((x >> 32) != 0 && (x >> 32) != 0xffffffff) abort();
    f->b->s[f->p] = x & 0xff;
    f->b->s[f->p + 1] = (x >> 8) & 0xff;
    f->b->s[f->p + 2] = (x >> 16) & 0xff;
    f->b->s[f->p + 3] = (x >> 24) & 0xff;
}
void fixall(struct blob *b) {
    struct fix *f;
    for (f = b->f; f != NULL; f = f->f) fixrel(f);
}
int num(union obj *x) {
    if (x->tag != TNUM) abort();
    return x->n.n;
}
void align(struct blob *b, int x, int a) {
    unsigned int i, n;
    if (a < 1 || (a & (a - 1)) != 0) abort();
    n = (a - (b->n & (a - 1))) & (a - 1);
    for (i = 0; i < n; i++) emitb(b, x);
    if (b->a <= (unsigned int)a) b->a = a;
}
void skip(struct blob *b, int x, int n) {
    int i;
    for (i = 0; i < n; i++) emitb(b, x);
}
void lea(struct blob *b, int r, union obj *e) {
    emitb(b, 0x48 + ((r & 8) >> 1) + ((0 >> 3) & 1));
    emitb(b, 0x8d);
    emitb(b, 0x00 + ((r & 7) << 3) + (5 & 7));
    rel(b, lab(b, car(cdr(cdr(e)))));
}
void textasm(union obj *o) {
    union obj *e;
    struct blob *b = mkblob();
    for (; o != nil; o = cdr(o)) {
        if (o->tag != TCONS) abort();
        e = car(o);
        if (e->tag != TCONS) abort();
        else if (symeql(car(e), "align")) { align(b, 0x90, num(car(cdr(e)))); }
        else if (symeql(car(e), "skip")) { skip(b, 0x90, num(car(cdr(e)))); }
        else if (symeql(car(e), "label")) { setlab(b, e); }
        else if (symeql(car(e), "jz"))  { jcc(b, 0x04, e); }
        else if (symeql(car(e), "jnz")) { jcc(b, 0x05, e); }
        else if (symeql(car(e), "jc"))  { jcc(b, 0x02, e); }
        else if (symeql(car(e), "jnc")) { jcc(b, 0x03, e); }
        else if (symeql(car(e), "jo"))  { jcc(b, 0x00, e); }
        else if (symeql(car(e), "jno")) { jcc(b, 0x01, e); }
        else if (symeql(car(e), "jg"))  { jcc(b, 0x0f, e); }
        else if (symeql(car(e), "jge")) { jcc(b, 0x0d, e); }
        else if (symeql(car(e), "jl"))  { jcc(b, 0x0c, e); }
        else if (symeql(car(e), "jle")) { jcc(b, 0x0e, e); }
        else if (symeql(car(e), "ja"))  { jcc(b, 0x07, e); }
        else if (symeql(car(e), "jae")) { jcc(b, 0x03, e); }
        else if (symeql(car(e), "jb"))  { jcc(b, 0x02, e); }
        else if (symeql(car(e), "jbe")) { jcc(b, 0x06, e); }
        else if (symeql(car(e), "call")) { jmp(b, 0xe8, e); }
        else if (symeql(car(e), "jmp")) { jmp(b, 0xe9, e); }
        else if (symeql(car(e), "callr")) { rop(b, 0xff, 2, ra(e)); }
        else if (symeql(car(e), "jmpr")) { rop(b, 0xff, 4, ra(e)); }
        else if (symeql(car(e), "loadi")) { rop(b, 0xc7, 0, ra(e)); imm(b, e); }
        else if (symeql(car(e), "lea")) { lea(b, ra(e), e); }
        else if (symeql(car(e), "load")) { mop(b, 0x8b, ra(e), rb(e)); }
        else if (symeql(car(e), "store")) { mop(b, 0x89, rb(e), ra(e)); }
        else if (symeql(car(e), "loadb")) { mop(b, 0x8a, ra(e), rb(e)); }
        else if (symeql(car(e), "storeb")) { mop(b, 0x88, rb(e), ra(e)); }
        else if (symeql(car(e), "mov")) { rop(b, 0x8b, ra(e), rb(e)); }
        else if (symeql(car(e), "or")) { rop(b, 0x0b, ra(e), rb(e)); }
        else if (symeql(car(e), "xor")) { rop(b, 0x33, ra(e), rb(e)); }
        else if (symeql(car(e), "and")) { rop(b, 0x23, ra(e), rb(e)); }
        else if (symeql(car(e), "add")) { rop(b, 0x03, ra(e), rb(e)); }
        else if (symeql(car(e), "sub")) { rop(b, 0x2b, ra(e), rb(e)); }
        else if (symeql(car(e), "cmp")) { rop(b, 0x3b, ra(e), rb(e)); }
        else if (symeql(car(e), "test")) { rop(b, 0x85, rb(e), ra(e)); }
        else if (symeql(car(e), "shl") && rb(e) == 1) rop(b, 0xd3, 4, ra(e));
        else if (symeql(car(e), "shr") && rb(e) == 1) rop(b, 0xd3, 5, ra(e));
        else if (symeql(car(e), "sar") && rb(e) == 1) rop(b, 0xd3, 7, ra(e));
        else if (symeql(car(e), "mul")) { rop(b, 0xf7, 4, ra(e)); }
        else if (symeql(car(e), "div")) { rop(b, 0xf7, 6, ra(e)); }
        else if (symeql(car(e), "not")) { rop(b, 0xf7, 2, ra(e)); }
        else if (symeql(car(e), "neg")) { rop(b, 0xf7, 3, ra(e)); }
        else if (symeql(car(e), "push")) { rop(b, 0xff, 6, ra(e)); }
        else if (symeql(car(e), "pop")) { rop(b, 0x8f, 0, ra(e)); }
        else if (symeql(car(e), "ret")) { emitb(b, 0xc3); }
        else if (symeql(car(e), "syscall")) { emitb(b, 0x0f); emitb(b, 0x05); }
        else if (symeql(car(e), "ud2")) { emitb(b, 0x0f); emitb(b, 0x0b); }
        else if (symeql(car(e), "nop")) { emitb(b, 0x90); }
        else if (symeql(car(e), "clc")) { emitb(b, 0xf8); }
        else if (symeql(car(e), "stc")) { emitb(b, 0xf9); }
        else abort();
    }
}
void eval(union obj *o) {
    if (o->tag != TCONS) abort();
    if (symeql(car(o), "text")) textasm(cdr(o));
    else abort();
}
void emitw(struct blob *b, unsigned short x) { emitb(b, x); emitb(b, x >> 8); }
void emiti(struct blob *b, unsigned int x) { emitw(b, x); emitw(b, x >> 16); }
void emitq(struct blob *b, unsigned long x) { emiti(b, x); emiti(b, x >> 32); }
unsigned long start() {
    struct blob *b; struct lab *l;
    for (b = blobs; b != NULL; b = b->b) {
        for (l = b->l; l != NULL; l = l->l) {
            if (symeql(l->n, "_start")) {
                return b->v + l->p;
            }
        }
    }
    abort();
}
void head(void) {
    struct blob *b, *p;
    int n = 0;
    for (p = blobs; p != NULL; p = p->b) n += 1;
    b = mkblob();
    if (n > 0xffff) abort();
    emitb(b, 0x7f); emitb(b, 'E'); emitb(b, 'L'); emitb(b, 'F');
    emitb(b, 2); emitb(b, 1); emitb(b, 1); emitb(b, 0);
    emitq(b, 0);
    emitw(b, 2); emitw(b, 0x3e);
    emiti(b, 1);
    emitq(b, start());
    emitq(b, 0x40);
    emitq(b, 0);
    emiti(b, 0);
    emitw(b, 0x40);
    emitw(b, 0x38); emitw(b, n);
    emitw(b, 0x40); emitw(b, 0);
    emitw(b, 0);
    for (p = blobs; p != NULL; p = p->b) {
        if (p == b) continue;
        emiti(b, 1); emiti(b, 5);
        emitq(b, p->o);
        emitq(b, p->v);
        emitq(b, 0);
        emitq(b, p->n);
        emitq(b, p->n);
        emitq(b, p->a);
    }
}
void done(void) {
    unsigned long v = 0x100000, o = 0;
    struct blob *b;
    o += 0x40;
    for (b = blobs; b != NULL; b = b->b)
        o += 0x38;
    for (b = blobs; b != NULL; b = b->b) {
        v += (b->a - (v & (b->a - 1))) & (b->a - 1);
        v += (4096 - (v & 4095)) & 4095;
        o += (4096 - (o & 4095)) & 4095;
        b->v = v;
        b->o = o;
        v += b->z;
        o += b->z;
        if (v > 0xffffffff) abort();
    }
    for (b = blobs; b != NULL; b = b->b)
        fixall(b);
    head();
    o = 0;
    for (b = blobs; b != NULL; b = b->b) {
        for (; o < b->o; o++)
            putc(0, stdout);
        fwrite(b->s, 1, b->n, stdout);
        o += b->n;
    }
}
int main(int argc, char **argv) {
    union obj *o; (void)argc; (void)argv;
    while ((o = read()) != NULL)
        eval(o);
    done();
    return 0;
}
