// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaViewportVirtualSizeEnums.generated.h"

UENUM()
enum class EAvaViewportVirtualSizeAspectRatioState : uint8
{
	Unlocked,
	Locked,
	LockedToCamera
};
