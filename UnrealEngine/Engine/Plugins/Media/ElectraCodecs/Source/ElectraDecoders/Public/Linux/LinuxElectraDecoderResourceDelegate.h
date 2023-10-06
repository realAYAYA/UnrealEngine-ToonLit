// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "IElectraDecoderResourceDelegateBase.h"

#if PLATFORM_LINUX || PLATFORM_UNIX

class IElectraDecoderResourceDelegateLinux : public IElectraDecoderResourceDelegateBase
{
public:
	virtual ~IElectraDecoderResourceDelegateLinux() = default;
};


using IElectraDecoderResourceDelegate = IElectraDecoderResourceDelegateLinux;

#endif
