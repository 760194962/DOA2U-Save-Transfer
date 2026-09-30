/* DOA2U Save Transfer - Win32 GUI (Chinese / English, DPI aware) */
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "doau_core.h"

#define APP_TITLE L"DOA2U Save Transfer 1.1"

enum { ID_SRC=100, ID_SRC_BR, ID_SRC_MAC, ID_SRC_OUI, ID_SRC_FIND,
       ID_HD, ID_DST_MAC, ID_REF, ID_REF_BR, ID_REF_OUI, ID_REF_FIND,
       ID_VERIFY, ID_CONVERT, ID_STOP, ID_PROG, ID_LOG, ID_SAVEPATH, ID_LANG,
       ID_GRP1, ID_GRP2, ID_L_SRC, ID_L_SRCMAC, ID_L_OUI1, ID_L_HD, ID_L_DSTMAC, ID_L_OUI2, ID_L_REF };
#define WM_SEARCH_DONE (WM_APP+1)
#define TIMER_PROG 1

/* ---------------- strings ---------------- */
enum { S_GRP1, S_GRP2, S_L_SRC, S_L_SRCMAC, S_L_OUI, S_L_HD, S_L_DSTMAC, S_L_REF,
       S_BROWSE, S_FIND_SRC, S_FIND_REF, S_VERIFY, S_CONVERT, S_STOP, S_LANGBTN,
       S_INTRO, S_H1, S_H2, S_H3, S_H4, S_H5,
       S_OPENFAIL, S_BADSIZE, S_BADMAC, S_BADHD, S_BADOUI, S_PROFILE,
       S_SEARCHING, S_FOUND, S_STOPPED, S_NOTFOUND, S_SRCMACNAME, S_DSTMACNAME, S_NEEDOUI,
       S_VHEAD, S_VSRC, S_VSRCOK, S_VSRCBAD, S_VNOSRCMAC, S_VREF, S_VHDOK, S_VHDBAD, S_VDSTOK, S_VDSTBAD,
       S_CHEAD, S_CSRCBAD, S_CEMBDIFF, S_CANCEL, S_WRITEFAIL, S_SAVED, S_CDETAIL, S_CFOLDER, S_COUNT };

