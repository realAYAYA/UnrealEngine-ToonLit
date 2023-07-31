// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * One animation sequence of keyframes. Contains a number of tracks of data.
 *
 */

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimCompressionTypes.h"
#include "CustomAttributes.h"
#include "Containers/ArrayView.h"
#include "Animation/CustomAttributes.h"
#include "Animation/AnimData/AnimDataNotifications.h"
#include "Animation/AttributeCurve.h"

#if WITH_EDITOR
#include "AnimData/AnimDataModel.h"
#endif // WITH_EDITOR

#include "AnimSequence.generated.h"


typedef TArray<FTransform> FTransformArrayA2;

class USkeletalMesh;
struct FAnimCompressContext;
struct FAnimSequenceDecompressionContext;
struct FCompactPose;
class ITargetPlatform;

namespace UE { namespace Anim { namespace Compression { struct FScopedCompressionGuard; } } }

// These two always should go together, but it is not right now. 
// I wonder in the future, we change all compressed to be inside as well, so they all stay together
// When remove tracks, it should be handled together 
USTRUCT()
struct ENGINE_API FAnimSequenceTrackContainer
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<struct FRawAnimSequenceTrack> AnimationTracks;

	UPROPERTY()
	TArray<FName>						TrackNames;

	// @todo expand this struct to work better and assign data better
	void Initialize(int32 NumNode)
	{
		AnimationTracks.Empty(NumNode);
		AnimationTracks.AddZeroed(NumNode);
		TrackNames.Empty(NumNode);
		TrackNames.AddZeroed(NumNode);
	}

	void Initialize(TArray<FName> InTrackNames)
	{
		TrackNames = MoveTemp(InTrackNames);
		const int32 NumNode = TrackNames.Num();
		AnimationTracks.Empty(NumNode);
		AnimationTracks.AddZeroed(NumNode);
	}

	int32 GetNum() const
	{
		check (TrackNames.Num() == AnimationTracks.Num());
		return (AnimationTracks.Num());
	}
};

/**
 * Keyframe position data for one track.  Pos(i) occurs at Time(i).  Pos.Num() always equals Time.Num().
 */
USTRUCT()
struct ENGINE_API FTranslationTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FVector3f> PosKeys;

	UPROPERTY()
	TArray<float> Times;
};

/**
 * Keyframe rotation data for one track.  Rot(i) occurs at Time(i).  Rot.Num() always equals Time.Num().
 */
USTRUCT()
struct ENGINE_API FRotationTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FQuat4f> RotKeys;

	UPROPERTY()
	TArray<float> Times;
};

/**
 * Keyframe scale data for one track.  Scale(i) occurs at Time(i).  Rot.Num() always equals Time.Num().
 */
USTRUCT()
struct ENGINE_API FScaleTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FVector3f> ScaleKeys;

	UPROPERTY()
	TArray<float> Times;
};


/**
 * Key frame curve data for one track
 * CurveName: Morph Target Name
 * CurveWeights: List of weights for each frame
 */
USTRUCT()
struct ENGINE_API FCurveTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName CurveName;

	UPROPERTY()
	TArray<float> CurveWeights;

	/** Returns true if valid curve weight exists in the array*/
	bool IsValidCurveTrack();
	
	/** This is very simple cut to 1 key method if all is same since I see so many redundant same value in every frame 
	 *  Eventually this can get more complicated 
	 *  Will return true if compressed to 1. Return false otherwise **/
	bool CompressCurveWeights();
};

USTRUCT()
struct ENGINE_API FCompressedTrack
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<uint8> ByteStream;

	UPROPERTY()
	TArray<float> Times;

	UPROPERTY()
	float Mins[3];

	UPROPERTY()
	float Ranges[3];


	FCompressedTrack()
	{
		for (int32 ElementIndex = 0; ElementIndex < 3; ElementIndex++)
		{
			Mins[ElementIndex] = 0;
		}
		for (int32 ElementIndex = 0; ElementIndex < 3; ElementIndex++)
		{
			Ranges[ElementIndex] = 0;
		}
	}

};

// Param structure for UAnimSequence::RequestAnimCompressionParams
struct ENGINE_API FRequestAnimCompressionParams
{
	// Is the compression to be performed Async
	bool bAsyncCompression;

	// Should we attempt to do framestripping (removing every other frame from raw animation tracks)
	bool bPerformFrameStripping;

	// If false we only perform frame stripping on even numbered frames (as a quality measure)
	bool bPerformFrameStrippingOnOddNumberedFrames;

	// Compression context
	TSharedPtr<FAnimCompressContext> CompressContext;

	// Constructors
	FRequestAnimCompressionParams(bool bInAsyncCompression, bool bInAllowAlternateCompressor = false, bool bInOutput = false, const ITargetPlatform* = nullptr);
	FRequestAnimCompressionParams(bool bInAsyncCompression, TSharedPtr<FAnimCompressContext> InCompressContext);

