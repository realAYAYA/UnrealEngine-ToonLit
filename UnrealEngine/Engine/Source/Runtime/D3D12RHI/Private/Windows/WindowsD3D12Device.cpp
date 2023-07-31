// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WindowsD3D12Device.cpp: Windows D3D device RHI implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "WindowsD3D12Adapter.h"
#include "Modules/ModuleManager.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsPlatformCrashContext.h"
#include <delayimp.h>

#if !PLATFORM_HOLOLENS && !PLATFORM_CPU_ARM_FAMILY
	#include "amd_ags.h"
	#define AMD_API_ENABLE 1
#else
	#define AMD_API_ENABLE 0
#endif

#if !PLATFORM_HOLOLENS && !PLATFORM_CPU_ARM_FAMILY
	#define NV_API_ENABLE 1
	#include "nvapi.h"
	#include "nvShaderExtnEnums.h"
#else
	#define NV_API_ENABLE 0
#endif

#if INTEL_EXTENSIONS
	#define INTC_IGDEXT_D3D12 1

	THIRD_PARTY_INCLUDES_START
	#include "igdext.h"
	THIRD_PARTY_INCLUDES_END
#endif

#include "Windows/HideWindowsPlatformTypes.h"

#include "HardwareInfo.h"
#include "IHeadMountedDisplayModule.h"
#include "GenericPlatform/GenericPlatformDriver.h"			// FGPUDriverInfo
#include "RHIValidation.h"

#include "ShaderCompiler.h"

#pragma comment(lib, "d3d12.lib")

IMPLEMENT_MODULE(FD3D12DynamicRHIModule, D3D12RHI);

extern bool D3D12RHI_ShouldCreateWithD3DDebug();
extern bool D3D12RHI_ShouldCreateWithWarp();
extern bool D3D12RHI_AllowSoftwareFallback();
extern bool D3D12RHI_ShouldAllowAsyncResourceCreation();
extern bool D3D12RHI_ShouldForceCompatibility();

FD3D12DynamicRHI* GD3D12RHI = nullptr;

#if NV_AFTERMATH

	bool GDX12NVAfterMathModuleLoaded = false;

	// Disabled by default since introduces stalls between render and driver threads
	int32 GDX12NVAfterMathEnabled = 0;
	static FAutoConsoleVariableRef CVarDX12NVAfterMathBufferSize(
	TEXT("r.DX12NVAfterMathEnabled"),
	GDX12NVAfterMathEnabled,
	TEXT("Use NV Aftermath for GPU crash analysis in D3D12"),
	ECVF_ReadOnly
	);

	int32 GDX12NVAfterMathTrackResources = 0;
	static FAutoConsoleVariableRef CVarDX12NVAfterMathTrackResources(
	TEXT("r.DX12NVAfterMathTrackResources"),
	GDX12NVAfterMathTrackResources,
	TEXT("Enable NV Aftermath resource tracing in D3D12"),
	ECVF_ReadOnly
	);

	int32 GDX12NVAfterMathMarkers = 0;

#endif // NV_AFTERMATH

int32 GMinimumWindowsBuildVersionForRayTracing = 0;
static FAutoConsoleVariableRef CVarMinBuildVersionForRayTracing(
	TEXT("r.D3D12.DXR.MinimumWindowsBuildVersion"),
	GMinimumWindowsBuildVersionForRayTracing,
	TEXT("Sets the minimum Windows build version required to enable ray tracing."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

int32 GMinimumDriverVersionForRayTracingNVIDIA = 0;
static FAutoConsoleVariableRef CVarMinDriverVersionForRayTracingNVIDIA(
	TEXT("r.D3D12.DXR.MinimumDriverVersionNVIDIA"),
	GMinimumDriverVersionForRayTracingNVIDIA,
	TEXT("Sets the minimum driver version required to enable ray tracing on NVIDIA GPUs."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

#define DXR_ALLOW_EMULATED_RAYTRACING 0

#if DXR_ALLOW_EMULATED_RAYTRACING
int32 GAllowEmulatedRayTracing = 0;
static FAutoConsoleVariableRef CVarAllowEmulatedRayTracing(
	TEXT("r.D3D12.DXR.AllowEmulatedRayTracing"),
	GAllowEmulatedRayTracing,
	TEXT("Allows ray tracing emulation support on NVIDIA cards with the Pascal architecture (default=0)."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);
#endif

// Use AGS_MAKE_VERSION() macro to define the version.
// i.e. AGS_MAKE_VERSION(major, minor, patch) ((major << 22) | (minor << 12) | patch)
int32 GMinimumDriverVersionForRayTracingAMD = 0;
static FAutoConsoleVariableRef CVarMinDriverVersionForRayTracingAMD(
	TEXT("r.D3D12.DXR.MinimumDriverVersionAMD"),
	GMinimumDriverVersionForRayTracingAMD,
	TEXT("Sets the minimum driver version required to enable ray tracing on AMD GPUs."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

int32 GAllowAsyncCompute = 1;
static FAutoConsoleVariableRef CVarAllowAsyncCompute(
	TEXT("r.D3D12.AllowAsyncCompute"),
	GAllowAsyncCompute,
	TEXT("Allow usage of async compute"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarExperimentalShaderModels(
	TEXT("r.D3D12.ExperimentalShaderModels"),
	0,
	TEXT("Controls whether D3D12 experimental shader models should be allowed. Not available in shipping builds. (default = 0)."),
	ECVF_ReadOnly
);
#endif // !UE_BUILD_SHIPPING

#if D3D12RHI_SUPPORTS_WIN_PIX
int32 GAutoAttachPIX = 0;
static FAutoConsoleVariableRef CVarAutoAttachPIX(
	TEXT("r.D3D12.AutoAttachPIX"),
	GAutoAttachPIX,
	TEXT("Automatically attach PIX on startup"),
	ECVF_ReadOnly | ECVF_RenderThreadSafe
);
#endif // D3D12RHI_SUPPORTS_WIN_PIX

static inline int D3D12RHI_PreferAdapterVendor()
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

	return -1;
}

using namespace D3D12RHI;

static bool bIsQuadBufferStereoEnabled = false;

/** This function is used as a SEH filter to catch only delay load exceptions. */
static bool IsDelayLoadException(PEXCEPTION_POINTERS ExceptionPointers)
{
	switch (ExceptionPointers->ExceptionRecord->ExceptionCode)
	{
	case VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND):
	case VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND):
		return EXCEPTION_EXECUTE_HANDLER;
	default:
		return EXCEPTION_CONTINUE_SEARCH;
	}
}

/**
 * Since CreateDXGIFactory is a delay loaded import from the DXGI DLL, if the user
 * doesn't have Vista/DX10, calling CreateDXGIFactory will throw an exception.
 * We use SEH to detect that case and fail gracefully.
 */
static void SafeCreateDXGIFactory(IDXGIFactory4** DXGIFactory)
{
#if !defined(D3D12_CUSTOM_VIEWPORT_CONSTRUCTOR) || !D3D12_CUSTOM_VIEWPORT_CONSTRUCTOR
	__try
	{
		bIsQuadBufferStereoEnabled = FParse::Param(FCommandLine::Get(), TEXT("quad_buffer_stereo"));

#if PLATFORM_HOLOLENS
		CreateDXGIFactory1(__uuidof(IDXGIFactory4), (void**)DXGIFactory);
#else
		CreateDXGIFactory(__uuidof(IDXGIFactory4), (void**)DXGIFactory);
#endif
	}
	__except (IsDelayLoadException(GetExceptionInformation()))
	{
		// We suppress warning C6322: Empty _except block. Appropriate checks are made upon returning. 
		CA_SUPPRESS(6322);
	}
#endif	//!D3D12_CUSTOM_VIEWPORT_CONSTRUCTOR
}

/**
 * Returns the minimum D3D feature level required to create based on
 * command line parameters.
 */
static D3D_FEATURE_LEVEL GetRequiredD3DFeatureLevel()
{
	return D3D_FEATURE_LEVEL_11_0;
}

static D3D_FEATURE_LEVEL FindHighestFeatureLevel(ID3D12Device* Device, D3D_FEATURE_LEVEL MinFeatureLevel)
{
	const D3D_FEATURE_LEVEL FeatureLevels[] =
	{
		// Add new feature levels that the app supports here.
#if D3D12_CORE_ENABLED
		D3D_FEATURE_LEVEL_12_2,
#endif
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};

	// Determine the max feature level supported by the driver and hardware.
	D3D12_FEATURE_DATA_FEATURE_LEVELS FeatureLevelCaps{};
	FeatureLevelCaps.pFeatureLevelsRequested = FeatureLevels;
	FeatureLevelCaps.NumFeatureLevels = UE_ARRAY_COUNT(FeatureLevels);

	if (SUCCEEDED(Device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &FeatureLevelCaps, sizeof(FeatureLevelCaps))))
	{
		return FeatureLevelCaps.MaxSupportedFeatureLevel;
	}

	return MinFeatureLevel;
}

static D3D_SHADER_MODEL FindHighestShaderModel(ID3D12Device* Device)
{
	// Because we can't guarantee older Windows versions will know about newer shader models, we need to check them all
	// in descending order and return the first result that succeeds.
	const D3D_SHADER_MODEL ShaderModelsToCheck[] =
	{
#if D3D12_CORE_ENABLED
		D3D_SHADER_MODEL_6_7,
		D3D_SHADER_MODEL_6_6,
#endif
		D3D_SHADER_MODEL_6_5,
		D3D_SHADER_MODEL_6_4,
		D3D_SHADER_MODEL_6_3,
		D3D_SHADER_MODEL_6_2,
		D3D_SHADER_MODEL_6_1,
		D3D_SHADER_MODEL_6_0,
	};

	D3D12_FEATURE_DATA_SHADER_MODEL FeatureShaderModel{};
	for (const D3D_SHADER_MODEL ShaderModelToCheck : ShaderModelsToCheck)
	{
		FeatureShaderModel.HighestShaderModel = ShaderModelToCheck;

		if (SUCCEEDED(Device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &FeatureShaderModel, sizeof(FeatureShaderModel))))
		{
			return FeatureShaderModel.HighestShaderModel;
		}
	}

	// Last ditch effort, the minimum requirement for DX12 is 5.1
	return D3D_SHADER_MODEL_5_1;
}

#if INTEL_EXTENSIONS
static void DestroyIntelExtensionsContext(INTCExtensionContext* IntelExtensionContext)
{
	if (IntelExtensionContext)
	{
		const HRESULT hr = INTC_DestroyDeviceExtensionContext(&IntelExtensionContext);

		if (hr == S_OK)
		{
			UE_LOG(LogD3D12RHI, Log, TEXT("Intel Extensions Framework unloaded"));
		}
		else if (hr == E_INVALIDARG)
		{
			UE_LOG(LogD3D12RHI, Log, TEXT("Intel Extensions Framework error when unloading"));
		}
	}
}

static INTCExtensionContext* CreateIntelExtensionsContext(ID3D12Device* Device, INTCExtensionInfo& INTCExtensionInfo)
{
	const INTCExtensionVersion AtomicsRequiredVersion = { 4, 7, 0 };

	if (FAILED(INTC_LoadExtensionsLibrary(false)))
	{
		UE_LOG(LogD3D12RHI, Log, TEXT("Failed to load Intel Extensions Library"));
	}

	INTCExtensionVersion* SupportedExtensionsVersions = nullptr;
	uint32_t SupportedExtensionsVersionCount = 0;
	if (SUCCEEDED(INTC_D3D12_GetSupportedVersions(Device, nullptr, &SupportedExtensionsVersionCount)))
	{
		SupportedExtensionsVersions = new INTCExtensionVersion[SupportedExtensionsVersionCount]{};
	}

	if (SUCCEEDED(INTC_D3D12_GetSupportedVersions(Device, SupportedExtensionsVersions, &SupportedExtensionsVersionCount)) && SupportedExtensionsVersions != nullptr)
	{
		for (uint32_t i = 0; i < SupportedExtensionsVersionCount; i++)
		{
			CA_SUPPRESS(6385);
			if ((SupportedExtensionsVersions[i].HWFeatureLevel >= AtomicsRequiredVersion.HWFeatureLevel) &&
				(SupportedExtensionsVersions[i].APIVersion >= AtomicsRequiredVersion.APIVersion) &&
				(SupportedExtensionsVersions[i].Revision >= AtomicsRequiredVersion.Revision))
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("Intel Extensions loaded requested version: %u.%u.%u"),
					SupportedExtensionsVersions[i].HWFeatureLevel,
					SupportedExtensionsVersions[i].APIVersion,
					SupportedExtensionsVersions[i].Revision);

				INTCExtensionInfo.RequestedExtensionVersion = SupportedExtensionsVersions[i];
				break;
			}
		}
	}

	INTCExtensionContext* IntelExtensionContext = nullptr;
	INTCExtensionAppInfo AppInfo{};
	AppInfo.pEngineName = TEXT("Unreal Engine");
	AppInfo.EngineVersion = 5;

	const HRESULT hr = INTC_D3D12_CreateDeviceExtensionContext(Device, &IntelExtensionContext, &INTCExtensionInfo, &AppInfo);

	if (SUCCEEDED(hr))
	{
		UE_LOG(LogD3D12RHI, Log, TEXT("Intel Extensions Framework enabled"));
	}
	else
	{
		if (hr == E_OUTOFMEMORY)
		{
			UE_LOG(LogD3D12RHI, Log, TEXT("Intel Extensions Framework not supported by driver"));
		}
		else if (hr == E_INVALIDARG)
		{
			UE_LOG(LogD3D12RHI, Log, TEXT("Intel Extensions Framework passed invalid creation arguments"));
		}

		if (IntelExtensionContext)
		{
			DestroyIntelExtensionsContext(IntelExtensionContext);
			IntelExtensionContext = nullptr;
		}
	}

	if (SupportedExtensionsVersions != nullptr)
	{
		delete[] SupportedExtensionsVersions;
	}

	return IntelExtensionContext;
}

static bool EnableIntelAtomic64Support(INTCExtensionContext* IntelExtensionContext, INTCExtensionInfo& INTCExtensionInfo)
{
	bool bEmulatedAtomic64Support = false;

	if (IntelExtensionContext)
	{
#if D3D12_CORE_ENABLED
		// only enabled Atomic64 emulation for selected platforms, explicitly enable emulation for the adapter, will affect all future device creations
		if (INTCExtensionInfo.IntelDeviceInfo.GTGeneration == 1270 /*IGFX_DG2*/)
		{
			INTC_D3D12_FEATURE INTCFeature;
			INTCFeature.EmulatedTyped64bitAtomics = true;

			const HRESULT hr = INTC_D3D12_SetFeatureSupport(IntelExtensionContext, &INTCFeature);
			if (SUCCEEDED(hr))
			{
				bEmulatedAtomic64Support = true;
				UE_LOG(LogD3D12RHI, Log, TEXT("Intel Extensions 64-bit Typed Atomics emulation enabled."));
			}
			else
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("Failed to enable Intel Extensions 64-bit Typed Atomics emulation."));
			}
		}
#endif
	}

	return bEmulatedAtomic64Support;
}
#endif

