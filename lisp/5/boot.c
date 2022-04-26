/******************************************************************************
 * Copyright (c) 2022 Eric Chai <electromatter@gmail.com>                     *
 *                                                                            *
 * Permission to use, copy, modify, and distribute this software for any      *
 * purpose with or without fee is hereby granted.                             *
 *                                                                            *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES   *
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF           *
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR    *
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES     *
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN      *
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF    *
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.             *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Basic integer type for fixnums. */
typedef unsigned int word_t;
#define WORD_MAX        ((word_t)-1)

/* Unmarked objects are not in the gray list and are not black. */
#define GC_QUEUE_WHITE  (NULL)

/* Special value to mark black objects. */
#define GC_QUEUE_BLACK  ((union obj *)-1)

/* Minimum garbage collection batch size. */
#define GC_THRESH_MIN   (1024)

/* Minimum hash table size. */
#define TABLE_MIN_SIZE  (16)

/* Minimum vector size. */
#define VECTOR_MIN_SIZE (16)

/* Wrapper to add and remove frames from the call stack. */
#define DECLARE_BUILTIN(name)                                           \
static void name(struct context *ctx)

#define BUILTIN(name)                                                   \
static void builtin_##name(struct context *, struct gcframe *);         \
static void name(struct context *ctx)                                   \
{                                                                       \
    struct gcframe frame = {NULL, #name, NULL};                         \
    frame.parent = ctx->cur_frame;                                      \
    ctx->cur_frame = &frame;                                            \
    builtin_##name(ctx, &frame);                                        \
    ctx->cur_frame = frame.parent;                                      \
}                                                                       \
static void                                                             \
builtin_##name(struct context *ctx, struct gcframe *gcframe)

/*
 * Define up to six local references in a BUILTIN function.
 * There must be exactly one LOCALn declaration in every BULTIN function.
 */
#define LOCALS0()                                                       \
    union obj **locals[] = {NULL};                                      \
    (gcframe)->vars = locals

#define LOCALS1(a)                                                      \
    union obj *a = NULL;                                                \
    union obj **locals[] = {&a, NULL};                                  \
    (gcframe)->vars = locals

#define LOCALS2(a, b)                                                   \
    union obj *a = NULL;                                                \
    union obj *b = NULL;                                                \
    union obj **locals[] = {&a, &b, NULL};                              \
    (gcframe)->vars = locals

#define LOCALS3(a, b, c)                                                \
    union obj *a = NULL;                                                \
    union obj *b = NULL;                                                \
    union obj *c = NULL;                                                \
    union obj **locals[] = {&a, &b, &c, NULL};                          \
    (gcframe)->vars = locals

#define LOCALS4(a, b, c, d)                                             \
    union obj *a = NULL;                                                \
    union obj *b = NULL;                                                \
    union obj *c = NULL;                                                \
    union obj *d = NULL;                                                \
    union obj **locals[] = {&a, &b, &c, &d, NULL};                      \
    (gcframe)->vars = locals

#define LOCALS5(a, b, c, d, e)                                          \
    union obj *a = NULL;                                                \
    union obj *b = NULL;                                                \
    union obj *c = NULL;                                                \
    union obj *d = NULL;                                                \
    union obj *e = NULL;                                                \
    union obj **locals[] = {&a, &b, &c, &d, &e, NULL};                  \
    (gcframe)->vars = locals

#define LOCALS6(a, b, c, d, e, f)                                       \
    union obj *a = NULL;                                                \
    union obj *b = NULL;                                                \
    union obj *c = NULL;                                                \
    union obj *d = NULL;                                                \
    union obj *e = NULL;                                                \
    union obj *f = NULL;                                                \
    union obj **locals[] = {&a, &b, &c, &d, &e, &f, NULL};              \
    (gcframe)->vars = locals

/*
 * Minimum capacity of the values vector. Any call/return can use the values
 * vector without allocating or checking the size of the values vector.
 *
 * Most sane functions don't use more than this. All of the built-in functions
 * are required to use only up to MIN_VALUES.
 */
#define MIN_VALUES      (6)

/* Current number of values. */
#define NVALS()         ((ctx)->nvals)

/* Store up to six values into the values vector. */
#define VALUES0()                                                       \
    do {                                                                \
        (ctx)->nvals = 0;                                               \
    } while (0)

#define VALUES1(a)                                                      \
    do {                                                                \
        (ctx)->vals->vstruct.slots[0] = (a);                            \
        (ctx)->nvals = 1;                                               \
    } while (0)

#define VALUES2(a, b)                                                   \
    do {                                                                \
        (ctx)->vals->vstruct.slots[0] = (a);                            \
        (ctx)->vals->vstruct.slots[1] = (b);                            \
        (ctx)->nvals = 2;                                               \
    } while (0)

#define VALUES3(a, b, c)                                                \
    do {                                                                \
        (ctx)->vals->vstruct.slots[0] = (a);                            \
        (ctx)->vals->vstruct.slots[1] = (b);                            \
        (ctx)->vals->vstruct.slots[2] = (c);                            \
        (ctx)->nvals = 3;                                               \
    } while (0)

#define VALUES4(a, b, c, d)                                             \
    do {                                                                \
        (ctx)->vals->vstruct.slots[0] = (a);                            \
        (ctx)->vals->vstruct.slots[1] = (b);                            \
        (ctx)->vals->vstruct.slots[2] = (c);                            \
        (ctx)->vals->vstruct.slots[3] = (d);                            \
        (ctx)->nvals = 4;                                               \
    } while (0)

#define VALUES5(a, b, c, d, e)                                          \
    do {                                                                \
        (ctx)->vals->vstruct.slots[0] = (a);                            \
        (ctx)->vals->vstruct.slots[1] = (b);                            \
        (ctx)->vals->vstruct.slots[2] = (c);                            \
        (ctx)->vals->vstruct.slots[3] = (d);                            \
        (ctx)->vals->vstruct.slots[4] = (e);                            \
        (ctx)->nvals = 5;                                               \
    } while (0)

#define VALUES6(a, b, c, d, e, f)                                       \
    do {                                                                \
        (ctx)->vals->vstruct.slots[0] = (a);                            \
        (ctx)->vals->vstruct.slots[1] = (b);                            \
        (ctx)->vals->vstruct.slots[2] = (c);                            \
        (ctx)->vals->vstruct.slots[3] = (d);                            \
        (ctx)->vals->vstruct.slots[4] = (e);                            \
        (ctx)->vals->vstruct.slots[5] = (f);                            \
        (ctx)->nvals = 6;                                               \
    } while (0)

#define PUSH_VALUE(value)                                               \
    do {                                                                \
        if ((ctx)->nvals >= (ctx)->vals->vstruct.nslots) {              \
            (ctx)->vals = extend_vstruct((ctx), (ctx)->vals);           \
        }                                                               \
        (ctx)->vals->vstruct.slots[(ctx)->nvals++] = (value);           \
    } while (0)

#define POP_VALUE()                                                     \
    ((ctx)->nvals > 0 ? (ctx)->vals->vstruct.slots[--(ctx)->nvals] : NULL)

/* Unpack values from the values vector. */
#define BIND1(a)                                                        \
    do {                                                                \
        (a) = (ctx)->nvals >= 1 ? (ctx)->vals->vstruct.slots[0] : NULL; \
    } while (0)

#define BIND2(a, b)                                                     \
    do {                                                                \
        (a) = (ctx)->nvals >= 1 ? (ctx)->vals->vstruct.slots[0] : NULL; \
        (b) = (ctx)->nvals >= 2 ? (ctx)->vals->vstruct.slots[1] : NULL; \
    } while (0)

#define BIND3(a, b, c)                                                  \
    do {                                                                \
        (a) = (ctx)->nvals >= 1 ? (ctx)->vals->vstruct.slots[0] : NULL; \
        (b) = (ctx)->nvals >= 2 ? (ctx)->vals->vstruct.slots[1] : NULL; \
        (c) = (ctx)->nvals >= 3 ? (ctx)->vals->vstruct.slots[2] : NULL; \
    } while (0)

#define BIND4(a, b, c, d)                                               \
    do {                                                                \
        (a) = (ctx)->nvals >= 1 ? (ctx)->vals->vstruct.slots[0] : NULL; \
        (b) = (ctx)->nvals >= 2 ? (ctx)->vals->vstruct.slots[1] : NULL; \
        (c) = (ctx)->nvals >= 3 ? (ctx)->vals->vstruct.slots[2] : NULL; \
        (d) = (ctx)->nvals >= 4 ? (ctx)->vals->vstruct.slots[3] : NULL; \
    } while (0)

#define BIND5(a, b, c, d, e)                                            \
    do {                                                                \
        (a) = (ctx)->nvals >= 1 ? (ctx)->vals->vstruct.slots[0] : NULL; \
        (b) = (ctx)->nvals >= 2 ? (ctx)->vals->vstruct.slots[1] : NULL; \
        (c) = (ctx)->nvals >= 3 ? (ctx)->vals->vstruct.slots[2] : NULL; \
        (d) = (ctx)->nvals >= 4 ? (ctx)->vals->vstruct.slots[3] : NULL; \
        (e) = (ctx)->nvals >= 5 ? (ctx)->vals->vstruct.slots[4] : NULL; \
    } while (0)

#define BIND6(a, b, c, d, e, f)                                         \
    do {                                                                \
        (a) = (ctx)->nvals >= 1 ? (ctx)->vals->vstruct.slots[0] : NULL; \
        (b) = (ctx)->nvals >= 2 ? (ctx)->vals->vstruct.slots[1] : NULL; \
        (c) = (ctx)->nvals >= 3 ? (ctx)->vals->vstruct.slots[2] : NULL; \
        (d) = (ctx)->nvals >= 4 ? (ctx)->vals->vstruct.slots[3] : NULL; \
        (e) = (ctx)->nvals >= 5 ? (ctx)->vals->vstruct.slots[4] : NULL; \
        (f) = (ctx)->nvals >= 6 ? (ctx)->vals->vstruct.slots[5] : NULL; \
    } while (0)

#define INTERN(name)            ((ctx)->special->vstruct.slots[SPECIAL_##name])

/*
 * Interned symbols that can be efficiently accessed with INTERN(name).
 *
 * Any symbols used by the early compiler must be defined here. This allows
 * the compiler to run without consing new symbols.
 */
enum special {
#define SPECIAL_SYMBOLS                                                 \
    SPECIAL_DEF(NIL, "nil")                                             \
    SPECIAL_DEF(T, "t")                                                 \
    SPECIAL_DEF(QUASIQUOTE, "quasiquote")                               \
    SPECIAL_DEF(QUOTE, "quote")                                         \
    SPECIAL_DEF(UNQUOTE, "unquote")                                     \
    SPECIAL_DEF(UNQUOTE_SPLICING, "unquote-splicing")                   \
    SPECIAL_DEF(FUNCTION, "function")                                   \
    SPECIAL_DEF(f, "f")                                                 \
    SPECIAL_DEF(g, "g")                                                 \
    SPECIAL_DEF(h, "h")                                                 \
    SPECIAL_DEF(i, "i")                                                 \
    SPECIAL_DEF(j, "j")                                                 \
    SPECIAL_DEF(k, "k")                                                 \
    SPECIAL_DEF(l, "l")                                                 \
    SPECIAL_DEF(m, "m")                                                 \
    SPECIAL_DEF(n, "n")                                                 \
    SPECIAL_DEF(o, "o")                                                 \
    SPECIAL_DEF(p, "p")                                                 \
    SPECIAL_DEF(q, "q")                                                 \
    SPECIAL_DEF(r, "r")                                                 \
    SPECIAL_DEF(s, "s")                                                 \
    SPECIAL_DEF(t, "t")                                                 \
    SPECIAL_DEF(u, "u")                                                 \
    SPECIAL_DEF(v, "v")                                                 \
    SPECIAL_DEF(w, "w")                                                 \
    SPECIAL_DEF(x, "x")                                                 \
    SPECIAL_DEF(y, "y")                                                 \
    SPECIAL_DEF(z, "z")                                                 \
    SPECIAL_DEF(THE_LAST_SYMBOL, "the-last-symbol")

#define SPECIAL_DEF(ident, name)    SPECIAL_##ident,
    SPECIAL_SYMBOLS
#undef SPECIAL_DEF

    NUM_SPECIAL
};

/* One tag value for each basic object type. */
enum tag {
    TAG_TABLE,
    TAG_FUNC,
    TAG_VSTRUCT,
    TAG_VEC,
    TAG_NVEC,
    TAG_STR,
    TAG_CONS,
    TAG_SYM,
    TAG_CHR,
    TAG_FIX
};

/* Common header of all objects. */
struct objhead {
    /* Object type. */
    enum tag tag;

    /* Singly linked list of all allocated objects. */
    union obj *gc_allobj;

    /* GC mark and gray list. */
    union obj *gc_queue;
};

/* A vary basic hash table. */
struct table {
    struct objhead head;
    word_t size;
    word_t fill;
    word_t load;
    union obj **keys;
    union obj **values;
};

/* Continuation. */
struct func {
    struct objhead head;
    void (*func)(struct context *ctx);
    union obj *closure;
    union obj *locals;
    union obj *code;
    word_t pc;
};

/* Vector struct. An ordinary object. */
struct vstruct {
    struct objhead head;
    word_t nslots;
    union obj *slots[];
};

/* Vector of objects. */
struct vec {
    struct objhead head;
    word_t capacity;
    word_t fill;
    union obj **slots;
};

/* Vector of fixnums. */
struct nvec {
    struct objhead head;
    word_t capacity;
    word_t fill;
    word_t *slots;
};

/* Vector of bytes. */
struct str {
    struct objhead head;
    word_t capacity;
    word_t fill;
    unsigned char *slots;
};

/* Cons cell. */
struct cons {
    struct objhead head;
    union obj *car;
    union obj *cdr;
};

/* Symbol. */
struct sym {
    struct objhead head;
    union obj *str;
};

/* Boxed byte. */
struct chr {
    struct objhead head;
    unsigned char value;
};

/* Boxed fixnum. */
struct fix {
    struct objhead head;
    word_t value;
};

union obj {
    struct objhead head;
    struct table table;
    struct func func;
    struct vstruct vstruct;
    struct vec vec;
    struct nvec nvec;
    struct str str;
    struct cons cons;
    struct sym sym;
    struct chr chr;
    struct fix fix;
};

/* A C frame for debugging and to allow the GC to see into the C stack. */
struct gcframe {
    /* Previous frame in the call stack. */
    struct gcframe *parent;

    /* Name of the C function. */
    const char *func;

    /* Pointers to local references. */
    union obj ***vars;
};

/* Global interpreter state. */
struct context {
    /* Frame that contains global references. */
    struct gcframe root_frame;
    union obj **globals[10];

    /* Current C call frame. */
    struct gcframe *cur_frame;

    /* Current (values ...) vector. */
    union obj *vals;
    word_t nvals;

    /* Internalized symbol hash table. */
    union obj *intern;

    /* Special symbol table. */
    union obj *special;

    /* Compiler state. */
    union obj *dyn_vars;
    union obj *dyn_funcs;
    union obj *dyn_macros;
    union obj *dyn_symbols;

    /* Interpreter state. */
    word_t next_pc;
    union obj *cur_code;
    union obj *cur_closure;
    union obj *cur_locals;

    /* All allocated objects. */
    union obj *gc_allobj;

    /* Gray set. */
    union obj *gc_queue;

    /* Garbage collector statistics. */
    word_t gc_live;
    word_t gc_pending;
    word_t gc_thresh;
};

/* Dump stack backtrace and locals to stderr. */
static void
backtrace(struct context *ctx)
{
    struct gcframe *frame;
    union obj ***vars;

    fflush(stdout);

    fprintf(stderr, "Backtrace (oldest frame last):\n");

    for (frame = ctx->cur_frame; frame != NULL; frame = frame->parent) {
        fprintf(stderr, "  %s:", frame->func);

        for (vars = frame->vars; *vars != NULL; vars++) {
            fprintf(stderr, " %p", (void *)**vars);
        }

        fprintf(stderr, "\n");
    }

    fflush(stderr);
}

/* Print an error message and abort. */
static void
die(struct context *ctx, const char *message)
{
    fflush(stdout);

    fprintf(stderr, "die: %s\n", message);

    if (ctx != NULL) {
        backtrace(ctx);
    }

    fflush(stderr);

    abort();
}

/* Move an object from the white set to the grey set. */
static void
mark_gray(struct context *ctx, union obj *o)
{
    /* Object already marked or reference is NULL, nothing to do. */
    if (o == NULL || o->head.gc_queue != GC_QUEUE_WHITE) {
        return;
    }

    /* Add the object to the gray queue. */
    o->head.gc_queue = ctx->gc_queue;
    ctx->gc_queue = o;
}

/* Move an object from the gray set to the black set. */
static void
mark(struct context *ctx)
{
    word_t i;
    union obj *o;

    /* Nothing in the queue. Nothing to do. */
    o = ctx->gc_queue;
    if (o == GC_QUEUE_BLACK) {
        return;
    }

    /* Remove the first object in the queue. */
    ctx->gc_queue = o->head.gc_queue;

    /* Blacken this object. */
    o->head.gc_queue = GC_QUEUE_BLACK;

    /* Mark referenced objects gray. */
    switch (o->head.tag) {
    case TAG_TABLE:
        for (i = 0; i < o->table.size; i++) {
            mark_gray(ctx, o->table.keys[i]);
            mark_gray(ctx, o->table.values[i]);
        }
        break;
    case TAG_FUNC:
        mark_gray(ctx, o->func.closure);
        mark_gray(ctx, o->func.locals);
        mark_gray(ctx, o->func.code);
        break;
    case TAG_VSTRUCT:
        for (i = 0; i < o->vstruct.nslots; i++) {
            mark_gray(ctx, o->vstruct.slots[i]);
        }
        break;
    case TAG_VEC:
        for (i = 0; i < o->vec.capacity; i++) {
            mark_gray(ctx, o->vec.slots[i]);
        }
        break;
    case TAG_NVEC:
        break;
    case TAG_STR:
        break;
    case TAG_CONS:
        mark_gray(ctx, o->cons.car);
        mark_gray(ctx, o->cons.cdr);
        break;
    case TAG_SYM:
        mark_gray(ctx, o->sym.str);
        break;
    case TAG_CHR:
        break;
    case TAG_FIX:
        break;
    default:
        die(ctx, "Invalid tag.");
    }
}

/* Do a full collection and release white objects. */
static void sweep(struct context *ctx)
{
    struct gcframe *frame;
    union obj ***vars;
    union obj **link;
    union obj *o;
    word_t live_count;

    /* Mark the stack. */
    for (frame = ctx-> cur_frame; frame != NULL; frame = frame->parent) {
        if (frame->vars != NULL) {
            for (vars = frame->vars; *vars != NULL; vars++) {
                mark_gray(ctx, **vars);
            }
        }
    }

    /* Drain the gray queue. */
    while (ctx->gc_queue != GC_QUEUE_BLACK) {
        mark(ctx);
    }

    live_count = 0;
    link = &ctx->gc_allobj;
    while (1) {
        o = *link;

        /* Reached the end of the all-objects list. */
        if (o == NULL) {
            break;
        }

        /* Black objects are live. Re-color white for next collection */
        if (o->head.gc_queue == GC_QUEUE_BLACK) {
            live_count += 1;
            o->head.gc_queue = GC_QUEUE_WHITE;
            link = &o->head.gc_allobj;
            continue;
        }

        /* Unlink this object so we can free it. */
        *link = o->head.gc_allobj;

        /* Release untraced memory. */
        switch (o->head.tag) {
        case TAG_TABLE:
            free(o->table.keys);
            free(o->table.values);
            break;
        case TAG_FUNC:
            break;
        case TAG_VSTRUCT:
            break;
        case TAG_VEC:
            break;
        case TAG_NVEC:
            free(o->nvec.slots);
            break;
        case TAG_STR:
            free(o->str.slots);
            break;
        case TAG_CONS:
            break;
        case TAG_SYM:
            break;
        case TAG_CHR:
            break;
        case TAG_FIX:
            break;
        default:
            die(ctx, "Invalid tag.");
        }

        free(o);
    }

    /* Update the GC statistics. */
    ctx->gc_live = live_count;
    ctx->gc_pending = 0;

    /* Use threshold of 25% new objects to get amortized linear time. */
    ctx->gc_thresh = ctx->gc_live / 4;
    if (ctx->gc_thresh < GC_THRESH_MIN) {
        ctx->gc_thresh = GC_THRESH_MIN;
    }
}

/* Do an incremental collection. */
static void
gc_step(struct context *ctx)
{
    word_t i;

    /* Mark a small batch of objects. */
    for (i = 0; i < GC_THRESH_MIN && ctx->gc_queue != GC_QUEUE_BLACK; i++) {
        mark(ctx);
    }

    /* If the mark queue is empty, then sweep. */
    if (ctx->gc_pending > 0 && ctx->gc_queue == GC_QUEUE_BLACK) {
        sweep(ctx);
    }
}

/*
 * Allocate a new object.
 *
 * **WARNING**
 * The returned reference is only valid until the next call to sweep.
 * It should be immediately assigned to a local reference.
 */
static union obj *
make_object(struct context *ctx, enum tag tag, word_t size)
{
    union obj *o;

    /* Step the incremental collection. */
    if (ctx->gc_pending >= ctx->gc_thresh) {
        gc_step(ctx);
    }

    /* Don't allow allocation failures. */
    o = malloc(size);
    if (o == NULL) {
        die(ctx, "Out of memory.");
    }
    memset(o, 0, size);

    /* Initialize the objhead. */
    o->head.tag = tag;
    o->head.gc_queue = GC_QUEUE_WHITE;

    /* Add the new object to all-objects list. */
    o->head.gc_allobj = ctx->gc_allobj;
    ctx->gc_allobj = o;

    /* Update statistics.*/
    ctx->gc_pending += 1;

    return o;
}

/* Allocate an empty hash table. */
static union obj*
make_table(struct context *ctx)
{
    union obj *o;
    o = make_object(ctx, TAG_TABLE, sizeof(struct table));
    o->table.size = 0;
    o->table.fill = 0;
    o->table.load = 0;
    o->table.keys = NULL;
    o->table.values = NULL;
    return o;
}

/* Allocate an empty function. */
static union obj*
make_func(struct context *ctx)
{
    union obj *o;
    o = make_object(ctx, TAG_FUNC, sizeof(struct func));
    o->func.closure = NULL;
    o->func.locals = NULL;
    o->func.code = NULL;
    o->func.pc = 0;
    return o;
}

/* Allocate an empty vector. */
static union obj*
make_vec(struct context *ctx)
{
    union obj *o;
    o = make_object(ctx, TAG_VEC, sizeof(struct vec));
    o->vec.capacity = 0;
    o->vec.fill = 0;
    o->vec.slots = NULL;
    return o;
}

/* Change the size of the backing array of a string. */
static void
resize_vec(struct context *ctx, union obj *o, word_t cap)
{
    union obj **slots = NULL;
    word_t i;

    if (o == NULL || o->head.tag != TAG_VEC) {
        die(ctx, "Expected vector to resize.");
    }

    /* Already at requested capacity, nothing to do. */
    if (o->vec.capacity == cap) {
        return;
    }

    if (cap > WORD_MAX / sizeof(union obj *)) {
        die(ctx, "Vector too large.");
    }

    slots = realloc(o->vec.slots, cap * sizeof(union obj *));
    if (slots == NULL) {
        die(ctx, "Out of memory.");
    }

    /* Initialize new space. */
    for (i = o->vec.capacity; i < cap; i++) {
        slots[i] = 0;
    }

    /* Ensure fill pointer stays within the vector. */
    if (cap < o->vec.fill) {
        o->vec.fill = cap;
    }

    o->vec.capacity = cap;
    o->vec.slots = slots;
}

/* Push a character to the end of the string. */
static void
extend_vec(struct context *ctx, union obj *o, union obj *x)
{
    word_t size;

    if (o == NULL || o->head.tag != TAG_VEC) {
        die(ctx, "Expected vector to extend.");
    }

    if (o->vec.fill >= o->vec.capacity) {
        if (o->vec.capacity > WORD_MAX / 2) {
            die(ctx, "Vector too large.");
        }

        size = o->vec.capacity * 2;
        if (size < VECTOR_MIN_SIZE) {
            size = VECTOR_MIN_SIZE;
        }

        resize_vec(ctx, o, size);
    }

    o->vec.slots[o->vec.fill++] = x;
}

/* Allocate an empty fixnum vector. */
static union obj*
make_nvec(struct context *ctx)
{
    union obj *o;
    o = make_object(ctx, TAG_NVEC, sizeof(struct nvec));
    o->nvec.capacity = 0;
    o->nvec.fill = 0;
    o->nvec.slots = NULL;
    return o;
}

/* Change the size of the backing array of a string. */
static void
resize_nvec(struct context *ctx, union obj *o, word_t cap)
{
    word_t *slots = NULL;
    word_t i;

    if (o == NULL || o->head.tag != TAG_NVEC) {
        die(ctx, "Expected fixnum vector to resize.");
    }

    /* Already at requested capacity, nothing to do. */
    if (o->nvec.capacity == cap) {
        return;
    }

    if (cap > WORD_MAX / sizeof(word_t)) {
        die(ctx, "Fixnum vector too large.");
    }

    slots = realloc(o->nvec.slots, cap * sizeof(word_t));
    if (slots == NULL) {
        die(ctx, "Out of memory.");
    }

    /* Initialize new space. */
    for (i = o->nvec.capacity; i < cap; i++) {
        slots[i] = 0;
    }

    /* Ensure fill pointer stays within the vector. */
    if (cap < o->nvec.fill) {
        o->nvec.fill = cap;
    }

    o->nvec.capacity = cap;
    o->nvec.slots = slots;
}

/* Push a character to the end of the string. */
static void
extend_nvec(struct context *ctx, union obj *o, word_t x)
{
    word_t size;

    if (o == NULL || o->head.tag != TAG_NVEC) {
        die(ctx, "Expected fixnum vector to extend.");
    }

    if (o->nvec.fill >= o->nvec.capacity) {
        if (o->nvec.capacity > WORD_MAX / 2) {
            die(ctx, "Fixnum vector too large.");
        }

        size = o->nvec.capacity * 2;
        if (size < VECTOR_MIN_SIZE) {
            size = VECTOR_MIN_SIZE;
        }

        resize_nvec(ctx, o, size);
    }

    o->nvec.slots[o->nvec.fill++] = x;
}

/* Allocate a struct with nslots. */
static union obj*
make_vstruct(struct context *ctx, word_t nslots)
{
    union obj *o;
    word_t size;

    /* Check for overflow. This should be unlikely. */
    size = sizeof(struct vstruct);
    if (nslots > (WORD_MAX - size) / sizeof(union obj *)) {
        die(ctx, "Struct has too many slots.");
    }
    size += nslots * sizeof(union obj *);

    o = make_object(ctx, TAG_VSTRUCT, size);
    o->vstruct.nslots = nslots;
    return o;
}

/* Expand a vstruct by at least one slot by making a larger copy. */
static union obj *
extend_vstruct(struct context *ctx, union obj *v)
{
    union obj *o;
    word_t i;
    word_t nslots;

    nslots = v->vstruct.nslots;
    if (nslots > WORD_MAX / 2) {
        die(ctx, "Struct has too many slots.");
    }
    nslots *= 2;

    o = make_vstruct(ctx, nslots);
    for (i = 0; i < v->vstruct.nslots; i++) {
        o->vstruct.slots[i] = v->vstruct.slots[i];
    }
    return o;
}

/* Allocate a string of the given size. */
static union obj *
make_str(struct context *ctx)
{
    unsigned char *storage = NULL;
    union obj *o = NULL;
    o = make_object(ctx, TAG_STR, sizeof(struct str));
    o->str.capacity = 0;
    o->str.fill = 0;
    o->str.slots = storage;
    return o;
}

/* Change the size of the backing array of a string. */
static void
resize_str(struct context *ctx, union obj *o, word_t cap)
{
    unsigned char *slots = NULL;
    word_t i;

    if (o == NULL || o->head.tag != TAG_STR) {
        die(ctx, "Expected string to resize.");
    }

    /* Already at requested capacity, nothing to do. */
    if (o->str.capacity == cap) {
        return;
    }

    slots = realloc(o->str.slots, cap);
    if (slots == NULL) {
        die(ctx, "Out of memory.");
    }

    /* Initialize new space. */
    for (i = o->str.capacity; i < cap; i++) {
        slots[i] = 0;
    }

    /* Ensure fill pointer stays within the vector. */
    if (cap < o->str.fill) {
        o->str.fill = cap;
    }

    o->str.capacity = cap;
    o->str.slots = slots;
}

/* Push a character to the end of the string. */
static void
extend_str(struct context *ctx, union obj *o, unsigned char ch)
{
    word_t size;

    if (o == NULL || o->head.tag != TAG_STR) {
        die(ctx, "Expected string to extend.");
    }

    if (o->str.fill >= o->str.capacity) {
        if (o->str.capacity > WORD_MAX / 2) {
            die(ctx, "String too large.");
        }

        size = o->str.capacity * 2;
        if (size < VECTOR_MIN_SIZE) {
            size = VECTOR_MIN_SIZE;
        }

        resize_str(ctx, o, size);
    }

    o->str.slots[o->str.fill++] = ch;
}

/* Allocate a string object from a C string. */
static union obj *
make_cstr(struct context *ctx, const char *value)
{
    union obj *o = NULL;
    size_t length = 0;

    /* Overflow should be impossible, but check anyway. */
    length = strlen(value);
    if (length > WORD_MAX) {
        die(ctx, "String value too long.");
    }

    o = make_str(ctx);
    resize_str(ctx, o, length);
    memcpy(o->str.slots, value, length);
    o->str.fill = length;
    return o;
}

/* Allocate an empty cons cell. */
static union obj *
make_cons(struct context *ctx)
{
    union obj *o;
    o = make_object(ctx, TAG_CONS, sizeof(struct cons));
    o->cons.car = NULL;
    o->cons.cdr = NULL;
    return o;
}

/* Allocate an empty symbol. */
static union obj *
make_sym(struct context *ctx)
{
    union obj *o;
    o = make_object(ctx, TAG_SYM, sizeof(struct sym));
    o->sym.str = NULL;
    return o;
}

/* Box a character value. */
static union obj *
make_chr(struct context *ctx, unsigned char value)
{
    union obj *o;
    o = make_object(ctx, TAG_CHR, sizeof(struct chr));
    o->chr.value = value;
    return o;
}

/* Box a fixnum value. */
static union obj *
make_fix(struct context *ctx, word_t value)
{
    union obj *o;
    o = make_object(ctx, TAG_FIX, sizeof(struct fix));
    o->fix.value = value;
    return o;
}

/* FNV-1a 32-bit hash for the symbol intern table. */
static word_t
hash_bytes(unsigned char *data, word_t length)
{
    word_t i;
    word_t hc;
    hc = 2166136261;
    for (i = 0; i < length; i++) {
        hc ^= (data[i] & 0xff);
        hc *= 16777619;
        hc &= 0xffffffff;
    }
    return hc;
}

/* Generic hash. */
static word_t
hash_object(struct context *ctx, union obj *o)
{
    if (o == NULL) {
        die(ctx, "Attempt to hash nothing.");
    }

    switch (o->head.tag) {
    case TAG_STR:
        return hash_bytes(o->str.slots, o->str.fill);
    case TAG_SYM:
        return hash_object(ctx, o->sym.str);
    default:
        die(ctx, "Object not hash-able.");
        return 0;
    }
}

/* Generic equals. */
static int
hash_equal(struct context *ctx, union obj *a, union obj *b)
{
    if (a == b) {
        return 1;
    }

    if (a == NULL || b == NULL || a->head.tag != b->head.tag) {
        return 0;
    }

    switch (a->head.tag) {
    case TAG_STR:
        if (a->str.fill != b->str.fill) {
            return 0;
        }
        return memcmp(a->str.slots, b->str.slots, a->str.fill) == 0;
    case TAG_SYM:
        return hash_equal(ctx, a->sym.str, b->sym.str);
    default:
        return 0;
    }
}

/* Re-insert values to resize hash table if it's overfull or under full. */
static void
table_rehash(struct context *ctx, union obj *t)
{
    union obj **keys;
    union obj **values;
    word_t cap;
    word_t new_size;
    word_t i;
    word_t j;
    word_t hc;
    word_t mask;

    /* If the table has a free slot and is at least half full, don't resize. */
    if (
        (t->table.size / 2 <= t->table.fill || t->table.size == TABLE_MIN_SIZE)
        && t->table.load + 1 < t->table.size
    ) {
        return;
    }

    /* Overflow check. */
    if (t->table.fill > WORD_MAX / (5 * sizeof(union obj *))) {
        die(ctx, "Table too large.");
    }

    /* Round up to the next power of two that is (5/4) larger than fill. */
    cap = ((t->table.fill + 1) * 5) / 4;
    new_size = TABLE_MIN_SIZE;
    while (new_size < cap) {
        new_size *= 2;
    }

    /* Allocate new storage. */
    keys = calloc(new_size, sizeof(union obj *));
    values = calloc(new_size, sizeof(union obj *));
    if (keys == NULL || values == NULL) {
        die(ctx, "Out of memory");
    }

    /* Re-insert old values. */
    mask = new_size - 1;
    for (j = 0; j < t->table.size; j++) {
        if (t->table.keys[j] == NULL || t->table.values[j] == NULL) {
            continue;
        }

        hc = hash_object(ctx, t->table.keys[j]);
        for (i = hc & mask; ; i = (i * 5 + 1) & mask) {
            if (keys[i] == NULL) {
                keys[i] = t->table.keys[j];
                values[i] = t->table.values[j];
                break;
            }
        }
    }

    /* Release the old storage. */
    free(t->table.keys);
    free(t->table.values);

    /* Update statistics. */
    t->table.keys = keys;
    t->table.values = values;
    t->table.size = new_size;
    t->table.load = t->table.fill;
}

/* Get the value of a key, or NULL if it is not set. */
static union obj *
table_get(struct context *ctx, union obj *t, union obj *k)
{
    word_t i;
    word_t hc;
    word_t mask;

    /* Empty table. */
    if (t->table.fill == 0) {
        return NULL;
    }

    hc = hash_object(ctx, k);
    mask = t->table.size - 1;
    for (i = hc & mask; ; i = (i * 5 + 1) & mask) {
        /* Reach the end of chain, value not found. */
        if (t->table.keys[i] == NULL) {
            return NULL;
        }

        /* Tombstone. */
        if (t->table.values[i] == NULL) {
            continue;
        }

        /* Continue down the chain if the key doesn't match. */
        if (hash_equal(ctx, k, t->table.keys[i])) {
            return t->table.values[i];
        }
    }
}

/* Add a value (v) to or remove (v=NULL) a key from the hash table. */
static void
table_set(struct context *ctx, union obj *t, union obj *k, union obj *v)
{
    word_t i;
    word_t ts;
    word_t hc;
    word_t mask;

    /* Rehash table to ensure at least one slot is free. */
    table_rehash(ctx, t);

    hc = hash_object(ctx, k);
    mask = t->table.size - 1;
    for (i = hc & mask, ts = -1; ; i = (i * 5 + 1) & mask) {
        /* Reach the end of chain, value not found. */
        if (t->table.keys[i] == NULL) {
            if (v != NULL) {
                if (ts == (word_t)-1) {
                    /* Add to the end of the chain. */
                    t->table.keys[i] = k;
                    t->table.values[i] = v;
                    t->table.fill += 1;
                    t->table.load += 1;
                } else {
                    /* Add to the first tombstone. */
                    t->table.keys[ts] = k;
                    t->table.values[ts] = v;
                    t->table.fill += 1;
                }
            }
            return;
        }

        /* Tombstone. */
        if (t->table.values[i] == NULL) {
            if (ts == (word_t)-1) {
                ts = i;
            }
            continue;
        }

        /* Continue down the chain if the key doesn't match. */
        if (hash_equal(ctx, k, t->table.keys[i])) {
            t->table.values[i] = v;

            /* Remove value */
            if (v == NULL) {
                t->table.fill -= 1;
            }

            return;
        }
    }
}

/* Internalize a symbol given a string that names it. */
BUILTIN(intern)
{
    LOCALS3(name, sym, table);

    BIND1(name);
    if (name == NULL) {
        die(ctx, "Not enough arguments to intern.");
    }

    table = ctx->intern;
    sym = table_get(ctx, table, name);
    if (sym == NULL) {
        sym = make_sym(ctx);
        sym->sym.str = name;
        table_set(ctx, table, name, sym);
    }

    VALUES1(sym);
}

/* Internalize all special symbols. */
BUILTIN(intern_special)
{
    LOCALS1(s);
#define SPECIAL_DEF(ident, name)                                        \
    do {                                                                \
        s = make_cstr(ctx, name);                                       \
        VALUES1(s);                                                     \
        intern(ctx);                                                    \
        BIND1(s);                                                       \
        INTERN(ident) = s;                                              \
    } while (0);
    SPECIAL_SYMBOLS
#undef SPECIAL_DEF
}

/* Initialize early global context. */
static struct context *
make_context(void)
{
    struct context *ctx;

    ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        die(NULL, "Out of memory.");
    }
    memset(ctx, 0, sizeof(*ctx));

    ctx->root_frame.parent = NULL;
    ctx->root_frame.func = "#<global>";
    ctx->root_frame.vars = ctx->globals;

    ctx->globals[0] = &ctx->vals;
    ctx->globals[1] = &ctx->special;
    ctx->globals[2] = &ctx->dyn_vars;
    ctx->globals[3] = &ctx->dyn_funcs;
    ctx->globals[4] = &ctx->dyn_macros;
    ctx->globals[5] = &ctx->dyn_symbols;
    ctx->globals[6] = &ctx->cur_code;
    ctx->globals[7] = &ctx->cur_closure;
    ctx->globals[8] = &ctx->cur_locals;
    ctx->globals[9] = NULL;

    ctx->cur_frame = &ctx->root_frame;

    ctx->vals = NULL;
    ctx->nvals = 0;

    ctx->intern = NULL;

    ctx->special = NULL;

    ctx->dyn_vars = NULL;
    ctx->dyn_funcs = NULL;
    ctx->dyn_macros = NULL;
    ctx->dyn_symbols = NULL;

    ctx->next_pc = 0;
    ctx->cur_code = NULL;
    ctx->cur_closure = NULL;
    ctx->cur_locals = NULL;

    ctx->gc_allobj = NULL;
    ctx->gc_queue = GC_QUEUE_BLACK;

    ctx->gc_live = 0;
    ctx->gc_pending = 0;

    ctx->gc_thresh = GC_THRESH_MIN;

    ctx->dyn_vars = make_table(ctx);
    ctx->dyn_funcs = make_table(ctx);
    ctx->dyn_macros = make_table(ctx);
    ctx->dyn_symbols = make_table(ctx);
    ctx->vals = make_vstruct(ctx, MIN_VALUES);
    ctx->special = make_vstruct(ctx, NUM_SPECIAL);

    ctx->intern = make_table(ctx);

    intern_special(ctx);

    return ctx;
}

