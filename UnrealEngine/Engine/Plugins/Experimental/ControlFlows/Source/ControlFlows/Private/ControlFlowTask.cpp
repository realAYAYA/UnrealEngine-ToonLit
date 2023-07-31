// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlFlowTask.h"
#include "ControlFlows.h"
#include "ControlFlowBranch.h"
#include "ControlFlowConcurrency.h"

//////////////////////////
//FControlFlowTask
//////////////////////////

FControlFlowSubTaskBase::FControlFlowSubTaskBase(const FString& TaskName)
:	DebugName(TaskName)
{

}

void FControlFlowSubTaskBase::Execute()
{
	TaskCompleteCallback.ExecuteIfBound();
}

void FControlFlowSubTaskBase::Cancel()
{
	TaskCancelledCallback.ExecuteIfBound();
}

//////////////////////////
//FControlFlowBranch
//////////////////////////

FControlFlowTask_BranchLegacy::FControlFlowTask_BranchLegacy(FControlFlowBranchDecider_Legacy& BranchDecider, const FString& TaskName)
	: FControlFlowSubTaskBase(TaskName)
	, BranchDelegate(BranchDecider)
{

}

FSimpleDelegate& FControlFlowTask_BranchLegacy::QueueFunction(int32 BranchIndex, const FString& FlowNodeDebugName /*= TEXT("")*/)
{
	return GetOrAddBranch(BranchIndex)->QueueFunction(FlowNodeDebugName);
}

FControlFlowWaitDelegate& FControlFlowTask_BranchLegacy::QueueWait(int32 BranchIndex, const FString& FlowNodeDebugName /*= TEXT("")*/)
{
	return GetOrAddBranch(BranchIndex)->QueueWait(FlowNodeDebugName);
}

FControlFlowPopulator& FControlFlowTask_BranchLegacy::QueueControlFlow(int32 BranchIndex, const FString& TaskName /*= TEXT("")*/, const FString& FlowNodeDebugName /*= TEXT("")*/)
{
	return GetOrAddBranch(BranchIndex)->QueueControlFlow(TaskName, FlowNodeDebugName);
}

TSharedRef<FControlFlowTask_BranchLegacy> FControlFlowTask_BranchLegacy::QueueBranch(int32 BranchIndex, FControlFlowBranchDecider_Legacy& BranchDecider, const FString& TaskName /*= TEXT("")*/, const FString& FlowNodeDebugName /*= TEXT("")*/)
{
	return GetOrAddBranch(BranchIndex)->QueueBranch(BranchDecider, TaskName, FlowNodeDebugName);
}

FControlFlowPopulator& FControlFlowTask_BranchLegacy::QueueLoop(int32 BranchIndex, FControlFlowLoopComplete& LoopCompleteDelgate, const FString& TaskName /*= TEXT("")*/, const FString& FlowNodeDebugName /*= TEXT("")*/)
{
	return GetOrAddBranch(BranchIndex)->QueueLoop(LoopCompleteDelgate, TaskName, FlowNodeDebugName);
}

void FControlFlowTask_BranchLegacy::HandleBranchCompleted()
{
	Branches.Reset();

	OnComplete().ExecuteIfBound();
}

void FControlFlowTask_BranchLegacy::HandleBranchCancelled()
{
	Branches.Reset();

	OnCancelled().ExecuteIfBound();
}

TSharedRef<FControlFlow> FControlFlowTask_BranchLegacy::GetOrAddBranch(int32 BranchIndex)
{
	if (!Branches.Contains(BranchIndex))
	{
		Branches.Add(BranchIndex, MakeShared<FControlFlow>());
	}

	return Branches[BranchIndex];
}

void FControlFlowTask_BranchLegacy::Execute()
{
	if (BranchDelegate.IsBound())
	{
		SelectedBranch = BranchDelegate.Execute();

		TSharedRef<FControlFlow> FlowToExecute = GetOrAddBranch(SelectedBranch);

		FlowToExecute->OnComplete().BindSP(SharedThis(this), &FControlFlowTask_BranchLegacy::HandleBranchCompleted);
		FlowToExecute->OnExecutedWithoutAnyNodes().BindSP(SharedThis(this), &FControlFlowTask_BranchLegacy::HandleBranchCompleted);
		FlowToExecute->OnCancelled().BindSP(SharedThis(this), &FControlFlowTask_BranchLegacy::HandleBranchCancelled);

		FlowToExecute->ExecuteFlow();
	}
	else
	{
		HandleBranchCompleted();
	}
}

