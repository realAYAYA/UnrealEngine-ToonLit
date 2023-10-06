// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AlphaBlend.h"
#include "AnimNodes/AnimNode_SequenceEvaluator.h"
#include "Animation/AnimExecutionContext.h"
#include "Animation/AnimNodeReference.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchHistory.h"
#include "PoseSearch/PoseSearchResult.h"
#include "SequenceEvaluatorLibrary.h"
#include "SequencePlayerLibrary.h"
#include "PoseSearchLibrary.generated.h"

namespace UE::PoseSearch
{
	struct FSearchContext;
} // namespace UE::PoseSearch

struct FAnimationUpdateContext;
struct FPoseSearchQueryTrajectory;

USTRUCT(BlueprintType, Category="Animation|Pose Search")
struct POSESEARCH_API FMotionMatchingState
{
	GENERATED_BODY()

	// Reset the state to a default state using the current Database
	void Reset(const FTransform& ComponentTransform);

	// Attempts to set the internal state to match the provided asset time including updating the internal DbPoseIdx. 
	// If the provided asset time is out of bounds for the currently playing asset then this function will reset the 
	// state back to the default state.
	void AdjustAssetTime(float AssetTime);

	// Internally stores the 'jump' to a new pose/sequence index and asset time for evaluation
	void JumpToPose(const FAnimationUpdateContext& Context, const UE::PoseSearch::FSearchResult& Result, int32 MaxActiveBlends, float BlendTime);

	void UpdateWantedPlayRate(const UE::PoseSearch::FSearchContext& SearchContext, const FFloatInterval& PlayRate, float TrajectorySpeedMultiplier);

	void UpdateRootBoneControl(const FAnimationUpdateContext& Context, float YawFromAnimationBlendRate);

	UE::PoseSearch::FSearchResult CurrentSearchResult;

	// Time since the last pose jump
	UPROPERTY(Transient)
	float ElapsedPoseSearchTime = 0.f;

	// wanted PlayRate to have the selected animation playing at the estimated requested speed from the query.
	UPROPERTY(Transient)
	float WantedPlayRate = 1.f;

	// true if a new animation has been selected
	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category=State)
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

UCLASS()
class POSESEARCH_API UPoseSearchLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

#if UE_POSE_SEARCH_TRACE_ENABLED
	static void TraceMotionMatchingState(
		const FPoseSearchQueryTrajectory& Trajectory,
		UE::PoseSearch::FSearchContext& SearchContext,
		const UE::PoseSearch::FSearchResult& CurrentResult,
		float ElapsedPoseSearchTime,
		const FTransform& RootMotionTransformDelta,
		const UObject* AnimInstance,
		int32 NodeId,
		float DeltaTime,
		bool bSearch,
		float RecordingTime,
		float SearchBestCost,
		float SearchBruteForceCost,
		int32 SearchBestPosePos);
#endif // UE_POSE_SEARCH_TRACE_ENABLED

	static FPoseSearchQueryTrajectory ProcessTrajectory(
		const FPoseSearchQueryTrajectory& Trajectory,
		const FTransform& ComponentWorldTransform,
		float RootBoneDeltaYaw,
		float YawFromAnimationTrajectoryBlendTime,
		float TrajectorySpeedMultiplier);

