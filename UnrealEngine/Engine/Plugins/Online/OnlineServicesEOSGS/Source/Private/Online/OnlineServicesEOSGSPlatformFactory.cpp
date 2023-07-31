// Copyright Epic Games, Inc. All Rights Reserved.

#include "Online/OnlineServicesEOSGSPlatformFactory.h"

#include "Online/OnlineServicesEOSGS.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/LazySingleton.h"
#include "Modules/ModuleManager.h"

#include "EOSShared.h"
#include "EOSSharedTypes.h"
#include "IEOSSDKManager.h"

#include "CoreMinimal.h"

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
	GConfig->GetString(*ConfigSection, TEXT("EncryptionKey"), PlatformConfig.EncryptionKey, GEngineIni);
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
	DefaultEOSPlatformHandle = CreatePlatform();
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
		return {};
	}

	EOS_EResult InitResult = SDKManager->Initialize();
	if (InitResult != EOS_EResult::EOS_Success)
	{
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
	PlatformOptions.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
	PlatformOptions.Reserved = nullptr;
	PlatformOptions.bIsServer = EOS_FALSE;
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
	PlatformOptions.CacheDirectory = CacheDirectory.Get();

	IEOSPlatformHandlePtr EOSPlatformHandle = SDKManager->CreatePlatform(PlatformOptions);
	if (!EOSPlatformHandle)
	{
		UE_LOG(LogOnlineServices, Warning, TEXT("[FOnlineServicesEOSGS::Initialize] Failed to create platform."));
	}
	return EOSPlatformHandle;
}


/* UE::Online */ }
