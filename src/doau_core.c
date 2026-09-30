#include "doau_core.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

extern const uint32_t BF_P_INIT[18];
extern const uint32_t BF_S_INIT[4][256];

/* ---------------- keys ---------------- */
static const uint8_t XBOX_CERT_KEY[16] = {0x5C,0x07,0x33,0xAE,0x04,0x01,0xF7,0xE8,0xBA,0x79,0x93,0xFD,0xCD,0x2F,0x1F,0xE0};
static const uint8_t DOAU_TITLE_KEY[16] = {0x42,0xCB,0xF5,0x32,0xBF,0x1F,0xD1,0x47,0x39,0xD0,0xF7,0xF6,0x6D,0xF8,0xB7,0xB1};
static const char MAGIC[] = "Lightning Offering Guy"; /* 22 chars + NUL = 23 bytes */

/* ---------------- helpers ---------------- */
static uint32_t rd32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void wr32(uint8_t *p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}

int parse_hex(const char *s, uint8_t *out, int n){
    int k=0, hi=-1;
    for(;*s;s++){
        int c=(unsigned char)*s, v;
        if(c==':'||c=='-'||c==' '||c=='\t') continue;
        if(c>='0'&&c<='9') v=c-'0'; else if(c>='a'&&c<='f') v=c-'a'+10; else if(c>='A'&&c<='F') v=c-'A'+10; else return 0;
        if(hi<0) hi=v; else { if(k>=n) return 0; out[k++]=(uint8_t)(hi<<4|v); hi=-1; }
    }
    return k==n && hi<0;
}
void format_mac(const uint8_t m[6], char *o){ sprintf(o,"%02X:%02X:%02X:%02X:%02X:%02X",m[0],m[1],m[2],m[3],m[4],m[5]); }