static bool CheckDeviceForEmulatedAtomic64Support(IDXGIAdapter* Adapter, ID3D12Device* Device)
{
	bool bEmulatedAtomic64Support = false;

#if INTEL_EXTENSIONS
	DXGI_ADAPTER_DESC AdapterDesc{};
	Adapter->GetDesc(&AdapterDesc);

	if (AdapterDesc.VendorId == 0x8086 && !FParse::Param(FCommandLine::Get(), TEXT("novendordevice")))
	{
		INTCExtensionInfo INTCExtensionInfo{};
		if (INTCExtensionContext* IntelExtensionContext = CreateIntelExtensionsContext(Device, INTCExtensionInfo))
		{
			bEmulatedAtomic64Support = EnableIntelAtomic64Support(IntelExtensionContext, INTCExtensionInfo);

			DestroyIntelExtensionsContext(IntelExtensionContext);
		}
	}
#endif

	return bEmulatedAtomic64Support;
}

inline bool ShouldCheckBindlessSupport(EShaderPlatform ShaderPlatform)
{
	return RHIGetBindlessResourcesConfiguration(ShaderPlatform) != ERHIBindlessConfiguration::Disabled
		|| RHIGetBindlessSamplersConfiguration(ShaderPlatform) != ERHIBindlessConfiguration::Disabled;
}

inline ERHIFeatureLevel::Type FindMaxRHIFeatureLevel(IDXGIAdapter* Adapter, ID3D12Device* Device, D3D_FEATURE_LEVEL InMaxFeatureLevel, D3D_SHADER_MODEL InMaxShaderModel, D3D12_RESOURCE_BINDING_TIER ResourceBindingTier)
{
	ERHIFeatureLevel::Type MaxRHIFeatureLevel = ERHIFeatureLevel::Num;

	if (InMaxFeatureLevel >= D3D_FEATURE_LEVEL_12_0 && InMaxShaderModel >= D3D_SHADER_MODEL_6_6)
	{
		D3D12_FEATURE_DATA_D3D12_OPTIONS1 D3D12Caps1{};
		Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &D3D12Caps1, sizeof(D3D12Caps1));

		D3D12_FEATURE_DATA_D3D12_OPTIONS9 D3D12Caps9{};
		Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS9, &D3D12Caps9, sizeof(D3D12Caps9));

		bool bHighEnoughBindingTier = true;
		if (ShouldCheckBindlessSupport(SP_PCD3D_SM6))
		{
			bHighEnoughBindingTier = ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3;
		}

		if (D3D12Caps1.WaveOps && bHighEnoughBindingTier)
		{
			if (CheckDeviceForEmulatedAtomic64Support(Adapter, Device) || D3D12Caps9.AtomicInt64OnTypedResourceSupported)
			{
				MaxRHIFeatureLevel = ERHIFeatureLevel::SM6;
			}
		}
	}

	if (MaxRHIFeatureLevel == ERHIFeatureLevel::Num && InMaxFeatureLevel >= D3D_FEATURE_LEVEL_11_0)
	{
		MaxRHIFeatureLevel = ERHIFeatureLevel::SM5;
	}

	return MaxRHIFeatureLevel;
}

inline void GetResourceTiers(ID3D12Device* Device, D3D12_RESOURCE_BINDING_TIER& OutResourceBindingTier, D3D12_RESOURCE_HEAP_TIER& OutResourceHeapTier)
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS D3D12Caps{};
	Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &D3D12Caps, sizeof(D3D12Caps));

	OutResourceBindingTier = D3D12Caps.ResourceBindingTier;
	OutResourceHeapTier = D3D12Caps.ResourceHeapTier;
}

/**
 * Attempts to create a D3D12 device for the adapter using at minimum MinFeatureLevel.
 * If creation is successful, true is returned and the max supported feature level is set in OutMaxFeatureLevel.
 */
