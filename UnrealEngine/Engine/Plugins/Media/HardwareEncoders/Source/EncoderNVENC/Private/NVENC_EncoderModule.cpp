// Copyright Epic Games, Inc. All Rights Reserved.


#include "CudaModule.h"
#include "IVulkanDynamicRHI.h"

#include "Misc/App.h"
#include "NVENC_Common.h"
#include "NVENC_EncoderH264.h"
#include "VideoEncoderFactory.h"

#include <vulkan_core.h>

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

				FModuleManager::LoadModuleChecked<FCUDAModule>("CUDA").OnPostCUDAInit.AddLambda([]()
				{
					if (IsRHIDeviceNVIDIA())
					{
						PRAGMA_DISABLE_DEPRECATION_WARNINGS
						FVideoEncoderNVENC_H264::Register(FVideoEncoderFactory::Get());
						PRAGMA_ENABLE_DEPRECATION_WARNINGS
					}
					
				});
			}
		}
	}
};

IMPLEMENT_MODULE(FNVENCEncoderModule, EncoderNVENC);
