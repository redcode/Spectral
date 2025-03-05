#pragma once
#ifdef _WIN32
#include <winsock2.h>

char* va(const char *fmt, ...);

// utf8 to wstring
wchar_t* widen( const char* input ) {
    if( !input ) {
        return NULL;
    }
    int inlen = strlen(input);
    int outlen = MultiByteToWideChar(CP_UTF8, 0, input, inlen, 0, 0);
    wchar_t *output = (wchar_t*)va("%.*s", (outlen+1) * sizeof(wchar_t), "");
    if( output ) {
        MultiByteToWideChar(CP_UTF8, 0, input, inlen, output, outlen);
        output[outlen] = L'\0';
    }
    return output;
}

// wstring to utf8
char* shorten( const wchar_t* input ) {
    if( !input ) {
        return NULL;
    }
    int inlen = wcslen(input);
    int outlen = WideCharToMultiByte(CP_UTF8, 0, input, inlen, 0, 0, NULL, NULL);
    char *output = va("%.*s", (outlen+1) * sizeof(char), "");
    if( output ) {
        WideCharToMultiByte(CP_UTF8, 0, input, inlen, output, outlen, NULL, NULL);
        output[outlen] = '\0';
    }
    return output;
}

#else

#define widen(x) (x)
#define shorten(x) (x)

#endif
