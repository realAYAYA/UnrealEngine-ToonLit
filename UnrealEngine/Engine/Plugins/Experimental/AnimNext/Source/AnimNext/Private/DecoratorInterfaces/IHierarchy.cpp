// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorInterfaces/IHierarchy.h"

#include "DecoratorBase/ExecutionContext.h"

namespace UE::AnimNext
{
	void IHierarchy::GetChildren(FExecutionContext& Context, const TDecoratorBinding<IHierarchy>& Binding, FChildrenArray& Children) const
	{
		TDecoratorBinding<IHierarchy> SuperBinding;
		if (Context.GetInterfaceSuper(Binding, SuperBinding))
		{
			SuperBinding.GetChildren(Context, Children);
		}
	}
}
