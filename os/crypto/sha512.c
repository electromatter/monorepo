/* https://www.rfc-editor.org/rfc/rfc4634 */

u64 sha512_h0[8] = {
	0x6a09e667f3bcc908, 0xbb67ae8584caa73b, 0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
	0x510e527fade682d1, 0x9b05688c2b3e6c1f, 0x1f83d9abfb41bd6b, 0x5be0cd19137e2179
};

void
sha512_init(u64 *r)
{
	int i;

	for (i = 0; i < 8; i++) {
		r[i] = sha512_h0[i];
	}
}

u64 sha512_k[80] = {
	0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc,
	0x3956c25bf348b538, 0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118,
	0xd807aa98a3030242, 0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
	0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235, 0xc19bf174cf692694,
	0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65,
	0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
	0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4,
	0xc6e00bf33da88fc2, 0xd5a79147930aa725, 0x06ca6351e003826f, 0x142929670a0e6e70,
	0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
	0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
	0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30,
	0xd192e819d6ef5218, 0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
	0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8,
	0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3,
	0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
	0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b,
	0xca273eceea26619c, 0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178,
	0x06f067aa72176fba, 0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
	0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc, 0x431d67c49c100d4c,
	0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817
};

void
sha512_rounds(u64 *r, byte *block)
{
	u64 *k = sha512_k;
	u64 w[80];
	u64 a; u64 b; u64 c; u64 d; u64 e; u64 f; u64 g; u64 h;
	u64 s0; u64 s1; u64 t1; u64 t2; u64 maj; u64 ch;
	int i;

	a = r[0]; b = r[1]; c = r[2]; d = r[3];
	e = r[4]; f = r[5]; g = r[6]; h = r[7];

	for (i = 0; i < 16; i++, block += 8) {
		w[i] = ((u64)block[0] << 56)
			| ((u64)block[1] << 48)
			| ((u64)block[2] << 40)
			| ((u64)block[3] << 32)
			| ((u64)block[4] << 24)
			| ((u64)block[5] << 16)
			| ((u64)block[6] << 8)
			| block[7];
	}

	for (i = 16; i < 80; i++) {
		s0 = ROR64(w[i - 15], 1)
			^ ROR64(w[i - 15], 8)
			^ (w[i - 15] >> 7);
		s1 = ROR64(w[i - 2], 19)
			^ ROR64(w[i - 2], 61)
			^ (w[i - 2] >> 6);
		w[i] = w[i - 16] + s0 + w[i - 7] + s1;
	}

	for (i = 0; i < 80; i++) {
		s1 = ROR64(e, 14) ^ ROR64(e, 18) ^ ROR64(e, 41);
		ch = (e & f) ^ ((~e) & g);
		t1 = h + s1 + ch + k[i] + w[i];
		s0 = ROR64(a, 28) ^ ROR64(a, 34) ^ ROR64(a, 39);
		maj = (a & b) ^ (a & c) ^ (b & c);
		t2 = s0 + maj;

		h = g; g = f; f = e; e = d + t1;
		d = c; c = b; b = a; a = t1 + t2;
	}

	r[0] += a; r[1] += b; r[2] += c; r[3] += d;
	r[4] += e; r[5] += f; r[6] += g; r[7] += h;
}

void
sha512_finish(u64 *r, int nblocks, byte *data, int len)
{
	byte final[128];
	u64 pad;
	int i;

	pad = (u64)nblocks * 1024;
	for (; len >= 128; data += 128, len -= 128, pad += 1024) {
		sha512_rounds(r, data);
	}
	pad += len * 8;

	for (i = 0; i < len; i++) {
		final[i] = data[i];
	}

	final[i++] = 0x80;

	for (; i < 128; i++) {
		final[i] = 0;
	}

	if (len + 17 > 128) {
		sha512_rounds(r, final);

		for (i = 0; i < 128; i++) {
			final[i] = 0;
		}
	}

	for (i = 127; i >= 120; i--, pad >>= 8) {
		final[i] = pad;
	}

	sha512_rounds(r, final);
}

void
sha512_digest(byte *digest, u64 *r)
{
	int i;

	for (i = 0; i < 8; i++) {
		*digest++ = r[i] >> 56;
		*digest++ = r[i] >> 48;
		*digest++ = r[i] >> 40;
		*digest++ = r[i] >> 32;
		*digest++ = r[i] >> 24;
		*digest++ = r[i] >> 16;
		*digest++ = r[i] >> 8;
		*digest++ = r[i];
	}
}

void
sha512(byte *digest, byte *data, int dlen)
{
	u64 r[8];

	sha512_init(r);
	sha512_finish(r, 0, data, dlen);
	sha512_digest(digest, r);
}

void
sha512_hmac(byte *mac, byte *key, int klen, byte *data, int dlen)
{
	byte digest[128];
	byte ipad[128];
	byte opad[128];
	u64 r[8];
	int i;

	for (i = 0; i < 128; i++) {
		digest[i] = 0;
	}

	if (klen <= 128) {
		for (i = 0; i < klen; i++) {
			digest[i] = key[i];
		}
	} else {
		sha512(digest, key, klen);
	}

	for(i = 0; i < 128; i++) {
		ipad[i] = digest[i] ^ 0x36;
	}

	for(i = 0; i < 128; i++) {
		opad[i] = digest[i] ^ 0x5c;
	}

	sha512_init(r);
	sha512_rounds(r, ipad);
	sha512_finish(r, 1, data, dlen);
	sha512_digest(digest, r);

	sha512_init(r);
	sha512_rounds(r, opad);
	sha512_finish(r, 1, digest, 64);
	sha512_digest(mac, r);
}
