// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utils/PCGNodeVisualLogs.h"

#include "PCGComponent.h"

#include "Algo/Find.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "PCGNode"

#if WITH_EDITOR

void FPCGNodeVisualLogs::Log(const FPCGStack& InPCGStack, ELogVerbosity::Type InVerbosity, const FText& InMessage)
{
	bool bAdded = false;

	{
		FWriteScopeLock ScopedWriteLock(LogsLock);

		FPCGPerNodeVisualLogs& NodeLogs = StackToLogs.FindOrAdd(InPCGStack);

		constexpr int32 MaxLogged = 1024;
		if (StackToLogs.Num() < MaxLogged)
		{
			NodeLogs.Emplace(InMessage, InVerbosity);

			bAdded = true;
		}
	}

	// Broadcast outside of write scope lock
	if (bAdded && IsInGameThread() && !InPCGStack.GetStackFrames().IsEmpty())
	{
		for (const FPCGStackFrame& Frame : InPCGStack.GetStackFrames())
		{
			if (const UPCGNode* Node = Cast<const UPCGNode>(Frame.Object.Get()))
			{
				Node->OnNodeChangedDelegate.Broadcast(const_cast<UPCGNode*>(Node), EPCGChangeType::Cosmetic);
			}
		}
	}
}

bool FPCGNodeVisualLogs::HasLogs(const FPCGStack& InPCGStack) const
{
	FReadScopeLock ScopedReadLock(LogsLock);

	for (const TPair<FPCGStack, FPCGPerNodeVisualLogs>& Entry : StackToLogs)
	{
		if (Entry.Key.BeginsWith(InPCGStack) && !Entry.Value.IsEmpty())
		{
			return true;
		}
	}

	return false;
}

bool FPCGNodeVisualLogs::HasLogs(const FPCGStack& InPCGStack, ELogVerbosity::Type& OutMinVerbosity) const
{
	OutMinVerbosity = ELogVerbosity::All;
	
	FReadScopeLock ScopedReadLock(LogsLock);

	for (const TPair<FPCGStack, FPCGPerNodeVisualLogs>& Entry : StackToLogs)
	{
		if (Entry.Key.BeginsWith(InPCGStack))
		{
			for (const FPCGNodeLogEntry& Log : Entry.Value)
			{
				OutMinVerbosity = FMath::Min(OutMinVerbosity, Log.Verbosity);
			}
		}
	}

	return OutMinVerbosity != ELogVerbosity::All;
}

bool FPCGNodeVisualLogs::HasLogsOfVerbosity(const FPCGStack& InPCGStack, ELogVerbosity::Type InVerbosity) const
{
	FReadScopeLock ScopedReadLock(LogsLock);

	for (const TPair<FPCGStack, FPCGPerNodeVisualLogs>& Entry : StackToLogs)
	{
		if (Entry.Key.BeginsWith(InPCGStack))
		{
			if (Algo::FindByPredicate(Entry.Value, [InVerbosity](const FPCGNodeLogEntry& Log) { return Log.Verbosity == InVerbosity; }))
			{
				return true;
			}
		}
	}

	return false;
}

FText FPCGNodeVisualLogs::GetLogsSummaryText(const UPCGNode* InNode, ELogVerbosity::Type& OutMinimumVerbosity) const
{
	FReadScopeLock ScopedReadLock(LogsLock);

	// Compose user friendly summary of first N errors
	FText Summary = FText::GetEmpty();
	OutMinimumVerbosity = ELogVerbosity::All;

	int LogCounter = 0;

	for (const TPair<FPCGStack, FPCGPerNodeVisualLogs>& Entry : StackToLogs)
	{
		const FPCGStack& Stack = Entry.Key;

		if (Stack.HasObject(InNode))
		{
			const FPCGPerNodeVisualLogs& NodeLogs = Entry.Value;

			for (int LogIndex = 0; LogIndex < NodeLogs.Num(); ++LogIndex)
			{
				++LogCounter;

				if (LogCounter >= MaxLogsInSummary)
				{
					Summary = FText::Format(FText::FromString(TEXT("{0}\n...")), Summary);
					return Summary;
				}

				if (LogCounter > 1)
				{
					Summary = FText::Format(FText::FromString(TEXT("{0}\n")), Summary);
				}

				const UPCGComponent* Component = Stack.GetRootComponent();
				FText ActorName = (Component && Component->GetOwner()) ? FText::FromString(Component->GetOwner()->GetActorLabel()) : FText::FromString(TEXT("MissingComponent"));
				
				OutMinimumVerbosity = FMath::Min(OutMinimumVerbosity, NodeLogs[LogIndex].Verbosity);
				const FText VerbosityText = NodeLogs[LogIndex].Verbosity == ELogVerbosity::Warning ? FText::FromString(TEXT("Warning")) : FText::FromString(TEXT("Error"));

				Summary = FText::Format(LOCTEXT("NodeTooltipLogWithActor", "{0}[{1}] {2}: {3}"), Summary, ActorName, VerbosityText, NodeLogs[LogIndex].Message);
			}
		}
	}

	return Summary;
}

