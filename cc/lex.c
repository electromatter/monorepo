syscall(n: int, a1: int, a2: int, a3: int, a4: int, a5: int, a6: int): int;

read(fd: int, buf: *byte, n: int): int {
	return syscall(0, fd, buf: int, n, 0, 0, 0);
}

write(fd: int, buf: *byte, n: int): int {
	return syscall(1, fd, buf: int, n, 0, 0, 0);
}

getchar(): int {
	var b: byte;
	var ret: int;
	ret = read(0, &b, 1);
	if (ret < 0) {
		exit(3);
	}
	if (ret == 0) {
		return -1;
	}
	return b: int;
}

putchar(ch: int): void {
	var b: byte;
	var ret: int;
	b = ch: byte;
	ret = write(1, &b, 1);
	if (ret != 1) {
		exit(3);
	}
}

exit(n: int): void {
	syscall(60, n, 0, 0, 0, 0, 0);
}

strcmp(a: *byte, b: *byte): int {
	var i: int;

	i = 0;

	loop {
		if (a[i] > b[i]) {
			return 1;
		}

		if (a[i] < b[i]) {
			return -1;
		}

		if (a[i] == 0:byte) {
			return 0;
		}

		i = i + 1;
	}
}

fdputc(fd: int, ch: int): void {
	var b: byte;
	var ret: int;
	b = ch: byte;
	ret = write(fd, &b, 1);
	if (ret != 1) {
		exit(3);
	}
}


fdputd(fd: int, n: int): void {
	var a: int;

	if (n < 0) {
		fdputc(fd, '-');
		a = -(n % 10);
		n = n / -10;
	} else {
		a = n % 10;
		n = n / 10;
	}

	if (n != 0) {
		fdputd(fd, n);
	}

	fdputc(fd, '0' + a);
}

fdput(fd: int, msg: *byte): void {
	var len: int;
	var ret: int;
	var off: int;
	len = strlen(msg);
	off = 0;
	loop {
		if (off == len) {
			break;
		}
		ret = write(fd, msg, len - off);
		if (ret < 0) {
			exit(3);
		}
		off = off + ret;
	}
}

die(msg: *byte): void {
	fdput(2, "die: ");
	fdput(2, msg);
	fdput(2, "\n");
	exit(1);
}

strlen(s: *byte): int {
	var ret: int;
	ret = 0;
	loop {
		if (s[ret] == 0:byte) {
			return ret;
		}
		ret = ret + 1;
	}
}

mmap(addr: int, len: int, prot: int, flags: int, fd: int, off: int): int {
	return syscall(9, addr, len, prot, flags, fd, off);
}

struct page {
	ptr: *byte;
	fill: int;
	size: int;
}

struct allocator {
	page: *page;
}

setup_alloc(a: *allocator): void {
	a.page = 0:*page;
}

alloc(a: *allocator, size: int): *byte {
	var page: *page;
	var mret: int;
	var ret: *byte;
	var psize: int;

	if (size < 0) {
		die("invalid alloc");
	}

	if (size >= 2048) {
		size = size + 4095;
		size = size & ~4095;
		mret = mmap(0, size, 3, 0x22, -1, 0);
		if (mret == -1) {
			die("out of memory");
		}
		ret = mret: *byte;
		return ret;
	}

	page = a.page;
	if (page) {
		if (size <= page.size - page.fill) {
			mret = page.ptr:int + page.fill;
			page.fill = page.fill + size;
			ret = mret: *byte;
			return ret;
		}
	}

	psize = 64 * 1024;

	mret = mmap(0, psize, 3, 0x22, -1, 0);
	if (mret == -1) {
		die("out of memory");
	}

	page = mret: *page;
	page.ptr = (&page[1]): *byte;
	ret = page.ptr;
	page.size = psize - sizeof(*page);
	page.fill = size;

	a.page = page;

	return ret;
}

_start(): void {
	main();
	exit(0);
}

struct compiler {
	a: allocator;
	nc: int;
	lineno: int;
	colno: int;
	tt: int;
	n: *nfa;
	buf: *byte;
	tlen: int;
	tmax: int;
	tags: *tag;
	ntags: int;
	nnfa: int;
	ndfa: int;
	d: *dfa;
}

