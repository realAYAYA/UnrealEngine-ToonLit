// Copyright Epic Games, Inc. All Rights Reserved.

#include "EOSSDKManager.h"

#if WITH_EOS_SDK

#include "Algo/AnyOf.h"
#include "Containers/Ticker.h"
#include "HAL/LowLevelMemTracker.h"

#if WITH_ENGINE
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformInput.h"
#include "InputCoreTypes.h"
#endif

#include "Misc/ScopeRWLock.h"
#include "Misc/App.h"
#include "Misc/CoreMisc.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Fork.h"
#include "Misc/NetworkVersion.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ProfilingDebugging/CallstackTrace.h"
#include "Stats/Stats.h"

#include "CoreGlobals.h"

#include "EOSShared.h"
#include "EOSSharedModule.h"
#include "EOSSharedTypes.h"

#include "eos_auth.h"
#include "eos_connect.h"
#include "eos_friends.h"
#include "eos_init.h"
#include "eos_integratedplatform_types.h"
#include "eos_logging.h"
#include "eos_presence.h"
#include "eos_userinfo.h"
#include "eos_version.h"

#ifndef EOS_TRACE_MALLOC
#define EOS_TRACE_MALLOC 1
#endif

namespace
{
	static void* EOS_MEMORY_CALL EosMalloc(size_t Bytes, size_t Alignment)
	{
		LLM_SCOPE(ELLMTag::RealTimeCommunications);

#if !EOS_TRACE_MALLOC
		CALLSTACK_TRACE_LIMIT_CALLSTACKRESOLVE_SCOPE();
#endif

		return FMemory::Malloc(Bytes, Alignment);
	}

	static void* EOS_MEMORY_CALL EosRealloc(void* Ptr, size_t Bytes, size_t Alignment)
	{
		LLM_SCOPE(ELLMTag::RealTimeCommunications);

#if !EOS_TRACE_MALLOC
		CALLSTACK_TRACE_LIMIT_CALLSTACKRESOLVE_SCOPE();
#endif

		return FMemory::Realloc(Ptr, Bytes, Alignment);
	}

	static void EOS_MEMORY_CALL EosFree(void* Ptr)
	{
		LLM_SCOPE(ELLMTag::RealTimeCommunications);

#if !EOS_TRACE_MALLOC
		CALLSTACK_TRACE_LIMIT_CALLSTACKRESOLVE_SCOPE();
#endif

		FMemory::Free(Ptr);
	}

#if !NO_LOGGING
	bool IsLogLevelSuppressable(EOS_ELogLevel Level)
	{
		return Level > EOS_ELogLevel::EOS_LOG_Off && Level < EOS_ELogLevel::EOS_LOG_Info;
	}

	void EOS_CALL EOSLogMessageReceived(const EOS_LogMessage* Message)
	{
#define EOSLOG(Level) UE_LOG(LogEOSSDK, Level, TEXT("%s: %s"), UTF8_TO_TCHAR(Message->Category), *MessageStr)
#define EOSLOG_SUPPRESS(Level) if(bSuppressLogLevel) { EOSLOG(Log); } else { EOSLOG(Level); }

		FString MessageStr(UTF8_TO_TCHAR(Message->Message));
		MessageStr.TrimStartAndEndInline();

		// Check if this log line is suppressed.
		bool bSuppressLogLevel = false;
		if (IsLogLevelSuppressable(Message->Level))
		{
			if(FEOSSharedModule* Module = FEOSSharedModule::Get())
			{
				bSuppressLogLevel = Algo::AnyOf(Module->GetSuppressedLogStrings(), [&MessageStr](const FString& SuppressedLogString) { return MessageStr.Contains(SuppressedLogString); });
			}
		}

		switch (Message->Level)
		{
		case EOS_ELogLevel::EOS_LOG_Fatal:			EOSLOG_SUPPRESS(Fatal); break;
		case EOS_ELogLevel::EOS_LOG_Error:			EOSLOG_SUPPRESS(Error); break;
		case EOS_ELogLevel::EOS_LOG_Warning:		EOSLOG_SUPPRESS(Warning); break;
		case EOS_ELogLevel::EOS_LOG_Info:			EOSLOG(Log); break;
		case EOS_ELogLevel::EOS_LOG_Verbose:		EOSLOG(Verbose); break;
		case EOS_ELogLevel::EOS_LOG_VeryVerbose:	EOSLOG(VeryVerbose); break;
		case EOS_ELogLevel::EOS_LOG_Off:
		default:
			// do nothing
			break;
		}
#undef EOSLOG
#undef EOSLOG_SUPPRESS
	}

	EOS_ELogLevel ConvertLogLevel(ELogVerbosity::Type UELogLevel)
	{
		switch (UELogLevel)
		{
		case ELogVerbosity::NoLogging:		return EOS_ELogLevel::EOS_LOG_Off;
		case ELogVerbosity::Fatal:			return EOS_ELogLevel::EOS_LOG_Fatal;
		case ELogVerbosity::Error:			return EOS_ELogLevel::EOS_LOG_Error;
		case ELogVerbosity::Warning:		return EOS_ELogLevel::EOS_LOG_Warning;
		default:							// Intentional fall through
		case ELogVerbosity::Display:		// Intentional fall through
		case ELogVerbosity::Log:			return EOS_ELogLevel::EOS_LOG_Info;
		case ELogVerbosity::Verbose:		return EOS_ELogLevel::EOS_LOG_Verbose;
		case ELogVerbosity::VeryVerbose:	return EOS_ELogLevel::EOS_LOG_VeryVerbose;
		}
	}
#endif // !NO_LOGGING

#if EOSSDK_RUNTIME_LOAD_REQUIRED
	static void* GetSdkDllHandle()
	{
		void* Result = nullptr;

		const FString ProjectBinaryPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory(), TEXT(EOSSDK_RUNTIME_LIBRARY_NAME)));
		if (FPaths::FileExists(ProjectBinaryPath))
		{
			Result = FPlatformProcess::GetDllHandle(*ProjectBinaryPath);
		}

		if (!Result)
		{
			const FString EngineBinaryPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::EngineDir(), TEXT("Binaries"), FPlatformProcess::GetBinariesSubdirectory(), TEXT(EOSSDK_RUNTIME_LIBRARY_NAME)));
			if (FPaths::FileExists(EngineBinaryPath))
			{
				Result = FPlatformProcess::GetDllHandle(*EngineBinaryPath);
			}
		}

		if (!Result)
		{
			Result = FPlatformProcess::GetDllHandle(TEXT(EOSSDK_RUNTIME_LIBRARY_NAME));
		}

		if (!Result)
		{
			bool bDllLoadFailureIsFatal = false;
			GConfig->GetBool(TEXT("EOSSDK"), TEXT("bDllLoadFailureIsFatal"), bDllLoadFailureIsFatal, GEngineIni);
			if (bDllLoadFailureIsFatal)
			{
				FPlatformMisc::MessageBoxExt(
					EAppMsgType::Ok,
					*FText::Format(NSLOCTEXT("EOSShared", "DllLoadFail", "Failed to load {0}. Please verify your installation. Exiting..."), FText::FromString(TEXT(EOSSDK_RUNTIME_LIBRARY_NAME))).ToString(),
					TEXT("Error")
				);
				UE_LOG(LogEOSSDK, Fatal, TEXT("Failed to load EOSSDK binary"));
			}
		}

		return Result;
	}
#endif
}


// if the platform wants to preload the EOS runtime library, then use the auto registration system to kick off
// the GetDllHandle call at a specified time. it is assumed that calling GetDllHandle here will not conflict
// with the call in FEOSSDKManager::FEOSSDKManager, and will only speed it up, even if it's not complete before
// the later one is called. if that's not the case, we will need an FEvent or similar to block the constructor
// until this is complete
#if EOSSDK_RUNTIME_LOAD_REQUIRED && defined(EOS_DLL_PRELOAD_PHASE)

#include "Misc/DelayedAutoRegister.h"
#include "Async/Async.h"

static FDelayedAutoRegisterHelper GKickoffDll(EDelayedRegisterRunPhase::EOS_DLL_PRELOAD_PHASE, []
{
	Async(EAsyncExecution::Thread, []
	{
		LLM_SCOPE(ELLMTag::RealTimeCommunications);
		SCOPED_BOOT_TIMING("Preloading EOS module");
		GetSdkDllHandle();
	});
});

#endif

FEOSSDKManager::FEOSSDKManager()
{
}

FEOSSDKManager::~FEOSSDKManager()
{
#if EOSSDK_RUNTIME_LOAD_REQUIRED
	if (SDKHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(SDKHandle);
	}
#endif
}

