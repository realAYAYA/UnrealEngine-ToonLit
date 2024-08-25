// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Animation/AnimationAsset.h"
#include "Curves/CurveFloat.h"
#include "DecoratorBase/Decorator.h"
#include "DecoratorInterfaces/IDiscreteBlend.h"
#include "DecoratorInterfaces/IEvaluate.h"
#include "DecoratorInterfaces/IUpdate.h"

#include "BlendSmootherPerBone.generated.h"

class UBlendProfile;

USTRUCT(meta = (DisplayName = "Blend Smoother Per Bone"))
struct FAnimNextBlendSmootherPerBoneDecoratorSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	/** Blend profile that configures how fast to blend each bone. */
	// TODO: Can't show list of blend profiles, we need to find a skeleton to perform the lookup with
	UPROPERTY(EditAnywhere, Category = "Default", meta = (Inline, UseAsBlendProfile=true))
	TObjectPtr<UBlendProfile> BlendProfile = nullptr;
};

namespace UE::AnimNext
{
	/**
	 * FBlendSmootherDecorator
	 * 
	 * A decorator that smoothly blends between discrete states over time.
	 */
	struct FBlendSmootherPerBoneDecorator : FAdditiveDecorator, IEvaluate, IUpdate, IDiscreteBlend
	{
		DECLARE_ANIM_DECORATOR(FBlendSmootherPerBoneDecorator, 0x3d4e213f, FAdditiveDecorator)

		using FSharedData = FAnimNextBlendSmootherPerBoneDecoratorSharedData;

		// Struct for tracking blends for each pose
		struct FBlendData
		{
			// Which blend alpha we started the blend with
			float StartAlpha = 0.0f;
		};

		struct FInstanceData : FDecorator::FInstanceData
		{
			// Blend state per child
			TArray<FBlendData> PerChildBlendData;

			// Per-bone blending data for each child
			TArray<FBlendSampleData> PerBoneSampleData;
		};

		// IEvaluate impl
		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const override;

		// IUpdate impl
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const override;

		// IDiscreteBlend impl
		virtual void OnBlendTransition(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const override;

		// Internal impl
		static void InitializeInstanceData(const FExecutionContext& Context, const FDecoratorBinding& Binding, const FSharedData* SharedData, FInstanceData* InstanceData);
	};
}
