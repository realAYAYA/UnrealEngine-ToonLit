// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DataTable.h"
#include "BoneContainer.h"
#include "CustomBoneIndexArray.h"
#include "MirrorDataTable.generated.h"

/** Type referenced by a row in the mirror data table */
UENUM()
namespace EMirrorRowType
{
	enum Type : int
	{
		Bone,
		AnimationNotify,
		Curve,
		SyncMarker,
		Custom
	};
}


/** Find and Replace Method for FMirrorFindReplaceExpression. */
UENUM()
namespace EMirrorFindReplaceMethod
{
	enum Type : int
	{
		/** Only find and replace matching strings at the start of the name  */
		Prefix,
        /** Only find and replace matching strings at the end of the name  */
        Suffix,
        /** Use regular expressions for find and replace, including support for captures $1 - $10 */
        RegularExpression
    };
}


/**  Base Mirror Table containing all data required by the animation mirroring system. */
USTRUCT()
struct FMirrorTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Mirroring)
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Mirroring)
	FName MirroredName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Mirroring)
	TEnumAsByte<EMirrorRowType::Type> MirrorEntryType;

	FMirrorTableRow()
		: Name(NAME_None)
		, MirroredName(NAME_None)
		, MirrorEntryType(EMirrorRowType::Bone) {}

	ENGINE_API FMirrorTableRow(const FMirrorTableRow& Other);
	ENGINE_API FMirrorTableRow& operator=(FMirrorTableRow const& Other);
	ENGINE_API bool operator==(FMirrorTableRow const& Other) const;
	ENGINE_API bool operator!=(FMirrorTableRow const& Other) const;
	ENGINE_API bool operator<(FMirrorTableRow const& Other) const;
};


/** Find and Replace expressions used to generate mirror tables*/
USTRUCT()
struct FMirrorFindReplaceExpression
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Mirroring)
	FName FindExpression;

	UPROPERTY(EditAnywhere, Category = Mirroring)
	FName ReplaceExpression;

	UPROPERTY(EditAnywhere, Category = Mirroring)
	TEnumAsByte<EMirrorFindReplaceMethod::Type> FindReplaceMethod;

	FMirrorFindReplaceExpression() 
		: FindExpression(NAME_None)
		, ReplaceExpression(NAME_None)
		, FindReplaceMethod(EMirrorFindReplaceMethod::Prefix) {}

	FMirrorFindReplaceExpression(FName InFindExpression, FName InReplaceExpression, EMirrorFindReplaceMethod::Type Method)
		: FindExpression(InFindExpression)
		, ReplaceExpression(InReplaceExpression)
		, FindReplaceMethod(Method)
	{
	}
};

/**
 * Data table for mirroring bones, notifies, and curves. The mirroring table allows self mirroring with entries where the name and mirrored name are identical
 */
UCLASS(MinimalAPI, BlueprintType, hideCategories = (ImportOptions, ImportSource) /* AutoExpandCategories = "MirrorDataTable,ImportOptions"*/)
class UMirrorDataTable : public UDataTable
{
	GENERATED_BODY()

	friend class UMirrorDataTableFactory;

public:
	UMirrorDataTable(const FObjectInitializer& ObjectInitializer);

	ENGINE_API virtual void PostLoad() override;

#if WITH_EDITOR
	ENGINE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR

	/**
	 * Apply the animation settings mirroring find and replace strings against the given name, returning
	 * the mirrored name or NAME_None if none of the find strings are found in the name. 
	 * 
	 * @param	InName		Name to map against animation settings mirroring find and replace 
	 * @return				The mirrored name or NAME_None
	 */
	ENGINE_API static FName GetSettingsMirrorName(FName InName); 

	/**
	 * Apply the provided find and replace strings against the given name, returning
	 * the mirrored name or NAME_None if none of the find strings are found in the name. 
	 * 
	 * @param	MirrorFindReplaceExpressions		Find and replace expressions.  The first matching expression will be returned
	 * @param	InName								Name to find and replace 
	 * @return										The mirrored name or NAME_None if none of the expressions match
	 */
	ENGINE_API static FName GetMirrorName(FName InName, const TArray<FMirrorFindReplaceExpression>& MirrorFindReplaceExpressions);

	/**
     * Create Mirror Bone Indices for the provided BoneContainer.  The CompactBonePoseMirrorBones provides an index map which can be used to mirror at runtime
	 *
	 * @param	BoneContainer					The Bone Container that the OutCompactPaseMirrorBones should match
	 * @param	MirrorBoneIndexes				Mirror bone indexes created for the ReferenceSkeleton used by the BoneContainer 
	 * @param	OutCompactPoseMirrorBones		An efficient representation of the bones to mirror which can be used at runtime
	 */
	ENGINE_API static void FillCompactPoseMirrorBones(const FBoneContainer& BoneContainer, const TCustomBoneIndexArray<FSkeletonPoseBoneIndex, FSkeletonPoseBoneIndex>& MirrorBoneIndexes, TCustomBoneIndexArray<FCompactPoseBoneIndex, FCompactPoseBoneIndex>& OutCompactPoseMirrorBones);


