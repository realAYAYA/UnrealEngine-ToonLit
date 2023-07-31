// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "AssetTypeActions/AssetTypeActions_Blueprint.h"


namespace UE::RenderGrid::Private
{
	/**
	 * This class adds support for the RenderGrid (RenderGridBlueprint) class to the AssetTools module.
	 */
	class FRenderGridBlueprintActions : public FAssetTypeActions_Blueprint
	{
	public:
		//~ Begin IAssetTypeActions Interface
		virtual FText GetName() const override;
		virtual FColor GetTypeColor() const override;
		virtual UClass* GetSupportedClass() const override;
		virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
		virtual uint32 GetCategories() override;
		//~ End IAssetTypeActions Interface

		//~ Begin FAssetTypeActions_Blueprint interface
		virtual UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) const override;
		//~ End FAssetTypeActions_Blueprint interface
	};
}
