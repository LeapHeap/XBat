#ifndef CRYPTO_H
#define CRYPTO_H

typedef struct {
	unsigned char s[256];
	int i, j;
} RC4_CTX;

#ifdef __cplusplus
extern "C" {
#endif
void RC4_Init(RC4_CTX *ctx, const unsigned char *key, int key_len);
void RC4_Process(RC4_CTX *ctx, unsigned char *data, int data_len);
unsigned int CalculateCRC32(const unsigned char *data, int len);
#ifdef __cplusplus
}
#endif

#endif
