/* m_36_sjis.c: Handling of '\\' in shift-JIS multi-byte character. */

#include    "defs.h"

#define     str( a)     # a

main( void)
{
    fputs( "started\n", stdout);

/* 36.1:    0x5c in multi-byte character is not an escape character */

#pragma __setlocale( "sjis")                /* For MCPP     */
#pragma setlocale( "japanese")              /* For Visual C */

#if     '��' == '\x8e\x9a' && '�\' != '\x95\x5c'
    fputs( "Bad handling of '\\' in multi-byte character", stdout);
    exit( 1);
#endif

/* 36.2:    # operater should not insert '\\' before 0x5c in multi-byte
        character   */
    assert( strcmp( str( "�\��"), "\"�\��\"") == 0);
    fputs( "�\��" "\"�\��\"\n", stdout);

    fputs( "success\n", stdout);
    return  0;
}

