// Copyright Epic Games, Inc. All Rights Reserved.

#include "AVDevice.h"

#include "Modules/ModuleManager.h"

#include "Video/Resources/VideoResourceCPU.h"

class FAVCodecCoreModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		FAVDevice::GetSoftwareDevice()->SetContext<FVideoContextCPU>(
			MakeShared<FVideoContextCPU>());
	}
};

IMPLEMENT_MODULE(FAVCodecCoreModule, AVCodecsCore);
