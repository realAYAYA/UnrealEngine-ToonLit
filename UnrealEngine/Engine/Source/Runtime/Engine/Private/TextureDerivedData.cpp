// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureDerivedData.cpp: Derived data management for textures.
=============================================================================*/

#include "CoreMinimal.h"
#include "Algo/AllOf.h"
#include "Misc/CommandLine.h"
#include "Stats/Stats.h"
#include "Async/AsyncWork.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Serialization/MemoryReader.h"
#include "UObject/Package.h"
#include "RenderUtils.h"
#include "TextureResource.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "Engine/Texture2DArray.h"
#include "Engine/TextureCubeArray.h"
#include "DeviceProfiles/DeviceProfile.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "TextureDerivedDataTask.h"
#include "Streaming/TextureStreamingHelpers.h"
#include "Engine/VolumeTexture.h"
#include "VT/VirtualTextureBuildSettings.h"
#include "VT/VirtualTextureBuiltData.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"

#if WITH_EDITOR

#include "ChildTextureFormat.h"
#include "ColorSpace.h"
#include "Compression/OodleDataCompressionUtil.h"
#include "DerivedDataCache.h"
#include "DerivedDataCacheInterface.h"
#include "DerivedDataRequestOwner.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/ITextureFormat.h"
#include "Misc/StringBuilder.h"
#include "ProfilingDebugging/CookStats.h"
#include "UObject/ArchiveCookContext.h"
#include "VT/VirtualTextureDataBuilder.h"
#include "VT/LightmapVirtualTexture.h"
#include "TextureCompiler.h"
#include "TextureEncodingSettings.h"


/*------------------------------------------------------------------------------
	Versioning for texture derived data.
------------------------------------------------------------------------------*/

// The current version string is set up to mimic the old versioning scheme and to make
// sure the DDC does not get invalidated right now. If you need to bump the version, replace it
// with a guid ( ex.: TEXT("855EE5B3574C43ABACC6700C4ADC62E6") )
// In case of merge conflicts with DDC versions, you MUST generate a new GUID and set this new
// guid as version
// this is put in the DDC1 and the DDC2 key

// next time this changes clean up SerializeForKey todo marks , search "@todo SerializeForKey"
#define TEXTURE_DERIVEDDATA_VER		TEXT("95BCE5A0BFB949539A18684748C633C9")

// This GUID is mixed into DDC version for virtual textures only, this allows updating DDC version for VT without invalidating DDC for all textures
// This is useful during development, but once large numbers of VT are present in shipped content, it will have the same problem as TEXTURE_DERIVEDDATA_VER
// This is put in the DDC1 key but NOT in the DDC2 key
#define TEXTURE_VT_DERIVEDDATA_VER	TEXT("7C16439390E24F1F9468894FB4D4BC54")


static bool IsUsingNewDerivedData()
{
	struct FTextureDerivedDataSetting
	{
		FTextureDerivedDataSetting()
		{
			bUseNewDerivedData = FParse::Param(FCommandLine::Get(), TEXT("DDC2AsyncTextureBuilds")) || FParse::Param(FCommandLine::Get(), TEXT("DDC2TextureBuilds"));
			if (!bUseNewDerivedData)
			{
				GConfig->GetBool(TEXT("TextureBuild"), TEXT("NewTextureBuilds"), bUseNewDerivedData, GEditorIni);
			}
			UE_CLOG(bUseNewDerivedData, LogTexture, Log, TEXT("Using new texture derived data builds."));
		}
		bool bUseNewDerivedData;
	};
	static const FTextureDerivedDataSetting TextureDerivedDataSetting;
	return TextureDerivedDataSetting.bUseNewDerivedData;
}


#if ENABLE_COOK_STATS
namespace TextureCookStats
{
	static FCookStats::FDDCResourceUsageStats UsageStats;
	static FCookStats::FDDCResourceUsageStats StreamingMipUsageStats;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("Texture.Usage"), TEXT("Inline"));
		StreamingMipUsageStats.LogStats(AddStat, TEXT("Texture.Usage"), TEXT("Streaming"));
	});
}
#endif

/*------------------------------------------------------------------------------
	Derived data key generation.
------------------------------------------------------------------------------*/

/**
 * Serialize build settings for use when generating the derived data key. (DDC1)
 * Must keep in sync with DDC2 Key WriteBuildSettings
 */
static void SerializeForKey(FArchive& Ar, const FTextureBuildSettings& Settings)
{
	uint32 TempUint32;
	float TempFloat;
	uint8 TempByte;
	FColor TempColor;
	FVector2f TempVector2f;
	FVector4f TempVector4f;
	UE::Color::FColorSpace TempColorSpace;
	FGuid TempGuid;

	TempFloat = Settings.ColorAdjustment.AdjustBrightness; Ar << TempFloat;
	TempFloat = Settings.ColorAdjustment.AdjustBrightnessCurve; Ar << TempFloat;
	TempFloat = Settings.ColorAdjustment.AdjustSaturation; Ar << TempFloat;
	TempFloat = Settings.ColorAdjustment.AdjustVibrance; Ar << TempFloat;
	TempFloat = Settings.ColorAdjustment.AdjustRGBCurve; Ar << TempFloat;
	TempFloat = Settings.ColorAdjustment.AdjustHue; Ar << TempFloat;
	TempFloat = Settings.ColorAdjustment.AdjustMinAlpha; Ar << TempFloat;
	TempFloat = Settings.ColorAdjustment.AdjustMaxAlpha; Ar << TempFloat;
	TempFloat = Settings.MipSharpening; Ar << TempFloat;
	TempUint32 = Settings.DiffuseConvolveMipLevel; Ar << TempUint32;
	TempUint32 = Settings.SharpenMipKernelSize; Ar << TempUint32;
	// NOTE: TextureFormatName is not stored in the key here.
	// NOTE: bHDRSource is not stored in the key here.
	TempByte = Settings.MipGenSettings; Ar << TempByte;
	TempByte = Settings.bCubemap; Ar << TempByte;
	TempByte = Settings.bTextureArray; Ar << TempByte;
	TempByte = Settings.bSRGB ? (Settings.bSRGB | ( Settings.bUseLegacyGamma ? 0 : 0x2 )) : 0; Ar << TempByte;

	if (Settings.SourceEncodingOverride != 0 /*UE::Color::EEncoding::None*/)
	{
		TempUint32 = UE::Color::ENCODING_TYPES_VER; Ar << TempUint32;
		TempByte = Settings.SourceEncodingOverride; Ar << TempByte;
	}

	if (Settings.bHasColorSpaceDefinition)
	{
		TempUint32 = UE::Color::COLORSPACE_VER; Ar << TempUint32;
		TempColorSpace = UE::Color::FColorSpace::GetWorking(); Ar << TempColorSpace;

		TempVector2f = FVector2f(Settings.RedChromaticityCoordinate); Ar << TempVector2f;
		TempVector2f = FVector2f(Settings.GreenChromaticityCoordinate); Ar << TempVector2f;
		TempVector2f = FVector2f(Settings.BlueChromaticityCoordinate); Ar << TempVector2f;
		TempVector2f = FVector2f(Settings.WhiteChromaticityCoordinate); Ar << TempVector2f;
		TempByte = Settings.ChromaticAdaptationMethod; Ar << TempByte;
	}

	TempByte = Settings.bPreserveBorder; Ar << TempByte;

	// bDitherMipMapAlpha was removed from Texture
	//  serialize to DDC as if it was still around and false to keep keys the same:
	uint8 bDitherMipMapAlpha = 0;
	TempByte = bDitherMipMapAlpha; Ar << TempByte;

	if (Settings.bDoScaleMipsForAlphaCoverage)
	{
		check( Settings.AlphaCoverageThresholds != FVector4f(0, 0, 0, 0) );
		TempVector4f = Settings.AlphaCoverageThresholds; Ar << TempVector4f;
	}
	
	TempByte = Settings.bComputeBokehAlpha; Ar << TempByte;
	TempByte = Settings.bReplicateRed; Ar << TempByte;
	TempByte = Settings.bReplicateAlpha; Ar << TempByte;
	TempByte = Settings.bDownsampleWithAverage; Ar << TempByte;
	
	{
		TempByte = Settings.bSharpenWithoutColorShift;

		if(Settings.bSharpenWithoutColorShift && Settings.MipSharpening != 0.0f)
		{
			// bSharpenWithoutColorShift prevented alpha sharpening. This got fixed
			// Here we update the key to get those cases recooked.
			TempByte = 2;
		}

		Ar << TempByte;
	}

	TempByte = Settings.bBorderColorBlack; Ar << TempByte;
	TempByte = Settings.bFlipGreenChannel; Ar << TempByte;
	TempByte = Settings.bApplyKernelToTopMip; Ar << TempByte;
	TempByte = Settings.CompositeTextureMode; Ar << TempByte;
	TempFloat = Settings.CompositePower; Ar << TempFloat;
	TempUint32 = Settings.MaxTextureResolution; Ar << TempUint32;
	TempByte = Settings.PowerOfTwoMode; Ar << TempByte;
	TempColor = Settings.PaddingColor; Ar << TempColor;
	TempByte = Settings.bChromaKeyTexture; Ar << TempByte;
	TempColor = Settings.ChromaKeyColor; Ar << TempColor;
	TempFloat = Settings.ChromaKeyThreshold; Ar << TempFloat;
	
	// Avoid changing key for non-VT enabled textures
	if (Settings.bVirtualStreamable)
	{
		TempByte = Settings.bVirtualStreamable; Ar << TempByte;
		TempByte = Settings.VirtualAddressingModeX; Ar << TempByte;
		TempByte = Settings.VirtualAddressingModeY; Ar << TempByte;
		TempUint32 = Settings.VirtualTextureTileSize; Ar << TempUint32;
		TempUint32 = Settings.VirtualTextureBorderSize; Ar << TempUint32;
		// compresion options removed: keep serializing them as "off" to keep the key the same:
		TempByte = 0; Ar << TempByte;
		TempByte = 0; Ar << TempByte;
		TempByte = Settings.LossyCompressionAmount; Ar << TempByte; // Lossy compression currently only used by VT
		TempByte = Settings.bApplyYCoCgBlockScale; Ar << TempByte; // YCoCg currently only used by VT

		
		// @todo SerializeForKey these can go away whenever we bump the overall ddc key:
		if ( Settings.bSRGB && Settings.bUseLegacyGamma )
		{
			// processing changed, modify ddc key :
			TempGuid = FGuid(0xA227BEFC,0x9F8643C6,0x81580369,0xC4C6F73E);
			Ar << TempGuid;
		}
	}

	// Avoid changing key if texture is not being downscaled
	if (Settings.Downscale > 1.0)
	{
		TempFloat = Settings.Downscale; Ar << TempFloat;
		TempByte = Settings.DownscaleOptions; Ar << TempByte;
	}

	// this is done in a funny way to add the bool that wasn't being serialized before
	//  without changing DDC keys where the bool is not set
	// @todo SerializeForKey these can go away whenever we bump the overall ddc key - just serialize the bool
	if (Settings.bForceAlphaChannel)
	{
		TempGuid = FGuid(0x2C9DF7E3, 0xBC9D413B, 0xBF963C7A, 0x3F27E8B1);
		Ar << TempGuid;
	}
	// fix - bForceNoAlphaChannel is not in key !
	// @todo SerializeForKey these can go away whenever we bump the overall ddc key - just serialize the bool
	if (Settings.bForceNoAlphaChannel)
	{
		TempGuid = FGuid(0x748fc0d4, 0x62004afa, 0x9530460a, 0xf8149d02);
		Ar << TempGuid;
	}

	if ( Settings.MaxTextureResolution != FTextureBuildSettings::MaxTextureResolutionDefault &&
		( Settings.MipGenSettings == TMGS_LeaveExistingMips || Settings.bDoScaleMipsForAlphaCoverage ) )
	{
		// @todo SerializeForKey these can go away whenever we bump the overall ddc key
		// behavior of MaxTextureResolution + LeaveExistingMips or bDoScaleMipsForAlphaCoverage changed, so modify the key :
		TempGuid = FGuid(0x418B8584, 0x72D54EA5, 0xBA8E8C2B, 0xECC880DE);
		Ar << TempGuid;
	}

	if ( Settings.bVolume )
	{
		// @todo SerializeForKey these can go away whenever we bump the overall ddc key
		TempGuid = FGuid(0xCC4348B8,0x84714993,0xAB1E2C93,0x8EA6C9E0);
		Ar << TempGuid;
	}

	if ( Settings.bVirtualStreamable && Settings.bSRGB && Settings.bUseLegacyGamma )
	{
		// @todo SerializeForKey these can go away whenever we bump the overall ddc key
		TempGuid = FGuid(0xCAEDDFB6,0xEDC2455D,0x8D45B90C,0x3A1B7783);
		Ar << TempGuid;
	}

	// do not change key if old mip filter is used for old textures
	// @todo SerializeForKey these can go away whenever we bump the overall ddc key
	if (Settings.bUseNewMipFilter)
	{
		TempGuid = FGuid(0x27B79A99, 0xE1A5458E, 0xAB619475, 0xCD01AD2A);
		Ar << TempGuid;
	}

	if (Settings.bLongLatSource)
	{
		// @todo SerializeForKey these can go away whenever we bump the overall ddc key
		// texture processing for cubemaps generated from longlat sources changed, so modify the key :
		TempGuid = FGuid(0x3D642836, 0xEBF64714, 0x9E8E3241, 0x39F66906);
		Ar << TempGuid;
	}

	if (Settings.CompressionCacheId.IsValid())
	{
		TempGuid = Settings.CompressionCacheId; Ar << TempGuid;
	}

	// Note - compression quality is added to the DDC by the formats (based on whether they
	// use them or not).
	// This is true for:
	//	LossyCompressionAmount
	//	CompressionQuality
	//	OodleEncodeEffort
	//	OodleUniversalTiling
	//  OodleTextureSdkVersion
}

/**
 * Computes the derived data key suffix for a texture with the specified compression settings.
 * @param Texture - The texture for which to compute the derived data key.
 * @param BuildSettings - Build settings for which to compute the derived data key.
 * @param OutKeySuffix - The derived data key suffix.
 */
void GetTextureDerivedDataKeySuffix(const UTexture& Texture, const FTextureBuildSettings* BuildSettingsPerLayer, FString& OutKeySuffix)
{
	uint16 Version = 0;

	// Build settings for layer0 (used by default)
	const FTextureBuildSettings& BuildSettings = BuildSettingsPerLayer[0];

	// get the version for this texture's platform format
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	const ITextureFormat* TextureFormat = NULL;
	if (TPM)
	{
		TextureFormat = TPM->FindTextureFormat(BuildSettings.TextureFormatName);
		if (TextureFormat)
		{
			Version = TextureFormat->GetVersion(BuildSettings.TextureFormatName, &BuildSettings);
		}
		// else error !?
	}
	// else error !?
	
	FString CompositeTextureStr;

	if(IsValid(Texture.CompositeTexture) && Texture.CompositeTextureMode != CTM_Disabled)
	{
		// CompositeTextureMode output changed so force a new DDC key value :
		CompositeTextureStr += TEXT("_Composite090802022_");
		CompositeTextureStr += Texture.CompositeTexture->Source.GetIdString();
	}

	// build the key, but don't use include the version if it's 0 to be backwards compatible
	OutKeySuffix = FString::Printf(TEXT("%s_%s%s%s_%02u_%s"),
		*BuildSettings.TextureFormatName.GetPlainNameString(),
		Version == 0 ? TEXT("") : *FString::Printf(TEXT("%d_"), Version),
		*Texture.Source.GetIdString(),
		*CompositeTextureStr,
		(uint32)NUM_INLINE_DERIVED_MIPS,
		(TextureFormat == NULL) ? TEXT("") : *TextureFormat->GetDerivedDataKeyString(BuildSettings)
		);

	// Add key data for extra layers beyond the first
	const int32 NumLayers = Texture.Source.GetNumLayers();
	for (int32 LayerIndex = 1; LayerIndex < NumLayers; ++LayerIndex)
	{
		const FTextureBuildSettings& LayerBuildSettings = BuildSettingsPerLayer[LayerIndex];
		const ITextureFormat* LayerTextureFormat = NULL;
		if (TPM)
		{
			LayerTextureFormat = TPM->FindTextureFormat(LayerBuildSettings.TextureFormatName);
		}

		uint16 LayerVersion = 0;
		if (LayerTextureFormat)
		{
			LayerVersion = LayerTextureFormat->GetVersion(LayerBuildSettings.TextureFormatName, &LayerBuildSettings);
		}
		OutKeySuffix.Append(FString::Printf(TEXT("%s%d%s_"),
			*LayerBuildSettings.TextureFormatName.GetPlainNameString(),
			LayerVersion,
			(LayerTextureFormat == NULL) ? TEXT("") : *LayerTextureFormat->GetDerivedDataKeyString(LayerBuildSettings)));
	}

	if (BuildSettings.bVirtualStreamable)
	{
		// Additional GUID for virtual textures, make it easier to force these to rebuild while developing
		OutKeySuffix.Append(FString::Printf(TEXT("VT%s_"), TEXTURE_VT_DERIVEDDATA_VER));
	}

#if PLATFORM_CPU_ARM_FAMILY
	// Separate out arm keys as x64 and arm64 clang do not generate the same data for a given
	// input. Add the arm specifically so that a) we avoid rebuilding the current DDC and
	// b) we can remove it once we get arm64 to be consistent.
	OutKeySuffix.Append(TEXT("_arm64"));
#endif

	// Serialize the compressor settings into a temporary array. The archive
	// is flagged as persistent so that machines of different endianness produce
	// identical binary results.
	TArray<uint8> TempBytes; 
	TempBytes.Reserve(64);
	FMemoryWriter Ar(TempBytes, /*bIsPersistent=*/ true);
	SerializeForKey(Ar, BuildSettings);

	for (int32 LayerIndex = 1; LayerIndex < NumLayers; ++LayerIndex)
	{
		const FTextureBuildSettings& LayerBuildSettings = BuildSettingsPerLayer[LayerIndex];
		SerializeForKey(Ar, LayerBuildSettings);
	}

	// Now convert the raw bytes to a string.
	const uint8* SettingsAsBytes = TempBytes.GetData();
	OutKeySuffix.Reserve(OutKeySuffix.Len() + TempBytes.Num());
	for (int32 ByteIndex = 0; ByteIndex < TempBytes.Num(); ++ByteIndex)
	{
		ByteToHex(SettingsAsBytes[ByteIndex], OutKeySuffix);
	}
}

/**
 * Returns the texture derived data version.
 */
const FGuid& GetTextureDerivedDataVersion()
{
	static FGuid Version(TEXTURE_DERIVEDDATA_VER);
	return Version;
}

/**
 * Constructs a derived data key from the key suffix.
 * @param KeySuffix - The key suffix.
 * @param OutKey - The full derived data key.
 */
void GetTextureDerivedDataKeyFromSuffix(const FString& KeySuffix, FString& OutKey)
{
	OutKey = FDerivedDataCacheInterface::BuildCacheKey(
		TEXT("TEXTURE"),
		TEXTURE_DERIVEDDATA_VER,
		*KeySuffix
		);
}

