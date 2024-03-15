#include <stdlib.h>
#include <stdio.h>

///////////////////////////////////////////////////////////////////////////////
// Forward Declarations                                                      //
///////////////////////////////////////////////////////////////////////////////

int lineno;
struct node;
struct type;
struct label;
struct fix;
struct sdecl;

struct node *type(void);
struct node *stmt_list(void);
struct node *expr(void);
struct label *mklabel(void);
void compute_struct(struct sdecl *s);
int type_sizeof(struct type *t);
void addfixup(struct label *l);

///////////////////////////////////////////////////////////////////////////////
// Helpers                                                                   //
///////////////////////////////////////////////////////////////////////////////

void
die(char *msg)
{
	fprintf(stderr, "die(%d): %s\n", lineno, msg);
	exit(1);
}

// strlen-ish
int
slen(unsigned char *s)
{
	int i;

	i = 0;
	while (1) {
		if (s[i] == 0) {
			return i;
		}

		i = i + 1;
	}
}

// strcmp-ish
int
cmp(unsigned char *a, unsigned char *b)
{
	int i;

	i = 0;
	while (1) {
		if (a[i] > b[i]) {
			return 1;
		}

		if (a[i] < b[i]) {
			return -1;
		}

		if (a[i] == 0) {
			return 0;
		}

		i = i + 1;
	}
}

///////////////////////////////////////////////////////////////////////////////
// Lexer                                                                     //
///////////////////////////////////////////////////////////////////////////////

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
};

// The token from the source
unsigned char token[4096];

// Token type
int tt;

// Token length
int tlen;

// Single character lookahead
int nc;

// Line number of the start of the current token
int lineno;

// Read in an identifier
void
feed_ident(void)
{
	tt = T_IDENT;
	while (1) {
		if (!((nc >= 'a' && nc <= 'z') ||
				(nc >= 'A' && nc <= 'Z') ||
				(nc >= '0' && nc <= '9') ||
				nc == '_')) {
			break;
		}
		if (tlen == sizeof(token) - 1) {
			die("identifier too long");
		}
		token[tlen] = nc;
		tlen = tlen + 1;
		nc = getchar();
	}
	token[tlen] = 0;
}

// Parse a hex digit
int
hexdig(int c)
{
	if (c >= '0' && c <= '9') {
		return c - '0';
	}

	if (c >= 'A' && c <= 'F') {
		return c - 'F' + 10;
	}

	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}

	die("invalid hex digit");
	return 0;
}

// Parse an escape and replace nc with the value
void
feed_escape(void)
{
	int hex;

	nc = getchar();
	if (nc == 't') {
		nc = '\t';
	} else if (nc == 'r') {
		nc = '\r';
	} else if (nc == 'n') {
		nc = '\n';
	} else if (nc == 'x') {
		nc = getchar();
		hex = hexdig(nc) * 16;

		nc = getchar();
		hex = hex + hexdig(nc);

		nc = hex;
	} else if (nc != '\\' && nc != '\'' && nc != '"') {
		die("invalid escape");
	}
}

// Read in a string
void
feed_str(void)
{
	tt = T_STR;
	nc = getchar();
	while (1) {
		if (nc == '"') {
			token[tlen] = 0;
			nc = getchar();
			return;
		}

		if (nc == EOF) {
			die("eof in string literal");
		}

		if (nc == 0 || nc == '\n') {
			die("invalid char in string");
		}

		if (tlen == sizeof(token) - 1) {
			die("string too long");
		}

		if (nc == '\\') {
			feed_escape();
		}

		token[tlen] = nc;
		tlen = tlen + 1;
		nc = getchar();
	}
}

// Read in a character
void
feed_char(void)
{
	tt = T_CHAR;
	nc = getchar();

	if (nc == 0 || nc == EOF || nc == '\'' || nc == '\n') {
		die("invalid char");
	}

	if (nc == '\\') {
		feed_escape();
	}

	token[tlen] = nc;
	tlen = tlen + 1;
	nc = getchar();

	if (nc != '\'') {
		die("expected '");
	}

	token[tlen] = 0;
	nc = getchar();
}

// Read a hex number
void
feed_hex(void)
{
	tt = T_HEX;

	tlen = 0;
	while (1) {
		if (!((nc >= '0' && nc <= '9')
				|| (nc >= 'a' && nc <= 'f')
				|| (nc >= 'A' && nc <= 'F'))) {
			break;
		}
		if (tlen == sizeof(token) - 1) {
			die("identifier too long");
		}
		token[tlen] = nc;
		tlen = tlen + 1;
		nc = getchar();
	}

	if (!tlen) {
		die("expected hex");
	}

	token[tlen] = 0;
}

// Read in a number
void
feed_num(void)
{
	tt = T_NUM;

	if (nc == '0') {
		token[tlen] = nc;
		tlen = tlen + 1;
		nc = getchar();

		if (nc == 'x') {
			nc = getchar();
			feed_hex();
			return;
		}
	}

	while (1) {
		if (!(nc >= '0' && nc <= '9')) {
			break;
		}
		if (tlen == sizeof(token) - 1) {
			die("identifier too long");
		}
		token[tlen] = nc;
		tlen = tlen + 1;
		nc = getchar();
	}
	token[tlen] = 0;
}

// Read in the next token
void
feed(void)
{
	tlen = 0;

	// Loop invariant: possible whitespace or EOF
	while (1) {
		if (nc == EOF) {
			// Reached the end of input
			tt = T_EOF;
			return;
		} else if (nc == ' ' || nc == '\t' || nc == '\r') {
			// Whitespace
			nc = getchar();
		} else if (nc == '\n') {
			// Line end
			nc = getchar();
			lineno = lineno + 1;
		} else if (nc == '/') {
			// Comment
			nc = getchar();
			if (nc == '/') {
				// Read until the end of the comment
				while (1) {
					if (nc == '\n' || nc == EOF) {
						break;
					}
					nc = getchar();
				}
			} else {
				tt = T_DIV;
				return;
			}
		} else {
			// Start of a real token
			break;
		}
	}

	// Identifier
	if ((nc >= 'a' && nc <= 'z') || (nc >= 'A' && nc <= 'Z') || nc == '_') {
		feed_ident();
		return;
	}

	// String
	if (nc == '"') {
		feed_str();
		return;
	}

	// Character
	if (nc == '\'') {
		feed_char();
		return;
	}

	// Number
	if (nc >= '0' && nc <= '9') {
		feed_num();
		return;
	}

	// Single character tokens
	if (nc == '(') {
		tt = T_LPAR;
		nc = getchar();
	} else if (nc == ')') {
		tt = T_RPAR;
		nc = getchar();
	} else if (nc == '{') {
		tt = T_LBRA;
		nc = getchar();
	} else if (nc == '}') {
		tt = T_RBRA;
		nc = getchar();
	} else if (nc == ',') {
		tt = T_COMMA;
		nc = getchar();
	} else if (nc == ';') {
		tt = T_SEMI;
		nc = getchar();
	} else if (nc == ':') {
		tt = T_COLON;
		nc = getchar();
	} else if (nc == '*') {
		tt = T_STAR;
		nc = getchar();
	} else if (nc == '.') {
		tt = T_DOT;
		nc = getchar();
	} else if (nc == '=') {
		tt = T_ASSIGN;
		nc = getchar();
		if (nc == '=') {
			tt = T_EQ;
			nc = getchar();
		}
	} else if (nc == '&') {
		tt = T_AMP;
		nc = getchar();
		if (nc == '&') {
			tt = T_BAND;
			nc = getchar();
		}
	} else if (nc == '~') {
		tt = T_NOT;
		nc = getchar();
	} else if (nc == '|') {
		tt = T_OR;
		nc = getchar();
		if (nc == '|') {
			tt = T_BOR;
			nc = getchar();
		}
	} else if (nc == '^') {
		tt = T_XOR;
		nc = getchar();
	} else if (nc == '!') {
		tt = T_BANG;
		nc = getchar();
		if (nc == '=') {
			tt = T_NE;
			nc = getchar();
		}
	} else if (nc == '<') {
		tt = T_LT;
		nc = getchar();
		if (nc == '<') {
			tt = T_LSH;
			nc = getchar();
		} else if (nc == '=') {
			tt = T_LE;
			nc = getchar();
		}
	} else if (nc == '>') {
		tt = T_GT;
		nc = getchar();
		if (nc == '>') {
			tt = T_RSH;
			nc = getchar();
		} else if (nc == '=') {
			tt = T_GE;
			nc = getchar();
		}
	} else if (nc == '[') {
		tt = T_LSQ;
		nc = getchar();
	} else if (nc == ']') {
		tt = T_RSQ;
		nc = getchar();
	} else if (nc == '+') {
		tt = T_ADD;
		nc = getchar();
	} else if (nc == '-') {
		tt = T_SUB;
		nc = getchar();
	} else if (nc == '%') {
		tt = T_MOD;
		nc = getchar();
	} else {
		die("invalid char");
	}
}

///////////////////////////////////////////////////////////////////////////////
// Grammar                                                                   //
///////////////////////////////////////////////////////////////////////////////

struct node {
	int kind;
	struct node *a;
	struct node *b;
	struct type *t;
	int n;
	unsigned char *s;
	int offset;
	int lineno;
};

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
};

// Construct a node
struct node *
mknode(int kind, struct node *a, struct node *b)
{
	struct node *n;

	n = malloc(sizeof(*n));
	if (!n) {
		die("out of memory");
	}

	n->kind = kind;
	n->a = a;
	n->b = b;
	n->t = 0;
	n->n = 0;
	n->s = 0;
	n->offset = 0;
	n->lineno = lineno;

	return n;
}

// Copy the current token
unsigned char *
intern(void)
{
	unsigned char *ret;
	int i;

	ret = malloc(tlen + 1);
	if (!ret) {
		return 0;
	}

	i = 0;
	while (1) {
		if (i >= tlen) {
			break;
		}
		ret[i] = token[i];
		i = i + 1;
	}
	ret[i] = 0;

	return ret;
}

