// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D12Viewport.cpp: D3D viewport RHI implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "RenderCore.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows.h"

#include "HAL/ThreadHeartBeat.h"

static const uint32 WindowsDefaultNumBackBuffers = 3;

extern FD3D12Texture* GetSwapChainSurface(FD3D12Device* Parent, EPixelFormat PixelFormat, uint32 SizeX, uint32 SizeY, IDXGISwapChain* SwapChain, uint32 BackBufferIndex, TRefCountPtr<ID3D12Resource> BackBufferResourceOverride);

static int32 GD3D12UseAllowTearing = 1;
static FAutoConsoleVariableRef CVarD3DUseAllowTearing(
	TEXT("r.D3D12.UseAllowTearing"),
	GD3D12UseAllowTearing,
	TEXT("Enable new dxgi flip mode with d3d12"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly
);

FD3D12Viewport::FD3D12Viewport(class FD3D12Adapter* InParent, HWND InWindowHandle, uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen, EPixelFormat InPreferredPixelFormat)
	: FD3D12AdapterChild(InParent)
	, WindowHandle(InWindowHandle)
	, SizeX(InSizeX)
	, SizeY(InSizeY)
	, bIsFullscreen(bInIsFullscreen)
	, bFullscreenLost(false)
	, PixelFormat(InPreferredPixelFormat)
	, bIsValid(true)
	, bHDRMetaDataSet(false)
	, ColorSpace(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)
	, NumBackBuffers(WindowsDefaultNumBackBuffers)
	, DummyBackBuffer_RenderThread(nullptr)
	, CurrentBackBufferIndex_RHIThread(0)
	, BackBuffer_RHIThread(nullptr)
	, BackBuffer_RenderThread(nullptr)
	, BackBufferUAV_RenderThread(nullptr)
#if WITH_MGPU
	, BackbufferMultiGPUBinding(0)
#endif //WITH_MGPU
	, ExpectedBackBufferIndex_RenderThread(0)
	, SDRDummyBackBuffer_RenderThread(nullptr)
	, SDRBackBuffer_RHIThread(nullptr)
	, SDRPixelFormat(PF_B8G8R8A8)
	, DisplayColorGamut(EDisplayColorGamut::sRGB_D65)
	, DisplayOutputFormat(EDisplayOutputFormat::SDR_sRGB)
#if WITH_MGPU
	, FramePacerRunnable(nullptr)
#endif //WITH_MGPU
{
	check(IsInGameThread());
	GetParentAdapter()->GetViewports().Add(this);
}

//Init for a Viewport that will do the presenting
void FD3D12Viewport::Init()
{
	FD3D12Adapter* Adapter = GetParentAdapter();
	IDXGIFactory2* Factory2 = Adapter->GetDXGIFactory2();

	bAllowTearing = false;
	if (GD3D12UseAllowTearing)
	{
		if (IDXGIFactory5* Factory5 = Adapter->GetDXGIFactory5())
		{
			BOOL AllowTearing = FALSE;
			Factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &AllowTearing, sizeof(AllowTearing));
			bAllowTearing = (AllowTearing != FALSE);
		}
	}

	CalculateSwapChainDepth(WindowsDefaultNumBackBuffers);

	UINT SwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	if (bAllowTearing)
	{
		SwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	}

	const DXGI_MODE_DESC BufferDesc = SetupDXGI_MODE_DESC();

	// The command queue used here is irrelevant in regard to multi - GPU as it gets overriden in the Resize
	ID3D12CommandQueue* CommandQueue = Adapter->GetDevice(0)->GetQueue(ED3D12QueueType::Direct).D3DCommandQueue;

	// Create the swapchain.
#if PLATFORM_HOLOLENS
	{
		// MSAA Sample count
		DXGI_SWAP_CHAIN_DESC1 SwapChainDesc{};
		SwapChainDesc.SampleDesc.Count = 1;
		SwapChainDesc.SampleDesc.Quality = 0;
		SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
		SwapChainDesc.Width = BufferDesc.Width;
		SwapChainDesc.Height = BufferDesc.Height;
		SwapChainDesc.Format = BufferDesc.Format;
		// 1:single buffering, 2:double buffering, 3:triple buffering
		SwapChainDesc.BufferCount = NumBackBuffers;
		SwapChainDesc.Scaling = DXGI_SCALING_NONE;
		SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		// DXGI_SWAP_EFFECT_DISCARD / DXGI_SWAP_EFFECT_SEQUENTIAL
		SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		SwapChainDesc.Flags = SwapChainFlags;

		IDXGIFactory4* Factory4 = Adapter->GetDXGIFactory4();
		check(Factory4);

		VERIFYD3D12RESULT(Factory4->CreateSwapChainForCoreWindow(
			CommandQueue,
			reinterpret_cast<IUnknown*>(Windows::UI::Core::CoreWindow::GetForCurrentThread()),
			&SwapChainDesc,
			NULL,
			SwapChain1.GetInitReference()
		));
	}
#else

	extern bool bNeedSwapChain;

	bNeedSwapChain = !FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen"));
	if (bNeedSwapChain)
	{
		if (FD3D12DynamicRHI::GetD3DRHI()->IsQuadBufferStereoEnabled())
		{
			if (Factory2->IsWindowedStereoEnabled())
			{
				DXGI_SWAP_CHAIN_DESC1 SwapChainDesc1{};

				// Enable stereo 
				SwapChainDesc1.Stereo = true;
				// MSAA Sample count
				SwapChainDesc1.SampleDesc.Count = 1;
				SwapChainDesc1.SampleDesc.Quality = 0;

				SwapChainDesc1.Format = GetRenderTargetFormat(PixelFormat);
				SwapChainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
				// Double buffering required to create stereo swap chain
				SwapChainDesc1.BufferCount = NumBackBuffers;
				SwapChainDesc1.Scaling = DXGI_SCALING_NONE;
				SwapChainDesc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
				SwapChainDesc1.Flags = SwapChainFlags;
				SwapChainDesc1.Width = SizeX;
				SwapChainDesc1.Height = SizeY;

				VERIFYD3D12RESULT(Factory2->CreateSwapChainForHwnd(CommandQueue, WindowHandle, &SwapChainDesc1, nullptr, nullptr, SwapChain1.GetInitReference()));
			}
			else
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("FD3D12Viewport::FD3D12Viewport was not able to create stereo SwapChain; Please enable stereo in driver settings."));
				FD3D12DynamicRHI::GetD3DRHI()->DisableQuadBufferStereo();
			}
		}

		// if stereo was not activated or not enabled in settings
		if (SwapChain1 == nullptr)
		{
			// Create the swapchain.
			DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
			SwapChainDesc.BufferDesc = BufferDesc;
			// MSAA Sample count
			SwapChainDesc.SampleDesc.Count = 1;
			SwapChainDesc.SampleDesc.Quality = 0;
			SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_SHADER_INPUT;
			// 1:single buffering, 2:double buffering, 3:triple buffering
			SwapChainDesc.BufferCount = NumBackBuffers;
			SwapChainDesc.OutputWindow = WindowHandle;
			SwapChainDesc.Windowed = !bIsFullscreen;
			// DXGI_SWAP_EFFECT_DISCARD / DXGI_SWAP_EFFECT_SEQUENTIAL
			SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			SwapChainDesc.Flags = SwapChainFlags;

			TRefCountPtr<IDXGISwapChain> SwapChain;
			HRESULT hr = Factory2->CreateSwapChain(CommandQueue, &SwapChainDesc, SwapChain.GetInitReference());
			if (FAILED(hr))
			{
				UE_LOG(LogD3D12RHI, Warning, TEXT("Failed to create swapchain with the following parameters:"));
				UE_LOG(LogD3D12RHI, Warning, TEXT("\tDXGI_MODE_DESC: width: %d height: %d DXGI format: %d"), SwapChainDesc.BufferDesc.Width, SwapChainDesc.BufferDesc.Height, SwapChainDesc.BufferDesc.Format);
				UE_LOG(LogD3D12RHI, Warning, TEXT("\tBack buffer count: %d"), NumBackBuffers);
				UE_LOG(LogD3D12RHI, Warning, TEXT("\tWindows handle: 0x%x (IsWindow: %s)"), WindowHandle, IsWindow(WindowHandle) ? TEXT("true") : TEXT("false"));
				UE_LOG(LogD3D12RHI, Warning, TEXT("\tFullscreen: %s"), bIsFullscreen ? TEXT("true") : TEXT("false"));
				UE_LOG(LogD3D12RHI, Warning, TEXT("\tSwapchain flags: %d"), SwapChainFlags);

				VERIFYD3D12RESULT(hr);
			}

			VERIFYD3D12RESULT(SwapChain->QueryInterface(IID_PPV_ARGS(SwapChain1.GetInitReference())));
		}
	}
