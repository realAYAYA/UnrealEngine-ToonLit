// Copyright Epic Games, Inc. All Rights Reserved.

#include "PlacementPaletteAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PlacementPaletteAsset)

UPlacementPaletteAssetFactory::UPlacementPaletteAssetFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UPlacementPaletteAsset::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UPlacementPaletteAssetFactory::FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	UPlacementPaletteAsset* PlacementPaletteAsset = NewObject<UPlacementPaletteAsset>(InParent, InClass, InName, Flags, Context);
	if (PlacementPaletteAsset)
	{
		PlacementPaletteAsset->GridGuid = FGuid::NewGuid();
	}
	return PlacementPaletteAsset;
}

bool UPlacementPaletteAssetFactory::ShouldShowInNewMenu() const
{
	return true;
}

FText UPlacementPaletteAssetFactory::GetToolTip() const
{
	return NSLOCTEXT("AssetPlacementEdMode", "PlacementPaletteToolTip", "Placement palettes can be used in the placement mode to quickly place many different asset types in a level with preconfigured settings.");
}

