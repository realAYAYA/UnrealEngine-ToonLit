// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorInterfaces/ISmoothBlend.h"

#include "DecoratorBase/ExecutionContext.h"

namespace UE::AnimNext
{
	float ISmoothBlend::GetBlendTime(const FExecutionContext& Context, const TDecoratorBinding<ISmoothBlend>& Binding, int32 ChildIndex) const
	{
		TDecoratorBinding<ISmoothBlend> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.GetBlendTime(Context, ChildIndex);
		}

		return 0.0f;
	}
}