/* Free memory owned by the global context. */
static void
free_context(struct context *ctx)
{
    /* Drop references. */
    ctx->cur_frame = NULL;

    /* Finish the in-progress collection. */
    sweep(ctx);

    /* Release memory. */
    sweep(ctx);
    free(ctx);
}

/* Skip white space to get to the start of a token. */
static int
skip_whitespace_getc(FILE *file)
{
    int ch;

    while (1) {
        ch = getc(file);

        if (ch != ';' && ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n') {
            /* Reached the start of a token. */
            break;
        }

        if (ch == ';') {
            /* Comment are treated as white space to the end of the line. */
            while (ch != '\n' && ch != EOF) {
                ch = getc(stdin);
            }
        }
    }

    return ch;
}

/* (cons car cdr) => #<cons> */
BUILTIN(cons)
{
    LOCALS3(car, cdr, cell);

    BIND2(car, cdr);
    if (car == NULL || cdr == NULL) {
        die(ctx, "cons expects exactly two arguments.");
    }

    cell = make_cons(ctx);
    cell->cons.car = car;
    cell->cons.cdr = cdr;

    VALUES1(cell);
}

/* (parse-integer s) => nil or #<fixnum> */
BUILTIN(parse_integer)
{
    int ch;
    word_t i;
    word_t x;
    LOCALS3(s, val, nil);
    BIND1(s);

    nil = INTERN(NIL);

    if (s == NULL || s->head.tag != TAG_STR) {
        VALUES1(nil);
        return;
    }

    x = 0;
    for (i = 0; i < s->str.fill; i++) {
        ch = s->str.slots[i];
        if (ch < '0' || ch > '9') {
            VALUES1(nil);
            return;
        }

        ch -= '0';
        if (x > WORD_MAX / 10 - ch) {
            die(ctx, "Overflow while parsing integer.");
        }

        x = x * 10 + ch;
    }

    val = make_fix(ctx, x);
    VALUES1(val);
}