/**
 * Constructs the derived data key for an individual mip.
 * @param KeySuffix - The key suffix.
 * @param MipIndex - The mip index.
 * @param OutKey - The full derived data key for the mip.
 */
void GetTextureDerivedMipKey(
	int32 MipIndex,
	const FTexture2DMipMap& Mip,
	const FString& KeySuffix,
	FString& OutKey
	)
{
	OutKey = FDerivedDataCacheInterface::BuildCacheKey(
		TEXT("TEXTURE"),
		TEXTURE_DERIVEDDATA_VER,
		*FString::Printf(TEXT("%s_MIP%u_%dx%d"), *KeySuffix, MipIndex, Mip.SizeX, Mip.SizeY)
		);
}

/**
 * Computes the derived data key for a texture with the specified compression settings.
 * @param Texture - The texture for which to compute the derived data key.
 * @param BuildSettingsPerLayer - Array of FTextureBuildSettings (1 per layer) for which to compute the derived data key.
 * @param OutKey - The derived data key.
 */
static void GetTextureDerivedDataKey(
	const UTexture& Texture,
	const FTextureBuildSettings* BuildSettingsPerLayer,
	FString& OutKey
	)
{
	FString KeySuffix;
	GetTextureDerivedDataKeySuffix(Texture, BuildSettingsPerLayer, KeySuffix);
	GetTextureDerivedDataKeyFromSuffix(KeySuffix, OutKey);
}

#endif // #if WITH_EDITOR

/*------------------------------------------------------------------------------
	Texture compression.
------------------------------------------------------------------------------*/

#if WITH_EDITOR

struct FTextureEncodeSpeedOptions
{
	ETextureEncodeEffort Effort = ETextureEncodeEffort::Default;
	ETextureUniversalTiling Tiling = ETextureUniversalTiling::Disabled;
	bool bUsesRDO = false;
	uint8 RDOLambda = 30;
};

// InEncodeSpeed must be fast or final.
static void GetEncodeSpeedOptions(ETextureEncodeSpeed InEncodeSpeed, FTextureEncodeSpeedOptions* OutOptions)
{
	// We have to cache this because we are hitting the options on a worker thread, and it'll
	// crash if we use GetDefault while someone edits the project settings.
	// At the moment there's no guaranteed game thread place to do this as jobs can be kicked
	// off from worker threads (async encodes shader/light map).
	static struct ThreadSafeInitCSO
	{
		FTextureEncodeSpeedOptions Fast, Final;
		ThreadSafeInitCSO()
		{

			const UTextureEncodingProjectSettings* Settings = GetDefault<UTextureEncodingProjectSettings>();
			Fast.Effort = Settings->FastEffortLevel;
			Fast.Tiling = Settings->FastUniversalTiling;
			Fast.bUsesRDO = Settings->bFastUsesRDO;
			Fast.RDOLambda = Settings->FastRDOLambda;

			Final.Effort = Settings->FinalEffortLevel;
			Final.Tiling = Settings->FinalUniversalTiling;
			Final.bUsesRDO = Settings->bFinalUsesRDO;
			Final.RDOLambda = Settings->FinalRDOLambda;
			
			// log settings once at startup
			UEnum* EncodeEffortEnum = StaticEnum<ETextureEncodeEffort>();
			
			UEnum* UniversalTilingEnum = StaticEnum<ETextureUniversalTiling>();

			FString FastRDOString;
			if ( Fast.bUsesRDO )
			{
				FastRDOString = FString(TEXT("On"));
				if ( Fast.Tiling != ETextureUniversalTiling::Disabled )
				{
					FastRDOString += TEXT(" UT=");
					FastRDOString += UniversalTilingEnum->GetNameStringByValue((int64)Fast.Tiling);
				}
			}
			else
			{
				FastRDOString = FString(TEXT("Off"));
			}
			
			FString FinalRDOString;
			if ( Final.bUsesRDO )
			{
				FinalRDOString = FString(TEXT("On"));
				if ( Final.Tiling != ETextureUniversalTiling::Disabled )
				{
					FinalRDOString += TEXT(" UT=");
					FinalRDOString += UniversalTilingEnum->GetNameStringByValue((int64)Final.Tiling);
				}
			}
			else
			{
				FinalRDOString = FString(TEXT("Off"));
			}

			UE_LOG(LogTexture, Display, TEXT("Oodle Texture Encode Speed settings: Fast: RDO %s Lambda=%d, Effort=%s Final: RDO %s Lambda=%d, Effort=%s"), \
				*FastRDOString, Fast.bUsesRDO ? Fast.RDOLambda : 0,  *(EncodeEffortEnum->GetNameStringByValue((int64)Fast.Effort)), \
				*FinalRDOString, Final.bUsesRDO ? Final.RDOLambda : 0,  *(EncodeEffortEnum->GetNameStringByValue((int64)Final.Effort)) );


		}
	} EncodeSpeedOptions;

	if (InEncodeSpeed == ETextureEncodeSpeed::Final)
	{
		*OutOptions = EncodeSpeedOptions.Final;
	}
	else
	{
		*OutOptions = EncodeSpeedOptions.Fast;
	}
}


// Convert the baseline build settings for all layers to one for the given layer.
// Note this gets called twice for layer 0, so needs to be idempotent.
static void FinalizeBuildSettingsForLayer(
	const UTexture& Texture, 
	int32 LayerIndex, 
	const ITargetPlatform* TargetPlatform, 
	ETextureEncodeSpeed InEncodeSpeed, // must be Final or Fast.
	FTextureBuildSettings& OutSettings,
	FTexturePlatformData::FTextureEncodeResultMetadata* OutBuildResultMetadata // can be nullptr if not needed
	)
{
	FTextureFormatSettings FormatSettings;
	Texture.GetLayerFormatSettings(LayerIndex, FormatSettings);

	OutSettings.bHDRSource = Texture.HasHDRSource(LayerIndex);
	OutSettings.bSRGB = FormatSettings.SRGB;
	OutSettings.bForceNoAlphaChannel = FormatSettings.CompressionNoAlpha;
	OutSettings.bForceAlphaChannel = FormatSettings.CompressionForceAlpha;
	OutSettings.bApplyYCoCgBlockScale = FormatSettings.CompressionYCoCg;

	if (FormatSettings.CompressionSettings == TC_Displacementmap || FormatSettings.CompressionSettings == TC_DistanceFieldFont)
	{
		OutSettings.bReplicateAlpha = true;
	}
	else if (FormatSettings.CompressionSettings == TC_Grayscale || FormatSettings.CompressionSettings == TC_Alpha)
	{
		OutSettings.bReplicateRed = true;
	}

	if (OutSettings.bVirtualStreamable)
	{
		OutSettings.TextureFormatName = TargetPlatform->FinalizeVirtualTextureLayerFormat(OutSettings.TextureFormatName);
	}

	// Now that we know the texture format, we can make decisions based on it.

	bool bSupportsEncodeSpeed = false;
	{
		ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
		if (TPM)
		{
			// Can be null with first finalize (at the end of GetTextureBuildSettings)
			const ITextureFormat* TextureFormat = TPM->FindTextureFormat(OutSettings.TextureFormatName);
			if (TextureFormat)
			{
				bSupportsEncodeSpeed = TextureFormat->SupportsEncodeSpeed(OutSettings.TextureFormatName);

				if (OutBuildResultMetadata)
				{
					OutBuildResultMetadata->Encoder = TextureFormat->GetEncoderName(OutSettings.TextureFormatName);
					OutBuildResultMetadata->bIsValid = true;
					OutBuildResultMetadata->bSupportsEncodeSpeed = bSupportsEncodeSpeed;
				}
			
				
				{
					static auto CVarSharedLinearTextureEncoding = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.SharedLinearTextureEncoding"));
					if (CVarSharedLinearTextureEncoding->GetValueOnAnyThread())
					{
						// Shared linear encoding can only work if the base texture format does not expect to
						// do the tiling itself (SupportsTiling == false).
						const FChildTextureFormat* ChildTextureFormat = TextureFormat->GetChildFormat();
						if (ChildTextureFormat && ChildTextureFormat->GetBaseFormatObject(OutSettings.TextureFormatName)->SupportsTiling() == false)
						{
							OutSettings.Tiler = ChildTextureFormat->GetTiler();
							if (OutSettings.Tiler && IsUsingNewDerivedData())
							{
								// New derived data wants to treat everything as the base format and then have a separate tiling function
								// afterwards.
								OutSettings.TextureFormatName = ChildTextureFormat->GetBaseFormatName(OutSettings.TextureFormatName);
							}
						}
					} // end if enabled
				} // end if ddc2
			} // end if texture format found.
		}
	}

	if (bSupportsEncodeSpeed)
	{
		FTextureEncodeSpeedOptions Options;
		GetEncodeSpeedOptions(InEncodeSpeed, &Options);

		// Always pass effort and tiling.
		OutSettings.OodleEncodeEffort = (uint8)Options.Effort;
		OutSettings.OodleUniversalTiling = (uint8)Options.Tiling;

		// LCA has no effect if disabled, and only override if not default.
		OutSettings.bOodleUsesRDO = Options.bUsesRDO;
		if (Options.bUsesRDO)
		{
			// If this mapping changes, update the tooltip in TextureEncodingSettings.h
			// this is an ETextureLossyCompressionAmount
			switch (OutSettings.LossyCompressionAmount)
			{
			default:
			case TLCA_Default: 
				{
					if (OutBuildResultMetadata)
					{
						OutBuildResultMetadata->RDOSource = FTexturePlatformData::FTextureEncodeResultMetadata::OodleRDOSource::Default;
					}
					OutSettings.OodleRDO = Options.RDOLambda; 
					break; // Use global defaults.
				}
			case TLCA_None:    OutSettings.OodleRDO = 0; break;		// "No lossy compression"
			case TLCA_Lowest:  OutSettings.OodleRDO = 1; break;		// "Lowest (Best Image quality, largest filesize)"
			case TLCA_Low:     OutSettings.OodleRDO = 10; break;	// "Low"
			case TLCA_Medium:  OutSettings.OodleRDO = 20; break;	// "Medium"
			case TLCA_High:    OutSettings.OodleRDO = 30; break;	// "High"
			case TLCA_Highest: OutSettings.OodleRDO = 40; break;	// "Highest (Worst Image quality, smallest filesize)"
			}
		}
		else
		{
			OutSettings.OodleRDO = 0;
		}

		if (OutBuildResultMetadata)
		{
			OutBuildResultMetadata->OodleRDO = OutSettings.OodleRDO;
			OutBuildResultMetadata->OodleEncodeEffort = OutSettings.OodleEncodeEffort;
			OutBuildResultMetadata->OodleUniversalTiling = OutSettings.OodleUniversalTiling;
		}
	}
}

ENGINE_API ETextureEncodeSpeed UTexture::GetDesiredEncodeSpeed() const
{
	if ( CompressFinal )
	{
		return ETextureEncodeSpeed::Final;
	}

	// Get the command line and config options with a one-time static init :
	static struct FThreadSafeInitializer
	{
		ETextureEncodeSpeed CachedEncodeSpeedOption;

		// For thread safety, we do all the work in a constructor that the compiler
		// guarantees will be called only once.
		FThreadSafeInitializer()
		{
			const UEnum* EncodeSpeedEnum = StaticEnum<ETextureEncodeSpeed>();

			// Overridden by command line?
			FString CmdLineSpeed;
			if (FParse::Value(FCommandLine::Get(), TEXT("-ForceTextureEncodeSpeed="), CmdLineSpeed))
			{
				int64 Value = EncodeSpeedEnum->GetValueByNameString(CmdLineSpeed);
				if (Value == INDEX_NONE)
				{
					UE_LOG(LogTexture, Error, TEXT("Invalid value for ForceTextureEncodeSpeed, ignoring. Valid values are the ETextureEncodeSpeed enum (Final, FinalIfAvailable, Fast)"));
				}
				else
				{
					CachedEncodeSpeedOption = (ETextureEncodeSpeed)Value;
					UE_LOG(LogTexture, Display, TEXT("Texture Encode Speed forced to %s via command line."), *EncodeSpeedEnum->GetNameStringByValue(Value));
					return;
				}
			}

			// Overridden by user settings?
			const UTextureEncodingUserSettings* UserSettings = GetDefault<UTextureEncodingUserSettings>();
			if (UserSettings->ForceEncodeSpeed != ETextureEncodeSpeedOverride::Disabled)
			{
				// enums have same values for payload.
				CachedEncodeSpeedOption = (ETextureEncodeSpeed)UserSettings->ForceEncodeSpeed;
				UE_LOG(LogTexture, Display, TEXT("Texture Encode Speed forced to %s via user settings."), *EncodeSpeedEnum->GetNameStringByValue((int64)CachedEncodeSpeedOption));
				return;
			}

			// Use project settings
			const UTextureEncodingProjectSettings* Settings = GetDefault<UTextureEncodingProjectSettings>();
			if (GIsEditor && !IsRunningCommandlet())
			{
				// Interactive editor
				CachedEncodeSpeedOption = Settings->EditorUsesSpeed;
				UE_LOG(LogTexture, Display, TEXT("Texture Encode Speed: %s (editor)."), *EncodeSpeedEnum->GetNameStringByValue((int64)CachedEncodeSpeedOption));
			}
			else
			{
				CachedEncodeSpeedOption = Settings->CookUsesSpeed;
				UE_LOG(LogTexture, Display, TEXT("Texture Encode Speed: %s (cook)."), *EncodeSpeedEnum->GetNameStringByValue((int64)CachedEncodeSpeedOption));
			}
			
		}
	} FThreadSafeInitializer;

	return FThreadSafeInitializer.CachedEncodeSpeedOption;
}


static FName ConditionalRemapOodleTextureSdkVersion(FName InOodleTextureSdkVersion, const ITargetPlatform* TargetPlatform)
{
#if WITH_EDITOR

	// optionally remap InOodleTextureSdkVersion

	if ( InOodleTextureSdkVersion.IsNone() )
	{
		//	new (optional) pref : OodleTextureSdkVersionToUseIfNone

		FString OodleTextureSdkVersionToUseIfNone;
		if ( TargetPlatform->GetConfigSystem()->GetString(TEXT("AlternateTextureCompression"), TEXT("OodleTextureSdkVersionToUseIfNone"), OodleTextureSdkVersionToUseIfNone, GEngineIni) )
		{
			return FName(OodleTextureSdkVersionToUseIfNone);
		}
	}

	// @todo Oodle : possibly also remap non-none versions
	//	so you could set up mapping tables like "if it was 2.9.4, now use 2.9.6"

#endif

	return InOodleTextureSdkVersion;
}

/**
 * Sets texture build settings.
 * @param Texture - The texture for which to build compressor settings.
 * @param OutBuildSettings - Build settings.
 * 
 * This function creates the build settings that are shared across all layers - you can not
 * assume a texture format at this time (See FinalizeBuildSettingsForLayer)
 */
