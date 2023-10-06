// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystem.h"


#if WITH_ENGINE
#endif

#define RAIL_METADATA_KEY_SEPARATOR TEXT(",")

/** pre-pended to all mcp logging */
#undef ONLINE_LOG_PREFIX
#define ONLINE_LOG_PREFIX TEXT("TENCENT: ")
