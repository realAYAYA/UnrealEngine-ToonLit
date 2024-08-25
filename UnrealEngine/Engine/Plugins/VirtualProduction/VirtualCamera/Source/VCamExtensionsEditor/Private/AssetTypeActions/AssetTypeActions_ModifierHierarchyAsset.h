// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "IVCamCoreEditorModule.h"
#include "Hierarchies/ModifierHierarchyAsset.h"

namespace UE::VCamExtensionsEditor::Private
{
	class FAssetTypeActions_ModifierHierarchyAsset : public FAssetTypeActions_Base
	{
	public:

		//~ Begin IAssetTypeActions Interface
		virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ModifierHierarchyAsset", "Modifier Hierarchy"); }
		virtual FColor GetTypeColor() const override { return FColor(201, 29, 85); }
		virtual UClass* GetSupportedClass() const override { return UModifierHierarchyAsset::StaticClass(); }
		virtual uint32 GetCategories() override { return VCamCoreEditor::IVCamCoreEditorModule::Get().GetAdvancedAssetCategoryForVCam(); }
		//~ End IAssetTypeActions Interface
	};
}