void FControlFlowTask_BranchLegacy::Cancel()
{
	if (Branches.Contains(SelectedBranch) && Branches[SelectedBranch]->IsRunning())
	{
		Branches[SelectedBranch]->CancelFlow();
	}
	else
	{
		HandleBranchCancelled();
	}
}

//////////////////////////////////
//FControlFlowSimpleSubTask
//////////////////////////////////

FControlFlowSimpleSubTask::FControlFlowSimpleSubTask(const FString& TaskName, TSharedRef<FControlFlow> FlowOwner)
	: FControlFlowSubTaskBase(TaskName)
	, TaskFlow(FlowOwner)
{

}

void FControlFlowSimpleSubTask::Execute()
{
	if (TaskPopulator.IsBound() && GetTaskFlow().IsValid())
	{
		GetTaskFlow()->OnComplete().BindSP(SharedThis(this), &FControlFlowSimpleSubTask::CompletedSubTask);
		GetTaskFlow()->OnExecutedWithoutAnyNodes().BindSP(SharedThis(this), &FControlFlowSimpleSubTask::CompletedSubTask);
		GetTaskFlow()->OnCancelled().BindSP(SharedThis(this), &FControlFlowSimpleSubTask::CancelledSubTask);

		TaskPopulator.Execute(GetTaskFlow().ToSharedRef());

		ensureAlwaysMsgf(!GetTaskFlow()->IsRunning(), TEXT("Did you call ExecuteFlow() on a SubFlow? You don't need to."));

		GetTaskFlow()->ExecuteFlow();
	}
	else
	{
		UE_LOG(LogControlFlows, Error, TEXT("ControlFlow - Executed Sub Task (%s) without proper set up"), *GetTaskName());

		CompletedSubTask();
	}
}

void FControlFlowSimpleSubTask::Cancel()
{
	if (GetTaskFlow().IsValid() && GetTaskFlow()->IsRunning())
	{
		GetTaskFlow()->CancelFlow();
	}
	else
	{
		CancelledSubTask();
	}
}

void FControlFlowSimpleSubTask::CompletedSubTask()
{
	OnComplete().ExecuteIfBound();
}

void FControlFlowSimpleSubTask::CancelledSubTask()
{
	OnCancelled().ExecuteIfBound();
}

//////////////////////////////////
//FControlFlowLoop
//////////////////////////////////

FControlFlowTask_Loop::FControlFlowTask_Loop(FControlFlowLoopComplete& TaskCompleteDelegate, const FString& TaskName, TSharedRef<FControlFlow> FlowOwner)
	: FControlFlowSimpleSubTask(TaskName, FlowOwner)
	, TaskCompleteDecider(TaskCompleteDelegate)
{

}

void FControlFlowTask_Loop::Execute()
{
	if (GetTaskPopulator().IsBound() && TaskCompleteDecider.IsBound() && GetTaskFlow().IsValid())
	{
		if (TaskCompleteDecider.Execute())
		{
			CompletedLoop();
		}
		else
		{
			GetTaskFlow()->OnComplete().BindSP(SharedThis(this), &FControlFlowTask_Loop::CompletedLoop);
			GetTaskFlow()->OnExecutedWithoutAnyNodes().BindSP(SharedThis(this), &FControlFlowTask_Loop::CompletedLoop);
			GetTaskFlow()->OnCancelled().BindSP(SharedThis(this), &FControlFlowTask_Loop::CancelledLoop);

			GetTaskPopulator().Execute(GetTaskFlow().ToSharedRef());

			GetTaskFlow()->ExecuteFlow();
		}
	}
	else
	{
		UE_LOG(LogControlFlows, Error, TEXT("ControlFlow - Executed Loop (%s) without proper bound delegates"), *GetTaskName());

		CompletedLoop();
	}
}

void FControlFlowTask_Loop::Cancel()
{
	if (GetTaskFlow().IsValid() && GetTaskFlow()->IsRunning())
	{
		GetTaskFlow()->CancelFlow();
	}
	else
	{
		CancelledLoop();
	}
}

void FControlFlowTask_Loop::CompletedLoop()
{
	if (TaskCompleteDecider.IsBound() && !TaskCompleteDecider.Execute())
	{
		Execute();
	}
	else
	{
		OnComplete().ExecuteIfBound();
	}
}

