syscall(n: int, a1: int, a2: int, a3: int, a4: int, a5: int, a6: int): int;

struct page {
	ptr: *byte;
	fill: int;
	size: int;
}

struct node {
	kind: int;
	a: *node;
	b: *node;
	lineno: int;
	colno: int;
	n: int;
	s: *byte;
	t: *type;
}

struct type {
	kind: int;
	st: *decl;
	val: *type;
	arg: *type;
}

struct fixup {
	next: *fixup;
	ptr: *byte;
	at: int;
}

struct label {
	fix: *fixup;
	at: int;
	fixed: int;
}

struct chunk {
	next: *chunk;
	buf: *byte;
	fill: int;
	cap: int;
}

struct decl {
	name: *byte;
	member_name: *byte;
	p: *decl;
	l: *decl;
	r: *decl;

	func_defined: int;
	func_type: *type;
	func_label: *label;
	func_def: *node;

	struct_defined: int;
	struct_size: int;
	struct_layout_done: int;
	struct_def: *node;

	member_defined: int;
	member_type: *type;
	member_offset: int;
	member_def: *node;

	enum_defined: int;
	enum_value: int;
	enum_def: *node;

	var_defined: int;
	var_type: *type;
	var_offset: int;
	var_def: *node;
}

struct compiler {
	// Allocator
	page: *page;

	// Lexer
	nc: int;
	lineno: int;
	colno: int;
	tt: int;
	token: *byte;
	tlen: int;
	tmax: int;

	// Assembler
	at: int;
	text: *chunk;
	text_end: *chunk;

	// Namespace
	decls: *decl;
}

enum {
	T_EOF,
	T_IDENT,
	T_NUM,
	T_HEX,
	T_STR,
	T_CHAR,
	T_LPAR,
	T_RPAR,
	T_LBRA,
	T_RBRA,
	T_COMMA,
	T_SEMI,
	T_COLON,
	T_STAR,
	T_DOT,
	T_NOT,
	T_ASSIGN,
	T_AMP,
	T_OR,
	T_XOR,
	T_LT,
	T_GT,
	T_LE,
	T_GE,
	T_EQ,
	T_NE,
	T_ADD,
	T_SUB,
	T_LSH,
	T_RSH,
	T_BANG,
	T_BOR,
	T_BAND,
	T_LSQ,
	T_RSQ,
	T_DIV,
	T_MOD,
}

enum {
	N_IDENT,
	N_NUM,
	N_CHAR,
	N_STR,
	N_STMTLIST,
	N_EXPRLIST,
	N_CALL,
	N_DOT,
	N_ARGLIST,
	N_FUNC,
	N_ARGDECL,
	N_FUNCDECL,
	N_PROGRAM,
	N_FUNCTYPE,
	N_PTRTYPE,
	N_STRUCT,
	N_MEMBERDECL,
	N_MEMBERLIST,
	N_CONDLIST,
	N_COND,
	N_ENUM,
	N_ENUMLIST,
	N_LOOP,
	N_BREAK,
	N_CONTINUE,
	N_RETURN,
	N_VARDECL,
	N_ASSIGN,
	N_SIZEOF,
	N_REF,
	N_DEREF,
	N_CAST,
	N_INDEX,
	N_LT,
	N_GT,
	N_LE,
	N_GE,
	N_EQ,
	N_NE,
	N_ADD,
	N_SUB,
	N_MUL,
	N_LSH,
	N_RSH,
	N_BNOT,
	N_BOR,
	N_BAND,
	N_AND,
	N_OR,
	N_XOR,
	N_NOT,
	N_POS,
	N_NEG,
	N_DIV,
	N_MOD,
}

enum {
	TY_VOID,
	TY_INT,
	TY_BYTE,
	TY_PTR,
	TY_ARG,
	TY_FUNC,
	TY_STRUCT,
}

exit(n: int): void {
	syscall(60, n, 0, 0, 0, 0, 0);
}

_start(): void {
	main();
	exit(0);
}

read(fd: int, buf: *byte, n: int): int {
	return syscall(0, fd, buf: int, n, 0, 0, 0);
}

write(fd: int, buf: *byte, n: int): int {
	return syscall(1, fd, buf: int, n, 0, 0, 0);
}

mmap(addr: int, len: int, prot: int, flags: int, fd: int, off: int): int {
	return syscall(9, addr, len, prot, flags, fd, off);
}

alloc(c: *compiler, size: int): *byte {
	var page: *page;
	var mret: int;
	var ret: *byte;
	var psize: int;

	if (size < 0) {
		die(c, "invalid alloc");
	}

	if (size >= 2048) {
		size = size + 4095;
		size = size & ~4095;
		mret = mmap(0, size, 3, 0x22, -1, 0);
		if (mret == -1) {
			die(c, "out of memory");
		}
		ret = mret: *byte;
		return ret;
	}

	page = c.page;
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
		die(c, "out of memory");
	}

	page = mret: *page;
	page.ptr = (&page[1]): *byte;
	ret = page.ptr;
	page.size = psize - sizeof(*page);
	page.fill = size;

	c.page = page;

	return ret;
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

