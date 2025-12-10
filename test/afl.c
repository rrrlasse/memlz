#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

#ifdef _WIN32
#include <Windows.h>
#include <io.h>
#include <fcntl.h>
#endif // _WIN32


#include "../memlz.h"

#ifndef __AFL_LOOP
#define __AFL_LOOP(n) abort()
#endif

char* realloc_or_abort(char* ptr, size_t len) {
    char* r = realloc(ptr, len);
    if(!r) {
        fprintf(stderr, "crashing at line %d\n", __LINE__);        
        abort();
    }
    return r;
}

size_t max_original_len = 1024 * 1024;

void afl_round(int argc, char* argv[], char** original, char** compressed, char** decompressed) {
    *original = realloc_or_abort(*original, max_original_len);
    size_t original_len = fread(*original, 1, max_original_len, stdin);
    *original = realloc_or_abort(*original, original_len);
    *compressed = realloc_or_abort(*compressed, memlz_max_compressed_len(original_len));
    size_t compressed_len = memlz_compress(*compressed, *original, original_len);

    if(memlz_compressed_len(*compressed) != compressed_len) {
        fprintf(stderr, "crashing at line %d\n", __LINE__);
        abort();
    }
    
    if(memlz_decompressed_len(*compressed) != original_len) {
        fprintf(stderr, "crashing at line %d\n", __LINE__);
        abort();
    }

    // For generating compressed test files for AFL
    if (argc == 2 && argv[1][0] == 'c') {
        size_t written = fwrite(*compressed, 1, compressed_len, stdout);
        if(written != compressed_len) {
            abort();
        }
        return;
    }

    *decompressed = realloc_or_abort(*decompressed, original_len);
    size_t decompressed_len = memlz_decompress(*decompressed, *compressed);

    if(decompressed_len != original_len) {
        fprintf(stderr, "crashing at line %d\n", __LINE__);
        abort();
    }

    if(memcmp(*original, *decompressed, original_len)) {
        fprintf(stderr, "crashing at line %d\n", __LINE__);
        abort();
    }

    fprintf(stderr, "roundtrip ok\n");

    if(original_len < memlz_header_len()) {
        fprintf(stderr, "stdin detected as invalid\n");    
        return;
    }

    if(original_len != memlz_compressed_len(*original)) {
        fprintf(stderr, "stdin detected as invalid\n");    
        return;
    }

    decompressed_len = memlz_decompressed_len(*original);
    if (decompressed_len == 0) {
        fprintf(stderr, "stdin detected as invalid\n");
        return;
    }

    if (original_len > memlz_max_compressed_len(decompressed_len)) {
        fprintf(stderr, "stdin detected as invalid\n");
        return;
    }

    if(decompressed_len <= max_original_len) {
        *decompressed = realloc_or_abort(*decompressed, decompressed_len);
        size_t ret = memlz_decompress(*decompressed, *original);
        
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
}

int main(int argc, char* argv[]) {
    char* original = 0;
    char* compressed = 0;
    char* decompressed = 0;
        
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif

    // For running manually to investigate a crash
    if (argc == 2 && argv[1][0] == 'm') {
        afl_round(argc, argv, &original, &compressed, &decompressed);
        exit(0);
    }

    // To test if stray reads are detected
    if (argc == 2 && argv[1][0] == 'x') {
        char volatile* c = ((char*)malloc(1024 * 1024 - 1));
        char must_crash = c[1024 * 1024 - 1];
        exit(0);
    }
            
    while (__AFL_LOOP(10000)) {
        afl_round(argc, argv, &original, &compressed, &decompressed);
    }
    
    free(original);
    free(compressed);
    free(decompressed);

    return 0;
}
