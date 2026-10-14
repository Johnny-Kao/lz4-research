/* Minimal PR gate: compare baseline HC backward-match scan with 8-byte variant.
 * Same generated inputs, same encoder API, identical encoded outputs required. */
#define _POSIX_C_SOURCE 200809L
#include "../lib/lz4.h"
#include "../lib/lz4hc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint32_t x = 0x19A72641u;
static uint32_t rnd(void) { x^=x<<13; x^=x>>17; x^=x<<5; return x; }
static double seconds(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (double)ts.tv_sec + ts.tv_nsec*1e-9; }
static void generate(unsigned char* p, size_t n, int kind) {
    size_t i;
    x=0x19A72641u;
    switch(kind) {
    case 0: /* high entropy */
        for(i=0;i<n;i++) p[i]=(unsigned char)rnd();
        break;
    case 1: /* repeated 2-KiB high-entropy sequences */
        for(i=0;i<n && i<2048;i++) p[i]=(unsigned char)rnd();
        for(;i<n;i++) p[i]=p[i%2048];
        break;
    case 2: /* mostly periodic with regularly perturbed positions */
        for(i=0;i<n;i++) p[i]=(i%127==0)?(unsigned char)rnd():(unsigned char)(i%251);
        break;
    case 3: /* long near-copy segments with sparse edits */
        for(i=0;i<n && i<16384;i++) p[i]=(unsigned char)rnd();
        for(;i<n;i++) p[i]=(i%131==0)?(unsigned char)rnd():p[i%16384];
        break;
    }
}
static uint64_t digest(const char* p,int size) {
    uint64_t h=1469598103934665603ULL;
    for(int i=0;i<size;i++){h^=(unsigned char)p[i];h*=1099511628211ULL;}
    return h;
}
int main(void) {
    const size_t sizes[]={262144,1048576};
    const char* const kinds[]={"random","repeat2k","periodic","nearcopy"};
    volatile uint64_t sink=0;
    for(unsigned si=0;si<2;si++) {
        size_t n=sizes[si];
        int cap=LZ4_compressBound((int)n);
        char* in=(char*)malloc(n),*out=(char*)malloc((size_t)cap),*decoded=(char*)malloc(n);
        if(!in||!out||!decoded)return 2;
        for(int k=0;k<4;k++) {
            generate((unsigned char*)in,n,k);
            for(int level=3;level<=9;level+=6) {
                int count=(n<500000)?14:5;
                int outputSize=0;
                for(int warm=0;warm<2;warm++) {
                    outputSize=LZ4_compress_HC(in,out,(int)n,cap,level);
                    if(outputSize<=0)return 3;
                }
                double t0=seconds();
                for(int rep=0;rep<count;rep++) {
                    outputSize=LZ4_compress_HC(in,out,(int)n,cap,level);
                    if(outputSize<=0)return 4;
                    sink+=(uint64_t)outputSize;
                }
                double dt=seconds()-t0;
                if(LZ4_decompress_safe(out,decoded,outputSize,(int)n)!=(int)n || memcmp(in,decoded,n))return 5;
                printf("CASE n=%zu kind=%s level=%d bytes=%d hash=%llu mibps=%.3f\n",n,kinds[k],level,outputSize,(unsigned long long)digest(out,outputSize),n*count/(1048576.0*dt));
            }
        }
        free(in);free(out);free(decoded);
    }
    fprintf(stderr,"sink=%llu\n",(unsigned long long)sink);
    return 0;
}
