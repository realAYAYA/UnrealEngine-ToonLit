// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"

struct FAssetData;

namespace UE::MVVM
{
	
class FAssetTypeActions_ViewModelBlueprint : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ViewModelBlueprint", "Viewmodel Blueprint"); }
	virtual FColor GetTypeColor() const override { return FColor(44, 180, 180); }
	virtual UClass* GetSupportedClass() const override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::UI; }
	virtual bool CanLocalize() const override { return false; }
	virtual void PerformAssetDiff(UObject* Asset1, UObject* Asset2, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const override;
	virtual FText GetAssetDescription( const FAssetData& AssetData ) const override;
	// End IAssetTypeActions Implementation
};

} //namespace