	/**
	 * Converts the mirror data table Name -> MirrorName map into an index map for the given ReferenceSkeleton
	 *
	 * @param	ReferenceSkeleton		The ReferenceSkeleton to compute the mirror index against
	 * @param	OutMirrorBoneIndexes	An array that provides the bone index of the mirror bone, or INDEX_NONE if the bone is not mirrored
	 */
	ENGINE_API void FillMirrorBoneIndexes(const USkeleton* Skeleton, TCustomBoneIndexArray<FSkeletonPoseBoneIndex, FSkeletonPoseBoneIndex>& OutMirrorBoneIndexes) const;

	/**
	 * Populates two arrays with a mapping of compact pose mirror bones and reference rotations
	 *
	 * @param	BoneContainer					Structure which holds the required bones
	 * @param	OutCompactPoseMirrorBones		Output array mapping compact pose mirror bones
	 * @param	OutComponentSpaceRefRotations	Output array mapping reference rotations
	 */
	ENGINE_API void FillCompactPoseAndComponentRefRotations(
		const FBoneContainer& BoneContainer,
		TCustomBoneIndexArray<FCompactPoseBoneIndex, FCompactPoseBoneIndex>& OutCompactPoseMirrorBones,
		TCustomBoneIndexArray<FQuat, FCompactPoseBoneIndex>& OutComponentSpaceRefRotations) const;

#if WITH_EDITOR  
	/**
	 * Populates the table by running the MirrorFindReplaceExpressions on bone names in the Skeleton.  If the mirrored name is also found 
	 * on the Skeleton it is added to the table.
	 */
	ENGINE_API void FindReplaceMirroredNames();
#endif // WITH_EDITOR

	/**
	 * Evaluate the MirrorFindReplaceExpressions on InName and return the replaced value of the first entry that matches
	 *
	 * @param	InName		The input string to find & replace
	 * @return				The replaced result of the first MirrorFindReplaceExpression where the find pattern matched
	 */
	ENGINE_API FName FindReplace(FName InName) const;

	/**
	 * Finds the "best matching" mirrored bone across the specified axis. Priority is given to bones with the mirrored name,
	 * falling back to spatial proximity if no mirrored bone is found using the naming rules.
	 *
	 * When falling back to proximity, bones within the SearchThreshold distance are considered coincident and a fuzzy string
	 * comparison is used to find the most likely bone that matches the input bone.
	 *
	 * NOTE: The naming scheme assumes a mirror axis of X (Left/Right). Naming rules for other axes are not supported.
	 *
	 * @param	InBoneName		The input bone for which you want to find the mirrored equivalent
	 * @param	InRefSkeleton	The reference skeleton used to find bone names and their spatial relationships (in ref pose)
	 * @param	InMirrorAxis	The axis to cross when searching for a mirrored bone
	 * @param	SearchThreshold	The distance in Unreal units to consider when trying to "tie-break" coincident bones
	 * @return					The "best match" mirrored bone
	 */
	ENGINE_API static FName FindBestMirroredBone(
		const FName InBoneName,
		const FReferenceSkeleton& InRefSkeleton,
		EAxis::Type InMirrorAxis,
		const float SearchThreshold = 2.0f);

public:

	UPROPERTY(EditAnywhere, Category = CreateTable)
	TArray<FMirrorFindReplaceExpression> MirrorFindReplaceExpressions;

	UPROPERTY(EditAnywhere, Category = Mirroring)
	TEnumAsByte<EAxis::Type> MirrorAxis;

	UPROPERTY(EditAnywhere, Category = Mirroring)
	bool  bMirrorRootMotion = true;
	
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, Category = Skeleton)
	TObjectPtr<USkeleton> Skeleton; 

	// Index of the mirror bone for a given bone index in the reference skeleton, or INDEX_NONE if the bone is not mirrored
	TCustomBoneIndexArray<FSkeletonPoseBoneIndex, FSkeletonPoseBoneIndex> BoneToMirrorBoneIndex;
	
	UE_DEPRECATED(5.3, "UID-based mirroring has been remove, please use CurveToMirrorCurveMap.")
	TArray<SmartName::UID_Type> CurveMirrorSourceUIDArray;
	
	UE_DEPRECATED(5.3, "UID-based mirroring has been remove, please use CurveToMirrorCurveMap.")
	TArray<SmartName::UID_Type> CurveMirrorTargetUIDArray;

	// Map from animation curve to mirrored animation curve
	TMap<FName, FName> CurveToMirrorCurveMap;
	
	// Map from animation notify to mirrored animation notify
	TMap<FName, FName> AnimNotifyToMirrorAnimNotifyMap;
	
	// Map from sync marker to mirrored sync marker 
	TMap<FName, FName> SyncToMirrorSyncMap;

protected: 

	// Fill BoneToMirrorBoneIndex, CurveMirrorSourceUIDArray, CurveMirrorTargetUIDArray and NotifyToMirrorNotifyIndex based on the Skeleton and Table Contents
	ENGINE_API void FillMirrorArrays();
};

