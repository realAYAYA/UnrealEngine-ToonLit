// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaRundownDefines.generated.h"

UENUM(BlueprintType, DisplayName = "Motion Design Rundown Page Play Type")
enum class EAvaRundownPagePlayType : uint8
{
	PlayFromStart,
	PreviewFromStart,
	PreviewFromFrame
};
