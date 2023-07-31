// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"

class FHairDescription;
class UGroomAsset;
class UGroomImportOptions;

struct HAIRSTRANDSEDITOR_API FHairImportContext
{
	FHairImportContext(UGroomImportOptions* InImportOptions, UObject* InParent = nullptr, UClass* InClass = nullptr, FName InName = FName(), EObjectFlags InFlags = EObjectFlags::RF_NoFlags);

	UGroomImportOptions* ImportOptions;
	UObject* Parent;
	UClass* Class;
	FName Name;
	EObjectFlags Flags;
};

struct HAIRSTRANDSEDITOR_API FHairStrandsImporter
{
	static UGroomAsset* ImportHair(const FHairImportContext& ImportContext, FHairDescription& HairDescription, UGroomAsset* ExistingHair = nullptr);
};
