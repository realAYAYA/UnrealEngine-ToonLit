// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utils/PCGExtraCapture.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGGraph.h"
#include "PCGSubgraph.h"
#include "Graph/PCGGraphCompiler.h"
#include "Graph/PCGGraphExecutor.h"

#if WITH_EDITOR

void PCGUtils::FExtraCapture::ResetCapturedMessages()
{
	CapturedMessages.Empty();
}

void PCGUtils::FExtraCapture::Update(const PCGUtils::FScopedCall& InScopedCall)
{
	if (!InScopedCall.Context || !InScopedCall.Context->Stack)
	{
		return;
	}

	const double CurrentTime = FPlatformTime::Seconds();
	const double ThisFrameTime = CurrentTime - InScopedCall.StartTime;

	FCallTime& Timer = const_cast<FPCGStack*>(InScopedCall.Context->Stack)->Timer;

	switch (InScopedCall.Phase)
	{
	case EPCGExecutionPhase::NotExecuted:
		Timer = FCallTime(); // reset it
		break;
	case EPCGExecutionPhase::PrepareData:
		if (Timer.PrepareDataFrameCount == 0)
		{
			Timer.PrepareDataStartTime = InScopedCall.StartTime;
		}

		Timer.PrepareDataFrameCount++;
		Timer.PrepareDataTime += ThisFrameTime;
		Timer.PrepareDataEndTime = CurrentTime;
		break;
	case EPCGExecutionPhase::Execute:
		if (Timer.ExecutionFrameCount == 0)
		{
			Timer.ExecutionStartTime = InScopedCall.StartTime;
		}

		Timer.ExecutionTime += ThisFrameTime;
		Timer.ExecutionFrameCount++;
		Timer.ExecutionEndTime = CurrentTime;

		Timer.MaxExecutionFrameTime = FMath::Max(Timer.MaxExecutionFrameTime, ThisFrameTime);
		Timer.MinExecutionFrameTime = FMath::Min(Timer.MinExecutionFrameTime, ThisFrameTime);
		break;
	case EPCGExecutionPhase::PostExecute:
		Timer.PostExecuteTime = ThisFrameTime;
		break;
	}

	FScopeLock ScopedLock(&Lock);
	if (!InScopedCall.CapturedMessages.IsEmpty())
	{
		TArray<FCapturedMessage>& InstanceMessages = CapturedMessages.FindOrAdd(InScopedCall.Context->Node);
		InstanceMessages.Append(std::move(InScopedCall.CapturedMessages));
	}
}

namespace PCGUtils
{
	void BuildTreeInfo(FCallTreeInfo& Info, const TMap<const UPCGNode*, const UPCGGraph*>& SubgraphNodeToGraphMap)
	{
		if (const UPCGGraph* const* Subgraph = SubgraphNodeToGraphMap.Find(Info.Node))
		{
			check(*Subgraph);
			// This is the same as in UPCGSubgraphSettings::GetAdditionalTitleInformation
			Info.Name = FName::NameToDisplayString((*Subgraph)->GetName(), /*bIsBool=*/false);
		}

		for (FCallTreeInfo& Child : Info.Children)
		{
			BuildTreeInfo(Child, SubgraphNodeToGraphMap);

			Info.CallTime.PrepareDataTime += Child.CallTime.PrepareDataTime;
			Info.CallTime.ExecutionTime += Child.CallTime.ExecutionTime;
			Info.CallTime.PostExecuteTime += Child.CallTime.PostExecuteTime;

			Info.CallTime.PrepareDataStartTime = FMath::Min(Info.CallTime.PrepareDataStartTime, Child.CallTime.PrepareDataStartTime);
			Info.CallTime.PrepareDataEndTime = FMath::Max(Info.CallTime.PrepareDataEndTime, Child.CallTime.PrepareDataEndTime);
			Info.CallTime.ExecutionStartTime = FMath::Min(Info.CallTime.ExecutionStartTime, Child.CallTime.ExecutionStartTime);
			Info.CallTime.ExecutionEndTime = FMath::Max(Info.CallTime.ExecutionEndTime, Child.CallTime.ExecutionEndTime);

			Info.CallTime.MinExecutionFrameTime = FMath::Min(Info.CallTime.MinExecutionFrameTime, Child.CallTime.MinExecutionFrameTime);
			Info.CallTime.MaxExecutionFrameTime = FMath::Max(Info.CallTime.MaxExecutionFrameTime, Child.CallTime.MaxExecutionFrameTime);
		}
	}
}

