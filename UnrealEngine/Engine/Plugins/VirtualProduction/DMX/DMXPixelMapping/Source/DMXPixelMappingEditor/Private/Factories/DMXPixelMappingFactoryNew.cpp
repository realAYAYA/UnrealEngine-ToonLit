// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/DMXPixelMappingFactoryNew.h"
#include "DMXPixelMappingEditorModule.h"
#include "DMXPixelMapping.h"
#include "AssetsTools/AssetTypeActions_DMXPixelMapping.h"
#include "DMXPixelMappingEditorUtils.h"

#include "IAssetTypeActions.h"
#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "DMXPixelMappingFactoryNew"

UDMXPixelMappingFactoryNew::UDMXPixelMappingFactoryNew()
{
	//~ Initialize parent class properties

	// This factory is responsible for manufacturing DMXPixelMapping assets.
	SupportedClass = UDMXPixelMapping::StaticClass();

	// This factory manufacture new objects from scratch.
	bCreateNew = true;

	// This factory will open the editor for each new object.
	bEditAfterNew = true;
}

FName UDMXPixelMappingFactoryNew::GetNewAssetThumbnailOverride() const
{
	return TEXT("ClassThumbnail.DMXPixelMapping");
}

UObject* UDMXPixelMappingFactoryNew::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UDMXPixelMapping::StaticClass()));
	ensure(0 != (RF_Public & Flags));

	UDMXPixelMapping* DMXPixelMapping = NewObject<UDMXPixelMapping>(InParent, Class, Name, Flags);

	// Create all essentials UObjects
	DMXPixelMapping->CreateOrLoadObjects();

	// Add at least one renderer for a new Asset
	FDMXPixelMappingEditorUtils::AddRenderer(DMXPixelMapping);

	return DMXPixelMapping;
}

FText UDMXPixelMappingFactoryNew::GetDisplayName() const
{
	// Get the displayname for this factory from the action type.
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	UClass* LocalSupportedClass = GetSupportedClass();
	if (LocalSupportedClass)
	{
		TArray<TWeakPtr<IAssetTypeActions>> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsListForClass(LocalSupportedClass);
		for (auto& CurrentAssetTypeAction : AssetTypeActions)
		{
			TSharedPtr<FAssetTypeActions_DMXPixelMapping> AssetTypeAction = StaticCastSharedPtr<FAssetTypeActions_DMXPixelMapping>(CurrentAssetTypeAction.Pin());
			if (AssetTypeAction.IsValid())
			{
				FText Name = AssetTypeAction->GetName();
				if (!Name.IsEmpty())
				{
					return Name;
				}
			}
		}
	}

	// Factories that have no supported class have no display name.
	return FText();
}

uint32 UDMXPixelMappingFactoryNew::GetMenuCategories() const
{
	return uint32(FDMXPixelMappingEditorModule::GetAssetCategory());
}

#undef LOCTEXT_NAMESPACE