static const wchar_t *STR[2][S_COUNT] = {
{ /* 0 = Chinese */
 L"① 源存档（要迁移的存档）", L"② 目标主机（要放进去的主机）",
 L"源 ups.dat：", L"源主机 MAC：", L"前缀：", L"目标 HD Key：", L"目标主机 MAC：", L"参考 ups.dat：",
 L"浏览…", L"从源存档搜索 MAC", L"从参考存档搜索 MAC", L"验证", L"转换并保存…", L"停止搜索", L"English",
 L"DOA2U Save Transfer — 在原版 Xbox / xemu / Xbox 360 之间迁移 Dead or Alive Ultimate 的 ups.dat",
 L"用法：",
 L" 1. 选源存档 ups.dat，填源主机 MAC（不知道就点「从源存档搜索 MAC」，默认前缀 00:50:F2 是 Microsoft 的 OUI，xemu 存档实测如此；搜不到就换成源主机设置里 MAC 的前 3 字节）",
 L" 2. 填目标主机 HD Key。最稳妥：在目标主机上新建一个档案，把它的 ups.dat 选为「参考」，点「从参考存档搜索 MAC」",
 L" 3. 点「验证」确认都是 ✓，再点「转换并保存…」",
 L" 4. 把整个存档文件夹（含 SaveMeta.xbx、SaveImage.xbx）放到 UDATA\\54430006\\，不要只替换 ups.dat",
 L"✗ 无法打开文件：%ls", L"✗ 文件大小 %ld 字节，不是 ups.dat（应为 %u 字节）",
 L"✗ %ls 格式不对，应为 12 位十六进制，如 00:50:F2:12:34:56", L"✗ HD Key 格式不对，应为 32 位十六进制",
 L"✗ MAC 前缀应为 6 位十六进制，如 00:50:F2", L"   档案名：%ls    内嵌 MAC：%hs",
 L"… 正在搜索 MAC %02X:%02X:%02X:xx:xx:xx（%d 线程，最多约 1677 万次，可能需要几分钟）",
 L"✓ 找到 MAC：%hs（已填入「%ls」）", L"■ 已停止",
 L"✗ 这个前缀下没有找到。可换一个前缀再试（默认 00:50:F2；也可试主机网络设置里显示的 MAC 的前 3 字节）",
 L"源主机 MAC", L"目标主机 MAC", L"✗ 请先填前缀，或先填目标主机 MAC（会取前 3 字节作为前缀）",
 L"—— 验证 ——", L"源存档：%ls", L"  ✓ 源 MAC 正确，可以解密", L"  ✗ 源 MAC 解不开这个存档，请用「从源存档搜索 MAC」",
 L"  · 未填源 MAC", L"参考存档：%ls", L"  ✓ 目标 HD Key 与参考存档签名一致", L"  ✗ 目标 HD Key 与参考存档签名不一致（HD Key 可能不对）",
 L"  ✓ 目标 MAC 正确（就是游戏实际读到的 MAC）", L"  ✗ 目标 MAC 解不开参考存档。游戏读到的 MAC 可能和系统设置里显示的不同，请用「从参考存档搜索 MAC」",
 L"—— 转换 ——", L"✗ 源 MAC 解不开源存档，请先用「从源存档搜索 MAC」",
 L"! 注意：存档内嵌 MAC（%hs）与源 MAC 不同，仍会改为目标 MAC", L"已取消", L"✗ 无法写入：%ls", L"✓ 已保存：%ls",
 L"   内嵌 MAC %hs → %hs，已用目标 MAC 重新加密、用目标 HD Key 重新签名",
 L"   ⚠ 请把新 ups.dat 放回源存档文件夹，再把整个文件夹放到 UDATA\\54430006\\，不要只替换目标主机上其他文件夹里的 ups.dat"
},
{ /* 1 = English */
 L"① Source save (the save to transfer)", L"② Target console (where it will be used)",
 L"Source ups.dat:", L"Source MAC:", L"Prefix:", L"Target HD Key:", L"Target MAC:", L"Reference ups.dat:",
 L"Browse…", L"Find MAC from source", L"Find MAC from reference", L"Verify", L"Convert && Save…", L"Stop search", L"中文",
 L"DOA2U Save Transfer — move Dead or Alive Ultimate ups.dat between Xbox / xemu / Xbox 360",
 L"How to use:",
 L" 1. Pick the source ups.dat and enter the source MAC (unknown? click \"Find MAC from source\"; default prefix 00:50:F2 is Microsoft's OUI and matched an xemu save; if not found, try the first 3 bytes of the MAC in the source console's settings)",
 L" 2. Enter the target HD Key. Safest: create a new profile on the target console, pick its ups.dat as \"Reference\" and click \"Find MAC from reference\"",
 L" 3. Click \"Verify\" until everything shows ✓, then \"Convert & Save…\"",
 L" 4. Copy the WHOLE save folder (with SaveMeta.xbx, SaveImage.xbx) into UDATA\\54430006\\ — do not just replace ups.dat",
 L"✗ Cannot open file: %ls", L"✗ File is %ld bytes, not a ups.dat (should be %u bytes)",
 L"✗ %ls is invalid, expected 12 hex digits, e.g. 00:50:F2:12:34:56", L"✗ HD Key is invalid, expected 32 hex digits",
 L"✗ MAC prefix must be 6 hex digits, e.g. 00:50:F2", L"   Profile: %ls    Embedded MAC: %hs",
 L"… Searching MAC %02X:%02X:%02X:xx:xx:xx (%d threads, up to 16.7M tries, may take a few minutes)",
 L"✓ Found MAC: %hs (filled into \"%ls\")", L"■ Stopped",
 L"✗ Not found under this prefix. Try another one (default 00:50:F2; or try the first 3 bytes of the MAC shown in the console's network settings)",
 L"Source MAC", L"Target MAC", L"✗ Enter a prefix first, or the target MAC (its first 3 bytes will be used)",
 L"—— Verify ——", L"Source: %ls", L"  ✓ Source MAC is correct, save decrypts", L"  ✗ Source MAC cannot decrypt this save, use \"Find MAC from source\"",
 L"  · Source MAC not entered", L"Reference: %ls", L"  ✓ Target HD Key matches the reference save signature", L"  ✗ Target HD Key does NOT match the reference save (wrong HD Key?)",
 L"  ✓ Target MAC is correct (this is the MAC the game actually sees)", L"  ✗ Target MAC cannot decrypt the reference save. The game may see a different MAC than system settings show; use \"Find MAC from reference\"",
 L"—— Convert ——", L"✗ Source MAC cannot decrypt the source save, use \"Find MAC from source\" first",
 L"! Note: embedded MAC (%hs) differs from the source MAC; it will still be changed to the target MAC", L"Cancelled", L"✗ Cannot write: %ls", L"✓ Saved: %ls",
 L"   Embedded MAC %hs → %hs; re-encrypted with the target MAC and re-signed with the target HD Key",
 L"   ⚠ Put the new ups.dat back into the SOURCE save folder and copy that whole folder into UDATA\\54430006\\ — don't drop it into another save folder on the target"
}};
static int g_lang;
#define T(i) STR[g_lang][i]

