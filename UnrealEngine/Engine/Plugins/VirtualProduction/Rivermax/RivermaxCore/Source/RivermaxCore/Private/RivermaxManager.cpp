// Copyright Epic Games, Inc. All Rights Reserved.

#include "RivermaxManager.h"

#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "IRivermaxCoreModule.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/CoreDelegates.h"
#include "RivermaxDeviceFinder.h"
#include "RivermaxHeader.h"
#include "RivermaxLog.h"



#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"

#include <ws2tcpip.h>

#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif



namespace UE::RivermaxCore::Private
{
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
		const bool bIsLibraryLoaded = LoadRivermaxLibrary();
		if (bIsLibraryLoaded && FApp::CanEverRender())
		{
			//Postpone initialization after all modules have been loaded
			FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddRaw(this, &FRivermaxManager::InitializeLibrary);
		}
		else
		{
			UE_LOG(LogRivermax, Log, TEXT("Skipping Rivermax initialization. Library won't be usable."));
		}
	}

	FRivermaxManager::~FRivermaxManager()
	{
		if (IsInitialized())
		{
			rmax_status_t Status = rmax_cleanup();
			if (Status != RMAX_OK)
			{
				UE_LOG(LogRivermax, Log, TEXT("Failed to cleanup Rivermax Library. Status: %d"), Status);
			}
			UE_LOG(LogRivermax, Log, TEXT("Rivermax Library has shutdown."));
		}

		if (LibraryHandle != nullptr)
		{
			FPlatformProcess::FreeDllHandle(LibraryHandle);
			LibraryHandle = nullptr;
		}
	}

	bool FRivermaxManager::IsInitialized() const
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

		if (IsInitialized() && GetTimeSource() == ERivermaxTimeSource::PTP)
		{
			//Rivermax is usable and configured to read PTP
			if (rmax_get_time(RMAX_CLOCK_PTP, &Time) != RMAX_OK)
			{
				UE_LOG(LogRivermax, Warning, TEXT("PTP time is the time source but was unavailable"));
			}
		}
		else
		{
			// PTP was not available so we fall back on system time
			Time = uint64(FPlatformTime::Seconds() * 1E9);
		}

		return Time;
	}

	ERivermaxTimeSource FRivermaxManager::GetTimeSource() const
	{
		return TimeSource;
	}

	void FRivermaxManager::InitializeLibrary()
	{
		rmax_init_config Config;
		memset(&Config, 0, sizeof(Config));
		Config.flags |= RIVERMAX_ENABLE_CLOCK_CONFIGURATION;
			
		const URivermaxSettings* Settings = GetDefault<URivermaxSettings>();

		bool bConfiguredSuccessfully = false;
		if (Settings->TimeSource == ERivermaxTimeSource::PTP)
		{
			Config.clock_configurations.clock_type = rmax_clock_types::RIVERMAX_PTP_CLOCK;

			FString PTPAddress;
			if (DeviceFinder->ResolveIP(Settings->PTPInterfaceAddress, PTPAddress))
			{
				if (inet_pton(AF_INET, StringCast<ANSICHAR>(*PTPAddress).Get(), &Config.clock_configurations.clock_u.rmax_ptp_clock.device_ip_addr))
				{
					UE_LOG(LogRivermax, Display, TEXT("Initialize Rivermax clock as PTP using device interface %s"), *PTPAddress);
					TimeSource = ERivermaxTimeSource::PTP;
					bConfiguredSuccessfully = true;
				}
			}

			if (bConfiguredSuccessfully == false)
			{
				UE_LOG(LogRivermax, Warning, TEXT("Could not configure Rivermax to use PTP using interface '%s'. Falling back to platform time."), *Settings->PTPInterfaceAddress);
			}
		}

		if(bConfiguredSuccessfully == false)
		{
			// Todo: Need to update clock fetcher to align with UTC. Refer to SDK
			Config.clock_configurations.clock_type = rmax_clock_types::RIVERMAX_USER_CLOCK_HANDLER;
			Config.clock_configurations.clock_u.rmax_user_clock_handler.clock_handler = RivermaxTimeHandler;
			Config.clock_configurations.clock_u.rmax_user_clock_handler.ctx = nullptr;
			TimeSource = ERivermaxTimeSource::Platform;
		}

		rmax_status_t Status = rmax_init(&Config);
		if (Status == RMAX_OK)
		{
			uint32 Major = 0;
			uint32 Minor = 0;
			uint32 Release = 0;
			uint32 Build = 0;
			Status = rmax_get_version(&Major, &Minor, &Release, &Build);
			if (Status == RMAX_OK)
			{
				bIsInitialized = true;
				UE_LOG(LogRivermax, Log, TEXT("Rivermax library version %d.%d.%d.%d succesfully initialized"), Major, Minor, Release, Build);
			}
			else
			{
				UE_LOG(LogRivermax, Log, TEXT("Failed to retrieve Rivermax library version. Status: %d"), Status);
			}
		}
		else if (Status == RMAX_ERR_LICENSE_ISSUE)
		{
			UE_LOG(LogRivermax, Log, TEXT("Rivermax License could not be found. Have you configured RIVERMAX_LICENSE_PATH environment variable?"));
		}

		if (bIsInitialized == false)
		{
			UE_LOG(LogRivermax, Log, TEXT("Rivermax library failed to initialize. Status: %d"), Status);
		}
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
}