/* Very basic lisp reader. */
BUILTIN(early_read)
{
    LOCALS4(val, end, item, nil);
    int ch;

    nil = INTERN(NIL);

    if (NVALS() != 0) {
        die(ctx, "early_read expects no arguments.");
    }

    ch = skip_whitespace_getc(stdin);

    /* Reached end of input. */
    if (ch == EOF) {
        return;
    }

    /* ,form => (unquote form) OR ,@form => (unquote-splicing form) */
    if (ch == ',') {
        ch = getc(stdin);
        if (ch == '@') {
            VALUES0();
            early_read(ctx);
            BIND1(item);
            if (item == NULL) {
                die(ctx, "Expected form after ,@ at end of input.");
            }

            VALUES2(item, nil);
            cons(ctx);
            BIND1(end);

            item = INTERN(UNQUOTE_SPLICING);
            VALUES2(item, end);
            cons(ctx);
            return;
        } else {
            ungetc(ch, stdin);

            end = INTERN(NIL);

            VALUES0();
            early_read(ctx);
            BIND1(item);
            if (item == NULL) {
                die(ctx, "Expected form after , at end of input.");
            }

            VALUES2(item, end);
            cons(ctx);
            BIND1(end);

            item = INTERN(UNQUOTE);
            VALUES2(item, end);
            cons(ctx);
            return;
        }
    }

    /* `form => (quasiquote form). */
    if (ch == '`') {
        end = INTERN(NIL);

        VALUES0();
        early_read(ctx);
        BIND1(item);
        if (item == NULL) {
            die(ctx, "Expected form after ` at end of input.");
        }

        VALUES2(item, end);
        cons(ctx);
        BIND1(end);

        item = INTERN(QUASIQUOTE);
        VALUES2(item, end);
        cons(ctx);
        return;
    }

    /* 'form => (quote form). */
    if (ch == '\'') {
        end = INTERN(NIL);

        VALUES0();
        early_read(ctx);
        BIND1(item);
        if (item == NULL) {
            die(ctx, "Expected form after ' at end of input.");
        }

        VALUES2(item, end);
        cons(ctx);
        BIND1(end);

        item = INTERN(QUOTE);
        VALUES2(item, end);
        cons(ctx);
        return;
    }

    /* "asdf" => #<string> */
    if (ch == '"') {
        val = make_str(ctx);

        while (1) {
            ch = getc(stdin);

            if (ch == '"') {
                VALUES1(val);
                return;
            }

            /* Simple escape. Backslash means next character is literal. */
            if (ch == '\\') {
                ch = getc(stdin);
            }

            /* Strings must be terminated. */
            if (ch == EOF) {
                die(ctx, "Reached end of input while reading a string.");
            }

            extend_str(ctx, val, ch);
        }
    }

    /* (a b ... y . z) => (cons a (cons b (cons ... (cons y z)))) */
    if (ch == '(') {
        val = nil;
        end = nil;

        while (1) {
            ch = skip_whitespace_getc(stdin);

            /* Closing parenthesis. End of current list. */
            if (ch == ')') {
                VALUES1(val);
                return;
            }

            /* Dot means read cdr. */
            if (ch == '.') {
                VALUES0();
                early_read(ctx);
                BIND1(item);
                if (item == NULL) {
                    die(ctx, "Reached end of input while reading list.");
                }

                ch = skip_whitespace_getc(stdin);
                if (ch != ')') {
                    die(ctx, "Expected ) after cdr item.");
                }

                if (end == nil) {
                    die(ctx, "List with cdr dot must have at least two items");
                }

                end->cons.cdr = item;

                VALUES1(val);
                return;
            }

            ungetc(ch, stdin);

            /* Read a car item. */
            VALUES0();
            early_read(ctx);
            BIND1(item);
            if (item == NULL) {
                die(ctx, "Reached end of input while reading list.");
            }

            if (end == nil) {
                VALUES2(item, nil);
                cons(ctx);
                BIND1(val);
                BIND1(end);
            } else {
                VALUES2(item, nil);
                cons(ctx);
                BIND1(item);
                end->cons.cdr = item;
                end = item;
            }
        }
    }

    /* Unmatched closing parenthesis */
    if (ch == ')') {
        die(ctx, "Extra unmatched closing parenthesis.");
    }

    /* Dispatching macro character. */
    if (ch == '#') {
        ch = getc(stdin);

        /* #'form => (function form) */
        if (ch == '\'') {
            end = INTERN(NIL);

            VALUES0();
            early_read(ctx);
            BIND1(item);
            if (item == NULL) {
                die(ctx, "Expected form after #' at end of input.");
            }

            VALUES2(item, end);
            cons(ctx);
            BIND1(end);

            item = INTERN(FUNCTION);
            VALUES2(item, end);
            cons(ctx);
            return;
        }

        die(ctx, "Undefined dispatching macro character.");
    }

    /* Symbol or number token. */
    val = make_str(ctx);
    while (1) {
        switch (ch) {
        default:
            /* Check for constituent characters. */
            if (ch >= '!' && ch <= '~') {
                extend_str(ctx, val, ch);
                break;
            }

        case ';': case '"': case '\'': case '(': case ')': case ',': case '`':
            /* Terminating characters. */
            ungetc(ch, stdin);
            VALUES1(val);
            parse_integer(ctx);
            BIND1(item);
            if (item != nil) {
                VALUES1(item);
                return;
            }
            VALUES1(val);
            intern(ctx);
            return;
        }

        ch = getc(stdin);
    }
}

