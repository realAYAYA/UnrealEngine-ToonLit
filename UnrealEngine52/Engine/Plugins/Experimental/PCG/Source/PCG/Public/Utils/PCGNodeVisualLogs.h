// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogVerbosity.h"

#include "Containers/Map.h"
#include "Internationalization/Text.h"
#include "UObject/WeakObjectPtr.h"

#if WITH_EDITOR

class UPCGComponent;
class UPCGNode;

struct FPCGNodeLogEntry
{
	explicit FPCGNodeLogEntry(const FText& InMessage, ELogVerbosity::Type InVerbosity, TWeakObjectPtr<UPCGComponent> InComponent)
		: Message(InMessage)
		, Verbosity(InVerbosity)
		, Component(InComponent)
	{
	}

	FText Message;
	ELogVerbosity::Type Verbosity;
	TWeakObjectPtr<UPCGComponent> Component;
};

typedef TArray<FPCGNodeLogEntry, TInlineAllocator<16>> FPCGPerNodeVisualLogs;

/** Collections per-node graph execution warnings and errors. */
class PCG_API FPCGNodeVisualLogs
{
public:
	/** Log warnings and errors to be displayed on node in graph editor. */
	void Log(TWeakObjectPtr<const UPCGNode> InNode, TWeakObjectPtr<UPCGComponent> InComponent, ELogVerbosity::Type InVerbosity, const FText& InMessage);

	/** True if any issues were logged during last execution. */
	bool HasLogs(TWeakObjectPtr<const UPCGNode> InNode, const UPCGComponent* InComponent) const;

	/** True if an issue with given severity was logged during last execution. */
	bool HasLogs(TWeakObjectPtr<const UPCGNode> InNode, const UPCGComponent* InComponent, ELogVerbosity::Type InVerbosity) const;

	/** Summary text of visual logs from recent execution, appropriate for display in graph editor tooltip. */
	FText GetLogsSummaryText(TWeakObjectPtr<const UPCGNode> InNode, const UPCGComponent* InComponent = nullptr) const;

	/** Clear all errors and warnings that occurred while executing the given component. */
	void ClearLogs(TWeakObjectPtr<const UPCGNode> InNode, const UPCGComponent* InComponent);

private:
	TMap<TWeakObjectPtr<const UPCGNode>, FPCGPerNodeVisualLogs> NodeToLogs;

	mutable FRWLock LogsLock;
};

#endif // WITH_EDITOR
