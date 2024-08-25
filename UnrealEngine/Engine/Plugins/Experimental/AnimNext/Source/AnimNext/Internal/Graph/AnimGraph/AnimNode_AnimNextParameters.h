// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "AnimNode_AnimNextParameters.generated.h"

namespace UE::AnimNext
{
	struct FParameterBlockProxy;
}

class UAnimNextParameterBlock;

USTRUCT(BlueprintInternalUseOnly)
struct FAnimNode_AnimNextParameters : public FAnimNode_Base
{
	GENERATED_BODY()

	friend class UAnimGraphNode_AnimNextParameters;

	ANIMNEXT_API FAnimNode_AnimNextParameters();
	FAnimNode_AnimNextParameters(const FAnimNode_AnimNextParameters& InOther);
	FAnimNode_AnimNextParameters& operator=(const FAnimNode_AnimNextParameters& InOther);
	FAnimNode_AnimNextParameters(FAnimNode_AnimNextParameters&& InOther) noexcept;
	FAnimNode_AnimNextParameters& operator=(FAnimNode_AnimNextParameters&& InOther) noexcept;
	ANIMNEXT_API virtual ~FAnimNode_AnimNextParameters() override;

private:
	UPROPERTY(EditAnywhere, Category = Links)
	FPoseLink Source;

	UPROPERTY(EditAnywhere, Category = Settings)
	TObjectPtr<UAnimNextParameterBlock> Parameters;

	// Cache previous param block so we know when it changes via pin
	TObjectPtr<UAnimNextParameterBlock> PreviousParameters;

	// Cached proxy
	TUniquePtr<UE::AnimNext::FParameterBlockProxy> ParametersProxy;

private:
	// FAnimNode_Base
	ANIMNEXT_API virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	ANIMNEXT_API virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	ANIMNEXT_API virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	ANIMNEXT_API virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	ANIMNEXT_API virtual void GatherDebugData(FNodeDebugData& DebugData) override;
};