	// Frame stripping initialization funcs (allow stripping per platform)
	void InitFrameStrippingFromCVar();
	void InitFrameStrippingFromPlatform();

	const ITargetPlatform* TargetPlatform;
};

UCLASS(config=Engine, hidecategories=(UObject, Length), BlueprintType)
class ENGINE_API UAnimSequence : public UAnimSequenceBase
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	/** The DCC framerate of the imported file. UI information only, unit are Hz */
	UPROPERTY(AssetRegistrySearchable, meta = (DisplayName = "Import File Framerate"))
	float ImportFileFramerate;

	/** The resample framerate that was computed during import. UI information only, unit are Hz */
	UPROPERTY(AssetRegistrySearchable, meta = (DisplayName = "Import Resample Framerate"))
	int32 ImportResampleFramerate;

protected:
	/** Contains the number of keys expected within the individual animation tracks. */
	UE_DEPRECATED(5.0, "NumFrames is deprecated see UAnimDataModel::GetNumberOfFrames for the number of source data frames, or GetNumberOfSampledKeys for the target keys")
	UPROPERTY()
	int32 NumFrames;

	/** The number of keys expected within the individual (non-uniform) animation tracks. */
	UE_DEPRECATED(5.0, "NumberOfKeys is deprecated see UAnimDataModel::GetNumberOfKeys for the number of source data keys, or GetNumberOfSampledKeys for the target keys")
	UPROPERTY()
	int32 NumberOfKeys;

	/** The frame rate at which the source animation is sampled. */
	UE_DEPRECATED(5.0, "SamplingFrameRate is deprecated see UAnimDataModel::GetFrameRate for the source frame rate, or GetSamplingFrameRate for the target frame rate instead")
	UPROPERTY()
	FFrameRate SamplingFrameRate;

	/**
	 * In the future, maybe keeping RawAnimSequenceTrack + TrackMap as one would be good idea to avoid inconsistent array size
	 * TrackToSkeletonMapTable(i) should contains  track mapping data for RawAnimationData(i). 
	 */
	UE_DEPRECATED(5.0, "TrackToSkeletonMapTable has been deprecated see FBoneAnimationTrack::BoneTreeIndex")
	UPROPERTY()
	TArray<struct FTrackToSkeletonMap> TrackToSkeletonMapTable;

	/**
	 * Raw uncompressed keyframe data. 
	 */
	UE_DEPRECATED(5.0, "RawAnimationData has been deprecated see FBoneAnimationTrack::InternalTrackData")
	TArray<struct FRawAnimSequenceTrack> RawAnimationData;

	// Update this if the contents of RawAnimationData changes;
	UPROPERTY()
	FGuid RawDataGuid;

	/**
	 * This is name of RawAnimationData tracks for editoronly - if we lose skeleton, we'll need relink them
	 */
	UE_DEPRECATED(5.0, "Animation track names has been deprecated see FBoneAnimationTrack::Name")
	UPROPERTY(VisibleAnywhere, Category="Animation")
	TArray<FName> AnimationTrackNames;

	/**
	 * Source RawAnimationData. Only can be overridden by when transform curves are added first time OR imported
	 */
	TArray<struct FRawAnimSequenceTrack> SourceRawAnimationData_DEPRECATED;

public:

	/**
	 * Allow frame stripping to be performed on this animation if the platform requests it
	 * Can be disabled if animation has high frequency movements that are being lost.
	 */
	UPROPERTY(Category = Compression, EditAnywhere)
	bool bAllowFrameStripping;

	/**
	 * Set a scale for error threshold on compression. This is useful if the animation will 
	 * be played back at a different scale (e.g. if you know the animation will be played
	 * on an actor/component that is scaled up by a factor of 10, set this value to 10)
	 */
	UPROPERTY(Category = Compression, EditAnywhere)
	float CompressionErrorThresholdScale;
#endif

	/** The bone compression settings used to compress bones in this sequence. */
	UPROPERTY(Category = Compression, EditAnywhere, meta = (ForceShowEngineContent))
	TObjectPtr<class UAnimBoneCompressionSettings> BoneCompressionSettings;

	/** The curve compression settings used to compress curves in this sequence. */
	UPROPERTY(Category = Compression, EditAnywhere, meta = (ForceShowEngineContent))
	TObjectPtr<class UAnimCurveCompressionSettings> CurveCompressionSettings;

	FCompressedAnimSequence CompressedData;

	/** Additive animation type. **/
	UPROPERTY(EditAnywhere, Category=AdditiveSettings, AssetRegistrySearchable)
	TEnumAsByte<enum EAdditiveAnimationType> AdditiveAnimType;

	/* Additive refrerence pose type. Refer above enum type */
	UPROPERTY(EditAnywhere, Category=AdditiveSettings, meta=(DisplayName = "Base Pose Type"))
	TEnumAsByte<enum EAdditiveBasePoseType> RefPoseType;

	/* Additve reference frame if RefPoseType == AnimFrame **/
	UPROPERTY(EditAnywhere, Category = AdditiveSettings)
	int32 RefFrameIndex;
	
	/* Additive reference animation if it's relevant - i.e. AnimScaled or AnimFrame **/
	UPROPERTY(EditAnywhere, Category=AdditiveSettings, meta=(DisplayName = "Base Pose Animation"))
	TObjectPtr<class UAnimSequence> RefPoseSeq;

	/** Base pose to use when retargeting */
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, Category=Animation)
	FName RetargetSource;

