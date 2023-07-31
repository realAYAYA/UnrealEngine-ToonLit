// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"
#include "GameFramework/LightWeightInstanceManager.h"


class FAssetTypeActions_LightWeightInstance : public FAssetTypeActions_Base
{
public:
	virtual FColor GetTypeColor() const override { return FColor(255, 105, 180); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Gameplay; }

	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_LightWeightInstance", "Light Weight Instance"); }
	virtual UClass* GetSupportedClass() const override { return ALightWeightInstanceManager::StaticClass(); }
//	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
//	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
//	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;
	// End IAssetTypeActions
};