static void GetTextureBuildSettings(
	const UTexture& Texture,
	const UTextureLODSettings& TextureLODSettings,
	const ITargetPlatform& TargetPlatform,
	ETextureEncodeSpeed InEncodeSpeed, // must be Final or Fast.
	FTextureBuildSettings& OutBuildSettings,
	FTexturePlatformData::FTextureEncodeResultMetadata* OutBuildResultMetadata // can be nullptr if not needed
	)
{
	const bool bPlatformSupportsTextureStreaming = TargetPlatform.SupportsFeature(ETargetPlatformFeatures::TextureStreaming);
	const bool bPlatformSupportsVirtualTextureStreaming = TargetPlatform.SupportsFeature(ETargetPlatformFeatures::VirtualTextureStreaming);

	if (OutBuildResultMetadata)
	{
		OutBuildResultMetadata->EncodeSpeed = (uint8)InEncodeSpeed;
	}
	OutBuildSettings.RepresentsEncodeSpeedNoSend = (uint8)InEncodeSpeed;

	OutBuildSettings.ColorAdjustment.AdjustBrightness = Texture.AdjustBrightness;
	OutBuildSettings.ColorAdjustment.AdjustBrightnessCurve = Texture.AdjustBrightnessCurve;
	OutBuildSettings.ColorAdjustment.AdjustVibrance = Texture.AdjustVibrance;
	OutBuildSettings.ColorAdjustment.AdjustSaturation = Texture.AdjustSaturation;
	OutBuildSettings.ColorAdjustment.AdjustRGBCurve = Texture.AdjustRGBCurve;
	OutBuildSettings.ColorAdjustment.AdjustHue = Texture.AdjustHue;
	OutBuildSettings.ColorAdjustment.AdjustMinAlpha = Texture.AdjustMinAlpha;
	OutBuildSettings.ColorAdjustment.AdjustMaxAlpha = Texture.AdjustMaxAlpha;
	OutBuildSettings.bUseLegacyGamma = Texture.bUseLegacyGamma;
	OutBuildSettings.bPreserveBorder = Texture.bPreserveBorder;

	// in Texture , the fields bDoScaleMipsForAlphaCoverage and AlphaCoverageThresholds are independent
	// but in the BuildSettings bDoScaleMipsForAlphaCoverage is only on if thresholds are valid (not all zero)
	if ( Texture.bDoScaleMipsForAlphaCoverage && Texture.AlphaCoverageThresholds != FVector4(0,0,0,0) )
	{
		OutBuildSettings.bDoScaleMipsForAlphaCoverage = Texture.bDoScaleMipsForAlphaCoverage;
		OutBuildSettings.AlphaCoverageThresholds = (FVector4f)Texture.AlphaCoverageThresholds;
	}
	else
	{
		OutBuildSettings.bDoScaleMipsForAlphaCoverage = false;
		OutBuildSettings.AlphaCoverageThresholds = FVector4f(0,0,0,0);
	}

	OutBuildSettings.CompressionCacheId = Texture.CompressionCacheId;
	OutBuildSettings.bUseNewMipFilter = Texture.bUseNewMipFilter;
	OutBuildSettings.bComputeBokehAlpha = (Texture.LODGroup == TEXTUREGROUP_Bokeh);
	OutBuildSettings.bReplicateAlpha = false;
	OutBuildSettings.bReplicateRed = false;
	OutBuildSettings.bVolume = false;
	OutBuildSettings.bCubemap = false;
	OutBuildSettings.bTextureArray = false;
	OutBuildSettings.DiffuseConvolveMipLevel = 0;
	OutBuildSettings.bLongLatSource = false;
	OutBuildSettings.SourceEncodingOverride = static_cast<uint8>(Texture.SourceColorSettings.EncodingOverride);
	OutBuildSettings.bHasColorSpaceDefinition = Texture.SourceColorSettings.ColorSpace != ETextureColorSpace::TCS_None;
	OutBuildSettings.RedChromaticityCoordinate = FVector2f(Texture.SourceColorSettings.RedChromaticityCoordinate);
	OutBuildSettings.GreenChromaticityCoordinate = FVector2f(Texture.SourceColorSettings.GreenChromaticityCoordinate);
	OutBuildSettings.BlueChromaticityCoordinate = FVector2f(Texture.SourceColorSettings.BlueChromaticityCoordinate);
	OutBuildSettings.WhiteChromaticityCoordinate = FVector2f(Texture.SourceColorSettings.WhiteChromaticityCoordinate);
	OutBuildSettings.ChromaticAdaptationMethod = static_cast<uint8>(Texture.SourceColorSettings.ChromaticAdaptationMethod);
	
	check( OutBuildSettings.MaxTextureResolution == FTextureBuildSettings::MaxTextureResolutionDefault );
	if (Texture.MaxTextureSize > 0)
	{
		OutBuildSettings.MaxTextureResolution = Texture.MaxTextureSize;
	}

	ETextureClass TextureClass = Texture.GetTextureClass();
	
	if ( TextureClass == ETextureClass::TwoD )
	{
		// nada
	}
	else if ( TextureClass == ETextureClass::Cube )
	{
		OutBuildSettings.bCubemap = true;
		OutBuildSettings.DiffuseConvolveMipLevel = GDiffuseConvolveMipLevel;
		check( Texture.Source.GetNumSlices() == 1 || Texture.Source.GetNumSlices() == 6 );
		OutBuildSettings.bLongLatSource = Texture.Source.IsLongLatCubemap();
	}
	else if ( TextureClass == ETextureClass::Array )
	{
		OutBuildSettings.bTextureArray = true;
	}
	else if ( TextureClass == ETextureClass::CubeArray )
	{
		OutBuildSettings.bCubemap = true;
		OutBuildSettings.bTextureArray = true;
		// beware IsLongLatCubemap
		// ambiguous with longlat cube arrays with multiple of 6 array size
		OutBuildSettings.bLongLatSource = Texture.Source.IsLongLatCubemap();
		check( ((Texture.Source.GetNumSlices()%6)==0) || OutBuildSettings.bLongLatSource );
	}
	else if ( TextureClass == ETextureClass::Volume )
	{
		OutBuildSettings.bVolume = true;
	}
	else if ( TextureClass == ETextureClass::TwoDDynamic ||
		TextureClass == ETextureClass::Other2DNoSource )
	{
		UE_LOG(LogTexture, Warning, TEXT("Unexpected texture build for dynamic texture? (%s)"),*Texture.GetName());
	}
	else
	{
		// unknown TextureType ?
		UE_LOG(LogTexture, Error, TEXT("Unexpected texture build for unknown texture class? (%s)"),*Texture.GetName());
	}

	bool bDownsampleWithAverage;
	bool bSharpenWithoutColorShift;
	bool bBorderColorBlack;
	TextureMipGenSettings MipGenSettings;
	TextureLODSettings.GetMipGenSettings( 
		Texture,
		MipGenSettings,
		OutBuildSettings.MipSharpening,
		OutBuildSettings.SharpenMipKernelSize,
		bDownsampleWithAverage,
		bSharpenWithoutColorShift,
		bBorderColorBlack
		);

	static const auto CVarVirtualTexturesEnabled = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.VirtualTextures")); check(CVarVirtualTexturesEnabled);
	// A ULightMapVirtualTexture2D with multiple layers saved in MapBuildData could be loaded with the r.VirtualTexture disabled, it will generate DDC before we decide to invalidate the light map data, to skip the ensure failure let it generate VT DDC anyway.
	const bool bForVirtualTextureStreamingBuild = ULightMapVirtualTexture2D::StaticClass() == Texture.GetClass();
	const bool bVirtualTextureStreaming = bForVirtualTextureStreamingBuild || (CVarVirtualTexturesEnabled->GetValueOnAnyThread() && bPlatformSupportsVirtualTextureStreaming && Texture.VirtualTextureStreaming);
	const FIntPoint SourceSize = Texture.Source.GetLogicalSize();

	OutBuildSettings.MipGenSettings = MipGenSettings;
	OutBuildSettings.bDownsampleWithAverage = bDownsampleWithAverage;
	OutBuildSettings.bSharpenWithoutColorShift = bSharpenWithoutColorShift;
	OutBuildSettings.bBorderColorBlack = bBorderColorBlack;
	OutBuildSettings.bFlipGreenChannel = Texture.bFlipGreenChannel;
	OutBuildSettings.CompositeTextureMode = Texture.CompositeTextureMode;
	OutBuildSettings.CompositePower = Texture.CompositePower;
	OutBuildSettings.LODBias = TextureLODSettings.CalculateLODBias(SourceSize.X, SourceSize.Y, Texture.MaxTextureSize, Texture.LODGroup, Texture.LODBias, Texture.NumCinematicMipLevels, Texture.MipGenSettings, bVirtualTextureStreaming);
	OutBuildSettings.LODBiasWithCinematicMips = TextureLODSettings.CalculateLODBias(SourceSize.X, SourceSize.Y, Texture.MaxTextureSize, Texture.LODGroup, Texture.LODBias, 0, Texture.MipGenSettings, bVirtualTextureStreaming);
	OutBuildSettings.bVirtualStreamable = bVirtualTextureStreaming;
	OutBuildSettings.PowerOfTwoMode = Texture.PowerOfTwoMode;
	OutBuildSettings.PaddingColor = Texture.PaddingColor;
	OutBuildSettings.ChromaKeyColor = Texture.ChromaKeyColor;
	OutBuildSettings.bChromaKeyTexture = Texture.bChromaKeyTexture;
	OutBuildSettings.ChromaKeyThreshold = Texture.ChromaKeyThreshold;
	OutBuildSettings.CompressionQuality = Texture.CompressionQuality - 1; // translate from enum's 0 .. 5 to desired compression (-1 .. 4, where -1 is default while 0 .. 4 are actual quality setting override)
	
	// do remap here before we send to TBW's which may not have access to config :
	OutBuildSettings.OodleTextureSdkVersion = ConditionalRemapOodleTextureSdkVersion(Texture.OodleTextureSdkVersion,&TargetPlatform);

	// if LossyCompressionAmount is Default, inherit from LODGroup :
	if ( Texture.LossyCompressionAmount == TLCA_Default )
	{
		const FTextureLODGroup& LODGroup = TextureLODSettings.GetTextureLODGroup(Texture.LODGroup);
		OutBuildSettings.LossyCompressionAmount = LODGroup.LossyCompressionAmount;
		if (OutBuildResultMetadata)
		{
			OutBuildResultMetadata->RDOSource = FTexturePlatformData::FTextureEncodeResultMetadata::OodleRDOSource::LODGroup;
		}
	}
	else
	{
		OutBuildSettings.LossyCompressionAmount = Texture.LossyCompressionAmount.GetValue();
		if (OutBuildResultMetadata)
		{
			OutBuildResultMetadata->RDOSource = FTexturePlatformData::FTextureEncodeResultMetadata::OodleRDOSource::Texture;
		}
	}

	OutBuildSettings.Downscale = 1.0f;
	// Downscale only allowed if NoMipMaps, 2d, and not VT
	//	silently does nothing otherwise
	if (! bVirtualTextureStreaming &&
		MipGenSettings == TMGS_NoMipmaps && 
		Texture.IsA(UTexture2D::StaticClass()))	// TODO: support more texture types
	{
		TextureLODSettings.GetDownscaleOptions(Texture, TargetPlatform, OutBuildSettings.Downscale, (ETextureDownscaleOptions&)OutBuildSettings.DownscaleOptions);
	}
	
	// For virtual texturing we take the address mode into consideration
	if (OutBuildSettings.bVirtualStreamable)
	{
		const UTexture2D *Texture2D = Cast<UTexture2D>(&Texture);
		checkf(Texture2D, TEXT("Virtual texturing is only supported on 2D textures"));
		if (Texture.Source.GetNumBlocks() > 1)
		{
			// Multi-block textures (UDIM) interpret UVs outside [0,1) range as different blocks, so wrapping within a given block doesn't make sense
			// We want to make sure address mode is set to clamp here, otherwise border pixels along block edges will have artifacts
			OutBuildSettings.VirtualAddressingModeX = TA_Clamp;
			OutBuildSettings.VirtualAddressingModeY = TA_Clamp;
		}
		else
		{
			OutBuildSettings.VirtualAddressingModeX = Texture2D->AddressX;
			OutBuildSettings.VirtualAddressingModeY = Texture2D->AddressY;
		}

		FVirtualTextureBuildSettings VirtualTextureBuildSettings;
		Texture.GetVirtualTextureBuildSettings(VirtualTextureBuildSettings);
		OutBuildSettings.VirtualTextureTileSize = FMath::RoundUpToPowerOfTwo(VirtualTextureBuildSettings.TileSize);

		// Apply any LOD group tile size bias here
		const int32 TileSizeBias = TextureLODSettings.GetTextureLODGroup(Texture.LODGroup).VirtualTextureTileSizeBias;
		OutBuildSettings.VirtualTextureTileSize >>= (TileSizeBias < 0) ? -TileSizeBias : 0;
		OutBuildSettings.VirtualTextureTileSize <<= (TileSizeBias > 0) ? TileSizeBias : 0;

		// Don't allow max resolution to be less than VT tile size
		OutBuildSettings.MaxTextureResolution = FMath::Max<uint32>(OutBuildSettings.MaxTextureResolution, OutBuildSettings.VirtualTextureTileSize);

		// 0 is a valid value for border size
		// 1 would be OK in some cases, but breaks BC compressed formats, since it will result in physical tiles that aren't divisible by block size (4)
		// Could allow border size of 1 for non BC compressed virtual textures, but somewhat complicated to get that correct, especially with multiple layers
		// Doesn't seem worth the complexity for now, so clamp the size to be at least 2
		OutBuildSettings.VirtualTextureBorderSize = (VirtualTextureBuildSettings.TileBorderSize > 0) ? FMath::RoundUpToPowerOfTwo(FMath::Max(VirtualTextureBuildSettings.TileBorderSize, 2)) : 0;
	}
	else
	{
		OutBuildSettings.VirtualAddressingModeX = TA_Wrap;
		OutBuildSettings.VirtualAddressingModeY = TA_Wrap;
		OutBuildSettings.VirtualTextureTileSize = 0;
		OutBuildSettings.VirtualTextureBorderSize = 0;
	}

	// By default, initialize settings for layer0
	FinalizeBuildSettingsForLayer(Texture, 0, &TargetPlatform, InEncodeSpeed, OutBuildSettings, OutBuildResultMetadata);
}

/**
 * Sets build settings for a texture on the current running platform
 * @param Texture - The texture for which to build compressor settings.
 * @param OutBuildSettings - Array of desired texture settings
 */
static void GetBuildSettingsForRunningPlatform(
	const UTexture& Texture,
	ETextureEncodeSpeed InEncodeSpeed, //  must be Fast or Final
	TArray<FTextureBuildSettings>& OutSettingPerLayer,
	TArray<FTexturePlatformData::FTextureEncodeResultMetadata>* OutResultMetadataPerLayer // can be nullptr if not needed
	)
{
	// Compress to whatever formats the active target platforms want
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	if (TPM)
	{
		ITargetPlatform* TargetPlatform = NULL;
		const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();

		check(Platforms.Num());

		TargetPlatform = Platforms[0];

		for (int32 Index = 1; Index < Platforms.Num(); Index++)
		{
			if (Platforms[Index]->IsRunningPlatform())
			{
				TargetPlatform = Platforms[Index];
				break;
			}
		}

		check(TargetPlatform != NULL);

		const UTextureLODSettings* LODSettings = (UTextureLODSettings*)UDeviceProfileManager::Get().FindProfile(TargetPlatform->PlatformName());
		FTextureBuildSettings SourceBuildSettings;
		FTexturePlatformData::FTextureEncodeResultMetadata SourceMetadata;
		GetTextureBuildSettings(Texture, *LODSettings, *TargetPlatform, InEncodeSpeed, SourceBuildSettings, &SourceMetadata);

		TArray< TArray<FName> > PlatformFormats;
		TargetPlatform->GetTextureFormats(&Texture, PlatformFormats);
		check(PlatformFormats.Num() > 0);

		const int32 NumLayers = Texture.Source.GetNumLayers();
		check(PlatformFormats[0].Num() == NumLayers);

		OutSettingPerLayer.Reserve(NumLayers);
		if (OutResultMetadataPerLayer)
		{
			OutResultMetadataPerLayer->Reserve(NumLayers);
		}
		for (int32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
		{
			FTextureBuildSettings& OutSettings = OutSettingPerLayer.Add_GetRef(SourceBuildSettings);
			OutSettings.TextureFormatName = PlatformFormats[0][LayerIndex];

			FTexturePlatformData::FTextureEncodeResultMetadata* OutMetadata = nullptr;
			if (OutResultMetadataPerLayer)
			{
				OutMetadata = &OutResultMetadataPerLayer->Add_GetRef(SourceMetadata);
			}
			
			FinalizeBuildSettingsForLayer(Texture, LayerIndex, TargetPlatform, InEncodeSpeed, OutSettings, OutMetadata);
		}
	}
}

static void GetBuildSettingsPerFormat(
	const UTexture& Texture, 
	const FTextureBuildSettings& SourceBuildSettings, 
	const FTexturePlatformData::FTextureEncodeResultMetadata* SourceResultMetadata, // can be nullptr if not capturing metadata
	const ITargetPlatform* TargetPlatform, 
	ETextureEncodeSpeed InEncodeSpeed, //  must be Fast or Final
	TArray< TArray<FTextureBuildSettings> >& OutBuildSettingsPerFormat,
	TArray< TArray<FTexturePlatformData::FTextureEncodeResultMetadata> >* OutResultMetadataPerFormat // can be nullptr if not capturing metadata
	)
{
	const int32 NumLayers = Texture.Source.GetNumLayers();

	TArray< TArray<FName> > PlatformFormats;
	TargetPlatform->GetTextureFormats(&Texture, PlatformFormats);

	OutBuildSettingsPerFormat.Reserve(PlatformFormats.Num());
	if (OutResultMetadataPerFormat)
	{
		OutResultMetadataPerFormat->Reserve(PlatformFormats.Num());
	}
	for (TArray<FName>& PlatformFormatsPerLayer : PlatformFormats)
	{
		check(PlatformFormatsPerLayer.Num() == NumLayers);
		TArray<FTextureBuildSettings>& OutSettingPerLayer = OutBuildSettingsPerFormat.AddDefaulted_GetRef();
		OutSettingPerLayer.Reserve(NumLayers);

		TArray<FTexturePlatformData::FTextureEncodeResultMetadata>* OutResultMetadataPerLayer = nullptr;
		if (OutResultMetadataPerFormat)
		{
			OutResultMetadataPerLayer = &OutResultMetadataPerFormat->AddDefaulted_GetRef();
			OutResultMetadataPerLayer->Reserve(NumLayers);
		}

		for (int32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
		{
			FTextureBuildSettings& OutSettings = OutSettingPerLayer.Add_GetRef(SourceBuildSettings);
			OutSettings.TextureFormatName = PlatformFormatsPerLayer[LayerIndex];

			FTexturePlatformData::FTextureEncodeResultMetadata* OutResultMetadata = nullptr;
			if (OutResultMetadataPerLayer)
			{
				OutResultMetadata = &OutResultMetadataPerLayer->Add_GetRef(*SourceResultMetadata);			
			}
			FinalizeBuildSettingsForLayer(Texture, LayerIndex, TargetPlatform, InEncodeSpeed, OutSettings, OutResultMetadata);
		}
	}
}

/**
 * Stores derived data in the DDC.
 * After this returns, all bulk data from streaming (non-inline) mips will be sent separately to the DDC and the BulkData for those mips removed.
 * @param DerivedData - The data to store in the DDC.
 * @param DerivedDataKeySuffix - The key suffix at which to store derived data.
 * @param bForceAllMipsToBeInlined - Whether to store all mips in the main DDC. Relates to how the texture resources get initialized (not supporting streaming).
 * @return number of bytes put to the DDC (total, including all mips)
 */
int64 PutDerivedDataInCache(FTexturePlatformData* DerivedData, const FString& DerivedDataKeySuffix, const FStringView& TextureName, bool bForceAllMipsToBeInlined, bool bReplaceExistingDDC)
{
	TArray64<uint8> RawDerivedData;
	FString DerivedDataKey;
	int64 TotalBytesPut = 0;

	// Build the key with which to cache derived data.
	GetTextureDerivedDataKeyFromSuffix(DerivedDataKeySuffix, DerivedDataKey);

	FString LogString;

	// Write out individual mips to the derived data cache.
	const int32 MipCount = DerivedData->Mips.Num();
	const int32 FirstInlineMip = bForceAllMipsToBeInlined ? 0 : FMath::Max(0, MipCount - FMath::Max((int32)NUM_INLINE_DERIVED_MIPS, (int32)DerivedData->GetNumMipsInTail()));
	const int32 WritableMipCount = MipCount - ((DerivedData->GetNumMipsInTail() > 0) ? (DerivedData->GetNumMipsInTail() - 1) : 0);
	for (int32 MipIndex = 0; MipIndex < WritableMipCount; ++MipIndex)
	{
		FString MipDerivedDataKey;
		FTexture2DMipMap& Mip = DerivedData->Mips[MipIndex];
		const bool bInline = (MipIndex >= FirstInlineMip);
		GetTextureDerivedMipKey(MipIndex, Mip, DerivedDataKeySuffix, MipDerivedDataKey);

		const bool bDDCError = !bInline && !Mip.BulkData.GetBulkDataSize();
		if (UE_LOG_ACTIVE(LogTexture,Verbose) || bDDCError)
		{
			if (LogString.IsEmpty())
			{
				LogString = FString::Printf(
					TEXT("Storing texture in DDC:\n  Name: %s\n  Key: %s\n  Format: %s\n"),
					*FString(TextureName),
					*DerivedDataKey,
					GPixelFormats[DerivedData->PixelFormat].Name
				);
			}

			LogString += FString::Printf(TEXT("  Mip%d %dx%d %d bytes%s %s\n"),
				MipIndex,
				Mip.SizeX,
				Mip.SizeY,
				Mip.BulkData.GetBulkDataSize(),
				bInline ? TEXT(" [inline]") : TEXT(""),
				*MipDerivedDataKey
				);
		}

		if (bDDCError)
		{
			UE_LOG(LogTexture, Fatal, TEXT("Error %s"), *LogString);
		}

		// Note that calling StoreInDerivedDataCache() also calls RemoveBulkData().
		// This means that the resource needs to load differently inlined mips and non inlined mips.
		if (!bInline)
		{
			// store in the DDC, also drop the bulk data storage.
			TotalBytesPut += Mip.StoreInDerivedDataCache(MipDerivedDataKey, TextureName, bReplaceExistingDDC);
		}
	}

	// Write out each VT chunk to the DDC
	bool bReplaceExistingDerivedDataDDC = bReplaceExistingDDC;
	if (DerivedData->VTData)
	{
		const int32 ChunkCount = DerivedData->VTData->Chunks.Num();
		for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
		{
			FVirtualTextureDataChunk& Chunk = DerivedData->VTData->Chunks[ChunkIndex];

			const FString ChunkDerivedDataKey = FDerivedDataCacheInterface::BuildCacheKey(
				TEXT("TEXTURE"), TEXTURE_VT_DERIVEDDATA_VER,
				*FString::Printf(TEXT("VTCHUNK%s"), *Chunk.BulkDataHash.ToString()));

			TotalBytesPut += Chunk.StoreInDerivedDataCache(ChunkDerivedDataKey, TextureName, bReplaceExistingDDC);
		}

		// VT always needs to replace the FVirtualTextureBuiltData in the DDC, otherwise we can be left in a situation where a local client is constantly attempting to rebuild chunks,
		// but failing to generate chunks that match the FVirtualTextureBuiltData in the DDC, due to non-determinism in texture generation
		bReplaceExistingDerivedDataDDC = true;
	}

	// Store derived data.
	// At this point we've stored all the non-inline data in the DDC, so this will only serialize and store the TexturePlatformData metadata and any inline mips
	FMemoryWriter64 Ar(RawDerivedData, /*bIsPersistent=*/ true);
	DerivedData->Serialize(Ar, NULL);
	const int64 RawDerivedDataSize = RawDerivedData.Num();
	TotalBytesPut += RawDerivedDataSize;

	using namespace UE::DerivedData;
	FRequestOwner AsyncOwner(EPriority::Normal);
	FValue Value = FValue::Compress(MakeSharedBufferFromArray(MoveTemp(RawDerivedData)));
	const ECachePolicy Policy = bReplaceExistingDerivedDataDDC ? ECachePolicy::Store : ECachePolicy::Default;
	GetCache().PutValue({{{TextureName}, ConvertLegacyCacheKey(DerivedDataKey), MoveTemp(Value), Policy}}, AsyncOwner);
	AsyncOwner.KeepAlive();

	UE_LOG(LogTexture, Verbose, TEXT("%s  Derived Data: %" INT64_FMT " bytes"), *LogString, RawDerivedDataSize);
	return TotalBytesPut;
}

#endif // #if WITH_EDITOR

/*------------------------------------------------------------------------------
	Derived data.
------------------------------------------------------------------------------*/

#if WITH_EDITOR

void FTexturePlatformData::Cache(
	UTexture& InTexture,
	const FTextureBuildSettings* InSettingsPerLayerFetchFirst, // can be null
	const FTextureBuildSettings* InSettingsPerLayerFetchOrBuild, // must be valid
	const FTexturePlatformData::FTextureEncodeResultMetadata* OutResultMetadataPerLayerFetchFirst, // can be nullptr
	const FTexturePlatformData::FTextureEncodeResultMetadata* OutResultMetadataPerLayerFetchOrBuild, // can be nullptr
	uint32 InFlags,
	ITextureCompressorModule* Compressor
	)
{
	//
	// Note this can be called off the main thread, despite referencing a UObject!
	// Be very careful!
	// (as of this writing, the shadow and light maps can call CachePlatformData
	// off the main thread via FAsyncEncode<>.)
	//
	
	TRACE_CPUPROFILER_EVENT_SCOPE(FTexturePlatformData::Cache);

	// Flush any existing async task and ignore results.
	CancelCache();

	ETextureCacheFlags Flags = ETextureCacheFlags(InFlags);

	if (IsUsingNewDerivedData() && InTexture.Source.GetNumLayers() == 1 && !InSettingsPerLayerFetchOrBuild->bVirtualStreamable)
	{
		COOK_STAT(auto Timer = TextureCookStats::UsageStats.TimeSyncWork());
		COOK_STAT(Timer.TrackCyclesOnly());
		EQueuedWorkPriority Priority = FTextureCompilingManager::Get().GetBasePriority(&InTexture);
		AsyncTask = CreateTextureBuildTask(
			InTexture, 
			*this, 
			InSettingsPerLayerFetchFirst, 
			*InSettingsPerLayerFetchOrBuild, 
			OutResultMetadataPerLayerFetchFirst, 
			OutResultMetadataPerLayerFetchOrBuild, 
			Priority, 
			Flags);
		if (AsyncTask)
		{
			return;
		}
		UE_LOG(LogTexture, Warning, TEXT("Failed to create requested DDC2 build task for texture %s -- falling back to DDC1"), *InTexture.GetName());
	}

	//
	// DDC1 from here on out.
	//

	static bool bForDDC = FString(FCommandLine::Get()).Contains(TEXT("Run=DerivedDataCache"));
	if (bForDDC)
	{
		Flags |= ETextureCacheFlags::ForDDCBuild;
	}

	bool bForceRebuild = EnumHasAnyFlags(Flags, ETextureCacheFlags::ForceRebuild);
	bool bAsync = EnumHasAnyFlags(Flags, ETextureCacheFlags::Async);

	if (!Compressor)
	{
		Compressor = &FModuleManager::LoadModuleChecked<ITextureCompressorModule>(TEXTURE_COMPRESSOR_MODULENAME);
	}

	if (InSettingsPerLayerFetchOrBuild[0].bVirtualStreamable)
	{
		Flags |= ETextureCacheFlags::ForVirtualTextureStreamingBuild;
	}

	if (bAsync && !bForceRebuild)
	{
		FQueuedThreadPool*  TextureThreadPool = FTextureCompilingManager::Get().GetThreadPool();
		EQueuedWorkPriority BasePriority      = FTextureCompilingManager::Get().GetBasePriority(&InTexture);

		COOK_STAT(auto Timer = TextureCookStats::UsageStats.TimeSyncWork());
		COOK_STAT(Timer.TrackCyclesOnly());
		FTextureAsyncCacheDerivedDataWorkerTask* LocalTask = new FTextureAsyncCacheDerivedDataWorkerTask(
			TextureThreadPool, 
			Compressor, 
			this, 
			&InTexture, 
			InSettingsPerLayerFetchFirst, 
			InSettingsPerLayerFetchOrBuild,
			OutResultMetadataPerLayerFetchFirst, 
			OutResultMetadataPerLayerFetchOrBuild,
			Flags);
		AsyncTask = LocalTask;
		LocalTask->StartBackgroundTask(TextureThreadPool, BasePriority, EQueuedWorkFlags::DoNotRunInsideBusyWait, LocalTask->GetTask().GetRequiredMemoryEstimate());
	}
	else
	{
		FTextureCacheDerivedDataWorker Worker(
			Compressor, 
			this, 
			&InTexture, 
			InSettingsPerLayerFetchFirst, 
			InSettingsPerLayerFetchOrBuild, 
			OutResultMetadataPerLayerFetchFirst,
			OutResultMetadataPerLayerFetchOrBuild,
			Flags);
		{
			COOK_STAT(auto Timer = TextureCookStats::UsageStats.TimeSyncWork());
			Worker.DoWork();
			Worker.Finalize();

			COOK_STAT(Timer.AddHitOrMiss(Worker.WasLoadedFromDDC() ? FCookStats::CallStats::EHitOrMiss::Hit : FCookStats::CallStats::EHitOrMiss::Miss, Worker.GetBytesCached()));
		}
	}
}

bool FTexturePlatformData::TryCancelCache()
{
	if (AsyncTask && AsyncTask->Cancel())
	{
		delete AsyncTask;
		AsyncTask = nullptr;
	}
	return !AsyncTask;
}

void FTexturePlatformData::CancelCache()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTexturePlatformData::CancelCache)

	// If we're unable to cancel, it means it's already being processed, we must finish it then.
	if (!TryCancelCache())
	{
		FinishCache();
	}
}

