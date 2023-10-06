// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/PCGStackContext.h"

#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSubgraph.h"

#include "Containers/UnrealString.h"
#include "Misc/StringBuilder.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGStackContext)

const FPCGStack* FPCGStackContext::GetStack(int32 InStackIndex) const
{
	if (ensure(Stacks.IsValidIndex(InStackIndex)))
	{
		return &Stacks[InStackIndex];
	}
	else
	{
		return nullptr;
	}
}

void FPCGStack::PopFrame()
{
	if (ensure(!StackFrames.IsEmpty()))
	{
		StackFrames.Pop();
	}
}

bool FPCGStack::CreateStackFramePath(FString& OutString, const UPCGNode* InNode, const UPCGPin* InPin) const
{
	// Give a healthy amount of scratch space on the stack, if it overflows it will use heap.
	TStringBuilderWithBuffer<TCHAR, 2048> StringBuilder;

	auto AddPathSeparator = [&StringBuilder]()
	{
		if (StringBuilder.Len())
		{
			StringBuilder << TEXT("/");
		}
	};

	for (const FPCGStackFrame& Frame : StackFrames)
	{
		if (Frame.Object.IsValid())
		{
			AddPathSeparator();
			const UObject* Object = Frame.Object.Get();

			if (!Object)
			{
				// If any object does not resolve, cannot build the string
				return false;
			}

			if (Object->IsA<UPCGGraph>())
			{
				StringBuilder << TEXT("GRAPH:") << Object->GetFullName();
			}
			else if (Object->IsA<UPCGNode>())
			{
				StringBuilder << TEXT("NODE:") << Object->GetFName();
			}
			else
			{
				// Unrecognized type, should not happen
				ensure(false);
				StringBuilder << TEXT("UNRECOGNIZED:") << Object->GetFullName();
			}
		}
		else if (Frame.LoopIndex != -1)
		{
			AddPathSeparator();
			StringBuilder << TEXT("LOOP:") << FString::FromInt(Frame.LoopIndex);
		}
	}

	if (InNode)
	{
		AddPathSeparator();
		StringBuilder << TEXT("NODE:") << InNode->GetFName().ToString();

		if (InPin)
		{
			AddPathSeparator();
			StringBuilder << TEXT("PIN:") << InPin->GetFName().ToString();
		}
	}

	OutString = StringBuilder;
	return true;
}

bool FPCGStack::operator==(const FPCGStack& Other) const
{
	// Stacks are the same if all stack frames are the same
	if (StackFrames.Num() != Other.StackFrames.Num())
	{
		return false;
	}

	for (int32 i = 0; i < StackFrames.Num(); i++)
	{
		if (StackFrames[i] != Other.StackFrames[i])
		{
			return false;
		}
	}

	return true;
}

uint32 GetTypeHash(const FPCGStack& In)
{
	uint32 Hash = 0;

	for (const FPCGStackFrame& Frame : In.StackFrames)
	{
		Hash = HashCombine(Hash, GetTypeHash(Frame));
	}

	return Hash;
}

int32 FPCGStackContext::PushFrame(const UObject* InFrameObject)
{
	if (CurrentStackIndex == INDEX_NONE)
	{
		// Create first stack using the given frame.
		FPCGStack& Stack = Stacks.Emplace_GetRef();
		Stack.PushFrame(FPCGStackFrame(InFrameObject));
		CurrentStackIndex = 0;
	}
	else
	{
		if (!ensure(Stacks.IsValidIndex(CurrentStackIndex)))
		{
			return INDEX_NONE;
		}

		// Append given frame object to current stack. Newly encountered stacks should generally
		// be unique, so we just commit to creating it immediately rather than searching to see
		// if it already exists first.
		FPCGStack CurrentStack = Stacks[CurrentStackIndex];
		CurrentStack.PushFrame(FPCGStackFrame(InFrameObject));
		CurrentStackIndex = Stacks.AddUnique(MoveTemp(CurrentStack));
	}

	return CurrentStackIndex;
}

