/* NoUbiNoCry - Far Cry 3: Blood Dragon
 *
 * dinput8.dll proxy that cuts the game's own network access, so its attempts to
 * reach Ubisoft's dead RendezVous backend fail instantly instead of timing out
 * (the "Connecting to Ubisoft servers" popup + long hang on boot and pause).
 *
 * How: FC3.dll / FC3_d3d11.dll import table entries for winsock name lookups
 * (getaddrinfo, gethostbyname), connect, sendto and WSASendTo are redirected to
 * stubs that fail anything not on this machine. The game's own login code then
 * runs unmodified and simply gets "host not found" right away.
 * Only the game module is affected: Ubisoft Connect (saves, achievements) and
 * Steam talk through their own DLLs and are untouched.
 *
 * NoUbiNoCry.log (next to the DLL, fresh each launch) records the patch result
 * and every blocked lookup/connection with a timestamp.
 *
 * Build: build.cmd (x86 MSVC). Install: copy dinput8.dll into <game>\bin\.
 *
 * Copyright (c) 2026 OutlawRGB. MIT License, see LICENSE.
 */
#define _WINSOCK_DEPRECATED_NO_WARNINGS   /* the game uses gethostbyname; we stub it */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "version.h"

#pragma comment(lib, "ws2_32.lib")
#pragma comment(linker, "/EXPORT:DirectInput8Create=_DirectInput8Create@20")

static char g_log[MAX_PATH];    /* empty = stdout (selftest) */
static DWORD g_t0;

static void logf(const char *fmt, ...)
{
    FILE *f = g_log[0] ? fopen(g_log, "a") : stdout;
    va_list ap;
    if (!f) return;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    if (f != stdout) fclose(f);
}

static int g_blocked;             /* log only the first few hundred */

static void log_blocked(const char *what, const char *target)
{
    DWORD ms = GetTickCount() - g_t0;
    if (++g_blocked > 300) return;
    logf("[%4lu.%03lus] blocked %s %s\n", ms / 1000, ms % 1000, what, target);
}

/* Lookups allowed: NULL/empty (local bind), localhost, this PC's own name,
 * numeric loopback. Everything else is "host not found". */
static int name_allowed(const char *name)
{
    char self[256];
    if (!name || !*name) return 1;
    if (!_stricmp(name, "localhost") || !strncmp(name, "127.", 4) || !strcmp(name, "::1")) return 1;
    return gethostname(self, sizeof(self)) == 0 && !_stricmp(name, self);
}

static int addr_allowed(const struct sockaddr *sa)
{
    if (!sa) return 1;
    if (sa->sa_family == AF_INET)
        return ((const struct sockaddr_in *)sa)->sin_addr.S_un.S_un_b.s_b1 == 127;
    if (sa->sa_family == AF_INET6)
        return IN6_IS_ADDR_LOOPBACK(&((const struct sockaddr_in6 *)sa)->sin6_addr);
    return 1;   /* not IP */
}

static const char *addr_text(const struct sockaddr *sa, char *buf, DWORD n)
{
    DWORD len = n;
    if (!sa || WSAAddressToStringA((LPSOCKADDR)sa, sa->sa_family == AF_INET6 ? sizeof(struct sockaddr_in6)
                                   : sizeof(struct sockaddr_in), NULL, buf, &len))
        strcpy_s(buf, n, "?");
    return buf;
}

static int WSAAPI net_getaddrinfo(PCSTR node, PCSTR service, const ADDRINFOA *hints, PADDRINFOA *res)
{
    if (name_allowed(node)) return getaddrinfo(node, service, hints, res);
    log_blocked("lookup", node);
    if (res) *res = NULL;
    WSASetLastError(WSAHOST_NOT_FOUND);
    return WSAHOST_NOT_FOUND;
}

static struct hostent *WSAAPI net_gethostbyname(const char *name)
{
    if (name_allowed(name)) return gethostbyname(name);
    log_blocked("lookup", name);
    WSASetLastError(WSAHOST_NOT_FOUND);
    return NULL;
}

static int WSAAPI net_connect(SOCKET s, const struct sockaddr *to, int len)
{
    char buf[64];
    if (addr_allowed(to)) return connect(s, to, len);
    log_blocked("connect", addr_text(to, buf, sizeof(buf)));
    WSASetLastError(WSAENETUNREACH);
    return SOCKET_ERROR;
}