bool FTexturePlatformData::IsAsyncWorkComplete() const
{
	return !AsyncTask || AsyncTask->Poll();
}

void FTexturePlatformData::FinishCache()
{
	if (AsyncTask)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FTexturePlatformData::FinishCache)
		{
			COOK_STAT(auto Timer = TextureCookStats::UsageStats.TimeAsyncWait());
			bool bFoundInCache = false;
			uint64 ProcessedByteCount = 0;
			AsyncTask->Wait();
			AsyncTask->Finalize(bFoundInCache, ProcessedByteCount);
			COOK_STAT(Timer.AddHitOrMiss(bFoundInCache ? FCookStats::CallStats::EHitOrMiss::Hit : FCookStats::CallStats::EHitOrMiss::Miss, int64(ProcessedByteCount)));
		}
		delete AsyncTask;
		AsyncTask = nullptr;
	}
}

typedef TArray<uint32, TInlineAllocator<MAX_TEXTURE_MIP_COUNT> > FAsyncMipHandles;
typedef TArray<uint32> FAsyncVTChunkHandles;

/**
 * Executes async DDC gets for mips stored in the derived data cache.
 * @param Mip - Mips to retrieve.
 * @param FirstMipToLoad - Index of the first mip to retrieve.
 * @param Callback - Callback invoked for each mip as it loads.
 * 
 * This function must be called after the initial DDC fetch is complete,
 * so we know what our in-use key is. This might be on the worker immediately
 * after the fetch completes.
 */
static bool LoadDerivedStreamingMips(FTexturePlatformData& PlatformData, int32 FirstMipToLoad, FStringView DebugContext, TFunctionRef<void (int32 MipIndex, FSharedBuffer MipData)> Callback)
{
	using namespace UE::DerivedData;

	bool bMiss = false;

	TIndirectArray<FTexture2DMipMap>& Mips = PlatformData.Mips;
	const int32 ReadableMipCount = Mips.Num() - (PlatformData.GetNumMipsInTail() > 0 ? PlatformData.GetNumMipsInTail() - 1 : 0);

	if (PlatformData.DerivedDataKey.IsType<FString>())
	{
		TArray<FCacheGetValueRequest, TInlineAllocator<MAX_TEXTURE_MIP_COUNT>> Requests;

		for (int32 MipIndex = FirstMipToLoad; MipIndex < ReadableMipCount; ++MipIndex)
		{
			const FTexture2DMipMap& Mip = Mips[MipIndex];
			if (Mip.IsPagedToDerivedData() && !Mip.BulkData.IsBulkDataLoaded())
			{
				TStringBuilder<256> MipNameBuilder;
				MipNameBuilder.Append(DebugContext).Appendf(TEXT(" [MIP %d]"), MipIndex);
				FCacheGetValueRequest& Request = Requests.AddDefaulted_GetRef();
				Request.Name = MipNameBuilder;
				Request.Key = ConvertLegacyCacheKey(PlatformData.GetDerivedDataMipKeyString(MipIndex, Mip));
				Request.UserData = MipIndex;
			}
		}

		if (!Requests.IsEmpty())
		{
			COOK_STAT(auto Timer = TextureCookStats::StreamingMipUsageStats.TimeSyncWork());
			uint64 Size = 0;
			FRequestOwner BlockingOwner(EPriority::Blocking);
			GetCache().GetValue(Requests, BlockingOwner, [Callback = MoveTemp(Callback), &Size, &bMiss](FCacheGetValueResponse&& Response)
			{
				Size += Response.Value.GetRawSize();
				if (Response.Status == EStatus::Ok)
				{
					Callback(int32(Response.UserData), Response.Value.GetData().Decompress());
				}
				else
				{
					bMiss = true;
				}
			});
			BlockingOwner.Wait();
			COOK_STAT(Timer.AddHitOrMiss(!bMiss ? FCookStats::CallStats::EHitOrMiss::Hit : FCookStats::CallStats::EHitOrMiss::Miss, int64(Size)));
		}
	}
	else if (PlatformData.DerivedDataKey.IsType<FCacheKeyProxy>())
	{
		TArray<FCacheGetChunkRequest, TInlineAllocator<MAX_TEXTURE_MIP_COUNT>> Requests;

		const FCacheKey& Key = *PlatformData.DerivedDataKey.Get<UE::DerivedData::FCacheKeyProxy>().AsCacheKey();
		for (int32 MipIndex = FirstMipToLoad; MipIndex < ReadableMipCount; ++MipIndex)
		{
			const FTexture2DMipMap& Mip = Mips[MipIndex];
			if (Mip.IsPagedToDerivedData() && !Mip.BulkData.IsBulkDataLoaded())
			{
				TStringBuilder<256> MipNameBuilder;
				MipNameBuilder.Append(DebugContext).Appendf(TEXT(" [MIP %d]"), MipIndex);
				FCacheGetChunkRequest& Request = Requests.AddDefaulted_GetRef();
				Request.Name = MipNameBuilder;
				Request.Key = Key;
				Request.Id = FTexturePlatformData::MakeMipId(MipIndex);
				Request.UserData = MipIndex;
			}
		}

		if (!Requests.IsEmpty())
		{
			COOK_STAT(auto Timer = TextureCookStats::StreamingMipUsageStats.TimeSyncWork());
			uint64 Size = 0;
			FRequestOwner BlockingOwner(EPriority::Blocking);
			GetCache().GetChunks(Requests, BlockingOwner, [Callback = MoveTemp(Callback), &Size, &bMiss](FCacheGetChunkResponse&& Response)
			{
				Size += Response.RawSize;
				if (Response.Status == EStatus::Ok)
				{
					Callback(int32(Response.UserData), MoveTemp(Response.RawData));
				}
				else
				{
					bMiss = true;
				}
			});
			BlockingOwner.Wait();
			COOK_STAT(Timer.AddHitOrMiss(!bMiss ? FCookStats::CallStats::EHitOrMiss::Hit : FCookStats::CallStats::EHitOrMiss::Miss, int64(Size)));
		}
	}
	else
	{
		UE_LOG(LogTexture, Error, TEXT("Attempting to stream in mips for texture that has not generated a supported derived data key format."));
	}

	return !bMiss;
}

static bool LoadDerivedStreamingVTChunks(const TArray<FVirtualTextureDataChunk>& Chunks, FStringView DebugContext, TFunctionRef<void (int32 ChunkIndex, FSharedBuffer ChunkData)> Callback)
{
	using namespace UE::DerivedData;
	TArray<FCacheGetValueRequest> Requests;

	for (int32 ChunkIndex = 0; ChunkIndex < Chunks.Num(); ++ChunkIndex)
	{
		const FVirtualTextureDataChunk& Chunk = Chunks[ChunkIndex];
		if (!Chunk.DerivedDataKey.IsEmpty() && !Chunk.BulkData.IsBulkDataLoaded())
		{
			FCacheGetValueRequest& Request = Requests.AddDefaulted_GetRef();
			Request.Name = FSharedString(WriteToString<256>(DebugContext, TEXT(" [Chunk ]"), ChunkIndex, TEXT("]")));
			Request.Key = ConvertLegacyCacheKey(Chunk.DerivedDataKey);
			Request.UserData = ChunkIndex;
		}
	}

	bool bMiss = false;

	if (!Requests.IsEmpty())
	{
		COOK_STAT(auto Timer = TextureCookStats::StreamingMipUsageStats.TimeSyncWork());
		uint64 Size = 0;
		FRequestOwner BlockingOwner(EPriority::Blocking);
		GetCache().GetValue(Requests, BlockingOwner, [Callback = MoveTemp(Callback), &Size, &bMiss](FCacheGetValueResponse&& Response)
		{
			Size += Response.Value.GetRawSize();
			if (Response.Status == EStatus::Ok)
			{
				Callback(int32(Response.UserData), Response.Value.GetData().Decompress());
			}
			else
			{
				bMiss = true;
			}
		});
		BlockingOwner.Wait();
		COOK_STAT(Timer.AddHitOrMiss(!bMiss ? FCookStats::CallStats::EHitOrMiss::Hit : FCookStats::CallStats::EHitOrMiss::Miss, int64(Size)));
	}

	return !bMiss;
}

/** Logs a warning that MipSize is correct for the mipmap. */
static void CheckMipSize(FTexture2DMipMap& Mip, EPixelFormat PixelFormat, int64 MipSize)
{
	// this check is incorrect ; it does not account of platform tiling and padding done on textures
	// re-enable if fixed

	#if 0
	// Only volume can have SizeZ != 1
	if (MipSize != Mip.SizeZ * CalcTextureMipMapSize(Mip.SizeX, Mip.SizeY, PixelFormat, 0))
	{
		UE_LOG(LogTexture, Warning,
			TEXT("%dx%d mip of %s texture has invalid data in the DDC. Got %" INT64_FMT " bytes, expected %" SIZE_T_FMT ". Key=%s"),
			Mip.SizeX,
			Mip.SizeY,
			GPixelFormats[PixelFormat].Name,
			MipSize,
			CalcTextureMipMapSize(Mip.SizeX, Mip.SizeY, PixelFormat, 0),
			*Mip.DerivedDataKey
			);
	}
	#endif
}

//
// Retrieve all built texture data in to the associated arrays, and don't return unless there's an error
// or we have the data.
//
static bool FetchAllTextureDataSynchronous(FTexturePlatformData* PlatformData, FStringView DebugContext, TArray<TArray64<uint8>>& OutMipData, TArray<TArray64<uint8>>& OutVTChunkData)
{
	const auto StoreMip = [&OutMipData](int32 MipIndex, FSharedBuffer MipBuffer)
	{
		OutMipData[MipIndex].Append(static_cast<const uint8*>(MipBuffer.GetData()), MipBuffer.GetSize());
	};

	const int32 MipCount = PlatformData->Mips.Num();
	OutMipData.Empty(MipCount);
	OutMipData.AddDefaulted(MipCount);

	if (!LoadDerivedStreamingMips(*PlatformData, 0, DebugContext, StoreMip))
	{
		return false;
	}

	for (int32 MipIndex = 0; MipIndex < MipCount; ++MipIndex)
	{
		FTexture2DMipMap& Mip = PlatformData->Mips[MipIndex];
		if (OutMipData[MipIndex].IsEmpty())
		{
			if (Mip.BulkData.IsBulkDataLoaded())
			{
				OutMipData[MipIndex].Append((uint8*)Mip.BulkData.LockReadOnly(), Mip.BulkData.GetBulkDataSize());
				Mip.BulkData.Unlock();
			}
			else
			{
				return false;
			}
		}
	}

	const auto StoreChunk = [&OutVTChunkData](int32 ChunkIndex, FSharedBuffer ChunkBuffer)
	{
		OutVTChunkData[ChunkIndex].Append(static_cast<const uint8*>(ChunkBuffer.GetData()), ChunkBuffer.GetSize());
	};

	const int32 ChunkCount = PlatformData->VTData ? PlatformData->VTData->Chunks.Num() : 0;
	OutVTChunkData.Empty(ChunkCount);
	if (ChunkCount)
	{
		OutVTChunkData.AddDefaulted(ChunkCount);

		if (!LoadDerivedStreamingVTChunks(PlatformData->VTData->Chunks, DebugContext, StoreChunk))
		{
			return false;
		}

		for (int32 ChunkIndex = 0; ChunkIndex < ChunkCount; ++ChunkIndex)
		{
			FVirtualTextureDataChunk& Chunk = PlatformData->VTData->Chunks[ChunkIndex];
			if (OutVTChunkData[ChunkIndex].IsEmpty())
			{
				if (Chunk.BulkData.IsBulkDataLoaded())
				{
					// The data is resident and we can just copy it.
					OutVTChunkData[ChunkIndex].Append((uint8*)Chunk.BulkData.LockReadOnly(), Chunk.BulkData.GetBulkDataSize());
					Chunk.BulkData.Unlock();
				}
				else
				{
					return false;
				}
			}
		}
	}

	return true;
}

