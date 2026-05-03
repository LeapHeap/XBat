#include "stub_full.h"
#include <stdlib.h>
#include <windows.h>
#include <tchar.h>
#include "../../common/lzma_sdk/lzmadec/LzmaDec.h"


// LZMA sdk allocator
static void* SzAlloc(ISzAllocPtr p, size_t size) { return malloc(size); }
static void SzFree(ISzAllocPtr p, void* address) { free(address); }
static ISzAlloc g_LzmaAlloc = { SzAlloc, SzFree };

BYTE* XBat_DecompressBuffer(const BYTE* pCompressedData, DWORD dwCompressedSize, DWORD dwExpectedSize) {
	if (!pCompressedData || dwCompressedSize <= LZMA_PROPS_SIZE) {
		return NULL;
	}
	
	BYTE* pDecompressedBuf = (BYTE*)malloc(dwExpectedSize);
	if (!pDecompressedBuf) return NULL;
	
	SizeT destLen = (SizeT)dwExpectedSize;
	SizeT srcLen = (SizeT)(dwCompressedSize - LZMA_PROPS_SIZE);
	ELzmaStatus status;

	SRes res = LzmaDecode(
						  pDecompressedBuf, &destLen,
						  pCompressedData + LZMA_PROPS_SIZE, &srcLen,
						  pCompressedData, LZMA_PROPS_SIZE,
						  LZMA_FINISH_ANY, &status, &g_LzmaAlloc
						  );
	
	if (res != SZ_OK || destLen != (SizeT)dwExpectedSize) {
		free(pDecompressedBuf);
		return NULL;
	}
	
	return pDecompressedBuf;
}