/* ---------------- state ---------------- */
static HWND hMain, hLog, hProg;
static HFONT hFont;
static int g_dpi = 96;
static volatile int g_stop;
static volatile uint32_t g_prog[64];
static int g_nthreads, g_running, g_target;
static uint8_t g_buf[UPS_SIZE];
static uint8_t g_oui[3];
static volatile LONG g_found;
static uint8_t g_foundmac[6];
static HANDLE g_threads[64];

static int S(int v){ return MulDiv(v, g_dpi, 96); }

/* ---------------- helpers ---------------- */
static void logw(const wchar_t *fmt, ...){
    wchar_t b[1200]; va_list ap; int n;
    va_start(ap,fmt); _vsnwprintf(b,1150,fmt,ap); va_end(ap); b[1150]=0;
    wcscat(b,L"\r\n");
    n=GetWindowTextLengthW(hLog);
    SendMessageW(hLog,EM_SETSEL,n,n); SendMessageW(hLog,EM_REPLACESEL,0,(LPARAM)b);
}
static void getA(int id,char *out,int n){ wchar_t w[512]; GetDlgItemTextW(hMain,id,w,512); WideCharToMultiByte(CP_UTF8,0,w,-1,out,n,0,0); }
static void getW(int id,wchar_t *out,int n){ GetDlgItemTextW(hMain,id,out,n); }
static void setA(int id,const char *s){ wchar_t w[128]; MultiByteToWideChar(CP_UTF8,0,s,-1,w,128); SetDlgItemTextW(hMain,id,w); }

static int load_ups(const wchar_t *path,uint8_t *buf){
    FILE *f=_wfopen(path,L"rb"); long sz;
    if(!f){ logw(T(S_OPENFAIL),path); return 0; }
    fseek(f,0,SEEK_END); sz=ftell(f); fseek(f,0,SEEK_SET);
    if(sz!=(long)UPS_SIZE){ fclose(f); logw(T(S_BADSIZE),sz,UPS_SIZE); return 0; }
    if(fread(buf,1,UPS_SIZE,f)!=UPS_SIZE){ fclose(f); logw(T(S_OPENFAIL),path); return 0; }
    fclose(f); return 1;
}
static int get_mac(int id,uint8_t mac[6],const wchar_t *what){
    char s[64]; getA(id,s,64);
    if(!parse_hex(s,mac,6)){ logw(T(S_BADMAC),what); return 0; }
    return 1;
}
static int get_hd(uint8_t hd[16]){
    char s[128]; getA(ID_HD,s,128);
    if(!parse_hex(s,hd,16)){ logw(T(S_BADHD)); return 0; }
    return 1;
}
static void show_profile(const uint8_t *plain){
    wchar_t name[20]; int i; char m[32];
    for(i=0;i<16;i++){ const uint8_t *p=plain+24+0xB1A9+2*i; name[i]=(wchar_t)(p[0]|p[1]<<8); if(!name[i]) break; }
    name[i<16?i:16]=0;
    format_mac(plain+UPS_MAC_OFFSET,m);
    logw(T(S_PROFILE),name,m);
}
static int browse(int id,int save){
    wchar_t f[MAX_PATH]; OPENFILENAMEW o; memset(&o,0,sizeof o);
    if(save) wcscpy(f,L"ups.dat"); else getW(id,f,MAX_PATH);
    o.lStructSize=sizeof o; o.hwndOwner=hMain; o.lpstrFile=f; o.nMaxFile=MAX_PATH;
    o.lpstrFilter=L"ups.dat\0ups.dat;*.dat\0*.*\0*.*\0";
    o.Flags=save?(OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST):(OFN_FILEMUSTEXIST);
    if(save? !GetSaveFileNameW(&o) : !GetOpenFileNameW(&o)) return 0;
    SetDlgItemTextW(hMain,id,f); return 1;
}

