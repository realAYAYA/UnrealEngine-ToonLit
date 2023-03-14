/* m_36_jis.c:  Handling of '\\' in ISO-2022-JP multi--byte character.  */

#include    "defs.h"

#define     str( a)     # a

main( void)
{
    fputs( "started\n", stdout);

/* 36.1:    0x5c in multi-byte character is not an escape character */

#pragma __setlocale( "jis")                 /* For MCPP     */
#pragma setlocale( "jis")                   /* For MCPP on VC   */

#if     '$B;z(B' == '\x3b\x7a' && '$B0\(B' != '\x30\x5c'
    fputs( "Bad handling of '\\' in multi-byte character", stdout);
    exit( 1);
#endif

/* 36.2:    # operater should not insert '\\' before 0x5c, 0x22 or 0x27
        in multi-byte character */
    assert( strcmp( str( "$B0\F0(B"), "\"$B0\F0(B\"") == 0);
    assert( strcmp( str( "$B1"M[(B"), "\"$B1"M[(B\"") == 0);
    assert( strcmp( str( "$B1'Ch(B"), "\"$B1'Ch(B\"") == 0);

    fputs( "$B0\F0(B" "\"$B0\F0(B\"\n", stdout);
    fputs( "$B1"M[(B" "\"$B1"M[(B\"\n", stdout);
    fputs( "$B1'Ch(B" "\"$B1'Ch(B\"\n", stdout);
    fputs( "success\n", stdout);
    return  0;
}