/* Write a single value out. */
BUILTIN(early_write)
{
    word_t i;
    LOCALS4(val, item, rest, nil);
    BIND1(val);
    nil = INTERN(NIL);

    if (val == NULL) {
        VALUES1(val);
        return;
    }

    switch (val->head.tag) {
    case TAG_TABLE:
        printf("#<hash-table>");
        VALUES1(val);
        return;

    case TAG_FUNC:
        printf("#<function>");
        VALUES1(val);
        return;

    case TAG_VSTRUCT:
        printf("#<struct>");
        VALUES1(val);
        return;

    case TAG_VEC:
        printf("#(");
        i = 0;
        while (i < val->vec.fill) {
            item = val->vec.slots[i++];
            VALUES1(item);
            early_write(ctx);
            if (i != val->vec.fill) {
                printf(" ");
            }
        }
        printf(")");
        VALUES1(val);
        return;

    case TAG_NVEC:
        printf("#(");
        i = 0;
        while (i < val->nvec.fill) {
            printf("%u", val->nvec.slots[i++]);
            if (i != val->nvec.fill) {
                printf(" ");
            }
        }
        printf(")");
        VALUES1(val);
        return;

    case TAG_STR:
        printf("\"");
        for (i = 0; i < val->str.fill; i++) {
            if (val->str.slots[i] == '"' || val->str.slots[i] == '\\') {
                printf("\\");
            }
            printf("%c", val->str.slots[i++]);
        }
        printf("\"");
        VALUES1(val);
        return;

    case TAG_CONS:
        printf("(");

        item = val;
        while (1) {
            rest = item->cons.cdr;
            item = item->cons.car;

            VALUES1(item);
            early_write(ctx);

            if (rest == nil) {
                break;
            } else if (rest->head.tag != TAG_CONS) {
                printf(" . ");
                VALUES1(rest);
                early_write(ctx);
                break;
            } else {
                printf(" ");
                item = rest;
            }
        }

        printf(")");
        VALUES1(val);
        return;


    case TAG_SYM:
        item = val->sym.str;
        for (i = 0; i < item->str.fill; i++) {
            printf("%c", item->str.slots[i]);
        }
        VALUES1(val);
        return;

    case TAG_CHR:
        switch (val->chr.value) {
        case ' ':
            printf("#\\Space");
            break;
        case '\t':
            printf("#\\Tab");
            break;
        case '\r':
            printf("#\\Return");
            break;
        case '\n':
            printf("#\\Newline");
            break;
        case '\f':
            printf("#\\Page");
            break;
        default:
            printf("#\\%c", val->chr.value);
            break;
        }
        VALUES1(val);
        return;

    case TAG_FIX:
        printf("%u", val->fix.value);
        VALUES1(val);
        return;

    default:
        die(ctx, "Invalid tag.");
    }
}

