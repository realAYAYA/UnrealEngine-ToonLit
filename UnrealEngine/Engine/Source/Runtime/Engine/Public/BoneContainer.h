// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtr.h"
#include "BoneIndices.h"
#include "ReferenceSkeleton.h"
#include "Animation/AnimTypes.h"
#include "BoneContainer.generated.h"

class USkeletalMesh;
class USkeleton;
class USkeletalMesh;
struct FBoneContainer;
struct FSkeletonRemapping;
struct FBlendedCurve;

/** Struct used to store per-component ref pose override */
struct FSkelMeshRefPoseOverride
{
	/** Inverse of (component space) ref pose matrices  */
	TArray<FMatrix44f> RefBasesInvMatrix;
	/** Per bone transforms (local space) for new ref pose */
	TArray<FTransform> RefBonePoses;
};

/** in the future if we need more bools, please convert to bitfield 
 * These are not saved in asset but per skeleton. 
 */
USTRUCT()
struct ENGINE_API FAnimCurveType
{
	GENERATED_USTRUCT_BODY()

	bool bMaterial;
	bool bMorphtarget;

	FAnimCurveType(bool bInMorphtarget = false, bool bInMaterial = false)
		: bMaterial(bInMaterial)
		, bMorphtarget(bInMorphtarget)
	{
	}
};

/**
* This is a native transient structure. Used to store virtual bone mappings for compact poses
**/
struct FVirtualBoneCompactPoseData
{
	/** Index of this virtual bone */
	FCompactPoseBoneIndex VBIndex;
	/** Index of source bone */
	FCompactPoseBoneIndex SourceIndex;
	/** Index of target bone */
	FCompactPoseBoneIndex TargetIndex;

	FVirtualBoneCompactPoseData(FCompactPoseBoneIndex InVBIndex, FCompactPoseBoneIndex InSourceIndex, FCompactPoseBoneIndex InTargetIndex)
		: VBIndex(InVBIndex)
		, SourceIndex(InSourceIndex)
		, TargetIndex(InTargetIndex)
	{}
};

/**
 * This is curve evaluation options for bone container
 */
struct FCurveEvaluationOption
{
	bool bAllowCurveEvaluation;
	const TArray<FName>* DisallowedList;
	int32 LODIndex;
	
	FCurveEvaluationOption(bool bInAllowCurveEvaluation = true, const TArray<FName>* InDisallowedList = nullptr, int32 InLODIndex = 0)
		: bAllowCurveEvaluation(bInAllowCurveEvaluation)
		, DisallowedList(InDisallowedList)
		, LODIndex(InLODIndex)
	{
	}
};

/** Stores cached data for Orient and Scale bone translation retargeting */
struct FOrientAndScaleRetargetingCachedData
{
	FQuat TranslationDeltaOrient;
	float TranslationScale;
	FVector SourceTranslation;
	FVector TargetTranslation;

	FOrientAndScaleRetargetingCachedData
	(
		const FQuat& InTranslationDeltaOrient, 
		const float InTranslationScale, 
		const FVector& InSourceTranslation,
		const FVector& InTargetTranslation
	)
		: TranslationDeltaOrient(InTranslationDeltaOrient)
		, TranslationScale(InTranslationScale)
		, SourceTranslation(InSourceTranslation)
		, TargetTranslation(InTargetTranslation)
	{
	}
};

/**
 * An array of cached curve remappings. This is used to remap curves between skeletons.
 * It is used in the FBoneContainer and is generated using a lazy approach. 
 */
struct FCachedSkeletonCurveMapping
{
	TArray<SmartName::UID_Type>	UIDToArrayIndices; /** The mapping table used for mapping curves. This is indexed by UID and returns the curve index, or MAX_uint16 in case its not used. */
	bool bIsDirty = true; /** Specifies whether we need to rebuild this cached data or not. */
};

