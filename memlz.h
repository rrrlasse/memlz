// SPDX-License-Identifier: MIT
//
// memlz 0.1 beta - extremely fast header-only compression library for C and C++ on x64/x86
//
// Copyright 2025, Lasse Mikkel Reinhold
//
// Attributions: This is an 8-byte-word version of the Chameleon compression algorithmn
// by Guillaume Voirin. Thanks to Charles Bloom for his simple reference implementation and 
// for the original LZP-style of algorithms.

#ifndef memlz_h
#define memlz_h

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <assert.h>

typedef struct memlz_state memlz_state;

/// Compress non-streaming data. The destination buffer must be at least
/// memlz_max_compressed_len(len) large.
static size_t memlz_compress(void* destination, const void* source, size_t len);

/// Decompress non-streaming data. The destination buffer must be at least 
/// memlz_decompressed_len(source) large.
static size_t memlz_decompress(void* destination, const void* source);

/// Compress stream: First call memlz_reset(state) and then call memlz_compress() repeatedly.
/// Each call will always compress and ouptput the full input data given. There is no flush
/// function.
///
/// The destination buffer must be at least memlz_max_compressed_len(len) large.
static size_t memlz_stream_compress(void* destination, const void* source, size_t len, memlz_state* state);

/// Decompress streaming data: First call memlz_reset(state) and then call
/// memlz_stream_decompress() repeatedly in the same order for the compressed data as when you
/// called memlz_compress(). 
///
/// The destination buffer must be at least memlz_decompressed_len(source) large.
static size_t memlz_stream_decompress(void* destination, const void* source, memlz_state* state);

/// Takes compressed data as input and returns the decompressed len. Only the first
/// memlz_header_len() number of bytes need to present
static size_t memlz_compressed_len(const void* src);

/// Takes compressed data as input and returns the compressed len. Only the first
/// memlz_header_len() number of bytes need to present
static size_t memlz_decompressed_len(const void* src);

/// Return the largest number of bytes that a given input len can compress into. Note that
/// certain kinds of data may grow beyond its original len.
static size_t memlz_max_compressed_len(size_t input);

/// Returns the number of bytes of compressed data that you need to call memlz_compressed_len()
/// and memlz_decompressed_len()
static size_t memlz_header_len();

///  Call this before the first call to memlz_compress() or memlz_decompress()
static void memlz_reset(memlz_state* c);

// The rest of this header file is internals
//////////////////////////////////////////////////////////////////////////////////////////////////

#define MEMLZ_DO_RLE
#define MEMLZ_DO_INCOMPRESSIBLE
#define MEMLZ_INCOMPRESSIBLE 256
#define MEMLZ_PROBELEN (4 * 4096)
#define MEMLZ_BLOCKLEN (256 * 1024)

#define MEMLZ_RESTRICT __restrict

#define MEMLZ_UNROLL4(op) op; op; op; op
#define MEMLZ_UNROLL16(op) op; op; op; op; op; op; op; op; op; op; op; op; op; op; op; op;
#define MEMLZ_NORMAL32 'A'
#define MEMLZ_NORMAL64 'B'
#define MEMLZ_UNCOMPRESSED 'C'
#define MEMLZ_RLE 'D'

static const size_t memlz_fields = 2;
static const size_t memlz_words_per_round = 16;

static uint16_t memlz_hash32(uint32_t v) {
    return (uint16_t)(((v * 2654435761ull) >> 16));
}

static uint16_t memlz_hash64(uint64_t v) {
    return (uint16_t)(((v * 11400714819323198485ull) >> 48));
}


static uint64_t memlz_read(const void* src) {
    uint8_t* s = (uint8_t*)src;
    return *s != 0xff ? *s
        : *(uint16_t*)(s + 1) != 0xffff ? *(uint16_t*)(s + 1)
        : *(uint32_t*)(s + 3) != 0xffffffff ? *(uint32_t*)(s + 3)
        : *(uint64_t*)(s + 7);
}

static size_t memlz_bytes(const void* src) {
    uint8_t* s = (uint8_t*)src;
    return *s != 0xff ? 1 
        : *(uint16_t*)(s + 1) != 0xffff ? 3 
        : *(uint32_t*)(s + 3) != 0xffffffff ? 7 
        : 15;
}

