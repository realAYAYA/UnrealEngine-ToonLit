// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions/AssetTypeActions_Blueprint.h"
#include "EditorAnimUtils.h"
#include "Animation/AnimBlueprint.h"

class UFactory;

class FAssetTypeActions_AnimBlueprint : public FAssetTypeActions_Blueprint
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AnimBlueprint", "Animation Blueprint"); }
	virtual FColor GetTypeColor() const override { return FColor(200,116,0); }
	virtual UClass* GetSupportedClass() const override { return UAnimBlueprint::StaticClass(); }
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Animation; }
	virtual void PerformAssetDiff(UObject* Asset1, UObject* Asset2, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const override;
	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;
	virtual TSharedPtr<SWidget> GetThumbnailOverlay(const FAssetData& AssetData) const override;
	virtual FText GetDisplayNameFromAssetData(const FAssetData& AssetData) const override;
	
	// FAssetTypeActions_Blueprint interface
	virtual UFactory* GetFactoryForBlueprintType(UBlueprint* InBlueprint) const override;

private:

	/** Handler for when FindSkeleton is selected */
	void ExecuteFindSkeleton(TArray<TWeakObjectPtr<UAnimBlueprint>> Objects);

	/** Handler for when AssignSkeleton is selected */
	void ExecuteAssignSkeleton(TArray<TWeakObjectPtr<UAnimBlueprint>> Objects);
	
	/** Certain options are not available for template anim BPs */
	bool AreOnlyNonTemplateAnimBlueprintsSelected(TArray<TWeakObjectPtr<UAnimBlueprint>> Objects) const;

	/** Certain options are not available for layer interfaces */
	bool AreOnlyNonInterfaceAnimBlueprintsSelected(TArray<TWeakObjectPtr<UAnimBlueprint>> Objects) const;
	
	/** When skeleton asset is missing, allow replacing skeleton asset */ 
	bool ReplaceMissingSkeleton(TArray<UObject*> InAnimBlueprints) const;
};