//
// Chunk the input data in to blocks of the compression block size, then
// run Oodle on the separate chunks in order to get an estimate of how
// much space on disk the texture will take during deployment. This
// exists so the editor can show the benefits of increasing RDO levels 
// on a texture.
//
// This is not exact! Due to the nature of iostore, we can't know exactly
// whether our data will be chunked on the boundaries we've chosen. However
// it is illustrative.
//
static void EstimateOnDiskCompressionForTextureData(
	TArray<TArray64<uint8>> InMipData,
	TArray<TArray64<uint8>> InVTChunkData,
	FOodleDataCompression::ECompressor InOodleCompressor,
	FOodleDataCompression::ECompressionLevel InOodleCompressionLevel,
	uint32 InCompressionBlockSize,
	uint64& OutUncompressedByteCount,
	uint64& OutCompressedByteCount
)
{
	//
	// This is written such that you can have both classic mip data and
	// virtual texture data, however actual unreal textures don't have
	// both.
	//
	uint64 UncompressedByteCount = 0;
	for (TArray64<uint8>& Mip : InMipData)
	{
		UncompressedByteCount += Mip.Num();
	}
	for (TArray64<uint8>& VTChunk : InVTChunkData)
	{
		UncompressedByteCount += VTChunk.Num();
	}

	OutUncompressedByteCount = UncompressedByteCount;

	if (UncompressedByteCount == 0)
	{
		OutCompressedByteCount = 0;
		return;
	}

	int32 MipIndex = 0;
	int32 VTChunkIndex = 0;
	int64 CurrentOffsetInContainer = 0;
	uint64 CompressedByteCount = 0;

	// Array for compressed data so we don't have to realloc.
	TArray<uint8> Compressed;
	Compressed.Reserve(InCompressionBlockSize + 1024);

	// When we cross our input array boundaries, we accumulate in to here.
	TArray64<uint8> ContinuousMemory;
	for (;;)
	{
		TArray64<uint8>& CurrentContainer = MipIndex < InMipData.Num() ? InMipData[MipIndex] : InVTChunkData[VTChunkIndex];

		uint64 NeedBytes = InCompressionBlockSize - ContinuousMemory.Num();
		uint64 CopyBytes = CurrentContainer.Num() - CurrentOffsetInContainer;
		if (CopyBytes > NeedBytes)
		{
			CopyBytes = NeedBytes;
		}

		// Can we compressed without an intervening copy?
		if (NeedBytes == InCompressionBlockSize && // We don't have a partial block copied
			CopyBytes == InCompressionBlockSize) // we can fit in this chunk
		{
			// Direct.
			Compressed.Empty();
			FOodleCompressedArray::CompressData(
				Compressed,
				CurrentContainer.GetData() + CurrentOffsetInContainer,
				InCompressionBlockSize,
				InOodleCompressor,
				InOodleCompressionLevel);

			CompressedByteCount += Compressed.Num();
		}
		else
		{
			// Need to accumulate in to our temp buffer.

			if (ContinuousMemory.Num() == 0)
			{
				ContinuousMemory.Reserve(InCompressionBlockSize);
			}

			ContinuousMemory.Append(CurrentContainer.GetData() + CurrentOffsetInContainer, CopyBytes);

			if (ContinuousMemory.Num() == InCompressionBlockSize)
			{
				// Filled a block - kick.
				Compressed.Empty();
				FOodleCompressedArray::CompressData(
					Compressed,
					ContinuousMemory.GetData(),
					InCompressionBlockSize,
					InOodleCompressor,
					InOodleCompressionLevel);

				CompressedByteCount += Compressed.Num();
				ContinuousMemory.Empty();
			}
		}

		// Advance read cursor.
		CurrentOffsetInContainer += CopyBytes;
		if (CurrentOffsetInContainer >= CurrentContainer.Num())
		{
			CurrentOffsetInContainer = 0;

			if (MipIndex < InMipData.Num())
			{
				MipIndex++;
			}
			else if (VTChunkIndex < InVTChunkData.Num())
			{
				VTChunkIndex++;
			}

			if (MipIndex >= InMipData.Num() && VTChunkIndex >= InVTChunkData.Num())
			{
				// No more source data.
				break;
			}
		}
	}

	if (ContinuousMemory.Num())
	{
		// If we ran out of source data before we completely filled, kick here.
		Compressed.Empty();
		FOodleCompressedArray::CompressData(
			Compressed,
			ContinuousMemory.GetData(),
			ContinuousMemory.Num(),
			InOodleCompressor,
			InOodleCompressionLevel);

		CompressedByteCount += Compressed.Num();
	}

	OutCompressedByteCount = CompressedByteCount;
}

//
// Grabs the texture data and then kicks off a task to block compress it
// in order to try and mimic how iostore does on disk compression.
//
// Returns the future result of the compression, with the compressed byte count
// in the first of the pair and the total in the second.
//
TFuture<TTuple<uint64, uint64>> FTexturePlatformData::LaunchEstimateOnDiskSizeTask(
	FOodleDataCompression::ECompressor InOodleCompressor,
	FOodleDataCompression::ECompressionLevel InOodleCompressionLevel,
	uint32 InCompressionBlockSize,
	FStringView InDebugContext
	)
{
	TArray<TArray64<uint8>> MipData;
	TArray<TArray64<uint8>> VTChunkData;
	if (FetchAllTextureDataSynchronous(this, InDebugContext, MipData, VTChunkData) == false)
	{
		return TFuture<TTuple<uint64, uint64>>();
	}
	
	struct FAsyncEstimateState
	{
		TPromise<TPair<uint64, uint64>> Promise;
		TArray<TArray64<uint8>> MipData;
		TArray<TArray64<uint8>> VTChunkData;
		FOodleDataCompression::ECompressor OodleCompressor;
		FOodleDataCompression::ECompressionLevel OodleCompressionLevel;
		uint32 CompressionBlockSize;
	};
	
	FAsyncEstimateState* State = new FAsyncEstimateState();
	State->MipData = MoveTemp(MipData);
	State->VTChunkData = MoveTemp(VTChunkData);
	State->OodleCompressor = InOodleCompressor;
	State->OodleCompressionLevel = InOodleCompressionLevel;
	State->CompressionBlockSize = InCompressionBlockSize;

	// Grab the future before we kick the task so there's no race.
	// (unlikely since compression is so long...)
	TFuture<TTuple<uint64, uint64>> ResultFuture = State->Promise.GetFuture();

	// Kick off a task with no dependencies that does the compression
	// and posts the result to the future.
	FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady([State]()
	{
		uint64 CompressedByteCount=0, UncompressedByteCount=0;

		EstimateOnDiskCompressionForTextureData(
			State->MipData, 
			State->VTChunkData,
			State->OodleCompressor,
			State->OodleCompressionLevel,
			State->CompressionBlockSize,
			UncompressedByteCount,
			CompressedByteCount);

		State->Promise.SetValue(TTuple<uint64, uint64>(CompressedByteCount, UncompressedByteCount));
		delete State;
	}, TStatId(), nullptr, ENamedThreads::AnyBackgroundThreadNormalTask);

	return ResultFuture;
}

bool FTexturePlatformData::TryInlineMipData(int32 FirstMipToLoad, FStringView DebugContext)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTexturePlatformData::TryInlineMipData);

	const auto StoreMip = [this](int32 MipIndex, FSharedBuffer MipBuffer)
	{
		FTexture2DMipMap& Mip = Mips[MipIndex];
		Mip.BulkData.Lock(LOCK_READ_WRITE);
		void* MipData = Mip.BulkData.Realloc(int64(MipBuffer.GetSize()));
		FMemory::Memcpy(MipData, MipBuffer.GetData(), MipBuffer.GetSize());
		Mip.BulkData.Unlock();
	};

	if (!LoadDerivedStreamingMips(*this, FirstMipToLoad, DebugContext, StoreMip))
	{
		return false;
	}

	const auto StoreChunk = [this](int32 ChunkIndex, FSharedBuffer ChunkBuffer)
	{
		FVirtualTextureDataChunk& Chunk = VTData->Chunks[ChunkIndex];
		Chunk.BulkData.Lock(LOCK_READ_WRITE);
		void* ChunkData = Chunk.BulkData.Realloc(int64(ChunkBuffer.GetSize()));
		FMemory::Memcpy(ChunkData, ChunkBuffer.GetData(), ChunkBuffer.GetSize());
		Chunk.BulkData.Unlock();
	};

	if (VTData && !LoadDerivedStreamingVTChunks(VTData->Chunks, DebugContext, StoreChunk))
	{
		return false;
	}

	return true;
}

#endif // #if WITH_EDITOR

FTexturePlatformData::FTexturePlatformData()
	: SizeX(0)
	, SizeY(0)
	, PackedData(0)
	, PixelFormat(PF_Unknown)
	, VTData(nullptr)
#if WITH_EDITORONLY_DATA
	, AsyncTask(nullptr)
#endif // #if WITH_EDITORONLY_DATA
{
}

FTexturePlatformData::~FTexturePlatformData()
{
#if WITH_EDITOR
	if (AsyncTask)
	{
		if (!AsyncTask->Cancel())
		{
			AsyncTask->Wait();
		}
		delete AsyncTask;
		AsyncTask = nullptr;
	}
#endif
	if (VTData) delete VTData;
}

bool FTexturePlatformData::IsReadyForAsyncPostLoad() const
{
#if WITH_EDITOR
	// Can't touch the Mips until async work is finished
	if (!IsAsyncWorkComplete())
	{
		return false;
	}
#endif

	for (int32 MipIndex = 0; MipIndex < Mips.Num(); ++MipIndex)
	{
		const FTexture2DMipMap& Mip = Mips[MipIndex];
		if (!Mip.BulkData.IsAsyncLoadingComplete())
		{
			return false;
		}
	}
	return true;
}

bool FTexturePlatformData::TryLoadMips(int32 FirstMipToLoad, void** OutMipData, FStringView DebugContext)
{
	// TryLoadMips fills mip pointers but not sizes
	//  dangerous, not robust, use TryLoadMipsWithSizes instead

	return TryLoadMipsWithSizes(FirstMipToLoad, OutMipData, nullptr, DebugContext);
}

bool FTexturePlatformData::TryLoadMipsWithSizes(int32 FirstMipToLoad, void** OutMipData, int64* OutMipSize, FStringView DebugContext)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FTexturePlatformData::TryLoadMips);

	int32 NumMipsCached = 0;
	const int32 LoadableMips = Mips.Num() - ((GetNumMipsInTail() > 0) ? (GetNumMipsInTail() - 1) : 0);
	check(LoadableMips >= 0);

#if WITH_EDITOR
	const auto StoreMip = [this, OutMipData, OutMipSize, FirstMipToLoad, &NumMipsCached](int32 MipIndex, FSharedBuffer MipBuffer)
	{
		FTexture2DMipMap& Mip = Mips[MipIndex];

		const int64 MipSize = static_cast<int64>(MipBuffer.GetSize());
		CheckMipSize(Mip, PixelFormat, MipSize);
		NumMipsCached++;

		if (OutMipData)
		{
			OutMipData[MipIndex - FirstMipToLoad] = FMemory::Malloc(MipSize);
			FMemory::Memcpy(OutMipData[MipIndex - FirstMipToLoad], MipBuffer.GetData(), MipSize);
		}
		if ( OutMipSize ) 
		{
			OutMipSize[MipIndex - FirstMipToLoad] = MipSize;
		}
	};

	if (!LoadDerivedStreamingMips(*this, FirstMipToLoad, DebugContext, StoreMip))
	{
		return false;
	}
#endif // #if WITH_EDITOR

	// Handle the case where we inlined more mips than we intend to keep resident
	// Discard unneeded mips
	for (int32 MipIndex = 0; MipIndex < FirstMipToLoad && MipIndex < LoadableMips; ++MipIndex)
	{
		FTexture2DMipMap& Mip = Mips[MipIndex];
		if (Mip.BulkData.IsBulkDataLoaded())
		{
			Mip.BulkData.Lock(LOCK_READ_ONLY);
			Mip.BulkData.Unlock();
		}
	}

	// Load remaining mips (if any) from bulk data.
	for (int32 MipIndex = FirstMipToLoad; MipIndex < LoadableMips; ++MipIndex)
	{
		FTexture2DMipMap& Mip = Mips[MipIndex];
		const int64 BulkDataSize = Mip.BulkData.GetBulkDataSize();
		if (BulkDataSize > 0)
		{
			if (OutMipData)
			{
#if PLATFORM_SUPPORTS_TEXTURE_STREAMING
				// We want to make sure that any non-streamed mips are coming from the texture asset file, and not from an external bulk file.
				// But because "r.TextureStreaming" is driven by the project setting as well as the command line option "-NoTextureStreaming", 
				// is it possible for streaming mips to be loaded in non streaming ways.
				// Also check if editor data is available, in which case we are probably loading cooked data in the editor.
				if (!FPlatformProperties::HasEditorOnlyData() && CVarSetTextureStreaming.GetValueOnAnyThread() != 0)
				{
					UE_CLOG(Mip.BulkData.IsInSeparateFile(), LogTexture, Error, TEXT("Loading non-streamed mips from an external bulk file.  This is not desireable.  File %s"),
						*(Mip.BulkData.GetDebugName()) );
				}
#endif
				Mip.BulkData.GetCopy(&OutMipData[MipIndex - FirstMipToLoad], true);
			}
			if (OutMipSize)
			{
				OutMipSize[MipIndex - FirstMipToLoad] = BulkDataSize;
			}
			NumMipsCached++;
		}
	}

	if (NumMipsCached != (LoadableMips - FirstMipToLoad))
	{
		UE_LOG(LogTexture, Verbose, TEXT("TryLoadMips failed for %.*s, NumMipsCached: %d, LoadableMips: %d, FirstMipToLoad: %d"),
			DebugContext.Len(), DebugContext.GetData(),
			NumMipsCached,
			LoadableMips,
			FirstMipToLoad);

		// Unable to cache all mips. Release memory for those that were cached.
		for (int32 MipIndex = FirstMipToLoad; MipIndex < LoadableMips; ++MipIndex)
		{
			FTexture2DMipMap& Mip = Mips[MipIndex];
			UE_LOG(LogTexture, Verbose, TEXT("  Mip %d, BulkDataSize: %" INT64_FMT),
				MipIndex,
				Mip.BulkData.GetBulkDataSize());

			if (OutMipData && OutMipData[MipIndex - FirstMipToLoad])
			{
				FMemory::Free(OutMipData[MipIndex - FirstMipToLoad]);
				OutMipData[MipIndex - FirstMipToLoad] = NULL;
			}
		}
		return false;
	}

	return true;
}

int32 FTexturePlatformData::GetNumNonStreamingMips(bool bIsStreamingPossible) const
{
	if (CanUseCookedDataPath())
	{
		// We're on a cooked platform so we should only be streaming mips that were not inlined in the texture by thecooker.
		int32 NumNonStreamingMips = Mips.Num();

		for (const FTexture2DMipMap& Mip : Mips)
		{
			if ( Mip.BulkData.IsInSeparateFile() || !Mip.BulkData.IsInlined() )
			{
				--NumNonStreamingMips;
			}
			else
			{
				break;
			}
		}

		if (NumNonStreamingMips == 0 && Mips.Num())
		{
			return 1;
		}
		else
		{
			if ( ! bIsStreamingPossible )
			{
				check( NumNonStreamingMips == Mips.Num() );
			}

			return NumNonStreamingMips;
		}
	}
	else if (Mips.Num() <= 1 || !bIsStreamingPossible)
	{
		return Mips.Num();
	}
	else
	{
		int32 MipCount = Mips.Num();
		int32 NumNonStreamingMips = 1;
		// MipCount >= 2 and bIsStreamingPossible

		// Take in to account the min resident limit.
		NumNonStreamingMips = FMath::Max(NumNonStreamingMips, (int32)GetNumMipsInTail());
		NumNonStreamingMips = FMath::Max(NumNonStreamingMips, UTexture2D::GetStaticMinTextureResidentMipCount());
		NumNonStreamingMips = FMath::Min(NumNonStreamingMips, MipCount);
		int32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
		int32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
		if (BlockSizeX > 1 || BlockSizeY > 1)
		{
			// ensure the top non-streamed mip size is >= BlockSize (and a multiple of block size!)
			// @todo Oodle : only do for BCN, not for ASTC
			
			// note: this is not right for non pow 2; NeverStream should set !bIsStreamingPossible in that case
			if ( FMath::IsPowerOfTwo(Mips[0].SizeX) && FMath::IsPowerOfTwo(Mips[0].SizeY) )
			{
				NumNonStreamingMips = FMath::Max<int32>(NumNonStreamingMips, MipCount - FPlatformMath::FloorLog2(Mips[0].SizeX / BlockSizeX));
				NumNonStreamingMips = FMath::Max<int32>(NumNonStreamingMips, MipCount - FPlatformMath::FloorLog2(Mips[0].SizeY / BlockSizeY));
			}
			else
			{
				// should have !bIsStreamingPossible, so we should not hit this branch, but it's not reliable
				NumNonStreamingMips = MipCount;
			}
		}

		return NumNonStreamingMips;
	}
}

int32 FTexturePlatformData::GetNumNonOptionalMips() const
{
	// TODO : Count from last mip to first.
	if (CanUseCookedDataPath())
	{
		int32 NumNonOptionalMips = Mips.Num();

		for (const FTexture2DMipMap& Mip : Mips)
		{
			if (Mip.BulkData.IsOptional())
			{
				--NumNonOptionalMips;
			}
			else
			{
				break;
			}
		}

		if (NumNonOptionalMips == 0 && Mips.Num())
		{
			return 1;
		}
		else
		{
			return NumNonOptionalMips;
		}
	}
	else // Otherwise, all mips are available.
	{
		return Mips.Num();
	}
}

bool FTexturePlatformData::CanBeLoaded() const
{
	for (const FTexture2DMipMap& Mip : Mips)
	{
		if (Mip.DerivedData.HasData())
		{
			return true;
		}
		if (Mip.BulkData.CanLoadFromDisk())
		{
			return true;
		}
	}
	return false;
}


int32 FTexturePlatformData::GetNumVTMips() const
{
	check(VTData);
	return VTData->GetNumMips();
}

EPixelFormat FTexturePlatformData::GetLayerPixelFormat(uint32 LayerIndex) const
{
	if (VTData)
	{
		check(LayerIndex < VTData->NumLayers);
		return VTData->LayerTypes[LayerIndex];
	}
	check(LayerIndex == 0u);
	return PixelFormat;
}

int64 FTexturePlatformData::GetPayloadSize(int32 MipBias) const
{
	int64 PayloadSize = 0;
	if (VTData)
	{
		int32 NumTiles = 0;
		for (uint32 MipIndex = MipBias; MipIndex < VTData->NumMips; MipIndex++)
		{
			NumTiles += VTData->TileOffsetData[MipIndex].Width * VTData->TileOffsetData[MipIndex].Height;
		}
		int32 TileSizeWithBorder = (int32)(VTData->TileSize + 2 * VTData->TileBorderSize);
		int32 TileBlockSizeX = FMath::DivideAndRoundUp(TileSizeWithBorder, GPixelFormats[PixelFormat].BlockSizeX);
		int32 TileBlockSizeY = FMath::DivideAndRoundUp(TileSizeWithBorder, GPixelFormats[PixelFormat].BlockSizeY);
		PayloadSize += (int64)GPixelFormats[PixelFormat].BlockBytes * TileBlockSizeX * TileBlockSizeY * VTData->NumLayers * NumTiles;
	}
	else
	{
		for (int32 MipIndex = MipBias; MipIndex < Mips.Num(); MipIndex++)
		{
			int32 BlockSizeX = FMath::DivideAndRoundUp(Mips[MipIndex].SizeX, GPixelFormats[PixelFormat].BlockSizeX);
			int32 BlockSizeY = FMath::DivideAndRoundUp(Mips[MipIndex].SizeY, GPixelFormats[PixelFormat].BlockSizeY);
			// for TextureCube and TextureCubeArray all the mipmaps contain the same number of slices, which is encoded in the PackedData member
			// at the same time we can not just use SizeZ of a TextureCube mipmap, because for compatibility reasons it is always set to 1 and not 6 (which is the actual number of slices)
			int32 BlockSizeZ = FMath::DivideAndRoundUp(FMath::Max(IsCubemap() ? GetNumSlices() : Mips[MipIndex].SizeZ, 1), GPixelFormats[PixelFormat].BlockSizeZ);
			PayloadSize += (int64)GPixelFormats[PixelFormat].BlockBytes * BlockSizeX * BlockSizeY * BlockSizeZ;
		}
	}
	return PayloadSize;
}

