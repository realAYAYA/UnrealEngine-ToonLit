// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxManager.h"

#include "CudaModule.h"
#include "HAL/IConsoleManager.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "IRivermaxCoreModule.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/CoreDelegates.h"
#include "RivermaxDeviceFinder.h"
#include "RivermaxLog.h"
#include "RivermaxTracingUtils.h"

#include <chrono>

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"

#include <ws2tcpip.h>

#include "Windows/HideWindowsPlatformTypes.h"
#endif

#if WITH_EDITOR
#include "Editor.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#endif

#define LOCTEXT_NAMESPACE "RivermaxManager"

namespace UE::RivermaxCore
{
	TMap<uint8, FString> FRivermaxTracingUtils::RmaxOutMediaCapturePipeTraceEvents;
	TMap<uint8, FString> FRivermaxTracingUtils::RmaxOutSendingFrameTraceEvents;
	TMap<uint8, FString> FRivermaxTracingUtils::RmaxOutFrameReadyTraceEvents;
	TMap<uint8, FString> FRivermaxTracingUtils::RmaxInStartingFrameTraceEvents;
	TMap<uint8, FString> FRivermaxTracingUtils::RmaxInReceivedFrameTraceEvents;
	TMap<uint8, FString> FRivermaxTracingUtils::RmaxInSelectedFrameTraceEvents;

	// Environment variable used to specify where Rivermax library is located. This enables having an installed version different than the one used by Unreal
	static const FString RivermaxLibraryEnvironmentVariable = TEXT("RIVERMAX_PATH");
}

namespace UE::RivermaxCore::Private
{
	static TAutoConsoleVariable<int32> CVarRivermaxEnableGPUDirectCapability(
		TEXT("Rivermax.GPUDirect"), 1,
		TEXT("Whether GPUDirect capability is validated at launch. Can be used to disable it entirely."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxEnableGPUDirectInputCapability(
		TEXT("Rivermax.GPUDirectInput"), 0,
		TEXT("Whether GPUDirect capability is enabled for input purposes."),
		ECVF_Default);

	static TAutoConsoleVariable<int32> CVarRivermaxEnableGPUDirectOutputCapability(
		TEXT("Rivermax.GPUDirectOutput"), 1,
		TEXT("Whether GPUDirect capability is enabled for output purposes."),
		ECVF_Default);


	static uint64 RivermaxTimeHandler(void* Context)
	{
		IRivermaxCoreModule* RivermaxModule = FModuleManager::GetModulePtr<IRivermaxCoreModule>("RivermaxCore");
		if (RivermaxModule)
		{
			return RivermaxModule->GetRivermaxManager()->GetTime();
		}

		return 0;
	}

	FRivermaxManager::FRivermaxManager()
		: DeviceFinder(MakeUnique<FRivermaxDeviceFinder>())
	{
		// No need loading Cuda if we won't be looking at gpudirect capabilities
		if (CVarRivermaxEnableGPUDirectCapability.GetValueOnGameThread())
		{
			FModuleManager::LoadModuleChecked<FCUDAModule>("CUDA");
		}

		//Postpone initialization after all modules have been loaded
		FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddRaw(this, &FRivermaxManager::InitializeLibrary);
	}

	FRivermaxManager::~FRivermaxManager()
	{
		if (bIsCleanupRequired)
		{
			rmx_status Status = GetApi()->rmx_cleanup();
			if (Status != RMX_OK)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Failed to cleanup Rivermax Library. Status: %d"), Status);
			}
			UE_LOG(LogRivermax, Log, TEXT("Rivermax Library has shutdown."));
		}

		if (LibraryHandle != nullptr)
		{
			FPlatformProcess::FreeDllHandle(LibraryHandle);
			LibraryHandle = nullptr;
		}
	}

	bool FRivermaxManager::IsManagerInitialized() const
	{
		return bIsInitialized;
	}

	bool FRivermaxManager::LoadRivermaxLibrary()
	{
#if defined(RIVERMAX_LIBRARY_PLATFORM_PATH) && defined(RIVERMAX_LIBRARY_NAME)

		FString LibraryPath = TEXT(PREPROCESSOR_TO_STRING(RIVERMAX_LIBRARY_PLATFORM_PATH));

		// Look for environment variable defining where the library version we are looking for is located

		FString EnvVarRivermaxPath = FPlatformMisc::GetEnvironmentVariable(*RivermaxLibraryEnvironmentVariable);
		if (!EnvVarRivermaxPath.IsEmpty())
		{
			if (FPaths::DirectoryExists(EnvVarRivermaxPath))
			{
				LibraryPath = EnvVarRivermaxPath;
			}
		}
		else
		{
			FString MellanoxInstalledPath;
			static const TCHAR* MLXFolder = TEXT("SOFTWARE\\Mellanox\\MLNX_WinOF2");
			static const TCHAR* InstalledPathKey = TEXT("InstalledPath");
			const bool bFoundKey = FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, MLXFolder, InstalledPathKey, MellanoxInstalledPath);
			if (bFoundKey)
			{
				// Mellanox installed path will point to OF2 folder. We need to get the parent of it since that's where Rivermax will get installed by default.
				const FString MellanoxRootDir = FPaths::Combine(MellanoxInstalledPath, TEXT("..\\Rivermax\\lib"));
				if (FPaths::DirectoryExists(MellanoxRootDir))
				{
					// Mellanox based Rivermax directory exists, replace default root dir
					LibraryPath = MellanoxRootDir;
				}
			}
		}

		if (FPaths::DirectoryExists(LibraryPath))
		{
			FPlatformProcess::PushDllDirectory(*LibraryPath);
			const FString LibraryName = TEXT(PREPROCESSOR_TO_STRING(RIVERMAX_LIBRARY_NAME));
			LibraryHandle = FPlatformProcess::GetDllHandle(*LibraryName);
			FPlatformProcess::PopDllDirectory(*LibraryPath);
		}
#endif

		const bool bIsLibraryValid = LibraryHandle != nullptr;
		if(!bIsLibraryValid)
		{
			UE_LOG(LogRivermax, Log, TEXT("Failed to load Rivermax library."));
		}
		else
		{
			UE_LOG(LogRivermax, Display, TEXT("Succesfully loaded Rivermax library from '%s'"), *LibraryPath);
		}

		return bIsLibraryValid;
	}

