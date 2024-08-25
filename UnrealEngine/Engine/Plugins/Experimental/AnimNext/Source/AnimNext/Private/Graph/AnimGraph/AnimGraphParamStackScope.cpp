// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimGraphParamStackScope.h"
#include "Param/ParamStack.h"
#include "Animation/AnimNodeBase.h"

namespace UE::AnimNext
{

FAnimGraphParamStackScope::FAnimGraphParamStackScope(const FAnimationBaseContext& InContext)
{
	TWeakPtr<FParamStack> ExternalParamStack = FParamStack::GetForCurrentThread();
	if(!ExternalParamStack.IsValid())
	{
		// If we havent already attached this thread, we should be using the pending object's stack
		ComponentObject = InContext.GetAnimInstanceObject()->GetOuter();
		if (ComponentObject == nullptr || !FParamStack::AttachToCurrentThreadForPendingObject(ComponentObject.Get()))
		{
			// No pending stack, we should use an owned one
			OwnedParamStack = MakeShared<FParamStack>();
			FParamStack::AttachToCurrentThread(OwnedParamStack);
		}
		else
		{
			bAttachedPending = true;
		}
	}
}

FAnimGraphParamStackScope::~FAnimGraphParamStackScope()
{
	// Check ownership invariance if we have an owned stack
	if(OwnedParamStack.IsValid())
	{
		TWeakPtr<FParamStack> DetachedStack = FParamStack::DetachFromCurrentThread();
		check(DetachedStack.Pin() == OwnedParamStack);
		DetachedStack.Reset();
		check(OwnedParamStack.IsUnique());
		OwnedParamStack.Reset();
	}
	else if(bAttachedPending)
	{
		check(ComponentObject.IsValid());
		FParamStack::DetachFromCurrentThreadForPendingObject(ComponentObject.Get());
	}
}

}