EOS_EResult FEOSSDKManager::Initialize()
{
	if (FForkProcessHelper::IsForkRequested() && !FForkProcessHelper::IsForkedChildProcess())
	{
		UE_LOG(LogEOSSDK, Error, TEXT("FEOSSDKManager::Initialize failed, pre-fork"));
		return EOS_EResult::EOS_InvalidState;
	}

#if EOSSDK_RUNTIME_LOAD_REQUIRED
	if (SDKHandle == nullptr)
	{
		LLM_SCOPE(ELLMTag::RealTimeCommunications);
		SDKHandle = GetSdkDllHandle();
	}
	if (SDKHandle == nullptr)
	{
		UE_LOG(LogEOSSDK, Log, TEXT("FEOSSDKManager::Initialize failed, SDKHandle=nullptr"));
		return EOS_EResult::EOS_InvalidState;
	}
#endif

	if (IsInitialized())
	{
		return EOS_EResult::EOS_Success;
	}
	else
	{
		UE_LOG(LogEOSSDK, Log, TEXT("Initializing EOSSDK Version:%s"), UTF8_TO_TCHAR(EOS_GetVersion()));

		const FTCHARToUTF8 ProductName(*GetProductName());
		const FTCHARToUTF8 ProductVersion(*GetProductVersion());

		EOS_InitializeOptions InitializeOptions = {};
		InitializeOptions.ApiVersion = 4;
		UE_EOS_CHECK_API_MISMATCH(EOS_INITIALIZE_API_LATEST, 4);
		InitializeOptions.AllocateMemoryFunction = &EosMalloc;
		InitializeOptions.ReallocateMemoryFunction = &EosRealloc;
		InitializeOptions.ReleaseMemoryFunction = &EosFree;
		InitializeOptions.ProductName = ProductName.Get();
		InitializeOptions.ProductVersion = ProductVersion.Length() > 0 ? ProductVersion.Get() : nullptr;
		InitializeOptions.Reserved = nullptr;
		InitializeOptions.SystemInitializeOptions = nullptr;
		InitializeOptions.OverrideThreadAffinity = nullptr;

		EOS_EResult EosResult = EOSInitialize(InitializeOptions);

		if (EosResult == EOS_EResult::EOS_Success)
		{
			bInitialized = true;

			FCoreDelegates::TSOnConfigSectionsChanged().AddRaw(this, &FEOSSDKManager::OnConfigSectionsChanged);
			LoadConfig();

#if !NO_LOGGING
			FCoreDelegates::OnLogVerbosityChanged.AddRaw(this, &FEOSSDKManager::OnLogVerbosityChanged);

			EosResult = EOS_Logging_SetCallback(&EOSLogMessageReceived);
			if (EosResult != EOS_EResult::EOS_Success)
			{
				UE_LOG(LogEOSSDK, Warning, TEXT("EOS_Logging_SetCallback failed error:%s"), *LexToString(EosResult));
			}

			EosResult = EOS_Logging_SetLogLevel(EOS_ELogCategory::EOS_LC_ALL_CATEGORIES, ConvertLogLevel(LogEOSSDK.GetVerbosity()));
			if (EosResult != EOS_EResult::EOS_Success)
			{
				UE_LOG(LogEOSSDK, Warning, TEXT("EOS_Logging_SetLogLevel failed Verbosity=%s error=[%s]"), ToString(LogEOSSDK.GetVerbosity()), *LexToString(EosResult));
			}
#endif // !NO_LOGGING

			FCoreDelegates::OnNetworkConnectionStatusChanged.AddRaw(this, &FEOSSDKManager::OnNetworkConnectionStatusChanged);
		}
		else
		{
			UE_LOG(LogEOSSDK, Warning, TEXT("EOS_Initialize failed error:%s"), *LexToString(EosResult));
		}

		return EosResult;
	}
}

const FEOSSDKPlatformConfig* FEOSSDKManager::GetPlatformConfig(const FString& PlatformConfigName, bool bLoadIfMissing)
{
	if (PlatformConfigName.IsEmpty())
	{
		UE_LOG(LogEOSSDK, Warning, TEXT("Platform name can't be empty"));
		return nullptr;
	}

	FEOSSDKPlatformConfig* PlatformConfig = PlatformConfigs.Find(PlatformConfigName);
	if (PlatformConfig || !bLoadIfMissing)
	{
		return PlatformConfig;
	}

	const FString SectionName(TEXT("EOSSDK.Platform.") + PlatformConfigName);
	if (!GConfig->DoesSectionExist(*SectionName, GEngineIni))
	{
		UE_LOG(LogEOSSDK, Warning, TEXT("Could not find platform config: %s"), *PlatformConfigName);
		return nullptr;
	}

	PlatformConfig = &PlatformConfigs.Emplace(PlatformConfigName);

	PlatformConfig->Name = PlatformConfigName;
	GConfig->GetString(*SectionName, TEXT("ProductId"), PlatformConfig->ProductId, GEngineIni);
	GConfig->GetString(*SectionName, TEXT("SandboxId"), PlatformConfig->SandboxId, GEngineIni);
	GConfig->GetString(*SectionName, TEXT("ClientId"), PlatformConfig->ClientId, GEngineIni);
	GConfig->GetString(*SectionName, TEXT("ClientSecret"), PlatformConfig->ClientSecret, GEngineIni);
	if (GConfig->GetString(*SectionName, TEXT("EncryptionKey"), PlatformConfig->EncryptionKey, GEngineIni))
	{
		// EncryptionKey gets removed from packaged builds due to IniKeyDenylist=EncryptionKey entry in BaseGame.ini.
		// Normally we could just add a remap in ConfigRedirects.ini but the section name varies with the PlatformConfigName.
		UE_LOG(LogEOSSDK, Warning, TEXT("Config section \"EOSSDK.Platform.%s\" contains deprecated key EncryptionKey, please migrate to ClientEncryptionKey."), *PlatformConfigName);
	}
	GConfig->GetString(*SectionName, TEXT("ClientEncryptionKey"), PlatformConfig->EncryptionKey, GEngineIni);
	GConfig->GetString(*SectionName, TEXT("OverrideCountryCode"), PlatformConfig->OverrideCountryCode, GEngineIni);
	GConfig->GetString(*SectionName, TEXT("OverrideLocaleCode"), PlatformConfig->OverrideLocaleCode, GEngineIni);
	GConfig->GetString(*SectionName, TEXT("DeploymentId"), PlatformConfig->DeploymentId, GEngineIni);

	if (GConfig->GetString(*SectionName, TEXT("CacheBaseSubdirectory"), PlatformConfig->CacheDirectory, GEngineIni))
	{
		PlatformConfig->CacheDirectory = GetCacheDirBase() / PlatformConfig->CacheDirectory;
	}
	GConfig->GetString(*SectionName, TEXT("CacheDirectory"), PlatformConfig->CacheDirectory, GEngineIni);

	bool bCheckRuntimeType = false;
	GConfig->GetBool(*SectionName, TEXT("bCheckRuntimeType"), bCheckRuntimeType, GEngineIni);
	if (bCheckRuntimeType)
	{
		PlatformConfig->bIsServer = IsRunningDedicatedServer();
		PlatformConfig->bLoadingInEditor = !IsRunningGame() && !IsRunningDedicatedServer();

		if (PlatformConfig->bIsServer || PlatformConfig->bLoadingInEditor)
		{
			// Don't attempt to load overlay for servers or editors.
			PlatformConfig->bDisableOverlay = true;
			PlatformConfig->bDisableSocialOverlay = true;
		}
		else
		{
			// Overlay is on by default, enable additional overlay options.
			PlatformConfig->bWindowsEnableOverlayD3D9 = true;
			PlatformConfig->bWindowsEnableOverlayD3D10 = true;
			PlatformConfig->bWindowsEnableOverlayOpenGL = true;
		}
	}

	GConfig->GetBool(*SectionName, TEXT("bIsServer"), PlatformConfig->bIsServer, GEngineIni);
	GConfig->GetBool(*SectionName, TEXT("bLoadingInEditor"), PlatformConfig->bLoadingInEditor, GEngineIni);
	GConfig->GetBool(*SectionName, TEXT("bDisableOverlay"), PlatformConfig->bDisableOverlay, GEngineIni);
	GConfig->GetBool(*SectionName, TEXT("bDisableSocialOverlay"), PlatformConfig->bDisableSocialOverlay, GEngineIni);
	GConfig->GetBool(*SectionName, TEXT("bWindowsEnableOverlayD3D9"), PlatformConfig->bWindowsEnableOverlayD3D9, GEngineIni);
	GConfig->GetBool(*SectionName, TEXT("bWindowsEnableOverlayD3D10"), PlatformConfig->bWindowsEnableOverlayD3D10, GEngineIni);
	GConfig->GetBool(*SectionName, TEXT("bWindowsEnableOverlayOpenGL"), PlatformConfig->bWindowsEnableOverlayOpenGL, GEngineIni);
	GConfig->GetBool(*SectionName, TEXT("bEnableRTC"), PlatformConfig->bEnableRTC, GEngineIni);
	GConfig->GetInt(*SectionName, TEXT("TickBudgetInMilliseconds"), PlatformConfig->TickBudgetInMilliseconds, GEngineIni);
	GConfig->GetArray(*SectionName, TEXT("OptionalConfig"), PlatformConfig->OptionalConfig, GEngineIni);

	UE_LOG(LogEOSSDK, Verbose, TEXT("Loaded platform config: %s"), *PlatformConfigName);
	return PlatformConfig;
}

