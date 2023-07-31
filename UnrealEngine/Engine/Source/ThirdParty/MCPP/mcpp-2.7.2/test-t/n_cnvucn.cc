/* n_cnvucn.t:  C++ Conversion from multi-byte character to UCN.    */
/* This conversion takes place in the translation phase 1.  */

#define str( a)     # a

    /* "\"\u6F22\u5B57\"" or "\"\\u6F22\\u5B57\""   */
    str( "����")

/* Multi-byte characters in identifier. */
#define �ޥ���  ����
#define �ؿ��ͥޥ���(����1, ����2)  ����1 ## ����2
/*  ����;   */
    �ޥ���;
/*  ������̾��; */
    �ؿ��ͥޥ���(������, ̾��);

/* Multi-byte character in pp-number.   */
#define mkname( a)  a ## 1��
#define mkstr( a)   xmkstr( a)
#define xmkstr( a)  # a
/*  "abc1��"    */
    char *  mkstr( mkname( abc));

