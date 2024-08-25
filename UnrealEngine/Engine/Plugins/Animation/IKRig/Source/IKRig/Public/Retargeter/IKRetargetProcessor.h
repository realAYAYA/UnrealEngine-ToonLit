// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IKRetargeter.h"
#include "IKRetargetSettings.h"
#include "IKRigLogger.h"
#include "Kismet/KismetMathLibrary.h"

#include "IKRetargetProcessor.generated.h"

class UIKRetargetProcessor;
enum class ERetargetTranslationMode : uint8;
enum class ERetargetRotationMode : uint8;
class URetargetChainSettings;
class UIKRigDefinition;
class UIKRigProcessor;
struct FReferenceSkeleton;
struct FBoneChain;
struct FIKRetargetPose;
class UObject;
class UIKRetargeter;
class USkeletalMesh;
class URetargetOpBase;

struct IKRIG_API FRetargetSkeleton
{
	TArray<FName> BoneNames;				// list of all bone names in ref skeleton order
	TArray<int32> ParentIndices;			// per-bone indices of parent bones (the hierarchy)
	TArray<FTransform> RetargetLocalPose;	// local space retarget pose
	TArray<FTransform> RetargetGlobalPose;	// global space retarget pose
	FName RetargetPoseName;					// the name of the retarget pose this was initialized with
	int32 RetargetPoseVersion;				// the version of the retarget pose this was initialized with (transient)
	USkeletalMesh* SkeletalMesh;			// the skeletal mesh this was initialized with
	TArray<FName> ChainThatContainsBone;	// record which chain is actually controlling each bone

	void Initialize(
		USkeletalMesh* InSkeletalMesh,
		const TArray<FBoneChain>& BoneChains,
		const FName InRetargetPoseName,
		const FIKRetargetPose* RetargetPose,
		const FName RetargetRootBone);

	void Reset();

	void GenerateRetargetPose(
		const FName InRetargetPoseName,
		const FIKRetargetPose* InRetargetPose,
		const FName RetargetRootBone);

	int32 FindBoneIndexByName(const FName InName) const;

	int32 GetParentIndex(const int32 BoneIndex) const;

	void UpdateGlobalTransformsBelowBone(
		const int32 StartBoneIndex,
		const TArray<FTransform>& InLocalPose,
		TArray<FTransform>& OutGlobalPose) const;

	void UpdateLocalTransformsBelowBone(
		const int32 StartBoneIndex,
		TArray<FTransform>& OutLocalPose,
		const TArray<FTransform>& InGlobalPose) const;
	
	void UpdateGlobalTransformOfSingleBone(
		const int32 BoneIndex,
		const TArray<FTransform>& InLocalPose,
		TArray<FTransform>& OutGlobalPose) const;
	
	void UpdateLocalTransformOfSingleBone(
		const int32 BoneIndex,
		TArray<FTransform>& OutLocalPose,
		const TArray<FTransform>& InGlobalPose) const;

	FTransform GetGlobalRetargetPoseOfSingleBone(
		const int32 BoneIndex,
		const TArray<FTransform>& InGlobalPose) const;

	int32 GetCachedEndOfBranchIndex(const int32 InBoneIndex) const;

	void GetChildrenIndices(const int32 BoneIndex, TArray<int32>& OutChildren) const;

	void GetChildrenIndicesRecursive(const int32 BoneIndex, TArray<int32>& OutChildren) const;
	
	bool IsParentOfChild(const int32 PotentialParentIndex, const int32 ChildBoneIndex) const;

private:
	
	/** One index per-bone. Lazy-filled on request. Stores the last element of the branch below the bone.
	 * You can iterate between in the indices stored here and the bone in question to iterate over all children recursively */
	mutable TArray<int32> CachedEndOfBranchIndices;
};

struct FTargetSkeleton : public FRetargetSkeleton
{
	TArray<FTransform> OutputGlobalPose;
	// true for bones that are in a target chain that is ALSO mapped to a source chain
	// ie, bones that are actually posed based on a mapped source chain
	TArray<bool> IsBoneRetargeted;

	void Initialize(
		USkeletalMesh* InSkeletalMesh,
		const TArray<FBoneChain>& BoneChains,
		const FName InRetargetPoseName,
		const FIKRetargetPose* RetargetPose,
		const FName RetargetRootBone);

