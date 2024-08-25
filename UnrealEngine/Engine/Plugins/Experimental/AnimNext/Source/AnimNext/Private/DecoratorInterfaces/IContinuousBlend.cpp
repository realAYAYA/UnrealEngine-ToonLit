// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorInterfaces/IContinuousBlend.h"

#include "DecoratorBase/ExecutionContext.h"

namespace UE::AnimNext
{
	float IContinuousBlend::GetBlendWeight(const FExecutionContext& Context, const TDecoratorBinding<IContinuousBlend>& Binding, int32 ChildIndex) const
	{
		TDecoratorBinding<IContinuousBlend> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.GetBlendWeight(Context, ChildIndex);
		}

		return -1.0f;
	}
}
