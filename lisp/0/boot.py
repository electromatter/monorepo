import sys


# This is a very poor half implementation of common lisp.
#
# The following defects are known:
#  - There is no character type
#  - macros are not evaluated at the correct time which leads to macros being
#    expanded in improper contexts
#  - Cons cells are immutable
#  - There are almost no error messages
#  - It is very inefficient
#  - This code is messy


class ClosingParenError(Exception):
    pass


class Symbol:
    def __init__(self, sym):
        if not isinstance(sym, str):
            raise TypeError('sym must be a string')
        self.sym = sym

    def __repr__(self):
        return self.sym

    def __hash__(self):
        return hash((type(self), self.sym))

    def __eq__(self, other):
        if not isinstance(other, Symbol):
            return False
        return self.sym == other.sym


# Bare bones reader
class Reader:
    def __init__(self, file=None, interned=None):
        if file is None:
            file = sys.stdin
        self.file = file
        self._ch = ''
        self.interned = interned or {}

    def intern(self, name):
        if not isinstance(name, str):
            raise TypeError
        try:
            return self.interned[name]
        except KeyError:
            pass
        return self.interned.setdefault(name, Symbol(name))

    @property
    def ch(self):
        if not self._ch:
            self._ch = self.file.read(1)
        return self._ch

    def consume(self, charset=''):
        ch = self.ch
        if not charset or ch in charset:
            self._ch = ''
            return ch
        return ''

    def read(self):
        while True:
            if not self.ch:
                raise EOFError

            if self.consume(' \t\n\f\r'):
                continue

            if self.consume(';'):
                while self.consume() not in '\r\n':
                    pass
                continue

            if self.consume(')'):
                raise ClosingParenError

            if self.consume('('):
                value = []
                while True:
                    try:
                        value.append(self.read())
                    except ClosingParenError:
                        break
                    except EOFError:
                        raise TypeError
                result = None
                while value:
                    result = (value.pop(), result)
                return result

            if self.consume("'"):
                try:
                    return (self.intern('quote'), (self.read(), None))
                except EOFError:
                    raise TypeError

            if self.consume("`"):
                try:
                    return (self.intern('quasiquote'), (self.read(), None))
                except EOFError:
                    raise TypeError

            if self.consume(","):
                try:
                    if self.consume("@"):
                        return (self.intern('unquote-splicing'), (self.read(), None))
                    return (self.intern('unquote'), (self.read(), None))
                except EOFError:
                    raise TypeError

            if self.consume("#"):
                if self.consume("'"):
                    try:
                        return (self.intern('function'), (self.read(), None))
                    except EOFError:
                        raise TypeError
                elif self.consume("\\"):
                    value = self.consume()
                    while self.ch.isalnum() or self.ch in '!@$%^&*-_=+<>?/':
                        value += self.consume()
                    if len(value) == 1:
                        return value
                    value = value.lower()
                    if value == 'space':
                        return ' '
                    elif value == 'tab':
                        return '\t'
                    elif value == 'newline':
                        return '\n'
                    elif value == 'return':
                        return '\r'
                    elif value == 'page':
                        return '\f'
                    else:
                        raise ValueError("invalid char name %r" % value)
                else:
                    raise ValueError("invalid char #")

            if self.consume('"'):
                value = []
                while True:
                    ch = self.consume()
                    if ch == '\\':
                        ch = self.consume()
                    elif ch == '"':
                        break
                    if not ch:
                        raise EOFError
                    value.append(ch)
                return "".join(value)

            if self.ch.isalnum() or self.ch in '!@$%^&*-_=+<>?/':
                value = ''
                while self.ch.isalnum() or self.ch in '!@$%^&*-_=+<>?/':
                    value += self.consume()

                if value.startswith('-') and value[1:].isdigit():
                    return int(value)

                if value.isdigit():
                    return int(value)

                return self.intern(value)

            raise ValueError('invalid char %r' % self.ch)


