// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorAnimUtils.h"

class UIKRetargeter;

//** Data needed to run a batch "duplicate and retarget" operation on a set of animation assets */
struct FIKRetargetBatchOperationContext
{
	
public:
	
	/** The source assets to duplicate and retarget */
	TArray<TWeakObjectPtr<UObject>> AssetsToRetarget;

	/** Source mesh to use to copy animation FROM. */
	USkeletalMesh* SourceMesh = nullptr;

	/** Target mesh to use to copy animation TO. */
	USkeletalMesh* TargetMesh = nullptr;

	/** The retargeter used to copy animation */
	UIKRetargeter* IKRetargetAsset = nullptr;

	/** Whether we are remapping assets that are referenced by the assets the user selects to remap */
	bool bRemapReferencedAssets = true;

	/* Rename rules for duplicated assets */
	EditorAnimUtils::FNameDuplicationRule NameRule;

	/* Reset all data (called when window re-opened */
	void Reset()
	{
		SourceMesh = nullptr;
		TargetMesh = nullptr;
		IKRetargetAsset = nullptr;
		bRemapReferencedAssets = true;
		NameRule.Prefix = "";
		NameRule.Suffix = "";
		NameRule.ReplaceFrom = "";
		NameRule.ReplaceTo = "";
	}

	/* Is the data configured in such a way that we could run the retarget? */
	bool IsValid() const
	{
		// todo validate compatibility
		return SourceMesh && TargetMesh && IKRetargetAsset && (SourceMesh != TargetMesh);
	}
};

//** Encapsulate ability to batch duplicate and retarget a set of animation assets */
struct FIKRetargetBatchOperation
{
	
public:

	/* Actually run the process to duplicate and retarget the assets for the given context */
	void RunRetarget(FIKRetargetBatchOperationContext& Context);

private:

	/**
	* Initialize set of referenced assets to retarget.
	* @return	Number of assets that need retargeting.
	*/
	int32 GenerateAssetLists(const FIKRetargetBatchOperationContext& Context);

	/* Duplicate all the assets to retarget */
	void DuplicateRetargetAssets(const FIKRetargetBatchOperationContext& Context, FScopedSlowTask& Progress);

	/* Retarget skeleton and animation on all the duplicates */
	void RetargetAssets(const FIKRetargetBatchOperationContext& Context, FScopedSlowTask& Progress);

	/* Convert animation on all the duplicates */
	void ConvertAnimation(const FIKRetargetBatchOperationContext& Context, FScopedSlowTask& Progress);

	/* Output notifications of results */
	void NotifyUserOfResults(const FIKRetargetBatchOperationContext& Context, FScopedSlowTask& Progress) const;

	/* Generate list of newly created assets to report to user */
	void GetNewAssets(TArray<UObject*>& NewAssets) const;

	/**
	* Duplicates the supplied AssetsToDuplicate and returns a map of original asset to duplicate. Templated wrapper that calls DuplicateAssetInternal.
	*
	* @param	AssetsToDuplicate	The animations to duplicate
	* @param	DestinationPackage	The package that the duplicates should be placed in
	*
	* @return	TMap of original animation to duplicate
	*/
	template<class AssetType>
	static TMap<AssetType*, AssetType*> DuplicateAssets(
		const TArray<AssetType*>& AssetsToDuplicate,
		UPackage* DestinationPackage,
		const EditorAnimUtils::FNameDuplicationRule* NameRule);
	
	/** Lists of assets to retarget. Populated from selection during init */
	TArray<UAnimationAsset*>	AnimationAssetsToRetarget;
	TArray<UAnimBlueprint*>		AnimBlueprintsToRetarget;

	/** Lists of original assets map to duplicate assets */
	TMap<UAnimationAsset*, UAnimationAsset*>	DuplicatedAnimAssets;
	TMap<UAnimBlueprint*, UAnimBlueprint*>		DuplicatedBlueprints;

	TMap<UAnimationAsset*, UAnimationAsset*>	RemappedAnimAssets;

	/** If we only chose one object to retarget store it here */
	UObject* SingleTargetObject = nullptr;
};
