/* One-shot LZ4F_write zero-byte fwrite overhead test.
 * In default buffered frames, most tiny write() calls have no encoded output.
 * Correctness gate: baseline and candidate must produce byte-identical files. */
#define _POSIX_C_SOURCE 200809L
#include "../lib/lz4file.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (double)t.tv_sec+(double)t.tv_nsec*1e-9;}
static uint64_t digest_file(FILE* fp,size_t* size){
    unsigned char buf[8192];
    size_t n;
    uint64_t h=1469598103934665603ULL;
    *size=0;
    rewind(fp);
    while((n=fread(buf,1,sizeof(buf),fp))>0) {
        *size+=n;
        for(size_t i=0;i<n;i++){h^=buf[i];h*=1099511628211ULL;}
    }
    if(ferror(fp)){fprintf(stderr,"reading output failed\n");exit(9);}
    return h;
}
static unsigned char source[65536];
int main(void){
    const size_t chunks[]={1,16,512,8192,65536};
    const int reps[]={200000,40000,3000,900,240};
    const char *kinds[]={"repeat","random"};
    for(size_t i=0;i<sizeof(source);i++) source[i]=(unsigned char)((i*131u+i/71u)&255u);
    for(int automode=0;automode<2;automode++){
        LZ4F_preferences_t prefs = LZ4F_INIT_PREFERENCES;
        prefs.autoFlush=(unsigned)automode;
        prefs.frameInfo.blockSizeID=LZ4F_max64KB;
        for(int kind=0;kind<2;kind++){
            if(kind==1){unsigned int r=0x34924713u;for(size_t i=0;i<sizeof(source);i++){r^=r<<13;r^=r>>17;r^=r<<5;source[i]=(unsigned char)r;}}
            for(int ix=0;ix<5;ix++){
                size_t const n=chunks[ix];int const loops=(automode&&n<512)?reps[ix]/10:reps[ix];
                FILE* fp=tmpfile();
                if(!fp){perror("tmpfile");return 2;}
                LZ4_writeFile_t* wf=NULL;
                size_t ret=LZ4F_writeOpen(&wf,fp,&prefs);
                if(LZ4F_isError(ret)){fprintf(stderr,"writeOpen:%s\n",LZ4F_getErrorName(ret));return 3;}
                double t0=now();
                for(int j=0;j<loops;j++){
                    const size_t bytes=LZ4F_write(wf,source,n);
                    if(bytes!=n){fprintf(stderr,"write:%zu != %zu\n",bytes,n);return 4;}
                }
                double sec=now()-t0;
                ret=LZ4F_writeClose(wf);
                if(LZ4F_isError(ret)){fprintf(stderr,"writeClose:%s\n",LZ4F_getErrorName(ret));return 5;}
                size_t outLen=0;uint64_t h=digest_file(fp,&outLen);
                fclose(fp);
                printf("CASE autoflush=%d kind=%s chunk=%zu loops=%d bytes=%zu hash=%llu ns_per_write=%.2f\n",
                  automode,kinds[kind],n,loops,outLen,(unsigned long long)h,sec*1.e9/loops);
            }
            /* restore periodic source for next case */
            for(size_t i=0;i<sizeof(source);i++)source[i]=(unsigned char)((i*131u+i/71u)&255u);
        }
    }
    return 0;
}
