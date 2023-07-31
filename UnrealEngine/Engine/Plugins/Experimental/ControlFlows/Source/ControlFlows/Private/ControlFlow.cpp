// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlFlow.h"
#include "ControlFlowManager.h"
#include "ControlFlowTask.h"
#include "ControlFlows.h"
#include "Misc/TrackedActivity.h"

static const int32 MAX_FLOW_LOOPS = 10000;

int32 FControlFlow::UnnamedControlFlowCounter = 0;

FControlFlow::FControlFlow(const FString& FlowDebugName)
{
	if (FlowDebugName.IsEmpty())
	{
		DebugName = FString::Format(TEXT("ControlFlow_{0}"), { FString::FormatAsNumber(UnnamedControlFlowCounter) });
		UnnamedControlFlowCounter++;
	}
	else
	{
		DebugName = FlowDebugName;
	}
}

void FControlFlow::ExecuteNextNodeInQueue()
{
	UE_LOG(LogControlFlows, Verbose, TEXT("ControlFlow - Executing %s (ExecuteNextInQueue)"), *GetFlowPath().Append(DebugName));

	CurrentNode = FlowQueue[0];
	if (Activity)
		Activity->Update(*CurrentNode->GetNodeName());


	FlowQueue.RemoveAt(0);
	CurrentNode->Execute();
}

void FControlFlow::ExecuteNode(TSharedRef<FControlFlowNode_SelfCompleting> SelfCompletingNode)
{
	// Calling Synchronous Function in Flow
	SelfCompletingNode->Process.ExecuteIfBound();

	if (SelfCompletingNode->HasCancelBeenRequested())
	{
		FlowQueue.Reset();

		OnCancelled().ExecuteIfBound();

		FControlFlowStatics::HandleControlFlowFinishedNotification();
	}
	else
	{
		SelfCompletingNode->ContinueFlow();
	}
}

void FControlFlow::HandleControlFlowNodeCompleted(TSharedRef<const FControlFlowNode> NodeCompleted)
{
	UE_LOG(LogControlFlows, Verbose, TEXT("ControlFlow - Executing %s%s(x%d).%s (FlowControlNodeCompleted)"), *GetFlowPath(), *DebugName, GetRepeatedFlowCount(), *NodeCompleted->GetNodeName());

	SubFlowStack_ForDebugging.Add(SharedThis(this));

	if (ensure(CurrentNode.IsValid() && &NodeCompleted.Get() == CurrentNode.Get()))
	{
		const bool bCancelRequested = !bInterpretCancelledNodeAsComplete && NodeCompleted->HasCancelBeenRequested();
		CurrentNode.Reset();
		CurrentlyRunningTask.Reset();

		if (FlowQueue.Num() > 0)
		{
			if (!ensure(SubFlowStack_ForDebugging.Num() < MAX_FLOW_LOOPS))
			{
				UE_LOG(LogControlFlows, Error, TEXT("ControlFlow - Hit maximum Flow loops. Is there an infinite recursion somewhere?"));
			}
			else
			{
				if (bCancelRequested)
				{
					CurrentNode = FlowQueue[0];
					if (Activity)
						Activity->Update(*CurrentNode->GetNodeName());

					FlowQueue.RemoveAt(0);
					CurrentNode->CancelFlow();
				}
				else
				{
					ExecuteNextNodeInQueue();
				}
			}
		}
		else
		{
			if (bCancelRequested)
			{
				OnCancelled().ExecuteIfBound();
			}
			else
			{
				OnComplete().ExecuteIfBound();
			}

			FControlFlowStatics::HandleControlFlowFinishedNotification();
		}
	}

	SubFlowStack_ForDebugging.Pop();
}

