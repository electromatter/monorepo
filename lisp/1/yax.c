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
#include <limits.h>
#include <string.h>

void printtb(void);

void die(const char *file, int line, const char *why) {
  fflush(stdout);
  fflush(stderr);
  printtb();
  fprintf(stderr, "die(%s:%d): %s\n", file, line, why);
  fflush(stderr);
  abort();
}

#define die(why) die(__FILE__, __LINE__, why)

enum tag {
  TAG_NIL,
  TAG_UNDEF,
  TAG_FIXNUM,
  TAG_SYM,
  TAG_CONS,
  TAG_BOX,
  TAG_FUNC,
  TAG_STR,
  TAG_FILE,
  TAG_CHAR,
  TAG_VEC,
};

void assertvalidtag(int tag);

int tagisptr(int tag) {
  switch (tag) {
  case TAG_NIL:
  case TAG_UNDEF:
  case TAG_FIXNUM:
  case TAG_FILE:
  case TAG_CHAR:
    return 0;
  case TAG_SYM:
  case TAG_CONS:
  case TAG_BOX:
  case TAG_FUNC:
  case TAG_STR:
  case TAG_VEC:
    return 1;
  default:
    die("bad tag");
  }
}

const char * tag2str(int tag) {
  static char temp[64] = "(unknown)";
#define T(x) case TAG_##x: return "TAG_"#x
  switch (tag) {
  T(NIL);
  T(UNDEF);
  T(FIXNUM);
  T(SYM);
  T(CONS);
  T(FUNC);
  T(STR);
  T(VEC);
  default:
    sprintf(temp, "(unknown %d)", tag);
    return temp;
  }
#undef T
}

union value {
  long fix;
  void *ptr;
};

struct cell {
  unsigned char tag;
  union value v;
};

#define OBJHEAD struct objhead *next;unsigned char tag, color;

struct objhead {
  OBJHEAD
};

struct cons {
  OBJHEAD
  unsigned char cartag, cdrtag;
  union value car, cdr;
} cons_t;

struct str {
  OBJHEAD
  int fill, cap, ro;
  char *buf;
};

struct sym {
  OBJHEAD
  int size;
  char sym[1];
};

struct func {
  OBJHEAD
  int *code;
  void (*run)(struct func *, int);
  int nargs, nvals, ncode, nclos;
  struct cell name;
  struct cell vals[1];
};

struct vec {
  OBJHEAD
  int fill, cap;
  struct cell *vals;
};

struct gcroot {
  struct cell *cells;
  int fill, cap;
};

struct objhead *allobjs = NULL;
int gcthresh = 0;
size_t room = 0;

enum constant {
  CONST_T,
  CONST_EOF,
  CONST_LEXICALS,
  CONST_TAGLABELS,
  CONST_MAX,
};

enum root {
  ROOT_CONSTANTS,
  ROOT_STACK,
  ROOT_TRACEBACK,
  ROOT_VALTAB,
  ROOT_GLOBALS,
  ROOT_FUNCTIONS,
  ROOT_MACROS,
  ROOT_LEXICALS,
  ROOT_TAGLABELS,
  ROOT_MAX,
};

struct gcroot gcroots[ROOT_MAX] = {};

void poison(void *ptr, size_t size) {
  memset(ptr, 0xcc, size);
}

void resizeroot(struct gcroot *root, int n) {
  void *temp;
  if (n < 0)
    die("negative root size");
  if (n > root->cap) {
    root->cap = n + 64 + (root->cap >> 3);
    temp = realloc(root->cells, root->cap * sizeof(*root->cells));
    if (!temp)
      die("nomem");
    root->cells = temp;
    poison(root->cells + root->fill, (root->cap - root->fill) * sizeof(*root->cells));
  }
  if (n < root->fill)
    poison(root->cells + n, (root->fill - n) * sizeof(*root->cells));
  else
    memset(root->cells + root->fill, 0x00, (n - root->fill) * sizeof(*root->cells));
  root->fill = n;
}

int rootfill(struct gcroot *root) {
  return root->fill;
}

struct cell *getcell(int n) {
  struct gcroot *stack = &gcroots[ROOT_STACK];
  if (n >= 0 || n < -stack->fill)
    die("read past end of stack");
  assertvalidtag(stack->cells[stack->fill + n].tag);
  return &stack->cells[stack->fill + n];
}

int stackdepth(void) {
  return rootfill(&gcroots[ROOT_STACK]);
}

void push(const struct cell *cell) {
  int top;
  struct gcroot *stack = &gcroots[ROOT_STACK];
  assertvalidtag(cell->tag);
  top = stack->fill;
  resizeroot(stack, top + 1);
  stack->cells[top] = *cell;
}

void pop1(struct cell *cell) {
  struct gcroot *stack = &gcroots[ROOT_STACK];
  if (!stack->fill)
    die("stack underflow");
  *cell = stack->cells[--stack->fill];
  assertvalidtag(cell->tag);
  poison(stack->cells + stack->fill, sizeof(*stack->cells));
}

void pop(int n) {
  struct gcroot *stack = &gcroots[ROOT_STACK];
  if (n == 0)
    return;
  if (n < 0 || n > stack->fill)
    die("stack underflow");
  stack->fill -= n;
  poison(stack->cells + stack->fill, n * sizeof(*stack->cells));
}

void pushroot(struct gcroot *root, int index) {
  if (index < 0 || index >= root->fill)
    die("root index error");
  push(&root->cells[index]);
}

void poproot(struct gcroot *root, int index) {
  if (index < 0 || index >= root->fill)
    die("root index error");
  pop1(&root->cells[index]);
}

void showroot(struct gcroot *root) {
  int i;
  for (i = 0; i < root->fill; i++)
    printf("[%s]\n", tag2str(root->cells[i].tag));
}

void assertvalidtag(int tag) {
  switch (tag) {
  case TAG_NIL:
  case TAG_UNDEF:
  case TAG_FIXNUM:
  case TAG_FILE:
  case TAG_CHAR:
  case TAG_SYM:
  case TAG_CONS:
  case TAG_BOX:
  case TAG_FUNC:
  case TAG_STR:
  case TAG_VEC:
    return;
  default:
    showroot(&gcroots[ROOT_STACK]);
    die("bad tag");
  }
}

int objroom(struct objhead *obj) {
  struct sym *sym;
  struct str *str;
  struct func *func;
  struct vec *vec;
  switch (obj->tag) {
  case TAG_SYM:
    sym = (void*)obj;
    return sizeof(*sym) + sym->size;
  case TAG_CONS:
    return sizeof(struct cons);
  case TAG_FUNC:
    func = (void*)obj;
    return sizeof(*func) + (func->nvals + func->nclos) * sizeof(struct cell) + func->ncode * sizeof(*func->code);
  case TAG_STR:
    str = (void*)obj;
    return sizeof(*str) + str->cap;
  case TAG_VEC:
    vec = (void*)obj;
    return sizeof(*vec) + vec->cap * sizeof(struct cell);
  default:
    die("invalid object");
  }
}

void poisoncell(struct cell *cell) {
  poison(cell, sizeof(*cell));
}

void freeobj(struct objhead *obj) {
  struct sym *sym;
  struct str *str;
  struct func *func;
  struct vec *vec;
  int size = objroom(obj);
  switch (obj->tag) {
  case TAG_SYM:
    sym = (void *)obj;
    poison(obj, sizeof(struct sym) + sym->size);
    break;
  case TAG_CONS:
    poison(obj, sizeof(struct cons));
    break;
  case TAG_FUNC:
    func = (void *)obj;
    poison(func->code, func->ncode * sizeof(*func->code));
    free(func->code);
    poison(obj, sizeof(struct func) + (func->nvals + func->nclos) * sizeof(struct cell));
    break;
  case TAG_STR:
    str = (void *)obj;
    poison(str->buf, str->cap);
    free(str->buf);
    poison(obj, sizeof(struct str));
    break;
  case TAG_VEC:
    vec = (void *)vec;
    poison(vec->vals, vec->cap * sizeof(*vec->vals));
    free(vec->vals);
    poison(obj, sizeof(*vec));
  default:
    die("invalid object");
  }
  free(obj);
  room -= size;
}

void mark(struct objhead *obj) {
  struct func *f;
  struct cons *c;
  struct vec *v;
  int i;
  obj->color = 1;
  switch (obj->tag) {
  case TAG_CONS: case TAG_BOX:
    c = (struct cons *)obj;
    if (tagisptr(c->cartag) && !((struct objhead*)c->car.ptr)->color)
      mark(c->car.ptr);
    if (tagisptr(c->cdrtag) && !((struct objhead*)c->cdr.ptr)->color)
      mark(c->cdr.ptr);
    break;
  case TAG_FUNC:
    f = (struct func *)obj;
    if (tagisptr(f->name.tag) && !((struct objhead*)f->name.v.ptr)->color)
      mark(f->name.v.ptr);
    for (i = 0; i < f->nvals + f->nclos; i++)
      if (tagisptr(f->vals[i].tag) && !((struct objhead*)f->vals[i].v.ptr)->color)
        mark(f->vals[i].v.ptr);
    break;
  case TAG_VEC:
    v = (struct vec*)obj;
    for (i = 0; i < v->fill; i++)
      if (tagisptr(v->vals[i].tag) && !((struct objhead*)v->vals[i].v.ptr)->color)
        mark(v->vals[i].v.ptr);
    break;
  case TAG_SYM: case TAG_STR:
    break;
  default:
    die("invalid tag");
  }
}

void markroot(struct gcroot *root) {
  int i;
  for (i = 0; i < root->fill; i++)
    if (tagisptr(root->cells[i].tag) && !((struct objhead*)root->cells[i].v.ptr)->color)
      mark(root->cells[i].v.ptr);
}