bool FEOSSDKManager::AddPlatformConfig(const FEOSSDKPlatformConfig& PlatformConfig)
{
	if (PlatformConfig.Name.IsEmpty())
	{
		UE_LOG(LogEOSSDK, Warning, TEXT("Platform name can't be empty"));
		return false;
	}

	if (PlatformConfigs.Find(PlatformConfig.Name))
	{
		UE_LOG(LogEOSSDK, Warning, TEXT("Platform config already exists: %s"), *PlatformConfig.Name);
		return false;
	}

	PlatformConfigs.Emplace(PlatformConfig.Name, PlatformConfig);
	UE_LOG(LogEOSSDK, Verbose, TEXT("Added platform config: %s"), *PlatformConfig.Name);
	return true;
}

const FString& FEOSSDKManager::GetDefaultPlatformConfigName()
{
	if (DefaultPlatformConfigName.IsEmpty())
	{
		FString PlatformConfigName;
		if (GConfig->GetString(TEXT("EOSSDK"), TEXT("DefaultPlatformConfigName"), PlatformConfigName, GEngineIni))
		{
			SetDefaultPlatformConfigName(PlatformConfigName);
		}
	}

	return DefaultPlatformConfigName;
}

void FEOSSDKManager::SetDefaultPlatformConfigName(const FString& PlatformConfigName)
{
	if (DefaultPlatformConfigName != PlatformConfigName)
	{
		UE_LOG(LogEOSSDK, Verbose, TEXT("Default platform name changed: New=%s Old=%s"), *PlatformConfigName, *DefaultPlatformConfigName);
		OnDefaultPlatformConfigNameChanged.Broadcast(PlatformConfigName, DefaultPlatformConfigName);
		DefaultPlatformConfigName = PlatformConfigName;
	}
}

EOS_HIntegratedPlatformOptionsContainer FEOSSDKManager::CreateIntegratedPlatformOptionsContainer()
{
	EOS_HIntegratedPlatformOptionsContainer Result;

	EOS_IntegratedPlatform_CreateIntegratedPlatformOptionsContainerOptions Options = { };
	Options.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_INTEGRATEDPLATFORM_CREATEINTEGRATEDPLATFORMOPTIONSCONTAINER_API_LATEST, 1);

	EOS_EResult CreationResult = EOS_IntegratedPlatform_CreateIntegratedPlatformOptionsContainer(&Options, &Result);
	if (CreationResult != EOS_EResult::EOS_Success)
	{
		UE_LOG(LogEOSSDK, Warning, TEXT("EOS_IntegratedPlatform_CreateIntegratedPlatformOptionsContainer failed with result [%s]"), *LexToString(CreationResult));
	}

	return Result;
}

const void* FEOSSDKManager::GetIntegratedPlatformOptions()
{
	return nullptr;
}

EOS_IntegratedPlatformType FEOSSDKManager::GetIntegratedPlatformType()
{
	return EOS_IPT_Unknown;
}

void FEOSSDKManager::ApplyIntegratedPlatformOptions(EOS_HIntegratedPlatformOptionsContainer& Container)
{
	if (IsRunningCommandlet())
	{
		UE_LOG(LogEOSSDK, Verbose, TEXT("[%hs] Method not supported when running Commandlet"), __FUNCTION__);
		Container = nullptr;
		return;
	}

	if (bEnablePlatformIntegration)
	{
		Container = CreateIntegratedPlatformOptionsContainer();

		if (Container != nullptr)
		{
			//UE does not support EOS_IPMF_LibraryManagedBySDK due to functionality overlap
			EOS_IntegratedPlatform_Options PlatformOptions = {};
			PlatformOptions.ApiVersion = 1;
			UE_EOS_CHECK_API_MISMATCH(EOS_INTEGRATEDPLATFORM_OPTIONS_API_LATEST, 1);
			PlatformOptions.Type = GetIntegratedPlatformType();
			PlatformOptions.Flags = IntegratedPlatformManagementFlags;
			PlatformOptions.InitOptions = GetIntegratedPlatformOptions();

			EOS_IntegratedPlatformOptionsContainer_AddOptions AddOptions = {};
			AddOptions.ApiVersion = 1;
			UE_EOS_CHECK_API_MISMATCH(EOS_INTEGRATEDPLATFORMOPTIONSCONTAINER_ADD_API_LATEST, 1);
			AddOptions.Options = &PlatformOptions;

			EOS_EResult Result = EOS_IntegratedPlatformOptionsContainer_Add(Container, &AddOptions);
			if (Result != EOS_EResult::EOS_Success)
			{
				UE_LOG(LogEOSSDK, Warning, TEXT("[%hs] Call to EOS_IntegratedPlatformOptionsContainer_Add returned with error: %s"), __FUNCTION__, *LexToString(Result));
			}
		}
	}
	else
	{
		Container = nullptr;
	}
}

void FEOSSDKManager::ApplySystemSpecificOptions(const void*& SystemSpecificOptions)
{
	SystemSpecificOptions = nullptr;
}

IEOSPlatformHandlePtr FEOSSDKManager::CreatePlatform(const FString& PlatformConfigName, FName InstanceName)
{
	if (PlatformConfigName.IsEmpty())
	{
		UE_LOG(LogEOSSDK, Warning, TEXT("Platform name can't be empty"));
		return IEOSPlatformHandlePtr();
	}

	const FEOSSDKPlatformConfig* const PlatformConfig = GetPlatformConfig(PlatformConfigName, true);
	if (!PlatformConfig)
	{
		return IEOSPlatformHandlePtr();
	}

	if (PlatformConfig->ProductId.IsEmpty() ||
		PlatformConfig->SandboxId.IsEmpty() ||
		PlatformConfig->DeploymentId.IsEmpty() ||
		PlatformConfig->ClientId.IsEmpty() ||
		PlatformConfig->ClientSecret.IsEmpty())
	{
		UE_LOG(LogEOSSDK, Warning, TEXT("Platform config missing required options"));
		return IEOSPlatformHandlePtr();
	}

	TMap<FName, IEOSPlatformHandleWeakPtr>* PlatformMap = PlatformHandles.Find(PlatformConfigName);
	if (PlatformMap)
	{
		IEOSPlatformHandleWeakPtr* WeakPlatformHandle = PlatformMap->Find(InstanceName);
		if (WeakPlatformHandle)
		{
			if (IEOSPlatformHandlePtr Pinned = WeakPlatformHandle->Pin())
			{
				UE_LOG(LogEOSSDK, Verbose, TEXT("Found existing platform handle: PlatformConfigName=%s InstanceName=%s"), *PlatformConfigName, *InstanceName.ToString());
				return Pinned;
			}

			UE_LOG(LogEOSSDK, Verbose, TEXT("Removing stale platform handle pointer: PlatformConfigName=%s InstanceName=%s"), *PlatformConfigName, *InstanceName.ToString());
			PlatformMap->Remove(InstanceName);
		}
	}
	else
	{
		PlatformMap = &PlatformHandles.Emplace(PlatformConfigName);
	}

	const FTCHARToUTF8 Utf8ProductId(*PlatformConfig->ProductId);
	const FTCHARToUTF8 Utf8SandboxId(*PlatformConfig->SandboxId);
	const FTCHARToUTF8 Utf8ClientId(*PlatformConfig->ClientId);
	const FTCHARToUTF8 Utf8ClientSecret(*PlatformConfig->ClientSecret);
	const FTCHARToUTF8 Utf8EncryptionKey(*PlatformConfig->EncryptionKey);
	const FTCHARToUTF8 Utf8OverrideCountryCode(*PlatformConfig->OverrideCountryCode);
	const FTCHARToUTF8 Utf8OverrideLocaleCode(*PlatformConfig->OverrideLocaleCode);
	const FTCHARToUTF8 Utf8DeploymentId(*PlatformConfig->DeploymentId);
	const FTCHARToUTF8 Utf8CacheDirectory(*PlatformConfig->CacheDirectory);

	EOS_Platform_Options PlatformOptions = {};
	PlatformOptions.ApiVersion = 13;
	UE_EOS_CHECK_API_MISMATCH(EOS_PLATFORM_OPTIONS_API_LATEST, 14);
	PlatformOptions.Reserved = nullptr;
	ApplySystemSpecificOptions(PlatformOptions.SystemSpecificOptions);
	ApplyIntegratedPlatformOptions(PlatformOptions.IntegratedPlatformOptionsContainerHandle);
	PlatformOptions.ProductId = Utf8ProductId.Length() ? Utf8ProductId.Get() : nullptr;
	PlatformOptions.SandboxId = Utf8SandboxId.Length() ? Utf8SandboxId.Get() : nullptr;
	PlatformOptions.ClientCredentials.ClientId = Utf8ClientId.Length() ? Utf8ClientId.Get() : nullptr;
	PlatformOptions.ClientCredentials.ClientSecret = Utf8ClientSecret.Length() ? Utf8ClientSecret.Get() : nullptr;
	PlatformOptions.bIsServer = PlatformConfig->bIsServer;
	PlatformOptions.EncryptionKey = Utf8EncryptionKey.Length() ? Utf8EncryptionKey.Get() : nullptr;
	PlatformOptions.OverrideCountryCode = Utf8OverrideCountryCode.Length() ? Utf8OverrideCountryCode.Get() : nullptr;
	PlatformOptions.OverrideLocaleCode = Utf8OverrideLocaleCode.Length() ? Utf8OverrideLocaleCode.Get() : nullptr;
	PlatformOptions.DeploymentId = Utf8DeploymentId.Length() ? Utf8DeploymentId.Get() : nullptr;

	PlatformOptions.Flags = 0;
	if (PlatformConfig->bLoadingInEditor) PlatformOptions.Flags |= EOS_PF_LOADING_IN_EDITOR;
	if (PlatformConfig->bDisableOverlay) PlatformOptions.Flags |= EOS_PF_DISABLE_OVERLAY;
	if (PlatformConfig->bDisableSocialOverlay) PlatformOptions.Flags |= EOS_PF_DISABLE_SOCIAL_OVERLAY;

	if (FPlatformMisc::IsCacheStorageAvailable())
	{
		PlatformOptions.CacheDirectory = Utf8CacheDirectory.Length() ? Utf8CacheDirectory.Get() : nullptr;
	}
	else
	{
		PlatformOptions.CacheDirectory = nullptr;
	}

	PlatformOptions.TickBudgetInMilliseconds = PlatformConfig->TickBudgetInMilliseconds;
	PlatformOptions.TaskNetworkTimeoutSeconds = nullptr;

	EOS_Platform_RTCOptions PlatformRTCOptions = {};
	PlatformRTCOptions.ApiVersion = 2;
	UE_EOS_CHECK_API_MISMATCH(EOS_PLATFORM_RTCOPTIONS_API_LATEST, 2);
	PlatformRTCOptions.PlatformSpecificOptions = nullptr;
	PlatformRTCOptions.BackgroundMode = PlatformConfig->RTCBackgroundMode;

	PlatformOptions.RTCOptions = PlatformConfig->bEnableRTC ? &PlatformRTCOptions : nullptr;

	IEOSPlatformHandlePtr PlatformHandle = CreatePlatform(*PlatformConfig, PlatformOptions);
	if (PlatformHandle.IsValid())
	{
		UE_LOG(LogEOSSDK, Verbose, TEXT("Created platform handle: PlatformConfigName=%s InstanceName=%s"), *PlatformConfigName, *InstanceName.ToString());
		PlatformMap->Emplace(InstanceName, PlatformHandle);
	}

	return PlatformHandle;
}

