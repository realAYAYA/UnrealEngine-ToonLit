// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NavDataGatheringMode.generated.h"

UENUM()
enum class ENavDataGatheringMode : uint8
{
	Default,
	Instant,
	Lazy
};