/* ---------------- search ---------------- */
typedef struct { uint32_t lo,hi; int idx; } job_t;
static job_t g_jobs[64];
static DWORD WINAPI worker(LPVOID p){
    job_t *j=(job_t*)p; uint8_t mac[6];
    if(ups_search(g_buf,g_oui,j->lo,j->hi,&g_stop,&g_prog[j->idx],mac)){
        if(InterlockedExchange(&g_found,1)==0) memcpy(g_foundmac,mac,6);
        g_stop=1;
    }
    return 0;
}
static DWORD WINAPI waiter(LPVOID p){
    (void)p; WaitForMultipleObjects(g_nthreads,g_threads,TRUE,INFINITE);
    PostMessageW(hMain,WM_SEARCH_DONE,0,0); return 0;
}
static void set_busy(int b){
    int ids[]={ID_SRC_FIND,ID_REF_FIND,ID_VERIFY,ID_CONVERT}; int i;
    for(i=0;i<4;i++) EnableWindow(GetDlgItem(hMain,ids[i]),!b);
    EnableWindow(GetDlgItem(hMain,ID_STOP),b);
}
static void start_search(int fileid,int ouiid,int target){
    wchar_t path[MAX_PATH]; char s[64]; SYSTEM_INFO si; uint32_t per; int i;
    if(g_running) return;
    getW(fileid,path,MAX_PATH); if(!load_ups(path,g_buf)) return;
    getA(ouiid,s,64);
    if(!parse_hex(s,g_oui,3)){ logw(T(S_BADOUI)); return; }
    GetSystemInfo(&si); g_nthreads=(int)si.dwNumberOfProcessors; if(g_nthreads<1)g_nthreads=1; if(g_nthreads>64)g_nthreads=64;
    g_stop=0; g_found=0; g_target=target; per=(0x1000000u+g_nthreads-1)/g_nthreads;
    for(i=0;i<g_nthreads;i++){
        g_prog[i]=0; g_jobs[i].idx=i; g_jobs[i].lo=per*i; g_jobs[i].hi=per*(i+1)>0x1000000u?0x1000000u:per*(i+1);
        g_threads[i]=CreateThread(0,0,worker,&g_jobs[i],0,0);
        SetThreadPriority(g_threads[i],THREAD_PRIORITY_BELOW_NORMAL);
    }
    CloseHandle(CreateThread(0,0,waiter,0,0,0));
    g_running=1; set_busy(1);
    SendMessageW(hProg,PBM_SETPOS,0,0);
    SetTimer(hMain,TIMER_PROG,300,0);
    logw(T(S_SEARCHING),g_oui[0],g_oui[1],g_oui[2],g_nthreads);
}
static void search_done(void){
    int i; char m[32];
    KillTimer(hMain,TIMER_PROG);
    for(i=0;i<g_nthreads;i++) CloseHandle(g_threads[i]);
    g_running=0; set_busy(0);
    if(g_found){
        format_mac(g_foundmac,m); setA(g_target,m);
        SendMessageW(hProg,PBM_SETPOS,1000,0);
        logw(T(S_FOUND),m,T(g_target==ID_SRC_MAC?S_SRCMACNAME:S_DSTMACNAME));
    } else if(g_stop) logw(T(S_STOPPED));
    else logw(T(S_NOTFOUND));
}

