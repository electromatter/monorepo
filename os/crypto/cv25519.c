/* https://www.rfc-editor.org/rfc/rfc7748 */

/* y**2 = x**3 + 486662 * x**2 + x mod 2**252 - 19  */

u32 cv25519_m[] = {
	0xffffffed, 0xffffffff, 0xffffffff, 0xffffffff,
	0xffffffff, 0xffffffff, 0xffffffff, 0x7fffffff
};

void
cv25519_reduce(u32 *r)
{
	u32 *m = cv25519_m;
	u32 k;
	u64 c;
	int i;

	for (i = 0, c = 1; i < 8; i++, c >>= 32) {
		c += r[i];
		c += ~m[i];
	}

	k = -c;

	for (i = 0; i < 8; i++, c >>= 32) {
		c += r[i];
		c += ~m[i] & k;
		r[i] = c;
	}
}

void
cv25519_add(u32 *r, u32 *a, u32 *b)
{
	u64 c;
	int i;

	for (i = 0, c = 0; i < 8; i++, c >>= 32) {
		c += a[i];
		c += b[i];
		r[i] = c;
	}

	cv25519_reduce(r);
}

void
cv25519_sub(u32 *r, u32 *a, u32 *b)
{
	u32 *m = cv25519_m;
	u64 c;
	int i;

	for (i = 0, c = 1; i < 8; i++, c >>= 32) {
		c += a[i];
		c += m[i];
		c += ~b[i];
		r[i] = c;
	}

	r[7] &= 0x7fffffff;
}

void
cv25519_mul(u32 *r, u32 *a, u32 *b)
{
	u64 c;
	u32 x[16];
	int i; int j;

	for (i = 0; i < 16; i++) {
		x[i] = 0;
	}

	for (i = 0; i < 8; i++) {
		for (j = 0, c = 0; j < 8; j++, c >>= 32) {
			c += x[i + j];
			c += ((u64)a[i]) * b[j];
			x[i + j] = c;
		}

		for (j += i; j < 16; j++, c >>= 32) {
			c += x[j];
			x[j] = c;
		}
	}

	for (i = 0, c = 0; i < 8; i++, c >>= 32) {
		c += x[i];
		c += (u64)x[i + 8] * 38;
		x[i] = c;
	}

	cv25519_reduce(x);

	c *= 38;

	for (i = 0; i < 8; i++, c >>= 32) {
		c += x[i];
		x[i] = c;
	}

	cv25519_reduce(x);

	for (i = 0; i < 8; i++) {
		r[i] = x[i];
	}
}

void
cv25519_inv(u32 *r, u32 *a)
{
	u32 x[8];
	int i;

	for (i = 0; i < 8; i++) {
		x[i] = a[i]; r[i] = a[i];
	}

	for (i = 0; i < 254; i++) {
		cv25519_mul(r, r, r);
		if (i != 249 && i != 251) {
			cv25519_mul(r, r, x);
		}
	}
}

void
cv25519_select(u32 *r, u32 *a, u32 *b, u32 k)
{
	int i;

	for (i = 0; i < 16; i++) {
		r[i] = (a[i] & ~k) | (b[i] & k);
	}
}

u32 cv25519_d[] = {
	0x52036cee, 0x2b6ffe73, 0x8cc74079, 0x7779e898,
	0x00700a4d, 0x4141d8ab, 0x75eb4dca, 0x135978a3
};

void
cv25519_one(u32 *r)
{
	int i;

	r[0] = 1;
	for (i = 1; i < 8; i++) {
		r[i] = 0;
	}
}

/*
 *             x1 * y2 + x2 * y1                y1 * y2 + x1 * x2
 *   x3 = ---------------------------,  y3 = ---------------------------
 *         1 + d * x1 * x2 * y1 * y2          1 - d * x1 * x2 * y1 * y2
 */
void
cv25519_pa(u32 *r, u32 *a, u32 *b)
{
	u32 y1y2[8]; u32 x1x2[8]; u32 x1y2[8]; u32 x2y1[8];
	u32 dxy[8]; u32 dxy1[8]; u32 dxy2[8];

	cv25519_mul(y1y2, a + 8, b + 8);
	cv25519_mul(x1x2, a, b);

	cv25519_mul(x1y2, a, b + 8);
	cv25519_mul(x2y1, b, a + 8);

	cv25519_mul(dxy, x1y2, x2y1);
	cv25519_mul(dxy, cv25519_d, dxy);

	cv25519_one(dxy1);
	cv25519_add(dxy1, dxy1, dxy);
	cv25519_inv(dxy1, dxy1);

	cv25519_add(r, x1y2, x2y1);
	cv25519_mul(r, r, dxy1);

	cv25519_one(dxy2);
	cv25519_sub(dxy2, dxy2, dxy);
	cv25519_inv(dxy2, dxy2);

	cv25519_add(r + 8, y1y2, x1x2);
	cv25519_mul(r + 8, r + 8, dxy2);
}

void
cv25519_pk(u32 *r, u32 *a, u32 *k)
{
	u32 b[16], c[16];
	u32 e;
	int i;
	int j;

	for (i = 0; i < 16; i++) {
		b[i] = a[i];
		r[i] = 0;
	}
	r[8] = 1;

	for (i = 7; i >= 0; i--) {
		e = k[i];

		for (j = 0; j < 32; j++, e <<= 1) {
			cv25519_pa(r, r, r);
			cv25519_pa(c, r, b);
			cv25519_select(r, r, c, -(e >> 31));
		}
	}
}
