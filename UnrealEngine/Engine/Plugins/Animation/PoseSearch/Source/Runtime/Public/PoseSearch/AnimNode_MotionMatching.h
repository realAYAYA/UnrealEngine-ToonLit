// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNode_AssetPlayerBase.h"
#include "GameplayTagContainer.h"
#include "BlendStack/AnimNode_BlendStack.h"
#include "PoseSearch/PoseSearchLibrary.h"
#include "AnimNode_MotionMatching.generated.h"

class UPoseSearchDatabase;

USTRUCT(BlueprintInternalUseOnly)
struct POSESEARCH_API FAnimNode_MotionMatching : public FAnimNode_BlendStack_Standalone
{
	GENERATED_BODY()

public:
	// Search InDatabase instead of the Database property on this node. Use InterruptMode to control the continuing pose search
	void SetDatabaseToSearch(UPoseSearchDatabase* InDatabase, EPoseSearchInterruptMode InterruptMode);

	// Search InDatabases instead of the Database property on the node. Use InterruptMode to control the continuing pose search.
	void SetDatabasesToSearch(TConstArrayView<UPoseSearchDatabase*> InDatabases, EPoseSearchInterruptMode InterruptMode);

	// Reset the effects of SetDatabaseToSearch/SetDatabasesToSearch and use the Database property on this node.
	void ResetDatabasesToSearch(EPoseSearchInterruptMode InterruptMode);

	// Use InterruptMode to control the continuing pose search
	void SetInterruptMode(EPoseSearchInterruptMode InterruptMode);

	const FMotionMatchingState& GetMotionMatchingState() const { return MotionMatchingState; }

	FVector GetEstimatedFutureRootMotionVelocity() const;

	const FAnimNodeFunctionRef& GetOnUpdateMotionMatchingStateFunction() const;

private:
	// FAnimNode_Base interface
	// @todo: implement CacheBones_AnyThread to rebind the schema bones
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	// End of FAnimNode_Base interface

	// FAnimNode_AssetPlayerBase interface
	virtual void UpdateAssetPlayer(const FAnimationUpdateContext& Context) override;
	virtual bool GetIgnoreForRelevancyTest() const override;
	virtual bool SetIgnoreForRelevancyTest(bool bInIgnoreForRelevancyTest) override;
	// End of FAnimNode_AssetPlayerBase interface

	// The database to search. This can be overridden by Anim Node Functions such as "On Become Relevant" and "On Update" via SetDatabaseToSearch/SetDatabasesToSearch.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinShownByDefault))
	TObjectPtr<const UPoseSearchDatabase> Database = nullptr;

	// Time in seconds to blend out to the new pose. Uses either inertial blending, requiring an Inertialization node after this node, or the internal blend stack, if MaxActiveBlends is greater than zero.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault, ClampMin="0"))
	float BlendTime = 0.2f;

	// Set Blend Profiles (editable in the skeleton) to determine how the blending is distributed among your character's bones. It could be used to differentiate between upper body and lower body to blend timing.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault, UseAsBlendProfile = true))
	TObjectPtr<UBlendProfile> BlendProfile;

	// How the blend is applied over time to the bones. Common selections are linear, ease in, ease out, and ease in and out.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault))
	EAlphaBlendOption BlendOption = EAlphaBlendOption::Linear;

	// Don't jump to poses of the same segment that are within the interval this many seconds away from the continuing pose.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault))
	FFloatInterval PoseJumpThresholdTime = FFloatInterval(0.f, 0.f);

	// Prevent re-selection of poses that have been selected previously within this much time (in seconds) in the past. This is across all animation segments that have been selected within this time range.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault, ClampMin = "0"))
	float PoseReselectHistory = 0.3f;

	// Minimum amount of time to wait between searching for a new pose segment. It allows users to define how often the system searches, default for locomotion is searching every update, but you may only want to search once for other situations, like jump.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault, ClampMin="0"))
	float SearchThrottleTime = 0.f;

	// Effective range of play rate that can be applied to the animations to account for discrepancies in estimated velocity between the movement model and the animation.
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault, ClampMin = "0.2", ClampMax = "3.0", UIMin = "0.2", UIMax = "3.0"))
	FFloatInterval PlayRate = FFloatInterval(1.f, 1.f);

	UPROPERTY(EditAnywhere, Category = Settings, Category = Settings)
	bool bUseInertialBlend = false;

	// Reset the motion matching selection state if it has become relevant to the graph after not being updated on previous frames.
	UPROPERTY(EditAnywhere, Category = Settings)
	bool bResetOnBecomingRelevant = true;

	// If set to false, the motion matching node will perform a search only if the continuing pose is invalid. This is useful if you want to stagger searches of different nodes for performance reasons
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault))
	bool bShouldSearch = true;

	// If set to true, the search of multiple databases with different schemas will try to share pose features data calculated during query build
	// the idea is to be able to share as much as possible the continuing pose features vector across different schemas (and potentially improve performances)
	// defaulted to false to preserve behavior backward compatibility
	UPROPERTY(EditAnywhere, Category = Settings, meta = (PinHiddenByDefault))
	bool bShouldUseCachedChannelData = false;
	
	// Encapsulated motion matching algorithm and internal state
	FMotionMatchingState MotionMatchingState;

	// Update Counter for detecting being relevant
	FGraphTraversalCounter UpdateCounter;

	// List of databases this node is searching.
	UPROPERTY()
	TArray<TObjectPtr<const UPoseSearchDatabase>> DatabasesToSearch;

	// Applied EPoseSearchInterruptMode on the next update that controls the continuing pose search eveluation. This is set back to EPoseSearchInterruptMode::DoNotInterrupt after each update.
	EPoseSearchInterruptMode NextUpdateInterruptMode = EPoseSearchInterruptMode::DoNotInterrupt;

	// True if the Database property on this node has been overridden by SetDatabaseToSearch/SetDatabasesToSearch.
	bool bOverrideDatabaseInput = false;

#if WITH_EDITORONLY_DATA
	// If true, "Relevant anim" nodes that look for the highest weighted animation in a state will ignore this node
	UPROPERTY(EditAnywhere, Category = Relevancy, meta = (FoldProperty, PinHiddenByDefault))
	bool bIgnoreForRelevancyTest = false;

	UPROPERTY(meta=(FoldProperty))
	FAnimNodeFunctionRef OnMotionMatchingStateUpdated;

#endif // WITH_EDITORONLY_DATA

	friend class UAnimGraphNode_MotionMatching;
};