/**
 * A scoped curve remapping.
 * This object is used in a RAII pattern. It initializes the curve remapping on the provided curve.
 * When the object gets destructed it will automatically revert the remapping on the curve object.
 * 
 * Usage would be something like this:
 * \code{.cpp}
 * FSkeletonRemapping* Mapping = TargetSkeleton->GetSkeletonRemapping(SourceSkeleton);
 * if (Mapping)
 * {
 *		FSkeletonRemappingCurve Context(BlendedCurve, RequiredBones, Mapping);
 *		EvaluateCurveData(BlendedCurve);
 * }
 * \endcode
 * 
 * @param InCurve The curve object that will be used during sampling of the animation data. We will modify its UIDToArrayIndexLUT member to point to a remapped version.
 * @param InBoneContainer The bone container that we're using. This stores the remappings.
 * @param InSkeletonMapping The skeleton mapping to use. This can be requested with USkeleton::GetSkeletonRemapping(SourceSkeleton).
 */
struct FSkeletonRemappingCurve
{
public:
	FSkeletonRemappingCurve() = delete;
	FSkeletonRemappingCurve(const FSkeletonRemappingCurve&) = delete;
	FSkeletonRemappingCurve(FSkeletonRemappingCurve&&) = delete;
	FSkeletonRemappingCurve(FBlendedCurve& InCurve, FBoneContainer& InBoneContainer, const FSkeletonRemapping* InSkeletonMapping);
	FSkeletonRemappingCurve(FBlendedCurve& InCurve, FBoneContainer& InBoneContainer, const USkeleton* SourceSkeleton);
	~FSkeletonRemappingCurve();

	FSkeletonRemappingCurve& operator = (const FSkeletonRemappingCurve&) = delete;
	FSkeletonRemappingCurve& operator = (FSkeletonRemappingCurve&&) = delete;

	FBlendedCurve& GetCurve() { return Curve;  }
	const FBlendedCurve& GetCurve() const { return Curve; }

private:
	FBlendedCurve& Curve;
	FBoneContainer& BoneContainer;
	bool bIsRemapping = false;
};


/** Retargeting cached data for a specific Retarget Source */
struct FRetargetSourceCachedData
{
	/** Orient and Scale cached data. */
	TArray<FOrientAndScaleRetargetingCachedData> OrientAndScaleData;

	/** LUT from CompactPoseIndex to OrientAndScaleIndex */
	TArray<int32> CompactPoseIndexToOrientAndScaleIndex;
};

/** Iterator for compact pose indices */
struct FCompactPoseBoneIndexIterator
{
	int32 Index;

	FCompactPoseBoneIndexIterator(int32 InIndex) : Index(InIndex) {}

	FCompactPoseBoneIndexIterator& operator++() { ++Index; return (*this); }
	bool operator==(FCompactPoseBoneIndexIterator& Rhs) { return Index == Rhs.Index; }
	bool operator!=(FCompactPoseBoneIndexIterator& Rhs) { return Index != Rhs.Index; }
	FCompactPoseBoneIndex operator*() const { return FCompactPoseBoneIndex(Index); }
};

/** Reverse iterator for compact pose indices */
struct FCompactPoseBoneIndexReverseIterator
{
	int32 Index;

	FCompactPoseBoneIndexReverseIterator(int32 InIndex) : Index(InIndex) {}

	FCompactPoseBoneIndexReverseIterator& operator++() { --Index; return (*this); }
	bool operator==(FCompactPoseBoneIndexReverseIterator& Rhs) { return Index == Rhs.Index; }
	bool operator!=(FCompactPoseBoneIndexReverseIterator& Rhs) { return Index != Rhs.Index; }
	FCompactPoseBoneIndex operator*() const { return FCompactPoseBoneIndex(Index); }
};

/**
* This is a native transient structure.
* Contains:
* - BoneIndicesArray: Array of RequiredBoneIndices for Current Asset. In increasing order. Mapping to current Array of Transforms (Pose).
* - BoneSwitchArray: Size of current Skeleton. true if Bone is contained in RequiredBones array, false otherwise.
**/
struct ENGINE_API FBoneContainer
{
private:
	/** Array of RequiredBonesIndices. In increasing order. */
	TArray<FBoneIndexType>	BoneIndicesArray;
	/** Array sized by Current RefPose. true if Bone is contained in RequiredBones array, false otherwise. */
	TBitArray<>				BoneSwitchArray;

