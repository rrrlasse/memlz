#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

#include "../memlz.h"

#define MALLOC(var, len) char* var = (char*)malloc(len); if(!var) { return 0;}
#define REALLOC(var, len) var = (char*)realloc(var, len); if(!var) { return 0;}

size_t max_original_len = 1024 * 1024;

int main(int argc, char* argv[]) {
	MALLOC(original, max_original_len)
    size_t original_len = fread(original, 1, max_original_len, stdin);
	REALLOC(original, original_len)
	MALLOC(compressed, memlz_max_compressed_len(original_len))
    size_t compressed_len = memlz_compress(compressed, original, original_len);

	if(memlz_compressed_len(compressed) != compressed_len) {
		fprintf(stderr, "crashing at line %d\n", __LINE__);
		abort();
	}
	
	if(memlz_decompressed_len(compressed) != original_len) {
		fprintf(stderr, "crashing at line %d\n", __LINE__);
		abort();
	}

	// For generating compressed test files
	if (argc == 2 && argv[1][0] == 'c') {
		fwrite(compressed, 1, compressed_len, stdout);
		return 0;
	}

	MALLOC(decompressed, original_len)
	size_t decompressed_len = memlz_decompress(decompressed, compressed);

	if(decompressed_len != original_len) {
		fprintf(stderr, "crashing at line %d\n", __LINE__);
		abort();
	}

	if(memcmp(original, decompressed, original_len)) {
		fprintf(stderr, "crashing at line %d\n", __LINE__);
		abort();
	}

	fprintf(stderr, "rundtrip ok\n");

	if(original_len < memlz_header_len()) {
		printf("stdin detected as invalid\n");	
		return 0;
	}

	if(original_len != memlz_compressed_len(original)) {
		printf("stdin detected as invalid\n");	
		return 0;
	}

	decompressed_len = memlz_decompressed_len(original);

	if(decompressed_len <= max_original_len) {
		MALLOC(decompressed, decompressed_len)
		size_t ret = memlz_decompress(decompressed, original);
		
		if(ret) {
			if(ret != decompressed_len) {
				fprintf(stderr, "crashing at line %d\n", __LINE__);
				abort();
			}
			else {
				fprintf(stderr, "stdin detected as valid\n");
			}
		}
		else {
			fprintf(stderr, "stdin detected as invalid\n");			
		}
	}

    return 0;
}