#if WITH_EDITORONLY_DATA
	/** If RetargetSource is set to Default (None), this is asset for the base pose to use when retargeting. Transform data will be saved in RetargetSourceAssetReferencePose. */
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, Category=Animation, meta = (DisallowedClasses = "/Script/ApexDestruction.DestructibleMesh"))
	TSoftObjectPtr<USkeletalMesh> RetargetSourceAsset;
#endif

	/** When using RetargetSourceAsset, use the post stored here */
	UPROPERTY()
	TArray<FTransform> RetargetSourceAssetReferencePose;

	/** This defines how values between keys are calculated **/
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, Category = Animation)
	EAnimInterpolationType Interpolation;
	
	/** If this is on, it will allow extracting of root motion **/
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, Category = RootMotion, meta = (DisplayName = "EnableRootMotion"))
	bool bEnableRootMotion;

	/** Root Bone will be locked to that position when extracting root motion.**/
	UPROPERTY(EditAnywhere, Category = RootMotion)
	TEnumAsByte<ERootMotionRootLock::Type> RootMotionRootLock;
	
	/** Force Root Bone Lock even if Root Motion is not enabled */
	UPROPERTY(EditAnywhere, Category = RootMotion)
	bool bForceRootLock;

	/** If this is on, it will use a normalized scale value for the root motion extracted: FVector(1.0, 1.0, 1.0) **/
	UPROPERTY(EditAnywhere, AssetRegistrySearchable, Category = RootMotion, meta = (DisplayName = "Use Normalized Root Motion Scale"))
	bool bUseNormalizedRootMotionScale;

	/** Have we copied root motion settings from an owning montage */
	UPROPERTY()
	bool bRootMotionSettingsCopiedFromMontage;

#if WITH_EDITORONLY_DATA
	/** Saved version number with CompressAnimations commandlet. To help with doing it in multiple passes. */
	UPROPERTY()
	int32 CompressCommandletVersion;

	/**
	 * Do not attempt to override compression scheme when running CompressAnimations commandlet.
	 * Some high frequency animations are too sensitive and shouldn't be changed.
	 */
	UPROPERTY(EditAnywhere, Category=Compression)
	uint32 bDoNotOverrideCompression:1;

	/** Importing data and options used for this mesh */
	UPROPERTY(VisibleAnywhere, Instanced, Category=ImportSettings)
	TObjectPtr<class UAssetImportData> AssetImportData;

	/***  for Reimport **/
	/** Path to the resource used to construct this skeletal mesh */
	UPROPERTY()
	FString SourceFilePath_DEPRECATED;

	/** Date/Time-stamp of the file from the last import */
	UPROPERTY()
	FString SourceFileTimestamp_DEPRECATED;

	UE_DEPRECATED(5.0, "bNeedsRebake has been deprecated, transform curves are now baked during compression")
	UPROPERTY(transient)
	bool bNeedsRebake;

	// Track whether we have updated markers so cached data can be updated
	int32 MarkerDataUpdateCounter;
#endif // WITH_EDITORONLY_DATA



public:
	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function
	UE_DEPRECATED(5.0, "Use version that takes FObjectPreSaveContext instead.")
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual bool IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform) override;
#endif // WITH_EDITOR
	virtual void BeginDestroy() override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	static void AddReferencedObjects(UObject* This, FReferenceCollector& Collector);
	//~ End UObject Interface

	//~ Begin UAnimationAsset Interface
	virtual bool IsValidAdditive() const override;
	virtual TArray<FName>* GetUniqueMarkerNames() { return &UniqueMarkerNames; }
#if WITH_EDITOR
	virtual bool GetAllAnimationSequencesReferred(TArray<UAnimationAsset*>& AnimationAssets, bool bRecursive = true) override;
	virtual void ReplaceReferredAnimations(const TMap<UAnimationAsset*, UAnimationAsset*>& ReplacementMap) override;