void sweep(struct objhead **ptr) {
  struct objhead *o = *ptr, *dead;
  while (o) {
    if (o->color) {
      o->color = 0;
      ptr = &o->next;
      o = o->next;
    } else {
      dead = o;
      *ptr = o->next;
      o = o->next;
      freeobj(dead);
    }
  }
}

size_t countobjs(void) {
  struct objhead *o = allobjs;
  size_t total = 0;
  while (o) {
    total += objroom(o);
    o = o->next;
  }
  return total;
}

void gc(void) {
  int i;
  size_t orig_room = room;
  gcthresh = 1000;
  for (i = 0; i < ROOT_MAX; i++)
    markroot(&gcroots[i]);
  sweep(&allobjs);
}

struct objhead * makeobj(int tag, size_t size) {
  struct objhead *head;
  if (size < sizeof(struct objhead))
    die("obj without head");
  if (!gcthresh--)
    gc();
  head = malloc(size);
  room += size;
  if (!head)
    die("no mem");
  head->tag = tag;
  head->color = 0;
  head->next = allobjs;
  allobjs = head;
  return head;
}

void pushfix(long x) {
  struct cell top = {TAG_FIXNUM};
  top.v.fix = x;
  push(&top);
}

void pushchr(long x) {
  struct cell top = {TAG_CHAR};
  top.v.fix = x;
  push(&top);
}

void pushfile(FILE *file) {
  struct cell top = {TAG_NIL};
  top.v.ptr = file;
  push(&top);
}

void doroom(struct func *func, int nargs) {
  pushfix(room);
}

void pushnil(void) {
  struct cell top = {TAG_NIL};
  push(&top);
}

void pushbool(int val) {
  if (val)
    pushroot(&gcroots[ROOT_CONSTANTS], CONST_T);
  else
    pushnil();
}

void mkstr(const char *s, size_t sz) {
  struct cell top = {TAG_STR};
  char *buf;
  struct str *str;
  buf = malloc(sz);
  if (!buf)
    die("nomem");
  room += sz;
  memcpy(buf, s, sz);
  str = (void *)makeobj(TAG_STR, sizeof(*str));
  str->fill = str->cap = sz;
  str->ro = 0;
  str->buf = buf;
  top.v.ptr = str;
  push(&top);
}

void mkvec(struct func *func, int nargs) {
  struct cell top = {TAG_VEC};
  struct vec *vec;
  vec = (void *)makeobj(TAG_VEC, sizeof(*vec));
  vec->fill = 0;
  vec->cap = 0;
  vec->vals = NULL;
  top.v.ptr = vec;
  push(&top);
}

void mksym(const char *s, size_t sz) {
  struct cell top = {TAG_SYM};
  struct sym *sym;
  sym = (void *)makeobj(TAG_SYM, sizeof(*sym) + sz);
  sym->size = sz;
  memcpy(sym->sym, s, sz);
  top.v.ptr = sym;
  push(&top);
}

#define csym(s) mksym((s), strlen(s))

void pushdup(int n) {
  struct cell top;
  top = *getcell(n);
  push(&top);
}

void doset(int n) {
  struct cell *c = getcell(n);
  *c = *getcell(-1);
  pop(1);
}

void shift(int n) {
  struct cell top;
  if (n == 0)
    return;
  if (n < 0)
    die("shift < 0");
  top = *getcell(-1);
  pop(n + 1);
  push(&top);
}

int gettag(int n) {
  return getcell(n)->tag;
}

#define cellisnil(n)    (gettag(n) == TAG_NIL)
#define cellisundef(n)  (gettag(n) == TAG_UNDEF)
#define cellisfix(n)    (gettag(n) == TAG_FIXNUM)
#define cellissym(n)    (gettag(n) == TAG_SYM)
#define celliscons(n)   (gettag(n) == TAG_CONS || gettag(n) == TAG_BOX)
#define cellisbox(n)    (gettag(n) == TAG_BOX)
#define cellisfunc(n)   (gettag(n) == TAG_FUNC)
#define cellisstr(n)    (gettag(n) == TAG_STR)
#define cellisvec(n)    (gettag(n) == TAG_VEC)
#define cellisfile(n)   (gettag(n) == TAG_FILE)
#define cellischar(n)   (gettag(n) == TAG_CHAR)

