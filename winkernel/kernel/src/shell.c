/* WinKernel NTKRNL-X -- cmd.exe-style interactive shell */

#include <kernel/shell.h>
#include <kernel/terminal.h>
#include <kernel/io.h>
#include <kernel/mm.h>
#include <kernel/hal.h>
#include <kernel/rtl.h>
#include <kernel/bsod.h>
#include <kernel/ps.h>
#include <kernel/net.h>
#include <kernel/gfx.h>
#include <ntdef.h>
#include <ntstatus.h>

static const CHAR* g_Commands[] = {
    "help","cls","echo","ver","mem","sysinfo","time",
    "set","dir","crash","reboot","shutdown","color",
    "net","netping","gfxtest","drivers","fetch",
    NULL
};

static CHAR g_History[SHELL_HISTORY_MAX][SHELL_INPUT_MAX];
static INT  g_HistoryCount = 0;
static INT  g_HistoryPos   = 0;
static SHELL_ENV_VAR g_Env[SHELL_ENV_MAX];
static CHAR g_Buf[SHELL_INPUT_MAX];
static INT  g_Len = 0;
static INT  g_Cur = 0;

static VOID _Execute(PCSTR Line);

VOID Shell_PrintBanner(VOID) {
    KdPrintColor("WinKernel Version 0.1.0 [x86_64]\n", CON_WHITE, CON_BLACK);
    KdPrintColor("(c) WinKernel Project. All rights reserved.\n\n", CON_GREY, CON_BLACK);
}

static VOID _Prompt(VOID) {
    KdPrintColor("C:\\> ", CON_WHITE, CON_BLACK);
}

static VOID _HistoryAdd(PCSTR line) {
    if (!line || !line[0]) return;
    if (g_HistoryCount == SHELL_HISTORY_MAX) {
        for (INT i = 0; i < SHELL_HISTORY_MAX - 1; i++)
            RtlCopyString(g_History[i], g_History[i+1], SHELL_INPUT_MAX);
        g_HistoryCount--;
    }
    RtlCopyString(g_History[g_HistoryCount++], line, SHELL_INPUT_MAX);
    g_HistoryPos = g_HistoryCount;
}

static VOID _RedrawSuffix(VOID) {
    DWORD col, row;
    KdGetCursor(&col, &row);
    DWORD saved = col;
    for (INT i = g_Cur; i < g_Len; i++) KdPutChar(g_Buf[i]);
    KdPutChar(' ');
    KdMoveCursor(saved, row);
}

static VOID _RedrawFull(DWORD pcol) {
    DWORD col, row;
    KdGetCursor(&col, &row);
    (VOID)col;
    KdMoveCursor(pcol, row);
    DWORD cols = KdGetCols();
    for (DWORD i = pcol; i < cols; i++) KdPutChar(' ');
    KdMoveCursor(pcol, row);
    for (INT i = 0; i < g_Len; i++) KdPutChar(g_Buf[i]);
    KdMoveCursor(pcol + (DWORD)g_Cur, row);
}

static VOID _TabComplete(DWORD pcol) {
    PCSTR match = NULL; INT hits = 0;
    for (INT i = 0; g_Commands[i]; i++)
        if (RtlStringStartsWith(g_Commands[i], g_Buf)) { match = g_Commands[i]; hits++; }
    if (hits != 1 || !match) return;
    RtlCopyString(g_Buf, match, SHELL_INPUT_MAX);
    g_Len = (INT)RtlStringLength(g_Buf);
    g_Cur = g_Len;
    _RedrawFull(pcol);
}