IEOSPlatformHandlePtr FEOSSDKManager::CreatePlatform(const FEOSSDKPlatformConfig& PlatformConfig, EOS_Platform_Options& PlatformOptions)
{
	OnPreCreateNamedPlatform.Broadcast(PlatformConfig, PlatformOptions);
	IEOSPlatformHandlePtr Result = CreatePlatform(PlatformOptions);
	if (Result)
	{
		static_cast<FEOSPlatformHandle&>(*Result.Get()).ConfigName = PlatformConfig.Name;
	}
	return Result;
}

IEOSPlatformHandlePtr FEOSSDKManager::CreatePlatform(EOS_Platform_Options& PlatformOptions)
{
	check(IsInGameThread());

	IEOSPlatformHandlePtr SharedPlatform;

	if (IsInitialized())
	{
		ApplySystemSpecificOptions(PlatformOptions.SystemSpecificOptions);
		ApplyIntegratedPlatformOptions(PlatformOptions.IntegratedPlatformOptionsContainerHandle);

		OnPreCreatePlatform.Broadcast(PlatformOptions);

		const EOS_HPlatform PlatformHandle = EOS_Platform_Create(&PlatformOptions);
		if (PlatformHandle)
		{
			EOS_IntegratedPlatformOptionsContainer_Release(PlatformOptions.IntegratedPlatformOptionsContainerHandle);

			SharedPlatform = MakeShared<FEOSPlatformHandle, ESPMode::ThreadSafe>(*this, PlatformHandle);
			{
				FRWScopeLock ScopeLock(ActivePlatformsCS, SLT_Write);
				ActivePlatforms.Emplace(PlatformHandle, SharedPlatform);
			}

			SetupTicker();

			EOS_Platform_SetApplicationStatus(PlatformHandle, CachedApplicationStatus);
			EOS_Platform_SetNetworkStatus(PlatformHandle, ConvertNetworkStatus(FPlatformMisc::GetNetworkConnectionStatus()));
			
			SetInvokeOverlayButton(PlatformHandle);

			// Tick the platform once to work around EOSSDK error logging that occurs if you create then immediately destroy a platform.
			SharedPlatform->Tick();
		}
		else
		{
			UE_LOG(LogEOSSDK, Warning, TEXT("FEOSSDKManager::CreatePlatform failed. EosPlatformHandle=nullptr"));
		}
	}
	else
	{
		UE_LOG(LogEOSSDK, Warning, TEXT("FEOSSDKManager::CreatePlatform failed. SDK not initialized"));
	}

	return SharedPlatform;
}

TArray<IEOSPlatformHandlePtr> FEOSSDKManager::GetActivePlatforms()
{
	FRWScopeLock ScopeLock(ActivePlatformsCS, SLT_ReadOnly);

	TArray<IEOSPlatformHandlePtr> Result;

	for (const TPair<EOS_HPlatform, IEOSPlatformHandleWeakPtr>& Entry : ActivePlatforms)
	{
		if (!ReleasedPlatforms.Contains(Entry.Key))
		{
			if (IEOSPlatformHandlePtr SharedPtr = Entry.Value.Pin())
			{
				Result.Add(SharedPtr);
			}
		}
	}

	return Result;
}

void FEOSSDKManager::OnConfigSectionsChanged(const FString& IniFilename, const TSet<FString>& SectionNames)
{
	if (IniFilename == GEngineIni && SectionNames.Contains(TEXT("EOSSDK")))
	{
		LoadConfig();
	}
}

void FEOSSDKManager::LoadConfig()
{
	const TCHAR* SectionName = TEXT("EOSSDK");

	ConfigTickIntervalSeconds = 0.f;
	GConfig->GetDouble(SectionName, TEXT("TickIntervalSeconds"), ConfigTickIntervalSeconds, GEngineIni);

	bEnablePlatformIntegration = false;
	GConfig->GetBool(SectionName, TEXT("bEnablePlatformIntegration"), bEnablePlatformIntegration, GEngineIni);

	InvokeOverlayButtonCombination = EOS_UI_EInputStateButtonFlags::EOS_UISBF_Special_Left;
	FString ButtonCombinationStr;
	GConfig->GetString(SectionName, TEXT("InvokeOverlayButtonCombination"), ButtonCombinationStr, GEngineIni);
	if (!ButtonCombinationStr.IsEmpty())
	{
		EOS_UI_EInputStateButtonFlags ButtonCombination;
		if (LexFromString(ButtonCombination, *ButtonCombinationStr))
		{
			InvokeOverlayButtonCombination = ButtonCombination;
		}
	}

	TArray<FString> ManagementFlags;
	if (GConfig->GetArray(SectionName, TEXT("IntegratedPlatformManagementFlags"), ManagementFlags, GEngineIni))
	{
		IntegratedPlatformManagementFlags = {};
		for (const FString& ManagementFlagStr : ManagementFlags)
		{
			EOS_EIntegratedPlatformManagementFlags NewManagementFlag = {};
			if (!LexFromString(NewManagementFlag, *ManagementFlagStr))
			{
				UE_LOG(LogEOSSDK, Verbose, TEXT("[%hs] Unable to parse a valid EOS_EIntegratedPlatformManagementFlags value from string: %s"), __FUNCTION__, *ManagementFlagStr);
			}
			
			IntegratedPlatformManagementFlags |= NewManagementFlag;
		}
	}

	SetupTicker();
}

void FEOSSDKManager::SetupTicker()
{
	check(IsInGameThread());

	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	int NumActivePlatforms = ActivePlatforms.Num();
	if (NumActivePlatforms > 0)
	{
		const double TickIntervalSeconds = ConfigTickIntervalSeconds > SMALL_NUMBER ? ConfigTickIntervalSeconds / NumActivePlatforms : 0.f;
		TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FEOSSDKManager::Tick), TickIntervalSeconds);
	}
}

#if WITH_ENGINE
void FEOSSDKManager::OnBackBufferReady_RenderThread(SWindow& SlateWindow, const FTexture2DRHIRef& InBackBuffer)
{
	UE_CALL_ONCE([]() {	UE_LOG(LogEOSSDK, VeryVerbose, TEXT("[%hs] The method is not implemented for this platform."), __FUNCTION__) });
}
#endif

