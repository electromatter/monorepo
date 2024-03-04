/* https://www.rfc-editor.org/rfc/rfc7539 */

u32 poly1305_m[] = {
	0xfffffffb, 0xffffffff, 0xffffffff, 0xffffffff, 0x3
};

void
poly1305_reduce(u32 *a)
{
	u32 *m = poly1305_m;
	u32 k;
	u64 c;
	int i;

	for (i = 0, c = 1; i < 5; i++, c >>= 32) {
		c += a[i];
		c += ~m[i];
	}

	k = -c;

	for (i = 0; i < 8; i++, c >>= 32) {
		c += a[i];
		c += ~m[i] & k;
		a[i] = c;
	}
}

void
poly1305_load(u32 *dest, byte *src, int n)
{
	int i;

	for (i = 0; i < n; i++, src += 4) {
		*dest++ = (u32)src[0]
			| ((u32)src[1] << 8)
			| ((u32)src[2] << 16)
			| ((u32)src[3] << 24);
	}
}

void
poly1305_digest(byte *dest, u32 *src)
{
	int i;

	poly1305_reduce(src);

	for (i = 0; i < 4; i++) {
		*dest++ = *src;
		*dest++ = *src >> 8;
		*dest++ = *src >> 16;
		*dest++ = *src++ >> 24;
	}
}

void
poly1305_add(u32 *a, u32 *x)
{
	u64 c;
	int i;

	for (i = 0, c = 0; i < 5; i++, c >>= 32) {
		c += (u64)x[i] + a[i];
		a[i] = c;
	}
}

void
poly1305_mul(u32 *a, u32 *r)
{
	u32 x[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	u64 c;
	int i;
	int j;

	/* Schoolbook Multiplication */
	for (i = 0; i < 5; i++) {
		for (j = 0, c = 0; j < 5; j++, c >>= 32) {
			c += (u64)a[i] * r[j] + x[i + j];
			x[i + j] = c;
		}

		c += x[i + j];
		x[i + j] = c;
	}

	for (i = 0; i < 4; i++) {
		a[i] = x[i];
	}
	a[i] = x[i] & 3;

	/* Modular reduction */
	for (i = 0, c = 0; i < 5; i++, c >>= 32) {
		c += ((u64)x[i + 4] >> 2) * 5;
		c += ((u64)(x[i + 5] & 3) << 30) * 5;
		c += a[i];
		a[i] = c;
	}
}

void
poly1305_truncate(u32 *dest, byte *key)
{
	poly1305_load(dest, key, 4);

	dest[0] &= 0x0fffffff; dest[1] &= 0x0ffffffc;
	dest[2] &= 0x0ffffffc; dest[3] &= 0x0ffffffc;
	dest[4] = 0;
}

void
poly1305_pad(u32 *dest, byte *msg, int n)
{
	byte pad[20];
	int i;

	for (i = 0; i < n; i++) {
		pad[i] = msg[i];
	}

	pad[i++] = 1;

	for (; i < 20; i++) {
		pad[i] = 0;
	}

	poly1305_load(dest, pad, 5);
}

void
poly1305(byte *mac, byte *key, byte *msg, int len)
{
	u32 a[5] = {0, 0, 0, 0, 0};
	u32 r[5];
	u32 s[5];
	int i;

	poly1305_truncate(r, key);

	for (i = 0; i <= len; i += 16) {
		if (len - i >= 16) {
			poly1305_pad(s, msg + i, 16);
		} else {
			poly1305_pad(s, msg + i, len - i);
		}

		poly1305_add(a, s);

		poly1305_mul(a, r);
	}

	poly1305_load(s, key + 16, 4);
	poly1305_add(a, s);

	poly1305_digest(mac, a);
}