FString FControlFlow::GetFlowPath() const
{
	static FString DuplicateFlowFormat = TEXT("{0}(x{1}).");

	FString FlowPath;

	int32 DuplicateCount = 0;

	for (int32 StackIndex = 0; StackIndex < SubFlowStack_ForDebugging.Num() - 1; ++StackIndex)
	{
		int32 NextInStackIndex = StackIndex + 1;

		if (SubFlowStack_ForDebugging[StackIndex] == SubFlowStack_ForDebugging[NextInStackIndex])
		{
			++DuplicateCount;

			if (NextInStackIndex == SubFlowStack_ForDebugging.Num())
			{
				FlowPath.Append(FString::Format(*DuplicateFlowFormat, { SubFlowStack_ForDebugging[StackIndex]->DebugName , FString::FromInt(DuplicateCount + 1) }));
			}
		}
		else
		{
			FlowPath.Append(FString::Format(*DuplicateFlowFormat, { SubFlowStack_ForDebugging[StackIndex]->DebugName , FString::FromInt(DuplicateCount + 1) }));

			DuplicateCount = 0;
		}
	}

	return FlowPath;
}

int32 FControlFlow::GetRepeatedFlowCount() const
{
	int32 FlowCount = 0;

	for (int32 StackIndex = SubFlowStack_ForDebugging.Num() - 1; StackIndex > -1; --StackIndex)
	{
		if (SubFlowStack_ForDebugging[StackIndex] == SharedThis(this))
		{
			++FlowCount;
		}
		else
		{
			break;
		}
	}

	return FlowCount;
}

void FControlFlow::LogNodeExecution(const FControlFlowNode& NodeExecuted)
{
	UE_LOG(LogControlFlows, Verbose, TEXT("ControlFlow - Executing %s%s(x%d).%s"), *GetFlowPath(), *DebugName, GetRepeatedFlowCount(), *NodeExecuted.GetNodeName());
}

void FControlFlow::ExecuteFlow()
{
	UE_LOG(LogControlFlows, Verbose, TEXT("ControlFlow - Executing %s (ExecuteFlow)"), *DebugName);

	if (ensureAlwaysMsgf(!CurrentNode.IsValid(), TEXT("Flow is already running! Or perhaps there are multiple instances of owning class? All flows should have a unique ID.")))
	{
		FControlFlowStatics::HandleControlFlowStartedNotification(AsShared());

		SubFlowStack_ForDebugging.Add(SharedThis(this));
		if (FlowQueue.Num() > 0)
		{
			ExecuteNextNodeInQueue();
		}
		else
		{
			OnExecutedWithoutAnyNodes().ExecuteIfBound();

			FControlFlowStatics::HandleControlFlowFinishedNotification();
		}

		SubFlowStack_ForDebugging.Pop();
	}

	UE_LOG(LogControlFlows, Verbose, TEXT("ControlFlow - Executing %s (ExecuteFlow Completed)"), *DebugName);
}

void FControlFlow::Reset()
{
	if (CurrentNode.IsValid())
	{
		//Flow is already active. perhaps fire an interrupted delegate?
	}

	CurrentNode.Reset();
	CurrentlyRunningTask.Reset();
	FlowQueue.Reset();
	SubFlowStack_ForDebugging.Reset();
	UnnamedNodeCounter = 0;
	UnnamedBranchCounter = 0;
}

void FControlFlow::CancelFlow()
{
	if (CurrentNode.IsValid())
	{
		CurrentNode->CancelFlow();
	}
	else
	{
		OnCancelled().ExecuteIfBound();
		FControlFlowStatics::HandleControlFlowFinishedNotification();
	}
}

FControlFlow& FControlFlow::SetCancelledNodeAsComplete(bool bCancelledNodeIsComplete)
{
	bInterpretCancelledNodeAsComplete = bCancelledNodeIsComplete;
	return *this;
}

FString FControlFlow::FormatOrGetNewNodeDebugName(const FString& FlowNodeDebugName)
{
	if (FlowNodeDebugName.IsEmpty())
	{
		UnnamedNodeCounter++;
		return FString::Format(TEXT("UnnamedNode_{0}"), { FString::FromInt(UnnamedNodeCounter - 1) });
	}

	return FlowNodeDebugName;
}

TOptional<FString> FControlFlow::GetCurrentStepDebugName() const
{
	if (CurrentNode.IsValid())
	{
		if (CurrentlyRunningTask.IsValid())
		{
			return CurrentlyRunningTask->GetNodeName();
		}
		
		return CurrentNode->GetNodeName();
	}

	return TOptional<FString>();
}

TSharedPtr<FTrackedActivity> FControlFlow::GetTrackedActivity() const
{
	return Activity;
}