# The dumbest chained environment
class Environment:
    def __init__(self, parent):
        self.parent = parent
        self.vars = {}

    def get(self, name):
        env = self
        while True:
            try:
                return env.vars[name]
            except KeyError:
                pass
            if env.parent is None:
                raise NameError(name)
            env = env.parent

    def set(self, name, value):
        if not isinstance(name, Symbol):
            raise TypeError
        env = self
        while name not in env.vars:
            if env.parent is None:
                break
            env = env.parent
        env.vars[name] = value


class Function:
    def __init__(self, args, body, fenv, env):
        self.args = args
        self.body = body
        self.fenv = fenv
        self.lexical_env = env

    def _eval(self, evaluator, args):
        body_list = self.body
        val = None
        while body_list is not None:
            val = evaluator._eval(body_list[0])
            body_list = body_list[1]
        return val


class Builtin(Function):
    def __init__(self, args, func):
        super().__init__(args, None, None, None)
        self.func = func

    def _eval(self, evaluator, args):
        return self.func(*args)


class LabelGo(Exception):
    def __init__(self, label):
        self.label = label


# Dumb tree walking evaluator
class Evaluator:
    def __init__(self, reader):
        self.reader = reader
        self.env = (Environment(None), Environment(None), Environment(None))

        # Compiler environment
        self.defbuiltin('set-function', ['name', 'func'], self.function_env.set)
        self.defbuiltin('set-macro', ['name', 'func'], self.macro_env.set)
        self.defbuiltin('apply', ['f', 'a'], self.apply)
        def _die(x):
            raise Exception(x)
        self.defbuiltin('die', ['x'], _die)

        # Primitive equality
        self.defbuiltin('equal', ['x', 'y'], lambda x, y: self.tnil(x == y))

        # Type checks
        self.defbuiltin('stringp', ['x'], lambda x: self.tnil(isinstance(x, str)))
        self.defbuiltin('characterp', ['x'], lambda x: self.tnil(isinstance(x, str) and len(x) == 1))
        self.defbuiltin('integerp', ['x'], lambda x: self.tnil(isinstance(x, int)))
        self.defbuiltin('symbolp', ['x'], lambda x: self.tnil(isinstance(x, Symbol)))
        self.defbuiltin('consp', ['x'], lambda x: self.tnil(isinstance(x, tuple)))

        # Lists
        self.defbuiltin('cons', ['a', 'd'], lambda *args: args)
        self.defbuiltin('car', ['x'], lambda x: x[0])
        self.defbuiltin('cdr', ['x'], lambda x: x[1])

        # IO
        def _readchar():
            ch = self.reader.consume()
            if not ch:
                return ""
            return ch
        self.defbuiltin('write-char', ['x'], lambda x: print(x, end=''))
        self.defbuiltin('read-char0', [], _readchar)

        # Strings
        def _concatenate(args):
            vals = []
            while args is not None:
                car = args[0]
                if not isinstance(car, str):
                    raise TypeError
                vals.append(car)
                args = args[1]
            return "".join(vals)
        self.defbuiltin('concatenate-string', ['&rest', 'args'], _concatenate)
        self.defbuiltin('elt', ['x', 'n'], lambda x, n: x[n])
        self.defbuiltin('length', ['x'], lambda x: len(x))

        # Characters
        self.defbuiltin('char-code', ['x'], ord)
        self.defbuiltin('code-char', ['x'], chr)

        def _reduce(f, cons, initial=0):
            x = initial
            while cons is not None:
                y = cons[0]
                x = f(x, y)
                cons = cons[1]
            return x

        # Integer math
        self.defbuiltin('+', ['&rest', 'args'], lambda args: _reduce(lambda x, y: x + y, args))
        self.defbuiltin('-', ['&rest', 'args'], lambda args: _reduce(lambda x, y: x - y, args))
        self.defbuiltin('*', ['&rest', 'args'], lambda args: _reduce(lambda x, y: x * y, args, 1))
        self.defbuiltin('floor', ['x', 'y'], lambda x, y: x // y)
        self.defbuiltin('mod', ['x', 'y'], lambda x, y: x % y)
        self.defbuiltin('<', ['x', 'y'], lambda x, y: self.tnil(x < y))
        self.defbuiltin('<=', ['x', 'y'], lambda x, y: self.tnil(x <= y))

        # Symbols
        self.defbuiltin('intern', ['x'], self.intern)
        def _string(x):
            if not isinstance(x, Symbol):
                raise TypeError
            return x.sym
        self.defbuiltin('string', ['x'], _string)

    def tnil(self, x):
        if x:
            return self.intern('t')
        return None

    def defbuiltin(self, name, args, impl):
        args = self._list(*map(self.intern, args))
        name = self.intern(name)
        func = Builtin(args, impl)
        self.function_env.set(name, func)

    def _unlist(self, x):
        result = []
        while x is not None:
            result.append(x[0])
            x = x[1]
        return result

    def _list(self, *args):
        args = list(args)
        result = None
        while args:
            result = (args.pop(), result)
        return result

    def intern(self, name):
        return self.reader.intern(name)

    def _mksetenv(n):
        def setter(self, val):
            self.env = tuple(val if i == n else x for i, x in enumerate(self.env))
        return setter
    lexical_env = property(lambda self: self.env[0], _mksetenv(0))
    function_env = property(lambda self: self.env[1], _mksetenv(1))
    macro_env = property(lambda self: self.env[2], _mksetenv(2))
    del _mksetenv

    def write(self, x):
        if x is None:
            print('nil', end='')
            return
        if not isinstance(x, tuple):
            print(repr(x), end='')
            return
        print('(', end='')
        while x is not None:
            car, cdr = x
            self.write(car)
            x = cdr
            if x is not None:
                print(' ', end='')
        print(')', end='')
        return x

    def print(self, x):
        self.write(x)
        print()
        return x

    def apply(self, f, a):
        frame = Environment(f.lexical_env)

        name_list = f.args
        val_list = a
        args = []
        while True:
            if name_list is None and val_list is None:
                break

            if name_list is None:
                raise TypeError('too many arguments expected=%r vals=%r' % (f.args, a))

            name = name_list[0]
            name_list = name_list[1]

            if name == self.intern('&rest'):
                if name_list is None:
                    raise TypeError('&rest at end of lambda list expected=%r vals=%r' % (f.args, a))

                name = name_list[0]
                name_list = name_list[1]

                if name_list is not None:
                    raise TypeError('&rest var must be at end of lambda list expected=%r vals=%r' % (f.args, a))

                frame.vars[name] = val_list
                args.append(val_list)
                break

            if val_list is None:
                raise TypeError('not enough arguments expected=%r vals=%r' % (f.args, a))

            val = val_list[0]
            val_list = val_list[1]

            frame.vars[name] = val
            args.append(val)

        saved = self.env
        self.lexical_env = frame
        self.function_env = f.fenv
        result = f._eval(self, args)
        self.env = saved

        return result

    def macroexpand(self, x):
        while True:
            if not isinstance(x, tuple):
                return x

            car, cdr = x

            try:
                macro = self.macro_env.get(car)
            except NameError:
                return x
            else:
                x = self.apply(macro, cdr)
                continue

    def _macroexpand(self, x):
        x = self.macroexpand(x)

        if not isinstance(x, tuple):
            return x

        # Don't expand quoted
        car, cdr = x
        if car == self.intern('quote'):
            return x
        elif car == self.intern('quasiquote'):
            return x
        else:
            form = [self._macroexpand(car)]

        # Expand sub forms
        while cdr is not None:
            form.append(self._macroexpand(cdr[0]))
            cdr = cdr[1]
        result = None
        while form:
            result = (form.pop(), result)
        return result

    def eval(self, x):
        x = self._macroexpand(x)
        return self._eval(x)

    def _eval(self, x):
        if isinstance(x, Symbol):
            return self.lexical_env.get(x)

        if not isinstance(x, tuple):
            return x

        # This is totally unreadable
        car, cdr = x
        caar, cdar = car if isinstance(car, tuple) else (None, None)
        cadar, cddar = cdar if isinstance(cdar, tuple) else (None, None)
        cadr, cddr = cdr if isinstance(cdr, tuple) else (None, None)
        caadr, cdadr = cadr if isinstance(cadr, tuple) else (None, None)
        cadadr, cddadr = cdadr if isinstance(cdadr, tuple) else (None, None)
        caddr, cdddr = cddr if isinstance(cddr, tuple) else (None, None)
        cadddr, cddddr = cdddr if isinstance(cdddr, tuple) else (None, None)

        if car == self.intern('quote'):
            return cadr
        elif car == self.intern('function'):
            if caadr == self.intern('lambda'):
                return Function(cadadr, cddadr, self.function_env, self.lexical_env)
            if not isinstance(cadr, Symbol):
                raise TypeError('got %r expected function name in %r' % (cadr, x))
            return self.function_env.get(cadr)
        elif car == self.intern('setq'):
            if not isinstance(cadr, Symbol):
                raise TypeError('got %r expected variable name in %r' % (cadr, x))
            val = self._eval(caddr)
            self.lexical_env.set(cadr, val)
            return val
        elif car == self.intern('if'):
            condition = self._eval(cadr)
            if condition is not None:
                return self._eval(caddr)
            else:
                return self._eval(cadddr)
        elif car == self.intern('progn'):
            body_list = cdr
            val = None
            while body_list is not None:
                val = self._eval(body_list[0])
                body_list = body_list[1]
            return val
        elif car == self.intern('labels'):
            saved = self.env
            self.function_env = Environment(self.function_env)
            func_list = cadr
            while func_list is not None:
                func = func_list[0]
                name = func[0]
                args = func[1][0]
                body = func[1][1]
                self.function_env.vars[name] = Function(args, body, self.function_env, self.lexical_env)
                func_list = func_list[1]
            body_list = cddr
            val = None
            while body_list is not None:
                val = self._eval(body_list[0])
                body_list = body_list[1]
            self.env = saved
            return val
        elif car == self.intern('tagbody'):
            # Scan for labels
            labels = {}
            body_list = cdr
            while body_list is not None:
                if isinstance(body_list[0], Symbol):
                    labels[body_list[0]] = body_list[1]
                body_list = body_list[1]
            # Execute the loop
            body_list = cdr
            val = None
            while body_list is not None:
                try:
                    saved = self.env
                    if not isinstance(body_list[0], Symbol):
                        val = self._eval(body_list[0])
                    body_list = body_list[1]
                except LabelGo as go:  # Need to re think this... need a real compiler go labels are lexically scoped. This is dynamically scoped
                    try:
                        body_list = labels[go.label]
                        self.env = saved
                    except KeyError:
                        raise go
            return None
        elif car == self.intern('go'):
            if not isinstance(cadr, Symbol):
                raise TypeError('got %r expected label name in %r' % (cadr, x))
            raise LabelGo(cadr)
        else:
            if not isinstance(car, Symbol):
                if caar != self.intern('lambda'):
                    raise TypeError('expected function name or lambda form in %r' % (x, ))
                func = Function(cadar, cddar, self.function_env, self.lexical_env)
            else:
                func = self.function_env.get(car)
            def eval_args(args):
                if args is None:
                    return None
                return (self._eval(args[0]), eval_args(args[1]))
            return self.apply(func, eval_args(cdr))


def main():
    source_reader = Reader(open(sys.argv[1], 'r'))
    stdin_reader = Reader(interned=source_reader.interned)
    evaluator = Evaluator(stdin_reader)
    while True:
        try:
            read_val = source_reader.read()
        except EOFError:
            break
        evaluator.eval(read_val)


if __name__ == '__main__':
    sys.setrecursionlimit(1000000)
    main()
