// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/HierarchicalLogArchive.h"

#include "Containers/StringConv.h"
#include "Containers/StringFwd.h"
#include "Misc/StringBuilder.h"

class FArchive;

FHierarchicalLogArchive::FHierarchicalLogArchive(FArchive& InInnerArchive)
	: FArchiveProxy(InInnerArchive)
	, Indentation(0)
{}

void FHierarchicalLogArchive::WriteLine(const FString& InLine, bool bIndent)
{
	TStringBuilder<512> Builder;

	if (Indentation > 0)
	{
		for (int i = 0; i < Indentation - 1; ++i)
		{
			Builder += TEXT(" |  ");
		}

		Builder += TEXT(" |- ");
	}
	
	if (bIndent)
	{
		Builder += TEXT("[+] ");
	}

	Builder += InLine;
	Builder += LINE_TERMINATOR;
	FString LineEntry = Builder.ToString();

	Serialize(TCHAR_TO_ANSI(*LineEntry), LineEntry.Len());
}