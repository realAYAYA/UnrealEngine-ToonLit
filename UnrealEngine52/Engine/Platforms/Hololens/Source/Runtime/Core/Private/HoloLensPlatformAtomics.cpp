// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoloLensPlatformAtomics.h"
#include "CoreMinimal.h"

#if PLATFORM_HOLOLENS

void FHoloLensAtomics::HandleAtomicsFailure(const TCHAR* InFormat, ...)
{
	TCHAR TempStr[1024];
	GET_VARARGS(TempStr, UE_ARRAY_COUNT(TempStr), UE_ARRAY_COUNT(TempStr) - 1, InFormat, InFormat);
	UE_LOG(LogTemp, Fatal, TEXT("%s"), TempStr);
}

#endif //PLATFORM_HOLOLENS