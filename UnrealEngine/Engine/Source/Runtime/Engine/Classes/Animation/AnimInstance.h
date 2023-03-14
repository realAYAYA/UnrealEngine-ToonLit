// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Animation/AnimTypes.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AnimMontage.h"
#include "BonePose.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimNotifyQueue.h"
#include "Animation/AnimSubsystemInstance.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimInstance.generated.h"

// Post Compile Validation requires WITH_EDITOR
#define ANIMINST_PostCompileValidation WITH_EDITOR

struct FDisplayDebugManager;
class FDebugDisplayInfo;
class IAnimClassInterface;
class UAnimInstance;
class UCanvas;
struct FAnimInstanceProxy;
struct FAnimNode_AssetPlayerBase;
struct FAnimNode_AssetPlayerRelevancyBase;
struct FAnimNode_StateMachine;
struct FAnimNode_LinkedInputPose;
struct FBakedAnimationStateMachine;
class FCompilerResultsLog;
struct FBoneContainer;
struct FAnimNode_LinkedAnimLayer;
enum class ETransitionRequestQueueMode : uint8;
enum class ETransitionRequestOverwriteMode : uint8;

typedef TArray<FTransform> FTransformArrayA2;

namespace UE::Anim
{
	using FSlotInertializationRequest = TPair<float, const UBlendProfile*>;
}	// namespace UE::Anim

struct FParallelEvaluationData
{
	FBlendedHeapCurve& OutCurve;
	FCompactPose& OutPose;
	UE::Anim::FHeapAttributeContainer& OutAttributes;
};

UENUM()
enum class EMontagePlayReturnType : uint8
{
	//Return value is the length of the montage (in seconds)
	MontageLength,
	//Return value is the play duration of the montage (length / play rate, in seconds)
	Duration,
};

DECLARE_DELEGATE_OneParam(FOnMontageStarted, UAnimMontage*)
DECLARE_DELEGATE_TwoParams(FOnMontageEnded, UAnimMontage*, bool /*bInterrupted*/)
DECLARE_DELEGATE_TwoParams(FOnMontageBlendingOutStarted, UAnimMontage*, bool /*bInterrupted*/)
/**
* Delegate for when Montage is started
*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMontageStartedMCDelegate, UAnimMontage*, Montage);

/**
* Delegate for when Montage is completed, whether interrupted or finished
* Weight of this montage is 0.f, so it stops contributing to output pose
*
* bInterrupted = true if it was not property finished
*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMontageEndedMCDelegate, UAnimMontage*, Montage, bool, bInterrupted);

/** Delegate for when all montage instances have ended. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllMontageInstancesEndedMCDelegate);

/**
* Delegate for when Montage started to blend out, whether interrupted or finished
* DesiredWeight of this montage becomes 0.f, but this still contributes to the output pose
*
* bInterrupted = true if it was not property finished
*/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMontageBlendingOutStartedMCDelegate, UAnimMontage*, Montage, bool, bInterrupted);

/** Delegate that native code can hook to to provide additional transition logic */
DECLARE_DELEGATE_RetVal(bool, FCanTakeTransition);

/** Delegate that native code can hook into to handle state entry/exit */
DECLARE_DELEGATE_ThreeParams(FOnGraphStateChanged, const struct FAnimNode_StateMachine& /*Machine*/, int32 /*PrevStateIndex*/, int32 /*NextStateIndex*/);

/** Delegate that allows users to insert custom animation curve values - for now, it's only single, not sure how to make this to multi delegate and retrieve value sequentially, so */
DECLARE_DELEGATE_OneParam(FOnAddCustomAnimationCurves, UAnimInstance*)

/** Delegate called by 'PlayMontageNotify' and 'PlayMontageNotifyWindow' **/
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPlayMontageAnimNotifyDelegate, FName, NotifyName, const FBranchingPointNotifyPayload&, BranchingPointPayload);


USTRUCT()
struct FA2Pose
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TArray<FTransform> Bones;

	FA2Pose() {}
};

/** Component space poses. */
USTRUCT()
struct ENGINE_API FA2CSPose : public FA2Pose
{
	GENERATED_USTRUCT_BODY()

private:
	/** Pointer to current BoneContainer. */
	const struct FBoneContainer* BoneContainer;

	/** Once evaluated to be mesh space, this flag will be set. */
	UPROPERTY()
	TArray<uint8> ComponentSpaceFlags;

public:
	FA2CSPose()
		: BoneContainer(NULL)
	{
	}

	/** Constructor - needs LocalPoses. */
	void AllocateLocalPoses(const FBoneContainer& InBoneContainer, const FA2Pose & LocalPose);

	/** Constructor - needs LocalPoses. */
	void AllocateLocalPoses(const FBoneContainer& InBoneContainer, const FTransformArrayA2 & LocalBones);

	/** Returns if this struct is valid. */
	bool IsValid() const;

	/** Get parent bone index for given bone index. */
	int32 GetParentBoneIndex(const int32 BoneIndex) const;

	/** Returns local transform for the bone index. **/
	FTransform GetLocalSpaceTransform(int32 BoneIndex);

	/** Do not access Bones array directly; use this instead. This will fill up gradually mesh space bases. */
	FTransform GetComponentSpaceTransform(int32 BoneIndex);

	/** convert to local poses **/
	void ConvertToLocalPoses(FA2Pose & LocalPoses) const;

private:
	/** Calculate all transform till parent **/
	void CalculateComponentSpaceTransform(int32 Index);
	void SetComponentSpaceTransform(int32 Index, const FTransform& NewTransform);

	/**
	 * Convert Bone to Local Space.
	 */
	void ConvertBoneToLocalSpace(int32 BoneIndex);


	void SetLocalSpaceTransform(int32 Index, const FTransform& NewTransform);

	// This is not really best way to protect SetComponentSpaceTransform, but we'd like to make sure that isn't called by anywhere else.
	friend class FAnimationRuntime;
};



/** Helper struct for Slot node pose evaluation. */
USTRUCT()
struct FSlotEvaluationPose
{
	GENERATED_USTRUCT_BODY()

	/** Type of additive for pose */
	UPROPERTY()
	TEnumAsByte<EAdditiveAnimationType> AdditiveType;

	/** Weight of pose */
	UPROPERTY()
	float Weight;

	/*** ATTENTION *****/
	/* These Pose/Curve is stack allocator. You should not use it outside of stack. */
	FCompactPose Pose;
	FBlendedCurve Curve;
	UE::Anim::FStackAttributeContainer Attributes;

	FSlotEvaluationPose()
		: AdditiveType(AAT_None)
		, Weight(0.0f)
	{
	}

	FSlotEvaluationPose(float InWeight, EAdditiveAnimationType InAdditiveType)
		: AdditiveType(InAdditiveType)
		, Weight(InWeight)
	{
	}

	FSlotEvaluationPose(FSlotEvaluationPose&& InEvaluationPose)
		: AdditiveType(InEvaluationPose.AdditiveType)
		, Weight(InEvaluationPose.Weight)
	{
		Pose.MoveBonesFrom(InEvaluationPose.Pose);
		Curve.MoveFrom(InEvaluationPose.Curve);
		Attributes.MoveFrom(InEvaluationPose.Attributes);
	}

	FSlotEvaluationPose(const FSlotEvaluationPose& InEvaluationPose) = default;
	FSlotEvaluationPose& operator=(const FSlotEvaluationPose& InEvaluationPose) = default;
};

/** Helper struct to store a Queued Montage BlendingOut event. */
struct FQueuedMontageBlendingOutEvent
{
	class UAnimMontage* Montage;
	bool bInterrupted;
	FOnMontageBlendingOutStarted Delegate;

	FQueuedMontageBlendingOutEvent()
		: Montage(NULL)
		, bInterrupted(false)
	{}

	FQueuedMontageBlendingOutEvent(class UAnimMontage* InMontage, bool InbInterrupted, FOnMontageBlendingOutStarted InDelegate)
		: Montage(InMontage)
		, bInterrupted(InbInterrupted)
		, Delegate(InDelegate)
	{}
};

/** Helper struct to store a Queued Montage Ended event. */
struct FQueuedMontageEndedEvent
{
	class UAnimMontage* Montage;
	int32 MontageInstanceID;
	bool bInterrupted;
	FOnMontageEnded Delegate;

	FQueuedMontageEndedEvent()
		: Montage(NULL)
		, MontageInstanceID(INDEX_NONE)
		, bInterrupted(false)
	{}

	FQueuedMontageEndedEvent(class UAnimMontage* InMontage, int32 InInstanceID, bool InbInterrupted, FOnMontageEnded InDelegate)
		: Montage(InMontage)
		, MontageInstanceID(InInstanceID)
		, bInterrupted(InbInterrupted)
		, Delegate(InDelegate)
	{}
};

/** Binding allowing native transition rule evaluation */
struct FNativeTransitionBinding
{
	/** State machine to bind to */
	FName MachineName;

	/** Previous state the transition comes from */
	FName PreviousStateName;

	/** Next state the transition goes to */
	FName NextStateName;

	/** Delegate to use when checking transition */
	FCanTakeTransition NativeTransitionDelegate;

#if WITH_EDITORONLY_DATA
	/** Name of this transition rule */
	FName TransitionName;
#endif

	FNativeTransitionBinding(const FName& InMachineName, const FName& InPreviousStateName, const FName& InNextStateName, const FCanTakeTransition& InNativeTransitionDelegate, const FName& InTransitionName = NAME_None)
		: MachineName(InMachineName)
		, PreviousStateName(InPreviousStateName)
		, NextStateName(InNextStateName)
		, NativeTransitionDelegate(InNativeTransitionDelegate)
#if WITH_EDITORONLY_DATA
		, TransitionName(InTransitionName)
#endif
	{
	}
};

/** Binding allowing native notification of state changes */
struct FNativeStateBinding
{
	/** State machine to bind to */
	FName MachineName;

	/** State to bind to */
	FName StateName;

	/** Delegate to use when checking transition */
	FOnGraphStateChanged NativeStateDelegate;

#if WITH_EDITORONLY_DATA
	/** Name of this binding */
	FName BindingName;
#endif

	FNativeStateBinding(const FName& InMachineName, const FName& InStateName, const FOnGraphStateChanged& InNativeStateDelegate, const FName& InBindingName = NAME_None)
		: MachineName(InMachineName)
		, StateName(InStateName)
		, NativeStateDelegate(InNativeStateDelegate)
#if WITH_EDITORONLY_DATA
		, BindingName(InBindingName)
#endif
	{
	}
};

/** Tracks state of active slot nodes in the graph */
struct FMontageActiveSlotTracker
{
	/** Local weight of Montages being played (local to the slot node) */
	float MontageLocalWeight;