/* ---------------- actions ---------------- */
static void do_verify(void){
    wchar_t path[MAX_PATH]; static uint8_t b[UPS_SIZE]; uint8_t mac[6],hd[16]; char s[64];
    logw(T(S_VHEAD));
    getW(ID_SRC,path,MAX_PATH);
    if(path[0] && load_ups(path,b)){
        logw(T(S_VSRC),path);
        getA(ID_SRC_MAC,s,64);
        if(s[0] && parse_hex(s,mac,6)){
            ups_decrypt(b,mac);
            if(ups_plain_ok(b)){ logw(T(S_VSRCOK)); show_profile(b); } else logw(T(S_VSRCBAD));
        } else logw(T(S_VNOSRCMAC));
    }
    getW(ID_REF,path,MAX_PATH);
    if(path[0] && load_ups(path,b)){
        logw(T(S_VREF),path);
        getA(ID_HD,s,64);
        if(s[0] && parse_hex(s,hd,16)) logw(T(sig_check(b,UPS_SIZE,hd)?S_VHDOK:S_VHDBAD));
        getA(ID_DST_MAC,s,64);
        if(s[0] && parse_hex(s,mac,6)){
            ups_decrypt(b,mac);
            if(ups_plain_ok(b)){ logw(T(S_VDSTOK)); show_profile(b); } else logw(T(S_VDSTBAD));
        }
    }
}
static void do_convert(void){
    wchar_t path[MAX_PATH]; static uint8_t src[UPS_SIZE],out[UPS_SIZE]; uint8_t sm[6],dm[6],hd[16],emb[6]; char a[32],c[32]; FILE *f;
    logw(T(S_CHEAD));
    getW(ID_SRC,path,MAX_PATH); if(!load_ups(path,src)) return;
    if(!get_mac(ID_SRC_MAC,sm,T(S_SRCMACNAME))) return;
    if(!get_hd(hd)) return;
    if(!get_mac(ID_DST_MAC,dm,T(S_DSTMACNAME))) return;
    if(ups_transfer(src,sm,dm,hd,out,emb)!=0){ logw(T(S_CSRCBAD)); return; }
    format_mac(emb,a); format_mac(dm,c);
    if(memcmp(emb,sm,6)) logw(T(S_CEMBDIFF),a);
    if(!browse(ID_SAVEPATH,1)){ logw(T(S_CANCEL)); return; }
    getW(ID_SAVEPATH,path,MAX_PATH);
    f=_wfopen(path,L"wb"); if(!f){ logw(T(S_WRITEFAIL),path); return; }
    fwrite(out,1,UPS_SIZE,f); fclose(f);
    logw(T(S_SAVED),path);
    logw(T(S_CDETAIL),a,c);
    logw(T(S_CFOLDER));
}

