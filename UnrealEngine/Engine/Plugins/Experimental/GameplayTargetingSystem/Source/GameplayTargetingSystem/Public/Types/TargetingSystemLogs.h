// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Logging/LogMacros.h"
#include "Stats/Stats2.h"

DECLARE_STATS_GROUP(TEXT("TargetingSystem"), STATGROUP_TargetingSystem, STATCAT_Advanced);

DECLARE_LOG_CATEGORY_EXTERN(LogTargetingSystem, Log, All);

#define TARGETING_LOG(Verbosity, Format, ...) UE_LOG(LogTargetingSystem, Verbosity, Format, ##__VA_ARGS__)
