// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
PHYSICSCONTROL_API DECLARE_LOG_CATEGORY_EXTERN(LogPhysicsControl, Log, Warning);
#else
PHYSICSCONTROL_API DECLARE_LOG_CATEGORY_EXTERN(LogPhysicsControl, Log, All);
#endif


