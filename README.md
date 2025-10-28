memlz is a compression library for special use cases where speeds approaching memcpy() are needed.

## Benchmark
Fast libraries like Snappy, FastLZ, LZAV and LZO have better compression ratios but are well below 1000 MB/s and not comparable. Only LZ4 with its acceleration parameter set around 32 to 64 and a few other libraries come close.

Benchmark of the files [Silesia](https://mattmahoney.net/dc/silesia.html) and [employees_50MB.json](https://sample.json-format.com/) on an Intel i7 with a non-cached memcpy() speed of 14000 MB/s:

![Benchmark](https://github.com/rrrlasse/memlz/blob/res/Figure_1.png)
<br>Decompression speed is likewise performant.

## User friendly
It's a header-only library. Simply include it and call `memlz_compress()`:
```
    #include "memlz.h"
    ...
    size_t len = memlz_compress(destination, source, size);
```
With streaming mode you can compress more data than fits in memory, or you increase compression ratio if you recevive data in small packets. Simply create a state variable and call `memlz_stream_compress()` repeatedly:
```
    memlz_state* state = (memlz_state*)malloc(sizeof(memlz_state));
    memlz_reset(state);
    while(...) {
        ...
        size_t len = memlz_stream_compress(destination, source, size, state);
        ...
    }
```
Each call to `memlz_stream_compress()` will compress and return the entire passed payload, which can then be fully decompressed by a single call to `memlz_stream_decompress()`.

The data format also contains a header that can tell the compressed and decompressed sizes:
```
    size_t memlz_compressed_len(source)
    size_t memlz_decompressed_len(source)
```
## Safety
Decompression of corrupted or manipulated data has two guarantees: 1) It will always return in regular time, and 2) No memory access outside the source or destination buffers will take place, according to what memlz_compressed_len() and memlz_decompressed_len() tell.
## No-copy
LZ4 and most other libraries need to maintain an internal payload queue when using streaming mode which adds one additional memcpy() operation. The memlz algorithm eliminates this need.

Let's test the effect by integrating memlz into the eXdupe file archiver in two different ways. eXdupe first performs deduplication and then emits small packets of some kilobytes in size to a traditional data compression library.

If we queue packets with memcpy() until they reach 1 MB and compress them at once we get:
```
F:\eXdupe>exdupe -x1k "f:\vm\25\Ubuntu 64-bit-s003.vmdk" -stdout > NUL
Input:                       4,012,638,208 B in 1 files
Output:                      2,148,219,886 B (53%)
Speed w/o init overhead:     4,368 MB/s
```
If we use streaming mode on each individual packet as they are received and get:
```
F:\eXdupe>exdupe -x1k "f:\vm\25\Ubuntu 64-bit-s003.vmdk" -stdout > NUL
Input:                       4,012,638,208 B in 1 files
Output:                      2,145,241,775 B (53%)
Speed w/o init overhead:     4,616 MB/s
```
The reason we cannot simply compress each small packet individually in non-streaming mode is that it would hurt compression ratio by alot.
## Beta
It's currently the very first beta version and has only been tested on Intel machines. Compatibility may be broken!
