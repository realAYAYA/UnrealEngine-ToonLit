// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "IElectraDecoderResourceDelegateBase.h"

#if PLATFORM_MAC || PLATFORM_IOS || PLATFORM_TVOS

class IElectraDecoderResourceDelegateApple : public IElectraDecoderResourceDelegateBase
{
public:
	virtual ~IElectraDecoderResourceDelegateApple() = default;

	// Hardware decoders might need to know the current Metal device.
	virtual bool GetMetalDevice(void **OutMetalDevice) = 0;
};


using IElectraDecoderResourceDelegate = IElectraDecoderResourceDelegateApple;

#endif
