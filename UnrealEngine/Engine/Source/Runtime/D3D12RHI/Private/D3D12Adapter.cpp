// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Adapter.cpp:D3D12 Adapter implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "Misc/CommandLine.h"
#include "Misc/EngineVersion.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsPlatformMisc.h"
#include "Windows/WindowsPlatformStackWalk.h"
#endif
#include "Modules/ModuleManager.h"

#if !PLATFORM_CPU_ARM_FAMILY && (PLATFORM_WINDOWS)
	#include "amd_ags.h"
#endif
#include "Windows/HideWindowsPlatformTypes.h"

#if ENABLE_RESIDENCY_MANAGEMENT
bool GEnableResidencyManagement = true;
static TAutoConsoleVariable<int32> CVarResidencyManagement(
	TEXT("D3D12.ResidencyManagement"),
	1,
	TEXT("Controls whether D3D12 resource residency management is active (default = on)."),
	ECVF_ReadOnly
);
#endif // ENABLE_RESIDENCY_MANAGEMENT

#if TRACK_RESOURCE_ALLOCATIONS
int32 GTrackedReleasedAllocationFrameRetention = 100;
static FAutoConsoleVariableRef CTrackedReleasedAllocationFrameRetention(
	TEXT("D3D12.TrackedReleasedAllocationFrameRetention"),
	GTrackedReleasedAllocationFrameRetention,
	TEXT("Amount of frames for which we keep freed allocation data around when resource tracking is enabled"),
	ECVF_RenderThreadSafe
);
#endif

#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
static int32 GD3D12EnableGPUBreadCrumbs = 0;
static int32 GD3D12EnableNvAftermath = 0;
static int32 GD3D12EnableDRED = 0;
#else
static int32 GD3D12EnableGPUBreadCrumbs = 1;
static int32 GD3D12EnableNvAftermath = 1;
static int32 GD3D12EnableDRED = 0;
#endif // UE_BUILD_SHIPPING || UE_BUILD_TEST

static FAutoConsoleVariableRef CVarD3D12EnableGPUBreadCrumbs(
	TEXT("r.D3D12.BreadCrumbs"),
	GD3D12EnableGPUBreadCrumbs,
	TEXT("Enable minimal overhead GPU Breadcrumbs to track the current GPU state and logs information what operations the GPU executed last.\n"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static FAutoConsoleVariableRef CVarD3D12EnableNvAftermath(
	TEXT("r.D3D12.NvAfterMath"),
	GD3D12EnableNvAftermath,
	TEXT("Enable NvAftermath to track the current GPU state and logs information what operations the GPU executed last.\n")
	TEXT("Only works on nVidia hardware and will dump GPU crashdumps as well.\n"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

static FAutoConsoleVariableRef CVarD3D12EnableDRED(
	TEXT("r.D3D12.DRED"),
	GD3D12EnableDRED,
	TEXT("Enable DRED GPU Crash debugging mode to track the current GPU state and logs information what operations the GPU executed last.")
	TEXT("Has GPU overhead but gives the most information on the current GPU state when it crashes or hangs.\n"),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

bool GD3D12TrackAllAlocations = false;
static TAutoConsoleVariable<int32> CVarD3D12TrackAllAllocations(
	TEXT("D3D12.TrackAllAllocations"),
	GD3D12TrackAllAlocations,
	TEXT("Controls whether D3D12 RHI should track all allocation information (default = off)."),
	ECVF_ReadOnly
);
#endif // PLATFORM_WINDOWS || PLATFORM_HOLOLENS

#if D3D12_SUPPORTS_INFO_QUEUE
static bool CheckD3DStoredMessages()
{
	bool bResult = false;

	TRefCountPtr<ID3D12Debug> d3dDebug;
	if (SUCCEEDED(D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)d3dDebug.GetInitReference())))
	{
		FD3D12DynamicRHI* D3D12RHI = FD3D12DynamicRHI::GetD3DRHI();
		TRefCountPtr<ID3D12InfoQueue> d3dInfoQueue;
		if (SUCCEEDED(D3D12RHI->GetAdapter().GetD3DDevice()->QueryInterface(__uuidof(ID3D12InfoQueue), (void**)d3dInfoQueue.GetInitReference())))
		{
			D3D12_MESSAGE* d3dMessage = nullptr;
			SIZE_T AllocateSize = 0;

			int StoredMessageCount = d3dInfoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
			for (int MessageIndex = 0; MessageIndex < StoredMessageCount; MessageIndex++)
			{
				SIZE_T MessageLength = 0;
				HRESULT hr = d3dInfoQueue->GetMessage(MessageIndex, nullptr, &MessageLength);

				// Ideally the exception handler should not allocate any memory because it could fail
				// and can cause another exception to be triggered and possible even cause a deadlock.
				// But for these D3D error message it should be fine right now because they are requested
				// exceptions when making an error against the API.
				// Not allocating memory for the messages is easy (cache memory in Adapter), but ANSI_TO_TCHAR
				// and UE_LOG will also allocate memory and aren't that easy to fix.

				// realloc the message
				if (MessageLength > AllocateSize)
				{
					if (d3dMessage)
					{
						FMemory::Free(d3dMessage);
						d3dMessage = nullptr;
						AllocateSize = 0;
					}

					d3dMessage = (D3D12_MESSAGE*)FMemory::Malloc(MessageLength);
					AllocateSize = MessageLength;
				}

				if (d3dMessage)
				{
					// get the actual message data from the queue
					hr = d3dInfoQueue->GetMessage(MessageIndex, d3dMessage, &MessageLength);

					if (d3dMessage->Severity == D3D12_MESSAGE_SEVERITY_ERROR)
					{
						UE_LOG(LogD3D12RHI, Error, TEXT("%s"), ANSI_TO_TCHAR(d3dMessage->pDescription));
					}
					else if (d3dMessage->Severity == D3D12_MESSAGE_SEVERITY_WARNING)
					{
						UE_LOG(LogD3D12RHI, Warning, TEXT("%s"), ANSI_TO_TCHAR(d3dMessage->pDescription));
					}
					else 
					{
						UE_LOG(LogD3D12RHI, Log, TEXT("%s"), ANSI_TO_TCHAR(d3dMessage->pDescription));
					}
				}

				// we got messages
				bResult = true;
			}

			if (AllocateSize > 0)
			{
				FMemory::Free(d3dMessage);
			}
		}
	}

	return bResult;
}
#endif // #if D3D12_SUPPORTS_INFO_QUEUE

#if PLATFORM_WINDOWS

/** Handle d3d messages and write them to the log file **/
static LONG __stdcall D3DVectoredExceptionHandler(EXCEPTION_POINTERS* InInfo)
{
	// Only handle D3D error codes here
	if (InInfo->ExceptionRecord->ExceptionCode == _FACDXGI)
	{
		if (CheckD3DStoredMessages())
		{
			if (FPlatformMisc::IsDebuggerPresent())
			{
				// when we get here, then it means that BreakOnSeverity was set for this error message, so request the debug break here as well
				// when the debugger is attached
				UE_DEBUG_BREAK();
			}
		}

		// Handles the exception
		return EXCEPTION_CONTINUE_EXECUTION;
	}

	// continue searching
	return EXCEPTION_CONTINUE_SEARCH;
}

#endif // #if PLATFORM_WINDOWS


FTransientUniformBufferAllocator::~FTransientUniformBufferAllocator()
{
	if (Adapter)
	{
		Adapter->ReleaseTransientUniformBufferAllocator(this);
	}
}

void FTransientUniformBufferAllocator::Cleanup()
{
	ClearResource();
	Adapter = nullptr;
}

FD3D12AdapterDesc::FD3D12AdapterDesc() = default;

FD3D12AdapterDesc::FD3D12AdapterDesc(const DXGI_ADAPTER_DESC& InDesc, int32 InAdapterIndex, const FD3D12DeviceBasicInfo& DeviceInfo)
	: Desc(InDesc)
	, AdapterIndex(InAdapterIndex)
	, MaxSupportedFeatureLevel(DeviceInfo.MaxFeatureLevel)
	, MaxSupportedShaderModel(DeviceInfo.MaxShaderModel)
	, ResourceBindingTier(DeviceInfo.ResourceBindingTier)
	, ResourceHeapTier(DeviceInfo.ResourceHeapTier)
	, MaxRHIFeatureLevel(DeviceInfo.MaxRHIFeatureLevel)
{
}

bool FD3D12AdapterDesc::IsValid() const
{
	return MaxSupportedFeatureLevel != (D3D_FEATURE_LEVEL)0 && AdapterIndex >= 0;
}

#if DXGI_MAX_FACTORY_INTERFACE >= 6
HRESULT FD3D12AdapterDesc::EnumAdapters(int32 AdapterIndex, DXGI_GPU_PREFERENCE GpuPreference, IDXGIFactory2* DxgiFactory2, IDXGIFactory6* DxgiFactory6, IDXGIAdapter** TempAdapter)
{
	if (!DxgiFactory6 || GpuPreference == DXGI_GPU_PREFERENCE_UNSPECIFIED)
	{
		return DxgiFactory2->EnumAdapters(AdapterIndex, TempAdapter);
	}
	else
	{
		return DxgiFactory6->EnumAdapterByGpuPreference(AdapterIndex, GpuPreference, IID_PPV_ARGS(TempAdapter));
	}
}

HRESULT FD3D12AdapterDesc::EnumAdapters(IDXGIFactory2* DxgiFactory2, IDXGIFactory6* DxgiFactory6, IDXGIAdapter** TempAdapter) const
{
	return EnumAdapters(AdapterIndex, GpuPreference, DxgiFactory2, DxgiFactory6, TempAdapter);
}
#endif

FD3D12Adapter::FD3D12Adapter(FD3D12AdapterDesc& DescIn)
	: Desc(DescIn)
	, RootSignatureManager(this)
	, PipelineStateCache(this)
	, DefaultContextRedirector(this, ED3D12QueueType::Direct, true)
#if USE_STATIC_ROOT_SIGNATURE
	, StaticGraphicsRootSignature(this)
	, StaticComputeRootSignature(this)
	, StaticRayTracingGlobalRootSignature(this)
	, StaticRayTracingLocalRootSignature(this)
#endif
{
	FMemory::Memzero(&UploadHeapAllocator, sizeof(UploadHeapAllocator));
	FMemory::Memzero(&Devices, sizeof(Devices));

	uint32 MaxGPUCount = 1; // By default, multi-gpu is disabled.
#if WITH_MGPU
	if (!FParse::Value(FCommandLine::Get(), TEXT("MaxGPUCount="), MaxGPUCount))
	{
		// If there is a mode token in the command line, enable multi-gpu.
		if (FParse::Param(FCommandLine::Get(), TEXT("AFR")))
		{
			MaxGPUCount = MAX_NUM_GPUS;
		}
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("VMGPU")))
	{
		GVirtualMGPU = 1;
		UE_LOG(LogD3D12RHI, Log, TEXT("Enabling virtual multi-GPU mode"), Desc.NumDeviceNodes);
	}
#endif

	if (GVirtualMGPU)
	{
		Desc.NumDeviceNodes = FMath::Min<uint32>(MaxGPUCount, MAX_NUM_GPUS);
	}
	else
	{
		Desc.NumDeviceNodes = FMath::Min3<uint32>(Desc.NumDeviceNodes, MaxGPUCount, (uint32)MAX_NUM_GPUS);
	}
}

/** Callback function called when the GPU crashes, when Aftermath is enabled */
static void D3D12AftermathCrashCallback(const void* InGPUCrashDump, const uint32_t InGPUCrashDumpSize, void* InUserData)
{
	// Forward to shared function which is also called when DEVICE_LOST return value is given
	D3D12RHI::TerminateOnGPUCrash(nullptr, InGPUCrashDump, InGPUCrashDumpSize);
}


void FD3D12Adapter::CreateRootDevice(bool bWithDebug)
{
	const bool bAllowVendorDevice = !FParse::Param(FCommandLine::Get(), TEXT("novendordevice"));

	// -d3ddebug is always allowed on Windows, but only allowed in non-shipping builds on other platforms.
	// -gpuvalidation is only supported on Windows.
#if PLATFORM_WINDOWS || !UE_BUILD_SHIPPING
	bool bWithGPUValidation = PLATFORM_WINDOWS && (FParse::Param(FCommandLine::Get(), TEXT("d3d12gpuvalidation")) || FParse::Param(FCommandLine::Get(), TEXT("gpuvalidation")));
	// If GPU validation is requested, automatically enable the debug layer.
	bWithDebug |= bWithGPUValidation;
	if (bWithDebug)
	{
		TRefCountPtr<ID3D12Debug> DebugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(DebugController.GetInitReference()))))
		{
			DebugController->EnableDebugLayer();
			bDebugDevice = true;

#if PLATFORM_WINDOWS
			if (bWithGPUValidation)
			{
				TRefCountPtr<ID3D12Debug1> DebugController1;
				VERIFYD3D12RESULT(DebugController->QueryInterface(IID_PPV_ARGS(DebugController1.GetInitReference())));
				DebugController1->SetEnableGPUBasedValidation(true);
				SetEmitDrawEvents(true);
			}
#endif
		}
		else
		{
			UE_LOG(LogD3D12RHI, Fatal, TEXT("The debug interface requires the D3D12 SDK Layers. Please install the Graphics Tools for Windows. See: https://docs.microsoft.com/en-us/windows/uwp/gaming/use-the-directx-runtime-and-visual-studio-graphics-diagnostic-features"));
		}
	}

	FGenericCrashContext::SetEngineData(TEXT("RHI.D3DDebug"), bWithDebug ? TEXT("true") : TEXT("false"));
	UE_LOG(LogD3D12RHI, Log, TEXT("InitD3DDevice: -D3DDebug = %s -D3D12GPUValidation = %s"), bWithDebug ? TEXT("on") : TEXT("off"), bWithGPUValidation ? TEXT("on") : TEXT("off"));
