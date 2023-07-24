// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WindowsD3D11Device.cpp: Windows D3D device RHI implementation.
=============================================================================*/
#include "Misc/EngineVersion.h"
#include "D3D11RHIPrivate.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsPlatformCrashContext.h"
#include <delayimp.h>
	#if WITH_NVAPI
		#include "nvapi.h"
		#include "nvShaderExtnEnums.h"
	#endif
	#if WITH_AMD_AGS
		#include "amd_ags.h"
	#endif
#include "Windows/HideWindowsPlatformTypes.h"

#include "HardwareInfo.h"
#include "IHeadMountedDisplayModule.h"
#include "GenericPlatform/GenericPlatformDriver.h"			// FGPUDriverInfo
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "RHIValidation.h"
#include "HAL/ExceptionHandling.h"
#include "HDRHelper.h"

#if NV_AFTERMATH
bool GDX11NVAfterMathEnabled = false;
bool GNVAftermathModuleLoaded = false;
bool GDX11NVAfterMathMarkers = false;
#endif

#if INTEL_METRICSDISCOVERY
bool GDX11IntelMetricsDiscoveryEnabled = false;
#endif

FD3D11DynamicRHI*	GD3D11RHI = nullptr;

extern bool D3D11RHI_ShouldCreateWithD3DDebug();
extern bool D3D11RHI_ShouldAllowAsyncResourceCreation();

static int D3D11RHI_PreferAdapterVendor()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("preferAMD")))
	{
		return 0x1002;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("preferIntel")))
	{
		return 0x8086;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("preferNvidia")))
	{
		return 0x10DE;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("preferMS")))
	{
		return 0x1414;
	}

	return -1;
}

static bool D3D11RHI_AllowSoftwareFallback()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("AllowSoftwareRendering")))
	{
		return true;
	}

	return false;
}

#ifdef AMD_AGS_API
// Filled in during InitD3DDevice if IsRHIDeviceAMD
struct AmdAgsInfo
{
	AGSContext* AmdAgsContext;
	AGSGPUInfo AmdGpuInfo;
};
static AmdAgsInfo AmdInfo;
#endif

static TAutoConsoleVariable<int32> CVarAMDUseMultiThreadedDevice(
	TEXT("r.AMDD3D11MultiThreadedDevice"),
	0,
	TEXT("If true, creates a multithreaded D3D11 device on AMD hardware (workaround for driver bug)\n")
	TEXT("Changes will only take effect in new game/editor instances - can't be changed at runtime.\n"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAMDDisableAsyncTextureCreation(
	TEXT("r.AMDDisableAsyncTextureCreation"),
	0,
	TEXT("If true, uses synchronous texture creation on AMD hardware (workaround for driver bug)\n")
	TEXT("Changes will only take effect in new game/editor instances - can't be changed at runtime.\n"),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarNVidiaTimestampWorkaround(
	TEXT("r.NVIDIATimestampWorkaround"),
	1,
	TEXT("If true we disable timestamps on pre-maxwell hardware (workaround for driver bug)\n"),
	ECVF_Default);

/**
 * Console variables used by the D3D11 RHI device.
 */
namespace RHIConsoleVariables
{
	int32 MaxFeatureSetLimit = -1;
	static FAutoConsoleVariableRef CVarMaxFeatureSetLimit(
		TEXT("RHI.FeatureSetLimit"),
		MaxFeatureSetLimit,
		TEXT("If set to 10, limit D3D RHI to D3D10 feature level. Otherwise, it will use default. Changing this at run-time has no effect. (default is -1)")
		);
};

static void FD3D11DumpLiveObjects()
{
	if (D3D11RHI_ShouldCreateWithD3DDebug())
	{
		TRefCountPtr<ID3D11Debug> DebugDevice = nullptr;
		VERIFYD3D11RESULT_EX(GD3D11RHI->GetDevice()->QueryInterface(__uuidof(ID3D11Debug), (void**)DebugDevice.GetInitReference()), GD3D11RHI->GetDevice());
		if (DebugDevice)
		{
			HRESULT HR = DebugDevice->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY|D3D11_RLDO_DETAIL);
			if (HR != S_OK)
			{
				UE_LOG(LogD3D11RHI, Warning, TEXT("ReportLiveDeviceObjects failed with 0x%x"), HR);
			}
		}		
	}
	else
	{
		UE_LOG(LogD3D11RHI, Warning, TEXT("Must run with -d3ddebug to report live objects"));
	}
}

FAutoConsoleCommand FD3DDumpLiveObjectsCommand
(
	TEXT("r.d3d11.dumpliveobjects"),
	TEXT("When using -d3ddebug will dump a list of live d3d objects.  Mostly for finding leaks."),
	FConsoleCommandDelegate::CreateStatic(&FD3D11DumpLiveObjects)
);

/** This function is used as a SEH filter to catch only delay load exceptions. */
static bool IsDelayLoadException(PEXCEPTION_POINTERS ExceptionPointers)
{
	switch(ExceptionPointers->ExceptionRecord->ExceptionCode)
	{
	case VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND):
	case VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND):
		return EXCEPTION_EXECUTE_HANDLER;
	default:
		return EXCEPTION_CONTINUE_SEARCH;
	}
}

static bool bIsQuadBufferStereoEnabled = false;
typedef HRESULT(WINAPI *FCreateDXGIFactory2)(UINT, REFIID, void **);
static FCreateDXGIFactory2 CreateDXGIFactory2FnPtr = nullptr;

/**
 * Since CreateDXGIFactory1 is a delay loaded import from the D3D11 DLL, if the user
 * doesn't have VistaSP2/DX10, calling CreateDXGIFactory1 will throw an exception.
 * We use SEH to detect that case and fail gracefully.
 */
static void SafeCreateDXGIFactory(IDXGIFactory1** DXGIFactory1, bool bWithDebug)
{
#if !defined(D3D11_CUSTOM_VIEWPORT_CONSTRUCTOR) || !D3D11_CUSTOM_VIEWPORT_CONSTRUCTOR
	__try
	{
		bool bQuadBufferStereoRequested = FParse::Param(FCommandLine::Get(), TEXT("quad_buffer_stereo"));

#if PLATFORM_HOLOLENS
		bool bIsWin8OrNewer = true;
#else
		bool bIsWin8OrNewer = FPlatformMisc::VerifyWindowsVersion(8, 0);
#endif
		if (bIsWin8OrNewer && (bQuadBufferStereoRequested || bWithDebug))
		{
			// CreateDXGIFactory2 is only available on Win8.1+, find it if it exists
			HMODULE DxgiDLL = (HMODULE)FPlatformProcess::GetDllHandle(TEXT("dxgi.dll"));
			if (DxgiDLL)
			{
#pragma warning(push)
#pragma warning(disable: 4191) // disable the "unsafe conversion from 'FARPROC' to 'blah'" warning
				CreateDXGIFactory2FnPtr = (FCreateDXGIFactory2)(GetProcAddress(DxgiDLL, "CreateDXGIFactory2"));
#pragma warning(pop)
				FPlatformProcess::FreeDllHandle(DxgiDLL);
			}

			if (bQuadBufferStereoRequested)
			{
				if (CreateDXGIFactory2FnPtr)
				{
					bIsQuadBufferStereoEnabled = true;
				}
				else
				{
					UE_LOG(LogD3D11RHI, Warning, TEXT("Win8.1 or above is required for quad_buffer_stereo support."));
				}
			}
		}

		// IDXGIFactory2 required for dx11.1 active stereo and DXGI debug (dxgi1.3)
		if (CreateDXGIFactory2FnPtr)
		{
			uint32 Flags = bWithDebug ? DXGI_CREATE_FACTORY_DEBUG : 0;
			CreateDXGIFactory2FnPtr(Flags, __uuidof(IDXGIFactory2), (void**)DXGIFactory1);
		}
		else
		{
			CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)DXGIFactory1);
		}
	}
	__except(IsDelayLoadException(GetExceptionInformation()))
	{
		// We suppress warning C6322: Empty _except block. Appropriate checks are made upon returning. 
		CA_SUPPRESS(6322);
	}
#endif	//!D3D11_CUSTOM_VIEWPORT_CONSTRUCTOR
}

/**
 * Returns the lowest D3D feature level we are allowed to created based on
 * command line parameters.
 */
static D3D_FEATURE_LEVEL GetMinAllowedD3DFeatureLevel()
{
	return D3D_FEATURE_LEVEL_11_0;
}

/**
 * Returns the highest D3D feature level we are allowed to created based on
 * command line parameters.
 */
static D3D_FEATURE_LEVEL GetMaxAllowedD3DFeatureLevel()
{
	return D3D_FEATURE_LEVEL_11_1;
}

/**
 * Attempts to create a D3D11 device for the adapter using at most MaxFeatureLevel.
 * If creation is successful, true is returned and the supported feature level is set in OutFeatureLevel.
 */
