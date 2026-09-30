/* DOA2U Save Transfer - Win32 GUI */
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include "doau_core.h"

#define APP_TITLE L"DOA2U Save Transfer 1.0"

enum { ID_SRC=100, ID_SRC_BR, ID_SRC_MAC, ID_SRC_OUI, ID_SRC_FIND,
       ID_HD, ID_DST_MAC, ID_REF, ID_REF_BR, ID_REF_OUI, ID_REF_FIND,
       ID_VERIFY, ID_CONVERT, ID_STOP, ID_PROG, ID_LOG };
#define WM_SEARCH_DONE (WM_APP+1)
#define TIMER_PROG 1

static HWND hMain, hLog, hProg;
static HFONT hFont;
static volatile int g_stop;
static volatile uint32_t g_prog[64];
static int g_nthreads, g_running, g_target; /* g_target: ID_SRC_MAC or ID_DST_MAC */
static uint8_t g_buf[UPS_SIZE];
static uint8_t g_oui[3];
static volatile int g_found;
static uint8_t g_foundmac[6];
static HANDLE g_threads[64];

/* ---------- helpers ---------- */
static void logw(const wchar_t *fmt, ...){
    wchar_t b[1024]; va_list ap; int n;
    va_start(ap,fmt); _vsnwprintf(b,1000,fmt,ap); va_end(ap); b[1000]=0;
    wcscat(b,L"\r\n");
    n=GetWindowTextLengthW(hLog);
    SendMessageW(hLog,EM_SETSEL,n,n); SendMessageW(hLog,EM_REPLACESEL,0,(LPARAM)b);
}
static void getA(int id,char *out,int n){ wchar_t w[512]; GetDlgItemTextW(hMain,id,w,512); WideCharToMultiByte(CP_UTF8,0,w,-1,out,n,0,0); }
static void getW(int id,wchar_t *out,int n){ GetDlgItemTextW(hMain,id,out,n); }
static void setA(int id,const char *s){ wchar_t w[128]; MultiByteToWideChar(CP_UTF8,0,s,-1,w,128); SetDlgItemTextW(hMain,id,w); }

static int load_ups(const wchar_t *path,uint8_t *buf){
    FILE *f=_wfopen(path,L"rb"); long sz;
    if(!f){ logw(L"✗ 无法打开文件 / cannot open: %ls",path); return 0; }
    fseek(f,0,SEEK_END); sz=ftell(f); fseek(f,0,SEEK_SET);
    if(sz!=(long)UPS_SIZE){ fclose(f); logw(L"✗ 文件大小 %ld，不是 ups.dat（应为 %u 字节）",sz,UPS_SIZE); return 0; }
    fread(buf,1,UPS_SIZE,f); fclose(f); return 1;
}
static int get_mac(int id,uint8_t mac[6],const wchar_t *what){
    char s[64]; getA(id,s,64);
    if(!parse_hex(s,mac,6)){ logw(L"✗ %ls 格式不对，应为 12 位十六进制，如 00:50:F2:12:34:56",what); return 0; }
    return 1;
}
static int get_hd(uint8_t hd[16]){
    char s[128]; getA(ID_HD,s,128);
    if(!parse_hex(s,hd,16)){ logw(L"✗ HD Key 格式不对，应为 32 位十六进制"); return 0; }
    return 1;
}
static void show_profile(const uint8_t *plain){
    wchar_t name[20]; int i; char m[32];
    for(i=0;i<16;i++){ const uint8_t *p=plain+24+0xB1A9+2*i; name[i]=(wchar_t)(p[0]|p[1]<<8); if(!name[i]) break; }
    name[i<16?i:16]=0;
    format_mac(plain+UPS_MAC_OFFSET,m);
    logw(L"   档案名 Profile: %ls   内嵌 MAC: %hs",name,m);
}
static int browse(int id,int save){
    wchar_t f[MAX_PATH]; OPENFILENAMEW o; memset(&o,0,sizeof o);
    if(save) wcscpy(f,L"ups.dat"); else getW(id,f,MAX_PATH);
    o.lStructSize=sizeof o; o.hwndOwner=hMain; o.lpstrFile=f; o.nMaxFile=MAX_PATH;
    o.lpstrFilter=L"ups.dat\0ups.dat;*.dat\0All files\0*.*\0";
    o.Flags=save?(OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST):(OFN_FILEMUSTEXIST);
    if(save? !GetSaveFileNameW(&o) : !GetOpenFileNameW(&o)) return 0;
    SetDlgItemTextW(hMain,id,f); return 1;
}