#endif

	if (SwapChain1)
	{
		SwapChain1->QueryInterface(IID_PPV_ARGS(SwapChain2.GetInitReference()));
#if DXGI_MAX_SWAPCHAIN_INTERFACE >= 3
		SwapChain1->QueryInterface(IID_PPV_ARGS(SwapChain3.GetInitReference()));
#endif
#if DXGI_MAX_SWAPCHAIN_INTERFACE >= 4
		SwapChain1->QueryInterface(IID_PPV_ARGS(SwapChain4.GetInitReference()));
#endif
	}

	{
		// Don't make the windows association call and release back buffer at the same time (see notes on critical section)
		FScopeLock Lock(&DXGIBackBufferLock);

		// Set the DXGI message hook to not change the window behind our back.
		Factory2->MakeWindowAssociation(WindowHandle, DXGI_MWA_NO_WINDOW_CHANGES);
	}

	// Resize to setup mGPU correctly.
	Resize(BufferDesc.Width, BufferDesc.Height, bIsFullscreen, PixelFormat);

#if !PLATFORM_HOLOLENS
	// Tell the window to redraw when they can.
	// @todo: For Slate viewports, it doesn't make sense to post WM_PAINT messages (we swallow those.)
	::PostMessageW(WindowHandle, WM_PAINT, 0, 0);
