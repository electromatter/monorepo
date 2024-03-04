/* https://www.rfc-editor.org/rfc/rfc4634 */

u32 sha256_h0[8] = {
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
	0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

void
sha256_init(u32 *r)
{
	int i;

	for (i = 0; i < 8; i++) {
		r[i] = sha256_h0[i];
	}
}

u32 sha256_k[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

void
sha256_rounds(u32 *r, byte *block)
{
	u32 *k = sha256_k;
	u32 w[64];
	u32 a; u32 b; u32 c; u32 d; u32 e; u32 f; u32 g; u32 h;
	u32 s0; u32 s1; u32 t1; u32 t2; u32 maj; u32 ch;
	int i;

	a = r[0]; b = r[1]; c = r[2]; d = r[3];
	e = r[4]; f = r[5]; g = r[6]; h = r[7];

	for (i = 0; i < 16; i++, block += 4) {
		w[i] = ((u32)block[0] << 24)
			| ((u32)block[1] << 16)
			| ((u32)block[2] << 8)
			| block[3];
	}

	for (i = 16; i < 64; i++) {
		s0 = ROR32(w[i - 15], 7)
			^ ROR32(w[i - 15], 18)
			^ (w[i - 15] >> 3);
		s1 = ROR32(w[i - 2], 17)
			^ ROR32(w[i - 2], 19)
			^ (w[i - 2] >> 10);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}

	for (i = 0; i < 64; i++) {
		s1 = ROR32(e, 6) ^ ROR32(e, 11) ^ ROR32(e, 25);
		ch = (e & f) ^ ((~e) & g);
		t1 = h + s1 + ch + k[i] + w[i];
		s0 = ROR32(a, 2) ^ ROR32(a, 13) ^ ROR32(a, 22);
		maj = (a & b) ^ (a & c) ^ (b & c);
		t2 = s0 + maj;

		h = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}

	r[0] += a; r[1] += b; r[2] += c; r[3] += d;
	r[4] += e; r[5] += f; r[6] += g; r[7] += h;
}

void
sha256_finish(u32 *r, int nblocks, byte *data, int len)
{
	byte final[64];
	u64 pad;
	int i;

	pad = (u64)nblocks * 512;
	for (; len >= 64; data += 64, len -= 64, pad += 512) {
		sha256_rounds(r, data);
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
		sha256_rounds(r, final);

		for (i = 0; i < 64; i++) {
			final[i] = 0;
		}
	}

	for (i = 63; i >= 56; i--, pad >>= 8) {
		final[i] = pad;
	}

	sha256_rounds(r, final);
}

void
sha256_digest(byte *digest, u32 *r)
{
	int i;

	for (i = 0; i < 8; i++) {
		*digest++ = r[i] >> 24;
		*digest++ = r[i] >> 16;
		*digest++ = r[i] >> 8;
		*digest++ = r[i];
	}
}

void
sha256(byte *digest, byte *data, int dlen)
{
	u32 r[8];

	sha256_init(r);
	sha256_finish(r, 0, data, dlen);
	sha256_digest(digest, r);
}

void
sha256_hmac(byte *mac, byte *key, int klen, byte *data, int dlen)
{
	byte digest[64];
	byte ipad[64];
	byte opad[64];
	u32 r[8];
	int i;

	for (i = 0; i < 64; i++) {
		digest[i] = 0;
	}

	if (klen <= 64) {
		for (i = 0; i < klen; i++) {
			digest[i] = key[i];
		}
	} else {
		sha256(digest, key, klen);
	}

	for(i = 0; i < 64; i++) {
		ipad[i] = digest[i] ^ 0x36;
	}

	for(i = 0; i < 64; i++) {
		opad[i] = digest[i] ^ 0x5c;
	}

	sha256_init(r);
	sha256_rounds(r, ipad);
	sha256_finish(r, 1, data, dlen);
	sha256_digest(digest, r);

	sha256_init(r);
	sha256_rounds(r, opad);
	sha256_finish(r, 1, digest, 32);
	sha256_digest(mac, r);
}