	uint64 FRivermaxManager::GetTime() const
	{
		uint64 Time = 0;

		switch (GetTimeSource())
		{
			case ERivermaxTimeSource::PTP:
			{
				//Rivermax is usable and configured to read PTP
				if (GetApi()->rmx_get_time(RMX_TIME_PTP, &Time) != RMX_OK)
				{
					UE_LOG(LogRivermax, Warning, TEXT("PTP time is the time source but was unavailable"));
				}
				break;
			}
			case ERivermaxTimeSource::Engine:
			{
				Time = uint64(FPlatformTime::Seconds() * 1E9);
				break;
			}
			case ERivermaxTimeSource::System:
			{
				// Using chrono to get nanoseconds precision. 
				Time = (uint64_t)std::chrono::duration_cast<std::chrono::nanoseconds>((std::chrono::system_clock::now()).time_since_epoch()).count();
				break;
			}
			default:
			{
				checkNoEntry();
			}
		}

		return Time;
	}

	bool FRivermaxManager::LoadRivermaxFunctions()
	{
		memset(&FuncList, 0, sizeof(RIVERMAX_API_FUNCTION_LIST));
		bIsLibraryLoaded = UE::RivermaxCore::Private::LoadLibraryFunctions(&FuncList, LibraryHandle);

		if (!bIsLibraryLoaded)
		{
			UE_LOG(LogRivermax, Log, TEXT("Failed to load Rivermax library functions."));
		}

		return bIsLibraryLoaded;
	}

	ERivermaxTimeSource FRivermaxManager::GetTimeSource() const
	{
		return TimeSource;
	}