setup(c: *compiler): void {
	setup_alloc(&c.a);
	c.nc = getchar();
	c.lineno = 1;
	c.colno = 1;
	c.tt = 0;
	c.n = 0: *nfa;
	c.tmax = 256;
	c.tlen = 0;
	c.buf = alloc(&c.a, c.tmax);
	c.tags = 0: *tag;
	c.ntags = 0;
	c.nnfa = 0;
	c.ndfa = 0;
	c.d = 0:*dfa;
	feed(c);
}

feedc(c: *compiler): void {
	c.nc = getchar();
	if (c.nc == '\n') {
		c.lineno = c.lineno + 1;
		c.colno = 0;
	}
	c.colno = c.colno +1;
}

enum {
	T_EOF,
	T_IDENT,
	T_LITERAL,
	T_CHARSET,
	T_DOT,
	T_STAR,
	T_PLUS,
	T_QMARK,
	T_LPAR,
	T_RPAR,
	T_ALT,
	T_EQUALS,
	T_SEMI,
}

feed(c: *compiler): void {
	c.n = 0:*nfa;
	c.tlen = 0;

	loop {
		if (c.nc == -1) {
			c.tt = T_EOF;
			return;
		} else if (c.nc == ' ' || c.nc == '\t' || c.nc == '\r' || c.nc == '\n') {
			feedc(c);
		} else if (c.nc == '/') {
			feedc(c);
			if (c.nc == '/') {
				loop {
					if (c.nc == '\n' || c.nc == -1) {
						break;
					}
					feedc(c);
				}
			} else {
				die("slash but a comment");
			}
		} else {
			break;
		}
	}

	if ((c.nc >= 'a' && c.nc <= 'z') || (c.nc >= 'A' && c.nc <= 'Z') || c.nc == '_') {
		feed_ident(c);
	} else if (c.nc == '"') {
		feed_literal(c);
	} else if (c.nc == '[') {
		feed_charset(c);
	} else if (c.nc == '.') {
		c.tt = T_DOT;
		feedc(c);
	} else if (c.nc == '*') {
		c.tt = T_STAR;
		feedc(c);
	} else if (c.nc == '+') {
		c.tt = T_PLUS;
		feedc(c);
	} else if (c.nc == '?') {
		c.tt = T_QMARK;
		feedc(c);
	} else if (c.nc == '(') {
		c.tt = T_LPAR;
		feedc(c);
	} else if (c.nc == ')') {
		c.tt = T_RPAR;
		feedc(c);
	} else if (c.nc == '|') {
		c.tt = T_ALT;
		feedc(c);
	} else if (c.nc == '=') {
		c.tt = T_EQUALS;
		feedc(c);
	} else if (c.nc == ';') {
		c.tt = T_SEMI;
		feedc(c);
	} else {
		die("invalid char");
	}
}

feed_ident(c: *compiler): void {
	c.tt = T_IDENT;
	loop {
		if (!((c.nc >= 'a' && c.nc <= 'z') ||
			(c.nc >= 'A' && c.nc <= 'Z') ||
			(c.nc >= '0' && c.nc <= '9') ||
			c.nc == '_')) {
			break;
		}

		c.buf[c.tlen] = c.nc:byte;
		c.tlen = c.tlen + 1;
		if (c.tlen == c.tmax) {
			die("ident too long");
		}
		c.buf[c.tlen] = 0:byte;

		feedc(c);
	}
}

hexdig(c: *compiler): int {
	if (c.nc >= '0' && c.nc <= '9') {
		return c.nc - '0';
	}

	if (c.nc >= 'A' && c.nc <= 'F') {
		return (c.nc - 'A') + 10;
	}

	if (c.nc >= 'a' && c.nc <= 'f') {
		return (c.nc - 'a') + 10;
	}

	die("invalid hex digit");
}

feed_escape(c: *compiler): void {
	var hex: int;

	feedc(c);

	if (c.nc == 't') {
		c.nc = '\t';
	} else if (c.nc == 'r') {
		c.nc = '\r';
	} else if (c.nc == 'n') {
		c.nc = '\n';
	} else if (c.nc == '\\') {
		c.nc = '\\';
	} else if (c.nc == '\'') {
		c.nc = '\'';
	} else if (c.nc == '\"') {
		c.nc = '\"';
	} else if (c.nc == '-') {
		c.nc = '-';
	} else if (c.nc == '[') {
		c.nc = '[';
	} else if (c.nc == ']') {
		c.nc = ']';
	} else if (c.nc == 'x') {
		feedc(c);
		hex = hexdig(c) * 16;
		feedc(c);
		hex = hex + hexdig(c);
		c.nc = hex;
	} else {
		die("invalid escape");
	}
}