VOID Shell_Run(VOID) {
    Shell_PrintBanner();
    while (TRUE) {
        _Prompt();
        g_Len = 0; g_Cur = 0;
        g_HistoryPos = g_HistoryCount;
        RtlZeroMemory(g_Buf, sizeof(g_Buf));

        DWORD pcol, prow;
        KdGetCursor(&pcol, &prow);
        (VOID)prow;

        while (TRUE) {
            KEY_EVENT ev;
            while (!IoConsoleReadEvent(&ev))
                __asm__ volatile ("hlt");
            if (ev.Released) continue;

            CHAR c = ev.Ascii;

            if (c == '\n' || c == '\r') {
                KdPutChar('\n');
                g_Buf[g_Len] = '\0';
                _HistoryAdd(g_Buf);
                _Execute(g_Buf);
                break;
            }

            if (c == '\b') {
                if (g_Cur > 0) {
                    for (INT i = g_Cur-1; i < g_Len-1; i++) g_Buf[i] = g_Buf[i+1];
                    g_Len--; g_Cur--;
                    g_Buf[g_Len] = '\0';
                    KdBackspace();
                    _RedrawSuffix();
                }
                continue;
            }

            if (c == '\t') { _TabComplete(pcol); continue; }

            if (c == 0) {
                DWORD col, row; KdGetCursor(&col, &row);
                if (ev.Scancode == 0x48) {
                    if (g_HistoryPos > 0) {
                        g_HistoryPos--;
                        RtlCopyString(g_Buf, g_History[g_HistoryPos], SHELL_INPUT_MAX);
                        g_Len = (INT)RtlStringLength(g_Buf); g_Cur = g_Len;
                        _RedrawFull(pcol);
                    }
                } else if (ev.Scancode == 0x50) {
                    if (g_HistoryPos < g_HistoryCount-1) {
                        g_HistoryPos++;
                        RtlCopyString(g_Buf, g_History[g_HistoryPos], SHELL_INPUT_MAX);
                    } else {
                        g_HistoryPos = g_HistoryCount;
                        RtlZeroMemory(g_Buf, sizeof(g_Buf));
                    }
                    g_Len = (INT)RtlStringLength(g_Buf); g_Cur = g_Len;
                    _RedrawFull(pcol);
                } else if (ev.Scancode == 0x4B) {
                    if (g_Cur > 0) { g_Cur--; KdMoveCursor(col-1, row); }
                } else if (ev.Scancode == 0x4D) {
                    if (g_Cur < g_Len) { g_Cur++; KdMoveCursor(col+1, row); }
                } else if (ev.Scancode == 0x47) {
                    g_Cur = 0; KdMoveCursor(pcol, row);
                } else if (ev.Scancode == 0x4F) {
                    g_Cur = g_Len; KdMoveCursor(pcol+(DWORD)g_Len, row);
                } else if (ev.Scancode == 0x53) {
                    if (g_Cur < g_Len) {
                        for (INT i = g_Cur; i < g_Len-1; i++) g_Buf[i] = g_Buf[i+1];
                        g_Len--; g_Buf[g_Len] = '\0';
                        _RedrawSuffix();
                    }
                }
                continue;
            }

            if (g_Len < SHELL_INPUT_MAX - 1) {
                for (INT i = g_Len; i > g_Cur; i--) g_Buf[i] = g_Buf[i-1];
                g_Buf[g_Cur] = c; g_Len++; g_Cur++;
                g_Buf[g_Len] = '\0';
                KdPutChar(c);
                _RedrawSuffix();
            }
        }
    }
}


/* ── Commands ───────────────────────────────────────────────────────────── */

static VOID _CmdHelp(VOID) {
    KdPrintColor("WinKernel NTKRNL-X Commands\n", CON_WHITE, CON_BLACK);
    KdPrintColor("---------------------------\n", CON_GREY, CON_BLACK);
    KdPrint("help              This help text.\n");
    KdPrint("cls               Clear screen.\n");
    KdPrint("echo [text]       Print text.\n");
    KdPrint("ver               Kernel version.\n");
    KdPrint("mem               Memory statistics.\n");
    KdPrint("sysinfo           CPU and system info.\n");
    KdPrint("time              System uptime.\n");
    KdPrint("set [var] [val]   Set environment variable.\n");
    KdPrint("dir               Volume info.\n");
    KdPrint("color [XY]        Set colors (e.g. color 0A).\n");
    KdPrint("net               Network adapter status.\n");
    KdPrint("netping           Send ARP probe.\n");
    KdPrint("gfxtest           Framebuffer color bar test.\n");
    KdPrint("drivers           List loaded drivers.\n");
    KdPrint("fetch             System summary.\n");
    KdPrint("crash             Trigger BSOD (test).\n");
    KdPrint("reboot            Restart system.\n");
    KdPrint("shutdown          Power off.\n\n");
}

static VOID _CmdCls(VOID) { KdClearScreen(); }