static void memlz_write(void* dst, uint64_t value, size_t bytes) {
    assert(bytes == 1 || bytes == 3 || bytes == 7 || bytes == 15);

    uint8_t* d = (uint8_t*)dst;

    if (bytes == 1) {
        assert(value < 0xff);
        *d = (uint8_t)value;
    }
    else if (bytes == 3) {
        assert(value < 0xffff);
        memset(dst, 0xff, 1);
        *(uint16_t*)(d + 1) = (uint16_t)value;
    }
    else if (bytes == 7) {
        assert(value < 0xffffffff);
        memset(dst, 0xff, 3);
        *(uint32_t*)(d + 3) = (uint32_t)value;
    }
    else if (bytes == 15) {
        memset(dst, 0xff, 7);
        *(uint64_t*)(d + 7) = (uint64_t)value;
    }
}

static uint64_t memlz_fit(uint64_t value) {
    return value < 0xff ? 1 : value < 0xffff ? 3 : value < 0xffffffff ? 7 : 15;
}

typedef struct memlz_state {
    uint64_t hash64[1 << 16];
    uint32_t hash32[1 << 16];
    size_t total_input;
    size_t total_output;
    size_t mod = 0;
    size_t wordlen = 8;
    size_t cs4 = 0;
    size_t cs8 = 0;
    char reset;
} memlz_state;

static size_t memlz_max_compressed_len(size_t input) {
    return 68 * input / 64 + 100; // todo, find real bound
}

static size_t memlz_header_len() {
    return 17;
}

static void memlz_reset(memlz_state* c) {
    memset(c->hash32, 0, sizeof(c->hash32));
    memset(c->hash64, 0, sizeof(c->hash64));
    c->total_input = 0;
    c->total_output = 0;
    c->cs4 = 0;
    c->cs8 = 0;
    c->mod = 0;
    c->wordlen = 8;
    c->reset = 'Y';
}

