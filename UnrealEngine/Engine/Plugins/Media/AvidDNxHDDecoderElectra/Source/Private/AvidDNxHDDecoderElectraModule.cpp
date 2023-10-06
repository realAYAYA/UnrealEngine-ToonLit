// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvidDNxHDDecoderElectraModule.h"
#include "Modules/ModuleManager.h"

#if PLATFORM_WINDOWS
#include "Windows/ElectraMediaAvidDNxHDDecoder.h"
#endif

#define LOCTEXT_NAMESPACE "AvidDNxHDDecoderElectraModule"

DEFINE_LOG_CATEGORY(LogAvidDNxHDElectraDecoder);

class FAvidDNxHDElectraDecoderModule : public IModuleInterface
{
public:
	void StartupModule() override
	{
#if PLATFORM_WINDOWS
		FElectraMediaAvidDNxHDDecoder::Startup();
#endif		
	}

	void ShutdownModule() override
	{
#if PLATFORM_WINDOWS
		FElectraMediaAvidDNxHDDecoder::Shutdown();
#endif		
	}

	bool SupportsDynamicReloading() override
	{
		// Codec could still be in use
		return false;
	}
};

IMPLEMENT_MODULE(FAvidDNxHDElectraDecoderModule, AvidDNxHDDecoderElectra);

#undef LOCTEXT_NAMESPACE