static VOID _CmdEcho(PCSTR a) {
    if (!a || !a[0] || (a[0]=='.'&&!a[1])) { KdPutChar('\n'); return; }
    KdPrint(a); KdPutChar('\n');
}

static VOID _CmdVer(VOID) {
    KdPrintColor("WinKernel Version 0.1.0 [x86_64]\n", CON_WHITE, CON_BLACK);
}

static VOID _CmdMem(VOID) {
    KdPrintColor("Memory Status\n", CON_WHITE, CON_BLACK);
    KdPrintColor("-----------------------------------------\n", CON_GREY, CON_BLACK);
    KdPrintf("Total physical:   %llu MB\n", MmGetTotalBytes()/(1024*1024));
    KdPrintf("Available:        %llu MB\n", MmGetAvailableBytes()/(1024*1024));
    KdPrintf("Kernel pool used: %llu KB\n", ExGetPoolUsed()/1024);
    KdPrintf("Kernel pool free: %llu KB\n\n", ExGetPoolFree()/1024);
}

static VOID _CmdSysinfo(VOID) {
    KdPrintColor("System Information\n", CON_WHITE, CON_BLACK);
    KdPrintColor("-----------------------------------------\n", CON_GREY, CON_BLACK);
    KdPrintf("CPU Vendor:   %s\n", HalCpuInfo.VendorString);
    KdPrintf("CPU Brand:    %s\n", HalCpuInfo.BrandString);
    KdPrintf("Family/Model: %u/%u  Stepping: %u\n",
             HalCpuInfo.Family, HalCpuInfo.Model, HalCpuInfo.Stepping);
    KdPrintf("Logical CPUs: %u\n", HalCpuInfo.LogicalCpuCount);
    KdPrintf("SSE:%s SSE2:%s SSE3:%s AVX:%s AVX2:%s\n",
             HalCpuInfo.HasSSE?"Y":"N", HalCpuInfo.HasSSE2?"Y":"N",
             HalCpuInfo.HasSSE3?"Y":"N", HalCpuInfo.HasAVX?"Y":"N",
             HalCpuInfo.HasAVX2?"Y":"N");
    KdPrint("Arch:         x86_64 (64-bit)\n\n");
}

static VOID _CmdTime(VOID) {
    QWORD u = HalGetUptimeSeconds();
    KdPrintf("Uptime: %llu:%02llu:%02llu (%llu s)\n", u/3600,(u%3600)/60,u%60,u);
}

static VOID _CmdSet(PCSTR a) {
    if (!a || !a[0]) {
        BOOL any = FALSE;
        for (INT i = 0; i < SHELL_ENV_MAX; i++)
            if (g_Env[i].Used) { KdPrintf("%s=%s\n",g_Env[i].Name,g_Env[i].Value); any=TRUE; }
        if (!any) KdPrint("No variables set.\n");
        return;
    }
    CHAR name[SHELL_ENV_NAME_MAX], val[SHELL_ENV_VAL_MAX];
    INT ni=0, vi=0;
    while (*a && *a!=' ' && ni<SHELL_ENV_NAME_MAX-1) name[ni++]=*a++;
    name[ni]='\0';
    while (*a==' ') a++;
    while (*a && vi<SHELL_ENV_VAL_MAX-1) val[vi++]=*a++;
    val[vi]='\0';
    INT slot=-1;
    for (INT i=0;i<SHELL_ENV_MAX;i++)
        if (g_Env[i].Used && RtlCompareString(g_Env[i].Name,name)==0){slot=i;break;}
    if (slot==-1)
        for (INT i=0;i<SHELL_ENV_MAX;i++)
            if (!g_Env[i].Used){slot=i;break;}
    if (slot==-1){KdPrint("Environment full.\n");return;}
    RtlCopyString(g_Env[slot].Name, name, SHELL_ENV_NAME_MAX);
    RtlCopyString(g_Env[slot].Value,val,  SHELL_ENV_VAL_MAX);
    g_Env[slot].Used=TRUE;
}

static VOID _CmdDir(VOID) {
    KdPrint(" Volume in drive C has no label.\n");
    KdPrint(" Volume Serial Number is DEAD-BEEF\n\n");
    KdPrint(" Directory of C:\\\n\n");
    KdPrint("No filesystem mounted.\n\n");
    KdPrint("               0 File(s)    0 bytes\n");
    KdPrint("               0 Dir(s)     0 bytes free\n\n");
}

