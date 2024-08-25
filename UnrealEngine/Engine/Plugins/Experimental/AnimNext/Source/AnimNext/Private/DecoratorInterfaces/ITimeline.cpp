// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorInterfaces/ITimeline.h"

#include "DecoratorBase/ExecutionContext.h"

namespace UE::AnimNext
{
	float ITimeline::GetPlayRate(const FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding) const
	{
		TDecoratorBinding<ITimeline> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.GetPlayRate(Context);
		}

		return 1.0f;
	}

	float ITimeline::AdvanceBy(const FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding, float DeltaTime) const
	{
		TDecoratorBinding<ITimeline> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.AdvanceBy(Context, DeltaTime);
		}

		return 0.0f;
	}

	void ITimeline::AdvanceToRatio(const FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding, float ProgressRatio) const
	{
		TDecoratorBinding<ITimeline> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.AdvanceToRatio(Context, ProgressRatio);
		}
	}
}