feed_literal(c: *compiler): void {
	var a: *nfa;

	c.tt = T_LITERAL;
	c.n = nfa_empty(c);

	feedc(c);

	loop {
		if (c.nc == '"') {
			feedc(c);
			return;
		}

		if (c.nc == -1 || c.nc == 0 || c.nc == '\n') {
			die("invalid char in literal");
		}

		if (c.nc == '\\') {
			feed_escape(c);
		}

		a = nfa_literal(c, c.nc: byte);
		c.n = nfa_concat(c, c.n, a);

		feedc(c);
	}
}

feed_charset(c: *compiler): void {
	var left: int;
	var right: int;
	var mode: int;
	var nonempty: int;
	var i: int;
	var a: *nfa;

	c.tt = T_CHARSET;

	feedc(c);

	mode = 1;
	nonempty = 0;

	i = left;
	loop {
		if (i == 256) {
			break;
		}
		c.buf[i] = 0: byte;
		i = i + 1;
	}

	loop {
		if (c.nc == ']') {
			feedc(c);
			break;
		}

		if (c.nc == -1 || c.nc == 0 || c.nc == '\n' || c.nc == '-') {
			die("invalid char in charset");
		}

		if (c.nc == '^') {
			if (!mode) {
				die("invalid char in charset");
			}

			feedc(c);
			if (mode && !nonempty) {
				i = 0;
				loop {
					if (i == 256) {
						break;
					}
					c.buf[i] = 1:byte;
					i = i + 1;
				}
			}
			mode = 0;
			continue;
		}

		if (c.nc == '\\') {
			feed_escape(c);
			left = c.nc;
			feedc(c);
		} else {
			left = c.nc;
			feedc(c);
		}

		if (c.nc == '-') {
			feedc(c);

			if (c.nc == -1 || c.nc == 0 || c.nc == '\n' || c.nc == '-') {
				die("invalid char in charset");
			}

			if (c.nc == '\\') {
				feed_escape(c);
				right = c.nc + 1;
				feedc(c);
			} else {
				right = c.nc + 1;
				feedc(c);
			}
		} else {
			right = left + 1;
		}

		nonempty = 1;
		i = left;
		loop {
			if (i >= right) {
				break;
			}
			c.buf[i] = mode: byte;
			i = i + 1;
		}
	}

	c.n = nfa_empty(c);
	c.n.left = 256;
	c.n.right = 256;

	i = 0;
	loop {
		loop {
			if (i == 256) {
				left = 256;
				break;
			}

			if (c.buf[i]) {
				left = i;
				break;
			}

			i = i + 1;
		}

		loop {
			if (i == 256) {
				right = 256;
				break;
			}

			if (!c.buf[i]) {
				right = i;
				break;
			}

			i = i + 1;
		}

		if (left >= right) {
			break;
		}

		a = nfa_empty(c);
		a.left = left;
		a.right = right;

		c.n = nfa_alt(c, c.n, a);
	}
}

parse_ident(c: *compiler): *tag {
	var t: *tag;
	if (c.tt != T_IDENT) {
		return 0: *tag;
	}
	t = find_tag(c, c.buf);
	feed(c);
	return t;
}

struct tag {
	next: *tag;
	s: *byte;
	id: int;
}

intern(c: *compiler): *byte {
	var s: *byte;
	var i: int;
	s = alloc(&c.a, c.tlen + 1);
	i = 0;
	loop {
		if (i == c.tlen) {
			break;
		}
		s[i] = c.buf[i];
		i = i + 1;
	}
	s[c.tlen] = 0:byte;
	return s;
}

find_tag(c: *compiler, s: *byte): *tag {
	var t: *tag;
	t = c.tags;
	loop {
		if (!t) {
			break;
		}
		if (!strcmp(t.s, s)) {
			return t;
		}
		t = t.next;
	}
	t = alloc(&c.a, sizeof(*t)): *tag;
	t.next = c.tags;
	t.s = intern(c);
	t.id = c.ntags;
	c.ntags = c.ntags + 1;
	c.tags = t;
	return t;
}

struct nfa {
	id: int;
	tag: *tag;
	left: int;
	right: int;
	live: int;
	a: *nfa;
	b: *nfa;
	end: *nfa;
}