	/** Global weight of this slot node */
	float NodeGlobalWeight;

	//Is the montage slot part of the active graph this tick
	bool  bIsRelevantThisTick;

	//Was the montage slot part of the active graph last tick
	bool  bWasRelevantOnPreviousTick;

	FMontageActiveSlotTracker()
		: MontageLocalWeight(0.f)
		, NodeGlobalWeight(0.f)
		, bIsRelevantThisTick(false)
		, bWasRelevantOnPreviousTick(false) 
	{}
};

struct FMontageEvaluationState
{
	FMontageEvaluationState(UAnimMontage* InMontage, float InPosition, FDeltaTimeRecord InDeltaTimeRecord, bool bInIsPlaying, bool bInIsActive, const FAlphaBlend& InBlendInfo, const UBlendProfile* InActiveBlendProfile, float InBlendStartAlpha)
		: Montage(InMontage)
		, BlendInfo(InBlendInfo)
		, ActiveBlendProfile(InActiveBlendProfile)
		, MontagePosition(InPosition)
		, DeltaTimeRecord(InDeltaTimeRecord)
		, BlendStartAlpha(InBlendStartAlpha)
		, bIsPlaying(bInIsPlaying)
		, bIsActive(bInIsActive)
	{
	}

	// The montage to evaluate
	TWeakObjectPtr<UAnimMontage> Montage;

	// The current blend information.
	FAlphaBlend BlendInfo;

	// The active blend profile. Montages have a profile for blending in and blending out.
	const UBlendProfile* ActiveBlendProfile;

	// The position to evaluate this montage at
	float MontagePosition;
	
	// The previous MontagePosition and delta leading into current
	FDeltaTimeRecord DeltaTimeRecord;

	// The linear alpha value where to start blending from. So not the blended value that already has been curve sampled.
	float BlendStartAlpha;

	// Whether this montage is playing
	bool bIsPlaying;

	// Whether this montage is valid and not stopped
	bool bIsActive;
};

UCLASS(transient, Blueprintable, hideCategories=AnimInstance, BlueprintType, Within=SkeletalMeshComponent)
class ENGINE_API UAnimInstance : public UObject
{
	GENERATED_UCLASS_BODY()

	typedef FAnimInstanceProxy ProxyType;

	// Disable compiler-generated deprecation warnings by implementing our own destructor
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	~UAnimInstance() {}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	/** This is used to extract animation. If Mesh exists, this will be overwritten by Mesh->Skeleton */
	UPROPERTY(transient)
	TObjectPtr<USkeleton> CurrentSkeleton;

	// Sets where this blueprint pulls Root Motion from
	UPROPERTY(Category = RootMotion, EditDefaultsOnly)
	TEnumAsByte<ERootMotionMode::Type> RootMotionMode;

	/**
	 * Allows this anim instance to update its native update, blend tree, montages and asset players on
	 * a worker thread. This flag is propagated from the UAnimBlueprint to this instance by the compiler.
	 * The compiler will attempt to pick up any issues that may occur with threaded update.
	 * For updates to run in multiple threads both this flag and the project setting "Allow Multi Threaded 
	 * Animation Update" should be set.
	 */
	UPROPERTY(meta=(BlueprintCompilerGeneratedDefaults))
	uint8 bUseMultiThreadedAnimationUpdate : 1;

	/** If this AnimInstance has nodes using 'CopyPoseFromMesh' this will be true. */
	UPROPERTY(meta = (BlueprintCompilerGeneratedDefaults))
	uint8 bUsingCopyPoseFromMesh : 1;

	/** Flag to check back on the game thread that indicates we need to run PostUpdateAnimation() in the post-eval call */
	uint8 bNeedsUpdate : 1;

	/** Flag to check if created by LinkedAnimGraph in ReinitializeLinkedAnimInstance */
	uint8 bCreatedByLinkedAnimGraph : 1;

	/** Whether to process notifies from any linked anim instances */
	UPROPERTY(EditDefaultsOnly, Category = Notifies)
	uint8 bReceiveNotifiesFromLinkedInstances : 1;

	/** Whether to propagate notifies to any linked anim instances */
	UPROPERTY(EditDefaultsOnly, Category = Notifies)
	uint8 bPropagateNotifiesToLinkedInstances : 1;

	/** If true, linked instances will use the main instance's montage data. (i.e. playing a montage on a main instance will play it on the linked layer too.) */
	UPROPERTY(EditDefaultsOnly, Category = "Montage")
	uint8 bUseMainInstanceMontageEvaluationData: 1;

private:
	/** True when Montages are being ticked, and Montage Events should be queued. 
	 * When Montage are being ticked, we queue AnimNotifies and Events. We trigger notifies first, then Montage events. */
	UPROPERTY(Transient)
	uint8 bQueueMontageEvents : 1;

#if DO_CHECK
	/** Guard flag used for checking whether we are in user callbacks for initialization */
	uint8 bInitializing : 1;

	/** Used to guard against recursive calls to UpdateAnimation */
	bool bPostUpdatingAnimation;

	/** Used to guard against recursive calls to UpdateAnimation */
	bool bUpdatingAnimation;
#endif

public:

	// @todo document
	void MakeMontageTickRecord(FAnimTickRecord& TickRecord, class UAnimMontage* Montage, float MoveDelta, float Weight, TArray<FPassedMarker>& MarkersPassedThisTick, FMarkerTickRecord& MarkerTickRecord);

	/** Get global weight in AnimGraph for this slot node.
	* Note: this is the weight of the node, not the weight of any potential montage it is playing. */
	float GetSlotNodeGlobalWeight(const FName& SlotNodeName) const;

	// Should Extract Root Motion or not. Return true if we do. 
	bool ShouldExtractRootMotion() const { return RootMotionMode == ERootMotionMode::RootMotionFromEverything || RootMotionMode == ERootMotionMode::IgnoreRootMotion; }

	/** Get Global weight of any montages this slot node is playing.
	* If this slot is not currently playing a montage, it will return 0. */
	float GetSlotMontageGlobalWeight(const FName& SlotNodeName) const;

	/** Get local weight of any montages this slot node is playing.
	* If this slot is not currently playing a montage, it will return 0.
	* This is double buffered, will return last frame data if called from Update or Evaluate. */
	float GetSlotMontageLocalWeight(const FName& SlotNodeName) const;

	/** Get local weight of any montages this slot is playing.
	* If this slot is not current playing a montage, it will return 0.
	* This will return up to date data if called during Update or Evaluate. */
	float CalcSlotMontageLocalWeight(const FName& SlotNodeName) const;

	// kismet event functions

	UFUNCTION(BlueprintCallable, Category = "Animation") 
	virtual APawn* TryGetPawnOwner() const;

	/** 
	 * Takes a snapshot of the current skeletal mesh component pose & saves it internally.
	 * This snapshot can then be retrieved by name in the animation blueprint for blending. 
	 * The snapshot is taken at the current LOD, so if for example you took the snapshot at LOD1 and then used it at LOD0 any bones not in LOD1 will use the reference pose 
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|Pose")
	virtual void SavePoseSnapshot(FName SnapshotName);

	/** Add an empty pose snapshot to the internal snapshot cache (or recycle an existing pose snapshot if the name is already in use) */
	FPoseSnapshot& AddPoseSnapshot(FName SnapshotName);

	/** Remove a previously saved pose snapshot from the internal snapshot cache */
	UFUNCTION(BlueprintCallable, Category = "Animation|Pose")
	void RemovePoseSnapshot(FName SnapshotName);

	/** Get a cached pose snapshot by name */
	const FPoseSnapshot* GetPoseSnapshot(FName SnapshotName) const;

	/**
	 * Takes a snapshot of the current skeletal mesh component pose and saves it to the specified snapshot.
	 * The snapshot is taken at the current LOD, so if for example you took the snapshot at LOD1 
	 * and then used it at LOD0 any bones not in LOD1 will use the reference pose 
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|Pose")
	virtual void SnapshotPose(UPARAM(ref) FPoseSnapshot& Snapshot);

	// Can this animation instance run Update or Evaluation work in parallel
	virtual bool CanRunParallelWork() const { return true; }

	// Are we being evaluated on a worker thread
	bool IsRunningParallelEvaluation() const;

	// Can does this anim instance need an update (parallel or not)?
	bool NeedsUpdate() const;

	/** Get whether to process notifies from any linked anim instances */
	UFUNCTION(BlueprintPure, Category = "Animation|Notifies")
	bool GetReceiveNotifiesFromLinkedInstances() const { return bReceiveNotifiesFromLinkedInstances; }

	/** Set whether to process notifies from any linked anim instances */
	UFUNCTION(BlueprintCallable, Category = "Animation|Notifies")
	void SetReceiveNotifiesFromLinkedInstances(bool bSet) { bReceiveNotifiesFromLinkedInstances = bSet; }

	/** Get whether to propagate notifies to any linked anim instances */
	UFUNCTION(BlueprintPure, Category = "Animation|Notifies")
	bool GetPropagateNotifiesToLinkedInstances() const { return bPropagateNotifiesToLinkedInstances; }

	/** Set whether to propagate notifies to any linked anim instances */
	UFUNCTION(BlueprintCallable, Category = "Animation|Notifies")
	void SetPropagateNotifiesToLinkedInstances(bool bSet) { bPropagateNotifiesToLinkedInstances = bSet; }

	UFUNCTION(BlueprintCallable, Category = "Animation|Linked Anim Graphs|Montage")
	bool IsUsingMainInstanceMontageEvaluationData() const { return bUseMainInstanceMontageEvaluationData; }

	UFUNCTION(BlueprintCallable, Category = "Animation|Linked Anim Graphs|Montage")
	void SetUseMainInstanceMontageEvaluationData(bool bSet) { bUseMainInstanceMontageEvaluationData = bSet; }

private:
	// Does this anim instance need immediate update (rather than parallel)?
	bool NeedsImmediateUpdate(float DeltaSeconds, bool bNeedsValidRootMotion) const;

public:
	/** Returns the owning actor of this AnimInstance */
	UFUNCTION(BlueprintCallable, Category = "Animation")
	AActor* GetOwningActor() const;
	
	// Returns the skeletal mesh component that has created this AnimInstance
	UFUNCTION(BlueprintCallable, Category = "Animation")
	USkeletalMeshComponent* GetOwningComponent() const;

public:

	/** Executed when the Animation is initialized */
	UFUNCTION(BlueprintImplementableEvent)
	void BlueprintInitializeAnimation();

	/** Executed when the Animation is updated */
	UFUNCTION(BlueprintImplementableEvent)
	void BlueprintUpdateAnimation(float DeltaTimeX);

	/** Executed after the Animation is evaluated */
	UFUNCTION(BlueprintImplementableEvent)
	void BlueprintPostEvaluateAnimation();

