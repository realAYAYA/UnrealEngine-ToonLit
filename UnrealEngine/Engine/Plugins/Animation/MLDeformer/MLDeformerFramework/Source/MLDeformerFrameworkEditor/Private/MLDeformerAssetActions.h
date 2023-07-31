// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

namespace UE::MLDeformer
{
	/**
	 * The asset actions class, describing what actions to take when double clicking on the ML Deformer asset type, etc.
	 */
	class FMLDeformerAssetActions
		: public FAssetTypeActions_Base
	{
	public:
		// IAssetTypeActions overrides.
		virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
		virtual FText GetName() const override			{ return NSLOCTEXT("MLDeformerAssetActions", "Name", "ML Deformer"); }
		virtual FColor GetTypeColor() const override	{ return FColor(255, 255, 0); }
		virtual uint32 GetCategories() override			{ return EAssetTypeCategories::Animation; }
		virtual UClass* GetSupportedClass() const override;
		virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
		virtual const TArray<FText>& GetSubMenus() const override;
		// ~END IAssetTypeActions overrides.
	};
}	// namespace UE::MLDeformer
