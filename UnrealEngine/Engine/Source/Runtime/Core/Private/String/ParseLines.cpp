// Copyright Epic Games, Inc. All Rights Reserved.

#include "String/ParseLines.h"

#include "Templates/Function.h"

namespace UE::String
{

void ParseLines(
	const FStringView View,
	const TFunctionRef<void (FStringView)> Visitor,
	const EParseLinesOptions Options)
{
	const TCHAR* ViewIt = View.GetData();
	const TCHAR* ViewEnd = ViewIt + View.Len();
	do
	{
		const TCHAR* LineStart = ViewIt;
		const TCHAR* LineEnd = ViewEnd;
		for (; ViewIt != ViewEnd; ++ViewIt)
		{
			const TCHAR CurrentChar = *ViewIt;
			if (CurrentChar == TEXT('\n'))
			{
				LineEnd = ViewIt++;
				break;
			}
			if (CurrentChar == TEXT('\r'))
			{
				LineEnd = ViewIt++;
				if (ViewIt != ViewEnd && *ViewIt == TEXT('\n'))
				{
					++ViewIt;
				}
				break;
			}
		}

		FStringView Line(LineStart, UE_PTRDIFF_TO_INT32(LineEnd - LineStart));
		if (EnumHasAnyFlags(Options, EParseLinesOptions::Trim))
		{
			Line = Line.TrimStartAndEnd();
		}
		if (!EnumHasAnyFlags(Options, EParseLinesOptions::SkipEmpty) || !Line.IsEmpty())
		{
			Visitor(Line);
		}
	}
	while (ViewIt != ViewEnd);
}

} // UE::String