static int WSAAPI net_sendto(SOCKET s, const char *data, int n, int flags, const struct sockaddr *to, int len)
{
    char buf[64];
    if (addr_allowed(to)) return sendto(s, data, n, flags, to, len);
    log_blocked("send", addr_text(to, buf, sizeof(buf)));
    WSASetLastError(WSAENETUNREACH);
    return SOCKET_ERROR;
}

static int WSAAPI net_WSASendTo(SOCKET s, LPWSABUF bufs, DWORD nbufs, LPDWORD sent, DWORD flags,
                                const struct sockaddr *to, int len, LPWSAOVERLAPPED ov,
                                LPWSAOVERLAPPED_COMPLETION_ROUTINE done)
{
    char buf[64];
    if (addr_allowed(to)) return WSASendTo(s, bufs, nbufs, sent, flags, to, len, ov, done);
    log_blocked("send", addr_text(to, buf, sizeof(buf)));
    WSASetLastError(WSAENETUNREACH);
    return SOCKET_ERROR;
}

static BOOL write_slot(void **slot, void *value)
{
    DWORD old;
    if (!VirtualProtect(slot, sizeof(*slot), PAGE_READWRITE, &old)) return FALSE;
    *slot = value;
    VirtualProtect(slot, sizeof(*slot), old, &old);
    return TRUE;
}

/* Points the module's WS2_32 import slots at the stubs above. Returns slots patched;
 * *bound = how many already held a ws2_32 address (i.e. the loader had filled them). */
static int block_network(BYTE *base, int *bound)
{
    IMAGE_NT_HEADERS *nt = (IMAGE_NT_HEADERS *)(base + ((IMAGE_DOS_HEADER *)base)->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    IMAGE_IMPORT_DESCRIPTOR *d = (IMAGE_IMPORT_DESCRIPTOR *)(base + dir.VirtualAddress);
    BYTE *ws2 = (BYTE *)GetModuleHandleA("ws2_32.dll");
    IMAGE_NT_HEADERS *ws2_nt = ws2 ? (IMAGE_NT_HEADERS *)(ws2 + ((IMAGE_DOS_HEADER *)ws2)->e_lfanew) : NULL;
    int patched = 0;
    *bound = 0;

    for (; dir.VirtualAddress && d->Name; d++) {
        IMAGE_THUNK_DATA *names, *iat;
        if (_stricmp((char *)base + d->Name, "WS2_32.dll")) continue;
        names = (IMAGE_THUNK_DATA *)(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
        iat = (IMAGE_THUNK_DATA *)(base + d->FirstThunk);
        for (; names->u1.AddressOfData; names++, iat++) {
            void *stub = NULL;
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) {
                switch (IMAGE_ORDINAL(names->u1.Ordinal)) {
                case 4:  stub = net_connect; break;
                case 20: stub = net_sendto; break;
                case 52: stub = net_gethostbyname; break;
                }
            } else {
                const char *fn = (const char *)((IMAGE_IMPORT_BY_NAME *)(base + names->u1.AddressOfData))->Name;
                if (!strcmp(fn, "getaddrinfo")) stub = net_getaddrinfo;
                else if (!strcmp(fn, "WSASendTo")) stub = net_WSASendTo;
                else if (!strcmp(fn, "connect")) stub = net_connect;
                else if (!strcmp(fn, "sendto")) stub = net_sendto;
                else if (!strcmp(fn, "gethostbyname")) stub = net_gethostbyname;
            }
            if (!stub) continue;
            if (ws2_nt && (BYTE *)iat->u1.Function >= ws2 &&
                (BYTE *)iat->u1.Function < ws2 + ws2_nt->OptionalHeader.SizeOfImage)
                (*bound)++;
            if (write_slot((void **)&iat->u1.Function, stub)) patched++;
        }
    }
    return patched;
}

/* Returns the number of network imports redirected (0 = game left online). */
static int apply(HMODULE mod, const char *name)
{
    int patched, bound;
    g_t0 = GetTickCount();
    patched = block_network((BYTE *)mod, &bound);
    logf("NoUbiNoCry " NUNC_VERSION_STR " | %s: network cut off (%d winsock imports redirected, %d were already bound)\n",
         name, patched, bound);
    return patched;
}

typedef HRESULT (WINAPI *DI8Create)(HINSTANCE, DWORD, const GUID *, void **, IUnknown *);

HRESULT WINAPI DirectInput8Create(HINSTANCE inst, DWORD ver, const GUID *riid, void **out, IUnknown *outer)
{
    static DI8Create real;
    if (!real) {
        char path[MAX_PATH];
        HMODULE sys;
        GetSystemDirectoryA(path, MAX_PATH);
        strcat_s(path, MAX_PATH, "\\dinput8.dll");
        sys = LoadLibraryA(path);
        real = sys ? (DI8Create)GetProcAddress(sys, "DirectInput8Create") : NULL;
        if (!real) return E_FAIL;
    }
    return real(inst, ver, riid, out, outer);
}

#ifndef SELFTEST
BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID reserved)
{
    HMODULE game;
    char *slash;
    (void)reserved;
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(self);

    GetModuleFileNameA(self, g_log, MAX_PATH);  /* log goes next to the DLL, fresh each launch */
    slash = strrchr(g_log, '\\');
    strcpy_s(slash + 1, g_log + MAX_PATH - (slash + 1), "NoUbiNoCry.log");
    DeleteFileA(g_log);

    if ((game = GetModuleHandleA("FC3.dll")) != NULL) apply(game, "FC3.dll");
    else if ((game = GetModuleHandleA("FC3_d3d11.dll")) != NULL) apply(game, "FC3_d3d11.dll");
    else logf("FC3.dll / FC3_d3d11.dll not loaded - nothing patched\n");
    return TRUE;
}
#else
/* selftest.exe <path\FC3.dll>: maps the DLL without running it, applies the real
 * import redirection and checks the block policy. Exit 0 = OK. */
