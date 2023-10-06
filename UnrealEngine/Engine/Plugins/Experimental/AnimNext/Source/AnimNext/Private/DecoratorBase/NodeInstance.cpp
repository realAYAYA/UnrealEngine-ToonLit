// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/NodeInstance.h"
#include "DecoratorBase/ExecutionContext.h"

namespace UE::AnimNext
{
	FNodeInstance::FNodeInstance(FNodeHandle NodeHandle)
		: ReferenceCount(0)
		, NodeHandle(NodeHandle)
	{
	}

	void FNodeInstance::ReleaseReference()
	{
		check(ReferenceCount > 0);

		if (ReferenceCount-- == 1)
		{
			// The last reference has been released, we can now safely destroy this node
			FExecutionContext* Context = GetThreadExecutionContext();
			check(Context != nullptr);

			Context->ReleaseNodeInstance(this);
		}
	}
}
