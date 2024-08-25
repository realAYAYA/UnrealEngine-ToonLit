// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMDefs.h"
#include "Misc/Paths.h"

int32 FDMUpdateGuard::GuardCount = 0;
uint32 FDMInitializationGuard::GuardCount = 0;

FString UE::DynamicMaterial::CreateNodeComment(const ANSICHAR* InFile, int InLine, const ANSICHAR* InFunction, const FString* InComment)
{
	FString File = FPaths::GetCleanFilename(ANSI_TO_TCHAR(InFile)).RightChop(2);

	if (InComment)
	{
		return FString::Printf(TEXT("%s[%d]: %hs: %s"), *File, InLine, InFunction, **InComment);
	}
	else
	{
		return FString::Printf(TEXT("%s[%d]: %hs"), *File, InLine, InFunction);
	}
}

bool FDMInitializationGuard::IsInitializing()
{
	return GuardCount > 0;
}

FDMInitializationGuard::FDMInitializationGuard()
{
	// Used the struct name to make it clear it's a static variable.
	++FDMInitializationGuard::GuardCount;
}

FDMInitializationGuard::~FDMInitializationGuard()
{
	if (FDMInitializationGuard::GuardCount > 0)
	{
		--FDMInitializationGuard::GuardCount;
	}
}
