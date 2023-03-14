// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D11RHIPrivate.h"
#include "RenderCore.h"

#include "Microsoft/AllowMicrosoftPlatformTypes.h"
	#include <DirectXMath.h>
#include "Microsoft/HideMicrosoftPlatformTypes.h"

#pragma warning(disable : 4946)	// reinterpret_cast used between related classes: 'Platform::Object' and ...

using namespace Windows::Foundation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::UI::Core;

uint32 FD3D11Viewport::GSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

DXGI_FORMAT GetSupportedSwapChainBufferFormat(DXGI_FORMAT InPreferredDXGIFormat)
{
	switch (InPreferredDXGIFormat)
	{
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
		return InPreferredDXGIFormat;

	// More sophisticated fallbacks here?

	default:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	}
}

// FD3D11Viewport::GetBackBufferFormat()
//{
//	return DXGI_FORMAT_B8G8R8A8_UNORM;
//}

FD3D11Viewport::FD3D11Viewport(FD3D11DynamicRHI* InD3DRHI,HWND InWindowHandle,uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen, EPixelFormat InPreferredPixelFormat):
	D3DRHI(InD3DRHI),
	LastFlipTime(0),
	LastFrameComplete(0),
	LastCompleteTime(0),
	SyncCounter(0),
	bSyncedLastFrame(false),
	WindowHandle(InWindowHandle),
	SizeX(InSizeX),
	SizeY(InSizeY),
	PresentFailCount(0),
	ValidState(0),
	PixelFormat(InPreferredPixelFormat),
	bIsFullscreen(bInIsFullscreen),
	bAllowTearing(false),
	FrameSyncEvent(InD3DRHI)
{
	D3DRHI->Viewports.Add(this);

	// Ensure that the D3D device has been created.
	D3DRHI->InitD3DDevice();

	// Create a backbuffer/swapchain for each viewport
	TRefCountPtr<IDXGIDevice> DXGIDevice;
	VERIFYD3D11RESULT_EX(D3DRHI->GetDevice()->QueryInterface(IID_IDXGIDevice, (void**)DXGIDevice.GetInitReference()), D3DRHI->GetDevice());

	uint32 DisplayIndex = D3DRHI->GetHDRDetectedDisplayIndex();
	bForcedFullscreenDisplay = FParse::Value(FCommandLine::Get(), TEXT("FullscreenDisplay="), DisplayIndex);

	if (bForcedFullscreenDisplay || GRHISupportsHDROutput)
	{
		TRefCountPtr<IDXGIAdapter> DXGIAdapter;
		DXGIDevice->GetAdapter((IDXGIAdapter**)DXGIAdapter.GetInitReference());

		if (S_OK != DXGIAdapter->EnumOutputs(DisplayIndex, ForcedFullscreenOutput.GetInitReference()))
		{
			UE_LOG(LogD3D11RHI, Log, TEXT("Failed to find requested output display (%i)."), DisplayIndex);
			ForcedFullscreenOutput = nullptr;
			bForcedFullscreenDisplay = false;
		}
	}
	else
	{
		ForcedFullscreenOutput = nullptr;
	}

	if (PixelFormat == PF_FloatRGBA && bIsFullscreen)
	{
		// Send HDR meta data to enable
		D3DRHI->EnableHDR();
	}

	// Skip swap chain creation in off-screen rendering mode
	bNeedSwapChain = !FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen"));
	if (bNeedSwapChain)
	{
		// Create the swapchain.
		if (InD3DRHI->IsQuadBufferStereoEnabled())
		{
			IDXGIFactory2* Factory2 = (IDXGIFactory2*)D3DRHI->GetFactory();

			BOOL stereoEnabled = Factory2->IsWindowedStereoEnabled();
			if (stereoEnabled)
			{
				DXGI_SWAP_CHAIN_DESC1 SwapChainDesc1;
				FMemory::Memzero(&SwapChainDesc1, sizeof(DXGI_SWAP_CHAIN_DESC1));

				// Enable stereo 
				SwapChainDesc1.Stereo = true;
				// MSAA Sample count
				SwapChainDesc1.SampleDesc.Count = 1;
				SwapChainDesc1.SampleDesc.Quality = 0;

				SwapChainDesc1.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
				SwapChainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
				// Double buffering required to create stereo swap chain
				SwapChainDesc1.BufferCount = 2;
				SwapChainDesc1.Scaling = DXGI_SCALING_NONE;
				SwapChainDesc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
				SwapChainDesc1.Flags = GSwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

				IDXGISwapChain1* SwapChain1 = nullptr;
				VERIFYD3D11RESULT_EX((Factory2->CreateSwapChainForCoreWindow(
					D3DRHI->GetDevice(), 
					reinterpret_cast<IUnknown*>(CoreWindow::GetForCurrentThread()),
					&SwapChainDesc1, 
					nullptr, 
					&SwapChain1)
					), D3DRHI->GetDevice());
				SwapChain = SwapChain1;
				GRHISupportsHDROutput =
					(SwapChainDesc1.Format == DXGI_FORMAT_R10G10B10A2_TYPELESS) ||
					(SwapChainDesc1.Format == DXGI_FORMAT_R16G16B16A16_FLOAT);
			}
			else
			{
				UE_LOG(LogD3D11RHI, Log, TEXT("FD3D11Viewport::FD3D11Viewport was not able to create stereo SwapChain; Please enable stereo in driver settings."));
				InD3DRHI->DisableQuadBufferStereo();
			}
		}

		// if stereo was not activated or not enabled in settings
		if (SwapChain == nullptr)
		{
			IDXGIAdapter* pdxgiAdapter;
			VERIFYD3D11RESULT(DXGIDevice->GetAdapter(&pdxgiAdapter));

			IDXGIFactory2* pdxgiFactory;
			VERIFYD3D11RESULT(pdxgiAdapter->GetParent(__uuidof(pdxgiFactory), reinterpret_cast<void**>(&pdxgiFactory)));

			// Create the swapchain.
			DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };
			swapChainDesc.Width = SizeX;
			swapChainDesc.Height = SizeY;
			swapChainDesc.Format = GetSupportedSwapChainBufferFormat(GetRenderTargetFormat(PixelFormat));
			swapChainDesc.Stereo = false;
			swapChainDesc.SampleDesc.Count = 1;                          // don't use multi-sampling
			swapChainDesc.SampleDesc.Quality = 0;
			swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
			swapChainDesc.BufferCount = 2;                               // use two buffers to enable flip effect
			swapChainDesc.Scaling = DXGI_SCALING_NONE;
			swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; // we recommend using this swap effect for all applications
			swapChainDesc.Flags = GSwapChainFlags = 0;

			IDXGISwapChain1* CreatedSwapChain = NULL;

			VERIFYD3D11RESULT(pdxgiFactory->CreateSwapChainForCoreWindow(
				D3DRHI->GetDevice(),
				reinterpret_cast<IUnknown*>(CoreWindow::GetForCurrentThread()),
				&swapChainDesc,
				NULL,
				(IDXGISwapChain1**)&CreatedSwapChain
			)
			);

			*(SwapChain.GetInitReference()) = CreatedSwapChain;

			GRHISupportsHDROutput =
				(swapChainDesc.Format == DXGI_FORMAT_R10G10B10A2_TYPELESS) ||
				(swapChainDesc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT);

		}
	}

	// Create a RHI surface to represent the viewport's back buffer.
	BackBuffer = GetSwapChainSurface(D3DRHI, PixelFormat, SizeX, SizeY, SwapChain);

	ENQUEUE_RENDER_COMMAND(FD3D11Viewport)(
	[this](FRHICommandListImmediate& RHICmdList)
	{
		// Initialize the query by issuing an initial event.
		FrameSyncEvent.IssueEvent();
	});
}