// ident := IDENT
struct node *
ident(void)
{
	struct node *n;

	if (tt != T_IDENT) {
		return 0;
	}

	n = mknode(N_IDENT, 0, 0);
	n->s = intern();
	feed();

	return n;
}

struct node *
hex(void)
{
	struct node *n;
	long x;
	int i;
	int d;

	x = 0;
	i = 0;
	while (1) {
		if (i >= tlen) {
			break;
		}

		d = token[i];

		if (d >= '0' && d <= '9') {
			d = d - '0';
		} else if (d >= 'a' && d <= 'f') {
			d = d - 'a' + 10;
		} else {
			d = d - 'A' + 10;
		}

		x = x * 16;
		x = x + d;
		i = i + 1;

		if (x > 0x7fffffff) {
			die("overflow");
		}
	}

	n = mknode(N_NUM, 0, 0);
	n->n = x;
	feed();

	return n;
}

// num := NUM
struct node *
num(void)
{
	struct node *n;
	long x;
	int d;
	int i;

	if (tt == T_HEX) {
		return hex();
	}

	if (tt != T_NUM) {
		return 0;
	}

	x = 0;
	i = 0;
	while (1) {
		if (i >= tlen) {
			break;
		}

		d = token[i] - '0';

		x = x * 10;

		x = x + d;
		i = i + 1;

		if (x > 0x7fffffff) {
			die("overflow");
		}
	}

	n = mknode(N_NUM, 0, 0);
	n->n = x;
	feed();

	return n;
}

// chr := CHAR
struct node *
chr(void)
{
	struct node *n;

	if (tt != T_CHAR) {
		return 0;
	}

	n = mknode(N_CHAR, 0, 0);
	n->n = token[0];
	feed();

	return n;
}

// str := STR
struct node *
str(void)
{
	struct node *n;

	if (tt != T_STR) {
		return 0;
	}

	n = mknode(N_STR, 0, 0);
	n->s = intern();
	feed();

	return n;
}

// primary := ident
//          | num
//          | str
//          | chr
//          | '(' expr ')'
struct node *
primary(void)
{
	struct node *n;

	n = ident();
	if (n) {
		return n;
	}

	n = num();
	if (n) {
		return n;
	}

	n = str();
	if (n) {
		return n;
	}

	n = chr();
	if (n) {
		return n;
	}

	if (tt == T_LPAR) {
		feed();

		n = expr();
		if (!n) {
			die("expected expr");
		}

		if (tt != T_RPAR) {
			die("expected )");
		}
		feed();

		return n;
	}

	return 0;
}

// expr_list := expr
//            | expr ',' expr_list
struct node *
expr_list(void)
{
	struct node *n;
	struct node *e;
	struct node *a;

	a = expr();
	if (!a) {
		return 0;
	}

	e = mknode(N_EXPRLIST, a, 0);
	n = e;

	while (1) {
		if (tt != T_COMMA) {
			return n;
		}
		feed();

		a = expr();
		if (!a) {
			die("expected expr");
		}

		e->b = mknode(N_EXPRLIST, a, 0);
		e = e->b;
	}
}

