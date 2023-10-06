// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"

#if PLATFORM_LINUX || PLATFORM_UNIX

#include "IElectraCodecFactory.h"

class FAACAudioDecoderLinux
{
public:
	static TSharedPtr<IElectraCodecFactory, ESPMode::ThreadSafe> CreateFactory();

};

#endif