/* Write a value and break the line. */
BUILTIN(early_print)
{
    LOCALS0();
    early_write(ctx);
    printf("\n");
}


/* Interpreter for compiled expressions. */
BUILTIN(execute_code)
{
    LOCALS0();
    /* TODO bounded C stack. */
    while (1) {
    }
}

/* Setup the environment to enter the interpreter. */
BUILTIN(apply)
{
    LOCALS5(func, args, saved, item, nil);
    BIND2(func, args);

    nil = INTERN(NIL);

    if (func == NULL || func->head.tag != TAG_FUNC) {
        die(ctx, "Can't evaluate something that's not a function.");
    }

    VALUES0();
    while (args != nil) {
        if (args == NULL || args->head.tag != TAG_CONS) {
            die(ctx, "Invalid argument list.");
        }
        PUSH_VALUE(item);
        args = args->cons.cdr;
    }

    saved = make_func(ctx);
    saved->func.closure = ctx->cur_closure;
    saved->func.locals = ctx->cur_locals;
    saved->func.code = ctx->cur_code;
    saved->func.pc = ctx->next_pc;

    ctx->cur_closure = func->func.closure;
    ctx->cur_locals = func->func.locals;
    ctx->cur_code = func->func.code;
    ctx->next_pc = func->func.pc;

    execute_code(ctx);

    ctx->cur_closure = saved->func.closure;
    ctx->cur_locals = saved->func.locals;
    ctx->cur_code = saved->func.code;
    ctx->next_pc = saved->func.pc;
}

