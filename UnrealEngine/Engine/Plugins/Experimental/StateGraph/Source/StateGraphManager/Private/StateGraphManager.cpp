// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateGraphManager.h"

#include "Modules/ModuleManager.h"
#include "StateGraph.h"

class FStateGraphManagerModule : public IModuleInterface
{
};

IMPLEMENT_MODULE(FStateGraphManagerModule, StateGraphManager);

namespace UE
{

FStateGraphManager::~FStateGraphManager()
{
}

void FStateGraphManager::AddCreateDelegate(const FStateGraphManagerCreateDelegate& Delegate)
{
	CreateDelegates.Add(Delegate);
}

UE::FStateGraphPtr FStateGraphManager::Create(const FString& ContextName)
{
	UE::FStateGraphPtr StateGraph = MakeShared<UE::FStateGraph>(GetStateGraphName(), ContextName);
	StateGraph->Initialize();

	for (int32 Index = 0; Index < CreateDelegates.Num(); Index++)
	{
		FStateGraphManagerCreateDelegate& Delegate = CreateDelegates[Index];
		if (!Delegate.IsBound())
		{
			// Cleanup stale delegates.
			CreateDelegates.RemoveAt(Index--);
		}
		else if (!Delegate.Execute(*StateGraph.Get()))
		{
			StateGraph.Reset();
			break;
		}
	}

	return StateGraph;
}

} // UE