	/** Executed when begin play is called on the owning component */
	UFUNCTION(BlueprintImplementableEvent)
	void BlueprintBeginPlay();

	/** Executed when the all Linked Animation Layers are initialized */
	UFUNCTION(BlueprintImplementableEvent)
	void BlueprintLinkedAnimationLayersInitialized();

	/** Executed when the Animation Blueprint is updated on a worker thread, just prior to graph update */
	UFUNCTION(BlueprintImplementableEvent, meta=(BlueprintThreadSafe))
	void BlueprintThreadSafeUpdateAnimation(float DeltaTime);
	
	bool CanTransitionSignature() const;
	
	/*********************************************************************************************
	* SlotAnimation
	********************************************************************************************* */
public:

	/** Play normal animation asset on the slot node by creating a dynamic UAnimMontage. You can only play one asset (whether montage or animsequence) at a time per SlotGroup. */
	UFUNCTION(BlueprintCallable, Category="Animation|Montage")
	UAnimMontage* PlaySlotAnimationAsDynamicMontage(UAnimSequenceBase* Asset, FName SlotNodeName, float BlendInTime = 0.25f, float BlendOutTime = 0.25f, float InPlayRate = 1.f, int32 LoopCount = 1, float BlendOutTriggerTime = -1.f, float InTimeToStartMontageAt = 0.f);

	/** Play normal animation asset on the slot node by creating a dynamic UAnimMontage with blend in arguments. You can only play one asset (whether montage or animsequence) at a time per SlotGroup. */
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	UAnimMontage* PlaySlotAnimationAsDynamicMontage_WithBlendArgs(UAnimSequenceBase* Asset, FName SlotNodeName, const FAlphaBlendArgs& BlendIn, const FAlphaBlendArgs& BlendOut, float InPlayRate = 1.f, int32 LoopCount = 1, float BlendOutTriggerTime = -1.f, float InTimeToStartMontageAt = 0.f);

	/** Play normal animation asset on the slot node by creating a dynamic UAnimMontage with blend in settings. You can only play one asset (whether montage or animsequence) at a time per SlotGroup. */
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	UAnimMontage* PlaySlotAnimationAsDynamicMontage_WithBlendSettings(UAnimSequenceBase* Asset, FName SlotNodeName, const FMontageBlendSettings& BlendInSettings, const FMontageBlendSettings& BlendOutSettings, float InPlayRate = 1.f, int32 LoopCount = 1, float BlendOutTriggerTime = -1.f, float InTimeToStartMontageAt = 0.f);

	/** Stops currently playing slot animation slot or all*/
	UFUNCTION(BlueprintCallable, Category="Animation|Montage")
	void StopSlotAnimation(float InBlendOutTime = 0.25f, FName SlotNodeName = NAME_None);

	/** Return true if it's playing the slot animation */
	UFUNCTION(BlueprintPure, Category="Animation|Montage")
	bool IsPlayingSlotAnimation(const UAnimSequenceBase* Asset, FName SlotNodeName) const;

	/** Return true if this instance playing the slot animation, also returning the montage it is playing on */
	bool IsPlayingSlotAnimation(const UAnimSequenceBase* Asset, FName SlotNodeName, UAnimMontage*& OutMontage) const;

	/*********************************************************************************************
	 * AnimMontage
	 ********************************************************************************************* */
public:
	/** Plays an animation montage. Returns the length of the animation montage in seconds. Returns 0.f if failed to play. */
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	float Montage_Play(UAnimMontage* MontageToPlay, float InPlayRate = 1.f, EMontagePlayReturnType ReturnValueType = EMontagePlayReturnType::MontageLength, float InTimeToStartMontageAt=0.f, bool bStopAllMontages = true);

	/** Plays an animation montage. Same as Montage_Play, but you can specify an AlphaBlend for Blend In settings. */
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	float Montage_PlayWithBlendIn(UAnimMontage* MontageToPlay, const FAlphaBlendArgs& BlendIn, float InPlayRate = 1.f, EMontagePlayReturnType ReturnValueType = EMontagePlayReturnType::MontageLength, float InTimeToStartMontageAt=0.f, bool bStopAllMontages = true);

	/** Plays an animation montage. Same as Montage_Play, but you can overwrite all of the montage's default blend in settings. */
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	float Montage_PlayWithBlendSettings(UAnimMontage* MontageToPlay, const FMontageBlendSettings& BlendInSettings, float InPlayRate = 1.f, EMontagePlayReturnType ReturnValueType = EMontagePlayReturnType::MontageLength, float InTimeToStartMontageAt=0.f, bool bStopAllMontages = true);

	/** Stops the animation montage. If reference is NULL, it will stop ALL active montages. */
	/** Stopped montages will blend out using their montage asset's BlendOut, with InBlendOutTime as the BlendTime */
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	void Montage_Stop(float InBlendOutTime, const UAnimMontage* Montage = NULL);

	/** Same as Montage_Stop. Uses values from the AlphaBlendArgs. Other settings come from the montage asset*/
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	void Montage_StopWithBlendOut(const FAlphaBlendArgs& BlendOut, const UAnimMontage* Montage = nullptr);

	/** Same as Montage_Stop, but all blend settings are provided instead of using the ones on the montage asset*/
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	void Montage_StopWithBlendSettings(const FMontageBlendSettings& BlendOutSettings, const UAnimMontage* Montage = nullptr);

	/** Stops all active montages belonging to a group. */
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	void Montage_StopGroupByName(float InBlendOutTime, FName GroupName);

	/** Pauses the animation montage. If reference is NULL, it will pause ALL active montages. */
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	void Montage_Pause(const UAnimMontage* Montage = NULL);

	/** Resumes a paused animation montage. If reference is NULL, it will resume ALL active montages. */
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	void Montage_Resume(const UAnimMontage* Montage);

	/** Makes a montage jump to a named section. If Montage reference is NULL, it will do that to all active montages. */
	UFUNCTION(BlueprintCallable, Category="Animation|Montage")
	void Montage_JumpToSection(FName SectionName, const UAnimMontage* Montage = NULL);

	/** Makes a montage jump to the end of a named section. If Montage reference is NULL, it will do that to all active montages. */
	UFUNCTION(BlueprintCallable, Category="Animation|Montage")
	void Montage_JumpToSectionsEnd(FName SectionName, const UAnimMontage* Montage = NULL);

	/** Relink new next section AFTER SectionNameToChange in run-time
	 *	You can link section order the way you like in editor, but in run-time if you'd like to change it dynamically, 
	 *	use this function to relink the next section
	 *	For example, you can have Start->Loop->Loop->Loop.... but when you want it to end, you can relink
	 *	next section of Loop to be End to finish the montage, in which case, it stops looping by Loop->End. 
	 
	 * @param SectionNameToChange : This should be the name of the Montage Section after which you want to insert a new next section
	 * @param NextSection	: new next section 
	 */
	UFUNCTION(BlueprintCallable, Category="Animation|Montage")
	void Montage_SetNextSection(FName SectionNameToChange, FName NextSection, const UAnimMontage* Montage = NULL);

	/** Change AnimMontage play rate. NewPlayRate = 1.0 is the default playback rate. */
	UFUNCTION(BlueprintCallable, Category="Animation|Montage")
	void Montage_SetPlayRate(const UAnimMontage* Montage, float NewPlayRate = 1.f);

	/** Returns true if the animation montage is active. If the Montage reference is NULL, it will return true if any Montage is active. */
	UFUNCTION(BlueprintPure, Category="Animation|Montage")
	bool Montage_IsActive(const UAnimMontage* Montage) const;

	/** Returns true if the animation montage is currently active and playing. 
	If reference is NULL, it will return true is ANY montage is currently active and playing. */
	UFUNCTION(BlueprintPure, Category="Animation|Montage")
	bool Montage_IsPlaying(const UAnimMontage* Montage) const;

	/** Returns the name of the current animation montage section. */
	UFUNCTION(BlueprintPure, Category="Animation|Montage")
	FName Montage_GetCurrentSection(const UAnimMontage* Montage = NULL) const;

	/** Get Current Montage Position */
	UFUNCTION(BlueprintPure, Category = "Animation|Montage")
	float Montage_GetPosition(const UAnimMontage* Montage) const;
	
	/** Set position. */
	UFUNCTION(BlueprintCallable, Category = "Animation|Montage")
	void Montage_SetPosition(const UAnimMontage* Montage, float NewPosition);
	
	/** return true if Montage is not currently active. (not valid or blending out) */
	UFUNCTION(BlueprintPure, Category = "Animation|Montage")
	bool Montage_GetIsStopped(const UAnimMontage* Montage) const;

	/** Get the current blend time of the Montage.
	If Montage reference is NULL, it will return the current blend time on the first active Montage found. */
	UFUNCTION(BlueprintPure, Category = "Animation|Montage")
	float Montage_GetBlendTime(const UAnimMontage* Montage) const;

	/** Get PlayRate for Montage.
	If Montage reference is NULL, PlayRate for any Active Montage will be returned.
	If Montage is not playing, 0 is returned. */
	UFUNCTION(BlueprintPure, Category = "Animation|Montage")
	float Montage_GetPlayRate(const UAnimMontage* Montage) const;

	/*********************************************************************************************
	* AnimMontage sync. See notes in AnimMontage.h
	********************************************************************************************* */

	/** Synchronize a montage to another anim instance's montage. Both montages must be playing already
	* @param MontageFollower : The montage that will follow the leader in OtherAnimInstance
	* @param OtherAnimInstance	: The other anim instance we want to synchronize to. Can be set to self
	* @param MontageLeader	: The montage we want to follow in the other anim instance */
	UFUNCTION(BlueprintCallable, Category = "Montage")
	void MontageSync_Follow(const UAnimMontage* MontageFollower, const UAnimInstance* OtherAnimInstance, const UAnimMontage* MontageLeader);

	/** Stop following the montage's leader in this anim instance
	* @param MontageFollower : The montage we want to stop synchronizing */
	UFUNCTION(BlueprintCallable, Category = "Montage")
	void MontageSync_StopFollowing(const UAnimMontage* MontageFollower);

	/** Returns true if any montage is playing currently. Doesn't mean it's active though, it could be blending out. */
	UFUNCTION(BlueprintPure, Category = "Animation|Montage")
	bool IsAnyMontagePlaying() const;

	/** Get a current Active Montage in this AnimInstance. 
		Note that there might be multiple Active at the same time. This will only return the first active one it finds. **/
	UFUNCTION(BlueprintPure, Category = "Animation|Montage")
	UAnimMontage* GetCurrentActiveMontage() const;