public:
	/**
	* Implementation of the core motion matching algorithm
	*
	* @param Context						Input animation update context providing access to the proxy and delta time
	* @param Databases						Input array of databases to search
	* @param Trajectory						Input motion trajectory samples for pose search queries. Expected to be in the space of the SkeletalMeshComponent. This is provided with the CharacterMovementTrajectory Component output.
	* @param BlendTime						Input time in seconds to blend out to the new pose. Uses either inertial blending, requiring an Inertialization node after this node, or the internal blend stack, if MaxActiveBlends is greater than zero.
	* @param MaxActiveBlends				Input number of max active animation segments being blended together in the blend stack. If MaxActiveBlends is zero then the blend stack is disabled.
	* @param PoseJumpThresholdTime			Input don't jump to poses of the same segment that are less than this many seconds away.
	* @param PoseReselectHistory			Input prevent re-selection of poses that have been selected previously within this much time (in seconds) in the past. This is across all animation segments that have been selected within this time range.
	* @param SearchThrottleTime				Input minimum amount of time to wait between searching for a new pose segment. It allows users to define how often the system searches, default for locomotion is searching every update, but you may only want to search once for other situations, like jump.
	* @param PlayRate						Input effective range of play rate that can be applied to the animations to account for discrepancies in estimated velocity between the movement modeland the animation.
	* @param InOutMotionMatchingState		Input/Output encapsulated motion matching algorithm and state
	* @param bForceInterrupt				Input force interrupt request (if true the continuing pose will be invalidated)
	* @param bShouldSearch					Input if false search will happen only if there's no valid continuing pose
	* @param bDebugDrawQuery				Input draw the composed query if valid
	* @param bDebugDrawCurResult			Input draw the current result if valid
	*/
	static void UpdateMotionMatchingState(
		const FAnimationUpdateContext& Context,
		const TArray<TObjectPtr<const UPoseSearchDatabase>>& Databases,
		const FPoseSearchQueryTrajectory& Trajectory,
		float TrajectorySpeedMultiplier,
		float BlendTime,
		int32 MaxActiveBlends,
		float PoseJumpThresholdTime,
		float PoseReselectHistory,
		float SearchThrottleTime,
		const FFloatInterval& PlayRate,
		FMotionMatchingState& InOutMotionMatchingState,
		float YawFromAnimationBlendRate,
		float YawFromAnimationTrajectoryBlendTime,
		bool bForceInterrupt = false,
		bool bShouldSearch = true,
		bool bDebugDrawQuery = false,
		bool bDebugDrawCurResult = false);

	/**
	* Implementation of the core motion matching algorithm
	*
	* @param AnimInstance					Input animation instance
	* @param Database						Input database to search
	* @param Trajectory						Input motion trajectory samples for pose search queries. Expected to be in the space of the SkeletalMeshComponent. This is provided with the CharacterMovementTrajectory Component output.
	* @param TrajectorySpeedMultiplier		Input Trajectory velocity will be multiplied by TrajectorySpeedMultiplier: values below 1 will result in selecting animation slower than requested from the original Trajectory
	* @param PoseHistoryName				Input tag of the associated PoseSearchHistoryCollector node in the anim graph
	* @param SelectedAnimation				Output selected animation from the Database asset
	* @param SelectedTime					Output selected animation time
	* @param bLoop							Output selected animation looping state
	* @param bIsMirrored					Output selected animation mirror state
	* @param BlendParameters				Output selected animation blend space parameters (if SelectedAnimation is a blend space)
	* @param SearchCost						Output search associated cost
	* @param FutureAnimation				Input animation we want to match after TimeToFutureAnimationStart seconds
	* @param FutureAnimationStartTime		Input start time for the first pose of FutureAnimation
	* @param TimeToFutureAnimationStart		Input time in seconds before start playing FutureAnimation (from FutureAnimationStartTime seconds)
	* @param DebugSessionUniqueIdentifier	Input unique identifier used to identify TraceMotionMatchingState (rewind debugger / pose search debugger) session. Similarly the MM node uses Context.GetCurrentNodeId()
	*/
	UFUNCTION(BlueprintPure, Category = "Animation|Pose Search", meta = (BlueprintThreadSafe, Keywords = "PoseMatch"))
	static void MotionMatch(
		UAnimInstance* AnimInstance,
		const UPoseSearchDatabase* Database,
		const FPoseSearchQueryTrajectory Trajectory,
		float TrajectorySpeedMultiplier,
		const FName PoseHistoryName,
		UAnimationAsset*& SelectedAnimation,
		float& SelectedTime,
		bool& bLoop,
		bool& bIsMirrored,
		FVector& BlendParameters,
		float& SearchCost,
		const UAnimationAsset* FutureAnimation = nullptr,
		float FutureAnimationStartTime = 0.f,
		float TimeToFutureAnimationStart = 0.f,
		const int DebugSessionUniqueIdentifier = 6174);
};

