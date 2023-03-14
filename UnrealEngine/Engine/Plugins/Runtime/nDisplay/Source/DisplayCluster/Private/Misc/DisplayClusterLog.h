// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats2.h"

// Module-wide log categories
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterEngine,     Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterModule,     Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterCluster,    Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterConfig,     Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterGame,       Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterNetwork,    Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterRender,     Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterRenderSync, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterViewport,   Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogDisplayClusterBlueprint,  Log, All);


DECLARE_STATS_GROUP(TEXT("nDisplay"), STATGROUP_NDisplay, STATCAT_Advanced)