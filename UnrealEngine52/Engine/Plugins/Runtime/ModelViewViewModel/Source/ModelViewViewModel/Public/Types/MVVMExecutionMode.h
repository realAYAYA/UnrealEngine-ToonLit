// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVMExecutionMode.generated.h"


/** */
UENUM()
enum class EMVVMExecutionMode : uint8
{
	/** Execute the binding as soon as the source value changes. */
	Immediate,
	/** Execute the binding at the end of the frame before drawing when the source value changes. */
	Delayed,
	/** Always execute the binding at the end of the frame. */
	Tick,
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