// post_expr := primary
//            | 'sizeof' '(' expr ')'
//            | post_expr '[' expr ']'
//            | post_expr '(' expr_list ')'
//            | post_expr '.' ident
//            | post_expr ':' type
struct node *
post_expr(void)
{
	struct node *n;
	struct node *b;

	if (tt == T_IDENT && !cmp(token, (unsigned char *)"sizeof")) {
		feed();

		if (tt != T_LPAR) {
			die("expected (");
		}
		feed();

		n = expr();
		if (!n) {
			die("expected expr");
		}

		if (tt != T_RPAR) {
			die("expected )");
		}
		feed();

		return mknode(N_SIZEOF, n, 0);;
	}

	n = primary();
	if (!n) {
		return 0;
	}

	while (1) {
		if (tt == T_LSQ) {
			feed();

			b = expr();

			if (tt != T_RSQ) {
				die("expected ]");
			}
			feed();

			n = mknode(N_INDEX, n, b);
		} else if (tt == T_LPAR) {
			feed();

			b = expr_list();

			if (tt != T_RPAR) {
				die("expected )");
			}
			feed();

			n  = mknode(N_CALL, n, b);
		} else if (tt == T_DOT) {
			feed();

			b = ident();
			if (!b) {
				die("expected ident");
			}

			n = mknode(N_DOT, n, b);
		} else if (tt == T_COLON) {
			feed();

			b = type();
			if (!b) {
				die("expected type");
			}

			n = mknode(N_CAST, n, b);
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
struct node *
unary_expr(void)
{
	struct node *n;

	if (tt == T_AMP) {
		feed();

		n = unary_expr();
		if (!n) {
			die("expected unary_expr");
		}

		return mknode(N_REF, n, 0);
	}

	if (tt == T_STAR) {
		feed();

		n = unary_expr();
		if (!n) {
			die("expected unary_expr");
		}

		return mknode(N_DEREF, n, 0);
	}

	if (tt == T_ADD) {
		feed();

		n = unary_expr();
		if (!n) {
			die("expected unary_expr");
		}

		return mknode(N_POS, n, 0);
	}

	if (tt == T_SUB) {
		feed();

		n = unary_expr();
		if (!n) {
			die("expected unary_expr");
		}

		return mknode(N_NEG, n, 0);
	}

	if (tt == T_NOT) {
		feed();

		n = unary_expr();
		if (!n) {
			die("expected unary_expr");
		}

		return mknode(N_NOT, n, 0);
	}

	if (tt == T_BANG) {
		feed();

		n = unary_expr();
		if (!n) {
			die("expected unary_expr");
		}

		return mknode(N_BNOT, n, 0);
	}

	return post_expr();
}


// shift_expr := unary_expr
//             | shift_expr '<<' unary_expr
//             | shift_expr '>>' unary_expr
struct node *
shift_expr(void)
{
	struct node *a;
	struct node *b;

	a = unary_expr();
	if (!a) {
		return 0;
	}

	while (1) {
		if (tt == T_LSH) {
			feed();

			b = unary_expr();
			if (!b) {
				die("expected unary_expr");
			}

			a = mknode(N_LSH, a, b);
		} else if (tt == T_RSH) {
			feed();

			b = unary_expr();
			if (!b) {
				die("expected unary_expr");
			}

			a = mknode(N_RSH, a, b);
		} else {
			return a;
		}
	}
}

// mul_expr := shift_expr
//           | mul_expr '*' shift_expr
//           | mul_expr '/' shift_expr
//           | mul_expr '%' shift_expr
//           | mul_expr '&' shift_expr
struct node *
mul_expr(void)
{
	struct node *a;
	struct node *b;

	a = shift_expr();
	if (!a) {
		return 0;
	}

	while (1) {
		if (tt == T_STAR) {
			feed();

			b = shift_expr();
			if (!b) {
				die("expected shift_expr");
			}

			a = mknode(N_MUL, a, b);
		} else if (tt == T_DIV) {
			feed();

			b = shift_expr();
			if (!b) {
				die("expected shift_expr");
			}

			a = mknode(N_DIV, a, b);
		} else if (tt == T_MOD) {
			feed();

			b = shift_expr();
			if (!b) {
				die("expected shift_expr");
			}

			a = mknode(N_MOD, a, b);
		} else if (tt == T_AMP) {
			feed();

			b = shift_expr();
			if (!b) {
				die("expected shift_expr");
			}

			a = mknode(N_AND, a, b);
		} else {
			return a;
		}
	}
}

// add_expr := mul_expr
//           | add_expr '+' mul_expr
//           | add_expr '-' mul_expr
//           | add_expr '|' mul_expr
//           | add_expr '^' mul_expr
struct node *
add_expr(void)
{
	struct node *a;
	struct node *b;

	a = mul_expr();
	if (!a) {
		return 0;
	}

	while (1) {
		if (tt == T_ADD) {
			feed();

			b = mul_expr();
			if (!b) {
				die("expected mul_expr");
			}

			a = mknode(N_ADD, a, b);
		} else if (tt == T_SUB) {
			feed();

			b = mul_expr();
			if (!b) {
				die("expected mul_expr");
			}

			a = mknode(N_SUB, a, b);
		} else if (tt == T_OR) {
			feed();

			b = mul_expr();
			if (!b) {
				die("expected mul_expr");
			}

			a = mknode(N_OR, a, b);
		} else if (tt == T_XOR) {
			feed();

			b = mul_expr();
			if (!b) {
				die("expected mul_expr");
			}

			a = mknode(N_XOR, a, b);
		} else {
			return a;
		}
	}
}

// comp_expr := add_expr
//            | add_expr '<' add_expr
//            | add_expr '>' add_expr
//            | add_expr '<=' add_expr
//            | add_expr '>=' add_expr
//            | add_expr '==' add_expr
//            | add_expr '!=' add_expr
struct node *
comp_expr(void)
{
	struct node *a;
	struct node *b;

	a = add_expr();
	if (!a) {
		return 0;
	}

	if (tt == T_LT) {
		feed();

		b = add_expr();
		if (!b) {
			die("expected add_expr");
		}

		return mknode(N_LT, a, b);
	}

	if (tt == T_GT) {
		feed();

		b = add_expr();
		if (!b) {
			die("expected add_expr");
		}

		return mknode(N_GT, a, b);
	}

	if (tt == T_LE) {
		feed();

		b = add_expr();
		if (!b) {
			die("expected add_expr");
		}

		return mknode(N_LE, a, b);
	}

	if (tt == T_GE) {
		feed();

		b = add_expr();
		if (!b) {
			die("expected add_expr");
		}

		return mknode(N_GE, a, b);
	}

	if (tt == T_EQ) {
		feed();

		b = add_expr();
		if (!b) {
			die("expected add_expr");
		}

		return mknode(N_EQ, a, b);
	}

	if (tt == T_NE) {
		feed();

		b = add_expr();
		if (!b) {
			die("expected add_expr");
		}

		return mknode(N_NE, a, b);
	}

	return a;
}

// bool_expr := bool_expr
//            | comp_expr '&&' bool_expr
//            | comp_expr '||' bool_expr
struct node *
bool_expr(void)
{
	struct node *a;
	struct node *b;

	a = comp_expr();
	if (!a) {
		return 0;
	}

	if (tt == T_BAND) {
		feed();

		b = bool_expr();
		if (!b) {
			die("expected bool_expr");
		}

		return mknode(N_BAND, a, b);
	}

	if (tt == T_BOR) {
		feed();

		b = bool_expr();
		if (!b) {
			die("expected bool_expr");
		}

		return mknode(N_BOR, a, b);
	}

	return a;
}

// expr := bool_expr
//       | bool_expr '=' expr
struct node *
expr(void)
{
	struct node *a;
	struct node *b;

	a = bool_expr();
	if (!a) {
		return 0;
	}

	if (tt == T_ASSIGN) {
		feed();

		b = expr();
		if (!b) {
			die("expected expr");
		}

		return mknode(N_ASSIGN, a, b);
	}

	return a;
}

// if_stmt := 'if' expr '{' stmt_list '}'
//          | 'if' expr '{' stmt_list '}' 'else' if_stmt
//          | 'if' expr '{' stmt_list '}' 'else' '{' stmt_list '}'
struct node *
if_stmt(void)
{
	struct node *n;
	struct node *e;
	struct node *a;
	struct node *b;

	if (tt != T_IDENT || cmp(token, (unsigned char *)"if")) {
		return 0;
	}
	feed();

	n = mknode(N_CONDLIST, 0, 0);
	e = n;

	while (1) {
		a = expr();
		if (!a) {
			die("expected expr");
		}

		if (tt != T_LBRA) {
			die("expected {");
		}
		feed();

		b = stmt_list();

		if (tt != T_RBRA) {
			die("expected }");
		}
		feed();

		e->a = mknode(N_COND, a, b);

		if (tt != T_IDENT || cmp(token, (unsigned char *)"else")) {
			return n;
		}
		feed();

		if (tt == T_LBRA) {
			feed();

			b = stmt_list();

			if (tt != T_RBRA) {
				die("expected }");
			}
			feed();

			e->b = mknode(N_CONDLIST, mknode(N_COND, 0, b), 0);

			return n;
		}

		if (tt != T_IDENT || cmp(token, (unsigned char *)"if")) {
			die("expected if");
		}
		feed();

		e->b = mknode(N_CONDLIST, 0, 0);
		e = e->b;
	}
}

// loop_stmt := 'loop' '{' stmt_list '}'
struct node *
loop_stmt(void)
{
	struct node *a;

	if (tt != T_IDENT || cmp(token, (unsigned char *)"loop")) {
		return 0;
	}
	feed();

	if (tt != T_LBRA) {
		die("expected {");
	}
	feed();

	a = stmt_list();

	if (tt != T_RBRA) {
		die("expected }");
	}
	feed();

	return mknode(N_LOOP, a, 0);
}

// break_stmt := 'break'
struct node *
break_stmt(void)
{
	if (tt != T_IDENT || cmp(token, (unsigned char *)"break")) {
		return 0;
	}
	feed();

	return mknode(N_BREAK, 0, 0);
}

// continue_stmt := 'continue'
struct node *
continue_stmt(void)
{
	if (tt != T_IDENT || cmp(token, (unsigned char *)"continue")) {
		return 0;
	}
	feed();

	return mknode(N_CONTINUE, 0, 0);
}

// return_stmt := 'return'
//              | 'return' expr
struct node *
return_stmt(void)
{
	struct node *a;

	if (tt != T_IDENT || cmp(token, (unsigned char *)"return")) {
		return 0;
	}
	feed();

	a = expr();

	return mknode(N_RETURN, a, 0);
}

// var_decl := ident ':' type
struct node *
var_decl(void)
{
	struct node *n;
	struct node *a;
	struct node *b;

	a = ident();
	if (!a) {
		return 0;
	}

	if (tt != T_COLON) {
		die("expected :");
	}
	feed();

	b = type();
	if (!b) {
		die("expected type");
	}

	return mknode(N_VARDECL, a, b);
}

// var_stmt := 'var' var_decl
struct node *
var_stmt(void)
{
	struct node *a;

	if (tt != T_IDENT || cmp(token, (unsigned char *)"var")) {
		return 0;
	}
	feed();

	a = var_decl();
	if (!a) {
		die("expected var_decl");
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
struct node *
stmt(void)
{
	struct node *n;

	n = if_stmt();
	if (n) {
		return n;
	}

	n = loop_stmt();
	if (n) {
		return n;
	}

	n = break_stmt();
	if (n) {
		if (tt != T_SEMI) {
			die("expected ;");
		}
		feed();

		return n;
	}

	n = return_stmt();
	if (n) {
		if (tt != T_SEMI) {
			die("expected ;");
		}
		feed();

		return n;
	}

	n = continue_stmt();
	if (n) {
		if (tt != T_SEMI) {
			die("expected ;");
		}
		feed();

		return n;
	}

	n = var_stmt();
	if (n) {
		if (tt != T_SEMI) {
			die("expected ;");
		}
		feed();

		return n;
	}

	n = expr();
	if (n) {
		if (tt != T_SEMI) {
			die("expected ;");
		}
		feed();

		return n;
	}

	return 0;
}

// stmt_list := stmt
//            | stmt stmt_list
struct node *
stmt_list(void)
{
	struct node *n;
	struct node *e;
	struct node *a;

	a = stmt();
	if (!a) {
		return 0;
	}

	e = mknode(N_STMTLIST, a, 0);
	n = e;

	while (1) {
		a = stmt();
		if (!a) {
			return n;
		}

		e->b = mknode(N_STMTLIST, a, 0);
		e = e->b;
	}
}

// type := ident
//       | '*' type
//       | '(' type ')'
struct node *
type(void)
{
	struct node *n;

	if (tt == T_STAR) {
		feed();

		n = type();

		return mknode(N_PTRTYPE, n, 0);
	}

	if (tt == T_LPAR) {
		feed();

		n = type();

		if (tt != T_RPAR) {
			die("expected )");
		}
		feed();

		return n;
	}

	return ident();
}

// arg_decl := ':' type
//             ident ':' type
struct node *
arg_decl(void)
{
	struct node *n;
	struct node *a;
	struct node *b;

	a = ident();
	if (!a) {
		return 0;
	}

	if (tt != T_COLON) {
		die("expected :");
	}
	feed();

	b = type();
	if (!b) {
		die("expected type");
	}

	return mknode(N_ARGDECL, a, b);
}

// arg_list := arg_decl
//           | arg_decl ',' arg_list
struct node *
arg_list(void)
{
	struct node *n;
	struct node *e;
	struct node *a;

	a = arg_decl();
	if (!a) {
		return 0;
	}

	e = mknode(N_ARGLIST, a, 0);
	n = e;

	while (1) {
		if (tt != T_COMMA) {
			return n;
		}
		feed();

		a = arg_decl();
		if (!a) {
			die("expected identifier");
		}

		e->b = mknode(N_ARGLIST, a, 0);
		e = e->b;
	}
}

// func_decl := ident '(' ')' ':' type
//            | ident '(' arg_list ')' ':' type
struct node *
func_decl(void)
{
	struct node *a;
	struct node *b;
	struct node *c;

	a = ident();
	if (!a) {
		return 0;
	}

	if (tt != T_LPAR) {
		die("expected (");
	}
	feed();

	b = arg_list();

	if (tt != T_RPAR) {
		die("expected )");
	}
	feed();

	if (tt != T_COLON) {
		die("expected :");
	}
	feed();

	c = type();
	if (!c) {
		die("expected type");
	}

	return mknode(N_FUNCDECL, a, mknode(N_FUNCTYPE, b, c));
}


// func := func_decl '{' stmt_list '}'
//       | func_decl ';'
struct node *
func(void)
{
	struct node *n;
	struct node *a;
	struct node *b;

	a = func_decl();
	if (!a) {
		return 0;
	}

	if (tt == T_SEMI) {
		feed();
		return a;
	}

	if (tt != T_LBRA) {
		die("expected {");
	}
	feed();

	b = stmt_list();

	if (tt != T_RBRA) {
		die("expected }");
	}
	feed();

	return mknode(N_FUNC, a, b);
}

// member_decl := ident ':' type
struct node *
member_decl(void)
{
	struct node *n;
	struct node *a;
	struct node *b;

	a = ident();
	if (!a) {
		return 0;
	}

	if (tt != T_COLON) {
		die("expected :");
	}
	feed();

	b = type();
	if (!b) {
		die("expected type");
	}

	return mknode(N_MEMBERDECL, a, b);
}

// enum_list := ident
//            | enum_list ',' enum_list
struct node *
enum_list(void)
{
	struct node *n;
	struct node *e;
	struct node *a;

	a = ident();
	if (!a) {
		return 0;
	}

	e = mknode(N_ENUMLIST, a, 0);
	n = e;

	while (1) {
		if (tt != T_COMMA) {
			return n;
		}
		feed();

		a = ident();
		if (!a) {
			return n;
		}

		e->b = mknode(N_ENUMLIST, a, 0);
		e = e->b;
	}
}

// enum_decl := 'enum' ident '{' enum_list '}'
struct node *
enum_decl(void)
{
	struct node *b;

	if (tt != T_IDENT) {
		return 0;
	}

	if (cmp(token, (unsigned char *)"enum")) {
		return 0;
	}
	feed();

	if (tt != T_LBRA) {
		die("expected {");
	}
	feed();

	b = enum_list();

	if (tt != T_RBRA) {
		die("expected }");
	}
	feed();

	return mknode(N_ENUM, 0, b);
}

// member_list := member_decl
//              | member_decl ';' member_list
struct node *
member_list(void)
{
	struct node *n;
	struct node *e;
	struct node *a;

	a = member_decl();
	if (!a) {
		return 0;
	}

	e = mknode(N_MEMBERLIST, a, 0);
	n = e;

	while (1) {
		if (tt != T_SEMI) {
			return n;
		}
		feed();

		a = member_decl();
		if (!a) {
			return n;
		}

		e->b = mknode(N_MEMBERLIST, a, 0);
		e = e->b;
	}
}

// struct_decl := 'struct' ident '{' member_list '}'
struct node *
struct_decl(void)
{
	struct node *a;
	struct node *b;

	if (tt != T_IDENT) {
		return 0;
	}

	if (cmp(token, (unsigned char *)"struct")) {
		return 0;
	}
	feed();

	a = ident();
	if (!a) {
		die("expected ident");
	}

	if (tt != T_LBRA) {
		die("expected {");
	}
	feed();

	b = member_list();

	if (tt != T_RBRA) {
		die("expected }");
	}
	feed();

	return mknode(N_STRUCT, a, b);
}

// decl := func
//       | struct_decl
struct node *
decl(void)
{
	struct node *n;

	n = enum_decl();
	if (n) {
		return n;
	}

	n = struct_decl();
	if (n) {
		return n;
	}

	return func();
}

// program := func
//          | func program
struct node *
program(void)
{
	struct node *n;
	struct node *e;
	struct node *d;

	d = decl();
	if (!d) {
		return 0;
	}

	e = mknode(N_PROGRAM, d, 0);
	n = e;

	while (1) {
		d = decl();
		if (!d) {
			if (tt != T_EOF) {
				die("expected eof");
			}

			return n;
		}

		e->b = mknode(N_PROGRAM, d, 0);
		e = e->b;
	}
}

///////////////////////////////////////////////////////////////////////////////
// Setup                                                                     //
///////////////////////////////////////////////////////////////////////////////

// Prime the parser by reading in the lookahead
void
setup(void)
{
	lineno = 1;
	nc = getchar();
	feed();
}

///////////////////////////////////////////////////////////////////////////////
// Type System                                                               //
///////////////////////////////////////////////////////////////////////////////

enum {
	TY_VOID,
	TY_INT,
	TY_BYTE,
	TY_PTR,
	TY_ARG,
	TY_FUNC,
	TY_STRUCT,
};

struct type {
	int kind;
	struct sdecl *s;
	struct type *val;
	struct type *arg;
};

struct decl {
	unsigned char *name;
	struct decl *p;
	struct decl *l;
	struct decl *r;
	struct type *ftype;
	struct node *args;
	struct node *body;
	struct label *label;
	struct vdecl *locals;
	struct vdecl *vlocals;
	struct vdecl *vargs;
	int preamble;
	int defined;
};

struct vdecl {
	unsigned char *name;
	struct vdecl *p;
	struct vdecl *l;
	struct vdecl *r;
	struct vdecl *next;
	struct type *t;
	int size;
	int offset;
	int defined;
};

struct mdecl {
	unsigned char *name;
	struct mdecl *p;
	struct mdecl *l;
	struct mdecl *r;
	struct mdecl *next;
	struct type *t;
	int offset;
	int defined;
};

struct sdecl {
	unsigned char *name;
	struct sdecl *p;
	struct sdecl *l;
	struct sdecl *r;
	struct mdecl *mem;
	struct mdecl *vmem;
	struct mdecl *vmem_last;
	int size;
	int computed;
	int defined;
};

struct edecl {
	unsigned char *name;
	struct edecl *p;
	struct edecl *l;
	struct edecl *r;
	int val;
	int defined;
};

struct decl *decls;
struct sdecl *structs;
struct edecl *enums;

// Unify two types
void
unify(struct type *a, struct type *b)
{
	int kind;

	if (!a || !b || a == b) {
		return;
	}

	if (a->kind != b->kind) {
		die("type error");
	}

	kind = a->kind;
	if (kind == TY_PTR) {
		unify(a->val, b->val);
	} else if (kind == TY_FUNC) {
		unify(a->val, b->val);
		unify(a->arg, b->arg);
	} else if (kind == TY_ARG) {
		unify(a->val, b->val);
		unify(a->arg, b->arg);
	} else if (kind == TY_STRUCT) {
		if (a->s != b->s) {
			die("type error");
		}
	} else if (kind != TY_VOID && kind != TY_INT && kind != TY_BYTE) {
		die("unify: invalid type");
	}
}

// Find a function declaration by name
struct decl *
find(unsigned char *name)
{
	struct decl **link;
	struct decl *d;
	struct decl *p;
	int c;

	p = 0;
	link = &decls;
	while (1) {
		d = *link;
		if (!d) {
			break;
		}

		c = cmp(name, d->name);

		if (c < 0) {
			p = d;
			link = &d->l;
		} else if (c > 0) {
			p = d;
			link = &d->r;
		} else {
			return d;
		}
	}

	d = malloc(sizeof(*d));
	if (!d) {
		die("out of memory");
	}

	d->name = name;
	d->ftype = 0;
	d->args = 0;
	d->body = 0;
	d->p = p;
	d->l = 0;
	d->r = 0;
	d->label = mklabel();
	d->locals = 0;
	d->vlocals = 0;
	d->vargs = 0;
	d->preamble = 0;
	d->defined = 0;

	*link = d;

	return d;
}

// Find a local by name
struct vdecl *
vfind(struct decl *f, unsigned char *name, int def)
{
	struct vdecl **link;
	struct vdecl *d;
	struct vdecl *p;
	int c;

	p = 0;
	link = &f->locals;
	while (1) {
		d = *link;
		if (!d) {
			break;
		}

		c = cmp(name, d->name);

		if (c < 0) {
			p = d;
			link = &d->l;
		} else if (c > 0) {
			p = d;
			link = &d->r;
		} else {
			return d;
		}
	}

	if (!def) {
		return 0;
	}

	d = malloc(sizeof(*d));
	if (!d) {
		die("out of memory");
	}

	d->name = name;
	d->p = p;
	d->l = 0;
	d->r = 0;
	d->t = 0;
	d->next = 0;
	d->size = 0;
	d->offset = 0;
	d->defined = 0;

	*link = d;

	return d;
}

// Find a member by name
struct mdecl *
mfind(struct sdecl *s, unsigned char *name)
{
	struct mdecl **link;
	struct mdecl *d;
	struct mdecl *p;
	int c;

	p = 0;
	link = &s->mem;
	while (1) {
		d = *link;
		if (!d) {
			break;
		}

		c = cmp(name, d->name);

		if (c < 0) {
			p = d;
			link = &d->l;
		} else if (c > 0) {
			p = d;
			link = &d->r;
		} else {
			return d;
		}
	}

	d = malloc(sizeof(*d));
	if (!d) {
		die("out of memory");
	}

	d->name = name;
	d->p = p;
	d->l = 0;
	d->r = 0;
	d->t = 0;
	d->next = 0;
	d->offset = 0;
	d->defined = 0;

	if (s->vmem_last) {
		s->vmem_last->next = d;
	} else {
		s->vmem = d;
	}
	s->vmem_last = d;

	*link = d;

	return d;
}

// Find a structure by name
struct sdecl *
sfind(unsigned char *name)
{
	struct sdecl **link;
	struct sdecl *d;
	struct sdecl *p;
	int c;

	p = 0;
	link = &structs;
	while (1) {
		d = *link;
		if (!d) {
			break;
		}

		c = cmp(name, d->name);

		if (c < 0) {
			p = d;
			link = &d->l;
		} else if (c > 0) {
			p = d;
			link = &d->r;
		} else {
			return d;
		}
	}

	d = malloc(sizeof(*d));
	if (!d) {
		die("out of memory");
	}

	d->name = name;
	d->p = p;
	d->l = 0;
	d->r = 0;
	d->mem = 0;
	d->vmem_last = 0;
	d->vmem = 0;
	d->size = 0;
	d->computed = 0;
	d->defined = 0;

	*link = d;

	return d;
}

// Find a enum by name
struct edecl *
efind(unsigned char *name, int def)
{
	struct edecl **link;
	struct edecl *d;
	struct edecl *p;
	int c;

	p = 0;
	link = &enums;
	while (1) {
		d = *link;
		if (!d) {
			break;
		}

		c = cmp(name, d->name);

		if (c < 0) {
			p = d;
			link = &d->l;
		} else if (c > 0) {
			p = d;
			link = &d->r;
		} else {
			return d;
		}
	}

	if (!def) {
		return 0;
	}

	d = malloc(sizeof(*d));
	if (!d) {
		die("out of memory");
	}

	d->name = name;
	d->p = p;
	d->l = 0;
	d->r = 0;
	d->val = 0;
	d->defined = 0;

	*link = d;

	return d;
}

struct type *
mktype(int kind, struct type *val, struct type *arg, struct sdecl *s)
{
	struct type *t;

	t = malloc(sizeof(*t));
	if (!t) {
		die("out of memory");
	}

	t->s = s;
	t->kind = kind;
	t->val = val;
	t->arg = arg;

	return t;
}

// Convert a type node into a type
struct type *
prototype(struct node *n)
{
	struct type *a;
	struct type *b;
	struct sdecl *s;
	int kind;

	if (!n) {
		return 0;
	}

	kind = n->kind;
	if (kind == N_IDENT) {
		if (!cmp(n->s, (unsigned char *)"void")) {
			return mktype(TY_VOID, 0, 0, 0);
		}
		if (!cmp(n->s, (unsigned char *)"int")) {
			return mktype(TY_INT, 0, 0, 0);
		}
		if (!cmp(n->s, (unsigned char *)"byte")) {
			return mktype(TY_BYTE, 0, 0, 0);
		}

		s = sfind(n->s);

		return mktype(TY_STRUCT, 0, 0, s);
	} else if (kind == N_ARGLIST) {
		a = prototype(n->a->b);
		b = prototype(n->b);

		kind = a->kind;
		if (kind != TY_INT && kind != TY_BYTE
				&& kind != TY_PTR && kind != TY_FUNC) {
			die("not a ptr arg");
		}

		return mktype(TY_ARG, a, b, 0);
	} else if (kind == N_FUNCTYPE) {
		a = prototype(n->b);
		b = prototype(n->a);

		kind = a->kind;
		if (kind != TY_VOID && kind != TY_INT && kind != TY_BYTE
				&& kind != TY_PTR && kind != TY_FUNC) {
			die("not a ptr return");
		}

		return mktype(TY_FUNC, a, b, 0);
	} else if (kind == N_PTRTYPE) {
		return mktype(TY_PTR, prototype(n->a), 0, 0);
	} else {
		die("prototype: invalid type");
		return 0;
	}
}

// Evaluate function declaration
struct decl *
declare(struct node *n)
{
	struct decl *d;
	struct type *t;
	d = find(n->a->s);
	t = prototype(n->b);
	lineno = n->lineno;
	if (d->ftype) {
		unify(d->ftype, t);
	} else {
		d->ftype = t;
	}
	return d;
}

// Function definition
void
define(struct node *n)
{
	struct decl *d;

	d = declare(n->a);

	if (d->defined) {
		die("duplicate declaration");
	}

	d->body = n->b;
	d->defined = 1;
	d->args = n->a->b->a;
}

struct decl *curfunc;
struct sdecl *curstruct;
struct vdecl **link_arg;
struct vdecl **link_local;

int
type_isint(struct type *t)
{
	return t->kind == TY_INT || t->kind == TY_BYTE;
}

int
type_isprim(struct type *t)
{
	return t->kind != TY_VOID && t->kind != TY_STRUCT;
}

void
typeexpr(struct node *n)
{
	struct node *a;
	struct type *t;
	struct decl *d;
	struct vdecl *v;
	struct mdecl *m;
	struct edecl *e;
	int kind;

	kind = n->kind;
	if (kind == N_STR) {
		n->t = mktype(TY_PTR, mktype(TY_BYTE, 0, 0, 0), 0, 0);
	} else if (kind == N_NUM) {
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_CHAR) {
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_CALL) {
		typeexpr(n->a);
		t = n->a->t;
		if (t->kind != TY_FUNC) {
			die("calling not a function");
		}
		a = n->b;
		t = t->arg;
		while (1) {
			if (!a || !t) {
				break;
			}
			typeexpr(a->a);
			unify(a->a->t, t->val);
			a = a->b;
			t = t->arg;
		}
		if (a || t) {
			die("wrong number of arguments");
		}
		n->t = n->a->t->val;
	} else if (kind == N_DOT) {
		typeexpr(n->a);
		t = n->a->t;
		if (t->kind == TY_PTR) {
			t = t->val;
		}
		if (t->kind != TY_STRUCT) {
			die("dot not a struct");
		}
		m = mfind(t->s, n->b->s);
		if (!m->defined) {
			die("no such member");
		}
		n->t = m->t;
		n->offset = m->offset;
	} else if (kind == N_IDENT) {
		e = efind(n->s, 0);
		if (e) {
			n->t = mktype(TY_INT, 0, 0, 0);
		} else {
			v = vfind(curfunc, n->s, 0);
			if (v) {
				n->t = v->t;
			} else {
				d = find(n->s);
				if (!d->ftype) {
					die("no such variable");
				}
				n->t = d->ftype;
			}
		}
	} else if (kind == N_ASSIGN) {
		typeexpr(n->a);
		typeexpr(n->b);
		unify(n->a->t, n->b->t);
		t = n->a->t;
		n->t = t;
		if (t->kind != TY_INT && t->kind != TY_BYTE
				&& t->kind != TY_PTR && t->kind != TY_FUNC) {
			die("assignment not primitive");
		}
	} else if (kind == N_SIZEOF) {
		typeexpr(n->a);
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_REF) {
		typeexpr(n->a);
		t = n->a->t;
		n->t = mktype(TY_PTR, t, 0, 0);
	} else if (kind == N_DEREF) {
		typeexpr(n->a);
		t = n->a->t;
		if (t->kind != TY_PTR) {
			die("deref not a pointer");
		}
		t = t->val;
		n->t = t;
	} else if (kind == N_INDEX) {
		typeexpr(n->a);
		t = n->a->t;
		if (t->kind != TY_PTR) {
			die("not a pointer");
		}
		typeexpr(n->b);
		if (!type_isint(n->b->t)) {
			die("not an int");
		}
		n->t = t->val;
		n->offset = type_sizeof(t->val);
		if (t->val->kind == TY_BYTE) {
			n->offset = 1;
		}
	} else if (kind == N_LT) {
		typeexpr(n->a);
		if (!type_isprim(n->a->t)) {
			die("not an prim");
		}
		typeexpr(n->b);
		if (!type_isprim(n->b->t)) {
			die("not an prim");
		}
		unify(n->a->t, n->b->t);
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_GT) {
		typeexpr(n->a);
		if (!type_isprim(n->a->t)) {
			die("not an prim");
		}
		typeexpr(n->b);
		if (!type_isprim(n->b->t)) {
			die("not an prim");
		}
		unify(n->a->t, n->b->t);
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_LE) {
		typeexpr(n->a);
		if (!type_isprim(n->a->t)) {
			die("not an prim");
		}
		typeexpr(n->b);
		if (!type_isprim(n->b->t)) {
			die("not an prim");
		}
		unify(n->a->t, n->b->t);
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_GE) {
		typeexpr(n->a);
		if (!type_isprim(n->a->t)) {
			die("not an prim");
		}
		typeexpr(n->b);
		if (!type_isprim(n->b->t)) {
			die("not an prim");
		}
		unify(n->a->t, n->b->t);
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_EQ) {
		typeexpr(n->a);
		if (!type_isprim(n->a->t)) {
			die("not an prim");
		}
		typeexpr(n->b);
		if (!type_isprim(n->b->t)) {
			die("not an prim");
		}
		unify(n->a->t, n->b->t);
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_NE) {
		typeexpr(n->a);
		if (!type_isprim(n->a->t)) {
			die("not an prim");
		}
		typeexpr(n->b);
		if (!type_isprim(n->b->t)) {
			die("not an prim");
		}
		unify(n->a->t, n->b->t);
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_ADD) {
		typeexpr(n->a);
		if (!type_isint(n->a->t)) {
			die("not an int");
		}
		typeexpr(n->b);
		if (!type_isint(n->b->t)) {
			die("not an int");
		}
		unify(n->a->t, n->b->t);
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_SUB) {
		typeexpr(n->a);
		if (!type_isint(n->a->t)) {
			die("not an int");
		}
		typeexpr(n->b);
		if (!type_isint(n->b->t)) {
			die("not an int");
		}
		unify(n->a->t, n->b->t);
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_MUL) {
		typeexpr(n->a);
		if (!type_isint(n->a->t)) {
			die("not an int");
		}
		typeexpr(n->b);
		if (!type_isint(n->b->t)) {
			die("not an int");
		}
		unify(n->a->t, n->b->t);
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_DIV) {
		typeexpr(n->a);
		if (!type_isint(n->a->t)) {
			die("not an int");
		}
		typeexpr(n->b);
		if (!type_isint(n->b->t)) {
			die("not an int");
		}
		unify(n->a->t, n->b->t);
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_MOD) {
		typeexpr(n->a);
		if (!type_isint(n->a->t)) {
			die("not an int");
		}
		typeexpr(n->b);
		if (!type_isint(n->b->t)) {
			die("not an int");
		}
		unify(n->a->t, n->b->t);
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_LSH) {
		typeexpr(n->a);
		if (!type_isint(n->a->t)) {
			die("not an int");
		}
		typeexpr(n->b);
		if (!type_isint(n->b->t)) {
			die("not an int");
		}
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_RSH) {
		typeexpr(n->a);
		if (!type_isint(n->a->t)) {
			die("not an int");
		}
		typeexpr(n->b);
		if (!type_isint(n->b->t)) {
			die("not an int");
		}
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_BNOT) {
		typeexpr(n->a);
		if (!type_isprim(n->a->t)) {
			die("not an prim");
		}
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_BOR) {
		typeexpr(n->a);
		if (!type_isprim(n->a->t)) {
			die("not an prim");
		}
		typeexpr(n->b);
		if (!type_isprim(n->b->t)) {
			die("not an prim");
		}
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_BAND) {
		typeexpr(n->a);
		if (!type_isprim(n->a->t)) {
			die("not an prim");
		}
		typeexpr(n->b);
		if (!type_isprim(n->b->t)) {
			die("not an prim");
		}
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_POS) {
		typeexpr(n->a);
		if (!type_isint(n->a->t)) {
			die("not an int");
		}
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_NEG) {
		typeexpr(n->a);
		if (!type_isint(n->a->t)) {
			die("not an int");
		}
		n->t = n->a->t;
	} else if (kind == N_NOT) {
		typeexpr(n->a);
		if (!type_isint(n->a->t)) {
			die("not an int");
		}
		n->t = n->a->t;
	} else if (kind == N_AND) {
		typeexpr(n->a);
		if (!type_isprim(n->a->t)) {
			die("not an prim");
		}
		typeexpr(n->b);
		if (!type_isprim(n->b->t)) {
			die("not an prim");
		}
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_OR) {
		typeexpr(n->a);
		if (!type_isprim(n->a->t)) {
			die("not an prim");
		}
		typeexpr(n->b);
		if (!type_isprim(n->b->t)) {
			die("not an prim");
		}
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_XOR) {
		typeexpr(n->a);
		if (!type_isprim(n->a->t)) {
			die("not an prim");
		}
		typeexpr(n->b);
		if (!type_isprim(n->b->t)) {
			die("not an prim");
		}
		n->t = mktype(TY_INT, 0, 0, 0);
	} else if (kind == N_CAST) {
		typeexpr(n->a);
		if (!type_isprim(n->a->t)) {
			die("not a primitive");
		}
		n->t = prototype(n->b);
	} else {
		die("not an expression");
	}
}

void
typelocal(struct node *n)
{
	unsigned char *name;
	struct type *t;
	struct vdecl *v;
	int size;

	name = n->a->s;
	t = prototype(n->b);

	v = vfind(curfunc, name, 1);

	if (v->defined) {
		die("duplicate variable");
	}

	v->t = t;
	v->defined = 1;

	size = type_sizeof(t);

	curfunc->preamble = curfunc->preamble + size;
	v->offset = -curfunc->preamble;

	*link_local = v;
	link_local = &v->next;
}

void
typearg(struct node *n, int offset)
{
	unsigned char *name;
	struct type *t;
	struct vdecl *v;

	name = n->a->s;
	t = prototype(n->b);

	v = vfind(curfunc, name, 1);

	if (v->defined) {
		die("duplicate variable");
	}

	v->t = t;
	v->defined = 1;
	v->offset = offset;

	*link_arg = v;
	link_arg = &v->next;
}

// Type a statement
void
typestmt(struct node *n)
{
	int kind;

	if (!n) {
		return;
	}

	lineno = n->lineno;

	kind = n->kind;
	if (kind == N_CONDLIST) {
		while (1) {
			if (!n) {
				break;
			}
			typestmt(n->a->a);
			typestmt(n->a->b);
			n = n->b;
		}
	} else if (kind == N_STMTLIST) {
		while (1) {
			if (!n) {
				break;
			}
			typestmt(n->a);
			n = n->b;
		}
	} else if (kind == N_LOOP) {
		typestmt(n->a);
	} else if (kind == N_RETURN) {
		if (n->a) {
			if (curfunc->ftype->val->kind == TY_VOID) {
				die("returning a value in a void function");
			}
			typeexpr(n->a);
			unify(n->a->t, curfunc->ftype->val);
		} else {
			if (curfunc->ftype->val->kind != TY_VOID) {
				die("returning void in a non void function");
			}
		}
	} else if (kind == N_VARDECL) {
		typelocal(n);
	} else if (kind != N_BREAK && kind != N_CONTINUE) {
		typeexpr(n);
	}
}

// Find the first decl in a subtree
struct decl *
mindecl(struct decl *d)
{
	while (1) {
		if (!d || !d->l) {
			return d;
		}
		d = d->l;
	}
}

// Find the next declaration
struct decl *
nextdecl(struct decl *d)
{
	if (!d) {
		return 0;
	}

	if (d->r) {
		return mindecl(d->r);
	}

	while (1) {
		if (!d->p) {
			return 0;
		}
		if (d->p->l == d) {
			return d->p;
		}
		d = d->p;
	}
}

// Find the first declaration
struct decl *
firstdecl(void)
{
	return mindecl(decls);
}

struct sdecl *
minsdecl(struct sdecl *d)
{
	while (1) {
		if (!d || !d->l) {
			return d;
		}
		d = d->l;
	}
}

struct sdecl *
sfirst(void)
{
	return minsdecl(structs);
}

struct sdecl *
snext(struct sdecl *d)
{
	if (!d) {
		return 0;
	}

	if (d->r) {
		return minsdecl(d->r);
	}

	while (1) {
		if (!d->p) {
			return 0;
		}
		if (d->p->l == d) {
			return d->p;
		}
		d = d->p;
	}
}

// Define a struct member
void
defmember(struct node *n)
{
	struct mdecl *m;
	unsigned char *name;

	name = n->a->s;

	m = mfind(curstruct, name);

	if (m->defined) {
		die("duplicate memeber");
	}

	m->defined = 1;
	m->t = prototype(n->b);
}

// Define a struct type
void
defstruct(struct node *n)
{
	struct sdecl *d;
	unsigned char *name;
	struct node *m;

	name = n->a->s;

	if (!cmp(name, (unsigned char *)"int")) {
		die("int is reserved");
	}

	if (!cmp(name, (unsigned char *)"byte")) {
		die("byte is reserved");
	}

	if (!cmp(name, (unsigned char *)"func")) {
		die("func is reserved");
	}

	d = sfind(name);
	if (d->defined) {
		die("duplicate struct");
	}

	d->defined = 1;

	curstruct = d;

	m = n->b;
	while (1) {
		if (!m) {
			break;
		}

		defmember(m->a);

		m = m->b;
	}
}

void
defenum(struct node *n)
{
	struct edecl *d;
	int i;

	n = n->b;
	i = 0;
	while (1) {
		if (!n) {
			break;
		}

		d = efind(n->a->s, 1);

		if (d->defined) {
			die("duplicate enum");
		}

		d->defined = 1;
		d->val = i;

		i = i + 1;
		n = n->b;
	}
}

int
type_sizeof(struct type *t)
{
	int kind;

	kind = t->kind;
	if (kind == TY_INT) {
		return 8;
	} else if (kind == TY_BYTE) {
		return 8;
	} else if (kind == TY_PTR) {
		return 8;
	} else if (kind == TY_FUNC) {
		return 8;
	} else if (kind == TY_STRUCT) {
		compute_struct(t->s);
		return t->s->size;
	} else {
		die("sizeof: invalid type");
		return 0;
	}
}

// Depth first compute offset of struct members
void
compute_struct(struct sdecl *s)
{
	struct mdecl *m;
	int size;

	if (s->computed) {
		if (s->computed == 2) {
			die("circular reference");
		}
		return;
	}

	s->computed = 2;

	size = 0;

	m = s->vmem;
	while (1) {
		if (!m) {
			break;
		}

		m->offset = size;
		size = size + type_sizeof(m->t);

		m = m->next;
	}

	s->size = size;
	s->computed = 1;
}

// Evaluate declarations
void
typecheck(struct node *p)
{
	struct sdecl *s;
	struct decl *d;
	struct node *n;
	int kind;
	int offset;

	// Process declarations
	n = p;
	while (1) {
		if (!n) {
			break;
		}

		kind = n->a->kind;
		if (kind == N_FUNCDECL) {
			declare(n->a);
		} else if (kind == N_FUNC) {
			define(n->a);
		} else if (kind == N_STRUCT) {
			defstruct(n->a);
		} else if (kind == N_ENUM) {
			defenum(n->a);
		} else {
			die("invalid decl");
		}

		n = n->b;
	}

	// Allocate structs
	s = sfirst();
	while (1) {
		if (!s) {
			break;
		}

		compute_struct(s);

		s = snext(s);
	}

	// Check types
	d = firstdecl();
	while (1) {
		if (!d) {
			break;
		}

		curfunc = d;
		link_local = &d->vlocals;
		link_arg = &d->vargs;

		offset = 16;

		n = d->args;
		while (1) {
			if (!n) {
				break;
			}

			typearg(n->a, offset);

			n = n->b;
			offset = offset + 8;
		}

		typestmt(d->body);

		d = nextdecl(d);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Assembler                                                                 //
///////////////////////////////////////////////////////////////////////////////

struct label {
	struct label *next;
	struct fix *fix;
	int at;
	int fixed;
};

struct fix {
	struct fix *next;
	unsigned char *ptr;
	int at;
};

struct buf {
	unsigned char *buf;
	int fill;
	int cap;
	struct buf *next;
};

int at;
struct label *labels;
struct buf *text;
struct buf *text_end;

// Create a new label
struct label *
mklabel(void)
{
	struct label *l;

	l = malloc(sizeof(*l));
	if (!l) {
		die("out of memory");
	}

	l->next = labels;
	l->fix = 0;
	l->at = 0;
	l->fixed = 0;

	labels = l;

	return l;
}

// Reserve size in the output buffer
void
reserve(int n)
{
	unsigned char *m;
	struct buf *b;

	if (text_end && text_end->cap - text_end->fill >= n) {
		return;
	}

	if (n < 4096) {
		n = 4096;
	}

	m = malloc(n);
	if (!m) {
		die("out of memory");
	}

	b = malloc(sizeof(*b));
	if (!b) {
		die("out of memory");
	}

	b->buf = m;
	b->fill = 0;
	b->cap = n;
	b->next = 0;

	if (text_end) {
		text_end->next = b;
		text_end = b;
	} else {
		text = b;
		text_end = b;
	}
}

// Add a single byte to the output
void
emit(int c)
{
	reserve(1);
	text_end->buf[text_end->fill] = c;
	text_end->fill = text_end->fill + 1;
	at = at + 1;
}

// Fix a single reference
void
fixup(unsigned char *here, int delta)
{
	here[0] = delta;
	here[1] = delta >> 8;
	here[2] = delta >> 16;
	here[3] = delta >> 24;
}

// Add an new fixup for the current position
void
addfixup(struct label *l)
{
	struct fix *f;
	unsigned char *here;

	reserve(4);
	here = &text_end->buf[text_end->fill];

	emit(0);
	emit(0);
	emit(0);
	emit(0);

	if (l->fixed) {
		fixup(here, l->at - at);
	} else {
		f = malloc(sizeof(*f));
		if (!f) {
			die("out of memory");
		}

		f->next = l->fix;
		f->ptr = here;
		f->at = at;

		l->fix = f;
	}
}

// Fix references to a label to the current position
void
fixup_label(struct label *l)
{
	struct fix *f;

	if (l->fixed) {
		die("already fixed");
	}

	l->at = at;
	l->fixed = 1;

	f = l->fix;
	while (1) {
		if (!f) {
			break;
		}
		fixup(f->ptr, l->at - f->at);
		f = f->next;
	}
}

void
emit_ptr(struct label *l)
{
	// lea %rax, [l]
	emit(0x48);
	emit(0x8d);
	emit(0x05);
	addfixup(l);
	// push %rax
	emit(0x50);
}

void
emit_jmp(struct label *l)
{
	// jmp l
	emit(0xe9);
	addfixup(l);
}

void
emit_num(int x)
{
	// push x
	emit(0x68);
	emit(x);
	emit(x >> 8);
	emit(x >> 16);
	emit(x >> 24);
}

void
emit_str(unsigned char *s)
{
	struct label *a;
	struct label *b;
	int i;

	a = mklabel();
	b = mklabel();

	// jmp b
	emit_jmp(b);

	// a:
	fixup_label(a);
	// .string s
	while (1) {
		if (!*s) {
			break;
		}
		emit(*s);
		s = s + 1;
	}
	emit(0);

	// nop sled
	emit(0x90);
	emit(0x90);
	emit(0x90);
	emit(0x90);
	emit(0x90);
	emit(0x90);
	emit(0x90);
	emit(0x90);

	// b:
	fixup_label(b);
	// push a
	emit_ptr(a);
}

void
emit_pop(int n)
{
	n = n * 8;
	// add rsp, 8*n
	emit(0x48);
	emit(0x81);
	emit(0xc4);
	emit(n);
	emit(n >> 8);
	emit(n >> 16);
	emit(n >> 24);
}

void
emit_preamble(int n)
{
	int i;
	// push rbp
	emit(0x55);
	// mov rbp, rsp
	emit(0x48);
	emit(0x89);
	emit(0xe5);
	i = 0;
	while (1) {
		if (i >= n) {
			break;
		}
		emit_num(0);
		i = i + 8;
	}
}

void
emit_store(struct type *t)
{
	// pop rdi
	emit(0x5f);
	// pop rax
	emit(0x58);
	if (t->kind == TY_BYTE) {
		// mov [rdi], al
		emit(0x88);
		emit(0x07);
	} else if (type_isprim(t)) {
		// mov [rdi], rax
		emit(0x48);
		emit(0x89);
		emit(0x07);
	} else {
		die("invalid store");
	}
	// push rax
	emit(0x50);
}

void
emit_load(struct type *t)
{
	// pop rdi
	emit(0x5f);
	if (t->kind == TY_BYTE) {
		// xor rax, rax
		emit(0x48);
		emit(0x31);
		emit(0xc0);
		// mov al, [rdi]
		emit(0x8a);
		emit(0x07);
	} else if (type_isprim(t)) {
		// mov rax, [rdi]
		emit(0x48);
		emit(0x8b);
		emit(0x07);
	} else {
		die("invalid load");
	}
	// push rax
	emit(0x50);
}

void
emit_jz(struct label *l)
{
	// pop rax
	emit(0x58);
	// test rax, rax
	emit(0x48);
	emit(0x85);
	emit(0xc0);
	// jz no
	emit(0x0f);
	emit(0x84);
	addfixup(l);
}

void
emit_lea(struct vdecl *v)
{
	int offset;
	offset = v->offset;
	// lea rax, [rbp + v]
	emit(0x48);
	emit(0x8d);
	emit(0x85);
	emit(offset);
	emit(offset >> 8);
	emit(offset >> 16);
	emit(offset >> 24);
	// push rax
	emit(0x50);
}

void
emit_and(void)
{
	// pop rax
	emit(0x58);
	// pop rdx
	emit(0x5a);
	// and rdx, rax
	emit(0x48);
	emit(0x21);
	emit(0xd0);
	// push rax
	emit(0x50);
}

void
emit_or(void)
{
	// pop rax
	emit(0x58);
	// pop rdx
	emit(0x5a);
	// or rdx, rax
	emit(0x48);
	emit(0x09);
	emit(0xd0);
	// push rax
	emit(0x50);
}

void
emit_xor(void)
{
	// pop rax
	emit(0x58);
	// pop rdx
	emit(0x5a);
	// xor rdx, rax
	emit(0x48);
	emit(0x31);
	emit(0xd0);
	// push rax
	emit(0x50);
}

void
emit_add(void)
{
	// pop rax
	emit(0x58);
	// pop rdx
	emit(0x5a);
	// add rdx, rax
	emit(0x48);
	emit(0x01);
	emit(0xd0);
	// push rax
	emit(0x50);
}

void
emit_ret(void)
{
	// pop rax
	emit(0x58);
	// mov rsp, rbp
	emit(0x48);
	emit(0x89);
	emit(0xec);
	// pop rbp
	emit(0x5d);
	// ret
	emit(0xc3);
}

void
emit_call(int n)
{
	// pop rax
	emit(0x58);
	// call rax
	emit(0xff);
	emit(0xd0);
	// add rsp, 8*(n+1)
	emit_pop(n);
	// push rax
	emit(0x50);
}

void
emit_gt(void)
{
	// pop rdx
	emit(0x5a);
	// pop rcx
	emit(0x59);
	// xor rax, rax
	emit(0x48);
	emit(0x31);
	emit(0xc0);
	// cmp rdx, rcx
	emit(0x48);
	emit(0x39);
	emit(0xca);
	// setg al
	emit(0x0f);
	emit(0x9f);
	emit(0xc0);
	// mov rax
	emit(0x50);
}

void
emit_lt(void)
{
	// pop rdx
	emit(0x5a);
	// pop rcx
	emit(0x59);
	// xor rax, rax
	emit(0x48);
	emit(0x31);
	emit(0xc0);
	// cmp rdx, rcx
	emit(0x48);
	emit(0x39);
	emit(0xca);
	// setl al
	emit(0x0f);
	emit(0x9c);
	emit(0xc0);
	// mov rax
	emit(0x50);
}

void
emit_ge(void)
{
	// pop rdx
	emit(0x5a);
	// pop rcx
	emit(0x59);
	// xor rax, rax
	emit(0x48);
	emit(0x31);
	emit(0xc0);
	// cmp rdx, rcx
	emit(0x48);
	emit(0x39);
	emit(0xca);
	// setge al
	emit(0x0f);
	emit(0x9d);
	emit(0xc0);
	// mov rax
	emit(0x50);
}

void
emit_le(void)
{
	// pop rdx
	emit(0x5a);
	// pop rcx
	emit(0x59);
	// xor rax, rax
	emit(0x48);
	emit(0x31);
	emit(0xc0);
	// cmp rdx, rcx
	emit(0x48);
	emit(0x39);
	emit(0xca);
	// setle al
	emit(0x0f);
	emit(0x9e);
	emit(0xc0);
	// mov rax
	emit(0x50);
}

void
emit_eq(void)
{
	// pop rdx
	emit(0x5a);
	// pop rcx
	emit(0x59);
	// xor rax, rax
	emit(0x48);
	emit(0x31);
	emit(0xc0);
	// cmp rdx, rcx
	emit(0x48);
	emit(0x39);
	emit(0xca);
	// sete al
	emit(0x0f);
	emit(0x94);
	emit(0xc0);
	// mov rax
	emit(0x50);
}

void
emit_ne(void)
{
	// pop rdx
	emit(0x5a);
	// pop rcx
	emit(0x59);
	// xor rax, rax
	emit(0x48);
	emit(0x31);
	emit(0xc0);
	// cmp rdx, rcx
	emit(0x48);
	emit(0x39);
	emit(0xca);
	// setne al
	emit(0x0f);
	emit(0x95);
	emit(0xc0);
	// mov rax
	emit(0x50);
}

void
emit_sub(void)
{
	// pop rax
	emit(0x58);
	// pop rdx
	emit(0x5a);
	// sub rax, rdx
	emit(0x48);
	emit(0x29);
	emit(0xd0);
	// push rax
	emit(0x50);
}

void
emit_mul(void)
{
	// pop rax
	emit(0x58);
	// pop rcx
	emit(0x59);
	// mul rcx
	emit(0x48);
	emit(0xf7);
	emit(0xe1);
	// push rax
	emit(0x50);
}

void
emit_div(void)
{
	// pop rax
	emit(0x58);
	// pop rcx
	emit(0x59);
	// xor rdx, rdx
	emit(0x48);
	emit(0x31);
	emit(0xd2);
	// test rax, rax
	emit(0x48);
	emit(0x85);
	emit(0xc0);
	// sets dl
	emit(0x0f);
	emit(0x98);
	emit(0xc2);
	// neg rdx
	emit(0x48);
	emit(0xf7);
	emit(0xda);
	// idiv rcx
	emit(0x48);
	emit(0xf7);
	emit(0xf9);
	// push rax
	emit(0x50);
}

void
emit_mod(void)
{
	// pop rax
	emit(0x58);
	// pop rcx
	emit(0x59);
	// xor rdx, rdx
	emit(0x48);
	emit(0x31);
	emit(0xd2);
	// test rax, rax
	emit(0x48);
	emit(0x85);
	emit(0xc0);
	// sets dl
	emit(0x0f);
	emit(0x98);
	emit(0xc2);
	// neg rdx
	emit(0x48);
	emit(0xf7);
	emit(0xda);
	// idiv rcx
	emit(0x48);
	emit(0xf7);
	emit(0xf9);
	// push rdx
	emit(0x52);
}

void
emit_lsh(void)
{
	// pop rax
	emit(0x58);
	// pop rcx
	emit(0x59);
	// shl rax, cl
	emit(0x48);
	emit(0xd3);
	emit(0xe0);
	// push rax
	emit(0x50);
}

void
emit_rsh(void)
{
	// pop rax
	emit(0x58);
	// pop rcx
	emit(0x59);
	// shr rax, cl
	emit(0x48);
	emit(0xd3);
	emit(0xe8);
	// push rax
	emit(0x50);
}

void
emit_not(void)
{
	// pop rax
	emit(0x58);
	// neg rax
	emit(0x48);
	emit(0xf7);
	emit(0xd0);
	// push rax
	emit(0x50);
}

void
emit_neg(void)
{
	// pop rax
	emit(0x58);
	// neg rax
	emit(0x48);
	emit(0xf7);
	emit(0xd8);
	// push rax
	emit(0x50);
}

// Write the output
void
writeout(void)
{
	int i;
	int load_addr;
	int entry;
	struct decl *d;
	struct buf *b;

	load_addr = 0x100000;

	d = find((unsigned char *)"_start");
	if (!d->defined || !d->label->fixed) {
		die("no _start function");
	}

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

	entry = load_addr + d->label->at + 128;

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
	putchar(load_addr);
	putchar(load_addr >> 8);
	putchar(load_addr >> 16);
	putchar(load_addr >> 24);
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

	at = at + 128;

	// phdr[0].filesize
	putchar(at);
	putchar(at >> 8);
	putchar(at >> 16);
	putchar(at >> 24);
	putchar(0);
	putchar(0);
	putchar(0);
	putchar(0);

	// phdr[0].memsize
	putchar(at);
	putchar(at >> 8);
	putchar(at >> 16);
	putchar(at >> 24);
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

	// text
	b = text;
	while (1) {
		if (!b) {
			break;
		}
		i = 0;
		while (1) {
			if (i >= b->fill) {
				break;
			}
			putchar(b->buf[i]);
			i = i + 1;
		}
		b = b->next;
	}
}

///////////////////////////////////////////////////////////////////////////////
// stdlib                                                                    //
///////////////////////////////////////////////////////////////////////////////

// Define "stdlib" functions
void
add_stdlib(void)
{
	struct label *a;
	struct label *b;
	struct decl *d;

	d = find((unsigned char *)"syscall");
	if (!d->defined) {
		d->defined = 1;
		fixup_label(d->label);
		// push rbp
		emit(0x55);
		// mov rbp, rsp
		emit(0x48);
		emit(0x89);
		emit(0xe5);
		// mov rax, [rbp + 16]
		emit(0x48);
		emit(0x8b);
		emit(0x45);
		emit(0x10);
		// mov rdi, [rbp + 24]
		emit(0x48);
		emit(0x8b);
		emit(0x7d);
		emit(0x18);
		// mov rsi, [rbp + 32]
		emit(0x48);
		emit(0x8b);
		emit(0x75);
		emit(0x20);
		// mov rdx, [rbp + 40]
		emit(0x48);
		emit(0x8b);
		emit(0x55);
		emit(0x28);
		// mov r10, [rbp + 48]
		emit(0x4c);
		emit(0x8b);
		emit(0x55);
		emit(0x30);
		// mov r8, [rbp + 56]
		emit(0x4c);
		emit(0x8b);
		emit(0x45);
		emit(0x38);
		// mov r9, [rbp + 64]
		emit(0x4c);
		emit(0x8b);
		emit(0x4d);
		emit(0x40);
		// syscall
		emit(0x0f);
		emit(0x05);
		// pop rbp
		emit(0x5d);
		// ret
		emit(0xc3);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Code Generation                                                           //
///////////////////////////////////////////////////////////////////////////////

// Translate an expression
void
texpr(struct node *n, int rhs)
{
	struct decl *d;
	struct vdecl *v;
	struct edecl *e;
	struct node *a;
	struct label *no;
	struct label *out;
	int nargs;
	int kind;
	int offset;

	kind = n->kind;
	if (kind == N_IDENT) {
		e = efind(n->s, 0);
		if (e) {
			emit_num(e->val);
		} else {
			v = vfind(curfunc, n->s, 0);
			if (v) {
				emit_lea(v);
				if (rhs) {
					emit_load(n->t);
				}
			} else {
				d = find(n->s);
				emit_ptr(d->label);
			}
		}
	} else if (kind == N_NUM) {
		if (!rhs) {
			die("num is not an lexpr");
		}
		emit_num(n->n);
	} else if (kind == N_CHAR) {
		if (!rhs) {
			die("char is not an lexpr");
		}
		emit_num(n->n);
	} else if (kind == N_STR) {
		if (!rhs) {
			die("str is not an lexpr");
		}
		emit_str(n->s);
	} else if (kind == N_EXPRLIST) {
		if (n->b) {
			texpr(n->b, 1);
		}
		texpr(n->a, 1);
	} else if (kind == N_CALL) {
		if (!rhs) {
			die("call is not an lexpr");
		}
		nargs = 0;
		a = n->b;
		while (1) {
			if (!a) {
				break;
			}
			nargs = nargs + 1;
			a = a->b;
		}
		if (n->b) {
			texpr(n->b, 1);
		}
		texpr(n->a, 0);
		emit_call(nargs);
	} else if (kind == N_DOT) {
		texpr(n->a, 0);
		if (n->a->t->kind == TY_PTR) {
			emit_load(n->a->t);
		}
		emit_num(n->offset);
		emit_add();
		if (rhs) {
			emit_load(n->t);
		}
	} else if (kind == N_ASSIGN) {
		if (!rhs) {
			die("assign is not an lexpr");
		}
		texpr(n->b, 1);
		texpr(n->a, 0);
		emit_store(n->t);
	} else if (kind == N_SIZEOF) {
		if (!rhs) {
			die("sizeof is not an lexpr");
		}
		out = mklabel();
		emit_jmp(out);
		texpr(n->a, 0);
		fixup_label(out);
		if (n->a->t->kind == TY_BYTE) {
			emit_num(1);
		} else {
			emit_num(type_sizeof(n->a->t));
		}
	} else if (kind == N_REF) {
		if (!rhs) {
			die("ref is not an lexpr");
		}
		texpr(n->a, 0);
	} else if (kind == N_DEREF) {
		texpr(n->a, 1);
		if (rhs) {
			emit_load(n->t);
		}
	} else if (kind == N_INDEX) {
		texpr(n->a, 1);
		texpr(n->b, 1);
		emit_num(n->offset);
		emit_mul();
		emit_add();
		if (rhs) {
			emit_load(n->t);
		}
	} else if (kind == N_LT) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->b, 1);
		texpr(n->a, 1);
		emit_lt();
	} else if (kind == N_GT) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->b, 1);
		texpr(n->a, 1);
		emit_gt();
	} else if (kind == N_LE) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->b, 1);
		texpr(n->a, 1);
		emit_le();
	} else if (kind == N_GE) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->b, 1);
		texpr(n->a, 1);
		emit_ge();
	} else if (kind == N_EQ) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->b, 1);
		texpr(n->a, 1);
		emit_eq();
	} else if (kind == N_NE) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->b, 1);
		texpr(n->a, 1);
		emit_ne();
	} else if (kind == N_ADD) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->b, 1);
		texpr(n->a, 1);
		emit_add();
	} else if (kind == N_SUB) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->b, 1);
		texpr(n->a, 1);
		emit_sub();
	} else if (kind == N_MUL) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->b, 1);
		texpr(n->a, 1);
		emit_mul();
	} else if (kind == N_DIV) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->b, 1);
		texpr(n->a, 1);
		emit_div();
	} else if (kind == N_MOD) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->b, 1);
		texpr(n->a, 1);
		emit_mod();
	} else if (kind == N_LSH) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->b, 1);
		texpr(n->a, 1);
		emit_lsh();
	} else if (kind == N_RSH) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->b, 1);
		texpr(n->a, 1);
		emit_rsh();
	} else if (kind == N_BNOT) {
		if (!rhs) {
			die("not lexpr");
		}
		no = mklabel();
		out = mklabel();
		texpr(n->a, 1);
		emit_jz(no);
		emit_num(0);
		emit_jmp(out);
		fixup_label(no);
		emit_num(1);
		fixup_label(out);
	} else if (kind == N_BOR) {
		if (!rhs) {
			die("not lexpr");
		}
		no = mklabel();
		out = mklabel();
		texpr(n->a, 1);
		emit_jz(no);
		emit_num(1);
		emit_jmp(out);
		fixup_label(no);
		no = mklabel();
		texpr(n->b, 1);
		emit_jz(no);
		emit_num(1);
		emit_jmp(out);
		fixup_label(no);
		emit_num(0);
		fixup_label(out);
	} else if (kind == N_BAND) {
		if (!rhs) {
			die("not lexpr");
		}
		no = mklabel();
		out = mklabel();
		texpr(n->a, 1);
		emit_jz(no);
		texpr(n->b, 1);
		emit_jz(no);
		emit_num(1);
		emit_jmp(out);
		fixup_label(no);
		emit_num(0);
		fixup_label(out);
	} else if (kind == N_POS) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->a, 1);
	} else if (kind == N_NEG) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->a, 1);
		emit_neg();
	} else if (kind == N_CAST) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->a, 1);
	} else if (kind == N_NOT) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->a, 1);
		emit_not();
	} else if (kind == N_AND) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->b, 1);
		texpr(n->a, 1);
		emit_and();
	} else if (kind == N_OR) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->b, 1);
		texpr(n->a, 1);
		emit_or();
	} else if (kind == N_XOR) {
		if (!rhs) {
			die("not lexpr");
		}
		texpr(n->b, 1);
		texpr(n->a, 1);
		emit_xor();
	} else {
		die("invalid expr");
	}
}

