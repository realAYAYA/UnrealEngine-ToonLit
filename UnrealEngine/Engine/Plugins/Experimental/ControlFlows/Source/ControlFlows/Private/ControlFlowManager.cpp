// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlFlowManager.h"
#include "Containers/Ticker.h"
#include "ControlFlows.h"

static TArray<TSharedRef<FControlFlowContainerBase>> NewlyCreatedFlows;
static TArray<TSharedRef<FControlFlowContainerBase>> PersistentFlows;
static TArray<TSharedRef<FControlFlowContainerBase>> ExecutingFlows;
static TArray<TSharedRef<FControlFlowContainerBase>> FinishedFlows;

static FTSTicker::FDelegateHandle NextFrameCheckForExecution;
static FTSTicker::FDelegateHandle NextFrameCheckForFlowCleanup;

TArray<TSharedRef<FControlFlowContainerBase>>& FControlFlowStatics::GetNewlyCreatedFlows()
{
	return NewlyCreatedFlows;
}

TArray<TSharedRef<FControlFlowContainerBase>>& FControlFlowStatics::GetPersistentFlows()
{
	return PersistentFlows;
}

TArray<TSharedRef<FControlFlowContainerBase>>& FControlFlowStatics::GetExecutingFlows()
{
	return ExecutingFlows;
}

TArray<TSharedRef<FControlFlowContainerBase>>& FControlFlowStatics::GetFinishedFlows()
{
	return FinishedFlows;
}

void FControlFlowStatics::HandleControlFlowStartedNotification(TSharedRef<const FControlFlow> InFlow)
{
	TArray<TSharedRef<FControlFlowContainerBase>>& NewFlows = GetNewlyCreatedFlows();
	for (size_t Idx = 0; Idx < NewFlows.Num(); ++Idx)
	{
		if (ensureAlways(NewFlows[Idx]->OwningObjectIsValid()))
		{
			if (InFlow == NewFlows[Idx]->GetControlFlow())
			{
				GetExecutingFlows().Add(NewFlows[Idx]);
			}
		}

		NewFlows.RemoveAtSwap(Idx);
		--Idx;
	}

	CheckForInvalidFlows();
}

void FControlFlowStatics::CheckNewlyCreatedFlows()
{
	FTSTicker::GetCoreTicker().RemoveTicker(NextFrameCheckForExecution);
	NextFrameCheckForExecution = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&FControlFlowStatics::IterateThroughNewlyCreatedFlows));
}

void FControlFlowStatics::CheckForInvalidFlows()
{
	FTSTicker::GetCoreTicker().RemoveTicker(NextFrameCheckForFlowCleanup);
	NextFrameCheckForFlowCleanup = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&FControlFlowStatics::IterateForInvalidFlows));
}

bool FControlFlowStatics::IterateThroughNewlyCreatedFlows(float DeltaTime)
{
	TArray<TSharedRef<FControlFlowContainerBase>>& NewFlows = GetNewlyCreatedFlows();
	for (size_t Idx = 0; Idx < NewFlows.Num(); ++Idx)
	{
		if (ensureAlways(NewFlows[Idx]->OwningObjectIsValid()))
		{
			TSharedRef<FControlFlow> NewFlow = NewFlows[Idx]->GetControlFlow();
			if (ensureAlwaysMsgf(NewFlow->IsRunning(), TEXT("Call to execute after queue-ing your steps to avoid this ensure. We will fire the flow 1 frame late to hopefully not cause anything from breaking")))
			{
				GetExecutingFlows().Add(NewFlows[Idx]);
			}
			else
			{
				if (ensureAlwaysMsgf(NewFlow->NumInQueue() > 0, TEXT("We should never have a newly created flow with no steps.")))
				{
					NewFlow->ExecuteFlow();
					GetExecutingFlows().Add(NewFlows[Idx]);
				}
				else
				{
					GetFinishedFlows().Add(NewFlows[Idx]);
					CheckForInvalidFlows();
				}
			}
		}
	}

	NewFlows.Reset();

	return false;
}

bool FControlFlowStatics::IterateForInvalidFlows(float DeltaTime)
{
	//Iterating through Persistent Flows
	{
		TArray<TSharedRef<FControlFlowContainerBase>>& Persistent = GetPersistentFlows();
		for (size_t Idx = 0; Idx < Persistent.Num(); ++Idx)
		{
			if (Persistent[Idx]->OwningObjectIsValid())
			{
				TSharedRef<FControlFlow> PersistentFlow = Persistent[Idx]->GetControlFlow();
				if (PersistentFlow->IsRunning())
				{
					ExecutingFlows.Add(Persistent[Idx]);
					Persistent.RemoveAtSwap(Idx);
					--Idx;
				}
			}
			else
			{
				Persistent.RemoveAtSwap(Idx);
				--Idx;
			}
		}
	}

	//Iterating through Executing Flows
	{
		TArray<TSharedRef<FControlFlowContainerBase>>& Executing = GetExecutingFlows();
		for (size_t Idx = 0; Idx < Executing.Num(); ++Idx)
		{
			if (Executing[Idx]->OwningObjectIsValid())
			{
				TSharedRef<FControlFlow> ExecutingFlow = Executing[Idx]->GetControlFlow();
				if (!ExecutingFlow->IsRunning() && ensureAlways(ExecutingFlow->NumInQueue() == 0))
				{
					Executing[Idx]->ControlFlow->Activity = nullptr;
					FinishedFlows.Add(Executing[Idx]);
					Executing.RemoveAtSwap(Idx);
					--Idx;
				}
			}
			else
			{
				Executing[Idx]->ControlFlow->Activity = nullptr;
				Executing.RemoveAtSwap(Idx);
				--Idx;
			}
		}
	}

	//Iterating through Completed Flows
	{
		TArray<TSharedRef<FControlFlowContainerBase>>& Completed = GetFinishedFlows();
		for (size_t Idx = 0; Idx < Completed.Num(); ++Idx)
		{
			if (!Completed[Idx]->OwningObjectIsValid())
			{
				UE_LOG(LogControlFlows, Warning, TEXT("Owning Object for completed flow is not valid!"));
			}
			
			TSharedRef<FControlFlow> CompletedFlow = Completed[Idx]->GetControlFlow();

			ensureAlwaysMsgf(!CompletedFlow->IsRunning() && CompletedFlow->NumInQueue() == 0, TEXT("Completed Flow (%s) still has items in it's queue"), *CompletedFlow->GetDebugName());

			Completed.RemoveAtSwap(Idx);
			--Idx;
		}
	}

	return false;
}