static bool SafeTestD3D11CreateDevice(IDXGIAdapter* Adapter,D3D_FEATURE_LEVEL MinFeatureLevel,D3D_FEATURE_LEVEL MaxFeatureLevel,D3D_FEATURE_LEVEL* OutFeatureLevel)
{
	ID3D11Device* D3DDevice = nullptr;
	ID3D11DeviceContext* D3DDeviceContext = nullptr;
	uint32 DeviceFlags = D3D11_CREATE_DEVICE_SINGLETHREADED;
	// Use a debug device if specified on the command line.
	if(D3D11RHI_ShouldCreateWithD3DDebug())
	{
		DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
	}

	// @MIXEDREALITY_CHANGE : BEGIN - Add BGRA flag for Windows Mixed Reality HMD's
	DeviceFlags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	// @MIXEDREALITY_CHANGE : END

	D3D_FEATURE_LEVEL RequestedFeatureLevels[] =
	{
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	
	// Trim to allowed feature levels
	int32 FirstAllowedFeatureLevel = 0;
	int32 NumAllowedFeatureLevels = UE_ARRAY_COUNT(RequestedFeatureLevels);
	int32 LastAllowedFeatureLevel = NumAllowedFeatureLevels - 1;
	
	while (FirstAllowedFeatureLevel < NumAllowedFeatureLevels)
	{
		if (RequestedFeatureLevels[FirstAllowedFeatureLevel] == MaxFeatureLevel)
		{
			break;
		}
		FirstAllowedFeatureLevel++;
	}

	while (LastAllowedFeatureLevel > 0)
	{
		if (RequestedFeatureLevels[LastAllowedFeatureLevel] >= MinFeatureLevel)
		{
			break;
		}
		LastAllowedFeatureLevel--;
	}
	
	NumAllowedFeatureLevels = LastAllowedFeatureLevel - FirstAllowedFeatureLevel + 1;
	if (MaxFeatureLevel < MinFeatureLevel || NumAllowedFeatureLevels <= 0)
	{
		return false;
	}

	__try
	{
		// We don't want software renderer. Ideally we specify D3D_DRIVER_TYPE_HARDWARE on creation but
		// when we specify an adapter we need to specify D3D_DRIVER_TYPE_UNKNOWN (otherwise the call fails).
		// We cannot check the device type later (seems this is missing functionality in D3D).
		HRESULT Result = D3D11CreateDevice(
			Adapter,
			D3D_DRIVER_TYPE_UNKNOWN,
			nullptr,
			DeviceFlags,
			&RequestedFeatureLevels[FirstAllowedFeatureLevel],
			NumAllowedFeatureLevels,
			D3D11_SDK_VERSION,
			&D3DDevice,
			OutFeatureLevel,
			&D3DDeviceContext);

		if (SUCCEEDED(Result))
		{
			D3DDevice->Release();
			D3DDeviceContext->Release();
			return true;
		}

		// Log any reason for failure to create test device. Extra debug help.
		VERIFYD3D11RESULT_NOEXIT(Result);

#if PLATFORM_HOLOLENS
		bool bIsWin10 = true;
#else
		bool bIsWin10 = FPlatformMisc::VerifyWindowsVersion(10, 0);
#endif

		// Fatal error on 0x887A002D
		if (DXGI_ERROR_SDK_COMPONENT_MISSING == Result && bIsWin10)
		{
			UE_LOG(LogD3D11RHI, Fatal, TEXT("-d3ddebug was used but optional Graphics Tools were not found. Install them through the Manage Optional Features in windows. See: https://docs.microsoft.com/en-us/windows/uwp/gaming/use-the-directx-runtime-and-visual-studio-graphics-diagnostic-features"));
		}
	}
	__except(IsDelayLoadException(GetExceptionInformation()))
	{
		// We suppress warning C6322: Empty _except block. Appropriate checks are made upon returning. 
		CA_SUPPRESS(6322);
	}

	return false;
}

struct DisplayChromacities
{
	float RedX, RedY;
	float GreenX, GreenY;
	float BlueX, BlueY;
	float WpX, WpY;
};

const DisplayChromacities DisplayChromacityList[] =
{
	{ 0.64000f, 0.33000f, 0.30000f, 0.60000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, // EDisplayColorGamut::sRGB_D65
	{ 0.68000f, 0.32000f, 0.26500f, 0.69000f, 0.15000f, 0.06000f, 0.31270f, 0.32900f }, // EDisplayColorGamut::DCIP3_D65
	{ 0.70800f, 0.29200f, 0.17000f, 0.79700f, 0.13100f, 0.04600f, 0.31270f, 0.32900f }, // EDisplayColorGamut::Rec2020_D65
	{ 0.73470f, 0.26530f, 0.00000f, 1.00000f, 0.00010f,-0.07700f, 0.32168f, 0.33767f }, // EDisplayColorGamut::ACES_D60
	{ 0.71300f, 0.29300f, 0.16500f, 0.83000f, 0.12800f, 0.04400f, 0.32168f, 0.33767f }, // EDisplayColorGamut::ACEScg_D60
};

static void SetHDRMonitorModeNVIDIA(uint32 IHVDisplayIndex, bool bEnableHDR, EDisplayColorGamut DisplayGamut, float MaxOutputNits, float MinOutputNits, float MaxCLL, float MaxFALL)
{
#ifdef NVAPI_INTERFACE
	NvAPI_Status NvStatus = NVAPI_OK;
	NvDisplayHandle hNvDisplay = NULL;
	NvU32 DisplayId = (NvU32)IHVDisplayIndex;

	NV_HDR_CAPABILITIES HDRCapabilities = {};
	HDRCapabilities.version = NV_HDR_CAPABILITIES_VER;

	NvStatus = NvAPI_Disp_GetHdrCapabilities(DisplayId, &HDRCapabilities);

	if (NvStatus == NVAPI_OK)
	{
		if (HDRCapabilities.isST2084EotfSupported)
		{
			NV_HDR_COLOR_DATA HDRColorData = {};
			memset(&HDRColorData, 0, sizeof(HDRColorData));

			HDRColorData.version = NV_HDR_COLOR_DATA_VER;
			HDRColorData.cmd = NV_HDR_CMD_SET;
			HDRColorData.static_metadata_descriptor_id = NV_STATIC_METADATA_TYPE_1;
			HDRColorData.hdrMode = bEnableHDR ? NV_HDR_MODE_UHDBD : NV_HDR_MODE_OFF;

			const DisplayChromacities& Chroma = DisplayChromacityList[(int32)DisplayGamut];

			HDRColorData.mastering_display_data.displayPrimary_x0 = NvU16(Chroma.RedX * 50000.0f);
			HDRColorData.mastering_display_data.displayPrimary_y0 = NvU16(Chroma.RedY * 50000.0f);
			HDRColorData.mastering_display_data.displayPrimary_x1 = NvU16(Chroma.GreenX * 50000.0f);
			HDRColorData.mastering_display_data.displayPrimary_y1 = NvU16(Chroma.GreenY * 50000.0f);
			HDRColorData.mastering_display_data.displayPrimary_x2 = NvU16(Chroma.BlueX * 50000.0f);
			HDRColorData.mastering_display_data.displayPrimary_y2 = NvU16(Chroma.BlueY * 50000.0f);
			HDRColorData.mastering_display_data.displayWhitePoint_x = NvU16(Chroma.WpX * 50000.0f);
			HDRColorData.mastering_display_data.displayWhitePoint_y = NvU16(Chroma.WpY * 50000.0f);
			HDRColorData.mastering_display_data.max_display_mastering_luminance = NvU16(MaxOutputNits);
			HDRColorData.mastering_display_data.min_display_mastering_luminance = NvU16(MinOutputNits);
			HDRColorData.mastering_display_data.max_content_light_level = NvU16(MaxCLL);
			HDRColorData.mastering_display_data.max_frame_average_light_level = NvU16(MaxFALL);

			NvStatus = NvAPI_Disp_HdrColorControl(DisplayId, &HDRColorData);

			// Ignore expected failures caused by insufficient driver version, remote desktop connections and similar
			if (NvStatus != NVAPI_OK && NvStatus != NVAPI_ERROR && NvStatus != NVAPI_NVIDIA_DEVICE_NOT_FOUND)
			{
				NvAPI_ShortString SzDesc;
				NvAPI_GetErrorMessage(NvStatus, SzDesc);
				UE_LOG(LogD3D11RHI, Warning, TEXT("NvAPI_Disp_HdrColorControl returned %s (%x)"), ANSI_TO_TCHAR(SzDesc), int(NvStatus));
			}
		}
	}
#endif //NVAPI_INTERFACE
}

static void SetHDRMonitorModeAMD(uint32 IHVDisplayIndex, bool bEnableHDR, EDisplayColorGamut DisplayGamut, float MaxOutputNits, float MinOutputNits, float MaxCLL, float MaxFALL)
{
#ifdef AMD_AGS_API
	const int32 AmdHDRDeviceIndex = (IHVDisplayIndex & 0xffff0000) >> 16;
	const int32 AmdHDRDisplayIndex = IHVDisplayIndex & 0x0000ffff;

	check(AmdInfo.AmdAgsContext != NULL && AmdHDRDeviceIndex != -1 && AmdHDRDisplayIndex != -1);
	check(AmdInfo.AmdGpuInfo.numDevices > AmdHDRDeviceIndex && AmdInfo.AmdGpuInfo.devices[AmdHDRDeviceIndex].numDisplays > AmdHDRDisplayIndex);

	const AGSDeviceInfo& DeviceInfo = AmdInfo.AmdGpuInfo.devices[AmdHDRDeviceIndex];
	const AGSDisplayInfo& DisplayInfo = DeviceInfo.displays[AmdHDRDisplayIndex];

	if (DisplayInfo.HDR10 != 0 || DisplayInfo.dolbyVision != 0)
	{
		AGSDisplaySettings HDRDisplaySettings;
		FMemory::Memzero(&HDRDisplaySettings, sizeof(HDRDisplaySettings));

		HDRDisplaySettings.mode = bEnableHDR ? AGSDisplaySettings::Mode_HDR10_scRGB : AGSDisplaySettings::Mode_SDR;

		if (bEnableHDR)
		{
			const DisplayChromacities& Chroma = DisplayChromacityList[(int32)DisplayGamut];
			HDRDisplaySettings.chromaticityRedX   = Chroma.RedX;
			HDRDisplaySettings.chromaticityRedY   = Chroma.RedY;
			HDRDisplaySettings.chromaticityGreenX = Chroma.GreenX;
			HDRDisplaySettings.chromaticityGreenY = Chroma.GreenY;
			HDRDisplaySettings.chromaticityBlueX  = Chroma.BlueX;
			HDRDisplaySettings.chromaticityBlueY  = Chroma.BlueY;
			HDRDisplaySettings.chromaticityWhitePointX = Chroma.WpX;
			HDRDisplaySettings.chromaticityWhitePointY = Chroma.WpY;
			HDRDisplaySettings.maxLuminance = MaxOutputNits;
			HDRDisplaySettings.minLuminance = MinOutputNits;
			HDRDisplaySettings.maxContentLightLevel = MaxCLL;
			HDRDisplaySettings.maxFrameAverageLightLevel = MaxFALL;
		}

		AGSReturnCode AmdStatus = agsSetDisplayMode(AmdInfo.AmdAgsContext, AmdHDRDeviceIndex, AmdHDRDisplayIndex, &HDRDisplaySettings);

		// Ignore expected failures caused by insufficient driver version
		if (AmdStatus != AGS_SUCCESS && AmdStatus != AGS_LEGACY_DRIVER)
		{
			UE_LOG(LogD3D11RHI, Warning, TEXT("agsSetDisplayMode returned (%x)"), int(AmdStatus));
		}
	}
#endif //AMD_AGS_API
}

/** Enable HDR meta data transmission */
void FD3D11DynamicRHI::EnableHDR()
{
	if ( GRHISupportsHDROutput && IsHDREnabled() )
	{
		const EDisplayOutputFormat OutputDevice = HDRGetDefaultDisplayOutputFormat();
		const EDisplayColorGamut DisplayGamut = HDRGetDefaultDisplayColorGamut();

		const float DisplayMaxOutputNits = HDRGetDisplayMaximumLuminance();
		const float DisplayMinOutputNits = 0.0f;	// Min output of the display
		const float DisplayMaxCLL = 0.0f;			// Max content light level in lumens (0.0 == unknown)
		const float DisplayFALL = 0.0f;				// Frame average light level (0.0 == unknown)

		if (IsRHIDeviceNVIDIA())
		{
			SetHDRMonitorModeNVIDIA(
				HDRDetectedDisplayIHVIndex,
				true,
				DisplayGamut,
				DisplayMaxOutputNits,
				DisplayMinOutputNits,
				DisplayMaxCLL,
				DisplayFALL);
		}
		else if (IsRHIDeviceAMD())
		{
			SetHDRMonitorModeAMD(
				HDRDetectedDisplayIHVIndex,
				true,
				DisplayGamut,
				DisplayMaxOutputNits,
				DisplayMinOutputNits,
				DisplayMaxCLL,
				DisplayFALL);
		}
		else if (IsRHIDeviceIntel())
		{
			UE_LOG(LogD3D11RHI, Warning, TEXT("There is no HDR output implementation currently available for this hardware."));
		}
	}
}

/** Disable HDR meta data transmission */
void FD3D11DynamicRHI::ShutdownHDR()
{
	if (GRHISupportsHDROutput)
	{
		// Default SDR display data
		const float DisplayMaxOutputNits = 100.0f;	// Max output of the display
		const float DisplayMinOutputNits = 0.0f;	// Min output of the display
		const float DisplayMaxCLL = 100.0f;			// Max content light level in lumens
		const float DisplayFALL = 20.0f;			// Frame average light level

		if (IsRHIDeviceNVIDIA())
		{
			SetHDRMonitorModeNVIDIA(
				HDRDetectedDisplayIHVIndex,
				false,
				EDisplayColorGamut::sRGB_D65,
				DisplayMaxOutputNits,
				DisplayMinOutputNits,
				DisplayMaxCLL,
				DisplayFALL);
		}
		else if (IsRHIDeviceAMD())
		{
			SetHDRMonitorModeAMD(
				HDRDetectedDisplayIHVIndex,
				false,
				EDisplayColorGamut::sRGB_D65,
				DisplayMaxOutputNits,
				DisplayMinOutputNits,
				DisplayMaxCLL,
				DisplayFALL);
		}
		else if (IsRHIDeviceIntel())
		{
			// Not yet implemented
		}
	}
}

bool FD3D11DynamicRHI::SetupDisplayHDRMetaData()
{
	check(GetDevice());

	// Default to primary display
	SetHDRDetectedDisplayIndices(0, 0);
	
	DisplayList.Empty();

#if WITH_EDITOR
	// Determines if any displays support HDR
	bool bSupportsHDROutput = false;
	{
		IDXGIAdapter* DXGIAdapter = Adapter.DXGIAdapter;
		if (!DXGIAdapter)
		{
			return false;
		}

		for (uint32 DisplayIndex = 0; true; ++DisplayIndex)
		{
			TRefCountPtr<IDXGIOutput> DXGIOutput;
			if (S_OK != DXGIAdapter->EnumOutputs(DisplayIndex, DXGIOutput.GetInitReference()))
			{
				break;
			}

			TRefCountPtr<IDXGIOutput6> Output6;
			if (SUCCEEDED(DXGIOutput->QueryInterface(IID_PPV_ARGS(Output6.GetInitReference()))))
			{
				DXGI_OUTPUT_DESC1 OutputDesc;
				VERIFYD3D11RESULT(Output6->GetDesc1(&OutputDesc));

				// Check for HDR support on the display.
				const bool bDisplaySupportsHDROutput = (OutputDesc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
				if (bDisplaySupportsHDROutput)
				{
					UE_LOG(LogD3D11RHI, Log, TEXT("HDR output is supported on adapter %i, display %u:"), 0, DisplayIndex);
					UE_LOG(LogD3D11RHI, Log, TEXT("\t\tMinLuminance = %f"), OutputDesc.MinLuminance);
					UE_LOG(LogD3D11RHI, Log, TEXT("\t\tMaxLuminance = %f"), OutputDesc.MaxLuminance);
					UE_LOG(LogD3D11RHI, Log, TEXT("\t\tMaxFullFrameLuminance = %f"), OutputDesc.MaxFullFrameLuminance);

					bSupportsHDROutput = true;
				}

				FDisplayInformation DisplayInformation{};
				DisplayInformation.bHDRSupported = bDisplaySupportsHDROutput;
				const RECT& DisplayCoords = OutputDesc.DesktopCoordinates;
				DisplayInformation.DesktopCoordinates = FIntRect(DisplayCoords.left, DisplayCoords.top, DisplayCoords.right, DisplayCoords.bottom);
				DisplayList.Add(DisplayInformation);
			}
		}
	}

	return bSupportsHDROutput;
#else

	// Grab the adapter
	TRefCountPtr<IDXGIDevice> DXGIDevice;
	VERIFYD3D11RESULT(Direct3DDevice->QueryInterface(IID_IDXGIDevice, (void**)DXGIDevice.GetInitReference()));

	TRefCountPtr<IDXGIAdapter> DXGIAdapter;
	DXGIDevice->GetAdapter((IDXGIAdapter**)DXGIAdapter.GetInitReference());
		
	uint32 DisplayIndex = 0;
	uint32 ForcedDisplayIndex = 0;
	bool bForcedDisplay = FParse::Value(FCommandLine::Get(), TEXT("FullscreenDisplay="), ForcedDisplayIndex);

	bool bSupportsHDROutput = false;
	for (; true; ++DisplayIndex)
	{
		TRefCountPtr<IDXGIOutput> DXGIOutput;
		if (S_OK != DXGIAdapter->EnumOutputs(DisplayIndex, DXGIOutput.GetInitReference()))
		{
			break;
		}

		// Query requested display only
		if (bForcedDisplay && DisplayIndex != ForcedDisplayIndex)
		{
			continue;
		}

		DXGI_OUTPUT_DESC OutputDesc;
		DXGIOutput->GetDesc(&OutputDesc);
		FDisplayInformation DisplayInformation{};
		const RECT& DisplayCoords = OutputDesc.DesktopCoordinates;
		DisplayInformation.DesktopCoordinates = FIntRect(DisplayCoords.left, DisplayCoords.top, DisplayCoords.right, DisplayCoords.bottom);
		DisplayInformation.bHDRSupported = false;

		if (IsRHIDeviceNVIDIA())
		{
#ifdef NVAPI_INTERFACE
			NvU32 DisplayId = 0;

			// Technically, the DeviceName is a WCHAR however, UE makes the assumption elsewhere that TCHAR == WCHAR on Windows
			NvAPI_Status Status = NvAPI_DISP_GetDisplayIdByDisplayName(TCHAR_TO_ANSI(OutputDesc.DeviceName), &DisplayId);

			if (Status == NVAPI_OK)
			{
				NV_HDR_CAPABILITIES HdrCapabilities = {};

				HdrCapabilities.version = NV_HDR_CAPABILITIES_VER;

				if (NVAPI_OK == NvAPI_Disp_GetHdrCapabilities(DisplayId, &HdrCapabilities))
				{		
					// we're only choosing the first supported HDR monitor
					if (HdrCapabilities.isST2084EotfSupported && !bSupportsHDROutput)
					{
						UE_LOG(LogD3D11RHI, Log, TEXT("HDR output is supported on display %i (NvId: 0x%x)."), DisplayIndex, DisplayId);
						SetHDRDetectedDisplayIndices(DisplayIndex, DisplayId);
						bSupportsHDROutput = true;
					}
					DisplayInformation.bHDRSupported = HdrCapabilities.isST2084EotfSupported;
				}
			}
			else if (Status != NVAPI_ERROR && Status != NVAPI_NVIDIA_DEVICE_NOT_FOUND)
			{
				NvAPI_ShortString szDesc;
				NvAPI_GetErrorMessage(Status, szDesc);
				UE_LOG(LogD3D11RHI, Log, TEXT("Failed to enumerate display ID for NVAPI (%s) (%s) unable to"), OutputDesc.DeviceName, ANSI_TO_TCHAR(szDesc));
			}
#endif //NVAPI_INTERFACE
		}
		else if (IsRHIDeviceAMD())
		{
#ifdef AMD_AGS_API
			// Search the device list for a matching display device name
			for (uint16 AMDDeviceIndex = 0; AMDDeviceIndex < AmdInfo.AmdGpuInfo.numDevices; ++AMDDeviceIndex)
			{
				const AGSDeviceInfo& DeviceInfo = AmdInfo.AmdGpuInfo.devices[AMDDeviceIndex];
				for (uint16 AMDDisplayIndex = 0; DeviceInfo.displays != nullptr && AMDDisplayIndex < DeviceInfo.numDisplays; ++AMDDisplayIndex)
				{
					const AGSDisplayInfo& DisplayInfo = DeviceInfo.displays[AMDDisplayIndex];
					if (FCStringAnsi::Strcmp(TCHAR_TO_ANSI(OutputDesc.DeviceName), DisplayInfo.displayDeviceName) == 0)
					{
						// AGS has flags for HDR10 and Dolby Vision instead of a flag for the ST2084 transfer function.
						// Both HDR10 and Dolby Vision use the ST2084 EOTF.
						bool DisplaySupportsHDR = (DisplayInfo.HDR10 != 0 || DisplayInfo.dolbyVision != 0);
						if (DisplaySupportsHDR && !bSupportsHDROutput)
						{
							UE_LOG(LogD3D11RHI, Log, TEXT("HDR output is supported on display %i (AMD Device: 0x%x, Display: 0x%x)."), DisplayIndex, AMDDeviceIndex, AMDDisplayIndex);
							SetHDRDetectedDisplayIndices(DisplayIndex, (uint32)(AMDDeviceIndex << 16) | (uint32)AMDDisplayIndex);
							bSupportsHDROutput = true;
						}
						DisplayInformation.bHDRSupported = DisplaySupportsHDR;
					}
				}
			}
#endif //AMD_AGS_API
		}
		else if (IsRHIDeviceIntel())
		{
			// Not yet implemented
		}
		DisplayList.Add(DisplayInformation);
	}

	return bSupportsHDROutput;
#endif
}

static bool IsDeviceOverclocked()
{
	if (IsRHIDeviceNVIDIA())
	{
#ifdef NVAPI_INTERFACE
		NvAPI_Status Status;
		NvU32 NumGPUs;
		NvPhysicalGpuHandle GPUHandles[NVAPI_MAX_PHYSICAL_GPUS];

		// Fetch GPU NV handles
		Status = NvAPI_EnumPhysicalGPUs(GPUHandles, &NumGPUs);

		if (Status != NVAPI_OK)
		{
			return false;
		}

		for (NvU32 GPU = 0; GPU < NumGPUs; ++GPU)
		{
			const NvPhysicalGpuHandle& GPUHandle = GPUHandles[GPU];

			NV_GPU_PERF_PSTATES20_INFO PS20Info = {0};
			PS20Info.version = NV_GPU_PERF_PSTATES20_INFO_VER;

			Status = NvAPI_GPU_GetPstates20(GPUHandle, &PS20Info);

			// Some GPUs use an older struct layout
			if (Status == NVAPI_INCOMPATIBLE_STRUCT_VERSION)
			{
				PS20Info.version = NV_GPU_PERF_PSTATES20_INFO_VER1;
				Status = NvAPI_GPU_GetPstates20(GPUHandle, &PS20Info);
			}

			// Assume non-overclocked on failure
			if (Status != NVAPI_OK)
			{
				return false;
			}

			// Where state was marked editable, check for relevant modifications
			if (PS20Info.bIsEditable == 0)
			{
				continue;
			}

			NvU32 PState, Clock, Voltage;
			for (PState = 0; PState < PS20Info.numPstates; PState++)
			{
				if (PS20Info.pstates[PState].bIsEditable != 0)
				{
					// Are clocks user modified?
					for (Clock = 0; Clock < PS20Info.numClocks; Clock++)
					{
						if (PS20Info.pstates[PState].clocks[Clock].bIsEditable != 0 && PS20Info.pstates[PState].clocks[Clock].freqDelta_kHz.value != 0)
						{
							return true;
						}
					}

					// Are voltages user modified?
					for (Voltage = 0; Voltage < PS20Info.numBaseVoltages; Voltage++)
					{
						if (PS20Info.pstates[PState].baseVoltages[Voltage].bIsEditable != 0 && PS20Info.pstates[PState].baseVoltages[Voltage].voltDelta_uV.value != 0)
						{
							return true;
						}
					}
				}
			}
		}
#endif // NVAPI_INTERFACE
	}

	// Assume non-overclocked by default
	return false;
}

void FD3D11DynamicRHIModule::StartupModule()
{
#if NV_AFTERMATH
	const bool bAllowVendorDevice = !FParse::Param(FCommandLine::Get(), TEXT("novendordevice"));
	if (bAllowVendorDevice)
	{
		// Note - can't check device type here, we'll check for that before actually initializing Aftermath

		const FString AftermathBinariesRoot = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/NVIDIA/NVaftermath/Win64/");

		FPlatformProcess::PushDllDirectory(*AftermathBinariesRoot);
		void* Handle = FPlatformProcess::GetDllHandle(TEXT("GFSDK_Aftermath_Lib.x64.dll"));
		FPlatformProcess::PopDllDirectory(*AftermathBinariesRoot);

		if (Handle == nullptr)
		{
			UE_LOG(LogD3D11RHI, Warning, TEXT("Failed to load GFSDK_Aftermath_Lib.x64.dll"));
			GNVAftermathModuleLoaded = false;
		}
		else
		{
			UE_LOG(LogD3D11RHI, Log, TEXT("Loaded GFSDK_Aftermath_Lib.x64.dll"));
			GNVAftermathModuleLoaded = true;
		}
	}
	else
	{
		UE_LOG(LogD3D11RHI, Log, TEXT("-novendordevice enabled, so won't load GFSDK_Aftermath_Lib.x64.dll"));
	}
#endif
}

bool FD3D11DynamicRHIModule::IsSupported()
{
	// if not computed yet
	if(!ChosenAdapter.IsValid())
	{
		FindAdapter();
	}

	// The hardware must support at least 11_0.
	return ChosenAdapter.IsValid()
		&& ChosenAdapter.MaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_11_0;
}


const TCHAR* GetFeatureLevelString(D3D_FEATURE_LEVEL FeatureLevel)
{
	switch(FeatureLevel)
	{
		case D3D_FEATURE_LEVEL_9_1:		return TEXT("9_1");
		case D3D_FEATURE_LEVEL_9_2:		return TEXT("9_2");
		case D3D_FEATURE_LEVEL_9_3:		return TEXT("9_3");
		case D3D_FEATURE_LEVEL_10_0:	return TEXT("10_0");
		case D3D_FEATURE_LEVEL_10_1:	return TEXT("10_1");
		case D3D_FEATURE_LEVEL_11_0:	return TEXT("11_0");
		case D3D_FEATURE_LEVEL_11_1:	return TEXT("11_1");
	}
	return TEXT("X_X");
}

static uint32 CountAdapterOutputs(TRefCountPtr<IDXGIAdapter>& Adapter)
{
	uint32 OutputCount = 0;
	for(;;)
	{
		TRefCountPtr<IDXGIOutput> Output;
		HRESULT hr = Adapter->EnumOutputs(OutputCount, Output.GetInitReference());
		if(FAILED(hr))
		{
			break;
		}
		++OutputCount;
	}

	return OutputCount;
}

static void LogDXGIAdapterDesc(const DXGI_ADAPTER_DESC& AdapterDesc)
{
	UE_LOG(LogD3D11RHI, Log, TEXT("    Description : %s"), AdapterDesc.Description);
	UE_LOG(LogD3D11RHI, Log, TEXT("    VendorId    : %04x"), AdapterDesc.VendorId);
	UE_LOG(LogD3D11RHI, Log, TEXT("    DeviceId    : %04x"), AdapterDesc.DeviceId);
	UE_LOG(LogD3D11RHI, Log, TEXT("    SubSysId    : %04x"), AdapterDesc.SubSysId);
	UE_LOG(LogD3D11RHI, Log, TEXT("    Revision    : %04x"), AdapterDesc.Revision);
	UE_LOG(LogD3D11RHI, Log, TEXT("    DedicatedVideoMemory : %zu bytes"), AdapterDesc.DedicatedVideoMemory);
	UE_LOG(LogD3D11RHI, Log, TEXT("    DedicatedSystemMemory : %zu bytes"), AdapterDesc.DedicatedSystemMemory);
	UE_LOG(LogD3D11RHI, Log, TEXT("    SharedSystemMemory : %zu bytes"), AdapterDesc.SharedSystemMemory);
	UE_LOG(LogD3D11RHI, Log, TEXT("    AdapterLuid : %lu %lu"), AdapterDesc.AdapterLuid.HighPart, AdapterDesc.AdapterLuid.LowPart);
}

void FD3D11DynamicRHIModule::FindAdapter()
{
	// Once we chosen one we don't need to do it again.
	check(!ChosenAdapter.IsValid());

	// Try to create the DXGIFactory1.  This will fail if we're not running Vista SP2 or higher.
	TRefCountPtr<IDXGIFactory1> DXGIFactory1;
	SafeCreateDXGIFactory(DXGIFactory1.GetInitReference(), D3D11RHI_ShouldCreateWithD3DDebug());
	if(!DXGIFactory1)
	{
		return;
	}

	TRefCountPtr<IDXGIFactory6> DXGIFactory6;
	DXGIFactory1->QueryInterface(__uuidof(IDXGIFactory6), (void**)DXGIFactory6.GetInitReference());

	bool bAllowPerfHUD = true;

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	bAllowPerfHUD = false;
#endif

	// Allow HMD to override which graphics adapter is chosen, so we pick the adapter where the HMD is connected
	uint64 HmdGraphicsAdapterLuid  = IHeadMountedDisplayModule::IsAvailable() ? IHeadMountedDisplayModule::Get().GetGraphicsAdapterLuid() : 0;
	// Non-static as it is used only a few times
	auto* CVarGraphicsAdapter = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GraphicsAdapter"));
	int32 CVarExplicitAdapterValue = HmdGraphicsAdapterLuid == 0 ? (CVarGraphicsAdapter ? CVarGraphicsAdapter->GetValueOnGameThread() : -1) : -2;
	FParse::Value(FCommandLine::Get(), TEXT("graphicsadapter="), CVarExplicitAdapterValue);

	const bool bFavorNonIntegrated = CVarExplicitAdapterValue == -1;

	TRefCountPtr<IDXGIAdapter> TempAdapter;
	D3D_FEATURE_LEVEL MinAllowedFeatureLevel = GetMinAllowedD3DFeatureLevel();
	D3D_FEATURE_LEVEL MaxAllowedFeatureLevel = GetMaxAllowedD3DFeatureLevel();

	UE_LOG(LogD3D11RHI, Log, TEXT("D3D11 min allowed feature level: %s"), GetFeatureLevelString(MinAllowedFeatureLevel));
	UE_LOG(LogD3D11RHI, Log, TEXT("D3D11 max allowed feature level: %s"), GetFeatureLevelString(MaxAllowedFeatureLevel));

	FD3D11Adapter FirstWithoutIntegratedAdapter;
	FD3D11Adapter FirstAdapter;

	UE_LOG(LogD3D11RHI, Log, TEXT("D3D11 adapters:"));

	int PreferredVendor = D3D11RHI_PreferAdapterVendor();
	bool bAllowSoftwareFallback = D3D11RHI_AllowSoftwareFallback();


	int GpuPreferenceInt = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
	FParse::Value(FCommandLine::Get(), TEXT("-gpupreference="), GpuPreferenceInt);
	DXGI_GPU_PREFERENCE GpuPreference;
	switch(GpuPreferenceInt)
	{
	case 1: GpuPreference = DXGI_GPU_PREFERENCE_MINIMUM_POWER; break;
	case 2: GpuPreference = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE; break;
	default: GpuPreference = DXGI_GPU_PREFERENCE_UNSPECIFIED; break;
	}

	auto LocalEnumAdapters = [&DXGIFactory6, &DXGIFactory1, GpuPreference](UINT AdapterIndex, IDXGIAdapter** Adapter) -> HRESULT
	{
		if(!DXGIFactory6 || GpuPreference == DXGI_GPU_PREFERENCE_UNSPECIFIED)
		{
			return DXGIFactory1->EnumAdapters(AdapterIndex, Adapter);
		}
		else
		{
			return DXGIFactory6->EnumAdapterByGpuPreference(AdapterIndex, GpuPreference, __uuidof(IDXGIAdapter), (void**)Adapter);
		}
	};

	// Enumerate the DXGIFactory's adapters.
	for(uint32 AdapterIndex = 0; LocalEnumAdapters(AdapterIndex,TempAdapter.GetInitReference()) != DXGI_ERROR_NOT_FOUND; ++AdapterIndex)
	{
		// Check that if adapter supports D3D11.
		if(TempAdapter)
		{
			UE_LOG(LogD3D11RHI, Log, TEXT("Testing D3D11 Adapter %u:"), AdapterIndex);
			DXGI_ADAPTER_DESC AdapterDesc;
			if (HRESULT DescResult = TempAdapter->GetDesc(&AdapterDesc); FAILED(DescResult))
			{
				UE_LOG(LogD3D11RHI, Warning, TEXT("Failed to get description for adapter %u."), AdapterIndex);
			}
			else
			{
				LogDXGIAdapterDesc(AdapterDesc);
			}

			D3D_FEATURE_LEVEL ActualFeatureLevel = (D3D_FEATURE_LEVEL)0;
			if(SafeTestD3D11CreateDevice(TempAdapter,MinAllowedFeatureLevel,MaxAllowedFeatureLevel,&ActualFeatureLevel))
			{
				// Log some information about the available D3D11 adapters.
				
				uint32 OutputCount = CountAdapterOutputs(TempAdapter);

				UE_LOG(LogD3D11RHI, Log,
					TEXT("  %2u. '%s' (Feature Level %s)"),
					AdapterIndex,
					AdapterDesc.Description,
					GetFeatureLevelString(ActualFeatureLevel)
					);
				UE_LOG(LogD3D11RHI, Log,
					TEXT("      %u/%u/%u MB DedicatedVideo/DedicatedSystem/SharedSystem, Outputs:%d, VendorId:0x%x"),
					(uint32)(AdapterDesc.DedicatedVideoMemory / (1024*1024)),
					(uint32)(AdapterDesc.DedicatedSystemMemory / (1024*1024)),
					(uint32)(AdapterDesc.SharedSystemMemory / (1024*1024)),
					OutputCount,
					AdapterDesc.VendorId
					);

				bool bIsAMD = AdapterDesc.VendorId == 0x1002;
				bool bIsIntel = AdapterDesc.VendorId == 0x8086;
				bool bIsNVIDIA = AdapterDesc.VendorId == 0x10DE;
				bool bIsMicrosoft = AdapterDesc.VendorId == 0x1414;

				// Simple heuristic but without profiling it's hard to do better
				bool bIsNonLocalMemoryPresent = false;
				TRefCountPtr<IDXGIAdapter3> TempDxgiAdapter3;
				DXGI_QUERY_VIDEO_MEMORY_INFO NonLocalVideoMemoryInfo;
				if (SUCCEEDED(TempAdapter->QueryInterface(_uuidof(IDXGIAdapter3), (void**)TempDxgiAdapter3.GetInitReference())) &&
					TempDxgiAdapter3.IsValid() && SUCCEEDED(TempDxgiAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &NonLocalVideoMemoryInfo)))
				{
					bIsNonLocalMemoryPresent = NonLocalVideoMemoryInfo.Budget != 0;
				}

				// TODO: Using GPUDetect for Intel GPUs to check for integrated vs discrete status, pending GPUDetect update

				const bool bIsSoftware = bIsMicrosoft;
				const bool bIsIntegrated = !bIsNonLocalMemoryPresent;
				// PerfHUD is for performance profiling
				const bool bIsPerfHUD = !FCString::Stricmp(AdapterDesc.Description,TEXT("NVIDIA PerfHUD"));

				FD3D11Adapter CurrentAdapter(TempAdapter, ActualFeatureLevel, bIsSoftware, bIsIntegrated);

				// Add special check to support HMDs, which do not have associated outputs.
				// To reject the software emulation, unless the cvar wants it.
				// https://msdn.microsoft.com/en-us/library/windows/desktop/bb205075(v=vs.85).aspx#WARP_new_for_Win8
				// Before we tested for no output devices but that failed where a laptop had a Intel (with output) and NVidia (with no output)
				const bool bSkipSoftwareAdapter = bIsSoftware && !bAllowSoftwareFallback && CVarExplicitAdapterValue < 0 && HmdGraphicsAdapterLuid == 0;
				
				// we don't allow the PerfHUD adapter
				const bool bSkipPerfHUDAdapter = bIsPerfHUD && !bAllowPerfHUD;

				// the HMD wants a specific adapter, not this one
				const bool bSkipHmdGraphicsAdapter = HmdGraphicsAdapterLuid != 0 && FMemory::Memcmp(&HmdGraphicsAdapterLuid, &AdapterDesc.AdapterLuid, sizeof(LUID)) != 0;

				// the user wants a specific adapter, not this one
				const bool bSkipExplicitAdapter = CVarExplicitAdapterValue >= 0 && AdapterIndex != CVarExplicitAdapterValue;
				
				const bool bSkipAdapter = bSkipSoftwareAdapter || bSkipPerfHUDAdapter || bSkipHmdGraphicsAdapter || bSkipExplicitAdapter;

				if (!bSkipAdapter)
				{
					if (!bIsIntegrated && !FirstWithoutIntegratedAdapter.IsValid())
					{
						FirstWithoutIntegratedAdapter = CurrentAdapter;
					}
					else if (PreferredVendor == AdapterDesc.VendorId && FirstWithoutIntegratedAdapter.IsValid())
					{
						FirstWithoutIntegratedAdapter = CurrentAdapter;
					}

					if (!FirstAdapter.IsValid())
					{
						FirstAdapter = CurrentAdapter;
					}
					else if (PreferredVendor == AdapterDesc.VendorId && FirstAdapter.IsValid())
					{
						FirstAdapter = CurrentAdapter;
					}
				}
			}
			else
			{
				UE_LOG(LogD3D11RHI, Log, TEXT("  %2u. Unknown, failed to create test device."), AdapterIndex);
			}
		}
		else
		{
			UE_LOG(LogD3D11RHI, Log, TEXT("  %2u. Unknown, failed to create adapter."), AdapterIndex);
		}
	}

	if(bFavorNonIntegrated)
	{
		ChosenAdapter = FirstWithoutIntegratedAdapter;

		// We assume Intel is integrated graphics (slower than discrete) than NVIDIA or AMD cards and rather take a different one
		if(!ChosenAdapter.IsValid())
		{
			ChosenAdapter = FirstAdapter;
		}
	}
	else
	{
		ChosenAdapter = FirstAdapter;
	}

	if(ChosenAdapter.IsValid())
	{
		UE_LOG(LogD3D11RHI, Log, TEXT("Chosen D3D11 Adapter:"));
		LogDXGIAdapterDesc(ChosenAdapter.DXGIAdapterDesc);
	}
	else
	{
		UE_LOG(LogD3D11RHI, Error, TEXT("Failed to choose a D3D11 Adapter."));
	}

	GRHIAdapterName = ChosenAdapter.DXGIAdapterDesc.Description;
	GRHIVendorId = ChosenAdapter.DXGIAdapterDesc.VendorId;
	GRHIDeviceId = ChosenAdapter.DXGIAdapterDesc.DeviceId;
	GRHIDeviceRevision = ChosenAdapter.DXGIAdapterDesc.Revision;
	GRHIDeviceIsIntegrated = ChosenAdapter.bIsIntegrated;
}

FDynamicRHI* FD3D11DynamicRHIModule::CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel)
{
#if PLATFORM_HOLOLENS
	GMaxRHIFeatureLevel = ERHIFeatureLevel::ES3_1;
	GMaxRHIShaderPlatform = SP_D3D_ES3_1_HOLOLENS;
#endif

	IDXGIFactory1* DXGIFactory1;
	VERIFYD3D11RESULT(ChosenAdapter.DXGIAdapter->GetParent(__uuidof(DXGIFactory1), reinterpret_cast<void**>(&DXGIFactory1)));

#if PLATFORM_HOLOLENS
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_D3D_ES3_1_HOLOLENS;
#else
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_PCD3D_ES3_1;
#endif
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = SP_PCD3D_SM5;

	ERHIFeatureLevel::Type PreviewFeatureLevel;
	if (RHIGetPreviewFeatureLevel(PreviewFeatureLevel))
	{
		// ES3.1 feature level emulation in D3D11
		GMaxRHIFeatureLevel = PreviewFeatureLevel;
	}
	else
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;
		if (RequestedFeatureLevel < ERHIFeatureLevel::SM6)
		{
			GMaxRHIFeatureLevel = RequestedFeatureLevel;
		}
	}

	if (!ensure(GMaxRHIFeatureLevel < ERHIFeatureLevel::SM6))
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;
	}
	GMaxRHIShaderPlatform = GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel];
	check(GMaxRHIShaderPlatform != SP_NumPlatforms);

	GD3D11RHI = new FD3D11DynamicRHI(DXGIFactory1, ChosenAdapter.MaxSupportedFeatureLevel,ChosenAdapter);
	FDynamicRHI* FinalRHI = GD3D11RHI;