fdputh(fd: int, n: int): void {
	var c: int;
	var r: int;

	r = 0;
	loop {
		if (n == 0) {
			break;
		}

		r = (r << 4) + (n & 15);
		n = n >> 4;
	}

	n = r;

	loop {
		c = n & 15;
		n = n >> 4;

		if (c < 10) {
			fdputc(fd, c + '0');
		} else {
			fdputc(fd, c + ('a' - 10));
		}

		if (n == 0) {
			break;
		}
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

show_context(c: *compiler): void {
	fdput(2, "on -:");
	fdputd(2, c.lineno);
	fdput(2, ":");
	fdputd(2, c.colno);
	fdput(2, "\n");
}

die(c: *compiler, msg: *byte): void {
	show_context(c);
	fdput(2, "die: ");
	fdput(2, msg);
	fdput(2, "\n");
	exit(1);
}

comp_setup(c: *compiler): void {
	c.page = 0:*page;

	c.nc = getchar();
	c.lineno = 1;
	c.colno = 1;
	c.tlen = 0;
	c.tmax = 4096;
	c.token = alloc(c, c.tmax);
	c.tt = 0;

	c.at = 0;
	c.text = 0:*chunk;
	c.text_end = 0:*chunk;

	c.decls = 0:*decl;

	feed(c);
}

feedc(c: *compiler): void {
	c.nc = getchar();
	if (c.nc == '\n') {
		c.lineno = c.lineno + 1;
		c.colno = 0;
	}
	c.colno = c.colno + 1;
}

tappend(c: *compiler): void {
	c.token[c.tlen] = c.nc:byte;
	c.tlen = c.tlen + 1;
	if (c.tlen == c.tmax) {
		die(c, "identifier too long");
	}
	c.token[c.tlen] = 0:byte;
	feedc(c);
}

feed(c: *compiler): void {
	c.tlen = 0;
	c.token[0] = 0:byte;

	loop {
		if (c.nc == -1) {
			// Reached the end of input
			c.tt = T_EOF;
			return;
		} else if (c.nc == ' ' || c.nc == '\t' || c.nc == '\r') {
			// Whitespace
			feedc(c);
		} else if (c.nc == '\n') {
			// Line end
			feedc(c);
		} else if (c.nc == '/') {
			// Comment
			feedc(c);
			if (c.nc == '/') {
				// Read until the end of the comment
				loop {
					if (c.nc == '\n' || c.nc == -1) {
						break;
					}
					feedc(c);
				}
			} else {
				c.tt = T_DIV;
				return;
			}

		} else {
			// Start of a real token
			break;
		}
	}

	// Identifier
	if ((c.nc >= 'a' && c.nc <= 'z') || (c.nc >= 'A' && c.nc <= 'Z') || c.nc == '_') {
		feed_ident(c);
		return;
	}

	// String
	if (c.nc == '"') {
		feed_str(c);
		return;
	}

	// Character
	if (c.nc == '\'') {
		feed_char(c);
		return;
	}

	// Number
	if (c.nc >= '0' && c.nc <= '9') {
		feed_num(c);
		return;
	}

	// Single character tokens
	if (c.nc == '(') {
		c.tt = T_LPAR;
		feedc(c);
	} else if (c.nc == ')') {
		c.tt = T_RPAR;
		feedc(c);
	} else if (c.nc == '{') {
		c.tt = T_LBRA;
		feedc(c);
	} else if (c.nc == '}') {
		c.tt = T_RBRA;
		feedc(c);
	} else if (c.nc == ',') {
		c.tt = T_COMMA;
		feedc(c);
	} else if (c.nc == ';') {
		c.tt = T_SEMI;
		feedc(c);
	} else if (c.nc == ':') {
		c.tt = T_COLON;
		feedc(c);
	} else if (c.nc == '*') {
		c.tt = T_STAR;
		feedc(c);
	} else if (c.nc == '.') {
		c.tt = T_DOT;
		feedc(c);
	} else if (c.nc == '=') {
		c.tt = T_ASSIGN;
		feedc(c);
		if (c.nc == '=') {
			c.tt = T_EQ;
			feedc(c);
		}
	} else if (c.nc == '&') {
		c.tt = T_AMP;
		feedc(c);
		if (c.nc == '&') {
			c.tt = T_BAND;
			feedc(c);
		}
	} else if (c.nc == '~') {
		c.tt = T_NOT;
		feedc(c);
	} else if (c.nc == '|') {
		c.tt = T_OR;
		feedc(c);
		if (c.nc == '|') {
			c.tt = T_BOR;
			feedc(c);
		}
	} else if (c.nc == '^') {
		c.tt = T_XOR;
		feedc(c);
	} else if (c.nc == '!') {
		c.tt = T_BANG;
		feedc(c);
		if (c.nc == '=') {
			c.tt = T_NE;
			feedc(c);
		}
	} else if (c.nc == '<') {
		c.tt = T_LT;
		feedc(c);
		if (c.nc == '<') {
			c.tt = T_LSH;
			feedc(c);
		} else if (c.nc == '=') {
			c.tt = T_LE;
			feedc(c);
		}
	} else if (c.nc == '>') {
		c.tt = T_GT;
		feedc(c);
		if (c.nc == '>') {
			c.tt = T_RSH;
			feedc(c);
		} else if (c.nc == '=') {
			c.tt = T_GE;
			feedc(c);
		}
	} else if (c.nc == '[') {
		c.tt = T_LSQ;
		feedc(c);
	} else if (c.nc == ']') {
		c.tt = T_RSQ;
		feedc(c);
	} else if (c.nc == '+') {
		c.tt = T_ADD;
		feedc(c);
	} else if (c.nc == '-') {
		c.tt = T_SUB;
		feedc(c);
	} else if (c.nc == '%') {
		c.tt = T_MOD;
		feedc(c);
	} else {
		die(c, "invalid char");
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
		tappend(c);
	}
}

hexdig(c: *compiler): int {
	if (c.nc >= '0' && c.nc <= '9') {
		return c.nc - '0';
	}

	if (c.nc >= 'A' && c.nc <= 'F') {
		return (c.nc - 'F') + 10;
	}

	if (c.nc >= 'a' && c.nc <= 'f') {
		return (c.nc - 'a') + 10;
	}

	die(c, "invalid hex digit");
	return 0;
}

feed_escape(c: *compiler): void {
	var hex: int;

	// backslash
	feedc(c);

	if (c.nc == 't') {
		c.nc = '\t';
	} else if (c.nc == 'r') {
		c.nc = '\r';
	} else if (c.nc == 'n') {
		c.nc = '\n';
	} else if (c.nc == 'x') {
		c.nc = getchar();
		hex = hexdig(c) * 16;

		c.nc = getchar();
		hex = hex + hexdig(c);

		c.nc = hex;
	} else if (c.nc != '\\' && c.nc != '\'' && c.nc != '"') {
		die(c, "invalid escape");
	}
}

feed_str(c: *compiler): void {
	c.tt = T_STR;

	// quote
	feedc(c);

	loop {
		if (c.nc == '"') {
			feedc(c);
			return;
		}

		if (c.nc == -1 || c.nc == 0 || c.nc == '\n') {
			die(c, "invalid char in string");
		}

		if (c.tlen == c.tmax) {
			die(c, "string too long");
		}

		if (c.nc == '\\') {
			feed_escape(c);
		}

		tappend(c);
	}
}

feed_char(c: *compiler): void {
	c.tt = T_CHAR;

	// quote
	feedc(c);

	if (c.nc == 0 || c.nc == -1 || c.nc == '\'' || c.nc == '\n') {
		die(c, "invalid char");
	}

	if (c.nc == '\\') {
		feed_escape(c);
	}

	tappend(c);

	if (c.nc != '\'') {
		die(c, "expected '");
	}

	feedc(c);
}

feed_hex(c: *compiler): void {
	c.tt = T_HEX;

	loop {
		if (!((c.nc >= '0' && c.nc <= '9')
				|| (c.nc >= 'a' && c.nc <= 'f')
				|| (c.nc >= 'A' && c.nc <= 'F'))) {
			break;
		}

		tappend(c);
	}

	if (c.tlen == 0) {
		die(c, "expected hex");
	}
}

feed_num(c: *compiler): void {
	c.tt = T_NUM;

	if (c.nc == '0') {
		tappend(c);

		if (c.nc == 'x') {
			feedc(c);
			feed_hex(c);
			return;
		}
	}

	loop {
		if (!(c.nc >= '0' && c.nc <= '9')) {
			break;
		}

		tappend(c);
	}
}

mknode(c: *compiler, kind: int, a: *node, b: *node): *node {
	var ret: *node;
	ret = alloc(c, sizeof(*ret)):*node;
	ret.kind = kind;
	ret.a = a;
	ret.b = b;
	ret.lineno = c.lineno;
	ret.colno = c.colno;
	ret.n = 0;
	ret.s = 0:*byte;
	ret.t = 0:*type;
	return ret;
}

mknode0(c: *compiler, kind: int): *node {
	return mknode(c, kind, 0:*node, 0:*node);
}

mknode1(c: *compiler, kind: int, a: *node): *node {
	return mknode(c, kind, a, 0:*node);
}

// Copy the current token
intern(c: *compiler): *byte {
	var ret: *byte;
	var i: int;

	ret = alloc(c, c.tlen + 1);

	i = 0;
	loop {
		if (i == c.tlen) {
			ret[i] = 0:byte;
			return ret;
		}

		ret[i] = c.token[i];

		i = i + 1;
	}
}

// ident := IDENT
parse_ident(c: *compiler): *node {
	var n: *node;

	if (c.tt != T_IDENT) {
		return 0:*node;
	}

	n = mknode0(c, N_IDENT);
	n.s = intern(c);
	feed(c);

	return n;
}

parse_hex(c: *compiler): *node {
	var n: *node;
	var x: int;
	var d: int;
	var i: int;

	x = 0;
	i = 0;
	loop {
		if (i == c.tlen) {
			break;
		}

		d = c.token[i]:int;

		if (d >= '0' && d <= '9') {
			d = d - '0';
		} else if (d >= 'a' && d <= 'f') {
			d = (d - 'a') + 10;
		} else {
			d = (d - 'A') + 10;
		}

		x = x * 16;
		x = x + d;
		i = i + 1;

		if (x > 0x7fffffff) {
			die(c, "overflow");
		}
	}

	n = mknode0(c, N_NUM);
	n.n = x;
	feed(c);

	return n;
}

// num := NUM
parse_num(c: *compiler): *node {
	var n: *node;
	var x: int;
	var d: int;
	var i: int;

	if (c.tt == T_HEX) {
		return parse_hex(c);
	}

	if (c.tt != T_NUM) {
		return 0:*node;
	}

	x = 0;
	i = 0;
	loop {
		if (i == c.tlen) {
			break;
		}

		d = (c.token[i]:int) - '0';

		x = x * 10;

		x = x + d;
		i = i + 1;

		if (x > 0x7fffffff) {
			die(c, "overflow");
		}
	}

	n = mknode0(c, N_NUM);
	n.n = x;
	feed(c);

	return n;
}

// chr := CHAR
parse_chr(c: *compiler): *node {
	var n: *node;

	if (c.tt != T_CHAR) {
		return 0:*node;
	}

	n = mknode0(c, N_CHAR);
	n.n = c.token[0]:int;
	feed(c);

	return n;
}

// str := STR
parse_str(c: *compiler): *node {
	var n: *node;

	if (c.tt != T_STR) {
		return 0:*node;
	}

	n = mknode0(c, N_STR);
	n.s = intern(c);
	feed(c);

	return n;
}

// primary := ident
//          | num
//          | str
//          | chr
//          | '(' expr ')'
parse_primary(c: *compiler): *node {
	var n: *node;

	n = parse_ident(c);
	if (n) {
		return n;
	}

	n = parse_num(c);
	if (n) {
		return n;
	}

	n = parse_str(c);
	if (n) {
		return n;
	}

	n = parse_chr(c);
	if (n) {
		return n;
	}

	if (c.tt == T_LPAR) {
		feed(c);

		n = parse_expr(c);
		if (!n) {
			die(c, "expected expr");
		}

		if (c.tt != T_RPAR) {
			die(c, "expected )");
		}
		feed(c);

		return n;
	}

	return 0:*node;
}

// expr_list := expr
//            | expr ',' expr_list
parse_expr_list(c: *compiler): *node {
	var n: *node;
	var e: *node;
	var a: *node;

	a = parse_expr(c);
	if (!a) {
		return 0:*node;
	}

	e = mknode1(c, N_EXPRLIST, a);
	n = e;

	loop {
		if (c.tt != T_COMMA) {
			return n;
		}
		feed(c);

		a = parse_expr(c);
		if (!a) {
			die(c, "expected expr");
		}

		e.b = mknode1(c, N_EXPRLIST, a);
		e = e.b;
	}
}

// post_expr := primary
//            | 'sizeof' '(' expr ')'
//            | post_expr '[' expr ']'
//            | post_expr '(' expr_list ')'
//            | post_expr '.' ident
//            | post_expr ':' type
parse_post_expr(c: *compiler): *node {
	var n: *node;
	var b: *node;

	if (c.tt == T_IDENT && !strcmp(c.token, "sizeof")) {
		feed(c);

		if (c.tt != T_LPAR) {
			die(c, "expected (");
		}
		feed(c);

		n = parse_expr(c);
		if (!n) {
			die(c, "expected expr");
		}

		if (c.tt != T_RPAR) {
			die(c, "expected )");
		}
		feed(c);

		return mknode1(c, N_SIZEOF, n);
	}

	n = parse_primary(c);
	if (!n) {
		return 0:*node;
	}

	loop {
		if (c.tt == T_LSQ) {
			feed(c);

			b = parse_expr(c);

			if (c.tt != T_RSQ) {
				die(c, "expected ]");
			}
			feed(c);

			n = mknode(c, N_INDEX, n, b);
		} else if (c.tt == T_LPAR) {
			feed(c);

			b = parse_expr_list(c);

			if (c.tt != T_RPAR) {
				die(c, "expected )");
			}
			feed(c);

			n  = mknode(c, N_CALL, n, b);
		} else if (c.tt == T_DOT) {
			feed(c);

			b = parse_ident(c);
			if (!b) {
				die(c, "expected ident");
			}

			n = mknode(c, N_DOT, n, b);
		} else if (c.tt == T_COLON) {
			feed(c);

			b = parse_type(c);
			if (!b) {
				die(c, "expected type");
			}

			n = mknode(c, N_CAST, n, b);
		} else {
			return n;
		}
	}
}

// unary_expr := post_expr
//             | '&' unary_expr
//             | '*' unary_expr
//             | '+' unary_expr
//             | '-' unary_expr
//             | '~' unary_expr
//             | '!' unary_expr
parse_unary_expr(c: *compiler): *node {
	var n: *node;

	if (c.tt == T_AMP) {
		feed(c);

		n = parse_unary_expr(c);
		if (!n) {
			die(c, "expected unary_expr");
		}

		return mknode1(c, N_REF, n);
	}

	if (c.tt == T_STAR) {
		feed(c);

		n = parse_unary_expr(c);
		if (!n) {
			die(c, "expected unary_expr");
		}

		return mknode1(c, N_DEREF, n);
	}

	if (c.tt == T_ADD) {
		feed(c);

		n = parse_unary_expr(c);
		if (!n) {
			die(c, "expected unary_expr");
		}

		return mknode1(c, N_POS, n);
	}

	if (c.tt == T_SUB) {
		feed(c);

		n = parse_unary_expr(c);
		if (!n) {
			die(c, "expected unary_expr");
		}

		return mknode1(c, N_NEG, n);
	}

	if (c.tt == T_NOT) {
		feed(c);

		n = parse_unary_expr(c);
		if (!n) {
			die(c, "expected unary_expr");
		}

		return mknode1(c, N_NOT, n);
	}

	if (c.tt == T_BANG) {
		feed(c);

		n = parse_unary_expr(c);
		if (!n) {
			die(c, "expected unary_expr");
		}

		return mknode1(c, N_BNOT, n);
	}

	return parse_post_expr(c);
}


// shift_expr := unary_expr
//             | unary_expr '<<' shift_expr
//             | unary_expr '>>' shift_expr
parse_shift_expr(c: *compiler): *node {
	var a: *node;
	var b: *node;

	a = parse_unary_expr(c);
	if (!a) {
		return 0:*node;
	}

	if (c.tt == T_LSH) {
		feed(c);

		b = parse_shift_expr(c);

		return mknode(c, N_LSH, a, b);
	}

	if (c.tt == T_RSH) {
		feed(c);

		b = parse_shift_expr(c);

		return mknode(c, N_RSH, a, b);
	}

	return a;
}

// mul_expr := shift_expr
//           | shift_expr '*' mul_expr
//           | shift_expr '/' mul_expr
//           | shift_expr '%' mul_expr
//           | shift_expr '&' mul_expr
parse_mul_expr(c: *compiler): *node {
	var a: *node;
	var b: *node;

	a = parse_shift_expr(c);
	if (!a) {
		return 0:*node;
	}

	if (c.tt == T_STAR) {
		feed(c);

		b = parse_mul_expr(c);

		return mknode(c, N_MUL, a, b);
	}

	if (c.tt == T_DIV) {
		feed(c);

		b = parse_mul_expr(c);

		return mknode(c, N_DIV, a, b);
	}

	if (c.tt == T_MOD) {
		feed(c);

		b = parse_mul_expr(c);

		return mknode(c, N_MOD, a, b);
	}

	if (c.tt == T_AMP) {
		feed(c);

		b = parse_mul_expr(c);

		return mknode(c, N_AND, a, b);
	}

	return a;
}

// add_expr := mul_expr
//           | mul_expr '+' add_expr
//           | mul_expr '-' add_expr
//           | mul_expr '|' add_expr
//           | mul_expr '^' add_expr
parse_add_expr(c: *compiler): *node {
	var a: *node;
	var b: *node;

	a = parse_mul_expr(c);
	if (!a) {
		return 0:*node;
	}

	if (c.tt == T_ADD) {
		feed(c);

		b = parse_add_expr(c);

		return mknode(c, N_ADD, a, b);
	}

	if (c.tt == T_SUB) {
		feed(c);

		b = parse_add_expr(c);

		return mknode(c, N_SUB, a, b);
	}

	if (c.tt == T_OR) {
		feed(c);

		b = parse_add_expr(c);

		return mknode(c, N_OR, a, b);
	}

	if (c.tt == T_XOR) {
		feed(c);

		b = parse_add_expr(c);

		return mknode(c, N_XOR, a, b);
	}

	return a;
}

// comp_expr := add_expr
//            | add_expr '<' add_expr
//            | add_expr '>' add_expr
//            | add_expr '<=' add_expr
//            | add_expr '>=' add_expr
//            | add_expr '==' add_expr
//            | add_expr '!=' add_expr
parse_comp_expr(c: *compiler): *node {
	var a: *node;
	var b: *node;

	a = parse_add_expr(c);
	if (!a) {
		return 0:*node;
	}

	if (c.tt == T_LT) {
		feed(c);

		b = parse_add_expr(c);
		if (!b) {
			die(c, "expected add_expr");
		}

		return mknode(c, N_LT, a, b);
	}

	if (c.tt == T_GT) {
		feed(c);

		b = parse_add_expr(c);
		if (!b) {
			die(c, "expected add_expr");
		}

		return mknode(c, N_GT, a, b);
	}

	if (c.tt == T_LE) {
		feed(c);

		b = parse_add_expr(c);
		if (!b) {
			die(c, "expected add_expr");
		}

		return mknode(c, N_LE, a, b);
	}

	if (c.tt == T_GE) {
		feed(c);

		b = parse_add_expr(c);
		if (!b) {
			die(c, "expected add_expr");
		}

		return mknode(c, N_GE, a, b);
	}

	if (c.tt == T_EQ) {
		feed(c);

		b = parse_add_expr(c);
		if (!b) {
			die(c, "expected add_expr");
		}

		return mknode(c, N_EQ, a, b);
	}

	if (c.tt == T_NE) {
		feed(c);

		b = parse_add_expr(c);
		if (!b) {
			die(c, "expected add_expr");
		}

		return mknode(c, N_NE, a, b);
	}

	return a;
}

// bool_expr := bool_expr
//            | add_expr '&&' bool_expr
//            | add_expr '||' bool_expr
parse_bool_expr(c: *compiler): *node {
	var a: *node;
	var b: *node;

	a = parse_comp_expr(c);
	if (!a) {
		return 0:*node;
	}

	if (c.tt == T_BAND) {
		feed(c);

		b = parse_bool_expr(c);
		if (!b) {
			die(c, "expected bool_expr");
		}

		return mknode(c, N_BAND, a, b);
	}

	if (c.tt == T_BOR) {
		feed(c);

		b = parse_bool_expr(c);
		if (!b) {
			die(c, "expected bool_expr");
		}

		return mknode(c, N_BOR, a, b);
	}

	return a;
}

// expr := bool_expr
//       | bool_expr '=' expr
parse_expr(c: *compiler): *node {
	var a: *node;
	var b: *node;

	a = parse_bool_expr(c);
	if (!a) {
		return 0:*node;
	}

	if (c.tt == T_ASSIGN) {
		feed(c);

		b = parse_expr(c);
		if (!b) {
			die(c, "expected expr");
		}

		return mknode(c, N_ASSIGN, a, b);
	}

	return a;
}

// if_stmt := 'if' expr '{' stmt_list '}'
//          | 'if' expr '{' stmt_list '}' 'else' if_stmt
//          | 'if' expr '{' stmt_list '}' 'else' '{' stmt_list '}'
parse_if_stmt(c: *compiler): *node {
	var n: *node;
	var e: *node;
	var a: *node;
	var b: *node;

	if (c.tt != T_IDENT || strcmp(c.token, "if")) {
		return 0:*node;
	}
	feed(c);

	n = mknode0(c, N_CONDLIST);
	e = n;

	loop {
		a = parse_expr(c);
		if (!a) {
			die(c, "expected expr");
		}

		if (c.tt != T_LBRA) {
			die(c, "expected {");
		}
		feed(c);

		b = parse_stmt_list(c);

		if (c.tt != T_RBRA) {
			die(c, "expected }");
		}
		feed(c);

		e.a = mknode(c, N_COND, a, b);

		if (c.tt != T_IDENT || strcmp(c.token, "else")) {
			return n;
		}
		feed(c);

		if (c.tt == T_LBRA) {
			feed(c);

			b = parse_stmt_list(c);

			if (c.tt != T_RBRA) {
				die(c, "expected }");
			}
			feed(c);

			e.b = mknode1(c, N_CONDLIST, mknode(c, N_COND, 0:*node, b));

			return n;
		}

		if (c.tt != T_IDENT || strcmp(c.token, "if")) {
			die(c, "expected if");
		}
		feed(c);

		e.b = mknode0(c, N_CONDLIST);
		e = e.b;
	}
}

// loop_stmt := 'loop' '{' stmt_list '}'
parse_loop_stmt(c: *compiler): *node {
	var a: *node;

	if (c.tt != T_IDENT || strcmp(c.token, "loop")) {
		return 0:*node;
	}
	feed(c);

	if (c.tt != T_LBRA) {
		die(c, "expected {");
	}
	feed(c);

	a = parse_stmt_list(c);

	if (c.tt != T_RBRA) {
		die(c, "expected }");
	}
	feed(c);

	return mknode1(c, N_LOOP, a);
}

// break_stmt := 'break'
parse_break_stmt(c: *compiler): *node {
	if (c.tt != T_IDENT || strcmp(c.token, "break")) {
		return 0:*node;
	}
	feed(c);

	return mknode0(c, N_BREAK);
}

// continue_stmt := 'continue'
parse_continue_stmt(c: *compiler): *node {
	if (c.tt != T_IDENT || strcmp(c.token, "continue")) {
		return 0:*node;
	}
	feed(c);

	return mknode0(c, N_CONTINUE);
}

// return_stmt := 'return'
//              | 'return' expr
parse_return_stmt(c: *compiler): *node {
	var a: *node;

	if (c.tt != T_IDENT || strcmp(c.token, "return")) {
		return 0:*node;
	}
	feed(c);

	a = parse_expr(c);

	return mknode1(c, N_RETURN, a);
}

// var_decl := ident ':' type
parse_var_decl(c: *compiler): *node {
	var n: *node;
	var a: *node;
	var b: *node;

	a = parse_ident(c);
	if (!a) {
		return 0:*node;
	}

	if (c.tt != T_COLON) {
		die(c, "expected :");
	}
	feed(c);

	b = parse_type(c);
	if (!b) {
		die(c, "expected type");
	}

	return mknode(c, N_VARDECL, a, b);
}

// var_stmt := 'var' var_decl
parse_var_stmt(c: *compiler): *node {
	var a: *node;

	if (c.tt != T_IDENT || strcmp(c.token, "var")) {
		return 0:*node;
	}
	feed(c);

	a = parse_var_decl(c);
	if (!a) {
		die(c, "expected var_decl");
	}

	return a;
}

// stmt := if_stmt
//       | loop_stmt
//       | break_stmt ';'
//       | continue_stmt ';'
//       | return_stmt ';'
//       | var_stmt ';'
//       | expr ';'
parse_stmt(c: *compiler): *node {
	var n: *node;

	n = parse_if_stmt(c);
	if (n) {
		return n;
	}

	n = parse_loop_stmt(c);
	if (n) {
		return n;
	}

	n = parse_break_stmt(c);
	if (n) {
		if (c.tt != T_SEMI) {
			die(c, "expected ;");
		}
		feed(c);

		return n;
	}

	n = parse_return_stmt(c);
	if (n) {
		if (c.tt != T_SEMI) {
			die(c, "expected ;");
		}
		feed(c);

		return n;
	}

	n = parse_continue_stmt(c);
	if (n) {
		if (c.tt != T_SEMI) {
			die(c, "expected ;");
		}
		feed(c);

		return n;
	}

	n = parse_var_stmt(c);
	if (n) {
		if (c.tt != T_SEMI) {
			die(c, "expected ;");
		}
		feed(c);

		return n;
	}

	n = parse_expr(c);
	if (n) {
		if (c.tt != T_SEMI) {
			die(c, "expected ;");
		}
		feed(c);

		return n;
	}

	return 0:*node;
}

// stmt_list := stmt
//            | stmt stmt_list
parse_stmt_list(c: *compiler): *node {
	var n: *node;
	var e: *node;
	var a: *node;

	a = parse_stmt(c);
	if (!a) {
		return 0:*node;
	}

	e = mknode1(c, N_STMTLIST, a);
	n = e;

	loop {
		a = parse_stmt(c);
		if (!a) {
			return n;
		}

		e.b = mknode1(c, N_STMTLIST, a);
		e = e.b;
	}
}

// enum_list := ident
//            | enum_list ',' enum_list
parse_enum_list(c: *compiler): *node {
	var n: *node;
	var e: *node;
	var a: *node;

	a = parse_ident(c);
	if (!a) {
		return 0:*node;
	}

	e = mknode1(c, N_ENUMLIST, a);
	n = e;

	loop {
		if (c.tt != T_COMMA) {
			return n;
		}
		feed(c);

		a = parse_ident(c);
		if (!a) {
			return n;
		}

		e.b = mknode1(c, N_ENUMLIST, a);
		e = e.b;
	}
}

// enum_decl := 'enum' ident '{' enum_list '}'
parse_enum_decl(c: *compiler): *node {
	var b: *node;

	if (c.tt != T_IDENT) {
		return 0:*node;
	}

	if (strcmp(c.token, "enum")) {
		return 0:*node;
	}
	feed(c);

	if (c.tt != T_LBRA) {
		die(c, "expected {");
	}
	feed(c);

	b = parse_enum_list(c);

	if (c.tt != T_RBRA) {
		die(c, "expected }");
	}
	feed(c);

	return mknode(c, N_ENUM, 0:*node, b);
}

// type := ident
//       | '*' type
//       | '(' type ')'
parse_type(c: *compiler): *node {
	var n: *node;

	if (c.tt == T_STAR) {
		feed(c);

		n = parse_type(c);

		return mknode1(c, N_PTRTYPE, n);
	}

	if (c.tt == T_LPAR) {
		feed(c);

		n = parse_type(c);

		if (c.tt != T_RPAR) {
			die(c, "expected )");
		}
		feed(c);

		return n;
	}

	return parse_ident(c);
}

// member_decl := ident ':' type
parse_member_decl(c: *compiler): *node {
	var n: *node;
	var a: *node;
	var b: *node;

	a = parse_ident(c);
	if (!a) {
		return 0: *node;
	}

	if (c.tt != T_COLON) {
		die(c, "expected :");
	}
	feed(c);

	b = parse_type(c);
	if (!b) {
		die(c, "expected type");
	}

	return mknode(c, N_MEMBERDECL, a, b);
}

// member_list := member_decl
//              | member_decl ';' member_list
parse_member_list(c: *compiler): *node {
	var n: *node;
	var e: *node;
	var a: *node;

	a = parse_member_decl(c);
	if (!a) {
		return 0:*node;
	}

	e = mknode1(c, N_MEMBERLIST, a);
	n = e;

	loop {
		if (c.tt != T_SEMI) {
			return n;
		}
		feed(c);

		a = parse_member_decl(c);
		if (!a) {
			return n;
		}

		e.b = mknode1(c, N_MEMBERLIST, a);
		e = e.b;
	}
}

// struct_decl := 'struct' ident '{' member_list '}'
parse_struct_decl(c: *compiler): *node {
	var a: *node;
	var b: *node;

	if (c.tt != T_IDENT) {
		return 0:*node;
	}

	if (strcmp(c.token, "struct")) {
		return 0:*node;
	}
	feed(c);

	a = parse_ident(c);
	if (!a) {
		die(c, "expected ident");
	}

	if (c.tt != T_LBRA) {
		die(c, "expected {");
	}
	feed(c);

	b = parse_member_list(c);

	if (c.tt != T_RBRA) {
		die(c, "expected }");
	}
	feed(c);

	return mknode(c, N_STRUCT, a, b);
}

// arg_decl := ':' type
//             ident ':' type
parse_arg_decl(c: *compiler): *node {
	var n: *node;
	var a: *node;
	var b: *node;

	a = parse_ident(c);
	if (!a) {
		return 0:*node;
	}

	if (c.tt != T_COLON) {
		die(c, "expected :");
	}
	feed(c);

	b = parse_type(c);
	if (!b) {
		die(c, "expected type");
	}

	return mknode(c, N_ARGDECL, a, b);
}

// arg_list := arg_decl
//           | arg_decl ',' arg_list
parse_arg_list(c: *compiler): *node {
	var n: *node;
	var e: *node;
	var a: *node;

	a = parse_arg_decl(c);
	if (!a) {
		return 0:*node;
	}

	e = mknode1(c, N_ARGLIST, a);
	n = e;

	loop {
		if (c.tt != T_COMMA) {
			return n;
		}
		feed(c);

		a = parse_arg_decl(c);
		if (!a) {
			die(c, "expected identifier");
		}

		e.b = mknode1(c, N_ARGLIST, a);
		e = e.b;
	}
}

// func_decl := ident '(' ')' ':' type
//            | ident '(' arg_list ')' ':' type
parse_func_decl(c: *compiler): *node {
	var a: *node;
	var b: *node;
	var t: *node;

	a = parse_ident(c);
	if (!a) {
		return 0:*node;
	}

	if (c.tt != T_LPAR) {
		die(c, "expected (");
	}
	feed(c);

	b = parse_arg_list(c);

	if (c.tt != T_RPAR) {
		die(c, "expected )");
	}
	feed(c);

	if (c.tt != T_COLON) {
		die(c, "expected :");
	}
	feed(c);

	t = parse_type(c);
	if (!t) {
		die(c, "expected type");
	}

	return mknode(c, N_FUNCDECL, a, mknode(c, N_FUNCTYPE, b, t));
}

// func := func_decl '{' stmt_list '}'
//       | func_decl ';'
parse_func(c: *compiler): *node {
	var n: *node;
	var a: *node;
	var b: *node;

	a = parse_func_decl(c);
	if (!a) {
		return 0:*node;
	}

	if (c.tt == T_SEMI) {
		feed(c);
		return a;
	}

	if (c.tt != T_LBRA) {
		die(c, "expected {");
	}
	feed(c);

	b = parse_stmt_list(c);

	if (c.tt != T_RBRA) {
		die(c, "expected }");
	}
	feed(c);

	return mknode(c, N_FUNC, a, b);
}

// decl := enum_decl
//       | struct_decl
//       | func
parse_decl(c: *compiler): *node {
	var n: *node;

	n = parse_enum_decl(c);
	if (n) {
		return n;
	}

	n = parse_struct_decl(c);
	if (n) {
		return n;
	}

	return parse_func(c);
}

// program := decl
//          | decl program
parse_program(c: *compiler): *node {
	var n: *node;
	var e: *node;
	var d: *node;

	d = parse_decl(c);
	if (!d) {
		if (c.tt != T_EOF) {
			die(c, "expected eof");
		}
		return 0:*node;
	}

	e = mknode1(c, N_PROGRAM, d);
	n = e;

	loop {
		d = parse_decl(c);
		if (!d) {
			if (c.tt != T_EOF) {
				die(c, "expected eof");
			}

			return n;
		}

		e.b = mknode1(c, N_PROGRAM, d);
		e = e.b;
	}
}

compile(c: *compiler, p: *node): void {
	var n: *node;
	var d: *decl;
	var kind: int;

	// Process enum and struct declarations
	n = p;
	loop {
		if (!n) {
			break;
		}

		kind = n.a.kind;
		if (kind == N_STRUCT) {
			defstruct(c, n.a);
		} else if (kind == N_ENUM) {
			defenum(c, n.a);
		} else if (kind != N_FUNC && kind != N_FUNCDECL) {
			die(c, "invalid decl");
		}

		n = n.b;
	}

	// Process function declarations
	n = p;
	loop {
		if (!n) {
			break;
		}

		kind = n.a.kind;
		if (kind == N_FUNCDECL) {
			defextern(c, n.a);
		} else if (kind == N_FUNC) {
			defun(c, n.a);
		}

		n = n.b;
	}

	// Allocate structs
	d = first_decl(c);
	loop {
		if (!d) {
			break;
		}

		if (d.struct_defined) {
			layout_struct(c, d);
		}

		d = next_decl(c, d);
	}

	// Compile functions
	d = first_decl(c);
	loop {
		if (!d) {
			break;
		}

		if (d.func_defined) {
			compile_func(c, d);
		}

		d = next_decl(c, d);
	}
}

defextern(c: *compiler, n: *node): *decl {
	var d: *decl;
	var name: *byte;
	var t: *type;

	name = n.a.s;

	t = prototype(c, n.b);

	d = find(c, name, 0:*byte, 1);

	if (d.func_defined) {
		die(c, "duplicate function");
	}

	d.func_defined = 1;
	d.func_type = t;

	return d;
}

defun(c: *compiler, n: *node): void {
	var d: *decl;

	d = defextern(c, n.a);

	d.func_def = n;
}

defstruct(c: *compiler, n: *node): void {
	var name: *byte;
	var d: *decl;

	name = n.a.s;

	if (!strcmp(name, "int") || !strcmp(name, "byte") || !strcmp(name, "func")) {
		die(c, "reserved word");
	}

	d = find(c, name, 0:*byte, 1);

	if (d.struct_defined) {
		die(c, "duplicate struct");
	}

	d.struct_defined = 1;
	d.struct_def = n;
}

defenum(c: *compiler, n: *node): void {
	var d: *decl;
	var i: int;
	var name: *byte;

	n = n.b;
	i = 0;
	loop {
		if (!n) {
			break;
		}

		name = n.a.s;
		d = find(c, name, 0:*byte, 1);

		if (d.enum_defined) {
			die(c, "duplicate enum");
		}

		d.enum_defined = 1;
		d.enum_value = i;
		d.enum_def = n;

		i = i + 1;
		n = n.b;
	}
}

type_sizeof(c: *compiler, t: *type): int {
	var kind: int;

	kind = t.kind;
	if (kind == TY_INT) {
		return 8;
	} else if (kind == TY_BYTE) {
		return 8;
	} else if (kind == TY_PTR) {
		return 8;
	} else if (kind == TY_FUNC) {
		return 8;
	} else if (kind == TY_STRUCT) {
		layout_struct(c, t.st);
		return t.st.struct_size;
	} else {
		die(c, "sizeof: invalid type");
	}
}

layout_struct(c: *compiler, d: *decl): void {
	var m: *node;
	var offset: int;
	var name: *byte;
	var md: *decl;
	var t: *type;

	if (d.struct_layout_done) {
		if (d.struct_layout_done == 2) {
			die(c, "circular struct definition");
		}

		return;
	}

	d.struct_layout_done = 2;

	m = d.struct_def.b;
	offset = 0;
	loop {
		if (!m) {
			break;
		}

		name = m.a.a.s;
		t = prototype(c, m.a.b);

		md = find(c, d.name, name, 1);

		if (d.member_defined) {
			die(c, "duplicate member");
		}

		md.member_defined = 1;
		md.member_type = t;
		md.member_offset = offset;
		md.member_def = m;

		offset = offset + type_sizeof(c, t);

		m = m.b;
	}

	d.struct_size = offset;
	d.struct_layout_done = 1;
}

compile_func(c: *compiler, d: *decl): void {
	var name: *byte;
	var v: *decl;
	var t: *type;
	var offset: int;
	var n: *node;

	if (!d.func_def) {
		return;
	}

	n = d.func_def.a.b.a;
	offset = 16;
	loop {
		if (!n) {
			break;
		}

		name = n.a.a.s;
		t = prototype(c, n.a.b);

		v = find(c, d.name, name, 1);
		if (v.var_defined) {
			die(c, "duplicate argument");
		}

		v.var_defined = 1;
		v.var_type = t;
		v.var_offset = offset;
		v.var_def = n.a;

		offset = offset + 8;
		n = n.b;
	}

	// Hoist locals
	offset = hoist_locals(c, d, d.func_def.b, 0);

	// Compile the function body
	emit_str(c, d.name);
	fixup_label(c, d.func_label);
	emit_preamble(c, offset);
	compile_stmt(c, d, d.func_def.b, 0:*label, 0:*label);
	emit_num(c, 0);
	emit_ret(c);
}

hoist_locals(c: *compiler, d: *decl, n: *node, offset: int): int {
	var kind: int;
	var name: *byte;
	var t: *type;
	var v: *decl;

	if (!n) {
		return offset;
	}

	kind = n.kind;
	if (kind == N_CONDLIST) {
		loop {
			if (!n) {
				return offset;
			}

			hoist_locals(c, d, n.a.b, offset);

			n = n.b;
		}
	} else if (kind == N_STMTLIST) {
		loop {
			if (!n) {
				return offset;
			}

			offset = hoist_locals(c, d, n.a, offset);

			n = n.b;
		}
	} else if (kind == N_LOOP) {
		return hoist_locals(c, d, n.a, offset);
	} else if (kind != N_VARDECL) {
		return offset;
	}

	name = n.a.s;
	t = prototype(c, n.b);

	v = find(c, d.name, name, 1);

	if (v.var_defined) {
		die(c, "duplicate variable");
	}

	v.var_type = t;
	v.var_defined = 1;

	offset = offset + type_sizeof(c, t);

	v.var_offset = -offset;

	return offset;
}

// Unify two types
unify(c: *compiler, a: *type, b: *type): void {
	var kind: int;

	if (a == b) {
		return;
	}

	if ((a && !b) || (b && !a) || a.kind != b.kind) {
		die(c, "type error");
	}

	kind = a.kind;
	if (kind == TY_PTR) {
		unify(c, a.val, b.val);
	} else if (kind == TY_FUNC) {
		unify(c, a.val, b.val);
		unify(c, a.arg, b.arg);
	} else if (kind == TY_ARG) {
		unify(c, a.val, b.val);
		unify(c, a.arg, b.arg);
	} else if (kind == TY_STRUCT) {
		if (a.st != b.st) {
			die(c, "type error");
		}
	} else if (kind != TY_VOID && kind != TY_INT && kind != TY_BYTE) {
		die(c, "unify: invalid type");
	}
}

count_args(c: *compiler, t: *type): int {
	var nargs: int;

	nargs = 0;
	loop {
		if (!t) {
			break;
		}

		t = t.arg;
		nargs = nargs + 1;
	}

	return nargs;
}

// Translate an expression
compile_expr(c: *compiler, d: *decl, n: *node, rhs: int): void {
	var no: *label;
	var out: *label;
	var v: *decl;
	var kind: int;

	kind = n.kind;
	if (kind == N_STR) {
		if (!rhs) {
			die(c, "str is not an lexpr");
		}

		emit_str(c, n.s);

		n.t = mktype1(c, TY_PTR, mktype0(c, TY_BYTE));
	} else if (kind == N_NUM) {
		if (!rhs) {
			die(c, "num is not an lexpr");
		}

		emit_num(c, n.n);

		n.t = mktype0(c, TY_INT);
	} else if (kind == N_CHAR) {
		if (!rhs) {
			die(c, "char is not an lexpr");
		}

		emit_num(c, n.n);

		n.t = mktype0(c, TY_INT);
	} else if (kind == N_EXPRLIST) {
		if (!rhs) {
			die(c, "call is not an lexpr");
		}

		if (n.b) {
			compile_expr(c, d, n.b, 1);
		}

		compile_expr(c, d, n.a, 1);

		if (n.b) {
			n.t = mktype2(c, TY_ARG, n.a.t, n.b.t);
		} else {
			n.t = mktype1(c, TY_ARG, n.a.t);
		}
	} else if (kind == N_CALL) {
		if (!rhs) {
			die(c, "call is not an lexpr");
		}

		if (n.b) {
			compile_expr(c, d, n.b, 1);
		}

		compile_expr(c, d, n.a, 0);

		if (n.a.t.kind != TY_FUNC) {
			die(c, "calling not a function");
		}

		if (n.b) {
			unify(c, n.a.t.arg, n.b.t);
		} else {
			unify(c, n.a.t.arg, 0: *type);
		}

		emit_call(c, count_args(c, n.a.t.arg));

		n.t = n.a.t.val;
	} else if (kind == N_DOT) {
		compile_expr(c, d, n.a, 0);

		if (n.a.t.kind == TY_PTR) {
			if (n.a.t.val.kind != TY_STRUCT) {
				die(c, "dot not a struct");
			}

			v = find(c, n.a.t.val.st.name, n.b.s, 0);

			emit_load(c, n.a.t);
		} else {
			if (n.a.t.kind != TY_STRUCT) {
				die(c, "dot not a struct");
			}

			v = find(c, n.a.t.st.name, n.b.s, 0);
		}

		if (!v || !v.member_defined) {
			die(c, "no such member");
		}

		emit_num(c, v.member_offset);
		emit_add(c);

		n.t = v.member_type;

		if (rhs) {
			emit_load(c, n.t);
		}
	} else if (kind == N_IDENT) {
		v = find(c, n.s, 0:*byte, 0);
		if (v && v.enum_defined) {
			emit_num(c, v.enum_value);
			n.t = mktype0(c, TY_INT);
			return;
		}

		v = find(c, d.name, n.s, 0);
		if (v && v.var_defined) {
			emit_lea(c, v.var_offset);
			n.t = v.var_type;
			if (rhs) {
				emit_load(c, n.t);
			}
			return;
		}

		v = find(c, n.s, 0:*byte, 0);
		if (v && v.func_defined) {
			emit_ptr(c, v.func_label);
			n.t = v.func_type;
			return;
		}

		die(c, "no such variable");
	} else if (kind == N_ASSIGN) {
		if (!rhs) {
			die(c, "assign is not an lexpr");
		}

		compile_expr(c, d, n.b, 1);
		compile_expr(c, d, n.a, 0);

		unify(c, n.a.t, n.b.t);

		n.t = n.a.t;

		emit_store(c, n.t);
	} else if (kind == N_SIZEOF) {
		if (!rhs) {
			die(c, "sizeof is not an lexpr");
		}

		out = mklabel(c);

		emit_jmp(c, out);

		compile_expr(c, d, n.a, 0);

		fixup_label(c, out);

		if (n.a.t.kind == TY_BYTE) {
			emit_num(c, 1);
		} else {
			emit_num(c, type_sizeof(c, n.a.t));
		}

		n.t = mktype0(c, TY_INT);
	} else if (kind == N_REF) {
		if (!rhs) {
			die(c, "ref is not an lexpr");
		}

		compile_expr(c, d, n.a, 0);

		n.t = mktype1(c, TY_PTR, n.a.t);
	} else if (kind == N_DEREF) {
		compile_expr(c, d, n.a, 1);

		if (n.a.t.kind != TY_PTR) {
			die(c, "deref not a pointer");
		}

		n.t = n.a.t.val;

		if (rhs) {
			emit_load(c, n.t);
		}
	} else if (kind == N_INDEX) {
		compile_expr(c, d, n.a, 1);
		compile_expr(c, d, n.b, 1);

		if (n.a.t.kind != TY_PTR) {
			die(c, "not a pointer");
		}

		if (!type_isint(c, n.b.t)) {
			die(c, "index: not an int");
		}

		n.t = n.a.t.val;

		if (n.t.kind == TY_BYTE) {
			emit_num(c, 1);
		} else {
			emit_num(c, type_sizeof(c, n.t));
		}

		emit_mul(c);
		emit_add(c);

		if (rhs) {
			emit_load(c, n.t);
		}
	} else if (kind == N_LT) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.b, 1);
		compile_expr(c, d, n.a, 1);
		emit_lt(c);

		unify(c, n.a.t, n.b.t);

		if (!type_isprim(c, n.a.t)) {
			die(c, "lt: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_GT) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.b, 1);
		compile_expr(c, d, n.a, 1);
		emit_gt(c);

		unify(c, n.a.t, n.b.t);

		if (!type_isprim(c, n.a.t)) {
			die(c, "gt: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_LE) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.b, 1);
		compile_expr(c, d, n.a, 1);
		emit_le(c);

		unify(c, n.a.t, n.b.t);

		if (!type_isprim(c, n.a.t)) {
			die(c, "le: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_GE) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.b, 1);
		compile_expr(c, d, n.a, 1);
		emit_ge(c);

		unify(c, n.a.t, n.b.t);

		if (!type_isprim(c, n.a.t)) {
			die(c, "ge: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_EQ) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.b, 1);
		compile_expr(c, d, n.a, 1);
		emit_eq(c);

		unify(c, n.a.t, n.b.t);

		if (!type_isprim(c, n.a.t)) {
			die(c, "eq: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_NE) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.b, 1);
		compile_expr(c, d, n.a, 1);
		emit_ne(c);

		unify(c, n.a.t, n.b.t);

		if (!type_isprim(c, n.a.t)) {
			die(c, "ne: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_BNOT) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		no = mklabel(c);
		out = mklabel(c);

		compile_expr(c, d, n.a, 1);

		emit_jz(c, no);
		emit_num(c, 0);
		emit_jmp(c, out);
		fixup_label(c, no);
		emit_num(c, 1);
		fixup_label(c, out);

		if (!type_isprim(c, n.a.t)) {
			die(c, "not an prim");
		}

		n.t = mktype0(c, TY_INT);
	} else if (kind == N_BOR) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		no = mklabel(c);
		out = mklabel(c);

		compile_expr(c, d, n.a, 1);
		emit_jz(c, no);
		emit_num(c, 1);
		emit_jmp(c, out);

		fixup_label(c, no);
		no = mklabel(c);

		compile_expr(c, d, n.b, 1);
		emit_jz(c, no);
		emit_num(c, 1);
		emit_jmp(c, out);

		fixup_label(c, no);
		emit_num(c, 0);

		fixup_label(c, out);

		if (!type_isprim(c, n.a.t)) {
			die(c, "not an prim");
		}

		if (!type_isprim(c, n.b.t)) {
			die(c, "not an prim");
		}

		n.t = mktype0(c, TY_INT);
	} else if (kind == N_BAND) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		no = mklabel(c);
		out = mklabel(c);

		compile_expr(c, d, n.a, 1);
		emit_jz(c, no);

		compile_expr(c, d, n.b, 1);
		emit_jz(c, no);

		emit_num(c, 1);
		emit_jmp(c, out);

		fixup_label(c, no);
		emit_num(c, 0);

		fixup_label(c, out);

		if (!type_isprim(c, n.a.t)) {
			die(c, "not an prim");
		}

		if (!type_isprim(c, n.b.t)) {
			die(c, "not an prim");
		}

		n.t = mktype0(c, TY_INT);
	} else if (kind == N_POS) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.a, 1);

		if (!type_isint(c, n.a.t)) {
			die(c, "pos: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_NEG) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.a, 1);
		emit_neg(c);

		if (!type_isint(c, n.a.t)) {
			die(c, "neg: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_NOT) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.a, 1);
		emit_not(c);

		if (!type_isint(c, n.a.t)) {
			die(c, "not: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_ADD) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.b, 1);
		compile_expr(c, d, n.a, 1);
		emit_add(c);

		unify(c, n.a.t, n.b.t);

		if (!type_isint(c, n.a.t)) {
			die(c, "add: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_SUB) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.b, 1);
		compile_expr(c, d, n.a, 1);
		emit_sub(c);

		unify(c, n.a.t, n.b.t);

		if (!type_isint(c, n.a.t)) {
			die(c, "sub: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_MUL) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.b, 1);
		compile_expr(c, d, n.a, 1);
		emit_mul(c);

		unify(c, n.a.t, n.b.t);

		if (!type_isint(c, n.a.t)) {
			die(c, "mul: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_DIV) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.b, 1);
		compile_expr(c, d, n.a, 1);
		emit_div(c);

		unify(c, n.a.t, n.b.t);

		if (!type_isint(c, n.a.t)) {
			die(c, "div: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_MOD) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.b, 1);
		compile_expr(c, d, n.a, 1);
		emit_mod(c);

		unify(c, n.a.t, n.b.t);

		if (!type_isint(c, n.a.t)) {
			die(c, "mod: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_LSH) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.b, 1);
		compile_expr(c, d, n.a, 1);
		emit_lsh(c);

		unify(c, n.a.t, n.b.t);

		if (!type_isint(c, n.a.t)) {
			die(c, "lsh: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_RSH) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.b, 1);
		compile_expr(c, d, n.a, 1);
		emit_rsh(c);

		unify(c, n.a.t, n.b.t);

		if (!type_isint(c, n.a.t)) {
			die(c, "rsh: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_AND) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.b, 1);
		compile_expr(c, d, n.a, 1);
		emit_and(c);

		unify(c, n.a.t, n.b.t);

		if (!type_isint(c, n.a.t)) {
			die(c, "and: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_OR) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.b, 1);
		compile_expr(c, d, n.a, 1);
		emit_or(c);

		unify(c, n.a.t, n.b.t);

		if (!type_isint(c, n.a.t)) {
			die(c, "or: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_XOR) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.b, 1);
		compile_expr(c, d, n.a, 1);
		emit_xor(c);

		unify(c, n.a.t, n.b.t);

		if (!type_isint(c, n.a.t)) {
			die(c, "xor: not an int");
		}

		n.t = n.a.t;
	} else if (kind == N_CAST) {
		if (!rhs) {
			die(c, "not lexpr");
		}

		compile_expr(c, d, n.a, 1);
		if (!type_isprim(c, n.a.t)) {
			die(c, "not a primitive");
		}

		n.t = prototype(c, n.b);
	} else {
		die(c, "not an expression");
	}
}

// Compile a statement
compile_stmt(c: *compiler, d: *decl, n: *node, top: *label, out: *label): void {
	var no: *label;
	var ifout: *label;
	var kind: int;

	if (!n) {
		return;
	}

	c.lineno = c.lineno;
	c.colno = 0;

	kind = n.kind;
	if (kind == N_CONDLIST) {
		ifout = mklabel(c);
		no = 0: *label;
		loop {
			if (no) {
				fixup_label(c, no);
			}

			if (!n) {
				break;
			}

			no = mklabel(c);

			if (n.a.a) {
				compile_expr(c, d, n.a.a, 1);
				emit_jz(c, no);
			}

			compile_stmt(c, d, n.a.b, top, out);
			emit_jmp(c, ifout);

			n = n.b;
		}
		fixup_label(c, ifout);
	} else if (kind == N_STMTLIST) {
		loop {
			if (!n) {
				break;
			}
			compile_stmt(c, d, n.a, top, out);
			n = n.b;
		}
	} else if (kind == N_LOOP) {
		top = mklabel(c);
		out = mklabel(c);
		fixup_label(c, top);
		compile_stmt(c, d, n.a, top, out);
		emit_jmp(c, top);
		fixup_label(c, out);
	} else if (kind == N_BREAK) {
		if (!out) {
			die(c, "break outside loop");
		}
		emit_jmp(c, out);
	} else if (kind == N_CONTINUE) {
		if (!top) {
			die(c, "continue outside loop");
		}
		emit_jmp(c, top);
	} else if (kind == N_RETURN) {
		if (n.a) {
			if (d.func_type.val.kind == TY_VOID) {
				die(c, "returning a value in a void function");
			}
			compile_expr(c, d, n.a, 1);
			unify(c, n.a.t, d.func_type.val);
		} else {
			if (d.func_type.val.kind != TY_VOID) {
				die(c, "returning void in a non void function");
			}
			emit_num(c, 0);
		}
		emit_ret(c);
	} else if (kind != N_VARDECL) {
		compile_expr(c, d, n, 1);
		emit_pop(c, 1);
	}
}

find(c: *compiler, name: *byte, member_name: *byte, make: int): *decl {
	var p: *decl;
	var d: *decl;
	var link: **decl;
	var dir: int;

	p = 0: *decl;
	link = &c.decls;
	loop {
		d = *link;
		if (!d) {
			break;
		}

		dir = strcmp(name, d.name);

		if (dir == 0) {
			if (!member_name && !d.member_name) {
				dir = 0;
			} else if (!member_name) {
				dir = -1;
			} else if (!d.member_name) {
				dir = 1;
			} else {
				dir = strcmp(member_name, d.member_name);
			}
		}

		if (dir < 0) {
			p = d;
			link = &d.l;
		} else if (dir > 0) {
			p = d;
			link = &d.r;
		} else {
			return d;
		}
	}


	if (!make) {
		return 0:*decl;
	}

	d = alloc(c, sizeof(*d)): *decl;

	d.name = name;
	d.member_name = member_name;

	d.p = p;
	d.l = 0:*decl;
	d.r = 0:*decl;

	d.func_defined = 0;
	d.func_type = 0:*type;
	d.func_label = mklabel(c);
	d.func_def = 0:*node;

	d.struct_defined = 0;
	d.struct_size = 0;
	d.struct_layout_done = 0;
	d.struct_def = 0:*node;

	d.member_defined = 0;
	d.member_type = 0:*type;
	d.member_offset = 0;
	d.member_def = 0:*node;

	d.enum_defined = 0;
	d.enum_value = 0;
	d.enum_def = 0:*node;

	d.var_defined = 0;
	d.var_type = 0:*type;
	d.var_offset = 0;
	d.var_def = 0:*node;

	*link = d;

	return d;
}

// Find the first declaration
first_decl(c: *compiler): *decl {
	var d: *decl;

	d = c.decls;
	if (!d) {
		return 0:*decl;
	}

	loop {
		if (!d.l) {
			return d;
		}

		d = d.l;
	}
}

next_decl(c: *compiler, d: *decl): *decl {
	if (!d) {
		return 0:*decl;
	}

	if (d.r) {
		d = d.r;

		loop {
			if (!d.l) {
				return d;
			}

			d = d.l;
		}
	}

	loop {
		if (!d.p) {
			return 0:*decl;
		}

		if (d.p.l == d) {
			return d.p;
		}

		d = d.p;
	}
}

mktype(c: *compiler, kind: int, a: *type, b: *type, st: *decl): *type {
	var t: *type;

	t = alloc(c, sizeof(*t)):*type;

	t.kind = kind;
	t.st = st;
	t.val = a;
	t.arg = b;

	return t;
}

mktype_struct(c: *compiler, st: *decl): *type {
	return mktype(c, TY_STRUCT, 0:*type, 0:*type, st);
}

mktype0(c: *compiler, kind: int): *type {
	return mktype(c, kind, 0:*type, 0:*type, 0:*decl);
}

mktype1(c: *compiler, kind: int, a: *type): *type {
	return mktype(c, kind, a, 0:*type, 0:*decl);
}

mktype2(c: *compiler, kind: int, a: *type, b: *type): *type {
	return mktype(c, kind, a, b, 0:*decl);
}

prototype(c: *compiler, n: *node): *type {
	var a: *type;
	var b: *type;
	var st: *decl;
	var kind: int;

	if (!n) {
		return 0:*type;
	}

	kind = n.kind;
	if (kind == N_IDENT) {
		if (!strcmp(n.s, "void")) {
			return mktype0(c, TY_VOID);
		}

		if (!strcmp(n.s, "int")) {
			return mktype0(c, TY_INT);
		}

		if (!strcmp(n.s, "byte")) {
			return mktype0(c, TY_BYTE);
		}

		st = find(c, n.s, 0:*byte, 0);
		if (!st || !st.struct_defined) {
			die(c, "unknown struct");
		}

		return mktype_struct(c, st);
	} else if (kind == N_ARGLIST) {
		a = prototype(c, n.a.b);
		b = prototype(c, n.b);

		kind = a.kind;
		if (kind != TY_INT && kind != TY_BYTE
				&& kind != TY_PTR && kind != TY_FUNC) {
			die(c, "not a ptr arg");
		}

		return mktype2(c, TY_ARG, a, b);
	} else if (kind == N_FUNCTYPE) {
		a = prototype(c, n.b);
		b = prototype(c, n.a);

		kind = a.kind;
		if (kind != TY_VOID && kind != TY_INT && kind != TY_BYTE
				&& kind != TY_PTR && kind != TY_FUNC) {
			die(c, "not a ptr return");
		}

		return mktype2(c, TY_FUNC, a, b);
	} else if (kind == N_PTRTYPE) {
		return mktype1(c, TY_PTR, prototype(c, n.a));
	} else {
		die(c, "prototype: invalid type");
	}
}

type_isint(c: *compiler, t: *type): int {
	return t.kind == TY_INT || t.kind == TY_BYTE;
}

type_isprim(c: *compiler, t: *type): int {
	return t.kind != TY_VOID && t.kind != TY_STRUCT;
}

// Create a new label
mklabel(c: *compiler): *label {
	var l: *label;

	l = alloc(c, sizeof(*l)):*label;

	l.fix = 0:*fixup;
	l.at = 0;
	l.fixed = 0;

	return l;
}

// Reserve size in the output buffer
reserve(c: *compiler, n: int): void {
	var m: *byte;
	var b: *chunk;

	if (c.text_end && c.text_end.cap - c.text_end.fill >= n) {
		return;
	}

	if (n < 4096) {
		n = 4096;
	}

	m = alloc(c, n);
	b = alloc(c, sizeof(*b)):*chunk;

	b.buf = m;
	b.fill = 0;
	b.cap = n;
	b.next = 0:*chunk;

	if (c.text_end) {
		c.text_end.next = b;
		c.text_end = b;
	} else {
		c.text = b;
		c.text_end = b;
	}
}

// Add a single byte to the output
emit(c: *compiler, x: int): void {
	reserve(c, 1);
	c.text_end.buf[c.text_end.fill] = x:byte;
	c.text_end.fill = c.text_end.fill + 1;
	c.at = c.at + 1;
}

// Fix a single reference
fixup(c: *compiler, here: *byte, delta: int): void {
	here[0] = delta: byte;
	here[1] = (delta >> 8): byte;
	here[2] = (delta >> 16): byte;
	here[3] = (delta >> 24): byte;
}

// Add an new fixup for the current position
addfixup(c: *compiler, l: *label): void {
	var f: *fixup;
	var here: *byte;

	reserve(c, 4);

	here = &c.text_end.buf[c.text_end.fill];

	emit(c, 0);
	emit(c, 0);
	emit(c, 0);
	emit(c, 0);

	if (l.fixed) {
		fixup(c, here, l.at - c.at);
	} else {
		f = alloc(c, sizeof(*f)): *fixup;

		f.next = l.fix;
		f.ptr = here;
		f.at = c.at;

		l.fix = f;
	}
}

// Fix references to a label to the current position
fixup_label(c: *compiler, l: *label): void {
	var f: *fixup;

	if (l.fixed) {
		die(c, "already fixed");
	}

	l.at = c.at;
	l.fixed = 1;

	f = l.fix;
	loop {
		if (!f) {
			break;
		}
		fixup(c, f.ptr, l.at - f.at);
		f = f.next;
	}
}

emit_ptr(c: *compiler, l: *label): void {
	// lea %rax, [l]
	emit(c, 0x48);
	emit(c, 0x8d);
	emit(c, 0x05);
	addfixup(c, l);
	// push %rax
	emit(c, 0x50);
}

emit_jmp(c: *compiler, l: *label): void {
	// jmp l
	emit(c, 0xe9);
	addfixup(c, l);
}

emit_num(c: *compiler, x: int): void {
	// push x
	emit(c, 0x68);
	emit(c, x);
	emit(c, x >> 8);
	emit(c, x >> 16);
	emit(c, x >> 24);
}

emit_str(c: *compiler, s: *byte): void {
	var a: *label;
	var b: *label;
	var i: int;

	a = mklabel(c);
	b = mklabel(c);

	// jmp b
	emit_jmp(c, b);

	// a:
	fixup_label(c, a);

	i = 0;
	// .string s
	loop {
		if (!s[i]) {
			break;
		}
		emit(c, s[i]:int);
		i = i + 1;
	}
	emit(c, 0);

	// nop sled
	emit(c, 0x90);
	emit(c, 0x90);
	emit(c, 0x90);
	emit(c, 0x90);
	emit(c, 0x90);
	emit(c, 0x90);
	emit(c, 0x90);
	emit(c, 0x90);

	// b:
	fixup_label(c, b);
	// push a
	emit_ptr(c, a);
}

emit_pop(c: *compiler, n: int): void {
	n = n * 8;
	// add rsp, 8*n
	emit(c, 0x48);
	emit(c, 0x81);
	emit(c, 0xc4);
	emit(c, n);
	emit(c, n >> 8);
	emit(c, n >> 16);
	emit(c, n >> 24);
}

emit_preamble(c: *compiler, n: int): void {
	var i: int;
	// push rbp
	emit(c, 0x55);
	// mov rbp, rsp
	emit(c, 0x48);
	emit(c, 0x89);
	emit(c, 0xe5);
	i = 0;
	loop {
		if (i >= n) {
			break;
		}
		emit_num(c, 0);
		i = i + 8;
	}
}

emit_store(c: *compiler, t: *type): void {
	// pop rdi
	emit(c, 0x5f);
	// pop rax
	emit(c, 0x58);
	if (t.kind == TY_BYTE) {
		// mov [rdi], al
		emit(c, 0x88);
		emit(c, 0x07);
	} else if (type_isprim(c, t)) {
		// mov [rdi], rax
		emit(c, 0x48);
		emit(c, 0x89);
		emit(c, 0x07);
	} else {
		die(c, "invalid store");
	}
	// push rax
	emit(c, 0x50);
}

emit_load(c: *compiler, t: *type): void {
	// pop rdi
	emit(c, 0x5f);
	if (t.kind == TY_BYTE) {
		// xor rax, rax
		emit(c, 0x48);
		emit(c, 0x31);
		emit(c, 0xc0);
		// mov al, [rdi]
		emit(c, 0x8a);
		emit(c, 0x07);
	} else if (type_isprim(c, t)) {
		// mov rax, [rdi]
		emit(c, 0x48);
		emit(c, 0x8b);
		emit(c, 0x07);
	} else {
		die(c, "invalid load");
	}
	// push rax
	emit(c, 0x50);
}

emit_jz(c: *compiler, l: *label): void {
	// pop rax
	emit(c, 0x58);
	// test rax, rax
	emit(c, 0x48);
	emit(c, 0x85);
	emit(c, 0xc0);
	// jz no
	emit(c, 0x0f);
	emit(c, 0x84);
	addfixup(c, l);
}

emit_lea(c: *compiler, offset: int): void {
	// lea rax, [rbp + offset]
	emit(c, 0x48);
	emit(c, 0x8d);
	emit(c, 0x85);
	emit(c, offset);
	emit(c, offset >> 8);
	emit(c, offset >> 16);
	emit(c, offset >> 24);
	// push rax
	emit(c, 0x50);
}

emit_and(c: *compiler): void {
	// pop rax
	emit(c, 0x58);
	// pop rdx
	emit(c, 0x5a);
	// and rdx, rax
	emit(c, 0x48);
	emit(c, 0x21);
	emit(c, 0xd0);
	// push rax
	emit(c, 0x50);
}

emit_or(c: *compiler): void {
	// pop rax
	emit(c, 0x58);
	// pop rdx
	emit(c, 0x5a);
	// or rdx, rax
	emit(c, 0x48);
	emit(c, 0x09);
	emit(c, 0xd0);
	// push rax
	emit(c, 0x50);
}

emit_xor(c: *compiler): void {
	// pop rax
	emit(c, 0x58);
	// pop rdx
	emit(c, 0x5a);
	// xor rdx, rax
	emit(c, 0x48);
	emit(c, 0x31);
	emit(c, 0xd0);
	// push rax
	emit(c, 0x50);
}

emit_add(c: *compiler): void {
	// pop rax
	emit(c, 0x58);
	// pop rdx
	emit(c, 0x5a);
	// add rdx, rax
	emit(c, 0x48);
	emit(c, 0x01);
	emit(c, 0xd0);
	// push rax
	emit(c, 0x50);
}

emit_ret(c: *compiler): void {
	// pop rax
	emit(c, 0x58);
	// mov rsp, rbp
	emit(c, 0x48);
	emit(c, 0x89);
	emit(c, 0xec);
	// pop rbp
	emit(c, 0x5d);
	// ret
	emit(c, 0xc3);
}

emit_call(c: *compiler, n: int): void {
	// pop rax
	emit(c, 0x58);
	// call rax
	emit(c, 0xff);
	emit(c, 0xd0);
	// add rsp, 8*(n+1)
	emit_pop(c, n);
	// push rax
	emit(c, 0x50);
}

emit_gt(c: *compiler): void {
	// pop rdx
	emit(c, 0x5a);
	// pop rcx
	emit(c, 0x59);
	// xor rax, rax
	emit(c, 0x48);
	emit(c, 0x31);
	emit(c, 0xc0);
	// cmp rdx, rcx
	emit(c, 0x48);
	emit(c, 0x39);
	emit(c, 0xca);
	// setg al
	emit(c, 0x0f);
	emit(c, 0x9f);
	emit(c, 0xc0);
	// mov rax
	emit(c, 0x50);
}

emit_lt(c: *compiler): void {
	// pop rdx
	emit(c, 0x5a);
	// pop rcx
	emit(c, 0x59);
	// xor rax, rax
	emit(c, 0x48);
	emit(c, 0x31);
	emit(c, 0xc0);
	// cmp rdx, rcx
	emit(c, 0x48);
	emit(c, 0x39);
	emit(c, 0xca);
	// setl al
	emit(c, 0x0f);
	emit(c, 0x9c);
	emit(c, 0xc0);
	// mov rax
	emit(c, 0x50);
}

emit_ge(c: *compiler): void {
	// pop rdx
	emit(c, 0x5a);
	// pop rcx
	emit(c, 0x59);
	// xor rax, rax
	emit(c, 0x48);
	emit(c, 0x31);
	emit(c, 0xc0);
	// cmp rdx, rcx
	emit(c, 0x48);
	emit(c, 0x39);
	emit(c, 0xca);
	// setge al
	emit(c, 0x0f);
	emit(c, 0x9d);
	emit(c, 0xc0);
	// mov rax
	emit(c, 0x50);
}

emit_le(c: *compiler): void {
	// pop rdx
	emit(c, 0x5a);
	// pop rcx
	emit(c, 0x59);
	// xor rax, rax
	emit(c, 0x48);
	emit(c, 0x31);
	emit(c, 0xc0);
	// cmp rdx, rcx
	emit(c, 0x48);
	emit(c, 0x39);
	emit(c, 0xca);
	// setle al
	emit(c, 0x0f);
	emit(c, 0x9e);
	emit(c, 0xc0);
	// mov rax
	emit(c, 0x50);
}

emit_eq(c: *compiler): void {
	// pop rdx
	emit(c, 0x5a);
	// pop rcx
	emit(c, 0x59);
	// xor rax, rax
	emit(c, 0x48);
	emit(c, 0x31);
	emit(c, 0xc0);
	// cmp rdx, rcx
	emit(c, 0x48);
	emit(c, 0x39);
	emit(c, 0xca);
	// sete al
	emit(c, 0x0f);
	emit(c, 0x94);
	emit(c, 0xc0);
	// mov rax
	emit(c, 0x50);
}

emit_ne(c: *compiler): void {
	// pop rdx
	emit(c, 0x5a);
	// pop rcx
	emit(c, 0x59);
	// xor rax, rax
	emit(c, 0x48);
	emit(c, 0x31);
	emit(c, 0xc0);
	// cmp rdx, rcx
	emit(c, 0x48);
	emit(c, 0x39);
	emit(c, 0xca);
	// setne al
	emit(c, 0x0f);
	emit(c, 0x95);
	emit(c, 0xc0);
	// mov rax
	emit(c, 0x50);
}

emit_sub(c: *compiler): void {
	// pop rax
	emit(c, 0x58);
	// pop rdx
	emit(c, 0x5a);
	// add rax, rdx
	emit(c, 0x48);
	emit(c, 0x29);
	emit(c, 0xd0);
	// push rax
	emit(c, 0x50);
}

emit_mul(c: *compiler): void {
	// pop rax
	emit(c, 0x58);
	// pop rcx
	emit(c, 0x59);
	// mul rcx
	emit(c, 0x48);
	emit(c, 0xf7);
	emit(c, 0xe1);
	// push rax
	emit(c, 0x50);
}

emit_div(c: *compiler): void {
	// pop rax
	emit(c, 0x58);
	// pop rcx
	emit(c, 0x59);
	// xor rdx, rdx
	emit(c, 0x48);
	emit(c, 0x31);
	emit(c, 0xd2);
	// test rax, rax
	emit(c, 0x48);
	emit(c, 0x85);
	emit(c, 0xc0);
	// sets dl
	emit(c, 0x0f);
	emit(c, 0x98);
	emit(c, 0xc2);
	// neg rdx
	emit(c, 0x48);
	emit(c, 0xf7);
	emit(c, 0xda);
	// idiv rcx
	emit(c, 0x48);
	emit(c, 0xf7);
	emit(c, 0xf9);
	// push rax
	emit(c, 0x50);
}

emit_mod(c: *compiler): void {
        // pop rax
        emit(c, 0x58);
        // pop rcx
        emit(c, 0x59);
        // xor rdx, rdx
        emit(c, 0x48);
        emit(c, 0x31);
        emit(c, 0xd2);
        // test rax, rax
        emit(c, 0x48);
        emit(c, 0x85);
        emit(c, 0xc0);
        // sets dl
        emit(c, 0x0f);
        emit(c, 0x98);
        emit(c, 0xc2);
        // neg rdx
        emit(c, 0x48);
        emit(c, 0xf7);
        emit(c, 0xda);
        // idiv rcx
        emit(c, 0x48);
        emit(c, 0xf7);
        emit(c, 0xf9);
        // push rdx
        emit(c, 0x52);
}

emit_lsh(c: *compiler): void {
	// pop rax
	emit(c, 0x58);
	// pop rcx
	emit(c, 0x59);
	// shl rax, cl
	emit(c, 0x48);
	emit(c, 0xd3);
	emit(c, 0xe0);
	// push rax
	emit(c, 0x50);
}

emit_rsh(c: *compiler): void {
	// pop rax
	emit(c, 0x58);
	// pop rcx
	emit(c, 0x59);
	// shr rax, cl
	emit(c, 0x48);
	emit(c, 0xd3);
	emit(c, 0xe8);
	// push rax
	emit(c, 0x50);
}

emit_not(c: *compiler): void {
	// pop rax
	emit(c, 0x58);
	// neg rax
	emit(c, 0x48);
	emit(c, 0xf7);
	emit(c, 0xd0);
	// push rax
	emit(c, 0x50);
}

emit_neg(c: *compiler): void {
	// pop rax
	emit(c, 0x58);
	// neg rax
	emit(c, 0x48);
	emit(c, 0xf7);
	emit(c, 0xd8);
	// push rax
	emit(c, 0x50);
}

gen_builtins(c: *compiler): void {
	var d: *decl;

	d = find(c, "syscall", 0:*byte, 1);
	if (d.func_defined && !d.func_label.fixed) {
		d.func_defined = 1;
		fixup_label(c, d.func_label);
		// push rbp
		emit(c, 0x55);
		// mov rbp, rsp
		emit(c, 0x48);
		emit(c, 0x89);
		emit(c, 0xe5);
		// mov rax, [rbp + 16]
		emit(c, 0x48);
		emit(c, 0x8b);
		emit(c, 0x45);
		emit(c, 0x10);
		// mov rdi, [rbp + 24]
		emit(c, 0x48);
		emit(c, 0x8b);
		emit(c, 0x7d);
		emit(c, 0x18);
		// mov rsi, [rbp + 32]
		emit(c, 0x48);
		emit(c, 0x8b);
		emit(c, 0x75);
		emit(c, 0x20);
		// mov rdx, [rbp + 40]
		emit(c, 0x48);
		emit(c, 0x8b);
		emit(c, 0x55);
		emit(c, 0x28);
		// mov r10, [rbp + 48]
		emit(c, 0x4c);
		emit(c, 0x8b);
		emit(c, 0x55);
		emit(c, 0x30);
		// mov r8, [rbp + 56]
		emit(c, 0x4c);
		emit(c, 0x8b);
		emit(c, 0x45);
		emit(c, 0x38);
		// mov r9, [rbp + 64]
		emit(c, 0x4c);
		emit(c, 0x8b);
		emit(c, 0x4d);
		emit(c, 0x40);
		// syscall
		emit(c, 0x0f);
		emit(c, 0x05);
		// pop rbp
		emit(c, 0x5d);
		// ret
		emit(c, 0xc3);
	}
}

writeout(c: *compiler): void {
	var b: *chunk;
	var i: int;
	var text_size: int;
	var load_addr: int;
	var entry: int;
	var d: *decl;

	load_addr = 0x100000;
	text_size = c.at;

	d = find(c, "_start", 0:*byte, 0);
	if (!d || !d.func_defined || !d.func_label.fixed) {
		die(c, "_start is not defined");
	}

	entry = load_addr + d.func_label.at + 128;
	text_size = text_size + 128;

	// magic
	putchar(0x7f);
	putchar('E');
	putchar('L');
	putchar('F');

	// class
	putchar(2);

	// endian
	putchar(1);

	// version
	putchar(1);

	// abi
	putchar(0);

	// abi version
	putchar(0);

	// padding
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);

	// type
	putchar(2);
	putchar(0);

	// machine
	putchar(62);
	putchar(0);

	// version
	putchar(1);
	putchar(0);
	putchar(0);
	putchar(0);

	// entry point
	putchar(entry);
	putchar(entry >> 8);
	putchar(entry >> 16);
	putchar(entry >> 24);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);

	// phoff
	putchar(64);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);

	// shoff
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);

	// flags
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);

	// ehsize
	putchar(64);
	putchar(0);

	// phentsize
	putchar(56);
	putchar(0);

	// phnum
	putchar(1);
	putchar(0);

	// shentsize
	putchar(64);
	putchar(0);

	// shnum
	putchar(0);
	putchar(0);

	// shstrndx
	putchar(0);
	putchar(0);

	// phdr[0].type
	putchar(1);
	putchar(0);
	putchar(0);
	putchar(0);

	// phdr[0].flags
	putchar(5);
	putchar(0);
	putchar(0);
	putchar(0);

	// phdr[0].offset
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);

	// phdr[0].vaddr
	putchar(0);
	putchar(0);
	putchar(0x10);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);

	// phdr[0].paddr
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);

	// phdr[0].filesize
	putchar(text_size);
	putchar(text_size >> 8);
	putchar(text_size >> 16);
	putchar(text_size >> 24);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);

	// phdr[0].memsize
	putchar(text_size);
	putchar(text_size >> 8);
	putchar(text_size >> 16);
	putchar(text_size >> 24);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);

	// phdr[0].align
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);

	// nop sled
	putchar(0x90);
	putchar(0x90);
	putchar(0x90);
	putchar(0x90);
	putchar(0x90);
	putchar(0x90);
	putchar(0x90);
	putchar(0x90);

	b = c.text;
	loop {
		if (!b) {
			break;
		}
		i = 0;
		loop {
			if (i >= b.fill) {
				break;
			}
			putchar(b.buf[i]: int);
			i = i + 1;
		}
		b = b.next;
	}
}

main(): void {
	var c: compiler;
	var p: *node;
	comp_setup(&c);
	p = parse_program(&c);
	compile(&c, p);
	gen_builtins(&c);
	writeout(&c);
}