#endif
}

void FD3D12Viewport::FinalDestroyInternal()
{
}

void FD3D12Viewport::ClearPresentQueue()
{
}

void FD3D12Viewport::ConditionalResetSwapChain(bool bIgnoreFocus)
{
	if (!bIsValid)
	{
		if (bFullscreenLost)
		{
			FlushRenderingCommands();
			bFullscreenLost = false;
			Resize(SizeX, SizeY, false, PixelFormat);
		}
		else
		{
#if !PLATFORM_HOLOLENS
			// Check if the viewport's window is focused before resetting the swap chain's fullscreen state.
			HWND FocusWindow = ::GetFocus();
			const bool bIsFocused = FocusWindow == WindowHandle;
			const bool bIsIconic = !!::IsIconic(WindowHandle);
			if (bIgnoreFocus || (bIsFocused && !bIsIconic))
#endif
			{
				FlushRenderingCommands();

				HRESULT Result = SwapChain1->SetFullscreenState(bIsFullscreen, nullptr);
				if (SUCCEEDED(Result))
				{
					bIsValid = true;
				}
				else if (Result != DXGI_ERROR_NOT_CURRENTLY_AVAILABLE && Result != DXGI_STATUS_MODE_CHANGE_IN_PROGRESS)
				{
					const TCHAR* Name = nullptr;
					switch (Result)
					{
#define CASE_ERROR_NAME(x)	case x: Name = TEXT(#x); break
						EMBED_DXGI_ERROR_LIST(CASE_ERROR_NAME, ;)
#undef CASE_ERROR_NAME
					default:
						Name = TEXT("unknown error status");
						break;
					}
					UE_LOG(LogD3D12RHI, Error, TEXT("IDXGISwapChain::SetFullscreenState returned 0x%08x, %s."), Result, Name);

					if (bIsFullscreen)
					{
						// Something went wrong, attempt to proceed in windowed mode.
						Result = SwapChain1->SetFullscreenState(FALSE, nullptr);
						if (SUCCEEDED(Result))
						{
							bIsValid = true;
							bIsFullscreen = false;
						}
					}
				}
			}
		}
	}
}