nfa_empty(c: *compiler): *nfa {
	var n: *nfa;
	n = alloc(&c.a, sizeof(*n)):*nfa;
	n.id = c.nnfa;
	n.left = -1;
	n.right = -1;
	n.live = 0;
	n.a = 0:*nfa;
	n.b = 0:*nfa;
	n.end = n;
	c.nnfa = c.nnfa + 1;
	return n;
}

nfa_literal(c: *compiler, a: byte): *nfa {
	var n: *nfa;
	n = nfa_empty(c);
	n.left = a:int;
	n.right = n.left + 1;
	return n;
}

nfa_alt(c: *compiler, a: *nfa, b: *nfa): *nfa {
	var i: *nfa;
	var o: *nfa;
	i = nfa_empty(c);
	o = nfa_empty(c);
	i.a = a;
	i.b = b;
	i.end = o;
	a.end.a = o;
	b.end.a = o;
	return i;
}

nfa_concat(c: *compiler, a: *nfa, b: *nfa): *nfa {
	a.end.a = b;
	a.end = b.end;
	return a;
}

nfa_plus(c: *compiler, a: *nfa): *nfa {
	var o: *nfa;
	o = nfa_empty(c);
	o.b = a;
	a.end.a = o;
	a.end = o;
	return a;
}

nfa_qmark(c: *compiler, a: *nfa): *nfa {
	var b: *nfa;
	b = nfa_empty(c);
	a = nfa_alt(c, a, b);
	return a;
}

nfa_star(c: *compiler, a: *nfa): *nfa {
	a = nfa_plus(c, a);
	a = nfa_qmark(c, a);
	return a;
}

parse_literal(c: *compiler): *nfa {
	var n: *nfa;
	if (c.tt != T_LITERAL) {
		return 0:*nfa;
	}
	n = c.n;
	feed(c);
	return n;
}

parse_charset(c: *compiler): *nfa {
	var n: *nfa;
	if (c.tt != T_CHARSET) {
		return 0:*nfa;
	}
	n = c.n;
	feed(c);
	return n;
}

parse_dot(c: *compiler): *nfa {
	var n: *nfa;
	if (c.tt != T_DOT) {
		return 0:*nfa;
	}
	feed(c);
	n = nfa_empty(c);
	n.left = 0;
	n.right = 256;
	return n;
}

// primary := literal
//          | charset
//          | dot
//          | '(' regex ')'
parse_primary(c: *compiler): *nfa {
	var n: *nfa;

	n = parse_literal(c);
	if (n) {
		return n;
	}

	n = parse_charset(c);
	if (n) {
		return n;
	}

	n = parse_dot(c);
	if (n) {
		return n;
	}

	if (c.tt == T_LPAR) {
		feed(c);

		n = parse_regex(c);

		if (c.tt != T_RPAR) {
			die("expected )");
		}
		feed(c);

		return n;
	}

	return 0: *nfa;
}

// post := primary
//      | post '*'
//      | post '+'
//      | post '?'
parse_post(c: *compiler): *nfa {
	var n: *nfa;

	n = parse_primary(c);
	if (!n) {
		return 0: *nfa;
	}

	loop {
		if (c.tt == T_STAR) {
			n = nfa_star(c, n);
			feed(c);
		} else if (c.tt == T_PLUS) {
			n = nfa_plus(c, n);
			feed(c);
		} else if (c.tt == T_QMARK) {
			n = nfa_qmark(c, n);
			feed(c);
		} else {
			return n;
		}
	}
}

// concat := post
//         | post concat
parse_concat(c: *compiler): *nfa {
	var n: *nfa;
	var b: *nfa;

	n = parse_post(c);
	if (!n) {
		return 0:*nfa;
	}

	loop {
		b = parse_post(c);
		if (!b) {
			return n;
		}

		n = nfa_concat(c, n, b);
	}
}

// regex := concat
//        | '|' regex
//        | concat '|' regex
parse_regex(c: *compiler): *nfa {
	var n: *nfa;
	var b: *nfa;

	n = parse_concat(c);
	if (!n) {
		n = nfa_empty(c);
	}

	loop {
		if (c.tt != T_ALT) {
			return n;
		}
		feed(c);

		b = parse_concat(c);
		if (!b) {
			b = nfa_empty(c);
		}

		n = nfa_alt(c, n, b);
	}
}