	/** Called when a montage starts blending out, whether interrupted or finished */
	UPROPERTY(BlueprintAssignable)
	FOnMontageBlendingOutStartedMCDelegate OnMontageBlendingOut;
	
	/** Called when a montage has started */
	UPROPERTY(BlueprintAssignable)
	FOnMontageStartedMCDelegate OnMontageStarted;

	/** Called when a montage has ended, whether interrupted or finished*/
	UPROPERTY(BlueprintAssignable)
	FOnMontageEndedMCDelegate OnMontageEnded;

	/** Called when all Montage instances have ended. */
	UPROPERTY(BlueprintAssignable)
	FOnAllMontageInstancesEndedMCDelegate OnAllMontageInstancesEnded;

	/*********************************************************************************************
	* AnimMontage native C++ interface
	********************************************************************************************* */
public:	
	void Montage_SetEndDelegate(FOnMontageEnded & InOnMontageEnded, UAnimMontage* Montage = NULL);
	
	void Montage_SetBlendingOutDelegate(FOnMontageBlendingOutStarted & InOnMontageBlendingOut, UAnimMontage* Montage = NULL);
	
	/** Get pointer to BlendingOutStarted delegate for Montage.
	If Montage reference is NULL, it will pick the first active montage found. */
	FOnMontageBlendingOutStarted* Montage_GetBlendingOutDelegate(UAnimMontage* Montage = NULL);

	/** Get next sectionID for given section ID */
	int32 Montage_GetNextSectionID(const UAnimMontage* Montage, int32 const & CurrentSectionID) const;

	/** Get Currently active montage instance.
		Note that there might be multiple Active at the same time. This will only return the first active one it finds. **/
	FAnimMontageInstance* GetActiveMontageInstance() const;

	/** Get Active FAnimMontageInstance for given Montage asset. Will return NULL if Montage is not currently Active. */
	FAnimMontageInstance* GetActiveInstanceForMontage(const UAnimMontage* Montage) const;

	/** Get the FAnimMontageInstance currently running that matches this ID.  Will return NULL if no instance is found. */
	FAnimMontageInstance* GetMontageInstanceForID(int32 MontageInstanceID);

	/** Stop all montages that are active **/
	void StopAllMontages(float BlendOut);

	/** AnimMontage instances that are running currently
	* - only one is primarily active per group, and the other ones are blending out
	*/
	TArray<struct FAnimMontageInstance*> MontageInstances;

	virtual void OnMontageInstanceStopped(FAnimMontageInstance & StoppedMontageInstance);
	void ClearMontageInstanceReferences(FAnimMontageInstance& InMontageInstance);

	UE_DEPRECATED(4.24, "Function renamed, please use GetLinkedInputPoseNode")
	FAnimNode_LinkedInputPose* GetSubInputNode(FName InSubInput = NAME_None, FName InGraph = NAME_None) { return GetLinkedInputPoseNode(InSubInput, InGraph); }

	/** 
	 * Get a linked input pose node by name, given a named graph.
	 * @param	InSubInput	The name of the linked input pose. If this is NAME_None, then we assume that the desired input is FAnimNode_LinkedInputPose::DefaultInputPoseName.
	 * @param	InGraph		The name of the graph in which to find the linked input. If this is NAME_None, then we assume that the desired graph is "AnimGraph", the default.
	 */
	FAnimNode_LinkedInputPose* GetLinkedInputPoseNode(FName InSubInput = NAME_None, FName InGraph = NAME_None);

	UE_DEPRECATED(4.24, "Function renamed, please use GetLinkedAnimGraphInstanceByTag")
	UAnimInstance* GetSubInstanceByTag(FName InTag) const { return GetLinkedAnimGraphInstanceByTag(InTag); }

	/** Runs through all nodes, attempting to find the first linked instance by name/tag */
	UFUNCTION(BlueprintPure, Category = "Animation|Linked Anim Graphs")
	UAnimInstance* GetLinkedAnimGraphInstanceByTag(FName InTag) const;

	UE_DEPRECATED(4.24, "Function renamed, please use GetLinkedAnimGraphInstancesByTag")
	void GetSubInstancesByTag(FName InTag, TArray<UAnimInstance*>& OutSubInstances) const;

	/** Runs through all nodes, attempting to find all linked instances that match the name/tag */
	UE_DEPRECATED(5.0, "Tags are unique so this function is no longer supported. Please use GetLinkedAnimGraphInstanceByTag instead")
	UFUNCTION(BlueprintPure, Category = "Animation|Linked Anim Graphs", meta=(DeprecatedFunction, DeprecationMessage="Tags are unique so this function is no longer supported. Please use GetLinkedAnimGraphInstanceByTag instead"))
	void GetLinkedAnimGraphInstancesByTag(FName InTag, TArray<UAnimInstance*>& OutLinkedInstances) const;

	UE_DEPRECATED(4.24, "Function renamed, please use LinkAnimGraphByTag")
	void SetSubInstanceClassByTag(FName InTag, TSubclassOf<UAnimInstance> InClass) { LinkAnimGraphByTag(InTag, InClass); }

	/** Runs through all nodes, attempting to find a linked instance by name/tag, then sets the class of each node if the tag matches */
	UFUNCTION(BlueprintCallable, Category = "Animation|Linked Anim Graphs")
	void LinkAnimGraphByTag(FName InTag, TSubclassOf<UAnimInstance> InClass);

	UE_DEPRECATED(4.24, "Function renamed, please use LinkAnimClassLayers")
	void SetLayerOverlay(TSubclassOf<UAnimInstance> InClass) { LinkAnimClassLayers(InClass); }

	/**
	 * Runs through all layer nodes, attempting to find layer nodes that are implemented by the specified class, then sets up a linked instance of the class for each.
	 * Allocates one linked instance to run each of the groups specified in the class, so state is shared. If a layer is not grouped (ie. NAME_None), then state is not shared
	 * and a separate linked instance is allocated for each layer node.
	 * If InClass is null, then all layers are reset to their defaults.
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|Linked Anim Graphs")
	virtual void LinkAnimClassLayers(TSubclassOf<UAnimInstance> InClass);

	UE_DEPRECATED(4.24, "Function renamed, please use UnlinkAnimClassLayers")
	void ClearLayerOverlay(TSubclassOf<UAnimInstance> InClass) { UnlinkAnimClassLayers(InClass); }

	/**
	 * Runs through all layer nodes, attempting to find layer nodes that are currently running the specified class, then resets each to its default value.
	 * State sharing rules are as with SetLayerOverlay.
	 * If InClass is null, does nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Animation|Linked Anim Graphs")
	virtual void UnlinkAnimClassLayers(TSubclassOf<UAnimInstance> InClass);

	UE_DEPRECATED(4.24, "Function renamed, please use GetLinkedAnimLayerInstanceByGroup")
	UAnimInstance* GetLayerSubInstanceByGroup(FName InGroup) const { return GetLinkedAnimLayerInstanceByGroup(InGroup); }

	/** Gets the layer linked instance corresponding to the specified group */
	UFUNCTION(BlueprintPure, Category = "Animation|Linked Anim Graphs")
	UAnimInstance* GetLinkedAnimLayerInstanceByGroup(FName InGroup) const;

	/** Runs through all nodes, attempting to find all distinct layer linked instances in the group */
	UFUNCTION(BlueprintPure, Category = "Animation|Linked Anim Graphs")
	void GetLinkedAnimLayerInstancesByGroup(FName InGroup, TArray<UAnimInstance*>& OutLinkedInstances) const;

	/** Gets layer linked instance that matches group and class */
	UFUNCTION(BlueprintPure, Category = "Animation|Linked Anim Graphs")
	UAnimInstance* GetLinkedAnimLayerInstanceByGroupAndClass(FName InGroup, TSubclassOf<UAnimInstance> InClass) const;

	UE_DEPRECATED(4.24, "Function renamed, please use GetLinkedAnimLayerInstanceByClass")
	UAnimInstance* GetLayerSubInstanceByClass(TSubclassOf<UAnimInstance> InClass) const { return GetLinkedAnimLayerInstanceByClass(InClass); }

	/** Gets the first layer linked instance corresponding to the specified class */
	UFUNCTION(BlueprintPure, Category = "Animation|Linked Anim Graphs")
	UAnimInstance* GetLinkedAnimLayerInstanceByClass(TSubclassOf<UAnimInstance> InClass) const;

	/** Sets up initial layer groupings */
	void InitializeGroupedLayers(bool bInDeferSubGraphInitialization);

	/** Allows other UObjects to bind custom event notifies similarly to the AnimBP */
	void AddExternalNotifyHandler(UObject* ExternalHandlerObject, FName NotifyEventName);
	/** Other UObjects should call this to remove themselves from the callbacks */
	void RemoveExternalNotifyHandler(UObject* ExternalHandlerObject, FName NotifyEventName);

	// Find a subsystem's instance-resident data. If no subsystem of the type exists this will return nullptr.
	// @param	InSubsystemType	The subsystem's type
	FAnimSubsystemInstance* FindSubsystem(UScriptStruct* InSubsystemType);

	// Get a subsystem's instance-resident data. If no subsystem of the type exists this will return nullptr.
	template<typename SubsystemType>
	SubsystemType* FindSubsystem()
	{
		FAnimSubsystemInstance* Subsystem = FindSubsystem(SubsystemType::StaticStruct());
		return static_cast<SubsystemType*>(Subsystem);
	}
	
	// Get a subsystem's instance-resident data. If no subsystem of the type exists this will assert.
	template<typename SubsystemType>
	SubsystemType& GetSubsystem()
	{
		FAnimSubsystemInstance* Subsystem = FindSubsystem(SubsystemType::StaticStruct());
		check(Subsystem);
		return static_cast<SubsystemType&>(*Subsystem);
	}

private:
	/** Helper function to perform layer overlay actions (set, clear) */
	void PerformLinkedLayerOverlayOperation(TSubclassOf<UAnimInstance> InClass, TFunctionRef<UClass*(UClass*, FAnimNode_LinkedAnimLayer*)> InClassSelectorFunction, bool bInDeferSubGraphInitialization = false);

protected:
	/** Map between Active Montages and their FAnimMontageInstance */
	TMap<class UAnimMontage*, struct FAnimMontageInstance*> ActiveMontagesMap;

	/**  Inertialization requests gathered this frame. Gets reset in UpdateMontageEvaluationData */
	TMap<FName, UE::Anim::FSlotInertializationRequest> SlotGroupInertializationRequestMap;

	/* StopAllMontagesByGroupName needs a BlendMode and BlendProfile to function properly if using non-default ones in your montages. If you want default BlendMode/BlendProfiles, you need to update the calling code to do so. */
	UE_DEPRECATED(5.0, "Use StopAllMontagesByGroupName with other signature.")
	void StopAllMontagesByGroupName(FName InGroupName, const FAlphaBlend& BlendOut);