#endif

#if PLATFORM_WINDOWS || (PLATFORM_HOLOLENS && !UE_BUILD_SHIPPING && WITH_PIX_EVENT_RUNTIME)
	
    SetupGPUCrashDebuggingModesCommon();

#if NV_AFTERMATH
	if (IsRHIDeviceNVIDIA() && GDX12NVAfterMathModuleLoaded)
	{
		// GPUcrash dump handler must be attached prior to device creation
		if (EnumHasAnyFlags(GPUCrashDebuggingModes, ED3D12GPUCrashDebuggingModes::NvAftermath))
		{
			HANDLE CurrentThread = ::GetCurrentThread();

			GFSDK_Aftermath_Result Result = GFSDK_Aftermath_EnableGpuCrashDumps(
				GFSDK_Aftermath_Version_API,
				GFSDK_Aftermath_GpuCrashDumpWatchedApiFlags_DX,
				GFSDK_Aftermath_GpuCrashDumpFeatureFlags_Default,
				D3D12AftermathCrashCallback,
				nullptr, //Shader debug callback
				nullptr, // description callback
				CurrentThread); // user data

			if (Result == GFSDK_Aftermath_Result_Success)
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("[Aftermath] Aftermath crash dumping enabled"));

				// enable core Aftermath to set the init flags
				GDX12NVAfterMathEnabled = 1;
			}
			else
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("[Aftermath] Aftermath crash dumping failed to initialize (%x)"), Result);

				GDX12NVAfterMathEnabled = 0;
			}
		}
	}
#endif

	// Setup DRED if requested
	bool bDRED = false;
	bool bDREDContext = false;
	if (EnumHasAnyFlags(GPUCrashDebuggingModes, ED3D12GPUCrashDebuggingModes::DRED))
	{
		TRefCountPtr<ID3D12DeviceRemovedExtendedDataSettings> DredSettings;
		HRESULT hr = D3D12GetDebugInterface(IID_PPV_ARGS(DredSettings.GetInitReference()));

		// Can fail if not on correct Windows Version - needs 1903 or newer
		if (SUCCEEDED(hr))
		{
			// Turn on AutoBreadcrumbs and Page Fault reporting
			DredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
			DredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);

			bDRED = true;
			UE_LOG(LogD3D12RHI, Log, TEXT("[DRED] Dred enabled"));
		}
		else
		{
			UE_LOG(LogD3D12RHI, Log, TEXT("[DRED] DRED requested but interface was not found, hresult: %x. DRED only works on Windows 10 1903+."), hr);
		}

#ifdef __ID3D12DeviceRemovedExtendedDataSettings1_INTERFACE_DEFINED__
		TRefCountPtr<ID3D12DeviceRemovedExtendedDataSettings1> DredSettings1;
		hr = D3D12GetDebugInterface(IID_PPV_ARGS(DredSettings1.GetInitReference()));
		if (SUCCEEDED(hr))
		{
			DredSettings1->SetBreadcrumbContextEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
			bDREDContext = true;
			UE_LOG(LogD3D12RHI, Log, TEXT("[DRED] Dred breadcrumb context enabled"));
		}
#endif
	}

	FGenericCrashContext::SetEngineData(TEXT("RHI.DRED"), bDRED ? TEXT("true") : TEXT("false"));
	FGenericCrashContext::SetEngineData(TEXT("RHI.DREDContext"), bDREDContext ? TEXT("true") : TEXT("false"));

#endif // PLATFORM_WINDOWS || (PLATFORM_HOLOLENS && !UE_BUILD_SHIPPING && WITH_PIX_EVENT_RUNTIME)

#if USE_PIX
	UE_LOG(LogD3D12RHI, Log, TEXT("Emitting draw events for PIX profiling."));
	SetEmitDrawEvents(true);
#endif

	CreateDXGIFactory(bWithDebug);

	// QI for the Adapter
	TRefCountPtr<IDXGIAdapter> TempAdapter;

	EnumAdapters(TempAdapter.GetInitReference());

	VERIFYD3D12RESULT(TempAdapter->QueryInterface(IID_PPV_ARGS(DxgiAdapter.GetInitReference())));

	bool bDeviceCreated = false;
#if !PLATFORM_CPU_ARM_FAMILY && (PLATFORM_WINDOWS)
	if (IsRHIDeviceAMD() && FD3D12DynamicRHI::GetD3DRHI()->GetAmdAgsContext())
	{
		auto* CVarShaderDevelopmentMode = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.ShaderDevelopmentMode"));
		auto* CVarDisableEngineAndAppRegistration = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DisableEngineAndAppRegistration"));

		const bool bDisableEngineRegistration = (CVarShaderDevelopmentMode && CVarShaderDevelopmentMode->GetValueOnAnyThread() != 0) ||
			(CVarDisableEngineAndAppRegistration && CVarDisableEngineAndAppRegistration->GetValueOnAnyThread() != 0);
		const bool bDisableAppRegistration = bDisableEngineRegistration || !FApp::HasProjectName();

		// Creating the Direct3D device with AGS registration and extensions.
		AGSDX12DeviceCreationParams AmdDeviceCreationParams = {
			GetAdapter(),											// IDXGIAdapter*               pAdapter;
			__uuidof(**(RootDevice.GetInitReference())),			// IID                         iid;
			GetFeatureLevel(),										// D3D_FEATURE_LEVEL           FeatureLevel;
		};

		AGSDX12ExtensionParams AmdExtensionParams;
		FMemory::Memzero(&AmdExtensionParams, sizeof(AmdExtensionParams));

		// Register the engine name with the AMD driver, e.g. "UnrealEngine4.19", unless disabled
		// (note: to specify nothing for pEngineName below, you need to pass an empty string, not a null pointer)
		FString EngineName = FApp::GetEpicProductIdentifier() + FEngineVersion::Current().ToString(EVersionComponent::Minor);
		AmdExtensionParams.pEngineName = bDisableEngineRegistration ? TEXT("") : *EngineName;
		AmdExtensionParams.engineVersion = AGS_UNSPECIFIED_VERSION;

		// Register the project name with the AMD driver, unless disabled or no project name
		// (note: to specify nothing for pAppName below, you need to pass an empty string, not a null pointer)
		AmdExtensionParams.pAppName = bDisableAppRegistration ? TEXT("") : FApp::GetProjectName();
		AmdExtensionParams.appVersion = AGS_UNSPECIFIED_VERSION;

		// From Shaders\Shared\ThirdParty\AMD\ags_shader_intrinsics_dx12.h, the default dummy UAV used
		// to access shader intrinsics is declared as below:
		// RWByteAddressBuffer AmdExtD3DShaderIntrinsicsUAV : register(u0, AmdExtD3DShaderIntrinsicsSpaceId);
		// So, use slot 0 here to match.
		AmdExtensionParams.uavSlot = 0;

		AGSDX12ReturnedParams DeviceCreationReturnedParams;
		FMemory::Memzero(&DeviceCreationReturnedParams, sizeof(DeviceCreationReturnedParams));
		AGSReturnCode DeviceCreation = agsDriverExtensionsDX12_CreateDevice(
			FD3D12DynamicRHI::GetD3DRHI()->GetAmdAgsContext(),
			&AmdDeviceCreationParams,
			&AmdExtensionParams,
			&DeviceCreationReturnedParams
		);

		if (DeviceCreation == AGS_SUCCESS)
		{
			RootDevice = DeviceCreationReturnedParams.pDevice;
			{
				static_assert(sizeof(AGSDX12ReturnedParams::ExtensionsSupported) == sizeof(uint32));
				uint32 AMDSupportedExtensionFlags;
				FMemory::Memcpy(&AMDSupportedExtensionFlags, &DeviceCreationReturnedParams.extensionsSupported, sizeof(uint32));
				FD3D12DynamicRHI::GetD3DRHI()->SetAmdSupportedExtensionFlags(AMDSupportedExtensionFlags);
			}
			bDeviceCreated = true;
		}
	}
#endif

	if (!bDeviceCreated)
	{
		// Creating the Direct3D device.
		VERIFYD3D12RESULT(D3D12CreateDevice(
			GetAdapter(),
			GetFeatureLevel(),
			IID_PPV_ARGS(RootDevice.GetInitReference())
		));
	}

#if ENABLE_RESIDENCY_MANAGEMENT
	if (!CVarResidencyManagement.GetValueOnAnyThread())
	{
		UE_LOG(LogD3D12RHI, Log, TEXT("D3D12 resource residency management is disabled."));
		GEnableResidencyManagement = false;
	}
#endif // ENABLE_RESIDENCY_MANAGEMENT

