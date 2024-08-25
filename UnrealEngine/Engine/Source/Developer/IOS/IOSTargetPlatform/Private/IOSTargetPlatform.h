// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IOSTargetPlatform.h: Declares the FIOSTargetPlatform class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Common/TargetPlatformBase.h"
#include "IOS/IOSPlatformProperties.h"
#include "Containers/Ticker.h"
#include "IOSMessageProtocol.h"
#include "IMessageContext.h"
#include "IOSTargetDevice.h"
#include "IOSDeviceHelper.h"
#include "Misc/ConfigCacheIni.h"


#if WITH_ENGINE
#include "AudioCompressionSettings.h"
#include "StaticMeshResources.h"
#endif // WITH_ENGINE

/**
 * FIOSTargetPlatform, abstraction for cooking iOS platforms
 */
class FIOSTargetPlatform : public TNonDesktopTargetPlatformBase<FIOSPlatformProperties>
{
public:

	/**
	 * Default constructor.
	 */
	IOSTARGETPLATFORM_API FIOSTargetPlatform(bool bInISTVOS, bool bInIsVisionOS, bool bInIsClientOnly);

	/**
	 * Destructor.
	 */
	~FIOSTargetPlatform();

public:

	virtual void EnableDeviceCheck(bool OnOff) override;

	virtual void GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const override;

	virtual bool GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& PakchunkMap, const TSet<int32>& PakchunkIndicesInUse) const override
	{
		return true;
	}

	virtual ITargetDevicePtr GetDefaultDevice( ) const override;

	virtual ITargetDevicePtr GetDevice( const FTargetDeviceId& DeviceId ) override;
		
	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const override;

	virtual bool CanSupportRemoteShaderCompile() const override;

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutTutorialPath) const override;
	virtual int32 CheckRequirements(bool bProjectHasCode, EBuildConfiguration Configuration, bool bRequiresAssetNativization, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const override;

	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override;

	virtual void GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const override;

	virtual void GetPlatformSpecificProjectAnalytics( TArray<struct FAnalyticsEventAttribute>& AnalyticsParamArray ) const override;

#if WITH_ENGINE
	virtual void GetReflectionCaptureFormats( TArray<FName>& OutFormats ) const override;

	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings( ) const override
	{
		return StaticMeshLODSettings;
	}

	virtual void GetTextureFormats( const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const override;

	virtual void GetAllTextureFormats( TArray<FName>& OutFormats) const override;

	virtual FName FinalizeVirtualTextureLayerFormat(FName Format) const override;

	virtual const UTextureLODSettings& GetTextureLODSettings() const override;

	virtual void RegisterTextureLODSettings(const UTextureLODSettings* InTextureLODSettings) override
	{
		TextureLODSettings = InTextureLODSettings;
	}

#endif // WITH_ENGINE

	virtual void GetBuildProjectSettingKeys(FString& OutSection, TArray<FString>& InBoolKeys, TArray<FString>& InIntKeys, TArray<FString>& InStringKeys) const override
	{
		OutSection = TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings");
		InBoolKeys.Add(TEXT("bGeneratedSYMFile"));
		InBoolKeys.Add(TEXT("bGeneratedSYMBundle"));
		InBoolKeys.Add(TEXT("bGenerateXCArchive"));
		if (bIsTVOS)
		{
			InStringKeys.Add(TEXT("MinimumTVOSVersion"));
		}
		else
		{
			InStringKeys.Add(TEXT("MinimumiOSVersion"));
		}
	}

	//~ Begin ITargetPlatform Interface

	virtual bool UsesDistanceFields() const override
	{
		return bDistanceField;
	}

private:

	// Handles received pong messages from the LauncherDaemon.
	void HandlePongMessage( const FIOSLaunchDaemonPong& Message, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& Context );

    void HandleDeviceConnected( const FIOSLaunchDaemonPong& Message );
    void HandleDeviceDisconnected( const FIOSLaunchDaemonPong& Message );

private:
	
	// true if this is targeting TVOS vs IOS
	bool bIsTVOS;
	bool bIsVisionOS;

	// Contains all discovered IOSTargetDevices over the network.
	TMap<FTargetDeviceId, FIOSTargetDevicePtr> Devices;

	// Holds the message endpoint used for communicating with the LaunchDaemon.
	TSharedPtr<FMessageEndpoint, ESPMode::ThreadSafe> MessageEndpoint;

	// r.Mobile.ShadingPath value
	int32 MobileShadingPath;

	// true if DistanceField is enabled
	bool bDistanceField;

	// r.Mobile.Forward.EnableClusteredReflections value
	bool bMobileForwardEnableClusteredReflections;

	// r.Mobile.VirtualTextures value
	bool bMobileVirtualTextures;

#if WITH_ENGINE
	// Holds the cache of the target LOD settings.
	const UTextureLODSettings* TextureLODSettings;

	// Holds the static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;
#endif // WITH_ENGINE

    // holds usb device helper
	FIOSDeviceHelper DeviceHelper;

};
