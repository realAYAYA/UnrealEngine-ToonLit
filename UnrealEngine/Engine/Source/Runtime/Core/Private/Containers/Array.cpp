// Copyright Epic Games, Inc. All Rights Reserved.

#include "Containers/Array.h"
#include "CoreGlobals.h"

void UE4Array_Private::OnInvalidArrayNum(const TCHAR* ArrayNameSuffix, unsigned long long NewNum)
{
	UE_LOG(LogCore, Fatal, TEXT("Trying to resize TArray%s to an invalid size of %llu"), ArrayNameSuffix, NewNum);
}