#if ENABLE_RHI_VALIDATION
	if (FParse::Param(FCommandLine::Get(), TEXT("RHIValidation")))
	{
		FinalRHI = new FValidationRHI(FinalRHI);
	}
#endif

	FGenericCrashContext::SetEngineData(TEXT("RHI.IntegratedGPU"), ChosenAdapter.bIsIntegrated ? TEXT("true") : TEXT("false"));

	return FinalRHI;
}

void FD3D11DynamicRHI::Init()
{
	InitD3DDevice();
}

void FD3D11DynamicRHI::PostInit()
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		// Make sure all global shaders are complete at this point
		extern RENDERCORE_API const int32 GlobalShaderMapId;

		TArray<int32> ShaderMapIds;
		ShaderMapIds.Add(GlobalShaderMapId);

		GShaderCompilingManager->FinishCompilation(TEXT("Global"), ShaderMapIds);
	}
}

bool FD3D11DynamicRHI::IsQuadBufferStereoEnabled()
{
	return bIsQuadBufferStereoEnabled;
}

void FD3D11DynamicRHI::DisableQuadBufferStereo()
{
	bIsQuadBufferStereoEnabled = false;
}

void FD3D11DynamicRHI::FlushPendingLogs()
{
#if !(UE_BUILD_SHIPPING && WITH_EDITOR)
	if (D3D11RHI_ShouldCreateWithD3DDebug())
	{
		TRefCountPtr<ID3D11InfoQueue> InfoQueue = nullptr;
		VERIFYD3D11RESULT_EX(Direct3DDevice->QueryInterface(IID_ID3D11InfoQueue, (void**)InfoQueue.GetInitReference()), Direct3DDevice);
		if (InfoQueue)
		{
			FString FullMessage;
			uint64 NumMessages = InfoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
			for (uint64 Index = 0; Index < NumMessages; ++Index)
			{
				SIZE_T Length = 0;
				if (SUCCEEDED(InfoQueue->GetMessage(Index, nullptr, &Length)))
				{
					TArray<uint8> Bytes;
					Bytes.AddUninitialized((int32)Length);
					D3D11_MESSAGE* Message = (D3D11_MESSAGE*)Bytes.GetData();
					if (SUCCEEDED(InfoQueue->GetMessage(Index, Message, &Length)))
					{
						FullMessage += TEXT("\n\t");
						FullMessage += Message->pDescription;
					}
				}
			}

			if (FullMessage.Len() > 0)
			{
				UE_LOG(LogD3D11RHI, Warning, TEXT("d3debug warnings/errors found:%s"), *FullMessage);
			}
			InfoQueue->ClearStoredMessages();
		}
	}
#endif
}

