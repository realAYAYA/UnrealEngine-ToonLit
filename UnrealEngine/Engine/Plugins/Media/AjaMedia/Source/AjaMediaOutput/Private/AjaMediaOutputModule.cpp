// Copyright Epic Games, Inc. All Rights Reserved.

#include "AjaMediaOutputModule.h"

#include "AjaMediaFrameGrabberProtocol.h"
#include "AjaLib.h"
#include "DynamicRHI.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "Misc/CoreDelegates.h"
#include "RenderingThread.h"
#include "RHI.h"

#define LOCTEXT_NAMESPACE "AjaMediaOutput"

DEFINE_LOG_CATEGORY(LogAjaMediaOutput);

void FAjaMediaOutputModule::StartupModule()
{
	const auto DMAInitializationFunc = [this]()
	{
		static const TArray<FString> SupportedGPUPrefixes= { 
			TEXT("RTX A4"),
			TEXT("RTX A5"),
			TEXT("RTX A6"),
			TEXT("Quadro")
		};

		const FGPUDriverInfo GPUDriverInfo = FPlatformMisc::GetGPUDriverInfo(GRHIAdapterName);
		bIsGPUTextureTransferAvailable = GPUDriverInfo.IsNVIDIA() &&!FModuleManager::Get().IsModuleLoaded("RenderDocPlugin");
		bIsGPUTextureTransferAvailable &= !GPUDriverInfo.DeviceDescription.Contains(TEXT("Tesla"));
		
		if (bIsGPUTextureTransferAvailable)
		{
			bIsGPUTextureTransferAvailable = false;
			for (const FString& GPUPrefix : SupportedGPUPrefixes)
			{
				if (GPUDriverInfo.DeviceDescription.Contains(GPUPrefix))
				{
					bIsGPUTextureTransferAvailable = true;
					break;
				}
			}
		}

		if (bIsGPUTextureTransferAvailable)
		{
			ENQUEUE_RENDER_COMMAND(AjaMediaCaptureInitialize)(
				[this](FRHICommandListImmediate& RHICmdList) mutable
				{
					if (!GDynamicRHI)
					{
						return;
					}

					auto GetRHI = []()
					{
						switch (RHIGetInterfaceType())
						{
						case ERHIInterfaceType::D3D11: return AJA::ERHI::D3D11;
						case ERHIInterfaceType::D3D12: return AJA::ERHI::D3D12;
						case ERHIInterfaceType::Vulkan: return AJA::ERHI::Vulkan;
						default: return AJA::ERHI::Invalid;
						}
					};

					AJA::FInitializeDMAArgs Args;
					AJA::ERHI RHI = GetRHI();
					Args.RHI = RHI;
					/* Re-enable when adding vulkan support
					if (RHI == AJA::ERHI::Vulkan)
					{
						FVulkanDynamicRHI* vkDynamicRHI = GetDynamicRHI<FVulkanDynamicRHI>();
						Args.InVulkanInstance = vkDynamicRHI->GetInstance();

						FMemory::Memcpy(Args.InRHIDeviceUUID, vkDynamicRHI->GetDevice()->GetDeviceIdProperties().deviceUUID, 16);
					}
					*/
					Args.RHIDevice = GDynamicRHI->RHIGetNativeDevice();
					Args.RHICommandQueue = GDynamicRHI->RHIGetNativeGraphicsQueue();

					UE_LOG(LogAjaMediaOutput, Display, TEXT("Initializing GPU Texture transfer from AJA"));
					bIsGPUTextureTransferAvailable = AJA::InitializeDMA(Args);
				});
		}
	};
	
	auto DMAUninitialize = [this]()
	{
		if (bIsGPUTextureTransferAvailable)
		{
			ENQUEUE_RENDER_COMMAND(AjaMediaOutputModuleUninitialize)(
				[](FRHICommandListImmediate& RHICmdList) mutable
				{
					AJA::UninitializeDMA();
				});
		}
	};

	//Postpone initialization after all modules have been loaded to be sure Aja library has been loaded
	FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddLambda(DMAInitializationFunc);
	//Same for shutdown, uninitialize ourselves before library is unloaded
	FCoreDelegates::OnEnginePreExit.AddLambda(DMAUninitialize);
}

void FAjaMediaOutputModule::ShutdownModule()
{

}

bool FAjaMediaOutputModule::IsGPUTextureTransferAvailable() const
{
	return bIsGPUTextureTransferAvailable;
}

IMPLEMENT_MODULE(FAjaMediaOutputModule, AjaMediaOutput )

#undef LOCTEXT_NAMESPACE
