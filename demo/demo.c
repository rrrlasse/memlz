#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "../memlz.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif


int main(int argc, char* argv[]) {
    size_t max = 100 * 1024 * 1024;

    if (argc == 4 && argv[1][0] == 'c') {
        void* in = malloc(max);
        FILE* f = fopen(argv[2], "rb");
        size_t r = fread(in, 1, max, f);
        void* out = malloc(memlz_max_compressed_len(r));
        size_t w = memlz_compress(out, in, r);
        f = fopen(argv[3], "wb");
        fwrite(out, 1, w, f);
    }
    else if (argc == 4 && argv[1][0] == 'd') {
        void* in = malloc(memlz_max_compressed_len(max));
        FILE* f = fopen(argv[2], "rb");
        fread(in, 1, max, f);
        size_t decompressed_len = memlz_decompressed_len(in);
        void* out = malloc(decompressed_len);
        size_t w = memlz_decompress(out, in);
        f = fopen(argv[3], "wb");
        fwrite(out, 1, w, f);
    }
    else {
       printf("Compress: demo c infile outfile\nDecompress: demo d infile outfile\n");
    }
}