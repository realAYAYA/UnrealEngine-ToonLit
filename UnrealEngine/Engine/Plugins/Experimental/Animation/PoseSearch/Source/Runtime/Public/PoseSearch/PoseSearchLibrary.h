// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimNodes/AnimNode_SequenceEvaluator.h"
#include "Animation/AnimExecutionContext.h"
#include "Animation/AnimNodeReference.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PoseSearch/PoseSearch.h"
#include "SequenceEvaluatorLibrary.h"
#include "SequencePlayerLibrary.h"

#include "PoseSearchLibrary.generated.h"

UENUM(BlueprintType, Category="Motion Trajectory", meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class EMotionMatchingFlags : uint8
{
	None = 0 UMETA(Hidden),
	JumpedToPose = 1 << 0,		// Signals that motion matching has made a significant deviation in the selected sequence/pose index
	JumpedToFollowUp = 1 << 1,	// Motion matching chose the follow up animation of the prior sequence
};
ENUM_CLASS_FLAGS(EMotionMatchingFlags);

USTRUCT(BlueprintType, Category = "Animation|Pose Search")
struct POSESEARCH_API FMotionMatchingSettings
{
	GENERATED_BODY()

	// Time in seconds to blend out to the new pose. Uses either inertial blending, requiring an Inertialization node after this node, or the internal blend stack, if MaxActiveBlends is greter than zero
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(ClampMin="0"))
	float BlendTime = 0.2f;

	// Number of max active blendin animation in the blend stack. If MaxActiveBlends is zero then blend stack is disabled
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(ClampMin="0"))
	int32 MaxActiveBlends = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(UseAsBlendProfile = true))
	TObjectPtr<UBlendProfile> BlendProfile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear;

	// If the pose jump requires a mirroring change and this value is greater than 0, it will be used instead of BlendTime
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(ClampMin="0", DislayAfter="BlendTime"))
	float MirrorChangeBlendTime = 0.0f;
	
	// Don't jump to poses that are less than this many seconds away
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(ClampMin="0"))
	float PoseJumpThresholdTime = 1.f;

	// Don't jump to poses that has been selected previously within this many seconds in the past
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (ClampMin = "0"))
	float PoseReselectHistory = 0.f;

	// Minimum amount of time to wait between pose search queries
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings, meta=(ClampMin="0"))
	float SearchThrottleTime = 0.1f;
};

USTRUCT(BlueprintType, Category="Animation|Pose Search")
struct POSESEARCH_API FMotionMatchingState
{
	GENERATED_BODY()

	// Reset the state to a default state using the current Database
	void Reset();

	// Checks if the currently playing asset can advance and stay in bounds under the provided DeltaTime. If the 
	// asset cannot advance but a follow up asset is provided by the database then this function will return true and 
	// also set bAdvanceToFollowUpAsset to true, while providing the pose to jump to in the follow up asset in OutFollowUpAsset
	bool CanAdvance(float DeltaTime, bool& bOutAdvanceToFollowUpAsset, UE::PoseSearch::FSearchResult& OutFollowUpAsset) const;

	// Attempts to set the internal state to match the provided asset time including updating the internal DbPoseIdx. 
	// If the provided asset time is out of bounds for the currently playing asset then this function will reset the 
	// state back to the default state.
	void AdjustAssetTime(float AssetTime);

	// Internally stores the 'jump' to a new pose/sequence index and asset time for evaluation
	void JumpToPose(const FAnimationUpdateContext& Context, const FMotionMatchingSettings& Settings, const UE::PoseSearch::FSearchResult& Result);

	const FPoseSearchIndexAsset* GetCurrentSearchIndexAsset() const;

	float ComputeJumpBlendTime(const UE::PoseSearch::FSearchResult& Result, const FMotionMatchingSettings& Settings) const;

	UE::PoseSearch::FSearchResult CurrentSearchResult;

	// Time since the last pose jump
	UPROPERTY(Transient)
	float ElapsedPoseJumpTime = 0.f;

	// Evaluation flags relevant to the state of motion matching
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=State)
	EMotionMatchingFlags Flags = EMotionMatchingFlags::None;

	// Root motion delta for currently playing animation. Only required
	// when UE_POSE_SEARCH_TRACE_ENABLED is active
	UPROPERTY(Transient)
	FTransform RootMotionTransformDelta = FTransform::Identity;

#if WITH_EDITORONLY_DATA
	enum { SearchCostHistoryNumSamples = 200 };
	FDebugFloatHistory SearchCostHistoryContinuing = FDebugFloatHistory(SearchCostHistoryNumSamples, 0, 0, true);
	FDebugFloatHistory SearchCostHistoryBruteForce = FDebugFloatHistory(SearchCostHistoryNumSamples, 0, 0, true);
	FDebugFloatHistory SearchCostHistoryKDTree = FDebugFloatHistory(SearchCostHistoryNumSamples, 0, 0, true);
#endif

	UE::PoseSearch::FPoseIndicesHistory PoseIndicesHistory;
};

/**
* Implementation of the core motion matching algorithm
*
* @param UpdateContext				Input animation update context providing access to the proxy and delta time
* @param Database					Input collection of animations for motion matching
* @param Trajectory					Input motion trajectory samples for pose search queries
* @param Settings					Input motion matching algorithm configuration settings
* @param InOutMotionMatchingState	Input/Output encapsulated motion matching algorithm and state
*/
POSESEARCH_API void UpdateMotionMatchingState(
	const FAnimationUpdateContext& Context,
	const UPoseSearchSearchableAsset* Searchable,
	const FGameplayTagContainer* ActiveTagsContainer,
	const FTrajectorySampleRange& Trajectory,
	const FMotionMatchingSettings& Settings,
	FMotionMatchingState& InOutMotionMatchingState,
	bool bForceInterrupt
);