/* Expand all macros. */
BUILTIN(compiler_macroexpand)
{
    LOCALS1(form);
    BIND1(form);
}

/* Top-level expression compiler. */
BUILTIN(compile_toplevel)
{
    LOCALS2(val, nil);
    BIND1(val);

    compiler_macroexpand(ctx);

    /* TODO:
     * quote
     * macros+macrolet+symbol-macrolet
     * tagbody+go+block+return-from+unwind-protect
     * catch+throw
     * setq+let+let*
     * flet+labels+lambda+function
     * locally+the
     * multiple-value-call+call
     * multiple-value-prog1
     * if
     * progn
     * progv
     * eval-when
     */

    VALUES0();
}

/* Very basic compiling eval. */
BUILTIN(early_eval)
{
    LOCALS3(form, func, nil);
    BIND1(form);

    nil = INTERN(NIL);

    VALUES1(form);
    compile_toplevel(ctx);
    BIND1(func);

    VALUES2(func, nil);
    apply(ctx);
}

/* Load source expressions from stdin and eval */
BUILTIN(early_read_eval_loop)
{
    LOCALS0();

    while (1) {
        /* Read a source expression. */
        VALUES0();
        early_read(ctx);

        /* No values are returned from early_read if we reached end of input. */
        if (NVALS() == 0) {
            break;
        }

        /* Compile and execute the expression. */
        early_eval(ctx);

        /* Print the expression. */
        early_print(ctx);
    }
}

int
main(int argc, char **argv)
{
    struct context *ctx;

    /* Setup the global environment. */
    ctx = make_context();

    /* Read and evaluate expressions. */
    early_read_eval_loop(ctx);

    /* Free memory. */
    free_context(ctx);

    return 0;
}

/* vim: set et sw=4 : */