#if NV_AFTERMATH
static void CacheNVAftermathEnabled()
{
	if (GNVAftermathModuleLoaded && IsRHIDeviceNVIDIA() && !FParse::Param(FCommandLine::Get(), TEXT("nogpucrashdebugging")))
	{
		// Two ways to enable aftermath, command line or the r.GPUCrashDebugging variable
		// Note: If intending to change this please alert game teams who use this for user support.
		if (FParse::Param(FCommandLine::Get(), TEXT("gpucrashdebugging")))
		{
			GDX11NVAfterMathEnabled = true;
		}
		else
		{
			static IConsoleVariable* GPUCrashDebugging = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUCrashDebugging"));
			if (GPUCrashDebugging)
			{
				GDX11NVAfterMathEnabled = GPUCrashDebugging->GetInt() != 0;
			}
		}
	}
	else
	{
		GDX11NVAfterMathEnabled = false;
	}
}

void FD3D11DynamicRHI::StartNVAftermath()
{
	bool bShouldStart = GDX11NVAfterMathEnabled
		&& Direct3DDevice
		&& Direct3DDeviceIMContext
		&& !NVAftermathIMContextHandle
		&& bAllowVendorDevice;

	if (bShouldStart)
	{
		static IConsoleVariable* MarkersCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUCrashDebugging.Aftermath.Markers"));
		static IConsoleVariable* CallstackCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUCrashDebugging.Aftermath.Callstack"));
		static IConsoleVariable* ResourcesCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUCrashDebugging.Aftermath.ResourceTracking"));
		static IConsoleVariable* TrackAllCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUCrashDebugging.Aftermath.TrackAll"));
		
		const bool bEnableInEditor = GIsEditor && !FParse::Param(FCommandLine::Get(), TEXT("nogpucrashdebugging"));
		const bool bEnableMarkers = FParse::Param(FCommandLine::Get(), TEXT("aftermathmarkers")) || (MarkersCVar && MarkersCVar->GetInt()) || bEnableInEditor;
		const bool bEnableCallstack = FParse::Param(FCommandLine::Get(), TEXT("aftermathcallstack")) || (CallstackCVar && CallstackCVar->GetInt());
		const bool bEnableResources = FParse::Param(FCommandLine::Get(), TEXT("aftermathresources")) || (ResourcesCVar && ResourcesCVar->GetInt());
		const bool bEnableAll = FParse::Param(FCommandLine::Get(), TEXT("aftermathall")) || (TrackAllCVar && TrackAllCVar->GetInt());

		uint32 Flags = GFSDK_Aftermath_FeatureFlags_Minimum;

		Flags |= bEnableMarkers ? GFSDK_Aftermath_FeatureFlags_EnableMarkers : 0;
		Flags |= bEnableCallstack ? GFSDK_Aftermath_FeatureFlags_CallStackCapturing : 0;
		Flags |= bEnableResources ? GFSDK_Aftermath_FeatureFlags_EnableResourceTracking : 0;
		Flags |= bEnableAll ? GFSDK_Aftermath_FeatureFlags_Maximum : 0;

		// @todo - GFSDK_Aftermath_FeatureFlags_EnableShaderErrorReporting is disabled to prevent TDRs until Nvidia fixes this
		Flags &= ~GFSDK_Aftermath_FeatureFlags_EnableShaderErrorReporting;

		GFSDK_Aftermath_Result Result = GFSDK_Aftermath_DX11_Initialize(
			GFSDK_Aftermath_Version_API, (GFSDK_Aftermath_FeatureFlags)Flags, Direct3DDevice);

		if (GFSDK_Aftermath_SUCCEED(Result)) //-V547 Expression is always true -- confirmed false positive, fix coming in future PVS version (v6.24)
		{
			Result = GFSDK_Aftermath_DX11_CreateContextHandle(Direct3DDeviceIMContext, &NVAftermathIMContextHandle);

			if (GFSDK_Aftermath_SUCCEED(Result)) //-V547 Expression is always true -- confirmed false positive, fix coming in future PVS version (v6.24)
			{
				UE_LOG(LogD3D11RHI, Log, TEXT("[Aftermath] Enabled and primed"));
			}
			else
			{
				UE_LOG(LogD3D11RHI, Log, TEXT("[Aftermath] Failed to create context handle. Result=%08x"), Result);
				GDX11NVAfterMathEnabled = false;
			}
		}
		else
		{
			UE_LOG(LogD3D11RHI, Log, TEXT("[Aftermath] Failed to initialize. Result=%08x"), Result);
			GDX11NVAfterMathEnabled = false;
		}

		if (GDX11NVAfterMathEnabled && (bEnableMarkers || bEnableAll))
		{
			SetEmitDrawEvents(true);
			GDX11NVAfterMathMarkers = true;
		}
	}

	FGenericCrashContext::SetEngineData(TEXT("RHI.Aftermath"), GDX11NVAfterMathEnabled ? TEXT("true") : TEXT("false"));
}

