// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"

/** How much to scroll for each click of the mouse wheel (in Slate Screen Units). */
extern SLATECORE_API TAutoConsoleVariable<float> GlobalScrollAmount;
inline float GetGlobalScrollAmount() { return GlobalScrollAmount.GetValueOnAnyThread(); }

/**  */
extern SLATECORE_API float GSlateContrast;