	void Reset();

	void SetBoneIsRetargeted(const int32 BoneIndex, const bool IsRetargeted);

	void UpdateGlobalTransformsAllNonRetargetedBones(TArray<FTransform>& InOutGlobalPose);
};

/** resolving an FBoneChain to an actual skeleton, used to validate compatibility and get all chain indices */
struct IKRIG_API FResolvedBoneChain
{
	FResolvedBoneChain(const FBoneChain& BoneChain, const FRetargetSkeleton& Skeleton, TArray<int32> &OutBoneIndices);

	/* Does the START bone exist in the skeleton? */
	bool bFoundStartBone = false;
	/* Does the END bone exist in the skeleton? */
	bool bFoundEndBone = false;
	/* Is the END bone equals or a child of the START bone? */
	bool bEndIsStartOrChildOfStart  = false;

	bool IsValid() const
	{
		return bFoundStartBone && bFoundEndBone && bEndIsStartOrChildOfStart;
	}
};

struct FRootSource
{
	FName BoneName;
	int32 BoneIndex;
	FQuat InitialRotation;
	float InitialHeightInverse;
	FVector InitialPosition;
	FVector CurrentPosition;
	FVector CurrentPositionNormalized;
	FQuat CurrentRotation;
};

struct FRootTarget
{
	FName BoneName;
	int32 BoneIndex;
	FVector InitialPosition;
	FQuat InitialRotation;
	float InitialHeight;

	FVector RootTranslationDelta;
	FQuat RootRotationDelta;
};

struct FRootRetargeter
{	
	FRootSource Source;
	FRootTarget Target;
	FTargetRootSettings Settings;

	void Reset();
	
	bool InitializeSource(
		const FName SourceRootBoneName,
		const FRetargetSkeleton& SourceSkeleton,
		FIKRigLogger& Log);
	
	bool InitializeTarget(
		const FName TargetRootBoneName,
		const FTargetSkeleton& TargetSkeleton,
		FIKRigLogger& Log);

	void EncodePose(const TArray<FTransform> &SourceGlobalPose);
	
	void DecodePose(TArray<FTransform> &OutTargetGlobalPose);

	FVector GetGlobalScaleVector() const
	{
		return GlobalScaleFactor * FVector(Settings.ScaleHorizontal, Settings.ScaleHorizontal, Settings.ScaleVertical);
	}

private:
	FVector GlobalScaleFactor;
};

struct FPoleVectorMatcher
{
	EAxis::Type SourcePoleAxis;
	EAxis::Type TargetPoleAxis;
	float TargetToSourceAngularOffsetAtRefPose;
	TArray<int32> AllChildrenWithinChain;

	bool Initialize(
		const TArray<int32>& SourceIndices,
		const TArray<int32>& TargetIndices,
		const TArray<FTransform> &SourceGlobalPose,
		const TArray<FTransform> &TargetGlobalPose,
		const FRetargetSkeleton& TargetSkeleton);

	void MatchPoleVector(
		const FTargetChainFKSettings& Settings,
		const TArray<int32>& SourceIndices,
		const TArray<int32>& TargetIndices,
		const TArray<FTransform> &SourceGlobalPose,
		TArray<FTransform> &OutTargetGlobalPose,
		FRetargetSkeleton& TargetSkeleton);
	
private:

	EAxis::Type CalculateBestPoleAxisForChain(
		const TArray<int32>& BoneIndices,
		const TArray<FTransform>& GlobalPose);
	
	static FVector CalculatePoleVector(
		const EAxis::Type& PoleAxis,
		const TArray<int32>& BoneIndices,
		const TArray<FTransform>& GlobalPose);

	static EAxis::Type GetMostDifferentAxis(
		const FTransform& Transform,
		const FVector& InNormal);

	static FVector GetChainAxisNormalized(
		const TArray<int32>& BoneIndices,
		const TArray<FTransform>& GlobalPose);
};

struct FChainFK
{
	TArray<FTransform> InitialGlobalTransforms;
	TArray<FTransform> InitialLocalTransforms;
	TArray<FTransform> CurrentGlobalTransforms;

	TArray<float> Params;
	TArray<int32> BoneIndices;