bool FTexturePlatformData::CanUseCookedDataPath() const
{
#if WITH_IOSTORE_IN_EDITOR
	return Mips.Num() > 0 && Mips[0].BulkData.IsUsingIODispatcher();
#else	
	return FPlatformProperties::RequiresCookedData();
#endif //WITH_IOSTORE_IN_EDITOR
}

#if WITH_EDITOR
bool FTexturePlatformData::AreDerivedMipsAvailable(FStringView Context) const
{
	if (DerivedDataKey.IsType<FString>())
	{
		using namespace UE::DerivedData;
		TArray<FCacheGetValueRequest, TInlineAllocator<16>> MipRequests;

		int32 MipIndex = 0;
		const FSharedString SharedContext = Context;
		for (const FTexture2DMipMap& Mip : Mips)
		{
			if (Mip.IsPagedToDerivedData())
			{
				const FCacheKey MipKey = ConvertLegacyCacheKey(GetDerivedDataMipKeyString(MipIndex, Mip));
				const ECachePolicy ExistsPolicy = ECachePolicy::Query | ECachePolicy::SkipData;
				MipRequests.Add({SharedContext, MipKey, ExistsPolicy});
			}
			++MipIndex;
		}

		if (MipRequests.IsEmpty())
		{
			return true;
		}

		// When performing async loading, prefetch the lowest streaming mip into local caches
		// to avoid high priority request stalls from the render thread.
		if (!IsInGameThread())
		{
			MipRequests.Last().Policy |= ECachePolicy::StoreLocal;
		}

		bool bAreDerivedMipsAvailable = true;
		FRequestOwner BlockingOwner(EPriority::Blocking);
		GetCache().GetValue(MipRequests, BlockingOwner, [&bAreDerivedMipsAvailable](FCacheGetValueResponse&& Response)
		{
			bAreDerivedMipsAvailable &= Response.Status == EStatus::Ok;
		});
		BlockingOwner.Wait();
		return bAreDerivedMipsAvailable;
	}
	else if (DerivedDataKey.IsType<UE::DerivedData::FCacheKeyProxy>())
	{
		return true;
	}

	return false;
}

bool FTexturePlatformData::AreDerivedVTChunksAvailable(FStringView Context) const
{
	check(VTData);

	using namespace UE::DerivedData;
	TArray<FCacheGetValueRequest, TInlineAllocator<16>> ChunkRequests;

	int32 ChunkIndex = 0;
	const FSharedString SharedContext = Context;
	for (const FVirtualTextureDataChunk& Chunk : VTData->Chunks)
	{
		if (!Chunk.DerivedDataKey.IsEmpty())
		{
			const FCacheKey ChunkKey = ConvertLegacyCacheKey(Chunk.DerivedDataKey);
			const ECachePolicy ExistsPolicy = ECachePolicy::Query | ECachePolicy::SkipData;
			ChunkRequests.Add({SharedContext, ChunkKey, ExistsPolicy});
		}
		++ChunkIndex;
	}

	if (ChunkRequests.IsEmpty())
	{
		return true;
	}

	// When performing async loading, prefetch the last chunk into local caches
	// to avoid high priority request stalls from the render thread.
	if (!IsInGameThread())
	{
		ChunkRequests.Last().Policy |= ECachePolicy::StoreLocal;
	}

	bool bAreDerivedChunksAvailable = true;
	FRequestOwner BlockingOwner(EPriority::Blocking);
	GetCache().GetValue(ChunkRequests, BlockingOwner, [&bAreDerivedChunksAvailable](FCacheGetValueResponse&& Response)
	{
		bAreDerivedChunksAvailable &= Response.Status == EStatus::Ok;
	});
	BlockingOwner.Wait();
	return bAreDerivedChunksAvailable;
}

bool FTexturePlatformData::AreDerivedMipsAvailable() const
{
	return AreDerivedMipsAvailable(TEXTVIEW("DerivedMips"));
}

bool FTexturePlatformData::AreDerivedVTChunksAvailable() const
{
	return AreDerivedVTChunksAvailable(TEXTVIEW("DerivedVTChunks"));
}

#endif // #if WITH_EDITOR

// Transient flags used to control behavior of platform data serialization
enum class EPlatformDataSerializationFlags : uint8
{
	None = 0,
	Cooked = 1<<0,
	Streamable = 1<<1,
};
ENUM_CLASS_FLAGS(EPlatformDataSerializationFlags);

static void SerializePlatformData(
	FArchive& Ar,
	FTexturePlatformData* PlatformData,
	UTexture* Texture,
	EPlatformDataSerializationFlags Flags
)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("SerializePlatformData"), STAT_Texture_SerializePlatformData, STATGROUP_LoadTime);

	// note: if BuildTexture failed, we still get called here,
	//	just with a default-constructed PlatformData
	//	(no mips, sizes=0, PF=Unknown)

	if (Ar.IsFilterEditorOnly())
	{
		constexpr int64 PlaceholderDerivedDataSize = 16;
		uint8 PlaceholderDerivedData[PlaceholderDerivedDataSize]{};
		Ar.Serialize(PlaceholderDerivedData, PlaceholderDerivedDataSize);
		check(Algo::AllOf(PlaceholderDerivedData, [](uint8 Value) { return Value == 0; }));
	}

	const bool bCooked = (Flags & EPlatformDataSerializationFlags::Cooked) == EPlatformDataSerializationFlags::Cooked;
	const bool bStreamable = (Flags & EPlatformDataSerializationFlags::Streamable) == EPlatformDataSerializationFlags::Streamable;

	UEnum* PixelFormatEnum = UTexture::GetPixelFormatEnum();

	Ar << PlatformData->SizeX;
	Ar << PlatformData->SizeY;
	Ar << PlatformData->PackedData;
	if (Ar.IsLoading())
	{
		FString PixelFormatString;
		Ar << PixelFormatString;
		const int64 PixelFormatValue = PixelFormatEnum->GetValueByName(*PixelFormatString);
		if (PixelFormatValue != INDEX_NONE && PixelFormatValue < PF_MAX)
		{
			PlatformData->PixelFormat = (EPixelFormat)PixelFormatValue;
		}
		else
		{
			UE_LOG(LogTexture, Warning, TEXT("Invalid pixel format '%s' for texture '%s'."), *PixelFormatString, Texture ? *Texture->GetPathName() : TEXT(""));
			PlatformData->PixelFormat = PF_Unknown;
		}
	}
	else if (Ar.IsSaving())
	{
		FString PixelFormatString = PixelFormatEnum->GetNameByValue(PlatformData->PixelFormat).GetPlainNameString();
		Ar << PixelFormatString;
	}

	if (PlatformData->GetHasOptData())
	{
		Ar << PlatformData->OptData;
	}

	int32 NumMips = PlatformData->Mips.Num();
	int32 FirstMipToSerialize = 0;

	bool bIsVirtual = false;
	if (Ar.IsSaving())
	{
		bIsVirtual = PlatformData->VTData != nullptr;
	}

	if (bCooked && bIsVirtual)
	{
		check(PlatformData->Mips.Num() == 0);
	}

	if (bCooked)
	{
#if WITH_EDITORONLY_DATA
		if (Ar.IsSaving())
		{
			check(Ar.CookingTarget());
			check(Texture);

			const int32 Width = PlatformData->SizeX;
			const int32 Height = PlatformData->SizeY;
			const int32 LODGroup = Texture->LODGroup;
			const int32 LODBias = Texture->LODBias;
			const TextureMipGenSettings MipGenSetting = Texture->MipGenSettings;
			const int32 LastMip = FMath::Max(NumMips - 1, 0);
			check(NumMips >= (int32)PlatformData->GetNumMipsInTail());
			const int32 FirstMipTailMip = NumMips - (int32)PlatformData->GetNumMipsInTail();

			FirstMipToSerialize = Ar.CookingTarget()->GetTextureLODSettings().CalculateLODBias(Width, Height, Texture->MaxTextureSize, LODGroup, LODBias, 0, MipGenSetting, bIsVirtual);
			if (!bIsVirtual)
			{
				// NumMips is the number of mips starting from FirstMipToSerialize
				FirstMipToSerialize = FMath::Clamp(FirstMipToSerialize, 0, PlatformData->GetNumMipsInTail() > 0 ? FirstMipTailMip : LastMip);
				NumMips = FMath::Max(0, NumMips - FirstMipToSerialize);
			}
			else
			{
				FirstMipToSerialize = FMath::Clamp(FirstMipToSerialize, 0, FMath::Max((int32)PlatformData->VTData->GetNumMips() - 1, 0));
			}
		}
#endif // #if WITH_EDITORONLY_DATA
		Ar << FirstMipToSerialize;
		if (Ar.IsLoading())
		{
			check(Texture);
			FirstMipToSerialize = 0;
		}
	}

	TArray<uint32> BulkDataMipFlags;

	// Force resident mips inline
	if (bCooked && Ar.IsSaving())
	{
		if (bIsVirtual == false)
		{
			BulkDataMipFlags.AddZeroed(PlatformData->Mips.Num());
			for (int32 MipIndex = 0; MipIndex < PlatformData->Mips.Num(); ++MipIndex)
			{
				BulkDataMipFlags[MipIndex] = PlatformData->Mips[MipIndex].BulkData.GetBulkDataFlags();
			}

			int32 MinMipToInline = 0;
			int32 OptionalMips = 0; // TODO: do we need to consider platforms saving texture assets as cooked files? all the info to calculate the optional is part of the editor only data
			bool DuplicateNonOptionalMips = false;
		
			// bStreamable comes from IsCandidateForTextureStreaming
			//  if not bStreamable, all mips are written inline
			//  so the runtime will see NumNonStreamingMips = all

#if WITH_EDITORONLY_DATA
			check(Ar.CookingTarget());
			// This also needs to check whether the project enables texture streaming.
			// Currently, there is no reliable way to implement this because there is no difference
			// between the project settings (CVar) and the command line setting (from -NoTextureStreaming)
			if (bStreamable && Ar.CookingTarget()->SupportsFeature(ETargetPlatformFeatures::TextureStreaming))
#else
			if (bStreamable)
#endif
			{
				int32 NumNonStreamingMips = PlatformData->GetNumNonStreamingMips(true);
				// NumMips has been reduced by FirstMipToSerialize (LODBias)
				NumNonStreamingMips = FMath::Min(NumNonStreamingMips,NumMips);
				// NumNonStreamingMips is not serialized
				// the runtime will just use NumNonStreamingMips == NumInlineMips
				MinMipToInline = NumMips - NumNonStreamingMips;
#if WITH_EDITORONLY_DATA
				const int32 LODGroup = Texture->LODGroup;

				// was a bug ? CalculateNumOptional mips is using full Width/Height ?  should be from FirstMipToSerialize? -> moot, it's not actually used
				const int32 FirstMipWidth  = PlatformData->Mips[FirstMipToSerialize].SizeX;
				const int32 FirstMipHeight = PlatformData->Mips[FirstMipToSerialize].SizeY;

				OptionalMips = Ar.CookingTarget()->GetTextureLODSettings().CalculateNumOptionalMips(LODGroup, FirstMipWidth, FirstMipHeight, NumMips, MinMipToInline, Texture->MipGenSettings);
				DuplicateNonOptionalMips = Ar.CookingTarget()->GetTextureLODSettings().TextureLODGroups[LODGroup].DuplicateNonOptionalMips;

				// Optional mips must not overlap the non-streaming mips : (MinMipToInline ensures this)
				check( OptionalMips + NumNonStreamingMips <= NumMips );
#endif
			}

			for (int32 MipIndex = 0; MipIndex < NumMips && MipIndex < OptionalMips; ++MipIndex) //-V654
			{
				// optional (and streamed) mips
				PlatformData->Mips[MipIndex + FirstMipToSerialize].BulkData.SetBulkDataFlags(BULKDATA_Force_NOT_InlinePayload | BULKDATA_OptionalPayload);
			}

			const uint32 AdditionalNonOptionalBulkDataFlags = DuplicateNonOptionalMips ? BULKDATA_DuplicateNonOptionalPayload : 0;
			for (int32 MipIndex = OptionalMips; MipIndex < NumMips && MipIndex < MinMipToInline; ++MipIndex)
			{
				// non-optional but streamed mips
				PlatformData->Mips[MipIndex + FirstMipToSerialize].BulkData.SetBulkDataFlags(BULKDATA_Force_NOT_InlinePayload | AdditionalNonOptionalBulkDataFlags);
			}
			for (int32 MipIndex = MinMipToInline; MipIndex < NumMips; ++MipIndex)
			{
				// non-streamed (inline) mips
				PlatformData->Mips[MipIndex + FirstMipToSerialize].BulkData.SetBulkDataFlags(BULKDATA_ForceInlinePayload | BULKDATA_SingleUse);
			}
		}
		else // bVirtual == true
		{
			const int32 NumChunks = PlatformData->VTData->Chunks.Num();
			BulkDataMipFlags.AddZeroed(NumChunks);
			for (int32 ChunkIndex = 0; ChunkIndex < NumChunks; ++ChunkIndex)
			{
				BulkDataMipFlags[ChunkIndex] = PlatformData->VTData->Chunks[ChunkIndex].BulkData.GetBulkDataFlags();
				PlatformData->VTData->Chunks[ChunkIndex].BulkData.SetBulkDataFlags(BULKDATA_Force_NOT_InlinePayload);
			}	
		}

		// Save cook tags
#if WITH_EDITORONLY_DATA
		if (Ar.GetCookContext() && Ar.GetCookContext()->GetCookTagList())
		{
			FCookTagList* CookTags = Ar.GetCookContext()->GetCookTagList();

			if (bIsVirtual)
			{
				FVirtualTextureBuiltData* VTData = PlatformData->VTData;
				CookTags->Add(Texture, "Size", FString::Printf(TEXT("%dx%d"), VTData->Width, VTData->Height));
	}
			else if ( PlatformData->Mips.Num() > 0 ) // PlatformData->Mips is empty if BuildTexture failed
			{
				FString DimensionsStr;
				FTexture2DMipMap& TopMip = PlatformData->Mips[FirstMipToSerialize];
				if (TopMip.SizeZ != 1)
				{
					DimensionsStr = FString::Printf(TEXT("%dx%dx%d"), TopMip.SizeX, TopMip.SizeY, TopMip.SizeZ);
				}
				else
				{
					DimensionsStr = FString::Printf(TEXT("%dx%d"), TopMip.SizeX, TopMip.SizeY);
				}
				CookTags->Add(Texture, "Size", MoveTemp(DimensionsStr));	
			}

			CookTags->Add(Texture, "Format", FString(GPixelFormats[PlatformData->PixelFormat].Name));

			// Add in diff keys for change detection/blame.
			{
				// Did the source change?
				CookTags->Add(Texture, "Diff_10_Tex2D_Source", Texture->Source.GetIdString());

				// Did the settings change?
				if (const UE::DerivedData::FCacheKeyProxy* CacheKey = PlatformData->DerivedDataKey.TryGet<UE::DerivedData::FCacheKeyProxy>())
				{
					CookTags->Add(Texture, "Diff_20_Tex2D_CacheKey", *WriteToString<64>(*CacheKey->AsCacheKey()));
				}
				else if (const FString* DDK = PlatformData->DerivedDataKey.TryGet<FString>())
				{
					CookTags->Add(Texture, "Diff_20_Tex2D_DDK", FString(*DDK));
				}
			}
		}
#endif
	}
	Ar << NumMips;
	check(NumMips >= (int32)PlatformData->GetNumMipsInTail());
	if (Ar.IsLoading())
	{
		check(FirstMipToSerialize == 0);
		PlatformData->Mips.Empty(NumMips);
		for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
		{
			PlatformData->Mips.Add(new FTexture2DMipMap());
		}
	}

	for (int32 MipIndex = 0; MipIndex < NumMips; ++MipIndex)
	{
		PlatformData->Mips[FirstMipToSerialize + MipIndex].Serialize(Ar, Texture, MipIndex);
	}

	Ar << bIsVirtual;
	if (bIsVirtual)
	{
		if (Ar.IsLoading() && PlatformData->VTData == nullptr)
		{
			PlatformData->VTData = new FVirtualTextureBuiltData();
		}
		else
		{
			check(PlatformData->VTData);
		}

		PlatformData->VTData->Serialize(Ar, Texture, FirstMipToSerialize);
	}

	if (bIsVirtual == false)
	{
		for (int32 MipIndex = 0; MipIndex < BulkDataMipFlags.Num(); ++MipIndex)
		{
			check(Ar.IsSaving());
			PlatformData->Mips[MipIndex].BulkData.ResetBulkDataFlags(BulkDataMipFlags[MipIndex]);
		}
	}
	else
	{
		for (int32 ChunkIndex = 0; ChunkIndex < BulkDataMipFlags.Num(); ++ChunkIndex)
		{
			check(Ar.IsSaving() && bCooked);
			PlatformData->VTData->Chunks[ChunkIndex].BulkData.ResetBulkDataFlags(BulkDataMipFlags[ChunkIndex]);
		}
	}
}

void FTexturePlatformData::Serialize(FArchive& Ar, UTexture* Owner)
{
	SerializePlatformData(Ar, this, Owner, EPlatformDataSerializationFlags::None);
}

#if WITH_EDITORONLY_DATA

FString FTexturePlatformData::GetDerivedDataMipKeyString(int32 MipIndex, const FTexture2DMipMap& Mip) const
{
	const FString& KeyString = DerivedDataKey.Get<FString>();
	return FString::Printf(TEXT("%s_MIP%u_%dx%d"), *KeyString, MipIndex, Mip.SizeX, Mip.SizeY);
}

UE::DerivedData::FValueId FTexturePlatformData::MakeMipId(int32 MipIndex)
{
	return UE::DerivedData::FValueId::FromName(WriteToString<16>(TEXTVIEW("Mip"), MipIndex));
}

#endif // WITH_EDITORONLY_DATA

void FTexturePlatformData::SerializeCooked(FArchive& Ar, UTexture* Owner, bool bStreamable)
{
	EPlatformDataSerializationFlags Flags = EPlatformDataSerializationFlags::Cooked;
	if (bStreamable)
	{
		Flags |= EPlatformDataSerializationFlags::Streamable;
	}
	SerializePlatformData(Ar, this, Owner, Flags);
	if (Ar.IsLoading())
	{
		// Patch up Size as due to mips being stripped out during cooking it could be wrong.
		if (Mips.Num() > 0)
		{
			SizeX = Mips[0].SizeX;
			SizeY = Mips[0].SizeY;
			
			// SizeZ is not the same as NumSlices for texture arrays and cubemaps.
			if (Owner && Owner->IsA(UVolumeTexture::StaticClass()))
			{
				SetNumSlices(Mips[0].SizeZ);
			}
		}
		else if (VTData)
		{
			SizeX = VTData->Width;
			SizeY = VTData->Height;
		}
	}
}

