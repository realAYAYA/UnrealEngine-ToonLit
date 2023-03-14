// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlFlowConcurrency.h"
#include "ControlFlow.h"
#include "ControlFlows.h"

FConcurrencySubFlowContainer::FConcurrencySubFlowContainer(const FString& InDebugName)
	: SubFlow(MakeShared<FControlFlow>(InDebugName))
{}

bool FConcurrencySubFlowContainer::HasBeenExecuted() const
{
	return bHasBeenExecuted;
}

bool FConcurrencySubFlowContainer::HasBeenCancelled() const
{
	return bHasBeenCancelled;
}

bool FConcurrencySubFlowContainer::IsCompleteOrCancelled() const
{
	const bool bIsComplete = HasBeenExecuted() && !SubFlow->IsRunning();
	return bIsComplete || HasBeenCancelled();
}

void FConcurrencySubFlowContainer::Execute()
{
	UE_LOG(LogControlFlows, Verbose, TEXT("ConcurrentControlFlowContainer - Executing %s"), *GetDebugName());

	if (ensureAlways(!IsCompleteOrCancelled()))
	{
		bHasBeenExecuted = true;
		SubFlow->ExecuteFlow();
	}
}

void FConcurrencySubFlowContainer::Cancel()
{
	UE_LOG(LogControlFlows, Verbose, TEXT("ConcurrentControlFlowContainer - Cancelling %s"), *GetDebugName());

	bHasBeenCancelled = true;
	if (SubFlow->IsRunning())
	{
		SubFlow->CancelFlow();
	}
}

FSimpleDelegate& FConcurrencySubFlowContainer::OnComplete()
{
	return SubFlow->OnComplete();
}

FSimpleDelegate& FConcurrencySubFlowContainer::OnExecutedWithoutAnyNodes()
{
	return SubFlow->OnExecutedWithoutAnyNodes();
}

FSimpleDelegate& FConcurrencySubFlowContainer::OnCancelled()
{
	return SubFlow->OnCancelled();
}

const FString& FConcurrencySubFlowContainer::GetDebugName()
{
	return SubFlow->GetDebugName();
}

TSharedRef<FControlFlow> FConcurrencySubFlowContainer::GetControlFlow() const
{
	return SubFlow;
}

////////////////////////////////////////////////////////////////////////////////////////

FControlFlow& FConcurrentControlFlows::AddOrGetFlow(int32 InIdentifier, const FString& DebugSubFlowName /*= TEXT("")*/)
{
	if (TSharedRef<FConcurrencySubFlowContainer>* ExistingFlow = ConcurrentFlows.Find(InIdentifier))
	{
		return (*ExistingFlow)->GetControlFlow().Get();
	}
	else
	{
		static int32 UnnamedControlConcurrencyFlowCounter = 0;
		const FString& DebugNameToUse = DebugSubFlowName.IsEmpty() ? FString::Format(TEXT("UnnamedConcurrencySubFlow_{0}"), { (UnnamedControlConcurrencyFlowCounter++) }) : DebugSubFlowName;

		TSharedRef<FConcurrencySubFlowContainer> NewConcurrentFlow = MakeShared<FConcurrencySubFlowContainer>(DebugNameToUse);

		NewConcurrentFlow->OnComplete().BindSP(SharedThis(this), &FConcurrentControlFlows::HandleConcurrentFlowCompleted, InIdentifier);
		NewConcurrentFlow->OnExecutedWithoutAnyNodes().BindSP(SharedThis(this), &FConcurrentControlFlows::HandleConcurrentFlowCompleted, InIdentifier);
		NewConcurrentFlow->OnCancelled().BindSP(SharedThis(this), &FConcurrentControlFlows::HandleConcurrentFlowCancelled, InIdentifier);

		ConcurrentFlows.Add(InIdentifier, NewConcurrentFlow);
		return NewConcurrentFlow->GetControlFlow().Get();
	}
}

FControlFlow& FConcurrentControlFlows::AddOrGetProng(int32 InIdentifier, const FString& DebugSubFlowName /*= TEXT("")*/)
{
	return AddOrGetFlow(InIdentifier, DebugSubFlowName);
}

