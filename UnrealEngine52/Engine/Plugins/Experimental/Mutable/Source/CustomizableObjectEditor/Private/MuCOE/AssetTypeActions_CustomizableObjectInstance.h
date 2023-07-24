// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "MuCO/CustomizableObjectInstance.h"

class FMenuBuilder;
class IToolkitHost;
class UClass;
class UObject;


class FAssetTypeActions_CustomizableObjectInstance : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_CustomizableObjectInstance", "Customizable Object Instance"); }
	FColor GetTypeColor() const override { return FColor(255, 82, 49); }
	UClass* GetSupportedClass() const override { return UCustomizableObjectInstance::StaticClass(); }
	void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	uint32 GetCategories() override;
	
	bool AssetsActivatedOverride(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType) override;
	bool CanFilter() override { return true; }
	bool ShouldForceWorldCentric() override { return false; }
	void PerformAssetDiff(UObject* OldAsset, UObject* NewAsset, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const override {}

private:

	void ExecuteEdit(TArray<TWeakObjectPtr<UCustomizableObjectInstance>> Objects);	
};