void FEOSSDKManager::CallUIPrePresent(const EOS_UI_PrePresentOptions& Options)
{
	// This call only returns valid platforms, so we can skip validity checks
	static TMap<EOS_HUI, EOS_EResult> LastResults;
	TArray<IEOSPlatformHandlePtr> ActivePlatformsChecked = GetActivePlatforms();
	for (const IEOSPlatformHandlePtr& ActivePlatform : ActivePlatformsChecked)
	{
		if (EOS_HUI UIHandle = EOS_Platform_GetUIInterface(*ActivePlatform))
		{
			EOS_EResult Result = EOS_UI_PrePresent(UIHandle, &Options);
			EOS_EResult& LastResult = LastResults.FindOrAdd(UIHandle);
			if (LastResult != Result)
			{
				LastResult = Result;
				if (Result == EOS_EResult::EOS_Success)
				{
					UE_LOG(LogEOSSDK, Verbose, TEXT("[%hs] EOS_UI_PrePresent is succeeding again."), __FUNCTION__);
				}
				else
				{
					UE_LOG(LogEOSSDK, Verbose, TEXT("[%hs] EOS_UI_PrePresent failed with error: %s"), __FUNCTION__, *LexToString(Result));
				}
			}
		}
	}
}

#if WITH_ENGINE
bool FEOSSDKManager::IsRenderReady()
{
	if (bEnablePlatformIntegration)
	{
		if (bRenderReady)
		{
			return true;
		}

		if (!FSlateApplication::IsInitialized())
		{
			return false;
		}

		FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer();
		if (!Renderer)
		{
			return false;
		}

		Renderer->OnBackBufferReadyToPresent().AddRaw(this, &FEOSSDKManager::OnBackBufferReady_RenderThread);
		bRenderReady = true;
		return true;
	}
	else
	{
		return false;
	}
}
#endif

void FEOSSDKManager::SetInvokeOverlayButton(const EOS_HPlatform PlatformHandle)
{
	if (bEnablePlatformIntegration)
	{
		if (EOS_HUI UIHandle = EOS_Platform_GetUIInterface(PlatformHandle))
		{
			EOS_UI_SetToggleFriendsButtonOptions Options = { };
			Options.ApiVersion = 1;
			UE_EOS_CHECK_API_MISMATCH(EOS_UI_SETTOGGLEFRIENDSBUTTON_API_LATEST, 1);
			Options.ButtonCombination = InvokeOverlayButtonCombination;

			const EOS_EResult Result = EOS_UI_SetToggleFriendsButton(UIHandle, &Options);
			if (Result != EOS_EResult::EOS_Success)
			{
				UE_LOG(LogEOSSDK, Verbose, TEXT("[%hs] EOS_UI_SetToggleFriendsButton failed with error: %s"), __FUNCTION__, *LexToString(Result));
			}
		}
	}
}

bool FEOSSDKManager::Tick(float)
{
	check(IsInGameThread());

#if WITH_ENGINE
	IsRenderReady();
#endif

	ReleaseReleasedPlatforms();

	if (ActivePlatforms.Num())
	{
		TArray<EOS_HPlatform> PlatformsToTick;

		TArray<EOS_HPlatform> ActivePlatformHandles;
		ActivePlatforms.GenerateKeyArray(ActivePlatformHandles);

		if (ConfigTickIntervalSeconds > SMALL_NUMBER)
		{
			PlatformTickIdx = (PlatformTickIdx + 1) % ActivePlatformHandles.Num();
			PlatformsToTick.Emplace(ActivePlatformHandles[PlatformTickIdx]);	
		}
		else
		{
			PlatformsToTick = ActivePlatformHandles;
		}

		for (EOS_HPlatform PlatformHandle : PlatformsToTick)
		{
			LLM_SCOPE(ELLMTag::RealTimeCommunications); // TODO should really be ELLMTag::EOSSDK
			QUICK_SCOPE_CYCLE_COUNTER(FEOSSDKManager_Tick);
			CSV_SCOPED_TIMING_STAT_EXCLUSIVE(EOSSDK);

			EOS_Platform_Tick(PlatformHandle);
		}
	}

	return true;
}

EOS_ENetworkStatus FEOSSDKManager::ConvertNetworkStatus(ENetworkConnectionStatus Status)
{
	switch (Status)
	{
	case ENetworkConnectionStatus::Unknown:			return EOS_ENetworkStatus::EOS_NS_Online;
	case ENetworkConnectionStatus::Disabled:		return EOS_ENetworkStatus::EOS_NS_Disabled;
	case ENetworkConnectionStatus::Local:			return EOS_ENetworkStatus::EOS_NS_Offline;
	case ENetworkConnectionStatus::Connected:		return EOS_ENetworkStatus::EOS_NS_Online;
	}

	checkNoEntry();
	return EOS_ENetworkStatus::EOS_NS_Disabled;
}

void FEOSSDKManager::OnNetworkConnectionStatusChanged(ENetworkConnectionStatus LastConnectionState, ENetworkConnectionStatus ConnectionState)
{
	check(IsInGameThread());

	const EOS_ENetworkStatus OldNetworkStatus = ConvertNetworkStatus(LastConnectionState);
	const EOS_ENetworkStatus NewNetworkStatus = ConvertNetworkStatus(ConnectionState);

	UE_LOG(LogEOSSDK, Log, TEXT("OnNetworkConnectionStatusChanged [%s] -> [%s]"), LexToString(OldNetworkStatus), LexToString(NewNetworkStatus));

	for (const TPair<EOS_HPlatform, IEOSPlatformHandleWeakPtr>& Entry : ActivePlatforms)
	{
		EOS_Platform_SetNetworkStatus(Entry.Key, NewNetworkStatus);
	}
}

void FEOSSDKManager::OnApplicationStatusChanged(EOS_EApplicationStatus ApplicationStatus)
{
	check(IsInGameThread());

	UE_LOG(LogEOSSDK, Log, TEXT("OnApplicationStatusChanged [%s] -> [%s]"), LexToString(CachedApplicationStatus), LexToString(ApplicationStatus));
	CachedApplicationStatus = ApplicationStatus;
	for (const TPair<EOS_HPlatform, IEOSPlatformHandleWeakPtr>& Entry : ActivePlatforms)
	{
		EOS_Platform_SetApplicationStatus(Entry.Key, ApplicationStatus);
	}
}

void FEOSSDKManager::OnLogVerbosityChanged(const FLogCategoryName& CategoryName, ELogVerbosity::Type OldVerbosity, ELogVerbosity::Type NewVerbosity)
{
#if !NO_LOGGING
	if (IsInitialized() &&
		CategoryName == LogEOSSDK.GetCategoryName())
	{
		const EOS_EResult EosResult = EOS_Logging_SetLogLevel(EOS_ELogCategory::EOS_LC_ALL_CATEGORIES, ConvertLogLevel(NewVerbosity));
		if (EosResult != EOS_EResult::EOS_Success)
		{
			UE_LOG(LogEOSSDK, Warning, TEXT("EOS_Logging_SetLogLevel failed Verbosity=%s error=[%s]"), ToString(NewVerbosity), *LexToString(EosResult));
		}
	}
#endif // !NO_LOGGING
}

FString FEOSSDKManager::GetProductName() const
{
	FString ProductName;
	if (!GConfig->GetString(TEXT("EOSSDK"), TEXT("ProductName"), ProductName, GEngineIni))
	{
		ProductName = FApp::GetProjectName();
	}
	return ProductName;
}

FString FEOSSDKManager::GetProductVersion() const
{
	return FApp::GetBuildVersion();
}

FString FEOSSDKManager::GetCacheDirBase() const
{
	return FPlatformProcess::UserDir();
}

FString FEOSSDKManager::GetOverrideCountryCode(const EOS_HPlatform Platform) const
{
	FString Result;

	char CountryCode[EOS_COUNTRYCODE_MAX_LENGTH + 1];
	int32_t CountryCodeLength = sizeof(CountryCode);
	if (EOS_Platform_GetOverrideCountryCode(Platform, CountryCode, &CountryCodeLength) == EOS_EResult::EOS_Success)
	{
		Result = UTF8_TO_TCHAR(CountryCode);
	}

	return Result;
}

FString FEOSSDKManager::GetOverrideLocaleCode(const EOS_HPlatform Platform) const
{
	FString Result;

	char LocaleCode[EOS_LOCALECODE_MAX_LENGTH + 1];
	int32_t LocaleCodeLength = sizeof(LocaleCode);
	if (EOS_Platform_GetOverrideLocaleCode(Platform, LocaleCode, &LocaleCodeLength) == EOS_EResult::EOS_Success)
	{
		Result = UTF8_TO_TCHAR(LocaleCode);
	}

	return Result;
}

void FEOSSDKManager::ReleasePlatform(EOS_HPlatform PlatformHandle)
{
	check(IsInGameThread());

	if(ActivePlatforms.Contains(PlatformHandle) && !ReleasedPlatforms.Contains(PlatformHandle))
	{
		FRWScopeLock ScopeLock(ActivePlatformsCS, SLT_Write);

		ReleasedPlatforms.Emplace(PlatformHandle);
	}
}

void FEOSSDKManager::ReleaseReleasedPlatforms()
{
	check(IsInGameThread());

	if (ReleasedPlatforms.Num() > 0)
	{
		{
			FRWScopeLock ScopeLock(ActivePlatformsCS, SLT_Write);

			for (EOS_HPlatform PlatformHandle : ReleasedPlatforms)
			{
				if (ensure(ActivePlatforms.Contains(PlatformHandle)))
				{
					EOS_Platform_Release(PlatformHandle);

					ActivePlatforms.Remove(PlatformHandle);
				}
			}

			ReleasedPlatforms.Empty();
		}

		SetupTicker();
	}
}