	void FRivermaxManager::InitializeLibrary()
	{
		bool bCanProceed = FApp::CanEverRender();
		
		if (bCanProceed)
		{
			bCanProceed = VerifyPrerequesites();
		}

		if(bCanProceed)
		{
			bCanProceed = LoadRivermaxLibrary();
		}
		
		if (bCanProceed)
		{
			bCanProceed = LoadRivermaxFunctions();
		}

		if (bCanProceed)
		{
			// Load the specific version we have the API For
			const rmx_version Policy = 
			{
				RMX_VERSION_MAJOR,
				RMX_VERSION_MINOR,
				RMX_VERSION_PATCH
			};

			// Bug with 1.31 version where this is required to be called before _init() even though it is optional
			GetApi()->rmx_enable_system_signal_handling();
			
			const rmx_status Status = GetApi()->_rmx_init(&Policy);
			if (Status == RMX_OK)
			{
				bIsCleanupRequired = true;

				const URivermaxSettings* Settings = GetDefault<URivermaxSettings>();
				TimeSource = Settings->TimeSource;
				bool bIsClockConfigured = InitializeClock(TimeSource);

				if (!bIsClockConfigured && TimeSource == ERivermaxTimeSource::PTP)
				{
					TimeSource = ERivermaxTimeSource::System;
					bIsClockConfigured = InitializeClock(TimeSource);
				}

				if (bIsClockConfigured)
				{
					const rmx_version* CurrentVersion = GetApi()->rmx_get_version_numbers();
					if (CurrentVersion)
					{
						if (CVarRivermaxEnableGPUDirectCapability.GetValueOnGameThread())
						{
							VerifyGPUDirectCapability();
						}

						InitializeTraceMarkupStrings();

						bIsLibraryInitialized = true;
						const TCHAR* GPUDirectSupport = bIsGPUDirectSupported ? TEXT("with GPUDirect support.") : TEXT("without GPUDirect support.");
						UE_LOG(LogRivermax, Log, TEXT("Rivermax library version %d.%d.%d succesfully initialized, %s"), CurrentVersion->major, CurrentVersion->minor, CurrentVersion->patch, GPUDirectSupport);
					}
					else
					{
						UE_LOG(LogRivermax, Log, TEXT("Failed to retrieve Rivermax library version. Status: %d"), Status);
					}
				}
			}
			else if (Status == RMX_LICENSE_ISSUE)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Rivermax License could not be found. Have you configured RIVERMAX_LICENSE_PATH environment variable?"));
			}
			else if (Status > RMX_INVALID_PARAM_MIX && Status <= RMX_INVALID_PARAM_10)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Rivermax initialization error, installed Rivermax SDK must be version %d.%d.%d."), RMX_VERSION_MAJOR, RMX_VERSION_MINOR, RMX_VERSION_PATCH);
			}