void FD3D12Viewport::ResizeInternal()
{
	FD3D12Adapter* Adapter = GetParentAdapter();

	CalculateSwapChainDepth(WindowsDefaultNumBackBuffers);

	UINT SwapChainFlags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	if (bAllowTearing)
	{
		SwapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
	}

#if WITH_MGPU
	if (GNumExplicitGPUsForRendering > 1)
	{
		TArray<ID3D12CommandQueue*> CommandQueues;
		TArray<uint32> NodeMasks;
		BackBufferGPUIndices.Empty(NumBackBuffers);

		for (uint32 i = 0; i < NumBackBuffers; ++i)
		{
			// When BackbufferMultiGPUBinding == INDEX_NONE, cycle through each GPU (for AFR or debugging).
			const uint32 BackBufferGPUIndex = BackbufferMultiGPUBinding >= 0 ? (uint32)BackbufferMultiGPUBinding : (i % GNumAlternateFrameRenderingGroups);
			BackBufferGPUIndices.Add(BackBufferGPUIndex);
		}

		// Interleave the swapchains between the AFR devices
		for (uint32 i = 0; i < NumBackBuffers; ++i)
		{
			const uint32 GPUIndex = BackBufferGPUIndices[i];
			FD3D12Device* Device = Adapter->GetDevice(GPUIndex);

			CommandQueues.Add(Device->GetQueue(ED3D12QueueType::Direct).D3DCommandQueue);
			NodeMasks.Add(Device->GetGPUMask().GetNative());
		}

		if (SwapChain3)
		{
			VERIFYD3D12RESULT_EX(SwapChain3->ResizeBuffers1(NumBackBuffers, SizeX, SizeY, GetRenderTargetFormat(PixelFormat), SwapChainFlags, NodeMasks.GetData(), (IUnknown**)CommandQueues.GetData()), Adapter->GetD3DDevice());
		}

		for (uint32 i = 0; i < NumBackBuffers; ++i)
		{
			const uint32 GPUIndex = BackBufferGPUIndices[i];
			FD3D12Device* Device = Adapter->GetDevice(GPUIndex);

			check(BackBuffers[i].GetReference() == nullptr);
			BackBuffers[i] = GetSwapChainSurface(Device, PixelFormat, SizeX, SizeY, SwapChain1, i, nullptr);
		}
	}
	else
#endif // WITH_MGPU
	{
		if (SwapChain1)
		{
			auto Lambda = [=]() -> FString
			{
				return FString::Printf(TEXT("Num=%d, Size=(%d,%d), PF=%d, DXGIFormat=0x%x, Flags=0x%x"), NumBackBuffers, SizeX, SizeY, (int32)PixelFormat, (int32)GetRenderTargetFormat(PixelFormat), SwapChainFlags);
			};

			VERIFYD3D12RESULT_LAMBDA(SwapChain1->ResizeBuffers(NumBackBuffers, SizeX, SizeY, GetRenderTargetFormat(PixelFormat), SwapChainFlags), Adapter->GetD3DDevice(), Lambda);
		}

		FD3D12Device* Device = Adapter->GetDevice(0);
		for (uint32 i = 0; i < NumBackBuffers; ++i)
		{
			check(BackBuffers[i].GetReference() == nullptr);
			BackBuffers[i] = GetSwapChainSurface(Device, PixelFormat, SizeX, SizeY, SwapChain1, i, nullptr);
		}
	}

	CurrentBackBufferIndex_RHIThread = 0;
	BackBuffer_RHIThread = BackBuffers[CurrentBackBufferIndex_RHIThread].GetReference();
	SDRBackBuffer_RHIThread = SDRBackBuffers[CurrentBackBufferIndex_RHIThread].GetReference();

#if !D3D12_USE_DUMMY_BACKBUFFER
    static_assert(false, "D3D12_USE_DUMMY_BACKBUFFER==0 has not been tested for viewports");
	ExpectedBackBufferIndex_RenderThread = 0;
	BackBuffer_RenderThread = BackBuffers[ExpectedBackBufferIndex_RenderThread].GetReference();
#endif

	// Create dummy back buffer which always reference to the actual RHI thread back buffer - can't be bound directly to D3D12
	DummyBackBuffer_RenderThread = CreateDummyBackBufferTextures(Adapter, PixelFormat, SizeX, SizeY, false);
	SDRDummyBackBuffer_RenderThread = (SDRBackBuffer_RHIThread != nullptr) ? CreateDummyBackBufferTextures(Adapter, PixelFormat, SizeX, SizeY, true) : nullptr;
}