void FEOSSDKManager::Shutdown()
{
	check(IsInGameThread());

	if (IsInitialized())
	{
		// Release already released platforms
		ReleaseReleasedPlatforms();

		if (ActivePlatforms.Num() > 0)
		{
			FRWScopeLock ScopeLock(ActivePlatformsCS, SLT_Write);

			UE_LOG(LogEOSSDK, Warning, TEXT("FEOSSDKManager::Shutdown Releasing %d remaining platforms"), ActivePlatforms.Num());

			TArray<EOS_HPlatform> ActivePlatformHandles;
			ActivePlatforms.GenerateKeyArray(ActivePlatformHandles);
			ReleasedPlatforms.Append(ActivePlatformHandles);
			ReleaseReleasedPlatforms();
		}

		FCoreDelegates::TSOnConfigSectionsChanged().RemoveAll(this);

#if !NO_LOGGING
		FCoreDelegates::OnLogVerbosityChanged.RemoveAll(this);
#endif // !NO_LOGGING

		const EOS_EResult Result = EOS_Shutdown();
		UE_LOG(LogEOSSDK, Log, TEXT("FEOSSDKManager::Shutdown EOS_Shutdown Result=[%s]"), *LexToString(Result));

		CallbackObjects.Empty();

		bInitialized = false;

		FCoreDelegates::OnNetworkConnectionStatusChanged.RemoveAll(this);

#if WITH_ENGINE
		// We can't check bRenderReady at this point as Slate might have shut down already
		if (FSlateApplication::IsInitialized())
		{
			FSlateRenderer* Renderer = FSlateApplication::Get().GetRenderer();
			if (Renderer)
			{
				Renderer->OnBackBufferReadyToPresent().RemoveAll(this);
			}
		}
#endif
	}
}

EOS_EResult FEOSSDKManager::EOSInitialize(EOS_InitializeOptions& Options)
{
	OnPreInitializeSDK.Broadcast(Options);

	return EOS_Initialize(&Options);
}

bool FEOSSDKManager::Exec_Runtime(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (!FParse::Command(&Cmd, TEXT("EOSSDK")))
	{
		return false;
	}

	if (FParse::Command(&Cmd, TEXT("INFO")))
	{
		LogInfo();
	}
	else
	{
		UE_LOG(LogEOSSDK, Warning, TEXT("Unknown exec command: %s]"), Cmd);
	}

	return true;
}

#define UE_LOG_EOSSDK_INFO(Format, ...) UE_LOG(LogEOSSDK, Log, TEXT("Info: %*s" Format), Indent * 2, TEXT(""), ##__VA_ARGS__)

void FEOSSDKManager::LogInfo(int32 Indent) const
{
	check(IsInGameThread());

	UE_LOG_EOSSDK_INFO("ProductName=%s", *GetProductName());
	UE_LOG_EOSSDK_INFO("ProductVersion=%s", *GetProductVersion());
	UE_LOG_EOSSDK_INFO("CacheDirBase=%s", *GetCacheDirBase());
	UE_LOG_EOSSDK_INFO("Platforms=%d", ActivePlatforms.Num());

	TArray<EOS_HPlatform> ActivePlatformHandles;
	ActivePlatforms.GenerateKeyArray(ActivePlatformHandles);
	for (int32 PlatformIndex = 0; PlatformIndex < ActivePlatformHandles.Num(); PlatformIndex++)
	{
		const EOS_HPlatform Platform = ActivePlatformHandles[PlatformIndex];
		UE_LOG_EOSSDK_INFO("Platform=%d", PlatformIndex);
		Indent++;
		LogPlatformInfo(Platform, Indent);
		Indent--;
	}
}

void FEOSSDKManager::LogPlatformInfo(const EOS_HPlatform Platform, int32 Indent) const
{
	UE_LOG_EOSSDK_INFO("ApplicationStatus=%s", LexToString(EOS_Platform_GetApplicationStatus(Platform)));
	UE_LOG_EOSSDK_INFO("NetworkStatus=%s", LexToString(EOS_Platform_GetNetworkStatus(Platform)));
	UE_LOG_EOSSDK_INFO("OverrideCountryCode=%s", *GetOverrideCountryCode(Platform));
	UE_LOG_EOSSDK_INFO("OverrideLocaleCode=%s", *GetOverrideLocaleCode(Platform));

	EOS_Platform_GetDesktopCrossplayStatusOptions GetDesktopCrossplayStatusOptions;
	GetDesktopCrossplayStatusOptions.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_PLATFORM_GETDESKTOPCROSSPLAYSTATUS_API_LATEST, 1);

	EOS_Platform_GetDesktopCrossplayStatusInfo GetDesktopCrossplayStatusInfo;
	EOS_EResult Result = EOS_Platform_GetDesktopCrossplayStatus(Platform, &GetDesktopCrossplayStatusOptions, &GetDesktopCrossplayStatusInfo);
	if (Result == EOS_EResult::EOS_Success)
	{
		UE_LOG_EOSSDK_INFO("DesktopCrossplayStatusInfo Status=%s ServiceInitResult=%d",
			LexToString(GetDesktopCrossplayStatusInfo.Status),
			GetDesktopCrossplayStatusInfo.ServiceInitResult);
	}
	else
	{
		UE_LOG_EOSSDK_INFO("DesktopCrossplayStatusInfo (EOS_Platform_GetDesktopCrossplayStatus failed: %s)", *LexToString(Result));
	}

	const EOS_HAuth AuthHandle = EOS_Platform_GetAuthInterface(Platform);
	const int32_t AuthLoggedInAccountsCount = EOS_Auth_GetLoggedInAccountsCount(AuthHandle);
	UE_LOG_EOSSDK_INFO("AuthLoggedInAccounts=%d", AuthLoggedInAccountsCount);

	for (int32_t AuthLoggedInAccountIndex = 0; AuthLoggedInAccountIndex < AuthLoggedInAccountsCount; AuthLoggedInAccountIndex++)
	{
		const EOS_EpicAccountId LoggedInAccount = EOS_Auth_GetLoggedInAccountByIndex(AuthHandle, AuthLoggedInAccountIndex);
		UE_LOG_EOSSDK_INFO("AuthLoggedInAccount=%d", AuthLoggedInAccountIndex);
		Indent++;
		LogUserInfo(Platform, LoggedInAccount, LoggedInAccount, Indent);
		LogAuthInfo(Platform, LoggedInAccount, Indent);
		LogPresenceInfo(Platform, LoggedInAccount, LoggedInAccount, Indent);
		LogFriendsInfo(Platform, LoggedInAccount, Indent);
		Indent--;
	}

	const EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(Platform);
	const int32_t ConnectLoggedInAccountsCount = EOS_Connect_GetLoggedInUsersCount(ConnectHandle);
	UE_LOG_EOSSDK_INFO("ConnectLoggedInAccounts=%d", ConnectLoggedInAccountsCount);

	for (int32_t ConnectLoggedInAccountIndex = 0; ConnectLoggedInAccountIndex < ConnectLoggedInAccountsCount; ConnectLoggedInAccountIndex++)
	{
		const EOS_ProductUserId LoggedInAccount = EOS_Connect_GetLoggedInUserByIndex(ConnectHandle, ConnectLoggedInAccountIndex);
		UE_LOG_EOSSDK_INFO("ConnectLoggedInAccount=%d", ConnectLoggedInAccountIndex);
		Indent++;
		LogConnectInfo(Platform, LoggedInAccount, Indent);
		Indent--;
	}
}

void FEOSSDKManager::LogAuthInfo(const EOS_HPlatform Platform, const EOS_EpicAccountId LoggedInAccount, int32 Indent) const
{
	const EOS_HAuth AuthHandle = EOS_Platform_GetAuthInterface(Platform);
	UE_LOG_EOSSDK_INFO("LoginStatus=%s", LexToString(EOS_Auth_GetLoginStatus(AuthHandle, LoggedInAccount)));

	EOS_Auth_CopyUserAuthTokenOptions CopyUserAuthTokenOptions;
	CopyUserAuthTokenOptions.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_COPYUSERAUTHTOKEN_API_LATEST, 1);

	EOS_Auth_Token* AuthToken;
	EOS_EResult Result = EOS_Auth_CopyUserAuthToken(AuthHandle, &CopyUserAuthTokenOptions, LoggedInAccount, &AuthToken);
	if (Result == EOS_EResult::EOS_Success)
	{
		UE_LOG_EOSSDK_INFO("AuthToken");
		Indent++;
		UE_LOG_EOSSDK_INFO("App=%s", UTF8_TO_TCHAR(AuthToken->App));
		UE_LOG_EOSSDK_INFO("ClientId=%s", UTF8_TO_TCHAR(AuthToken->ClientId));
#if !UE_BUILD_SHIPPING
		UE_LOG_EOSSDK_INFO("AccessToken=%s", UTF8_TO_TCHAR(AuthToken->AccessToken));
#endif // !UE_BUILD_SHIPPING
		UE_LOG_EOSSDK_INFO("ExpiresIn=%f", AuthToken->ExpiresIn);
		UE_LOG_EOSSDK_INFO("ExpiresAt=%s", UTF8_TO_TCHAR(AuthToken->ExpiresAt));
		UE_LOG_EOSSDK_INFO("AuthType=%s", LexToString(AuthToken->AuthType));
#if !UE_BUILD_SHIPPING
		UE_LOG_EOSSDK_INFO("RefreshToken=%s", UTF8_TO_TCHAR(AuthToken->RefreshToken));
#endif // !UE_BUILD_SHIPPING
		UE_LOG_EOSSDK_INFO("RefreshExpiresIn=%f", AuthToken->RefreshExpiresIn);
		UE_LOG_EOSSDK_INFO("RefreshExpiresAt=%s", UTF8_TO_TCHAR(AuthToken->RefreshExpiresAt));
		Indent--;
		EOS_Auth_Token_Release(AuthToken);
	}
	else
	{
		UE_LOG_EOSSDK_INFO("AuthToken (EOS_Auth_CopyUserAuthToken failed: %s)", *LexToString(Result));
	}

