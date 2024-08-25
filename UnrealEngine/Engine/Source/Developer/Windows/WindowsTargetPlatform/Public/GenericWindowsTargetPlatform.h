// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Common/TargetPlatformBase.h"
#include "Misc/ConfigCacheIni.h"
#include "AnalyticsEventAttribute.h"
#if PLATFORM_WINDOWS
#include "LocalPcTargetDevice.h"
#endif
#include "Serialization/MemoryLayout.h"
#include "SteamDeck/SteamDeckDevice.h"

#if WITH_ENGINE
	#include "Sound/SoundWave.h"
	#include "TextureResource.h"
	#include "Engine/VolumeTexture.h"
	#include "StaticMeshResources.h"
	#include "RHI.h"
	#include "DataDrivenShaderPlatformInfo.h"
	#include "AudioCompressionSettings.h"
#endif // WITH_ENGINE
#include "Windows/WindowsPlatformProperties.h"
#include "HAL/IConsoleManager.h"

#define LOCTEXT_NAMESPACE "TGenericWindowsTargetPlatform"

/**
 * Template for Windows target platforms
 */
#if PLATFORM_WINDOWS
template<typename TProperties, typename TTargetDevice = TLocalPcTargetDevice<PLATFORM_64BITS>>
#else
template<typename TProperties>
#endif

class TGenericWindowsTargetPlatform : public TTargetPlatformBase<TProperties>
{
public:
	typedef TTargetPlatformBase<TProperties> TSuper;

