from typing import *
import io
import sys


class DFANode:
    def __init__(self):
        self.links = {}
        self.backlinks = {}
        self.tags = set()

    def link(self, ch, other):
        if self.links.get(ch, other) != other:
            raise ValueError
        self.links[ch] = other
        other.backlinks.setdefault(ch, set()).add(self)

    def _enumerate(self):
        stack = [self]
        seen = set()
        classes = {}
        while stack:
            node = stack.pop()
            if node in seen:
                continue
            seen.add(node)
            tags = frozenset(node.tags)
            classes.setdefault(tags, set()).add(node)
            for peer in node.links.values():
                stack.append(peer)
        return set(frozenset(cls) for cls in classes.values())

    def minimize(self):
        'Preform a Hopcroft minimization of this DFA'
        parts = self._enumerate()
        queue = set(parts)
        while queue:
            state = queue.pop()
            trans = {}
            for node in state:
                for ch, peers in node.backlinks.items():
                    trans.setdefault(ch, set()).update(peers)
            for pred in trans.values():
                for part in list(parts):
                    sect = frozenset(part & pred)
                    rem = frozenset(part - pred)
                    if not sect or not rem:
                        continue
                    parts.discard(part)
                    parts.add(sect)
                    parts.add(rem)
                    queue.discard(part)
                    queue.add(sect)
                    queue.add(rem)
        table = {}
        for part in parts:
            newnode = DFANode()
            for node in part:
                table[node] = newnode
            newnode.tags.update(node.tags)
        stack = [self]
        seen = set()
        while stack:
            node = stack.pop()
            newnode = table[node]
            if newnode in seen:
                continue
            seen.add(newnode)
            for ch, peer in node.links.items():
                newpeer = table[peer]
                newnode.link(ch, newpeer)
                stack.append(peer)
        return table[self]

    def show(self, file=sys.stdout):
        'Render the DFA as a graphviz plot'
        do_return = False
        if file is None:
            file = io.StringIO()
            do_return = True

        file.write('digraph{\n')

        table = {}
        stack = [self]
        while stack:
            node = stack.pop()
            if node in table:
                continue
            num = len(table)
            table[node] = num
            file.write('%s [shape=%s];\n' % (num, 'doublecircle' if node.tags else 'circle'))
            stack.extend(node.links.values())
        stack = [self]
        seen = set()
        while stack:
            node = stack.pop()
            if node in seen:
                continue
            seen.add(node)
            links = {}
            for a, b, peer in node.get_ranges():
                def esc(ch):
                    if ch < 0x20 or ch >= 127:
                        return '\\\\%o' % ch
                    elif ch == ord('"'):
                        return '\\"'
                    elif ch == ord('\\'):
                        return '\\\\\\\\'
                    elif ch == ord('-'):
                        return '\\\\-'
                    return chr(ch)
                if a == b:
                    ch = esc(a)
                else:
                    ch = '%s-%s' % (esc(a), esc(b))
                links.setdefault(peer, '')
                links[peer] += ch
            for peer, label in links.items():
                file.write('%s -> %s [label="%s"];\n' % (table[node], table[peer], label))
                stack.append(peer)
        file.write('}\n')
        if do_return:
            return file.getvalue()

    def get_ranges(self):
        def decompose(seq):
            it = iter(sorted(seq))
            a = b = ord(next(it))
            for ch in it:
                ch = ord(ch)
                if b + 1 < ch:
                    yield a, b
                    a = b = ch
                b = ch
            else:
                yield a, b
        peers = {}
        for ch, peer in self.links.items():
            peers.setdefault(peer, set()).add(ch)
        result = []
        for peer, charset in peers.items():
            for a, b in decompose(charset):
                result.append((a, b, peer))
        result.sort(key=lambda x: x[:2])
        return result

    def genc(self, file=sys.stdout):
        do_return = False
        if file is None:
            file = io.StringIO()
            do_return = True

        states = {}
        stack = [self]
        seen = set()
        while stack:
            node = stack.pop()
            if node in seen:
                continue
            seen.add(node)
            states[node] = len(states)
            for ch, node in sorted(node.links.items()):
                stack.append(node)

        tags = set()
        for node in states:
            if node.tags:
                if len(node.tags) > 1:
                    print('WARNING: ambiguous match', file=sys.stderr)
                tag = next(iter(node.tags))
                tags.add(tag)

        # Write out accept states
        file.write('enum token {\n')
        file.write('TOKEN_ERROR = -2,\n')
        file.write('TOKEN_EOF = -1,\n')
        file.write(',\n'.join(sorted(tags)))
        file.write('\n};\n')

        file.write('const char *strtok(int tok)\n{\nswitch(tok){\n')
        for tag in sorted(tags):
            file.write('case %s:return "%s";\n' % (tag, tag))
        file.write('case TOKEN_ERROR:return "TOKEN_ERROR";\n')
        file.write('case -1:return "EOF";\n')
        file.write('default:return "UNKNOWN";\n}\n}\n')

        # Write out the pattern
        file.write('int dogetch(void);\n')
        file.write('void doungetch(int ch);\n')
        file.write('int match(void)\n{\n')
        if not self.links:
            file.write('return TOKEN_ERROR;\n')
        else:
            file.write('int ch;\n')
            for state in states:
                if not state.links:
                    continue
                file.write('state%d:\n' % states[state])
                file.write('ch=dogetch();\n')
                file.write('if(ch<0){\n')
                def out(state, eof=False):
                    if state.tags:
                        file.write('return %s;\n' % next(iter(state.tags)))
                    else:
                        if eof:
                            file.write('return -1;\n')
                        else:
                            file.write('return TOKEN_ERROR;\n')
                out(state, True)
                file.write('}\n')
                ab = []
                prev = 0
                for a, b, peer in state.get_ranges():
                    if a != prev:
                        ab.append((prev, a - 1, None))
                    ab.append((a, b, peer))
                    prev = b + 1
                else:
                    if prev < 255:
                        ab.append((prev + 1, 255, None))
                def treeify(l, r):
                    '[l, r)'
                    mid = (l + r) // 2
                    if l == mid:
                        peer = ab[l][2]
                        if peer is None:
                            file.write('doungetch(ch);')
                            out(state)
                        else:
                            if not peer.links:
                                out(peer)
                            else:
                                file.write('goto state%d;\n' % states[peer])
                        return
                    ch = ab[mid][0]
                    if ch == ord('\''):
                        val = '\\\''
                    elif ch == ord('\\'):
                        val = '\\\\'
                    elif ch >= 7 and ch <= 13:
                        val = '\\' + 'abtnvfr'[ch - 7]
                    elif ch >= 0x20 and ch < 0x7f:
                        val = chr(ch)
                    else:
                        val = '\\%o' % ch
                    file.write('if(ch<\'%s\'){\n' % val)
                    treeify(l, mid)
                    file.write('}\n')
                    treeify(mid, r)
                treeify(0, len(ab))
        file.write('}\n')

        if do_return:
            return file.getvalue()


