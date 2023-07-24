// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Class of pose asset that can evaluate pose by weights
 *
 */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/SmartName.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimCurveTypes.h"
#include "PoseAsset.generated.h"

class UAnimSequence;
class USkeletalMeshComponent;
struct FLiveLinkCurveElement;

/** 
 * Pose data 
 * 
 * This is one pose data structure
 * This will let us blend poses quickly easily
 * All poses within this asset should contain same number of tracks, 
 * so that we can blend quickly
 */

USTRUCT()
struct ENGINE_API FPoseData
{
	GENERATED_USTRUCT_BODY()

#if WITH_EDITORONLY_DATA
	// source local space pose, this pose is always full pose
	// the size this array matches Tracks in the pose container
	UPROPERTY()
	TArray<FTransform>		SourceLocalSpacePose;

	// source curve data that is full value
	UPROPERTY()
	TArray<float>			SourceCurveData;
#endif // WITH_EDITORONLY_DATA

	// local space pose, # of array match with # of TrackToBufferIndex
	// it only saves the one with delta as base pose or ref pose if full pose
	UPROPERTY()
	TArray<FTransform>		LocalSpacePose;

	// # of array match with # of Curves in PoseDataContainer
	// curve data is not compressed
 	UPROPERTY()
 	TArray<float>			CurveData;
};

USTRUCT()
struct FPoseAssetInfluence
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	int32 PoseIndex = INDEX_NONE;

	UPROPERTY()
	int32 BoneTransformIndex = INDEX_NONE;
};

USTRUCT()
struct FPoseAssetInfluences
{
	GENERATED_USTRUCT_BODY()

    UPROPERTY()
	TArray<FPoseAssetInfluence> Influences;
};

/**
* Pose data container
* 
* Contains animation and curve for all poses
*/
USTRUCT()
struct ENGINE_API FPoseDataContainer
{
	GENERATED_USTRUCT_BODY()

private:
	// pose names - horizontal data
	UPROPERTY()
	TArray<FSmartName>						PoseNames;

	// this is list of tracks - vertical data
	UPROPERTY()
	TArray<FName>							Tracks;

	// cache containting the skeleton indices for FName in Tracks array
	UPROPERTY(transient)
	TArray<int32>						TrackBoneIndices;

	UPROPERTY()
	TArray<FPoseAssetInfluences>		TrackPoseInfluenceIndices;
	
	// this is list of poses
	UPROPERTY()
	TArray<FPoseData>						Poses;
	
	
	// curve meta data # of Curve UIDs should match with Poses.CurveValues.Num
	UPROPERTY()
	TArray<FAnimCurveBase>					Curves;

	void Reset();

	FPoseData* FindPoseData(FSmartName PoseName);
	FPoseData* FindOrAddPoseData(FSmartName PoseName);

	int32 GetNumPoses() const { return Poses.Num();  }
	bool Contains(FSmartName PoseName) const { return PoseNames.Contains(PoseName); }

	bool IsValid() const { return PoseNames.Num() == Poses.Num() && Tracks.Num() == TrackBoneIndices.Num(); }
	void GetPoseCurve(const FPoseData* PoseData, FBlendedCurve& OutCurve) const;
	void BlendPoseCurve(const FPoseData* PoseData, FBlendedCurve& OutCurve, float Weight) const;

	// we have to delete tracks if skeleton has modified
	// usually this may not be issue since once cooked, it should match
	void DeleteTrack(int32 TrackIndex);
	
	// get default transform - it considers for retarget source if exists
	FTransform GetDefaultTransform(const FName& InTrackName, USkeleton* InSkeleton, const TArray<FTransform>& RefPose) const;
	FTransform GetDefaultTransform(int32 SkeletonIndex, const TArray<FTransform>& RefPose) const;

#if WITH_EDITOR
	void AddOrUpdatePose(const FSmartName& InPoseName, const TArray<FTransform>& InlocalSpacePose, const TArray<float>& InCurveData);
	void RenamePose(FSmartName OldPoseName, FSmartName NewPoseName);
	int32 DeletePose(FSmartName PoseName);
	bool DeleteCurve(FSmartName CurveName);
	bool InsertTrack(const FName& InTrackName, USkeleton* InSkeleton, const TArray<FTransform>& RefPose);
	
	bool FillUpSkeletonPose(FPoseData* PoseData, const USkeleton* InSkeleton);
	void RetrieveSourcePoseFromExistingPose(bool bAdditive, int32 InBasePoseIndex, const TArray<FTransform>& InBasePose, const TArray<float>& InBaseCurve);

	// editor features for full pose <-> additive pose
	void ConvertToFullPose(USkeleton* InSkeleton, const TArray<FTransform>& RefPose);
	void ConvertToAdditivePose(const TArray<FTransform>& InBasePose, const TArray<float>& InBaseCurve);
#endif // WITH_EDITOR
	friend class UPoseAsset;
};

/**
 * Pose Asset that can be blended by weight of curves 
 */
