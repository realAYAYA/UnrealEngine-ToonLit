// Copyright Epic Games, Inc. All Rights Reserved.

#include "GPUTextureTransferModule.h"

#if DVP_SUPPORTED_PLATFORM
#include "D3D11TextureTransfer.h"
#include "D3D12TextureTransfer.h"
#include "VulkanTextureTransfer.h"
#include "TextureTransferBase.h"
#endif

#if DVP_SUPPORTED_PLATFORM || PLATFORM_LINUX
#define VULKAN_PLATFORM 1
#else
#define VULKAN_PLATFORM 0
#endif

#if VULKAN_PLATFORM
#include "IVulkanDynamicRHI.h"
#endif

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformDriver.h"
#include "HAL/PlatformMisc.h"
#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"

DEFINE_LOG_CATEGORY(LogGPUTextureTransfer);

namespace 
{
	auto ConvertRHI = [](ERHIInterfaceType RHI)
	{
		switch (RHI)
		{
		case ERHIInterfaceType::D3D11: return UE::GPUTextureTransfer::ERHI::D3D11;
		case ERHIInterfaceType::D3D12: return UE::GPUTextureTransfer::ERHI::D3D12;
		case ERHIInterfaceType::Vulkan: return UE::GPUTextureTransfer::ERHI::Vulkan;
		default: return UE::GPUTextureTransfer::ERHI::Invalid;
		}
	};
}

void FGPUTextureTransferModule::StartupModule()
{
	if (FApp::CanEverRender())
	{
		if (LoadGPUDirectBinary())
		{
			// Always provide the necessary Vulkan extensions (it will just get ignored if a different RHI is used)
			{
#if DVP_SUPPORTED_PLATFORM
				const TArray<const ANSICHAR*> ExtentionsToAdd{ VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME, VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME, VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME, VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };
#elif PLATFORM_LINUX
				const TArray<const ANSICHAR*> ExtentionsToAdd{ VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME, VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME, VK_KHR_SURFACE_EXTENSION_NAME };
#endif

#if VULKAN_PLATFORM
				IVulkanDynamicRHI::AddEnabledDeviceExtensionsAndLayers(ExtentionsToAdd, TArray<const ANSICHAR*>());
#endif
			}

			TransferObjects.AddDefaulted(RHI_MAX);

			// Since this module is started before the RHI is initialized, we have to delay initialization to later. 
			FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddRaw(this, &FGPUTextureTransferModule::InitializeTextureTransfer);
			// Same for shutdown, uninitialize ourselves before library is unloaded
			FCoreDelegates::OnEnginePreExit.AddRaw(this, &FGPUTextureTransferModule::UninitializeTextureTransfer);
		}
	}
}

void FGPUTextureTransferModule::ShutdownModule()
{
}

UE::GPUTextureTransfer::TextureTransferPtr FGPUTextureTransferModule::GetTextureTransfer()
{
#if DVP_SUPPORTED_PLATFORM
	UE::GPUTextureTransfer::ERHI SupportedRHI = ConvertRHI(RHIGetInterfaceType());
	if (SupportedRHI == UE::GPUTextureTransfer::ERHI::Invalid) 
	{
		UE_LOG(LogGPUTextureTransfer, Error, TEXT("The current RHI is not supported with GPU Texture Transfer."));
		return nullptr;
	}
	
	const uint8 RHIIndex = static_cast<uint8>(SupportedRHI);
	if (TransferObjects[RHIIndex])
	{
		return TransferObjects[RHIIndex];
	}
#endif
	return nullptr;
}

bool FGPUTextureTransferModule::IsAvailable()
{
#if DVP_SUPPORTED_PLATFORM
	return FModuleManager::Get().IsModuleLoaded("GPUTextureTransfer");
#else
	return false;
#endif
}

FGPUTextureTransferModule& FGPUTextureTransferModule::Get()
{
	return FModuleManager::LoadModuleChecked<FGPUTextureTransferModule>("GPUTextureTransfer");
}