bool FConcurrentControlFlows::AreAllSubFlowsCompletedOrCancelled() const
{
	for (const TPair<int32, TSharedRef<FConcurrencySubFlowContainer>>& PairIt : ConcurrentFlows)
	{
		if (!PairIt.Value->IsCompleteOrCancelled())
		{
			return false;
		}
	}

	return true;
}

bool FConcurrentControlFlows::HasAnySubFlowBeenExecuted() const
{
	for (const TPair<int32, TSharedRef<FConcurrencySubFlowContainer>>& PairIt : ConcurrentFlows)
	{
		if (PairIt.Value->HasBeenExecuted())
		{
			return true;
		}
	}

	return false;
}

void FConcurrentControlFlows::HandleConcurrentFlowCompleted(int32 FlowIndex)
{
	check(ConcurrentFlows.Contains(FlowIndex));

	UE_LOG(LogControlFlows, Verbose, TEXT("ConcurrentControlFlow Finished: %s"), *ConcurrentFlows[FlowIndex]->GetDebugName());

	if (!bCancelAllHasBegun)
	{
		if (GetConcurrencyBehavior().GetContinueCondition() == FConcurrentControlFlowBehavior::EContinueConditions::ContinueOnAll_CompletedOrCancelled)
		{
			if (AreAllSubFlowsCompletedOrCancelled())
			{
				OnConcurrencyCompleted.ExecuteIfBound();
			}
			else
			{
				UE_LOG(LogControlFlows, Verbose, TEXT("Other flows are still running"));
			}
		}
		else
		{
			checkf(false, TEXT("Unhandled Continue Condition"));
		}
	}
}

void FConcurrentControlFlows::HandleConcurrentFlowCancelled(int32 FlowIndex)
{
	check(ConcurrentFlows.Contains(FlowIndex));

	UE_LOG(LogControlFlows, Verbose, TEXT("ConcurrentControlFlow Cancelled %s"), *ConcurrentFlows[FlowIndex]->GetDebugName());

	if (!bCancelAllHasBegun)
	{
		HandleConcurrentFlowCompleted(FlowIndex);
	}
}

void FConcurrentControlFlows::Execute()
{
	ensureAlwaysMsgf(!this->HasAnySubFlowBeenExecuted(), TEXT("Did you call ExecuteFlow() on a SubFlow? Do not do this! You only need to call ExecuteFlow once per FControlFlowStatics::Create!"));

	bool bFlowExecuted = false;

	for (TPair<int32, TSharedRef<FConcurrencySubFlowContainer>>& PairIt : ConcurrentFlows)
	{
		UE_LOG(LogControlFlows, Verbose, TEXT("ConcurrentControlFlow::Execute - Executing Subflow %s"), *PairIt.Value->GetDebugName());

		bFlowExecuted = true;
		PairIt.Value->Execute();

		if (bCancelAllHasBegun)
		{
			break;
		}
	}
	
	if (ConcurrentFlows.Num() > 0)
	{
		ensureAlwaysMsgf(bFlowExecuted, TEXT("There were defined Subflows, but none were executed"));
	}

	if (!bFlowExecuted)
	{
		OnAllCompleted();
	}
}

void FConcurrentControlFlows::CancelAll()
{
	if (!bCancelAllHasBegun)
	{
		UE_LOG(LogControlFlows, Verbose, TEXT("FConcurrentControlFlows::CancelAll() called"));

		bCancelAllHasBegun = true;

		for (TPair<int32, TSharedRef<FConcurrencySubFlowContainer>>& PairIt : ConcurrentFlows)
		{
			UE_LOG(LogControlFlows, Verbose, TEXT("ConcurrentControlFlow::Cancel All - Cancelling Subflow %s"), *PairIt.Value->GetDebugName());

			PairIt.Value->Cancel();
		}

		OnAllCancelled();
	}
}

void FConcurrentControlFlows::OnAllCompleted()
{
	UE_LOG(LogControlFlows, Verbose, TEXT("ConcurrentControlFlow::OnAllCompleted()"));

	ConcurrentFlows.Reset();
	OnConcurrencyCompleted.ExecuteIfBound();
}

void FConcurrentControlFlows::OnAllCancelled()
{
	ConcurrentFlows.Reset();
	OnConcurrencyCancelled.ExecuteIfBound();
}

FConcurrentControlFlowBehavior FConcurrentControlFlows::GetConcurrencyBehavior() const
{
	return FConcurrentControlFlowBehavior();
}