static bool SafeTestD3D12CreateDevice(IDXGIAdapter* Adapter, D3D_FEATURE_LEVEL MinFeatureLevel, FD3D12DeviceBasicInfo& OutInfo)
{
	__try
	{
		ID3D12Device* Device = nullptr;
		const HRESULT D3D12CreateDeviceResult = D3D12CreateDevice(Adapter, MinFeatureLevel, IID_PPV_ARGS(&Device));
		if (SUCCEEDED(D3D12CreateDeviceResult))
		{
			OutInfo.MaxFeatureLevel = FindHighestFeatureLevel(Device, MinFeatureLevel);
			OutInfo.MaxShaderModel = FindHighestShaderModel(Device);
			GetResourceTiers(Device, OutInfo.ResourceBindingTier, OutInfo.ResourceHeapTier);
			OutInfo.NumDeviceNodes = Device->GetNodeCount();
			OutInfo.MaxRHIFeatureLevel = FindMaxRHIFeatureLevel(Adapter, Device, OutInfo.MaxFeatureLevel, OutInfo.MaxShaderModel, OutInfo.ResourceBindingTier);

			Device->Release();
			return true;
		}
		else
		{
			UE_LOG(LogD3D12RHI, Log, TEXT("D3D12CreateDevice failed with code 0x%08X"), static_cast<int32>(D3D12CreateDeviceResult));
		}
	}
	__except (IsDelayLoadException(GetExceptionInformation()))
	{
		// We suppress warning C6322: Empty _except block. Appropriate checks are made upon returning. 
		CA_SUPPRESS(6322);
	}

	return false;
}

static bool SupportsDepthBoundsTest(FD3D12DynamicRHI* D3DRHI)
{
	// Determines if the primary adapter supports depth bounds test
	check(D3DRHI && D3DRHI->GetNumAdapters() >= 1);

	return D3DRHI->GetAdapter().IsDepthBoundsTestSupported();
}

