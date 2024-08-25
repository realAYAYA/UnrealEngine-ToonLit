// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorInterfaces/IGroupSynchronization.h"

#include "DecoratorBase/ExecutionContext.h"

namespace UE::AnimNext
{
	FName IGroupSynchronization::GetGroupName(const FExecutionContext& Context, const TDecoratorBinding<IGroupSynchronization>& Binding) const
	{
		TDecoratorBinding<IGroupSynchronization> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.GetGroupName(Context);
		}

		return NAME_None;
	}

	EAnimGroupRole::Type IGroupSynchronization::GetGroupRole(const FExecutionContext& Context, const TDecoratorBinding<IGroupSynchronization>& Binding) const
	{
		TDecoratorBinding<IGroupSynchronization> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.GetGroupRole(Context);
		}

		return EAnimGroupRole::CanBeLeader;
	}

	float IGroupSynchronization::AdvanceBy(const FExecutionContext& Context, const TDecoratorBinding<IGroupSynchronization>& Binding, float DeltaTime) const
	{
		TDecoratorBinding<IGroupSynchronization> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.AdvanceBy(Context, DeltaTime);
		}

		return 0.0f;
	}

	void IGroupSynchronization::AdvanceToRatio(const FExecutionContext& Context, const TDecoratorBinding<IGroupSynchronization>& Binding, float ProgressRatio) const
	{
		TDecoratorBinding<IGroupSynchronization> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.AdvanceToRatio(Context, ProgressRatio);
		}
	}
}
