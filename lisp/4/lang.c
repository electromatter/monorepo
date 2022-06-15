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
#include <assert.h>
#include <string.h>

#define HTBL_INIT   {0, 0, 0, 0, 0, NULL}

#define HTBL_FOREACH(var, tab)                                      \
    for (                                                           \
        (var) = &(tab)->slots[0].item;                              \
        (var) != &(tab)->slots[(tab)->size].item;                   \
        (var) = (void *)((char *)(var) + sizeof(*(tab)->slots))     \
    )                                                               \
        if (2 <= *(unsigned int *)(                                 \
                (char *)(var) +                                     \
                ((char *)&(tab)->slots[0].hc -                      \
                    (char *)&(tab)->slots[0].item)                  \
            )                                                       \
        )

#define HTBL_DESTROY(tab) do {                                      \
        free((tab)->slots);                                         \
        (tab)->quota = 0;                                           \
        (tab)->load = 0;                                            \
        (tab)->fill = 0;                                            \
        (tab)->cap = 0;                                             \
        (tab)->size = 0;                                            \
        (tab)->slots = NULL;                                        \
    } while (0)

#define HTBL_DEF(name, t, hashf, eqf)                               \
struct name {                                                       \
    unsigned int quota, load, fill, cap, size;                      \
    struct {                                                        \
        unsigned int hc;                                            \
        t item;                                                     \
    } *slots;                                                       \
};                                                                  \
                                                                    \
int                                                                 \
name##_slot(struct name *tab, t key, t **slot, int add)             \
{                                                                   \
    void *new_slots;                                                \
    struct name saved;                                              \
    unsigned int i, hc, mask, new_size;                             \
                                                                    \
    /* load=occupied, fill=tombstones+load */                       \
    assert(tab->load <= tab->fill);                                 \
    assert(tab->fill <= tab->cap);                                  \
    assert(tab->size == 0 || tab->cap < tab->size);                 \
    assert((tab->size & (tab->size - 1)) == 0);                     \
                                                                    \
    /* rehash if table is overloaded */                             \
    if (tab->fill >= tab->cap || tab->load < tab->quota) {          \
        new_size = 32;                                              \
        while (new_size < tab->load * 2)                            \
            new_size *= 2;                                          \
                                                                    \
        new_slots = calloc(new_size, sizeof(*tab->slots));          \
        if (new_slots == NULL) {                                    \
            /* only complain when adding items */                   \
            if (add > 0)                                            \
                return -1;                                          \
        } else {                                                    \
            /* update housekeeping */                               \
            saved = *tab;                                           \
            tab->slots = new_slots;                                 \
            tab->quota = new_size < 32 ? 32 : new_size / 2;         \
            tab->cap = (new_size * 3) / 4;                          \
            tab->size = new_size;                                   \
            tab->fill = tab->load;                                  \
                                                                    \
            /* reinsert the old items */                            \
            mask = new_size - 1;                                    \
            for (i = 0; i < saved.size; i++) {                      \
                if (saved.slots[i].hc >= 2) {                       \
                    hc = saved.slots[i].hc & mask;                  \
                    while (tab->slots[hc].hc != 0)                  \
                        hc = (hc * 5 + 1) & mask;                   \
                    tab->slots[hc] = saved.slots[i];                \
                }                                                   \
            }                                                       \
                                                                    \
            /* release the old table */                             \
            free(saved.slots);                                      \
        }                                                           \
    }                                                               \
                                                                    \
    /* table size is power of two */                                \
    mask = tab->size - 1;                                           \
                                                                    \
    /* hc=0: empty, hc=1: tombstone */                              \
    hc = (hashf)(key);                                              \
    if (hc < 2)                                                     \
        hc = ~(unsigned int)0;                                      \
                                                                    \
    i = hc & mask;                                                  \
    while (1) {                                                     \
        /* empty slot or tombstone */                               \
        if (tab->slots[i].hc < 2) {                                 \
            if (add > 0) {                                          \
                /* set: empty slot, increase load */                \
                if (tab->slots[i].hc == 0)                          \
                    tab->fill++;                                    \
                tab->load++;                                        \
                /* set: make occupied */                            \
                tab->slots[i].hc = hc;                              \
                if (slot != NULL)                                   \
                    *slot = &tab->slots[i].item;                    \
            } else {                                                \
                /* del/get: not found */                            \
                if (slot != NULL)                                   \
                    *slot = NULL;                                   \
            }                                                       \
            return 0;                                               \
        }                                                           \
                                                                    \
        /* occupied slot, check match */                            \
        if (tab->slots[i].hc == hc &&                               \
                (eqf)(key, tab->slots[i].item)) {                   \
            /* del: convert to tombstone */                         \
            if (add < 0) {                                          \
                tab->slots[i].hc = 1;                               \
                tab->load--;                                        \
            }                                                       \
            /* match found */                                       \
            if (slot != NULL)                                       \
                *slot = &tab->slots[i].item;                        \
            return 1;                                               \
        }                                                           \
                                                                    \
        /* open addressing using maximal order lcg */               \
        i = (i * 5 + 1) & mask;                                     \
    }                                                               \
}

