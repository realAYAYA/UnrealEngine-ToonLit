// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AndroidTargetPlatform.h: Declares the FAndroidTargetPlatform class.
=============================================================================*/

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Delegates/IDelegateInstance.h"
#include "Containers/Map.h"
#include "Delegates/Delegate.h"
#include "Containers/Ticker.h"
#include "Misc/ScopeLock.h"
#include "Android/AndroidPlatformProperties.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "Common/TargetPlatformBase.h"
#include "Interfaces/IAndroidDeviceDetection.h"
#include "Interfaces/IAndroidDeviceDetectionModule.h"
#include "AndroidTargetDevice.h"

#if WITH_ENGINE
#include "Engine/TextureCube.h"
#include "Internationalization/Text.h"
#include "StaticMeshResources.h"
#endif // WITH_ENGINE

#define LOCTEXT_NAMESPACE "FAndroidTargetPlatform" 

class FTargetDeviceId;
class IAndroidDeviceDetection;
class ITargetPlatform;
class UTextureLODSettings;
enum class ETargetPlatformFeatures;
template<typename TPlatformProperties> class TTargetPlatformBase;

template< typename InElementType, typename KeyFuncs , typename Allocator > class TSet;
template<typename KeyType,typename ValueType,typename SetAllocator ,typename KeyFuncs > class TMap;
template<typename KeyType,typename ValueType,typename SetAllocator ,typename KeyFuncs > class TMultiMap;
template<typename TPlatformProperties> class TTargetPlatformBase;

/** Listed in order of priority...if device supports multiple formats, first format in list will be chosen */
enum class EAndroidTextureFormatCategory
{
	DXT,
	ETC2,
	ASTC,

	Count,
};

/**
 * FAndroidTargetPlatform, abstraction for cooking Android platforms
 */
class ANDROIDTARGETPLATFORM_API FAndroidTargetPlatform : public TNonDesktopTargetPlatformBase<FAndroidPlatformProperties>
{
public:

	/**
	 * Default constructor.
	 */
	FAndroidTargetPlatform(bool bIsClient, const TCHAR* FlavorName, const TCHAR* OverrideIniPlatformName=nullptr);

	/**
	 * Destructor
	 */
	virtual ~FAndroidTargetPlatform();

public:

	/**
	 * Gets the name of the Android platform variant, i.e. ASTC, ETC2, DXT, etc.
	 *
	 * @param Variant name.
	 */
	FString GetAndroidVariantName() const
	{
		return PlatformInfo->PlatformFlavor.ToString();
	}

public:

	//~ Begin ITargetPlatform Interface

	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual bool AddDevice( const FString& DeviceName, bool bDefault ) override
	{
		return false;
	}

	virtual void GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const override;

	virtual bool GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& PakchunkMap, const TSet<int32>& PakchunkIndicesInUse) const override
	{
		return true;
	}

	virtual ITargetDevicePtr GetDefaultDevice( ) const override;

	virtual ITargetDevicePtr GetDevice( const FTargetDeviceId& DeviceId ) override;

	virtual bool IsSdkInstalled(bool bProjectHasCode, FString& OutDocumentationPath) const override;

	virtual int32 CheckRequirements(bool bProjectHasCode, EBuildConfiguration Configuration, bool bRequiresAssetNativization, FString& OutTutorialPath, FString& OutDocumentationPath, FText& CustomizedLogMessage) const override;

	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const override;

	virtual bool SupportsTextureFormatCategory(EAndroidTextureFormatCategory Category) const
	{
		return true;
	}

	virtual bool SupportsCompressedNonPOT( ) const
	{
		// most formats do support non-POT compressed textures
		return true;
	}

	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override;

	virtual void GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const override;

	virtual void GetPlatformSpecificProjectAnalytics( TArray<struct FAnalyticsEventAttribute>& AnalyticsParamArray ) const override;
	
#if WITH_ENGINE
	virtual void GetReflectionCaptureFormats( TArray<FName>& OutFormats ) const override;

	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings() const override;

	virtual void GetTextureFormats( const UTexture* InTexture, TArray< TArray<FName> >& OutFormats) const override;

	virtual FName FinalizeVirtualTextureLayerFormat(FName Format) const override;

	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override;

	virtual const UTextureLODSettings& GetTextureLODSettings() const override;

	virtual void RegisterTextureLODSettings(const UTextureLODSettings* InTextureLODSettings) override
	{
		TextureLODSettings = InTextureLODSettings;
	}

