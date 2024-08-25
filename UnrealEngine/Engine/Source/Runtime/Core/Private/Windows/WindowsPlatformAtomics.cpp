// Copyright Epic Games, Inc. All Rights Reserved.

#include "Windows/WindowsPlatformAtomics.h"
#include "Misc/AssertionMacros.h"
#include "Logging/LogMacros.h"
#include "Templates/UnrealTemplate.h"
#include "CoreGlobals.h"


void FWindowsPlatformAtomics::HandleAtomicsFailure(const TCHAR* InFormat, ...)
{
	TCHAR TempStr[1024];
	GET_TYPED_VARARGS(TCHAR, TempStr, UE_ARRAY_COUNT(TempStr), UE_ARRAY_COUNT(TempStr) - 1, InFormat, InFormat);
	UE_LOG(LogWindows, Fatal, TEXT("%s"), TempStr);
}
