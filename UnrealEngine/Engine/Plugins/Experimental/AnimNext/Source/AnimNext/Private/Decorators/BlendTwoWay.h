// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DecoratorBase/Decorator.h"
#include "DecoratorInterfaces/IEvaluate.h"
#include "DecoratorInterfaces/IHierarchy.h"
#include "DecoratorInterfaces/IUpdate.h"

#include "BlendTwoWay.generated.h"

USTRUCT()
struct FAnimNextBlendTwoWayDecoratorSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	UPROPERTY()
	FAnimNextDecoratorHandle Children[2];

	UPROPERTY()
	double BlendWeight = 0.0;
};

namespace UE::AnimNext
{
	/**
	 * FBlendTwoWayDecorator
	 * 
	 * A decorator that can blend two inputs.
	 */
	struct FBlendTwoWayDecorator : FBaseDecorator, IEvaluate, IUpdate, IHierarchy
	{
		DECLARE_ANIM_DECORATOR(FBlendTwoWayDecorator, 0x96a81d1e, FBaseDecorator)

		using FSharedData = FAnimNextBlendTwoWayDecoratorSharedData;

		struct FInstanceData : FDecorator::FInstanceData
		{
			FDecoratorPtr Children[2];
		};

		// IEvaluate impl
		virtual void PostEvaluate(FExecutionContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const override;

		// IUpdate impl
		virtual void PreUpdate(FExecutionContext& Context, const TDecoratorBinding<IUpdate>& Binding) const override;

		// IHierarchy impl
		virtual void GetChildren(FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding, FChildrenArray& Children) const override;
	};
}