bool FD3D12DynamicRHI::SetupDisplayHDRMetaData()
{
	// Determines if any displays support HDR
	check(GetNumAdapters() >= 1);

	DisplayList.Empty();

	bool bSupportsHDROutput = false;
	const int32 NumAdapters = GetNumAdapters();
	for (int32 AdapterIndex = 0; AdapterIndex < NumAdapters; ++AdapterIndex)
	{
		FD3D12Adapter& Adapter = GetAdapter(AdapterIndex);
		IDXGIAdapter* DXGIAdapter = Adapter.GetAdapter();

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
				VERIFYD3D12RESULT(Output6->GetDesc1(&OutputDesc));

				// Check for HDR support on the display.
				const bool bDisplaySupportsHDROutput = (OutputDesc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
				if (bDisplaySupportsHDROutput)
				{
					UE_LOG(LogD3D12RHI, Log, TEXT("HDR output is supported on adapter %i, display %u:"), AdapterIndex, DisplayIndex);
					UE_LOG(LogD3D12RHI, Log, TEXT("\t\tMinLuminance = %f"), OutputDesc.MinLuminance);
					UE_LOG(LogD3D12RHI, Log, TEXT("\t\tMaxLuminance = %f"), OutputDesc.MaxLuminance);
					UE_LOG(LogD3D12RHI, Log, TEXT("\t\tMaxFullFrameLuminance = %f"), OutputDesc.MaxFullFrameLuminance);

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
}

static bool IsAdapterBlocked(FD3D12Adapter* InAdapter)
{
#if !UE_BUILD_SHIPPING
	if (InAdapter)
	{
		FString BlockedIHVString;
		if (GConfig->GetString(TEXT("SystemSettings"), TEXT("RHI.BlockIHVD3D12"), BlockedIHVString, GEngineIni))
		{
			TArray<FString> BlockedIHVs;
			BlockedIHVString.ParseIntoArray(BlockedIHVs, TEXT(","));

			const TCHAR* VendorId = RHIVendorIdToString(EGpuVendorId(InAdapter->GetD3DAdapterDesc().VendorId));
			for (const FString& BlockedVendor : BlockedIHVs)
			{
				if (BlockedVendor.Equals(VendorId, ESearchCase::IgnoreCase))
				{
					return true;
				}
			}
		}
	}
#endif

	return false;
}

static bool IsAdapterSupported(FD3D12Adapter* InAdapter, ERHIFeatureLevel::Type InRequestedFeatureLevel)
{
	if (InAdapter)
	{
		if (const FD3D12AdapterDesc& Desc = InAdapter->GetDesc(); Desc.IsValid())
		{
			return Desc.MaxRHIFeatureLevel != ERHIFeatureLevel::Num && Desc.MaxRHIFeatureLevel >= InRequestedFeatureLevel;
		}
	}

	return false;
}

bool FD3D12DynamicRHIModule::IsSupported(ERHIFeatureLevel::Type RequestedFeatureLevel)
{
#if !PLATFORM_HOLOLENS
	// Windows version 15063 is Windows 1703 aka "Windows Creator Update"
	// This is the first version that supports ID3D12Device2 which is our minimum runtime device version.
	if (!FPlatformMisc::VerifyWindowsVersion(10, 0, 15063))
	{
		UE_LOG(LogD3D12RHI, Warning, TEXT("Missing full support for Direct3D 12. Update to Windows 1703 or newer for D3D12 support."));
		return false;
	}
#endif

	// If not computed yet
	if (ChosenAdapters.Num() == 0)
	{
		FindAdapter();
	}

	return ChosenAdapters.Num() > 0
		&& !IsAdapterBlocked(ChosenAdapters[0].Get())
		&& IsAdapterSupported(ChosenAdapters[0].Get(), RequestedFeatureLevel);
}

namespace D3D12RHI
{
	const TCHAR* GetFeatureLevelString(D3D_FEATURE_LEVEL FeatureLevel)
	{
		switch (FeatureLevel)
		{
		case D3D_FEATURE_LEVEL_9_1:		return TEXT("9_1");
		case D3D_FEATURE_LEVEL_9_2:		return TEXT("9_2");
		case D3D_FEATURE_LEVEL_9_3:		return TEXT("9_3");
		case D3D_FEATURE_LEVEL_10_0:	return TEXT("10_0");
		case D3D_FEATURE_LEVEL_10_1:	return TEXT("10_1");
		case D3D_FEATURE_LEVEL_11_0:	return TEXT("11_0");
		case D3D_FEATURE_LEVEL_11_1:	return TEXT("11_1");
		case D3D_FEATURE_LEVEL_12_0:	return TEXT("12_0");
		case D3D_FEATURE_LEVEL_12_1:	return TEXT("12_1");
#if D3D12_CORE_ENABLED
		case D3D_FEATURE_LEVEL_12_2:	return TEXT("12_2");
#endif
		}
		return TEXT("X_X");
	}
}

static uint32 CountAdapterOutputs(TRefCountPtr<IDXGIAdapter>& Adapter)
{
	uint32 OutputCount = 0;
	for (;;)
	{
		TRefCountPtr<IDXGIOutput> Output;
		HRESULT hr = Adapter->EnumOutputs(OutputCount, Output.GetInitReference());
		if (FAILED(hr))
		{
			break;
		}
		++OutputCount;
	}

	return OutputCount;
}

void FD3D12DynamicRHIModule::FindAdapter()
{
	// Once we've chosen one we don't need to do it again.
	check(ChosenAdapters.Num() == 0);

#if !UE_BUILD_SHIPPING
	if (CVarExperimentalShaderModels.GetValueOnAnyThread() == 1)
	{
		// Experimental features must be enabled before doing anything else with D3D.

		UUID ExperimentalFeatures[] =
		{
			D3D12ExperimentalShaderModels
		};
		HRESULT hr = D3D12EnableExperimentalFeatures(UE_ARRAY_COUNT(ExperimentalFeatures), ExperimentalFeatures, nullptr, nullptr);
		if (SUCCEEDED(hr))
		{
			UE_LOG(LogD3D12RHI, Log, TEXT("D3D12 experimental shader models enabled"));
		}
	}
#endif

	// Try to create the DXGIFactory.  This will fail if we're not running Vista.
	TRefCountPtr<IDXGIFactory4> DXGIFactory4;
	SafeCreateDXGIFactory(DXGIFactory4.GetInitReference());
	if (!DXGIFactory4)
	{
		return;
	}

	TRefCountPtr<IDXGIFactory6> DXGIFactory6;
	DXGIFactory4->QueryInterface(__uuidof(IDXGIFactory6), (void**)DXGIFactory6.GetInitReference());

	bool bAllowPerfHUD = true;

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	bAllowPerfHUD = false;
#endif

	// Allow HMD to override which graphics adapter is chosen, so we pick the adapter where the HMD is connected
	uint64 HmdGraphicsAdapterLuid = IHeadMountedDisplayModule::IsAvailable() ? IHeadMountedDisplayModule::Get().GetGraphicsAdapterLuid() : 0;
	// Non-static as it is used only a few times
	auto* CVarGraphicsAdapter = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.GraphicsAdapter"));
	int32 CVarExplicitAdapterValue = HmdGraphicsAdapterLuid == 0 ? (CVarGraphicsAdapter ? CVarGraphicsAdapter->GetValueOnGameThread() : -1) : -2;
	FParse::Value(FCommandLine::Get(), TEXT("graphicsadapter="), CVarExplicitAdapterValue);

	const bool bFavorDiscreteAdapter = CVarExplicitAdapterValue == -1;

	TRefCountPtr<IDXGIAdapter> TempAdapter;
	const D3D_FEATURE_LEVEL MinRequiredFeatureLevel = GetRequiredD3DFeatureLevel();

	FD3D12AdapterDesc FirstDiscreteAdapter;
	FD3D12AdapterDesc FirstAdapter;

	bool bRequestedWARP = D3D12RHI_ShouldCreateWithWarp();
	bool bAllowSoftwareRendering = D3D12RHI_AllowSoftwareFallback();

	int GpuPreferenceInt = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE;
	FParse::Value(FCommandLine::Get(), TEXT("-gpupreference="), GpuPreferenceInt);
	DXGI_GPU_PREFERENCE GpuPreference;
	switch(GpuPreferenceInt)
	{
	case 1: GpuPreference = DXGI_GPU_PREFERENCE_MINIMUM_POWER; break;
	case 2: GpuPreference = DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE; break;
	default: GpuPreference = DXGI_GPU_PREFERENCE_UNSPECIFIED; break;
	}

	int PreferredVendor = D3D12RHI_PreferAdapterVendor();

	// Enumerate the DXGIFactory's adapters.
	for (uint32 AdapterIndex = 0; FD3D12AdapterDesc::EnumAdapters(AdapterIndex, GpuPreference, DXGIFactory4, DXGIFactory6, TempAdapter.GetInitReference()) != DXGI_ERROR_NOT_FOUND; ++AdapterIndex)
	{
		// Check that if adapter supports D3D12.
		if (TempAdapter)
		{
			FD3D12DeviceBasicInfo DeviceInfo;
			if (SafeTestD3D12CreateDevice(TempAdapter, MinRequiredFeatureLevel, DeviceInfo))
			{
				check(DeviceInfo.NumDeviceNodes > 0);
				// Log some information about the available D3D12 adapters.
				DXGI_ADAPTER_DESC AdapterDesc;
				VERIFYD3D12RESULT(TempAdapter->GetDesc(&AdapterDesc));
				uint32 OutputCount = CountAdapterOutputs(TempAdapter);

				UE_LOG(LogD3D12RHI, Log,
					TEXT("Found D3D12 adapter %u: %s (Max supported Feature Level %s, shader model %d.%d)"),
					AdapterIndex,
					AdapterDesc.Description,
					GetFeatureLevelString(DeviceInfo.MaxFeatureLevel),
					(DeviceInfo.MaxShaderModel >> 4), (DeviceInfo.MaxShaderModel & 0xF)
				);
				UE_LOG(LogD3D12RHI, Log,
					TEXT("Adapter has %uMB of dedicated video memory, %uMB of dedicated system memory, and %uMB of shared system memory, %d output[s]"),
					(uint32)(AdapterDesc.DedicatedVideoMemory / (1024 * 1024)),
					(uint32)(AdapterDesc.DedicatedSystemMemory / (1024 * 1024)),
					(uint32)(AdapterDesc.SharedSystemMemory / (1024 * 1024)),
					OutputCount
				);

				bool bIsAMD = AdapterDesc.VendorId == 0x1002;
				bool bIsIntel = AdapterDesc.VendorId == 0x8086;
				bool bIsNVIDIA = AdapterDesc.VendorId == 0x10DE;
				bool bIsWARP = AdapterDesc.VendorId == 0x1414;

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
				const bool bIsIntegrated = !bIsNonLocalMemoryPresent;

				// PerfHUD is for performance profiling
				const bool bIsPerfHUD = !FCString::Stricmp(AdapterDesc.Description, TEXT("NVIDIA PerfHUD"));

				FD3D12AdapterDesc CurrentAdapter(AdapterDesc, AdapterIndex, DeviceInfo);

				CurrentAdapter.NumDeviceNodes = DeviceInfo.NumDeviceNodes;
				CurrentAdapter.GpuPreference = GpuPreference;
				CurrentAdapter.bIsIntegrated = bIsIntegrated;

				// If requested WARP, then reject all other adapters. If WARP not requested, then reject the WARP device if software rendering support is disallowed
				const bool bSkipWARP = (bRequestedWARP && !bIsWARP) || (!bRequestedWARP && bIsWARP && !bAllowSoftwareRendering);

				// we don't allow the PerfHUD adapter
				const bool bSkipPerfHUDAdapter = bIsPerfHUD && !bAllowPerfHUD;

				// the HMD wants a specific adapter, not this one
				const bool bSkipHmdGraphicsAdapter = HmdGraphicsAdapterLuid != 0 && FMemory::Memcmp(&HmdGraphicsAdapterLuid, &AdapterDesc.AdapterLuid, sizeof(LUID)) != 0;

				// the user wants a specific adapter, not this one
				const bool bSkipExplicitAdapter = CVarExplicitAdapterValue >= 0 && AdapterIndex != CVarExplicitAdapterValue;

				const bool bSkipAdapter = bSkipWARP || bSkipPerfHUDAdapter || bSkipHmdGraphicsAdapter || bSkipExplicitAdapter;

				if (!bSkipAdapter)
				{
					if (!bIsWARP && !bIsIntegrated && !FirstDiscreteAdapter.IsValid())
					{
						FirstDiscreteAdapter = CurrentAdapter;
					}
					else if (PreferredVendor == AdapterDesc.VendorId && FirstDiscreteAdapter.IsValid())
					{
						FirstDiscreteAdapter = CurrentAdapter;
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
		}
	}

	TSharedPtr<FD3D12Adapter> NewAdapter;
	if (bFavorDiscreteAdapter)
	{
		// We assume Intel is integrated graphics (slower than discrete) than NVIDIA or AMD cards and rather take a different one
		if (FirstDiscreteAdapter.IsValid())
		{
			NewAdapter = TSharedPtr<FD3D12Adapter>(new FWindowsD3D12Adapter(FirstDiscreteAdapter));
			ChosenAdapters.Add(NewAdapter);
		}
		else
		{
			NewAdapter = TSharedPtr<FD3D12Adapter>(new FWindowsD3D12Adapter(FirstAdapter));
			ChosenAdapters.Add(NewAdapter);
		}
	}
	else
	{
		NewAdapter = TSharedPtr<FD3D12Adapter>(new FWindowsD3D12Adapter(FirstAdapter));
		ChosenAdapters.Add(NewAdapter);
	}

	if (ChosenAdapters.Num() > 0 && ChosenAdapters[0]->GetDesc().IsValid())
	{
		UE_LOG(LogD3D12RHI, Log, TEXT("Chosen D3D12 Adapter Id = %u"), ChosenAdapters[0]->GetAdapterIndex());

		const DXGI_ADAPTER_DESC& AdapterDesc = ChosenAdapters[0]->GetD3DAdapterDesc();
		GRHIVendorId = AdapterDesc.VendorId;
		GRHIAdapterName = AdapterDesc.Description;
		GRHIDeviceId = AdapterDesc.DeviceId;
		GRHIDeviceRevision = AdapterDesc.Revision;
	}
	else
	{
		UE_LOG(LogD3D12RHI, Error, TEXT("Failed to choose a D3D12 Adapter."));
	}
}

static bool DoesAnyAdapterSupportSM6(const TArray<TSharedPtr<FD3D12Adapter>>& Adapters)
{
	for (const TSharedPtr<FD3D12Adapter>& Adapter : Adapters)
	{
		if (IsAdapterSupported(Adapter.Get(), ERHIFeatureLevel::SM6))
		{
			return true;
		}
	}

	return false;
}

FDynamicRHI* FD3D12DynamicRHIModule::CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel)
{
#if PLATFORM_HOLOLENS
	check(RequestedFeatureLevel == ERHIFeatureLevel::ES3_1);

	GMaxRHIFeatureLevel = ERHIFeatureLevel::ES3_1;
	GMaxRHIShaderPlatform = SP_D3D_ES3_1_HOLOLENS;

	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_D3D_ES3_1_HOLOLENS;
#else
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_PCD3D_ES3_1;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = SP_PCD3D_SM5;
	if (DoesAnyAdapterSupportSM6(ChosenAdapters))
	{
		GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM6] = SP_PCD3D_SM6;
	}
#endif

	ERHIFeatureLevel::Type PreviewFeatureLevel;
	if (!GIsEditor && RHIGetPreviewFeatureLevel(PreviewFeatureLevel))
	{
		check(PreviewFeatureLevel == ERHIFeatureLevel::ES3_1);

		// ES3.1 feature level emulation in D3D
		GMaxRHIFeatureLevel = PreviewFeatureLevel;
	}
	else
	{
		GMaxRHIFeatureLevel = RequestedFeatureLevel;
	}

	if (!ensure(GMaxRHIFeatureLevel < ERHIFeatureLevel::Num))
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;
	}

	GMaxRHIShaderPlatform = GShaderPlatformForFeatureLevel[GMaxRHIFeatureLevel];
	check(GMaxRHIShaderPlatform != SP_NumPlatforms);

#if D3D12RHI_SUPPORTS_WIN_PIX
	bool bPixEventEnabled = (WindowsPixDllHandle != nullptr);
#else
	bool bPixEventEnabled = false;
#endif // USE_PIX
	
	if (ChosenAdapters.Num() > 0 && ChosenAdapters[0].IsValid())
	{
		FGenericCrashContext::SetEngineData(TEXT("RHI.IntegratedGPU"), ChosenAdapters[0].Get()->GetDesc().bIsIntegrated ? TEXT("true") : TEXT("false"));
		GRHIDeviceIsIntegrated = ChosenAdapters[0].Get()->GetDesc().bIsIntegrated;
	}

	const FString FeatureLevelString = LexToString(GMaxRHIFeatureLevel);
	UE_LOG(LogD3D12RHI, Display, TEXT("Creating D3D12 RHI with Max Feature Level %s"), *FeatureLevelString);

	GD3D12RHI = new FD3D12DynamicRHI(ChosenAdapters, bPixEventEnabled);
#if ENABLE_RHI_VALIDATION
	if (FParse::Param(FCommandLine::Get(), TEXT("RHIValidation")))
	{
		return new FValidationRHI(GD3D12RHI);
	}
#endif
	return GD3D12RHI;
}

void FD3D12DynamicRHIModule::StartupModule()
{
#if NV_AFTERMATH
	// Note - can't check device type here, we'll check for that before actually initializing Aftermath
	const FString AftermathBinariesRoot = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/NVIDIA/NVaftermath/Win64/");
	
	FPlatformProcess::PushDllDirectory(*AftermathBinariesRoot);
	void* Handle = FPlatformProcess::GetDllHandle(TEXT("GFSDK_Aftermath_Lib.x64.dll"));
	FPlatformProcess::PopDllDirectory(*AftermathBinariesRoot);

	if (Handle == nullptr)
	{
		UE_LOG(LogD3D12RHI, Warning, TEXT("Failed to load GFSDK_Aftermath_Lib.x64.dll"));
		GDX12NVAfterMathModuleLoaded = false;
		return;
	}
	else
	{
		UE_LOG(LogD3D12RHI, Log, TEXT("Aftermath initialized"));
		GDX12NVAfterMathModuleLoaded = true;
	}
#endif

#if D3D12RHI_SUPPORTS_WIN_PIX
#if PLATFORM_CPU_ARM_FAMILY
	static FString WindowsPixDllRelativePath = FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/Windows/WinPixEventRuntime/arm64"));
	static const TCHAR* WindowsPixDll = TEXT("WinPixEventRuntime_UAP.dll");
#else
	static FString WindowsPixDllRelativePath = FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries/ThirdParty/Windows/WinPixEventRuntime/x64"));
	static const TCHAR* WindowsPixDll = TEXT("WinPixEventRuntime.dll");
#endif

	UE_LOG(LogD3D12RHI, Log, TEXT("Loading %s for PIX profiling (from %s)."), WindowsPixDll, *WindowsPixDllRelativePath);
	WindowsPixDllHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine(WindowsPixDllRelativePath, WindowsPixDll));
	if (WindowsPixDllHandle == nullptr)
	{
		const int32 ErrorNum = FPlatformMisc::GetLastError();
		TCHAR ErrorMsg[1024];
		FPlatformMisc::GetSystemErrorMessage(ErrorMsg, 1024, ErrorNum);
		UE_LOG(LogD3D12RHI, Error, TEXT("Failed to get %s handle: %s (%d)"), WindowsPixDll, ErrorMsg, ErrorNum);
	}
#endif
}

void FD3D12DynamicRHIModule::ShutdownModule()
{
#if D3D12RHI_SUPPORTS_WIN_PIX
	if (WindowsPixDllHandle)
	{
		FPlatformProcess::FreeDllHandle(WindowsPixDllHandle);
		WindowsPixDllHandle = nullptr;
	}
#endif
}

static bool IsRayTracingEmulated(uint32 DeviceId)
{
	uint32 EmulatedRayTracingIds[] =
	{
		0x1B80, // "NVIDIA GeForce GTX 1080"
		0x1B81, // "NVIDIA GeForce GTX 1070"
		0x1B82, // "NVIDIA GeForce GTX 1070 Ti"
		0x1B83, // "NVIDIA GeForce GTX 1060 6GB"
		0x1B84, // "NVIDIA GeForce GTX 1060 3GB"
		0x1C01, // "NVIDIA GeForce GTX 1050 Ti"
		0x1C02, // "NVIDIA GeForce GTX 1060 3GB"
		0x1C03, // "NVIDIA GeForce GTX 1060 6GB"
		0x1C04, // "NVIDIA GeForce GTX 1060 5GB"
		0x1C06, // "NVIDIA GeForce GTX 1060 6GB"
		0x1C08, // "NVIDIA GeForce GTX 1050"
		0x1C81, // "NVIDIA GeForce GTX 1050"
		0x1C82, // "NVIDIA GeForce GTX 1050 Ti"
		0x1C83, // "NVIDIA GeForce GTX 1050"
		0x1B06, // "NVIDIA GeForce GTX 1080 Ti"
		0x1B00, // "NVIDIA TITAN X (Pascal)"
		0x1B02, // "NVIDIA TITAN Xp"
		0x1D81, // "NVIDIA TITAN V"
	};

	for (int Index = 0; Index < UE_ARRAY_COUNT(EmulatedRayTracingIds); ++Index)
	{
		if (DeviceId == EmulatedRayTracingIds[Index])
		{
			return true;
		}
	}

	return false;
}

static bool IsNvidiaAmpereGPU(uint32 DeviceId)
{
	uint32 DeviceList[] =
	{
		0x2200,	// GA102
		0x2204,	// GA102 - GeForce RTX 3090
		0x2205,	// GA102 - GeForce RTX 3080 Ti 20GB
		0x2206,	// GA102 - GeForce RTX 3080
		0x2208,	// GA102 - GeForce RTX 3080 Ti
		0x220a,	// GA102 - GeForce RTX 3080 12GB
		0x220d,	// GA102 - CMP 90HX
		0x2216,	// GA102 - GeForce RTX 3080 Lite Hash Rate
		0x2230,	// GA102GL - RTX A6000
		0x2231,	// GA102GL - RTX A5000
		0x2232,	// GA102GL - RTX A4500
		0x2233,	// GA102GL - RTX A5500
		0x2235,	// GA102GL - A40
		0x2236,	// GA102GL - A10
		0x2237,	// GA102GL - A10G
		0x2238,	// GA102GL - A10M
		0x223f,	// GA102G
		0x2302,	// GA103
		0x2321,	// GA103
		0x2414,	// GA103 - GeForce RTX 3060 Ti
		0x2420,	// GA103M - GeForce RTX 3080 Ti Mobile
		0x2460,	// GA103M - GeForce RTX 3080 Ti Laptop GPU
		0x2482,	// GA104 - GeForce RTX 3070 Ti
		0x2483,	// GA104
		0x2484,	// GA104 - GeForce RTX 3070
		0x2486,	// GA104 - GeForce RTX 3060 Ti
		0x2487,	// GA104 - GeForce RTX 3060
		0x2488,	// GA104 - GeForce RTX 3070 Lite Hash Rate
		0x2489,	// GA104 - GeForce RTX 3060 Ti Lite Hash Rate
		0x248a,	// GA104 - CMP 70HX
		0x249c,	// GA104M - GeForce RTX 3080 Mobile / Max-Q 8GB/16GB
		0x249f,	// GA104M
		0x24a0,	// GA104 - Geforce RTX 3070 Ti Laptop GPU
		0x24b0,	// GA104GL - RTX A4000
		0x24b6,	// GA104GLM - RTX A5000 Mobile
		0x24b7,	// GA104GLM - RTX A4000 Mobile
		0x24b8,	// GA104GLM - RTX A3000 Mobile
		0x24dc,	// GA104M - GeForce RTX 3080 Mobile / Max-Q 8GB/16GB
		0x24dd,	// GA104M - GeForce RTX 3070 Mobile / Max-Q
		0x24e0,	// GA104M - Geforce RTX 3070 Ti Laptop GPU
		0x24fa,	// GA104 - RTX A4500 Embedded GPU 
		0x2501,	// GA106 - GeForce RTX 3060
		0x2503,	// GA106 - GeForce RTX 3060
		0x2504,	// GA106 - GeForce RTX 3060 Lite Hash Rate
		0x2505,	// GA106
		0x2507,	// GA106 - Geforce RTX 3050
		0x2520,	// GA106M - GeForce RTX 3060 Mobile / Max-Q
		0x2523,	// GA106M - GeForce RTX 3050 Ti Mobile / Max-Q
		0x2531,	// GA106 - RTX A2000
		0x2560,	// GA106M - GeForce RTX 3060 Mobile / Max-Q
		0x2563,	// GA106M - GeForce RTX 3050 Ti Mobile / Max-Q
		0x2571,	// GA106 - RTX A2000 12GB
		0x2583,	// GA107 - GeForce RTX 3050
		0x25a0,	// GA107M - GeForce RTX 3050 Ti Mobile
		0x25a2,	// GA107M - GeForce RTX 3050 Mobile
		0x25a4,	// GA107
		0x25a5,	// GA107M - GeForce RTX 3050 Mobile
		0x25a6,	// GA107M - GeForce MX570
		0x25a7,	// GA107M - GeForce MX570
		0x25a9,	// GA107M - GeForce RTX 2050
		0x25b5,	// GA107GLM - RTX A4 Mobile
		0x25b8,	// GA107GLM - RTX A2000 Mobile
		0x25b9,	// GA107GLM - RTX A1000 Laptop GPU
		0x25e0,	// GA107BM - GeForce RTX 3050 Ti Mobile
		0x25e2,	// GA107BM - GeForce RTX 3050 Mobile
		0x25e5,	// GA107BM - GeForce RTX 3050 Mobile
		0x25f9,	// GA107 - RTX A1000 Embedded GPU 
		0x25fa,	// GA107 - RTX A2000 Embedded GPU
	};

	for (uint32 KnownDeviceId : DeviceList)
	{
		if (DeviceId == KnownDeviceId)
		{
			return true;
		}
	}

	return false;
}

static void DisableRayTracingSupport()
{
	GRHISupportsRayTracing = false;
	GRHISupportsRayTracingPSOAdditions = false;
	GRHISupportsRayTracingDispatchIndirect = false;
	GRHISupportsRayTracingAsyncBuildAccelerationStructure = false;
	GRHISupportsRayTracingAMDHitToken = false;
	GRHISupportsInlineRayTracing = false;
	GRHISupportsRayTracingShaders = false;
}

void FD3D12DynamicRHI::Init()
{
#if D3D12RHI_SUPPORTS_WIN_PIX
	// PIX GPU capture dll: makes PIX be able to attach to our process. !GetModuleHandle() is required because PIX may already be attached.
	if (GAutoAttachPIX || FParse::Param(FCommandLine::Get(), TEXT("attachPIX")))
	{
		// If PIX is not already attached, load its dll to auto attach ourselves
		if (!FPlatformProcess::GetDllHandle(L"WinPixGpuCapturer.dll"))
		{
			// This should always be loaded from the installed PIX directory.
			// This function does assume it's installed under Program Files so we may have to revisit for custom install locations.
			WinPixGpuCapturerHandle = PIXLoadLatestWinPixGpuCapturerLibrary();
		}
	}
#endif

	check(!GIsRHIInitialized);

	const DXGI_ADAPTER_DESC& AdapterDesc = GetAdapter().GetD3DAdapterDesc();

	UE_LOG(LogD3D12RHI, Log, TEXT("    GPU DeviceId: 0x%x (for the marketing name, search the web for \"GPU Device Id\")"), AdapterDesc.DeviceId);

#if AMD_API_ENABLE
	// Initialize the AMD AGS utility library, when running on an AMD device
	AGSGPUInfo AmdAgsGpuInfo = {};
	if (IsRHIDeviceAMD() && bAllowVendorDevice)
	{
		check(AmdAgsContext == nullptr);
		check(AmdSupportedExtensionFlags == 0);

		// agsInit should be called before D3D device creation
		agsInitialize(AGS_MAKE_VERSION(AMD_AGS_VERSION_MAJOR, AMD_AGS_VERSION_MINOR, AMD_AGS_VERSION_PATCH), nullptr, &AmdAgsContext, &AmdAgsGpuInfo);
	}
#endif

	// Create a device chain for each of the adapters we have chosen. This could be a single discrete card,
	// a set discrete cards linked together (i.e. SLI/Crossfire) an Integrated device or any combination of the above
	for (TSharedPtr<FD3D12Adapter>& Adapter : ChosenAdapters)
	{
		check(Adapter->GetDesc().IsValid());
		Adapter->InitializeDevices();
	}

	bool bHasVendorSupportForAtomic64 = false;

#if AMD_API_ENABLE
	// Warn if we are trying to use RGP frame markers but are either running on a non-AMD device
	// or using an older AMD driver without RGP marker support
	if (IsRHIDeviceAMD())
	{
		if (bAllowVendorDevice)
		{
			static_assert(sizeof(AGSDX12ReturnedParams::ExtensionsSupported) == sizeof(uint32));
			AGSDX12ReturnedParams::ExtensionsSupported AMDSupportedExtensions;
			FMemory::Memcpy(&AMDSupportedExtensions, &AmdSupportedExtensionFlags, sizeof(uint32));

			if (GEmitRgpFrameMarkers && AMDSupportedExtensions.userMarkers == 0)
		    {
			    UE_LOG(LogD3D12RHI, Warning, TEXT("Attempting to use RGP frame markers without driver support. Update AMD driver."));
		    }

			bHasVendorSupportForAtomic64 = AMDSupportedExtensions.intrinsics19 != 0;  // "intrinsics19" includes AtomicU64
			bHasVendorSupportForAtomic64 = bHasVendorSupportForAtomic64 && AMDSupportedExtensions.UAVBindSlot != 0;
		}
	}
	else if (GEmitRgpFrameMarkers)
	{
		UE_LOG(LogD3D12RHI, Warning, TEXT("Attempting to use RGP frame markers on a non-AMD device."));
	}
#endif

#if !PLATFORM_HOLOLENS
	// Disable ray tracing for Windows build versions

	if (GRHISupportsRayTracing
		&& GMinimumWindowsBuildVersionForRayTracing > 0
		&& !FPlatformMisc::VerifyWindowsVersion(10, 0, GMinimumWindowsBuildVersionForRayTracing))
	{
		DisableRayTracingSupport();
		UE_LOG(LogD3D12RHI, Warning, TEXT("Ray tracing is disabled because it requires Windows 10 version %u"), (uint32)GMinimumWindowsBuildVersionForRayTracing);
	}
#endif

#if NV_API_ENABLE
	if (IsRHIDeviceNVIDIA() && bAllowVendorDevice)
	{
		const NvAPI_Status NvStatus = NvAPI_Initialize();
		if (NvStatus == NVAPI_OK)
		{
			NvAPI_Status NvStatusAtomicU64 = NvAPI_D3D12_IsNvShaderExtnOpCodeSupported(GetAdapter().GetD3DDevice(), NV_EXTN_OP_UINT64_ATOMIC, &bHasVendorSupportForAtomic64);
			if (NvStatusAtomicU64 != NVAPI_OK)
			{
				UE_LOG(LogD3D12RHI, Warning, TEXT("Failed to query support for 64 bit atomics"));
			}
		}
		else
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("Failed to initialize NVAPI"));
		}

		NvU32 DriverVersion = UINT32_MAX;

		if (NvStatus == NVAPI_OK)
		{
			NvAPI_ShortString BranchString("");
			if (NvAPI_SYS_GetDriverAndBranchVersion(&DriverVersion, BranchString) != NVAPI_OK)
			{
				UE_LOG(LogD3D12RHI, Warning, TEXT("Failed to query NVIDIA driver version"));
			}
		}

		// Disable ray tracing for old Nvidia drivers
		if (GRHISupportsRayTracing 
			&& GMinimumDriverVersionForRayTracingNVIDIA > 0
			&& DriverVersion < (uint32)GMinimumDriverVersionForRayTracingNVIDIA)
		{
			DisableRayTracingSupport();
			UE_LOG(LogD3D12RHI, Warning, TEXT("Ray tracing is disabled because the driver is too old"));
		}

		// Disable indirect ray tracing dispatch on drivers that have a known bug.
		if (GRHISupportsRayTracingDispatchIndirect 
			&& DriverVersion < (uint32)46611)
		{
			GRHISupportsRayTracingDispatchIndirect = false;
			UE_LOG(LogD3D12RHI, Warning,
				TEXT("Indirect ray tracing dispatch is disabled because of known bugs in the current driver. ")
				TEXT("Please update to NVIDIA driver version 466.11 or newer."));
		}

		if (GRHISupportsRayTracing
			&& IsRayTracingEmulated(AdapterDesc.DeviceId))
		{
#if DXR_ALLOW_EMULATED_RAYTRACING
			if (!GAllowEmulatedRayTracing)
			{
				DisableRayTracingSupport();
				UE_LOG(LogD3D12RHI, Warning, TEXT("Ray tracing is disabled for NVIDIA cards with the Pascal architecture. This can be overridden with the following CVar: r.D3D12.DXR.AllowEmulatedRayTracing=1"));
			}
#else
			DisableRayTracingSupport();
			UE_LOG(LogD3D12RHI, Warning, TEXT("Ray tracing is disabled for NVIDIA cards with the Pascal architecture."));
#endif // DXR_ALLOW_EMULATED_RAYTRACING
		}

		if (GRHISupportsRayTracing && DriverVersion < 45700u)
		{
			GD3D12WorkaroundFlags.bAllowGetShaderIdentifierOnCollectionSubObject = false;
			UE_LOG(LogD3D12RHI, Warning, TEXT("GD3D12WorkaroundFlags.bAllowGetShaderIdentifierOnCollectionSubObject is disabled due to a known issue with current driver version."));
		}
	} // if NVIDIA
#endif // NV_API_ENABLE

#if AMD_API_ENABLE
	if (GRHISupportsRayTracing
		&& IsRHIDeviceAMD()
		&& AmdAgsContext
		&& AmdAgsGpuInfo.radeonSoftwareVersion)
	{
		if (GMinimumDriverVersionForRayTracingAMD > 0 
			&& agsCheckDriverVersion(AmdAgsGpuInfo.radeonSoftwareVersion, GMinimumDriverVersionForRayTracingAMD) == AGS_SOFTWAREVERSIONCHECK_OLDER)
		{
			DisableRayTracingSupport();
			UE_LOG(LogD3D12RHI, Warning, TEXT("Ray tracing is disabled because the driver is too old"));
		}

		if (GRHISupportsRayTracing)
		{
			static_assert(sizeof(AGSDX12ReturnedParams::ExtensionsSupported) == sizeof(uint32));
			AGSDX12ReturnedParams::ExtensionsSupported AMDSupportedExtensions;
			FMemory::Memcpy(&AMDSupportedExtensions, &AmdSupportedExtensionFlags, sizeof(uint32));

			GRHISupportsRayTracingAMDHitToken = AMDSupportedExtensions.rayHitToken;
			UE_LOG(LogD3D12RHI, Log, TEXT("AMD hit token extension is %s"), GRHISupportsRayTracingAMDHitToken ? TEXT("supported") : TEXT("not supported"));
		}
	}
#endif // AMD_API_ENABLE

#if INTEL_EXTENSIONS
	if (IsRHIDeviceIntel() && bAllowVendorDevice)
	{
		INTCExtensionInfo INTCExtensionInfo{};
		IntelExtensionContext = CreateIntelExtensionsContext(GetAdapter().GetD3DDevice(), INTCExtensionInfo);

		if (IntelExtensionContext)
		{
			bHasVendorSupportForAtomic64 = (INTCExtensionInfo.RequestedExtensionVersion.HWFeatureLevel > 0);
		}

		if (GRHISupportsMeshShadersTier0 || GRHISupportsMeshShadersTier1)
		{
			GRHISupportsMeshShadersTier0 = GRHISupportsMeshShadersTier1 = false;
			UE_LOG(LogD3D12RHI, Warning, TEXT("Mesh shaders are disabled due to a known driver issue."));
		}
	}
#endif // INTEL_EXTENSIONS

	GRHIPersistentThreadGroupCount = 1440; // TODO: Revisit based on vendor/adapter/perf query

	GTexturePoolSize = 0;

	// Issue: 32bit windows doesn't report 64bit value, we take what we get.
	FD3D12GlobalStats::GDedicatedVideoMemory = int64(AdapterDesc.DedicatedVideoMemory);
	FD3D12GlobalStats::GDedicatedSystemMemory = int64(AdapterDesc.DedicatedSystemMemory);
	FD3D12GlobalStats::GSharedSystemMemory = int64(AdapterDesc.SharedSystemMemory);

	// Total amount of system memory, clamped to 8 GB
	int64 TotalPhysicalMemory = FMath::Min(int64(FPlatformMemory::GetConstants().TotalPhysicalGB), 8ll) * (1024ll * 1024ll * 1024ll);

	// Consider 50% of the shared memory but max 25% of total system memory.
	int64 ConsideredSharedSystemMemory = FMath::Min(FD3D12GlobalStats::GSharedSystemMemory / 2ll, TotalPhysicalMemory / 4ll);

	TRefCountPtr<IDXGIAdapter3> DxgiAdapter3;
	VERIFYD3D12RESULT(GetAdapter().GetAdapter()->QueryInterface(IID_PPV_ARGS(DxgiAdapter3.GetInitReference())));
	DXGI_QUERY_VIDEO_MEMORY_INFO LocalVideoMemoryInfo;
	VERIFYD3D12RESULT(DxgiAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &LocalVideoMemoryInfo));
	const int64 TargetBudget = LocalVideoMemoryInfo.Budget * 0.90f;	// Target using 90% of our budget to account for some fragmentation.
	FD3D12GlobalStats::GTotalGraphicsMemory = TargetBudget;

	if (sizeof(SIZE_T) < 8)
	{
		// Clamp to 1 GB if we're less than 64-bit
		FD3D12GlobalStats::GTotalGraphicsMemory = FMath::Min(FD3D12GlobalStats::GTotalGraphicsMemory, 1024ll * 1024ll * 1024ll);
	}

	if (GPoolSizeVRAMPercentage > 0)
	{
		float PoolSize = float(GPoolSizeVRAMPercentage) * 0.01f * float(FD3D12GlobalStats::GTotalGraphicsMemory);

		// Truncate GTexturePoolSize to MB (but still counted in bytes)
		GTexturePoolSize = int64(FGenericPlatformMath::TruncToFloat(PoolSize / 1024.0f / 1024.0f)) * 1024 * 1024;

		UE_LOG(LogRHI, Log, TEXT("Texture pool is %llu MB (%d%% of %llu MB)"),
			GTexturePoolSize / 1024 / 1024,
			GPoolSizeVRAMPercentage,
			FD3D12GlobalStats::GTotalGraphicsMemory / 1024 / 1024);
	}

	RequestedTexturePoolSize = GTexturePoolSize;

	VERIFYD3D12RESULT(DxgiAdapter3->SetVideoMemoryReservation(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, FMath::Min((int64)LocalVideoMemoryInfo.AvailableForReservation, FD3D12GlobalStats::GTotalGraphicsMemory)));