// When a window has moved or resized we need to check whether it is on a HDR monitor or not. Set the correct color space of the monitor
void FD3D11Viewport::CheckHDRMonitorStatus()
{
	PixelColorSpace = EColorSpaceAndEOTF::ERec709_sRGB;
}

void FD3D11Viewport::ConditionalResetSwapChain(bool bIgnoreFocus)
{
	uint32 Valid = ValidState;
	if (0 != (Valid & VIEWPORT_INVALID))
	{
		if (0 != (Valid & VIEWPORT_FULLSCREEN_LOST))
		{
			FlushRenderingCommands();
			ValidState &= ~(VIEWPORT_FULLSCREEN_LOST);
			Resize(SizeX, SizeY, false, PixelFormat);
		}
		else
		{
			ResetSwapChainInternal(bIgnoreFocus);
		}
	}
}

void FD3D11Viewport::ResetSwapChainInternal(bool bIgnoreFocus)
{
	if (0 != (ValidState & VIEWPORT_INVALID))
	{
		const bool bIsFocused = true;
		const bool bIsIconic = false;
		if(bIgnoreFocus || (bIsFocused && !bIsIconic) )
		{
			FlushRenderingCommands();

			HRESULT Result = SwapChain->SetFullscreenState(bIsFullscreen,NULL);
			if(SUCCEEDED(Result))
			{
				ValidState &= ~(VIEWPORT_INVALID);
			}
			else
			{
				// Even though the docs say SetFullscreenState always returns S_OK, that doesn't always seem to be the case.
				UE_LOG(LogD3D11RHI, Log, TEXT("IDXGISwapChain::SetFullscreenState returned %08x; waiting for the next frame to try again."),Result);
			}
		}
	}
}

void FD3D11DynamicRHI::RHIGetDisplaysInformation(FDisplayInformationArray& OutDisplayInformation)
{
}
