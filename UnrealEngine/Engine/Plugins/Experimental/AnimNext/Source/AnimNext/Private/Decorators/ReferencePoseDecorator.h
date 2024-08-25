// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/Decorator.h"
#include "DecoratorInterfaces/IEvaluate.h"

#include "ReferencePoseDecorator.generated.h"

// TODO: Ideally, the reference pose we output should be a tag and the task that consumes the reference pose should
// be the one to determine whether it should be in local space or the additive identity (or something else).
// This way, we have a single option in the graph, and the system can figure out what to use removing the room the user mistakes.
// It will also be more efficient since we don't need to manipulate the reference pose until the point of use where we might
// be able to avoid the copy.

UENUM()
enum class EAnimNextReferencePoseType : int32
{
	MeshLocalSpace,
	AdditiveIdentity,
};

USTRUCT(meta = (DisplayName = "Reference Pose"))
struct FAnimNextReferencePoseDecoratorSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	/** The type of the reference pose. */
	UPROPERTY(EditAnywhere, Category = "Default", meta = (Inline))
	EAnimNextReferencePoseType ReferencePoseType = EAnimNextReferencePoseType::MeshLocalSpace;
};

namespace UE::AnimNext
{
	/**
	 * FReferencePoseDecorator
	 * 
	 * A decorator that outputs a reference pose.
	 */
	struct FReferencePoseDecorator : FBaseDecorator, IEvaluate
	{
		DECLARE_ANIM_DECORATOR(FReferencePoseDecorator, 0xc03d6afc, FBaseDecorator)

		using FSharedData = FAnimNextReferencePoseDecoratorSharedData;

		// IEvaluate impl
		virtual void PreEvaluate(FEvaluateTraversalContext& Context, const TDecoratorBinding<IEvaluate>& Binding) const override;
	};
}