FControlFlow& FControlFlow::TrackActivities(TSharedPtr<FTrackedActivity> InActivity)
{
	if (!InActivity)
		InActivity = MakeShared<FTrackedActivity>(*DebugName, TEXT(""));
	Activity = InActivity;
	return *this;
}

FSimpleDelegate& FControlFlow::QueueFunction(const FString& FlowNodeDebugName)
{
	TSharedRef<FControlFlowNode_SelfCompleting> NewNode = MakeShared<FControlFlowNode_SelfCompleting>(SharedThis(this), FormatOrGetNewNodeDebugName(FlowNodeDebugName));
	FlowQueue.Add(NewNode);
	
	return NewNode->Process;
}

FControlFlowWaitDelegate& FControlFlow::QueueWait(const FString& FlowNodeDebugName)
{
	TSharedRef<FControlFlowNode_RequiresCallback> NewNode = MakeShared<FControlFlowNode_RequiresCallback>(SharedThis(this), FormatOrGetNewNodeDebugName(FlowNodeDebugName));
	FlowQueue.Add(NewNode);

	return NewNode->Process;
}

FControlFlowPopulator& FControlFlow::QueueControlFlow(const FString& TaskName /*= TEXT("")*/, const FString& FlowNodeDebugName /*= TEXT("")*/)
{
	TSharedRef<FControlFlowSimpleSubTask> NewTask = MakeShared<FControlFlowSimpleSubTask>(TaskName, MakeShared<FControlFlow>(TaskName));
	TSharedRef<FControlFlowNode_Task> NewNode = MakeShared<FControlFlowNode_Task>(SharedThis(this), NewTask, FormatOrGetNewNodeDebugName(FlowNodeDebugName));
	NewTask->GetTaskFlow()->Activity = Activity;
	NewNode->OnExecute().BindSP(SharedThis(this), &FControlFlow::HandleTaskNodeExecuted);
	NewNode->OnCancelRequested().BindSP(SharedThis(this), &FControlFlow::HandleTaskNodeCancelled);

	FlowQueue.Add(NewNode);

	return NewTask->GetTaskPopulator();
}

FControlFlowBranchDefiner& FControlFlow::QueueControlFlowBranch(const FString& TaskName /*= TEXT("")*/, const FString& FlowNodeDebugName /*= TEXT("")*/)
{
	TSharedRef<FControlFlowTask_Branch> NewTask = MakeShared<FControlFlowTask_Branch>(TaskName);
	TSharedRef<FControlFlowNode_Task> NewNode = MakeShared<FControlFlowNode_Task>(SharedThis(this), NewTask, FormatOrGetNewNodeDebugName(FlowNodeDebugName));

	NewTask->Activity = Activity;
	NewNode->OnExecute().BindSP(SharedThis(this), &FControlFlow::HandleTaskNodeExecuted);
	NewNode->OnCancelRequested().BindSP(SharedThis(this), &FControlFlow::HandleTaskNodeCancelled);

	FlowQueue.Add(NewNode);

	return NewTask->GetDelegate();
}

FConcurrentFlowsDefiner& FControlFlow::QueueConcurrentFlows(const FString& TaskName /*= TEXT("")*/, const FString& FlowNodeDebugName /*= TEXT("")*/)
{
	TSharedRef<FControlFlowTask_ConcurrentFlows> NewTask = MakeShared<FControlFlowTask_ConcurrentFlows>(TaskName);
	TSharedRef<FControlFlowNode_Task> NewNode = MakeShared<FControlFlowNode_Task>(SharedThis(this), NewTask, FormatOrGetNewNodeDebugName(FlowNodeDebugName));

	NewNode->OnExecute().BindSP(SharedThis(this), &FControlFlow::HandleTaskNodeExecuted);
	NewNode->OnCancelRequested().BindSP(SharedThis(this), &FControlFlow::HandleTaskNodeCancelled);

	FlowQueue.Add(NewNode);

	return NewTask->GetDelegate();
}

