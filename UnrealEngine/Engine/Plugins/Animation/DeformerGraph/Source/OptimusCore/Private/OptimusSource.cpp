// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusSource.h"

void UOptimusSource::SetSource(const FString& InText)
{
	SourceText = InText;
	
	Modify();
}

#if WITH_EDITOR	

FString UOptimusSource::GetNameForShaderTextEditor() const
{
	return GetFName().ToString();
}

#endif // WITH_EDITOR	
