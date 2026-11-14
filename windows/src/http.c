#include "luxai.h"
#include <winhttp.h>


/* WinHTTP tabanli HTTPS istemcisi (Windows 7 SP1 uyumlu; TLS 1.2 destegi
 * isletim sisteminde etkinse kullanilir). Goumlen veri parca parca sink'e verilir. */

typedef struct { HINTERNET hSession, hConnect, hRequest; } WHTTP;

static void whtttp_close(WHTTP *w)
{
    if (w->hRequest) WinHttpCloseHandle(w->hRequest);
    if (w->hConnect) WinHttpCloseHandle(w->hConnect);
    if (w->hSession) WinHttpCloseHandle(w->hSession);
    w->hRequest = w->hConnect = w->hSession = NULL;
}

static void set_err(Buf *err, const wchar_t *what)
{
    char *s = u8(what);
    if (err) { buf_adds(err, s ? s : "error"); }
    free(s);
}

int http_request(const wchar_t *url, const wchar_t *extra_headers,
                 const char *body, size_t bodylen,
                 HttpSink sink, void *ud, int *status, Buf *err,
                 volatile LONG *cancel)
{
    WHTTP w = { NULL, NULL, NULL };
    wchar_t host[256], path[2048];
    URL_COMPONENTS uc;
    int rc = -1;

    if (status) *status = 0;
    if (err) buf_clear(err);

    ZeroMemory(&uc, sizeof uc);
    uc.dwStructSize = sizeof uc;
    uc.lpszHostName = host;  uc.dwHostNameLength = 255;
    uc.lpszUrlPath = path;   uc.dwUrlPathLength = 2047;
    if (!WinHttpCrackUrl(url, 0, 0, &uc)) { set_err(err, L"URL ayristirilamadi"); return -1; }

    w.hSession = WinHttpOpen(L"LuxAI-Windows/3.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                             WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!w.hSession) { set_err(err, L"WinHttpOpen basarisiz"); return -1; }
    /* Win7'de de TLS 1.2 denenir (sistemde etkinse calisir) */
    {
        DWORD prot = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
#ifdef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1
        prot |= WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_1 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1;
#endif
        WinHttpSetOption(w.hSession, WINHTTP_OPTION_SECURE_PROTOCOLS, &prot, sizeof prot);
    }
    WinHttpSetTimeouts(w.hSession, 30000, 30000, 60000, 300000);

    w.hConnect = WinHttpConnect(w.hSession, host, uc.nPort, 0);
    if (!w.hConnect) { set_err(err, L"Sunucuya baglanilamadi (WinHttpConnect)"); goto done; }

    w.hRequest = WinHttpOpenRequest(w.hConnect, L"POST", path, NULL,
                                   WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                   (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
    if (!w.hRequest) { set_err(err, L"Istek olusturulamadi"); goto done; }

    if (!WinHttpSendRequest(w.hRequest,
                            extra_headers ? extra_headers : WINHTTP_NO_ADDITIONAL_HEADERS,
                            extra_headers ? (DWORD)-1L : 0,
                            (LPVOID)body, (DWORD)bodylen, (DWORD)bodylen, 0)) {
        set_err(err, L"Gonderim basarisiz (ag/TLS hatasi)");
        goto done;
    }
    if (!WinHttpReceiveResponse(w.hRequest, NULL)) {
        set_err(err, L"Yanit alinamadi (ag/TLS hatasi)");
        goto done;
    }
    if (status) {
        DWORD code = 0, len = sizeof code;
        WinHttpQueryHeaders(w.hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &code, &len, WINHTTP_NO_HEADER_INDEX);
        *status = (int)code;
    }
    for (;;) {
        if (cancel && *cancel) { set_err(err, L"Iptal edildi"); goto done; }
        DWORD avail = 0;
        if (!WinHttpQueryDataAvailable(w.hRequest, &avail)) {
            set_err(err, L"Veri okunamadi");
            goto done;
        }
        if (avail == 0) { rc = 0; break; }
        char *chunk = (char *)malloc(avail + 1);
        if (!chunk) { set_err(err, L"bellek"); goto done; }
        DWORD got = 0;
        if (!WinHttpReadData(w.hRequest, chunk, avail, &got)) {
            free(chunk);
            set_err(err, L"Veri okunamadi");
            goto done;
        }
        chunk[got] = 0;
        if (got && sink && sink(chunk, got, ud) != 0) { free(chunk); rc = 0; break; }
        free(chunk);
        if (got == 0) { rc = 0; break; }
    }
done:
    whtttp_close(&w);
    return rc;
}
