// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVisualDebugger/ChaosVDContextProvider.h"

#if WITH_CHAOS_VISUAL_DEBUGGER
bool FChaosVDThreadContext::GetCurrentContext(FChaosVDContext& OutContext)
{
	if (LocalContextStack.Num() > 0)
	{
		OutContext = LocalContextStack.Top();
		return true;
	}

	return false;
}

const FChaosVDContext* FChaosVDThreadContext::GetCurrentContext()
{
	if (LocalContextStack.Num() > 0)
	{
		return &LocalContextStack.Top();
	}
	else
	{
		return nullptr;
	}
}

void FChaosVDThreadContext::PushContext(const FChaosVDContext& InContext)
{
	LocalContextStack.Add(InContext);
}

void FChaosVDThreadContext::PopContext()
{
	LocalContextStack.Pop();
}
#endif //WITH_CHAOS_VISUAL_DEBUGGER