/*------------------------------------------------------------------------------
	Texture derived data interface.
------------------------------------------------------------------------------*/

bool UTexture2DArray::GetMipData(int32 InFirstMipToLoad, TArray<FUniqueBuffer, TInlineAllocator<MAX_TEXTURE_MIP_COUNT>>& OutMipData)
{
	FTexturePlatformData* LocalPlatformData = GetPlatformData();
	const int32 ReadableMipCount = LocalPlatformData->Mips.Num() - (LocalPlatformData->GetNumMipsInTail() > 0 ? LocalPlatformData->GetNumMipsInTail() - 1 : 0);

	int32 OutputMipCount = ReadableMipCount - InFirstMipToLoad;

	check(OutputMipCount <= MAX_TEXTURE_MIP_COUNT);

	void* MipData[MAX_TEXTURE_MIP_COUNT] = {};
	int64 MipSizes[MAX_TEXTURE_MIP_COUNT];
	if (LocalPlatformData->TryLoadMipsWithSizes(InFirstMipToLoad, MipData, MipSizes, GetPathName()) == false)
	{
		// Unable to load mips from the cache. Rebuild the texture and try again.
		UE_LOG(LogTexture, Warning, TEXT("GetMipData failed for %s (%s)"),
			*GetPathName(), GPixelFormats[GetPixelFormat()].Name);
#if WITH_EDITOR
		if (!GetOutermost()->bIsCookedForEditor)
		{
			ForceRebuildPlatformData();
			if (LocalPlatformData->TryLoadMipsWithSizes(InFirstMipToLoad, MipData, MipSizes, GetPathName()) == false)
			{
				UE_LOG(LogTexture, Error, TEXT("TryLoadMipsWithSizes still failed after ForceRebuildPlatformData %s."), *GetPathName());
				return false;
			}
		}
#else // #if WITH_EDITOR
		return false;
#endif // WITH_EDITOR
	}

	for (int32 MipIndex = 0; MipIndex < OutputMipCount; MipIndex++)
	{
		OutMipData.Emplace(FUniqueBuffer::TakeOwnership(MipData[MipIndex], MipSizes[MipIndex], [](void* Ptr) {FMemory::Free(Ptr);} ));
	}
	return true;
}

void UTexture2D::GetMipData(int32 FirstMipToLoad, void** OutMipData)
{
	if (GetPlatformData()->TryLoadMips(FirstMipToLoad, OutMipData, GetPathName()) == false)
	{
		// Unable to load mips from the cache. Rebuild the texture and try again.
		UE_LOG(LogTexture,Warning,TEXT("GetMipData failed for %s (%s)"),
			*GetPathName(), GPixelFormats[GetPixelFormat()].Name);
#if WITH_EDITOR
		if (!GetOutermost()->bIsCookedForEditor)
		{
			ForceRebuildPlatformData();
			if (GetPlatformData()->TryLoadMips(FirstMipToLoad, OutMipData, GetPathName()) == false)
			{
				UE_LOG(LogTexture, Error, TEXT("TryLoadMips still failed after ForceRebuildPlatformData %s."), *GetPathName());
			}
		}
#endif // #if WITH_EDITOR
	}
}

void UTextureCube::GetMipData(int32 FirstMipToLoad, void** OutMipData)
{
	if (GetPlatformData()->TryLoadMips(FirstMipToLoad, OutMipData, GetPathName()) == false)
	{
		// Unable to load mips from the cache. Rebuild the texture and try again.
		UE_LOG(LogTexture,Warning,TEXT("GetMipData failed for %s (%s)"),
			*GetPathName(), GPixelFormats[GetPixelFormat()].Name);
#if WITH_EDITOR
		if (!GetOutermost()->bIsCookedForEditor)
		{
			ForceRebuildPlatformData();
			if (GetPlatformData()->TryLoadMips(FirstMipToLoad, OutMipData, GetPathName()) == false)
			{
				UE_LOG(LogTexture, Error, TEXT("TryLoadMips still failed after ForceRebuildPlatformData %s."), *GetPathName());
			}
		}
#endif // #if WITH_EDITOR
	}
}

void UTexture::UpdateCachedLODBias()
{
	CachedCombinedLODBias = UDeviceProfileManager::Get().GetActiveProfile()->GetTextureLODSettings()->CalculateLODBias(this);
}

#if WITH_EDITOR
void UTexture::CachePlatformData(bool bAsyncCache, bool bAllowAsyncBuild, bool bAllowAsyncLoading, ITextureCompressorModule* Compressor)
{
	//
	// NOTE this can be called off the main thread via FAsyncEncode<> for shadow/light maps!
	// This is why the compressor is passed in, to avoid calling LoadModule off the main thread.
	//

	TRACE_CPUPROFILER_EVENT_SCOPE(UTexture::CachePlatformData);

	FTexturePlatformData** PlatformDataLinkPtr = GetRunningPlatformData();
	if (PlatformDataLinkPtr)
	{
		FTexturePlatformData*& PlatformDataLink = *PlatformDataLinkPtr;
		if (Source.IsValid() && FApp::CanEverRender())
		{
			bool bPerformCache = false;

			ETextureCacheFlags CacheFlags =
				(bAsyncCache ? ETextureCacheFlags::Async : ETextureCacheFlags::None) |
				(bAllowAsyncBuild? ETextureCacheFlags::AllowAsyncBuild : ETextureCacheFlags::None) |
				(bAllowAsyncLoading? ETextureCacheFlags::AllowAsyncLoading : ETextureCacheFlags::None);

			ETextureEncodeSpeed EncodeSpeed = GetDesiredEncodeSpeed();

			//
			// Step 1 of the caching process is to determine whether or not we need to actually
			// do a cache. To check this, we compare the keys for the FetchOrBuild settings since we
			// know we always have those. If we need the FetchFirst key, we generate it later when
			// we know we're actually going to Cache()
			//
			TArray<FTextureBuildSettings> BuildSettingsFetchOrBuild;			
			TArray<FTexturePlatformData::FTextureEncodeResultMetadata> ResultMetadataFetchOrBuild;

			// These might be empty.
			TArray<FTextureBuildSettings> BuildSettingsFetchFirst;
			TArray<FTexturePlatformData::FTextureEncodeResultMetadata> ResultMetadataFetchFirst;

			switch (EncodeSpeed)
			{
			case ETextureEncodeSpeed::FinalIfAvailable:
				{
					GetBuildSettingsForRunningPlatform(*this, ETextureEncodeSpeed::Final, BuildSettingsFetchFirst, &ResultMetadataFetchFirst);
					GetBuildSettingsForRunningPlatform(*this, ETextureEncodeSpeed::Fast, BuildSettingsFetchOrBuild, &ResultMetadataFetchOrBuild);
					break;
				}
			case ETextureEncodeSpeed::Fast:
				{
					GetBuildSettingsForRunningPlatform(*this, ETextureEncodeSpeed::Fast, BuildSettingsFetchOrBuild, &ResultMetadataFetchOrBuild);
					break;
				}
			case ETextureEncodeSpeed::Final:
				{
					GetBuildSettingsForRunningPlatform(*this, ETextureEncodeSpeed::Final, BuildSettingsFetchOrBuild, &ResultMetadataFetchOrBuild);
					break;
				}
			default:
				{
					UE_LOG(LogTexture, Fatal, TEXT("Invalid encode speed in CachePlatformData"));
				}
			}

			// If we're open in a texture editor, then we might have custom build settings.
			if (TextureEditorCustomEncoding.IsValid())
			{
				TSharedPtr<FTextureEditorCustomEncode> CustomEncoding = TextureEditorCustomEncoding.Pin();
				if (CustomEncoding.IsValid() && // (threading) could have been destroyed between weak ptr IsValid and Pin
					CustomEncoding->bUseCustomEncode)
				{
					// If we are overriding, we don't want to have a fetch first, so just set our encode
					// speed to whatever we already have staged, then set those settings to the custom
					// ones.
					EncodeSpeed = (ETextureEncodeSpeed)BuildSettingsFetchOrBuild[0].RepresentsEncodeSpeedNoSend;
					BuildSettingsFetchFirst.Empty();
					ResultMetadataFetchFirst.Empty();

					for (int32 i = 0; i < BuildSettingsFetchOrBuild.Num(); i++)
					{
						FTextureBuildSettings& BuildSettings = BuildSettingsFetchOrBuild[i];
						FTexturePlatformData::FTextureEncodeResultMetadata& ResultMetadata = ResultMetadataFetchOrBuild[i];

						BuildSettings.OodleRDO = CustomEncoding->OodleRDOLambda;
						BuildSettings.bOodleUsesRDO = CustomEncoding->OodleRDOLambda ? true : false;
						BuildSettings.OodleEncodeEffort = CustomEncoding->OodleEncodeEffort;
						BuildSettings.OodleUniversalTiling = CustomEncoding->OodleUniversalTiling;

						ResultMetadata.OodleRDO = CustomEncoding->OodleRDOLambda;
						ResultMetadata.OodleEncodeEffort = CustomEncoding->OodleEncodeEffort;
						ResultMetadata.OodleUniversalTiling = CustomEncoding->OodleUniversalTiling;
						ResultMetadata.EncodeSpeed = (uint8)EncodeSpeed;

						ResultMetadata.bWasEditorCustomEncoding = true;
					}
				}
			}

			check(BuildSettingsFetchOrBuild.Num() == Source.GetNumLayers());

			// The only time we don't cache is if we a) have existing data and b) it matches what we want.
			bPerformCache = true;
			if (PlatformDataLink != nullptr)
			{
				bPerformCache = false;

				// Check if our keys match. If we have two, they both have to match, otherwise a change that only affects one
				// might not cause a rebuild, leading to confusion in the texture editor.
				if (IsUsingNewDerivedData() && (Source.GetNumLayers() == 1) && !BuildSettingsFetchOrBuild[0].bVirtualStreamable)
				{
					// DDC2 version
					using namespace UE::DerivedData;
					if (const FTexturePlatformData::FStructuredDerivedDataKey* ExistingDerivedDataKey = PlatformDataLink->FetchOrBuildDerivedDataKey.TryGet<FTexturePlatformData::FStructuredDerivedDataKey>())
					{
						if (*ExistingDerivedDataKey != CreateTextureDerivedDataKey(*this, CacheFlags, BuildSettingsFetchOrBuild[0]))
						{
							bPerformCache = true;
						}						
					}

					if (BuildSettingsFetchFirst.Num())
					{
						if (const FTexturePlatformData::FStructuredDerivedDataKey* ExistingDerivedDataKey = PlatformDataLink->FetchFirstDerivedDataKey.TryGet<FTexturePlatformData::FStructuredDerivedDataKey>())
						{
							if (*ExistingDerivedDataKey != CreateTextureDerivedDataKey(*this, CacheFlags, BuildSettingsFetchFirst[0]))
							{
								bPerformCache = true;
							}
						}
					}
				} // end if ddc2
				else
				{
					// DDC1 version.
					if (const FString* ExistingDerivedDataKey = PlatformDataLink->FetchOrBuildDerivedDataKey.TryGet<FString>())
					{
						FString DerivedDataKey;
						GetTextureDerivedDataKey(*this, BuildSettingsFetchOrBuild.GetData(), DerivedDataKey);
						if (*ExistingDerivedDataKey != DerivedDataKey)
						{
							bPerformCache = true;
						}
					}

					if (BuildSettingsFetchFirst.Num())
					{
						if (const FString* ExistingDerivedDataKey = PlatformDataLink->FetchFirstDerivedDataKey.TryGet<FString>())
						{
							FString DerivedDataKey;
							GetTextureDerivedDataKey(*this, BuildSettingsFetchFirst.GetData(), DerivedDataKey);
							if (*ExistingDerivedDataKey != DerivedDataKey)
							{
								bPerformCache = true;
							}
						}
					}
				} // end if ddc1
			} // end if checking existing data matches.

			if (bPerformCache)
			{
				// Release our resource if there is existing derived data.
				if (PlatformDataLink)
				{
					ReleaseResource();

					// Need to wait for any previous InitRHI() to complete before modifying PlatformData
					// We could remove this flush if InitRHI() was modified to not access PlatformData directly
					FlushRenderingCommands();
				}
				else
				{
					PlatformDataLink = new FTexturePlatformData();
				}

				PlatformDataLink->Cache(
					*this, 
					BuildSettingsFetchFirst.Num() ? BuildSettingsFetchFirst.GetData() : nullptr, 
					BuildSettingsFetchOrBuild.GetData(), 
					ResultMetadataFetchFirst.Num() ? ResultMetadataFetchFirst.GetData() : nullptr,
					ResultMetadataFetchOrBuild.GetData(),
					uint32(CacheFlags), 
					Compressor);
			}
		}
		else if (PlatformDataLink == NULL)
		{
			// If there is no source art available, create an empty platform data container.
			PlatformDataLink = new FTexturePlatformData();
		}

		UpdateCachedLODBias();
	}
}

void UTexture::BeginCachePlatformData()
{
	CachePlatformData(true, true, true);

#if 0 // don't cache in post load, this increases our peak memory usage, instead cache just before we save the package
	// enable caching in postload for derived data cache commandlet and cook by the book
	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	if (TPM && (TPM->RestrictFormatsToRuntimeOnly() == false))
	{
		ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
		TArray<ITargetPlatform*> Platforms = TPM->GetActiveTargetPlatforms();
		// Cache for all the shader formats that the cooking target requires
		for (int32 FormatIndex = 0; FormatIndex < Platforms.Num(); FormatIndex++)
		{
			BeginCacheForCookedPlatformData(Platforms[FormatIndex]);
		}
	}
#endif
}

void UTexture::BeginCacheForCookedPlatformData( const ITargetPlatform *TargetPlatform )
{
	TMap<FString, FTexturePlatformData*>* CookedPlatformDataPtr = GetCookedPlatformData();
	if (CookedPlatformDataPtr && !GetOutermost()->HasAnyPackageFlags(PKG_FilterEditorOnly))
	{
		TMap<FString,FTexturePlatformData*>& CookedPlatformData = *CookedPlatformDataPtr;
		
		// Make sure the pixel format enum has been cached.
		UTexture::GetPixelFormatEnum();

		// Retrieve formats to cache for target platform.
		bool HaveFetch = false;
		TArray< TArray<FTextureBuildSettings> > BuildSettingsToCacheFetch; // can be empty
		TArray< TArray<FTextureBuildSettings> > BuildSettingsToCacheFetchOrBuild;
		ETextureEncodeSpeed EncodeSpeed = GetDesiredEncodeSpeed();
		if (EncodeSpeed == ETextureEncodeSpeed::FinalIfAvailable)
		{			
			FTextureBuildSettings BuildSettingsFinal, BuildSettingsFast;
			GetTextureBuildSettings(*this, TargetPlatform->GetTextureLODSettings(), *TargetPlatform, ETextureEncodeSpeed::Final, BuildSettingsFinal, nullptr);
			GetTextureBuildSettings(*this, TargetPlatform->GetTextureLODSettings(), *TargetPlatform, ETextureEncodeSpeed::Fast, BuildSettingsFast, nullptr);

			// Try and fetch Final, but build Fast.
			GetBuildSettingsPerFormat(*this, BuildSettingsFinal, nullptr, TargetPlatform, ETextureEncodeSpeed::Final, BuildSettingsToCacheFetch, nullptr);
			GetBuildSettingsPerFormat(*this, BuildSettingsFast, nullptr, TargetPlatform, ETextureEncodeSpeed::Fast, BuildSettingsToCacheFetchOrBuild, nullptr);
			HaveFetch = true;
		}
		else
		{
			FTextureBuildSettings BuildSettings;
			GetTextureBuildSettings(*this, TargetPlatform->GetTextureLODSettings(), *TargetPlatform, EncodeSpeed, BuildSettings, nullptr);
			GetBuildSettingsPerFormat(*this, BuildSettings, nullptr, TargetPlatform, EncodeSpeed, BuildSettingsToCacheFetchOrBuild, nullptr);
		}
		
		// Cull redundant settings by comparing derived data keys.
		// There's an assumption here where we believe that if
		// a Fetch key is unique, so is its associated FetchOrBuild key,
		// and visa versa. Since we know we have FetchOrBuild, but not
		// necessarily Fetch, we just do the uniqueness check on FetchOrBuild.
		TArray<FString> BuildSettingsCacheKeysFetchOrBuild;
		for (int32 i=0; i<BuildSettingsToCacheFetchOrBuild.Num(); i++)
		{
			TArray<FTextureBuildSettings>& LayerBuildSettingsFetchOrBuild = BuildSettingsToCacheFetchOrBuild[i];
			check(LayerBuildSettingsFetchOrBuild.Num() == Source.GetNumLayers());

			FString DerivedDataKeyFetchOrBuild;
			GetTextureDerivedDataKey(*this, LayerBuildSettingsFetchOrBuild.GetData(), DerivedDataKeyFetchOrBuild);

			if (BuildSettingsCacheKeysFetchOrBuild.Find(DerivedDataKeyFetchOrBuild) != INDEX_NONE)
			{
				BuildSettingsToCacheFetchOrBuild.RemoveAtSwap(i);
				if (HaveFetch)
				{
					BuildSettingsToCacheFetch.RemoveAtSwap(i);
				}
				i--;
				continue;
			}

			BuildSettingsCacheKeysFetchOrBuild.Add(MoveTemp(DerivedDataKeyFetchOrBuild));
		}

		// Now have a unique list - kick off the caches.
		for (int32 SettingsIndex = 0; SettingsIndex < BuildSettingsCacheKeysFetchOrBuild.Num(); ++SettingsIndex)
		{
			// If we have two platforms that generate the same key, we can have duplicates (e.g. -run=DerivedDataCache  -TargetPlatform=WindowsEditor+Windows) 
			if (CookedPlatformData.Find(BuildSettingsCacheKeysFetchOrBuild[SettingsIndex]))
			{
				continue;
			}

			FTexturePlatformData* PlatformDataToCache = new FTexturePlatformData();
			PlatformDataToCache->Cache(
				*this,
				HaveFetch ? BuildSettingsToCacheFetch[SettingsIndex].GetData() : nullptr,
				BuildSettingsToCacheFetchOrBuild[SettingsIndex].GetData(),
				nullptr,
				nullptr,
				uint32(ETextureCacheFlags::Async | ETextureCacheFlags::InlineMips | ETextureCacheFlags::AllowAsyncBuild | ETextureCacheFlags::AllowAsyncLoading),
				nullptr
				);

			CookedPlatformData.Add(BuildSettingsCacheKeysFetchOrBuild[SettingsIndex], PlatformDataToCache);
		}
	}
}

