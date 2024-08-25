// Copyright Epic Games, Inc. All Rights Reserved.

#include "Render/Synchronization/DisplayClusterRenderSyncHelper.h"

#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterStrings.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "RHI.h"
#include "DynamicRHI.h"
#include "UnrealClient.h"

#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include "dxgi1_3.h"
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"



TUniquePtr<IDisplayClusterRenderSyncHelper> FDisplayClusterRenderSyncHelper::CreateHelper()
{
	if (GDynamicRHI)
	{
		const ERHIInterfaceType RHIType = RHIGetInterfaceType();

		// Instantiate DX helper
		if (RHIType == ERHIInterfaceType::D3D11 || RHIType == ERHIInterfaceType::D3D12)
		{
			return MakeUnique<FDisplayClusterRenderSyncHelper::FDCRSHelperDX>();
		}
		// Instantiate Vulkan helper
		else if (RHIType == ERHIInterfaceType::Vulkan)
		{
			return MakeUnique<FDisplayClusterRenderSyncHelper::FDCRSHelperVulkan>();
		}
	}

	// Null-helper stub as fallback
	return MakeUnique<FDisplayClusterRenderSyncHelper::FDCRSHelperNull>();
}


bool FDisplayClusterRenderSyncHelper::FDCRSHelperDX::IsWaitForVBlankSupported()
{
	return true;
}

bool FDisplayClusterRenderSyncHelper::FDCRSHelperDX::WaitForVBlank()
{
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		if (FRHIViewport* RHIViewport = GEngine->GameViewport->Viewport->GetViewportRHI().GetReference())
		{
			if (IDXGISwapChain* DXGISwapChain = static_cast<IDXGISwapChain*>(RHIViewport->GetNativeSwapChain()))
			{
				IDXGIOutput* DXOutput = nullptr;
				DXGISwapChain->GetContainingOutput(&DXOutput);

				if (DXOutput)
				{
					DXOutput->WaitForVBlank();
					DXOutput->Release();

					// Return true to notify about successful v-blank awaiting
					return true;
				}
			}
		}
	}

	// Something went wrong. We have to let the caller know that
	// we are not at the v-blank now.
	return false;
}

bool FDisplayClusterRenderSyncHelper::FDCRSHelperDX::GetMaximumFrameLatency(uint32& OutMaximumFrameLatency)
{
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		if (FRHIViewport* RHIViewport = GEngine->GameViewport->Viewport->GetViewportRHI().GetReference())
		{
			if (IDXGISwapChain2* DXGISwapChain = static_cast<IDXGISwapChain2*>(RHIViewport->GetNativeSwapChain()))
			{
				const HRESULT Result = DXGISwapChain->GetMaximumFrameLatency(&OutMaximumFrameLatency);

				if (Result == S_OK)
				{
					UE_LOG(LogDisplayClusterRenderSync, Verbose, TEXT("Swapchain frame latency: %u"), OutMaximumFrameLatency);
				}
				else
				{
					UE_LOG(LogDisplayClusterRenderSync, Warning, TEXT("Couldn't get maximum frame latency. Error: %x"), Result);
				}

				return Result == S_OK;
			}
		}
	}

	return false;
}

bool FDisplayClusterRenderSyncHelper::FDCRSHelperDX::SetMaximumFrameLatency(uint32 MaximumFrameLatency)
{
	if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
	{
		if (FRHIViewport* RHIViewport = GEngine->GameViewport->Viewport->GetViewportRHI().GetReference())
		{
			if (IDXGISwapChain2* DXGISwapChain = static_cast<IDXGISwapChain2*>(RHIViewport->GetNativeSwapChain()))
			{
				const HRESULT Result = DXGISwapChain->SetMaximumFrameLatency(MaximumFrameLatency);

				if (Result == S_OK)
				{
					UE_LOG(LogDisplayClusterRenderSync, Verbose, TEXT("Swapchain frame latency was set to: %u"), MaximumFrameLatency);
				}
				else
				{
					UE_LOG(LogDisplayClusterRenderSync, Warning, TEXT("Couldn't set maximum frame latency to %u. Error: %x"), MaximumFrameLatency, Result);
				}

				return Result == S_OK;
			}
		}
	}

	return false;
}

bool FDisplayClusterRenderSyncHelper::FDCRSHelperVulkan::IsWaitForVBlankSupported()
{
	return false;
}

bool FDisplayClusterRenderSyncHelper::FDCRSHelperVulkan::WaitForVBlank()
{
	return false;
}

bool FDisplayClusterRenderSyncHelper::FDCRSHelperVulkan::GetMaximumFrameLatency(uint32& OutMaximumFrameLatency)
{
	return false;
}

bool FDisplayClusterRenderSyncHelper::FDCRSHelperVulkan::SetMaximumFrameLatency(uint32 MaximumFrameLatency)
{
	return false;
}
