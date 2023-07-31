// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"


/** Maximum time that TickInput/Fetch will block waiting for samples (in seconds). */
#define MEDIAUTILS_MAX_BLOCKONFETCH_SECONDS 10


/** Declares a log category for this module. */
DECLARE_LOG_CATEGORY_EXTERN(LogMediaUtils, Log, All);