/* ---------- search ---------- */
typedef struct { uint32_t lo,hi; int idx; } job_t;
static job_t g_jobs[64];
static DWORD WINAPI worker(LPVOID p){
    job_t *j=(job_t*)p; uint8_t mac[6];
    if(ups_search(g_buf,g_oui,j->lo,j->hi,&g_stop,&g_prog[j->idx],mac)){
        if(InterlockedExchange((LONG*)&g_found,1)==0){ memcpy(g_foundmac,mac,6); }
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
    if(!parse_hex(s,g_oui,3)){ logw(L"✗ MAC 前缀应为 6 位十六进制，如 00:50:F2"); return; }
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
    logw(L"… 正在搜索 MAC %02X:%02X:%02X:xx:xx:xx（%d 线程，最多约 1677 万次，可能需要几分钟）",g_oui[0],g_oui[1],g_oui[2],g_nthreads);
}
static void search_done(void){
    int i; char m[32];
    KillTimer(hMain,TIMER_PROG);
    for(i=0;i<g_nthreads;i++) CloseHandle(g_threads[i]);
    g_running=0; set_busy(0);
    if(g_found){
        format_mac(g_foundmac,m); setA(g_target,m);
        SendMessageW(hProg,PBM_SETPOS,1000,0);
        logw(L"✓ 找到 MAC: %hs（已填入%ls）",m,g_target==ID_SRC_MAC?L"源主机 MAC":L"目标主机 MAC");
    } else if(g_stop) logw(L"■ 已停止");
    else logw(L"✗ 这个前缀下没有找到。可换一个前缀再试（原版 Xbox / xemu 多为 00:50:F2，360 可先试网络设置里 MAC 的前 3 字节）");
}

/* ---------- actions ---------- */
static void do_verify(void){
    wchar_t path[MAX_PATH]; static uint8_t b[UPS_SIZE]; uint8_t mac[6],hd[16]; char s[64]; int havehd;
    logw(L"—— 验证 Verify ——");
    getW(ID_SRC,path,MAX_PATH);
    if(path[0] && load_ups(path,b)){
        logw(L"源存档: %ls",path);
        getA(ID_SRC_MAC,s,64);
        if(s[0] && parse_hex(s,mac,6)){
            ups_decrypt(b,mac);
            if(ups_plain_ok(b)){ logw(L"  ✓ 源 MAC 正确，可以解密"); show_profile(b); }
            else logw(L"  ✗ 源 MAC 解不开这个存档，请用「搜索」找出正确的 MAC");
        } else logw(L"  · 未填源 MAC");
    }
    getW(ID_REF,path,MAX_PATH);
    if(path[0] && load_ups(path,b)){
        logw(L"参考存档: %ls",path);
        getA(ID_HD,s,64); havehd=s[0] && parse_hex(s,hd,16);
        if(havehd) logw(sig_check(b,UPS_SIZE,hd)?L"  ✓ 目标 HD Key 与参考存档签名一致":L"  ✗ 目标 HD Key 与参考存档签名不一致（HD Key 可能不对）");
        getA(ID_DST_MAC,s,64);
        if(s[0] && parse_hex(s,mac,6)){
            ups_decrypt(b,mac);
            if(ups_plain_ok(b)){ logw(L"  ✓ 目标 MAC 正确（游戏读到的就是这个 MAC）"); show_profile(b); }
            else logw(L"  ✗ 目标 MAC 解不开参考存档。游戏读到的 MAC 可能和网络设置显示的不同，请用「搜索」");
        }
    }
}
static void do_convert(void){
    wchar_t path[MAX_PATH]; static uint8_t src[UPS_SIZE],out[UPS_SIZE]; uint8_t sm[6],dm[6],hd[16],emb[6]; char a[32],c[32]; FILE *f; int r;
    logw(L"—— 转换 Convert ——");
    getW(ID_SRC,path,MAX_PATH); if(!load_ups(path,src)) return;
    if(!get_mac(ID_SRC_MAC,sm,L"源主机 MAC")) return;
    if(!get_hd(hd)) return;
    if(!get_mac(ID_DST_MAC,dm,L"目标主机 MAC")) return;
    r=ups_transfer(src,sm,dm,hd,out,emb);
    if(r==-1){ logw(L"✗ 源 MAC 解不开源存档，请先用「搜索」找出源主机的 MAC"); return; }
    format_mac(emb,a); format_mac(dm,c);
    if(memcmp(emb,sm,6)) logw(L"! 注意：存档内嵌 MAC (%hs) 与源 MAC 不同，仍会改为目标 MAC",a);
    if(!browse(ID_LOG+1,1)) { logw(L"已取消"); return; }
    GetDlgItemTextW(hMain,ID_LOG+1,path,MAX_PATH);
    f=_wfopen(path,L"wb"); if(!f){ logw(L"✗ 无法写入 %ls",path); return; }
    fwrite(out,1,UPS_SIZE,f); fclose(f);
    logw(L"✓ 已保存: %ls",path);
    logw(L"   内嵌 MAC %hs → %hs，已用目标 MAC 重新加密、用目标 HD Key 重新签名",a,c);
    logw(L"   ⚠ 放回主机时请把整个存档文件夹（含 SaveMeta.xbx、SaveImage.xbx）一起放入 UDATA\\54430006\\，不要只替换 ups.dat");
}

/* ---------- layout ---------- */
static HWND mk(const wchar_t *cls,const wchar_t *txt,DWORD st,int x,int y,int w,int h,int id){
    HWND c=CreateWindowExW(wcscmp(cls,L"EDIT")==0?WS_EX_CLIENTEDGE:0,cls,txt,WS_CHILD|WS_VISIBLE|st,x,y,w,h,hMain,(HMENU)(INT_PTR)id,GetModuleHandleW(0),0);
    SendMessageW(c,WM_SETFONT,(WPARAM)hFont,TRUE); return c;
}
static void build(void){
    int y=10;
    mk(L"BUTTON",L"① 源存档 Source（要迁移的存档）",BS_GROUPBOX,10,y,640,92,0);
    mk(L"STATIC",L"ups.dat:",0,24,y+26,110,20,0);
    mk(L"EDIT",L"",ES_AUTOHSCROLL,140,y+22,410,24,ID_SRC);
    mk(L"BUTTON",L"浏览…",BS_PUSHBUTTON,558,y+22,80,24,ID_SRC_BR);
    mk(L"STATIC",L"源主机 MAC:",0,24,y+60,110,20,0);
    mk(L"EDIT",L"",ES_AUTOHSCROLL,140,y+56,170,24,ID_SRC_MAC);
    mk(L"STATIC",L"前缀:",0,322,y+60,40,20,0);
    mk(L"EDIT",L"00:50:F2",ES_AUTOHSCROLL,364,y+56,90,24,ID_SRC_OUI);
    mk(L"BUTTON",L"从源存档搜索 MAC",BS_PUSHBUTTON,462,y+56,176,24,ID_SRC_FIND);
    y+=104;
    mk(L"BUTTON",L"② 目标主机 Target（要放进去的主机）",BS_GROUPBOX,10,y,640,126,0);
    mk(L"STATIC",L"HD Key:",0,24,y+26,110,20,0);
    mk(L"EDIT",L"",ES_AUTOHSCROLL,140,y+22,498,24,ID_HD);
    mk(L"STATIC",L"目标主机 MAC:",0,24,y+60,110,20,0);
    mk(L"EDIT",L"",ES_AUTOHSCROLL,140,y+56,170,24,ID_DST_MAC);
    mk(L"STATIC",L"前缀:",0,322,y+60,40,20,0);
    mk(L"EDIT",L"",ES_AUTOHSCROLL,364,y+56,90,24,ID_REF_OUI);
    mk(L"BUTTON",L"从参考存档搜索 MAC",BS_PUSHBUTTON,462,y+56,176,24,ID_REF_FIND);
    mk(L"STATIC",L"参考 ups.dat:",0,24,y+94,110,20,0);
    mk(L"EDIT",L"",ES_AUTOHSCROLL,140,y+90,410,24,ID_REF);
    mk(L"BUTTON",L"浏览…",BS_PUSHBUTTON,558,y+90,80,24,ID_REF_BR);
    y+=138;
    mk(L"BUTTON",L"验证",BS_PUSHBUTTON,10,y,120,32,ID_VERIFY);
    mk(L"BUTTON",L"转换并保存…",BS_DEFPUSHBUTTON,140,y,160,32,ID_CONVERT);
    mk(L"BUTTON",L"停止搜索",BS_PUSHBUTTON,310,y,110,32,ID_STOP);
    EnableWindow(GetDlgItem(hMain,ID_STOP),FALSE);
    hProg=mk(PROGRESS_CLASSW,L"",0,430,y+6,220,20,ID_PROG);
    SendMessageW(hProg,PBM_SETRANGE32,0,1000);
    y+=42;
    hLog=mk(L"EDIT",L"",ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|WS_VSCROLL,10,y,640,230,ID_LOG);
    mk(L"EDIT",L"",0,0,0,0,0,ID_LOG+1); ShowWindow(GetDlgItem(hMain,ID_LOG+1),SW_HIDE);
    logw(L"DOA2U Save Transfer — 在不同主机 / xemu / Xbox 360 之间迁移 Dead or Alive Ultimate 的 ups.dat");
    logw(L"用法：");
    logw(L" 1. 选源存档 ups.dat，填源主机 MAC（不知道就点「从源存档搜索 MAC」）");
    logw(L" 2. 填目标主机 HD Key 和 MAC。最稳妥：在目标主机上新建一个档案，把它的 ups.dat 选为参考，点「从参考存档搜索 MAC」");
    logw(L" 3. 点「验证」确认，再点「转换并保存…」");
    logw(L" 4. 把整个存档文件夹放回目标主机，不要只替换 ups.dat");
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
                else { logw(L"✗ 请先填目标主机 MAC 的前缀（前 3 字节），或填目标主机 MAC"); break; } }
            start_search(ID_REF,ID_REF_OUI,ID_DST_MAC); break; }
        case ID_STOP: g_stop=1; break;
        case ID_VERIFY: do_verify(); break;
        case ID_CONVERT: do_convert(); break;
        }
        return 0;
    case WM_TIMER: {
        uint64_t t=0; int i; for(i=0;i<g_nthreads;i++) t+=g_prog[i];
        SendMessageW(hProg,PBM_SETPOS,(WPARAM)(t*1000/0x1000000u),0); return 0; }
    case WM_SEARCH_DONE: search_done(); return 0;
    case WM_CTLCOLORSTATIC: {
        HDC dc=(HDC)w; SetBkMode(dc,TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW); }
    case WM_DESTROY: g_stop=1; PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h,m,w,l);
}

