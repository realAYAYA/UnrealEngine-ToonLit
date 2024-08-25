// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	GenericMacTargetPlatform.h: Declares the TGenericMacTargetPlatform class template.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Common/TargetPlatformBase.h"
#include "Mac/MacPlatformProperties.h"
#include "Misc/ConfigCacheIni.h"
#include "LocalMacTargetDevice.h"
#include "AnalyticsEventAttribute.h"


#if WITH_ENGINE
#include "AudioCompressionSettings.h"
#include "Sound/SoundWave.h"
#include "TextureResource.h"
#include "StaticMeshResources.h"
#endif // WITH_ENGINE

#define LOCTEXT_NAMESPACE "TGenericMacTargetPlatform"

/**
 * Template for Mac target platforms
 */
template<bool HAS_EDITOR_DATA, bool IS_DEDICATED_SERVER, bool IS_CLIENT_ONLY>
class TGenericMacTargetPlatform
	: public TTargetPlatformBase<FMacPlatformProperties<HAS_EDITOR_DATA, IS_DEDICATED_SERVER, IS_CLIENT_ONLY> >
{
public:

	typedef FMacPlatformProperties<HAS_EDITOR_DATA, IS_DEDICATED_SERVER, IS_CLIENT_ONLY> TProperties;
	typedef TTargetPlatformBase<TProperties> TSuper;

	/**
	 * Default constructor.
	 */
	TGenericMacTargetPlatform( )
	{
#if PLATFORM_MAC
		// only add local device if actually running on Mac
		LocalDevice = MakeShareable(new FLocalMacTargetDevice(*this));
#endif

		#if WITH_ENGINE
			TextureLODSettings = nullptr;
			StaticMeshLODSettings.Initialize(this);
		#endif
	}

public:

	//~ Begin ITargetPlatform Interface

	virtual void EnableDeviceCheck(bool OnOff) override {}

	virtual void GetAllDevices( TArray<ITargetDevicePtr>& OutDevices ) const override
	{
		OutDevices.Reset();
		if (LocalDevice.IsValid())
		{
			OutDevices.Add(LocalDevice);
		}
	}

	virtual bool GenerateStreamingInstallManifest(const TMultiMap<FString, int32>& PakchunkMap, const TSet<int32>& PakchunkIndicesInUse) const override
	{
		return true;
	}

	virtual ITargetDevicePtr GetDefaultDevice( ) const override
	{
		if (LocalDevice.IsValid())
		{
			return LocalDevice;
		}

		return nullptr;
	}

	virtual ITargetDevicePtr GetDevice( const FTargetDeviceId& DeviceId )
	{
		if (LocalDevice.IsValid() && (DeviceId == LocalDevice->GetId()))
		{
			return LocalDevice;
		}

		return NULL;
	}

	virtual bool IsRunningPlatform( ) const override
	{
		// Must be Mac platform as editor for this to be considered a running platform
		return PLATFORM_MAC && !UE_SERVER && !UE_GAME && WITH_EDITOR && HAS_EDITOR_DATA;
	}

	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const override
	{
		// we currently do not have a build target for MacServer
		if (Feature == ETargetPlatformFeatures::Packaging)
		{
			return (HAS_EDITOR_DATA || !IS_DEDICATED_SERVER);
		}

		if (Feature == ETargetPlatformFeatures::CanCookPackages)
		{
			return HAS_EDITOR_DATA;
		}

		return TSuper::SupportsFeature(Feature);
	}

	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override
	{
		// no shaders needed for dedicated server target
		if (!IS_DEDICATED_SERVER)
		{
			static FName NAME_SF_METAL_SM5(TEXT("SF_METAL_SM5"));
			OutFormats.AddUnique(NAME_SF_METAL_SM5);
            static FName NAME_SF_METAL_SM6(TEXT("SF_METAL_SM6"));
            OutFormats.AddUnique(NAME_SF_METAL_SM6);
			static FName NAME_SF_METAL_MACES3_1(TEXT("SF_METAL_MACES3_1"));
			OutFormats.AddUnique(NAME_SF_METAL_MACES3_1);
			static FName NAME_SF_METAL_MRT_MAC(TEXT("SF_METAL_MRT_MAC"));
			OutFormats.AddUnique(NAME_SF_METAL_MRT_MAC);
		}
	}

	virtual void GetAllTargetedShaderFormats(TArray<FName>& OutFormats) const override
	{
		// Get the Target RHIs for this platform, we do not always want all those that are supported.
		TArray<FString>TargetedShaderFormats;
		GConfig->GetArray(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("TargetedRHIs"), TargetedShaderFormats, GEngineIni);

		// Gather the list of Target RHIs and filter out any that may be invalid.
		TArray<FName> PossibleShaderFormats;
		GetAllPossibleShaderFormats(PossibleShaderFormats);

		for (int32 ShaderFormatIdx = TargetedShaderFormats.Num() - 1; ShaderFormatIdx >= 0; ShaderFormatIdx--)
		{
			FString ShaderFormat = TargetedShaderFormats[ShaderFormatIdx];
			if (PossibleShaderFormats.Contains(FName(*ShaderFormat)) == false)
			{
				TargetedShaderFormats.RemoveAt(ShaderFormatIdx);
			}
		}

		for(const FString& ShaderFormat : TargetedShaderFormats)
		{
			OutFormats.AddUnique(FName(*ShaderFormat));
		}
	}

	virtual void GetPlatformSpecificProjectAnalytics( TArray<FAnalyticsEventAttribute>& AnalyticsParamArray ) const override
	{
		TSuper::GetPlatformSpecificProjectAnalytics(AnalyticsParamArray);

		TSuper::AppendAnalyticsEventConfigArray(AnalyticsParamArray, TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("TargetedRHIs"), GEngineIni);
	}

#if WITH_ENGINE
	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings( ) const override
	{
		return StaticMeshLODSettings;
	}

	virtual void GetTextureFormats( const UTexture* Texture, TArray< TArray<FName> >& OutFormats) const override
	{
		if (!IS_DEDICATED_SERVER)
		{
			// just use the standard texture format name for this texture (with DX11 support)
			GetDefaultTextureFormatNamePerLayer(OutFormats.AddDefaulted_GetRef(), this, Texture, true, 4, true);
		}
	}

	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override
	{
		if (!IS_DEDICATED_SERVER)
		{
			// just use the standard texture format name for this texture (with DX11 support)
			GetAllDefaultTextureFormats(this, OutFormats);
		}
	}

	virtual bool SupportsLQCompressionTextureFormat() const override { return false; };

	virtual const UTextureLODSettings& GetTextureLODSettings() const override
	{
		return *TextureLODSettings;
	}

	virtual void RegisterTextureLODSettings(const UTextureLODSettings* InTextureLODSettings) override
	{
		TextureLODSettings = InTextureLODSettings;
	}

	virtual bool CanSupportRemoteShaderCompile() const override
	{
		return true;
	}
	
	virtual void GetShaderCompilerDependencies(TArray<FString>& OutDependencies) const override
	{
		FTargetPlatformBase::AddDependencySCArrayHelper(OutDependencies, TEXT("Binaries/ThirdParty/ShaderConductor/Mac/libdxcompiler.dylib"));
		FTargetPlatformBase::AddDependencySCArrayHelper(OutDependencies, TEXT("Binaries/ThirdParty/ShaderConductor/Mac/libShaderConductor.dylib"));
	}

#endif //WITH_ENGINE


	virtual bool SupportsVariants() const override
	{
		return true;
	}


	virtual float GetVariantPriority() const override
	{
		return TProperties::GetVariantPriority();
	}

	static FORCEINLINE bool SupportsRayTracing()
	{
		return true;
	}

	virtual bool UsesRayTracing() const override
	{
		bool bEnableRayTracing = false;
		GConfig->GetBool(TEXT("/Script/MacTargetPlatform.MacTargetSettings"), TEXT("bEnableRayTracing"), bEnableRayTracing, GEngineIni);
	 
		return bEnableRayTracing;
	}
	//~ End ITargetPlatform Interface

private:

	// Holds the local device.
	ITargetDevicePtr LocalDevice;

#if WITH_ENGINE
	// Holds the texture LOD settings.
	const UTextureLODSettings* TextureLODSettings;

	// Holds the static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;

#endif // WITH_ENGINE

};

#undef LOCTEXT_NAMESPACE