#endif
	//~ End UAnimationAsset Interface

	//~ Begin UAnimSequenceBase Interface
	virtual void HandleAssetPlayerTickedInternal(FAnimAssetTickContext &Context, const float PreviousTime, const float MoveDelta, const FAnimTickRecord &Instance, struct FAnimNotifyQueue& NotifyQueue) const override;
	virtual bool HasRootMotion() const override { return bEnableRootMotion; }
	virtual void RefreshCacheData() override;
	virtual EAdditiveAnimationType GetAdditiveAnimType() const override { return AdditiveAnimType; }
	virtual int32 GetNumberOfSampledKeys() const override;
	virtual const FFrameRate& GetSamplingFrameRate() const override { return TargetFrameRate; }
	virtual void EvaluateCurveData(FBlendedCurve& OutCurve, float CurrentTime, bool bForceUseRawData = false) const override;
	virtual float EvaluateCurveData(SmartName::UID_Type CurveUID, float CurrentTime, bool bForceUseRawData = false) const override;
	virtual bool HasCurveData(SmartName::UID_Type CurveUID, bool bForceUseRawData) const override;

#if WITH_EDITOR
	UE_DEPRECATED(5.0, "MarkRawDataAsModified has been deprecated, any (Raw Data) modification should be applied using the UAnimDataController API instead. This will handle updating the GUID instead.")
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	virtual void MarkRawDataAsModified(bool bForceNewRawDatGuid = true) override
	{
		Super::MarkRawDataAsModified();
		bUseRawDataOnly = true;
		RawDataGuid = bForceNewRawDatGuid ? FGuid::NewGuid() : GenerateGuidFromRawData();
		FlagDependentAnimationsAsRawDataOnly();
		UpdateDependentStreamingAnimations();
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
	//~ End UAnimSequenceBase Interface

	// Extract Root Motion transform from the animation
	FTransform ExtractRootMotion(float StartTime, float DeltaTime, bool bAllowLooping) const;

	// Extract Root Motion transform from a contiguous position range (no looping)
	FTransform ExtractRootMotionFromRange(float StartTrackPosition, float EndTrackPosition) const;

	// Extract the transform from the root track for the given animation position
	FTransform ExtractRootTrackTransform(float Pos, const FBoneContainer * RequiredBones) const;

	// Begin Transform related functions 
	virtual void GetAnimationPose(FAnimationPoseData& OutAnimationPoseData, const FAnimExtractContext& ExtractionContext) const override;

	/**
	* Get Bone Transform of the animation for the Time given, relative to Parent for all RequiredBones
	*
	* @param	OutPose				[out] Array of output bone transforms
	* @param	OutCurve			[out] Curves to fill	
	* @param	ExtractionContext	Extraction Context (position, looping, root motion, etc.)
	* @param	bForceUseRawData	Override other settings and force raw data pose extraction
	*/
	UE_DEPRECATED(4.26, "Use other GetBonePose signature")
	void GetBonePose(FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext, bool bForceUseRawData=false) const;
	
	/**
	* Get Bone Transform of the Time given, relative to Parent for all RequiredBones
	* This returns different transform based on additive or not. Or what kind of additive.
	*
	* @param	OutAnimationPoseData  [out] Animation Pose related data to populate
	* @param	ExtractionContext	  Extraction Context (position, looping, root motion, etc.)
	*/
	void GetBonePose(struct FAnimationPoseData& OutAnimationPoseData, const FAnimExtractContext& ExtractionContext, bool bForceUseRawData = false) const;

	const TArray<FTrackToSkeletonMap>& GetCompressedTrackToSkeletonMapTable() const { return CompressedData.CompressedTrackToSkeletonMapTable; }
	const TArray<struct FSmartName>& GetCompressedCurveNames() const { return CompressedData.CompressedCurveNames; }

#if WITH_EDITORONLY_DATA
protected:
	void UpdateCompressedCurveName(SmartName::UID_Type CurveUID, const struct FSmartName& NewCurveName);
#endif
private:
#if WITH_EDITORONLY_DATA
	void UpdateRetargetSourceAsset();

	/** Updates the stored sampling frame-rate using the sequence length and number of sampling keys */
	UE_DEPRECATED(5.0, "UpdateFrameRate has been deprecated see UAnimDataController::SetFrameRate")
	void UpdateFrameRate();
#endif
	
	const TArray<FTransform>& GetRetargetTransforms() const;
	FName GetRetargetTransformsSourceName() const;

	/**
	* Retarget a single bone transform, to apply right after extraction.
	*
	* @param	BoneTransform		BoneTransform to read/write from.
	* @param	SkeletonBoneIndex	Bone Index in USkeleton.
	* @param	BoneIndex			Bone Index in Bone Transform array.
	* @param	RequiredBones		BoneContainer
	*/
	void RetargetBoneTransform(FTransform& BoneTransform, const int32 SkeletonBoneIndex, const FCompactPoseBoneIndex& BoneIndex, const FBoneContainer& RequiredBones, const bool bIsBakedAdditive) const;

public:
	/**
	* Get Bone Transform of the additive animation for the Time given, relative to Parent for all RequiredBones
	*
	* @param	OutPose				[out] Output bone transforms
	* @param	OutCurve			[out] Curves to fill	
	* @param	ExtractionContext	Extraction Context (position, looping, root motion, etc.)
	*/
	UE_DEPRECATED(4.26, "Use other GetBonePose_Additive signature")
	void GetBonePose_Additive(FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const;
	void GetBonePose_Additive(FAnimationPoseData& OutAnimationPoseData, const FAnimExtractContext& ExtractionContext) const;

	/**
	* Get Bone Transform of the base (reference) pose of the additive animation for the Time given, relative to Parent for all RequiredBones
	*
	* @param	OutPose				[out] Output bone transforms
	* @param	OutCurve			[out] Curves to fill	
	* @param	ExtractionContext	Extraction Context (position, looping, root motion, etc.)
	*/
	UE_DEPRECATED(4.26, "Use other GetAdditiveBasePose signature")
	void GetAdditiveBasePose(FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const;
	void GetAdditiveBasePose(FAnimationPoseData& OutAnimationPoseData, const FAnimExtractContext& ExtractionContext) const;

	/**
	 * Get Bone Transform of the Time given, relative to Parent for the Track Given
	 *
	 * @param	OutAtom			[out] Output bone transform.
	 * @param	TrackIndex		Index of track to interpolate.
	 * @param	Time			Time on track to interpolate to.
	 * @param	bUseRawData		If true, use raw animation data instead of compressed data.
	 */
	void GetBoneTransform(FTransform& OutAtom, int32 TrackIndex, float Time, bool bUseRawData) const;

	/**
	 * Get Bone Transform of the Time given, relative to Parent for the Track Given
	 *
	 * @param	OutAtom			[out] Output bone transform.
	 * @param	TrackIndex		Index of track to interpolate.
	 * @param	DecompContext	Decompression context to use.
	 * @param	bUseRawData		If true, use raw animation data instead of compressed data.
	 */
	void GetBoneTransform(FTransform& OutAtom, int32 TrackIndex, FAnimSequenceDecompressionContext& DecompContext, bool bUseRawData) const;
	// End Transform related functions 

	// Begin Memory related functions

	/** @return	estimate uncompressed raw size. This is *not* the real raw size. 
				Here we estimate what it would be with no trivial compression. */
#if WITH_EDITOR
	int32 GetUncompressedRawSize() const;

	/**
	 * @return		The approximate size of raw animation data.
	 */
	int32 GetApproxRawSize() const;
#endif // WITH_EDITOR

	/**
	 * @return		The approximate size of compressed animation data for only bones.
	 */
	int32 GetApproxBoneCompressedSize() const;
	
	/**
	 * @return		The approximate size of compressed animation data.
	 */
	int32 GetApproxCompressedSize() const;

	// Get compressed data for this UAnimSequence. May be built directly or pulled from DDC
#if WITH_EDITOR
	bool ShouldPerformStripping(const bool bPerformFrameStripping, const bool bPerformStrippingOnOddFramedAnims) const;
	FString GetDDCCacheKeySuffix(const bool bPerformStripping, const ITargetPlatform* TargetPlatform) const;
	void ApplyCompressedData(const FString& DataCacheKeySuffix, const bool bPerformFrameStripping, const TArray<uint8>& Data);

	void WaitOnExistingCompression(const bool bWantResults=true);
	void RequestAnimCompression(FRequestAnimCompressionParams Params);
	void RequestSyncAnimRecompression(bool bOutput = false)
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		RequestAnimCompression(FRequestAnimCompressionParams(false, false, bOutput));
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}

	void RequestAsyncAnimRecompression(bool bOutput = false)
	{ 
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		RequestAnimCompression(FRequestAnimCompressionParams(true, false, bOutput));
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
#endif

protected:
	void ApplyCompressedData(const TArray<uint8>& Data);
    void ClearCompressedBoneData();
    void ClearCompressedCurveData();
    // Write the compressed data to the supplied FArchive
    void SerializeCompressedData(FArchive& Ar, bool bDDCData);
#if WITH_EDITOR
	void SetCompressionComplete();
#endif

public:
	bool IsCompressedDataValid() const;
	bool IsCurveCompressedDataValid() const;
	// End Memory related functions

	// Begin Utility functions

	/**
	* Get Skeleton Bone Index from Track Index for compressed data
	*
	* @param	TrackIndex		Track Index
	*/
	int32 GetSkeletonIndexFromCompressedDataTrackIndex(const int32 TrackIndex) const
	{
		return GetCompressedTrackToSkeletonMapTable()[TrackIndex].BoneTreeIndex;
	}
	// End Utility functions
	
#if WITH_EDITOR
	void BakeTrackCurvesToRawAnimationTracks(TArray<FRawAnimSequenceTrack>& NewRawTracks, TArray<FName>& NewTrackNames, TArray<FTrackToSkeletonMap>& NewTrackToSkeletonMapTable);

	/**
	 * Add Key to Transform Curves
	 */
	void AddKeyToSequence(float Time, const FName& BoneName, const FTransform& AdditiveTransform);

	/**
	* Return true if compressed data is out of date / missing and so animation needs to use raw data
	*/
	bool DoesNeedRecompress() const { return GetSkeleton() && (bUseRawDataOnly || (GetSkeletonVirtualBoneGuid() != GetSkeleton()->GetVirtualBoneGuid())); }

	/**
	 * Create Animation Sequence from Reference Pose of the Mesh
	 */
	bool CreateAnimation(class USkeletalMesh* Mesh);
	/**
	 * Create Animation Sequence from the Mesh Component's current bone transform
	 */
	bool CreateAnimation(class USkeletalMeshComponent* MeshComponent);
	/**
	 * Create Animation Sequence from the given animation
	 */
	bool CreateAnimation(class UAnimSequence* Sequence);

	/** 
	 * Add validation check to see if it's being ready to play or not
	 */
	virtual bool IsValidToPlay() const override;

	// Get a pointer to the data for a given Anim Notify
	uint8* FindSyncMarkerPropertyData(int32 SyncMarkerIndex, FArrayProperty*& ArrayProperty);

	virtual int32 GetMarkerUpdateCounter() const { return MarkerDataUpdateCounter; }
#endif

	/** Sort the sync markers array by time, earliest first. */
	void SortSyncMarkers();

	// Advancing based on markers
	float GetCurrentTimeFromMarkers(FMarkerPair& PrevMarker, FMarkerPair& NextMarker, float PositionBetweenMarkers) const;
	virtual void AdvanceMarkerPhaseAsLeader(bool bLooping, float MoveDelta, const TArray<FName>& ValidMarkerNames, float& CurrentTime, FMarkerPair& PrevMarker, FMarkerPair& NextMarker, TArray<FPassedMarker>& MarkersPassed, const UMirrorDataTable* MirrorTable) const;
	virtual void AdvanceMarkerPhaseAsFollower(const FMarkerTickContext& Context, float DeltaRemaining, bool bLooping, float& CurrentTime, FMarkerPair& PreviousMarker, FMarkerPair& NextMarker, const UMirrorDataTable* MirrorTable) const;
	virtual void GetMarkerIndicesForTime(float CurrentTime, bool bLooping, const TArray<FName>& ValidMarkerNames, FMarkerPair& OutPrevMarker, FMarkerPair& OutNextMarker) const;

	UE_DEPRECATED(5.0, "Use other GetMarkerSyncPositionfromMarkerIndicies signature")
	virtual FMarkerSyncAnimPosition GetMarkerSyncPositionfromMarkerIndicies(int32 PrevMarker, int32 NextMarker, float CurrentTime) const { return UAnimSequence::GetMarkerSyncPositionFromMarkerIndicies(PrevMarker, NextMarker, CurrentTime, nullptr); }
	virtual FMarkerSyncAnimPosition GetMarkerSyncPositionFromMarkerIndicies(int32 PrevMarker, int32 NextMarker, float CurrentTime, const UMirrorDataTable* MirrorTable) const;
	virtual void GetMarkerIndicesForPosition(const FMarkerSyncAnimPosition& SyncPosition, bool bLooping, FMarkerPair& OutPrevMarker, FMarkerPair& OutNextMarker, float& CurrentTime, const UMirrorDataTable* MirrorTable) const;
	
	virtual float GetFirstMatchingPosFromMarkerSyncPos(const FMarkerSyncAnimPosition& InMarkerSyncGroupPosition) const override;
	virtual float GetNextMatchingPosFromMarkerSyncPos(const FMarkerSyncAnimPosition& InMarkerSyncGroupPosition, const float& StartingPosition) const override;
	virtual float GetPrevMatchingPosFromMarkerSyncPos(const FMarkerSyncAnimPosition& InMarkerSyncGroupPosition, const float& StartingPosition) const override;

	// to support anim sequence base to all montages
	virtual void EnableRootMotionSettingFromMontage(bool bInEnableRootMotion, const ERootMotionRootLock::Type InRootMotionRootLock) override;

#if WITH_EDITOR
	virtual class UAnimSequence* GetAdditiveBasePose() const override 
	{ 
		if (IsValidAdditive())
		{
			return RefPoseSeq;
		}

		return nullptr;
	}

	// Is this animation valid for baking into additive
	bool CanBakeAdditive() const;

	// Bakes out track data for the skeletons virtual bones into the raw data
	void BakeOutVirtualBoneTracks(TArray<FRawAnimSequenceTrack>& NewRawTracks, TArray<FName>& NewAnimationTrackNames, TArray<FTrackToSkeletonMap>& NewTrackToSkeletonMapTable);

	// Performs multiple evaluations of the animation as a test of compressed data validatity
	void TestEvalauteAnimation() const;

	// Bakes out the additive version of this animation into the raw data.
	void BakeOutAdditiveIntoRawData(TArray<FRawAnimSequenceTrack>& NewRawTracks, TArray<FName>& NewAnimationTrackNames, TArray<FTrackToSkeletonMap>& NewTrackToSkeletonMapTable, TArray<FFloatCurve>& NewCurveTracks, TArray<FRawAnimSequenceTrack>& AdditiveBaseAnimationData);

	// Test whether at any point we will scale a bone to 0 (needed for validating additive anims)
	bool DoesSequenceContainZeroScale() const;

	// Helper function to allow us to notify animations that depend on us that they need to update
	void FlagDependentAnimationsAsRawDataOnly() const;

	// Helper function to allow us to update streaming animations that depend on us with our data when we are updated
	void UpdateDependentStreamingAnimations() const;

	// Generate a GUID from a hash of our own raw data
	FGuid GenerateGuidFromRawData() const;

	// Should we be always using our raw data (i.e is our compressed data stale)
	bool OnlyUseRawData() const { return bUseRawDataOnly; }
	void SetUseRawDataOnly(bool bInUseRawDataOnly) { bUseRawDataOnly = bInUseRawDataOnly; }

	// Return this animations guid for the raw data
	FGuid GetRawDataGuid() const
	{ 
		return RawDataGuid;
	}

	/** Resets Bone Animation, Curve data and Notify tracks **/
	void ResetAnimation();
#endif

private:
	/**
	* Get Bone Transform of the animation for the Time given, relative to Parent for all RequiredBones
	* This return mesh rotation only additive pose
	*
	* @param	OutPose				[out] Output bone transforms
	* @param	OutCurve			[out] Curves to fill	
	* @param	ExtractionContext	Extraction Context (position, looping, root motion, etc.)
	*/
	UE_DEPRECATED(4.26, "Use GetBonePose_AdditiveMeshRotationOnly with other signature")
	void GetBonePose_AdditiveMeshRotationOnly(FCompactPose& OutPose, FBlendedCurve& OutCurve, const FAnimExtractContext& ExtractionContext) const;

	void GetBonePose_AdditiveMeshRotationOnly(FAnimationPoseData& OutAnimationPoseData, const FAnimExtractContext& ExtractionContext) const;

	/** Returns whether or not evaluation of the raw (source) animation data is possible according to whether or not the (editor only) data has been stripped */
	bool CanEvaluateRawAnimationData() const;

#if WITH_EDITOR
	/**
	 * Remap Tracks to New Skeleton
	 */ 
	virtual void RemapTracksToNewSkeleton( USkeleton* NewSkeleton, bool bConvertSpaces ) override;

	/**
	 * Remap NaN tracks from the RawAnimation data and recompress
	 */	
	void RemoveNaNTracks();

	/** Retargeting functions */
	bool ConvertAnimationDataToRiggingData(FAnimSequenceTrackContainer & RiggingAnimationData);
	bool ConvertRiggingDataToAnimationData(FAnimSequenceTrackContainer & RiggingAnimationData);
	int32 GetSpaceBasedAnimationData(TArray< TArray<FTransform> > & AnimationDataInComponentSpace, FAnimSequenceTrackContainer * RiggingAnimationData) const;

public:
#endif
	/** Refresh sync marker data*/
	void RefreshSyncMarkerDataFromAuthored();

	/** Take a set of marker positions and validates them against a requested start position, updating them as desired */
	void ValidateCurrentPosition(const FMarkerSyncAnimPosition& Position, bool bPlayingForwards, bool bLooping, float&CurrentTime, FMarkerPair& PreviousMarker, FMarkerPair& NextMarker, const UMirrorDataTable* MirrorTable = nullptr) const;
	bool UseRawDataForPoseExtraction(const FBoneContainer& RequiredBones) const;
	// Should we be always using our raw data (i.e is our compressed data stale)
	bool bUseRawDataOnly;

public:
	/** Authored Sync markers */
	UPROPERTY()
	TArray<FAnimSyncMarker>		AuthoredSyncMarkers;

	/** List of Unique marker names in this animation sequence */
	TArray<FName>				UniqueMarkerNames;

private:
#if WITH_EDITOR
	// Are we currently compressing this animation
	bool bCompressionInProgress;
#endif

public:
#if WITH_EDITOR
	UE_DEPRECATED(5.0, "AddBoneCustomAttribute has been deprecated see UAnimDataController::AddAttribute")
	UFUNCTION(BlueprintCallable, Category=CustomAttributes, meta=(DeprecatedFunction, DeprecationMessage="AddBoneFloatCustomAttribute has been deprecated, use UAnimDataController::AddAttribute instead"))
	void AddBoneFloatCustomAttribute(const FName& BoneName, const FName& AttributeName, const TArray<float>& TimeKeys, const TArray<float>& ValueKeys);

	UE_DEPRECATED(5.0, "AddBoneCustomAttribute has been deprecated see UAnimDataController::AddAttribute")
	UFUNCTION(BlueprintCallable, Category = CustomAttributes, meta=(DeprecatedFunction, DeprecationMessage="AddBoneIntegerCustomAttribute has been deprecated, use UAnimDataController::AddAttribute instead"))
	void AddBoneIntegerCustomAttribute(const FName& BoneName, const FName& AttributeName, const TArray<float>& TimeKeys, const TArray<int32>& ValueKeys);

	UE_DEPRECATED(5.0, "AddBoneStringCustomAttribute has been deprecated see UAnimDataController::AddAttribute")
	UFUNCTION(BlueprintCallable, Category = CustomAttributes, meta=(DeprecatedFunction, DeprecationMessage="AddBoneStringCustomAttribute has been deprecated, use UAnimDataController::AddAttribute instead"))
	void AddBoneStringCustomAttribute(const FName& BoneName, const FName& AttributeName, const TArray<float>& TimeKeys, const TArray<FString>& ValueKeys);

	UE_DEPRECATED(5.0, "RemoveCustomAttribute has been deprecated see UAnimDataController::RemoveAttribute")
	UFUNCTION(BlueprintCallable, Category = CustomAttributes, meta=(DeprecatedFunction, DeprecationMessage="RemoveCustomAttribute has been deprecated, use UAnimDataController::RemoveAttribute instead"))
	void RemoveCustomAttribute(const FName& BoneName, const FName& AttributeName);

	UE_DEPRECATED(5.0, "RemoveAllCustomAttributesForBone has been deprecated see UAnimDataController::RemoveAllAttributesForBone")
	UFUNCTION(BlueprintCallable, Category = CustomAttributes, meta=(DeprecatedFunction, DeprecationMessage="RemoveAllCustomAttributesForBone has been deprecated, use UAnimDataController::RemoveAllAttributesForBone instead"))
	void RemoveAllCustomAttributesForBone(const FName& BoneName);

	UE_DEPRECATED(5.0, "RemoveAllCustomAttributes has been deprecated see UAnimDataController::RemoveAllAttributes")
	UFUNCTION(BlueprintCallable, Category = CustomAttributes, meta=(DeprecatedFunction, DeprecationMessage="RemoveAllCustomAttributes has been deprecated, use UAnimDataController::RemoveAllAttributes instead"))
	void RemoveAllCustomAttributes();
#endif // WITH_EDITOR

	void EvaluateAttributes(FAnimationPoseData& OutAnimationPoseData, const FAnimExtractContext& ExtractionContext, bool bUseRawData) const;	
protected:
#if WITH_EDITOR
	void SynchronousAnimatedBoneAttributesCompression();
	void MoveAttributesToModel();
#endif // WITH_EDITOR

protected:
#if WITH_EDITOR
	// Begin UAnimSequenceBase virtual overrides
	virtual void OnModelModified(const EAnimDataModelNotifyType& NotifyType, UAnimDataModel* Model, const FAnimDataModelNotifPayload& Payload) override;
	virtual void PopulateModel() override;
	// End UAnimSequenceBase virtual overrides

	void EnsureValidRawDataGuid();
	void RecompressAnimationData();
	void ResampleAnimationTrackData();

	void DeleteBoneAnimationData();
	void DeleteDeprecatedRawAnimationData();
public:
	const TArray<FBoneAnimationTrack>& GetResampledTrackData() const { return ResampledAnimationTrackData; }
	void DeleteNotifyTrackData();
#endif // WITH_EDITOR

protected:
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Category = "Animation")
	FFrameRate TargetFrameRate;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, AssetRegistrySearchable, Category = "Animation", Transient, DuplicateTransient)
	int32 NumberOfSampledKeys;

	UPROPERTY(VisibleAnywhere, Category = "Animation", Transient, DuplicateTransient)
	int32 NumberOfSampledFrames;

	UPROPERTY(VisibleAnywhere, Category = "Animation", Transient, DuplicateTransient)
	TArray<FBoneAnimationTrack> ResampledAnimationTrackData;

	bool bBlockCompressionRequests;

	FString RequestedCompression_DDCKey;

private:
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.0, "PerBoneCustomAttributeData has been deprecated see UAnimDataModel::AnimatedBoneAttributes")
	UPROPERTY(VisibleAnywhere, EditFixedSize, Category=CustomAttributes)
	TArray<FCustomAttributePerBoneData> PerBoneCustomAttributeData;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA

protected:
	UPROPERTY()
	TMap<FAnimationAttributeIdentifier, FAttributeCurve> AttributeCurves;

public:
	friend class UAnimationAsset;
	friend struct FScopedAnimSequenceRawDataCache;
	friend class UAnimationBlueprintLibrary;
	friend class UAnimBoneCompressionSettings;
	friend class FCustomAttributeCustomization;
	friend class FAnimSequenceTestBase;
	friend struct UE::Anim::Compression::FScopedCompressionGuard;
};
