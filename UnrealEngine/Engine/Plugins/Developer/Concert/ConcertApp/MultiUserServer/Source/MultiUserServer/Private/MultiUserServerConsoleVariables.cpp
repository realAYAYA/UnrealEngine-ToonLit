// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiUserServerConsoleVariables.h"

#include "Containers/UnrealString.h"

TAutoConsoleVariable<bool> UE::MultiUserServer::ConsoleVariables::CVarLogActivityDependencyGraphOnDelete(TEXT("MultiUserServer.LogActivityDependencyGraphOnDelete"), false, TEXT("When a user requests to delete any activity, log the dependency graph (in GraphViz format)"));
