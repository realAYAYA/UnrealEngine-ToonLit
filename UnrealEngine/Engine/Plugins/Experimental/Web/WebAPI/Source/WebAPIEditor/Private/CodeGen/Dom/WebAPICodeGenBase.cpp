// Copyright Epic Games, Inc. All Rights Reserved.

#include "CodeGen/Dom/WebAPICodeGenBase.h"

FWebAPICodeGenBase::FWebAPICodeGenBase()
{
	Specifiers.Add(TEXT("BlueprintType"), TEXT(""));
	Metadata.Add(TEXT("DisplayName"), GetName(true));
}

void FWebAPICodeGenBase::SetModule(const FString& InModule)
{
	Module = InModule;
}

// Wraps the Array overload
void FWebAPICodeGenBase::GetIncludePaths(TSet<FString>& OutIncludePaths) const
{
	TArray<FString> IncludePathsArray;
	GetIncludePaths(IncludePathsArray);
	OutIncludePaths.Append(IncludePathsArray);
}

const FName& FWebAPICodeGenBase::GetTypeName()
{
	static const FName None = NAME_None;
	return None;
}
