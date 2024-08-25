// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

class FWindowsPlatformPerfCounters
{
public:
	static CORE_API void Init();
	static CORE_API void Shutdown();
};

typedef FWindowsPlatformPerfCounters FPlatformPerfCounters;