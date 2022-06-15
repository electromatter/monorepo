#!/usr/bin/env python3
################################################################################
# Copyright (c) 2022 Eric Chai <electromatter@gmail.com>                       #
#                                                                              #
# Permission to use, copy, modify, and/or distribute this software for any     #
# purpose with or without fee is hereby granted, provided that the above       #
# copyright notice and this permission notice appear in all copies.            #
#                                                                              #
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES     #
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF             #
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR      #
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES       #
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER              #
# IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING       #
# OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.        #
################################################################################


import io
import sys
import collections


class Item(collections.namedtuple('_Item', 'pattern index root rule')):
    def advance(self):
        if self.index == len(self.pattern):
            raise ValueError
        return Item(self.pattern, self.index + 1, False, self.rule)

    def is_kernel(self):
        return bool(self.index) or self.root


class Rule:
    def __init__(self, name):
        self.name = name
        self.items = []
        self.item_set = set()

    def prod(self, *args):
        item = Item(tuple(filter(None, args)), 0, False, self)
        if item in self.item_set:
            raise ValueError('Duplicate production')
        self.items.append(item)
        self.item_set.add(item)

    def gen(self):
        'Generate the LALR parser table for a grammar rooted at this rule'
        def gotos(state):
            links = {}
            queue = list(state)
            seen = set()
            while queue:
                item = queue.pop()
                if item in seen or item.index >= len(item.pattern):
                    continue
                seen.add(item)
                symbol = item.pattern[item.index]
                links.setdefault(symbol, set()).add(item.advance())
                if not isinstance(symbol, Rule):
                    continue
                for subitem in symbol.items:
                    queue.append(subitem)
            return {sym: frozenset(items) for sym, items in links.items()}
        # Compute the states of the SR
        start_state = frozenset([Item((self, None), 0, True, self)])
        states = {}
        queue = [start_state]
        while queue:
            state = queue.pop()
            if state in states:
                continue
            links = gotos(state)
            states[state] = links
            queue.extend(links.values())
        first_sets = {}
        def first(x):
            result = first_sets.get(x)
            if result is not None:
                return result
            queue = {x}
            back = {}
            while queue:
                item = queue.pop()
                result = set()
                if isinstance(item, Rule):
                    for sub in item.items:
                        back.setdefault(sub, set()).add(item)
                        firstset = first_sets.get(sub)
                        if firstset is None:
                            queue.add(sub)
                        else:
                            result.update(firstset)
                else:
                    for sym in item.pattern[item.index:]:
                        if not isinstance(sym, Rule):
                            result.add(sym)
                            break
                        back.setdefault(sym, set()).add(item)
                        firstset = first_sets.get(sym)
                        if firstset is None:
                            queue.add(sym)
                            break
                        result |= firstset
                        result.discard('')
                        if '' not in firstset:
                            break
                    else:
                        result.add('')
                result = frozenset(result)
                if first_sets.get(item) != result:
                    first_sets[item] = result
                    queue.update(back.get(item, ()))
            return first_sets[x]
        # Compute look-a-head tokens start with $ for the start state
        def mklookaheads():
            start = next(iter(start_state))
            result = {}
            stack = [(start_state, start, frozenset([None]))]
            while stack:
                state, item, newlook = stack.pop()
                look = result.setdefault(state, {}).setdefault(item, set())
                if newlook <= look:
                    continue
                look.update(newlook)
                if item.index >= len(item.pattern):
                    continue
                symbol = item.pattern[item.index]
                rest = item.advance()
                go = states[state][symbol]
                stack.append((go, rest, look))
                if isinstance(symbol, Rule):
                    sublook = set(first(rest))
                    if '' in sublook:
                        sublook |= look
                        sublook.discard('')
                    sublook = frozenset(sublook)
                    for subitem in symbol.items:
                        stack.append((state, subitem, sublook))
            return result
        lookaheads = mklookaheads()
        def mkident():
            identifiers = {}
            def ident(state):
                state_id = identifiers.get(state)
                if state_id is None:
                    state_id = len(identifiers)
                    identifiers[state] = state_id
                return state_id
            return ident
        sident = mkident()
        rident = mkident()
        sr_table = {}
        goto_table = {}
        # Build tables
        for state, links in states.items():
            gtab = goto_table.setdefault(sident(state), {})
            stab = sr_table.setdefault(sident(state), {})
            for symbol, next_state in links.items():
                if isinstance(symbol, Rule):
                    gtab[rident(symbol)] = sident(next_state)
                else:
                    stab[symbol] = ('s', sident(next_state))
            if Item((self, None), 1, False, self) in state:
                stab[None] = ('a', None)
            for item in state:
                if item.index < len(item.pattern):
                    continue
                for symbol in lookaheads[state][item]:
                    if stab.get(symbol, '_')[0] == 's':
                        print('shift/reduce conflict, shift selected.')
                        continue
                    elif stab.get(symbol, '_')[0] == 'r':
                        print('reduce/reduce conflict, arbitrarily selected.')
                        continue
                    stab[symbol] = ('r', (rident(item.rule), len(item.pattern), item.rule.name))
        return sr_table, goto_table, sident(start_state)

    def __repr__(self):
        return '<' + self.name + '>'


