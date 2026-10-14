/* Research gate for lz4/lz4 issue #1661: avoid match extension /255 on short matches.
 * Baseline and candidate must produce byte-identical compressed output. */
#define _POSIX_C_SOURCE 200809L
#include "../lib/lz4.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static uint32_t state;
static uint32_t rnd(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static double tick(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+(double)t.tv_nsec/1e9;}
static void fill(unsigned char *p,size_t n,int kind){
    state=0x2B3749A1u+(uint32_t)n+(uint32_t)kind*10279u;
    size_t i=0;
    switch(kind){
    case 0:
        for(i=0;i<n;i++)p[i]=(unsigned char)rnd();
        break;
    case 1: /* many short matches: 40-byte repeating runs */
        for(i=0;i<n;i++){if(i%41==0)p[i]=(unsigned char)rnd();else p[i]=(unsigned char)((i%40)*11);}
        break;
    case 2: /* long matches: repeated pseudo-random 256-byte tile */
        for(i=0;i<256;i++)p[i]=(unsigned char)rnd();
        for(;i<n;i++)p[i]=p[i%256];
        break;
    case 3: /* many medium matches: 96-byte tile with disturbances */
        for(i=0;i<96;i++)p[i]=(unsigned char)rnd();
        for(;i<n;i++)p[i]=(i%133==0)?(unsigned char)rnd():p[i%96];
        break;
    case 4: /* structured text */
        for(i=0;i<n;i++)p[i]=(unsigned char)("event=updated status=ok request=completed; "[i%43]);
        for(i=77;i<n;i+=193)p[i]=(unsigned char)rnd();
        break;
    default: /* high entropy repeated 8KiB with sparse edits */
        for(i=0;i<8192 && i<n;i++)p[i]=(unsigned char)rnd();
        for(;i<n;i++)p[i]=(i%401==0)?(unsigned char)rnd():p[i%8192];
    }
}
static unsigned long long digest(const char* p,int size){
    unsigned long long v=1469598103934665603ull;for(int i=0;i<size;i++){v^=(unsigned char)p[i];v*=1099511628211ull;}return v;
}
int main(void){
    const size_t sizes[]={4096,65536,262144};
    const char *names[]={"random","shortmatch","longmatch","medmatch","text","nearcopy"};
    volatile unsigned long long sink=0;
    for(int si=0;si<3;si++){
        size_t n=sizes[si];int cap=LZ4_compressBound((int)n);
        char *src=malloc(n),*dst=malloc((size_t)cap),*decoded=malloc(n);
        if(!src||!dst||!decoded)return 2;
        for(int kind=0;kind<6;kind++){
            fill((unsigned char*)src,n,kind);
            int reps=n<8192?3500:n<100000?800:250;
            int bytes=0;
            for(int warm=0;warm<30;warm++) {
                bytes=LZ4_compress_fast(src,dst,(int)n,cap,1);
                if(bytes<=0)return 3;
            }
            double begin=tick();
            for(int rep=0;rep<reps;rep++){
                bytes=LZ4_compress_fast(src,dst,(int)n,cap,1);
                if(bytes<=0)return 4;
                sink+=(unsigned long long)bytes;
            }
            double elapsed=tick()-begin;
            if(LZ4_decompress_safe(dst,decoded,bytes,(int)n)!=(int)n || memcmp(src,decoded,n))return 5;
            printf("CASE size=%zu kind=%s bytes=%d hash=%llu throughput_mibps=%.3f\n",n,names[kind],bytes,digest(dst,bytes),n*reps/(1048576.*elapsed));
        }
        free(src);free(dst);free(decoded);
    }
    fprintf(stderr,"sink=%llu\n",(unsigned long long)sink);
    return 0;
}