FText FPCGNodeVisualLogs::GetLogsSummaryText(const FPCGStack& InBaseStack, ELogVerbosity::Type* OutMinimumVerbosity) const
{
	FReadScopeLock ScopedReadLock(LogsLock);

	FText Summary = FText::GetEmpty();

	if (OutMinimumVerbosity)
	{
		*OutMinimumVerbosity = ELogVerbosity::All;
	}

	int32 LogCounter = 0;

	for (const TPair<FPCGStack, FPCGPerNodeVisualLogs>& Entry : StackToLogs)
	{
		if (!Entry.Key.BeginsWith(InBaseStack))
		{
			continue;
		}

		const FPCGPerNodeVisualLogs& NodeLogs = Entry.Value;

		for (int32 LogIndex = 0; LogIndex < NodeLogs.Num(); ++LogIndex)
		{
			++LogCounter;

			if (LogCounter > MaxLogsInSummary)
			{
				Summary = FText::Format(FText::FromString(TEXT("{0}\n...")), Summary);
				return Summary;
			}

			if (LogCounter > 1)
			{
				Summary = FText::Format(FText::FromString(TEXT("{0}\n")), Summary);
			}

			if (OutMinimumVerbosity)
			{
				*OutMinimumVerbosity = FMath::Min(*OutMinimumVerbosity, NodeLogs[LogIndex].Verbosity);
			}

			const FText MessageVerbosity = NodeLogs[LogIndex].Verbosity == ELogVerbosity::Warning ? FText::FromString(TEXT("Warning")) : FText::FromString(TEXT("Error"));
			Summary = FText::Format(LOCTEXT("NodeTooltipLog", "{0}{1}: {2}"), Summary, MessageVerbosity, NodeLogs[LogIndex].Message);
		}
	}

	return Summary;
}

void FPCGNodeVisualLogs::ClearLogs(const FPCGStack& InPCGStack)
{
	TSet<const UPCGNode*> TouchedNodes;

	{
		FWriteScopeLock ScopedWriteLock(LogsLock);

		TArray<FPCGStack> StacksToRemove;
		for (const TPair<FPCGStack, FPCGPerNodeVisualLogs>& Entry : StackToLogs)
		{
			if (Entry.Key.BeginsWith(InPCGStack))
			{
				StacksToRemove.Add(Entry.Key);

				for (const FPCGStackFrame& Frame : Entry.Key.GetStackFrames())
				{
					if (const UPCGNode* Node = Cast<const UPCGNode>(Frame.Object.Get()))
					{
						TouchedNodes.Add(Node);
					}
				}
			}
		}

		for (const FPCGStack& StackToRemove : StacksToRemove)
		{
			StackToLogs.Remove(StackToRemove);
		}
	}

	// Broadcast change notification outside of write scope lock
	for (const UPCGNode* TouchedNode : TouchedNodes)
	{
		TouchedNode->OnNodeChangedDelegate.Broadcast(const_cast<UPCGNode*>(TouchedNode), EPCGChangeType::Cosmetic);
	}
}

void FPCGNodeVisualLogs::ClearLogs(const UPCGComponent* InComponent)
{
	FPCGStack Stack;
	Stack.PushFrame(InComponent);
	ClearLogs(Stack);
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
