// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#if ELECTRA_DECODERS_HAVE_PLATFORM_DEFAULTS
#include COMPILED_PLATFORM_HEADER(PlatformElectraDecoders.h)
#else

class IElectraCodecRegistry;
class FPlatformElectraDecodersNull
{
public:
	static void Startup()
	{
	}
	static void Shutdown()
	{
	}
	static void RegisterWithCodecFactory(IElectraCodecRegistry* InCodecFactoryToRegisterWith)
	{
	}
};

using FPlatformElectraDecoders = FPlatformElectraDecodersNull;

#endif
