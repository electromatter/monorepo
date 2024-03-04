void
rsa_mul(u32 *y, u32 *a, u32 *b, int n)
{
	u64 c;
	int i; int j;

	for (i = 0; i < 2 * n; i++) {
		y[i] = 0;
	}

	for (i = 0; i < n; i++) {
		for (j = 0, c = 0; j < n; j++, c >>= 32) {
			c += (u64)a[i] * b[j];
			c += y[i + j];
			y[i + j] = c;
		}

		for (j += i; j < 2 * n; j++, c >>= 32) {
			c += y[j];
			y[j] = c;
		}
	}
}

u32
rsa_trymod(u32 *r, u32 *d, u32 q, int n)
{
	int i;
	u64 a; u64 s;

	for (i = 0, a = 0, s = 1; i < n; i++, a >>= 32, s >>= 32) {
		a += (u64)d[i] * q;
		s += (~a) & (u32)-1;
		s += r[i];
	}

	s += (~a) & (u32)-1;
	s += r[i];
	s >>= 32;

	return ~s & (((u64)q + (u32)-1) >> 32);
}

void
rsa_domod(u32 *r, u32 *d, u32 q, int n)
{
	int i;
	u64 a; u64 s;

	for (i = 0, a = 0, s = 1; i < n; i++, a >>= 32, s >>= 32) {
		a += (u64)d[i] * q;
		s += (~a) & (u32)-1;
		s += r[i];
		r[i] = s;
	}

	s += (~a) & (u32)-1;
	s += r[i];
	r[i] = s;
}

/*
 * Based on Kunth's Algorithm D.
 *
 * I found this blog post helpful in understanding:
 *
 * - https://ridiculousfish.com/blog/posts/labor-of-division-episode-iv.html
 */
void
rsa_mod(u32 *r, u32 *x, u32 *d, int n)
{
	int i; int j; int nd; int k;
	u32 h; u64 q; u64 a;

	nd = n;
	while (d[nd - 1] == 0 && nd > 0) {
		nd--;
	}

	if (nd == 1) {
		for (i = 2 * n - 1, a = 0; i >= 0; i--) {
			a = ((a << 32) + x[i]) % d[nd - 1];
			r[i] = 0;
		}

		r[0] = a;
		return;
	}

	h = d[nd - 1];
	for (k = 0; (h & (1 << 31)) == 0; k++) {
		h <<= 1;
	}
	h |= ((u64)d[nd - 2] << k) >> 32;

	for (i = 0; i < 2 * n; i++) {
		r[i] = x[i];
	}
	r[i] = 0;

	for (i = 2 * n, j = 2 * n - nd; j >= 0; i--, j--) {
		a = ((u64)r[i] << (k + 32))
			| ((u64)r[i - 1] << k)
			| (((u64)r[i - 2] << k) >> 32);

		q = a / h;

		q -= q >> 32;

		q -= rsa_trymod(r + j, d, q, n);

		q -= rsa_trymod(r + j, d, q, n);

		rsa_domod(r + j, d, q, n);
	}
}

void
rsa_sel(u32 *y, u32 *x, int n, u32 s)
{
	int i;

	for (i = 0; i < n; i++) {
		y[i] = (y[i] & ~s) | (x[i] & s);
	}
}

void
rsa_pow(u32 *y, u32 *x, u32 *d, u32 *m, u32 *t, int n)
{
	int nd; int i;
	u32 e;

	y[0] = 1;
	for (i = 1; i < 2 * n; i++) {
		y[i] = 0;
	}

	for (nd = n - 1; nd >= 0; nd--) {
		for (e = d[nd], i = 0; i < 32; i++, e <<= 1) {
			rsa_mul(t, y, y, n);
			rsa_mod(y, t, m, n);

			rsa_mul(t, y, x, n);
			rsa_mod(t, t, m, n);

			rsa_sel(y, t, n, -(e >> 31));
		}
	}
}
