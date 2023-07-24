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
#include "RivermaxHeader.h"
#include "RivermaxLog.h"

#include <chrono>

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"

#include <ws2tcpip.h>

#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif


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
			rmax_status_t Status = rmax_cleanup();
			if (Status != RMAX_OK)
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
		const FString LibraryPath = FPaths::Combine(FPaths::EngineSourceDir(), TEXT("ThirdParty/NVIDIA/Rivermax/lib"), TEXT(PREPROCESSOR_TO_STRING(RIVERMAX_LIBRARY_PLATFORM_PATH)));
		const FString LibraryName = TEXT(PREPROCESSOR_TO_STRING(RIVERMAX_LIBRARY_NAME));

		FPlatformProcess::PushDllDirectory(*LibraryPath);
		LibraryHandle = FPlatformProcess::GetDllHandle(*LibraryName);
		FPlatformProcess::PopDllDirectory(*LibraryPath);
#endif

		const bool bIsLibraryValid = LibraryHandle != nullptr;
		if(!bIsLibraryValid)
		{
			UE_LOG(LogRivermax, Log, TEXT("Failed to load required library %s. Rivermax library will not be functional."), *LibraryName);
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
				if (rmax_get_time(RMAX_CLOCK_PTP, &Time) != RMAX_OK)
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
			rmax_init_config Config;
			memset(&Config, 0, sizeof(Config));

			rmax_status_t Status = rmax_init(&Config);
			if (Status == RMAX_OK)
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
					uint32 Major = 0;
					uint32 Minor = 0;
					uint32 Release = 0;
					uint32 Build = 0;
					Status = rmax_get_version(&Major, &Minor, &Release, &Build);
					if (Status == RMAX_OK)
					{
						if (CVarRivermaxEnableGPUDirectCapability.GetValueOnGameThread())
						{
							VerifyGPUDirectCapability();
						}

						bIsLibraryInitialized = true;
						const TCHAR* GPUDirectSupport = bIsGPUDirectSupported ? TEXT("with GPUDirect support.") : TEXT("without GPUDirect support.");
						UE_LOG(LogRivermax, Log, TEXT("Rivermax library version %d.%d.%d.%d succesfully initialized, %s"), Major, Minor, Release, Build, GPUDirectSupport);
					}
					else
					{
						UE_LOG(LogRivermax, Log, TEXT("Failed to retrieve Rivermax library version. Status: %d"), Status);
					}
				}
			}
			else if (Status == RMAX_ERR_LICENSE_ISSUE)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Rivermax License could not be found. Have you configured RIVERMAX_LICENSE_PATH environment variable?"));
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
		rmax_clock_t ClockConfig;
		memset(&ClockConfig, 0, sizeof(ClockConfig));

		switch (DesiredTimeSource)
		{
			case ERivermaxTimeSource::PTP:
			{
				ClockConfig.clock_type = rmax_clock_types::RIVERMAX_PTP_CLOCK;

				FString PTPAddress;
				const URivermaxSettings* Settings = GetDefault<URivermaxSettings>();
				if (DeviceFinder->ResolveIP(Settings->PTPInterfaceAddress, PTPAddress))
				{
					if (inet_pton(AF_INET, StringCast<ANSICHAR>(*PTPAddress).Get(), &ClockConfig.clock_u.rmax_ptp_clock.device_ip_addr))
					{
						UE_LOG(LogRivermax, Display, TEXT("Configure Rivermax clock for PTP using device interface %s"), *PTPAddress);
						rmax_status_t Status = rmax_set_clock(&ClockConfig);
						if (Status == RMAX_OK)
						{
							return true;
						}
						else if (Status == RMAX_ERR_CLOCK_TYPE_NOT_SUPPORTED)
						{
							UE_LOG(LogRivermax, Warning, TEXT("PTP clock not supported on device interface %s. Falling back to system clock."), *PTPAddress);
						}
						else
						{
							UE_LOG(LogRivermax, Warning, TEXT("Failed to configure PTP clock on device interface %s with error '%d'. Falling back to system clock."), *PTPAddress, Status);
						}

						return false;
					}
				}

				UE_LOG(LogRivermax, Warning, TEXT("Failed to configure PTP clock. Device interface %s was invalid. Falling back to system clock."), *PTPAddress);
				return false;
			}
			case ERivermaxTimeSource::Engine:
			{
				ClockConfig.clock_type = rmax_clock_types::RIVERMAX_USER_CLOCK_HANDLER;
				ClockConfig.clock_u.rmax_user_clock_handler.clock_handler = RivermaxTimeHandler;
				ClockConfig.clock_u.rmax_user_clock_handler.ctx = nullptr;

				UE_LOG(LogRivermax, Display, TEXT("Configure Rivermax clock for engine time"));
				rmax_status_t Status = rmax_set_clock(&ClockConfig);
				if (Status != RMAX_OK)
				{
					UE_LOG(LogRivermax, Warning, TEXT("Failed to configure clock to use engine time with error '%d'."), Status);
				}

				return Status == RMAX_OK;
			}
			case ERivermaxTimeSource::System:
			{
				ClockConfig.clock_type = rmax_clock_types::RIVERMAX_SYSTEM_CLOCK;

				UE_LOG(LogRivermax, Display, TEXT("Configure Rivermax clock for system time"));
				rmax_status_t Status = rmax_set_clock(&ClockConfig);
				if (Status != RMAX_OK)
				{
					UE_LOG(LogRivermax, Warning, TEXT("Failed to configure clock to use system time with error '%d'."), Status);
				}

				return Status == RMAX_OK;
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

		uint64_t CapabilityMask = RMAX_DEV_CAP_RTP_DYNAMIC_HDS;
		rmax_device_caps_t Capabilities;
		Capabilities.supported_caps = 0;

		rmax_status_t Status = rmax_device_get_caps(RivermaxDevice.sin_addr, CapabilityMask, &Capabilities);
		if (Status != RMAX_OK) 
		{
			UE_LOG(LogRivermax, Warning, TEXT("Failed to query capabilities for device IP '%s'"), *Interface);
			return false;
		}

		const bool bIsDynamicHeaderSplitSupported = (Capabilities.supported_caps & RMAX_DEV_CAP_RTP_DYNAMIC_HDS) != 0;
		if (!bIsDynamicHeaderSplitSupported) 
		{
			UE_LOG(LogRivermax, Warning, TEXT("RTP dynamic header data split is not supported for device IP '%s'"), *Interface)
			return false;
		}

		rmax_device_config_t DeviceConfig;
		FMemory::Memset(&DeviceConfig, 0, sizeof(DeviceConfig));
		DeviceConfig.config_flags = RMAX_DEV_CONFIG_RTP_SMPTE_2110_20_DYNAMIC_HDS_CONFIG;
		DeviceConfig.ip_address = RivermaxDevice.sin_addr;

		Status = rmax_set_device_config(&DeviceConfig);
		if (Status != RMAX_OK)
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

			if (bCanUnsetDevice)
			{
				rmax_device_config_t DeviceConfig;
				FMemory::Memset(&DeviceConfig, 0, sizeof(DeviceConfig));
				DeviceConfig.config_flags = RMAX_DEV_CONFIG_RTP_SMPTE_2110_20_DYNAMIC_HDS_CONFIG;
				DeviceConfig.ip_address = DeviceToUse.sin_addr;

				const rmax_status_t Status = rmax_unset_device_config(&DeviceConfig);
				if (Status != RMAX_OK)
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

}