class SRDriver:
    def __init__(self, sr_table, goto_table, start):
        self.sr_table = sr_table
        self.goto_table = goto_table
        self.start = start
        self.state_stack = []
        self.item_state = []

    def reset(self):
        self.state_stack = [self.start]
        self.item_stack = []

    def run(self, stream):
        self.reset()
        it = iter(stream)
        saved = None
        def peek():
            nonlocal saved
            if saved is None:
                saved = next(it, None)
            return saved
        while True:
            state = self.state_stack[-1]
            a, s = self.sr_table[state][peek()]
            if a == 's':
                print('Shift', s)
                self.state_stack.append(s)
                self.item_stack.append(peek())
                saved = None
            elif a == 'r':
                r, n, name = s
                print('Reduce', name, n)
                args = self.item_stack[-n:]
                del self.item_stack[-n:]
                del self.state_stack[-n:]
                state = self.state_stack[-1]
                self.state_stack.append(self.goto_table[state][r])
                self.item_stack.append((name,) + tuple(args))
            elif a == 'a':
                print('Accept')
                print(self.item_stack[0])
                return

    def genc(self, file=sys.stdout):
        do_return = False
        if file is None:
            file = io.StringIO()
            do_return = True

        file.write('''void reduce(int rule, int num);
void push(int state);
void clearstack(int start);
int getstate(void);
void consume(void);
int peektok(void);
enum rule{
RULE_ERROR = -2,
RULE_EOF = -1,
''')
        seen = set()
        for state in self.sr_table.values():
            for act, arg in state.values():
                if act == 'r':
                    _, _, name = arg
                    if name in seen:
                        continue
                    seen.add(name)
        for name in sorted(seen):
            file.write(name + ',\n')
        file.write('};\n')

        file.write('const char *strrule(int rule){\nswitch(rule){\n')
        for name in sorted(seen):
            file.write('case %s:return "%s";\n' % (name, name))
        file.write('default:return "UNKNOWN";\n}\n}\n')

        seengoto = set()
        def outgoto(num, rule):
            if rule in seengoto:
                file.write('goto goto%s;\n' % rule)
                return
            seengoto.add(rule)
            file.write('goto%s:\n' % rule)
            file.write('switch(getstate()){\n')
            for state, tab in self.goto_table.items():
                if rule in tab:
                    file.write('case %d:push(%d);break;\n' % (state, tab[rule]))
            file.write('default:return 0;\n')
            file.write('}\n')
            file.write('continue;\n')
        file.write('int parse(void){\n')
        file.write('clearstack(%d);\n' % self.start)
        file.write('while(1){\n')
        file.write('switch(getstate()){\n')
        for num, table in sorted(self.sr_table.items(), key=lambda x: x[0]):
            file.write('case %d:\n' % num)
            file.write('switch(peektok()){\n')
            tokens = sorted(filter(None, table))
            if None in table:
                tokens.append(None)
            for tok in tokens:
                act, arg = table[tok]
                if tok is None:
                    tok = '-1'
                file.write('case %s:\n' % tok)
                if act == 'a':
                    file.write('return 1;\n')
                elif act == 's':
                    file.write('push(%s);consume();\ncontinue;\n' % arg)
                elif act == 'r':
                    file.write('reduce(%s, %d);\n' % (arg[2], arg[1]))
                    outgoto(num, arg[0])
            file.write('default:return 0;\n}\n')
        file.write('default:return 0;\n}\n}\n}\n')

        if do_return:
            return file.getvalue()


def main():
    BASE = Rule('BASE')

    MUL = Rule('MUL')
    MUL.prod(BASE, '*', MUL)
    MUL.prod(BASE, '/', MUL)
    MUL.prod(BASE, '%', MUL)
    MUL.prod(BASE)

    ADD = Rule('ADD')
    ADD.prod(MUL, '+', ADD)
    ADD.prod(MUL, '-', ADD)
    ADD.prod(MUL)

    BASE.prod('(', ADD, ')')
    BASE.prod('NUM')
    BASE.prod('+', BASE)
    BASE.prod('-', BASE)

    EXPR = Rule('EXPR')
    EXPR.prod(ADD)

    sr, gt, s = EXPR.gen()
    print('Start', s)
    driver = SRDriver(sr, gt, s)
    driver.run(['NUM', '*', 'NUM'])

    STMT_LIST = Rule('STMT_LIST')
    IF = Rule('IF')

    STMT_LIST.prod(IF, STMT_LIST)
    STMT_LIST.prod(IF)

    IF.prod('IF', 'PRIM', IF)
    IF.prod('IF', 'PRIM', IF, 'ELSE', IF)
    IF.prod('PRIM')

    sr, gt, s = STMT_LIST.gen()
    driver = SRDriver(sr, gt, s)
    driver.run(['IF', 'PRIM', 'IF', 'PRIM', 'PRIM', 'ELSE', 'PRIM'])


def main2():
    BASE = Rule('BASE_EXPR')

    MUL = Rule('MUL_EXPR')
    MUL.prod(BASE, 'MUL', MUL)
    MUL.prod(BASE, 'DIV', MUL)
    MUL.prod(BASE, 'MOD', MUL)
    MUL.prod(BASE)

    ADD = Rule('ADD_EXPR')
    ADD.prod(MUL, 'PLUS', ADD)
    ADD.prod(MUL, 'NEG', ADD)
    ADD.prod(MUL)

    BASE.prod('LPAREN', ADD, 'RPAREN')
    BASE.prod('NUM')
    BASE.prod('PLUS', BASE)
    BASE.prod('NEG', BASE)

    EXPR = Rule('EXPR')
    EXPR.prod(ADD)

    sr, gt, s = EXPR.gen()
    driver = SRDriver(sr, gt, s)
    #driver.run(['NUM', 'PLUS', 'NUM'])
    driver.genc()


if __name__ == '__main__':
    main2()

# TODO: rule precedence, proofs, grammar parsing, translate into scheme
