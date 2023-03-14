// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OnlineSubsystem.h"

#include "OnlineSubsystemTencent.h"
#include "OnlineSubsystemTencentTypes.h"

#if WITH_ENGINE
#include "OnlineSubsystemUtils.h"
#endif

#define RAIL_METADATA_KEY_SEPARATOR TEXT(",")

/** pre-pended to all mcp logging */
#undef ONLINE_LOG_PREFIX
#define ONLINE_LOG_PREFIX TEXT("TENCENT: ")