void FD3D11DynamicRHI::StopNVAftermath()
{
	bool bShouldStop = GDX11NVAfterMathEnabled
		&& NVAftermathIMContextHandle
		&& bAllowVendorDevice;

	if (bShouldStop)
	{
#if UE_BUILD_SHIPPING
		SetEmitDrawEvents(false);
#endif
		GFSDK_Aftermath_Result Result = GFSDK_Aftermath_ReleaseContextHandle(NVAftermathIMContextHandle);

		if (GFSDK_Aftermath_SUCCEED(Result)) //-V547 Expression is always true -- confirmed false positive, fix coming in future PVS version (v6.24)
		{
			UE_LOG(LogD3D11RHI, Log, TEXT("[Aftermath] Stopped"));
			NVAftermathIMContextHandle = nullptr;
		}
		else
		{
			UE_LOG(LogD3D11RHI, Log, TEXT("[Aftermath] Failed to release context handle. Result=%08x"), Result);
			GDX11NVAfterMathEnabled = false;
		}
	}
}

static void D3D11AftermathCrashCallback(const void* InGPUCrashDump, const uint32_t InGPUCrashDumpSize, void* InUserData)
{
	// decode the GPU marker stack data
	if (GDynamicRHI)
	{
		GDynamicRHI->CheckGpuHeartbeat();
	}

	// Write out crash dump to project log dir - exception handling code will take care of copying it to the correct location
	const FString GPUMiniDumpPath = FPaths::Combine(FPaths::ProjectLogDir(), FWindowsPlatformCrashContext::UEGPUAftermathMinidumpName);

	// Just use raw windows file routines for the GPU minidump (TODO: refactor to our own functions?)
	HANDLE FileHandle = CreateFileW(*GPUMiniDumpPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		WriteFile(FileHandle, InGPUCrashDump, InGPUCrashDumpSize, nullptr, nullptr);
	}
	CloseHandle(FileHandle);

	// Report the GPU crash which will raise the exception
	ReportGPUCrash(TEXT("Aftermath GPU Crash dump Triggered"), nullptr);
}

void EnableNVAftermathCrashDumps()
{
	if (GNVAftermathModuleLoaded)
	{
		static IConsoleVariable* GPUCrashDump = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUCrashDump"));
		if (FParse::Param(FCommandLine::Get(), TEXT("gpucrashdump")) || (GPUCrashDump && GPUCrashDump->GetInt()))
		{

			GFSDK_Aftermath_Result Result = GFSDK_Aftermath_EnableGpuCrashDumps(
				GFSDK_Aftermath_Version_API,
				GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_DX,
				GFSDK_Aftermath_GpuCrashDumpFeatureFlags_Default,
				&D3D11AftermathCrashCallback,
				nullptr, //Shader debug callback
				nullptr, // description callback
				nullptr, // resolve marker callback
				nullptr); // user data

			if (Result == GFSDK_Aftermath_Result_Success)
			{
				UE_LOG(LogD3D11RHI, Log, TEXT("[Aftermath] Aftermath crash dumping enabled"));
			}
			else
			{
				UE_LOG(LogD3D11RHI, Log, TEXT("[Aftermath] Aftermath crash dumping failed to initialize (%x)"), Result);
			}
		}
	}
}

#define CACHE_NV_AFTERMATH_ENABLED() CacheNVAftermathEnabled()
#define START_NV_AFTERMATH() StartNVAftermath()
#define STOP_NV_AFTERMATH() StopNVAftermath()
#define ENABLE_NV_AFTERMATH_CRASH_DUMPS() EnableNVAftermathCrashDumps()

#else

#define CACHE_NV_AFTERMATH_ENABLED()
#define START_NV_AFTERMATH()
#define STOP_NV_AFTERMATH()
#define ENABLE_NV_AFTERMATH_CRASH_DUMPS()

#endif

#if INTEL_EXTENSIONS
void FD3D11DynamicRHI::StartIntelExtensions()
{
	if (!bAllowVendorDevice)
	{
		return;
	}

	const INTCExtensionVersion AtomicsRequiredVersion = { 3, 4, 1 }; // version 3.4.1
	const INTCExtensionVersion UAVOverlapRequiredVersion = { 1, 1, 0 };

	INTCExtensionVersion* SupportedExtensionsVersions = nullptr;
	uint32_t SupportedExtensionsVersionCount = 0;
	INTCExtensionInfo INTCExtensionInfo = {};

	if (FAILED(INTC_LoadExtensionsLibrary(false)))
	{
		UE_LOG(LogD3D11RHI, Log, TEXT("Failed to load Intel Extensions Library"));
	}

	if (SUCCEEDED(INTC_D3D11_GetSupportedVersions(Direct3DDevice, nullptr, &SupportedExtensionsVersionCount)))
	{
		SupportedExtensionsVersions = new INTCExtensionVersion[SupportedExtensionsVersionCount]{};
	}

	// Workaround for C6385, if we pass in SupportedExtensionsVersionCount again, the static analyzer thinks it may be different from the first call
	uint32_t DummyCount = SupportedExtensionsVersionCount;
	if (SUCCEEDED(INTC_D3D11_GetSupportedVersions(Direct3DDevice, SupportedExtensionsVersions, &DummyCount)) && SupportedExtensionsVersions != nullptr)
	{
		check(SupportedExtensionsVersionCount == DummyCount);
		for (uint32_t i = 0; i < SupportedExtensionsVersionCount; i++)
		{
			if ((SupportedExtensionsVersions[i].HWFeatureLevel >= AtomicsRequiredVersion.HWFeatureLevel) &&
				(SupportedExtensionsVersions[i].APIVersion >= AtomicsRequiredVersion.APIVersion) &&
				(SupportedExtensionsVersions[i].Revision >= AtomicsRequiredVersion.Revision) &&
				!GRHISupportsAtomicUInt64)
			{
				UE_LOG(LogD3D11RHI, Log, TEXT("Intel Extensions loaded requested version Atomics Version: %u.%u.%u"),
					SupportedExtensionsVersions[i].HWFeatureLevel,
					SupportedExtensionsVersions[i].APIVersion,
					SupportedExtensionsVersions[i].Revision);

				INTCExtensionInfo.RequestedExtensionVersion = SupportedExtensionsVersions[i];
				GRHISupportsAtomicUInt64 = true;
			}

			if ((SupportedExtensionsVersions[i].HWFeatureLevel >= UAVOverlapRequiredVersion.HWFeatureLevel) &&
				(SupportedExtensionsVersions[i].APIVersion >= UAVOverlapRequiredVersion.APIVersion) &&
				(SupportedExtensionsVersions[i].Revision >= UAVOverlapRequiredVersion.Revision) &&
				!bIntelSupportsUAVOverlap)
			{
				UE_LOG(LogD3D11RHI, Log, TEXT("Intel Extensions loaded requested version for UAVOverlap: %u.%u.%u"),
					SupportedExtensionsVersions[i].HWFeatureLevel,
					SupportedExtensionsVersions[i].APIVersion,
					SupportedExtensionsVersions[i].Revision);

				if (SupportedExtensionsVersions[i].HWFeatureLevel >= INTCExtensionInfo.RequestedExtensionVersion.HWFeatureLevel &&
					SupportedExtensionsVersions[i].APIVersion >= INTCExtensionInfo.RequestedExtensionVersion.APIVersion &&
					SupportedExtensionsVersions[i].Revision >= INTCExtensionInfo.RequestedExtensionVersion.Revision)
				{
					INTCExtensionInfo.RequestedExtensionVersion = SupportedExtensionsVersions[i];
				}
				bIntelSupportsUAVOverlap = true;
			}

			if (GRHISupportsAtomicUInt64 && bIntelSupportsUAVOverlap)
			{
				break;
			}
		}
	}

	check(IntelExtensionContext == nullptr);
	INTCExtensionAppInfo AppInfo = {};
	AppInfo.pEngineName = TEXT("Unreal Engine");
	AppInfo.EngineVersion = 5;

	HRESULT hr = INTC_D3D11_CreateDeviceExtensionContext(Direct3DDevice, &IntelExtensionContext, &INTCExtensionInfo, &AppInfo);
	bool bEnabled = false;
	if (SUCCEEDED(hr))
	{
		bEnabled = true;
		UE_LOG(LogD3D11RHI, Log, TEXT("Intel Extensions Framework enabled"));
	}
	else if (hr == E_OUTOFMEMORY)
	{
		UE_LOG(LogD3D11RHI, Log, TEXT("Intel Extensions Framework not supported by driver"));
	}
	else if (hr == E_INVALIDARG)
	{
		UE_LOG(LogD3D11RHI, Log, TEXT("Intel Extensions Framework passed invalid creation arguments"));
	}

	if (!bEnabled)
	{
		GRHISupportsAtomicUInt64 = false;
		StopIntelExtensions();
	}

	if (SupportedExtensionsVersions != nullptr)
	{
		delete[] SupportedExtensionsVersions;
	}
}

void FD3D11DynamicRHI::StopIntelExtensions()
{
	if(IntelExtensionContext && bAllowVendorDevice)
	{
		HRESULT hr = INTC_DestroyDeviceExtensionContext(&IntelExtensionContext);

		if (hr == S_OK)
		{
			UE_LOG(LogD3D11RHI, Log, TEXT("Intel Extensions Framework unloaded"));
		}
		else if (hr == E_INVALIDARG)
		{
			UE_LOG(LogD3D11RHI, Log, TEXT("Intel Extensions Framework error when unloading"));
		}

		IntelExtensionContext = nullptr;
	}
}
#endif // INTEL_EXTENSIONS

#if INTEL_METRICSDISCOVERY
static int32 GetIntelDriverBuildNumber(const FString& VerStr)
{
	int32 LastDotPos, FirstDotPos;

	// https://www.intel.com/content/www/us/en/support/articles/000005654/graphics.html
	// Older Windows drivers follow 9.18.10.3310 where the last four digits are the driver number
	// Newer Windows drivers follow 27.20.100.9466 where the last seven digits are the driver number
	// Linux drivers follow 6000.0001 where the last four digits are the driver number. Not supported here

	// Chop off the last 8 characters. On older drivers the first character will be a dot instead of a number
	FString RightPart = VerStr.Right(8);
	RightPart.FindChar(TEXT('.'), FirstDotPos);

	if (FirstDotPos == 0)
	{
		// Old driver naming, use last four digits
		if (VerStr.FindLastChar(TEXT('.'), LastDotPos) && FCString::IsNumeric(&VerStr[LastDotPos + 1]))
		{
			return FCString::Atoi(&VerStr[LastDotPos + 1]);
		}
	}
	else
	{
		// New driver naming, use seven digits after removing dot
		RightPart = RightPart.Replace(TEXT("."), TEXT(""));
		if (FCString::IsNumeric(&RightPart[0]) && RightPart.Len() == 7)
		{
			return FCString::Atoi(&RightPart[0]);
		}
	}
	return -1;
}

