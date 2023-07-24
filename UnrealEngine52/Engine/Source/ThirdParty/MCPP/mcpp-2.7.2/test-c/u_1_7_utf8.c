/* u_1_7_utf8.c:    Invalid multi-byte character sequence (in string literal,
        character constant, header-name or comment).    */

#define str( a)     # a
#pragma __setlocale( "utf8")                /* For MCPP     */

main( void)
{
    char *  cp = str( "字");   /* 0xe5ad97 : legal */
    char *  ecp1 = str( "��");   /* 0xc0af   : overlong  */
    char *  ecp2 = str( "���");   /* 0xe09fbf : overlong  */
    char *  ecp3 = str( "�");   /* 0xeda080 : UTF-16 surrogate  */

    return  0;
}

