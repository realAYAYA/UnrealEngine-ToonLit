// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"

THIRD_PARTY_INCLUDES_START
#include <VideoToolbox/VideoToolbox.h>
THIRD_PARTY_INCLUDES_END

class VTSessionHelpers
{
public:
    static FString CFStringToString(const CFStringRef CfString);
	static void SetVTSessionProperty(VTSessionRef Session, CFStringRef Key, int32_t Value);
	static void SetVTSessionProperty(VTSessionRef Session, CFStringRef Key, bool Value);
	static void SetVTSessionProperty(VTSessionRef Session, CFStringRef Key, CFStringRef Value);
};