/* ---------------- SHA-1 / HMAC ---------------- */
typedef struct { uint32_t h[5]; uint64_t len; uint8_t buf[64]; size_t n; } sha1_ctx;
#define ROL(x,c) (((x)<<(c))|((x)>>(32-(c))))
static void sha1_block(sha1_ctx *c,const uint8_t *p){
    uint32_t w[80],a,b,d,e,f,k,t,cc; int i;
    for(i=0;i<16;i++) w[i]=(uint32_t)p[4*i]<<24|(uint32_t)p[4*i+1]<<16|(uint32_t)p[4*i+2]<<8|p[4*i+3];
    for(i=16;i<80;i++) w[i]=ROL(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
    a=c->h[0];b=c->h[1];cc=c->h[2];d=c->h[3];e=c->h[4];
    for(i=0;i<80;i++){
        if(i<20){f=(b&cc)|(~b&d);k=0x5A827999;} else if(i<40){f=b^cc^d;k=0x6ED9EBA1;}
        else if(i<60){f=(b&cc)|(b&d)|(cc&d);k=0x8F1BBCDC;} else {f=b^cc^d;k=0xCA62C1D6;}
        t=ROL(a,5)+f+e+k+w[i]; e=d; d=cc; cc=ROL(b,30); b=a; a=t;
    }
    c->h[0]+=a;c->h[1]+=b;c->h[2]+=cc;c->h[3]+=d;c->h[4]+=e;
}
static void sha1_init(sha1_ctx *c){c->h[0]=0x67452301;c->h[1]=0xEFCDAB89;c->h[2]=0x98BADCFE;c->h[3]=0x10325476;c->h[4]=0xC3D2E1F0;c->len=0;c->n=0;}
static void sha1_update(sha1_ctx *c,const uint8_t *p,size_t l){
    c->len+=l;
    while(l){ size_t t=64-c->n; if(t>l)t=l; memcpy(c->buf+c->n,p,t); c->n+=t; p+=t; l-=t; if(c->n==64){sha1_block(c,c->buf);c->n=0;} }
}
static void sha1_final(sha1_ctx *c,uint8_t out[20]){
    uint64_t bits=c->len*8; uint8_t pad=0x80, z=0, L[8]; int i;
    sha1_update(c,&pad,1); while(c->n!=56) sha1_update(c,&z,1);
    for(i=0;i<8;i++) L[i]=(uint8_t)(bits>>(56-8*i));
    sha1_update(c,L,8);
    for(i=0;i<5;i++){out[4*i]=(uint8_t)(c->h[i]>>24);out[4*i+1]=(uint8_t)(c->h[i]>>16);out[4*i+2]=(uint8_t)(c->h[i]>>8);out[4*i+3]=(uint8_t)c->h[i];}
}
static void hmac_sha1(const uint8_t *key,size_t kl,const uint8_t *d,size_t dl,uint8_t out[20]){
    uint8_t k[64]={0},ip[64],op[64],ih[20]; sha1_ctx c; int i;
    memcpy(k,key,kl);
    for(i=0;i<64;i++){ip[i]=k[i]^0x36;op[i]=k[i]^0x5C;}
    sha1_init(&c);sha1_update(&c,ip,64);sha1_update(&c,d,dl);sha1_final(&c,ih);
    sha1_init(&c);sha1_update(&c,op,64);sha1_update(&c,ih,20);sha1_final(&c,out);
}
static void title_sig_key(uint8_t k[16]){ uint8_t h[20]; hmac_sha1(XBOX_CERT_KEY,16,DOAU_TITLE_KEY,16,h); memcpy(k,h,16); }
void sig_roamable(const uint8_t *data,size_t len,uint8_t out[20]){ uint8_t k[16]; title_sig_key(k); hmac_sha1(k,16,data,len,out); }
void sig_nonroamable(const uint8_t *data,size_t len,const uint8_t hd[16],uint8_t out[20]){ uint8_t r[20]; sig_roamable(data,len,r); hmac_sha1(hd,16,r,20,out); }
int sig_check(const uint8_t *f,size_t len,const uint8_t hd[16]){ uint8_t s[20]; sig_nonroamable(f+20,len-20,hd,s); return memcmp(s,f,20)==0; }

/* ---------------- MT19937 ---------------- */
#define MTN 624
typedef struct { uint32_t mt[MTN]; int i; } mt_t;
static void mt_seed(mt_t *m,uint32_t s){ int i; m->mt[0]=s; for(i=1;i<MTN;i++) m->mt[i]=1812433253u*(m->mt[i-1]^(m->mt[i-1]>>30))+(uint32_t)i; m->i=MTN; }
static void mt_seed_array(mt_t *m,const uint32_t *k,int kl){
    int i=1,j=0,c; mt_seed(m,19650218u);
    for(c=MTN>kl?MTN:kl;c;c--){ m->mt[i]=(m->mt[i]^((m->mt[i-1]^(m->mt[i-1]>>30))*1664525u))+k[j]+(uint32_t)j; i++; j++; if(i>=MTN){m->mt[0]=m->mt[MTN-1];i=1;} if(j>=kl) j=0; }
    for(c=MTN-1;c;c--){ m->mt[i]=(m->mt[i]^((m->mt[i-1]^(m->mt[i-1]>>30))*1566083941u))-(uint32_t)i; i++; if(i>=MTN){m->mt[0]=m->mt[MTN-1];i=1;} }
    m->mt[0]=0x80000000u; m->i=MTN;
}
static void mt_twist(mt_t *m){
    static const uint32_t mag[2]={0,0x9908b0dfu}; uint32_t *mt=m->mt,y; int k;
    for(k=0;k<MTN-397;k++){y=(mt[k]&0x80000000u)|(mt[k+1]&0x7fffffffu);mt[k]=mt[k+397]^(y>>1)^mag[y&1];}
    for(;k<MTN-1;k++){y=(mt[k]&0x80000000u)|(mt[k+1]&0x7fffffffu);mt[k]=mt[k+397-MTN]^(y>>1)^mag[y&1];}
    y=(mt[MTN-1]&0x80000000u)|(mt[0]&0x7fffffffu);mt[MTN-1]=mt[396]^(y>>1)^mag[y&1]; m->i=0;
}
static uint32_t mt_next(mt_t *m){ uint32_t y; if(m->i>=MTN) mt_twist(m); y=m->mt[m->i++];
    y^=y>>11; y^=(y<<7)&0x9d2c5680u; y^=(y<<15)&0xefc60000u; y^=y>>18; return y; }
static void mt_skip(mt_t *m,size_t n){ while(n){ size_t a; if(m->i>=MTN) mt_twist(m); a=(size_t)(MTN-m->i); if(a>n)a=n; m->i+=(int)a; n-=a; } }

/* ---------------- Blowfish ---------------- */
typedef struct { uint32_t P[18], S[4][256]; } bf_t;
#define BFF(x) (((b->S[0][(x)>>24]+b->S[1][((x)>>16)&255])^b->S[2][((x)>>8)&255])+b->S[3][(x)&255])
static void bf_enc(const bf_t *b,uint32_t *l,uint32_t *r){ uint32_t L=*l,R=*r,t; int i; for(i=0;i<16;i++){L^=b->P[i];R^=BFF(L);t=L;L=R;R=t;} t=L;L=R;R=t; R^=b->P[16]; L^=b->P[17]; *l=L;*r=R; }
static void bf_dec(const bf_t *b,uint32_t *l,uint32_t *r){ uint32_t L=*l,R=*r,t; int i; for(i=17;i>1;i--){L^=b->P[i];R^=BFF(L);t=L;L=R;R=t;} t=L;L=R;R=t; R^=b->P[1]; L^=b->P[0]; *l=L;*r=R; }
static void bf_key(bf_t *b,const uint8_t *k,int kl){
    int i,j=0,n,s; uint32_t l=0,r=0;
    memcpy(b->P,BF_P_INIT,sizeof b->P); memcpy(b->S,BF_S_INIT,sizeof b->S);
    for(i=0;i<18;i++){ uint32_t d=0; for(n=0;n<4;n++){ d=(d<<8)|k[j]; j=(j+1)%kl; } b->P[i]^=d; }
    for(i=0;i<18;i+=2){ bf_enc(b,&l,&r); b->P[i]=l; b->P[i+1]=r; }
    for(s=0;s<4;s++) for(i=0;i<256;i+=2){ bf_enc(b,&l,&r); b->S[s][i]=l; b->S[s][i+1]=r; }
}

/* ---------------- ups.dat ---------------- */
static void setup(const uint8_t *buf,const uint8_t mac[6],mt_t *m,bf_t *b){
    uint32_t key[3]; uint8_t kb[56]; int i;
    key[0]=rd32(buf+20);
    key[1]=(uint32_t)mac[0]|((uint32_t)mac[1]<<8)|((uint32_t)mac[2]<<16)|((uint32_t)mac[3]<<24);
    key[2]=(uint32_t)mac[4]|((uint32_t)mac[5]<<8);
    mt_seed_array(m,key,3);
    for(i=0;i<14;i++) wr32(kb+4*i,mt_next(m));
    bf_key(b,kb,56);
}
void ups_decrypt(uint8_t *buf,const uint8_t mac[6]){
    static mt_t m; static bf_t b; size_t i,nw=(UPS_SIZE-24)/4; uint8_t *p=buf+24;
    setup(buf,mac,&m,&b);
    for(i=0;i<nw;i++) wr32(p+4*i,rd32(p+4*i)^mt_next(&m));
    for(i=0;i<nw;i+=2){ uint32_t l=rd32(p+4*i),r=rd32(p+4*i+4); bf_dec(&b,&l,&r); wr32(p+4*i,l); wr32(p+4*i+4,r); }
}
void ups_encrypt(uint8_t *buf,const uint8_t mac[6]){
    static mt_t m; static bf_t b; size_t i,nw=(UPS_SIZE-24)/4; uint8_t *p=buf+24;
    setup(buf,mac,&m,&b);
    for(i=0;i<nw;i+=2){ uint32_t l=rd32(p+4*i),r=rd32(p+4*i+4); bf_enc(&b,&l,&r); wr32(p+4*i,l); wr32(p+4*i+4,r); }
    for(i=0;i<nw;i++) wr32(p+4*i,rd32(p+4*i)^mt_next(&m));
}
int ups_plain_ok(const uint8_t *p){ return p[24]==0 && memcmp(p+UPS_SIZE-23,MAGIC,23)==0; }

int ups_try_mac(const uint8_t *buf,const uint8_t mac[6]){
    mt_t m; bf_t b; size_t nw=(UPS_SIZE-24)/4; uint32_t l,r;
    setup(buf,mac,&m,&b);
    mt_skip(&m,nw-2);
    l=rd32(buf+24+4*(nw-2))^mt_next(&m); r=rd32(buf+24+4*(nw-1))^mt_next(&m);
    bf_dec(&b,&l,&r);
    return l==rd32((const uint8_t*)"ing ") && r==rd32((const uint8_t*)"Guy\0");
}
int ups_search(const uint8_t *buf,const uint8_t oui[3],uint32_t lo,uint32_t hi,
               volatile int *stop,volatile uint32_t *progress,uint8_t out[6]){
    uint32_t x; uint8_t mac[6];
    mac[0]=oui[0];mac[1]=oui[1];mac[2]=oui[2];
    for(x=lo;x<hi;x++){
        if(stop && *stop) return 0;
        mac[3]=(uint8_t)(x>>16);mac[4]=(uint8_t)(x>>8);mac[5]=(uint8_t)x;
        if(ups_try_mac(buf,mac)){ memcpy(out,mac,6); return 1; }
        if(progress) (*progress)++;
    }
    return 0;
}

int ups_transfer(const uint8_t *src,const uint8_t smac[6],const uint8_t dmac[6],const uint8_t hd[16],uint8_t *out,uint8_t emb[6]){
    memcpy(out,src,UPS_SIZE);
    ups_decrypt(out,smac);
    if(!ups_plain_ok(out)) return -1;
    memcpy(emb,out+UPS_MAC_OFFSET,6);
    memcpy(out+UPS_MAC_OFFSET,dmac,6);         /* profile owner MAC stored in plaintext */
    ups_encrypt(out,dmac);
    sig_nonroamable(out+20,UPS_SIZE-20,hd,out);
    return 0;
}
