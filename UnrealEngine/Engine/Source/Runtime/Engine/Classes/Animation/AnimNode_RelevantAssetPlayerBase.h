// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNode_RelevantAssetPlayerBase.generated.h"

/* Base class for any asset playing anim node */
USTRUCT(BlueprintInternalUseOnly)
struct ENGINE_API FAnimNode_AssetPlayerRelevancyBase : public FAnimNode_Base
{
	GENERATED_BODY()

public:
	/** Get the animation asset associated with the node, derived classes should implement this */
	virtual class UAnimationAsset* GetAnimAsset() const;

	/** Get the currently referenced time within the asset player node */
	virtual float GetAccumulatedTime() const;

	/** Override the currently accumulated time */
	virtual void SetAccumulatedTime(float NewTime);

	// Functions to report data to getters, this is required for all asset players (but can't be pure abstract because of struct instantiation generated code).
	virtual float GetCurrentAssetLength() const;
	virtual float GetCurrentAssetTime() const;
	virtual float GetCurrentAssetTimePlayRateAdjusted() const;

	// Check whether this node should be ignored when testing for relevancy in state machines
	virtual bool GetIgnoreForRelevancyTest() const;

	// Set whether this node should be ignored when testing for relevancy in state machines
	virtual bool SetIgnoreForRelevancyTest(bool bInIgnoreForRelevancyTest);

	/** Get the last encountered blend weight for this node */
	virtual float GetCachedBlendWeight() const;

	/** Set the cached blendweight to zero */
	virtual void ClearCachedBlendWeight();
};
