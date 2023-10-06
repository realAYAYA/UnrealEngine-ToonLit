// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

#include "IVulkanDynamicRHI.h"
#include "Misc/App.h"

#include "Video/Resources/VideoResourceRHI.h"
#include "Video/Resources/VideoResourceVulkan.h"
#include "Video/Decoders/VideoDecoderRHI.h"
#include "Video/Decoders/Configs/VideoDecoderConfigH264.h"
#include "Video/Decoders/Configs/VideoDecoderConfigH265.h"
#include "Video/Encoders/VideoEncoderRHI.h"
#include "Video/Encoders/Configs/VideoEncoderConfigH264.h"
#include "Video/Encoders/Configs/VideoEncoderConfigH265.h"

#if PLATFORM_WINDOWS
#include "ID3D11DynamicRHI.h"
#include "ID3D12DynamicRHI.h"

#include "Video/Resources/Windows/VideoResourceD3D.h"
#endif

class FAVCodecsCoreRHI : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		if (FApp::CanEverRender())
		{
#if PLATFORM_WINDOWS
			TCHAR const* DynamicRHIModuleName = GetSelectedDynamicRHIModuleName(false);
#elif PLATFORM_LINUX
			TCHAR const* DynamicRHIModuleName = TEXT("VulkanRHI");
#endif

			if (FString("VulkanRHI") == FString(DynamicRHIModuleName))
			{
#if PLATFORM_WINDOWS
				TArray<ANSICHAR const*> const ExtensionsToAdd { VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME, VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME };
#elif PLATFORM_LINUX
				TArray<ANSICHAR const*> const ExtensionsToAdd { VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME, VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME };
#endif

				IVulkanDynamicRHI::AddEnabledDeviceExtensionsAndLayers(ExtensionsToAdd, TArray<ANSICHAR const*>());
			}

			FCoreDelegates::OnPostEngineInit.AddLambda([]()
			{
				switch (GDynamicRHI->GetInterfaceType())
				{
				case ERHIInterfaceType::Vulkan:
					FAVDevice::GetHardwareDevice()->SetContext<FVideoContextVulkan>(
						MakeShared<FVideoContextVulkan>(
							GetIVulkanDynamicRHI()->RHIGetVkInstance(),
							GetIVulkanDynamicRHI()->RHIGetVkDevice(),
							GetIVulkanDynamicRHI()->RHIGetVkPhysicalDevice(),
							[] (char const* Name) -> PFN_vkVoidFunction {
								return (PFN_vkVoidFunction)GetIVulkanDynamicRHI()->RHIGetVkDeviceProcAddr(Name);
							}));
				
					break;
#if PLATFORM_WINDOWS
				case ERHIInterfaceType::D3D11:
					FAVDevice::GetHardwareDevice()->SetContext<FVideoContextD3D11>(
						MakeShared<FVideoContextD3D11>(
							GetID3D11DynamicRHI()->RHIGetDevice()));
				
					break;
				case ERHIInterfaceType::D3D12:
					FAVDevice::GetHardwareDevice()->SetContext<FVideoContextD3D12>(
						MakeShared<FVideoContextD3D12>(
							GetID3D12DynamicRHI()->RHIGetDevice(0)));
				
					break;
#endif
				default:
					break;
				}

				FAVDevice::GetHardwareDevice()->SetContext<FVideoContextRHI>(MakeShared<FVideoContextRHI>());
			});
		}

		FVideoEncoder::Register<TVideoEncoderRHI<FVideoEncoderConfigH264>, FVideoResourceRHI, FVideoEncoderConfigH264>();
		FVideoEncoder::Register<TVideoEncoderRHI<FVideoEncoderConfigH265>, FVideoResourceRHI, FVideoEncoderConfigH265>();

		FVideoDecoder::Register<TVideoDecoderRHI<FVideoDecoderConfigH264>, FVideoResourceRHI, FVideoDecoderConfigH264>();
		FVideoDecoder::Register<TVideoDecoderRHI<FVideoDecoderConfigH265>, FVideoResourceRHI, FVideoDecoderConfigH265>();
	}
};

IMPLEMENT_MODULE(FAVCodecsCoreRHI, AVCodecsCoreRHI);
