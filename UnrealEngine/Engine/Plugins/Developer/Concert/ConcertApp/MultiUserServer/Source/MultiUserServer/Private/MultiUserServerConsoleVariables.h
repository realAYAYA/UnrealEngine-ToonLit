// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

namespace UE::MultiUserServer::ConsoleVariables
{
	/** When a user requests to delete any activity, log the dependency graph (in GraphViz format) */
	extern TAutoConsoleVariable<bool> CVarLogActivityDependencyGraphOnDelete;
}