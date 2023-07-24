// Copyright Epic Games, Inc. All Rights Reserved.

#include "LyraRuntimeOptions.h"

#include "UObject/Class.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LyraRuntimeOptions)

ULyraRuntimeOptions::ULyraRuntimeOptions()
{
	OptionCommandPrefix = TEXT("ro");
}

ULyraRuntimeOptions* ULyraRuntimeOptions::GetRuntimeOptions()
{
	return GetMutableDefault<ULyraRuntimeOptions>();
}

const ULyraRuntimeOptions& ULyraRuntimeOptions::Get()
{
	const ULyraRuntimeOptions& RuntimeOptions = *GetDefault<ULyraRuntimeOptions>();
	return RuntimeOptions;
}
