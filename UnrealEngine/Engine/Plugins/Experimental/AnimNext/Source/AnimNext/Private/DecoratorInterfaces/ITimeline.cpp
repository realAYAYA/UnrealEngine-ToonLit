// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorInterfaces/ITimeline.h"

#include "DecoratorBase/ExecutionContext.h"

namespace UE::AnimNext
{
	double ITimeline::GetPlayRate(FExecutionContext& Context, const TDecoratorBinding<ITimeline>& Binding) const
	{
		TDecoratorBinding<ITimeline> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.GetPlayRate(Context);
		}

		return 1.0;
	}
}
