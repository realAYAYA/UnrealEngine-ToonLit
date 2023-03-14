// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/Skeleton.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"
#include "EditorAnimUtils.h"


/**
 * Remap Skeleton Asset Data
 */
struct FAssetToRemapSkeleton
{
	FName					PackageName;
	TWeakObjectPtr<UObject> Asset;
	FText					FailureReason;
	bool					bRemapFailed;

	FAssetToRemapSkeleton(FName InPackageName)
		: PackageName(InPackageName)
		, bRemapFailed(false)
	{}

	// report it failed
	void ReportFailed(const FText& InReason)
	{
		FailureReason = InReason;
		bRemapFailed = true;
	}
};

class FAssetTypeActions_Skeleton : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_Skeleton", "Skeleton"); }
	virtual FColor GetTypeColor() const override { return FColor(105,181,205); }
	virtual UClass* GetSupportedClass() const override { return USkeleton::StaticClass(); }
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Animation; }

private: // Helper functions

	void FillCreateMenu(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USkeleton>> Skeletons) const;
	bool OnAssetCreated(TArray<UObject*> NewAssets) const;
};
