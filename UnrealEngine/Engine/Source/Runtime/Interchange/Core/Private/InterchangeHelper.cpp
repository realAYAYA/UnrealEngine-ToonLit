// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeHelper.h"

namespace UE::Interchange
{
	FString MakeName(const FString& InName, bool bIsJoint)
	{
		const TCHAR* InvalidChar = bIsJoint ? INVALID_OBJECTNAME_CHARACTERS TEXT("+ ") : INVALID_OBJECTNAME_CHARACTERS;
		FString TmpName = InName;

		while (*InvalidChar)
		{
			TmpName.ReplaceCharInline(*InvalidChar, TCHAR('_'), ESearchCase::CaseSensitive);
			++InvalidChar;
		}

		return TmpName;
	}
};