HRESULT FD3D12Viewport::PresentInternal(int32 SyncInterval)
{
	UINT Flags = 0;

	if(!SyncInterval && !bIsFullscreen && bAllowTearing)
	{
		Flags |= DXGI_PRESENT_ALLOW_TEARING;
	}

	FThreadHeartBeat::Get().PresentFrame();

	if (SwapChain1)
	{
		return SwapChain1->Present(SyncInterval, Flags);
	}

	return S_OK;
}

void FD3D12Viewport::EnableHDR()
{
	if ( GRHISupportsHDROutput && IsHDREnabled() )
	{
		const float DisplayMaxOutputNits = HDRGetDisplayMaximumLuminance();
		const float DisplayMinOutputNits = 0.0f;	// Min output of the display
		const float DisplayMaxCLL = 0.0f;			// Max content light level in lumens (0.0 == unknown)
		const float DisplayFALL = 0.0f;				// Frame average light level (0.0 == unknown)

		// Ideally we can avoid setting TV meta data and instead the engine can do tone mapping based on the
		// actual current display properties (display mapping).
		SetHDRTVMode(true,
			DisplayColorGamut,
			DisplayMaxOutputNits,
			DisplayMinOutputNits,
			DisplayMaxCLL,
			DisplayFALL);

		// Ensure we have the correct color space set.
		EnsureColorSpace(DisplayColorGamut, DisplayOutputFormat);
	}
}

void FD3D12Viewport::ShutdownHDR()
{
	if (GRHISupportsHDROutput)
	{
		// Default SDR display data
		const EDisplayColorGamut DisplayGamut = EDisplayColorGamut::sRGB_D65;
		const EDisplayOutputFormat OutputDevice = EDisplayOutputFormat::SDR_sRGB;

		// Note: These values aren't actually used.
		const float DisplayMaxOutputNits = 100.0f;	// Max output of the display
		const float DisplayMinOutputNits = 0.0f;	// Min output of the display
		const float DisplayMaxCLL = 100.0f;			// Max content light level in lumens
		const float DisplayFALL = 20.0f;			// Frame average light level

		// Ideally we can avoid setting TV meta data and instead the engine can do tone mapping based on the
		// actual current display properties (display mapping).
		SetHDRTVMode(
			false,
			DisplayGamut,
			DisplayMaxOutputNits,
			DisplayMinOutputNits,
			DisplayMaxCLL,
			DisplayFALL);

		// Ensure we have the correct color space set.
		EnsureColorSpace(DisplayGamut, OutputDevice);
	}
}

