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
 * Name		: StrRef.h
 *
 * Author	: Duncan Barbee <dbarbee@perforce.com>
 *
 * Description	: A set of classes used to return strings.
 *
 ******************************************************************************/
#pragma once

class StrPtr;

/******************************************************************************
// StrRef is an abstract base class for any reference to text data that needs 
//   to be returned from the bridge. It provides a char* + length interface so 
//   that it can easily be returned in a preallocated buffer by first reading 
//   the length, allocating the buffer, then fetching string data.
******************************************************************************/

 class OutStr abstract: public p4base
{
public:
    OutStr(void) : p4base(Type()) {};
    virtual ~OutStr(void) {};

    virtual int Type(void) { return tOutStr; }

    void CopyToBuff( void * buff, int length);

    // Get the Text data and it's length
    virtual int IsUnicode() = 0;
    virtual const char * Text() = 0;
    virtual int Length() = 0;
};

/******************************************************************************
// StrPtrRef is wrap a StrPtr object, so  that it can easily be returned in a 
//   preallocated buffer by first reading the length, allocating the buffer, 
//   then fetching string data.
******************************************************************************/

class OutStrPtr : public OutStr
{
public:
    OutStrPtr(const StrPtr &pval, int isUni);
    virtual ~OutStrPtr(void);

    // Get the Text data and it's length
    virtual int IsUnicode() { return isUnicode; }
    virtual const char * Text();
    virtual int Length();

private:
    // Don't allow uninitialized references
    //StrPtrRef(void) {};

    int isUnicode;
    const StrPtr &value;
};

/******************************************************************************
// CharPtrRef is wrap a char *, so  that it can easily be returned in a 
//   preallocated buffer by first reading the length, allocating the buffer, 
//   then fetching string data.
******************************************************************************/
class OutCharPtr : public OutStr
{
public:
    OutCharPtr(char * pval);
    virtual ~OutCharPtr(void);

    // Get the Text data and it's length
    virtual int IsUnicode() { return 0; }
    virtual const char * Text();
    virtual int Length();

private:
    // Don't allow uninitialized references
    OutCharPtr(void) {};

    char * value;
    int length;
};

/******************************************************************************
// CharPtrRef is wrap a char *, so  that it can easily be returned in a 
//   preallocated buffer by first reading the length, allocating the buffer, 
//   then fetching string data.
******************************************************************************/
class OutWCharPtr : public OutStr
{
public:
    OutWCharPtr(WCHAR * pval);
    virtual ~OutWCharPtr(void);

    // Get the Text data and it's length
    virtual int IsUnicode() { return 1; }
    virtual const char * Text();
    virtual int Length();

private:
    // Don't allow uninitialized references
    OutWCharPtr(void) {};

    WCHAR * value;
    int length;
};

