// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimExecutionContext.h"
#include "Animation/AnimNodeReference.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "PoseSearch/PoseSearchResult.h"
#include "PoseSearch/PoseSearchRole.h"
#include "SequenceEvaluatorLibrary.h"
#include "SequencePlayerLibrary.h"
#include "PoseSearchLibrary.generated.h"

namespace UE::PoseSearch
{
	struct FSearchContext;
} // namespace UE::PoseSearch

UENUM()
enum class EPoseSearchInterruptMode : uint8
{
	// continuing pose search will be performed if valid
	DoNotInterrupt,

	// continuing pose search will be interrupted if its database is not listed in the searchable databases
	InterruptOnDatabaseChange,

	// continuing pose search will be interrupted if its database is not listed in the searchable databases, 
	// and continuing pose will be invalidated (forcing the schema to use pose history to build the query)
	InterruptOnDatabaseChangeAndInvalidateContinuingPose,

	// continuing pose search will always be interrupted
	ForceInterrupt,

	/// continuing pose search will always be interrupted
	// and continuing pose will be invalidated (forcing the schema to use pose history to build the query)
	ForceInterruptAndInvalidateContinuingPose,
};

struct FAnimationUpdateContext;

struct FMotionMatchingState
{
	// Reset the state to a default state using the current Database
	void Reset(const FTransform& ComponentTransform);

	// Attempts to set the internal state to match the provided asset time including updating the internal DbPoseIdx. 
	// If the provided asset time is out of bounds for the currently playing asset then this function will reset the 
	// state back to the default state.
	void AdjustAssetTime(float AssetTime);

	// Internally stores the 'jump' to a new pose/sequence index and asset time for evaluation
	void JumpToPose(const FAnimationUpdateContext& Context, const UE::PoseSearch::FSearchResult& Result, int32 MaxActiveBlends, float BlendTime);

	void UpdateWantedPlayRate(const UE::PoseSearch::FSearchContext& SearchContext, const FFloatInterval& PlayRate, float TrajectorySpeedMultiplier);

	FVector GetEstimatedFutureRootMotionVelocity() const;

	UE::PoseSearch::FSearchResult CurrentSearchResult;

	// Time since the last pose jump
	float ElapsedPoseSearchTime = 0.f;

	// wanted PlayRate to have the selected animation playing at the estimated requested speed from the query.
	float WantedPlayRate = 1.f;

	// true if a new animation has been selected
	bool bJumpedToPose = false;

	UE::PoseSearch::FPoseIndicesHistory PoseIndicesHistory;

	// Component delta yaw (also considered as root bone delta yaw)
	float ComponentDeltaYaw = 0.f;

	// Internal component yaw in world space. Initialized as FRotator(AnimInstanceProxy->GetComponentTransform().GetRotation()).Yaw, but then integrated by ComponentDeltaYaw
	float ComponentWorldYaw = 0.f;
	
	// RootMotionTransformDelta yaw at the end of FAnimNode_MotionMatching::Evaluate_AnyThread (it represents the previous frame animation delta yaw)
	float AnimationDeltaYaw = 0.f;

#if UE_POSE_SEARCH_TRACE_ENABLED
	// Root motion delta for currently playing animation (or animation tree if from the blend stack)
	FTransform RootMotionTransformDelta = FTransform::Identity;
#endif //UE_POSE_SEARCH_TRACE_ENABLED
};

USTRUCT(Experimental, BlueprintType, Category="Animation|Pose Search")
struct POSESEARCH_API FPoseSearchFutureProperties
{
	GENERATED_BODY()

public:
	// Animation to play (it'll start at AnimationTime seconds)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=State)
	TObjectPtr<UObject> Animation;

	// Start time for Animation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=State)
	float AnimationTime = 0.f;

	// Interval time before playing Animation
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=State)
	float IntervalTime = 0.f;
};

UCLASS()
class POSESEARCH_API UPoseSearchLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

#if UE_POSE_SEARCH_TRACE_ENABLED
	static void TraceMotionMatchingState(
		UE::PoseSearch::FSearchContext& SearchContext,
		const UE::PoseSearch::FSearchResult& CurrentResult,
		float ElapsedPoseSearchTime,
		const FTransform& RootMotionTransformDelta,
		int32 NodeId,
		float DeltaTime,
		bool bSearch,
		float RecordingTime);
#endif // UE_POSE_SEARCH_TRACE_ENABLED

