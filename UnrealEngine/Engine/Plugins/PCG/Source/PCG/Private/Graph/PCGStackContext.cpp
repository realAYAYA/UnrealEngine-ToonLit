// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/PCGStackContext.h"

#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSubgraph.h"

#include "Algo/Find.h"
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

			if (Object->IsA<UPCGComponent>())
			{
				StringBuilder << TEXT("COMPONENT:") << Object->GetFullName();
			}
			else if (Object->IsA<UPCGGraph>())
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

uint32 FPCGStack::GetNumGraphLevels() const
{
	uint32 GraphCount = 0;
	for (const FPCGStackFrame& Frame : StackFrames)
	{
		GraphCount += (Frame.Object.IsValid() && Frame.Object->IsA<UPCGGraph>()) ? 1 : 0;
	}

	return GraphCount;
}

bool FPCGStack::BeginsWith(const FPCGStack& Other) const
{
	if (Other.GetStackFrames().Num() > GetStackFrames().Num())
	{
		return false;
	}

	for (int32 StackIndex = 0; StackIndex < Other.GetStackFrames().Num(); ++StackIndex)
	{
		if (Other.GetStackFrames()[StackIndex] != GetStackFrames()[StackIndex])
		{
			return false;
		}
	}

	return true;
}

const UPCGComponent* FPCGStack::GetRootComponent() const
{
	return StackFrames.IsEmpty() ? nullptr : Cast<const UPCGComponent>(StackFrames[0].Object.Get());
}

const UPCGGraph* FPCGStack::GetRootGraph(int32* OutRootFrameIndex) const
{
	for (int StackIndex = 0; StackIndex < GetStackFrames().Num(); ++StackIndex)
	{
		if (const UPCGGraph* Graph = Cast<const UPCGGraph>(StackFrames[StackIndex].Object.Get()))
		{
			if (OutRootFrameIndex)
			{
				*OutRootFrameIndex = StackIndex;
			}

			return Graph;
		}
	}

	return nullptr;
}

const UPCGGraph* FPCGStack::GetGraphForCurrentFrame() const
{
	for (int StackIndex = GetStackFrames().Num() - 1; StackIndex >= 0; --StackIndex)
	{
		if (const UPCGGraph* Graph = Cast<const UPCGGraph>(StackFrames[StackIndex].Object.Get()))
		{
			return Graph;
		}
	}

	return nullptr;
}

const UPCGNode* FPCGStack::GetCurrentFrameNode() const
{
	return StackFrames.IsEmpty() ? nullptr : Cast<const UPCGNode>(StackFrames.Last().Object.Get());
}

bool FPCGStack::HasObject(const UObject* InObject) const
{
	return !!Algo::FindByPredicate(StackFrames, [InObject](const FPCGStackFrame& Frame)
	{
		return Frame.Object == InObject;
	});
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

int32 FPCGStackContext::PushFrame(const UObject* InFrameObject)
{
	if (CurrentStackIndex == INDEX_NONE)
	{
		// Create first stack using the given frame.
		FPCGStack& Stack = Stacks.Emplace_GetRef();
		Stack.PushFrame(InFrameObject);
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

bool FPCGStackContext::operator==(const FPCGStackContext& Other) const
{
	return (CurrentStackIndex == Other.CurrentStackIndex) && (Stacks == Other.Stacks);
}