UCLASS(MinimalAPI, BlueprintType)
class UPoseAsset : public UAnimationAsset
{
	GENERATED_UCLASS_BODY()

private:
	/** Animation Pose Data*/
	UPROPERTY()
	struct FPoseDataContainer PoseContainer;

	/** Whether or not Additive Pose or not - these are property that needs post process, so */
	UPROPERTY()
	bool bAdditivePose;

	/** if -1, use ref pose */
	UPROPERTY()
	int32 BasePoseIndex;

public: 
	/** Base pose to use when retargeting */
	UPROPERTY(Category=Animation, EditAnywhere)
	FName RetargetSource;

#if WITH_EDITORONLY_DATA
	/** If RetargetSource is set to Default (None), this is asset for the base pose to use when retargeting. Transform data will be saved in RetargetSourceAssetReferencePose. */
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, Category=Animation, meta = (DisallowedClasses = "/Script/ApexDestruction.DestructibleMesh"))
	TSoftObjectPtr<USkeletalMesh> RetargetSourceAsset;
#endif

	/** When using RetargetSourceAsset, use the post stored here */
	UPROPERTY()
	TArray<FTransform> RetargetSourceAssetReferencePose;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Category=Source, EditAnywhere)
	TObjectPtr<UAnimSequence> SourceAnimation;

	/** GUID cached when the contained poses were last updated according to SourceAnimation - used to keep track of out-of-date/sync data*/ 
	UPROPERTY()	
	FGuid SourceAnimationRawDataGUID;
#endif // WITH_EDITORONLY_DATA

	/**
	* Get Animation Pose from one pose of PoseIndex and with PoseWeight
	* This returns OutPose and OutCurve of one pose of PoseIndex with PoseWeight
	*
	* @param	OutPose				Pose object to fill
	* @param	InOutCurve			Curves to fill
	* @param	PoseIndex			Index of Pose
	* @param	PoseWeight			Weight of pose
	*/
	UE_DEPRECATED(4.26, "Use GetAnimationPose with other signature")
	ENGINE_API bool GetAnimationPose(struct FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const;
	ENGINE_API bool GetAnimationPose(struct FAnimationPoseData& OutAnimationPoseData, const FAnimExtractContext& ExtractionContext) const;

	UE_DEPRECATED(4.26, "Use GetBaseAnimationPose with other signature")
	ENGINE_API void GetBaseAnimationPose(struct FCompactPose& OutPose, FBlendedCurve& OutCurve) const;
	ENGINE_API void GetBaseAnimationPose(struct FAnimationPoseData& OutAnimationPoseData) const;

	virtual bool HasRootMotion() const { return false; }
	virtual bool IsValidAdditive() const { return bAdditivePose; }

	// this is utility function that just cares by names to be used by live link
	// this isn't fast. Use it at your caution
	ENGINE_API void GetAnimationCurveOnly(TArray<FName>& InCurveNames, TArray<float>& InCurveValues, TArray<FName>& OutCurveNames, TArray<float>& OutCurveValues) const;

	//Begin UObject Interface
	virtual void PostLoad() override;
	virtual bool IsPostLoadThreadSafe() const override;
	virtual void Serialize(FArchive& Ar) override;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function
	UE_DEPRECATED(5.0, "Use version that takes FObjectPreSaveContext instead.")
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	//End UObject Interface

public:
	ENGINE_API int32 GetNumPoses() const;
	ENGINE_API int32 GetNumCurves() const;
	ENGINE_API int32 GetNumTracks() const;
	ENGINE_API const TArray<FSmartName> GetPoseNames() const;
	ENGINE_API const TArray<FName>		GetTrackNames() const;
	ENGINE_API const TArray<FSmartName> GetCurveNames() const;
	ENGINE_API const TArray<FAnimCurveBase> GetCurveData() const;
	ENGINE_API const TArray<float> GetCurveValues(const int32 PoseIndex) const;

	/** Find index of a track with a given bone name. Returns INDEX_NONE if not found. */
	ENGINE_API const int32 GetTrackIndexByName(const FName& InTrackName) const;

	/** 
	 *	Return value of a curve for a particular pose 
	 *	@return	Returns true if OutValue is valid, false if not
	 */
	ENGINE_API bool GetCurveValue(const int32 PoseIndex, const int32 CurveIndex, float& OutValue) const;
	 
	ENGINE_API bool ContainsPose(const FSmartName& InPoseName) const { return PoseContainer.Contains(InPoseName); }
	ENGINE_API bool ContainsPose(const FName& InPoseName) const;

#if WITH_EDITOR
	/** Renames a specific pose */
	UFUNCTION(BlueprintCallable, Category=PoseAsset)
	void RenamePose(const FName& OriginalPoseName, const FName& NewPoseName);
	
	/** Returns the name of all contained poses */
	UFUNCTION(BlueprintPure, Category=PoseAsset)
	void GetPoseNames(TArray<FName>& PoseNames) const;
	
	ENGINE_API bool AddOrUpdatePoseWithUniqueName(const USkeletalMeshComponent* MeshComponent, FSmartName* OutPoseName = nullptr);
	ENGINE_API void AddOrUpdatePose(const FSmartName& PoseName, const USkeletalMeshComponent* MeshComponent, bool bUpdateCurves = true);
	ENGINE_API void AddReferencePose(const FSmartName& PoseName, const FReferenceSkeleton& ReferenceSkeleton);
	ENGINE_API void CreatePoseFromAnimation(class UAnimSequence* AnimSequence, const TArray<FSmartName>* InPoseNames = nullptr);

	/** Contained poses are re-generated from the provided Animation Sequence*/
	UFUNCTION(BlueprintCallable, Category=PoseAsset)
	ENGINE_API void UpdatePoseFromAnimation(class UAnimSequence* AnimSequence);

	// Begin AnimationAsset interface
	virtual bool GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets, bool bRecursive = true) override;
	virtual void ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& ReplacementMap) override;
	// End AnimationAsset interface

	ENGINE_API bool ModifyPoseName(FName OldPoseName, FName NewPoseName, const SmartName::UID_Type* NewUID);

	// Rename the smart names used by this Pose Asset
	ENGINE_API void RenameSmartName(const FName& InOriginalName, const FName& InNewName);

	// Remove poses or curves using the smart names supplied
	ENGINE_API void RemoveSmartNames(const TArray<FName>& InNamesToRemove);

	// editor interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// Return full (local space, non additive) pose. Will do conversion if PoseAsset is Additive. 
	ENGINE_API bool GetFullPose(int32 PoseIndex, TArray<FTransform>& OutTransforms) const;
	
	// util to return transform of a bone from the pose asset in component space, by walking up tracks in pose asset */
	ENGINE_API FTransform GetComponentSpaceTransform(FName BoneName, const TArray<FTransform>& LocalTransforms) const;

	ENGINE_API int32 DeletePoses(TArray<FName> PoseNamesToDelete);
	ENGINE_API int32 DeleteCurves(TArray<FName> CurveNamesToDelete);
	ENGINE_API bool ConvertSpace(bool bNewAdditivePose, int32 NewBasePoseInde);
	ENGINE_API const FName GetPoseNameByIndex(int32 InBasePoseIndex) const { return PoseContainer.PoseNames.IsValidIndex(InBasePoseIndex) ? PoseContainer.PoseNames[InBasePoseIndex].DisplayName : NAME_None; }