#if (UE_BUILD_SHIPPING && WITH_EDITOR) && PLATFORM_WINDOWS && !PLATFORM_64BITS
	// Disable PIX for windows in the shipping editor builds
	D3DPERF_SetOptions(1);
#endif

	// Multi-threaded resource creation is always supported in DX12, but allow users to disable it.
	GRHISupportsAsyncTextureCreation = D3D12RHI_ShouldAllowAsyncResourceCreation();
	if (GRHISupportsAsyncTextureCreation)
	{
		UE_LOG(LogD3D12RHI, Log, TEXT("Async texture creation enabled"));
	}
	else
	{
		UE_LOG(LogD3D12RHI, Log, TEXT("Async texture creation disabled: %s"),
			D3D12RHI_ShouldAllowAsyncResourceCreation() ? TEXT("no driver support") : TEXT("disabled by user"));
	}

	if (GRHISupportsAtomicUInt64)
	{
		UE_LOG(LogD3D12RHI, Log, TEXT("RHI has support for 64 bit atomics"));
	}
	else if (bHasVendorSupportForAtomic64)
	{
		GRHISupportsAtomicUInt64 = true;

		UE_LOG(LogD3D12RHI, Log, TEXT("RHI has vendor support for 64 bit atomics"));
	}
	else
	{
		UE_LOG(LogD3D12RHI, Log, TEXT("RHI does not have support for 64 bit atomics"));
	}

	D3D12_FEATURE_DATA_D3D12_OPTIONS6 options = {};
	HRESULT Options6HR = GetAdapter().GetD3DDevice()->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS6, &options, sizeof(options));

	// Allow async compute by default on nVidia cards which support PerPrimitiveShadingRateSupportedWithViewportIndexing 
	// this should be a good metric according to nVidia itself (this is set for Ampere and newer cards)
	bool bnVidiaAsyncComputeSupported = false;
	// Disable async compute on nVidia by default because of Async compute GPU crashes (UE-163646)
	/*
	if (IsRHIDeviceNVIDIA() && Options6HR == S_OK && options.PerPrimitiveShadingRateSupportedWithViewportIndexing)
	{
		bnVidiaAsyncComputeSupported = true;
	}
	*/

	GSupportsEfficientAsyncCompute = GAllowAsyncCompute && (FParse::Param(FCommandLine::Get(), TEXT("ForceAsyncCompute")) || (GRHISupportsParallelRHIExecute && (IsRHIDeviceAMD() || bnVidiaAsyncComputeSupported)));

	GSupportsDepthBoundsTest = SupportsDepthBoundsTest(this);

	{
		GRHISupportsHDROutput = SetupDisplayHDRMetaData();

		// Specify the desired HDR pixel format.
		// Possible values are:
		//	1) PF_FloatRGBA - FP16 format that allows for linear gamma. This is the current engine default.
		//					r.HDR.Display.ColorGamut = 0 (sRGB which is the same gamut as ScRGB)
		//					r.HDR.Display.OutputDevice = 5 or 6 (ScRGB)
		//	2) PF_A2B10G10R10 - Save memory vs FP16 as well as allow for possible performance improvements 
		//						in fullscreen by avoiding format conversions.
		//					r.HDR.Display.ColorGamut = 2 (Rec2020 / BT2020)
		//					r.HDR.Display.OutputDevice = 3 or 4 (ST-2084)
#if WITH_EDITOR
		GRHIHDRDisplayOutputFormat = PF_FloatRGBA;
#else
		GRHIHDRDisplayOutputFormat = PF_A2B10G10R10;
#endif
	}

	FHardwareInfo::RegisterHardwareInfo(NAME_RHI, TEXT("D3D12"));

	GRHISupportsTextureStreaming = true;
	GRHISupportsFirstInstance = true;

	// Indicate that the RHI needs to use the engine's deferred deletion queue.
	GRHINeedsExtraDeletionLatency = true;

	// There is no need to defer deletion of streaming textures
	// - Suballocated ones are defer-deleted by their allocators
	// - Standalones are added to the deferred deletion queue of its parent FD3D12Adapter
	GRHIForceNoDeletionLatencyForStreamingTextures = !!PLATFORM_WINDOWS;

	if(Options6HR == S_OK && options.VariableShadingRateTier != D3D12_VARIABLE_SHADING_RATE_TIER_NOT_SUPPORTED)
	{
		GRHISupportsPipelineVariableRateShading = true;		// We have at least tier 1.
		GRHISupportsLargerVariableRateShadingSizes = (options.AdditionalShadingRatesSupported != 0);

		if (options.VariableShadingRateTier == D3D12_VARIABLE_SHADING_RATE_TIER_2)
		{
			GRHISupportsAttachmentVariableRateShading = true;
			GRHISupportsComplexVariableRateShadingCombinerOps = true;

			GRHIVariableRateShadingImageTileMinWidth = options.ShadingRateImageTileSize;
			GRHIVariableRateShadingImageTileMinHeight = options.ShadingRateImageTileSize;
			GRHIVariableRateShadingImageTileMaxWidth = options.ShadingRateImageTileSize;
			GRHIVariableRateShadingImageTileMaxHeight = options.ShadingRateImageTileSize;

			GRHIVariableRateShadingImageDataType = VRSImage_Palette;
			GRHIVariableRateShadingImageFormat = PF_R8_UINT;
		}
	}
	else
	{
		GRHISupportsAttachmentVariableRateShading = GRHISupportsPipelineVariableRateShading = false;
		GRHIVariableRateShadingImageTileMinWidth = 1;
		GRHIVariableRateShadingImageTileMinHeight = 1;
		GRHIVariableRateShadingImageTileMaxWidth = 1;
		GRHIVariableRateShadingImageTileMaxHeight = 1;
	}

	GRHISupportsUAVFormatAliasing = (GetAdapter().GetResourceHeapTier() > D3D12_RESOURCE_HEAP_TIER_1 && IsRHIDeviceNVIDIA());

	InitializeSubmissionPipe();

	GRHICommandList.GetImmediateCommandList().InitializeImmediateContexts();

	for (TSharedPtr<FD3D12Adapter>& Adapter : ChosenAdapters)
	{
		FD3D12BufferedGPUTiming::Initialize(Adapter.Get());
	}

	FRenderResource::InitPreRHIResources();
	GIsRHIInitialized = true;
}

