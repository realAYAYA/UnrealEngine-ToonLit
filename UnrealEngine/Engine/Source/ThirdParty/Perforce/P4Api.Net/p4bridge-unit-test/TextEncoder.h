#pragma once

typedef struct surrogatePair
{
    wchar_t first;
    wchar_t second;
} SurrogatePair;

class TextEncoder
{
public:
    TextEncoder(void);
    ~TextEncoder(void);

    static void SmokeTest(void);

    static char * CopyStr(const char *s);
    static wchar_t * CopyWStr(const wchar_t *s);

    static SurrogatePair UnicodePointToSurrogatePair(unsigned long codePoint);

    static unsigned long SurrogatePairToUnicodePoint(wchar_t first, wchar_t second);
    static unsigned long SurrogatePairToUnicodePoint(SurrogatePair surrogates);

    static void RecodeUtf8CharInUtf16(const char * pSrc, wchar_t * pDest);
    static void RecodeUtf16CharInUtf8(const wchar_t * pSrc, char * pDest);

    static wchar_t * Utf8ToUtf16(const char * pStr);
    static wchar_t * AsciiToUtf16(const char * pStr);
    static int Utf16StrLen(const wchar_t * pStr);  // length in characters, not bytes
    static int Utf16StrBytes(const wchar_t * pStr);  // length in bytes, not characters
    static int Utf16CharSize(const wchar_t c);  // How many wchar_t's in this character, 1 or 2?

    static char * Utf16ToUtf8(const wchar_t * pStr);
    static char * AsciiToUtf8(const char * pStr);
    static int Utf8StrLen(const char* pStr);  // length in characters, not bytes
    static int Utf8StrBytes(const char * pStr);  // length in bytes, not characters
    static int Utf8CharSize(const char c);  // How many bytes in this character, 1, 2, 3, or 4?

    static char * Utf16ToAscii(const wchar_t * pStr);
    static char * Utf8ToAscii(const char * pStr);
};