#if NV_AFTERMATH
	// Enable aftermath when GPU crash debugging is enabled
	if (EnumHasAnyFlags(GPUCrashDebuggingModes, ED3D12GPUCrashDebuggingModes::NvAftermath) && GDX12NVAfterMathEnabled)
	{
		if (IsRHIDeviceNVIDIA() && bAllowVendorDevice)
		{
			static IConsoleVariable* MarkersCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUCrashDebugging.Aftermath.Markers"));
			static IConsoleVariable* CallstackCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUCrashDebugging.Aftermath.Callstack"));
			static IConsoleVariable* ResourcesCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUCrashDebugging.Aftermath.ResourceTracking"));
			static IConsoleVariable* TrackAllCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUCrashDebugging.Aftermath.TrackAll"));

			const bool bEnableMarkers = FParse::Param(FCommandLine::Get(), TEXT("aftermathmarkers")) || (MarkersCVar && MarkersCVar->GetInt());
			const bool bEnableCallstack = FParse::Param(FCommandLine::Get(), TEXT("aftermathcallstack")) || (CallstackCVar && CallstackCVar->GetInt());
			const bool bEnableResources = FParse::Param(FCommandLine::Get(), TEXT("aftermathresources")) || (ResourcesCVar && ResourcesCVar->GetInt());
			const bool bEnableAll = FParse::Param(FCommandLine::Get(), TEXT("aftermathall")) || (TrackAllCVar && TrackAllCVar->GetInt());

			uint32 Flags = GFSDK_Aftermath_FeatureFlags_Minimum;

			Flags |= bEnableMarkers ? GFSDK_Aftermath_FeatureFlags_EnableMarkers : 0;
			Flags |= bEnableCallstack ? GFSDK_Aftermath_FeatureFlags_CallStackCapturing : 0;
			Flags |= bEnableResources ? GFSDK_Aftermath_FeatureFlags_EnableResourceTracking : 0;
			Flags |= bEnableAll ? GFSDK_Aftermath_FeatureFlags_Maximum : 0;

			GFSDK_Aftermath_Result Result = GFSDK_Aftermath_DX12_Initialize(GFSDK_Aftermath_Version_API, (GFSDK_Aftermath_FeatureFlags)Flags, RootDevice);
			if (Result == GFSDK_Aftermath_Result_Success)
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("[Aftermath] Aftermath enabled and primed"));
			}
			else
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("[Aftermath] Aftermath enabled but failed to initialize (%x)"), Result);
				GDX12NVAfterMathEnabled = 0;
			}

			if (GDX12NVAfterMathEnabled && (bEnableMarkers || bEnableAll))
			{
				SetEmitDrawEvents(true);
				GDX12NVAfterMathMarkers = 1;
			}

			GDX12NVAfterMathTrackResources = bEnableResources || bEnableAll;
			if (GDX12NVAfterMathEnabled && GDX12NVAfterMathTrackResources)
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("[Aftermath] Aftermath resource tracking enabled"));
			}
		}
		else
		{
			GDX12NVAfterMathEnabled = 0;
			UE_LOG(LogD3D12RHI, Warning, TEXT("[Aftermath] Skipping aftermath initialization on non-Nvidia device"));
		}
	}
	else
	{
		GDX12NVAfterMathEnabled = 0;
	}

	FGenericCrashContext::SetEngineData(TEXT("RHI.Aftermath"), GDX12NVAfterMathEnabled ? TEXT("true") : TEXT("false"));
#endif

#if PLATFORM_WINDOWS
	if (bWithDebug)
	{
		// add vectored exception handler to write the debug device warning & error messages to the log
		ExceptionHandlerHandle = AddVectoredExceptionHandler(1, D3DVectoredExceptionHandler);
	}
#endif // PLATFORM_WINDOWS

#if D3D12_SUPPORTS_DXGI_DEBUG
	if (bWithDebug)
	{
		// Manually load dxgi debug if available
		DxgiDebugDllHandle = (HMODULE)FPlatformProcess::GetDllHandle(TEXT("dxgidebug.dll"));
		if (DxgiDebugDllHandle)
		{
			typedef HRESULT(WINAPI* FDXGIGetDebugInterface)(REFIID, void**);
			FDXGIGetDebugInterface DXGIGetDebugInterfaceFnPtr = (FDXGIGetDebugInterface)(void*)(GetProcAddress(DxgiDebugDllHandle, "DXGIGetDebugInterface"));
			if (DXGIGetDebugInterfaceFnPtr != nullptr)
			{
				DXGIGetDebugInterfaceFnPtr(IID_PPV_ARGS(DXGIDebug.GetInitReference()));
			}
		}
	}
#endif //  (PLATFORM_WINDOWS || PLATFORM_HOLOLENS)

#if UE_BUILD_DEBUG	&& D3D12_SUPPORTS_INFO_QUEUE
	//break on debug
	TRefCountPtr<ID3D12Debug> d3dDebug;
	if (SUCCEEDED(RootDevice->QueryInterface(__uuidof(ID3D12Debug), (void**)d3dDebug.GetInitReference())))
	{
		TRefCountPtr<ID3D12InfoQueue> d3dInfoQueue;
		if (SUCCEEDED(d3dDebug->QueryInterface(__uuidof(ID3D12InfoQueue), (void**)d3dInfoQueue.GetInitReference())))
		{
			d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			//d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		}
	}
#endif

#if !(UE_BUILD_SHIPPING && WITH_EDITOR) && D3D12_SUPPORTS_INFO_QUEUE
	// Add some filter outs for known debug spew messages (that we don't care about)
	if (bWithDebug)
	{
		ID3D12InfoQueue *pd3dInfoQueue = nullptr;
		VERIFYD3D12RESULT(RootDevice->QueryInterface(__uuidof(ID3D12InfoQueue), (void**)&pd3dInfoQueue));
		if (pd3dInfoQueue)
		{
			D3D12_INFO_QUEUE_FILTER NewFilter;
			FMemory::Memzero(&NewFilter, sizeof(NewFilter));

			// Turn off info msgs as these get really spewy
			D3D12_MESSAGE_SEVERITY DenySeverity = D3D12_MESSAGE_SEVERITY_INFO;
			NewFilter.DenyList.NumSeverities = 1;
			NewFilter.DenyList.pSeverityList = &DenySeverity;

			// Be sure to carefully comment the reason for any additions here!  Someone should be able to look at it later and get an idea of whether it is still necessary.
			TArray<D3D12_MESSAGE_ID, TInlineAllocator<16>> DenyIds = {

				// The Pixel Shader expects a Render Target View bound to slot 0, but the PSO indicates that none will be bound.
				// This typically happens when a non-depth-only pixel shader is used for depth-only rendering.
				D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_RENDERTARGETVIEW_NOT_SET,

				// QUERY_BEGIN_ABANDONING_PREVIOUS_RESULTS - The RHI exposes the interface to make and issue queries and a separate interface to use that data.
				//		Currently there is a situation where queries are issued and the results may be ignored on purpose.  Filtering out this message so it doesn't
				//		swarm the debug spew and mask other important warnings
				//D3D12_MESSAGE_ID_QUERY_BEGIN_ABANDONING_PREVIOUS_RESULTS,
				//D3D12_MESSAGE_ID_QUERY_END_ABANDONING_PREVIOUS_RESULTS,

				// D3D12_MESSAGE_ID_CREATEINPUTLAYOUT_EMPTY_LAYOUT - This is a warning that gets triggered if you use a null vertex declaration,
				//       which we want to do when the vertex shader is generating vertices based on ID.
				D3D12_MESSAGE_ID_CREATEINPUTLAYOUT_EMPTY_LAYOUT,

				// D3D12_MESSAGE_ID_COMMAND_LIST_DRAW_INDEX_BUFFER_TOO_SMALL - This warning gets triggered by Slate draws which are actually using a valid index range.
				//		The invalid warning seems to only happen when VS 2012 is installed.  Reported to MS.  
				//		There is now an assert in DrawIndexedPrimitive to catch any valid errors reading from the index buffer outside of range.
				D3D12_MESSAGE_ID_COMMAND_LIST_DRAW_INDEX_BUFFER_TOO_SMALL,

				// D3D12_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET - This warning gets triggered by shadow depth rendering because the shader outputs
				//		a color but we don't bind a color render target. That is safe as writes to unbound render targets are discarded.
				//		Also, batched elements triggers it when rendering outside of scene rendering as it outputs to the GBuffer containing normals which is not bound.
				//(D3D12_MESSAGE_ID)3146081, // D3D12_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET,
				// BUGBUG: There is a D3D12_MESSAGE_ID_DEVICE_DRAW_DEPTHSTENCILVIEW_NOT_SET, why not one for RT?

				// D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE/D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE - 
				//      This warning gets triggered by ClearDepthStencilView/ClearRenderTargetView because when the resource was created
				//      it wasn't passed an optimized clear color (see CreateCommitedResource). This shows up a lot and is very noisy.
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
				D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,

				// D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_GPU_WRITTEN_READBACK_RESOURCE_MAPPED - This warning gets triggered by ExecuteCommandLists.
				//		if it contains a readback resource that still has mapped subresources when executing a command list that performs a copy operation to the resource.
				//		This may be ok if any data read from the readback resources was flushed by calling Unmap() after the resourcecopy operation completed.
				//		We intentionally keep the readback resources persistently mapped.
				D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_GPU_WRITTEN_READBACK_RESOURCE_MAPPED,

				// This shows up a lot and is very noisy. It would require changes to the resource tracking system
				// but will hopefully be resolved when the RHI switches to use the engine's resource tracking system.
				D3D12_MESSAGE_ID_RESOURCE_BARRIER_DUPLICATE_SUBRESOURCE_TRANSITIONS,

				// This error gets generated on the first run when you install a new driver. The code handles this error properly and resets the PipelineLibrary,
				// so we can safely ignore this message. It could possibly be avoided by adding driver version to the PSO cache filename, but an average user is unlikely
				// to be interested in keeping PSO caches associated with old drivers around on disk, so it's better to just reset.
				D3D12_MESSAGE_ID_CREATEPIPELINELIBRARY_DRIVERVERSIONMISMATCH,

				// D3D complain about overlapping GPU addresses when aliasing DataBuffers in the same command list when using the Transient Allocator - it looks like
				// it ignored the aliasing barriers to validate, and probably can't check them when called from IASetVertexBuffers because it only has GPU Virtual Addresses then
				D3D12_MESSAGE_ID_HEAP_ADDRESS_RANGE_INTERSECTS_MULTIPLE_BUFFERS,

				// Ignore draw vertex buffer not set or too small - these are warnings and if the shader doesn't read from it it's fine. This happens because vertex
				// buffers are not removed from the cache, but only get removed when another buffer is set at the same slot or when the buffer gets destroyed.
				D3D12_MESSAGE_ID_COMMAND_LIST_DRAW_VERTEX_BUFFER_NOT_SET,
				D3D12_MESSAGE_ID_COMMAND_LIST_DRAW_VERTEX_BUFFER_TOO_SMALL,

				// D3D12 complains when a buffer is created with a specific initial resource state while all buffers are currently created in COMMON state. The 
				// next transition is then done use state promotion. It's just a warning and we need to keep track of the correct initial state as well for upcoming
				// internal transitions.
				D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED,

#if ENABLE_RESIDENCY_MANAGEMENT
				// TODO: Remove this when the debug layers work for executions which are guarded by a fence
				D3D12_MESSAGE_ID_INVALID_USE_OF_NON_RESIDENT_RESOURCE,
#endif
			};

#if PLATFORM_DESKTOP
			if (!FWindowsPlatformMisc::VerifyWindowsVersion(10, 0, 18363))
			{
				// Ignore a known false positive error due to a bug in validation layer in certain older Windows versions
				DenyIds.Add(D3D12_MESSAGE_ID_COPY_DESCRIPTORS_INVALID_RANGES);
			}
#endif // PLATFORM_DESKTOP

			NewFilter.DenyList.NumIDs = DenyIds.Num();
			NewFilter.DenyList.pIDList = DenyIds.GetData();

			pd3dInfoQueue->PushStorageFilter(&NewFilter);

			// Break on D3D debug errors.
			pd3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

			// Enable this to break on a specific id in order to quickly get a callstack
			//pd3dInfoQueue->SetBreakOnID(D3D12_MESSAGE_ID_DEVICE_DRAW_CONSTANT_BUFFER_TOO_SMALL, true);

			if (FParse::Param(FCommandLine::Get(), TEXT("d3dbreakonwarning")))
			{
				pd3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
			}

			pd3dInfoQueue->Release();
		}
	}
#endif

#if WITH_MGPU
	GNumExplicitGPUsForRendering = 1;
	if (Desc.NumDeviceNodes > 1)
	{
		GNumExplicitGPUsForRendering = Desc.NumDeviceNodes;
		UE_LOG(LogD3D12RHI, Log, TEXT("Enabling multi-GPU with %d nodes"), Desc.NumDeviceNodes);
	}

	// Viewport ignores AFR if PresentGPU is specified.
	int32 Dummy;
	if (!FParse::Value(FCommandLine::Get(), TEXT("PresentGPU="), Dummy))
	{
		bool bWantsAFR = false;
		if (FParse::Value(FCommandLine::Get(), TEXT("NumAFRGroups="), GNumAlternateFrameRenderingGroups))
		{
			bWantsAFR = true;
		}
		else if (FParse::Param(FCommandLine::Get(), TEXT("AFR")))
		{
			bWantsAFR = true;
			GNumAlternateFrameRenderingGroups = GNumExplicitGPUsForRendering;
		}

		if (bWantsAFR)
		{
			if (GNumAlternateFrameRenderingGroups <= 1 || GNumAlternateFrameRenderingGroups > GNumExplicitGPUsForRendering)
			{
				UE_LOG(LogD3D12RHI, Error, TEXT("Cannot enable alternate frame rendering because NumAFRGroups (%u) must be > 1 and <= MaxGPUCount (%u)"), GNumAlternateFrameRenderingGroups, GNumExplicitGPUsForRendering);
				GNumAlternateFrameRenderingGroups = 1;
			}
			else if (GNumExplicitGPUsForRendering % GNumAlternateFrameRenderingGroups != 0)
			{
				UE_LOG(LogD3D12RHI, Error, TEXT("Cannot enable alternate frame rendering because MaxGPUCount (%u) must be evenly divisible by NumAFRGroups (%u)"), GNumExplicitGPUsForRendering, GNumAlternateFrameRenderingGroups);
				GNumAlternateFrameRenderingGroups = 1;
			}
			else
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("Enabling alternate frame rendering with %u AFR groups"), GNumAlternateFrameRenderingGroups);
			}
		}
	}