#endif //WITH_ENGINE

	virtual bool SupportsVariants() const override;

	virtual void GetBuildProjectSettingKeys(FString& OutSection, TArray<FString>& InBoolKeys, TArray<FString>& InIntKeys, TArray<FString>& InStringKeys) const override
	{
		OutSection = TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings");
		InBoolKeys.Add(TEXT("bBuildForArm64"));	InBoolKeys.Add(TEXT("bBuildForX8664"));
		InBoolKeys.Add(TEXT("bBuildForES31")); InBoolKeys.Add(TEXT("bBuildWithHiddenSymbolVisibility"));
		InBoolKeys.Add(TEXT("bSaveSymbols")); InStringKeys.Add(TEXT("NDKAPILevel"));
	}

	virtual bool ShouldExpandTo32Bit(const uint16* Indices, const int32 NumIndices) const override;
	//~ End ITargetPlatform Interface

	virtual void InitializeDeviceDetection();
	
	virtual bool UsesDistanceFields() const override
	{
		return bDistanceField;
	}

protected:
	/**
	 * Return true if this device has a supported set of extensions for this platform.
	 *
	 * @param Extensions - The GL extensions string.
	 * @param GLESVersion - The GLES version reported by this device.
	 */
	virtual bool SupportedByExtensionsString( const FString& ExtensionsString, const int GLESVersion ) const
	{
		return true;
	}

	virtual FAndroidTargetDevicePtr CreateTargetDevice(const ITargetPlatform& InTargetPlatform, const FString& InSerialNumber, const FString& InAndroidVariant) const;

	// query for rendering mode support
	bool SupportsES31() const;
	bool SupportsVulkan() const;
	bool SupportsVulkanSM5() const;

protected:

	// Handles when the ticker fires.
	bool HandleTicker( float DeltaTime );

	virtual FAndroidTargetDeviceRef CreateNewDevice(const FAndroidDeviceInfo &DeviceInfo);

	// Holds a map of valid devices.
	TMap<FString, FAndroidTargetDevicePtr> Devices;

	// Holds a delegate to be invoked when the widget ticks.
	FTickerDelegate TickDelegate;

	// Handle to the registered TickDelegate.
	FTSTicker::FDelegateHandle TickDelegateHandle;

	// Pointer to the device detection handler that grabs device ids in another thread
	IAndroidDeviceDetection* DeviceDetection;

	// r.Mobile.ShadingPath value
	int32 MobileShadingPath;
	
	// true if DistanceField is enabled
	bool bDistanceField;

	// r.Mobile.Forward.EnableClusteredReflections value
	bool bMobileForwardEnableClusteredReflections;
	
#if WITH_ENGINE
	// Holds a cache of the target LOD settings.
	const UTextureLODSettings* TextureLODSettings;

	// Holds the static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;

	ITargetDevicePtr DefaultDevice;
#endif //WITH_ENGINE
};


//#include "AndroidTargetPlatform.inl"


class FAndroid_DXTTargetPlatform : public FAndroidTargetPlatform
{
public:
	FAndroid_DXTTargetPlatform(bool bIsClient) : FAndroidTargetPlatform(bIsClient, TEXT("DXT"))
	{
	}
	
#if WITH_ENGINE
	virtual void GetTextureFormats(const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const override;
#endif

	virtual bool SupportsTextureFormatCategory(EAndroidTextureFormatCategory Category) const override
	{
		return Category == EAndroidTextureFormatCategory::DXT;
	}

	virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const override
	{
		return (ExtensionsString.Contains(TEXT("GL_NV_texture_compression_s3tc")) || ExtensionsString.Contains(TEXT("GL_EXT_texture_compression_s3tc")));
	}

	virtual float GetVariantPriority() const override
	{
		float Priority;
		return (GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("TextureFormatPriority_DXT"), Priority, GEngineIni) ?
			Priority : 0.6f) * 10.0f + (IsClientOnly() ? 0.25f : 0.5f);
	}
};

class FAndroid_ASTCTargetPlatform : public FAndroidTargetPlatform
{
public:
	FAndroid_ASTCTargetPlatform(bool bIsClient) : FAndroidTargetPlatform(bIsClient, TEXT("ASTC")) 
	{
	}

	virtual bool SupportsTextureFormatCategory(EAndroidTextureFormatCategory Category) const override
	{
		return Category == EAndroidTextureFormatCategory::ASTC;
	}

#if WITH_ENGINE
	virtual void GetTextureFormats(const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const;
	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override;
#endif

	virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const override
	{
		return ExtensionsString.Contains(TEXT("GL_KHR_texture_compression_astc_ldr"));
	}

	virtual float GetVariantPriority() const override
	{
		float Priority;
		return (GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("TextureFormatPriority_ASTC"), Priority, GEngineIni) ?
			Priority : 0.9f) * 10.0f + (IsClientOnly() ? 0.25f : 0.5f);
	}
};


class FAndroid_ETC2TargetPlatform : public FAndroidTargetPlatform
{
public:

	FAndroid_ETC2TargetPlatform(bool bIsClient) : FAndroidTargetPlatform(bIsClient, TEXT("ETC2"))
	{
	}

	virtual bool SupportsTextureFormatCategory(EAndroidTextureFormatCategory Category) const override
	{
		return Category == EAndroidTextureFormatCategory::ETC2;
	}

#if WITH_ENGINE
	virtual void GetTextureFormats(const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const;
	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override;
#endif

	virtual bool SupportedByExtensionsString(const FString& ExtensionsString, const int GLESVersion) const override
	{
		return GLESVersion >= 0x30000;
	}

	virtual float GetVariantPriority() const override
	{
		float Priority;
		return (GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("TextureFormatPriority_ETC2"), Priority, GEngineIni) ?
			Priority : 0.2f) * 10.0f + (IsClientOnly() ? 0.25f : 0.5f);
	}
};

class FAndroid_MultiTargetPlatform : public FAndroidTargetPlatform
{
	TArray<ITargetPlatform*> FormatTargetPlatforms;
	FString FormatTargetString;

public:
	FAndroid_MultiTargetPlatform(bool bIsClient) : FAndroidTargetPlatform(bIsClient, TEXT("Multi"))
	{
	}

	// set up all of the multiple formats together into this one
	void LoadFormats(TArray<FAndroidTargetPlatform*> SingleFormatTPs)
	{
		// sort formats by priority so higher priority formats are packaged (and thus used by the device) first
		// note that we passed this by value, not ref, so we can sort it
		SingleFormatTPs.Sort([](const FAndroidTargetPlatform& A, const FAndroidTargetPlatform& B)
		{
			float PriorityA = 0.f;
			float PriorityB = 0.f;
			FString VariantA = A.GetAndroidVariantName().Replace(TEXT("Client"), TEXT(""));
			FString VariantB = B.GetAndroidVariantName().Replace(TEXT("Client"), TEXT(""));
			GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), *(FString(TEXT("TextureFormatPriority_")) + VariantA), PriorityA, GEngineIni);
			GConfig->GetFloat(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), *(FString(TEXT("TextureFormatPriority_")) + VariantB), PriorityB, GEngineIni);
			return PriorityA > PriorityB;
		});

		FormatTargetPlatforms.Empty();
		FormatTargetString = TEXT("");

		TSet<FString> SeenFormats;

		// Load the TargetPlatform module for each format
		for (FAndroidTargetPlatform* SingleFormatTP : SingleFormatTPs)
		{
			// only use once each
			if (SeenFormats.Contains(SingleFormatTP->GetAndroidVariantName()))
			{
				continue;
			}
			SeenFormats.Add(SingleFormatTP->GetAndroidVariantName());

			bool bEnabled = false;
			FString SettingsName = FString(TEXT("bMultiTargetFormat_")) + *SingleFormatTP->GetAndroidVariantName();
			GConfig->GetBool(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), *SettingsName, bEnabled, GEngineIni);
			if (bEnabled)
			{
				if (FormatTargetPlatforms.Num())
				{
					FormatTargetString += TEXT(",");
				}
				FormatTargetString += SingleFormatTP->GetAndroidVariantName();
				FormatTargetPlatforms.Add(SingleFormatTP);
			}
		}

		// @todo do we need this with DisplayName below?
		PlatformInfo::UpdatePlatformDisplayName(TEXT("Android_Multi"), DisplayName());
	}

	virtual FText DisplayName() const override
	{
		return FText::Format(LOCTEXT("Android_Multi", "Android (Multi:{0})"), FText::FromString(FormatTargetString));
	}

#if WITH_ENGINE
	virtual void GetTextureFormats(const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const
	{
		// Ask each platform variant to choose texture formats
		for (ITargetPlatform* Platform : FormatTargetPlatforms)
		{
			TArray< TArray<FName> > PlatformFormats;
			Platform->GetTextureFormats(Texture, PlatformFormats);
			for (const TArray<FName>& FormatPerLayer : PlatformFormats)
			{
				OutFormats.AddUnique(FormatPerLayer);
			}
		}
	}

	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override
	{
		// Ask each platform variant to choose texture formats
		for (ITargetPlatform* Platform : FormatTargetPlatforms)
		{
			TArray<FName> PlatformFormats;
			Platform->GetAllTextureFormats(PlatformFormats);
			for (FName Format : PlatformFormats)
			{
				OutFormats.AddUnique(Format);
			}
		}
	}
#endif	

	virtual float GetVariantPriority() const override
	{
		// lowest priority so specific variants are chosen first
		return (IsClientOnly() ? 0.25f : 0.5f);
	}
};

#undef LOCTEXT_NAMESPACE