	int32 ChainParentBoneIndex;
	FTransform ChainParentInitialGlobalTransform;

	bool Initialize(
		const FRetargetSkeleton& Skeleton,
		const TArray<int32>& InBoneIndices,
		const TArray<FTransform> &InitialGlobalPose,
		FIKRigLogger& Log);

	FTransform GetTransformAtParam(const TArray<FTransform>& Transforms, const float& Param) const;

private:
	
	bool CalculateBoneParameters(FIKRigLogger& Log);

protected:

	static void FillTransformsWithLocalSpaceOfChain(
		const FRetargetSkeleton& Skeleton,
		const TArray<FTransform>& InGlobalPose,
		const TArray<int32>& BoneIndices,
		TArray<FTransform>& OutLocalTransforms);

	void PutCurrentTransformsInRefPose(
		const TArray<int32>& BoneIndices,
		const FRetargetSkeleton& Skeleton,
		const TArray<FTransform>& InCurrentGlobalPose);
};

struct FChainEncoderFK : public FChainFK
{
	TArray<FTransform> CurrentLocalTransforms;
	FTransform ChainParentCurrentGlobalTransform;
	
	void EncodePose(
		const FRetargetSkeleton& SourceSkeleton,
		const TArray<int32>& SourceBoneIndices,
		const TArray<FTransform> &InSourceGlobalPose);

	void TransformCurrentChainTransforms(const FTransform& NewParentTransform);
};

struct FChainDecoderFK : public FChainFK
{
	void InitializeIntermediateParentIndices(
		const int32 RetargetRootBoneIndex,
		const int32 ChainRootBoneIndex,
		const FTargetSkeleton& TargetSkeleton);
	
	void DecodePose(
		const FRootRetargeter& RootRetargeter,
		const FTargetChainFKSettings& Settings,
		const TArray<int32>& TargetBoneIndices,
		FChainEncoderFK& SourceChain,
		const FTargetSkeleton& TargetSkeleton,
		TArray<FTransform> &InOutGlobalPose);

	void MatchPoleVector(
		const FTargetChainFKSettings& Settings,
		const TArray<int32>& TargetBoneIndices,
		FChainEncoderFK& SourceChain,
		const FTargetSkeleton& TargetSkeleton,
		TArray<FTransform> &InOutGlobalPose);
	
private:
	
	void UpdateIntermediateParents(
		const FTargetSkeleton& TargetSkeleton,
		TArray<FTransform> &InOutGlobalPose);

	TArray<int32> IntermediateParentIndices;
};

struct FDecodedIKChain
{
	FVector EndEffectorPosition = FVector::ZeroVector;
	FQuat EndEffectorRotation = FQuat::Identity;
	FVector PoleVectorPosition = FVector::ZeroVector;
};

struct FSourceChainIK
{
	int32 StartBoneIndex = INDEX_NONE;
	int32 EndBoneIndex = INDEX_NONE;
	
	FVector InitialEndPosition = FVector::ZeroVector;
	FQuat InitialEndRotation = FQuat::Identity;
	float InvInitialLength = 1.0f;

	// results after encoding...
	FVector PreviousEndPosition = FVector::ZeroVector;
	FVector CurrentEndPosition = FVector::ZeroVector;
	FVector CurrentEndDirectionNormalized = FVector::ZeroVector;
	FQuat CurrentEndRotation = FQuat::Identity;
	float CurrentHeightFromGroundNormalized = 0.0f;
};

struct FTargetChainIK
{
	int32 BoneIndexA = INDEX_NONE;
	int32 BoneIndexC = INDEX_NONE;
	
	float InitialLength = 1.0f;
	FVector InitialEndPosition = FVector::ZeroVector;
	FQuat InitialEndRotation = FQuat::Identity;
	FVector PrevEndPosition = FVector::ZeroVector;
};

struct FChainDebugData
{
	FTransform InputTransformStart = FTransform::Identity;
	FTransform InputTransformEnd = FTransform::Identity;
	FTransform OutputTransformEnd = FTransform::Identity;
};

struct FChainRetargeterIK
{
	FSourceChainIK Source;
	FTargetChainIK Target;
	
	FDecodedIKChain Results;