class NFANode:
    'A node in the transition graph'

    def __init__(self, other=None):
        self._links = {}
        self.links = set()
        self.tags = set(other.tags if other else ())

    def link(self, ch, other):
        self._links.setdefault(ch, set()).add(other)
        self.links.add((ch, other))


class NFA:
    'A non-deterministic finite automata'

    def __init__(self, other=None):
        self.start = NFANode()
        self.end = {self.start}
        self.out = set()
        self.stack = []
        if other is not None:
            self.concat(other)

    def push(self):
        'Remember an alteration point'
        join = NFANode()
        for end in self.end:
            end.link('', join)
        self.stack.append((join, self.out))
        self.end = {join}
        self.out = set()

    def alt(self):
        'Add a branch to the current alteration point'
        self.out |= self.end
        self.end = {self.stack[-1][0]}

    def pop(self):
        'Join the branches of an alteration'
        _, out = self.stack.pop()
        self.end |= self.out
        self.out = out

    def string(self, val):
        'Match a particular string'
        for ch in val:
            node = NFANode()
            for end in self.end:
                end.link(ch, node)
            self.end = {node}

    def charset(self, chars):
        'Match any character in the given set'
        node = NFANode()
        for end in self.end:
            for ch in set(chars):
                end.link(ch, node)
        self.end = {node}

    def eof(self):
        'Match the end of input'
        node = NFANode()
        for end in self.end:
            end.link(None, node)
        self.end = {node}

    def accept(self, tag):
        'Add an accepting node'
        for end in self.end:
            end.tags.add(tag)

    def concat(self, other):
        'Concatenate other into self'
        outs = other.out | other.end
        for _, out in other.stack:
            outs |= out
        table = {}
        def memo(node):
            new = table.get(node)
            if new is None:
                new = NFANode(node)
                table[node] = new
            return new
        seen = set()
        stack = [other.start]
        while stack:
            node = stack.pop()
            if node in seen:
                continue
            seen.add(node)
            mnode = memo(node)
            for ch, peer in node.links:
                mpeer = memo(peer)
                if node == other.start:
                    for end in self.end:
                        end.link(ch, mpeer)
                else:
                    mnode.link(ch, mpeer)
                stack.append(peer)
        self.end = set(map(memo, outs))

    def repeat(self, a, b=None):
        'Pop the last alteration point repeating it [a, b] times (b=None is infinity)'
        join = self.stack[-1][0]
        outs = self.out | self.end
        if a <= 0:
            self.alt()
        elif a > 1:
            raise NotImplementedError
        self.pop()
        if b is None:
            for out in outs:
                out.link('', join)
        else:
            if b < a or b < 1:
                raise ValueError('Must repeat at least once')
            if b != 1:
                raise NotImplementedError

    def optional(self):
        'repeat(0, 1)'
        self.repeat(0, 1)

    def star(self):
        'repeat(0, None)'
        self.repeat(0)

    def plus(self):
        'repeat(1, None)'
        self.repeat(1)

    def show(self, file=sys.stdout):
        'Render the NFA as a graphviz plot'
        do_return = False
        if file is None:
            file = io.StringIO()
            do_return = True

        file.write('digraph{\n')

        table = {}
        stack = [self.start]
        while stack:
            node = stack.pop()
            if node in table:
                continue
            num = len(table)
            table[node] = num
            file.write('%s [shape=%s];\n' % (num, 'doublecircle' if node.tags else 'circle'))
            for _, peer in node.links:
                stack.append(peer)
        file.write('out;\n')
        outs = self.end | self.out
        for _, out in self.stack:
            outs |= out
        stack = [self.start]
        seen = set()
        while stack:
            node = stack.pop()
            if node in seen:
                continue
            seen.add(node)
            if node in outs:
                file.write('%s -> out;\n' % table[node])
            for ch, peer in node.links:
                file.write('%s -> %s [label="%s"];\n' % (table[node], table[peer], ch.replace('"', '\\"')))
                stack.append(peer)
        file.write('}\n')

        if do_return:
            return file.getvalue()

    def expand(self):
        'Expand the NFA into a DFA'
        active = set()
        trans = set()
        def gather(node):
            stack = [node]
            seen = set()
            while stack:
                node = stack.pop()
                if node in seen:
                    continue
                seen.add(node)
                active.add(node)
                for ch, peer in node.links:
                    if ch == '':
                        stack.append(peer)
                    else:
                        trans.add(ch)
        table = {}
        def ident(tag):
            dfa = table.get(tag)
            if dfa is None:
                dfa = DFANode()
                for node in active:
                    dfa.tags |= node.tags
                table[tag] = dfa
            return dfa
        gather(self.start)
        tag = frozenset(active)
        start = ident(tag)
        stack = [(tag, frozenset(trans))]
        seen = set()
        while stack:
            tag, trans = stack.pop()
            if tag in seen:
                continue
            seen.add(tag)
            dfanode = ident(tag)
            for ch in trans:
                active = set()
                trans = set()
                for node in tag:
                    for peer in node._links.get(ch, ()):
                        gather(peer)
                newtag = frozenset(active)
                dfapeer = ident(newtag)
                dfanode.link(ch, dfapeer)
                stack.append((newtag, frozenset(trans)))
        return start


