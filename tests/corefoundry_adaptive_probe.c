#include "../lib/lz4.h"
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static uint32_t state=0x12345678;
static uint32_t rnd32(void){uint32_t x=state;x^=x<<13;x^=x>>17;x^=x<<5;return state=x;}
static double secs(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static void fill(unsigned char *p,size_t n,int kind){
    if(kind==0){for(size_t i=0;i<n;i++)p[i]=(unsigned char)("the family office considers liquidity and governance."[i%51]);}
    if(kind==1){for(size_t i=0;i<n;i++)p[i]=(unsigned char)rnd32();}
    if(kind==2){unsigned char tile[4096];for(int j=0;j<4096;j++)tile[j]=(unsigned char)rnd32();for(size_t i=0;i<n;i++)p[i]=tile[i%4096];}
    if(kind==3){for(size_t i=0;i<n;i++)p[i]= i<n/2 ? (unsigned char)rnd32() : (unsigned char)("AAAAABBBBBCCCCCDDDDDEEEEE"[i%25]);}
    if(kind==4){for(size_t i=0;i<n;i++)p[i]=(unsigned char)(i%128);for(size_t i=n/2;i<n;i++)if(i%7==0)p[i]=(unsigned char)rnd32();}
}
static int sample_accel(const unsigned char *p,size_t n,int *matches){
    uint32_t seen[256];for(int j=0;j<256;j++)seen[j]=0;
    int hits=0, count=0;
    for(size_t k=0;k<64;k++){
        size_t at=k*(n-4)/63;
        uint32_t v;memcpy(&v,p+at,4);
        uint32_t idx=((v*2654435761u)>>24)&255;
        hits+=(seen[idx]==v);
        seen[idx]=v;count++;
    }
    *matches=hits;
    return hits<2?16:(hits>10?1:4);
}
static double entropy(const unsigned char *p,size_t n){size_t count[256]={0};double h=0;for(size_t i=0;i<n;i++)count[p[i]]++;for(int i=0;i<256;i++)if(count[i]){double q=(double)count[i]/(double)n;h-=q*log2(q);}return h;}
static const char *kinds[]={"text-repeat","random","high-entropy-repeated-4K","mixed","structured"};
int main(void){
    const size_t sizes[]={4096,65536,262144};
    for(int si=0;si<3;si++){
        size_t n=sizes[si];int bound=LZ4_compressBound((int)n);
        unsigned char *in=malloc(n);char *out=malloc((size_t)bound),*dec=malloc(n);
        if(!in||!out||!dec)return 2;
        for(int kind=0;kind<5;kind++){
            fill(in,n,kind);
            int hits=0,selected=sample_accel(in,n,&hits);
            printf("DATA size=%zu kind=%s byte_entropy=%.4f sampled_hits=%d\n",n,kinds[kind],entropy(in,n),hits);
            int repeats=(int)(100000000/n);if(repeats<200)repeats=200;
            for(int mode=0;mode<4;mode++){
                int accel=(mode==0?1:(mode==1?4:(mode==2?16:selected)));
                size_t total=0;
                double t0=secs();
                for(int i=0;i<repeats;i++){
                    int m=accel;
                    if(mode==3){int dummy; m=sample_accel(in,n,&dummy);}
                    int nc=LZ4_compress_fast((const char*)in,out,(int)n,bound,m);
                    if(nc<=0)return 3;
                    total+=(size_t)nc;
                }
                double dur=secs()-t0;
                int nc=LZ4_compress_fast((const char*)in,out,(int)n,bound,accel);
                if(LZ4_decompress_safe(out,dec,nc,(int)n)!=(int)n||memcmp(in,dec,n))return 4;
                printf("size=%zu kind=%s mode=%s accel=%d hits=%d MiBps=%.1f ratio=%.4f\n",n,kinds[kind],mode==0?"fixed1":mode==1?"fixed4":mode==2?"fixed16":"adaptive",accel,hits,n*repeats/(1048576.0*dur),(double)total/(n*repeats));
            }
        }
        free(in);free(out);free(dec);
    }
    return 0;
}
