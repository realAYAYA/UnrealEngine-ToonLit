// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorInterfaces/IHierarchy.h"

#include "DecoratorBase/ExecutionContext.h"

namespace UE::AnimNext
{
	uint32 IHierarchy::GetNumChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding) const
	{
		TDecoratorBinding<IHierarchy> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			return SuperBinding.GetNumChildren(Context);
		}

		return 0;
	}

	void IHierarchy::GetChildren(const FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding, FChildrenArray& Children) const
	{
		TDecoratorBinding<IHierarchy> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.GetChildren(Context, Children);
		}
	}
}
