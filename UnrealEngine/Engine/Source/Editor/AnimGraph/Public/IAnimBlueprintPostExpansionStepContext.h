// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FCompilerResultsLog;
class UEdGraph;
struct FKismetCompilerOptions;

// Interface passed to PostExpansionStep delegate
class IAnimBlueprintPostExpansionStepContext
{
public:
	virtual ~IAnimBlueprintPostExpansionStepContext() {}

	// Get the message log for the current compilation
	FCompilerResultsLog& GetMessageLog() const { return GetMessageLogImpl(); }

	// Get the consolidated uber graph during compilation
	UEdGraph* GetConsolidatedEventGraph() const { return GetConsolidatedEventGraphImpl(); }

	// Get the compiler options we are currently using
	const FKismetCompilerOptions& GetCompileOptions() const { return GetCompileOptionsImpl(); }

protected:
	// Get the message log for the current compilation
	virtual FCompilerResultsLog& GetMessageLogImpl() const = 0;

	// Get the consolidated uber graph during compilation
	virtual UEdGraph* GetConsolidatedEventGraphImpl() const = 0;

	// Get the compiler options we are currently using
	virtual const FKismetCompilerOptions& GetCompileOptionsImpl() const = 0;
};