#define CH_EPSILON  (-1)

struct nfa {
    struct nfa *out, *alt, *dom;
    int indeg, ch, mark;
};

struct nfa *
nod(int ch)
{
    struct nfa *n;
    n = malloc(sizeof(*n));
    if (n == NULL)
        return NULL;
    memset(n, 0, sizeof(*n));
    n->dom = n;
    n->ch = ch;
    return n;
}

struct nfa *
cat(struct nfa *a, struct nfa *b)
{
    assert(a != NULL && b != NULL);
    a->dom->out = b;
    a->dom->dom = b->dom;
    a->dom = b->dom;
    b->indeg++;
    return a;
}

struct nfa *
alt(struct nfa *a, struct nfa *b)
{
    struct nfa *in = NULL, *out = NULL;
    assert(a != NULL && b != NULL && a != b);

    in = nod(CH_EPSILON);
    if (in == NULL)
        return NULL;

    out = nod(CH_EPSILON);
    if (out == NULL) {
        free(in);
        return NULL;
    }

    a->dom->out = out;
    a->dom->dom = out;
    a->dom = out;

    b->dom->out = out;
    b->dom->dom = out;
    b->dom = out;

    out->indeg = 2;

    in->out = a;
    in->alt = b;
    in->dom = out;

    a->indeg++;
    b->indeg++;

    return in;
}

struct nfa *
plus(struct nfa *a)
{
    struct nfa *ep;
    assert(a != NULL);
    ep = nod(CH_EPSILON);
    if (ep == NULL)
        return NULL;
    a->dom->out = ep;
    a->dom->dom = ep;
    a->dom = ep;
    ep->indeg = 1;
    ep->alt = a;
    a->indeg++;
    return a;
}

void
destroy(struct nfa *a)
{
    struct nfa *out, *alt;
    while (a != NULL) {
        out = a->out;
        a->out = NULL;

        alt = a->alt;
        a->alt = NULL;

        if (a->indeg == 0)
            free(a);

        a = out;

        if (alt != NULL) {
            alt->indeg--;
            destroy(alt);
        }

        if (a != NULL)
            a->indeg--;
    }
}

struct dfa {
    struct dfa *out[256];
    int indeg, accept, mark;
};

struct dfa *
mkd(void)
{
    struct dfa *d;
    d = malloc(sizeof(*d));
    if (d == NULL)
        return NULL;
    memset(d, 0, sizeof(*d));
    return d;
}

void
destroyd(struct dfa *d)
{
    struct dfa tmp;
    struct dfa *b;
    int i, indeg;
    d->indeg++;
    for (i = 0; i < 256; i++) {
        b = d->out[i];
        if (b == NULL)
            continue;
        d->out[i] = NULL;
        b->indeg--;
        destroyd(b);
    }
    if (--d->indeg == 0)
        free(d);
}

struct act {
    struct dfa *d;
    unsigned int n, cap;
    struct nfa **st;
};

int
acteq(struct act a, struct act b)
{
    return a.n == b.n && !memcmp(a.st, b.st, sizeof(*a.st) * a.n);
}

int
ptrcmp(const void *a, const void *b)
{
    if ((unsigned long)*(void **)a < (unsigned long)*(void **)b)
        return -1;
    if ((unsigned long)*(void **)a > (unsigned long)*(void **)b)
        return 1;
    return 0;
}

void
reduceact(struct act *act) {
    unsigned int i, j;
    qsort(act->st, act->n, sizeof(*act->st), ptrcmp);
    for (i = 1, j = 0; i < act->n; i++)
        if (act->st[j] != act->st[i])
            act->st[++j] = act->st[i];
    if (act->n >= 2 && j != act->n)
        act->n = j;
}

void
trimact(struct act *act) {
    void *tmp;
    reduceact(act);
    if (act->cap != act->n) {
        tmp = realloc(act->st, sizeof(act->st[0]) * act->n);
        if (tmp != NULL)
            act->st = tmp;
        act->cap = act->n;
    }
}

unsigned int
acthash(struct act act)
{
    unsigned int hc = 11, i;
    for (i = 0; i < act.n; i++)
        hc = (hc * 3) ^ ((unsigned long)act.st[i]);
    return hc;
}

HTBL_DEF(acttab, struct act, acthash, acteq)

int
pushact(struct act *act, struct nfa *n)
{
    void *tmp;
    unsigned int new_cap;
    if (act->n == act->cap) {
        reduceact(act);
        new_cap = act->cap > 0 ? act->cap * 2 : 16;
        tmp = realloc(act->st, sizeof(act->st) * new_cap);
        if (tmp == NULL)
            return -1;
        act->st = tmp;
        act->cap = new_cap;
    }
    act->st[act->n++] = n;
    return 0;
}