	/** Asset BoneIndicesArray was made for. Typically a SkeletalMesh. */
	TWeakObjectPtr<UObject>	Asset;
	/** If Asset is a SkeletalMesh, this will be a pointer to it. Can be NULL if Asset is a USkeleton. */
	TWeakObjectPtr<USkeletalMesh> AssetSkeletalMesh;
	/** If Asset is a Skeleton that will be it. If Asset is a SkeletalMesh, that will be its Skeleton. */
	TWeakObjectPtr<USkeleton> AssetSkeleton;

	/** Pointer to RefSkeleton of Asset. */
	const FReferenceSkeleton* RefSkeleton;

	/** Mapping table between Skeleton Bone Indices and Pose Bone Indices. */
	TArray<int32> SkeletonToPoseBoneIndexArray;

	/** Mapping table between Pose Bone Indices and Skeleton Bone Indices. */
	TArray<int32> PoseToSkeletonBoneIndexArray;

	// Look up from skeleton to compact pose format
	TArray<int32> CompactPoseToSkeletonIndex;

	// Look up from compact pose format to skeleton
	TArray<FCompactPoseBoneIndex> SkeletonToCompactPose;

	// Compact pose format of Parent Bones (to save us converting to mesh space and back)
	TArray<FCompactPoseBoneIndex> CompactPoseParentBones;

	// Array of cached virtual bone data so that animations running from raw data can generate them.
	TArray<FVirtualBoneCompactPoseData> VirtualBoneCompactPoseData;

	/** Look up table of UID to array index UIDToArrayIndexLUT[InUID] = ArrayIndex of order. If MAX_uint16, it is invalid.*/
	TArray<uint16> UIDToArrayIndexLUT;

	/** Number of valid entries in UIDToArrayIndexLUT. I.e. a count of entries whose value does not equal to  MAX_uint16. */
	int32 UIDToArrayIndexLUTValidCount;

	TSharedPtr<FSkelMeshRefPoseOverride> RefPoseOverride;
	
	// The serial number of this bone container. This is incremented each time the container is regenerated and can
	// be used to track whether to cache bone data. If this value is zero then the bone container is considered invalid
	// as it has never been regenerated.
	uint16 SerialNumber;

	/** For debugging. */
#if DO_CHECK
	/** The LOD that we calculated required bones when regenerated */
	int32 CalculatedForLOD;
#endif
	/** Disable Retargeting. Extract animation, but do not retarget it. */
	bool bDisableRetargeting;
	/** Disable animation compression, use RAW data instead. */
	bool bUseRAWData;
	/** Use Source Data that is imported that are not compressed. */
	bool bUseSourceData;
	
public:

	FBoneContainer();

	FBoneContainer(const TArray<FBoneIndexType>& InRequiredBoneIndexArray, const FCurveEvaluationOption& CurveEvalOption, UObject& InAsset);

	/** Initialize BoneContainer to a new Asset, RequiredBonesArray and RefPoseArray. */
	void InitializeTo(const TArray<FBoneIndexType>& InRequiredBoneIndexArray, const FCurveEvaluationOption& CurveEvalOption, UObject& InAsset);

	/** Returns true if FBoneContainer is Valid. Needs an Asset, a RefPoseArray, and a RequiredBonesArray. */
	const bool IsValid() const
	{
		return (Asset.IsValid() && (RefSkeleton != NULL) && (BoneIndicesArray.Num() > 0));
	}

	/** Get Asset this BoneContainer was made for. Typically a SkeletalMesh, but could also be a USkeleton. */
	UObject* GetAsset() const
	{
		return Asset.Get();
	}

	/** Get SkeletalMesh Asset this BoneContainer was made for. Could be NULL if Asset is a Skeleton. */
	USkeletalMesh* GetSkeletalMeshAsset() const
	{
		return AssetSkeletalMesh.Get();
	}

	/** Get Skeleton Asset. Could either be the SkeletalMesh's Skeleton, or the Skeleton this BoneContainer was made for. Is non NULL is BoneContainer is valid. */
	USkeleton* GetSkeletonAsset() const
	{
		return AssetSkeleton.Get();
	}

	/** Disable Retargeting for debugging. */
	void SetDisableRetargeting(bool InbDisableRetargeting)
	{
		bDisableRetargeting = InbDisableRetargeting;
	}

	/** True if retargeting is disabled for debugging. */
	bool GetDisableRetargeting() const
	{
		return bDisableRetargeting;
	}

