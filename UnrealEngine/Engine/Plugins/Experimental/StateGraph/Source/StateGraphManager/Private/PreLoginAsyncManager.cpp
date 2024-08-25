// Copyright Epic Games, Inc. All Rights Reserved.

#include "PreLoginAsyncManager.h"

namespace UE::PreLoginAsync
{
	const FName StateGraphName("PreLoginAsync");
	const FName OptionsName("Options");
} // UE::PreLoginAsync

#if WITH_SERVER_CODE

void UPreLoginAsyncManager::InitializeStateGraph(UE::FStateGraph& StateGraph, const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, const AGameModeBase::FOnPreLoginCompleteDelegate& OnComplete)
{
	UE::FStateGraphRef* OldStateGraph = RunningStateGraphs.Find(UniqueId);
	if (OldStateGraph)
	{
		UE_LOG(LogStateGraph, Warning, TEXT("[%s] Duplicate login detected"), *StateGraph.GetContextName());
		CompleteLogin((*OldStateGraph).Get(), TEXT("Duplicate login detected"));
	}

	RunningStateGraphs.Emplace(UniqueId, StateGraph.AsShared());

	StateGraph.OnStatusChanged.AddWeakLambda(this,
		[this](UE::FStateGraph& StateGraph, UE::FStateGraph::EStatus OldStatus, UE::FStateGraph::EStatus NewStatus)
		{
			if (NewStatus == UE::FStateGraph::EStatus::Completed)
			{
				CompleteLogin(StateGraph, FString());
			}
			else if (NewStatus == UE::FStateGraph::EStatus::Blocked)
			{
				CompleteLogin(StateGraph, TEXT("PreLoginAsync blocked"));
			}
			else if (NewStatus == UE::FStateGraph::EStatus::TimedOut)
			{
				CompleteLogin(StateGraph, TEXT("PreLoginAsync timed out"));
			}
		});

	StateGraph.CreateNode<UE::PreLoginAsync::FOptions>(this, Options, Address, UniqueId, OnComplete);
}

void UPreLoginAsyncManager::CompleteLogin(UE::FStateGraph& StateGraph, const FString& Error)
{
	UE_LOG(LogStateGraph, Log, TEXT("[%s] CompleteLogin: %s"), *StateGraph.GetContextName(), Error.IsEmpty() ? TEXT("Success") : *Error);

	UE::PreLoginAsync::FOptionsPtr Options = UE::PreLoginAsync::FOptions::Get(StateGraph);
	if (Options)
	{
		Options->OnComplete.ExecuteIfBound(Error);
		TObjectPtr<UPreLoginAsyncManager> Manager = Options->WeakManager.Get();
		if (Manager)
		{
			Manager->RunningStateGraphs.Remove(Options->UniqueId);
		}
	}
}

#endif // WITH_SERVER_CODE
