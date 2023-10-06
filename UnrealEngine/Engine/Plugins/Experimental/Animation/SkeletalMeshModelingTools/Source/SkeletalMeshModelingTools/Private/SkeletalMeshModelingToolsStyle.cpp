// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshModelingToolsStyle.h"

#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"


FSkeletalMeshModelingToolsStyle::FSkeletalMeshModelingToolsStyle() : 
	FSlateStyleSet("SkeletalMeshModelingToolsStyle")
{
	
}


FSkeletalMeshModelingToolsStyle& FSkeletalMeshModelingToolsStyle::Get()
{
	static FSkeletalMeshModelingToolsStyle Instance;
	return Instance;
}


void FSkeletalMeshModelingToolsStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}


void FSkeletalMeshModelingToolsStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}