static size_t memlz_stream_compress(void* MEMLZ_RESTRICT destination, const void* MEMLZ_RESTRICT source, size_t len, memlz_state* state) {
    const size_t max = memlz_max_compressed_len(len) > len ? memlz_max_compressed_len(len) : len;
    const size_t header_len = memlz_fields * memlz_fit(max);
    size_t missing = len;
    const uint8_t* src = (const uint8_t*)source;
    uint8_t* dst = (uint8_t*)destination;
    uint16_t flags = 0;
    dst += header_len;

    for(;;) {
            // Compress 8-byte words, then 4-byte words and compare ratios and select best
            state->mod++;
            if (state->mod == MEMLZ_PROBELEN / 128) {
                state->cs8 = (state->total_output + (dst - (uint8_t*)destination)) - state->cs8;
                state->cs4 = (state->total_output + (dst - (uint8_t*)destination));
                state->wordlen = 4;
            }
            else if (state->mod == 3 * MEMLZ_PROBELEN / 128) {
                state->cs4 = state->total_output + (dst - (uint8_t*)destination) - state->cs4;
                if (state->cs8 < state->cs4) {
                    state->wordlen = 8;
                }
            }
            else if (state->mod == (MEMLZ_BLOCKLEN + MEMLZ_PROBELEN) / 128) {
                state->wordlen = 8;
                state->mod = 0;
                state->cs8 = state->total_output + (dst - (uint8_t*)destination);
                state->cs4 = 0;
            }

#ifdef MEMLZ_DO_RLE
        {
            typedef uint64_t memlz_rle;
            size_t e = 1;
            while (e < missing / sizeof(memlz_rle) && ((memlz_rle*)src)[e] == *(memlz_rle*)src) {
                e++;
            }
            if (e >= 4) {
                *dst++ = MEMLZ_RLE;
                size_t length = memlz_fit(e);
                memlz_write(dst, e, length);
                *(memlz_rle*)(dst + length) = *(memlz_rle*)src;
                dst += sizeof(memlz_rle) + length;
                missing -= e * sizeof(memlz_rle);
                src += e * sizeof(memlz_rle);
                continue;
            }
        }
#endif
        {

            *dst++ = state->wordlen == 8 ? MEMLZ_NORMAL64 : MEMLZ_NORMAL32;
            if (missing < 16 * state->wordlen) {
                break;
            }

            uint16_t* flags_ptr = (uint16_t*)dst;
            dst += 2;

            #define MEMLZ_ENCODE_WORD(tbl, typ, h, i, next) \
            flags <<= 1; \
            if (tbl[h] == ((typ*)src)[i]) { \
                flags |= 1; \
                *(uint16_t*)dst = (uint16_t)h; \
                dst += 2; \
                next; \
            } else { \
                tbl[h] = ((typ*)src)[i]; \
                *(typ*)dst = ((typ*)src)[i]; \
                dst += sizeof(typ); \
                next; \
            }

            #define MEMLZ_ENCODE4(tbl, typ, a, b, c, d) MEMLZ_ENCODE_WORD \
                    (tbl, typ, a, 0, MEMLZ_ENCODE_WORD \
                    (tbl, typ, b, 1, MEMLZ_ENCODE_WORD \
                    (tbl, typ, c, 2, MEMLZ_ENCODE_WORD \
                    (tbl, typ, d, 3, ))))

            
            if (state->wordlen == 8) {
                uint64_t a, b, c, d;
                MEMLZ_UNROLL4(\
                    a = memlz_hash64(((uint64_t*)src)[0]); \
                    b = memlz_hash64(((uint64_t*)src)[1]); \
                    c = memlz_hash64(((uint64_t*)src)[2]); \
                    d = memlz_hash64(((uint64_t*)src)[3]); \
                    MEMLZ_ENCODE4(state->hash64, uint64_t, a, b, c, d); \
                    src += 4 * sizeof(uint64_t);
                )
            }
            else {
                uint32_t a, b, c, d;
                MEMLZ_UNROLL4(\
                    a = memlz_hash32(((uint32_t*)src)[0]); \
                    b = memlz_hash32(((uint32_t*)src)[1]); \
                    c = memlz_hash32(((uint32_t*)src)[2]); \
                    d = memlz_hash32(((uint32_t*)src)[3]); \
                    MEMLZ_ENCODE4(state->hash32, uint32_t, a, b, c, d); \
                    src += 4 * sizeof(uint32_t);
                )
            }

            *flags_ptr = (uint16_t)flags;
            missing -= 16 * state->wordlen;
        }

#ifdef MEMLZ_DO_INCOMPRESSIBLE
        {
            size_t u = MEMLZ_INCOMPRESSIBLE;
            if (flags == 0 && (char*)src - (char*)source + state->total_input >= 4 * 128 && missing >= u) {
                *dst++ = MEMLZ_UNCOMPRESSED;
                *(uint16_t*)dst = (uint16_t)u;
                memlz_write(dst, u, memlz_fit(u));
                dst += memlz_fit(u);
                memcpy(dst, src, u);
                dst += u;
                src += u;
                missing -= u;
            }
        }
#endif
    }
  
    if (missing >= state->wordlen) {
        uint16_t* flag_ptr = (uint16_t*)dst;
        dst += 2;
        flags = 0;
        int flags_left = memlz_words_per_round;

        while (missing >= state->wordlen) {

            if (state->wordlen == 8) {
                uint64_t a = memlz_hash64(*(uint64_t*)src);
                MEMLZ_ENCODE_WORD(state->hash64, uint64_t, a, 0, )
            }
            else {
                uint32_t a = memlz_hash32(*(uint32_t*)src);
                MEMLZ_ENCODE_WORD(state->hash32, uint32_t, a, 0, )
            }

            src += state->wordlen;
            flags_left--;
            missing -= state->wordlen;
        }

        flags <<= flags_left;
        *flag_ptr = flags;
    }

    size_t tail_count = missing;
    memcpy(dst, src, tail_count);
    dst += tail_count;

    size_t compressed_len = size_t(dst - (uint8_t*)destination);
    if (compressed_len < memlz_header_len()) {
        memset(dst, 'M', memlz_header_len() - compressed_len);
        compressed_len = memlz_header_len();
    }

    memlz_write(destination, len, header_len / memlz_fields);
    memlz_write((uint8_t*)destination + header_len / memlz_fields, compressed_len, header_len / memlz_fields);

    state->total_input += len;
    state->total_output += compressed_len;

    return compressed_len;
}


static size_t memlz_decompressed_len(const void* src) {
    return memlz_read(src);
}


static size_t memlz_compressed_len(const void* src) {
    size_t header_field_len = memlz_bytes(src);
    return memlz_read((uint8_t*)src + header_field_len);
}