	/** Ignore compressed data and use RAW data instead, for debugging. */
	void SetUseRAWData(bool InbUseRAWData)
	{
		bUseRAWData = InbUseRAWData;
	}

	/** True if we're requesting RAW data instead of compressed data. For debugging. */
	bool ShouldUseRawData() const
	{
		return bUseRAWData;
	}

	/** use Source data instead.*/
	void SetUseSourceData(bool InbUseSourceData)
	{
		bUseSourceData = InbUseSourceData;
	}

	/** True if we're requesting Source data instead of RawAnimationData. For debugging. */
	bool ShouldUseSourceData() const
	{
		return bUseSourceData;
	}

	/**
	 * Returns array of the size of compact pose, mapping to mesh pose index
	 * returns Required Bone Indices Array
	 */
	const TArray<FBoneIndexType>& GetBoneIndicesArray() const
	{
		return BoneIndicesArray;
	}

	/**
	* returns virutal bone cached data
	*/
	const TArray<FVirtualBoneCompactPoseData>& GetVirtualBoneCompactPoseData() const { return VirtualBoneCompactPoseData; }

	/**
	* returns Bone Switch Array. BitMask for RequiredBoneIndex array.
	*/
	const TBitArray<>& GetBoneSwitchArray() const
	{
		return BoneSwitchArray;
	}

	/** Pointer to RefPoseArray for current Asset. */
	const TArray<FTransform>& GetRefPoseArray() const
	{
		return RefSkeleton->GetRefBonePose();
	}

	const TArray<FCompactPoseBoneIndex>& GetCompactPoseParentBoneArray() const
	{
		return CompactPoseParentBones;
	}

	// Fill the supplied buffer with the compact pose reference pose
	template<typename ArrayType>
	void FillWithCompactRefPose(ArrayType& OutTransforms) const
	{
		const int32 CompactPoseBoneCount = GetCompactPoseNumBones();
		OutTransforms.Reset(CompactPoseBoneCount);
		if (RefPoseOverride.IsValid())
		{
			OutTransforms.Append(RefPoseOverride->RefBonePoses);
		}
		else
		{
			OutTransforms.SetNumUninitialized(CompactPoseBoneCount);
			const TArray<FTransform>& RefPoseTransforms = RefSkeleton->GetRefBonePose();
			for (int32 CompactBoneIndex = 0; CompactBoneIndex < CompactPoseBoneCount; ++CompactBoneIndex)
			{
				OutTransforms[CompactBoneIndex] = RefPoseTransforms[BoneIndicesArray[CompactBoneIndex]];
			}
		}
	}
	
	const FTransform& GetRefPoseTransform(const FCompactPoseBoneIndex& BoneIndex) const
	{
		if (RefPoseOverride.IsValid())
		{
			return RefPoseOverride->RefBonePoses[BoneIndex.GetInt()];
		}
		else
		{
			return RefSkeleton->GetRefBonePose()[BoneIndicesArray[BoneIndex.GetInt()]];
		}
	}

	/** Override skeleton ref pose. */
	void SetRefPoseOverride(const TSharedPtr<FSkelMeshRefPoseOverride>& InRefPoseOverride)
	{
		if (InRefPoseOverride.Get() != RefPoseOverride.Get())
		{
			RefPoseOverride = InRefPoseOverride;
		}
	}

	/** Access to Asset's RefSkeleton. */
	const FReferenceSkeleton& GetReferenceSkeleton() const
	{
		return *RefSkeleton;
	}

	/** Number of Bones in RefPose for current asset. This is NOT the number of bones in RequiredBonesArray, but the TOTAL number of bones in the RefPose of the current Asset! */
	const int32 GetNumBones() const
	{
		return RefSkeleton->GetNum();
	}

	const int32 GetCompactPoseNumBones() const
	{
		return BoneIndicesArray.Num();
	}

	/** Get BoneIndex for BoneName for current Asset. */
	int32 GetPoseBoneIndexForBoneName(const FName& BoneName) const;

	/** Get ParentBoneIndex for current Asset. */
	int32 GetParentBoneIndex(const int32 BoneIndex) const;

	/** Get ParentBoneIndex for current Asset. */
	FCompactPoseBoneIndex GetParentBoneIndex(const FCompactPoseBoneIndex& BoneIndex) const;