	bool ResetThisTick;
	FVectorSpringState PlantingSpringState;

	#if WITH_EDITOR
	FChainDebugData DebugData;
	#endif

	bool InitializeSource(
		const TArray<int32>& BoneIndices,
		const TArray<FTransform> &SourceInitialGlobalPose,
		FIKRigLogger& Log);
	
	bool InitializeTarget(
		const TArray<int32>& BoneIndices,
		const TArray<FTransform> &TargetInitialGlobalPose,
		FIKRigLogger& Log);

	void EncodePose(const TArray<FTransform> &SourceInputGlobalPose);
	
	void DecodePose(
		const FTargetChainIKSettings& Settings,
		const FTargetChainSpeedPlantSettings& SpeedPlantSettings,
		const FRootRetargeter& RootRetargeter,
		const TMap<FName, float>& SpeedValuesFromCurves,
		const float DeltaTime,
		const TArray<FTransform>& InGlobalPose);

	void SaveDebugInfo(const TArray<FTransform>& InGlobalPose);
};

struct FRetargetChainPair
{
	TArray<int32> SourceBoneIndices;
	TArray<int32> TargetBoneIndices;
	
	FName SourceBoneChainName;
	FName TargetBoneChainName;

	virtual ~FRetargetChainPair() = default;
	
	virtual bool Initialize(
		const FBoneChain& SourceBoneChain,
		const FBoneChain& TargetBoneChain,
		const FRetargetSkeleton& SourceSkeleton,
		const FTargetSkeleton& TargetSkeleton,
		FIKRigLogger& Log);

private:

	bool ValidateBoneChainWithSkeletalMesh(
		const bool IsSource,
		const FBoneChain& BoneChain,
		const FRetargetSkeleton& RetargetSkeleton,
		FIKRigLogger& Log);
};

struct FRetargetChainPairFK : FRetargetChainPair
{
	FTargetChainFKSettings Settings;
	FChainEncoderFK FKEncoder;
	FChainDecoderFK FKDecoder;
	FPoleVectorMatcher PoleVectorMatcher;
	
	virtual bool Initialize(
        const FBoneChain& SourceBoneChain,
        const FBoneChain& TargetBoneChain,
        const FRetargetSkeleton& SourceSkeleton,
        const FTargetSkeleton& TargetSkeleton,
        FIKRigLogger& Log) override;
};

struct FRetargetChainPairIK : FRetargetChainPair
{
	FTargetChainIKSettings Settings;
	FTargetChainSpeedPlantSettings SpeedPlantSettings;
	FChainRetargeterIK IKChainRetargeter;
	FName IKGoalName;
	FName PoleVectorGoalName;

	virtual bool Initialize(
        const FBoneChain& SourceBoneChain,
        const FBoneChain& TargetBoneChain,
        const FRetargetSkeleton& SourceSkeleton,
        const FTargetSkeleton& TargetSkeleton,
        FIKRigLogger& Log) override;
};

struct FRetargetDebugData
{
	FTransform StrideWarpingFrame;
};

/** The runtime processor that converts an input pose from a source skeleton into an output pose on a target skeleton.
 * To use:
 * 1. Initialize a processor with a Source/Target skeletal mesh and a UIKRetargeter asset.
 * 2. Call RunRetargeter and pass in a source pose as an array of global-space transforms
 * 3. RunRetargeter returns an array of global space transforms for the target skeleton.
 */
UCLASS()
class IKRIG_API UIKRetargetProcessor : public UObject
{
	GENERATED_BODY()

public:

	UIKRetargetProcessor();
	
	/**
	* Initialize the retargeter to enable running it.
	* @param SourceSkeleton - the skeletal mesh to poses FROM
	* @param TargetSkeleton - the skeletal mesh to poses TO
	* @param InRetargeterAsset - the source asset to use for retargeting settings
	* @param bSuppressWarnings - if true, will not output warnings during initialization
	* @warning - Initialization does a lot of validation and can fail for many reasons. Check bIsLoadedAndValid afterwards.
	*/
	void Initialize(
		USkeletalMesh *SourceSkeleton,
		USkeletalMesh *TargetSkeleton,
		UIKRetargeter* InRetargeterAsset,
		const bool bSuppressWarnings=false);

