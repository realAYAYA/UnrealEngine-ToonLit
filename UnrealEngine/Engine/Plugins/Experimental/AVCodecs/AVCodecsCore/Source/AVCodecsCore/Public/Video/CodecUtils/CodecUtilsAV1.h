// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Utils/BitstreamReader.h"

#include "AVResult.h"
#include "Video/VideoPacket.h"

enum class EAV1Profile : uint8
{
    Auto,
    Main,
    // We currently don't support these other profiles
    // High,
    // Professional,
	MAX
};