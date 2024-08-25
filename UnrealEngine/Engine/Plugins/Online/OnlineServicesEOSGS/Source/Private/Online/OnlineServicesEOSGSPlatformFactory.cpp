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

FString LoadEOSPlatformConfig(IEOSSDKManager* const SDKManager)
{
	FString ConfigSectionName = FString::Printf(TEXT("OnlineServices.%s"), UE::Online::FOnlineServicesEOSGS::GetConfigNameStatic());
	if (!GConfig->DoesSectionExist(*ConfigSectionName, GEngineIni))
	{
		return FString();
	}

	// Check for explicit shared name to use.
	FString PlatformConfigName;
	GConfig->GetString(*ConfigSectionName, TEXT("PlatformConfigName"), PlatformConfigName, GEngineIni);
	if (!PlatformConfigName.IsEmpty())
	{
		return PlatformConfigName;
	}

	// Check for cached config.
	if (SDKManager->GetPlatformConfig(ConfigSectionName))
	{
		return ConfigSectionName;
	}

	// Check for legacy config. This should be handled by EOSShared instead, see FEOSSDKManager::GetPlatformConfig for details.
	FEOSSDKPlatformConfig PlatformConfig;
	PlatformConfig.Name = ConfigSectionName;

	GConfig->GetString(*ConfigSectionName, TEXT("ProductId"), PlatformConfig.ProductId, GEngineIni);
	if (PlatformConfig.ProductId.IsEmpty())
	{
		// If we're missing ProductId, assume we're missing the rest and instead rely on default EOSShared config.
		return FString();
	}

	// Instead of specifying this config under the OnlineServices.EOS section, options should be moved to a new EOSSDK.Platform.<name> section so
	// all modules relying on the EOSSDK can share the same config and platform instance. See FEOSSDKManager::GetPlatformConfig for details.
	UE_LOG(LogOnlineServices, Warning, TEXT("[LoadEOSPlatformConfig] Using legacy config from %s, use EOSShared named config instead."), *ConfigSectionName);

	GConfig->GetString(*ConfigSectionName, TEXT("SandboxId"), PlatformConfig.SandboxId, GEngineIni);
	GConfig->GetString(*ConfigSectionName, TEXT("DeploymentId"), PlatformConfig.DeploymentId, GEngineIni);
	GConfig->GetString(*ConfigSectionName, TEXT("ClientId"), PlatformConfig.ClientId, GEngineIni);
	GConfig->GetString(*ConfigSectionName, TEXT("ClientSecret"), PlatformConfig.ClientSecret, GEngineIni);
	// Config key renamed to ClientEncryptionKey as EncryptionKey gets removed from packaged builds due to IniKeyDenylist=EncryptionKey entry in BaseGame.ini.
	GConfig->GetString(*ConfigSectionName, TEXT("ClientEncryptionKey"), PlatformConfig.EncryptionKey, GEngineIni);

	PlatformConfig.CacheDirectory = SDKManager->GetCacheDirBase() / TEXT("OnlineServicesEOS");

	PlatformConfig.bIsServer = IsRunningDedicatedServer() ? EOS_TRUE : EOS_FALSE;
	if (!IsRunningGame())
	{
		PlatformConfig.bLoadingInEditor = true;
	}
	else
	{
		PlatformConfig.bWindowsEnableOverlayD3D9 = true;
		PlatformConfig.bWindowsEnableOverlayD3D10 = true;
		PlatformConfig.bWindowsEnableOverlayOpenGL = true;
	}

	if (!SDKManager->AddPlatformConfig(PlatformConfig))
	{
		return FString();
	}

	return ConfigSectionName;
}

namespace UE::Online {

FOnlineServicesEOSGSPlatformFactory::FOnlineServicesEOSGSPlatformFactory()
{
}

FOnlineServicesEOSGSPlatformFactory& FOnlineServicesEOSGSPlatformFactory::Get()
{
	return TLazySingleton<FOnlineServicesEOSGSPlatformFactory>::Get();
}

void FOnlineServicesEOSGSPlatformFactory::TearDown()
{
	return TLazySingleton<FOnlineServicesEOSGSPlatformFactory>::TearDown();
}

IEOSPlatformHandlePtr FOnlineServicesEOSGSPlatformFactory::CreatePlatform(FName InstanceName)
{
	const FName EOSSharedModuleName = TEXT("EOSShared");
	if (!FModuleManager::Get().IsModuleLoaded(EOSSharedModuleName))
	{
		FModuleManager::Get().LoadModuleChecked(EOSSharedModuleName);
	}

	IEOSSDKManager* const SDKManager = IEOSSDKManager::Get();
	if (!SDKManager)
	{
		UE_LOG(LogOnlineServices, Error, TEXT("[FOnlineServicesEOSGSPlatformFactory::CreatePlatform] EOSSDK has not been loaded."));
		return {};
	}

	if (!SDKManager->IsInitialized())
	{
		UE_LOG(LogOnlineServices, Error, TEXT("[FOnlineServicesEOSGSPlatformFactory::CreatePlatform] EOSSDK has not been initialized."));
		return {};
	}

	FString PlatformConfigName = LoadEOSPlatformConfig(SDKManager);
	if (PlatformConfigName.IsEmpty())
	{
		// Check for default platform config that other modules may have setup.
		PlatformConfigName = SDKManager->GetDefaultPlatformConfigName();
		if (PlatformConfigName.IsEmpty())
		{
			UE_LOG(LogOnlineServices, Verbose, TEXT("[FOnlineServicesEOSGSPlatformFactory::CreatePlatform] Could not find platform config."));
			return {};
		}
	}

	IEOSPlatformHandlePtr EOSPlatformHandle = SDKManager->CreatePlatform(PlatformConfigName, InstanceName);
	if (!EOSPlatformHandle)
	{
		UE_LOG(LogOnlineServices, Warning, TEXT("[FOnlineServicesEOSGSPlatformFactory::CreatePlatform] Failed to create platform."));
	}

	return EOSPlatformHandle;
}

IEOSPlatformHandlePtr FOnlineServicesEOSGSPlatformFactory::GetDefaultPlatform()
{
	if (!DefaultEOSPlatformHandle)
	{
		DefaultEOSPlatformHandle = CreatePlatform(NAME_None);
	}

	return DefaultEOSPlatformHandle;
}

/* UE::Online */ }
