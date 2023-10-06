// Copyright Epic Games, Inc. All Rights Reserved.

#include "MemoryUsageQueriesModule.h"
#include "MemoryUsageQueries.h"

#include "Engine/Console.h"
#include "Modules/ModuleManager.h"

void FMemoryUsageQueriesModule::StartupModule()
{
}

void FMemoryUsageQueriesModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FMemoryUsageQueriesModule, MemoryUsageQueries);