#endif
}

FD3D12TransientHeapCache& FD3D12Adapter::GetOrCreateTransientHeapCache()
{
	if (!TransientMemoryCache)
	{
		TransientMemoryCache = FD3D12TransientHeapCache::Create(this, FRHIGPUMask::All());
	}

	return static_cast<FD3D12TransientHeapCache&>(*TransientMemoryCache);
}

void FD3D12Adapter::InitializeDevices()
{
	check(IsInGameThread());

	// Wait for the rendering thread to go idle.
	FlushRenderingCommands();

	// Use a debug device if specified on the command line.
	bool bWithD3DDebug = D3D12RHI_ShouldCreateWithD3DDebug();

	// If we don't have a device yet, either because this is the first viewport, or the old device was removed, create a device.
	if (!RootDevice)
	{
		CreateRootDevice(bWithD3DDebug);

		// See if we can get any newer device interfaces (to use newer D3D12 features).
		if (D3D12RHI_ShouldForceCompatibility())
		{
			UE_LOG(LogD3D12RHI, Log, TEXT("Forcing D3D12 compatibility."));
		}
		else
		{
#if D3D12_MAX_DEVICE_INTERFACE >= 1
			if (SUCCEEDED(RootDevice->QueryInterface(IID_PPV_ARGS(RootDevice1.GetInitReference()))))
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("ID3D12Device1 is supported."));
			}
#endif
#if D3D12_MAX_DEVICE_INTERFACE >= 2
			if (SUCCEEDED(RootDevice->QueryInterface(IID_PPV_ARGS(RootDevice2.GetInitReference()))))
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("ID3D12Device2 is supported."));
			}

			if (RootDevice1 == nullptr || RootDevice2 == nullptr)
			{
				// Note: we require Windows 1703 in FD3D12DynamicRHIModule::IsSupported()
				// If we still lack support, the user's drivers could be out of date.
				UE_LOG(LogD3D12RHI, Fatal, TEXT("Missing full support for Direct3D 12. Please update to the latest drivers."));
			}
#endif
#if D3D12_MAX_DEVICE_INTERFACE >= 3
			if (SUCCEEDED(RootDevice->QueryInterface(IID_PPV_ARGS(RootDevice3.GetInitReference()))))
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("ID3D12Device3 is supported."));
			}
#endif
#if D3D12_MAX_DEVICE_INTERFACE >= 4
			if (SUCCEEDED(RootDevice->QueryInterface(IID_PPV_ARGS(RootDevice4.GetInitReference()))))
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("ID3D12Device4 is supported."));
			}
#endif
#if D3D12_MAX_DEVICE_INTERFACE >= 5
			if (SUCCEEDED(RootDevice->QueryInterface(IID_PPV_ARGS(RootDevice5.GetInitReference()))))
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("ID3D12Device5 is supported."));
			}
#endif
#if D3D12_MAX_DEVICE_INTERFACE >= 6
			if (SUCCEEDED(RootDevice->QueryInterface(IID_PPV_ARGS(RootDevice6.GetInitReference()))))
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("ID3D12Device6 is supported."));
			}
#endif
#if D3D12_MAX_DEVICE_INTERFACE >= 7
			if (SUCCEEDED(RootDevice->QueryInterface(IID_PPV_ARGS(RootDevice7.GetInitReference()))))
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("ID3D12Device7 is supported."));
			}
#endif
#if D3D12_MAX_DEVICE_INTERFACE >= 8
			if (SUCCEEDED(RootDevice->QueryInterface(IID_PPV_ARGS(RootDevice8.GetInitReference()))))
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("ID3D12Device8 is supported."));

				// D3D12_HEAP_FLAG_CREATE_NOT_ZEROED is supported
				bHeapNotZeroedSupported = true;
			}
#endif
#if D3D12_MAX_DEVICE_INTERFACE >= 9
			if (SUCCEEDED(RootDevice->QueryInterface(IID_PPV_ARGS(RootDevice9.GetInitReference()))))
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("ID3D12Device9 is supported."));
			}
#endif
#if D3D12_MAX_DEVICE_INTERFACE >= 10
			if (SUCCEEDED(RootDevice->QueryInterface(IID_PPV_ARGS(RootDevice10.GetInitReference()))))
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("ID3D12Device10 is supported."));
			}
#endif

			const bool bRenderDocPresent = D3D12RHI_IsRenderDocPresent(RootDevice);

			if (GetResourceBindingTier() == D3D12_RESOURCE_BINDING_TIER_1)
			{
				MaxNonSamplerDescriptors = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1;
			}
			else if (GetResourceBindingTier() == D3D12_RESOURCE_BINDING_TIER_2)
			{
				MaxNonSamplerDescriptors = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2;
			}
			else if (GetResourceBindingTier() == D3D12_RESOURCE_BINDING_TIER_3)
			{
				// From: https://microsoft.github.io/DirectX-Specs/d3d/ResourceBinding.html#levels-of-hardware-support
				//   For Tier 3, the max # descriptors is listed as 1000000+. The + indicates that the runtime allows applications
				//   to try creating descriptor heaps with more than 1000000 descriptors, leaving the driver to decide whether 
				//   it can support the request or fail the call. There is no cap exposed indicating how large of a descriptor 
				//   heap the hardware could support – applications can just try what they want and fall back to 1000000 if 
				//   larger doesn’t work.
				// RenderDoc seems to give up on subsequent API calls if one of them return E_OUTOFMEMORY, so we don't use this
				// detection method if the RD plug-in is loaded.
				if (!bRenderDocPresent)
				{
#if D3D12_SUPPORTS_INFO_QUEUE
					// Temporarily silence CREATE_DESCRIPTOR_HEAP_LARGE_NUM_DESCRIPTORS since we know we might break on it
					TRefCountPtr<ID3D12InfoQueue> InfoQueue;
					RootDevice->QueryInterface(IID_PPV_ARGS(InfoQueue.GetInitReference()));
					if (InfoQueue)
					{
						D3D12_MESSAGE_ID MessageId = D3D12_MESSAGE_ID_CREATE_DESCRIPTOR_HEAP_LARGE_NUM_DESCRIPTORS;

						D3D12_INFO_QUEUE_FILTER NewFilter{};
						NewFilter.DenyList.NumIDs = 1;
						NewFilter.DenyList.pIDList = &MessageId;

						InfoQueue->PushStorageFilter(&NewFilter);
					}
#endif

					// create an overly large heap and test for failure
					D3D12_DESCRIPTOR_HEAP_DESC TempHeapDesc{};
					TempHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
					TempHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
					TempHeapDesc.NodeMask = FRHIGPUMask::All().GetNative();
					TempHeapDesc.NumDescriptors = 2 * D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2;

					TRefCountPtr<ID3D12DescriptorHeap> TempHeap;
					HRESULT hr = RootDevice->CreateDescriptorHeap(&TempHeapDesc, IID_PPV_ARGS(TempHeap.GetInitReference()));
					if (SUCCEEDED(hr))
					{
						MaxNonSamplerDescriptors = -1;
					}
					else
					{
						MaxNonSamplerDescriptors = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2;
					}

#if D3D12_SUPPORTS_INFO_QUEUE
					// Restore the info queue to its old state to ensure we get CREATE_DESCRIPTOR_HEAP_LARGE_NUM_DESCRIPTORS.
					if (InfoQueue)
					{
						InfoQueue->PopStorageFilter();
					}
#endif
				}
				else
				{
					MaxNonSamplerDescriptors = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2;
				}
			}
			else
			{
				checkNoEntry();

				MaxNonSamplerDescriptors = D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2;
			}

			MaxSamplerDescriptors = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;

			// From: https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_DynamicResources.html
			//     ResourceDescriptorHeap/SamplerDescriptorHeap must be supported on devices that support both D3D12_RESOURCE_BINDING_TIER_3 and D3D_SHADER_MODEL_6_6
			if (GetHighestShaderModel() >= D3D_SHADER_MODEL_6_6 && GetResourceBindingTier() >= D3D12_RESOURCE_BINDING_TIER_3)
			{
				GRHISupportsBindless = true;
				UE_LOG(LogD3D12RHI, Log, TEXT("Bindless resources are supported"));
			}

			// Detect availability of shader model 6.0 wave operations
			{
				D3D12_FEATURE_DATA_D3D12_OPTIONS1 Features{};
				RootDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &Features, sizeof(Features));
				GRHISupportsWaveOperations = Features.WaveOps;
				GRHIMinimumWaveSize = Features.WaveLaneCountMin;
				GRHIMaximumWaveSize = Features.WaveLaneCountMax;
			}

