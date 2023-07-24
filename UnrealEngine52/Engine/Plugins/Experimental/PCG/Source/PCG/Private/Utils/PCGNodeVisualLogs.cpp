// Copyright Epic Games, Inc. All Rights Reserved.

#include "Utils/PCGNodeVisualLogs.h"

#include "PCGComponent.h"

#include "Algo/Find.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "PCGNode"

#if WITH_EDITOR

void FPCGNodeVisualLogs::Log(TWeakObjectPtr<const UPCGNode> InNode, TWeakObjectPtr<UPCGComponent> InComponent, ELogVerbosity::Type InVerbosity, const FText& InMessage)
{
	if (!ensure(InNode.Get()) || !ensure(InComponent.Get()))
	{
		return;
	}

	bool bAdded = false;

	{
		FWriteScopeLock ScopedWriteLock(LogsLock);

		FPCGPerNodeVisualLogs* NodeLogs = NodeToLogs.Find(InNode);
		if (!NodeLogs)
		{
			NodeLogs = &NodeToLogs.Add(InNode);
		}

		constexpr int32 MaxLogged = 1024;
		if (NodeToLogs.Num() < MaxLogged)
		{
			NodeLogs->Emplace(InMessage, InVerbosity, InComponent);

			bAdded = true;
		}
	}

	// Broadcast outside of write scope lock
	if (bAdded)
	{
		InNode->OnNodeChangedDelegate.Broadcast(const_cast<UPCGNode*>(InNode.Get()), EPCGChangeType::Cosmetic);
	}
}

bool FPCGNodeVisualLogs::HasLogs(TWeakObjectPtr<const UPCGNode> InNode, const UPCGComponent* InComponent) const
{
	FReadScopeLock ScopedReadLock(LogsLock);

	if (!ensure(InNode.Get()))
	{
		return false;
	}

	const FPCGPerNodeVisualLogs* NodeLogs = NodeToLogs.Find(InNode);
	if (!NodeLogs)
	{
		return false;
	}

	if (!InComponent)
	{
		return !NodeLogs->IsEmpty();
	}
	else
	{
		return !!Algo::FindByPredicate(*NodeLogs, [InComponent](const FPCGNodeLogEntry& Log)
		{
			return Log.Component == InComponent;
		});
	}
}

bool FPCGNodeVisualLogs::HasLogs(TWeakObjectPtr<const UPCGNode> InNode, const UPCGComponent* InComponent, ELogVerbosity::Type InVerbosity) const
{
	FReadScopeLock ScopedReadLock(LogsLock);

	if (!ensure(InNode.Get()))
	{
		return false;
	}

	const FPCGPerNodeVisualLogs* NodeLogs = NodeToLogs.Find(InNode);
	if (!NodeLogs)
	{
		return false;
	}

	return !!Algo::FindByPredicate(*NodeLogs, [InVerbosity, InComponent](const FPCGNodeLogEntry& Log)
	{
		return Log.Verbosity == InVerbosity && (!InComponent || Log.Component == InComponent);
	});
}

FText FPCGNodeVisualLogs::GetLogsSummaryText(TWeakObjectPtr<const UPCGNode> InNode, const UPCGComponent* InComponent) const
{
	FReadScopeLock ScopedReadLock(LogsLock);

	if (!ensure(InNode.Get()))
	{
		return FText::GetEmpty();
	}

	const FPCGPerNodeVisualLogs* NodeLogsPtr = NodeToLogs.Find(InNode);
	if (!NodeLogsPtr)
	{
		return FText::GetEmpty();
	}
	const FPCGPerNodeVisualLogs& NodeLogs = *NodeLogsPtr;

	// Compose user friendly summary of first N errors
	FText ResultText = FText::GetEmpty();
	int32 LogCounter = 0;

	for (int32 i = 0; i < NodeLogs.Num(); ++i)
	{
		if (InComponent && NodeLogs[i].Component != InComponent)
		{
			continue;
		}

		++LogCounter;

		if (LogCounter > 1)
		{
			// New lines after each log
			ResultText = FText::Format(FText::FromString(TEXT("{0}\n")), ResultText);
		}

		const FText MessageVerbosity = NodeLogs[i].Verbosity == ELogVerbosity::Warning ? FText::FromString(TEXT("Warning")) : FText::FromString(TEXT("Error"));

		if (!InComponent)
		{
			FText ActorName = ensure(NodeLogs[i].Component.Get() && NodeLogs[i].Component->GetOwner()) ? FText::FromName(NodeLogs[i].Component->GetOwner()->GetFName()) : FText::FromString(TEXT("MissingComponent"));
			ResultText = FText::Format(LOCTEXT("NodeTooltipLogWithActor", "{0}{1}/{2}: [{3}] {4}: {5}"), ResultText, i + 1, NodeLogs.Num(), ActorName, MessageVerbosity, NodeLogs[i].Message);
		}
		else
		{
			ResultText = FText::Format(LOCTEXT("NodeTooltipLog", "{0}{1}/{2} {3}: {4}"), ResultText, i + 1, NodeLogs.Num(), MessageVerbosity, NodeLogs[i].Message);
		}

		constexpr int32 MaxLogs = 8;
		if (LogCounter >= MaxLogs)
		{
			ResultText = FText::Format(FText::FromString(TEXT("{0}\n...")), ResultText);
			break;
		}
	}

	return ResultText;
}

void FPCGNodeVisualLogs::ClearLogs(TWeakObjectPtr<const UPCGNode> InNode, const UPCGComponent* InComponent)
{
	if (!ensure(InNode.Get()) || !ensure(InComponent))
	{
		return;
	}

	FPCGPerNodeVisualLogs* NodeLogs = nullptr;
	{
		FReadScopeLock ScopedReadLock(LogsLock);
		NodeLogs = NodeToLogs.Find(InNode);

		if (!NodeLogs || NodeLogs->IsEmpty())
		{
			// No logs, nothing to do
			return;
		}
	}

	bool bAnyRemoved = false;
	{
		FWriteScopeLock ScopedWriteLock(LogsLock);

		check(NodeLogs);
		for (int32 i = NodeLogs->Num() - 1; i >= 0; --i)
		{
			// Remove entry if it matches the given component, or if the component is no longer valid
			if (!(*NodeLogs)[i].Component.IsValid() || (*NodeLogs)[i].Component == InComponent)
			{
				NodeLogs->RemoveAtSwap(i);
				bAnyRemoved = true;
			}
		}
	}

	// Broadcast change notification outside of write scope lock
	if (bAnyRemoved)
	{
		InNode->OnNodeChangedDelegate.Broadcast(const_cast<UPCGNode*>(InNode.Get()), EPCGChangeType::Cosmetic);
	}
}

#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