static size_t memlz_stream_decompress(void* MEMLZ_RESTRICT destination, const void* MEMLZ_RESTRICT source, memlz_state* state) {
    const size_t decompressed_len = memlz_decompressed_len(source);
    const size_t compressed_len = memlz_compressed_len(source);
    size_t header_length = memlz_bytes(source) * memlz_fields;
    const uint8_t* src = (const uint8_t*)source + header_length;
    uint8_t* dst = (uint8_t*)destination;
    size_t missing = decompressed_len;
    uint8_t blocktype = 0;
    size_t memlz_wordlen = 8;

    for (;;)
    {
        blocktype = *(uint8_t*)src++;

#ifdef MEMLZ_DO_INCOMPRESSIBLE
        if (blocktype == MEMLZ_UNCOMPRESSED) {
            size_t unc = memlz_read(src);
            src += memlz_bytes(src);
            memcpy(dst, src, unc);
            dst += unc;
            src += unc;
            missing -= unc;
            continue;
        }
#endif

#ifdef MEMLZ_DO_RLE
        if (blocktype == MEMLZ_RLE) {
            typedef uint64_t rle;
            size_t len = memlz_bytes(src);
            uint64_t z = memlz_read(src);
            src += len;

            uint64_t v = *((rle*)src);
            src += sizeof(rle);

            for (uint64_t n = 0; n < z; n++) {
                ((rle*)dst)[n] = v;
            }
            dst += z * sizeof(rle);
            missing -= z * sizeof(rle);
            continue;
        }
#endif
        memlz_wordlen = blocktype == MEMLZ_NORMAL64 ? 8 : 4;

        if (missing < memlz_wordlen * 16) {
            break;
        }

        const uint16_t flags = *(uint16_t*)src;
        src += 2;
        int bitpos = memlz_words_per_round - 1;

#define MEMLZ_DECODE_WORD(h, tbl, typ) \
        if (flags & (1 << bitpos)) { \
            word = tbl[*(uint16_t*)src]; \
            src += 2; \
        } else { \
            word = *((const typ*)src); \
            src += sizeof(typ); \
            tbl[h(word)] = word; \
        } \
        *(typ*)dst = word; \
        dst += sizeof(typ); \
        bitpos--;

        if (blocktype == MEMLZ_NORMAL64) {
            uint64_t word;
            MEMLZ_UNROLL16(MEMLZ_DECODE_WORD(memlz_hash64, state->hash64, uint64_t))
                missing -= 16 * sizeof(uint64_t);
        }
        else if (blocktype == MEMLZ_NORMAL32) {
            uint32_t word;
            MEMLZ_UNROLL16(MEMLZ_DECODE_WORD(memlz_hash32, state->hash32, uint32_t))
                missing -= 16 * sizeof(uint32_t);
        }
        else {
            return 0;
        }
    }

    if (missing >= memlz_wordlen) {
        const uint16_t flags = *(uint16_t*)src;
        src += 2;

        int bitpos = memlz_words_per_round - 1;
        while (missing >= memlz_wordlen) {
            if (memlz_wordlen == 8) {
                uint64_t word;
                MEMLZ_DECODE_WORD(memlz_hash64, state->hash64, uint64_t)
            }
            else {
                uint32_t word;
                MEMLZ_DECODE_WORD(memlz_hash32, state->hash32, uint32_t)
            }
            missing -= memlz_wordlen;
        }
    }


    size_t tail_count = missing;
    memcpy(dst, src, tail_count);

    state->total_input += compressed_len;
    state->total_output += decompressed_len;
    return decompressed_len;
}

static size_t memlz_decompress(void* MEMLZ_RESTRICT destination, const void* MEMLZ_RESTRICT source) {
    memlz_state* s = (memlz_state*)malloc(sizeof(memlz_state));
    memlz_reset(s);
    size_t r = memlz_stream_decompress(destination, source, s);
    free(s);
    return r;
}


static size_t memlz_compress(void* MEMLZ_RESTRICT destination, const void* MEMLZ_RESTRICT source, size_t len) { 
    memlz_state* state = (memlz_state*)malloc(sizeof(memlz_state));
    memlz_reset(state);
    size_t r = memlz_stream_compress(destination, source, len, state);
    free(state);
    return r;
}

#undef MEMLZ_UNROLL4
#undef MEMLZ_UNROLL16
#undef MEMLZ_ENCODE_WORD
#undef MEMLZ_DECODE_WORD
#undef MEMLZ_VOID
#undef MEMLZ_NORMAL32
#undef MEMLZ_NORMAL64
#undef MEMLZ_UNCOMPRESSED
#undef MEMLZ_RLE
#undef MEMLZ_WORDPROBE4096
#undef MEMLZ_BLOCKLEN

#endif // memlz_h