#if D3D12_RHI_RAYTRACING
			D3D12_FEATURE_DATA_D3D12_OPTIONS5 D3D12Caps5 = {};
			if (SUCCEEDED(RootDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &D3D12Caps5, sizeof(D3D12Caps5))))
			{
				static IConsoleVariable* RequireSM6CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.RequireSM6"));
				const bool bRequireSM6 = RequireSM6CVar && RequireSM6CVar->GetBool();

				const bool bRayTracingAllowedOnCurrentShaderPlatform = (bRequireSM6 == false) || (GMaxRHIShaderPlatform == SP_PCD3D_SM6);

				if (D3D12Caps5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0
					&& GetResourceBindingTier() >= D3D12_RESOURCE_BINDING_TIER_2
					&& RootDevice5
					&& FDataDrivenShaderPlatformInfo::GetSupportsRayTracing(GMaxRHIShaderPlatform)
					&& !FParse::Param(FCommandLine::Get(), TEXT("noraytracing")))
				{
					if (D3D12Caps5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_1
						&& GRHISupportsBindless
						&& RootDevice7)
					{
						if (bRayTracingAllowedOnCurrentShaderPlatform)
						{
							UE_LOG(LogD3D12RHI, Log, TEXT("D3D12 ray tracing tier 1.1 and bindless resources are supported."));

							GRHISupportsRayTracing = RHISupportsRayTracing(GMaxRHIShaderPlatform);
							GRHISupportsRayTracingShaders = GRHISupportsRayTracing && RHISupportsRayTracingShaders(GMaxRHIShaderPlatform);

							GRHISupportsRayTracingPSOAdditions = true;
							GRHISupportsInlineRayTracing = GRHISupportsRayTracing && RHISupportsInlineRayTracing(GMaxRHIShaderPlatform) && (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM6);
						}
						else
						{
 							UE_LOG(LogD3D12RHI, Log, TEXT("Ray tracing is disabled because SM6 shader platform is required (r.RayTracing.RequireSM6=1)."));
						}
					}
					else if (!GRHISupportsBindless)
					{
						UE_LOG(LogD3D12RHI, Log, TEXT("Ray tracing is disabled because bindless resources are not supported (Shader Model 6.6 and Resource Binding Tier 3 are required)."));
					}
					else
					{
						UE_LOG(LogD3D12RHI, Log, TEXT("Ray tracing is disabled because D3D12 ray tracing tier 1.1 is required but only tier 1.0 is supported."));
					}
				}
				else if (D3D12Caps5.RaytracingTier == D3D12_RAYTRACING_TIER_NOT_SUPPORTED 
					&& bRenderDocPresent
					&& !FParse::Param(FCommandLine::Get(), TEXT("noraytracing")))
				{
					UE_LOG(LogD3D12RHI, Warning, TEXT("Ray Tracing is disabled because the RenderDoc plugin is currently not compatible with D3D12 ray tracing."));
				}
			}

			GRHIRayTracingAccelerationStructureAlignment = uint32(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
			GRHIRayTracingScratchBufferAlignment = uint32(D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT);
			GRHIRayTracingShaderTableAlignment = uint32(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
			GRHIRayTracingInstanceDescriptorSize = uint32(sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

#endif // D3D12_RHI_RAYTRACING

#if PLATFORM_WINDOWS
			{
				D3D12_FEATURE_DATA_D3D12_OPTIONS7 D3D12Caps7 = {};
				RootDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &D3D12Caps7, sizeof(D3D12Caps7));

				D3D12_FEATURE_DATA_D3D12_OPTIONS9 D3D12Caps9 = {};
				RootDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS9, &D3D12Caps9, sizeof(D3D12Caps9));

				D3D12_FEATURE_DATA_D3D12_OPTIONS11 D3D12Caps11 = {};
				RootDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS11, &D3D12Caps11, sizeof(D3D12Caps11));

				if (GMaxRHIFeatureLevel >= ERHIFeatureLevel::SM6)
				{
					GRHISupportsMeshShadersTier0 = GRHISupportsMeshShadersTier1 = (D3D12Caps7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1);
				}

				if (D3D12Caps7.MeshShaderTier >= D3D12_MESH_SHADER_TIER_1)
				{
					UE_LOG(LogD3D12RHI, Log, TEXT("Mesh shader tier 1.0 is supported"));
				}

				if (D3D12Caps9.AtomicInt64OnTypedResourceSupported)
				{
					UE_LOG(LogD3D12RHI, Log, TEXT("AtomicInt64OnTypedResource is supported"));
					GRHISupportsAtomicUInt64 = true;
				}

				if (D3D12Caps9.AtomicInt64OnGroupSharedSupported)
				{
					UE_LOG(LogD3D12RHI, Log, TEXT("AtomicInt64OnGroupShared is supported"));
				}

				if (D3D12Caps11.AtomicInt64OnDescriptorHeapResourceSupported)
				{
					UE_LOG(LogD3D12RHI, Log, TEXT("AtomicInt64OnDescriptorHeapResource is supported"));
				}

				if (D3D12Caps9.AtomicInt64OnTypedResourceSupported && D3D12Caps11.AtomicInt64OnDescriptorHeapResourceSupported)
				{
					GRHISupportsDX12AtomicUInt64 = true;
				}

				if (GRHISupportsDX12AtomicUInt64)
				{
					UE_LOG(LogD3D12RHI, Log, TEXT("Shader Model 6.6 atomic64 is supported"));
				}
				else
				{
					UE_LOG(LogD3D12RHI, Log, TEXT("Shader Model 6.6 atomic64 is not supported"));
				}
			}
#endif // PLATFORM_WINDOWS
		}

#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
		D3D12_FEATURE_DATA_D3D12_OPTIONS2 D3D12Caps2 = {};
		if (FAILED(RootDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &D3D12Caps2, sizeof(D3D12Caps2))))
		{
			D3D12Caps2.DepthBoundsTestSupported = false;
			D3D12Caps2.ProgrammableSamplePositionsTier = D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_NOT_SUPPORTED;
		}
		bDepthBoundsTestSupported = !!D3D12Caps2.DepthBoundsTestSupported;
#endif

		D3D12_FEATURE_DATA_ROOT_SIGNATURE D3D12RootSignatureCaps = {};
		D3D12RootSignatureCaps.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;	// This is the highest version we currently support. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
		if (FAILED(RootDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &D3D12RootSignatureCaps, sizeof(D3D12RootSignatureCaps))))
		{
			D3D12RootSignatureCaps.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}
		RootSignatureVersion = D3D12RootSignatureCaps.HighestVersion;

		{
			D3D12_FEATURE_DATA_D3D12_OPTIONS3 D3D12Caps3 = {};
			if (FAILED(RootDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS3, &D3D12Caps3, sizeof(D3D12Caps3))))
			{
				D3D12Caps3.CopyQueueTimestampQueriesSupported = false;
			}

			// @todo - fix copy-queue timestamps. 
			// ResolveQueryData() can only be called on graphics/compute command lists, since it internally dispatches a compute shader to do the resolve work, but the submission thread is calling this on a copy command list.
			// This leads to D3DDebug errors claiming SetComputeRootSignature was called on a copy command list.
			bCopyQueueTimestampQueriesSupported = false;//!!D3D12Caps3.CopyQueueTimestampQueriesSupported;
		}

#if TRACK_RESOURCE_ALLOCATIONS
		// Set flag if we want to track all allocations - comes with some overhead and only possible when Tier 2 is available
		// (because we will create placed buffers for texture allocation to retrieve the GPU virtual addresses)
		bTrackAllAllocation = (GD3D12TrackAllAlocations || GPUCrashDebuggingModes == ED3D12GPUCrashDebuggingModes::All) && (GetResourceHeapTier() == D3D12_RESOURCE_HEAP_TIER_2);
#endif 

		CreateCommandSignatures();

		// Context redirectors allow RHI commands to be executed on multiple GPUs at the
		// same time in a multi-GPU system. Redirectors have a physical mask for the GPUs
		// they can support and an active mask which restricts commands to operate on a
		// subset of the physical GPUs. The default context redirectors used by the
		// immediate command list can support all physical GPUs, whereas context containers
		// used by the parallel command lists might only support a subset of GPUs in the
		// system.
		DefaultContextRedirector.SetPhysicalGPUMask(FRHIGPUMask::All());

		// Create all of the FD3D12Devices.
		for (uint32 GPUIndex : FRHIGPUMask::All())
		{
			Devices[GPUIndex] = new FD3D12Device(FRHIGPUMask::FromIndex(GPUIndex), this);
			Devices[GPUIndex]->SetupAfterDeviceCreation();

			// The redirectors allow to broadcast to any GPU set
			DefaultContextRedirector.SetPhysicalContext(&Devices[GPUIndex]->GetDefaultCommandContext());
		}

		FrameFence = MakeUnique<FD3D12ManualFence>(this);

		const FString Name(L"Upload Buffer Allocator");
		for (uint32 GPUIndex : FRHIGPUMask::All())
		{
			// Safe to init as we have a device;
			UploadHeapAllocator[GPUIndex] = new FD3D12UploadHeapAllocator(this,	Devices[GPUIndex], Name);
			UploadHeapAllocator[GPUIndex]->Init();
		}


		// ID3D12Device1::CreatePipelineLibrary() requires each blob to be specific to the given adapter. To do this we create a unique file name with from the adpater desc. 
		// Note that : "The uniqueness of an LUID is guaranteed only until the system is restarted" according to windows doc and thus can not be reused.
		const FString UniqueDeviceCachePath = FString::Printf(TEXT("V%d_D%d_S%d_R%d.ushaderprecache"), Desc.Desc.VendorId, Desc.Desc.DeviceId, Desc.Desc.SubSysId, Desc.Desc.Revision);
		FString GraphicsCacheFile = PIPELINE_STATE_FILE_LOCATION / FString::Printf(TEXT("D3DGraphics_%s"), *UniqueDeviceCachePath);
	    FString ComputeCacheFile = PIPELINE_STATE_FILE_LOCATION / FString::Printf(TEXT("D3DCompute_%s"), *UniqueDeviceCachePath);
		FString DriverBlobFilename = PIPELINE_STATE_FILE_LOCATION / FString::Printf(TEXT("D3DDriverByteCodeBlob_%s"), *UniqueDeviceCachePath);

		PipelineStateCache.Init(GraphicsCacheFile, ComputeCacheFile, DriverBlobFilename);
		PipelineStateCache.RebuildFromDiskCache();

		ERHIBindlessConfiguration BindlessResourcesConfig = ERHIBindlessConfiguration::Disabled;
		ERHIBindlessConfiguration BindlessSamplersConfig = ERHIBindlessConfiguration::Disabled;

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
		if (Desc.MaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_12_0 && Desc.MaxSupportedShaderModel >= D3D_SHADER_MODEL_6_6 && Desc.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3)
		{
			BindlessResourcesConfig = RHIGetBindlessResourcesConfiguration(GMaxRHIShaderPlatform);
			BindlessSamplersConfig = RHIGetBindlessSamplersConfiguration(GMaxRHIShaderPlatform);

			bBindlessResourcesAllowed = (BindlessResourcesConfig != ERHIBindlessConfiguration::Disabled);
			bBindlessSamplersAllowed = (BindlessSamplersConfig != ERHIBindlessConfiguration::Disabled);
		}
#endif

#if USE_STATIC_ROOT_SIGNATURE
		ED3D12RootSignatureFlags GraphicsFlags{};
#if PLATFORM_SUPPORTS_MESH_SHADERS
		EnumAddFlags(GraphicsFlags, ED3D12RootSignatureFlags::AllowMeshShaders);
#endif
		if (BindlessResourcesConfig == ERHIBindlessConfiguration::AllShaders)
		{
			EnumAddFlags(GraphicsFlags, ED3D12RootSignatureFlags::BindlessResources);
		}
		if (BindlessSamplersConfig == ERHIBindlessConfiguration::AllShaders)
		{
			EnumAddFlags(GraphicsFlags, ED3D12RootSignatureFlags::BindlessSamplers);
		}

		StaticGraphicsRootSignature.InitStaticGraphicsRootSignature(GraphicsFlags);
		StaticComputeRootSignature.InitStaticComputeRootSignatureDesc(GraphicsFlags);