bool FD3D12Viewport::CurrentOutputSupportsHDR() const
{
	// Default to no support.
	bool bOutputSupportsHDR = false;

	if (SwapChain4.GetReference())
	{
		// Output information is cached on the DXGI Factory. If it is stale we need to create
		// a new factory which will re-enumerate the displays.
		FD3D12Adapter* Adapter = GetParentAdapter();
		IDXGIFactory2* DxgiFactory2 = Adapter->GetDXGIFactory2();
		if (DxgiFactory2)
		{
			if (!DxgiFactory2->IsCurrent())
			{
				Adapter->CreateDXGIFactory(false);
			}

			check(Adapter->GetDXGIFactory2()->IsCurrent());

			// Get information about the display we are presenting to.
			TRefCountPtr<IDXGIOutput> Output;
			VERIFYD3D12RESULT(SwapChain4->GetContainingOutput(Output.GetInitReference()));

			TRefCountPtr<IDXGIOutput6> Output6;
			if (SUCCEEDED(Output->QueryInterface(IID_PPV_ARGS(Output6.GetInitReference()))))
			{
				DXGI_OUTPUT_DESC1 OutputDesc;
				VERIFYD3D12RESULT(Output6->GetDesc1(&OutputDesc));

				// Check for HDR support on the display.
				bOutputSupportsHDR = (OutputDesc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
			}
		}
	}

	return bOutputSupportsHDR;
}

static const FString GetDXGIColorSpaceString(DXGI_COLOR_SPACE_TYPE ColorSpace)
{
	switch (ColorSpace)
	{
	case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709:
		return TEXT("RGB_FULL_G22_NONE_P709");
	case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709:
		return TEXT("RGB_FULL_G10_NONE_P709");
	case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020:
		return TEXT("RGB_FULL_G2084_NONE_P2020");
	case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020:
		return TEXT("RGB_FULL_G22_NONE_P2020");
	default:
		break;
	}

	return FString::FromInt(ColorSpace);
};

void FD3D12Viewport::EnsureColorSpace(EDisplayColorGamut DisplayGamut, EDisplayOutputFormat OutputDevice)
{
	ensure(SwapChain4.GetReference());

	DXGI_COLOR_SPACE_TYPE NewColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;	// sRGB;
	const bool bPrimaries2020 = (DisplayGamut == EDisplayColorGamut::Rec2020_D65);

	// See console variable r.HDR.Display.OutputDevice.
	switch (OutputDevice)
	{
		// Gamma 2.2
	case EDisplayOutputFormat::SDR_sRGB:
	case EDisplayOutputFormat::SDR_Rec709:
		NewColorSpace = bPrimaries2020 ? DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
		break;

		// Gamma ST.2084
	case EDisplayOutputFormat::HDR_ACES_1000nit_ST2084:
	case EDisplayOutputFormat::HDR_ACES_2000nit_ST2084:
		NewColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
		break;

		// Gamma 1.0 (Linear)
	case EDisplayOutputFormat::HDR_ACES_1000nit_ScRGB:
	case EDisplayOutputFormat::HDR_ACES_2000nit_ScRGB:
		// Linear. Still supports expanded color space with values >1.0f and <0.0f.
		// The actual range is determined by the pixel format (e.g. a UNORM format can only ever have 0-1).
		NewColorSpace = DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709;
		break;
	}

	if (ColorSpace != NewColorSpace)
	{
		uint32 ColorSpaceSupport = 0;
		HRESULT hr = SwapChain4->CheckColorSpaceSupport(NewColorSpace, &ColorSpaceSupport);
		FString NewColorSpaceName = GetDXGIColorSpaceString(NewColorSpace);
		if (SUCCEEDED(hr) && ((ColorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) != 0))
		{
			VERIFYD3D12RESULT(SwapChain4->SetColorSpace1(NewColorSpace));
			UE_LOG(LogD3D12RHI, Verbose, TEXT("Setting color space on swap chain (%#016llx): %s"), SwapChain4.GetReference(), *NewColorSpaceName);
			ColorSpace = NewColorSpace;
		}
		else
		{
			UE_LOG(LogD3D12RHI, Error, TEXT("Warning: unabled to set color space %s to the swapchain: verify EDisplayOutputFormat / swapchain format"), *NewColorSpaceName);
		}
	}
}

void FD3D12Viewport::SetHDRTVMode(bool bEnableHDR, EDisplayColorGamut DisplayGamut, float MaxOutputNits, float MinOutputNits, float MaxCLL, float MaxFALL)
{
	ensure(SwapChain4.GetReference());

	static const DisplayChromacities DisplayChromacityList[] =
	{
		{ 0.64000f, 0.33000f, 0.30000f, 0.60000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, // EDisplayColorGamut::sRGB_D65
		{ 0.68000f, 0.32000f, 0.26500f, 0.69000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, // EDisplayColorGamut::DCIP3_D65
		{ 0.70800f, 0.29200f, 0.17000f, 0.79700f, 0.13100f, 0.04600f, 0.31270f, 0.32900f }, // EDisplayColorGamut::Rec2020_D65
		{ 0.73470f, 0.26530f, 0.00000f, 1.00000f, 0.00010f,-0.07700f, 0.32168f, 0.33767f }, // EDisplayColorGamut::ACES_D60
		{ 0.71300f, 0.29300f, 0.16500f, 0.83000f, 0.12800f, 0.04400f, 0.32168f, 0.33767f }, // EDisplayColorGamut::ACEScg_D60
	};

	if (bEnableHDR)
	{
		const DisplayChromacities& Chroma = DisplayChromacityList[(int32)DisplayGamut];

		// Set HDR meta data
		DXGI_HDR_METADATA_HDR10 HDR10MetaData = {};
		HDR10MetaData.RedPrimary[0] = static_cast<uint16>(Chroma.RedX * 50000.0f);
		HDR10MetaData.RedPrimary[1] = static_cast<uint16>(Chroma.RedY * 50000.0f);
		HDR10MetaData.GreenPrimary[0] = static_cast<uint16>(Chroma.GreenX * 50000.0f);
		HDR10MetaData.GreenPrimary[1] = static_cast<uint16>(Chroma.GreenY * 50000.0f);
		HDR10MetaData.BluePrimary[0] = static_cast<uint16>(Chroma.BlueX * 50000.0f);
		HDR10MetaData.BluePrimary[1] = static_cast<uint16>(Chroma.BlueY * 50000.0f);
		HDR10MetaData.WhitePoint[0] = static_cast<uint16>(Chroma.WpX * 50000.0f);
		HDR10MetaData.WhitePoint[1] = static_cast<uint16>(Chroma.WpY * 50000.0f);
		HDR10MetaData.MaxMasteringLuminance = static_cast<uint32>(MaxOutputNits * 10000.0f);
		HDR10MetaData.MinMasteringLuminance = static_cast<uint32>(MinOutputNits * 10000.0f);
		HDR10MetaData.MaxContentLightLevel = static_cast<uint16>(MaxCLL);
		HDR10MetaData.MaxFrameAverageLightLevel = static_cast<uint16>(MaxFALL);

		VERIFYD3D12RESULT(SwapChain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_HDR10, sizeof(HDR10MetaData), &HDR10MetaData));
		UE_LOG(LogD3D12RHI, Log, TEXT("Setting HDR meta data on swap chain (%#016llx) using DisplayGamut %u:"), SwapChain4.GetReference(), static_cast<uint32>(DisplayGamut));
		UE_LOG(LogD3D12RHI, Log, TEXT("\t\tMaxMasteringLuminance = %.4f nits"), HDR10MetaData.MaxMasteringLuminance * .0001f);
		UE_LOG(LogD3D12RHI, Log, TEXT("\t\tMinMasteringLuminance = %.4f nits"), HDR10MetaData.MinMasteringLuminance * .0001f);
		UE_LOG(LogD3D12RHI, Log, TEXT("\t\tMaxContentLightLevel = %u nits"), HDR10MetaData.MaxContentLightLevel);
		UE_LOG(LogD3D12RHI, Log, TEXT("\t\tMaxFrameAverageLightLevel %u = nits"), HDR10MetaData.MaxFrameAverageLightLevel);
		bHDRMetaDataSet = true;
	}
	else
	{
		if (bHDRMetaDataSet)
		{
			// Clear meta data.
			VERIFYD3D12RESULT(SwapChain4->SetHDRMetaData(DXGI_HDR_METADATA_TYPE_NONE, 0, nullptr));
			UE_LOG(LogD3D12RHI, Log, TEXT("Clearing HDR meta data on swap chain (%#016llx)."), SwapChain4.GetReference());
			bHDRMetaDataSet = false;
		}
	}
}


void FD3D12Viewport::OnResumeRendering()
{}


void FD3D12Viewport::OnSuspendRendering()
{}

FD3D12Texture* FD3D12Viewport::GetBackBuffer_RenderThread() const
{
	check(IsInRenderingThread());
	const bool sIsSDR = false;
	FD3D12Texture* const DummyBackBuffer = GetDummyBackBuffer_RenderThread(sIsSDR);
	return DummyBackBuffer;
}

bool FD3D12Viewport::IsPresentAllowed()
{
	return !FD3D12DynamicRHI::GetD3DRHI()->RHIIsRenderingSuspended();
}

void FD3D12DynamicRHI::RHIGetDisplaysInformation(FDisplayInformationArray& OutDisplayInformation)
{
	OutDisplayInformation.Append(DisplayList);
}

#include "Windows/HideWindowsPlatformTypes.h"