void FControlFlowTask_Loop::CancelledLoop()
{
	OnCancelled().ExecuteIfBound();
}

//////////////////////////////////
//FControlFlowTask_Branch
//////////////////////////////////

void FControlFlowTask_Branch::Execute()
{
	if (ensureAlways(BranchDelegate.IsBound() && !SelectedBranchFlow.IsValid()))
	{
		TSharedRef<FControlFlowBranch> BranchDefinitions = MakeShared<FControlFlowBranch>();
		int32 SelectedBranchKey = BranchDelegate.Execute(BranchDefinitions);

		if (ensureAlwaysMsgf(BranchDefinitions->Contains(SelectedBranchKey), TEXT("You've returned a Branch Key that doesn't exist!")))
		{
			SelectedBranchFlow = BranchDefinitions->FindChecked(SelectedBranchKey);
			SelectedBranchFlow->Activity = Activity;

			SelectedBranchFlow->OnComplete().BindSP(SharedThis(this), &FControlFlowTask_Branch::HandleBranchCompleted);
			SelectedBranchFlow->OnExecutedWithoutAnyNodes().BindSP(SharedThis(this), &FControlFlowTask_Branch::HandleBranchCompleted);
			SelectedBranchFlow->OnCancelled().BindSP(SharedThis(this), &FControlFlowTask_Branch::HandleBranchCancelled);

			ensureAlwaysMsgf(!BranchDefinitions->IsAnyBranchRunning(), TEXT("Did you call ExecuteFlow() on a Branch? Do not do this! You only need to call ExecuteFlow once per FControlFlowStatics::Create!"));

			SelectedBranchFlow->ExecuteFlow();
		}
		else
		{
			HandleBranchCompleted();
		}
	}
	else
	{
		HandleBranchCompleted();
	}
}

void FControlFlowTask_Branch::Cancel()
{
	if (ensureAlways(SelectedBranchFlow.IsValid()) && SelectedBranchFlow->IsRunning())
	{
		SelectedBranchFlow->CancelFlow();
	}
	else
	{
		HandleBranchCancelled();
	}
}

void FControlFlowTask_Branch::HandleBranchCompleted()
{
	SelectedBranchFlow.Reset();

	if (BranchDelegate.IsBound())
	{
		BranchDelegate.Unbind();
	}

	OnComplete().ExecuteIfBound();
}

void FControlFlowTask_Branch::HandleBranchCancelled()
{
	SelectedBranchFlow.Reset();

	if (BranchDelegate.IsBound())
	{
		BranchDelegate.Unbind();
	}

	OnCancelled().ExecuteIfBound();
}

//////////////////////////////////
//FControlFlowTask_ConcurrentBranch
//////////////////////////////////

void FControlFlowTask_ConcurrentFlows::Execute()
{
	if (ensureAlways(ConcurrentFlowDelegate.IsBound() && !ConcurrentFlows.IsValid()))
	{
		ConcurrentFlows = MakeShared<FConcurrentControlFlows>();
		ConcurrentFlows->OnConcurrencyCompleted.BindSP(SharedThis(this), &FControlFlowTask_ConcurrentFlows::HandleConcurrentFlowsCompleted);
		ConcurrentFlows->OnConcurrencyCancelled.BindSP(SharedThis(this), &FControlFlowTask_ConcurrentFlows::HandleConcurrentFlowsCancelled);

		ConcurrentFlowDelegate.Execute(ConcurrentFlows.ToSharedRef());
		ConcurrentFlows->Execute();
	}
	else
	{
		HandleConcurrentFlowsCompleted();
	}
}

void FControlFlowTask_ConcurrentFlows::Cancel()
{
	if (ConcurrentFlows.IsValid())
	{
		ConcurrentFlows->CancelAll();
	}
	else
	{
		HandleConcurrentFlowsCancelled();
	}
}

void FControlFlowTask_ConcurrentFlows::HandleConcurrentFlowsCompleted()
{
	if (ConcurrentFlowDelegate.IsBound())
	{
		ConcurrentFlowDelegate.Unbind();
	}

	OnComplete().ExecuteIfBound();
}

void FControlFlowTask_ConcurrentFlows::HandleConcurrentFlowsCancelled()
{
	if (ConcurrentFlowDelegate.IsBound())
	{
		ConcurrentFlowDelegate.Unbind();
	}

	OnCancelled().ExecuteIfBound();
}
