/*
 * Copyright 1995, 1996 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */


/*
 * BASE64 - decode base64 strings
 *
 */

class Base64
{
    public:
	static int      Decode( StrPtr &in, StrBuf &out );
	static void     Encode( StrPtr &in, StrBuf &out );
} ;
