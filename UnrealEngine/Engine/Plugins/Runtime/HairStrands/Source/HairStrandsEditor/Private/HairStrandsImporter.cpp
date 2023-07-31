// Copyright Epic Games, Inc. All Rights Reserved.

#include "HairStrandsImporter.h"

#include "HairDescription.h"
#include "GroomAsset.h"
#include "GroomBuilder.h"
#include "GroomImportOptions.h"

DEFINE_LOG_CATEGORY_STATIC(LogHairImporter, Log, All);

FHairImportContext::FHairImportContext(UGroomImportOptions* InImportOptions, UObject* InParent, UClass* InClass, FName InName, EObjectFlags InFlags)
	: ImportOptions(InImportOptions)
	, Parent(InParent)
	, Class(InClass)
	, Name(InName)
	, Flags(InFlags)
{
}

UGroomAsset* FHairStrandsImporter::ImportHair(const FHairImportContext& ImportContext, FHairDescription& HairDescription, UGroomAsset* ExistingHair)
{
	const uint32 GroupCount = ImportContext.ImportOptions->InterpolationSettings.Num();
	UGroomAsset* OutHairAsset = nullptr;
	if (ExistingHair)
	{
		OutHairAsset = ExistingHair;
	}
	else
	{
		OutHairAsset = NewObject<UGroomAsset>(ImportContext.Parent, ImportContext.Class, ImportContext.Name, ImportContext.Flags);
		if (!OutHairAsset)
		{
			UE_LOG(LogHairImporter, Warning, TEXT("Failed to import hair: Could not allocate memory to create asset."));
			return nullptr;
		}
	}
	OutHairAsset->SetNumGroup(GroupCount);

	// Sanity check
	check(OutHairAsset->AreGroupsValid());
	check(uint32(OutHairAsset->GetNumHairGroups()) == GroupCount);

	OutHairAsset->CommitHairDescription(MoveTemp(HairDescription), UGroomAsset::EHairDescriptionType::Source);

	// Populate the interpolation settings with the new settings from the importer	
	for (uint32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		// Insure the interpolation settings matches between the importer and the actual asset
		const FHairGroupsInterpolation& InterpolationSettings = ImportContext.ImportOptions->InterpolationSettings[GroupIndex];
		OutHairAsset->HairGroupsInterpolation[GroupIndex] = InterpolationSettings;
	}

	const bool bSucceeded = OutHairAsset->CacheDerivedDatas();
	if (!bSucceeded)
	{
		// Purge the newly created asset that failed to import
		if (OutHairAsset != ExistingHair)
		{
			OutHairAsset->ClearFlags(RF_Standalone);
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
		return nullptr;
	}

	return OutHairAsset;
}
