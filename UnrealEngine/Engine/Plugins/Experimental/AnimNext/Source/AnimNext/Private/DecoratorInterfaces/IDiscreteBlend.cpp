// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorInterfaces/IDiscreteBlend.h"

#include "DecoratorBase/ExecutionContext.h"

namespace UE::AnimNext
{
	float IDiscreteBlend::GetBlendWeight(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		TDecoratorBinding<IDiscreteBlend> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.GetBlendWeight(Context, ChildIndex);
		}

		return -1.0f;
	}

	const FAlphaBlend* IDiscreteBlend::GetBlendState(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		TDecoratorBinding<IDiscreteBlend> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.GetBlendState(Context, ChildIndex);
		}

		return nullptr;
	}

	int32 IDiscreteBlend::GetBlendDestinationChildIndex(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding) const
	{
		TDecoratorBinding<IDiscreteBlend> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.GetBlendDestinationChildIndex(Context);
		}

		return INDEX_NONE;
	}

	void IDiscreteBlend::OnBlendTransition(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 OldChildIndex, int32 NewChildIndex) const
	{
		TDecoratorBinding<IDiscreteBlend> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.OnBlendTransition(Context, OldChildIndex, NewChildIndex);
		}
	}

	void IDiscreteBlend::OnBlendInitiated(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		TDecoratorBinding<IDiscreteBlend> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.OnBlendInitiated(Context, ChildIndex);
		}
	}

	void IDiscreteBlend::OnBlendTerminated(const FExecutionContext& Context, const TDecoratorBinding<IDiscreteBlend>& Binding, int32 ChildIndex) const
	{
		TDecoratorBinding<IDiscreteBlend> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.OnBlendTerminated(Context, ChildIndex);
		}
	}
}