			if (bIsLibraryInitialized == false)
			{
				UE_LOG(LogRivermax, Error, TEXT("Rivermax library failed to initialize. Status: %d"), Status);
			}
		}
		else
		{
			UE_LOG(LogRivermax, Log, TEXT("Skipping Rivermax initialization. Library won't be usable."));
		}

		bIsInitialized = true;
		PostInitDelegate.Broadcast();
	}

	TConstArrayView<UE::RivermaxCore::FRivermaxDeviceInfo> FRivermaxManager::GetDevices() const
	{
		return DeviceFinder->GetDevices();
	}

	bool FRivermaxManager::GetMatchingDevice(const FString& InSourceIP, FString& OutDeviceIP) const
	{
		return DeviceFinder->ResolveIP(InSourceIP, OutDeviceIP);
	}

	bool FRivermaxManager::IsValidIP(const FString& InSourceIP) const
	{
		return DeviceFinder->IsValidIP(InSourceIP);
	}

	bool FRivermaxManager::IsGPUDirectSupported() const
	{
		return bIsGPUDirectSupported;
	}

	void FRivermaxManager::VerifyGPUDirectCapability()
	{
		bIsGPUDirectSupported = false;

		const ERHIInterfaceType RHIType = RHIGetInterfaceType();
		if (RHIType != ERHIInterfaceType::D3D12)
		{
			UE_LOG(LogRivermax, Log, TEXT("GPUDirect won't be available for Rivermax as it's only available with D3D12 RHI."));
			return;
		}
		
		FCUDAModule* CudaModule = FModuleManager::GetModulePtr<FCUDAModule>("CUDA");
		if (CudaModule)
		{	
			CUcontext Context = CudaModule->GetCudaContext();
			if (Context == nullptr)
			{
				UE_LOG(LogRivermax, Log, TEXT("GPUDirect won't be available for Rivermax since Cuda context can't be fetched."));
				return;
			}

			CudaModule->DriverAPI()->cuCtxPushCurrent(Context);

			int DeviceCount = -1;
			CUresult Result = CudaModule->DriverAPI()->cuDeviceGetCount(&DeviceCount);
			if (DeviceCount <= 0)
			{
				UE_LOG(LogRivermax, Log, TEXT("No Cuda compatible device found. GPUDirect won't be available for Rivermax. Cuda status: %d"), Result);
				return;
			}

			// todo: add support for mgpu compatibility verification
			const int GPUIndex = CudaModule->GetCudaDeviceIndex();
			CUdevice CudaDevice;
			Result = CudaModule->DriverAPI()->cuDeviceGet(&CudaDevice, GPUIndex);
			if (Result != CUDA_SUCCESS)
			{
				UE_LOG(LogRivermax, Log, TEXT("Could not get a valid Cuda device. GPUDirect won't be available for Rivermax. Cuda status: %d"), Result);
				return;
			}


			int SupportsVMM = 0;
			Result  = CudaModule->DriverAPI()->cuDeviceGetAttribute(&SupportsVMM, CU_DEVICE_ATTRIBUTE_VIRTUAL_MEMORY_MANAGEMENT_SUPPORTED, CudaDevice);
			if (SupportsVMM != 1)
			{
				UE_LOG(LogRivermax, Log, TEXT("Cuda device doesn't support virtual memory management. GPUDirect won't be available for Rivermax. Cuda status: %d"), Result);
				return;
			}

			int SupportsRDMA = 0;
			Result = CudaModule->DriverAPI()->cuDeviceGetAttribute(&SupportsRDMA, CU_DEVICE_ATTRIBUTE_GPU_DIRECT_RDMA_WITH_CUDA_VMM_SUPPORTED, CudaDevice);
			if (SupportsRDMA != 1)
			{
				UE_LOG(LogRivermax, Log, TEXT("Cuda device doesn't support RDMA. GPUDirect won't be available for Rivermax. Cuda status: %d"), Result);
				return;
			}

			bIsGPUDirectSupported = true;
		}
	}

	bool FRivermaxManager::InitializeClock(ERivermaxTimeSource DesiredTimeSource)
	{
		switch (DesiredTimeSource)
		{
			case ERivermaxTimeSource::PTP:
			{
				FString PTPAddress;
				const URivermaxSettings* Settings = GetDefault<URivermaxSettings>();
				if (DeviceFinder->ResolveIP(Settings->PTPInterfaceAddress, PTPAddress))
				{
					in_addr DeviceIPAddr;
					if (inet_pton(AF_INET, StringCast<ANSICHAR>(*PTPAddress).Get(), &DeviceIPAddr))
					{
						UE_LOG(LogRivermax, Display, TEXT("Configure Rivermax clock for PTP using device interface %s"), *PTPAddress);
						rmx_device_iface DeviceInterface;
						rmx_ip_addr RmaxDeviceAddr;
						RmaxDeviceAddr.family = AF_INET;
						RmaxDeviceAddr.addr.ipv4 = DeviceIPAddr;
						rmx_status Status = GetApi()->rmx_retrieve_device_iface(&DeviceInterface, &RmaxDeviceAddr);
						if (Status == RMX_OK)
						{
							rmx_ptp_clock_params PTPClockParameters;
							GetApi()->rmx_init_ptp_clock(&PTPClockParameters);
							GetApi()->rmx_set_ptp_clock_device(&PTPClockParameters, &DeviceInterface);
							Status = GetApi()->rmx_use_ptp_clock(&PTPClockParameters);
							if (Status == RMX_OK)
							{
								return true;
							}
							else if (Status == RMX_CLOCK_TYPE_NOT_SUPPORTED)
							{
								UE_LOG(LogRivermax, Warning, TEXT("PTP clock not supported on device interface %s. Falling back to system clock."), *PTPAddress);
							}
							else
							{
								UE_LOG(LogRivermax, Warning, TEXT("Failed to configure PTP clock on device interface %s with error '%d'. Falling back to system clock."), *PTPAddress, Status);
							}
						}
						else
						{
							UE_LOG(LogRivermax, Warning, TEXT("Failed to get device with IP %s during PTP clock configuration, Status: '%d'. Falling back to system clock."), *PTPAddress, Status);
						}

						return false;
					}
				}

				UE_LOG(LogRivermax, Warning, TEXT("Failed to configure PTP clock. Device interface %s was invalid. Falling back to system clock."), *PTPAddress);
				return false;
			}
			case ERivermaxTimeSource::Engine:
			{
				rmx_user_clock_params UserClockParameters;
				FuncList.rmx_init_user_clock(&UserClockParameters);
				FuncList.rmx_set_user_clock_handler(&UserClockParameters, RivermaxTimeHandler);

				UE_LOG(LogRivermax, Display, TEXT("Configure Rivermax clock for engine time"));
				const rmx_status Status = FuncList.rmx_use_user_clock(&UserClockParameters);
				if (Status != RMX_OK)
				{
					UE_LOG(LogRivermax, Warning, TEXT("Failed to configure clock to use engine time with error '%d'."), Status);
				}

				return Status == RMX_OK;
			}
			case ERivermaxTimeSource::System:
			{
				// System clock is the default Rivermax clock if none others are used.
				UE_LOG(LogRivermax, Display, TEXT("Configure Rivermax clock for system time"));
				return true;
			}
			default:
			{
				checkNoEntry();
				return false;
			}
		}
	}

	bool FRivermaxManager::IsLibraryInitialized() const
	{
		return bIsLibraryInitialized;
	}
	
	bool FRivermaxManager::ValidateLibraryIsLoaded() const
	{
		static const FText ErrorText = LOCTEXT("RivermaxLibraryError", "Rivermax library was not initialized correctly. Did you install the Rivermax SDK?");

		if (!bIsLibraryInitialized)
		{
#if WITH_EDITOR
			if (GEditor)
			{
				FNotificationInfo Info{ErrorText};
				Info.ExpireDuration = 5.0f;
				FSlateNotificationManager::Get().AddNotification(Info);
			}
#endif
			UE_LOG(LogRivermax, Error, TEXT("%s"), *ErrorText.ToString());
		}
		
		return bIsLibraryInitialized;
	}

	const UE::RivermaxCore::Private::RIVERMAX_API_FUNCTION_LIST* FRivermaxManager::GetApi() const
	{
		if (bIsLibraryLoaded)
		{
			return &FuncList;
		}

		return nullptr;
	}

	FOnPostRivermaxManagerInit& FRivermaxManager::OnPostRivermaxManagerInit()
	{
		return PostInitDelegate;
	}

	bool FRivermaxManager::VerifyPrerequesites()
	{
		// This registry key is required in order to use rivermax sdk. This puts the network adapter in rivermax mode.
		// With 1.20, SDK currently crashes if that is not present / set. Avoid loading the library if that's the case and notify the user.
		
		bool bSuccess = false;
		FString RivermaxSetupValue;
		static const TCHAR* MLXFolder = TEXT("SYSTEM\\CurrentControlSet\\Services\\mlx5\\Parameters");
		static const TCHAR* RivermaxKey = TEXT("RivermaxSetup");
		const bool bFoundKey = FWindowsPlatformMisc::QueryRegKey(HKEY_LOCAL_MACHINE, MLXFolder, RivermaxKey, RivermaxSetupValue);
		if (bFoundKey)
		{
			int32 KeyValue = 0;
			LexFromString(KeyValue, *RivermaxSetupValue);
			if (KeyValue == 1)
			{
				bSuccess = true;
				UE_LOG(LogRivermax, Verbose, TEXT("Rivermax setup registry key was found and enabled."));
			}
			else
			{
				UE_LOG(LogRivermax, Warning, TEXT("Rivermax setup registry key was found but not enabled."));
			}
		}
		else
		{
			UE_LOG(LogRivermax, Warning, TEXT("Could not find RivermaxSetup registry key. Verify installation and reboot your machine."));
		}

		if (bSuccess)
		{
			// Crash was reported calling rmax_init with all prerequesite installation correctly done but no card installed. 
			// This will prevent us from going forward with rivermax initialization
			bSuccess = DeviceFinder->GetDevices().Num() > 0;
			if (!bSuccess)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Could not find any compatible Rivermax network adapters."));
			}
		}

		return bSuccess;
	}

	bool FRivermaxManager::IsGPUDirectInputSupported() const
	{
		return bIsGPUDirectSupported && CVarRivermaxEnableGPUDirectInputCapability.GetValueOnAnyThread();
	}

	bool FRivermaxManager::IsGPUDirectOutputSupported() const
	{
		return bIsGPUDirectSupported && CVarRivermaxEnableGPUDirectOutputCapability.GetValueOnAnyThread();
	}

	bool FRivermaxManager::EnableDynamicHeaderSupport(const FString& Interface)
	{
		if(uint32* Users = DynamicHeaderUsers.Find(Interface))
		{
			(*Users)++;
			return true;
		}

		sockaddr_in RivermaxDevice;
		FMemory::Memset(&RivermaxDevice, 0, sizeof(RivermaxDevice));
		if (inet_pton(AF_INET, StringCast<ANSICHAR>(*Interface).Get(), &RivermaxDevice.sin_addr) != 1)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Failed to use inet_pton for interface '%s'"), *Interface);
			return false;
		}

		rmx_device_iface DeviceInterface;
		rmx_ip_addr RmaxDeviceAddr;
		RmaxDeviceAddr.family = AF_INET;
		RmaxDeviceAddr.addr.ipv4 = RivermaxDevice.sin_addr;
		rmx_status Status = GetApi()->rmx_retrieve_device_iface(&DeviceInterface, &RmaxDeviceAddr);
		if (Status != RMX_OK)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Failed to get device for interface '%s'"), *Interface);
			return false;
		}

		rmx_device_capabilities Capabilities;
		rmx_clear_device_capabilities_enquiry(&Capabilities);
		rmx_mark_device_capability_for_enquiry(&Capabilities, RMX_DEVICE_CAP_RTP_DYNAMIC_HDS);
		Status = GetApi()->rmx_enquire_device_capabilities(&DeviceInterface, &Capabilities);
		if (Status != RMX_OK) 
		{
			UE_LOG(LogRivermax, Warning, TEXT("Failed to query capabilities for device IP '%s'"), *Interface);
			return false;
		}

		const bool bIsDynamicHeaderSplitSupported = rmx_is_device_capability_supported(&Capabilities, RMX_DEVICE_CAP_RTP_DYNAMIC_HDS);
		if (!bIsDynamicHeaderSplitSupported) 
		{
			UE_LOG(LogRivermax, Warning, TEXT("RTP dynamic header data split is not supported for device IP '%s'.\nMake sure firmware is updated and FLEX_PARSER is configured according to deployment guide."), *Interface)
			return false;
		}

		rmx_device_config DeviceConfiguration;
		rmx_clear_device_config_attributes(&DeviceConfiguration);
		constexpr rmx_device_config_attribute Attribute = RMX_DEVICE_CONFIG_RTP_SMPTE_2110_20_DYNAMIC_HDS;
		rmx_set_device_config_attribute(&DeviceConfiguration, Attribute);
		
		Status = GetApi()->rmx_apply_device_config(&DeviceInterface, &DeviceConfiguration);
		if (Status != RMX_OK)
		{
			UE_LOG(LogRivermax, Warning, TEXT("Could not enable dynamic header split for device IP '%s'. Error: %d"), *Interface, Status);
			return false;
		}

		UE_LOG(LogRivermax, Log, TEXT("Dynamic header split for device IP '%s' has been enabled."), *Interface);
		DynamicHeaderUsers.FindOrAdd(Interface, 1);
		return true;
	}

	void FRivermaxManager::DisableDynamicHeaderSupport(const FString& Interface)
	{
		if (uint32* Users = DynamicHeaderUsers.Find(Interface))
		{
			(*Users)--;
			if((*Users) > 0)
			{
				return;
			}

			bool bCanUnsetDevice = true;
			sockaddr_in DeviceToUse;
			FMemory::Memset(&DeviceToUse, 0, sizeof(DeviceToUse));
			if (inet_pton(AF_INET, StringCast<ANSICHAR>(*Interface).Get(), &DeviceToUse.sin_addr) != 1)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Failed to use inet_pton for interface '%s'"), *Interface);
				bCanUnsetDevice = false;
			}

			rmx_device_iface DeviceInterface;
			if (bCanUnsetDevice)
			{
				rmx_ip_addr RmaxDeviceAddr;
				RmaxDeviceAddr.family = AF_INET;
				RmaxDeviceAddr.addr.ipv4 = DeviceToUse.sin_addr;
				rmx_status Status = GetApi()->rmx_retrieve_device_iface(&DeviceInterface, &RmaxDeviceAddr);
				if (Status != RMX_OK)
				{
					UE_LOG(LogRivermax, Warning, TEXT("Failed to get device for interface '%s'"), *Interface);
					bCanUnsetDevice = false;
				}
			}

			if (bCanUnsetDevice)
			{

				rmx_device_config DeviceConfiguration;
				rmx_clear_device_config_attributes(&DeviceConfiguration);
				constexpr rmx_device_config_attribute Attribute = RMX_DEVICE_CONFIG_RTP_SMPTE_2110_20_DYNAMIC_HDS;
				rmx_set_device_config_attribute(&DeviceConfiguration, Attribute);

				const rmx_status Status = GetApi()->rmx_revert_device_config(&DeviceInterface, &DeviceConfiguration);
				if (Status != RMX_OK)
				{
					UE_LOG(LogRivermax, Warning, TEXT("Could not disable dynamic header split for device IP '%s'. Error: %d"), *Interface, Status);
				}
				else
				{
					UE_LOG(LogRivermax, Log, TEXT("Dynamic header split for device IP '%s' has been disabled."), *Interface);
				}
			}

			DynamicHeaderUsers.Remove(Interface);
		}
	}

	void FRivermaxManager::InitializeTraceMarkupStrings()
	{
		constexpr uint8 MarkupCount = 10;

		for (uint8 Index = 0; Index < MarkupCount; ++Index)
		{
			FRivermaxTracingUtils::RmaxOutMediaCapturePipeTraceEvents.Add(Index, FString::Printf(TEXT("MediaCapturePipe: %u"), Index));
			FRivermaxTracingUtils::RmaxOutSendingFrameTraceEvents.Add(Index, FString::Printf(TEXT("RmaxOut Sending: %u"), Index));
			FRivermaxTracingUtils::RmaxOutFrameReadyTraceEvents.Add(Index, FString::Printf(TEXT("RmaxOut FrameReady: %u"), Index));
			FRivermaxTracingUtils::RmaxInStartingFrameTraceEvents.Add(Index, FString::Printf(TEXT("RmaxIn Starting: %u"), Index));
			FRivermaxTracingUtils::RmaxInReceivedFrameTraceEvents.Add(Index, FString::Printf(TEXT("RmaxIn Received: %u"), Index));
			FRivermaxTracingUtils::RmaxInSelectedFrameTraceEvents.Add(Index, FString::Printf(TEXT("RmaxIn Selected: %u"), Index));
		}
	}

}

#undef LOCTEXT_NAMESPACE // RivermaxManager