// decl := ident '=' regex ';'
parse_decl(c: *compiler): *nfa {
	var regex: *nfa;
	var t: *tag;
	var n: *nfa;

	t = parse_ident(c);
	if (!t) {
		return 0: *nfa;
	}

	if (c.tt != T_EQUALS) {
		die("expected =");
	}
	feed(c);

	regex = parse_regex(c);

	if (c.tt != T_SEMI) {
		die("expected ;");
	}
	feed(c);

	n = nfa_empty(c);
	n.tag = t;

	return nfa_concat(c, regex, n);
}

// progam := decl
//         | decl program
parse_program(c: *compiler): *nfa {
	var n: *nfa;
	var p: *nfa;

	p = 0: *nfa;
	loop {
		n = parse_decl(c);
		if (!n) {
			if (c.tt != T_EOF) {
				die("expected eof");
			}
			return p;
		}

		if (p) {
			p = nfa_alt(c, p, n);
		} else {
			p = n;
		}
	}
}

struct dfa {
	id: int;
	link: **dfa;
	key: nlist;
	l: *dfa;
	r: *dfa;
	seen: int;
}

struct nlist {
	live: **nfa;
	fill: int;
	cap: int;
	tag: *tag;
}

alloc_nlist(c: *compiler, l: *nlist, cap: int): void {
	l.cap = cap;
	l.live = alloc(&c.a, sizeof(*l.live) * cap):**nfa;
	l.fill = 0;
	l.tag = 0:*tag;
}

activate(l: *nlist, n: *nfa): void {
	if (n.live) {
		return;
	}

	n.live = 1;

	l.live[l.fill] = n;
	l.fill = l.fill + 1;

	if (n.left == -1) {
		if ((!l.tag) || (n.tag && n.tag.id < l.tag.id)) {
			l.tag = n.tag;
		}

		if (n.a) {
			activate(l, n.a);
		}

		if (n.b) {
			activate(l, n.b);
		}
	}
}

nlist_cmp(a: *nlist, b: *nlist): int {
	var i: int;

	i = 0;
	loop {
		if (i == a.fill && i == b.fill) {
			break;
		}

		if (i == a.fill) {
			return -1;
		}

		if (i == b.fill) {
			return 1;
		}

		if (a.live[i].id > b.live[i].id) {
			return 1;
		}

		if (a.live[i].id < b.live[i].id) {
			return -1;
		}

		i = i + 1;
	}

	if (!a.tag && !b.tag) {
		return 0;
	}

	if (!a.tag) {
		return -1;
	}

	if (!b.tag) {
		return 1;
	}

	if (a.tag.id < b.tag.id) {
		return -1;
	}

	if (a.tag.id > b.tag.id) {
		return 1;
	}

	return 0;
}

nlist_sort(l: *nlist): void {
	var i: int;
	var j: int;
	var k: int;
	var tmp: *nfa;

	if (l.fill < 2) {
		return;
	}

	i = l.fill - 1;
	loop {
		if (i == 0) {
			break;
		}
		j = (i - 1) >> 1;
		if (l.live[i].id > l.live[j].id) {
			tmp = l.live[j];
			l.live[j] = l.live[i];
			l.live[i] = tmp;
		}
		i = i - 1;
	}

	i = l.fill - 1;
	loop {
		if (i == 0) {
			break;
		}
		tmp = l.live[i];
		l.live[i] = l.live[0];
		j = 0;
		loop {
			k = (j << 1) + 1;
			if (k >= i) {
				break;
			}
			if (k + 1 < i && l.live[k].id < l.live[k + 1].id) {
				k = k + 1;
			}
			if (tmp.id >= l.live[k].id) {
				break;
			}
			j = k;
		}
		l.live[j] = tmp;
		i = i - 1;
	}
}

alloc_link(c: *compiler): **dfa {
	var link: **dfa;
	var i: int;
	link = alloc(&c.a, sizeof(*link) * 256): **dfa;
	i = 0;
	loop {
		if (i == 256) {
			break;
		}
		link[i] = 0:*dfa;
		i = i + 1;
	}
	return link;
}

nlist_copy(c: *compiler, dest: *nlist, src: *nlist): void {
	var i: int;
	alloc_nlist(c, dest, src.fill);
	dest.fill = src.fill;
	dest.tag = src.tag;
	i = 0;
	loop {
		if (i >= src.fill) {
			break;
		}
		dest.live[i] = src.live[i];
		i = i + 1;
	}
}

