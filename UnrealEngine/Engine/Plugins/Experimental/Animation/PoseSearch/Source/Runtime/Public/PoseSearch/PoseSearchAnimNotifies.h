// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/AnimNotifies/AnimNotifyState.h"

#include "PoseSearchAnimNotifies.generated.h"

// Base class for pose search anim notify states
UCLASS(Abstract)
class POSESEARCH_API UAnimNotifyState_PoseSearchBase : public UAnimNotifyState
{
	GENERATED_BODY()
};

// Use this notify state to remove animation segments from the database completely, they will never play or return from
// a search result
UCLASS(Blueprintable, meta = (DisplayName = "Pose Matching: Exclude From Database"))
class POSESEARCH_API UAnimNotifyState_PoseSearchExcludeFromDatabase : public UAnimNotifyState_PoseSearchBase
{
	GENERATED_BODY()
};

// A pose matching search will not return results that overlap this notify, but the animation segment can still play
// if a previous search result advances into it.
UCLASS(Blueprintable, meta = (DisplayName = "Pose Matching: Block Transition"))
class POSESEARCH_API UAnimNotifyState_PoseSearchBlockTransition : public UAnimNotifyState_PoseSearchBase
{
	GENERATED_BODY()
};

// Pose matching cost will be affected by this, making the animation segment more or less likely to be selected based
// on the notify parameters
UCLASS(Blueprintable, meta = (DisplayName = "Pose Matching: Override Base Cost Bias"))
// @todo: rename into UAnimNotifyState_PoseSearchOverrideBaseCostBias
class POSESEARCH_API UAnimNotifyState_PoseSearchModifyCost : public UAnimNotifyState_PoseSearchBase
{
	GENERATED_BODY()

public:

	// A negative value reduces the cost and makes the segment more likely to be chosen. A positive value, conversely,
	// makes the segment less likely to be chosen
	UPROPERTY(EditAnywhere, Category = Config, meta = (DisplayName = "Modifier"))
	float CostAddend = -1.0f;
};

// Pose matching cost for the continuing pose will be affected by this, making the animation segment more or less 
// likely to be continuing playing based on the notify parameters
UCLASS(Blueprintable, meta = (DisplayName = "Pose Matching: Override Continuing Pose Cost Bias"))
class POSESEARCH_API UAnimNotifyState_PoseSearchOverrideContinuingPoseCostBias : public UAnimNotifyState_PoseSearchBase
{
	GENERATED_BODY()

public:

	// A negative value reduces the cost and makes the segment more likely to continuing playing. A positive value, conversely,
	// makes the segment less likely to continuing playing
	UPROPERTY(EditAnywhere, Category = Config, meta = (DisplayName = "Modifier"))
	float CostAddend = -1.0f;
};