PCGUtils::FCallTreeInfo PCGUtils::FExtraCapture::CalculateCallTreeInfo(const UPCGComponent* Component, const FPCGStack& RootStack) const
{
	FCallTreeInfo RootInfo;

	// Basically, what we want is - visit all entries in the "NodeToStacksInWhichNodeExecuted" and build our information from there.
	TMap<TObjectKey<const UPCGNode>, TSet<FPCGStack>> NodeToStacksInWhichNodeExecuted = Component->GetExecutedNodeStacks();

	TArray<const UPCGNode*> NodePath;
	TArray<int32> NodePathLoop;
	TMap<const UPCGNode*, const UPCGGraph*> SubgraphToGraphMap;

	auto GetNodePath = [&NodePath, &NodePathLoop, &RootStack](const FPCGStack& Stack)
	{
		NodePath.Reset();
		NodePathLoop.Reset();

		if (!Stack.BeginsWith(RootStack))
		{
			return false;
		}

		const TArray<FPCGStackFrame>& StackFrames = Stack.GetStackFrames();
		for(int StackFrameIndex = RootStack.GetStackFrames().Num(); StackFrameIndex < StackFrames.Num(); ++StackFrameIndex)
		{
			const FPCGStackFrame& StackFrame = StackFrames[StackFrameIndex];

			if (const UPCGNode* Node = Cast<UPCGNode>(StackFrame.Object.Get()))
			{
				NodePath.Add(Node);
				NodePathLoop.Add(-1);
			}
			else if (StackFrame.LoopIndex != INDEX_NONE)
			{
				NodePath.Add(nullptr);
				NodePathLoop.Add(StackFrame.LoopIndex);
			}
		}

		return true;
	};

	auto GetCallInfo = [&NodePath, &NodePathLoop, &RootInfo]()
	{
		FCallTreeInfo* Current = &RootInfo;

		int NodeDepth = 0;
		while (NodeDepth < NodePath.Num())
		{
			const UPCGNode* NodeToFind = NodePath[NodeDepth];
			const int32 LoopToFind = NodePathLoop[NodeDepth];
			++NodeDepth;

			bool bFound = false;
			for (FCallTreeInfo& Child : Current->Children)
			{
				if((NodeToFind != nullptr && Child.Node == NodeToFind) || 
					(LoopToFind != INDEX_NONE && Child.Node == nullptr && Child.LoopIndex == LoopToFind))
				{
					bFound = true;
					Current = &Child;
					break;
				}
			}

			if (!bFound)
			{
				FCallTreeInfo& Child = Current->Children.Emplace_GetRef();
				Child.Node = NodeToFind;
				Child.LoopIndex = LoopToFind;
				Current = &Child;
			}
		}

		return Current;
	};

	for (const auto& NodeToStacks : NodeToStacksInWhichNodeExecuted)
	{
		const UPCGNode* Node = NodeToStacks.Key.ResolveObjectPtr();
		const TSet<FPCGStack>& Stacks = NodeToStacks.Value;

		for (const FPCGStack& Stack : Stacks)
		{
			if (!GetNodePath(Stack))
			{
				continue;
			}

			// Set subgraph node name if immediate parent is a subgraph (loop or subgraph - NOT spawn actor)
			const UPCGNode* ParentNode = NodePath.IsEmpty() ? nullptr : ((NodePathLoop.Last() != INDEX_NONE && NodePath.Num() > 1) ? NodePath.Last(1) : NodePath.Last());
			if (const UPCGSubgraphNode* SubgraphNode = Cast<UPCGSubgraphNode>(ParentNode))
			{
				const UPCGGraph* CurrentGraph = Stack.GetGraphForCurrentFrame();
				if(CurrentGraph && !SubgraphToGraphMap.Contains(SubgraphNode))
				{
					SubgraphToGraphMap.Add(SubgraphNode, CurrentGraph);
				}
			}
			
			// Need to add "this" node to the stack
			NodePath.Add(Node);
			NodePathLoop.Add(INDEX_NONE);

			FCallTreeInfo* Info = GetCallInfo();

			check(Info);
			Info->CallTime = Stack.Timer;
		}
	}

	PCGUtils::BuildTreeInfo(RootInfo, SubgraphToGraphMap);
	return RootInfo;
}

PCGUtils::FScopedCall::FScopedCall(const IPCGElement& InOwner, FPCGContext* InContext)
	: Owner(InOwner)
	, Context(InContext)
	, Phase(InContext->CurrentPhase)
	, ThreadID(FPlatformTLS::GetCurrentThreadId())
{
	StartTime = FPlatformTime::Seconds();

	GLog->AddOutputDevice(this);
}

PCGUtils::FScopedCall::~FScopedCall()
{
	GLog->RemoveOutputDevice(this);
	if (Context && Context->SourceComponent.IsValid())
	{
		Context->SourceComponent->ExtraCapture.Update(*this);
	}
}

void PCGUtils::FScopedCall::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	// TODO: this thread id check will also filter out messages spawned from threads spawned inside of nodes. To improve that,
	// perhaps set at TLS bit on things from here and inside of PCGAsync spawned jobs. If this was done, CapturedMessages below also will
	// need protection
	if (Verbosity > ELogVerbosity::Warning || FPlatformTLS::GetCurrentThreadId() != ThreadID)
	{
		// ignore
		return;
	}

	// this is a dumb counter just so messages can be sorted in a similar order as when they were logged
	static volatile int32 MessageCounter = 0;

	CapturedMessages.Add(PCGUtils::FCapturedMessage{ MessageCounter++, Category, V, Verbosity });
}

#endif // WITH_EDITOR