#define mkaccessor(rtype, name, etag, field) \
rtype \
name(int n) \
{ \
  struct cell *c = getcell(n); \
  if (!etag(n)) { \
    printf("got a %s\n", tag2str(gettag(n))); \
    die("expected a " #etag); \
  } \
  return (rtype)c->v.field; \
}

mkaccessor(long, getfix, cellisfix, fix)
mkaccessor(struct sym *, getsym, cellissym, ptr)
mkaccessor(struct cons *, getcons, celliscons, ptr)
mkaccessor(struct str *, getstr, cellisstr, ptr)
mkaccessor(struct vec *, getvec, cellisvec, ptr)
mkaccessor(struct func *, getfunc, cellisfunc, ptr)
mkaccessor(FILE *, getfile, cellisfile, ptr)
mkaccessor(int, getchr, cellischar, fix)

void docons(struct func *func, int nargs) {
  struct cell *car, *top;
  struct cons *cons;
  car = getcell(-1);
  top = getcell(-2);
  cons = (void *)makeobj(TAG_CONS, sizeof(*cons));
  cons->cartag = car->tag;
  cons->car = car->v;
  cons->cdrtag = top->tag;
  cons->cdr = top->v;
  pop(1);
  top->tag = TAG_CONS;
  top->v.ptr = cons;
}

void docar(struct func *func, int nargs) {
  struct cell top;
  struct cons *cons;
  if (cellisnil(-1))
    return;
  cons = getcons(-1);
  top.tag = cons->cartag;
  top.v = cons->car;
  push(&top);
  shift(1);
}

void docdr(struct func *func, int nargs) {
  struct cell top;
  struct cons *cons;
  if (cellisnil(-1))
    return;
  cons = getcons(-1);
  top.tag = cons->cdrtag;
  top.v = cons->cdr;
  push(&top);
  shift(1);
}

void dosetcar(struct func *func, int nargs) {
  struct cons *cons;
  struct cell *val;
  cons = getcons(-1);
  val = getcell(-2);
  cons->cartag = val->tag;
  cons->car = val->v;
  pop(2);
}

void dosetcdr(struct func *func, int nargs) {
  struct cons *cons;
  struct cell *val;
  cons = getcons(-1);
  val = getcell(-2);
  cons->cdrtag = val->tag;
  cons->cdr = val->v;
  pop(2);
}

void domul(struct func *func, int nargs) {
  long a, b;
  a = getfix(-1);
  b = getfix(-2);
  pop(2);
  pushfix(a * b);
}

void doadd(struct func *func, int nargs) {
  long a, b;
  a = getfix(-1);
  b = getfix(-2);
  pop(2);
  pushfix(a + b);
}

void dosub(struct func *func, int nargs) {
  long a, b;
  a = getfix(-1);
  b = getfix(-2);
  pop(2);
  pushfix(a - b);
}

void doquot(struct func *func, int nargs) {
  long a, b;
  a = getfix(-1);
  b = getfix(-2);
  pop(2);
  pushfix(a / b);
}

void dorem(struct func *func, int nargs) {
  long a, b;
  a = getfix(-1);
  b = getfix(-2);
  pop(2);
  pushfix(a % b);
}

void doand(struct func *func, int nargs) {
  long a, b;
  a = getfix(-1);
  b = getfix(-2);
  pop(2);
  pushfix(a & b);
}

void door(struct func *func, int nargs) {
  long a, b;
  a = getfix(-1);
  b = getfix(-2);
  pop(2);
  pushfix(a | b);
}

void doxor(struct func *func, int nargs) {
  long a, b;
  a = getfix(-1);
  b = getfix(-2);
  pop(2);
  pushfix(a ^ b);
}

void donot(struct func *func, int nargs) {
  long a;
  a = getfix(-1);
  pop(1);
  pushfix(~a);
}

void doash(struct func *func, int nargs) {
  long a, sh;
  a = getfix(-1);
  sh = getfix(-2);
  pop(2);
  for (; sh > 0; sh--)
    a <<= 1;
  for (; sh < 0; sh++)
    a >>= 1;
  pushfix(a);
}

int symequal(struct sym *a, struct sym *b) {
  if (a == b)
    return 1;
  return a->size == b->size && !memcmp(a->sym, b->sym, a->size);
}

int celleql(struct cell *a, struct cell *b) {
  if (a == b)
    return 1;
  if (a->tag != b->tag)
    return 0;
  switch (gettag(-1)) {
  case TAG_NIL:
    return 1;
  case TAG_UNDEF:
    return 0;
  case TAG_FIXNUM: case TAG_CHAR:
    return a->v.fix == b->v.fix;
  case TAG_SYM:
    return symequal(a->v.ptr, b->v.ptr);
  default:
    return a->v.ptr == b->v.ptr;
  }
}

int iseql(struct func *func, int nargs) {
  int eql = celleql(getcell(-1), getcell(-2));
  pop(2);
  return eql;
}

int csymequal(const char *s) {
  struct sym *sym = getsym(-1);
  return sym->size == strlen(s) && !memcmp(sym->sym, s, sym->size);
}

int strequal(struct str *a, struct str *b) {
  if (a == b)
    return 1;
  return a->fill == b->fill && !memcmp(a->buf, b->buf, a->fill);
}

int cellequal(struct cell *a, struct cell *b) {
  struct cell ca, cb;
  if (celleql(a, b))
      return 1;
  if (a->tag != b->tag)
    return 0;
  switch (a->tag) {
  case TAG_NIL:
    return 1;
  case TAG_UNDEF:
    return 0;
  case TAG_FIXNUM: case TAG_CHAR:
    return a->v.fix == b->v.fix;
  case TAG_SYM:
    return symequal(a->v.ptr, b->v.ptr);
  case TAG_CONS:
    ca.tag = ((struct cons *)a->v.ptr)->cartag;
    ca.v = ((struct cons *)a->v.ptr)->car;
    cb.tag = ((struct cons *)b->v.ptr)->cartag;
    cb.v = ((struct cons *)b->v.ptr)->car;
    if (!cellequal(&ca, &cb))
      return 0;
    ca.tag = ((struct cons *)a->v.ptr)->cdrtag;
    ca.v = ((struct cons *)a->v.ptr)->cdr;
    cb.tag = ((struct cons *)b->v.ptr)->cdrtag;
    cb.v = ((struct cons *)b->v.ptr)->cdr;
    return cellequal(&ca, &cb);
  case TAG_STR:
    return strequal(a->v.ptr, b->v.ptr);
  case TAG_BOX:
  default:
    return a == b;
  }
}

int isequal(struct func *func, int nargs) {
  int eql = cellequal(getcell(-1), getcell(-2));
  pop(2);
  return eql;
}

void eqlp(struct func *func, int nargs) {
  pushbool(iseql(func, nargs));
}

void equalp(struct func *func, int nargs) {
  pushbool(isequal(func, nargs));
}

void findrootbox(struct gcroot *root) {
  struct cell *name = getcell(-1);
  int top = rootfill(root), i;
  for (i = 0; i < top; i += 2)
    if (cellequal(name, &root->cells[i])) {
      pushroot(root, i + 1);
      shift(1);
      return;
    }
  resizeroot(root, top + 2);
  pushdup(-1);
  poproot(root, top);
  pushnil();
  getcell(-1)->tag = TAG_UNDEF;
  pushdup(-2);
  docons(NULL, 2);
  getcell(-1)->tag = TAG_BOX;
  pushdup(-1);
  poproot(root, top + 1);
  shift(1);
}

#define funbox() findrootbox(&gcroots[ROOT_FUNCTIONS])
#define macrobox() findrootbox(&gcroots[ROOT_MACROS])
#define varbox() findrootbox(&gcroots[ROOT_GLOBALS])

int getlexical(int scope) {
  int depth = 0;
  pushroot(&gcroots[ROOT_CONSTANTS], scope);
  while (!cellisnil(-1)) {
    pushdup(-1);
    docar(NULL, 1);
    while (!cellisnil(-1)) {
      pushdup(-1);
      docar(NULL, 1);
      pushdup(-1);
      docar(NULL, 1);
      pushdup(-5);
      if (isequal(NULL, 1)) {
        docdr(NULL, 1);
        depth = getfix(-1);
        pop(4);
        return depth;
      }
      pop(1);
      docdr(NULL, 1);
    }
    pop(1);
    docdr(NULL, 1);
  }
  pop(2);
  return 0;
}

int gettoplexical(int scope) {
  int depth = 0;
  pushroot(&gcroots[ROOT_CONSTANTS], scope);
  docar(NULL, 1);
  while (!cellisnil(-1)) {
    pushdup(-1);
    docar(NULL, 1);
    pushdup(-1);
    docar(NULL, 1);
    pushdup(-5);
    if (isequal(NULL, 1)) {
      docdr(NULL, 1);
      depth = getfix(-1);
      pop(3);
      return depth;
    }
    pop(1);
    docdr(NULL, 1);
  }
  pop(2);
  return 0;
}

void deflexical(int scope, int depth) {
  pushroot(&gcroots[ROOT_CONSTANTS], scope);
  docar(NULL, 1);
  pushfix(depth);
  pushdup(-3);
  docons(NULL, 2);
  docons(NULL, 2);
  pushroot(&gcroots[ROOT_CONSTANTS], scope);
  dosetcar(NULL, 1);
  pop(1);
}

#define getlexvar() getlexical(CONST_LEXICALS)
#define deflexvar(depth) deflexical(CONST_LEXICALS, depth)
#define gettoplextag() gettoplexical(CONST_TAGLABELS)
#define deflextag(lab) deflexical(CONST_TAGLABELS, lab)

void pushlexical(int scope) {
  pushroot(&gcroots[ROOT_CONSTANTS], scope);
  pushnil();
  docons(NULL, 2);
  poproot(&gcroots[ROOT_CONSTANTS], scope);
}

void poplexical(int scope) {
  pushroot(&gcroots[ROOT_CONSTANTS], scope);
  docdr(NULL, 1);
  poproot(&gcroots[ROOT_CONSTANTS], scope);
}

int lisnewline(int ch) {
  return ch == '\n';
}

int lisspace(int ch) {
  return lisnewline(ch) || ch == ' '  || ch == '\t' ||
                     ch == '\f' || ch == '\r';
}

int listerm(int ch) {
  return lisspace(ch) || ch == '"' || ch == '\'' || ch == '(' ||
                   ch == ')' || ch == ','  || ch == ';' ||
             ch == '`';
}

void skipws(void) {
  int ch;
  while ((ch = getc(stdin)) != EOF) {
    if (ch == ';')
      while (ch != EOF && !lisnewline(ch))
        ch = getc(stdin);
    if (!lisspace(ch))
      break;
  }
  ungetc(ch, stdin);
}

static char *readbuf = NULL;
static size_t readsize = 0, readcap = 0;

void pushbuf(int ch) {
  char *temp;
  if (readsize == readcap) {
    readcap += 64 + (readcap >> 3);
    temp = realloc(readbuf, readcap);
    if (!temp)
      die("nomem");
    readbuf = temp;
  }
  readbuf[readsize++] = ch;
}

void readesc(void) {
  int ch = getc(stdin);
  if (ch == EOF)
    die("EOF");
  pushbuf(ch);
}

void readstr(void) {
  struct str *s;
  int ch;
  while ((ch = getc(stdin)) != '"') {
    if (ch == EOF)
      die("EOF");
    if (ch == '\\')
      readesc();
    pushbuf(ch);
  }
  mkstr(readbuf, readsize);
  s = getstr(-1);
  s->ro = 1;
  readsize = 0;
}

int basedig(int c, int base, int neg) {
  int dig;
  switch (base) {
  case 2:
    if (c < '0' || c > '1')
      return 0;
    dig = c - '0';
  case 8:
    if (c < '0' || c > '7')
      return 0;
    dig = c - '0';
    break;
  case 10:
    if (c < '0' || c > '9')
      return 0;
    dig = c - '0';
    break;
  case 16:
    if (c >= '0' && c <= '9')
      dig = c - '0';
    else if (c >= 'a' && c <= 'f')
      dig = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F')
      dig = c - 'a' + 10;
    else
      return 0;
    break;
  default:
    return 0;
  }
  pushfix(base);
  domul(NULL, 2);
  pushfix(neg ? -dig : dig);
  doadd(NULL, 2);
  return 1;
}

int parsenum(char *s, size_t sz) {
  int base = 10, neg = 0;
  if (sz && (s[0] == '+' || s[0] == '-'))
    neg = (s[0] == '-'), s++, sz--;
  if (sz > 2 && s[0] == '#') {
    switch (s[1]) {
    case 'b': case 'B':
      base = 2;
      break;
    case 'o': case 'O':
      base = 8;
      break;
    case 'x': case 'X':
      base = 16;
      break;
    default:
      return 0;
    }
    s += 2, sz -= 2;
  }
  if (!sz)
    return 0;
  pushfix(0);
  for (; sz; s++, sz--) {
    if (!basedig(s[0], base, neg)) {
      pop(1);
      return 0;
    }
  }
  return 1;
}

int parsechr(char *s, size_t sz) {
  unsigned int ch;
  if (sz < 2 || s[0] != '#' || s[1] != '\\')
    return 0;
  if (sz == 2) {
    die("bad character");
  } else if (sz == 3) {
    pushchr(s[2]);
    return 1;
  } else if (s[2] == 'x') {
    s[sz] = 0;
    sscanf(s + 3, "%x", &ch);
    if (ch < 0 || ch > 0xff)
      die("bad character");
    pushchr(ch);
    return 1;
  } else {
    s[sz] = 0;
    if (!strcmp(s + 2, "Newline"))
      ch = '\n';
    else if (!strcmp(s + 2, "Linefeed"))
      ch = '\n';
    else if (!strcmp(s + 2, "Formfeed"))
      ch = '\f';
    else if (!strcmp(s + 2, "Page"))
      ch = '\f';
    else if (!strcmp(s + 2, "Backspace"))
      ch = '\b';
    else if (!strcmp(s + 2, "Rubout"))
      ch = '\177';
    else if (!strcmp(s + 2, "Delete"))
      ch = '\177';
    else if (!strcmp(s + 2, "Tab"))
      ch = '\t';
    else if (!strcmp(s + 2, "Space"))
      ch = ' ';
    else if (!strcmp(s + 2, "Return"))
      ch = '\r';
    else if (!strcmp(s + 2, "Nul"))
      ch = '\0';
    else if (!strcmp(s + 2, "Null"))
      ch = '\0';
    else
      die("bad character");
    pushchr(ch);
    return 1;
  }
}

int parsesym(char *s, size_t sz) {
  size_t i, j;
  if (!sz)
    die("empty symbol");
  if (s[0] == '#') {
    die("reserved symbol");
  }
  for (i = 0, j = 0; i < sz; i++) {
    if (s[i] == '\\')
      i++;
    s[j++] = s[i];
  }
  mksym(s, j);
  return 1;
}

void readtok(void) {
  int ch;
  while ((ch = getc(stdin)) != EOF && !listerm(ch)) {
    pushbuf(ch);
    if (ch == '\\')
      readesc();
  }
  ungetc(ch, stdin);
  if (!parsenum(readbuf, readsize) && !parsechr(readbuf, readsize)
      && !parsesym(readbuf, readsize)) {
    die("bad token");
  }
  readsize = 0;
}

void read(struct func *func, int nargs) {
  int ch;
  skipws();
  switch ((ch = getc(stdin))) {
  case '"':
    readstr();
    return;

  case '`':
    pushnil();
    read(NULL, 0);
    docons(NULL, 2);
    csym("quasiquote");
    docons(NULL, 2);
    return;

  case ',':
    ch = getc(stdin);
    if (ch != '@')
      ungetc(ch, stdin);
    pushnil();
    read(NULL, 0);
    docons(NULL, 2);
    csym(ch == '@' ? "unquote-splicing" : "unquote");
    docons(NULL, 2);
    return;

  case '\'':
    pushnil();
    read(NULL, 0);
    docons(NULL, 2);
    csym("quote");
    docons(NULL, 2);
    return;

  case '(':
    pushnil();
    pushnil();
    while ((ch = getc(stdin)) != ')') {
      ungetc(ch, stdin);
      pushnil();
      read(NULL, 0);
      if (cellissym(-1) && csymequal("+EOF+"))
        die("cons eof");
      docons(NULL, 2);
      if (cellisnil(-2)) {
        shift(2);
        pushdup(-1);
      } else {
        pushdup(-1);
        pushdup(-3);
        dosetcdr(NULL, 2);
        shift(1);
      }
      skipws();
    }
    pop(1);
    return;

  default:
    ungetc(ch, stdin);
    readtok();
    return;

  case ')':
    die("unmatched )");

  case EOF:
    pushroot(&gcroots[ROOT_CONSTANTS], CONST_EOF);
    return;
  }
}

void print(struct func *func, int nargs) {
  struct sym *sym;
  struct str *s;
  struct vec *v;
  char *ptr, *end;
  int istwolist, i;
  if (cellisnil(-1)) {
    fputs("nil", stdout);
  } else if (cellisundef(-1)) {
    fputs("**UNDEFINED**", stdout);
  } else if (cellisfix(-1)) {
    printf("%ld", getcell(-1)->v.fix);
  } else if (cellissym(-1)) {
    sym = getsym(-1);
    printf("%.*s", sym->size, sym->sym);
  } else if (cellisbox(-1)) {
    printf("#box<");
    pushdup(-1);
    docar(NULL, 1);
    print(NULL, 1);
    pop(1);
    fputc('.', stdout);
    pushdup(-1);
    docdr(NULL, 1);
    print(NULL, 1);
    pop(1);
    fputc('>', stdout);
  } else if (celliscons(-1)) {
    istwolist = 1;
    pushdup(-1);
    docdr(NULL, 1);
    istwolist = istwolist && !cellisnil(-1);
    docdr(NULL, 1);
    istwolist = istwolist && cellisnil(-1);
    pop(1);
    pushdup(-1);
    docar(NULL, 1);
    if (istwolist && csymequal("quote")) {
      pop(1);
      docdr(NULL, 1);
      docar(NULL, 1);
      fputc('\'', stdout);
      print(NULL, 1);
    } else if (istwolist && csymequal("quasiquote")) {
      pop(1);
      docdr(NULL, 1);
      docar(NULL, 1);
      fputc('`', stdout);
      print(NULL, 1);
    } else if (istwolist && csymequal("unquote")) {
      pop(1);
      docdr(NULL, 1);
      docar(NULL, 1);
      fputc(',', stdout);
      print(NULL, 1);
    } else if (istwolist && csymequal("unquote-splicing")) {
      pop(1);
      docdr(NULL, 1);
      docar(NULL, 1);
      fputc(',', stdout);
      fputc('@', stdout);
      print(NULL, 1);
    } else {
      pop(1);
      putc('(', stdout);
      pushdup(-1);
      while (1) {
        pushdup(-1);
        docar(NULL, 1);
        print(NULL, 1);
        pop(1);
        docdr(NULL, 1);
        if (!celliscons(-1))
          break;
        putc(' ', stdout);
      }
      if (!cellisnil(-1)) {
        fputs(" . ", stdout);
        print(NULL, 1);
      }
      pop(1);
      putc(')', stdout);
    }
  } else if (cellisfunc(-1)) {
    sym = (void *)getfunc(-1)->name.v.ptr;
    fprintf(stdout, "#<func %.*s>", sym->size, sym->sym);
  } else if (cellisstr(-1)) {
    s = getstr(-1);
    putc('"', stdout);
    ptr = s->buf;
    end = ptr + s->fill;
    for (; ptr < end; ptr++) {
      if (*ptr == '"')
        putc('\\', stdout);
      putc(*ptr, stdout);
    }
    putc('"', stdout);
  } else if (cellisvec(-1)) {
    putc('#', stdout);
    putc('(', stdout);
    v = getvec(-1);
    for (i = 0; i < v->fill; i++) {
      push(&v->vals[i]);
      print(NULL, 1);
      pop(1);
      if (i < v->fill - 1)
        putc(' ', stdout);
    }
    putc(')', stdout);
  } else if (cellisfile(-1)) {
    fprintf(stdout, "#<file %p>", getfile(-1));
  } else if (cellischar(-1)) {
    switch (getchr(-1)) {
    case '\n':
      fprintf(stdout, "#\\Newline", getchr(-1));
      break;
    case '\r':
      fprintf(stdout, "#\\Return", getchr(-1));
      break;
    case ' ':
      fprintf(stdout, "#\\Space", getchr(-1));
      break;
    case '\b':
      fprintf(stdout, "#\\Backspace", getchr(-1));
      break;
    case '\177':
      fprintf(stdout, "#\\Delete", getchr(-1));
      break;
    case '\f':
      fprintf(stdout, "#\\Page", getchr(-1));
      break;
    case '\t':
      fprintf(stdout, "#\\Tab", getchr(-1));
      break;
    default:
      if (getchr(-1) < ' ' || getchr(-1) > 0x7f)
        fprintf(stdout, "#\\x%02x", getchr(-1));
      else
        fprintf(stdout, "#\\%c", getchr(-1));
      break;
    }
  } else {
    printf("%s\n", tag2str(gettag(-1)));
    die("unknown tag");
  }
  if (func)
    putc('\n', stdout);
}

void terpri(struct func *func, int nargs) {
  putc('\n', stdout);
  pushnil();
}

int *code = NULL;
int codefill = 0, codecap = 0, codenvals = 0;
int codetop = 0, tagtop = -1;

void pushop(int op) {
  void *temp;
  if (codefill == codecap) {
    codecap += 64 + (codecap >> 3);
    temp = realloc(code, codecap * sizeof(*code));
    if (!temp)
      die("nomem");
    code = temp;
  }
  code[codefill++] = op;
}

int saveconst(void) {
  struct gcroot *root = &gcroots[ROOT_VALTAB];
  struct cell *top = getcell(-1);
  int i, ret = rootfill(root);
  for (i = 0; i < ret; i++)
    if (cellequal(&root->cells[i], top)) {
      pop(1);
      return i;
    }
  resizeroot(root, ret + 1);
  poproot(root, ret);
  return ret;
}

int savedyn(void) {
  return saveconst();
}

void mkfunc(int nargs) {
  void *tmp;
  struct func *func;
  struct cell top = {TAG_FUNC};
  struct gcroot *valtab = &gcroots[ROOT_VALTAB];
  int nvaltab = rootfill(valtab);
  if (nvaltab < 0)
    die("no func name");
  func = (void *)makeobj(TAG_FUNC, sizeof(*func) + nvaltab * sizeof(struct cell));
  tmp = realloc(code, codefill * sizeof(*code));
  if (!tmp)
    die("nomem");
  room += codefill * sizeof(*code);
  func->code = tmp;
  func->ncode = codefill;
  func->run = NULL;
  func->nargs = nargs;
  func->nvals = nvaltab;
  func->nclos = 0;
  memcpy(func->vals, valtab->cells, nvaltab * sizeof(struct cell));
  func->name = func->vals[0];
  code = NULL;
  codefill = 0;
  codecap = 0;
  resizeroot(valtab, 0);
  top.v.ptr = func;
  push(&top);
}

void doclosure(struct func *func, int nlocals, int nargs, int *code, int end) {
  struct func *clo;
  struct cell top = {TAG_FUNC};
  int *dupcode, tabsz;
  if (end < 0)
    die("empty closure");
  dupcode = malloc(sizeof(*code) * end);
  if (!dupcode)
    die("nomem");
  room += end * sizeof(*code);
  memcpy(dupcode, code, sizeof(*code) * end);
  tabsz = func->nvals + nlocals;
  clo = (void *)makeobj(TAG_FUNC, sizeof(*clo) + tabsz * sizeof(struct cell));
  clo->code = dupcode;
  clo->run = NULL;
  clo->nargs = nargs;
  clo->nvals = func->nvals;
  clo->ncode = end;
  clo->nclos = nlocals;
  memcpy(clo->vals, func->vals, func->nvals * sizeof(struct cell));
  pop1(&clo->name);
  if (nlocals)
    memcpy(clo->vals + func->nvals, getcell(-1) + 1 - nlocals, nlocals * sizeof(struct cell));
  pop(nlocals);
  top.v.ptr = clo;
  push(&top);
}

void defbuiltin(const char *name, int nargs, void (*fp)(struct func *, int)) {
  struct func *func;
  mksym(name, strlen(name));
  pushdup(-1);
  saveconst();
  mkfunc(nargs);
  func = getfunc(-1);
  func->run = fp;
  pushdup(-2);
  funbox();
  dosetcdr(NULL, 2);
  pop(1);
}

void printtb(void) {
  struct gcroot *root = &gcroots[ROOT_TRACEBACK];
  int i, top = rootfill(root);
  struct sym *name;
  for (i = 0; i < top; i++) {
    name = ((struct func *)root->cells[i].v.ptr)->name.v.ptr;
    fprintf(stderr, "tb: %.*s\n", name->size, name->sym);
  }
}

void pushtb(void) {
  struct gcroot *root = &gcroots[ROOT_TRACEBACK];
  int top = rootfill(root);
  resizeroot(root, top + 1);
  pushdup(-1);
  poproot(root, top);
}

void poptb(void) {
  struct gcroot *root = &gcroots[ROOT_TRACEBACK];
  resizeroot(root, rootfill(root) - 1);
}

void dofunc(struct func *func, int nargs);

void docall(int nargs) {
  struct func *fun = getfunc(-1);
  int top = stackdepth(), depth;
  pushtb();
  if (nargs < 0)
    die("bad nargs");
  if (fun->nargs >= 0 && nargs != fun->nargs)
    die("wrong number of args");
  pop(1);
  if (fun->run)
    fun->run(fun, nargs);
  else
    dofunc(fun, nargs);
  top -= nargs;
  depth = stackdepth();
  if (depth != top) {
    fprintf(stderr, "got: %d returns\n", depth - top);
    die("wrong number of returns");
  }
  poptb();
}

void docallrev(int nrev) {
  int i, j;
  struct cell *top;
  struct cell temp;
  if (nrev < 0)
    die("bad nargs");
  top = getcell(-1) + 1;
  for (i = 2, j = nrev + 1; i < j; i++, j--) {
    temp = top[-i];
    top[-i] = top[-j];
    top[-j] = temp;
  }
  docall(nrev);
}

enum opcode {
  OP_CONST,
  OP_VAR,
  OP_CALLREV,
  OP_RET,
  OP_POP,
  OP_SET,
  OP_SETVAR,
  OP_PUSH,
  OP_SHIFT,
  OP_JMPNIL,
  OP_JMP,
  OP_CLOSURE,
  OP_CVAR,
};

void dofunc(struct func *func, int nargs) {
  int *code = func->code, val, b, c;
  struct sym *sym;
  if (nargs > stackdepth())
    die("nargs > stacktop");
  if (func->nargs >= 0 && func->nargs != nargs)
    die("wrong number of arguments");
  while (1) {
    switch (*code++) {
    case OP_CONST:
      val = *code++;
      if (val < 0 || val > func->nvals)
        die("bad constant");
      push(&func->vals[val]);
      break;
    case OP_VAR:
      val = *code++;
      if (val < 0 || val > func->nvals)
        die("bad constant");
      push(&func->vals[val]);
      docdr(NULL, 1);
      if (cellisundef(-1)) {
        pop(1);
        push(&func->vals[val]);
	docar(NULL, 1);
	sym = getsym(-1);
        fprintf(stderr, "loaded undefined: %.*s\n", sym->size, sym->sym);
        die("loaded undefined");
      }
      break;
    case OP_RET:
      return;
    case OP_POP:
      pop(1);
      break;
    case OP_CALLREV:
      docallrev(*code++);
      break;
    case OP_SET:
      doset(*code++);
      break;
    case OP_SETVAR:
      val = *code++;
      if (val < 0 || val > func->nvals)
        die("bad constant");
      push(&func->vals[val]);
      dosetcdr(NULL, 1);
      break;
    case OP_PUSH:
      pushdup(*code++);
      break;
    case OP_SHIFT:
      shift(*code++);
      break;
    case OP_JMPNIL:
      val = *code++;
      if (cellisnil(-1))
        code += val;
      pop(1);
      break;
    case OP_JMP:
      val = *code++;
      code += val;
      break;
    case OP_CLOSURE:
      val = *code++;
      b = *code++;
      c = *code++;
      doclosure(func, val, b, code, c);
      code += c;
      break;
    case OP_CVAR:
      val = *code++;
      if (val < 0 || val > func->nclos)
        die("bad cvar");
      push(&func->vals[val + func->nvals]);
      break;
    default:
      printf("*code: %d (0x%x)\n", code[-1], code[-1]);
      die("bad code");
    }
  }
}

void showvals(struct func *func) {
  int i;
  printf("++++++\n");
  push(&func->name);
  print(NULL, 1);
  pop(1);
  printf("(%d)\n", func->nargs);
  for (i = 0; i < func->nvals; i++) {
    push(&func->vals[i]);
    printf("%4d: ", i);
    print(func, 1);
    pop(1);
  }
  printf("------\n");
  for (i = 0; i < func->nclos; i++) {
    push(&func->vals[func->nvals + i]);
    printf("%4d: ", i);
    print(func, 1);
    pop(1);
  }
  printf("------\n");
}

void showcode(struct func *func) {
  int *code = func->code, i;
  if (func->run)
    printf("**BUILTIN**\n");
  for (i = 0; i < func->ncode; i++) {
    switch (code[i]) {
    case OP_CONST:
      printf("OP_CONST(%d)\n", code[++i]);
      break;
    case OP_VAR:
      printf("OP_VAR(%d)\n", code[++i]);
      break;
    case OP_CALLREV:
      printf("OP_CALLREV(%d)\n", code[++i]);
      break;
    case OP_RET:
      printf("OP_RET\n");
      break;
    case OP_POP:
      printf("OP_POP\n");
      break;
    case OP_SET:
      printf("OP_SET(%d)\n", code[++i]);
      break;
    case OP_SETVAR:
      printf("OP_SETVAR(%d)\n", code[++i]);
      break;
    case OP_PUSH:
      printf("OP_PUSH(%d)\n", code[++i]);
      break;
    case OP_SHIFT:
      printf("OP_SHIFT(%d)\n", code[++i]);
      break;
    case OP_JMPNIL:
      printf("OP_JMPNIL(%d)\n", code[++i]);
      break;
    case OP_JMP:
      printf("OP_JMP(%d)\n", code[++i]);
      break;
    case OP_CLOSURE:
      printf("OP_CLOSURE(");
      printf("nvals=%d, ", code[++i]);
      printf("nargs=%d, ", code[++i]);
      printf("end=%d", code[++i]);
      printf(")\n");
      break;
    case OP_CVAR:
      printf("OP_CVAR(%d)\n", code[++i]);
      break;
    default:
      printf("*code: %d (0x%x)\n", code[i], code[i]);
      die("bad code");
    }
  }
  printf("++++++\n");
}

void doshowcode(struct func *func, int nargs) {
  struct func *victim = getfunc(-1);
  showvals(victim);
  showcode(victim);
}

void compilecall(void);
void compileclosure(void);

void compilefunc(void) {
  if (!cellissym(-1)) {
    pushdup(-1);
    docar(NULL, 1);
    if (!csymequal("lambda"))
      die("must either be lambda or function name");
    pop(1);
    compileclosure();
    return;
  }
  pushop(OP_VAR);
  funbox();
  pushop(savedyn());
  codetop++;
}

void compilevar(void) {
  int depth;
  pushdup(-1);
  depth = getlexvar();
  if (depth < 0) {
    pop(1);
    pushop(OP_CVAR);
    pushop(-depth - 1);
    codetop++;
  } else if (!depth) {
    pushop(OP_VAR);
    codetop++;
    varbox();
    pushop(savedyn());
  } else {
    pop(1);
    pushop(OP_PUSH);
    pushop(depth - codetop - 1);
    codetop++;
  }
}

void compileconst(void) {
  pushop(OP_CONST);
  pushop(saveconst());
  codetop++;
}

void macroexpand(struct func *func, int nargs) {
  while (celliscons(-1)) {
    pushdup(-1);
    docar(NULL, 1);
    macrobox();
    if (cellisnil(-1)) {
      pop(1);
      return;
    }
    docdr(NULL, 1);
    if (!cellisfunc(-1)) {
      pop(1);
      return;
    }
    pushdup(-2);
    docdr(NULL, 1);
    pushdup(-2);
    docall(1);
    shift(2);
  }
}

void compileexpr(void) {
  macroexpand(NULL, 1);
  switch (gettag(-1)) {
  case TAG_SYM:
    compilevar();
    break;
  case TAG_CONS:
    compilecall();
    break;
  default:
    compileconst();
    break;
  }
}

void compileprogn(void) {
  docdr(NULL, 1);
  while (1) {
    pushdup(-1);
    docar(NULL, 1);
    compileexpr();
    docdr(NULL, 1);
    if (cellisnil(-1))
      break;
    pushop(OP_POP);
    codetop--;
  }
  pop(1);
}

int compileargs(void) {
  int n = 0;
  while (!cellisnil(-1)) {
    pushdup(-1);
    docar(NULL, 1);
    compileexpr();
    docdr(NULL, 1);
    n += 1;
  }
  pop(1);
  return n;
}

void compilequote() {
  docdr(NULL, 1);
  pushdup(-1);
  docar(NULL, 1);
  compileconst();
  docdr(NULL, 1);
  if (!cellisnil(-1))
    die("quote with more than one thing");
  pop(1);
}

void compileapply(void) {
  int n;
  pushdup(-1);
  docar(NULL, 1);
  if (csymequal("funcall")) {
    pop(1);
    pushdup(-1);
    docdr(NULL, 1);
    docdr(NULL, 1);
    n = compileargs();
    docdr(NULL, 1);
    docar(NULL, 1);
    compileexpr();
  } else {
    pop(1);
    pushdup(-1);
    docdr(NULL, 1);
    n = compileargs();
    docar(NULL, 1);
    compilefunc();
  }
  pushop(OP_CALLREV);
  codetop -= n;
  pushop(n);
}

void compilelet(void) {
  int n = 0;
  pushlexical(CONST_LEXICALS);
  docdr(NULL, 1);
  pushdup(-1);
  docar(NULL, 1);
  while (!cellisnil(-1)) {
    pushdup(-1);
    docar(NULL, 1);
    pushdup(-1);
    docdr(NULL, 1);
    pushdup(-1);
    docdr(NULL, 1);
    if (!cellisnil(-1))
      die("let expects (var expr) bindings");
    pop(1);
    docar(NULL, 1);
    compileexpr();
    docar(NULL, 1);
    deflexvar(codetop);
    n++;
    docdr(NULL, 1);
  }
  pop(1);
  compileprogn();
  poplexical(CONST_LEXICALS);
  if (n > 0) {
    pushop(OP_SHIFT);
    pushop(n);
  }
  codetop -= n;
}

void compileset(void) {
  int depth;
  docdr(NULL, 1);
  pushdup(-1);
  docdr(NULL, 1);
  pushdup(-1);
  docdr(NULL, 1);
  if (!cellisnil(-1))
    die("expected (set x expr)");
  pop(1);
  docar(NULL, 1);
  compileexpr();
  docar(NULL, 1);
  pushdup(-1);
  depth = getlexvar();
  if (!depth) {
    pushop(OP_PUSH);
    pushop(-1);
    codetop++;
    pushop(OP_SETVAR);
    varbox();
    pushop(savedyn());
    codetop--;
  } else {
    pop(1);
    pushop(OP_PUSH);
    pushop(-1);
    codetop++;
    pushop(OP_SET);
    pushop(depth - codetop - 1);
    codetop--;
  }
}

int *labels = NULL;
int labfill = 0, labcap = 0;

int pushlab(void) {
  int lab;
  void *temp;
  if (labfill == labcap) {
    labcap += 64 + (labcap >> 3);
    temp = realloc(labels, labcap * sizeof(*labels));
    if (!temp)
      die("nomem");
    labels = temp;
  }
  if (labfill == 0)
    labfill++;
  labels[(lab = labfill++)] = -1;
  return lab;
}

void setlab(int lab) {
  if (lab < 0 || lab > labfill)
    die("invalid label");
  labels[lab] = codefill;
}

void resolvelab(void) {
  int i, lab;
  if (!code)
    return;
  for (i = 0; i < codefill; i++) {
    switch (code[i]) {
    case OP_CONST: case OP_VAR: case OP_CALLREV:
    case OP_SET: case OP_SETVAR: case OP_PUSH: case OP_SHIFT:
    case OP_CVAR:
      i++;
      break;
    case OP_POP: case OP_RET:
      break;
    case OP_CLOSURE:
      i += 2;
    case OP_JMPNIL: case OP_JMP:
      lab = code[i + 1];
      if (!lab || labels[lab] == -1)
        die("bad label");
      code[i + 1] = labels[lab] - i - 2;
      i++;
      break;
    default:
      printf("*code: %d (0x%x)\n", code[i], code[i]);
      die("bad code");
    }
  }
  labfill = 0;
}

void compilecond(void) {
  int next, end, top;
  next = pushlab();
  end = pushlab();
  docdr(NULL, 1);
  while (!cellisnil(-1)) {
    pushdup(-1);
    docar(NULL, 1);
    pushdup(-1);
    docar(NULL, 1);
    setlab(next);
    next = pushlab();
    top = codetop;
    compileexpr();
    if (codetop != top + 1)
      die("expected exactly one return from cond expr");
    pushop(OP_JMPNIL);
    pushop(next);
    codetop--;
    compileprogn();
    if (codetop != top + 1)
      die("expected exactly one return from cond expr");
    pushop(OP_JMP);
    pushop(end);
    codetop--;
    docdr(NULL, 1);
  }
  setlab(next);
  compileconst();
  setlab(end);
}

int compileclolocals(void) {
  int nlocals = 0;
  pushroot(&gcroots[ROOT_CONSTANTS], CONST_LEXICALS);
  while (!cellisnil(-1)) {
    pushdup(-1);
    docar(NULL, 1);
    while (!cellisnil(-1)) {
      pushdup(-1);
      docar(NULL, 1);
      docar(NULL, 1);
      pushdup(-1);
      compilevar();
      nlocals++;
      deflexvar(-nlocals);
      docdr(NULL, 1);
    }
    pop(1);
    docdr(NULL, 1);
  }
  pop(1);
  return nlocals;
}

int compilecloargs(void) {
  int n;
  if (cellisnil(-1)) {
    pop(1);
    return 0;
  }
  pushdup(-1);
  docdr(NULL, 1);
  n = compilecloargs();
  docar(NULL, 1);
  codetop++;
  deflexvar(codetop);
  return n + 1;
}

void compileclosure() {
  int top, nlocals, nargs, end, tag;
  pushlexical(CONST_LEXICALS);
  tag = tagtop;
  tagtop = -1;
  end = pushlab();
  top = codetop;
  nlocals = compileclolocals();
  codetop = 0;
  pushdup(-1);
  docdr(NULL, 1);
  docar(NULL, 1);
  nargs = compilecloargs();
  pushdup(-1);
  docar(NULL, 1);
  compileconst();
  pushop(OP_CLOSURE);
  codetop--;
  pushop(nlocals);
  pushop(nargs);
  pushop(end);
  docdr(NULL, 1);
  compileprogn();
  if (nargs) {
    pushop(OP_SHIFT);
    pushop(nargs);
  }
  pushop(OP_RET);
  setlab(end);
  codetop = top + 1;
  tagtop = tag;
  poplexical(CONST_LEXICALS);
}

int mklexlab(void) {
  int lab;
  pushdup(-1);
  lab = gettoplextag();
  if (lab == 0) {
    lab = pushlab();
    deflextag(lab);
  } else
    pop(1);
  return lab;
}

void compiletag(void) {
  int lab = mklexlab();
  setlab(lab);
}

void compilego(void) {
  int lab;
  docdr(NULL, 1);
  pushdup(-1);
  docdr(NULL, 1);
  if (!cellisnil(-1))
    die("expected (go lab)");
  pop(1);
  docar(NULL, 1);
  lab = mklexlab();
  if (codetop < tagtop)
    die("jump up the stack too far");
  else if (codetop > tagtop) {
    if (codetop > tagtop + 1) {
      pushop(OP_SHIFT);
      pushop(codetop - tagtop - 1);
    }
    pushop(OP_POP);
  }
  pushop(OP_JMP);
  pushop(lab);
  pushnil();
  compileconst();
}

void compiletagbody(void) {
  int top, lab;
  pushlexical(CONST_TAGLABELS);
  top = tagtop;
  tagtop = codetop;
  docdr(NULL, 1);
  while (!cellisnil(-1)) {
    pushdup(-1);
    docar(NULL, 1);
    switch (gettag(-1)) {
    case TAG_FIXNUM: case TAG_SYM:
      compiletag();
      break;
    default:
      compileexpr();
      pushop(OP_POP);
      codetop--;
      break;
    }
    docdr(NULL, 1);
  }
  pop(1);
  pushnil();
  compileconst();
  tagtop = top;
  poplexical(CONST_TAGLABELS);
}

void compilecall(void) {
  int n;
  pushdup(-1);
  docar(NULL, 1);
  if (!cellissym(-1) && !celliscons(-1))
    die("call expected ((lambda ...) args...) or (fun args...)");
  if (!cellissym(-1)) {
    pop(1);
    compileapply();
    return;
  }
  if (csymequal("lambda")) {
    pop(1); compileclosure();
  } else if (csymequal("progn")) {
    pop(1); compileprogn();
  } else if (csymequal("quote")) {
    pop(1); compilequote();
  } else if (csymequal("cond")) {
    pop(1); compilecond();
  } else if (csymequal("let")) {
    pop(1); compilelet();
  } else if (csymequal("set")) {
    pop(1); compileset();
  } else if (csymequal("tagbody")) {
    pop(1); compiletagbody();
  } else if (csymequal("go")) {
    pop(1); compilego();
  } else {
    pop(1); compileapply();
  }
}

void checklex(void) {
  pushroot(&gcroots[ROOT_CONSTANTS], CONST_LEXICALS);
  if (!cellisnil(-1))
    die("non-empty lexvars");
  pop(1);
  pushroot(&gcroots[ROOT_CONSTANTS], CONST_TAGLABELS);
  if (!cellisnil(-1))
    die("non-empty lexvars");
  pop(1);
}

void eval(struct func *func, int nargs) {
  static int neval = 0;
  char name[64];
  sprintf(name, "*eval-body-%d*", neval++);
  csym(name);
  saveconst();
  checklex();
  compileexpr();
  checklex();
  pushop(OP_RET);
  resolvelab();
  mkfunc(0);
  docall(0);
}

void dorepl(void) {
  pushnil();
  pushnil();
  csym("repl");
  docons(NULL, 2);
  csym("go");
  docons(NULL, 2);
  docons(NULL, 2);
  pushnil();
  pushnil();
  pushnil();
  csym("read");
  docons(NULL, 2);
  docons(NULL, 2);
  csym("eval");
  docons(NULL, 2);
  docons(NULL, 2);
  csym("print");
  docons(NULL, 2);
  docons(NULL, 2);
  csym("repl");
  docons(NULL, 2);
  csym("tagbody");
  docons(NULL, 2);
  eval(NULL, 1);
}

void fixp(struct func *func, int nargs) {
  int val = cellisfix(-1);
  pop(1);
  pushbool(val);
}

void symp(struct func *func, int nargs) {
  int val = cellissym(-1);
  pop(1);
  pushbool(val);
}

void consp(struct func *func, int nargs) {
  int val = celliscons(-1);
  pop(1);
  pushbool(val);
}

void functionp(struct func *func, int nargs) {
  int val = cellisfunc(-1);
  pop(1);
  pushbool(val);
}

void strp(struct func *func, int nargs) {
  int val = cellisstr(-1);
  pop(1);
  pushbool(val);
}

void characterp(struct func *func, int nargs) {
  int val = cellischar(-1);
  pop(1);
  pushbool(val);
}

void nilsetcar(struct func *func, int nargs) {
  pushdup(-2);
  pushdup(-2);
  dosetcar(func, nargs);
  pop(1);
}

void nilsetcdr(struct func *func, int nargs) {
  pushdup(-2);
  pushdup(-2);
  dosetcdr(func, nargs);
  pop(1);
}

void dodie(struct func *func, int nargs) {
  printf("DODIE: ");
  print(func, nargs);
  die("user requested die");
}

void dofopen(struct func *func, int nargs) {
  struct str *s;
  char *name;
  FILE *file;
  int mode;
  s = getstr(-1);
  mode = !cellisnil(-2);
  name = malloc(s->fill + 1);
  if (!name)
    die("nomem");
  memcpy(name, s->buf, s->fill);
  name[s->fill] = 0;
  pop(2);
  file = fopen(name, mode ? "wb" : "r+");
  free(name);
  if (!file)
    pushnil();
  else
    pushfile(file);
}

void dofputs(struct func *func, int nargs) {
  FILE *file;
  struct str *s;
  file = getfile(-1);
  s = getstr(-2);
  fwrite(s->buf, 1, s->fill, file);
  pop(1);
}

void dofputc(struct func *func, int nargs) {
  FILE *file;
  int ch;
  file = getfile(-1);
  ch = getchr(-2);
  fputc(ch, file);
  pop(1);
}

void dofgetc(struct func *func, int nargs) {
  FILE *file;
  int ch;
  file = getfile(-1);
  ch = fgetc(file);
  pop(1);
  if (ch == EOF)
    pushroot(&gcroots[ROOT_CONSTANTS], CONST_EOF);
  else
    pushchr(ch);
}

void doftell(struct func *func, int nargs) {
  FILE *file = getfile(-1);
  pushfix(ftell(file));
  shift(1);
}

void dofseek(struct func *func, int nargs) {
  FILE *file = getfile(-1);
  long off = getfix(-2);
  fseek(file, off, SEEK_SET);
  shift(1);
}

void dofclose(struct func *func, int nargs) {
  FILE *file = getfile(-1);
  fclose(file);
}

void dolength(struct func *func, int nargs) {
  struct str *s;
  struct vec *v;
  switch (gettag(-1)) {
  case TAG_STR:
    s = getstr(-1);
    pop(1);
    pushfix(s->fill);
    break;
  case TAG_VEC:
    v = getvec(-1);
    pop(1);
    pushfix(v->fill);
    break;
  default:
    die("length ");
  }
}

void dochar(struct func *func, int nargs) {
  struct str *s;
  int i;
  s = getstr(-1);
  i = getfix(-2);
  if (i < 0 || i >= s->fill)
    die("index out of bounds");
  pop(2);
  pushchr(s->buf[i]);
}

void dosetchar(struct func *func, int nargs) {
  struct str *s, *t;
  long i, ch;
  s = getstr(-1);
  i = getfix(-2);
  ch = getchr(-3);
  if (s->ro)
    die("can't modify a read string");
  if (i < 0 || i >= s->fill)
    die("index out of bounds");
  pop(2);
  s->buf[i] = ch;
}

void dosymbol(struct func *func, int nargs) {
  struct str *s;
  if (cellissym(-1))
    return;
  s = getstr(-1);
  pop(1);
  mksym(s->buf, s->fill);
}

void dostring(struct func *func, int nargs) {
  struct sym *sym;
  char tmp[1];
  if (cellisstr(-1))
    return;
  if (cellischar(-1)) {
    tmp[0] = getchr(-1);
    pop(1);
    mkstr(tmp, 1);
    return;
  }
  sym = getsym(-1);
  pop(1);
  mkstr(sym->sym, sym->size);
}

int topbit(unsigned long n) {
  int i = 1;
  while (n > 1)
    n >>= 1, i++;
  return i;
}

void doint2str(struct func *func, int nargs) {
  char temp[80], *p = temp;
  long n, base, top;
  n = getfix(-1);
  base = getfix(-2);
  pop(2);
  if (n < 0)
    *p++ = '-', n = -n;
  switch (base) {
  case 16:
    sprintf(p, "#x%lx", n);
    break;
  case 10:
    sprintf(p, "%lu", n);
    break;
  case 8:
    sprintf(p, "#o%lo", n);
    break;
  case 2:
    top = topbit(n);
    *p++ = '#';
    *p++ = 'b';
    while (top > 0)
      *p++ = '0' + ((n >> (--top)) & 1);
    *p++ = 0;
    break;
  default:
    die("bad base");
  }
  mkstr(temp, strlen(temp));
}

void allocstr(struct str *s, int extra) {
  void *temp;
  if (extra > s->cap || s->fill > s->cap - extra) {
    room -= s->cap;
    s->cap += 64 + extra + (s->cap >> 3);
    temp = realloc(s->buf, s->cap);
    room += s->cap;
    if (!temp)
      die("nomem");
    s->buf = temp;
    poison(s->buf + s->fill, s->cap - s->fill);
  }
}

void resizestr(struct str *s, int len) {
  if (len < 0)
    die("negative size");
  if (s->ro)
    die("can't modify a read string");
  if (len < s->fill) {
    poison(s->buf + len, s->fill - len);
    s->fill = len;
  } else {
    allocstr(s, len - s->fill);
    memset(s->buf + s->fill, 0, len - s->fill);
    s->fill = len;
  }
}

void resizevec(struct vec *v, int fill) {
  void *temp;
  if (fill < 0)
    die("negative size");
  if (fill > v->cap) {
    room -= v->cap * sizeof(struct cell);
    v->cap = 64 + fill + (v->cap >> 3);
    temp = realloc(v->vals, v->cap * sizeof(struct cell));
    room += v->cap * sizeof(struct cell);
    if (!temp)
      die("nomem");
    v->vals = temp;
    poison(v->vals + v->fill, (v->cap - v->fill) * sizeof(struct cell));
  }
  if (fill < v->fill)
    poison(v->vals + fill, (v->fill - fill) * sizeof(struct cell));
  else
    memset(v->vals + v->fill, 0, (fill - v->fill) * sizeof(struct cell));
  v->fill = fill;
}

void dogensym(struct func *func, int nargs) {
  static long ngensym = 0;
  struct str *s;
  int len;
  char *buf;
  s = getstr(-1);
  buf = malloc(s->fill + 64);
  if (!buf)
    die("nomem");
  len = sprintf(buf, "%%%.*s:%ld", s->fill, s->buf, ngensym++);
  pop(1);
  mksym(buf, len);
  free(buf);
}

void dosetlength(struct func *func, int nargs) {
  struct str *s;
  struct vec *v;
  long len = getfix(-2);
  switch (gettag(-1)) {
  case TAG_STR:
    s = getstr(-1);
    resizestr(s, len);
    break;
  case TAG_VEC:
    v = getvec(-1);
    resizevec(v, len);
    break;
  default:
    die("can only set fill-pointer on vec and str");
  }
  shift(1);
}

void dostrcat(struct func *func, int nargs) {
  struct str *s, *b;
  s = getstr(-1);
  b = getstr(-2);
  if (s->ro)
    die("can't modify a read string");
  allocstr(s, b->fill);
  memcpy(s->buf + s->fill, b->buf, b->fill);
  s->fill += b->fill;
  shift(1);
}

void dostrpush(struct func *func, int nargs) {
  struct str *s;
  int ch;
  s = getstr(-1);
  if (s->ro)
    die("can't modify a read string");
  ch = getchr(-2);
  allocstr(s, 1);
  s->buf[s->fill++] = ch;
  shift(1);
}

void dostrpop(struct func *func, int nargs) {
  struct str *s = getstr(-1);
  int top;
  if (!s->fill)
    die("pop from empty string");
  if (s->ro)
    die("can't modify a read string");
  top = s->buf[--s->fill];
  pop(1);
  pushchr(top);
}

void dosubstr(struct func *func, int nargs) {
  struct str *s;
  long a, b;
  s = getstr(-1);
  a = getfix(-2);
  b = getfix(-3);
  if (a < 0)
    a = 0;
  if (b > s->fill)
    b = s->fill;
  if (a > b)
    b = a;
  pop(3);
  mkstr(s->buf + a, b - a);
}

void domakestr(struct func *func, int nargs) {
  mkstr(NULL, 0);
}

void dostrchr(struct func *func, int nargs) {
  struct str *s;
  char *ptr;
  int ch;
  s = getstr(-1);
  ch = getchr(-2);
  pop(2);
  ptr = memchr(s->buf, ch, s->fill);
  if (!ptr)
    pushnil();
  else
    pushfix(ptr - s->buf);
}

void dostrstr(struct func *func, int nargs) {
  struct str *a, *b;
  long i;
  a = getstr(-1);
  b = getstr(-2);
  pop(2);
  for (i = 0; i <= a->fill - b->fill; i++)
    if (!memcmp(a->buf + i, b->buf, b->fill)) {
      pushfix(i);
      return;
    }
  pushnil();
}

void vecpush(struct func *func, int nargs) {
  struct vec *v = getvec(-2);
  resizevec(v, v->fill + 1);
  pop1(&v->vals[v->fill - 1]);
}

void vecpop(struct func *func, int nargs) {
  struct vec *v = getvec(-1);
  if (!v->fill)
    die("vector has no value to pop");
  push(v->vals + v->fill - 1);
  v->fill--;
  poison(v->vals + v->fill, sizeof(struct cell));
}

void vecelt(struct func *func, int nargs) {
  struct vec *v = getvec(-1);
  long i = getfix(-2);
  if (i < 0 || i > v->fill)
    die("index out of vector bounds");
  push(v->vals + i);
  shift(2);
}

void setvecelt(struct func *func, int nargs) {
  struct vec *v = getvec(-1);
  long i = getfix(-2);
  if (i < 0 || i > v->fill)
    die("index out of vector bounds");
  pushdup(-3);
  pop1(v->vals + i);
  pop(2);
}

void dostr2int(struct func *func, int nargs) {
  struct str *s = getstr(-1);
  if (!parsenum(s->buf, s->fill))
    pushnil();
  shift(1);
}

void docharcode(struct func *func, int nargs) {
  int ch = getchr(-1);
  pushfix(ch);
  shift(1);
}

void docodechar(struct func *func, int nargs) {
  long ch = getfix(-1);
  if (ch < 0 || ch > 255)
    die("bad code-char");
  pushchr(ch);
  shift(1);
}

void donumge(struct func *func, int nargs) {
  long a, b;
  int val;
  a = getfix(-1);
  b = getfix(-2);
  val = (a >= b);
  pop(2);
  pushbool(val);
}

void donumle(struct func *func, int nargs) {
  long a, b;
  int val;
  a = getfix(-1);
  b = getfix(-2);
  val = (a <= b);
  pop(2);
  pushbool(val);
}

void donumgt(struct func *func, int nargs) {
  long a, b;
  int val;
  a = getfix(-1);
  b = getfix(-2);
  val = (a > b);
  pop(2);
  pushbool(val);
}

void donumlt(struct func *func, int nargs) {
  long a, b;
  int val;
  a = getfix(-1);
  b = getfix(-2);
  val = (a < b);
  pop(2);
  pushbool(val);
}

void donumeq(struct func *func, int nargs) {
  long a, b;
  int val;
  a = getfix(-1);
  b = getfix(-2);
  val = (a == b);
  pop(2);
  pushbool(val);
}

void dotb(struct func *func, int nargs) {
  printtb();
  pushnil();
}

void getfunction(struct func *func, int nargs) {
  struct sym *sym = getsym(-1);
  funbox();
  docdr(NULL, 1);
  if (cellisundef(-1)) {
    fprintf(stderr, "function undefined %.*s\n", sym->size, sym->sym);
    die("function undefined");
  }
}

void setfunction(struct func *func, int nargs) {
  if (!cellissym(-1) || !cellisfunc(-2))
    die("expected (set-function name function)");
  pushdup(-2);
  pushdup(-2);
  funbox();
  dosetcdr(NULL, 2);
  pushdup(-2);
  shift(2);
}

void getmacro(struct func *func, int nargs) {
  struct sym *sym = getsym(-1);
  macrobox();
  docdr(NULL, 1);
  if (cellisundef(-1)) {
    fprintf(stderr, "function undefined %.*s\n", sym->size, sym->sym);
    die("function undefined");
  }
}

void setmacro(struct func *func, int nargs) {
  if (!cellissym(-1) || !cellisfunc(-2))
    die("expected (set-macro name function)");
  pushdup(-2);
  pushdup(-2);
  macrobox();
  dosetcdr(NULL, 2);
  pushdup(-2);
  shift(2);
}

void getfuncname(struct func *func, int nargs) {
  struct func *f = getfunc(-1);
  push(&f->name);
  shift(1);
}

void setfuncname(struct func *func, int nargs) {
  struct func *f = getfunc(-1);
  if (!cellissym(-2))
    die("Expected function name");
  pushdup(-2);
  pop1(&f->name);
  shift(1);
}

void dodefmacro(struct func *func, int nargs) {
  pushdup(-1);
  pushdup(-3);
  setfuncname(NULL, 2);
  pop(1);
  setmacro(NULL, 2);
}

void dodefun(struct func *func, int nargs) {
  pushdup(-1);
  pushdup(-3);
  setfuncname(NULL, 2);
  pop(1);
  setfunction(NULL, 2);
}

void dogc(struct func *f, int nargs) {
  gc();
  pushnil();
}

void doobjroom(struct func *func, int nargs) {
  if (tagisptr(gettag(-1)))
    pushfix(objroom(getcell(-1)->v.ptr));
  else
    pushnil();
  shift(1);
}

void setupbuiltin(void) {
  resizeroot(&gcroots[ROOT_CONSTANTS], CONST_MAX);
  pushnil();
  csym("nil");
  varbox();
  dosetcdr(NULL, 2);
  csym("t");
  pushdup(-1);
  poproot(&gcroots[ROOT_CONSTANTS], CONST_T);
  pushdup(-1);
  varbox();
  dosetcdr(NULL, 2);
  csym("+EOF+");
  pushdup(-1);
  poproot(&gcroots[ROOT_CONSTANTS], CONST_EOF);
  pushdup(-1);
  varbox();
  dosetcdr(NULL, 2);
  defbuiltin("read", 0, read);
  defbuiltin("eval", 1, eval);
  defbuiltin("print", 1, print);
  defbuiltin("terpri", 0, terpri);
  defbuiltin("equal", 2, equalp);
  defbuiltin("eql", 2, eqlp);
  defbuiltin("disassemble", 1, doshowcode);
  defbuiltin("cons", 2, docons);
  defbuiltin("car", 1, docar);
  defbuiltin("cdr", 1, docdr);
  defbuiltin("room", 0, doroom);
  defbuiltin("objroom", 1, doobjroom);
  defbuiltin("quot", 2, doquot);
  defbuiltin("rem", 2, dorem);
  defbuiltin("add", 2, doadd);
  defbuiltin("sub", 2, dosub);
  defbuiltin("mul", 2, domul);
  defbuiltin("logand2", 2, doand);
  defbuiltin("logior2", 2, door);
  defbuiltin("logxor2", 2, doxor);
  defbuiltin("lognot", 1, donot);
  defbuiltin("ash", 2, doash);
  defbuiltin("fixnump", 1, fixp);
  defbuiltin("symbolp", 1, symp);
  defbuiltin("consp", 1, consp);
  defbuiltin("functionp", 1, functionp);
  defbuiltin("stringp", 1, strp);
  defbuiltin("characterp", 1, characterp);
  defbuiltin("setcar", 2, nilsetcar);
  defbuiltin("setcdr", 2, nilsetcdr);
  defbuiltin("die", 1, dodie);
  defbuiltin("fopen", 2, dofopen);
  defbuiltin("fputs", 2, dofputs);
  defbuiltin("fputc", 2, dofputc);
  defbuiltin("fgetc", 1, dofgetc);
  defbuiltin("ftell", 1, doftell);
  defbuiltin("fseek", 2, dofseek);
  defbuiltin("fclose", 1, dofclose);
  defbuiltin("char", 2, dochar);
  defbuiltin("setchar", 3, dosetchar);
  defbuiltin("fill-pointer", 1, dolength);
  defbuiltin("set-fill-pointer", 1, dosetlength);
  defbuiltin("symbol", 1, dosymbol);
  defbuiltin("string", 1, dostring);
  defbuiltin("gensym", 1, dogensym);
  defbuiltin("subseq", 3, dosubstr);
  defbuiltin("make-string", 0, domakestr);
  defbuiltin("position", 2, dostrchr);
  defbuiltin("search", 2, dostrstr);
  defbuiltin("int->str", 2, doint2str);
  defbuiltin("str->int", 1, dostr2int);
  defbuiltin("strcat", 2, dostrcat);
  defbuiltin("strpush", 2, dostrpush);
  defbuiltin("strpop", 1, dostrpop);
  defbuiltin("char-code", 1, docharcode);
  defbuiltin("code-char", 1, docodechar);
  defbuiltin(">=", 2, donumge);
  defbuiltin("<=", 2, donumle);
  defbuiltin(">", 2, donumgt);
  defbuiltin("<", 2, donumlt);
  defbuiltin("=", 2, donumeq);
  defbuiltin("tb", 0, dotb);
  defbuiltin("gc", 0, dogc);
  defbuiltin("make-vector", 0, mkvec);
  defbuiltin("vecpush", 2, vecpush);
  defbuiltin("vecpop", 1, vecpop);
  defbuiltin("vecelt", 2, vecelt);
  defbuiltin("setvecelt", 3, setvecelt);
  defbuiltin("macroexpand", 1, macroexpand);
  defbuiltin("get-function", 1, getfunction);
  defbuiltin("set-function", 2, setfunction);
  defbuiltin("get-macro", 1, getmacro);
  defbuiltin("set-macro", 2, setmacro);
  defbuiltin("get-function-name", 1, getfuncname);
  defbuiltin("set-function-name", 2, setfuncname);
  defbuiltin("set-macro-and-name", 2, dodefmacro);
  defbuiltin("set-function-and-name", 2, dodefun);
}

int main(int argc, char **argv) {
  setupbuiltin();
  read(NULL, 0);
  eval(NULL, 1);
  pop(1);
  return 0;
}