def main():
    nfa = NFA()

    nfa.push()
    nfa.push()
    nfa.charset('+-')
    nfa.optional()
    nfa.push()
    nfa.push()
    nfa.string('.')
    nfa.push()
    nfa.charset('0123456789')
    nfa.repeat(1)
    nfa.alt()
    nfa.push()
    nfa.charset('0123456789')
    nfa.repeat(1)
    nfa.string('.')
    nfa.push()
    nfa.charset('0123456789')
    nfa.repeat(0)
    nfa.pop()
    nfa.push()
    nfa.charset('eE')
    nfa.push()
    nfa.charset('+-')
    nfa.optional()
    nfa.push()
    nfa.charset('0123456789')
    nfa.repeat(1)
    nfa.optional()
    nfa.alt()
    nfa.push()
    nfa.charset('0123456789')
    nfa.repeat(1)
    nfa.charset('eE')
    nfa.push()
    nfa.charset('+-')
    nfa.optional()
    nfa.push()
    nfa.charset('0123456789')
    nfa.repeat(1)
    nfa.pop()
    nfa.push()
    nfa.charset('lLfF')
    nfa.optional()
    nfa.accept('FLOAT')

    nfa.alt()

    nfa.push()

    nfa.push()
    nfa.charset('0123456')
    nfa.repeat(1)

    nfa.alt()
    nfa.charset('123456789')
    nfa.push()
    nfa.charset('0123456789')
    nfa.repeat(0)

    nfa.alt()
    nfa.string('0')
    nfa.charset('xX')
    nfa.push()
    nfa.charset('0123456789abcdefABCDEF')
    nfa.repeat(1)
    nfa.pop()

    nfa.push()
    nfa.charset('uU')
    nfa.alt()
    nfa.charset('lL')
    nfa.alt()
    nfa.charset('lL')
    nfa.charset('lL')
    nfa.repeat(0)

    nfa.accept('INT')

    nfa.alt()
    nfa.charset('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_')
    nfa.push()
    nfa.charset('abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_0123456789')
    nfa.repeat(0)
    nfa.accept('IDENT')

    nfa.alt()
    nfa.string("'")

    nfa.push()
    nfa.string('\\')
    nfa.push()
    nfa.charset('\'"?\\abfnrtv')
    nfa.alt()
    nfa.charset('01234567')
    nfa.alt()
    nfa.charset('01234567')
    nfa.charset('01234567')
    nfa.alt()
    nfa.charset('01234567')
    nfa.charset('01234567')
    nfa.charset('01234567')
    nfa.alt()
    nfa.string('x')
    nfa.push()
    nfa.charset('0123456789abcdefABCDEF')
    nfa.repeat(1)
    nfa.pop()
    nfa.alt()
    nfa.charset(''.join(x for x in map(chr, range(256)) if x not in '\'\\\n'))
    nfa.pop()

    nfa.string("'")
    nfa.accept('CHAR')

    nfa.alt()
    nfa.string('"')
    nfa.accept('STRING')

    nfa.pop()

    assert not nfa.stack

    dfa = nfa.expand()
    dfa.minimize().genc()


def main2():
    nfa = NFA()
    nfa.push()
    nfa.string('+')
    nfa.accept('PLUS')
    nfa.alt()
    nfa.string('-')
    nfa.accept('NEG')
    nfa.alt()
    nfa.string('/')
    nfa.accept('DIV')
    nfa.alt()
    nfa.string('%')
    nfa.accept('MOD')
    nfa.alt()
    nfa.string('*')
    nfa.accept('MUL')
    nfa.alt()
    nfa.string('(')
    nfa.accept('LPAREN')
    nfa.alt()
    nfa.string(')')
    nfa.accept('RPAREN')
    nfa.alt()
    nfa.push()
    nfa.charset('0123456789')
    nfa.repeat(1)
    nfa.accept('NUM')
    nfa.pop()
    assert not nfa.stack
    nfa.expand().minimize().genc()


if __name__ == '__main__':
    main2()

#TODO: proofs of correctness, finite repeat, translate into scheme, regex parsing, submatch boundaries, state caching