	/** Get Depth between bones for current asset. */
	int32 GetDepthBetweenBones(const int32 BoneIndex, const int32 ParentBoneIndex) const;

	/** Returns true if bone is child of for current asset. */
	bool BoneIsChildOf(const int32 BoneIndex, const int32 ParentBoneIndex) const;

	/** Returns true if bone is child of for current asset. */
	bool BoneIsChildOf(const FCompactPoseBoneIndex& BoneIndex, const FCompactPoseBoneIndex& ParentBoneIndex) const;

	/** Get UID To Array look up table */
	TArray<uint16> const& GetUIDToArrayLookupTable() const
	{
		return UIDToArrayIndexLUT;
	}

	/** Returns the number of valid entries in the GetUIDToArrayLookupTable result array */
	int32 GetUIDToArrayIndexLookupTableValidCount() const
	{
		return UIDToArrayIndexLUTValidCount;
	}

	/** DEPRECATED: Get UID To Name look up table */
	UE_DEPRECATED(5.0, "GetUIDToNameLookupTable is deprecated, please access from the SmartNameMapping directly via GetSkeletonAsset()->GetSmartNameContainer(USkeleton::AnimCurveMappingName)")
	TArray<FName> const& GetUIDToNameLookupTable() const
	{
		static TArray<FName> Dummy;
		return Dummy;
	}

	/** DEPRECATED: Get UID To curve type look up table */
	UE_DEPRECATED(5.0, "GetUIDToCurveTypeLookupTable is deprecated, please access from the SmartNameMapping directly via GetSkeletonAsset()->GetSmartNameContainer(USkeleton::AnimCurveMappingName)")
	TArray<FAnimCurveType> const& GetUIDToCurveTypeLookupTable() const
	{
		static TArray<FAnimCurveType> Dummy;
		return Dummy;
	}

	
	/** Get the array that maps UIDs to array indexes. */
	TArray<SmartName::UID_Type> const& GetUIDToArrayLookupTableBackup() const
	{
		return UIDToArrayIndexLUTBackup;
	}

	/**
	* Serializes the bones
	*
	* @param Ar - The archive to serialize into.
	* @param Rect - The bone container to serialize.
	*
	* @return Reference to the Archive after serialization.
	*/
	friend FArchive& operator<<(FArchive& Ar, FBoneContainer& B)
	{
		Ar
			<< B.BoneIndicesArray
			<< B.BoneSwitchArray
			<< B.Asset
			<< B.AssetSkeletalMesh
			<< B.AssetSkeleton
			<< B.SkeletonToPoseBoneIndexArray
			<< B.PoseToSkeletonBoneIndexArray
			<< B.bDisableRetargeting
			<< B.bUseRAWData
			<< B.bUseSourceData
			;

		return Ar;
	}

	/**
	* Returns true of RequiredBonesArray contains this bone index.
	*/
	bool Contains(FBoneIndexType NewIndex) const
	{
		return BoneSwitchArray[NewIndex];
	}

	/** Const accessor to SkeletonToPoseBoneIndexArray. */
	UE_DEPRECATED(5.0, "Please use GetMeshPoseIndexFromSkeletonPoseIndex")
	TArray<int32> const & GetSkeletonToPoseBoneIndexArray() const
	{
		return SkeletonToPoseBoneIndexArray;
	}

	/** Const accessor to PoseToSkeletonBoneIndexArray. */
	UE_DEPRECATED(5.0, "Please use GetSkeletonPoseIndexFromMeshPoseIndex")
	TArray<int32> const & GetPoseToSkeletonBoneIndexArray() const
	{
		return PoseToSkeletonBoneIndexArray;
	}
	
	template<typename IterType>
	struct FRangedForSupport
	{
		const FBoneContainer& BoneContainer;

		FRangedForSupport(const FBoneContainer& InBoneContainer) : BoneContainer(InBoneContainer) {};
		
		IterType begin() { return BoneContainer.MakeBeginIter(); }
		IterType end() { return BoneContainer.MakeEndIter(); }
	};

	template<typename IterType>
	struct FRangedForReverseSupport
	{
		const FBoneContainer& BoneContainer;

		FRangedForReverseSupport(const FBoneContainer& InBoneContainer) : BoneContainer(InBoneContainer) {};

