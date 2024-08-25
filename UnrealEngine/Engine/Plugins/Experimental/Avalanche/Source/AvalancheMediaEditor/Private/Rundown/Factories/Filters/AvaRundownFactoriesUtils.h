// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Misc/EnumClassFlags.h"

/** Type of the Page list */
enum class EAvaRundownSearchListType : uint8
{
	None      = 0x00, // None
	Template  = 0x01, // Template pages type
	Instanced = 0x10 // Instanced pages type
};
ENUM_CLASS_FLAGS(EAvaRundownSearchListType)