void FD3D12DynamicRHI::PostInit()
{
	if (!FPlatformProperties::RequiresCookedData() && (GRHISupportsRayTracing || GRHISupportsRHIThread))
	{
		// Make sure all global shaders are complete at this point
		extern RENDERCORE_API const int32 GlobalShaderMapId;

		TArray<int32> ShaderMapIds;
		ShaderMapIds.Add(GlobalShaderMapId);

		GShaderCompilingManager->FinishCompilation(TEXT("Global"), ShaderMapIds);
	}

	if (GRHISupportsRayTracing)
	{
		for (TSharedPtr<FD3D12Adapter>& Adapter : ChosenAdapters)
		{
			Adapter->InitializeRayTracing();
		}
	}

	if (GRHISupportsRHIThread)
	{
		SetupRecursiveResources();
	}
}

bool FD3D12DynamicRHI::IsQuadBufferStereoEnabled() const
{
	return bIsQuadBufferStereoEnabled;
}

void FD3D12DynamicRHI::DisableQuadBufferStereo()
{
	bIsQuadBufferStereoEnabled = false;
}

int32 FD3D12DynamicRHI::GetResourceBarrierBatchSizeLimit()
{
	return INT32_MAX;
}


void FD3D12Device::CreateSamplerInternal(const D3D12_SAMPLER_DESC& Desc, D3D12_CPU_DESCRIPTOR_HANDLE Descriptor)
{
	GetDevice()->CreateSampler(&Desc, Descriptor);
}