		IterType begin() { return BoneContainer.MakeBeginIterReverse(); }
		IterType end() { return BoneContainer.MakeEndIterReverse(); }
	};
	
	FORCEINLINE FRangedForSupport<FCompactPoseBoneIndexIterator> ForEachCompactPoseBoneIndex() const
	{
		return FRangedForSupport<FCompactPoseBoneIndexIterator>(*this);
	}
	FORCEINLINE FRangedForReverseSupport<FCompactPoseBoneIndexReverseIterator> ForEachCompactPoseBoneIndexReverse() const
	{
		return FRangedForReverseSupport<FCompactPoseBoneIndexReverseIterator>(*this);
	}
	FORCEINLINE FCompactPoseBoneIndexIterator MakeBeginIter() const
	{
		return FCompactPoseBoneIndexIterator(0);
	}
	FORCEINLINE FCompactPoseBoneIndexIterator MakeEndIter() const
	{
		return FCompactPoseBoneIndexIterator(GetCompactPoseNumBones());
	}
	FORCEINLINE FCompactPoseBoneIndexReverseIterator MakeBeginIterReverse() const
	{
		return FCompactPoseBoneIndexReverseIterator(GetCompactPoseNumBones() - 1);
	}
	FORCEINLINE FCompactPoseBoneIndexReverseIterator MakeEndIterReverse() const
	{
		return FCompactPoseBoneIndexReverseIterator(-1);
	}
	
	/** 
	 * Map skeleton bone index to mesh index
	 * @return	the mesh pose bone index for the specified skeleton pose bone index. Returns an invalid index if the mesh
	 *			does not include the specified skeleton bone.
	 */
	FMeshPoseBoneIndex GetMeshPoseIndexFromSkeletonPoseIndex(const FSkeletonPoseBoneIndex& SkeletonIndex) const
	{
		if (SkeletonToPoseBoneIndexArray.IsValidIndex(SkeletonIndex.GetInt()))
		{
			return FMeshPoseBoneIndex(SkeletonToPoseBoneIndexArray[SkeletonIndex.GetInt()]);
		}
		
		return FMeshPoseBoneIndex(INDEX_NONE);
	}

	/**
	 * Map mesh bone index to skeleton index
	 * @return	the skeleton pose bone index for the specified mesh pose bone index. Ensures and returns an invalid
	 *			index if the skeleton does not include the specified mesh bone.
	 */
	FSkeletonPoseBoneIndex GetSkeletonPoseIndexFromMeshPoseIndex(const FMeshPoseBoneIndex& MeshIndex) const
	{
		if (ensure(PoseToSkeletonBoneIndexArray.IsValidIndex(MeshIndex.GetInt())))
		{
			return FSkeletonPoseBoneIndex(PoseToSkeletonBoneIndexArray[MeshIndex.GetInt()]);
		}
		
		return FSkeletonPoseBoneIndex(INDEX_NONE);
	}

	// DEPRECATED - Ideally should use GetSkeletonPoseIndexFromCompactPoseIndex due to raw int32 here
	int32 GetSkeletonIndex(const FCompactPoseBoneIndex& BoneIndex) const
	{
		return CompactPoseToSkeletonIndex[BoneIndex.GetInt()];
	}

	/**
	 * Map compact bone index to skeleton index
	 * @return	the skeleton pose bone index for the specified compact pose bone index. Ensures and returns an invalid
	 *			index if the skeleton does not include the specified compact pose bone.
	 */	
	FSkeletonPoseBoneIndex GetSkeletonPoseIndexFromCompactPoseIndex(const FCompactPoseBoneIndex& BoneIndex) const
	{
		if (ensure(CompactPoseToSkeletonIndex.IsValidIndex(BoneIndex.GetInt())))
		{
			return FSkeletonPoseBoneIndex(CompactPoseToSkeletonIndex[BoneIndex.GetInt()]);
		}
		
		return FSkeletonPoseBoneIndex(INDEX_NONE);
	}

	// DEPRECATED - Ideally should use GetCompactPoseIndexFromSkeletonPoseIndex due to raw int32 here
	FCompactPoseBoneIndex GetCompactPoseIndexFromSkeletonIndex(const int32 SkeletonIndex) const
	{
		if (ensure(SkeletonToCompactPose.IsValidIndex(SkeletonIndex)))
		{
			return SkeletonToCompactPose[SkeletonIndex];
		}

		return FCompactPoseBoneIndex(INDEX_NONE);
	}

