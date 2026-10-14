#include "../lib/lz4frame.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
extern void cf_reset_counters(void);
extern size_t cf_get_calls(void);
extern size_t cf_get_bytes(void);
static double clk(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return t.tv_sec + t.tv_nsec / 1e9; }
int main(void) {
    const size_t n = 5*1024*1024+137;
    char *in=malloc(n), *out=malloc(n*2+2048), *check=malloc(n);
    if (!in || !out || !check) return 1;
    for(size_t i=0;i<n;i++) in[i]=(char)((i*17+(i>>7))%251);
    for(int linked=0;linked<=1;linked++) for(int flush=0;flush<=1;flush++) for(int chunk=1024;chunk<=65536;chunk*=8) {
        LZ4F_cctx *ctx=NULL; LZ4F_dctx *dc=NULL;
        LZ4F_preferences_t pref=LZ4F_INIT_PREFERENCES;
        pref.frameInfo.blockMode=linked ? LZ4F_blockLinked : LZ4F_blockIndependent;
        pref.frameInfo.blockSizeID=LZ4F_max64KB;
        pref.autoFlush=flush;
        if(LZ4F_isError(LZ4F_createCompressionContext(&ctx,LZ4F_VERSION)))return 2;
        cf_reset_counters();
        double start=clk(); size_t compressed=0;
        for(int rep=0;rep<80;rep++) {
            size_t pos=0, used=LZ4F_compressBegin(ctx,out,n*2+2048,&pref);
            if(LZ4F_isError(used))return 3;
            for(;pos<n;pos+=chunk) {
                size_t size=n-pos < (size_t)chunk ? n-pos : (size_t)chunk;
                size_t got=LZ4F_compressUpdate(ctx,out+used,n*2+2048-used,in+pos,size,NULL);
                if(LZ4F_isError(got))return 4;
                used+=got;
            }
            size_t end=LZ4F_compressEnd(ctx,out+used,n*2+2048-used,NULL);
            if(LZ4F_isError(end))return 5;
            compressed=used+end;
        }
        double elapsed=clk()-start;
        if(LZ4F_isError(LZ4F_createDecompressionContext(&dc,LZ4F_VERSION)))return 6;
        size_t from=0,to=0;
        while(from<compressed) {
            size_t take=compressed-from, put=n-to;
            size_t next=LZ4F_decompress(dc,check+to,&put,out+from,&take,NULL);
            if(LZ4F_isError(next)||(!take&&!put&&next))return 7;
            from+=take;to+=put;if(!next)break;
        }
        if(to!=n||memcmp(check,in,n))return 8;
        unsigned long hash=5381;
        for(size_t i=0;i<compressed;i++) hash=((hash<<5)+hash)^(unsigned char)out[i];
        printf("linked=%d autoflush=%d chunk=%d calls=%zu savedMiB=%.2f MBps=%.1f output=%zu digest=%lu PASS\n", linked,flush,chunk,cf_get_calls(),cf_get_bytes()/1048576.0,n*80/1048576.0/elapsed,compressed,hash);
        LZ4F_freeDecompressionContext(dc);LZ4F_freeCompressionContext(ctx);
    }
    free(in);free(out);free(check);return 0;
}