nlist2dfa(c: *compiler, l: *nlist): *dfa {
	var link: **dfa;
	var d: *dfa;
	var n: *nfa;
	var t: *tag;
	var dir: int;
	var i: int;
	var j: int;

	if (l.fill == 0 && !l.tag) {
		return 0:*dfa;
	}

	link = &c.d;
	loop {
		d = *link;
		if (!d) {
			break;
		}

		dir = nlist_cmp(l, &d.key);

		if (dir < 0) {
			link = &d.l;
		} else if (dir > 0) {
			link = &d.r;
		} else {
			return d;
		}
	}

	d = alloc(&c.a, sizeof(*d)): *dfa;
	d.id = c.ndfa;
	d.link = alloc_link(c);
	nlist_copy(c, &d.key, l);
	d.l = 0: *dfa;
	d.r = 0: *dfa;
	d.seen = 0;

	c.ndfa = c.ndfa + 1;

	*link = d;

	i = 0;
	loop {
		if (i == 256) {
			break;
		}

		deactivate(l);
		j = 0;
		loop {
			if (j >= d.key.fill) {
				break;
			}

			n = d.key.live[j];
			if (n.left <= i && i < n.right) {
				if (n.a) {
					activate(l, n.a);
				}
				if (n.b) {
					activate(l, n.b);
				}
			}

			j = j + 1;
		}

		d.link[i] = nlist2dfa(c, l);

		i = i + 1;
	}

	return d;
}

deactivate(l: *nlist): void {
	var i: int;
	i = 0;
	loop {
		if (i >= l.fill) {
			l.fill = 0;
			l.tag = 0: *tag;
			break;
		}
		l.live[i].live = 0;
		i = i + 1;
	}
}

powerset(c: *compiler, n: *nfa): *dfa {
	var live: nlist;
	alloc_nlist(c, &live, c.nnfa);
	activate(&live, n);
	return nlist2dfa(c, &live);
}

codegen(c: *compiler, a: *dfa): void {
	var i: int;
	var b: *dfa;
	var lo: int;
	var hi: int;

	if (a.seen) {
		return;
	}
	a.seen = 1;

	fdput(1, ":_");
	fdputd(1, a.id);
	fdput(1, ";\n");

	if (a.key.tag) {
		fdput(1, "\tlexmark(l, T_");
		fdput(1, a.key.tag.s);
		fdput(1, ");\n");
	}

	fdput(1, "\tch = lexfeedc(l);\n");

	i = 0;
	loop {
		loop {
			if (i == 256 || a.link[i]) {
				lo = i;
				break;
			}
			i = i + 1;
		}

		if (lo == 256) {
			break;
		}

		b = a.link[i];

		loop {
			if (i == 256 || a.link[i] != b) {
				hi = i;
				break;
			}
			i = i + 1;
		}

		if (lo != (hi - 1)) {
			fdput(1, "\tif (ch >= ");
			fdputd(1, lo);
			fdput(1, " && ch <= ");
			fdputd(1, hi - 1);
		} else {
			fdput(1, "\tif (ch == ");
			fdputd(1, lo);
		}
		fdput(1, ") { ");
		fdput(1, "goto _");
		fdputd(1, b.id);
		fdput(1, "; }\n");

	}

	fdput(1, "\treturn;\n");

	i = 0;
	loop {
		if (i == 256) {
			break;
		}

		if (a.link[i]) {
			codegen(c, a.link[i]);
		}

		i = i + 1;
	}
}

gen(c: *compiler, a: *dfa): void {
	var t: *tag;
	t = c.tags;
	fdput(1, "enum {\n");
	fdput(1, "\tT_invalid,\n");
	fdput(1, "\tT_eof,\n");
	loop {
		if (!t) {
			break;
		}
		fdput(1, "\tT_");
		fdput(1, t.s);
		fdput(1, ",\n");
		t = t.next;
	}
	fdput(1, "}\n");
	fdput(1, "\n");
	fdput(1, "lexstep(l: *lex_state): void {\n");
	fdput(1, "\tvar ch: int;\n");
	fdput(1, "\tlexmark(l, T_invalid);\n");
	codegen(c, a);
	fdput(1, "}\n");
}

main(): void {
	var c: compiler;
	var n: *nfa;
	var a: *dfa;
	setup(&c);
	n = parse_program(&c);
	a = powerset(&c, n);
	gen(&c, a);
}