#if D3D12_RHI_RAYTRACING
TRefCountPtr<ID3D12StateObject> FD3D12Device::DeserializeRayTracingStateObject(D3D12_SHADER_BYTECODE Bytecode, ID3D12RootSignature* RootSignature)
{
	checkNoEntry();
	TRefCountPtr<ID3D12StateObject> Result;
	return Result;
}

bool FD3D12Device::GetRayTracingPipelineInfo(ID3D12StateObject* Pipeline, FD3D12RayTracingPipelineInfo* OutInfo)
{
	// Return a safe default result on Windows, as there is no API to query interesting pipeline metrics.
	FD3D12RayTracingPipelineInfo Result = {};
	*OutInfo = Result;

	return false;
}

#endif // D3D12_RHI_RAYTRACING

/**
 *	Retrieve available screen resolutions.
 *
 *	@param	Resolutions			TArray<FScreenResolutionRHI> parameter that will be filled in.
 *	@param	bIgnoreRefreshRate	If true, ignore refresh rates.
 *
 *	@return	bool				true if successfully filled the array
 */
bool FD3D12DynamicRHI::RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
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

	FD3D12Adapter& ChosenAdapter = GetAdapter();

	HRESULT HResult = S_OK;
	TRefCountPtr<IDXGIAdapter> Adapter;
	//TODO: should only be called on display out device
	HResult = ChosenAdapter.EnumAdapters(Adapter.GetInitReference());

	if (DXGI_ERROR_NOT_FOUND == HResult)
	{
		return false;
	}
	if (FAILED(HResult))
	{
		return false;
	}

	// get the description of the adapter
	DXGI_ADAPTER_DESC AdapterDesc;
	if (FAILED(Adapter->GetDesc(&AdapterDesc)))
	{
		return false;
	}

	int32 CurrentOutput = 0;
	do
	{
		TRefCountPtr<IDXGIOutput> Output;
		HResult = Adapter->EnumOutputs(CurrentOutput, Output.GetInitReference());
		if (DXGI_ERROR_NOT_FOUND == HResult)
		{
			break;
		}
		if (FAILED(HResult))
		{
			return false;
		}

		// TODO: GetDisplayModeList is a terribly SLOW call.  It can take up to a second per invocation.
		//  We might want to work around some DXGI badness here.
		DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uint32 NumModes = 0;
		HResult = Output->GetDisplayModeList(Format, 0, &NumModes, nullptr);
		if (HResult == DXGI_ERROR_NOT_FOUND)
		{
			++CurrentOutput;
			continue;
		}
		else if (HResult == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
		{
			UE_LOG(LogD3D12RHI, Warning,
				TEXT("RHIGetAvailableResolutions() can not be used over a remote desktop configuration")
				);
			return false;
		}

		checkf(NumModes > 0, TEXT("No display modes found for the standard format DXGI_FORMAT_R8G8B8A8_UNORM!"));

		DXGI_MODE_DESC* ModeList = new DXGI_MODE_DESC[NumModes];
		VERIFYD3D12RESULT(Output->GetDisplayModeList(Format, 0, &NumModes, ModeList));

		for (uint32 m = 0; m < NumModes; m++)
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
	} while (CurrentOutput < 1);

	return true;
}

