// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVisualDebugger/ChaosVDContextProvider.h"

#include "ChaosVisualDebugger/ChaosVisualDebuggerTrace.h"

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

const FChaosVDContext* FChaosVDThreadContext::GetCurrentContext(EChaosVDContextType Type)
{
	if (LocalContextStack.Num() > 0)
	{
		const FChaosVDContext* Context = &LocalContextStack.Top();

		return Context && Context->Type == static_cast<int32>(Type) ? Context : nullptr;
	}
	else
	{
		return nullptr;
	}
}

const FChaosVDContext* FChaosVDThreadContext::GetCurrentContext()
{
	return LocalContextStack.Num() > 0 ? &LocalContextStack.Top() : nullptr;
}

void FChaosVDThreadContext::PushContext(const FChaosVDContext& InContext)
{
	if (!FChaosVisualDebuggerTrace::IsTracing())
	{
		return;
	}

	LocalContextStack.Add(InContext);
}

void FChaosVDThreadContext::PopContext()
{
	if (LocalContextStack.Num() > 0)
	{
		LocalContextStack.Pop();
	}
}
#endif //WITH_CHAOS_VISUAL_DEBUGGER
