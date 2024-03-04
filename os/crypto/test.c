#define ROL32(x, k)	(((x) << (k)) | ((x) >> (32 - (k))))
#define ROL64(x, k)	(((x) << (k)) | ((x) >> (64 - (k))))

#define ROR32(x, k)	(((x) >> (k)) | ((x) << (32 - (k))))
#define ROR64(x, k)	(((x) >> (k)) | ((x) << (64 - (k))))

typedef unsigned char byte;
typedef unsigned int u32;
typedef unsigned long u64;

#include "chacha20.c"
#include "poly1305.c"
#include "sha1.c"
#include "sha256.c"
#include "sha512.c"
#include "aes.c"
#include "cv25519.c"
#include "rsa.c"

#include <stdio.h>
#include <string.h>

void
dump32(u32 *ptr, int n)
{
	int i, j;

	for (i = 0; i < n; i += 8) {
		printf("%03d  ", i);

		for (j = i; j < i + 8 && j < n; j++) {
			printf("%08x ", ptr[j]);
		}

		printf("\n");
	}
}

void
dump(byte *ptr, int n)
{
	int i, j;
	byte *s;

	for (i = 0, s = ptr; i < n; i += 16) {
		printf("%03d  ", i);

		for (j = i; j < i + 16 && j < n; j++) {
			printf("%02x ", (int)s[j]);
		}

		for (; j < i + 16; j++) {
			printf("   ");
		}

		printf(" ");

		for (j = i; j < i + 16 && j < n; j++) {
			printf("%c", s[j] >= ' ' && s[j] < 127 ? s[j] : '.');
		}

		printf("\n");
	}
}

int
test_chacha20(void)
{
	byte key[32] = {
		0, 1, 2, 3, 4, 5, 6, 7,
		8, 9, 10, 11, 12, 13, 14, 15,
		16, 17, 18, 19, 20, 21, 22, 23,
		24, 25, 26, 27, 28, 29, 30, 31
	};
	u64 index = 64;
	byte nonce[12] = {0, 0, 0, 0, 0, 0, 0, 0x4a, 0, 0, 0, 0};
	byte plain[114] = "Ladies and Gentl" "emen of the clas"
		"s of '99: If I c" "ould offer you o"
		"nly one tip for " "the future, suns"
		"creen would be i" "t.";
	byte cipher[114];
	byte expected[114] = {
		0x6e, 0x2e, 0x35, 0x9a, 0x25, 0x68, 0xf9, 0x80,
		0x41, 0xba, 0x07, 0x28, 0xdd, 0x0d, 0x69, 0x81,
		0xe9, 0x7e, 0x7a, 0xec, 0x1d, 0x43, 0x60, 0xc2,
		0x0a, 0x27, 0xaf, 0xcc, 0xfd, 0x9f, 0xae, 0x0b,
		0xf9, 0x1b, 0x65, 0xc5, 0x52, 0x47, 0x33, 0xab,
		0x8f, 0x59, 0x3d, 0xab, 0xcd, 0x62, 0xb3, 0x57,
		0x16, 0x39, 0xd6, 0x24, 0xe6, 0x51, 0x52, 0xab,
		0x8f, 0x53, 0x0c, 0x35, 0x9f, 0x08, 0x61, 0xd8,
		0x07, 0xca, 0x0d, 0xbf, 0x50, 0x0d, 0x6a, 0x61,
		0x56, 0xa3, 0x8e, 0x08, 0x8a, 0x22, 0xb6, 0x5e,
		0x52, 0xbc, 0x51, 0x4d, 0x16, 0xcc, 0xf8, 0x06,
		0x81, 0x8c, 0xe9, 0x1a, 0xb7, 0x79, 0x37, 0x36,
		0x5a, 0xf9, 0x0b, 0xbf, 0x74, 0xa3, 0x5b, 0xe6,
		0xb4, 0x0b, 0x8e, 0xed, 0xf2, 0x78, 0x5e, 0x42,
		0x87, 0x4d
	};

	printf("# key\n");
	dump(key, sizeof(key));

	printf("# nonce\n");
	dump(nonce, sizeof(nonce));

	printf("# plain\n");
	dump(plain, sizeof(plain));

	chacha20_stream(cipher, plain, 114, &index, key, nonce);

	printf("# cipher\n");
	dump(cipher, sizeof(cipher));

	return memcmp(cipher, expected, sizeof(expected)) != 0;
}