int
collapse_epsilon(struct act *next, struct nfa *a, struct dfa *d)
{
    while (1) {
        if (a == NULL)
            return 0;

        if (a->dom == a)
            d->accept = 1;

        if (a->ch == CH_EPSILON) {
            if (a->alt != NULL && collapse_epsilon(next, a->alt, d))
                return -1;
            a = a->out;
        } else {
            return pushact(&next[a->ch], a);
        }
    }
}

struct pentry {
    struct pentry *next;
    struct dfa *d;
    struct act key;
};

int
memo_states(
        struct acttab *tab,
        struct act *next,
        struct dfa *cur,
        struct pentry **stack
)
{
    unsigned int i;
    struct pentry *ent;
    struct act *slot;
    struct dfa *b;
    int ret;
    for (i = 0; i < 256; i++) {
        if (next[i].n > 0) {
            trimact(&next[i]);
            ret = acttab_slot(tab, next[i], &slot, 1);
            if (ret < 0) {
                return -1;
            } else if (ret == 0) {
                memset(slot, 0, sizeof(*slot));

                b = mkd();
                if (b == NULL)
                    return -1;

                ent = malloc(sizeof(*ent));
                if (ent == NULL)
                    return -1;
                ent->d = b;
                ent->key = next[i];
                ent->next = *stack;

                *stack = ent;
                *slot = next[i];
                slot->d = b;
            } else {
                free(next[i].st);
            }
            cur->out[i] = slot->d;
            slot->d->indeg++;
        }
    }
    memset(next, 0, sizeof(next[0]) * 256);
    return 0;
}

struct dfa *
power_expand(struct nfa *a)
{
    unsigned int i;
    struct acttab tab = HTBL_INIT;
    struct act *slot, next[256];
    struct dfa *root, *cur;
    struct pentry *stack = NULL, *top;

    /* initial state */
    root = mkd();
    if (root == NULL)
        return NULL;
    cur = root;

    /* expand the initial state */
    memset(next, 0, sizeof(next));
    if (collapse_epsilon(next, a, cur))
        goto fail;
    if (memo_states(&tab, next, cur, &stack))
        goto fail;

    while (stack != NULL) {
        top = stack;
        stack = top->next;
        cur = top->d;
        /* expand links */
        for (i = 0; i < top->key.n; i++) {
            a = top->key.st[i];
            if (collapse_epsilon(next, a->out, cur))
                goto fail;
            if (collapse_epsilon(next, a->alt, cur))
                goto fail;
        }
        free(top);
        /* lookup dfa note */
        if (memo_states(&tab, next, cur, &stack))
            goto fail;
    }

    /* release the memo table */
    HTBL_FOREACH(slot, &tab)
        free(slot->st);
    HTBL_DESTROY(&tab);
    return root;
fail:
    abort();
}

struct minnode {
    int id, indeg;
    struct {
        int c;
        struct minnode *node;
    } edge[0];
};

struct minnode *
flip(struct dfa *d, int *nextid)
{
    int i;
    struct minnode *r;
    r = malloc(sizeof(*r) + sizeof(r->edge) * d->indeg);
    if (r == NULL)
        abort();
    r->id = (*nextid)++;
    r->indeg = d->indeg;
    for (i = 0; i < 256; i++) {
        if (d->out[i] != NULL) {

        }
    }

}

struct dfa *
minimize(struct dfa *a)
{
    struct minnode *r;
    int nextid = 0;
    r = flip(a, &nextid);
    return mkd();
}

void
showd(struct dfa *d)
{
    int i;
    d->mark = 1;
    printf("%p%c", d, d->accept ? '*' : ' ');
    for (i = 0; i < 256; i++) {
        if (d->out[i] != NULL)
            printf(" %x=%p", i, d->out[i]);
    }
    printf("\n");
    for (i = 0; i < 256; i++)
        if (d->out[i] != NULL && !d->out[i]->mark)
            showd(d->out[i]);
}

void
show(struct nfa *n)
{
    if (n->mark)
        return;
    n->mark = 1;
    printf("%p", n);
    if (n->ch != CH_EPSILON)
        printf("%c(%c): ", n->dom == n ? '*' : ' ', n->ch);
    else
        printf("%c:    ", n->dom == n ? '*' : ' ');
    printf("%p %p\n", n->out, n->alt);
    if (n->out != NULL)
        show(n->out);
    if (n->alt != NULL)
        show(n->alt);
}

int
main(int argc, char **argv)
{
    struct nfa *n;
    struct dfa *d, *m;
    /* (a(eb|cb)d)* */
    n = (
        plus(
            cat(
                nod('a'),
                cat(
                    alt(
                        cat(
                            nod('e'),
                            nod('b')
                        ),
                        cat(
                            nod('c'),
                            nod('b')
                        )
                    ),
                    nod('d')
                )
            )
        )
    );
    d = power_expand(n);
    printf("nfa:\n");
    show(n);
    printf("dfa:\n");
    showd(d);
    m = minimize(d);
    printf("min:\n");
    showd(m);
    destroy(n);
    destroyd(m);
    return 0;
}
