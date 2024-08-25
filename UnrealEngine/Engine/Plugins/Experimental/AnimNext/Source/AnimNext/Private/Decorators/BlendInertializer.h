// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DecoratorBase/Decorator.h"
#include "DecoratorInterfaces/IDiscreteBlend.h"
#include "DecoratorInterfaces/ISmoothBlend.h"

#include "BlendInertializer.generated.h"

class UBlendProfile;

USTRUCT(meta = (DisplayName = "Blend Inertializer"))
struct FAnimNextBlendInertializerDecoratorSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	/** Blend profile that configures how fast to blend each bone. */
	// TODO: Can't show list of blend profiles, we need to find a skeleton to perform the lookup with
	UPROPERTY(EditAnywhere, Category = "Default", meta = (Inline, UseAsBlendProfile = true))
	TObjectPtr<UBlendProfile> BlendProfile = nullptr;
};

namespace UE::AnimNext
{
	/**
	 * FBlendInertializerDecorator
	 * 
	 * A decorator that converts a normal smooth blend into an inertializing blend.
	 */
	struct FBlendInertializerDecorator : FAdditiveDecorator, IDiscreteBlend, ISmoothBlend
	{
		DECLARE_ANIM_DECORATOR(FBlendInertializerDecorator, 0xa6f83827, FAdditiveDecorator)

		using FSharedData = FAnimNextBlendInertializerDecoratorSharedData;
		using FInstanceData = FDecorator::FInstanceData;

		// IDiscreteBlend impl
		virtual void OnBlendTransition(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const override;

		// ISmoothBlend impl
		virtual float GetBlendTime(const FExecutionContext& Context, const TDecoratorBinding<ISmoothBlend>& Binding, int32 ChildIndex) const override;
	};
}