#endif // WITH_EDITOR

	ENGINE_API int32 GetBasePoseIndex() const { return BasePoseIndex;  }
	ENGINE_API const int32 GetPoseIndexByName(const FName& InBasePoseName) const;
	ENGINE_API const int32 GetCurveIndexByName(const FName& InCurveName) const;

#if WITH_EDITOR
private: 
	DECLARE_MULTICAST_DELEGATE(FOnPoseListChangedMulticaster)
	FOnPoseListChangedMulticaster OnPoseListChanged;

public:
	typedef FOnPoseListChangedMulticaster::FDelegate FOnPoseListChanged;

	/** Registers a delegate to be called after the preview animation has been changed */
	FDelegateHandle RegisterOnPoseListChanged(const FOnPoseListChanged& Delegate)
	{
		return OnPoseListChanged.Add(Delegate);
	}
	/** Unregisters a delegate to be called after the preview animation has been changed */
	void UnregisterOnPoseListChanged(FDelegateHandle Handle)
	{
		OnPoseListChanged.Remove(Handle);
	}

	ENGINE_API static FName GetUniquePoseName(const USkeleton* Skeleton);
	ENGINE_API static FSmartName GetUniquePoseSmartName(USkeleton* Skeleton);

protected:
	virtual void RemapTracksToNewSkeleton(USkeleton* NewSkeleton, bool bConvertSpaces) override;
private: 
	// this will do multiple things, it will add tracks and make sure it fix up all poses with it
	// use same as retarget source system we have for animation
	void CombineTracks(const TArray<FName>& NewTracks);

	void ConvertToFullPose();
	void ConvertToAdditivePose(int32 NewBasePoseIndex);
	bool GetBasePoseTransform(TArray<FTransform>& OutBasePose, TArray<float>& OutCurve) const;
	void Reinitialize();

	// After any update to SourceLocalPoses, this does update runtime data
	void AddOrUpdatePose(const FSmartName& PoseName, const TArray<FName>& TrackNames, const TArray<FTransform>& LocalTransform, const TArray<float>& CurveValues);
	void PostProcessData();
#endif // WITH_EDITOR	

private:
	void UpdateTrackBoneIndices();
	bool RemoveInvalidTracks();

#if WITH_EDITORONLY_DATA
	void UpdateRetargetSourceAsset();
#endif
	const TArray<FTransform>& GetRetargetTransforms() const;
	FName GetRetargetTransformsSourceName() const;
};