static VOID _CmdColor(PCSTR a) {
    if (!a || RtlStringLength(a)<2){KdPrint("Usage: color XY\n");return;}
    BYTE bg=0,fg=0;
    CHAR c0=a[0],c1=a[1];
    if(c0>='0'&&c0<='9')bg=(BYTE)(c0-'0');
    else if(c0>='a'&&c0<='f')bg=(BYTE)(c0-'a'+10);
    else if(c0>='A'&&c0<='F')bg=(BYTE)(c0-'A'+10);
    if(c1>='0'&&c1<='9')fg=(BYTE)(c1-'0');
    else if(c1>='a'&&c1<='f')fg=(BYTE)(c1-'a'+10);
    else if(c1>='A'&&c1<='F')fg=(BYTE)(c1-'A'+10);
    if(bg==fg){KdPrint("fg and bg cannot match.\n");return;}
    KdSetColor(fg,bg); KdClearScreen();
}

static VOID _CmdNet(VOID) { NetPrintStatus(); }

static VOID _CmdNetping(VOID) {
    if (!NetIsAvailable()){KdPrintColor("No adapter.\n",CON_RED,CON_BLACK);return;}
    BYTE frame[60]; RtlZeroMemory(frame,60);
    MAC_ADDR src; NetGetMac(&src);
    for (BYTE i=0;i<ETH_ALEN;i++) frame[i]=0xFF;
    for (BYTE i=0;i<ETH_ALEN;i++) frame[6+i]=src.b[i];
    frame[12]=0x08; frame[13]=0x06;
    BYTE* arp=frame+14;
    arp[0]=0;arp[1]=1;arp[2]=8;arp[3]=0;arp[4]=6;arp[5]=4;arp[6]=0;arp[7]=1;
    for (BYTE i=0;i<ETH_ALEN;i++) arp[8+i]=src.b[i];
    arp[24]=192;arp[25]=168;arp[26]=1;arp[27]=1;
    NTSTATUS st=NetSendFrame(frame,60);
    KdPrintColor(NT_SUCCESS(st)?"ARP probe sent.\n":"Send failed.\n",
                 NT_SUCCESS(st)?CON_GREEN:CON_RED, CON_BLACK);
}

static VOID _CmdGfxtest(VOID) {
    GFX_MODE mode; GfxGetMode(&mode);
    if (!mode.Framebuffer){KdPrintColor("No framebuffer.\n",CON_RED,CON_BLACK);return;}
    static const DWORD colors[8]={
        GFX_WHITE,GFX_YELLOW,GFX_CYAN,GFX_GREEN,
        GFX_MAGENTA,GFX_RED,GFX_BLUE,GFX_BLACK
    };
    DWORD bw=mode.Width/8;
    for (DWORD i=0;i<8;i++) GfxFillRect(i*bw,0,bw,mode.Height,colors[i]);
    KdPrint("Press any key to return...");
    IoKeyboardGetChar();
    GfxFillScreen(GFX_BLACK);
    KdClearScreen();
}

static VOID _CmdDrivers(VOID) {
    DWORD n=IoGetLoadedDriverCount();
    KdPrintColor("Loaded drivers\n",CON_WHITE,CON_BLACK);
    KdPrintColor("-----------------------------------------\n",CON_GREY,CON_BLACK);
    if (!n){KdPrint("  (none)\n\n");return;}
    CHAR name[IO_DRIVER_NAME_MAX];
    for (DWORD i=0;i<n;i++)
        if (IoGetLoadedDriverName(i,name,sizeof(name)))
            KdPrintf("  [%u] %s\n",i+1,name);
    KdPrint("\n");
}