bool FGPUTextureTransferModule::LoadGPUDirectBinary()
{
#if DVP_SUPPORTED_PLATFORM
	FString GPUDirectPath = FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/NVIDIA/GPUDirect"), FPlatformProcess::GetBinariesSubdirectory());
	FPlatformProcess::PushDllDirectory(*GPUDirectPath);

	FString DVPDll;

	DVPDll = TEXT("dvp.dll");

	DVPDll = FPaths::Combine(GPUDirectPath, DVPDll);

	TextureTransferHandle = FPlatformProcess::GetDllHandle(*DVPDll);
	if (TextureTransferHandle == nullptr)
	{
		UE_LOG(LogGPUTextureTransfer, Display, TEXT("Failed to load required library %s. GPU Texture transfer will not be functional."), *DVPDll);
	}

	FPlatformProcess::PopDllDirectory(*GPUDirectPath);

#endif
	return !!TextureTransferHandle;
}


void FGPUTextureTransferModule::InitializeTextureTransfer()
{
#if DVP_SUPPORTED_PLATFORM
	bIsGPUTextureTransferAvailable = true;

	static const TArray<FString> SupportedGPUPrefixes = {
		TEXT("RTX A4"),
		TEXT("RTX A5"),
		TEXT("RTX A6"),
		TEXT("Quadro")
	};

	// This must be called on game thread 
	const FGPUDriverInfo GPUDriverInfo = FPlatformMisc::GetGPUDriverInfo(GRHIAdapterName);
	bIsGPUTextureTransferAvailable = GPUDriverInfo.IsNVIDIA() && !FModuleManager::Get().IsModuleLoaded("RenderDocPlugin") && !GPUDriverInfo.DeviceDescription.Contains(TEXT("Tesla"));

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
	if (!bIsGPUTextureTransferAvailable)
	{
		return;
	}

	ENQUEUE_RENDER_COMMAND(InitializeGPUTextureTransfer)(
	[this](FRHICommandListImmediate& RHICmdList) mutable
	{
		if (!GDynamicRHI)
		{
			return;
		}

		UE::GPUTextureTransfer::TextureTransferPtr TextureTransfer;

		UE::GPUTextureTransfer::ERHI RHI = ConvertRHI(RHIGetInterfaceType());

		switch (RHI)
		{
		case UE::GPUTextureTransfer::ERHI::D3D11:
			TextureTransfer = MakeShared<UE::GPUTextureTransfer::Private::FD3D11TextureTransfer>();
			break;
		case UE::GPUTextureTransfer::ERHI::D3D12:
			TextureTransfer = MakeShared<UE::GPUTextureTransfer::Private::FD3D12TextureTransfer>();
			break;
		case UE::GPUTextureTransfer::ERHI::Vulkan:
			TextureTransfer = MakeShared<UE::GPUTextureTransfer::Private::FVulkanTextureTransfer>();
			break;
		default:
			ensureAlways(false);
			break;
		}

		UE::GPUTextureTransfer::FInitializeDMAArgs InitializeArgs;
		InitializeArgs.RHI = RHI;
		InitializeArgs.RHIDevice = GDynamicRHI->RHIGetNativeDevice();
		InitializeArgs.RHICommandQueue = GDynamicRHI->RHIGetNativeGraphicsQueue();
#if VULKAN_PLATFORM
		if (RHI == UE::GPUTextureTransfer::ERHI::Vulkan)
		{
			IVulkanDynamicRHI* DynRHI = GetIVulkanDynamicRHI();
			InitializeArgs.VulkanInstance = DynRHI->RHIGetVkInstance();
			FMemory::Memcpy(InitializeArgs.RHIDeviceUUID, DynRHI->RHIGetVulkanDeviceUUID(), 16);
		}
#endif

		const uint8 RHIIndex = static_cast<uint8>(RHI);
		UE_LOG(LogGPUTextureTransfer, Display, TEXT("Initializing GPU Texture transfer"));
		if (TextureTransfer->Initialize(InitializeArgs))
		{
			TransferObjects[RHIIndex] = TextureTransfer;
		}
	});
#endif // DVP_SUPPORTED_PLATFORM
}

void FGPUTextureTransferModule::UninitializeTextureTransfer()
{
#if DVP_SUPPORTED_PLATFORM
	ENQUEUE_RENDER_COMMAND(UninitializeGPUTextureTransfer)(
		[this](FRHICommandListImmediate& RHICmdList) mutable
		{
			for (uint8 RhiIt = 1; RhiIt < RHI_MAX; RhiIt++)
			{
				if (const UE::GPUTextureTransfer::TextureTransferPtr& TextureTransfer = TransferObjects[RhiIt])
				{
					TextureTransfer->Uninitialize();
				}
			}
		});
#endif
}

IMPLEMENT_MODULE(FGPUTextureTransferModule, GPUTextureTransfer);