	/** Stop all active montages belonging to 'InGroupName' */
	void StopAllMontagesByGroupName(FName InGroupName, const FMontageBlendSettings& BlendOutSettings);

	/** Update weight of montages  **/
	virtual void Montage_UpdateWeight(float DeltaSeconds);
	/** Advance montages **/
	virtual void Montage_Advance(float DeltaSeconds);

	void Montage_StopInternal(TFunctionRef<FMontageBlendSettings(const FAnimMontageInstance*)> AlphaBlendSelectorFunction, const UAnimMontage* Montage = nullptr);
	float Montage_PlayInternal(UAnimMontage* MontageToPlay, const FMontageBlendSettings& BlendInSettings, float InPlayRate = 1.f, EMontagePlayReturnType ReturnValueType = EMontagePlayReturnType::MontageLength, float InTimeToStartMontageAt = 0.f, bool bStopAllMontages = true);

public:

	/**  Builds an inertialization request from the montage's group, provided duration and optional blend profile*/
	void RequestMontageInertialization(const UAnimMontage* Montage, float Duration, const UBlendProfile* BlendProfile = nullptr);

	/**  Requests an inertial blend during the next anim graph update. Requires your anim graph to have a slot node belonging to the specified group name */
	UFUNCTION(BlueprintCallable, Category = "Animation|Inertial Blending")
	void RequestSlotGroupInertialization(FName InSlotGroupName, float Duration, const UBlendProfile* BlendProfile = nullptr);

	/** Queue a Montage BlendingOut Event to be triggered. */
	void QueueMontageBlendingOutEvent(const FQueuedMontageBlendingOutEvent& MontageBlendingOutEvent);

	/** Queue a Montage Ended Event to be triggered. */
	void QueueMontageEndedEvent(const FQueuedMontageEndedEvent& MontageEndedEvent);

private:
	/** Trigger queued Montage events. */
	void TriggerQueuedMontageEvents();

	/** Queued Montage BlendingOut events. */
	TArray<FQueuedMontageBlendingOutEvent> QueuedMontageBlendingOutEvents;

	/** Queued Montage Ended Events */
	TArray<FQueuedMontageEndedEvent> QueuedMontageEndedEvents;

	/** Trigger a Montage BlendingOut event */
	void TriggerMontageBlendingOutEvent(const FQueuedMontageBlendingOutEvent& MontageBlendingOutEvent);

	/** Trigger a Montage Ended event */
	void TriggerMontageEndedEvent(const FQueuedMontageEndedEvent& MontageEndedEvent);

public:

#if DO_CHECK
	/** Is this animation currently running post update */
	bool IsPostUpdatingAnimation() const { return bPostUpdatingAnimation; }

	/** Is this animation currently running update */
	bool IsUpdatingAnimation() const { return bUpdatingAnimation; }
#endif

	/** Set RootMotionMode */
	UFUNCTION(BlueprintCallable, Category = "Animation|Root Motion")
	void SetRootMotionMode(TEnumAsByte<ERootMotionMode::Type> Value);

	/** 
	 * NOTE: Derived anim getters
	 *
	 * Anim getter functions can be defined for any instance deriving UAnimInstance.
	 * To do this the function must be marked BlueprintPure, and have the AnimGetter metadata entry set to
	 * "true". Following the instructions below, getters should appear correctly in the blueprint node context
	 * menu for the derived classes
	 *
	 * A context string can be provided in the GetterContext metadata and can contain any (or none) of the
	 * following entries separated by a pipe (|)
	 * Transition  - Only available in a transition rule
	 * AnimGraph   - Only available in an animgraph (also covers state anim graphs)
	 * CustomBlend - Only available in a custom blend graph
	 *
	 * Anim getters support a number of automatic parameters that will be baked at compile time to be passed
	 * to the functions. They will not appear as pins on the graph node. They are as follows:
	 * AssetPlayerIndex - Index of an asset player node to operate on, one getter will be added to the blueprint action list per asset node available
	 * MachineIndex     - Index of a state machine in the animation blueprint, one getter will be added to the blueprint action list per state machine
	 * StateIndex       - Index of a state inside a state machine, also requires MachineIndex. One getter will be added to the blueprint action list per state
	 * TransitionIndex  - Index of a transition inside a state machine, also requires MachineIndex. One getter will be added to the blueprint action list per transition
	 */

	/** Gets the length in seconds of the asset referenced in an asset player node */
	UFUNCTION(BlueprintPure, Category="Animation|Asset Player", meta=(DisplayName="Length", BlueprintInternalUseOnly="true", AnimGetter="true", BlueprintThreadSafe))
	float GetInstanceAssetPlayerLength(int32 AssetPlayerIndex);

	/** Get the current accumulated time in seconds for an asset player node */
	UFUNCTION(BlueprintPure, Category="Animation|Asset Player", meta = (DisplayName = "Current Time", BlueprintInternalUseOnly = "true", AnimGetter = "true", BlueprintThreadSafe))
	float GetInstanceAssetPlayerTime(int32 AssetPlayerIndex);

	/** Get the current accumulated time as a fraction for an asset player node */
	UFUNCTION(BlueprintPure, Category="Animation|Asset Player", meta=(DisplayName="Current Time (ratio)", BlueprintInternalUseOnly="true", AnimGetter="true", BlueprintThreadSafe))
	float GetInstanceAssetPlayerTimeFraction(int32 AssetPlayerIndex);

	/** Get the time in seconds from the end of an animation in an asset player node */
	UFUNCTION(BlueprintPure, Category="Animation|Asset Player", meta=(DisplayName="Time Remaining", BlueprintInternalUseOnly="true", AnimGetter="true", BlueprintThreadSafe))
	float GetInstanceAssetPlayerTimeFromEnd(int32 AssetPlayerIndex);

	/** Get the time as a fraction of the asset length of an animation in an asset player node */
	UFUNCTION(BlueprintPure, Category="Animation|Asset Player", meta=(DisplayName="Time Remaining (ratio)", BlueprintInternalUseOnly="true", AnimGetter="true", BlueprintThreadSafe))
	float GetInstanceAssetPlayerTimeFromEndFraction(int32 AssetPlayerIndex);

	/** Get the blend weight of a specified state machine */
	UFUNCTION(BlueprintPure, Category = "Animation|State Machines", meta = (DisplayName = "Machine Weight", BlueprintInternalUseOnly = "true", AnimGetter = "true", BlueprintThreadSafe))
	float GetInstanceMachineWeight(int32 MachineIndex);

	/** Get the blend weight of a specified state */
	UFUNCTION(BlueprintPure, Category="Animation|State Machines", meta = (DisplayName="State Weight", BlueprintInternalUseOnly = "true", AnimGetter="true", BlueprintThreadSafe))
	float GetInstanceStateWeight(int32 MachineIndex, int32 StateIndex);

	/** Get the current elapsed time of a state within the specified state machine */
	UFUNCTION(BlueprintPure, Category="Animation|State Machines", meta = (DisplayName="Current State Time", BlueprintInternalUseOnly = "true", AnimGetter="true", GetterContext="Transition", BlueprintThreadSafe))
	float GetInstanceCurrentStateElapsedTime(int32 MachineIndex);

	/** Get the crossfade duration of a specified transition */
	UFUNCTION(BlueprintPure, Category="Animation|State Machines", meta = (DisplayName="Get Transition Crossfade Duration", BlueprintInternalUseOnly = "true", AnimGetter="true", BlueprintThreadSafe))
	float GetInstanceTransitionCrossfadeDuration(int32 MachineIndex, int32 TransitionIndex);

	/** Get the elapsed time in seconds of a specified transition */
	UFUNCTION(BlueprintPure, Category="Animation|State Machines", meta = (DisplayName="Get Transition Time Elapsed", BlueprintInternalUseOnly = "true", AnimGetter="true", GetterContext="CustomBlend", BlueprintThreadSafe))
	float GetInstanceTransitionTimeElapsed(int32 MachineIndex, int32 TransitionIndex);

	/** Get the elapsed time as a fraction of the crossfade duration of a specified transition */
	UFUNCTION(BlueprintPure, Category="Animation|State Machines", meta = (DisplayName="Get Transition Time Elapsed (ratio)", BlueprintInternalUseOnly = "true", AnimGetter="true", GetterContext="CustomBlend", BlueprintThreadSafe))
	float GetInstanceTransitionTimeElapsedFraction(int32 MachineIndex, int32 TransitionIndex);

	/** Get the time remaining in seconds for the most relevant animation in the source state */
	UFUNCTION(BlueprintPure, Category="Animation|State Machines", meta = (BlueprintInternalUseOnly = "true", AnimGetter="true", GetterContext="Transition", BlueprintThreadSafe))
	float GetRelevantAnimTimeRemaining(int32 MachineIndex, int32 StateIndex);