TSharedRef<FControlFlowTask_BranchLegacy> FControlFlow::QueueBranch(FControlFlowBranchDecider_Legacy& BranchDecider, const FString& TaskName /*= TEXT("")*/, const FString& FlowNodeDebugName /*= TEXT("")*/)
{
	ensureAlwaysMsgf(false, TEXT("Deprecated. Use 'QueueControlFlowBranch'"));

	TSharedRef<FControlFlowTask_BranchLegacy> NewTask = MakeShared<FControlFlowTask_BranchLegacy>(BranchDecider, TaskName);
	TSharedRef<FControlFlowNode_Task> NewNode = MakeShared<FControlFlowNode_Task>(SharedThis(this), NewTask, FormatOrGetNewNodeDebugName(FlowNodeDebugName));

	NewNode->OnExecute().BindSP(SharedThis(this), &FControlFlow::HandleTaskNodeExecuted);
	NewNode->OnCancelRequested().BindSP(SharedThis(this), &FControlFlow::HandleTaskNodeCancelled);

	FlowQueue.Add(NewNode);

	return NewTask;
}

FControlFlowPopulator& FControlFlow::QueueLoop(FControlFlowLoopComplete& LoopCompleteDelgate, const FString& TaskName /*= TEXT("")*/, const FString& FlowNodeDebugName /*= TEXT("")*/)
{
	TSharedRef<FControlFlowTask_Loop> NewTask = MakeShared<FControlFlowTask_Loop>(LoopCompleteDelgate, TaskName, MakeShared<FControlFlow>(TaskName));
	TSharedRef<FControlFlowNode_Task> NewNode = MakeShared<FControlFlowNode_Task>(SharedThis(this), NewTask, FormatOrGetNewNodeDebugName(FlowNodeDebugName));
	NewTask->GetTaskFlow()->Activity = Activity;
	NewNode->OnExecute().BindSP(SharedThis(this), &FControlFlow::HandleTaskNodeExecuted);
	NewNode->OnCancelRequested().BindSP(SharedThis(this), &FControlFlow::HandleTaskNodeCancelled);

	FlowQueue.Add(NewNode);

	return NewTask->GetTaskPopulator();
}

void FControlFlow::HandleTaskNodeExecuted(TSharedRef<FControlFlowNode_Task> TaskNode)
{
	UE_LOG(LogControlFlows, Verbose, TEXT("ControlFlow - Executing %s%s (ExecuteTask)"), *GetFlowPath(), *DebugName);

	if (ensure(CurrentNode.IsValid() && CurrentNode == TaskNode && !CurrentlyRunningTask.IsValid()))
	{
		CurrentlyRunningTask = TaskNode;
		CurrentNode = CurrentlyRunningTask;
		if (Activity)
			Activity->Update(*TaskNode->GetFlowTask()->DebugName);

		TaskNode->GetFlowTask()->OnComplete().BindSP(SharedThis(this), &FControlFlow::HandleOnTaskComplete);
		TaskNode->GetFlowTask()->OnCancelled().BindSP(SharedThis(this), &FControlFlow::HandleOnTaskCancelled);

		TaskNode->GetFlowTask()->Execute();
	}
}

void FControlFlow::HandleTaskNodeCancelled(TSharedRef<FControlFlowNode_Task> TaskNode)
{
	CurrentlyRunningTask = TaskNode;

	TaskNode->GetFlowTask()->OnComplete().BindSP(SharedThis(this), &FControlFlow::HandleOnTaskComplete);
	TaskNode->GetFlowTask()->OnCancelled().BindSP(SharedThis(this), &FControlFlow::HandleOnTaskCancelled);

	TaskNode->GetFlowTask()->Cancel();
}

void FControlFlow::HandleOnTaskComplete()
{
	UE_LOG(LogControlFlows, Verbose, TEXT("ControlFlow - Executing %s%s (TaskComplete)"), *GetFlowPath(), *DebugName);

	if (CurrentlyRunningTask.IsValid())
	{
		CurrentlyRunningTask->ContinueFlow();
	}
}

void FControlFlow::HandleOnTaskCancelled()
{
	UE_LOG(LogControlFlows, Verbose, TEXT("ControlFlow - Cancelling %s%s (TaskCancelled)"), *GetFlowPath(), *DebugName);

	if (CurrentlyRunningTask.IsValid())
	{
		CurrentlyRunningTask->CompleteCancelFlow();
	}
}
