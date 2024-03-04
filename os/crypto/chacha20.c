/* https://www.rfc-editor.org/rfc/rfc7539 */

void
chacha20_qround(u32 *a, u32 *b, u32 *c, u32 *d)
{
	*a += *b; *d ^= *a; *d = ROL32(*d, 16);
	*c += *d; *b ^= *c; *b = ROL32(*b, 12);
	*a += *b; *d ^= *a; *d = ROL32(*d, 8);
	*c += *d; *b ^= *c; *b = ROL32(*b, 7);
}

void
chacha20_load(u32 *dest, byte *src, int n)
{
	int i;

	for (i = 0; i < n; i++, src += 4) {
		*dest++ = src[0]
			| ((u32)src[1] << 8)
			| ((u32)src[2] << 16)
			| ((u32)src[3] << 24);
	}
}

void
chacha20_store(byte *dest, u32 *src, int n)
{
	int i;

	for (i = 0; i < n; i++) {
		*dest++ = *src;
		*dest++ = *src >> 8;
		*dest++ = *src >> 16;
		*dest++ = *src++ >> 24;
	}
}

void
chacha20_block(byte *block, byte *key, u32 counter, byte *nonce)
{
	u32 initial[16];
	u32 s[16];
	int i;

	s[0] = 0x61707865; s[1] = 0x3320646e;
	s[2] = 0x79622d32; s[3] = 0x6b206574;

	chacha20_load(s + 4, key, 8);

	s[12] = counter;

	chacha20_load(s + 13, nonce, 3);

	for (i = 0; i < 16; i++) {
		initial[i] = s[i];
	}

	for (i = 0; i < 10; i++) {
		chacha20_qround(s + 0, s + 4, s + 8, s + 12);
		chacha20_qround(s + 1, s + 5, s + 9, s + 13);
		chacha20_qround(s + 2, s + 6, s + 10, s + 14);
		chacha20_qround(s + 3, s + 7, s + 11, s + 15);
		chacha20_qround(s + 0, s + 5, s + 10, s + 15);
		chacha20_qround(s + 1, s + 6, s + 11, s + 12);
		chacha20_qround(s + 2, s + 7, s + 8, s + 13);
		chacha20_qround(s + 3, s + 4, s + 9, s + 14);
	}

	for (i = 0; i < 16; i++) {
		s[i] += initial[i];
	}

	chacha20_store(block, s, 16);
}

void
chacha20_stream(byte *cipher, byte *plain, int len, u64 *index, byte *key, byte *nonce)
{
	byte block[64];
	int i;
	int j;

	j = 0;

	if (*index & 63) {
		chacha20_block(block, key, *index >> 6, nonce);

		for (i = *index & 63; j < len && i < 64; (*index)++, i++, j++) {
			cipher[j] = plain[j] ^ block[i];
		}
	}

	while (j < len) {
		chacha20_block(block, key, *index >> 6, nonce);

		for (i = 0; j < len && i < 64; i++, j++) {
			cipher[j] = plain[j] ^ block[i];
		}

		*index += 64;
	}
}