void FD3D11DynamicRHI::CreateIntelMetricsDiscovery()
{
	// Per Jeff from Intel: So far drivers >6323 are known working
	if (IsRHIDeviceIntel() && GetIntelDriverBuildNumber(GRHIAdapterUserDriverVersion) > 6323)
	{
		IntelMetricsDiscoveryHandle = MakeUnique<Intel_MetricsDiscovery_ContextData>();

		MDH_Context::Result Result;
		Result = IntelMetricsDiscoveryHandle->MDHContext.Initialize();

		if (Result != MDH_Context::Result::RESULT_OK)
		{
			UE_LOG(LogD3D11RHI, Log, TEXT("[IntelMetricsDiscovery] Failed to initialize context. Result=%08x"), Result);
			GDX11IntelMetricsDiscoveryEnabled = false;
			IntelMetricsDiscoveryHandle = nullptr;
			return;
		}

		GDX11IntelMetricsDiscoveryEnabled = true;
	}
	else
	{
		GDX11IntelMetricsDiscoveryEnabled = false;
	}
}

void FD3D11DynamicRHI::StartIntelMetricsDiscovery()
{
	bool bShouldStart = GDX11IntelMetricsDiscoveryEnabled
		&& IntelMetricsDiscoveryHandle;

	if (bShouldStart)
	{
		IntelMetricsDiscoveryHandle->MDConcurrentGroup = MDH_FindConcurrentGroup(IntelMetricsDiscoveryHandle->MDHContext.MDDevice, "OA");
		IntelMetricsDiscoveryHandle->MDMetricSet = MDH_FindMetricSet(IntelMetricsDiscoveryHandle->MDConcurrentGroup, "RenderBasic");
		auto GPUFreqValue = MDH_FindGlobalSymbol(IntelMetricsDiscoveryHandle->MDHContext.MDDevice, "GpuTimestampFrequency");
		IntelMetricsDiscoveryHandle->GPUTimeIndex = MDH_FindMetric(IntelMetricsDiscoveryHandle->MDMetricSet, "GpuTime");

		if (IntelMetricsDiscoveryHandle->GPUTimeIndex == UINT32_MAX ||
			GPUFreqValue.ValueType == MetricsDiscovery::VALUE_TYPE_LAST)
		{
			UE_LOG(LogD3D11RHI, Log, TEXT("[IntelMetricsDiscovery] Failed to initialize metrics set"));
			IntelMetricsDiscoveryHandle->MDHContext.Finalize();
			GDX11IntelMetricsDiscoveryEnabled = false;
			return;
		}

		if(!IntelMetricsDiscoveryHandle->MDHRangeMetrics.Initialize(IntelMetricsDiscoveryHandle->MDHContext.MDDevice,
			IntelMetricsDiscoveryHandle->MDConcurrentGroup, IntelMetricsDiscoveryHandle->MDMetricSet, GetDevice(), 2))
		{
			UE_LOG(LogD3D11RHI, Log, TEXT("[IntelMetricsDiscovery] Failed to initialize range metrics"));
			IntelMetricsDiscoveryHandle->MDHContext.Finalize();
			GDX11IntelMetricsDiscoveryEnabled = false;
			IntelMetricsDiscoveryHandle = nullptr;
			return;
		}

		IntelMetricsDiscoveryHandle->bFrameBegun = false;

		UE_LOG(LogD3D11RHI, Log, TEXT("[IntelMetricsDiscovery] Started"));
	}
}

void FD3D11DynamicRHI::StopIntelMetricsDiscovery()
{
	bool bShouldStop = GDX11IntelMetricsDiscoveryEnabled
		&& IntelMetricsDiscoveryHandle;

	if (bShouldStop)
	{
		IntelMetricsDiscoveryHandle->MDHRangeMetrics.Finalize();
		IntelMetricsDiscoveryHandle->MDHContext.Finalize();

		UE_LOG(LogD3D11RHI, Log, TEXT("[IntelMetricsDiscovery] Stopped"));
		GDX11IntelMetricsDiscoveryEnabled = false;
		IntelMetricsDiscoveryHandle = nullptr;
	}
}

void FD3D11DynamicRHI::IntelMetricsDicoveryBeginFrame()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_IntelMetricsDiscovery_BeginFrame);

	bool bShouldBeginFrame = GDX11IntelMetricsDiscoveryEnabled
		&& IntelMetricsDiscoveryHandle && !IntelMetricsDiscoveryHandle->bFrameBegun;

	if (bShouldBeginFrame)
	{
		IntelMetricsDiscoveryHandle->ReportInUse = IntelMetricsDiscoveryHandle->ReportInUse == 1 ? 0 : 1;
		IntelMetricsDiscoveryHandle->bFrameBegun = true;
		IntelMetricsDiscoveryHandle->MDHRangeMetrics.BeginRange(GetDeviceContext(), IntelMetricsDiscoveryHandle->ReportInUse);
	}
}

void FD3D11DynamicRHI::IntelMetricsDicoveryEndFrame()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_IntelMetricsDiscovery_EndFrame);

	bool bShouldEndFrame = GDX11IntelMetricsDiscoveryEnabled
		&& IntelMetricsDiscoveryHandle && IntelMetricsDiscoveryHandle->bFrameBegun;

	if (bShouldEndFrame)
	{
		IntelMetricsDiscoveryHandle->MDHRangeMetrics.EndRange(GetDeviceContext(), IntelMetricsDiscoveryHandle->ReportInUse);
		IntelMetricsDiscoveryHandle->bFrameBegun = false;

		static bool bFirstFrame = true;

		if (!bFirstFrame)
		{
			uint32 ReportToGather = IntelMetricsDiscoveryHandle->ReportInUse == 1 ? 0 : 1;

			IntelMetricsDiscoveryHandle->MDHRangeMetrics.GetRangeReports(GetDeviceContext(), ReportToGather, 1);
			IntelMetricsDiscoveryHandle->MDHRangeMetrics.ExecuteRangeEquations(GetDeviceContext(), ReportToGather, 1);

			auto GPUTime = IntelMetricsDiscoveryHandle->MDHRangeMetrics.ReportValues.GetValue(ReportToGather, IntelMetricsDiscoveryHandle->GPUTimeIndex).ValueUInt64;

			uint64 CyclesPerMs = 0.001 / FPlatformTime::GetSecondsPerCycle();
			uint64 GPUTimeMs = GPUTime / (1000 * 1000);
			uint64 GPUCycles = GPUTimeMs * CyclesPerMs;

			IntelMetricsDiscoveryHandle->LastGPUTime = GPUCycles;
		}

		if (bFirstFrame)
		{
			bFirstFrame = false;
		}
	}
}

double FD3D11DynamicRHI::IntelMetricsDicoveryGetGPUTime()
{
	return IntelMetricsDiscoveryHandle->LastGPUTime;
}
#endif // INTEL_METRICSDISCOVERY