int
test_poly1305(void)
{
	byte msg[34] = "Cryptographic Forum Research Group";
	byte key[32] = {
		0x85, 0xd6, 0xbe, 0x78, 0x57, 0x55, 0x6d, 0x33,
		0x7f, 0x44, 0x52, 0xfe, 0x42, 0xd5, 0x06, 0xa8,
		0x01, 0x03, 0x80, 0x8a, 0xfb, 0x0d, 0xb2, 0xfd,
		0x4a, 0xbf, 0xf6, 0xaf, 0x41, 0x49, 0xf5, 0x1b
	};
	byte mac[16];
	byte expected[16] = {
		0xa8, 0x06, 0x1d, 0xc1, 0x30, 0x51, 0x36, 0xc6,
		0xc2, 0x2b, 0x8b, 0xaf, 0x0c, 0x01, 0x27, 0xa9
	};

	printf("# key\n");
	dump(key, sizeof(key));

	printf("# msg\n");
	dump(msg, sizeof(msg));

	poly1305(mac, key, msg, sizeof(msg));

	printf("# mac\n");
	dump(mac, sizeof(mac));

	return memcmp(mac, expected, sizeof(expected)) != 0;
}

int
test_sha1(void)
{
	byte data[43] = "The quick brown fox jumps over the lazy dog";
	byte digest[20];
	byte expected[20] = {
		0x2f, 0xd4, 0xe1, 0xc6, 0x7a, 0x2d, 0x28, 0xfc,
		0xed, 0x84, 0x9e, 0xe1, 0xbb, 0x76, 0xe7, 0x39,
		0x1b, 0x93, 0xeb, 0x12
	};

	printf("# data\n");
	dump(data, sizeof(data));

	sha1(digest, data, sizeof(data));

	printf("# sha1\n");
	dump(digest, sizeof(digest));

	return memcmp(digest, expected, sizeof(expected)) != 0;
}

int
test_sha256(void)
{
	byte data[43] = "The quick brown fox jumps over the lazy dog";
	byte digest[32];
	byte expected[32] = {
		0xd7, 0xa8, 0xfb, 0xb3, 0x07, 0xd7, 0x80, 0x94,
		0x69, 0xca, 0x9a, 0xbc, 0xb0, 0x08, 0x2e, 0x4f,
		0x8d, 0x56, 0x51, 0xe4, 0x6d, 0x3c, 0xdb, 0x76,
		0x2d, 0x02, 0xd0, 0xbf, 0x37, 0xc9, 0xe5, 0x92
	};

	printf("# data\n");
	dump(data, sizeof(data));

	sha256(digest, data, sizeof(data));

	printf("# sha256\n");
	dump(digest, sizeof(digest));

	return memcmp(digest, expected, sizeof(expected)) != 0;
}

