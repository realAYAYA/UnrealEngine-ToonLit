// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "RHI.h"

#include "NVDEC.h"
#include "NVENC.h"

class FNVCodecRHIModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (FApp::CanEverRender())
		{
			FCoreDelegates::OnPostEngineInit.AddLambda([]()
				{
					const_cast<FNVDEC&>(FAPI::Get<FNVDEC>()).bHasCompatibleGPU = IsRHIDeviceNVIDIA();
					const_cast<FNVENC&>(FAPI::Get<FNVENC>()).bHasCompatibleGPU = IsRHIDeviceNVIDIA();
				});
		}
	}
};

IMPLEMENT_MODULE(FNVCodecRHIModule, NVCodecsRHI);
