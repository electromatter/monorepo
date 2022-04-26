#include <stdlib.h>
#include <stdio.h>

#define COLLECT_RATIO   (4)
#define NUM_VALS        (256)

typedef unsigned long word_t;
#define SIGNED(x)       ((int)((x) & 0x7fff) - (int)((x) & 0x8000))
#define OC(op, A, B, C) (((word_t)(C) << 48) | ((word_t)(B) << 32) |        \
                         ((A) << 16) | ((op) << 1))
#define CO(x)           (A = ((x) >> 16), B = ((x) >> 32) & 0xffff,         \
                         C = ((x) >> 48) & 0xffff, ((x) >> 1) & 0xff)
#define ABC_MAX         (0xffff)

#define NIL             ((word_t)0)
#define PTRP(x)         (((x) & 3) == 0)

#define FIX_MAX         ((word_t)-1 >> 1)
#define FIXP(x)         ((x) & 1)
#define XIF(x)          ((word_t)(x) >> 1)
#define FIX(x)          (1 | ((word_t)(x) << 1))

#define CHRP(c)         (((c) & 7) == 2)
#define CHR(c)          (2 | ((word_t)((c) & 0xff) << 8))
#define RHC(c)          ((unsigned char)((c) >> 8))

#define NEXT(o)         (((word_t *)(o))[-2])
#define HEAD(o)         (((word_t *)(o))[-1])
#define TAIL(o)         (((word_t *)(o)) + TLEN(HEAD(o)))

#define MARK(o)         (HEAD(o) |= 8)
#define UNMARK(o)       (HEAD(o) &= ~(word_t)8)
#define MARKP(o)        ((HEAD(o) & 8) != 0)

#define TMASK           (48)
#define TAG(n, t)       (6 | (t) | ((word_t)(n) << 8))
#define TLEN(x)         ((x) >> 8)
#define TAGP(x)         (((x) & 7) == 6)

#define OTAG            (16)
#define OCONS(n)        (cons((n), OTAG))
#define OBJP(o)         ((HEAD(o) & TMASK) == OTAG)
#define OLEN(o)         (TLEN(HEAD(o)))
#define SLOT(o, n)      ((word_t *)(o))[(n)]

#define VTAG            (32)
#define VCONS(n)        (cons((n), 2))
#define VECP(v)         ((HEAD(v) & TMASK) == VTAG)
#define VLEN(v)         (TLEN(HEAD(v)))
#define VELT(v, n)      ((word_t *)(v))[(n)]

#define STAG            (48)
#define SCONS(n)        (cons(((n) + sizeof(word_t) - 1) / sizeof(word_t), 3))
#define STRP(s)         ((HEAD(s) & TMASK) == STAG)
#define SLEN(s)         (TLEN(HEAD(s)) * sizeof(word_t))
#define SELT(s, n)      ((unsigned char *)(s))[(n)]