int32 FPCGStackContext::PopFrame()
{
	if (!ensure(Stacks.IsValidIndex(CurrentStackIndex)))
	{
		return INDEX_NONE;
	}

	// Find the 'parent' callstack (current stack minus latest frame). Can be anywhere in the list of stacks so do a search.
	CurrentStackIndex = Stacks.IndexOfByPredicate([&CurrentStack = Stacks[CurrentStackIndex]](const FPCGStack& OtherStack)
	{
		const int32 RequiredFrameCount = CurrentStack.StackFrames.Num() - 1;
		if (OtherStack.StackFrames.Num() != RequiredFrameCount)
		{
			return false;
		}
		
		for (int32 i = 0; i < RequiredFrameCount; ++i)
		{
			if (OtherStack.StackFrames[i] != CurrentStack.StackFrames[i])
			{
				return false;
			}
		}

		return true;
	});
	ensure(CurrentStackIndex != INDEX_NONE);

	return CurrentStackIndex;
}

void FPCGStackContext::AppendStacks(const FPCGStackContext& InStacks)
{
	if (!ensure(Stacks.IsValidIndex(CurrentStackIndex)))
	{
		return;
	}

	for (const FPCGStack& SubgraphStack : InStacks.Stacks)
	{
		FPCGStack& NewStack = Stacks.Emplace_GetRef();
		NewStack.StackFrames.Reserve(Stacks[CurrentStackIndex].StackFrames.Num() + SubgraphStack.StackFrames.Num());
		
		NewStack.StackFrames.Append(Stacks[CurrentStackIndex].StackFrames);
		NewStack.StackFrames.Append(SubgraphStack.StackFrames);
	}
}

void FPCGStackContext::PrependParentStack(const FPCGStack* InParentStack)
{
	if (!InParentStack || InParentStack->StackFrames.IsEmpty())
	{
		return;
	}

	for (FPCGStack& Stack : Stacks)
	{
		Stack.StackFrames.Insert(InParentStack->StackFrames, 0);
	}
}

FPCGStackContext FPCGStackContext::CreateStackContextFromGraph(const UPCGGraph* InPCGGraph)
{
	FPCGStackContext StackContext;

	auto ParseGraphRecursive = [](const UPCGGraph* InPCGGraph, FPCGStackContext& InStackContext, TArray<const UPCGGraph*>& VisitedGraphStack, auto RecursiveCallback)
	{
		if (!InPCGGraph)
		{
			return;
		}

		InStackContext.PushFrame(InPCGGraph);

		for (const UPCGNode* PCGNode : InPCGGraph->GetNodes())
		{
			if (PCGNode)
			{
				// TODO: GetSettings() has no execution context and therefore cannot recurse on dynamically chosen subgraphs
				if (const UPCGBaseSubgraphSettings* SubgraphSettings = Cast<UPCGBaseSubgraphSettings>(PCGNode->GetSettings()))
				{
					if (const UPCGGraph* PCGSubgraph = SubgraphSettings->GetSubgraph())
					{
						// Skip graphs we have already visited to avoid cycles
						// TODO: This prevents recursive subgraphs
						if (!VisitedGraphStack.Contains(PCGSubgraph))
						{
							InStackContext.PushFrame(PCGNode);
							VisitedGraphStack.Push(PCGSubgraph);

							FPCGStackContext SubgraphStackContext;
							RecursiveCallback(PCGSubgraph, SubgraphStackContext, VisitedGraphStack, RecursiveCallback);
							InStackContext.AppendStacks(SubgraphStackContext);

							VisitedGraphStack.Pop();
							InStackContext.PopFrame();
						}
					}
				}
			}
		}
	};

	TArray<const UPCGGraph*> VisitedGraphStack;
	VisitedGraphStack.Push(InPCGGraph);

	ParseGraphRecursive(InPCGGraph, StackContext, VisitedGraphStack, ParseGraphRecursive);

	return StackContext;
}
