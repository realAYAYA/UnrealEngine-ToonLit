// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "IVulkanDynamicRHI.h"

#include "NVENC_Common.h"
#include "NVENC_EncoderH264.h"
#include "VideoEncoderFactory.h"

#include "Misc/CoreDelegates.h"

class FNVENCEncoderModule : public IModuleInterface
{
public:
	void StartupModule()
	{
		using namespace AVEncoder;
		if (FApp::CanEverRender())
		{
			FNVENCCommon& NVENC = FNVENCCommon::Setup();

			if (NVENC.GetIsAvailable())
			{
#if PLATFORM_WINDOWS
                const TCHAR* DynamicRHIModuleName = GetSelectedDynamicRHIModuleName(false);
#elif PLATFORM_LINUX
                const TCHAR* DynamicRHIModuleName = TEXT("VulkanRHI");
#endif
				
				if (FString("VulkanRHI") == FString(DynamicRHIModuleName))
				{					
#if PLATFORM_WINDOWS
					const TArray<const ANSICHAR*> ExtentionsToAdd{ VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME, VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME };
#elif PLATFORM_LINUX
					const TArray<const ANSICHAR*> ExtentionsToAdd{ VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME, VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME };
#endif
					IVulkanDynamicRHI::AddEnabledDeviceExtensionsAndLayers(ExtentionsToAdd, TArray<const ANSICHAR*>());
				}

				FModuleManager::LoadModuleChecked<FCUDAModule>("CUDA").OnPostCUDAInit.AddLambda([]() {FVideoEncoderNVENC_H264::Register(FVideoEncoderFactory::Get());});
			}
		}
	}
};

IMPLEMENT_MODULE(FNVENCEncoderModule, EncoderNVENC);
