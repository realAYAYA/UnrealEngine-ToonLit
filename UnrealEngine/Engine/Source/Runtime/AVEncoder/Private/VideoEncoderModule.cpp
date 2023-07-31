// Copyright Epic Games, Inc. All Rights Reserved.

#include "VideoEncoderCommon.h"
#include "Modules/ModuleManager.h"

#include "VideoEncoderFactory.h"

class FAVEncoderModule : public IModuleInterface
{
public:

	void ShutdownModule()
	{
		AVEncoder::FVideoEncoderFactory::Shutdown();
	}
};

IMPLEMENT_MODULE(FAVEncoderModule, AVEncoder);