/* ---------------- layout ---------------- */
static HWND mk(const wchar_t *cls,DWORD st,int x,int y,int w,int h,int id){
    HWND c=CreateWindowExW(wcscmp(cls,L"EDIT")==0?WS_EX_CLIENTEDGE:0,cls,L"",WS_CHILD|WS_VISIBLE|st,
                           S(x),S(y),S(w),S(h),hMain,(HMENU)(INT_PTR)id,GetModuleHandleW(0),0);
    SendMessageW(c,WM_SETFONT,(WPARAM)hFont,TRUE); return c;
}
static void apply_texts(void){
    static const struct { int id, s; } map[] = {
        {ID_GRP1,S_GRP1},{ID_GRP2,S_GRP2},{ID_L_SRC,S_L_SRC},{ID_L_SRCMAC,S_L_SRCMAC},{ID_L_OUI1,S_L_OUI},
        {ID_L_HD,S_L_HD},{ID_L_DSTMAC,S_L_DSTMAC},{ID_L_OUI2,S_L_OUI},{ID_L_REF,S_L_REF},
        {ID_SRC_BR,S_BROWSE},{ID_REF_BR,S_BROWSE},{ID_SRC_FIND,S_FIND_SRC},{ID_REF_FIND,S_FIND_REF},
        {ID_VERIFY,S_VERIFY},{ID_CONVERT,S_CONVERT},{ID_STOP,S_STOP},{ID_LANG,S_LANGBTN} };
    int i;
    for(i=0;i<(int)(sizeof map/sizeof map[0]);i++) SetDlgItemTextW(hMain,map[i].id,T(map[i].s));
}
static void show_help(void){
    SetWindowTextW(hLog,L"");
    logw(T(S_INTRO)); logw(T(S_H1)); logw(T(S_H2)); logw(T(S_H3)); logw(T(S_H4)); logw(T(S_H5));
}
/* layout in 96-DPI units; client area 720 x 560 */
#define LX 24      /* label x */
#define LW 140     /* label width */
#define EX 170     /* edit x */
#define RX 596     /* right button x */
#define RW 110
static void build(void){
    int y=8;
    mk(L"BUTTON",BS_GROUPBOX,10,y,700,100,ID_GRP1);
    mk(L"STATIC",SS_LEFT,LX,y+30,LW,22,ID_L_SRC);
    mk(L"EDIT",ES_AUTOHSCROLL,EX,y+26,RX-EX-8,26,ID_SRC);
    mk(L"BUTTON",BS_PUSHBUTTON,RX,y+26,RW,26,ID_SRC_BR);
    mk(L"STATIC",SS_LEFT,LX,y+66,LW,22,ID_L_SRCMAC);
    mk(L"EDIT",ES_AUTOHSCROLL,EX,y+62,170,26,ID_SRC_MAC);
    mk(L"STATIC",SS_RIGHT,346,y+66,60,22,ID_L_OUI1);
    mk(L"EDIT",ES_AUTOHSCROLL,410,y+62,90,26,ID_SRC_OUI);
    mk(L"BUTTON",BS_PUSHBUTTON,508,y+62,RX+RW-508,26,ID_SRC_FIND);
    SetDlgItemTextW(hMain,ID_SRC_OUI,L"00:50:F2");
    y+=110;
    mk(L"BUTTON",BS_GROUPBOX,10,y,700,136,ID_GRP2);
    mk(L"STATIC",SS_LEFT,LX,y+30,LW,22,ID_L_HD);
    mk(L"EDIT",ES_AUTOHSCROLL,EX,y+26,RX+RW-EX,26,ID_HD);
    mk(L"STATIC",SS_LEFT,LX,y+66,LW,22,ID_L_DSTMAC);
    mk(L"EDIT",ES_AUTOHSCROLL,EX,y+62,170,26,ID_DST_MAC);
    mk(L"STATIC",SS_RIGHT,346,y+66,60,22,ID_L_OUI2);
    mk(L"EDIT",ES_AUTOHSCROLL,410,y+62,90,26,ID_REF_OUI);
    mk(L"BUTTON",BS_PUSHBUTTON,508,y+62,RX+RW-508,26,ID_REF_FIND);
    mk(L"STATIC",SS_LEFT,LX,y+102,LW,22,ID_L_REF);
    mk(L"EDIT",ES_AUTOHSCROLL,EX,y+98,RX-EX-8,26,ID_REF);
    mk(L"BUTTON",BS_PUSHBUTTON,RX,y+98,RW,26,ID_REF_BR);
    y+=146;
    mk(L"BUTTON",BS_PUSHBUTTON,10,y,120,32,ID_VERIFY);
    mk(L"BUTTON",BS_DEFPUSHBUTTON,138,y,170,32,ID_CONVERT);
    mk(L"BUTTON",BS_PUSHBUTTON,316,y,130,32,ID_STOP);
    EnableWindow(GetDlgItem(hMain,ID_STOP),FALSE);
    hProg=mk(PROGRESS_CLASSW,0,454,y+7,150,18,ID_PROG);
    SendMessageW(hProg,PBM_SETRANGE32,0,1000);
    mk(L"BUTTON",BS_PUSHBUTTON,612,y,98,32,ID_LANG);
    y+=42;
    hLog=mk(L"EDIT",ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,10,y,700,560-y-10,ID_LOG);
    mk(L"EDIT",0,0,0,0,0,ID_SAVEPATH); ShowWindow(GetDlgItem(hMain,ID_SAVEPATH),SW_HIDE);
    apply_texts(); show_help();
}