	/**
	* Run the retarget to generate a new pose.
	* @param InSourceGlobalPose -  is the source mesh input pose in Component/Global space
	* @return The retargeted Component/Global space pose for the target skeleton
	*/
	TArray<FTransform>& RunRetargeter(
		const TArray<FTransform>& InSourceGlobalPose,
		const TMap<FName, float>& SpeedValuesFromCurves,
		const float DeltaTime);

	/** Apply the settings stored in a retarget profile. Call this before RunRetargeter() to use the settings stored in a profile. */
	void ApplySettingsFromProfile(const FRetargetProfile& Profile);
	
	/** Apply the settings stored in the retargeter asset. */
	void ApplySettingsFromAsset();

	/** Get read-only access to either source or target skeleton. */
	const FRetargetSkeleton& GetSkeleton(ERetargetSourceOrTarget SourceOrTarget) const;

	/** Get name of the root bone of the given skeleton. */
	FName GetRetargetRoot(ERetargetSourceOrTarget SourceOrTarget) const;
	
	/** Get whether this processor is ready to call RunRetargeter() and generate new poses. */
	bool IsInitialized() const { return bIsInitialized; };

	/** Get whether this processor was initialized with these skeletal meshes and retarget asset*/
	bool WasInitializedWithTheseAssets(
		const TObjectPtr<USkeletalMesh> InSourceMesh,
		const TObjectPtr<USkeletalMesh> InTargetMesh,
		const TObjectPtr<UIKRetargeter> InRetargetAsset);

	/** Get the currently running IK Rig processor for the target */
	UIKRigProcessor* GetTargetIKRigProcessor() const { return IKRigProcessor; };
	
	/** Get read only access to the core IK retarget chains */
	const TArray<FRetargetChainPairIK>& GetIKChainPairs() const { return ChainPairsIK; };
	
	/** Get read only access to the core FK retarget chains */
	const TArray<FRetargetChainPairFK>& GetFKChainPairs() const { return ChainPairsFK; };
	
	/** Get read only access to the core root retargeter */
	const FRootRetargeter& GetRootRetargeter() const { return RootRetargeter; };
	
	// Get read only access to the retarget ops currently running in processor
	const TArray<TObjectPtr<URetargetOpBase>>& GetRetargetOps() const {return OpStack; };

	// Get read only access to the current global settings
	const FRetargetGlobalSettings& GetGlobalSettings() const {return GlobalSettings; };
	
	/** Reset the IK planting state. */
	void ResetPlanting();

	/** Does a partial reinitialization (at runtime) whenever the retarget pose is swapped to a different or if the
	 * pose has been modified. Does nothing if the pose has not changed. */
	void UpdateRetargetPoseAtRuntime(const FName NewRetargetPoseName, ERetargetSourceOrTarget SourceOrTarget);

	/** logging system */
	FIKRigLogger Log;

	/** Set that this processor needs to be reinitialized. */
	void SetNeedsInitialized();
	
#if WITH_EDITOR
private:
	DECLARE_MULTICAST_DELEGATE(FOnRetargeterInitialized);
	FOnRetargeterInitialized RetargeterInitialized;
public:
	/** Returns true if the bone is part of a retarget chain or root bone, false otherwise. */
	bool IsBoneRetargeted(const FName BoneName, const ERetargetSourceOrTarget SourceOrTarget) const;
	/** Returns index of the bone with the given name in either Source or Target skeleton. */
	int32 GetBoneIndexFromName(const FName BoneName, const ERetargetSourceOrTarget SourceOrTarget) const;
	/** Returns name of the chain associated with this bone. Returns NAME_None if bone is not in a chain. */
	FName GetChainNameForBone(const FName BoneName, const ERetargetSourceOrTarget SourceOrTarget) const;
	/** Get a transform at a given param in a chain */
	FTransform GetGlobalRetargetPoseAtParam(const FName InChainName, const float Param, const ERetargetSourceOrTarget SourceOrTarget) const;
	/** Get access to the internal chain on the given skeleton */
	const FChainFK* GetChain(const FName InChainName, const ERetargetSourceOrTarget SourceOrTarget) const;
	/** Get the param of the bone in it's retarget chain. Ranges from 0 to NumBonesInChain. */
	float GetParamOfBoneInChain(const FName InBoneName, const ERetargetSourceOrTarget SourceOrTarget) const;
	/** Get the bone in the chain at the given param. */
	FName GetBoneAtParam(const FName InChainName, const float InParam, const ERetargetSourceOrTarget SourceOrTarget) const;
	/** Get the chain mapped to this one. */
	FName GetMappedChainName(const FName InChainName, const ERetargetSourceOrTarget SourceOrTarget);
	/** store data for debug drawing */
	FRetargetDebugData DebugData;
	/** Attach a delegate to be notified whenever this processor is initialized. */
	FOnRetargeterInitialized& OnRetargeterInitialized(){ return RetargeterInitialized; };
#endif

private:

	/** Only true once Initialize() has successfully completed.*/
	bool bIsInitialized = false;
	int32 AssetVersionInitializedWith = -1;
	/** true when roots are able to be retargeted */
	bool bRootsInitialized = false;
	/** true when at least one pair of bone chains is able to be retargeted */
	bool bAtLeastOneValidBoneChainPair = false;
	/** true when IK Rig has been initialized and is ready to run*/
	bool bIKRigInitialized = false;

	/** The source asset this processor was initialized with. */
	UIKRetargeter* RetargeterAsset = nullptr;

	/** The internal data structures used to represent the SOURCE skeleton / pose during retargeter.*/
	FRetargetSkeleton SourceSkeleton;

	/** The internal data structures used to represent the TARGET skeleton / pose during retargeter.*/
	FTargetSkeleton TargetSkeleton;

	/** The IK Rig processor for running IK on the target */
	UPROPERTY(Transient) // must be property to keep from being GC'd
	TObjectPtr<UIKRigProcessor> IKRigProcessor = nullptr;

	/** The Source/Target pairs of Bone Chains retargeted using the FK algorithm */
	TArray<FRetargetChainPairFK> ChainPairsFK;

	/** The Source/Target pairs of Bone Chains retargeted using the IK Rig */
	TArray<FRetargetChainPairIK> ChainPairsIK;

	/** The Source/Target pair of Root Bones retargeted with scaled translation */
	FRootRetargeter RootRetargeter;

	/** The currently used global settings (driven either by source asset or a profile) */
	FRetargetGlobalSettings GlobalSettings;

	/** The collection of operations to run in the final phase of retargeting */
	UPROPERTY(Transient) // must be property to keep from being GC'd
	TArray<TObjectPtr<URetargetOpBase>> OpStack;
	
	/** Initializes the FRootRetargeter */
	bool InitializeRoots();

	/** Initializes the all the chain pairs */
	bool InitializeBoneChainPairs();

	/** Initializes the IK Rig that evaluates the IK solve for the target IK chains */
	bool InitializeIKRig(UObject* Outer, const USkeletalMesh* InSkeletalMesh);

	// Initialize the retarget op stack
	bool InitializeOpStack(const TArray<TObjectPtr<URetargetOpBase>>& OpStackFromAsset);
	
	/** Internal retarget phase for the root. */
	void RunRootRetarget(const TArray<FTransform>& InGlobalTransforms, TArray<FTransform>& OutGlobalTransforms);

	/** Internal retarget phase for the FK chains. */
	void RunFKRetarget(const TArray<FTransform>& InGlobalTransforms, TArray<FTransform>& OutGlobalTransforms);

	/** Internal retarget phase for the IK chains. */
	void RunIKRetarget(
		const TArray<FTransform>& InSourceGlobalPose,
		TArray<FTransform>& OutTargetGlobalPose,
		const TMap<FName, float>& SpeedValuesFromCurves,
		const float DeltaTime);

	/** Internal retarget phase for the pole matching feature of FK chains. */
	void RunPoleVectorMatching(const TArray<FTransform>& InGlobalTransforms, TArray<FTransform>& OutGlobalTransforms);

	/** Runs in the after the base IK retarget to apply stride warping to IK goals. */
	void RunStrideWarping(const TArray<FTransform>& InTargetGlobalPose);

	/** Run all post process operations on the retargeted result. */
	void RunRetargetOps(const TArray<FTransform>& InSourceGlobalPose, TArray<FTransform>& OutTargetGlobalPose);
};
