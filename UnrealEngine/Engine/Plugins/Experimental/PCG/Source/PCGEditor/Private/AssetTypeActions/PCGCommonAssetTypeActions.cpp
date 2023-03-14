// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGCommonAssetTypeActions.h"
#include "PCGEditorModule.h"

uint32 FPCGCommonAssetTypeActions::GetCategories()
{
	return FPCGEditorModule::GetAssetCategory();
}

FColor FPCGCommonAssetTypeActions::GetTypeColor() const
{
	return FColor::Turquoise;
}