void UTexture::ClearCachedCookedPlatformData( const ITargetPlatform* TargetPlatform )
{
	TMap<FString, FTexturePlatformData*> *CookedPlatformDataPtr = GetCookedPlatformData();

	if ( CookedPlatformDataPtr )
	{
		TMap<FString,FTexturePlatformData*>& CookedPlatformData = *CookedPlatformDataPtr;

		// Make sure the pixel format enum has been cached.
		UTexture::GetPixelFormatEnum();

		// Get the list of keys associated with the target platform so we know
		// what to evict from the CookedPlatformData array.

		// The cooked platform data map is keyed off of the FetchOrBuild ddc key, so we don't
		// bother generating the Fetch one.
		// Retrieve formats to cache for target platform.			
		TArray< TArray<FTextureBuildSettings> > BuildSettingsForPlatform;
		ETextureEncodeSpeed EncodeSpeed = GetDesiredEncodeSpeed();
		if (EncodeSpeed == ETextureEncodeSpeed::FinalIfAvailable ||
			EncodeSpeed == ETextureEncodeSpeed::Fast)
		{
			FTextureBuildSettings BuildSettings;
			GetTextureBuildSettings(*this, TargetPlatform->GetTextureLODSettings(), *TargetPlatform, ETextureEncodeSpeed::Fast, BuildSettings, nullptr);
			GetBuildSettingsPerFormat(*this, BuildSettings, nullptr, TargetPlatform, ETextureEncodeSpeed::Fast, BuildSettingsForPlatform, nullptr);
		}
		else
		{
			FTextureBuildSettings BuildSettings;
			GetTextureBuildSettings(*this, TargetPlatform->GetTextureLODSettings(), *TargetPlatform, ETextureEncodeSpeed::Final, BuildSettings, nullptr);
			GetBuildSettingsPerFormat(*this, BuildSettings, nullptr, TargetPlatform, ETextureEncodeSpeed::Final, BuildSettingsForPlatform, nullptr);
		}
		
		// If the cooked platform data contains our data, evict it
		// This also is likely to only be handful of entries... try using an array and having
		// FTargetPlatformSet track what platforms the data is valid for. Once all are cleared, wipe...
		for (int32 SettingsIndex = 0; SettingsIndex < BuildSettingsForPlatform.Num(); ++SettingsIndex)
		{
			check(BuildSettingsForPlatform[SettingsIndex].Num() == Source.GetNumLayers());

			FString DerivedDataKey;
			GetTextureDerivedDataKey(*this, BuildSettingsForPlatform[SettingsIndex].GetData(), DerivedDataKey);

			if ( CookedPlatformData.Contains( DerivedDataKey ) )
			{
				FTexturePlatformData *PlatformData = CookedPlatformData.FindAndRemoveChecked( DerivedDataKey );
				delete PlatformData;
			}
		}
	}
}

void UTexture::ClearAllCachedCookedPlatformData()
{
	TMap<FString, FTexturePlatformData*> *CookedPlatformDataPtr = GetCookedPlatformData();

	if ( CookedPlatformDataPtr )
	{
		TMap<FString, FTexturePlatformData*> &CookedPlatformData = *CookedPlatformDataPtr;

		for ( auto It : CookedPlatformData )
		{
			delete It.Value;
		}

		CookedPlatformData.Empty();
	}
}

bool UTexture::IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform)
{ 
	const TMap<FString, FTexturePlatformData*>* CookedPlatformDataPtr = GetCookedPlatformData();
	if (!CookedPlatformDataPtr)
	{
		// when WITH_EDITOR is 0, the derived classes don't compile their GetCookedPlatformData()
		// so this returns the base class (nullptr). Since this function only exists when
		// WITH_EDITOR is 1, we can assume we have this data. This code should never get hit.
		return true; 
	}

	// CookedPlatformData is keyed off of FetchOrBuild settings.
	ETextureEncodeSpeed EncodeSpeed = GetDesiredEncodeSpeed();

	TArray<TArray<FTextureBuildSettings>> BuildSettingsAllFormats;	
	if (EncodeSpeed == ETextureEncodeSpeed::Fast ||
		EncodeSpeed == ETextureEncodeSpeed::FinalIfAvailable)
	{
		FTextureBuildSettings BuildSettings;
		GetTextureBuildSettings(*this, TargetPlatform->GetTextureLODSettings(), *TargetPlatform, ETextureEncodeSpeed::Fast, BuildSettings, nullptr);
		GetBuildSettingsPerFormat(*this, BuildSettings, nullptr, TargetPlatform, ETextureEncodeSpeed::Fast, BuildSettingsAllFormats, nullptr);
	}
	else
	{
		FTextureBuildSettings BuildSettings;
		GetTextureBuildSettings(*this, TargetPlatform->GetTextureLODSettings(), *TargetPlatform, ETextureEncodeSpeed::Final, BuildSettings, nullptr);
		GetBuildSettingsPerFormat(*this, BuildSettings, nullptr, TargetPlatform, ETextureEncodeSpeed::Final, BuildSettingsAllFormats, nullptr);
	}

	
	

	for (const TArray<FTextureBuildSettings>& FormatBuildSettings : BuildSettingsAllFormats)
	{
		check(FormatBuildSettings.Num() == Source.GetNumLayers());

		FString DerivedDataKey;
		GetTextureDerivedDataKey(*this, FormatBuildSettings.GetData(), DerivedDataKey);

		FTexturePlatformData* PlatformData = (*CookedPlatformDataPtr).FindRef(DerivedDataKey);

		// begin cache hasn't been called
		if (!PlatformData)
		{
			if (!HasAnyFlags(RF_ClassDefaultObject) && Source.SizeX != 0 && Source.SizeY != 0)
			{
				// In case an UpdateResource happens, cooked platform data might be cleared and we might need to reschedule
				BeginCacheForCookedPlatformData(TargetPlatform);
			}
			return false;
		}

		if (PlatformData->AsyncTask && PlatformData->AsyncTask->Poll())
		{
			PlatformData->FinishCache();
		}

		if (PlatformData->AsyncTask)
		{
			return false;
		}
	}
	// if we get here all our stuff is cached :)
	return true;
}

bool UTexture::IsAsyncCacheComplete() const
{
	if (const FTexturePlatformData* const* RunningPlatformDataPtr = const_cast<UTexture*>(this)->GetRunningPlatformData())
	{
		if (const FTexturePlatformData* PlatformData = *RunningPlatformDataPtr)
		{
			if (!PlatformData->IsAsyncWorkComplete())
			{
				return false;
			}
		}
	}

	if (const TMap<FString, FTexturePlatformData*>* CookedPlatformDataPtr = const_cast<UTexture*>(this)->GetCookedPlatformData())
	{
		for (const TTuple<FString, FTexturePlatformData*>& Kvp : *CookedPlatformDataPtr)
		{
			if (const FTexturePlatformData* PlatformData = Kvp.Value)
			{
				if (!PlatformData->IsAsyncWorkComplete())
				{
					return false;
				}
			}
		}
	}

	return true;
}

bool UTexture::TryCancelCachePlatformData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UTexture::TryCancelCachePlatformData);

	FTexturePlatformData* const* RunningPlatformDataPtr = GetRunningPlatformData();
	if (RunningPlatformDataPtr)
	{
		FTexturePlatformData* RunningPlatformData = *RunningPlatformDataPtr;
		if (RunningPlatformData && !RunningPlatformData->TryCancelCache())
		{
			return false;
		}
	}

	TMap<FString, FTexturePlatformData*>* CookedPlatformDataPtr = GetCookedPlatformData();
	if (CookedPlatformDataPtr)
	{
		for (TTuple<FString, FTexturePlatformData*>& Kvp : *CookedPlatformDataPtr)
		{
			FTexturePlatformData* PlatformData = Kvp.Value;
			if (PlatformData && !PlatformData->TryCancelCache())
			{
				return false;
			}
		}
	}

	return true;
}

void UTexture::FinishCachePlatformData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UTexture::FinishCachePlatformData);

	FTexturePlatformData** RunningPlatformDataPtr = GetRunningPlatformData();
	if (RunningPlatformDataPtr)
	{
		FTexturePlatformData*& RunningPlatformData = *RunningPlatformDataPtr;
		
		if (Source.IsValid() && FApp::CanEverRender())
		{
			if ( RunningPlatformData == NULL )
			{
				// begin cache never called
				CachePlatformData();
			}
			else
			{
				// make sure async requests are finished
				RunningPlatformData->FinishCache();
			}
		}
	}

	UpdateCachedLODBias();
}

void UTexture::ForceRebuildPlatformData(uint8 InEncodeSpeedOverride /* =255 ETextureEncodeSpeedOverride::Disabled */)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UTexture::ForceRebuildPlatformData)

	FTexturePlatformData** PlatformDataLinkPtr = GetRunningPlatformData();
	if (PlatformDataLinkPtr && *PlatformDataLinkPtr && FApp::CanEverRender())
	{
		FTexturePlatformData *&PlatformDataLink = *PlatformDataLinkPtr;
		FlushRenderingCommands();

		ETextureEncodeSpeed EncodeSpeed;
		if (InEncodeSpeedOverride != (uint8)ETextureEncodeSpeedOverride::Disabled)
		{
			EncodeSpeed = (ETextureEncodeSpeed)InEncodeSpeedOverride;
		}
		else
		{
			EncodeSpeed = GetDesiredEncodeSpeed();
		}

		TArray<FTextureBuildSettings> BuildSettingsFetch;
		TArray<FTextureBuildSettings> BuildSettingsFetchOrBuild;
		TArray<FTexturePlatformData::FTextureEncodeResultMetadata> ResultMetadataFetch;
		TArray<FTexturePlatformData::FTextureEncodeResultMetadata> ResultMetadataFetchOrBuild;

		if (EncodeSpeed == ETextureEncodeSpeed::FinalIfAvailable)
		{
			GetBuildSettingsForRunningPlatform(*this, ETextureEncodeSpeed::Final, BuildSettingsFetch, &ResultMetadataFetch);
			GetBuildSettingsForRunningPlatform(*this, ETextureEncodeSpeed::Fast, BuildSettingsFetchOrBuild, &ResultMetadataFetchOrBuild);
		}
		else
		{
			GetBuildSettingsForRunningPlatform(*this, EncodeSpeed, BuildSettingsFetchOrBuild, &ResultMetadataFetchOrBuild);
		}
		
		check(BuildSettingsFetchOrBuild.Num() == Source.GetNumLayers());

		PlatformDataLink->Cache(
			*this,
			BuildSettingsFetch.GetData(),
			BuildSettingsFetchOrBuild.GetData(),
			ResultMetadataFetch.GetData(),
			ResultMetadataFetchOrBuild.GetData(),
			uint32(ETextureCacheFlags::ForceRebuild),
			nullptr
			);
	}
}

void UTexture::MarkPlatformDataTransient()
{
}
#endif // #if WITH_EDITOR

void UTexture::GetVirtualTextureBuildSettings(FVirtualTextureBuildSettings& OutSettings) const
{
	OutSettings.Init();
}

void UTexture::CleanupCachedRunningPlatformData()
{
	FTexturePlatformData **RunningPlatformDataPtr = GetRunningPlatformData();

	if ( RunningPlatformDataPtr )
	{
		FTexturePlatformData *&RunningPlatformData = *RunningPlatformDataPtr;

		if ( RunningPlatformData != NULL )
		{
			delete RunningPlatformData;
			RunningPlatformData = NULL;
		}
	}
}


void UTexture::SerializeCookedPlatformData(FArchive& Ar)
{
	if (IsTemplate() )
	{
		return;
	}

	DECLARE_SCOPE_CYCLE_COUNTER( TEXT("UTexture::SerializeCookedPlatformData"), STAT_Texture_SerializeCookedData, STATGROUP_LoadTime );

	UEnum* PixelFormatEnum = UTexture::GetPixelFormatEnum();

#if WITH_EDITOR
	if (Ar.IsCooking() && Ar.IsPersistent())
	{
		bCookedIsStreamable.Reset();
		if (Ar.CookingTarget()->AllowAudioVisualData())
		{
			TArray<FTexturePlatformData*> PlatformDataToSerialize;

			if (GetOutermost()->bIsCookedForEditor)
			{
				// For cooked packages, simply grab the current platform data and serialize it
				FTexturePlatformData** RunningPlatformDataPtr = GetRunningPlatformData();
				if (RunningPlatformDataPtr == nullptr)
				{
					return;
				}
				FTexturePlatformData* RunningPlatformData = *RunningPlatformDataPtr;
				if (RunningPlatformData == nullptr)
				{
					return;
				}
				PlatformDataToSerialize.Add(RunningPlatformData);
			}
			else
			{
				TMap<FString, FTexturePlatformData*> *CookedPlatformDataPtr = GetCookedPlatformData();
				if (CookedPlatformDataPtr == nullptr)
				{
					return;
				}

				// Kick off builds for anything we don't have on hand already.
				ETextureEncodeSpeed EncodeSpeed = GetDesiredEncodeSpeed();

				TArray< TArray<FTextureBuildSettings> > BuildSettingsToCacheFetch;
				TArray< TArray<FTextureBuildSettings> > BuildSettingsToCacheFetchOrBuild;
				if (EncodeSpeed == ETextureEncodeSpeed::FinalIfAvailable)
				{
					FTextureBuildSettings BuildSettingsFetch;
					GetTextureBuildSettings(*this, Ar.CookingTarget()->GetTextureLODSettings(), *Ar.CookingTarget(), ETextureEncodeSpeed::Final, BuildSettingsFetch, nullptr);
					GetBuildSettingsPerFormat(*this, BuildSettingsFetch, nullptr, Ar.CookingTarget(), ETextureEncodeSpeed::Final, BuildSettingsToCacheFetch, nullptr);

					FTextureBuildSettings BuildSettingsFetchOrBuild;
					GetTextureBuildSettings(*this, Ar.CookingTarget()->GetTextureLODSettings(), *Ar.CookingTarget(), ETextureEncodeSpeed::Fast, BuildSettingsFetchOrBuild, nullptr);
					GetBuildSettingsPerFormat(*this, BuildSettingsFetchOrBuild, nullptr, Ar.CookingTarget(), ETextureEncodeSpeed::Fast, BuildSettingsToCacheFetchOrBuild, nullptr);
				}
				else
				{
					FTextureBuildSettings BuildSettingsFetchOrBuild;
					GetTextureBuildSettings(*this, Ar.CookingTarget()->GetTextureLODSettings(), *Ar.CookingTarget(), EncodeSpeed, BuildSettingsFetchOrBuild, nullptr);
					GetBuildSettingsPerFormat(*this, BuildSettingsFetchOrBuild, nullptr, Ar.CookingTarget(), EncodeSpeed, BuildSettingsToCacheFetchOrBuild, nullptr);
				}

				for (int32 SettingIndex = 0; SettingIndex < BuildSettingsToCacheFetchOrBuild.Num(); SettingIndex++)
				{
					check(BuildSettingsToCacheFetchOrBuild[SettingIndex].Num() == Source.GetNumLayers());

					// CookedPlatformData is keyed off of the fetchorbuild key.
					FString DerivedDataKeyFetchOrBuild;
					GetTextureDerivedDataKey(*this, BuildSettingsToCacheFetchOrBuild[SettingIndex].GetData(), DerivedDataKeyFetchOrBuild);

					FTexturePlatformData *PlatformDataPtr = (*CookedPlatformDataPtr).FindRef(DerivedDataKeyFetchOrBuild);
					if (PlatformDataPtr == NULL)
					{
						PlatformDataPtr = new FTexturePlatformData();
						PlatformDataPtr->Cache(*this, 
							BuildSettingsToCacheFetch.Num() ? BuildSettingsToCacheFetch[SettingIndex].GetData() : nullptr,
							BuildSettingsToCacheFetchOrBuild[SettingIndex].GetData(), 
							nullptr,
							nullptr,
							uint32(ETextureCacheFlags::InlineMips | ETextureCacheFlags::Async), 
							nullptr);

						CookedPlatformDataPtr->Add(DerivedDataKeyFetchOrBuild, PlatformDataPtr);
					}
					PlatformDataToSerialize.Add(PlatformDataPtr);
				}
			}

			for (FTexturePlatformData* PlatformDataToSave : PlatformDataToSerialize)
			{
				PlatformDataToSave->FinishCache();

				// Update bCookedIsStreamable for later use in IsCandidateForTextureStreaming
				FStreamableRenderResourceState State;
				if (GetStreamableRenderResourceState(PlatformDataToSave, State))
				{
					bCookedIsStreamable = !bCookedIsStreamable.IsSet() ? State.bSupportsStreaming : (*bCookedIsStreamable || State.bSupportsStreaming);
				}

				FName PixelFormatName = PixelFormatEnum->GetNameByValue(PlatformDataToSave->PixelFormat);
				Ar << PixelFormatName;

				const int64 SkipOffsetLoc = Ar.Tell();
				int64 SkipOffset = 0;
				{
					Ar << SkipOffset;
				}

				// Pass streamable flag for inlining mips
				bool bTextureIsStreamable = GetTextureIsStreamableOnPlatform(*this, *Ar.CookingTarget());
				PlatformDataToSave->SerializeCooked(Ar, this, bTextureIsStreamable);

				SkipOffset = Ar.Tell() - SkipOffsetLoc;
				Ar.Seek(SkipOffsetLoc);
				Ar << SkipOffset;
				Ar.Seek(SkipOffsetLoc + SkipOffset);
			}
		}
		FName PixelFormatName = NAME_None;
		Ar << PixelFormatName;
	}
	else
#endif // #if WITH_EDITOR
	{

		FTexturePlatformData** RunningPlatformDataPtr = GetRunningPlatformData();
		if (RunningPlatformDataPtr == nullptr)
		{
			return;
		}

		CleanupCachedRunningPlatformData();
		FTexturePlatformData*& RunningPlatformData = *RunningPlatformDataPtr;
		check(RunningPlatformData == nullptr);
		RunningPlatformData = new FTexturePlatformData();

		FName PixelFormatName = NAME_None;
		Ar << PixelFormatName;
		while (PixelFormatName != NAME_None)
		{
			const int64 PixelFormatValue = PixelFormatEnum->GetValueByName(PixelFormatName);
			const EPixelFormat PixelFormat = (PixelFormatValue != INDEX_NONE && PixelFormatValue < PF_MAX) ? (EPixelFormat)PixelFormatValue : PF_Unknown;

			const int64 SkipOffsetLoc = Ar.Tell();
			int64 SkipOffset = 0;
			Ar << SkipOffset;
			if (RunningPlatformData->PixelFormat == PF_Unknown && GPixelFormats[PixelFormat].Supported)
			{
				// Extra arg is unused here because we're loading
				const bool bStreamable = false;
				RunningPlatformData->SerializeCooked(Ar, this, bStreamable);
			}
			else
			{
				Ar.Seek(SkipOffsetLoc + SkipOffset);
			}
			Ar << PixelFormatName;
		}
	}

	if (Ar.IsLoading())
	{
		LODBias = 0;
	}
}

int32 UTexture::GMinTextureResidentMipCount = NUM_INLINE_DERIVED_MIPS;

void UTexture::SetMinTextureResidentMipCount(int32 InMinTextureResidentMipCount)
{
	int32 MinAllowedMipCount = FPlatformProperties::RequiresCookedData() ? 1 : NUM_INLINE_DERIVED_MIPS;
	GMinTextureResidentMipCount = FMath::Max(InMinTextureResidentMipCount, MinAllowedMipCount);
}
