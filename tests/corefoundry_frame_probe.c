#include "../lib/lz4frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec * 1e-9; }
int main(void) {
    enum { INPUT_SIZE = 1024 * 1024, REPEATS = 600 };
    char *src = malloc(INPUT_SIZE), *dst = malloc(INPUT_SIZE * 2 + 1024);
    if (!src || !dst) return 2;
    for (int i = 0; i < INPUT_SIZE; i++) src[i] = (char)((i * 17 + (i >> 7)) % 251);
    for (int chunk = 1024; chunk <= 65536; chunk *= 8) {
        for (int stable = 0; stable <= 1; stable++) {
            LZ4F_cctx *ctx = NULL;
            if (LZ4F_isError(LZ4F_createCompressionContext(&ctx, LZ4F_VERSION))) return 3;
            LZ4F_preferences_t pref = LZ4F_INIT_PREFERENCES;
            pref.frameInfo.blockMode = LZ4F_blockLinked;
            pref.autoFlush = 1;
            LZ4F_compressOptions_t opt = {0};
            opt.stableSrc = stable;
            volatile size_t sink = 0;
            size_t last_size = 0;
            double begin = now();
            for (int rep = 0; rep < REPEATS; rep++) {
                size_t used = LZ4F_compressBegin(ctx, dst, INPUT_SIZE * 2 + 1024, &pref);
                if (LZ4F_isError(used)) return 4;
                for (size_t pos = 0; pos < INPUT_SIZE; pos += chunk) {
                    size_t n = (size_t)chunk;
                    if (n > INPUT_SIZE - pos) n = INPUT_SIZE - pos;
                    size_t out = LZ4F_compressUpdate(ctx, dst + used, INPUT_SIZE * 2 + 1024 - used, src + pos, n, &opt);
                    if (LZ4F_isError(out)) { fprintf(stderr, "compressUpdate error %zu\n", out); return 5; }
                    used += out;
                }
                size_t out = LZ4F_compressEnd(ctx, dst + used, INPUT_SIZE * 2 + 1024 - used, NULL);
                if (LZ4F_isError(out)) return 6;
                last_size = used + out;
                sink += last_size;
            }
            double duration = now() - begin;
            LZ4F_dctx *dc = NULL;
            char *decoded = malloc(INPUT_SIZE);
            if (!decoded || LZ4F_isError(LZ4F_createDecompressionContext(&dc, LZ4F_VERSION))) return 7;
            size_t in_pos = 0, out_pos = 0;
            while (in_pos < last_size) {
                size_t in_n = last_size - in_pos, out_n = INPUT_SIZE - out_pos;
                size_t next = LZ4F_decompress(dc, decoded + out_pos, &out_n, dst + in_pos, &in_n, NULL);
                if (LZ4F_isError(next) || (!in_n && !out_n && next)) return 8;
                in_pos += in_n; out_pos += out_n;
                if (next == 0) break;
            }
            if (out_pos != INPUT_SIZE || memcmp(decoded, src, INPUT_SIZE)) { fprintf(stderr, "roundtrip failed chunk=%d stable=%d\n", chunk, stable); return 9; }
            LZ4F_freeDecompressionContext(dc);
            free(decoded);
            printf("chunk=%d stableSrc=%d MBps=%.2f bytes=%zu roundtrip=PASS sink=%zu\n", chunk, stable, INPUT_SIZE * REPEATS / (1024.0 * 1024.0) / duration, last_size, (size_t)sink);
            LZ4F_freeCompressionContext(ctx);
        }
    }
    free(src); free(dst);
    return 0;
}