enum opcodes {
#define OP(name, act) OP_##name,
#define OPCODES                                                         \
    /* Control */                                                       \
    OP(EXIT, exit(XIF(R(A))))                                           \
    OP(BEQ, P += (R(A) == R(B) ? 1 : 0))                                \
    OP(BLE, P += (R(A) <= R(B) ? 1 : 0))                                \
    OP(BLT, P += (R(A) < R(B) ? 1 : 0))                                 \
    OP(BNIL, P += (R(A) == NIL ? 1 : 0))                                \
    OP(BFIX, P += (FIXP(R(A)) ? 1 : 0))                                 \
    OP(BCHR, P += (CHRP(R(A)) ? 1 : 0))                                 \
    OP(BSTR, P += (PTRP(R(A)) && STRP(R(A)) ? 1 : 0))                   \
    OP(BVEC, P += (PTRP(R(A)) && VECP(R(A)) ? 1 : 0))                   \
    OP(BOBJ, P += (PTRP(R(A)) && OBJP(R(A)) ? 1 : 0))                   \
    OP(BVEQ, P += (N == A ? 1 : 0))                                     \
    OP(BVLT, P += (N < A ? 1 : 0))                                      \
    OP(JMP, P += SIGNED(A))                                             \
    OP(LINK, SLOT(R(A), B) = R)                                         \
    OP(GO, SWITCH(P += SIGNED(A); R = R(B)))                            \
    OP(CALL, CONSING(C = OCONS(XIF(SLOT(R(A), 0))));                    \
             SLOT(C, 0) = R; SLOT(C, 1) = E; SLOT(C, 2) = P;            \
             SWITCH(E = SLOT(R(A), 1); P = SLOT(R(A), 2); R = C))       \
    OP(RET, SWITCH(E = R(1); P = R(2); R = R(0)))                       \
    /* Values */                                                        \
    OP(MOVE, R(A) = R(B))                                               \
    OP(POP, N -= 1; R(A) = V(N); V(N) = NIL)                            \
    OP(PUSH, V(N) = R(A); N += 1)                                       \
    OP(GC, R(A) = gc())                                                 \
    /* Object */                                                        \
    OP(OCONS, CONSING(C = OCONS(B)); R(A) = C)                          \
    OP(OLEN, R(A) = FIX(OLEN(R(B))))                                    \
    OP(LDO, R(A) = SLOT(R(B), C))                                       \
    OP(STO, SLOT(R(A), B) = R(C))                                       \
    /* Vector */                                                        \
    OP(VCONS, CONSING(C = VCONS(XIF(R(B)))); R(A) = C)                  \
    OP(VLEN, R(A) = FIX(VLEN(R(B))))                                    \
    OP(LDV, R(A) = VELT(R(B), XIF(R(C))))                               \
    OP(STV, VELT(R(A), XIF(R(B))) = R(C))                               \
    /* String */                                                        \
    OP(SCONS, CONSING(C = SCONS(XIF(R(B)))); R(A) = C)                  \
    OP(SLEN, R(A) = FIX(SLEN(R(B))))                                    \
    OP(LDC, R(A) = CHR(SELT(R(B), XIF(R(C)))))                          \
    OP(STC, SELT(R(A), XIF(R(B))) = RHC(R(C)))                          \
    /* Character */                                                     \
    OP(CODE, R(A) = FIX(RHC(R(B))))                                     \
    OP(CHAR, R(A) = CHR(XIF(R(B))))                                     \
    OP(GETC, R(A) = (C = getchar()) == (word_t)EOF ? NIL : CHR(C))      \
    OP(PUTC, putchar(RHC(R(A))))                                        \
    /* Fixnum */                                                        \
    OP(NEG, R(A) = FIX(-XIF(R(C))))                                     \
    OP(ADD, R(A) = FIX(XIF(R(B)) + XIF(R(C))))                          \
    OP(SUB, R(A) = FIX(XIF(R(B)) - XIF(R(C))))                          \
    OP(MUL, R(A) = FIX(XIF(R(B)) * XIF(R(C))))                          \
    OP(DIV, R(A) = FIX(XIF(R(B)) / XIF(R(C))))                          \
    OP(MOD, R(A) = FIX(XIF(R(B)) % XIF(R(C))))                          \
    OP(NOT, R(A) = FIX(~XIF(R(B))))                                     \
    OP(IOR, R(A) = FIX(XIF(R(B)) | XIF(R(C))))                          \
    OP(XOR, R(A) = FIX(XIF(R(B)) ^ XIF(R(C))))                          \
    OP(LSH, R(A) = FIX(XIF(R(B)) << XIF(R(C))))                         \
    OP(RSH, R(A) = FIX(XIF(R(B)) >> XIF(R(C))))
  OPCODES
#undef OP
};

word_t code = NIL, frame = NIL, vals = NIL, objs = NIL;
word_t alloc_size = 0, live_size = 0;

void
mark(word_t o)
{
  word_t tmp, *a = &tmp, *b = 0;
  do {
    if (TAGP(o))
      o = *b, *b = (word_t)(a + 1), a = b, b = (word_t *)o;
    else if (PTRP(o) && o != NIL && !MARKP(o)) {
      MARK(o);
      if (!STRP(o))
        *a = (word_t)b, b = a, a = TAIL(o);
    }
    o = *--a;
  } while (b != 0);
}

word_t
gc(void)
{
  word_t *o, *l = &objs;
  mark(code); mark(frame); mark(vals);
  live_size = 0;
  while ((o = (word_t *)*l) != NIL)
    if (MARKP(o))
      l = &NEXT(o), UNMARK(o), live_size += OLEN(o);
    else
      *l = NEXT(o), free(o - 2);
  alloc_size = live_size;
  return FIX(live_size);
}

word_t
cons(word_t n, int tag)
{
  word_t *o;
  if (alloc_size > live_size * COLLECT_RATIO)
    gc();
  alloc_size += n;
  if ((o = calloc(sizeof(word_t), n + 2)) == NULL)
    fprintf(stderr, "OOM\n"), abort();
  o += 2;
  NEXT(o) = objs; HEAD(o) = TAG(n, tag); objs = (word_t)o;
  return (word_t)o;
}

const word_t readeval[] = {
#define I(op, A, B, C)  OC(op, A, B, C),
#include "readeval.h"
};

int
main(int argc, char **argv)
{
  word_t N = 0, A, B, C, E, P = 0, R, V, instr;
#define CONSING(act)    act; (E = code, R = frame, V = vals)
#define SWITCH(act)     act; frame = R;
#define R(n)            ((word_t *)R)[(n)]
#define E(n)            ((word_t *)E)[(n)]
#define V(n)            ((word_t *)V)[(n)]
  (void)argc; (void)argv;
  CONSING(code = VCONS(sizeof(readeval) / sizeof(word_t));
          frame = VCONS(256); vals = VCONS(NUM_VALS));
  R(0) = R; R(1) = E; R(2) = P; R(3) = NIL; R(4) = FIX(0); R(5) = FIX(1);
  R(6) = FIX(FIX_MAX); R(7) = FIX(NUM_VALS); R(8) = FIX(ABC_MAX);
  for (A = 0; A < sizeof(readeval) / sizeof(readeval[0]); A++)
    E(A) = readeval[A];
  while (1) {
    instr = E(P), P++;
    switch (CO(instr)) {
#define OP(name, act)   case OP_##name: act; break;
      OPCODES
      default: abort();
    }
  }
}
