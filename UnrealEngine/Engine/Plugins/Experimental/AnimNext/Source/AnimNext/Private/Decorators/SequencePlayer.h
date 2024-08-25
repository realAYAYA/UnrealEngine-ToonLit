// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/Decorator.h"
#include "DecoratorInterfaces/IEvaluate.h"
#include "DecoratorInterfaces/ITimeline.h"
#include "DecoratorInterfaces/IUpdate.h"
#include "Animation/AnimSequence.h"

#include "SequencePlayer.generated.h"

USTRUCT(meta = (DisplayName = "Sequence Player"))
struct FAnimNextSequencePlayerDecoratorSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	/** The sequence to play. */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (Inline))
	TObjectPtr<UAnimSequence> AnimSequence;

	/** The play rate multiplier at which this sequence plays. */
	UPROPERTY(EditAnywhere, Category = "Default")
	float PlayRate = 1.0f;

	/** The time at which we should start playing this sequence. */
	UPROPERTY(EditAnywhere, Category = "Default")
	float StartPosition = 0.0f;

	/** Whether or not this sequence playback will loop. */
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bLoop = 0.0f;

	// Latent pin support boilerplate
	#define DECORATOR_LATENT_PROPERTIES_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(PlayRate) \
		GeneratorMacro(StartPosition) \
		GeneratorMacro(bLoop) \

	GENERATE_DECORATOR_LATENT_PROPERTIES(FAnimNextSequencePlayerDecoratorSharedData, DECORATOR_LATENT_PROPERTIES_ENUMERATOR)
	#undef DECORATOR_LATENT_PROPERTIES_ENUMERATOR
};

namespace UE::AnimNext
{
	/**
	 * FSequencePlayerDecorator
	 * 
	 * A decorator that can play an animation sequence.
	 */
	struct FSequencePlayerDecorator : FBaseDecorator, IEvaluate, ITimeline, IUpdate
	{
		DECLARE_ANIM_DECORATOR(FSequencePlayerDecorator, 0xa628ad12, FBaseDecorator)

		using FSharedData = FAnimNextSequencePlayerDecoratorSharedData;

		struct FInstanceData : FDecorator::FInstanceData
		{
			float InternalTimeAccumulator = 0.0f;

			void Construct(const FExecutionContext& Context, const FDecoratorBinding& Binding);
		};

		// IEvaluate impl
		virtual void PreEvaluate(FEvaluateTraversalContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const override;

		// ITimeline impl
		virtual float GetPlayRate(const FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding) const override;
		virtual float AdvanceBy(const FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding, float DeltaTime) const override;
		virtual void AdvanceToRatio(const FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding, float ProgressRatio) const override;

		// IUpdate impl
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const override;
	};
}
