// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"


class FAssetTypeActions_SlateWidgetStyle : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SlateStyle", "Slate Widget Style"); }
	virtual FColor GetTypeColor() const override { return FColor(62, 140, 35); }
	virtual UClass* GetSupportedClass() const override { return USlateWidgetStyleAsset::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::UI; }
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;

	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
};