	/** 
	 * Map skeleton bone index to compact pose index
	 * @return	the compact pose bone index for the specified skeleton pose bone index. Returns an invalid index if the
	 *			compact pose does not include the specified skeleton bone.
	 */	
	FCompactPoseBoneIndex GetCompactPoseIndexFromSkeletonPoseIndex(const FSkeletonPoseBoneIndex& SkeletonIndex) const
	{
		if (SkeletonToCompactPose.IsValidIndex(SkeletonIndex.GetInt()))
		{
			return SkeletonToCompactPose[SkeletonIndex.GetInt()];
		}

		return FCompactPoseBoneIndex(INDEX_NONE);
	}

	FMeshPoseBoneIndex MakeMeshPoseIndex(const FCompactPoseBoneIndex& BoneIndex) const
	{
		return FMeshPoseBoneIndex(GetBoneIndicesArray()[BoneIndex.GetInt()]);
	}

	FCompactPoseBoneIndex MakeCompactPoseIndex(const FMeshPoseBoneIndex& BoneIndex) const
	{
		return FCompactPoseBoneIndex(GetBoneIndicesArray().IndexOfByKey(BoneIndex.GetInt()));
	}

	/** Cache required Anim Curve Uids */
	void CacheRequiredAnimCurveUids(const FCurveEvaluationOption& CurveEvalOption);

	const FRetargetSourceCachedData& GetRetargetSourceCachedData(const FName& InRetargetSource) const;
	const FRetargetSourceCachedData& GetRetargetSourceCachedData(const FName& InSourceName, const TArray<FTransform>& InRetargetTransforms) const;

#if DO_CHECK
	/** Get the LOD that we calculated required bones when regenerated */
	int32 GetCalculatedForLOD() const { return CalculatedForLOD; }
#endif
	
	// Curve remapping
	const FCachedSkeletonCurveMapping& GetOrCreateCachedCurveMapping(const FSkeletonRemapping* SkeletonRemapping);
	void MarkAllCachedCurveMappingsDirty();

	// Get the serial number of this bone container. @see SerialNumber
	uint16 GetSerialNumber() const { return SerialNumber; }
	
private:
	/** The map of cached curve mapping indexes, which is used for skeleton remapping. The key of this table is the source skeleton of the asset we are sampling curve values from. */
	mutable TMap<USkeleton*, FCachedSkeletonCurveMapping> CachedCurveMappingTable;

	/** The backup of the original UIDToArrayIndexLUT array. This is needed for skeleton remapping curves, as we have to modify this array and later need to restore it again. */
	TArray<SmartName::UID_Type> UIDToArrayIndexLUTBackup;

	/** 
	 * Runtime cached data for retargeting from a specific RetargetSource to this current SkelMesh LOD.
	 * @todo: We could also cache this once per skelmesh per lod, rather than creating it at runtime for each skelmesh instance.
	 */
	mutable TMap<FName, FRetargetSourceCachedData> RetargetSourceCachedDataLUT;

	/** Initialize FBoneContainer. */
	void Initialize(const FCurveEvaluationOption& CurveEvalOption);

	/** Cache remapping data if current Asset is a SkeletalMesh, with all compatible Skeletons. */
	void RemapFromSkelMesh(USkeletalMesh const & SourceSkeletalMesh, USkeleton& TargetSkeleton);

	/** Cache remapping data if current Asset is a Skeleton, with all compatible Skeletons. */
	void RemapFromSkeleton(USkeleton const & SourceSkeleton);

	// Regenerate the serial number after internal data is updated
	void RegenerateSerialNumber();
};


USTRUCT()
struct FBoneReference
{
	GENERATED_USTRUCT_BODY()

	/** Name of bone to control. This is the main bone chain to modify from. **/
	UPROPERTY(EditAnywhere, Category = BoneReference)
	FName BoneName;

	/** Cached bone index for run time - right now bone index of skeleton **/
	int32 BoneIndex:31;