	/** Get the time remaining as a fraction of the duration for the most relevant animation in the source state */
	UFUNCTION(BlueprintPure, Category = "Animation|State Machines", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition", BlueprintThreadSafe))
	float GetRelevantAnimTimeRemainingFraction(int32 MachineIndex, int32 StateIndex);

	/** Get the length in seconds of the most relevant animation in the source state */
	UFUNCTION(BlueprintPure, Category = "Animation|State Machines", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition", BlueprintThreadSafe))
	float GetRelevantAnimLength(int32 MachineIndex, int32 StateIndex);

	/** Get the current accumulated time in seconds for the most relevant animation in the source state */
	UFUNCTION(BlueprintPure, Category = "Animation|State Machines", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition", BlueprintThreadSafe))
	float GetRelevantAnimTime(int32 MachineIndex, int32 StateIndex);

	/** Get the current accumulated time as a fraction of the length of the most relevant animation in the source state */
	UFUNCTION(BlueprintPure, Category = "Animation|State Machines", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition", BlueprintThreadSafe))
	float GetRelevantAnimTimeFraction(int32 MachineIndex, int32 StateIndex);

	/** Get whether a particular notify state was active in any state machine last tick.*/
	UFUNCTION(BlueprintPure, Category = "Animation|State Machines", meta = (BlueprintThreadSafe, Keywords = "Window,TransitionWindow,NotifyState,NotifyStateTransition"))
	bool WasAnimNotifyStateActiveInAnyState(TSubclassOf<UAnimNotifyState> AnimNotifyStateType);

	/** Get whether a particular notify state is active in a specific state machine last tick.  */
	UFUNCTION(BlueprintPure, Category = "Animation|State Machines", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition", BlueprintThreadSafe, Keywords = "Window,TransitionWindow,NotifyState,NotifyStateTransition"))
	bool WasAnimNotifyStateActiveInStateMachine(int32 MachineIndex, TSubclassOf<UAnimNotifyState> AnimNotifyStateType);

	/** Get whether a particular notify state is active in a specific state last tick. */
	UFUNCTION(BlueprintPure, Category = "Animation|State Machines", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition", BlueprintThreadSafe, Keywords = "Window,TransitionWindow,NotifyState,NotifyStateTransition"))
	bool WasAnimNotifyStateActiveInSourceState(int32 MachineIndex, int32 StateIndex, TSubclassOf<UAnimNotifyState> AnimNotifyStateType);

	/** Get whether the most relevant animation was in a particular notify state last tick. */
	UFUNCTION(BlueprintPure, Category = "Animation|State Machines", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition", BlueprintThreadSafe, Keywords = "Window,TransitionWindow,NotifyState,NotifyStateTransition"))
    bool WasAnimNotifyTriggeredInSourceState(int32 MachineIndex, int32 StateIndex,  TSubclassOf<UAnimNotify> AnimNotifyType);

	/** Get whether the most relevant animation triggered the animation notify with the specified name last tick.. */
	UFUNCTION(BlueprintPure, Category = "Animation|State Machines", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition", BlueprintThreadSafe, Keywords = "Window,TransitionWindow,NotifyState,NotifyStateTransition"))
    bool WasAnimNotifyNameTriggeredInSourceState(int32 MachineIndex, int32 StateIndex, FName NotifyName);

	/** Get whether a particular notify type was active in a specific state machine last tick.  */
	UFUNCTION(BlueprintPure, Category = "Animation|State Machines", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition", BlueprintThreadSafe, Keywords = "Window,TransitionWindow,NotifyState,NotifyStateTransition"))
    bool WasAnimNotifyTriggeredInStateMachine(int32 MachineIndex, TSubclassOf<UAnimNotify> AnimNotifyType);

	/** Get whether the given state machine triggered the animation notify with the specified name last tick. */
	UFUNCTION(BlueprintPure, Category = "Animation|State Machines", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition", BlueprintThreadSafe, Keywords = "Window,TransitionWindow,NotifyState,NotifyStateTransition"))
    bool WasAnimNotifyNameTriggeredInStateMachine(int32 MachineIndex, FName NotifyName);
	
	/**  Get whether an animation notify of a given type was triggered last tick. */
	UFUNCTION(BlueprintPure, Category = "Animation|State Machines", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition", BlueprintThreadSafe, Keywords = "Window,TransitionWindow,NotifyState,NotifyStateTransition"))
    bool WasAnimNotifyTriggeredInAnyState(TSubclassOf<UAnimNotify> AnimNotifyType);

	/** Get whether the animation notify with the specified name triggered last tick. */
	UFUNCTION(BlueprintPure, Category = "Animation|State Machines", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition", BlueprintThreadSafe, Keywords = "Window,TransitionWindow,NotifyState,NotifyStateTransition"))
    bool WasAnimNotifyNameTriggeredInAnyState(FName NotifyName);
	
	/** Gets the runtime instance of the specified state machine by Name */
	const FAnimNode_StateMachine* GetStateMachineInstanceFromName(FName MachineName) const;

	/** Get the machine description for the specified instance. Does not rely on PRIVATE_MachineDescription being initialized */
	const FBakedAnimationStateMachine* GetMachineDescription(IAnimClassInterface* AnimBlueprintClass, FAnimNode_StateMachine* MachineInstance);

	void GetStateMachineIndexAndDescription(FName InMachineName, int32& OutMachineIndex, const FBakedAnimationStateMachine** OutMachineDescription);

	/** Returns the baked sync group index from the compile step */
	int32 GetSyncGroupIndexFromName(FName SyncGroupName) const;

	/** Gets the index of the state machine matching MachineName */
	int32 GetStateMachineIndex(FName MachineName) const;

	/** Gets the runtime instance of the specified state machine */
	const FAnimNode_StateMachine* GetStateMachineInstance(int32 MachineIndex) const;

	/** 
	 * Get the index of the specified instance asset player. Useful to pass to GetInstanceAssetPlayerLength (etc.).
	 * Passing NAME_None to InstanceName will return the first (assumed only) player instance index found.
	 */
	int32 GetInstanceAssetPlayerIndex(FName MachineName, FName StateName, FName InstanceName = NAME_None) const;

	/** Returns all Animation Nodes of FAnimNode_AssetPlayerBase class within the specified (named) Animation Graph */
	TArray<const FAnimNode_AssetPlayerBase*> GetInstanceAssetPlayers(const FName& GraphName) const;

	/** Returns all Animation Nodes of FAnimNode_AssetPlayerBase class within the specified (named) Animation Graph */
	TArray<FAnimNode_AssetPlayerBase*> GetMutableInstanceAssetPlayers(const FName& GraphName);

	/** Returns all Animation Nodes implementing FAnimNode_AssetPlayerRelevancyBase within the specified (named) Animation Graph */
	TArray<const FAnimNode_AssetPlayerRelevancyBase*> GetInstanceRelevantAssetPlayers(const FName& GraphName) const;

	/** Returns all Animation Nodes of FAnimNode_AssetPlayerRelevancyBase class within the specified (named) Animation Graph */
	TArray<FAnimNode_AssetPlayerRelevancyBase*> GetMutableInstanceRelevantAssetPlayers(const FName& GraphName);

	/** Gets the runtime instance desc of the state machine specified by name */
	const FBakedAnimationStateMachine* GetStateMachineInstanceDesc(FName MachineName) const;

	/** Gets the most relevant asset player in a specified state */
	UE_DEPRECATED(5.1, "Please use GetRelevantAssetPlayerInterfaceFromState")
	const FAnimNode_AssetPlayerBase* GetRelevantAssetPlayerFromState(int32 MachineIndex, int32 StateIndex) const
	{
		return nullptr;
	}

	/** Gets the most relevant asset player in a specified state */
	const FAnimNode_AssetPlayerRelevancyBase* GetRelevantAssetPlayerInterfaceFromState(int32 MachineIndex, int32 StateIndex) const;

	//////////////////////////////////////////////////////////////////////////

public:
	/** Returns the value of a named curve. */
	UFUNCTION(BlueprintPure, Category="Animation|Curves", meta=(BlueprintThreadSafe))
	float GetCurveValue(FName CurveName) const;

	/** 
	* Returns whether a named curve was found, its value, and a default value when it's not found.
	* @param	AnimInstance	The anim instance to find this curve value for.
	* @param	CurveName		The name of the curve.
	* @param	DefaultValue	Value to use when the curve is not found.
	* @param	OutValue		The curve's value.
	*/
	UFUNCTION(BlueprintPure, Category="Animation|Curves", meta=(BlueprintThreadSafe))
	bool GetCurveValueWithDefault(FName CurveName, float DefaultValue, float& OutValue);

	/** This returns last up-to-date list of active curve names */
	UFUNCTION(BlueprintPure, Category = "Animation|Curves", meta=(BlueprintThreadSafe))
	void GetActiveCurveNames(EAnimCurveType CurveType, TArray<FName>& OutNames) const;

	/* This returns all curve names */
	UFUNCTION(BlueprintPure, Category = "Animation|Curves", meta=(BlueprintThreadSafe))
	void GetAllCurveNames(TArray<FName>& OutNames) const;

	/** Returns value of named curved in OutValue, returns whether the curve was actually found or not. */
	bool GetCurveValue(FName CurveName, float& OutValue) const;

	/** Returns the name of a currently active state in a state machine. */
	UFUNCTION(BlueprintPure, Category="Animation|State Machines", meta=(BlueprintInternalUseOnly = "true", AnimGetter = "true", BlueprintThreadSafe))
	FName GetCurrentStateName(int32 MachineIndex);

	/** Sets a morph target to a certain weight. */
	UFUNCTION(BlueprintCallable, Category="Animation|Morph Targets")
	void SetMorphTarget(FName MorphTargetName, float Value);

	/** Clears the current morph targets. */
	UFUNCTION(BlueprintCallable, Category="Animation|Morph Targets")
	void ClearMorphTargets();

	UE_DEPRECATED(5.0, "Please use UKismetAnimationLibrary::CalculateDirection instead")
	UFUNCTION(BlueprintCallable, Category="Animation", meta=(BlueprintThreadSafe))
	float CalculateDirection(const FVector& Velocity, const FRotator& BaseRotation) const;

	//--- AI communication start ---//
	/** locks indicated AI resources of animated pawn
	 *	DEPRECATED. Use LockAIResourcesWithAnimation instead */
	UFUNCTION(BlueprintCallable, Category = "Animation", BlueprintAuthorityOnly, Meta=(DeprecatedFunction, DeprecationMessage="Use LockAIResourcesWithAnimation instead"))
	void LockAIResources(bool bLockMovement, bool LockAILogic);

	/** unlocks indicated AI resources of animated pawn. Will unlock only animation-locked resources.
	 *	DEPRECATED. Use UnlockAIResourcesWithAnimation instead */
	UFUNCTION(BlueprintCallable, Category = "Animation", BlueprintAuthorityOnly, Meta=(DeprecatedFunction, DeprecationMessage="Use UnlockAIResourcesWithAnimation instead"))
	void UnlockAIResources(bool bUnlockMovement, bool UnlockAILogic);
	//--- AI communication end ---//

	UFUNCTION(BlueprintCallable, Category = "Animation|Synchronization", meta=(BlueprintThreadSafe))
	bool GetTimeToClosestMarker(FName SyncGroup, FName MarkerName, float& OutMarkerTime) const;

	UFUNCTION(BlueprintCallable, Category = "Animation|Synchronization", meta=(BlueprintThreadSafe))
	bool HasMarkerBeenHitThisFrame(FName SyncGroup, FName MarkerName) const;

	UFUNCTION(BlueprintCallable, Category = "Animation|Synchronization", meta=(BlueprintThreadSafe))
	bool IsSyncGroupBetweenMarkers(FName InSyncGroupName, FName PreviousMarker, FName NextMarker, bool bRespectMarkerOrder = true) const;

	UFUNCTION(BlueprintCallable, Category = "Animation|Synchronization", meta=(BlueprintThreadSafe))
	FMarkerSyncAnimPosition GetSyncGroupPosition(FName InSyncGroupName) const;

	/** Attempts to queue a transition request, returns true if the request was successful */
	UFUNCTION(BlueprintCallable, Category="Animation", meta = (BlueprintThreadSafe, Keywords = "Event,Request,Transition"))
	bool RequestTransitionEvent(const FName EventName, const double RequestTimeout, const ETransitionRequestQueueMode QueueMode, const ETransitionRequestOverwriteMode OverwriteMode);

	/** Removes all queued transition requests with the given event name */
	UFUNCTION(BlueprintCallable, Category = "Animation", meta = (BlueprintThreadSafe, Keywords = "Event,Request,Transition"))
	void ClearTransitionEvents(const FName EventName);

	/** Removes all queued transition requests */
	UFUNCTION(BlueprintCallable, Category = "Animation", meta = (BlueprintThreadSafe, Keywords = "Event,Request,Transition"))
	void ClearAllTransitionEvents();

	/** Returns whether or not the given event transition request has been queued */
	UFUNCTION(BlueprintPure, Category = "Transitions", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition", BlueprintThreadSafe, Keywords = "Event,Request,Transition"))
	bool QueryTransitionEvent(int32 MachineIndex, int32 TransitionIndex, FName EventName);

	/** Behaves like QueryTransitionEvent but additionally marks the event for consumption */
	UFUNCTION(BlueprintPure, Category = "Transitions", meta = (BlueprintInternalUseOnly = "true", AnimGetter = "true", GetterContext = "Transition", BlueprintThreadSafe, Keywords = "Event,Request,Transition"))
	bool QueryAndMarkTransitionEvent(int32 MachineIndex, int32 TransitionIndex, FName EventName);

public:
	//~ Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;
	virtual void PostInitProperties() override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	//~ End UObject Interface

#if WITH_EDITORONLY_DATA // ANIMINST_PostCompileValidation
	/** Name of Class to do Post Compile Validation.
	* See Class UAnimBlueprintPostCompileValidation. */
	UPROPERTY()
	FSoftClassPath PostCompileValidationClassName;

	/** Warn if AnimNodes are not using fast path during AnimBP compilation. */
	virtual bool PCV_ShouldWarnAboutNodesNotUsingFastPath() const { return false; }
	virtual bool PCV_ShouldNotifyAboutNodesNotUsingFastPath() const { return false; }

	// Called on the newly created CDO during anim blueprint compilation to allow subclasses a chance to replace animations (experimental)
	virtual void ApplyAnimOverridesToCDO(FCompilerResultsLog& MessageLog) {}
#endif // WITH_EDITORONLY_DATA

	/** Called when skipping an animation update because of URO. */
	virtual void OnUROSkipTickAnimation() {}

	UE_DEPRECATED(4.22, "This function is deprecated, please use OnUROPreInterpolation_AnyThread")
	virtual void OnUROPreInterpolation() {}

	/** 
	 * Called before URO interpolation is performed. Useful for modifying bone space transforms etc. before interpolation is performed.
	 * Note that this can be called on a worker thread. 
	 */
	virtual void OnUROPreInterpolation_AnyThread(FAnimationEvaluationContext& InOutContext) {}

	/** Flag passed to UpdateAnimation, determines the path we follow */
	enum class EUpdateAnimationFlag : uint8
	{
		/** Enforces a parallel update, regardless of state */
		ForceParallelUpdate,
		/** Use state to determine whether or not to immediately or update in parallel */
		Default
	};

	// Animation phase trigger
	// start with initialize
	// update happens in every tick. Can happen in parallel with others if conditions are right.
	// evaluate happens when condition is met - i.e. depending on your skeletalmeshcomponent update flag
	// post eval happens after evaluation is done
	// uninitialize happens when owner is unregistered
	// @param	bInDeferRootNodeInitialization	When set to true, defer init of the blend tree until the first Update() call
	void InitializeAnimation(bool bInDeferRootNodeInitialization = false);

	/** Called on the game thread before UpdateAnimation is called on linked instances */
	virtual void PreUpdateLinkedInstances(float DeltaSeconds) {}

	/** Update Animation code-paths, updates and advances animation state, returns whether or not the actual update should have been called immediately */
	void UpdateAnimation(float DeltaSeconds, bool bNeedsValidRootMotion, EUpdateAnimationFlag UpdateFlag = EUpdateAnimationFlag::Default );

	/** Run update animation work on a worker thread */
	void ParallelUpdateAnimation();

	/** Called after updates are completed, dispatches notifies etc. */
	void PostUpdateAnimation();

	/** Called on the game thread pre-evaluation. */
	void PreEvaluateAnimation();

	/** Check whether evaluation can be performed on the supplied skeletal mesh. Can be called from worker threads. */
	bool ParallelCanEvaluate(const USkeletalMesh* InSkeletalMesh) const;

	/** Perform evaluation. Can be called from worker threads. */
	void ParallelEvaluateAnimation(bool bForceRefPose, const USkeletalMesh* InSkeletalMesh, FParallelEvaluationData& OutAnimationPoseData);

	UE_DEPRECATED(4.26, "Please use ParallelEvaluateAnimation with different signature.")
	void ParallelEvaluateAnimation(bool bForceRefPose, const USkeletalMesh* InSkeletalMesh, FBlendedHeapCurve& OutCurve, FCompactPose& OutPose);

	void PostEvaluateAnimation();
	void UninitializeAnimation();

	// the below functions are the native overrides for each phase
	// Native initialization override point
	virtual void NativeInitializeAnimation();
	// Native update override point. It is usually a good idea to simply gather data in this step and 
	// for the bulk of the work to be done in NativeThreadSafeUpdateAnimation.
	virtual void NativeUpdateAnimation(float DeltaSeconds);
	// Native thread safe update override point. Executed on a worker thread just prior to graph update 
	// for linked anim instances, only called when the hosting node(s) are relevant
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds);
	// Native Post Evaluate override point
	virtual void NativePostEvaluateAnimation();
	// Native Uninitialize override point
	virtual void NativeUninitializeAnimation();

	// Executed when begin play is called on the owning component
	virtual void NativeBeginPlay();

	// Sets up a native transition delegate between states with PrevStateName and NextStateName, in the state machine with name MachineName.
	// Note that a transition already has to exist for this to succeed
	void AddNativeTransitionBinding(const FName& MachineName, const FName& PrevStateName, const FName& NextStateName, const FCanTakeTransition& NativeTransitionDelegate, const FName& TransitionName = NAME_None);

	// Check for whether a native rule is bound to the specified transition
	bool HasNativeTransitionBinding(const FName& MachineName, const FName& PrevStateName, const FName& NextStateName, FName& OutBindingName);

	// Sets up a native state entry delegate from state with StateName, in the state machine with name MachineName.
	void AddNativeStateEntryBinding(const FName& MachineName, const FName& StateName, const FOnGraphStateChanged& NativeEnteredDelegate);
	
	// Check for whether a native entry delegate is bound to the specified state
	bool HasNativeStateEntryBinding(const FName& MachineName, const FName& StateName, FName& OutBindingName);

	// Sets up a native state exit delegate from state with StateName, in the state machine with name MachineName.
	void AddNativeStateExitBinding(const FName& MachineName, const FName& StateName, const FOnGraphStateChanged& NativeExitedDelegate);

	// Check for whether a native exit delegate is bound to the specified state
	bool HasNativeStateExitBinding(const FName& MachineName, const FName& StateName, FName& OutBindingName);

	// Debug output for this anim instance. Info for SyncGroups, Graph, Montages, etc.
	void DisplayDebug(UCanvas* Canvas, const FDebugDisplayInfo& DebugDisplay, float& YL, float& YPos);

	// Display debug info about AnimInstance. Can be overridden to add custom info from child classes.
	virtual void DisplayDebugInstance(FDisplayDebugManager& DisplayDebugManager, float& Indent);

	/** Reset any dynamics running simulation-style updates (e.g. on teleport, time skip etc.) */
	UFUNCTION(BlueprintCallable, Category = "Animation|Dynamics")
	void ResetDynamics(ETeleportType InTeleportType);

	UE_DEPRECATED(4.20, "Please use ResetDynamics with a ETeleportType argument")
	void ResetDynamics();

	/** 
	 * Get the 'animation' LOD level, which by default is the PredictedLODLevel of this anim instance's skeletal mesh component.
	 * This function is used by the anim graph to determine the LOD level at which to run.
	 * @return the current LOD level
	 */
	virtual int32 GetLODLevel() const;

public:
	/** Access a read only version of the Updater Counter from the AnimInstanceProxy on the GameThread. */
	const FGraphTraversalCounter& GetUpdateCounter() const; 

	/** Access the required bones array */
	FBoneContainer& GetRequiredBones();
	const FBoneContainer& GetRequiredBones() const;
	const FBoneContainer& GetRequiredBonesOnAnyThread() const;

	/** Pending teleport type, set in ResetDynamics and cleared in UpdateAnimation */
	ETeleportType PendingDynamicResetTeleportType;

	/** Animation Notifies that has been triggered in the latest tick **/
	UPROPERTY(transient)
	FAnimNotifyQueue NotifyQueue;

	/** Currently Active AnimNotifyState, stored as a copy of the event as we need to
		call NotifyEnd on the event after a deletion in the editor. After this the event
		is removed correctly. */
	UPROPERTY(transient)
	TArray<FAnimNotifyEvent> ActiveAnimNotifyState;

	UPROPERTY(transient)
	TArray<FAnimNotifyEventReference> ActiveAnimNotifyEventReference;
	
private:
	/** Reset Animation Curves */
	void ResetAnimationCurves();

public: 
	/** Pushes blended heap curve to output curves in the proxy using required bones cached data */
	void UpdateCurvesToEvaluationContext(const FAnimationEvaluationContext& InOutContext);

	/** Update curves once evaluation has taken place. Mostly pushes curves to materials/morphs */
	void UpdateCurvesPostEvaluation();

	/** Swap curves out for evaluation */
	void SwapCurveWithEvaluationContext(FAnimationEvaluationContext& InOutContext);

	/** Update all internal curves from Blended Curve */
	void UpdateCurves(const FBlendedHeapCurve& InCurves);

	/** Copy curves from external source */
	void CopyCurveValues(const UAnimInstance& InSourceInstance);

	/** Refresh currently existing curves */
	void RefreshCurves(USkeletalMeshComponent* Component);

	/** Check whether we have active morph target curves */
	bool HasMorphTargetCurves() const;

	/** Check whether we have any active curves */
	bool HasActiveCurves() const;

	/** Get the current delta time */
	UFUNCTION(BlueprintPure, Category="Animation", meta=(BlueprintThreadSafe))
	float GetDeltaSeconds() const;
	
	/** 
	 * Append the type of curve to the OutCurveList specified by Curve Flags
	 */
	void AppendAnimationCurveList(EAnimCurveType Type, TMap<FName, float>& InOutCurveList) const;


	UE_DEPRECATED(4.19, "This function is deprecated. Use AppendAnimationCurveList instead.")
	void GetAnimationCurveList(EAnimCurveType Type, TMap<FName, float>& InOutCurveList) const;
	/**
	 *	Return the list of curves that are specified by type 
	 */
	const TMap<FName, float>& GetAnimationCurveList(EAnimCurveType Type) const;

#if WITH_EDITORONLY_DATA
	// Maximum playback position ever reached (only used when debugging in Persona)
	double LifeTimer;

	// Current scrubbing playback position (only used when debugging in Persona)
	double CurrentLifeTimerScrubPosition;
#endif

public:
	FGraphTraversalCounter DebugDataCounter;

private:
	TMap<FName, FMontageActiveSlotTracker> SlotWeightTracker;
	TMap<FName, FSimpleMulticastDelegate> ExternalNotifyHandlers;

	bool CheckOnInstanceAndMainInstance(TFunctionRef<bool (FAnimInstanceProxy* )> ProxyLambdaFunc);

public:
	/** 
	 * Recalculate Required Bones [RequiredBones]
	 * Is called when bRequiredBonesUpToDate = false
	 */
	void RecalcRequiredBones();

	/**
	* Recalculate Required Curves based on Required Bones [RequiredBones]
	*/
	void RecalcRequiredCurves(const FCurveEvaluationOption& CurveEvalOption);

	// @todo document
	inline USkeletalMeshComponent* GetSkelMeshComponent() const { return CastChecked<USkeletalMeshComponent>(GetOuter()); }

	virtual UWorld* GetWorld() const override;

	/** Trigger AnimNotifies **/
	void TriggerAnimNotifies(float DeltaSeconds);

	/** Trigger an AnimNotify.  Note that this version does not provide any context for the event **/ 
	void TriggerSingleAnimNotify(const FAnimNotifyEvent* AnimNotifyEvent);

	/** Trigger an AnimNotify using an EventReference that provide context used in derived notify events**/ 
	void TriggerSingleAnimNotify(FAnimNotifyEventReference& EventReference);

	/** Triggers end on active notify states and clears the array */
	void EndNotifyStates();

	/** Add curve float data using a curve Uid, the name of the curve will be resolved from the skeleton **/
	void AddCurveValue(const USkeleton::AnimCurveUID Uid, float Value);

	/** Add curve float data using a curve Uid, the name of the curve will be resolved from the skeleton. This uses an already-resolved proxy and mapping table for efficency **/
	void AddCurveValue(const FSmartNameMapping& Mapping, const FName& CurveName, float Value);

	/** Given a machine index, record a state machine weight for this frame */
	void RecordMachineWeight(const int32 InMachineClassIndex, const float InMachineWeight);
	/** 
	 * Add curve float data, using a curve name. External values should all be added using
	 * The curve UID to the public version of this method
	 */
	void AddCurveValue(const FName& CurveName, float Value);

	/** Given a machine and state index, record a state weight for this frame */
	void RecordStateWeight(const int32 InMachineClassIndex, const int32 InStateIndex, const float InStateWeight, const float InElapsedTime);

protected:
#if WITH_EDITORONLY_DATA
	// Returns true if a snapshot is being played back and the remainder of Update should be skipped.
	bool UpdateSnapshotAndSkipRemainingUpdate();
#endif

	/** Implementable custom function to handle notifies */
	virtual bool HandleNotify(const FAnimNotifyEvent& AnimNotifyEvent);

	// Root Motion
public:
	/** Get current RootMotion FAnimMontageInstance if any. NULL otherwise. */
	FAnimMontageInstance * GetRootMotionMontageInstance() const;

	/** Get current accumulated root motion, removing it from the AnimInstance in the process */
	FRootMotionMovementParams ConsumeExtractedRootMotion(float Alpha);

	/**  
	 * Queue blended root motion. This is used to blend in root motion transforms according to 
	 * the correctly-updated slot weight (after the animation graph has been updated).
	 */
	void QueueRootMotionBlend(const FTransform& RootTransform, const FName& SlotName, float Weight);

private:
	/** Active Root Motion Montage Instance, if any. */
	struct FAnimMontageInstance* RootMotionMontageInstance;

	/** Temporarily queued root motion blend */
	struct FQueuedRootMotionBlend
	{
		FQueuedRootMotionBlend(const FTransform& InTransform, const FName& InSlotName, float InWeight)
			: Transform(InTransform)
			, SlotName(InSlotName)
			, Weight(InWeight)
		{}

		FTransform Transform;
		FName SlotName;
		float Weight;
	};

	/** 
	 * Blend queue for blended root motion. This is used to blend in root motion transforms according to 
	 * the correctly-updated slot weight (after the animation graph has been updated).
	 */
	TArray<FQueuedRootMotionBlend> RootMotionBlendQueue;

	// Root motion read from proxy (where it is calculated) and stored here to avoid potential stalls by calling GetProxyOnGameThread
	FRootMotionMovementParams ExtractedRootMotion;

private:
	// update montage
	void UpdateMontage(float DeltaSeconds);
	void UpdateMontageSyncGroup();

protected:
	// Updates the montage data used for evaluation based on the current playing montages
	void UpdateMontageEvaluationData();

	/** Called to setup for updates */
	virtual void PreUpdateAnimation(float DeltaSeconds);

	/** update animation curves to component */
	void UpdateCurvesToComponents(USkeletalMeshComponent* Component);

	/** Override point for derived classes to create their own proxy objects (allows custom allocation) */
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy();

	/** Override point for derived classes to destroy their own proxy objects (allows custom allocation) */
	virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy);

	/** Access the proxy but block if a task is currently in progress as it wouldn't be safe to access it 
	 *	This is protected static member for allowing derived to access
	 */
	template <typename T /*= FAnimInstanceProxy*/>	// @TODO: Cant default parameters to this function on Xbox One until we move off the VS2012 compiler
	FORCEINLINE static T* GetProxyOnGameThreadStatic(UAnimInstance* InAnimInstance)
	{
		if (InAnimInstance)
		{
			check(IsInGameThread());
			UObject* OuterObj = InAnimInstance->GetOuter();
			if (OuterObj && OuterObj->IsA<USkeletalMeshComponent>())
			{
				bool bBlockOnTask = true;
				bool bPerformPostAnimEvaluation = true;
				InAnimInstance->GetSkelMeshComponent()->HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation);
			}
			if (InAnimInstance->AnimInstanceProxy == nullptr)
			{
				InAnimInstance->AnimInstanceProxy = InAnimInstance->CreateAnimInstanceProxy();
			}
			return static_cast<T*>(InAnimInstance->AnimInstanceProxy);
		}

		return nullptr;
	}
	/** Access the proxy but block if a task is currently in progress as it wouldn't be safe to access it */
	template <typename T /*= FAnimInstanceProxy*/>	// @TODO: Cant default parameters to this function on Xbox One until we move off the VS2012 compiler
	FORCEINLINE T& GetProxyOnGameThread()
	{
		return *GetProxyOnGameThreadStatic<T>(this);
	}

	/** Access the proxy but block if a task is currently in progress as it wouldn't be safe to access it */
	template <typename T/* = FAnimInstanceProxy*/>	// @TODO: Cant default parameters to this function on Xbox One until we move off the VS2012 compiler
	FORCEINLINE const T& GetProxyOnGameThread() const
	{
		check(IsInGameThread());
		if(GetOuter() && GetOuter()->IsA<USkeletalMeshComponent>())
		{
			bool bBlockOnTask = true;
			bool bPerformPostAnimEvaluation = true;
			GetSkelMeshComponent()->HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation);
		}
		if(AnimInstanceProxy == nullptr)
		{
			AnimInstanceProxy = const_cast<UAnimInstance*>(this)->CreateAnimInstanceProxy();
		}
		return *static_cast<const T*>(AnimInstanceProxy);
	}

	/** Access the proxy but block if a task is currently in progress (and we are on the game thread) as it wouldn't be safe to access it */
	template <typename T/* = FAnimInstanceProxy*/>	// @TODO: Cant default parameters to this function on Xbox One until we move off the VS2012 compiler
	FORCEINLINE T& GetProxyOnAnyThread()
	{
		if(GetOuter() && GetOuter()->IsA<USkeletalMeshComponent>())
		{
			if(IsInGameThread())
			{
				bool bBlockOnTask = true;
				bool bPerformPostAnimEvaluation = true;
				GetSkelMeshComponent()->HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation);
			}
		}
		if(AnimInstanceProxy == nullptr)
		{
			AnimInstanceProxy = CreateAnimInstanceProxy();
		}
		return *static_cast<T*>(AnimInstanceProxy);
	}

	/** Access the proxy but block if a task is currently in progress (and we are on the game thread) as it wouldn't be safe to access it */
	template <typename T/* = FAnimInstanceProxy*/>	// @TODO: Cant default parameters to this function on Xbox One until we move off the VS2012 compiler
	FORCEINLINE const T& GetProxyOnAnyThread() const
	{
		if(GetOuter() && GetOuter()->IsA<USkeletalMeshComponent>())
		{
			if(IsInGameThread())
			{
				bool bBlockOnTask = true;
				bool bPerformPostAnimEvaluation = true;
				GetSkelMeshComponent()->HandleExistingParallelEvaluationTask(bBlockOnTask, bPerformPostAnimEvaluation);
			}
		}
		if(AnimInstanceProxy == nullptr)
		{
			AnimInstanceProxy = const_cast<UAnimInstance*>(this)->CreateAnimInstanceProxy();
		}
		return *static_cast<const T*>(AnimInstanceProxy);
	}

	friend struct FAnimNode_LinkedAnimGraph;
	friend struct FAnimNode_LinkedAnimLayer;
	friend struct FAnimInstanceProxy;
	
public:
	/** Return whether this AnimNotifyState should be triggered */
	virtual bool ShouldTriggerAnimNotifyState(const UAnimNotifyState* AnimNotifyState) const;

protected:
	/** Proxy object, nothing should access this from an externally-callable API as it is used as a scratch area on worker threads */
	mutable FAnimInstanceProxy* AnimInstanceProxy;

public:
	/** Called when a montage hits a 'PlayMontageNotify' or 'PlayMontageNotifyWindow' begin */
	FPlayMontageAnimNotifyDelegate OnPlayMontageNotifyBegin;

	/** Called when a montage hits a 'PlayMontageNotify' or 'PlayMontageNotifyWindow' end */
	FPlayMontageAnimNotifyDelegate OnPlayMontageNotifyEnd;

public:
	/** Dispatch AnimEvents (AnimNotifies, Montage Events) queued during UpdateAnimation() */
	void DispatchQueuedAnimEvents();

#if WITH_EDITOR
	// Helper function to handle reinstancing in editor
	virtual void HandleObjectsReinstanced(const TMap<UObject*, UObject*>& OldToNewInstanceMap);

	/** Returns true if this anim instance is actively being debugged, false otherwise */
	bool IsBeingDebugged() const;
#endif
};