	/**
	 * Default constructor.
	 */
	TGenericWindowsTargetPlatform( )
	{
#if PLATFORM_WINDOWS
		// only add local device if actually running on Windows
		LocalDevice = MakeShareable(new TTargetDevice(*this));

		// quick solution to not having WinGDK steamdeck devices
		if (this->PlatformName().StartsWith(TEXT("Windows")))
		{
			// Check if we have any SteamDeck devices around
			SteamDevices = TSteamDeckDevice<TLocalPcTargetDevice<true>>::DiscoverDevices(*this, TEXT("Proton"));
		}
#endif

	#if WITH_ENGINE
		TextureLODSettings = nullptr; // These are registered by the device profile system.
		StaticMeshLODSettings.Initialize(this);


		// Get the Target RHIs for this platform, we do not always want all those that are supported.
		TArray<FName> TargetedShaderFormats;
		TGenericWindowsTargetPlatform::GetAllTargetedShaderFormats(TargetedShaderFormats);

		static FName NAME_PCD3D_SM6(TEXT("PCD3D_SM6"));
		static FName NAME_PCD3D_SM5(TEXT("PCD3D_SM5"));
		static FName NAME_VULKAN_SM5(TEXT("SF_VULKAN_SM5"));
		static FName NAME_VULKAN_SM6(TEXT("SF_VULKAN_SM6"));
		static FName NAME_PCD3D_ES3_1(TEXT("PCD3D_ES31"));
		static FName NAME_SF_VULKAN_ES31(TEXT("SF_VULKAN_ES31"));
		static FName NAME_OPENGL_150_ES3_1(TEXT("GLSL_150_ES31"));

		// If we are targeting ES3.1, we also must cook encoded HDR reflection captures
		bRequiresEncodedHDRReflectionCaptures =	TargetedShaderFormats.Contains(NAME_SF_VULKAN_ES31)
												|| TargetedShaderFormats.Contains(NAME_OPENGL_150_ES3_1)
												|| TargetedShaderFormats.Contains(NAME_PCD3D_ES3_1);

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

		for (const ITargetDevicePtr& SteamDeck : SteamDevices)
		{
			if (SteamDeck.IsValid())
			{
				OutDevices.Add(SteamDeck);
			}
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

		for (const ITargetDevicePtr& SteamDeck : SteamDevices)
		{
			if (SteamDeck.IsValid() && DeviceId == SteamDeck->GetId())
			{
				return SteamDeck;
			}
		}

		return nullptr;
	}

	virtual bool IsRunningPlatform( ) const override
	{
		// Must be Windows platform as editor for this to be considered a running platform
		return PLATFORM_WINDOWS && !UE_SERVER && !UE_GAME && WITH_EDITOR && TProperties::HasEditorOnlyData();
	}

	virtual void GetShaderCompilerDependencies(TArray<FString>& OutDependencies) const override
	{		
		FTargetPlatformBase::AddDependencySCArrayHelper(OutDependencies, TEXT("Binaries/ThirdParty/Windows/DirectX/x64/d3dcompiler_47.dll"));
		FTargetPlatformBase::AddDependencySCArrayHelper(OutDependencies, TEXT("Binaries/ThirdParty/ShaderConductor/Win64/ShaderConductor.dll"));
		FTargetPlatformBase::AddDependencySCArrayHelper(OutDependencies, TEXT("Binaries/ThirdParty/ShaderConductor/Win64/dxcompiler.dll"));
		FTargetPlatformBase::AddDependencySCArrayHelper(OutDependencies, TEXT("Binaries/ThirdParty/ShaderConductor/Win64/dxil.dll"));
	}

	virtual bool SupportsFeature( ETargetPlatformFeatures Feature ) const override
	{
		// we currently do not have a build target for WindowsServer
		if (Feature == ETargetPlatformFeatures::Packaging)
		{
			return (TProperties::HasEditorOnlyData() || !TProperties::IsServerOnly());
		}

		if ( Feature == ETargetPlatformFeatures::ShouldSplitPaksIntoSmallerSizes )
		{
			return TProperties::IsClientOnly();
		}

		if (Feature == ETargetPlatformFeatures::CanCookPackages)
		{
			return TProperties::HasEditorOnlyData();
		}

		if (Feature == ETargetPlatformFeatures::MobileRendering)
		{
			static bool bCachedSupportsMobileRendering = false;
#if WITH_ENGINE
			static bool bHasCachedValue = false;
			if (!bHasCachedValue)
			{
				TArray<FName> TargetedShaderFormats;
				GetAllTargetedShaderFormats(TargetedShaderFormats);

				for (const FName& Format : TargetedShaderFormats)
				{
					if (IsMobilePlatform(ShaderFormatToLegacyShaderPlatform(Format)))
					{
						bCachedSupportsMobileRendering = true;
						break;
					}
				}
				bHasCachedValue = true;
			}
#endif

			return bCachedSupportsMobileRendering;
		}

		return TSuper::SupportsFeature(Feature);
	}

	virtual void GetBuildProjectSettingKeys(FString& OutSection, TArray<FString>& InBoolKeys, TArray<FString>& InIntKeys, TArray<FString>& InStringKeys) const override
	{
		OutSection = TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings");
		InStringKeys.Add(TEXT("MinimumOSVersion"));
	}

	virtual void GetAllPossibleShaderFormats( TArray<FName>& OutFormats ) const override
	{
		// no shaders needed for dedicated server target
		if (!TProperties::IsServerOnly())
		{
			static FName NAME_PCD3D_SM6(TEXT("PCD3D_SM6"));
			static FName NAME_PCD3D_SM5(TEXT("PCD3D_SM5"));
			static FName NAME_VULKAN_ES31(TEXT("SF_VULKAN_ES31"));
			static FName NAME_OPENGL_150_ES3_1(TEXT("GLSL_150_ES31"));
			static FName NAME_VULKAN_SM5(TEXT("SF_VULKAN_SM5"));
			static FName NAME_VULKAN_SM6(TEXT("SF_VULKAN_SM6"));
			static FName NAME_PCD3D_ES3_1(TEXT("PCD3D_ES31"));

			OutFormats.AddUnique(NAME_PCD3D_SM5);
			OutFormats.AddUnique(NAME_PCD3D_SM6);
			OutFormats.AddUnique(NAME_VULKAN_ES31);
			OutFormats.AddUnique(NAME_OPENGL_150_ES3_1);
			OutFormats.AddUnique(NAME_VULKAN_SM5);
			OutFormats.AddUnique(NAME_VULKAN_SM6);
			OutFormats.AddUnique(NAME_PCD3D_ES3_1);
		}
	}

	void GetAllTargetedShaderFormatsInternal(TArrayView<TCHAR const*> RelevantSettings, TArray<FName>& OutFormats) const
	{
		TArray<FString> TargetedShaderFormats;

		for (const TCHAR* Name : RelevantSettings)
		{
			TArray<FString> NewTargetedShaderFormats;
			GConfig->GetArray(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), Name, NewTargetedShaderFormats, GEngineIni);

			for (const FString& NewShaderFormat : NewTargetedShaderFormats)
			{
				TargetedShaderFormats.AddUnique(NewShaderFormat);
			}
		}
		
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

	virtual void GetAllTargetedShaderFormats( TArray<FName>& OutFormats ) const override 
	{
		// Get the Target RHIs for this platform, we do not always want all those that are supported. (reload in case user changed in the editor)

		TCHAR const* RelevantSettings[] = 
		{
			TEXT("TargetedRHIs"),
			TEXT("D3D12TargetedShaderFormats"),
			TEXT("D3D11TargetedShaderFormats"),
			TEXT("VulkanTargetedShaderFormats")
		};

		GetAllTargetedShaderFormatsInternal(RelevantSettings, OutFormats);
	}

	virtual void GetRayTracingShaderFormats( TArray<FName>& OutFormats ) const override 
	{
		if (UsesRayTracing())
		{
			TCHAR const* RelevantSettings[] = 
			{
				TEXT("VulkanTargetedShaderFormats")
			};

			GetAllTargetedShaderFormatsInternal(RelevantSettings, OutFormats);

			// We always support ray tracing shaders when cooking for D3D12 SM6, however we may skip them for SM5 based on project settings.
			OutFormats.AddUnique(FName(TEXT("PCD3D_SM6")));

			static IConsoleVariable* RequireSM6CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing.RequireSM6"));
			const bool bRequireSM6 = RequireSM6CVar && RequireSM6CVar->GetBool();
			if (!bRequireSM6)
			{
				OutFormats.AddUnique(FName(TEXT("PCD3D_SM5")));
			}
		}
	}

	virtual void GetPlatformSpecificProjectAnalytics( TArray<FAnalyticsEventAttribute>& AnalyticsParamArray ) const override
	{
		TSuper::GetPlatformSpecificProjectAnalytics(AnalyticsParamArray);

		AppendAnalyticsEventAttributeArray(AnalyticsParamArray,
			TEXT("UsesRayTracing"), UsesRayTracing()
		);
	
		TSuper::AppendAnalyticsEventConfigString(AnalyticsParamArray, TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("DefaultGraphicsRHI"), GEngineIni);

		TSuper::AppendAnalyticsEventConfigArray(AnalyticsParamArray, TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("D3D12TargetedShaderFormats"), GEngineIni);
		TSuper::AppendAnalyticsEventConfigArray(AnalyticsParamArray, TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("D3D11TargetedShaderFormats"), GEngineIni);
		TSuper::AppendAnalyticsEventConfigArray(AnalyticsParamArray, TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("VulkanTargetedShaderFormats"), GEngineIni);
		TSuper::AppendAnalyticsEventConfigArray(AnalyticsParamArray, TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("TargetedRHIs"), GEngineIni, TEXT("TargetedRHIs_Deprecated") );
	}

#if WITH_ENGINE
	virtual void GetReflectionCaptureFormats(TArray<FName>& OutFormats) const override
	{
		if (bRequiresEncodedHDRReflectionCaptures)
		{
			OutFormats.Add(FName(TEXT("EncodedHDR")));
		}

		OutFormats.Add(FName(TEXT("FullHDR")));
	}

	virtual void GetShaderFormatModuleHints(TArray<FName>& OutModuleNames) const override
	{
		OutModuleNames.Add(TEXT("ShaderFormatD3D"));
		OutModuleNames.Add(TEXT("ShaderFormatOpenGL"));
		OutModuleNames.Add(TEXT("VulkanShaderFormat"));
	}

	virtual const class FStaticMeshLODSettings& GetStaticMeshLODSettings( ) const override
	{
		return StaticMeshLODSettings;
	}

	virtual void GetTextureFormats( const UTexture* InTexture, TArray< TArray<FName> >& OutFormats) const override
	{
		if (!TProperties::IsServerOnly())
		{
			GetDefaultTextureFormatNamePerLayer(OutFormats.AddDefaulted_GetRef(), this, InTexture, true, 4, true);
		}
	}

	virtual void GetAllTextureFormats(TArray<FName>& OutFormats) const override
	{
		if (!TProperties::IsServerOnly())
		{
			GetAllDefaultTextureFormats(this, OutFormats);
		}
	}

	virtual const UTextureLODSettings& GetTextureLODSettings() const override
	{
		return *TextureLODSettings;
	}

	virtual void RegisterTextureLODSettings(const UTextureLODSettings* InTextureLODSettings) override
	{
		TextureLODSettings = InTextureLODSettings;
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

	virtual bool UsesDistanceFields() const override
	{
		bool bEnableDistanceFields = false;
		GConfig->GetBool(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("bEnableDistanceFields"), bEnableDistanceFields, GEngineIni);

		return bEnableDistanceFields && TSuper::UsesDistanceFields();
	}

	virtual bool UsesRayTracing() const override
	{
		bool bEnableRayTracing = false;
		GConfig->GetBool(TEXT("/Script/WindowsTargetPlatform.WindowsTargetSettings"), TEXT("bEnableRayTracing"), bEnableRayTracing, GEngineIni);

		return bEnableRayTracing && TSuper::UsesRayTracing();
	}

	//~ End ITargetPlatform Interface

private:

	// Holds the local device.
	ITargetDevicePtr LocalDevice;

	TArray<ITargetDevicePtr> SteamDevices;

#if WITH_ENGINE
	// Holds the texture LOD settings.
	const UTextureLODSettings* TextureLODSettings;

	// Holds static mesh LOD settings.
	FStaticMeshLODSettings StaticMeshLODSettings;

	// True if the project requires encoded HDR reflection captures
	bool bRequiresEncodedHDRReflectionCaptures;

#endif // WITH_ENGINE

};

#undef LOCTEXT_NAMESPACE
