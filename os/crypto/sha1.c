/* https://www.rfc-editor.org/rfc/rfc3174 */

u32 sha1_h0[5] = {
	0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0,
};

void
sha1_init(u32 *r)
{
	int i;

	for (i = 0; i < 5; i++) {
		r[i] = sha1_h0[i];
	}
}

void
sha1_rounds(u32 *r, byte *block)
{
	u32 w[80];
	u32 a; u32 b; u32 c; u32 d; u32 e;
	u32 t; u32 f; u32 k;
	int i;

	a = r[0]; b = r[1]; c = r[2]; d = r[3]; e = r[4];

	for (i = 0; i < 16; i++, block += 4) {
		w[i] = ((u32)block[0] << 24)
			| ((u32)block[1] << 16)
			| ((u32)block[2] << 8)
			| block[3];
	}

	for (i = 16; i < 80; i++) {
		t = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
		w[i] = ROL32(t, 1);
	}

	for (i = 0; i < 20; i++) {
		f = (b & c) | (~b & d);
		k = 0x5a827999;
		t = ROL32(a, 5) + f + e + k + w[i];
		e = d; d = c; c = ROL32(b, 30); b = a; a = t;
	}

	for (; i < 40; i++) {
		f = b ^ c ^ d;
		k = 0x6ed9eba1;
		t = ROL32(a, 5) + f + e + k + w[i];
		e = d; d = c; c = ROL32(b, 30); b = a; a = t;
	}

	for (; i < 60; i++) {
		f = (b & c) | (b & d) | (c & d);
		k = 0x8f1bbcdc;
		t = ROL32(a, 5) + f + e + k + w[i];
		e = d; d = c; c = ROL32(b, 30); b = a; a = t;
	}

	for (; i < 80; i++) {
		f = b ^ c ^ d;
		k = 0xca62c1d6;
		t = ROL32(a, 5) + f + e + k + w[i];
		e = d; d = c; c = ROL32(b, 30); b = a; a = t;
	}

	r[0] += a; r[1] += b; r[2] += c; r[3] += d; r[4] += e;
}

void
sha1_finish(u32 *r, int nblocks, byte *data, int len)
{
	byte final[64];
	u64 pad;
	int i;

	pad = (u64)nblocks * 512;
	for (; len >= 64; data += 64, len -= 64, pad += 512) {
		sha1_rounds(r, data);
	}
	pad += len * 8;

	for (i = 0; i < len; i++) {
		final[i] = data[i];
	}

	final[i++] = 0x80;

	for (; i < 64; i++) {
		final[i] = 0;
	}

	if (len + 9 > 64) {
		sha1_rounds(r, final);

		for (i = 0; i < 64; i++) {
			final[i] = 0;
		}
	}

	for (i = 63; i >= 56; i--, pad >>= 8) {
		final[i] = pad;
	}

	sha1_rounds(r, final);
}

void
sha1_digest(byte *digest, u32 *r)
{
	int i;

	for (i = 0; i < 5; i++) {
		*digest++ = r[i] >> 24;
		*digest++ = r[i] >> 16;
		*digest++ = r[i] >> 8;
		*digest++ = r[i];
	}
}

void
sha1(byte *digest, byte *data, int dlen)
{
	u32 r[5];

	sha1_init(r);
	sha1_finish(r, 0, data, dlen);
	sha1_digest(digest, r);
}