void FD3D11DynamicRHI::InitD3DDevice()
{
	check( IsInGameThread() );

	// UE no longer supports clean-up and recovery on DEVICE_LOST.

	// If we don't have a device yet, either because this is the first viewport, or the old device was removed, create a device.
	if(!Direct3DDevice)
	{
		// Wait for the rendering thread to go idle.
		FlushRenderingCommands();

		UE_LOG(LogD3D11RHI, Log, TEXT("Creating new Direct3DDevice"));
		check(!GIsRHIInitialized);

		// Clear shadowed shader resources.
		ClearState();

		// In Direct3D 11, if you are trying to create a hardware or a software device, set pAdapter != NULL which constrains the other inputs to be:
		//		DriverType must be D3D_DRIVER_TYPE_UNKNOWN 
		//		Software must be NULL. 
		D3D_DRIVER_TYPE DriverType = D3D_DRIVER_TYPE_UNKNOWN;	

		uint32 DeviceFlags = D3D11RHI_ShouldAllowAsyncResourceCreation() ? 0 : D3D11_CREATE_DEVICE_SINGLETHREADED;

		// Use a debug device if specified on the command line.
		const bool bWithD3DDebug = D3D11RHI_ShouldCreateWithD3DDebug();
		FGenericCrashContext::SetEngineData(TEXT("RHI.D3DDebug"), bWithD3DDebug ? TEXT("true") : TEXT("false"));

		if (bWithD3DDebug)
		{
			DeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;

			UE_LOG(LogD3D11RHI, Log, TEXT("InitD3DDevice: -D3DDebug = %s"), bWithD3DDebug ? TEXT("on") : TEXT("off"));
		}

		// @MIXEDREALITY_CHANGE : BEGIN - Add BGRA flag for Windows Mixed Reality HMD's
		DeviceFlags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		// @MIXEDREALITY_CHANGE : END

		GTexturePoolSize = 0;

		// turn off creation on other threads for NVidia since a driver heuristic will notice that and make the creation synchronous, and that is not desirable given that large number of shaders will still be created on a single thread
		GRHISupportsMultithreadedShaderCreation = !IsRHIDeviceNVIDIA(); 

		UE_LOG(LogD3D11RHI, Log, TEXT("    GPU DeviceId: 0x%x (for the marketing name, search the web for \"GPU Device Id\")"), Adapter.DXGIAdapterDesc.DeviceId);

		// Issue: 32bit windows doesn't report 64bit value, we take what we get.
		FD3D11GlobalStats::GDedicatedVideoMemory = int64(Adapter.DXGIAdapterDesc.DedicatedVideoMemory);
		FD3D11GlobalStats::GDedicatedSystemMemory = int64(Adapter.DXGIAdapterDesc.DedicatedSystemMemory);
		FD3D11GlobalStats::GSharedSystemMemory = int64(Adapter.DXGIAdapterDesc.SharedSystemMemory);

		// Total amount of system memory, clamped to 8 GB
		int64 TotalPhysicalMemory = FMath::Min(int64(FPlatformMemory::GetConstants().TotalPhysicalGB), 8ll) * (1024ll * 1024ll * 1024ll);

		// Consider 50% of the shared memory but max 25% of total system memory.
		int64 ConsideredSharedSystemMemory = FMath::Min( FD3D11GlobalStats::GSharedSystemMemory / 2ll, TotalPhysicalMemory / 4ll );

		TRefCountPtr<IDXGIAdapter3> DxgiAdapter3;
		DXGI_QUERY_VIDEO_MEMORY_INFO LocalVideoMemoryInfo;
		FD3D11GlobalStats::GTotalGraphicsMemory = 0;
		if (Adapter.bIsIntegrated)
		{
			// It's all system memory.
			FD3D11GlobalStats::GTotalGraphicsMemory = FD3D11GlobalStats::GDedicatedVideoMemory;
			FD3D11GlobalStats::GTotalGraphicsMemory += FD3D11GlobalStats::GDedicatedSystemMemory;
			FD3D11GlobalStats::GTotalGraphicsMemory += ConsideredSharedSystemMemory;
		}
		else if (IsRHIDeviceAMD() && SUCCEEDED(Adapter.DXGIAdapter->QueryInterface(_uuidof(IDXGIAdapter3), (void**)DxgiAdapter3.GetInitReference())) &&
			DxgiAdapter3.IsValid() && SUCCEEDED(DxgiAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &LocalVideoMemoryInfo)))
		{
			// use the entire budget for D3D11, in keeping with setting GTotalGraphicsMemory to all of AdapterDesc.DedicatedVideoMemory
			// in the other method directly below
			FD3D11GlobalStats::GTotalGraphicsMemory = LocalVideoMemoryInfo.Budget;
		}
		else if ( FD3D11GlobalStats::GDedicatedVideoMemory >= 200*1024*1024 )
		{
			// Use dedicated video memory, if it's more than 200 MB
			FD3D11GlobalStats::GTotalGraphicsMemory = FD3D11GlobalStats::GDedicatedVideoMemory;
		}
		else if ( FD3D11GlobalStats::GDedicatedSystemMemory >= 200*1024*1024 )
		{
			// Use dedicated system memory, if it's more than 200 MB
			FD3D11GlobalStats::GTotalGraphicsMemory = FD3D11GlobalStats::GDedicatedSystemMemory;
		}
		else if ( FD3D11GlobalStats::GSharedSystemMemory >= 400*1024*1024 )
		{
			// Use some shared system memory, if it's more than 400 MB
			FD3D11GlobalStats::GTotalGraphicsMemory = ConsideredSharedSystemMemory;
		}
		else
		{
			// Otherwise consider 25% of total system memory for graphics.
			FD3D11GlobalStats::GTotalGraphicsMemory = TotalPhysicalMemory / 4ll;
		}

		if ( sizeof(SIZE_T) < 8 )
		{
			// Clamp to 1 GB if we're less than 64-bit
			FD3D11GlobalStats::GTotalGraphicsMemory = FMath::Min( FD3D11GlobalStats::GTotalGraphicsMemory, 1024ll * 1024ll * 1024ll );
		}

		if ( GPoolSizeVRAMPercentage > 0 )
		{
			float PoolSize = float(GPoolSizeVRAMPercentage) * 0.01f * float(FD3D11GlobalStats::GTotalGraphicsMemory);

			// Truncate GTexturePoolSize to MB (but still counted in bytes)
			GTexturePoolSize = int64(FGenericPlatformMath::TruncToFloat(PoolSize / 1024.0f / 1024.0f)) * 1024 * 1024;

			UE_LOG(LogRHI,Log,TEXT("Texture pool is %llu MB (%d%% of %llu MB)"),
				GTexturePoolSize / 1024 / 1024,
				GPoolSizeVRAMPercentage,
				FD3D11GlobalStats::GTotalGraphicsMemory / 1024 / 1024);
		}

		const bool bIsPerfHUD = !FCString::Stricmp(Adapter.DXGIAdapterDesc.Description,TEXT("NVIDIA PerfHUD"));

		if(bIsPerfHUD)
		{
			DriverType =  D3D_DRIVER_TYPE_REFERENCE;
		}

#ifdef AMD_AGS_API
		if (IsRHIDeviceAMD() && bAllowVendorDevice)
		{
			check(AmdAgsContext == nullptr);

			// agsInit should be called before D3D device creation
			if (agsInitialize(AGS_MAKE_VERSION(AMD_AGS_VERSION_MAJOR, AMD_AGS_VERSION_MINOR, AMD_AGS_VERSION_PATCH), nullptr, &AmdAgsContext, &AmdInfo.AmdGpuInfo) == AGS_SUCCESS)
			{
				AmdInfo.AmdAgsContext = AmdAgsContext;
				bool bFoundMatchingDevice = false;
				// Search the device list for a matching vendor ID and device ID marked as GCN
				for (int32 DeviceIndex = 0; DeviceIndex < AmdInfo.AmdGpuInfo.numDevices; DeviceIndex++)
				{
					const AGSDeviceInfo& DeviceInfo = AmdInfo.AmdGpuInfo.devices[DeviceIndex];
					GRHIDeviceIsAMDPreGCNArchitecture |= (Adapter.DXGIAdapterDesc.VendorId == DeviceInfo.vendorId) && (Adapter.DXGIAdapterDesc.DeviceId == DeviceInfo.deviceId) && (DeviceInfo.asicFamily == AGSDeviceInfo::AsicFamily_PreGCN);
					bFoundMatchingDevice |= (Adapter.DXGIAdapterDesc.VendorId == DeviceInfo.vendorId) && (Adapter.DXGIAdapterDesc.DeviceId == DeviceInfo.deviceId);
				}
				check(bFoundMatchingDevice);

				if (GRHIDeviceIsAMDPreGCNArchitecture)
				{
					UE_LOG(LogD3D11RHI, Log, TEXT("AMD Pre GCN architecture detected, some driver workarounds will be in place"));
				}
			}
			else
			{
				FMemory::Memzero(&AmdInfo, sizeof(AmdInfo));
				// If agsInit returns anything but AGS_SUCCESS, the context pointer should be
				// guaranteed to be NULL, but we'll set it here explicitly, just to be safe.
				AmdAgsContext = nullptr;
			}
		}
		else
		{
			FMemory::Memzero(&AmdInfo, sizeof(AmdInfo));
		}
#endif //AMD_AGS_API

		D3D_FEATURE_LEVEL ActualFeatureLevel = (D3D_FEATURE_LEVEL)0;

		if (IsRHIDeviceAMD() && CVarAMDUseMultiThreadedDevice.GetValueOnAnyThread())
		{
			DeviceFlags &= ~D3D11_CREATE_DEVICE_SINGLETHREADED;
		}

		bool bDeviceCreated = false;
#ifdef AMD_AGS_API
		if (IsRHIDeviceAMD() && AmdAgsContext && bAllowVendorDevice)
		{
			uint32 AmdSupportedExtensionFlags = 0;

			UE_LOG(LogD3D11RHI, Log, TEXT("Creating D3DDevice with AMD AGS, using adapter:"));
			LogDXGIAdapterDesc(Adapter.DXGIAdapterDesc);

			AGSDX11DeviceCreationParams DeviceCreationParams = 
			{
				Adapter.DXGIAdapter,
				DriverType,
				NULL,
				DeviceFlags,
				&FeatureLevel,
				1,
				D3D11_SDK_VERSION,
				nullptr
			};

			// Engine registration can be disabled via console var. Also disable automatically if ShaderDevelopmentMode is on.
			auto* CVarShaderDevelopmentMode = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ShaderDevelopmentMode"));
			auto* CVarDisableEngineAndAppRegistration = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DisableEngineAndAppRegistration"));
			const bool bDisableEngineRegistration = (CVarShaderDevelopmentMode && CVarShaderDevelopmentMode->GetValueOnAnyThread() != 0) || 
				(CVarDisableEngineAndAppRegistration && CVarDisableEngineAndAppRegistration->GetValueOnAnyThread() != 0);
			const bool bDisableAppRegistration = bDisableEngineRegistration || !FApp::HasProjectName();

			AGSDX11ExtensionParams AmdExtensionParams;
			FMemory::Memzero(&AmdExtensionParams, sizeof(AmdExtensionParams));
			// Set the reserved UAV slot - matching the other vendor extensions.
			AmdExtensionParams.uavSlot = 7;
			// Disable old-style, "automatic" alternate-frame rendering (AFR) MGPU driver behavior
			AmdExtensionParams.crossfireMode = AGS_CROSSFIRE_MODE_DISABLE;

			// Register the engine name with the AMD driver, e.g. "UnrealEngine4.19", unless disabled
			// (note: to specify nothing for pEngineName below, you need to pass an empty string, not a null pointer)
			FString EngineName = FApp::GetEpicProductIdentifier() + FEngineVersion::Current().ToString(EVersionComponent::Minor);
			AmdExtensionParams.pEngineName = bDisableEngineRegistration ? TEXT("") : *EngineName;
			AmdExtensionParams.engineVersion = AGS_UNSPECIFIED_VERSION;

			// Register the project name with the AMD driver, unless disabled or no project name
			// (note: to specify nothing for pAppName below, you need to pass an empty string, not a null pointer)
			AmdExtensionParams.pAppName = bDisableAppRegistration ? TEXT("") : FApp::GetProjectName();
			AmdExtensionParams.appVersion = AGS_UNSPECIFIED_VERSION;

			// agsDriverExtensionsDX11_CreateDevice will not check for the DriverStore if there is not already a D3D11 device
			// initialized. In order to use AMF we require libraries to also be loaded from the driver store so we temporarily
			// initialize a device here and destroy it once the agsDriverExtensionsDX11_CreateDevice is run.
			ID3D11Device* D3DDevicePreload = nullptr;
			ID3D11DeviceContext* D3DDeviceContextPreload = nullptr;

			int32 NumAllowedFeatureLevels = 1;
			D3D_FEATURE_LEVEL OutFeatureLevel = FeatureLevel;
			HRESULT Result = D3D11CreateDevice(
				Adapter.DXGIAdapter,
				D3D_DRIVER_TYPE_UNKNOWN,
				nullptr,
				DeviceFlags,
				&FeatureLevel,
				NumAllowedFeatureLevels,
				D3D11_SDK_VERSION,
				&D3DDevicePreload,
				&ActualFeatureLevel,
				&D3DDeviceContextPreload);
			if (FAILED(Result))
			{
				UE_LOG(LogD3D11RHI, Error, TEXT("Failed to load the AMD DriverStore library"));
			}

			AGSDX11ReturnedParams DeviceCreationReturnedParams;
			FMemory::Memzero(&DeviceCreationReturnedParams, sizeof(DeviceCreationReturnedParams));
			AGSReturnCode DeviceCreation =
				agsDriverExtensionsDX11_CreateDevice(
					AmdAgsContext,
					&DeviceCreationParams,
					&AmdExtensionParams,
					&DeviceCreationReturnedParams);

			// Destroy temporary device and context			
			D3DDevicePreload->Release();
			D3DDevicePreload = nullptr;

			D3DDeviceContextPreload->Release();
			D3DDeviceContextPreload = nullptr;

			if (DeviceCreation == AGS_SUCCESS)
			{
				Direct3DDevice = DeviceCreationReturnedParams.pDevice;
				ActualFeatureLevel = DeviceCreationReturnedParams.featureLevel;
				Direct3DDeviceIMContext = DeviceCreationReturnedParams.pImmediateContext;
				AmdSupportedExtensionFlags = *(uint32*)&DeviceCreationReturnedParams.extensionsSupported;
				bDeviceCreated = true;
				UE_LOG(LogD3D11RHI, Log, TEXT("Created device via AGS, feature level %s, supported extensions %x."), GetFeatureLevelString(ActualFeatureLevel), AmdSupportedExtensionFlags);
			}
			else
			{
				agsDeInitialize(AmdAgsContext);
				AmdAgsContext = nullptr;
				AmdSupportedExtensionFlags = 0;
				FMemory::Memzero(&AmdInfo, sizeof(AmdInfo));
				GRHIDeviceIsAMDPreGCNArchitecture = false;
				UE_LOG(LogD3D11RHI, Warning, TEXT("Failed to create device via AGS, code %d."), DeviceCreation);
			}

			GRHISupportsAtomicUInt64 = DeviceCreationReturnedParams.extensionsSupported.intrinsics19 != 0;  // "intrinsics19" includes AtomicU64
			GSupportsDepthBoundsTest = DeviceCreationReturnedParams.extensionsSupported.depthBoundsTest != 0;
		}
#endif //AMD_AGS_API

#if INTEL_METRICSDISCOVERY
		if (IsRHIDeviceIntel() && bAllowVendorDevice)
		{
			// Needs to be done before device creation
			CreateIntelMetricsDiscovery();
		}
#endif
		if (IsRHIDeviceNVIDIA())
		{
			// crash dump hooks need to be attached before device creation
			ENABLE_NV_AFTERMATH_CRASH_DUMPS();
		}

		if (!bDeviceCreated)
		{
			UE_LOG(LogD3D11RHI, Log, TEXT("Creating D3DDevice using adapter:"));
			LogDXGIAdapterDesc(Adapter.DXGIAdapterDesc);

			// Creating the Direct3D device.
			VERIFYD3D11RESULT(D3D11CreateDevice(
				Adapter.DXGIAdapter,
				DriverType,
				NULL,
				DeviceFlags,
				&FeatureLevel,
				1,
				D3D11_SDK_VERSION,
				Direct3DDevice.GetInitReference(),
				&ActualFeatureLevel,
				Direct3DDeviceIMContext.GetInitReference()
			));
		}

		// We should get the feature level we asked for as earlier we checked to ensure it is supported.
		check(ActualFeatureLevel == FeatureLevel);

		GRHIPersistentThreadGroupCount = 1440; // TODO: Revisit based on vendor/adapter/perf query

		StateCache.Init(Direct3DDeviceIMContext);

#if (UE_BUILD_SHIPPING && WITH_EDITOR) && PLATFORM_WINDOWS && !PLATFORM_64BITS
		// Disable PIX for windows in the shipping editor builds
		D3DPERF_SetOptions(1);
#endif

		// Check for async texture creation support.
		D3D11_FEATURE_DATA_THREADING ThreadingSupport = {0};
		VERIFYD3D11RESULT_EX(Direct3DDevice->CheckFeatureSupport(D3D11_FEATURE_THREADING, &ThreadingSupport, sizeof(ThreadingSupport)), Direct3DDevice);
		GRHISupportsAsyncTextureCreation = !!ThreadingSupport.DriverConcurrentCreates
			&& (DeviceFlags & D3D11_CREATE_DEVICE_SINGLETHREADED) == 0;

		if (IsRHIDeviceAMD() && CVarAMDDisableAsyncTextureCreation.GetValueOnAnyThread())
		{
			GRHISupportsAsyncTextureCreation = false;
		}

		GRHISupportsMultithreadedResources = GRHISupportsAsyncTextureCreation;

#ifdef NVAPI_INTERFACE

		if (IsRHIDeviceNVIDIA() && bAllowVendorDevice)
		{
			NvAPI_Status NvStatus;
			NvStatus = NvAPI_Initialize();
			if (NvStatus == NVAPI_OK)
			{
				NvStatus = NvAPI_D3D11_IsNvShaderExtnOpCodeSupported(Direct3DDevice, NV_EXTN_OP_UINT64_ATOMIC, &GRHISupportsAtomicUInt64);
				if (NvStatus != NVAPI_OK)
				{
					UE_LOG(LogD3D11RHI, Warning, TEXT("Failed to query support for 64 bit atomics"));
				}
			}
			else
			{
				UE_LOG(LogD3D11RHI, Warning, TEXT("Failed to initialize NVAPI"));
			}
		}

		if( IsRHIDeviceNVIDIA() && CVarNVidiaTimestampWorkaround.GetValueOnAnyThread() )
		{
			// Workaround for pre-maxwell TDRs with realtime GPU stats (timestamp queries)
			// Note: Since there is no direct check for Kepler hardware and beyond, check for SHFL instruction
			bool bNVSHFLSupported = false;
			if (NvAPI_D3D11_IsNvShaderExtnOpCodeSupported(Direct3DDevice, NV_EXTN_OP_SHFL, &bNVSHFLSupported) == NVAPI_OK && !bNVSHFLSupported)
			{
				UE_LOG(LogD3D11RHI, Display, TEXT("Timestamp queries are currently disabled on this hardware due to instability. Realtime GPU stats will not be available. You can override this behaviour by setting r.NVIDIATimestampWorkaround to 0"));
				GSupportsTimestampRenderQueries = false;
			}
		}
#endif //NVAPI_INTERFACE

		CACHE_NV_AFTERMATH_ENABLED();

#if PLATFORM_WINDOWS
		IUnknown* RenderDoc;
		IID RenderDocID;
		if (SUCCEEDED(IIDFromString(L"{A7AA6116-9C8D-4BBA-9083-B4D816B71B78}", &RenderDocID)))
		{
			if (SUCCEEDED(Direct3DDevice->QueryInterface(RenderDocID, (void**)(&RenderDoc))))
			{
				bRenderDoc = true;

				// Running under RenderDoc, so enable capturing mode
				GDynamicRHI->EnableIdealGPUCaptureOptions(true);
			}
		}

		IUnknown* IntelGPA;
		static const IID IntelGPAID = { 0xCCFFEF16, 0x7B69, 0x468F, {0xBC, 0xE3, 0xCD, 0x95, 0x33, 0x69, 0xA3, 0x9A} };

		if (SUCCEEDED(Direct3DDevice->QueryInterface(IntelGPAID, (void**)(&IntelGPA))))
		{
			// Running under Intel GPA, so enable capturing mode
			GDynamicRHI->EnableIdealGPUCaptureOptions(true);
		}
#endif

		if (IsRHIDeviceNVIDIA())
		{
			GSupportsDepthBoundsTest = true;
			if (!bRenderDoc)
			{
				START_NV_AFTERMATH();
			}
		}

#if INTEL_EXTENSIONS
		if (IsRHIDeviceIntel() && bAllowVendorDevice)
		{
			StartIntelExtensions();
		}
#endif // INTEL_EXTENSIONS

		if (GRHISupportsAtomicUInt64)
		{
			UE_LOG(LogD3D11RHI, Log, TEXT("RHI has support for 64 bit atomics"));
		}
		else
		{
			UE_LOG(LogD3D11RHI, Log, TEXT("RHI does not have support for 64 bit atomics"));
		}

#if INTEL_METRICSDISCOVERY
		if (IsRHIDeviceIntel() && bAllowVendorDevice)
		{
			StartIntelMetricsDiscovery();

			if (GDX11IntelMetricsDiscoveryEnabled)
			{
				GRHISupportsDynamicResolution = true;
				GRHISupportsFrameCyclesBubblesRemoval = true;
			}
		}
#endif // INTEL_METRICSDISCOVERY

		// Disable the RHI thread by default for devices that will likely suffer in performance
		if (Adapter.bIsIntegrated || FPlatformMisc::NumberOfCores() < 4)
		{
			GRHISupportsRHIThread = false;
		}

		SetupAfterDeviceCreation();

#if !(UE_BUILD_SHIPPING && WITH_EDITOR)
		// Add some filter outs for known debug spew messages (that we don't care about)
		if(DeviceFlags & D3D11_CREATE_DEVICE_DEBUG)
		{
			TRefCountPtr<ID3D11InfoQueue> InfoQueue;
			VERIFYD3D11RESULT_EX(Direct3DDevice->QueryInterface( IID_ID3D11InfoQueue, (void**)InfoQueue.GetInitReference()), Direct3DDevice);
			if (InfoQueue)
			{
				D3D11_INFO_QUEUE_FILTER NewFilter;
				FMemory::Memzero(&NewFilter,sizeof(NewFilter));

				// Turn off info msgs as these get really spewy
				D3D11_MESSAGE_SEVERITY DenySeverity = D3D11_MESSAGE_SEVERITY_INFO;
				NewFilter.DenyList.NumSeverities = 1;
				NewFilter.DenyList.pSeverityList = &DenySeverity;

				// Be sure to carefully comment the reason for any additions here!  Someone should be able to look at it later and get an idea of whether it is still necessary.
				D3D11_MESSAGE_ID DenyIds[]  = {
					// OMSETRENDERTARGETS_INVALIDVIEW - d3d will complain if depth and color targets don't have the exact same dimensions, but actually
					//	if the color target is smaller then things are ok.  So turn off this error.  There is a manual check in FD3D11DynamicRHI::SetRenderTarget
					//	that tests for depth smaller than color and MSAA settings to match.
					D3D11_MESSAGE_ID_OMSETRENDERTARGETS_INVALIDVIEW, 

					// QUERY_BEGIN_ABANDONING_PREVIOUS_RESULTS - The RHI exposes the interface to make and issue queries and a separate interface to use that data.
					//		Currently there is a situation where queries are issued and the results may be ignored on purpose.  Filtering out this message so it doesn't
					//		swarm the debug spew and mask other important warnings
					D3D11_MESSAGE_ID_QUERY_BEGIN_ABANDONING_PREVIOUS_RESULTS,
					D3D11_MESSAGE_ID_QUERY_END_ABANDONING_PREVIOUS_RESULTS,

					// D3D11_MESSAGE_ID_CREATEINPUTLAYOUT_EMPTY_LAYOUT - This is a warning that gets triggered if you use a null vertex declaration,
					//       which we want to do when the vertex shader is generating vertices based on ID.
					D3D11_MESSAGE_ID_CREATEINPUTLAYOUT_EMPTY_LAYOUT,

					// D3D11_MESSAGE_ID_DEVICE_DRAW_INDEX_BUFFER_TOO_SMALL - This warning gets triggered by Slate draws which are actually using a valid index range.
					//		The invalid warning seems to only happen when VS 2012 is installed.  Reported to MS.  
					//		There is now an assert in DrawIndexedPrimitive to catch any valid errors reading from the index buffer outside of range.
					D3D11_MESSAGE_ID_DEVICE_DRAW_INDEX_BUFFER_TOO_SMALL,

					// D3D11_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET - This warning gets triggered by shadow depth rendering because the shader outputs
					//		a color but we don't bind a color render target. That is safe as writes to unbound render targets are discarded.
					//		Also, batched elements triggers it when rendering outside of scene rendering as it outputs to the GBuffer containing normals which is not bound.
					(D3D11_MESSAGE_ID)3146081, // D3D11_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET,

					// Spams constantly as we change the debug name on rendertargets that get reused.
					D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS, 
				};

				NewFilter.DenyList.NumIDs = sizeof(DenyIds)/sizeof(D3D11_MESSAGE_ID);
				NewFilter.DenyList.pIDList = (D3D11_MESSAGE_ID*)&DenyIds;

				InfoQueue->PushStorageFilter(&NewFilter);

				// Break on D3D debug errors.
				InfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR,true);

				// Enable this to break on a specific id in order to quickly get a callstack
				//InfoQueue->SetBreakOnID(D3D11_MESSAGE_ID_DEVICE_DRAW_CONSTANT_BUFFER_TOO_SMALL, true);

				if (FParse::Param(FCommandLine::Get(),TEXT("d3dbreakonwarning")))
				{
					InfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_WARNING,true);
				}
			}
		}
