#include <iostream>
#include <vector>
#include <memory>

#include "../memlz.h"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

int main(int argc, char* argv[]) {
    const size_t packet_len = 1024 * 1024;
    std::vector<char> in(memlz_max_compressed_len(packet_len));
    std::vector<char> out(memlz_max_compressed_len(packet_len));

    auto state = std::make_unique<memlz_state>();
    memlz_reset(state.get());

#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

   if (argc == 2 && argv[1][0] == 'c') {
        size_t r;

        while ((r = fread(in.data(), 1, packet_len, stdin))) {
            size_t w = memlz_stream_compress(out.data(), in.data(), r, state.get());
            fwrite(out.data(), 1, w, stdout);
        }
    }
    else if (argc == 2 && argv[1][0] == 'd') {
		size_t header = memlz_header_len();
		
        while(fread(in.data(), 1, header, stdin)) {
            size_t len = memlz_compressed_len(in.data());
            assert(fread(in.data() + header, 1, len - header, stdin) == len - header);
			
            size_t decompressed = memlz_stream_decompress(out.data(), in.data(), state.get());
            assert(decompressed);			
			
            fwrite(out.data(), 1, decompressed, stdout);
        }
    }
    else {
        std::cerr << "Compress: demo c < infile > outfile\nDecompress: demo d < infile > outfile\n";
    }
}