#if D3D12_RHI_RAYTRACING
		ED3D12RootSignatureFlags RayTracingFlags{};
		if (BindlessResourcesConfig != ERHIBindlessConfiguration::Disabled)
		{
			EnumAddFlags(RayTracingFlags, ED3D12RootSignatureFlags::BindlessResources);
		}
		if (BindlessSamplersConfig != ERHIBindlessConfiguration::Disabled)
		{
			EnumAddFlags(RayTracingFlags, ED3D12RootSignatureFlags::BindlessSamplers);
		}

		StaticRayTracingGlobalRootSignature.InitStaticRayTracingGlobalRootSignatureDesc(RayTracingFlags);
		StaticRayTracingLocalRootSignature.InitStaticRayTracingLocalRootSignatureDesc();
#endif
#endif // USE_STATIC_ROOT_SIGNATURE
	}
}

void FD3D12Adapter::InitializeRayTracing()
{
#if D3D12_RHI_RAYTRACING
	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		if (Devices[GPUIndex]->GetDevice5())
		{
			Devices[GPUIndex]->InitRayTracing();
		}
	}
#endif // D3D12_RHI_RAYTRACING
}

void FD3D12Adapter::CreateCommandSignatures()
{
	ID3D12Device* Device = GetD3DDevice();

	// ExecuteIndirect command signatures
	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.NumArgumentDescs = 1;
	commandSignatureDesc.ByteStride = 20;
	commandSignatureDesc.NodeMask = FRHIGPUMask::All().GetNative();

	D3D12_INDIRECT_ARGUMENT_DESC indirectParameterDesc[1] = {};
	commandSignatureDesc.pArgumentDescs = indirectParameterDesc;

	indirectParameterDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
	commandSignatureDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
	VERIFYD3D12RESULT(Device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(DrawIndirectCommandSignature.GetInitReference())));

	indirectParameterDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	commandSignatureDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
	VERIFYD3D12RESULT(Device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(DrawIndexedIndirectCommandSignature.GetInitReference())));

	indirectParameterDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
	commandSignatureDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
	VERIFYD3D12RESULT(Device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(DispatchIndirectGraphicsCommandSignature.GetInitReference())));

	checkf(DispatchIndirectComputeCommandSignature.IsValid(), TEXT("Indirect compute dispatch command signature is expected to be created by platform-specific D3D12 adapter implementation."))
}

void FD3D12Adapter::SetupGPUCrashDebuggingModesCommon()
{
	// Multiple ways to enable the different D3D12 crash debugging modes:
	// - via RHI independent r.GPUCrashDebugging cvar: by default enable low overhead breadcrumbs and NvAftermath are enabled
	// - via 'gpucrashdebugging' command line argument: enable all possible GPU crash debug modes (minor performance impact)
	// - via 'r.D3D12.BreadCrumbs', 'r.D3D12.AfterMath' or 'r.D3D12.Dred' each type of GPU crash debugging mode can be enabled
	// - via '-gpubreadcrumbs(=0)', '-nvaftermath(=0)' or '-dred(=0)' command line argument: each type of gpu crash debugging mode can enabled/disabled
	if (FParse::Param(FCommandLine::Get(), TEXT("gpucrashdebugging")))
	{
		GPUCrashDebuggingModes = ED3D12GPUCrashDebuggingModes::All;
	}
	else
	{
		// Parse the specific GPU crash debugging cvars and enable the different modes
		const auto ParseCVar = [this](const TCHAR* CVarName, ED3D12GPUCrashDebuggingModes DebuggingMode)
		{
			IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(CVarName);
			if (ConsoleVariable && ConsoleVariable->GetInt() > 0)
			{
				EnumAddFlags(GPUCrashDebuggingModes, DebuggingMode);
			}
		};
		ParseCVar(TEXT("r.GPUCrashDebugging"), ED3D12GPUCrashDebuggingModes((int)ED3D12GPUCrashDebuggingModes::BreadCrumbs | (int)ED3D12GPUCrashDebuggingModes::NvAftermath));
		ParseCVar(TEXT("r.D3D12.BreadCrumbs"), ED3D12GPUCrashDebuggingModes::BreadCrumbs);
		ParseCVar(TEXT("r.D3D12.NvAfterMath"), ED3D12GPUCrashDebuggingModes::NvAftermath);
		ParseCVar(TEXT("r.D3D12.DRED"), ED3D12GPUCrashDebuggingModes::DRED);

		// Enable/disable specific crash debugging modes if requested via command line argument
		const auto ParseCommandLine = [this](const TCHAR* CommandLineArgument, ED3D12GPUCrashDebuggingModes DebuggingMode)
		{
			int32 Value = 0;
			if (FParse::Value(FCommandLine::Get(), *FString::Printf(TEXT("%s="), CommandLineArgument), Value))
			{
				if (Value > 0)
				{
					EnumAddFlags(GPUCrashDebuggingModes, DebuggingMode);
				}
				else
				{
					EnumRemoveFlags(GPUCrashDebuggingModes, DebuggingMode);
				}
			}
			else  if (FParse::Param(FCommandLine::Get(), CommandLineArgument))
			{
				EnumAddFlags(GPUCrashDebuggingModes, DebuggingMode);
			}
		};
		ParseCommandLine(TEXT("gpubreadcrumbs"), ED3D12GPUCrashDebuggingModes::BreadCrumbs);
		ParseCommandLine(TEXT("nvaftermath"), ED3D12GPUCrashDebuggingModes::NvAftermath);
		ParseCommandLine(TEXT("dred"), ED3D12GPUCrashDebuggingModes::DRED);
	}

	// Submit draw events when any crash debugging mode is enabled
	if (GPUCrashDebuggingModes != ED3D12GPUCrashDebuggingModes::None)
	{
		SetEmitDrawEvents(true);
	}

	bool bBreadcrumbs = EnumHasAnyFlags(GPUCrashDebuggingModes, ED3D12GPUCrashDebuggingModes::BreadCrumbs);
	FGenericCrashContext::SetEngineData(TEXT("RHI.Breadcrumbs"), bBreadcrumbs ? TEXT("true") : TEXT("false"));

}

void FD3D12Adapter::CleanupResources()
{
	for (auto& Viewport : Viewports)
	{
		Viewport->IssueFrameEvent();
		Viewport->WaitForFrameEventCompletion();
	}

	TransientMemoryCache = nullptr;

#if D3D12_RHI_RAYTRACING
	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		Devices[GPUIndex]->CleanupRayTracing();
	}
#endif // D3D12_RHI_RAYTRACING
}

FD3D12Adapter::~FD3D12Adapter()
{
	// Release allocation data of all thread local transient uniform buffer allocators
	for (FTransientUniformBufferAllocator* Allocator : TransientUniformBufferAllocators)
	{
		Allocator->Cleanup();
	}
	TransientUniformBufferAllocators.Empty();

	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		delete(Devices[GPUIndex]);
		Devices[GPUIndex] = nullptr;
	}

	Viewports.Empty();
	DrawingViewport = nullptr;

	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		if (UploadHeapAllocator[GPUIndex])
		{
			UploadHeapAllocator[GPUIndex]->Destroy();
			delete(UploadHeapAllocator[GPUIndex]);
			UploadHeapAllocator[GPUIndex] = nullptr;
		}
	}

	FrameFence = {};

	TransientMemoryCache.Reset();

	if (RootDevice)
	{
		PipelineStateCache.Close();
	}
	RootSignatureManager.Destroy();

	DrawIndirectCommandSignature.SafeRelease();
	DrawIndexedIndirectCommandSignature.SafeRelease();
	DispatchIndirectGraphicsCommandSignature.SafeRelease();
	DispatchIndirectComputeCommandSignature.SafeRelease();
	DispatchRaysIndirectCommandSignature.SafeRelease();

#if D3D12_SUPPORTS_DXGI_DEBUG
	// trace all leak D3D resource
	if (DXGIDebug != nullptr)
	{
		DXGIDebug->ReportLiveObjects(
			GUID{ 0xe48ae283, 0xda80, 0x490b, { 0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x8 } }, // DXGI_DEBUG_ALL
			DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL));
		DXGIDebug.SafeRelease();

#if D3D12_SUPPORTS_INFO_QUEUE
		CheckD3DStoredMessages();
#endif
	}
#endif

#if PLATFORM_WINDOWS
	if (ExceptionHandlerHandle != INVALID_HANDLE_VALUE)
	{
		RemoveVectoredExceptionHandler(ExceptionHandlerHandle);
	}

	if (D3D12RHI_ShouldCreateWithD3DDebug())
	{
		TRefCountPtr<ID3D12DebugDevice> Debug;
		if (SUCCEEDED(GetD3DDevice()->QueryInterface(IID_PPV_ARGS(Debug.GetInitReference()))))
		{
			D3D12_RLDO_FLAGS rldoFlags = D3D12_RLDO_DETAIL;
			Debug->ReportLiveDeviceObjects(rldoFlags);
		}
	}
#endif

#if D3D12_SUPPORTS_DXGI_DEBUG
	FPlatformProcess::FreeDllHandle(DxgiDebugDllHandle);
#endif

#if PLATFORM_WINDOWS
	FPlatformProcess::FreeDllHandle(DxgiDllHandle);
#endif
}

void FD3D12Adapter::CreateDXGIFactory(bool bWithDebug)
{
#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
	typedef HRESULT(WINAPI FCreateDXGIFactory2)(UINT, REFIID, void**);

#if PLATFORM_WINDOWS
	// Dynamically load this otherwise Win7 fails to boot as it's missing on that DLL
	DxgiDllHandle = (HMODULE)FPlatformProcess::GetDllHandle(TEXT("dxgi.dll"));
	check(DxgiDllHandle);

	FCreateDXGIFactory2* CreateDXGIFactory2FnPtr = (FCreateDXGIFactory2*)(void*)::GetProcAddress(DxgiDllHandle, "CreateDXGIFactory2");
#else
	FCreateDXGIFactory2* CreateDXGIFactory2FnPtr = &CreateDXGIFactory2;
#endif

	check(CreateDXGIFactory2FnPtr);

	const uint32 Flags = bWithDebug ? DXGI_CREATE_FACTORY_DEBUG : 0;
	VERIFYD3D12RESULT(CreateDXGIFactory2FnPtr(Flags, IID_PPV_ARGS(DxgiFactory2.GetInitReference())));

	InitDXGIFactoryVariants(DxgiFactory2);
#endif // #if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
}

void FD3D12Adapter::InitDXGIFactoryVariants(IDXGIFactory2* InDxgiFactory2)
{
#if DXGI_MAX_FACTORY_INTERFACE >= 3
	InDxgiFactory2->QueryInterface(IID_PPV_ARGS(DxgiFactory3.GetInitReference()));
#endif
#if DXGI_MAX_FACTORY_INTERFACE >= 4
	InDxgiFactory2->QueryInterface(IID_PPV_ARGS(DxgiFactory4.GetInitReference()));
#endif
#if DXGI_MAX_FACTORY_INTERFACE >= 5
	InDxgiFactory2->QueryInterface(IID_PPV_ARGS(DxgiFactory5.GetInitReference()));
#endif
#if DXGI_MAX_FACTORY_INTERFACE >= 6
	InDxgiFactory2->QueryInterface(IID_PPV_ARGS(DxgiFactory6.GetInitReference()));
#endif
}

HRESULT FD3D12Adapter::EnumAdapters(IDXGIAdapter** TempAdapter) const
{
#if DXGI_MAX_FACTORY_INTERFACE >= 6
	return FD3D12AdapterDesc::EnumAdapters(Desc.AdapterIndex, Desc.GpuPreference, DxgiFactory2, DxgiFactory6, TempAdapter);
#else
	return DxgiFactory2->EnumAdapters(Desc.AdapterIndex, TempAdapter);
#endif
}

