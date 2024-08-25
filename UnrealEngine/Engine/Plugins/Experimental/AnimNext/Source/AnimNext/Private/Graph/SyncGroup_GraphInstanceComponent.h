// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimationAsset.h"
#include "DecoratorBase/DecoratorPtr.h"
#include "DecoratorInterfaces/IUpdate.h"
#include "Graph/GraphInstanceComponent.h"

namespace UE::AnimNext
{
	/**
	 * FSyncGroupGraphInstanceComponent
	 *
	 * This component maintains the necessary state to support group based synchronization.
	 */
	struct FSyncGroupGraphInstanceComponent : public FGraphInstanceComponent
	{
		DECLARE_ANIM_GRAPH_INSTANCE_COMPONENT(FSyncGroupGraphInstanceComponent)

		void RegisterWithGroup(FName GroupName, EAnimGroupRole::Type GroupRole, const FWeakDecoratorPtr& DecoratorPtr, const FDecoratorUpdateState& DecoratorState);

		// FGraphInstanceComponent impl
		virtual void PreUpdate(FExecutionContext& Context) override;
		virtual void PostUpdate(FExecutionContext& Context) override;

	private:
		struct FSyncGroupMember
		{
			FDecoratorUpdateState				DecoratorState;
			FWeakDecoratorPtr					DecoratorPtr;
			TEnumAsByte<EAnimGroupRole::Type>	GroupRole;
		};

		struct FSyncGroupState
		{
			TArray<FSyncGroupMember> Members;
		};

		TMap<FName, FSyncGroupState> SyncGroupMap;
	};
}
