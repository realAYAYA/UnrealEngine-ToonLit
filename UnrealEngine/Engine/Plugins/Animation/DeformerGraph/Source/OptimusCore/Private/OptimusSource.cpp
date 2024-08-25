// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusSource.h"
#include "OptimusHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusSource)

void UOptimusSource::SetSource(const FString& InText)
{
	SourceText = InText;
	
	Modify();
}

FString UOptimusSource::GetVirtualPath() const 
{
	FString ShaderPathName = GetPathName();
	Optimus::ConvertObjectPathToShaderFilePath(ShaderPathName);
	return ShaderPathName;
}

#if WITH_EDITOR	

FString UOptimusSource::GetNameForShaderTextEditor() const
{
	return GetFName().ToString();
}

bool UOptimusSource::IsShaderTextReadOnly() const
{
	return false;
}

#endif // WITH_EDITOR	
