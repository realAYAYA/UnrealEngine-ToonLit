// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HoloLensTargetPlatform.h: Declares the FXboxOneTargetPlatform class.
=============================================================================*/

#pragma once

#include "Common/TargetPlatformBase.h"
#include "HoloLensPlatformProperties.h"
#include "Misc/ConfigCacheIni.h"
#include "HoloLensTargetDevice.h"
#include "Misc/ScopeLock.h"
#include "IHoloLensDeviceDetector.h"

#if PLATFORM_WINDOWS || PLATFORM_HOLOLENS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#if WITH_ENGINE
#include "StaticMeshResources.h"
#endif // WITH_ENGINE

#define LOCTEXT_NAMESPACE "HoloLensTargetPlatform"

/**
 * FHoloLensTargetPlatform, abstraction for cooking HoloLens platforms
 */
class HOLOLENSTARGETPLATFORM_API FHoloLensTargetPlatform
	: public TNonDesktopTargetPlatformBase<FHoloLensPlatformProperties>
{
public:

	/**
	 * Default constructor.
	 */
	FHoloLensTargetPlatform(bool bIsClient);

	/**
	 * Destructor.
	 */
	virtual ~FHoloLensTargetPlatform();

public:

	//~ Begin ITargetPlatform Interface

	virtual bool AddDevice(const FString& DeviceId, const FString& DeviceUserFriendlyName, const FString& Username, const FString& Password, bool bDefault) override;

	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual void GetAllDevices(TArray<ITargetDevicePtr>& OutDevices) const override;

	virtual ITargetDevicePtr GetDefaultDevice() const override;

	virtual ITargetDevicePtr GetDevice(const FTargetDeviceId& DeviceId) override;

	//virtual ECompressionFlags GetBaseCompressionMethod() const override { return ECompressionFlags::COMPRESS_ZLIB; }

	virtual bool GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& ChunkMap, const TSet<int32>& ChunkIDsInUse) const override { return true; }

	virtual bool SupportsFeature(ETargetPlatformFeatures Feature) const override;

	virtual bool SupportsBuildTarget(EBuildTargetType BuildTarget) const override;

	virtual void GetAllPossibleShaderFormats(TArray<FName>& OutFormats) const override;

	virtual void GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const override;	

	virtual void GetPlatformSpecificProjectAnalytics( TArray<struct FAnalyticsEventAttribute>& AnalyticsParamArray ) const override;

#if WITH_ENGINE
	virtual void GetReflectionCaptureFormats(TArray<FName>& OutFormats) const override;

	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings() const override { return StaticMeshLODSettings; }

	virtual void GetTextureFormats(const UTexture* InTexture, TArray< TArray<FName> >& OutFormats) const override;

	virtual const UTextureLODSettings& GetTextureLODSettings() const override { return *TextureLODSettings; }

	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override;

	virtual void RegisterTextureLODSettings(const UTextureLODSettings* InTextureLODSettings) override
	{
		TextureLODSettings = InTextureLODSettings;
	}
	
#endif //WITH_ENGINE

	virtual EPlatformAuthentication RequiresUserCredentials() const override
	{
		return EPlatformAuthentication::Possible;
	}

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const override;

	virtual int32 CheckRequirements(bool bProjectHasCode, EBuildConfiguration Configuration, bool bRequiresAssetNativization, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const override;
	//~ End ITargetPlatform Interface

private:
	IHoloLensDeviceDetectorPtr DeviceDetector;

	void OnDeviceDetected(const FHoloLensDeviceInfo& Info);

	mutable FCriticalSection DevicesLock;
	TArray<ITargetDevicePtr> Devices;

	FDelegateHandle DeviceDetectedRegistration;

#if WITH_ENGINE
	// Holds a cache of the target LOD settings.
	const UTextureLODSettings* TextureLODSettings;

	// Holds the static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;
#endif //WITH_ENGINE

};


#undef LOCTEXT_NAMESPACE

#include "Windows/HideWindowsPlatformTypes.h"

DECLARE_LOG_CATEGORY_EXTERN(LogHoloLensTargetPlatform, Log, All);
