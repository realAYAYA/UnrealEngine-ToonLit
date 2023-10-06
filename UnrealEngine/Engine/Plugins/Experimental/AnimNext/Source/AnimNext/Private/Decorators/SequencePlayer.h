// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "DecoratorBase/Decorator.h"
#include "DecoratorInterfaces/IEvaluate.h"
#include "DecoratorInterfaces/ITimeline.h"
#include "DecoratorInterfaces/IUpdate.h"

#include "SequencePlayer.generated.h"

class UAnimSequence;

USTRUCT()
struct FAnimNextSequencePlayerDecoratorSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UAnimSequence> AnimSeq;

	UPROPERTY()
	double PlayRate = 1.0;
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
			double CurrentTime = 0.0;
		};

		// IEvaluate impl
		virtual void PreEvaluate(FExecutionContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const override;

		// ITimeline impl
		virtual double GetPlayRate(FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding) const override;

		// IUpdate impl
		virtual void PreUpdate(FExecutionContext& Context, const TDecoratorBinding<IUpdate>& Binding) const override;
	};
}
