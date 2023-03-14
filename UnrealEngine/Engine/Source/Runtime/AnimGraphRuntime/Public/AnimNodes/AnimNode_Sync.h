// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSync.h"
#include "AnimNode_Sync.generated.h"

USTRUCT(BlueprintInternalUseOnly)
struct ANIMGRAPHRUNTIME_API FAnimNode_Sync : public FAnimNode_Base
{
	GENERATED_BODY()

	friend class UAnimGraphNode_Sync;

public:
	// Get the group name that we synchronize with
	FName GetGroupName() const { return GroupName; }

private:
	UPROPERTY(EditAnywhere, Category = Links)
	FPoseLink Source;

	// The group name that we synchronize with. All nodes employing sync beyond this in the anim graph will implicitly use this sync group.
	UPROPERTY(EditAnywhere, Category = Settings)
	FName GroupName;

	// The role this player can assume within the group
	UPROPERTY(EditAnywhere, Category = Settings)
	TEnumAsByte<EAnimGroupRole::Type> GroupRole = EAnimGroupRole::CanBeLeader;

private:
	// FAnimNode_Base
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	virtual void GatherDebugData(FNodeDebugData& DebugData) override;
};