void FD3D12Adapter::EndFrame()
{
	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		uint64 FrameLag = 20;
		GetUploadHeapAllocator(GPUIndex).CleanUpAllocations(FrameLag);
	}

	if (TransientMemoryCache)
	{
		TransientMemoryCache->GarbageCollect();
	}

#if TRACK_RESOURCE_ALLOCATIONS
	FScopeLock Lock(&TrackedAllocationDataCS); 

	// remove tracked released resources older than n amount of frames
	int32 ReleaseCount = 0;
	uint64 CurrentFrameID = GetFrameFence().GetNextFenceToSignal();
	for (; ReleaseCount < ReleasedAllocationData.Num(); ++ReleaseCount)
	{
		if (ReleasedAllocationData[ReleaseCount].ReleasedFrameID + GTrackedReleasedAllocationFrameRetention > CurrentFrameID)
		{
			break;
		}
	}
	if (ReleaseCount > 0)
	{
		ReleasedAllocationData.RemoveAt(0, ReleaseCount, false);
	}
#endif
}

#if WITH_MGPU
FD3D12Adapter::FTemporalEffect& FD3D12Adapter::GetTemporalEffect(const FName& EffectName)
{
	FTemporalEffect* Effect = TemporalEffectMap.Find(EffectName);

	if (Effect == nullptr)
	{
		Effect = &TemporalEffectMap.Emplace(EffectName, FTemporalEffect());
		Effect->AddDefaulted(FRHIGPUMask::All().GetNumActive());
	}

	check(Effect);
	return *Effect;
}
#endif // WITH_MGPU

FD3D12FastConstantAllocator& FD3D12Adapter::GetTransientUniformBufferAllocator()
{
	// Multi-GPU support : is using device 0 always appropriate here?
	return FTransientUniformBufferAllocator::Get([this]() -> FTransientUniformBufferAllocator*
	{
		FTransientUniformBufferAllocator* Alloc = new FTransientUniformBufferAllocator(this, Devices[0], FRHIGPUMask::All());

		// Register so the underlying resource location can be freed during adapter cleanup instead of when thread local allocation is destroyed
		{
			FScopeLock Lock(&TransientUniformBufferAllocatorsCS);
			TransientUniformBufferAllocators.Add(Alloc);
		}

		return Alloc;
	});
}

void FD3D12Adapter::ReleaseTransientUniformBufferAllocator(FTransientUniformBufferAllocator* InAllocator)
{
	FScopeLock Lock(&TransientUniformBufferAllocatorsCS);
	verify(TransientUniformBufferAllocators.Remove(InAllocator) == 1);
}

void FD3D12Adapter::UpdateMemoryInfo()
{
#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
	const uint64 UpdateFrame = FrameFence != nullptr ? FrameFence->GetNextFenceToSignal() : 0;

	// Avoid spurious query calls if we have already captured this frame.
	if (MemoryInfo.UpdateFrameNumber == UpdateFrame)
	{
		return;
	}

	// Update the frame number that the memory is captured from.
	MemoryInfo.UpdateFrameNumber = UpdateFrame;

	TRefCountPtr<IDXGIAdapter3> Adapter3;
	VERIFYD3D12RESULT(GetAdapter()->QueryInterface(IID_PPV_ARGS(Adapter3.GetInitReference())));

	VERIFYD3D12RESULT(Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &MemoryInfo.LocalMemoryInfo));
	VERIFYD3D12RESULT(Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &MemoryInfo.NonLocalMemoryInfo));

	// Over budget?
	if (MemoryInfo.LocalMemoryInfo.CurrentUsage > MemoryInfo.LocalMemoryInfo.Budget)
	{
		MemoryInfo.AvailableLocalMemory = 0;
		MemoryInfo.DemotedLocalMemory = MemoryInfo.LocalMemoryInfo.CurrentUsage - MemoryInfo.LocalMemoryInfo.Budget;
	}
	else
	{
		MemoryInfo.AvailableLocalMemory = MemoryInfo.LocalMemoryInfo.Budget - MemoryInfo.LocalMemoryInfo.CurrentUsage;
		MemoryInfo.DemotedLocalMemory = 0;
	}

	// Update global RHI state (for warning output, etc.)
	GDemotedLocalMemorySize = MemoryInfo.DemotedLocalMemory;

	if (!GVirtualMGPU)
	{
		for (uint32 Index = 1; Index < GNumExplicitGPUsForRendering; ++Index)
		{
			DXGI_QUERY_VIDEO_MEMORY_INFO TempVideoMemoryInfo;
			VERIFYD3D12RESULT(Adapter3->QueryVideoMemoryInfo(Index, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &TempVideoMemoryInfo));

			DXGI_QUERY_VIDEO_MEMORY_INFO TempSystemMemoryInfo;
			VERIFYD3D12RESULT(Adapter3->QueryVideoMemoryInfo(Index, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &TempSystemMemoryInfo));
			
			MemoryInfo.LocalMemoryInfo.Budget = FMath::Min(MemoryInfo.LocalMemoryInfo.Budget, TempVideoMemoryInfo.Budget);
			MemoryInfo.LocalMemoryInfo.CurrentUsage = FMath::Min(MemoryInfo.LocalMemoryInfo.CurrentUsage, TempVideoMemoryInfo.CurrentUsage);

			MemoryInfo.NonLocalMemoryInfo.Budget = FMath::Min(MemoryInfo.NonLocalMemoryInfo.Budget, TempSystemMemoryInfo.Budget);
			MemoryInfo.NonLocalMemoryInfo.CurrentUsage = FMath::Min(MemoryInfo.NonLocalMemoryInfo.CurrentUsage, TempSystemMemoryInfo.CurrentUsage);
		}
	}
#endif
}

void FD3D12Adapter::BlockUntilIdle()
{
	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		GetDevice(GPUIndex)->BlockUntilIdle();
	}
}


void FD3D12Adapter::TrackAllocationData(FD3D12ResourceLocation* InAllocation, uint64 InAllocationSize, bool bCollectCallstack)
{
#if TRACK_RESOURCE_ALLOCATIONS
	FTrackedAllocationData AllocationData;
	AllocationData.ResourceAllocation = InAllocation;
	AllocationData.AllocationSize = InAllocationSize;
	if (bCollectCallstack)
	{
		AllocationData.StackDepth = FPlatformStackWalk::CaptureStackBackTrace(&AllocationData.Stack[0], FTrackedAllocationData::MaxStackDepth);
	}
	else 
	{
		AllocationData.StackDepth = 0;
	}

	FScopeLock Lock(&TrackedAllocationDataCS);
	check(!TrackedAllocationData.Contains(InAllocation));
	TrackedAllocationData.Add(InAllocation, AllocationData);
#endif
}

void FD3D12Adapter::ReleaseTrackedAllocationData(FD3D12ResourceLocation* InAllocation, bool bDefragFree)
{
#if TRACK_RESOURCE_ALLOCATIONS
	FScopeLock Lock(&TrackedAllocationDataCS);

	D3D12_GPU_VIRTUAL_ADDRESS GPUAddress = InAllocation->GetGPUVirtualAddress();
	if (GPUAddress != 0 || IsTrackingAllAllocations())
	{
		FReleasedAllocationData ReleasedData;
		ReleasedData.GPUVirtualAddress = GPUAddress;
		ReleasedData.AllocationSize = InAllocation->GetSize();
		ReleasedData.ResourceName = InAllocation->GetResource()->GetName();
		ReleasedData.ResourceDesc = InAllocation->GetResource()->GetDesc();
		ReleasedData.ReleasedFrameID = GetFrameFence().GetNextFenceToSignal();
		ReleasedData.bDefragFree = bDefragFree;
		ReleasedData.bBackBuffer = InAllocation->GetResource()->IsBackBuffer();
		ReleasedData.bTransient = InAllocation->IsTransient();
		// Only the backbuffer doesn't have a valid gpu virtual address
		check(ReleasedData.GPUVirtualAddress != 0 || ReleasedData.bBackBuffer);
		ReleasedAllocationData.Add(ReleasedData);
	}

	verify(TrackedAllocationData.Remove(InAllocation) == 1);
#endif
}


void FD3D12Adapter::TrackHeapAllocation(FD3D12Heap* InHeap)
{
#if TRACK_RESOURCE_ALLOCATIONS
	FScopeLock Lock(&TrackedAllocationDataCS);
	check(!TrackedHeaps.Contains(InHeap));
	TrackedHeaps.Add(InHeap);
#endif
}

void FD3D12Adapter::ReleaseTrackedHeap(FD3D12Heap* InHeap)
{
#if TRACK_RESOURCE_ALLOCATIONS
	FScopeLock Lock(&TrackedAllocationDataCS);

	D3D12_GPU_VIRTUAL_ADDRESS GPUVirtualAddress = InHeap->GetGPUVirtualAddress();
	if (GPUVirtualAddress != 0 || IsTrackingAllAllocations())
	{
		FReleasedAllocationData ReleasedData;
		ReleasedData.GPUVirtualAddress	= GPUVirtualAddress;
		ReleasedData.AllocationSize		= InHeap->GetHeapDesc().SizeInBytes;
		ReleasedData.ResourceName		= InHeap->GetName();
		ReleasedData.ReleasedFrameID	= GetFrameFence().GetNextFenceToSignal();
		ReleasedData.bHeap				= true;
		ReleasedAllocationData.Add(ReleasedData);
	}

	verify(TrackedHeaps.Remove(InHeap) == 1);
#endif
}

void FD3D12Adapter::FindResourcesNearGPUAddress(D3D12_GPU_VIRTUAL_ADDRESS InGPUVirtualAddress, uint64 InRange, TArray<FAllocatedResourceResult>& OutResources)
{
#if TRACK_RESOURCE_ALLOCATIONS
	FScopeLock Lock(&TrackedAllocationDataCS);

	TArray<FTrackedAllocationData> Allocations;
	TrackedAllocationData.GenerateValueArray(Allocations);
	FInt64Range TrackRange((int64)(InGPUVirtualAddress - InRange), (int64)(InGPUVirtualAddress + InRange));
	for (FTrackedAllocationData& AllocationData : Allocations)
	{
		D3D12_GPU_VIRTUAL_ADDRESS GPUAddress = AllocationData.ResourceAllocation->GetResource()->GetGPUVirtualAddress();
		FInt64Range AllocationRange((int64)GPUAddress, (int64)(GPUAddress + AllocationData.AllocationSize));
		if (TrackRange.Overlaps(AllocationRange))
		{
			bool bContainsAllocation = AllocationRange.Contains(InGPUVirtualAddress);
			int64 Distance = bContainsAllocation ? 0 : ((InGPUVirtualAddress < GPUAddress) ? GPUAddress - InGPUVirtualAddress : InGPUVirtualAddress - AllocationRange.GetUpperBoundValue());
			check(Distance >= 0);

			FAllocatedResourceResult Result;
			Result.Allocation = AllocationData.ResourceAllocation;
			Result.Distance = Distance;
			OutResources.Add(Result);
		}
	}

	// Sort the resources on distance from the requested address
	Algo::Sort(OutResources, [InGPUVirtualAddress](const FAllocatedResourceResult& InLHS, const FAllocatedResourceResult& InRHS)
		{
			return InLHS.Distance < InRHS.Distance;
		});
#endif
}

