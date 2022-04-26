#!/usr/bin/env python3


import sys


STDIN = sys.stdin.buffer
STDOUT = sys.stdout.buffer


def main():
    c = STDIN.read()
    suppress = 1
    env = {}
    while suppress >= 0:
        i = 0
        link = []
        stack = []
        saved = dict(env)
        env["here"] = 0
        env["offset"] = 0
        while i < len(c):
            if c[i] in b" \v\t\r\n\f":
                i += 1
            elif c[i] in b";":
                i += 1
                while i < len(c):
                    if c [i] in b"\n":
                        break
                    i += 1
            elif c[i] in b"[":
                i += 1
                stack.append(i)
                depth = 0
                while i < len(c):
                    if c[i] in b"]":
                        if depth:
                            depth -= 1
                        else:
                            i += 1
                            break
                    elif c[i] in b"[":
                        depth += 1
                    elif c[i] in b"\\":
                        i += 1
                    i += 1
            elif c[i] in b"]":
                i = link.pop()
            elif c[i] in b"0123456789ABCDEFabcdef":
                a = 0
                while i < len(c) and c[i] in b"0123456789ABCDEFabcdef":
                    a = (a * 16 + int(chr(c[i]), 16)) & 0xffffffffffffffff
                    i += 1
                stack.append(a)
            elif c[i] in b"?":
                i += 1
                target = stack.pop()
                if target == 0:
                    lineno = c[:i].count(b'\n') + 1
                    raise ValueError(f'undefined at line {lineno} stack {stack}')
                if stack.pop():
                    link.append(i)
                    i = target
            elif c[i] in b"@":
                i += 1
                a = stack.pop()
                key = []
                depth = 0
                while a < len(c):
                    if c[a] in b"]":
                        if not depth:
                            break
                        else:
                            depth -= 1
                    elif c[a] in b"[":
                        depth += 1
                    elif c[a] in b"\\":
                        a += 1
                    key.append(chr(c[a]))
                    a += 1
                key = "".join(key)
                stack.append(env.get(key, 0))
            elif c[i] in b"!":
                i += 1
                b = stack.pop()
                a = stack.pop()
                key = []
                while b < len(c):
                    if c[b] in b"]":
                        if not depth:
                            break
                        else:
                            depth -= 1
                    elif c[b] in b"[":
                        depth += 1
                    elif c[b] in b"\\":
                        b += 1
                    key.append(chr(c[b]))
                    b += 1
                key = "".join(key)
                env[key] = a
            elif c[i] in b"<":
                i += 1
                b = stack.pop()
                a = stack.pop()
                stack.append(int(a < b))
            elif c[i] in b">":
                i += 1
                b = stack.pop()
                a = stack.pop()
                stack.append(int(a > b))
            elif c[i] in b"=":
                i += 1
                b = stack.pop()
                a = stack.pop()
                stack.append(int(a == b))
            elif c[i] in b"+":
                i += 1
                b = stack.pop()
                a = stack.pop()
                stack.append((a + b) & 0xffffffffffffffff)
            elif c[i] in b"-":
                i += 1
                b = stack.pop()
                a = stack.pop()
                stack.append((a - b) & 0xffffffffffffffff)
            elif c[i] in b"*":
                i += 1
                b = stack.pop()
                a = stack.pop()
                stack.append((a * b) & 0xffffffffffffffff)
            elif c[i] in b"/":
                i += 1
                b = stack.pop()
                a = stack.pop()
                stack.append((a // b) & 0xffffffffffffffff)
            elif c[i] in b"%":
                i += 1
                b = stack.pop()
                a = stack.pop()
                stack.append((a % b) & 0xffffffffffffffff)
            elif c[i] in b"&":
                i += 1
                b = stack.pop()
                a = stack.pop()
                stack.append((a & b) & 0xffffffffffffffff)
            elif c[i] in b"|":
                i += 1
                b = stack.pop()
                a = stack.pop()
                stack.append((a | b) & 0xffffffffffffffff)
            elif c[i] in b"^":
                i += 1
                b = stack.pop()
                a = stack.pop()
                stack.append((a ^ b) & 0xffffffffffffffff)
            elif c[i] in b"l":
                i += 1
                b = stack.pop()
                a = stack.pop()
                stack.append((a << b) & 0xffffffffffffffff)
            elif c[i] in b"r":
                i += 1
                b = stack.pop()
                a = stack.pop()
                stack.append((a >> b) & 0xffffffffffffffff)
            elif c[i] in b"~":
                i += 1
                a = stack.pop()
                stack.append((~a) & 0xffffffffffffffff)
            elif c[i] in b".":
                i += 1
                a = stack.pop()
                if not suppress:
                    STDOUT.write(bytes([
                        a & 0xff,
                    ]))
                env["here"] += 1
                env["offset"] += 1
            elif c[i] in b"h":
                i += 1
                a = stack.pop()
                if not suppress:
                    STDOUT.write(bytes([
                        a & 0xff,
                        (a >> 8) & 0xff,
                    ]))
                env["here"] += 2
                env["offset"] += 2
            elif c[i] in b"i":
                i += 1
                a = stack.pop()
                if not suppress:
                    STDOUT.write(bytes([
                        a & 0xff,
                        (a >> 8) & 0xff,
                        (a >> 16) & 0xff,
                        (a >> 24) & 0xff,
                    ]))
                env["here"] += 4
                env["offset"] += 4
            elif c[i] in b"q":
                i += 1
                a = stack.pop()
                if not suppress:
                    STDOUT.write(bytes([
                        a & 0xff,
                        (a >> 8) & 0xff,
                        (a >> 16) & 0xff,
                        (a >> 24) & 0xff,
                        (a >> 32) & 0xff,
                        (a >> 40) & 0xff,
                        (a >> 48) & 0xff,
                        (a >> 56) & 0xff,
                    ]))
                env["here"] += 8
                env["offset"] += 8
            elif c[i] in b"s":
                i += 1
                j = stack.pop()
                depth = 0
                while j < len(c):
                    if c[j] in b"]":
                        if not depth:
                            break
                        else:
                            depth -= 1
                    elif c[j] in b"[":
                        depth += 1
                    elif c[j] in b"\\":
                        j += 1
                    if not suppress:
                        STDOUT.write(bytes([c[j]]))
                    j += 1
                    env["here"] += 1
                    env["offset"] += 1
            elif c[i] in b"p":
                i += 1
                depth = stack.pop()
                del stack[-depth:]
            elif c[i] in b"o":
                i += 1
                depth = stack.pop()
                stack.append(stack[-depth])
            elif c[i] in b"t":
                i += 1
                depth = stack.pop()
                stack[-depth:-1], stack[-1] = stack[-depth + 1:], stack[-depth]
            elif c[i] in b"x":
                msg = []
                a = stack.pop()
                while a < len(c):
                    if c[a] in b"]":
                        if not depth:
                            break
                        else:
                            depth -= 1
                    elif c[a] in b"[":
                        depth += 1
                    elif c[a] in b"\\":
                        a += 1
                    msg.append(chr(c[a]))
                    a += 1
                msg = "".join(msg)
                lineno = c[:i].count(b'\n') + 1
                raise AssertionError(f'{msg} at line {lineno} stack {stack}')
            elif c[i] in b"`":
                i += 1
                print(stack, file=sys.stderr)
            else:
                raise NotImplementedError(chr(c[i]))
        if not suppress and env != saved:
            raise ValueError(f"{env} != {saved}")
        suppress -= 1
        if stack:
            raise ValueError(f"{stack}")


if __name__ == "__main__":
    main()
