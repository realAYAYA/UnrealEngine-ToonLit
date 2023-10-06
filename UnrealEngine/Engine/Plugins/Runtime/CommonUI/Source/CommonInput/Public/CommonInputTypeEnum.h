// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/EnumRange.h"
#include "CommonInputTypeEnum.generated.h"

UENUM(BlueprintType)
enum class ECommonInputType : uint8
{
	MouseAndKeyboard,
	Gamepad,
	Touch,
	Count
};

ENUM_RANGE_BY_COUNT(ECommonInputType, ECommonInputType::Count);
