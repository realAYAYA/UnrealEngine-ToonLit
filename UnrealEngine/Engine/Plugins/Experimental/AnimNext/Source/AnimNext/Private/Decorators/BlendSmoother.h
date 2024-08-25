// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AlphaBlend.h"
#include "DecoratorBase/Decorator.h"
#include "DecoratorInterfaces/IDiscreteBlend.h"
#include "DecoratorInterfaces/IEvaluate.h"
#include "DecoratorInterfaces/ISmoothBlend.h"
#include "DecoratorInterfaces/IUpdate.h"

#include "BlendSmoother.generated.h"

class UCurveFloat;

USTRUCT(meta = (DisplayName = "Blend Smoother"))
struct FAnimNextBlendSmootherDecoratorSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	/** How long to take when blending into each child. */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (Inline))
	TArray<float> BlendTimes;

	/** What type of blend equation to use when converting the time elapsed into a blend weight. */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (Inline))
	EAlphaBlendOption BlendType = EAlphaBlendOption::Linear;

	/** Custom curve to use when the Custom blend type is used. */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (Inline))
	TObjectPtr<UCurveFloat> CustomBlendCurve;
};

namespace UE::AnimNext
{
	/**
	 * FBlendSmootherDecorator
	 * 
	 * A decorator that smoothly blends between discrete states over time.
	 */
	struct FBlendSmootherDecorator : FAdditiveDecorator, IEvaluate, IUpdate, IDiscreteBlend, ISmoothBlend
	{
		DECLARE_ANIM_DECORATOR(FBlendSmootherDecorator, 0x7b6c3d2e, FAdditiveDecorator)

		using FSharedData = FAnimNextBlendSmootherDecoratorSharedData;

		// Struct for tracking blends for each pose
		struct FBlendData
		{
			// Helper struct to update a time based weight
			FAlphaBlend Blend;

			// Current child weight (normalized with all children)
			float Weight = 0.0f;

			// Whether or not this child is actively blending
			bool bIsBlending = false;
		};

		struct FInstanceData : FDecorator::FInstanceData
		{
			// Blend state per child
			TArray<FBlendData> PerChildBlendData;
		};

		// IEvaluate impl
		virtual void PostEvaluate(FEvaluateTraversalContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const override;

		// IUpdate impl
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const override;

		// IDiscreteBlend impl
		virtual float GetBlendWeight(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const override;
		virtual const FAlphaBlend* GetBlendState(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const override;
		virtual void OnBlendTransition(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const override;

		// ISmoothBlend impl
		virtual float GetBlendTime(const FExecutionContext& Context, const TDecoratorBinding<ISmoothBlend>& Binding, int32 ChildIndex) const override;

		// Internal impl
		static void InitializeInstanceData(const FExecutionContext& Context, const FDecoratorBinding& Binding, const FSharedData* SharedData, FInstanceData* InstanceData);
	};
}