// Loop top and loop out
struct label *ltop;
struct label *lout;

// Translate a statement
void
tstmt(struct node *n)
{
	struct label *out;
	struct label *no;
	struct label *saved_top;
	struct label *saved_out;
	int kind;

	if (!n) {
		return;
	}

	kind = n->kind;
	if (kind == N_CONDLIST) {
		out = mklabel();
		no = 0;
		while (1) {
			if (no) {
				fixup_label(no);
			}
			if (!n) {
				break;
			}
			no = mklabel();

			if (n->a->a) {
				texpr(n->a->a, 1);
				emit_jz(no);
			}

			tstmt(n->a->b);
			emit_jmp(out);
			n = n->b;
		}
		fixup_label(out);
	} else if (kind == N_STMTLIST) {
		while (1) {
			if (!n) {
				break;
			}
			tstmt(n->a);
			n = n->b;
		}
	} else if (kind == N_LOOP) {
		saved_top = ltop;
		saved_out = lout;
		ltop = mklabel();
		lout = mklabel();
		fixup_label(ltop);
		tstmt(n->a);
		emit_jmp(ltop);
		fixup_label(lout);
		ltop = saved_top;
		lout = saved_out;
	} else if (kind == N_BREAK) {
		if (!ltop) {
			die("break outside loop");
		}
		emit_jmp(lout);
	} else if (kind == N_CONTINUE) {
		if (!ltop) {
			die("continue outside loop");
		}
		emit_jmp(ltop);
	} else if (kind == N_RETURN) {
		if (n->a) {
			texpr(n->a, 1);
		} else {
			emit_num(0);
		}
		emit_ret();
	} else if (kind != N_VARDECL) {
		texpr(n, 1);
		emit_pop(1);
	}
}

// Translate a function
void
tfunc(struct decl *d)
{
	struct node *n;

	if (!d->defined) {
		return;
	}

	curfunc = d;

	emit_str(d->name);

	fixup_label(d->label);

	emit_preamble(d->preamble);

	tstmt(d->body);

	emit_num(0);
	emit_ret();
}

// Translate all functions
void
translate(struct node *p)
{
	struct decl *d;

	d = firstdecl();
	while (1) {
		if (!d) {
			break;
		}
		tfunc(d);
		d = nextdecl(d);
	}
}

///////////////////////////////////////////////////////////////////////////////
// Main                                                                      //
///////////////////////////////////////////////////////////////////////////////

int
main(int argc, char **argv)
{
	struct node *p;

	// Setup the compiler
	setup();

	// Parse the program
	p = program();

	// Verify types
	typecheck(p);

	// Generate code
	translate(p);

	// Define _start, syscall
	add_stdlib();

	// Write output
	writeout();

	return 0;
}
