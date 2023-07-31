/*******************************************************************************
Copyright (c) 2010, Perforce Software, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL PERFORCE SOFTWARE, INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*******************************************************************************
 * Name		: OutStr.cpp
 *
 * Author	: Duncan Barbee <dbarbee@perforce.com>
 *
 * Description	: A set of classes used to return strings.
 *
 ******************************************************************************/

#include "stdafx.h"
#include "OutStr.h"
#include "wchar.h"

void OutStr::CopyToBuff( void * buff, int length)
{
    if (this->IsUnicode())
        wcsncpy((WCHAR *) buff,(WCHAR *)Text(), length) ;
    else 
        strncpy((char *) buff, Text(), length);
}

//
//OutStrPtr::OutStrPtr(void) {};

OutStrPtr::OutStrPtr(const StrPtr &pval, int isUni) 
    : value (pval)
{
    isUnicode = isUni;
}
OutStrPtr::~OutStrPtr(void)
{
}

const char * OutStrPtr::Text()
{
    return value.Text();
}

int OutStrPtr::Length()
{
    return value.Length();
}

/******************************************************************************
// OutCharPtr is wrap a char *, so  that it can easily be returned in a 
//   preallocated buffer by first reading the length, allocating the buffer, 
//   then fetching string data.
******************************************************************************/

OutCharPtr::OutCharPtr(char * pval)
{
    // copy the string value, so we don't need to worry about the data changing
    // or the pointer being freed.
    value = CopyStr(pval);
    length = strlen(value);
}
OutCharPtr::~OutCharPtr(void)
{
    // free our copy of the data
    delete value;
}
const char * OutCharPtr::Text()
{
    return value;
}
int OutCharPtr::Length()
{
    return length;
}

/******************************************************************************
// OutWCharPtr is wrap a WCHAR *, so  that it can easily be returned in a 
//   preallocated buffer by first reading the length, allocating the buffer, 
//   then fetching string data.
******************************************************************************/

OutWCharPtr::OutWCharPtr(WCHAR * pval)
{
    // copy the string value, so we don't need to worry about the data changing
    // or the pointer being freed.
    value = CopyWStr(pval);
    length = wcslen(value);
}
OutWCharPtr::~OutWCharPtr(void)
{
    // free our copy of the data
    delete value;
}
const char * OutWCharPtr::Text()
{
    return (char *) value;
}
int OutWCharPtr::Length()
{
    return length;
}