static LRESULT CALLBACK WndProc(HWND h,UINT m,WPARAM w,LPARAM l){
    switch(m){
    case WM_CREATE: hMain=h; build(); return 0;
    case WM_COMMAND:
        switch(LOWORD(w)){
        case ID_SRC_BR: browse(ID_SRC,0); break;
        case ID_REF_BR: browse(ID_REF,0); break;
        case ID_SRC_FIND: start_search(ID_SRC,ID_SRC_OUI,ID_SRC_MAC); break;
        case ID_REF_FIND: {
            wchar_t o[32]; getW(ID_REF_OUI,o,32);
            if(!o[0]){ char s[64]; uint8_t mac[6]; getA(ID_DST_MAC,s,64);
                if(parse_hex(s,mac,6)){ char t[16]; sprintf(t,"%02X:%02X:%02X",mac[0],mac[1],mac[2]); setA(ID_REF_OUI,t); }
                else { logw(T(S_NEEDOUI)); break; } }
            start_search(ID_REF,ID_REF_OUI,ID_DST_MAC); break; }
        case ID_STOP: g_stop=1; break;
        case ID_VERIFY: do_verify(); break;
        case ID_CONVERT: do_convert(); break;
        case ID_LANG: g_lang=!g_lang; apply_texts(); if(!g_running) show_help(); break;
        }
        return 0;
    case WM_TIMER: {
        uint64_t t=0; int i; for(i=0;i<g_nthreads;i++) t+=g_prog[i];
        SendMessageW(hProg,PBM_SETPOS,(WPARAM)(t*1000/0x1000000u),0); return 0; }
    case WM_SEARCH_DONE: search_done(); return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc=(HDC)w; SetBkMode(dc,TRANSPARENT);
        if((HWND)l==hLog) break;
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW); }
    case WM_DESTROY: g_stop=1; PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

int WINAPI wWinMain(HINSTANCE hi,HINSTANCE hp,PWSTR cmd,int show){
    WNDCLASSW wc; MSG msg; INITCOMMONCONTROLSEX ic={sizeof ic,ICC_PROGRESS_CLASS|ICC_STANDARD_CLASSES};
    RECT r; NONCLIENTMETRICSW ncm; HDC dc; DWORD st=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
    (void)hp;
    InitCommonControlsEx(&ic);
    dc=GetDC(0); g_dpi=GetDeviceCaps(dc,LOGPIXELSY); ReleaseDC(0,dc); if(g_dpi<96) g_dpi=96;
    g_lang = (PRIMARYLANGID(GetUserDefaultUILanguage())==LANG_CHINESE) ? 0 : 1;
    if(cmd && wcsstr(cmd,L"--en")) g_lang=1;
    if(cmd && wcsstr(cmd,L"--zh")) g_lang=0;
    memset(&ncm,0,sizeof ncm); ncm.cbSize=sizeof ncm;
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof ncm,&ncm,0);
    ncm.lfMessageFont.lfHeight=-MulDiv(9,g_dpi,72);   /* 9pt, scaled */
    hFont=CreateFontIndirectW(&ncm.lfMessageFont);
    memset(&wc,0,sizeof wc);
    wc.lpfnWndProc=WndProc; wc.hInstance=hi; wc.lpszClassName=L"DOA2USaveTransfer";
    wc.hCursor=LoadCursor(0,IDC_ARROW); wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
    RegisterClassW(&wc);
    r.left=0; r.top=0; r.right=S(720); r.bottom=S(560);
    AdjustWindowRect(&r,st,FALSE);
    CreateWindowW(L"DOA2USaveTransfer",APP_TITLE,st,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,0,0,hi,0);
    ShowWindow(hMain,show); UpdateWindow(hMain);
    while(GetMessageW(&msg,0,0,0)>0){ if(!IsDialogMessageW(hMain,&msg)){ TranslateMessage(&msg); DispatchMessageW(&msg);} }
    return 0;
}