int
test_sha512(void)
{
	byte data[43] = "The quick brown fox jumps over the lazy dog";
	byte digest[64];
	byte expected[64] = {
		0x07, 0xe5, 0x47, 0xd9, 0x58, 0x6f, 0x6a, 0x73,
		0xf7, 0x3f, 0xba, 0xc0, 0x43, 0x5e, 0xd7, 0x69,
		0x51, 0x21, 0x8f, 0xb7, 0xd0, 0xc8, 0xd7, 0x88,
		0xa3, 0x09, 0xd7, 0x85, 0x43, 0x6b, 0xbb, 0x64,
		0x2e, 0x93, 0xa2, 0x52, 0xa9, 0x54, 0xf2, 0x39,
		0x12, 0x54, 0x7d, 0x1e, 0x8a, 0x3b, 0x5e, 0xd6,
		0xe1, 0xbf, 0xd7, 0x09, 0x78, 0x21, 0x23, 0x3f,
		0xa0, 0x53, 0x8f, 0x3d, 0xb8, 0x54, 0xfe, 0xe6
	};

	printf("# data\n");
	dump(data, sizeof(data));

	sha512(digest, data, sizeof(data));

	printf("# sha512\n");
	dump(digest, sizeof(digest));

	return memcmp(digest, expected, sizeof(expected)) != 0;
}

int
test_aes(void)
{
	byte key[16] = {
		0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
		0xab, 0xf7, 0x15, 0x88, 0x09, 0xcf, 0x4f, 0x3c
	};
	byte plain[16] = {
		0x32, 0x43, 0xf6, 0xa8, 0x88, 0x5a, 0x30, 0x8d,
		0x31, 0x31, 0x98, 0xa2, 0xe0, 0x37, 0x07, 0x34,
	};
	byte cipher[16];
	byte expected[16] = {
		0x39, 0x25, 0x84, 0x1d, 0x02, 0xdc, 0x09, 0xfb,
		0xdc, 0x11, 0x85, 0x97, 0x19, 0x6a, 0x0b, 0x32
	};
	byte w[176];

	printf("# key\n");
	aes_expand(w, key);

	dump(key, sizeof(key));

	printf("# plain\n");
	dump(plain, sizeof(plain));

	aes_cipher(cipher, plain, w);

	printf("# cipher\n");
	dump(cipher, sizeof(cipher));

	return memcmp(cipher, expected, sizeof(expected)) != 0;
}

int
test_cv25519(void)
{
	u32 r[16];
	u32 p[16] = {
		9, 0, 0, 0, 0, 0, 0, 0,
		0x7eced3d9, 0x29e9c5a2, 0x6d7c61b2, 0x923d4d7e,
		0x7748d14c, 0xe01edd2c, 0xb8a086b4, 0x20ae19a1
	};
	u32 k[8] = {1, 0, 0, 0, 0, 0, 0, 0};

	printf("# cv25519\n");
	cv25519_pk(r, p, k);
	dump32(r, 16);

	return 0;
}

int
test_rsa(void)
{
	u32 r[17];
	u32 t[17];
	u32 m[8] = {
		1, 0, 0, 0, 0, 0, 0, 1
	};
	u32 d[8] = {
		1, 0, 0, 0, 0, 0, 0, 1
	};
	u32 x[16] = {
		2, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0
	};

	printf("# rsa\n");
	rsa_pow(r, x, d, m, t, 8);

	dump32(r, 16);

	return 0;
}

int
main(int argc, char **argv)
{
	int ret, status;
	(void)argc; (void)argv;

	status = 0;

	ret = test_chacha20();
	status |= ret;
	if (ret) {
		printf("FAIL: test_chacha20\n");
	}

	ret = test_poly1305();
	status |= ret;
	if (ret) {
		printf("FAIL: test_poly1305\n");
	}

	ret = test_sha1();
	status |= ret;
	if (ret) {
		printf("FAIL: test_sha1\n");
	}

	ret = test_sha256();
	status |= ret;
	if (ret) {
		printf("FAIL: test_sha256\n");
	}

	ret = test_sha512();
	status |= ret;
	if (ret) {
		printf("FAIL: test_sha512\n");
	}

	ret = test_aes();
	status |= ret;
	if (ret) {
		printf("FAIL: test_aes\n");
	}

	ret = test_cv25519();
	status |= ret;
	if (ret) {
		printf("FAIL: test_x25519\n");
	}

	ret = test_rsa();
	status |= ret;
	if (ret) {
		printf("FAIL: test_rsa\n");
	}

	return status;
}