void FD3D12Adapter::FindHeapsContainingGPUAddress(D3D12_GPU_VIRTUAL_ADDRESS InGPUVirtualAddress, TArray<FD3D12Heap*>& OutHeaps)
{
#if TRACK_RESOURCE_ALLOCATIONS
	FScopeLock Lock(&TrackedAllocationDataCS);

	for (FD3D12Heap* AllocatedHeap : TrackedHeaps)
	{
		D3D12_GPU_VIRTUAL_ADDRESS GPUAddress = AllocatedHeap->GetGPUVirtualAddress();
		FInt64Range HeapRange((int64)GPUAddress, (int64)(GPUAddress + AllocatedHeap->GetHeapDesc().SizeInBytes));
		if (HeapRange.Contains(InGPUVirtualAddress))
		{			
			OutHeaps.Add(AllocatedHeap);
		}
	}
#endif
}

void FD3D12Adapter::FindReleasedAllocationData(D3D12_GPU_VIRTUAL_ADDRESS InGPUVirtualAddress, TArray<FReleasedAllocationData>& OutAllocationData)
{
#if TRACK_RESOURCE_ALLOCATIONS
	FScopeLock Lock(&TrackedAllocationDataCS);

	for (FReleasedAllocationData& AllocationData : ReleasedAllocationData)
	{
		if (InGPUVirtualAddress >= AllocationData.GPUVirtualAddress && InGPUVirtualAddress < (AllocationData.GPUVirtualAddress + AllocationData.AllocationSize))
		{
			// Add in reverse order, so last released resources at first in the array
			OutAllocationData.EmplaceAt(0, AllocationData);
		}
	}
#endif
}

#if TRACK_RESOURCE_ALLOCATIONS

static FAutoConsoleCommandWithOutputDevice GDumpTrackedD3D12AllocationsCmd(
	TEXT("D3D12.DumpTrackedAllocations"),
	TEXT("Dump all tracked d3d12 resource allocations."),
	FConsoleCommandWithOutputDeviceDelegate::CreateStatic([](FOutputDevice& OutputDevice)
	{
		FD3D12DynamicRHI::GetD3DRHI()->GetAdapter().DumpTrackedAllocationData(OutputDevice, false, false);
	})
);

static FAutoConsoleCommandWithOutputDevice GDumpTrackedD3D12AllocationCallstacksCmd(
	TEXT("D3D12.DumpTrackedAllocationCallstacks"),
	TEXT("Dump all tracked d3d12 resource allocation callstacks."),
	FConsoleCommandWithOutputDeviceDelegate::CreateStatic([](FOutputDevice& OutputDevice)
	{
		FD3D12DynamicRHI::GetD3DRHI()->GetAdapter().DumpTrackedAllocationData(OutputDevice, false, true);
	})
);

static FAutoConsoleCommandWithOutputDevice GDumpTrackedD3D12ResidentAllocationsCmd(
	TEXT("D3D12.DumpTrackedResidentAllocations"),
	TEXT("Dump all tracked resisdent d3d12 resource allocations."),
	FConsoleCommandWithOutputDeviceDelegate::CreateStatic([](FOutputDevice& OutputDevice)
		{
			FD3D12DynamicRHI::GetD3DRHI()->GetAdapter().DumpTrackedAllocationData(OutputDevice, true, false);
		})
);

static FAutoConsoleCommandWithOutputDevice GDumpTrackedD3D12ResidentAllocationCallstacksCmd(
	TEXT("D3D12.DumpTrackedResidentAllocationCallstacks"),
	TEXT("Dump all tracked resident d3d12 resource allocation callstacks."),
	FConsoleCommandWithOutputDeviceDelegate::CreateStatic([](FOutputDevice& OutputDevice)
		{
			FD3D12DynamicRHI::GetD3DRHI()->GetAdapter().DumpTrackedAllocationData(OutputDevice, true, true);
		})
);

void FD3D12Adapter::DumpTrackedAllocationData(FOutputDevice& OutputDevice, bool bResidentOnly, bool bWithCallstack)
{
	FScopeLock Lock(&TrackedAllocationDataCS);

	TArray<FTrackedAllocationData> Allocations;
	TrackedAllocationData.GenerateValueArray(Allocations);
	Allocations.Sort([](const FTrackedAllocationData& InLHS, const FTrackedAllocationData& InRHS)
		{
			return InLHS.AllocationSize > InRHS.AllocationSize;
		});

	TArray<FTrackedAllocationData> BufferAllocations;	
	TArray<FTrackedAllocationData> TextureAllocations;
	uint64 TotalAllocatedBufferSize = 0;
	uint64 TotalResidentBufferSize = 0;
	uint64 TotalAllocatedTextureSize = 0;
	uint64 TotalResidentTextureSize = 0;
	for (FTrackedAllocationData& AllocationData : Allocations)
	{
		D3D12_RESOURCE_DESC ResourceDesc = AllocationData.ResourceAllocation->GetResource()->GetDesc();
		if (ResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			BufferAllocations.Add(AllocationData);
			TotalAllocatedBufferSize += AllocationData.AllocationSize;			
#if ENABLE_RESIDENCY_MANAGEMENT
			TotalResidentBufferSize += (AllocationData.ResourceAllocation->GetResidencyHandle().ResidencyStatus == D3DX12Residency::ManagedObject::RESIDENCY_STATUS::RESIDENT) ? AllocationData.AllocationSize : 0;
#else
			TotalResidentBufferSize += AllocationData.AllocationSize;
#endif 
		}
		else
		{
			TextureAllocations.Add(AllocationData);
			TotalAllocatedTextureSize += AllocationData.AllocationSize;
#if ENABLE_RESIDENCY_MANAGEMENT
			TotalResidentTextureSize += (AllocationData.ResourceAllocation->GetResidencyHandle().ResidencyStatus == D3DX12Residency::ManagedObject::RESIDENCY_STATUS::RESIDENT) ? AllocationData.AllocationSize : 0;
#else
			TotalResidentTextureSize += AllocationData.AllocationSize;
#endif 
		}
	}

	const size_t STRING_SIZE = 16 * 1024;
	ANSICHAR StackTrace[STRING_SIZE];

	FString OutputData;
	OutputData += FString::Printf(TEXT("\n%d Tracked Texture Allocations (Total size: %4.3fMB - Resident: %4.3fMB):\n"), TextureAllocations.Num(), TotalAllocatedTextureSize / (1024.0f * 1024), TotalResidentTextureSize / (1024.0f * 1024));
	for (const FTrackedAllocationData& AllocationData : TextureAllocations)
	{
		D3D12_RESOURCE_DESC ResourceDesc = AllocationData.ResourceAllocation->GetResource()->GetDesc();

		bool bResident = true;
#if ENABLE_RESIDENCY_MANAGEMENT
		bResident = AllocationData.ResourceAllocation->GetResidencyHandle().ResidencyStatus == D3DX12Residency::ManagedObject::RESIDENCY_STATUS::RESIDENT;
#endif 
		if (!bResident && bResidentOnly)
		{
			continue;
		}

		FString Flags;
		if (EnumHasAnyFlags(ResourceDesc.Flags, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET))
		{
			Flags += "RT";
		}
		else if (EnumHasAnyFlags(ResourceDesc.Flags, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
		{
			Flags += "DS";
		}
		if (EnumHasAnyFlags(ResourceDesc.Flags, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
		{
			Flags += EnumHasAnyFlags(ResourceDesc.Flags, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) ? "|UAV" : "UAV";
		}

		OutputData += FString::Printf(TEXT("\tName: %s - Size: %3.3fMB - Width: %d - Height: %d - DepthOrArraySize: %d - MipLevels: %d - Flags: %s - Resident: %s\n"), 
			*AllocationData.ResourceAllocation->GetResource()->GetName().ToString(), 
			AllocationData.AllocationSize / (1024.0f * 1024),
			ResourceDesc.Width, ResourceDesc.Height, ResourceDesc.DepthOrArraySize, ResourceDesc.MipLevels,
			Flags.IsEmpty() ? TEXT("None") : *Flags,
			bResident ? TEXT("Yes") : TEXT("No"));

		if (bWithCallstack)
		{
			static uint32 EntriesToSkip = 3;
			for (uint32 Index = EntriesToSkip; Index < AllocationData.StackDepth; ++Index)
			{
				StackTrace[0] = 0;
				FPlatformStackWalk::ProgramCounterToHumanReadableString(Index, AllocationData.Stack[Index], StackTrace, STRING_SIZE, nullptr);
				OutputData += FString::Printf(TEXT("\t\t%d %s\n"), Index - EntriesToSkip, ANSI_TO_TCHAR(StackTrace));
			}
		}
	}

	OutputData += FString::Printf(TEXT("\n\n%d Tracked Buffer Allocations (Total size: %4.3fMB - Resident: %4.3fMB):\n"), BufferAllocations.Num(), TotalAllocatedBufferSize / (1024.0f * 1024), TotalResidentBufferSize / (1024.0f * 1024));
	for (const FTrackedAllocationData& AllocationData : BufferAllocations)
	{
		D3D12_RESOURCE_DESC ResourceDesc = AllocationData.ResourceAllocation->GetResource()->GetDesc();

		bool bResident = true;
#if ENABLE_RESIDENCY_MANAGEMENT
		bResident = AllocationData.ResourceAllocation->GetResidencyHandle().ResidencyStatus == D3DX12Residency::ManagedObject::RESIDENCY_STATUS::RESIDENT;
#endif 
		if (!bResident && bResidentOnly)
		{
			continue;
		}

		OutputData += FString::Printf(TEXT("\tName: %s - Size: %3.3fMB - Width: %d - UAV: %s - Resident: %s\n"), 
			*AllocationData.ResourceAllocation->GetResource()->GetName().ToString(), 
			AllocationData.AllocationSize / (1024.0f * 1024),
			ResourceDesc.Width,
			EnumHasAnyFlags(ResourceDesc.Flags, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) ? TEXT("Yes") : TEXT("No"),
			bResident ? TEXT("Yes") : TEXT("No"));

		if (bWithCallstack)
		{
			static uint32 EntriesToSkip = 3;
			for (uint32 Index = EntriesToSkip; Index < AllocationData.StackDepth; ++Index)
			{
				StackTrace[0] = 0;
				FPlatformStackWalk::ProgramCounterToHumanReadableString(Index, AllocationData.Stack[Index], StackTrace, STRING_SIZE, nullptr);
				OutputData += FString::Printf(TEXT("\t\t%d %s\n"), Index - EntriesToSkip, ANSI_TO_TCHAR(StackTrace));
			}
		}
	}

	OutputDevice.Log(OutputData);
}

#endif // TRACK_RESOURCE_ALLOCATIONS

void FD3D12Adapter::SetResidencyPriority(ID3D12Pageable* Pageable, D3D12_RESIDENCY_PRIORITY HeapPriority, uint32 GPUIndex)
{
#if D3D12_RHI_RAYTRACING // ID3D12Device5 is currently tied to DXR support
	// GUID for our custom private data that holds the current priority
	static const GUID DataGuid = GUID{ 0xB365961D, 0xA209, 0x4475, { 0x84, 0x9B, 0xDA, 0xFF, 0x90, 0x91, 0x2E, 0xB1 } };

	const uint32 DataSize = uint32(sizeof(HeapPriority));
	uint32 ExistingDataSize = DataSize;
	D3D12_RESIDENCY_PRIORITY ExistingPriority = (D3D12_RESIDENCY_PRIORITY)0;

	HRESULT Result = Pageable->GetPrivateData(DataGuid, &ExistingDataSize, &ExistingPriority);

	if (!SUCCEEDED(Result) || ExistingDataSize != DataSize || ExistingPriority != HeapPriority)
	{
		FD3D12Device* NodeDevice = GetDevice(GPUIndex);
		NodeDevice->GetDevice5()->SetResidencyPriority(1, &Pageable, &HeapPriority);
		Pageable->SetPrivateData(DataGuid, DataSize, &HeapPriority);
	}
#endif // D3D12_RHI_RAYTRACING
}