#define CHECK(cond, msg) do { if (!(cond)) { printf("FAIL: %s\n", msg); return 1; } } while (0)

int main(int argc, char **argv)
{
    struct sockaddr_in lo = { AF_INET }, ubi = { AF_INET };
    PADDRINFOA res = NULL;
    WSADATA wsa;
    HMODULE mod;
    const char *name;
    char self[256];

    if (argc != 2) { printf("usage: selftest <FC3.dll|FC3_d3d11.dll>\n"); return 2; }
    name = strrchr(argv[1], '\\') ? strrchr(argv[1], '\\') + 1 : argv[1];
    mod = LoadLibraryExA(argv[1], NULL, DONT_RESOLVE_DLL_REFERENCES);
    CHECK(mod, "cannot map the DLL");
    CHECK(apply(mod, name) == 5, "expected 5 winsock imports (connect, sendto, gethostbyname, getaddrinfo, WSASendTo)");

    WSAStartup(MAKEWORD(2, 2), &wsa);
    gethostname(self, sizeof(self));
    CHECK(name_allowed(NULL) && name_allowed("localhost") && name_allowed("127.0.0.1") && name_allowed(self),
          "local names must resolve");
    CHECK(!name_allowed("pdc-lb-rdv-prod02.ubisoft.com") && !name_allowed("onlineconfigservice.ubi.com"),
          "Ubisoft hosts must be blocked");
    CHECK(net_getaddrinfo("pdc-lb-rdv-prod02.ubisoft.com", "80", NULL, &res) == WSAHOST_NOT_FOUND && !res,
          "getaddrinfo stub");
    CHECK(!net_gethostbyname("onlineconfigservice.ubi.com") && WSAGetLastError() == WSAHOST_NOT_FOUND,
          "gethostbyname stub");
    lo.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ubi.sin_addr.s_addr = inet_addr("185.38.21.5");
    ubi.sin_port = htons(3074);
    CHECK(addr_allowed((struct sockaddr *)&lo) && !addr_allowed((struct sockaddr *)&ubi), "address policy");
    CHECK(net_sendto(INVALID_SOCKET, "x", 1, 0, (struct sockaddr *)&ubi, sizeof(ubi)) == SOCKET_ERROR &&
          WSAGetLastError() == WSAENETUNREACH, "sendto stub");

    printf("OK: %s\n", name);
    return 0;
}
#endif
