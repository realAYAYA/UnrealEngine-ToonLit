// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureBuilderFactory.h"

#include "AssetTypeCategories.h"
#include "VT/VirtualTextureBuilder.h"

UVirtualTextureBuilderFactory::UVirtualTextureBuilderFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UVirtualTextureBuilder::StaticClass();
	
	bCreateNew = true;
	bEditAfterNew = false;
	bEditorImport = false;
}

UObject* UVirtualTextureBuilderFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	return NewObject<UVirtualTextureBuilder>(InParent, Class, Name, Flags);
}
