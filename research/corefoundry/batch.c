#define _POSIX_C_SOURCE 200809L
#include "../../lib/lz4.h"
#include "../../lib/lz4hc.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static volatile uint64_t sink = 0;
static uint32_t rng = 0x93a5b72du;
static uint32_t prng(void) { uint32_t x=rng; x^=x<<13; x^=x>>17; x^=x<<5; return rng=x; }
static double clock_s(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t); return (double)t.tv_sec + t.tv_nsec*1e-9; }
static uint64_t digest(const unsigned char *b, size_t n) { uint64_t h=1469598103934665603ULL; for(size_t i=0;i<n;i++){h^=b[i];h*=1099511628211ULL;} return h; }
static void generate(unsigned char *p, size_t n, int kind) {
    rng=0x93a5b72du + (uint32_t)(kind*677);
    if(kind==0) { for(size_t i=0;i<n;i++) p[i]=(unsigned char)("LZ4 research notes: repeated text and metadata. "[i%47]); return; }
    if(kind==1) { for(size_t i=0;i<n;i++) p[i]=(unsigned char)prng(); return; }
    if(kind==2) { unsigned char tile[4096]; for(int i=0;i<4096;i++) tile[i]=(unsigned char)prng(); for(size_t i=0;i<n;i++) p[i]=tile[i%4096]; return; }
    if(kind==3) { for(size_t i=0;i<n;i++) p[i]=(unsigned char)((i%83) + ((i/512)%4)*13); for(size_t i=0;i<n;i+=41) p[i]=(unsigned char)prng(); return; }
    if(kind==4) { for(size_t i=0;i<n/2;i++) p[i]=(unsigned char)prng(); for(size_t i=n/2;i<n;i++) p[i]=(unsigned char)("aaaaabbbbbcccccdddddeeeee"[i%25]); return; }
}
static const char* kinds[]={"text","random","repeat-random-4K","structured","mixed"};
static int roundtrip(const char *src, int n, const char *dst, int c, char* restored) {
    return LZ4_decompress_safe(dst,restored,c,n)==n && memcmp(restored,src,(size_t)n)==0;
}
static void median_sort(double* v,int n) { for(int i=0;i<n;i++)for(int j=i+1;j<n;j++)if(v[j]<v[i]) {double t=v[i];v[i]=v[j];v[j]=t;} }
static int hc(void) {
    enum{N=65536, REPS=24};
    unsigned char *in=malloc(N);
    char *out=malloc(LZ4_compressBound(N)),*dec=malloc(N);
    if(!in||!out||!dec) return 2;
    const int levels[]={3,9,12};
    for(int kind=0;kind<5;kind++){generate(in,N,kind);
        for(int l=0;l<3;l++){
            int level=levels[l], c=0;
            double trial[3];
            for(int pass=0;pass<3;pass++){
                double t=clock_s();
                for(int j=0;j<REPS;j++){
                    c=LZ4_compress_HC((const char*)in,out,N,LZ4_compressBound(N),level);
                    if(c<=0)return 3;
                    sink+=(unsigned char)out[c-1];
                }
                trial[pass]=(clock_s()-t)/REPS;
            }
            median_sort(trial,3);
            if(!roundtrip((const char*)in,N,out,c,dec))return 4;
            printf("HC,%s,%d,%.3f,%d,%llu,PASS\n",kinds[kind],level,trial[1]*1e6,c,(unsigned long long)digest((unsigned char*)out,(size_t)c));
        }
    }
    free(in);free(out);free(dec);return 0;
}
static int oracle(void) {
    enum{N=65536,REPS=160};
    unsigned char *in=malloc(N);
    char *out=malloc(LZ4_compressBound(N)),*dec=malloc(N);
    if(!in||!out||!dec)return 2;
    const int accelerations[]={1,4,16};
    for(int kind=0;kind<5;kind++){
        generate(in,N,kind);
        double times[3][4]={0};
        int bytes[3]={0};
        for(int pass=0;pass<4;pass++){
            for(int m=0;m<3;m++){
                int idx=(pass%2)?(2-m):m;
                double t=clock_s();int c=0;
                for(int j=0;j<REPS;j++){
                    c=LZ4_compress_fast((const char*)in,out,N,LZ4_compressBound(N),accelerations[idx]);
                    if(c<=0)return 3;
                    sink+=(unsigned char)out[c-1];
                }
                times[idx][pass]=(clock_s()-t)/REPS;bytes[idx]=c;
                if(!roundtrip((const char*)in,N,out,c,dec))return 4;
            }
        }
        for(int m=0;m<3;m++) {
            median_sort(times[m],4);
            double t=(times[m][1]+times[m][2])/2;
            printf("ORACLE,%s,%d,%.3f,%d,%.7f,PASS\n",kinds[kind],accelerations[m],t*1e6,bytes[m],(double)bytes[m]/N);
        }
    }
    free(in);free(out);free(dec);return 0;
}
static int delta(void) {
    enum{PAGE=4096, PAIR=8192, REPS=1000};
    unsigned char base[PAGE], next[PAGE], src[PAIR], reconstructed[PAIR];
    unsigned char packed[PAGE+2+3*PAGE];
    char lz4out[LZ4_COMPRESSBOUND(PAIR)];
    int const counts[]={0,1,8,64,512,2048,4096};
    for(int kind=0;kind<3;kind++){
        for(int ci=0;ci<7;ci++) {
            int changes=counts[ci];
            generate(base,PAGE,kind==0?1:(kind==1?0:3));
            memcpy(next,base,PAGE);
            for(int j=0;j<changes;j++){ int pos=(j*4057+17)%PAGE; next[pos]^=0x5a; }
            memcpy(src,base,PAGE);memcpy(src+PAGE,next,PAGE);
            double t=clock_s();int c=0;
            for(int rep=0;rep<REPS;rep++){
                c=LZ4_compress_default((char*)src,lz4out,PAIR,LZ4_COMPRESSBOUND(PAIR));
                if(c<=0)return 3;
                sink+=(unsigned char)lz4out[c-1];
            }
            double lz4ns=(clock_s()-t)*1e9/REPS;
            t=clock_s();
            for(int rep=0;rep<REPS;rep++) {
                if(LZ4_decompress_safe(lz4out,(char*)reconstructed,c,PAIR)!=PAIR)return 4;
                sink+=(unsigned char)reconstructed[0];
            }
            double lz4decns=(clock_s()-t)*1e9/REPS;
            if(memcmp(src,reconstructed,PAIR))return 4;
            t=clock_s();int used=0;
            for(int rep=0;rep<REPS;rep++) {
                memcpy(packed,base,PAGE);
                used=PAGE+2;
                int count=0;
                for(int pos=0;pos<PAGE;pos++){
                    if(base[pos]!=next[pos]){
                        packed[used++]=(unsigned char)pos;
                        packed[used++]=(unsigned char)(pos>>8);
                        packed[used++]=next[pos];
                        count++;
                    }
                }
                packed[PAGE]=(unsigned char)(count&255);
                packed[PAGE+1]=(unsigned char)(count>>8);
                sink+=packed[used-1];
            }
            double deltans=(clock_s()-t)*1e9/REPS;
            int encoded_changes=(int)packed[PAGE]|((int)packed[PAGE+1]<<8);
            if(encoded_changes!=changes || used!=PAGE+2+3*encoded_changes)return 5;
            t=clock_s();
            for(int rep=0;rep<REPS;rep++) {
                memcpy(reconstructed,packed,PAGE);
                memcpy(reconstructed+PAGE,packed,PAGE);
                int read=PAGE+2;
                for(int j=0;j<encoded_changes;j++){int pos=(int)packed[read]|((int)packed[read+1]<<8);reconstructed[PAGE+pos]=packed[read+2];read+=3;}
                sink+=(unsigned char)reconstructed[0];
            }
            double deltadecns=(clock_s()-t)*1e9/REPS;
            if(memcmp(src,reconstructed,PAIR))return 5;
            printf("DELTA,%s,%d,%.1f,%d,%.1f,%d,%.1f,%.1f,PASS\n",kind==0?"random-base":kind==1?"text-base":"structured-base",changes,lz4ns,c,deltans,used,lz4decns,deltadecns);
        }
    }
    return 0;
}
int main(int argc,char**argv) {
    if(argc!=2){fprintf(stderr,"usage: %s hc|oracle|delta\n",argv[0]);return 1;}
    int code=!strcmp(argv[1],"hc")?hc():!strcmp(argv[1],"oracle")?oracle():!strcmp(argv[1],"delta")?delta():1;
    fprintf(stderr,"probe=%s rc=%d sink=%llu\n",argv[1],code,(unsigned long long)sink);
    return code;
}
