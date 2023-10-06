// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimCustomInstanceHelper.h"
#include "Animation/AnimInstanceProxy.h"
#include "AnimNodes/AnimNode_CopyPoseFromMesh.h"
#include "AnimPreviewAttacheInstance.generated.h"

/** Proxy override for this UAnimInstance-derived class */
USTRUCT()
struct FAnimPreviewAttacheInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

public:
	FAnimPreviewAttacheInstanceProxy()
	{
	}

	FAnimPreviewAttacheInstanceProxy(UAnimInstance* InAnimInstance)
		: FAnimInstanceProxy(InAnimInstance)
	{
	}

	virtual void Initialize(UAnimInstance* InAnimInstance) override;
	virtual void PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds) override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& InContext) override;
	virtual bool Evaluate(FPoseContext& Output) override;

private:
	/** Pose blend node for evaluating pose assets (for previewing curve sources) */
	FAnimNode_CopyPoseFromMesh CopyPoseFromMesh;
};

/**
 * This Instance only contains one AnimationAsset, and produce poses
 * Used by Preview in AnimGraph, Playing single animation in Kismet2 and etc
 */

UCLASS(transient, NotBlueprintable, noteditinlinenew)
class ANIMGRAPH_API UAnimPreviewAttacheInstance : public UAnimInstance
{
	GENERATED_UCLASS_BODY()

	//~ Begin UAnimInstance Interface
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
	//~ End UAnimInstance Interface
};