	/** Change this to Bitfield if we have more than one bool 
	 * This specifies whether or not this indices is mesh or skeleton
	 */
	uint32 bUseSkeletonIndex:1;

	FCompactPoseBoneIndex CachedCompactPoseIndex;

	FBoneReference()
		: BoneName(NAME_None)
		, BoneIndex(INDEX_NONE)
		, bUseSkeletonIndex(false)
		, CachedCompactPoseIndex(INDEX_NONE)
	{
	}

	FBoneReference(const FName& InBoneName)
		: BoneName(InBoneName)
		, BoneIndex(INDEX_NONE)
		, bUseSkeletonIndex(false)
		, CachedCompactPoseIndex(INDEX_NONE)
	{
	}

	bool operator==(const FBoneReference& Other) const
	{
		return BoneName == Other.BoneName;
	}

	bool operator!=(const FBoneReference& Other) const
	{
		return BoneName != Other.BoneName;
	}

	/** Initialize Bone Reference, return TRUE if success, otherwise, return false **/
	ENGINE_API bool Initialize(const FBoneContainer& RequiredBones);

	// only used by blendspace 'PerBoneBlend'. This is skeleton indices since the input is skeleton
	// @note, if you use this function, it won't work with GetCompactPoseIndex, GetMeshPoseIndex;
	// it triggers ensure in those functions
	ENGINE_API bool Initialize(const USkeleton* Skeleton);


	/** return true if it has valid set up */
	bool HasValidSetup() const
	{
		return (BoneIndex != INDEX_NONE);
	}

	/** return true if has valid index, and required bones contain it **/
	ENGINE_API bool IsValidToEvaluate(const FBoneContainer& RequiredBones) const;
	/** return true if has valid compact index. This will return invalid if you're using skeleton index */
	ENGINE_API bool IsValidToEvaluate() const
	{
		return (!bUseSkeletonIndex && CachedCompactPoseIndex != INDEX_NONE);
	}

	void InvalidateCachedBoneIndex()
	{
		BoneIndex = INDEX_NONE;
		CachedCompactPoseIndex = FCompactPoseBoneIndex(INDEX_NONE);
	}

	FSkeletonPoseBoneIndex GetSkeletonPoseIndex(const FBoneContainer& RequiredBones) const
	{ 
		// accessing array with invalid index would cause crash, so we have to check here
		if (BoneIndex != INDEX_NONE)
		{
			if (bUseSkeletonIndex)
			{
				return FSkeletonPoseBoneIndex(BoneIndex);
			}
			else
			{
				return RequiredBones.GetSkeletonPoseIndexFromMeshPoseIndex(FMeshPoseBoneIndex(BoneIndex));
			}
		}

		return FSkeletonPoseBoneIndex(INDEX_NONE);
	}
	
	FMeshPoseBoneIndex GetMeshPoseIndex(const FBoneContainer& RequiredBones) const
	{ 
		// accessing array with invalid index would cause crash, so we have to check here
		if (BoneIndex != INDEX_NONE)
		{
			if (bUseSkeletonIndex)
			{
				return RequiredBones.GetMeshPoseIndexFromSkeletonPoseIndex(FSkeletonPoseBoneIndex(BoneIndex));
			}
			else
			{
				return FMeshPoseBoneIndex(BoneIndex);
			}
		}

		return FMeshPoseBoneIndex(INDEX_NONE);
	}

	FCompactPoseBoneIndex GetCompactPoseIndex(const FBoneContainer& RequiredBones) const 
	{ 
		if (bUseSkeletonIndex)
		{
			//If we were initialized with a skeleton we wont have a cached index.
			if (BoneIndex != INDEX_NONE)
			{
				// accessing array with invalid index would cause crash, so we have to check here
				return RequiredBones.GetCompactPoseIndexFromSkeletonPoseIndex(FSkeletonPoseBoneIndex(BoneIndex));
			}
			return FCompactPoseBoneIndex(INDEX_NONE);
		}
		
		return CachedCompactPoseIndex;
	}

	// need this because of BoneReference being used in CurveMetaData and that is in SmartName
	friend FArchive& operator<<(FArchive& Ar, FBoneReference& B)
	{
		Ar << B.BoneName;
		return Ar;
	}

	bool Serialize(FArchive& Ar)
	{
		Ar << *this;
		return true;
	}
};
