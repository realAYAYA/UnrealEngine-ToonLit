// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/OnlineServicesEOSGSPlatformFactory.h"

#include "EOSShared.h"
#include "Online/OnlineExecHandler.h"
#include "Online/OnlineServicesEOSGS.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/Fork.h"
#include "Misc/LazySingleton.h"
#include "Modules/ModuleManager.h"

#include "IEOSSDKManager.h"

#include "Online/OnlineServicesLog.h"
#include <eos_base.h>

struct FEOSPlatformConfig
{
	FString ProductId;
	FString SandboxId;
	FString DeploymentId;
	FString ClientId;
	FString ClientSecret;
	FString EncryptionKey;
};

FEOSPlatformConfig LoadEOSPlatformConfig()
{
	FEOSPlatformConfig PlatformConfig;
	FString ConfigSection = FString::Printf(TEXT("OnlineServices.%s"), UE::Online::FOnlineServicesEOSGS::GetConfigNameStatic());
	GConfig->GetString(*ConfigSection, TEXT("ProductId"), PlatformConfig.ProductId, GEngineIni);
	GConfig->GetString(*ConfigSection, TEXT("SandboxId"), PlatformConfig.SandboxId, GEngineIni);
	GConfig->GetString(*ConfigSection, TEXT("DeploymentId"), PlatformConfig.DeploymentId, GEngineIni);
	GConfig->GetString(*ConfigSection, TEXT("ClientId"), PlatformConfig.ClientId, GEngineIni);
	GConfig->GetString(*ConfigSection, TEXT("ClientSecret"), PlatformConfig.ClientSecret, GEngineIni);
	// Config key renamed to ClientEncryptionKey as EncryptionKey gets removed from packaged builds due to IniKeyDenylist=EncryptionKey entry in BaseGame.ini.
	GConfig->GetString(*ConfigSection, TEXT("ClientEncryptionKey"), PlatformConfig.EncryptionKey, GEngineIni);
	return PlatformConfig;
}

bool IsEOSPlatformConfigValid(const FEOSPlatformConfig& InConfig)
{
	return !InConfig.ProductId.IsEmpty() &&
		!InConfig.SandboxId.IsEmpty() &&
		!InConfig.DeploymentId.IsEmpty() &&
		!InConfig.ClientId.IsEmpty() &&
		!InConfig.ClientSecret.IsEmpty();
}

namespace UE::Online {

FOnlineServicesEOSGSPlatformFactory::FOnlineServicesEOSGSPlatformFactory()
{
	// If a fork is requested, we need to wait for post-fork to create the default platform
	if (!FForkProcessHelper::IsForkRequested() || FForkProcessHelper::IsForkedChildProcess())
	{
		GetDefaultPlatform();
	}
}

FOnlineServicesEOSGSPlatformFactory& FOnlineServicesEOSGSPlatformFactory::Get()
{
	return TLazySingleton<FOnlineServicesEOSGSPlatformFactory>::Get();
}

void FOnlineServicesEOSGSPlatformFactory::TearDown()
{
	return TLazySingleton<FOnlineServicesEOSGSPlatformFactory>::TearDown();
}

IEOSPlatformHandlePtr FOnlineServicesEOSGSPlatformFactory::CreatePlatform()
{
	const FName EOSSharedModuleName = TEXT("EOSShared");
	if (!FModuleManager::Get().IsModuleLoaded(EOSSharedModuleName))
	{
		FModuleManager::Get().LoadModuleChecked(EOSSharedModuleName);
	}
	IEOSSDKManager* const SDKManager = IEOSSDKManager::Get();
	if (!SDKManager)
	{
		UE_LOG(LogOnlineServices, Error, TEXT("[FOnlineServicesEOSGS::Initialize] EOSSDK has not been loaded."));
		return {};
	}

	if (!SDKManager->IsInitialized())
	{
		UE_LOG(LogOnlineServices, Error, TEXT("[FOnlineServicesEOSGS::Initialize] EOSSDK has not been initialized."));
		return {};
	}

	// Load config
	FEOSPlatformConfig EOSPlatformConfig = LoadEOSPlatformConfig();
	if (!IsEOSPlatformConfigValid(EOSPlatformConfig))
	{
		return {};
	}
	const FTCHARToUTF8 ProductId(*EOSPlatformConfig.ProductId);
	const FTCHARToUTF8 SandboxId(*EOSPlatformConfig.SandboxId);
	const FTCHARToUTF8 DeploymentId(*EOSPlatformConfig.DeploymentId);
	const FTCHARToUTF8 ClientId(*EOSPlatformConfig.ClientId);
	const FTCHARToUTF8 ClientSecret(*EOSPlatformConfig.ClientSecret);
	const FTCHARToUTF8 EncryptionKey(EOSPlatformConfig.EncryptionKey.IsEmpty() ? nullptr : *EOSPlatformConfig.EncryptionKey);
	const FTCHARToUTF8 CacheDirectory(*(SDKManager->GetCacheDirBase() / TEXT("OnlineServicesEOS")));

	EOS_Platform_Options PlatformOptions = {};
	PlatformOptions.ApiVersion = 12;
	UE_EOS_CHECK_API_MISMATCH(EOS_PLATFORM_OPTIONS_API_LATEST, 12);
	PlatformOptions.Reserved = nullptr;
	PlatformOptions.bIsServer = IsRunningDedicatedServer() ? EOS_TRUE : EOS_FALSE;
	PlatformOptions.OverrideCountryCode = nullptr;
	PlatformOptions.OverrideLocaleCode = nullptr;
	// Can't check GIsEditor here because it is too soon!
	if (!IsRunningGame())
	{
		PlatformOptions.Flags = EOS_PF_LOADING_IN_EDITOR;
	}
	else
	{
		PlatformOptions.Flags = EOS_PF_WINDOWS_ENABLE_OVERLAY_D3D9 | EOS_PF_WINDOWS_ENABLE_OVERLAY_D3D10 | EOS_PF_WINDOWS_ENABLE_OVERLAY_OPENGL; // Enable overlay support for D3D9/10 and OpenGL. This sample uses D3D11 or SDL.
	}

	PlatformOptions.ProductId = ProductId.Get();
	PlatformOptions.SandboxId = SandboxId.Get();
	PlatformOptions.DeploymentId = DeploymentId.Get();
	PlatformOptions.ClientCredentials.ClientId = ClientId.Get();
	PlatformOptions.ClientCredentials.ClientSecret = ClientSecret.Get();
	PlatformOptions.EncryptionKey = EncryptionKey.Get();

	if (FPlatformMisc::IsCacheStorageAvailable())
	{
		PlatformOptions.CacheDirectory = CacheDirectory.Get();
	}
	else
	{
		PlatformOptions.CacheDirectory = nullptr;
	}

	IEOSPlatformHandlePtr EOSPlatformHandle = SDKManager->CreatePlatform(PlatformOptions);
	if (!EOSPlatformHandle)
	{
		UE_LOG(LogOnlineServices, Warning, TEXT("[FOnlineServicesEOSGS::Initialize] Failed to create platform."));
	}
	return EOSPlatformHandle;
}

IEOSPlatformHandlePtr FOnlineServicesEOSGSPlatformFactory::GetDefaultPlatform()
{
	if (!DefaultEOSPlatformHandle)
	{
		DefaultEOSPlatformHandle = CreatePlatform();
	}

	return DefaultEOSPlatformHandle;
}

/* UE::Online */ }
