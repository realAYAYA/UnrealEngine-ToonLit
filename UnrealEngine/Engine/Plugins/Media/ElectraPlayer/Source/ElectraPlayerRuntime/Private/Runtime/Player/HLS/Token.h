// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace Electra
{
    namespace HLSPlaylistParser
    {
        const TCHAR CComment = TCHAR('#');
        const TCHAR CColumn = TCHAR(':');
        const TCHAR CLineBreak = TCHAR('\n');
        const TCHAR CAttributeSeparation = TCHAR(',');
        const TCHAR CSpace = TCHAR(' ');

        typedef unsigned int Token;

        const Token TUnknown = 0xFFFFFFFF;
        const Token TComment = 1;
        const Token TLineBreak = 1 << 1;
        const Token TColumn = 1 << 2;
        const Token TAttributeDeclaration = 1 << 3;
        const Token TSpace = 1 << 4;
        const Token TQuote = 1 << 5;
        const Token TAttributeSeparation = 1 << 6;
        const Token TEOF = 1 << 7;
        const Token TIgnore = 1 << 8;

        Token LookupToken(TCHAR p);
    }
} // namespace Electra