public:
	/**
	* Implementation of the core motion matching algorithm
	*
	* @param Context						Input animation update context providing access to the proxy and delta time
	* @param Databases						Input array of databases to search
	* @param BlendTime						Input time in seconds to blend out to the new pose. Uses either inertial blending, requiring an Inertialization node after this node, or the internal blend stack, if MaxActiveBlends is greater than zero.
	* @param MaxActiveBlends				Input number of max active animation segments being blended together in the blend stack. If MaxActiveBlends is zero then the blend stack is disabled.
	* @param PoseJumpThresholdTime			Input don't jump to poses of the same segment that are within the interval this many seconds away from the continuing pose.
	* @param PoseReselectHistory			Input prevent re-selection of poses that have been selected previously within this much time (in seconds) in the past. This is across all animation segments that have been selected within this time range.
	* @param SearchThrottleTime				Input minimum amount of time to wait between searching for a new pose segment. It allows users to define how often the system searches, default for locomotion is searching every update, but you may only want to search once for other situations, like jump.
	* @param PlayRate						Input effective range of play rate that can be applied to the animations to account for discrepancies in estimated velocity between the movement modeland the animation.
	* @param InOutMotionMatchingState		Input/Output encapsulated motion matching algorithm and state
	* @param InterruptMode					Input continuing pose search interrupt mode
	* @param bShouldSearch					Input if false search will happen only if there's no valid continuing pose
	* @param bDebugDrawQuery				Input draw the composed query if valid
	* @param bDebugDrawCurResult			Input draw the current result if valid
	*/
	static void UpdateMotionMatchingState(
		const FAnimationUpdateContext& Context,
		const TArray<TObjectPtr<const UPoseSearchDatabase>>& Databases,
		float BlendTime,
		int32 MaxActiveBlends,
		const FFloatInterval& PoseJumpThresholdTime,
		float PoseReselectHistory,
		float SearchThrottleTime,
		const FFloatInterval& PlayRate,
		FMotionMatchingState& InOutMotionMatchingState,
		EPoseSearchInterruptMode InterruptMode = EPoseSearchInterruptMode::DoNotInterrupt,
		bool bShouldSearch = true,
		bool bShouldUseCachedChannelData = true,
		bool bDebugDrawQuery = false,
		bool bDebugDrawCurResult = false);

	/**
	* Implementation of the core motion matching algorithm
	*
	* @param AnimInstance					Input animation instance
	* @param AssetsToSearch					Input assets to search (UPoseSearchDatabase or any animation asset containing UAnimNotifyState_PoseSearchBranchIn)
	* @param PoseHistoryName				Input tag of the associated PoseSearchHistoryCollector node in the anim graph
	* @param Future							Input future properties to match (animation / start time / time offset)
	* @param SelectedAnimation				Output selected animation from the Database asset
	* @param Result							Output FPoseSearchBlueprintResult with the search result
	* @param DebugSessionUniqueIdentifier	Input unique identifier used to identify TraceMotionMatchingState (rewind debugger / pose search debugger) session. Similarly the MM node uses Context.GetCurrentNodeId()
	*/
	UFUNCTION(BlueprintPure, Category = "Animation|Pose Search|Experimental", meta = (BlueprintThreadSafe, Keywords = "PoseMatch"))
	static void MotionMatch(
		UAnimInstance* AnimInstance,
		TArray<UObject*> AssetsToSearch,
		const FName PoseHistoryName,
		FPoseSearchFutureProperties Future,
		FPoseSearchBlueprintResult& Result,
		const int32 DebugSessionUniqueIdentifier = 6174);

	/**
	* Implementation of the core motion matching algorithm for multiple characters
	*
	* @param AnimInstances					Input animation instances
	* @param Roles							Input Roles associated to the animation instances
	* @param AssetsToSearch					Input assets to search (UPoseSearchDatabase or any animation asset containing UAnimNotifyState_PoseSearchBranchIn)
	* @param PoseHistoryName				Input tag of the associated PoseSearchHistoryCollector node in the anim graphs of the AnimInstances
	* @param Result							Output FPoseSearchBlueprintResult with the search result
	* @param DebugSessionUniqueIdentifier	Input unique identifier used to identify TraceMotionMatchingState (rewind debugger / pose search debugger) session. Similarly the MM node uses Context.GetCurrentNodeId()
	*/
	UFUNCTION(BlueprintPure, Category = "Animation|Pose Search|Experimental", meta = (BlueprintThreadSafe, Keywords = "PoseMatch"))
	static void MotionMatchMulti(
		TArray<ACharacter*> AnimInstances,
		TArray<FName> Roles,
		TArray<UObject*> AssetsToSearch,
		const FName PoseHistoryName,
		FPoseSearchBlueprintResult& Result,
		const int32 DebugSessionUniqueIdentifier = 6174);

	static void MotionMatch(
		TArrayView<UAnimInstance*> AnimInstances,
		TArrayView<const UE::PoseSearch::FRole> Roles,
		TArrayView<const UObject*> AssetsToSearch,
		const FName PoseHistoryName,
		const FPoseSearchFutureProperties& Future,
		FPoseSearchBlueprintResult& Result,
		const int32 DebugSessionUniqueIdentifier);

	static UE::PoseSearch::FSearchResult MotionMatch(
		const FAnimationBaseContext& Context,
		TArrayView<const UObject*> AssetsToSearch,
		const UObject* PlayingAsset = nullptr,
		float PlayingAssetAccumulatedTime = 0.f);

	static UE::PoseSearch::FSearchResult MotionMatch(
		TArrayView<UAnimInstance*> AnimInstances,
		TArrayView<const UE::PoseSearch::FRole> Roles,
		TArrayView<const UE::PoseSearch::IPoseHistory*> PoseHistories, 
		TArrayView<const UObject*> AssetsToSearch,
		const UObject* PlayingAsset,
		float PlayingAssetAccumulatedTime,
		const int32 DebugSessionUniqueIdentifier);
};