void FWindowsD3D12Adapter::CreateCommandSignatures()
{
	ID3D12Device* Device = GetD3DDevice();

#if D3D12_RHI_RAYTRACING

	if (GRHISupportsRayTracing)
	{
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 D3D12Caps5 = {};
		if (SUCCEEDED(Device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &D3D12Caps5, sizeof(D3D12Caps5))))
		{
			if (D3D12Caps5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_1)
			{
				GRHISupportsRayTracingDispatchIndirect = true;
			}
		}
	}

	if (GRHISupportsRayTracingDispatchIndirect)
	{
		D3D12_COMMAND_SIGNATURE_DESC SignatureDesc = {};
		SignatureDesc.NumArgumentDescs = 1;
		SignatureDesc.ByteStride = sizeof(D3D12_DISPATCH_RAYS_DESC);
		SignatureDesc.NodeMask = FRHIGPUMask::All().GetNative();

		D3D12_INDIRECT_ARGUMENT_DESC ArgumentDesc[1] = {};
		ArgumentDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_RAYS;
		SignatureDesc.pArgumentDescs = ArgumentDesc;

		checkf(DispatchRaysIndirectCommandSignature == nullptr, TEXT("Indirect ray tracing dispatch command signature is expected to be initialized by FWindowsD3D12Adapter."));
		VERIFYD3D12RESULT(Device->CreateCommandSignature(&SignatureDesc, nullptr, IID_PPV_ARGS(DispatchRaysIndirectCommandSignature.GetInitReference())));
	}

#endif // D3D12_RHI_RAYTRACING

	// Create windows-specific indirect compute dispatch command signature
	{
		D3D12_COMMAND_SIGNATURE_DESC CommandSignatureDesc = {};
		CommandSignatureDesc.NumArgumentDescs = 1;
		CommandSignatureDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
		CommandSignatureDesc.NodeMask = FRHIGPUMask::All().GetNative();

		D3D12_INDIRECT_ARGUMENT_DESC IndirectParameterDesc[1] = {};
		IndirectParameterDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
		CommandSignatureDesc.pArgumentDescs = IndirectParameterDesc;

		checkf(DispatchIndirectComputeCommandSignature == nullptr, TEXT("Indirect compute dispatch command signature is expected to be initialized by FWindowsD3D12Adapter."));
		VERIFYD3D12RESULT(Device->CreateCommandSignature(&CommandSignatureDesc, nullptr, IID_PPV_ARGS(DispatchIndirectComputeCommandSignature.GetInitReference())));

#if PLATFORM_SUPPORTS_MESH_SHADERS
		if (GRHISupportsMeshShadersTier0)
		{
			IndirectParameterDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;
			CommandSignatureDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
			VERIFYD3D12RESULT(Device->CreateCommandSignature(&CommandSignatureDesc, nullptr, IID_PPV_ARGS(DispatchIndirectMeshCommandSignature.GetInitReference())));
		}
#endif
	}

	// Create all the generic / cross-platform command signatures

	FD3D12Adapter::CreateCommandSignatures();
}

TUniquePtr<FD3D12DiagnosticBuffer> FD3D12Device::CreateDiagnosticBuffer(const D3D12_RESOURCE_DESC& Desc, const TCHAR* Name)
{
	TRefCountPtr<ID3D12Device3> D3D12Device3;
	HRESULT hr = GetDevice()->QueryInterface(IID_PPV_ARGS(D3D12Device3.GetInitReference()));
	if (SUCCEEDED(hr))
	{
		void* BreadCrumbResourceAddress = VirtualAlloc(nullptr, Desc.Width, MEM_COMMIT, PAGE_READWRITE);
		if (BreadCrumbResourceAddress)
		{
			ID3D12Heap* D3D12Heap = nullptr;
			hr = D3D12Device3->OpenExistingHeapFromAddress(BreadCrumbResourceAddress, IID_PPV_ARGS(&D3D12Heap));
			if (SUCCEEDED(hr))
			{
				TRefCountPtr<FD3D12Heap> BreadCrumbHeap = new FD3D12Heap(this, GetVisibilityMask());
				BreadCrumbHeap->SetHeap(D3D12Heap, TEXT("DiagnosticBuffer"));

				TRefCountPtr<FD3D12Resource> BreadCrumbResource;
				hr = GetParentAdapter()->CreatePlacedResource(Desc, BreadCrumbHeap.GetReference(), 0, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, BreadCrumbResource.GetInitReference(), Name, false);
				if (SUCCEEDED(hr))
				{
					UE_LOG(LogD3D12RHI, Log, TEXT("[GPUBreadCrumb] Successfully setup breadcrumb resource for %s"), Name);

					return MakeUnique<FD3D12DiagnosticBuffer>(MoveTemp(BreadCrumbHeap), MoveTemp(BreadCrumbResource), BreadCrumbResourceAddress, BreadCrumbResource->GetGPUVirtualAddress());
				}
				else
				{
					BreadCrumbHeap.SafeRelease();
					VirtualFree(BreadCrumbResourceAddress, 0, MEM_RELEASE);
					UE_LOG(LogD3D12RHI, Warning, TEXT("[GPUBreadCrumb] Failed to CreatePlacedResource, error: %x"), hr);
				}
			}
			else
			{
				VirtualFree(BreadCrumbResourceAddress, 0, MEM_RELEASE);
				UE_LOG(LogD3D12RHI, Warning, TEXT("[GPUBreadCrumb] Failed to OpenExistingHeapFromAddress, error: %x"), hr);
			}
		}
		else
		{
			UE_LOG(LogD3D12RHI, Warning, TEXT("[GPUBreadCrumb] Failed to VirtualAlloc resource memory"));
		}
	}
	else
	{
		UE_LOG(LogD3D12RHI, Warning, TEXT("[GPUBreadCrumb] ID3D12Device3 not available (only available on Windows 10 1709+), error: %x"), hr);
	}

	return nullptr;
}

FD3D12DiagnosticBuffer::~FD3D12DiagnosticBuffer()
{
	Resource.SafeRelease();
	Heap.SafeRelease();

	VirtualFree(CpuAddress, 0, MEM_RELEASE);
	CpuAddress = nullptr;
	GpuAddress = 0;
}