#if !UE_BUILD_SHIPPING
	EOS_Auth_CopyIdTokenOptions CopyIdTokenOptions;
	CopyIdTokenOptions.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_AUTH_COPYIDTOKEN_API_LATEST, 1);
	CopyIdTokenOptions.AccountId = LoggedInAccount;

	EOS_Auth_IdToken* IdToken;
	Result = EOS_Auth_CopyIdToken(AuthHandle, &CopyIdTokenOptions, &IdToken);
	if (Result == EOS_EResult::EOS_Success)
	{
		UE_LOG_EOSSDK_INFO("IdToken=%s", UTF8_TO_TCHAR(IdToken->JsonWebToken));
		EOS_Auth_IdToken_Release(IdToken);
	}
	else
	{
		UE_LOG_EOSSDK_INFO("IdToken (EOS_Auth_CopyIdToken failed: %s)", *LexToString(Result));
	}
#endif // !UE_BUILD_SHIPPING
}

void FEOSSDKManager::LogUserInfo(const EOS_HPlatform Platform, const EOS_EpicAccountId LoggedInAccount, const EOS_EpicAccountId TargetAccount, int32 Indent) const
{
	UE_LOG_EOSSDK_INFO("EpicAccountId=%s", *LexToString(TargetAccount));

	EOS_UserInfo_CopyUserInfoOptions CopyUserInfoOptions;
	CopyUserInfoOptions.ApiVersion = 3;
	UE_EOS_CHECK_API_MISMATCH(EOS_USERINFO_COPYUSERINFO_API_LATEST, 3);
	CopyUserInfoOptions.LocalUserId = LoggedInAccount;
	CopyUserInfoOptions.TargetUserId = TargetAccount;

	const EOS_HUserInfo UserInfoHandle = EOS_Platform_GetUserInfoInterface(Platform);
	EOS_UserInfo* UserInfo;
	EOS_EResult Result = EOS_UserInfo_CopyUserInfo(UserInfoHandle, &CopyUserInfoOptions, &UserInfo);
	if (Result == EOS_EResult::EOS_Success)
	{
		UE_LOG_EOSSDK_INFO("UserInfo");
		Indent++;
		UE_LOG_EOSSDK_INFO("Country=%s", UTF8_TO_TCHAR(UserInfo->Country));
		UE_LOG_EOSSDK_INFO("DisplayName=%s", UTF8_TO_TCHAR(UserInfo->DisplayName));
		UE_LOG_EOSSDK_INFO("PreferredLanguage=%s", UTF8_TO_TCHAR(UserInfo->PreferredLanguage));
		UE_LOG_EOSSDK_INFO("Nickname=%s", UTF8_TO_TCHAR(UserInfo->Nickname));
		UE_LOG_EOSSDK_INFO("DisplayNameSanitized=%s", UTF8_TO_TCHAR(UserInfo->DisplayNameSanitized));
		Indent--;

		EOS_UserInfo_Release(UserInfo);
	}
	else
	{
		UE_LOG_EOSSDK_INFO("UserInfo (EOS_UserInfo_CopyUserInfo failed: %s)", *LexToString(Result));
	}

	EOS_UserInfo_CopyBestDisplayNameOptions Options = {};
	Options.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_USERINFO_COPYBESTDISPLAYNAME_API_LATEST, 1);
	Options.LocalUserId = LoggedInAccount;
	Options.TargetUserId = TargetAccount;

	EOS_UserInfo_BestDisplayName* BestDisplayName;
	Result = EOS_UserInfo_CopyBestDisplayName(UserInfoHandle, &Options, &BestDisplayName);

	if (Result == EOS_EResult::EOS_UserInfo_BestDisplayNameIndeterminate)
	{
		EOS_UserInfo_CopyBestDisplayNameWithPlatformOptions WithPlatformOptions = {};
		WithPlatformOptions.ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_USERINFO_COPYBESTDISPLAYNAMEWITHPLATFORM_API_LATEST, 1);
		WithPlatformOptions.LocalUserId = LoggedInAccount;
		WithPlatformOptions.TargetUserId = TargetAccount;
		WithPlatformOptions.TargetPlatformType = EOS_OPT_Epic;

		Result = EOS_UserInfo_CopyBestDisplayNameWithPlatform(UserInfoHandle, &WithPlatformOptions, &BestDisplayName);
	}

	if (Result == EOS_EResult::EOS_Success)
	{
		Indent++;
		UE_LOG_EOSSDK_INFO("BestDisplayName");
		Indent++;
		UE_LOG_EOSSDK_INFO("DisplayName=%hs", BestDisplayName->DisplayName);
		UE_LOG_EOSSDK_INFO("DisplayNameSanitized=%hs", BestDisplayName->DisplayNameSanitized);
		UE_LOG_EOSSDK_INFO("Nickname=%hs", BestDisplayName->Nickname);
		Indent -= 2;

		EOS_UserInfo_BestDisplayName_Release(BestDisplayName);
	}
	else
	{
		UE_LOG_EOSSDK_INFO("UserInfo (BestDisplayName retrieval failed: %s)", *LexToString(Result));
	}
}

void FEOSSDKManager::LogPresenceInfo(const EOS_HPlatform Platform, const EOS_EpicAccountId LoggedInAccount, const EOS_EpicAccountId TargetAccount, int32 Indent) const
{
	EOS_Presence_HasPresenceOptions HasPresenceOptions;
	HasPresenceOptions.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_PRESENCE_HASPRESENCE_API_LATEST, 1);
	HasPresenceOptions.LocalUserId = LoggedInAccount;
	HasPresenceOptions.TargetUserId = TargetAccount;

	const EOS_HPresence PresenceHandle = EOS_Platform_GetPresenceInterface(Platform);
	if (!EOS_Presence_HasPresence(PresenceHandle, &HasPresenceOptions))
	{
		UE_LOG_EOSSDK_INFO("Presence (None)");
		return;
	}

	EOS_Presence_CopyPresenceOptions CopyPresenceOptions;
	CopyPresenceOptions.ApiVersion = 3;
	UE_EOS_CHECK_API_MISMATCH(EOS_PRESENCE_COPYPRESENCE_API_LATEST, 3);
	CopyPresenceOptions.LocalUserId = LoggedInAccount;
	CopyPresenceOptions.TargetUserId = TargetAccount;

	EOS_Presence_Info* PresenceInfo;
	EOS_EResult Result = EOS_Presence_CopyPresence(PresenceHandle, &CopyPresenceOptions, &PresenceInfo);
	if (Result == EOS_EResult::EOS_Success)
	{
		UE_LOG_EOSSDK_INFO("Presence");
		Indent++;
		UE_LOG_EOSSDK_INFO("Status=%s", LexToString(PresenceInfo->Status));
		UE_LOG_EOSSDK_INFO("ProductId=%s", UTF8_TO_TCHAR(PresenceInfo->ProductId));
		UE_LOG_EOSSDK_INFO("ProductName=%s", UTF8_TO_TCHAR(PresenceInfo->ProductName));
		UE_LOG_EOSSDK_INFO("ProductVersion=%s", UTF8_TO_TCHAR(PresenceInfo->ProductVersion));
		UE_LOG_EOSSDK_INFO("Platform=%s", UTF8_TO_TCHAR(PresenceInfo->Platform));
		UE_LOG_EOSSDK_INFO("IntegratedPlatform=%s", UTF8_TO_TCHAR(PresenceInfo->IntegratedPlatform));
		UE_LOG_EOSSDK_INFO("RichText=%s", UTF8_TO_TCHAR(PresenceInfo->RichText));
		UE_LOG_EOSSDK_INFO("RecordsCount=%d", PresenceInfo->RecordsCount);
		Indent++;
		for (int32_t Index = 0; Index < PresenceInfo->RecordsCount; Index++)
		{
			UE_LOG_EOSSDK_INFO("Key=%s Value=%s", UTF8_TO_TCHAR(PresenceInfo->Records[Index].Key), UTF8_TO_TCHAR(PresenceInfo->Records[Index].Value));
		}
		Indent--;
		Indent--;

		EOS_Presence_Info_Release(PresenceInfo);
	}
	else
	{
		UE_LOG_EOSSDK_INFO("Presence (EOS_Presence_CopyPresence failed: %s)", *LexToString(Result));
	}
}

