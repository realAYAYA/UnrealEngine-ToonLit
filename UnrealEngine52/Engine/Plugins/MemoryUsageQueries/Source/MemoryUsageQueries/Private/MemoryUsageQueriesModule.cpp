// Copyright Epic Games, Inc. All Rights Reserved.

#include "MemoryUsageQueriesModule.h"
#include "MemoryUsageQueries.h"

#include "Engine/Console.h"
#include "Modules/ModuleManager.h"

void FMemoryUsageQueriesModule::StartupModule()
{
	Handle_RegisterConsoleAutoCompleteEntries = UConsole::RegisterConsoleAutoCompleteEntries.AddStatic(MemoryUsageQueries::RegisterConsoleAutoCompleteEntries);
}

void FMemoryUsageQueriesModule::ShutdownModule()
{
	UConsole::RegisterConsoleAutoCompleteEntries.Remove(Handle_RegisterConsoleAutoCompleteEntries);
}

IMPLEMENT_MODULE(FMemoryUsageQueriesModule, MemoryUsageQueries);
