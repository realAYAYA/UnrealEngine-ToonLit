// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "IVCamCoreEditorModule.h"
#include "Styling/ModifierBoundWidgetStylesAsset.h"

namespace UE::VCamExtensionsEditor::Private
{
	class FAssetTypeActions_ModifierBoundWidgetStylesAsset : public FAssetTypeActions_Base
	{
	public:

		//~ Begin IAssetTypeActions Interface
		virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "FAssetTypeActions_ModifierBoundWidgetStyleCollectionAsset", "Modifier Bound Widget Styles"); }
		virtual FColor GetTypeColor() const override { return FColor(201, 29, 85); }
		virtual UClass* GetSupportedClass() const override { return UModifierBoundWidgetStylesAsset::StaticClass(); }
		virtual uint32 GetCategories() override { return VCamCoreEditor::IVCamCoreEditorModule::Get().GetAdvancedAssetCategoryForVCam(); }
		//~ End IAssetTypeActions Interface
	};
}


