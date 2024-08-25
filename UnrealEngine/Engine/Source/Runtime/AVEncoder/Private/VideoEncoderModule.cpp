// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoEncoderCommon.h"
#include "Modules/ModuleManager.h"

#include "VideoEncoderFactory.h"

class FAVEncoderModule : public IModuleInterface
{
public:

	void ShutdownModule()
	{
		PRAGMA_DISABLE_DEPRECATION_WARNINGS
		AVEncoder::FVideoEncoderFactory::Shutdown();
		PRAGMA_ENABLE_DEPRECATION_WARNINGS
	}
};

IMPLEMENT_MODULE(FAVEncoderModule, AVEncoder);
