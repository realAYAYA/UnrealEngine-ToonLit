// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
CHAOS_API DECLARE_LOG_CATEGORY_EXTERN(LogChaosJoint, Log, Warning);
#else
CHAOS_API DECLARE_LOG_CATEGORY_EXTERN(LogChaosJoint, Log, All);
#endif