static VOID _CmdFetch(VOID) {
    QWORD am=MmGetAvailableBytes()/(1024*1024);
    QWORD tm=MmGetTotalBytes()/(1024*1024);
    QWORD u=HalGetUptimeSeconds();
    KdPrintColor("  __        ___       _  __                    _ \n",CON_CYAN,CON_BLACK);
    KdPrintColor(" \\ \\      / (_)_ __  | |/ /___ _ __ _ __   ___| |\n",CON_CYAN,CON_BLACK);
    KdPrintColor("  \\ \\ /\\ / /| | '_ \\ | ' // _ \\ '__| '_ \\ / _ \\ |\n",CON_CYAN,CON_BLACK);
    KdPrintColor("   \\ V  V / | | | | || . \\  __/ |  | | | |  __/ |\n",CON_CYAN,CON_BLACK);
    KdPrintColor("    \\_/\\_/  |_|_| |_||_|\\_\\___|_|  |_| |_|\\___|_|\n\n",CON_CYAN,CON_BLACK);
    KdPrintfColor(CON_WHITE,CON_BLACK," OS      : WinKernel NTKRNL-X v0.1.0\n");
    KdPrintfColor(CON_WHITE,CON_BLACK," CPU     : %s\n",HalCpuInfo.BrandString);
    KdPrintfColor(CON_WHITE,CON_BLACK," Cores   : %u logical\n",HalCpuInfo.LogicalCpuCount);
    KdPrintfColor(CON_WHITE,CON_BLACK," Memory  : %llu / %llu MB\n",am,tm);
    KdPrintfColor(CON_WHITE,CON_BLACK," Uptime  : %llu:%02llu:%02llu\n",u/3600,(u%3600)/60,u%60);
    KdPrintfColor(CON_WHITE,CON_BLACK," Network : %s\n",NetIsAvailable()?"RTL8139 (up)":"Offline");
    KdPrintfColor(CON_WHITE,CON_BLACK," Drivers : %u loaded\n\n",IoGetLoadedDriverCount());
}

static VOID _CmdCrash(VOID) {
    KeBugCheckEx(STOP_MANUALLY_INITIATED_CRASH,0,0,0,0);
}

static VOID _CmdReboot(VOID) {
    KdPrint("Rebooting...\n");
    BYTE v; do{v=HalReadPortByte(0x64);}while(v&0x02);
    HalWritePortByte(0x64,0xFE);
    __asm__ volatile("lidt (%0)\nint $0\n"::"r"((QWORD[]){0,0}));
    __builtin_unreachable();
}

static VOID _CmdShutdown(VOID) {
    KdPrint("Shutting down...\n");
    HalWritePortWord(0x604,0x2000);
    HalWritePortWord(0xB004,0x2000);
    __asm__ volatile("cli; hlt");
    __builtin_unreachable();
}

static VOID _Execute(PCSTR line) {
    while (*line==' ') line++;
    if (!*line) return;
    CHAR cmd[64]; INT ci=0;
    while (*line && *line!=' ' && ci<63) cmd[ci++]=*line++;
    cmd[ci]='\0';
    while (*line==' ') line++;
    PCSTR args=line;

    if      (!RtlCompareString(cmd,"help"))     _CmdHelp();
    else if (!RtlCompareString(cmd,"cls"))      _CmdCls();
    else if (!RtlCompareString(cmd,"echo"))     _CmdEcho(args);
    else if (!RtlCompareString(cmd,"ver"))      _CmdVer();
    else if (!RtlCompareString(cmd,"mem"))      _CmdMem();
    else if (!RtlCompareString(cmd,"sysinfo"))  _CmdSysinfo();
    else if (!RtlCompareString(cmd,"time"))     _CmdTime();
    else if (!RtlCompareString(cmd,"set"))      _CmdSet(args);
    else if (!RtlCompareString(cmd,"dir"))      _CmdDir();
    else if (!RtlCompareString(cmd,"color"))    _CmdColor(args);
    else if (!RtlCompareString(cmd,"net"))      _CmdNet();
    else if (!RtlCompareString(cmd,"netping"))  _CmdNetping();
    else if (!RtlCompareString(cmd,"gfxtest"))  _CmdGfxtest();
    else if (!RtlCompareString(cmd,"drivers"))  _CmdDrivers();
    else if (!RtlCompareString(cmd,"fetch"))    _CmdFetch();
    else if (!RtlCompareString(cmd,"crash"))    _CmdCrash();
    else if (!RtlCompareString(cmd,"reboot"))   _CmdReboot();
    else if (!RtlCompareString(cmd,"shutdown")) _CmdShutdown();
    else KdPrintf("'%s' is not recognized. Type 'help'.\n", cmd);
}