int WINAPI wWinMain(HINSTANCE hi,HINSTANCE hp,PWSTR cmd,int show){
    WNDCLASSW wc; MSG msg; INITCOMMONCONTROLSEX ic={sizeof ic,ICC_PROGRESS_CLASS|ICC_STANDARD_CLASSES};
    RECT r={0,0,660,520}; NONCLIENTMETRICSW ncm; (void)hp;(void)cmd;
    InitCommonControlsEx(&ic);
    ncm.cbSize=sizeof ncm; SystemParametersInfoW(SPI_GETNONCLIENTMETRICS,sizeof ncm,&ncm,0);
    hFont=CreateFontIndirectW(&ncm.lfMessageFont);
    memset(&wc,0,sizeof wc);
    wc.lpfnWndProc=WndProc; wc.hInstance=hi; wc.lpszClassName=L"DOA2USaveTransfer";
    wc.hCursor=LoadCursor(0,IDC_ARROW); wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
    wc.hIcon=LoadIconW(hi,MAKEINTRESOURCEW(1));
    RegisterClassW(&wc);
    AdjustWindowRect(&r,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,FALSE);
    CreateWindowW(L"DOA2USaveTransfer",APP_TITLE,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,
                  CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,0,0,hi,0);
    ShowWindow(hMain,show); UpdateWindow(hMain);
    while(GetMessageW(&msg,0,0,0)>0){ if(!IsDialogMessageW(hMain,&msg)){ TranslateMessage(&msg); DispatchMessageW(&msg);} }
    return 0;
}