#endif
		
		{
			GRHISupportsHDROutput = SetupDisplayHDRMetaData();
		}

		// Add device overclock state to crash context
		const bool bIsGPUOverclocked = IsDeviceOverclocked();
		FGenericCrashContext::SetEngineData(TEXT("RHI.IsGPUOverclocked"), bIsGPUOverclocked ? TEXT("true") : TEXT("false"));
		
		FHardwareInfo::RegisterHardwareInfo( NAME_RHI, TEXT( "D3D11" ) );

		GRHISupportsTextureStreaming = true;
		GRHISupportsFirstInstance = true;
		GRHINeedsExtraDeletionLatency = false;
		GRHISupportsEfficientUploadOnResourceCreation = true;

		GRHICommandList.GetImmediateCommandList().InitializeImmediateContexts();

		// Now that the driver extensions have been initialized, turn on UAV overlap for the first time.
		EnableUAVOverlap();

		FRenderResource::InitPreRHIResources();
		GIsRHIInitialized = true;
	}
}

void FD3D11DynamicRHI::RHIPerFrameRHIFlushComplete()
{
	RHIPollRenderQueryResults();

	extern void D3D11RHIQueryBatcherPerFrameCleanup();

	D3D11RHIQueryBatcherPerFrameCleanup();

#if NV_AFTERMATH
	if (GDX11NVAfterMathEnabled)
	{
		static auto* CVarGPUCrashCollectionEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.gpucrash.collectionenable"));
		bool bGPUCrashCollectionEnabled = CVarGPUCrashCollectionEnabled ? CVarGPUCrashCollectionEnabled->GetValueOnRenderThread() != 0 : false;

		if (NVAftermathIMContextHandle && !bGPUCrashCollectionEnabled)
		{
			StopNVAftermath();
		}
		else if (!NVAftermathIMContextHandle && bGPUCrashCollectionEnabled)
		{
			StartNVAftermath();
		}
	}
#endif

	for (int32 Frequency = 0; Frequency < SF_NumStandardFrequencies; ++Frequency)
	{
		DirtyUniformBuffers[Frequency] = 0;

		for (int32 BindIndex = 0; BindIndex < MAX_UNIFORM_BUFFERS_PER_SHADER_STAGE; ++BindIndex)
		{
			BoundUniformBuffers[Frequency][BindIndex] = nullptr;
		}
	}
}

/**
 *	Retrieve available screen resolutions.
 *
 *	@param	Resolutions			TArray<FScreenResolutionRHI> parameter that will be filled in.
 *	@param	bIgnoreRefreshRate	If true, ignore refresh rates.
 *
 *	@return	bool				true if successfully filled the array
 */
bool FD3D11DynamicRHI::RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
	int32 MinAllowableResolutionX = 0;
	int32 MinAllowableResolutionY = 0;
	int32 MaxAllowableResolutionX = 10480;
	int32 MaxAllowableResolutionY = 10480;
	int32 MinAllowableRefreshRate = 0;
	int32 MaxAllowableRefreshRate = 10480;

	if (MaxAllowableResolutionX == 0) //-V547
	{
		MaxAllowableResolutionX = 10480;
	}
	if (MaxAllowableResolutionY == 0) //-V547
	{
		MaxAllowableResolutionY = 10480;
	}
	if (MaxAllowableRefreshRate == 0) //-V547
	{
		MaxAllowableRefreshRate = 10480;
	}

	HRESULT HResult = S_OK;

	int32 CurrentOutput = 0;
	do 
	{
		TRefCountPtr<IDXGIOutput> Output;
		HResult = Adapter.DXGIAdapter->EnumOutputs(CurrentOutput,Output.GetInitReference());
		if(DXGI_ERROR_NOT_FOUND == HResult)
			break;
		if(FAILED(HResult))
			return false;

		// TODO: GetDisplayModeList is a terribly SLOW call.  It can take up to a second per invocation.
		//  We might want to work around some DXGI badness here.
		const DXGI_FORMAT DisplayFormats[] = {DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM};
		DXGI_FORMAT Format = DisplayFormats[0];
		bool bFoundValidResult = false;
		uint32 NumModes = 0;	

		for (DXGI_FORMAT CurrentFormat : DisplayFormats)
		{
			HResult = Output->GetDisplayModeList(CurrentFormat, 0, &NumModes, NULL);

			if(FAILED(HResult))
			{
				if (HResult == DXGI_ERROR_NOT_FOUND)
				{
					UE_LOG(LogD3D11RHI, Warning, TEXT("RHIGetAvailableResolutions failed with generic error."));
					continue;
				}
				else if (HResult == DXGI_ERROR_MORE_DATA)
				{
					UE_LOG(LogD3D11RHI, Warning, TEXT("RHIGetAvailableResolutions failed trying to return too much data."));
					continue;
				}
				else if (HResult == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
				{
					UE_LOG(LogD3D11RHI, Warning, TEXT("RHIGetAvailableResolutions does not return results when running under remote desktop."));
					return false;
				}
				else
				{
					UE_LOG(LogD3D11RHI, Warning, TEXT("RHIGetAvailableResolutions failed with unknown error (0x%x)."), HResult);
					return false;
				}
			}
			else
			{
				bFoundValidResult = true;
				if (NumModes)
				{
					Format = CurrentFormat;
					break;
				}
			}
		}

		// If we couldn't iterate any formats for this output, it might be dead in the driver and there's nothing we can do here except move on.
		if (!bFoundValidResult)
		{
			// Grab diagnostic information from the output, if at all possible.
			DXGI_OUTPUT_DESC OutputDesc;
			FMemory::Memzero(OutputDesc);
			HRESULT OutputDescRes = Output->GetDesc(&OutputDesc);

			UE_LOG(LogD3D11RHI, Warning,
				TEXT("RHIGetAvailableResolutions could not get any display modes from output %i (D:%i) (Res:'%s'(0x%08X)"),
				CurrentOutput,
				OutputDesc.AttachedToDesktop,
				*GetD3D11ErrorString(OutputDescRes, GetDevice()), OutputDescRes);

			++CurrentOutput;
			continue;
		}

		// It's still invalid to "succeed" and be given no modes.
		checkf(NumModes > 0, TEXT("No display modes found for DXGI_FORMAT_R8G8B8A8_UNORM or DXGI_FORMAT_B8G8R8A8_UNORM formats!"));

		DXGI_MODE_DESC* ModeList = new DXGI_MODE_DESC[ NumModes ];
		VERIFYD3D11RESULT(Output->GetDisplayModeList(Format, 0, &NumModes, ModeList));

		for(uint32 m = 0;m < NumModes;m++)
		{
			CA_SUPPRESS(6385);
			if (((int32)ModeList[m].Width >= MinAllowableResolutionX) &&
				((int32)ModeList[m].Width <= MaxAllowableResolutionX) &&
				((int32)ModeList[m].Height >= MinAllowableResolutionY) &&
				((int32)ModeList[m].Height <= MaxAllowableResolutionY)
				)
			{
				bool bAddIt = true;
				if (bIgnoreRefreshRate == false)
				{
					if (((int32)ModeList[m].RefreshRate.Numerator < MinAllowableRefreshRate * ModeList[m].RefreshRate.Denominator) ||
						((int32)ModeList[m].RefreshRate.Numerator > MaxAllowableRefreshRate * ModeList[m].RefreshRate.Denominator)
						)
					{
						continue;
					}
				}
				else
				{
					// See if it is in the list already
					for (int32 CheckIndex = 0; CheckIndex < Resolutions.Num(); CheckIndex++)
					{
						FScreenResolutionRHI& CheckResolution = Resolutions[CheckIndex];
						if ((CheckResolution.Width == ModeList[m].Width) &&
							(CheckResolution.Height == ModeList[m].Height))
						{
							// Already in the list...
							bAddIt = false;
							break;
						}
					}
				}

				if (bAddIt)
				{
					// Add the mode to the list
					int32 Temp2Index = Resolutions.AddZeroed();
					FScreenResolutionRHI& ScreenResolution = Resolutions[Temp2Index];

					ScreenResolution.Width = ModeList[m].Width;
					ScreenResolution.Height = ModeList[m].Height;
					ScreenResolution.RefreshRate = ModeList[m].RefreshRate.Numerator / ModeList[m].RefreshRate.Denominator;
				}
			}
		}

		delete[] ModeList;

		++CurrentOutput;

	// TODO: Cap at 1 for default output
	} while(CurrentOutput < 1); //-V654

	return true;
}
