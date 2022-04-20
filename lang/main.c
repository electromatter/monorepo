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


union object *gclist = NULL;
union object *markhead = NULL;
union object **sweephead = NULL;
unsigned int youngcount = 0;
unsigned int oldcount = 0;


void sweep1(void) {
    union object *obj;
    obj = *sweephead;
    if (obj == NULL) {
        sweephead = NULL;
        return;
    }
    if (obj->head.mark) {
        obj->head.mark = 0;
        sweephead = &obj->head.next;
    } else {
        *sweephead = obj->head.next;
        free(obj);
    }
}


void mark(union object *obj) {
    if (obj->head.mark != 0) {
        return;
    }
    obj->head.mark = 1;
    obj->head.back = markhead;
    markhead = obj;
}


void mark1(void) {
    union object *obj;
    obj = markhead;
    if (obj == NULL) {
        return;
    }
    markhead = obj->head.back;
    switch (obj->head.tag) {
    case TAG_NIL:
        break;
    case TAG_CONS:
        mark(obj->cons.car);
        mark(obj->cons.cdr);
        break;
    case TAG_STRING:
        break;
    case TAG_SYMBOL:
        mark(obj->symbol.name);
        break;
    case TAG_FIXNUM:
        break;
    default:
        die("INVALID TAG %p", (void *)obj);
    }
}


void markroots(void) {
}


void collect(int full) {
    unsigned int i;

    if (!full && youngcount < oldcount) {
        return;
    }

    if (markhead == NULL && sweephead == NULL) {
        markroots();
    }

    for (i = 0; full || (i < GC_MAX_BATCH); i++) {
        if (markhead != NULL) {
            mark1();
        } else if (sweephead != NULL) {
            sweep1();
        } else {
            return;
        }
    }
}


union object *makeobj(int tag, size_t size) {
    union object *ret;
    collect(0);
    ret = malloc(size);
    if (ret == NULL) {
        die("OUT OF MEMORY");
    }
    ret->head.tag = tag;
    ret->head.mark = 0;
    ret->head.next = gclist;
    ret->head.back = NULL;
    gclist = ret;
    youngcount += 1;
    return ret;
}


union object *makenil(void) {
    static union object ob = {{TAG_NIL, 0, NULL, NULL}};
    return &ob;
}


union object *makecons(union object *car, union object *cdr) {
    union object *ret = NULL;
    ret = makeobj(TAG_CONS, sizeof(ret->cons));
    ret->cons.car = car;
    ret->cons.cdr = cdr;
    return ret;
}


union object *makestring(unsigned char *value, unsigned int length) {
    union object *ret = NULL;
    ret = makeobj(TAG_STRING, sizeof(ret->string) + length);
    memcpy(ret + 1, value, length);
    ret->string.value = (unsigned char *)(ret + 1);
    ret->string.length = length;
    return ret;
}


union object *makesymbol(unsigned char *name, unsigned int length) {
    union object *ret = NULL;
    ret = makeobj(TAG_SYMBOL, sizeof(ret->symbol));
    ret->symbol.name = makestring(name, length);
    return ret;
}


union object *makefixnum(int value) {
    union object *ret = NULL;
    ret = makeobj(TAG_FIXNUM, sizeof(ret->fixnum));
    ret->fixnum.value = value;
    return ret;
}


union object *cintern(char *name) {
    if (strcmp(name, "nil") == 0) {
        return makenil();
    }
    return makesymbol((unsigned char *)name, strlen(name));
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
            die("FIXNUM OVERFLOW");
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

    if (length == 3 && memcmp(maybeint, "nil", 3) == 0) {
        return makenil();
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
                cintern("quote"),
                makecons(lisp_read(1), makenil())
            );

        case '`':
            return makecons(
                cintern("quasiquote"),
                makecons(lisp_read(1), makenil())
            );

        case ',':
            ch = getc(stdin);
            if (ch == '@') {
                obj = cintern("unquote-splicing");
            } else {
                obj = cintern("unquote");
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
        fputs("nil", stdout);
        break;

    case TAG_CONS:
        putc('(', stdout);
        while (1) {
            lisp_write(object->cons.car);
            if (lisp_isnil(object->cons.cdr)) {
                break;
            } else if (object->cons.cdr == NULL || object->cons.cdr->head.tag != TAG_CONS) {
                fputs(" . ", stdout);
                lisp_write(object->cons.cdr);
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
    collect(1);

    printf("Hello, world!\n");
    return 0;
}
