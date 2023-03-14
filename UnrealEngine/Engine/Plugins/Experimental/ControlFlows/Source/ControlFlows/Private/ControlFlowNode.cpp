// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlFlowNode.h"

#include "ControlFlow.h"
#include "ControlFlows.h"
#include "ControlFlowTask.h"


FControlFlowNode::FControlFlowNode()
	: Parent(nullptr)
{
	
}

FControlFlowNode::FControlFlowNode(TSharedRef<FControlFlow> ControlFlowParent, const FString& FlowNodeDebugName)
	: Parent(ControlFlowParent)
	, NodeName(FlowNodeDebugName)
{

}

FControlFlowNode::~FControlFlowNode()
{

}

void FControlFlowNode::ContinueFlow()
{
	if (ensureAlways(Parent.IsValid()))
	{
		Parent.Pin()->HandleControlFlowNodeCompleted(SharedThis(this));
	}
}

void FControlFlowNode::CancelFlow()
{
	if (ensureAlways(Parent.IsValid()))
	{
		bCancelled = true;
		Parent.Pin()->HandleControlFlowNodeCompleted(SharedThis(this));
	}
}

TSharedPtr<FTrackedActivity> FControlFlowNode::GetTrackedActivity() const
{
	if (ensureAlways(Parent.IsValid()))
	{
		return Parent.Pin()->GetTrackedActivity();
	}
	return nullptr;
}

void FControlFlowNode::LogExecution()
{
	if (ensureAlways(Parent.IsValid()))
	{
		Parent.Pin()->LogNodeExecution(*this);
	}
}

FString FControlFlowNode::GetNodeName() const
{
	return NodeName;
}

///////////////////////////////////////////////////

FControlFlowNode_SelfCompleting::FControlFlowNode_SelfCompleting(TSharedRef<FControlFlow> ControlFlowParent, const FString& FlowNodeDebugName)
	: FControlFlowNode(ControlFlowParent, FlowNodeDebugName)
{
}

FControlFlowNode_SelfCompleting::FControlFlowNode_SelfCompleting(TSharedRef<FControlFlow> ControlFlowParent, const FString& FlowNodeDebugName, const FSimpleDelegate& InCallback)
	: FControlFlowNode(ControlFlowParent, FlowNodeDebugName)
	, Process(InCallback)
{

}

void FControlFlowNode_SelfCompleting::Execute()
{
	LogExecution();

	if (ensureAlways(Parent.IsValid()))
	{
		Parent.Pin()->ExecuteNode(SharedThis(this));
	}
}

///////////////////////////////////////////////////

FControlFlowNode_RequiresCallback::FControlFlowNode_RequiresCallback(TSharedRef<FControlFlow> ControlFlowParent, const FString& FlowNodeDebugName)
	: FControlFlowNode(ControlFlowParent, FlowNodeDebugName)
{
}

FControlFlowNode_RequiresCallback::FControlFlowNode_RequiresCallback(TSharedRef<FControlFlow> ControlFlowParent, const FString& FlowNodeDebugName, const FControlFlowWaitDelegate& InCallback)
	: FControlFlowNode(ControlFlowParent, FlowNodeDebugName)
	, Process(InCallback)
{
}

void FControlFlowNode_RequiresCallback::Execute()
{
	LogExecution();

	if (Process.IsBound())
	{
		if (ensureAlways(Parent.IsValid()))
		{
			UE_LOG(LogControlFlows, Verbose, TEXT("ControlFlow - Executing %s.%s -  Waiting On Callback"), *(Parent.Pin()->GetFlowPath().Append(Parent.Pin()->GetDebugName())), *NodeName);
		}

		Process.Execute(SharedThis(this));
	}
	else
	{
		ContinueFlow();
	}
}

///////////////////////////////////////////////////

FControlFlowNode_Task::FControlFlowNode_Task(TSharedRef<FControlFlow> ControlFlowParent, TSharedRef<FControlFlowSubTaskBase> Module, const FString& FlowNodeDebugName)
	: FControlFlowNode(ControlFlowParent, FlowNodeDebugName)
	, FlowTask(Module)
{

}

void FControlFlowNode_Task::Execute()
{
	LogExecution();

	if (OnExecuteDelegate.IsBound())
	{
		OnExecuteDelegate.Execute(SharedThis(this));
	}
	else
	{
		ContinueFlow();
	}
}

void FControlFlowNode_Task::CancelFlow()
{
	bCancelled = true;
	OnCancelRequestedDelegate.ExecuteIfBound(SharedThis(this));
}

FString FControlFlowNode_Task::GetNodeName() const
{
	const FString& TaskName = GetFlowTask()->GetTaskName();
	return TaskName.IsEmpty() ? NodeName : TaskName;
}

void FControlFlowNode_Task::CompleteCancelFlow()
{
	FControlFlowNode::CancelFlow();
}
