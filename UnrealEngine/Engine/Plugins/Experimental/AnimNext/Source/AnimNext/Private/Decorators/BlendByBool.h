// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DecoratorBase/Decorator.h"
#include "DecoratorInterfaces/IDiscreteBlend.h"
#include "DecoratorInterfaces/IHierarchy.h"
#include "DecoratorInterfaces/IUpdate.h"

#include "BlendByBool.generated.h"

USTRUCT(meta = (DisplayName = "Blend By Bool"))
struct FAnimNextBlendByBoolDecoratorSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	/** First output to be blended. */
	UPROPERTY()
	FAnimNextDecoratorHandle TrueChild;

	/** Second output to be blended. */
	UPROPERTY()
	FAnimNextDecoratorHandle FalseChild;

	/** The boolean condition that decides which child is active. */
	UPROPERTY(EditAnywhere, Category = "Default")
	bool bCondition = false;

	// Latent pin support boilerplate
	#define DECORATOR_LATENT_PROPERTIES_ENUMERATOR(GeneratorMacro) \
		GeneratorMacro(bCondition) \

	GENERATE_DECORATOR_LATENT_PROPERTIES(FAnimNextBlendByBoolDecoratorSharedData, DECORATOR_LATENT_PROPERTIES_ENUMERATOR)
	#undef DECORATOR_LATENT_PROPERTIES_ENUMERATOR
};

namespace UE::AnimNext
{
	/**
	 * FBlendByBoolDecorator
	 * 
	 * A decorator that can blend two discrete inputs through a boolean.
	 */
	struct FBlendByBoolDecorator : FBaseDecorator, IUpdate, IHierarchy, IDiscreteBlend
	{
		DECLARE_ANIM_DECORATOR(FBlendByBoolDecorator, 0xc6d8c9ea, FBaseDecorator)

		using FSharedData = FAnimNextBlendByBoolDecoratorSharedData;

		struct FInstanceData : FDecorator::FInstanceData
		{
			FDecoratorPtr TrueChild;
			FDecoratorPtr FalseChild;

			int32 PreviousChildIndex = INDEX_NONE;
		};

		// IUpdate impl
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const override;
		virtual void QueueChildrenForTraversal(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState, FUpdateTraversalQueue& TraversalQueue) const override;

		// IHierarchy impl
		virtual uint32 GetNumChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding) const override;
		virtual void GetChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding, FChildrenArray& Children) const override;

		// IDiscreteBlend impl
		virtual float GetBlendWeight(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const override;
		virtual int32 GetBlendDestinationChildIndex(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding) const override;
		virtual void OnBlendTransition(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const override;
		virtual void OnBlendInitiated(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const override;
		virtual void OnBlendTerminated(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const override;
	};
}
