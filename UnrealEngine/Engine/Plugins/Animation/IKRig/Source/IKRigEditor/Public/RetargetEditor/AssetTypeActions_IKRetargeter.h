// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"
#include "EditorAnimUtils.h"

class UIKRetargeter;

class FAssetTypeActions_IKRetargeter : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_IKRetargeter", "IK Retargeter"); }
	virtual FColor GetTypeColor() const override { return FColor(0, 128, 255); }
	virtual UClass* GetSupportedClass() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Animation; }
	virtual const TArray<FText>& GetSubMenus() const override;
	virtual UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;

	static void ExtendAnimSequenceToolMenu();
	static void CreateRetargetSubMenu(FToolMenuSection& InSection);
};
