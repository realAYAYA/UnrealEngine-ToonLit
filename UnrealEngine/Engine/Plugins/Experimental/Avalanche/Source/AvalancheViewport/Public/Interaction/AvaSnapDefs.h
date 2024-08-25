// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaSnapDefs.generated.h"

UENUM(BlueprintType, Meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EAvaViewportSnapState : uint8
{
	Off = 0 UMETA(Hidden),
	Global = 1 << 0,
	Screen = 1 << 1,
	Grid = 1 << 2,
	Actor = 1 << 3,
	All = Screen | Grid | Actor
};

ENUM_CLASS_FLAGS(EAvaViewportSnapState);
