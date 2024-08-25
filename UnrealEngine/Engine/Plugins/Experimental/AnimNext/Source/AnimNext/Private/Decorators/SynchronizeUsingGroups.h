// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimationAsset.h"
#include "DecoratorBase/Decorator.h"
#include "DecoratorInterfaces/IGroupSynchronization.h"
#include "DecoratorInterfaces/IUpdate.h"
#include "DecoratorInterfaces/ITimeline.h"

#include "SynchronizeUsingGroups.generated.h"

USTRUCT(meta = (DisplayName = "Synchronize Using Groups"))
struct FAnimNextSynchronizeUsingGroupsDecoratorSharedData : public FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	// The group name
	// If no name is provided, this decorator is inactive
	UPROPERTY(EditAnywhere, Category = "Default", meta = (Inline))
	FName GroupName;

	// The role this player can assume within the group
	UPROPERTY(EditAnywhere, Category = "Default", meta = (Inline))
	TEnumAsByte<EAnimGroupRole::Type> GroupRole = EAnimGroupRole::CanBeLeader;
};

namespace UE::AnimNext
{
	/**
	 * FSynchronizeUsingGroupsDecorator
	 * 
	 * A decorator that synchronizes animation sequence playback using named groups.
	 */
	struct FSynchronizeUsingGroupsDecorator : FAdditiveDecorator, IUpdate, IGroupSynchronization, ITimeline
	{
		DECLARE_ANIM_DECORATOR(FSynchronizeUsingGroupsDecorator, 0x09b4c174, FAdditiveDecorator)

		using FSharedData = FAnimNextSynchronizeUsingGroupsDecoratorSharedData;

		struct FInstanceData : FDecoratorInstanceData
		{
			bool bFreezeTimeline = false;
		};

		// IUpdate impl
		virtual void PreUpdate(FUpdateTraversalContext& Context, const TDecoratorBinding<IUpdate>& Binding, const FDecoratorUpdateState& DecoratorState) const override;

		// IGroupSynchronization impl
		virtual FName GetGroupName(const FExecutionContext& Context, const TDecoratorBinding<IGroupSynchronization>& Binding) const override;
		virtual EAnimGroupRole::Type GetGroupRole(const FExecutionContext& Context, const TDecoratorBinding<IGroupSynchronization>& Binding) const override;
		virtual float AdvanceBy(const FExecutionContext& Context, const TDecoratorBinding<IGroupSynchronization>& Binding, float DeltaTime) const override;
		virtual void AdvanceToRatio(const FExecutionContext& Context, const TDecoratorBinding<IGroupSynchronization>& Binding, float ProgressRatio) const override;

		// ITimeline impl
		virtual float AdvanceBy(const FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding, float DeltaTime) const override;
		virtual void AdvanceToRatio(const FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding, float ProgressRatio) const override;
	};
}
