// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/PCGStackContext.h"

#include "Containers/Map.h"
#include "Internationalization/Text.h"
#include "Logging/LogVerbosity.h"
#include "UObject/WeakObjectPtr.h"

#if WITH_EDITOR

class UPCGComponent;
class UPCGNode;

struct FPCGNodeLogEntry
{
	explicit FPCGNodeLogEntry(const FText& InMessage, ELogVerbosity::Type InVerbosity)
		: Message(InMessage)
		, Verbosity(InVerbosity)
	{
	}

	FText Message;
	ELogVerbosity::Type Verbosity;
};

typedef TArray<FPCGNodeLogEntry, TInlineAllocator<16>> FPCGPerNodeVisualLogs;

/** Collections per-node graph execution warnings and errors. */
class PCG_API FPCGNodeVisualLogs
{
public:
	/** Log warnings and errors to be displayed on node in graph editor. */
	void Log(const FPCGStack& InPCGStack, ELogVerbosity::Type InVerbosity, const FText& InMessage);

	/** Returns true if any issues were logged during last execution. */
	bool HasLogs(const FPCGStack& InPCGStack) const;

	/** Returns true if an issue with given severity was logged during last execution, and writes the minimum encountered verbosity to OutMinVerbosity. */
	bool HasLogs(const FPCGStack& InPCGStack, ELogVerbosity::Type& OutMinVerbosity) const;

	/** Returns true if an issue with given severity was logged during last execution. */
	bool HasLogsOfVerbosity(const FPCGStack& InPCGStack, ELogVerbosity::Type InVerbosity) const;

	/** Summary text of all visual logs produced while executing the provided base stack, appropriate for display in graph editor tooltip. */
	FText GetLogsSummaryText(const FPCGStack& InBaseStack, ELogVerbosity::Type* OutMinimumVerbosity = nullptr) const;

	/**
	* Returns summary text of visual logs from recent execution, appropriate for display in graph editor tooltip. Writes the minimum encountered verbosity
	* to OutMinimumVerbosity.
	*/
	FText GetLogsSummaryText(const UPCGNode* InNode, ELogVerbosity::Type& OutMinimumVerbosity) const;

	/** Clear all errors and warnings that occurred while executing stacks beginning with the given stack. */
	void ClearLogs(const FPCGStack& InPCGStack);

	/** Clear all errors and warnings corresponding to the given component. */
	void ClearLogs(const UPCGComponent* InComponent);

private:
	const int MaxLogsInSummary = 8;

	TMap<FPCGStack, FPCGPerNodeVisualLogs> StackToLogs;
	mutable FRWLock LogsLock;
};

#endif // WITH_EDITOR