void FEOSSDKManager::LogFriendsInfo(const EOS_HPlatform Platform, const EOS_EpicAccountId LoggedInAccount, int32 Indent) const
{
	EOS_Friends_GetFriendsCountOptions FriendsCountOptions;
	FriendsCountOptions.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_FRIENDS_GETFRIENDSCOUNT_API_LATEST, 1);
	FriendsCountOptions.LocalUserId = LoggedInAccount;

	const EOS_HFriends FriendsHandle = EOS_Platform_GetFriendsInterface(Platform);
	const int32_t FriendsCount = EOS_Friends_GetFriendsCount(FriendsHandle, &FriendsCountOptions);
	UE_LOG_EOSSDK_INFO("Friends=%d", FriendsCount);

	EOS_Friends_GetFriendAtIndexOptions FriendAtIndexOptions;
	FriendAtIndexOptions.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_FRIENDS_GETFRIENDATINDEX_API_LATEST, 1);
	FriendAtIndexOptions.LocalUserId = LoggedInAccount;

	for (int32_t FriendIndex = 0; FriendIndex < FriendsCount; FriendIndex++)
	{
		FriendAtIndexOptions.Index = FriendIndex;
		const EOS_EpicAccountId FriendId = EOS_Friends_GetFriendAtIndex(FriendsHandle, &FriendAtIndexOptions);
		UE_LOG_EOSSDK_INFO("Friend=%d", FriendIndex);
		Indent++;

		EOS_Friends_GetStatusOptions GetStatusOptions;
		GetStatusOptions.ApiVersion = 1;
		UE_EOS_CHECK_API_MISMATCH(EOS_FRIENDS_GETSTATUS_API_LATEST, 1);
		GetStatusOptions.LocalUserId = LoggedInAccount;
		GetStatusOptions.TargetUserId = FriendId;

		const EOS_EFriendsStatus FriendStatus = EOS_Friends_GetStatus(FriendsHandle, &GetStatusOptions);
		UE_LOG_EOSSDK_INFO("FriendStatus=%s", LexToString(FriendStatus));

		LogUserInfo(Platform, LoggedInAccount, FriendId, Indent);
		LogPresenceInfo(Platform, LoggedInAccount, FriendId, Indent);
		Indent--;
	}
}

void FEOSSDKManager::LogConnectInfo(const EOS_HPlatform Platform, const EOS_ProductUserId LoggedInAccount, int32 Indent) const
{
	UE_LOG_EOSSDK_INFO("ProductUserId=%s", *LexToString(LoggedInAccount));

	const EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(Platform);
	UE_LOG_EOSSDK_INFO("LoginStatus=%s", LexToString(EOS_Connect_GetLoginStatus(ConnectHandle, LoggedInAccount)));

	EOS_EResult Result;

#if !UE_BUILD_SHIPPING
	EOS_Connect_CopyIdTokenOptions CopyIdTokenOptions;
	CopyIdTokenOptions.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_COPYIDTOKEN_API_LATEST, 1);
	CopyIdTokenOptions.LocalUserId = LoggedInAccount;

	EOS_Connect_IdToken* IdToken;
	Result = EOS_Connect_CopyIdToken(ConnectHandle, &CopyIdTokenOptions, &IdToken);
	if (Result == EOS_EResult::EOS_Success)
	{
		UE_LOG_EOSSDK_INFO("IdToken=%s", UTF8_TO_TCHAR(IdToken->JsonWebToken));
		EOS_Connect_IdToken_Release(IdToken);
	}
	else
	{
		UE_LOG_EOSSDK_INFO("IdToken (EOS_Connect_CopyIdToken failed: %s)", *LexToString(Result));
	}
#endif // !UE_BUILD_SHIPPING

	EOS_Connect_GetProductUserExternalAccountCountOptions ExternalAccountCountOptions;
	ExternalAccountCountOptions.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_GETPRODUCTUSEREXTERNALACCOUNTCOUNT_API_LATEST, 1);
	ExternalAccountCountOptions.TargetUserId = LoggedInAccount;

	const uint32_t ExternalAccountCount = EOS_Connect_GetProductUserExternalAccountCount(ConnectHandle, &ExternalAccountCountOptions);
	UE_LOG_EOSSDK_INFO("ExternalAccounts=%d", ExternalAccountCount);

	EOS_Connect_CopyProductUserExternalAccountByIndexOptions ExternalAccountByIndexOptions;
	ExternalAccountByIndexOptions.ApiVersion = 1;
	UE_EOS_CHECK_API_MISMATCH(EOS_CONNECT_COPYPRODUCTUSEREXTERNALACCOUNTBYINDEX_API_LATEST, 1);
	ExternalAccountByIndexOptions.TargetUserId = LoggedInAccount;

	for (uint32_t ExternalAccountIndex = 0; ExternalAccountIndex < ExternalAccountCount; ExternalAccountIndex++)
	{
		UE_LOG_EOSSDK_INFO("ExternalAccount=%u", ExternalAccountIndex);
		Indent++;

		ExternalAccountByIndexOptions.ExternalAccountInfoIndex = ExternalAccountIndex;
		EOS_Connect_ExternalAccountInfo* ExternalAccountInfo;
		Result = EOS_Connect_CopyProductUserExternalAccountByIndex(ConnectHandle, &ExternalAccountByIndexOptions, &ExternalAccountInfo);
		if (Result == EOS_EResult::EOS_Success)
		{
			UE_LOG_EOSSDK_INFO("ExternalAccountInfo");
			Indent++;
			UE_LOG_EOSSDK_INFO("DisplayName=%s", UTF8_TO_TCHAR(ExternalAccountInfo->DisplayName));
			UE_LOG_EOSSDK_INFO("AccountId=%s", UTF8_TO_TCHAR(ExternalAccountInfo->AccountId));
			UE_LOG_EOSSDK_INFO("AccountIdType=%s", LexToString(ExternalAccountInfo->AccountIdType));
			UE_LOG_EOSSDK_INFO("LastLoginTime=%lld", ExternalAccountInfo->LastLoginTime);
			Indent--;

			EOS_Connect_ExternalAccountInfo_Release(ExternalAccountInfo);
		}
		else
		{
			UE_LOG_EOSSDK_INFO("ExternalAccountInfo (EOS_Connect_CopyProductUserExternalAccountByIndex failed: %s)", *LexToString(Result));
		}

		Indent--;
	}
}

void FEOSSDKManager::AddCallbackObject(TUniquePtr<FCallbackBase> CallbackObj)
{
	CallbackObjects.Emplace(MoveTemp(CallbackObj));
}

FEOSPlatformHandle::~FEOSPlatformHandle()
{
	Manager.ReleasePlatform(PlatformHandle);
}

void FEOSPlatformHandle::Tick()
{
	LLM_SCOPE(ELLMTag::RealTimeCommunications);
	QUICK_SCOPE_CYCLE_COUNTER(FEOSPlatformHandle_Tick);
	CSV_SCOPED_TIMING_STAT_EXCLUSIVE(EOSSDK);
	EOS_Platform_Tick(PlatformHandle);
}

FString FEOSPlatformHandle::GetConfigName() const
{
	return ConfigName;
}

FString FEOSPlatformHandle::GetOverrideCountryCode() const
{
	return Manager.GetOverrideCountryCode(PlatformHandle);
}

FString FEOSPlatformHandle::GetOverrideLocaleCode() const
{
	return Manager.GetOverrideLocaleCode(PlatformHandle);
}

void FEOSPlatformHandle::LogInfo(int32 Indent) const
{
	Manager.LogPlatformInfo(PlatformHandle, Indent);
}

void FEOSPlatformHandle::LogAuthInfo(const EOS_EpicAccountId LoggedInAccount, int32 Indent) const
{
	Manager.LogAuthInfo(PlatformHandle, LoggedInAccount, Indent);
}

void FEOSPlatformHandle::LogUserInfo(const EOS_EpicAccountId LoggedInAccount, const EOS_EpicAccountId TargetAccount, int32 Indent) const
{
	Manager.LogUserInfo(PlatformHandle, LoggedInAccount, TargetAccount, Indent);
}

void FEOSPlatformHandle::LogPresenceInfo(const EOS_EpicAccountId LoggedInAccount, const EOS_EpicAccountId TargetAccount, int32 Indent) const
{
	Manager.LogPresenceInfo(PlatformHandle, LoggedInAccount, TargetAccount, Indent);
}

void FEOSPlatformHandle::LogFriendsInfo(const EOS_EpicAccountId LoggedInAccount, int32 Indent) const
{
	Manager.LogFriendsInfo(PlatformHandle, LoggedInAccount, Indent);
}

void FEOSPlatformHandle::LogConnectInfo(const EOS_ProductUserId LoggedInAccount, int32 Indent) const
{
	Manager.LogConnectInfo(PlatformHandle, LoggedInAccount, Indent);
}

#endif // WITH_EOS_SDK