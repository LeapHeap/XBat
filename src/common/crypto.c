#include "crypto.h"
#include <windows.h>


void RC4_Init(RC4_CTX *ctx, const unsigned char *key, int key_len) {
	int i, j = 0;
	unsigned char temp;
	for (i = 0; i < 256; i++) ctx->s[i] = i;
	for (i = 0; i < 256; i++) {
		j = (j + ctx->s[i] + key[i % key_len]) % 256;
		temp = ctx->s[i];
		ctx->s[i] = ctx->s[j];
		ctx->s[j] = temp;
	}
	ctx->i = ctx->j = 0;
}

void RC4_Process(RC4_CTX *ctx, unsigned char *data, int data_len) {
	int i = ctx->i, j = ctx->j;
	unsigned char temp, k;
	for (int n = 0; n < data_len; n++) {
		i = (i + 1) % 256;
		j = (j + ctx->s[i]) % 256;
		temp = ctx->s[i];
		ctx->s[i] = ctx->s[j];
		ctx->s[j] = temp;
		k = ctx->s[(ctx->s[i] + ctx->s[j]) % 256];
		data[n] ^= k;
	}
	ctx->i = i; ctx->j = j;
}

unsigned int CalculateCRC32(const unsigned char *data, int len) {
	unsigned int crc = 0xFFFFFFFF;
	for (int i = 0; i < len; i++) {
		crc ^= data[i];
		for (int j = 0; j < 8; j++) {
			if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
			else crc >>= 1;
		}
	}
	return ~crc;
}
