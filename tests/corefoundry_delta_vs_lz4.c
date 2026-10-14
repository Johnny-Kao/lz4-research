/* Research-only: compare two reversible sparse-reference delta formats against
 * native LZ4 fast compression WITH the same shared reference as its dictionary.
 * Not a replacement for the LZ4 block wire format. */
#define _POSIX_C_SOURCE 200809L
#include "../lib/lz4.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint32_t prng=0xC013F0D7u;
static uint32_t next_rand(void) { uint32_t v=prng;v^=v<<13;v^=v>>17;v^=v<<5;return prng=v; }
static double now(void) { struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (double)t.tv_sec+(double)t.tv_nsec/1.0e9; }
static void make_ref(unsigned char *ref, size_t n, int family) {
    prng=(uint32_t)(0xA4D38191u+n+family*9871);
    for(size_t i=0;i<n;i++) ref[i]=family? (unsigned char)(i%131): (unsigned char)next_rand();
}
static void make_target(unsigned char *target,const unsigned char *ref,size_t n,int per_mille,int clustered){
    memcpy(target,ref,n);
    size_t count=n*(size_t)per_mille/1000;
    if(count==0 && per_mille>0) count=1;
    uint32_t seed=(uint32_t)(n+per_mille*29+clustered*721);
    if(clustered){
        size_t start=n/3;
        for(size_t k=0;k<count;k++) target[(start+k)%n]^=0x5bu;
    }else{
        size_t start=n/7;
        /* coprime step: odd and n power of two */
        size_t step=(n/2-1);
        for(size_t k=0;k<count;k++) target[(start+k*step)%n]^=(unsigned char)(1u+(seed&0x7fu));
    }
}
static size_t bitmap_encode(unsigned char *out,const unsigned char *ref,const unsigned char *src,size_t n){
    size_t mask=(n+7)/8; memset(out,0,mask);size_t pos=mask;
    for(size_t i=0;i<n;i++)if(ref[i]!=src[i]){out[i>>3]|=(unsigned char)(1u<<(i&7));out[pos++]=src[i];}
    return pos;
}
static int bitmap_decode(unsigned char *out,const unsigned char *ref,const unsigned char *code,size_t len,size_t n){
    size_t mask=(n+7)/8;if(len<mask)return 0;memcpy(out,ref,n);size_t pos=mask;
    for(size_t i=0;i<n;i++)if(code[i>>3]&(1u<<(i&7))){if(pos>=len)return 0;out[i]=code[pos++];}
    return pos==len;
}
static size_t varint_encode(unsigned char *out,const unsigned char *ref,const unsigned char *src,size_t n){
    size_t written=0, base=0;
    for(size_t i=0;i<n;i++)if(ref[i]!=src[i]){
        size_t gap=i-base+1;
        do {unsigned char v=(unsigned char)(gap&127u);gap>>=7;out[written++]=(unsigned char)(v|(gap?128:0));} while(gap);
        out[written++]=src[i];base=i+1;
    }
    return written;
}
static int varint_decode(unsigned char *out,const unsigned char *ref,const unsigned char *code,size_t len,size_t n){
    memcpy(out,ref,n);size_t pos=0,base=0;
    while(pos<len){
        size_t gap=0;unsigned shift=0;unsigned char b;
        do { if(pos>=len||shift>=sizeof(size_t)*8)return 0; b=code[pos++];gap|=((size_t)(b&127))<<shift;shift+=7;}while(b&128);
        if(gap==0||base>n||gap>n-base||pos>=len)return 0;
        size_t index=base+gap-1;out[index]=code[pos++];base=index+1;
    }
    return 1;
}
static unsigned long long hash_bytes(const unsigned char *p,size_t n){
    unsigned long long v=1469598103934665603ull;
    for(size_t i=0;i<n;i++){v^=p[i];v*=1099511628211ull;}return v;
}
int main(void){
    const size_t sizes[]={4096,32768};
    const int changes[]={0,1,10,50,200}; /* per mille */
    volatile size_t sink=0;
    for(int si=0;si<2;si++)for(int family=0;family<2;family++)for(int ci=0;ci<5;ci++)for(int clustered=0;clustered<2;clustered++){
        const size_t n=sizes[si],cap=4*n+(size_t)LZ4_compressBound((int)n);
        unsigned char *ref=malloc(n),*src=malloc(n),*encoded=malloc(cap),*decoded=malloc(n);
        LZ4_stream_t *stream=LZ4_createStream();
        if(!ref||!src||!encoded||!decoded||!stream)return 2;
        make_ref(ref,n,family);make_target(src,ref,n,changes[ci],clustered);
        const int reps=(n<=4096)?260:90;
        for(int mode=0;mode<3;mode++){
            size_t bytes=0;double t0=now();
            for(int rep=0;rep<reps;rep++){
                if(mode==0){
                    int load=LZ4_loadDict(stream,(const char *)ref,(int)n);
                    if(load<=0){fprintf(stderr,"failed loadDict %d\n",load);return 3;}
                    int count=LZ4_compress_fast_continue(stream,(const char *)src,(char*)encoded,(int)n,(int)cap,1);
                    if(count<=0)return 4;
                    bytes=(size_t)count;
                }else if(mode==1)bytes=bitmap_encode(encoded,ref,src,n);
                else bytes=varint_encode(encoded,ref,src,n);
                sink+=bytes;
            }
            double encode_elapsed=now()-t0;
            if(mode==0){
                if(LZ4_decompress_safe_usingDict((const char*)encoded,(char*)decoded,(int)bytes,(int)n,(const char*)ref,(int)n)!=(int)n)return 5;
            }else if(mode==1){
                if(!bitmap_decode(decoded,ref,encoded,bytes,n))return 6;
            }else if(!varint_decode(decoded,ref,encoded,bytes,n))return 7;
            if(memcmp(src,decoded,n)!=0)return 8;
            t0=now();
            for(int rep=0;rep<reps;rep++){
                int ok=1;
                if(mode==0)ok=(LZ4_decompress_safe_usingDict((const char*)encoded,(char*)decoded,(int)bytes,(int)n,(const char*)ref,(int)n)==(int)n);
                else if(mode==1)ok=bitmap_decode(decoded,ref,encoded,bytes,n);
                else ok=varint_decode(decoded,ref,encoded,bytes,n);
                if(!ok)return 9;
                sink+=(size_t)decoded[rep%n];
            }
            double decode_elapsed=now()-t0;
            printf("CASE n=%zu ref=%s edits=%d pattern=%s mode=%s bytes=%zu encode_ns=%.0f decode_ns=%.0f checksum=%llu correctness=PASS\n",
                n,family?"periodic":"random",changes[ci],clustered?"cluster":"scatter",
                mode==0?"lz4dict":mode==1?"bitmap":"positions",bytes,
                1.e9*encode_elapsed/reps,1.e9*decode_elapsed/reps,
                hash_bytes(decoded,n));
        }
        LZ4_freeStream(stream);free(ref);free(src);free(encoded);free(decoded);
    }
    fprintf(stderr,"sink=%zu\n",sink);
    return 0;
}
