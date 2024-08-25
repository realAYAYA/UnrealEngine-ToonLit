// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureImportSettings.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"
#include "UObject/PropertyPortFlags.h"
#include "Misc/ConfigCacheIni.h"
#include "ImageCoreUtils.h"

#if WITH_EDITOR
#include "Engine/Texture.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(TextureImportSettings)

UTextureImportSettings::UTextureImportSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SectionName = TEXT("Importing");
}

void UTextureImportSettings::PostInitProperties()
{
	Super::PostInitProperties();
#if WITH_EDITOR
	if (IsTemplate())
	{
		ImportConsoleVariableValues();
	}
#endif // #if WITH_EDITOR
}

// Get the PNGInfill setting, with Default mapped to a concrete choice
ETextureImportPNGInfill UTextureImportSettings::GetPNGInfillMapDefault() const
{
	if ( PNGInfill == ETextureImportPNGInfill::Default )
	{
		// Default is OnlyOnBinaryTransparency unless changed by legacy config

		// get legacy config :
		bool bFillPNGZeroAlpha = true;
		if ( GConfig )
		{
			GConfig->GetBool(TEXT("TextureImporter"), TEXT("FillPNGZeroAlpha"), bFillPNGZeroAlpha, GEditorIni);		
		}
		
		return bFillPNGZeroAlpha ? ETextureImportPNGInfill::OnlyOnBinaryTransparency : ETextureImportPNGInfill::Never;
	}
	else
	{
		return PNGInfill;
	}
}

#if WITH_EDITOR
void UTextureImportSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property)
	{
		ExportValuesToConsoleVariables(PropertyChangedEvent.Property);
	}
}
#endif // #if WITH_EDITOR



namespace UE::TextureUtilitiesCommon
{
	#if WITH_EDITOR
	/* Set default properties on Texture for newly imported textures, or reimports.
	*  Should be called after all texture properties are set, before PostEditChange() 
	*/
	TEXTUREUTILITIESCOMMON_API void ApplyDefaultsForNewlyImportedTextures(UTexture * Texture, bool bIsReimport)
	{
	
		// things that are done for both fresh import and reimport :

		if ( bIsReimport )
		{
			Texture->UpdateOodleTextureSdkVersionToLatest();

			return;
		}

		// things that are for fresh import only :
		
		// SetModern does UpdateOodleTextureSdkVersionToLatest
		Texture->SetModernSettingsForNewOrChangedTexture();

		const UTextureImportSettings* Settings = GetDefault<UTextureImportSettings>();

		// cannot check for TC_Normalmap here
		//	because of the way NormalmapIdentification is delayed in Interchange
		//	it's harmless to just always turn it on
		//	it will be ignored if we are not TC_Normalmap
		//	OutBuildSettings.bNormalizeNormals = Texture.bNormalizeNormals && Texture.IsNormalMap();
		//if ( Texture->CompressionSettings == TC_Normalmap )
		{
			Texture->bNormalizeNormals = Settings->bEnableNormalizeNormals;
		}

		Texture->bUseNewMipFilter = Settings->bEnableFastMipFilter;

		// the pipeline before here will have set floating point textures to TC_HDR
		//	could alternatively check Texture->HasHDRSource
		if ( Texture->CompressionSettings == TC_HDR )
		{
			if ( Settings->CompressedFormatForFloatTextures == ETextureImportFloatingPointFormat::HDRCompressed_BC6 )
			{
				// use BC6H
				Texture->CompressionSettings = TC_HDR_Compressed;
			}
			else if ( Settings->CompressedFormatForFloatTextures == ETextureImportFloatingPointFormat::HDR_F32_or_F16 )
			{
				// set output format to match source format
				ETextureSourceFormat TSF = Texture->Source.GetFormat();
				if ( TSF == TSF_RGBA32F )
				{
					Texture->CompressionSettings = 	TC_HDR_F32;	
				}
				else if ( TSF == TSF_R32F )
				{
					Texture->CompressionSettings = 	TC_SingleFloat;
				}
				else if ( TSF == TSF_R16F )
				{
					Texture->CompressionSettings = 	TC_HalfFloat;
				}
				else
				{
					// else leave TC_HDR
					check( Texture->CompressionSettings == TC_HDR );
				}
			}
			else if ( Settings->CompressedFormatForFloatTextures == ETextureImportFloatingPointFormat::HDR_F16 )
			{
				// always use F16 HDR (legacy behavior)
				// leave TC_HDR
				check( Texture->CompressionSettings == TC_HDR );
			}
			else
			{
				// all cases should have been handled
				checkNoEntry();
			}
		}
	}
	#endif // WITH_EDITOR

	/* Get the default value for Texture->SRGB
	* ImportImageSRGB is the SRGB setting of the imported image
	*/
	TEXTUREUTILITIESCOMMON_API bool GetDefaultSRGB(TextureCompressionSettings TC, ETextureSourceFormat ImportImageFormat, bool ImportImageSRGB)
	{
		// Texture->SRGB sets the gamma correction of the platform texture we make
		//	so this is not just = ImportImageSRGB
		
		if ( TC == TC_Default || TC == TC_EditorIcon )
		{
			// DXT1,DXT3,R8G8B8 encodings
			//	we typically want SRGB on for these (for the platform encoding)
			// only exception is if the source is U8 linear
			//	 in that case, staying U8 linear probably preserves bits better
			//	 and ALSO we don't have the choice of turning on SRGB even if we want to
			//	 because of the way it is overloaded to mean both the source encoding and the platform encoding

			if ( ERawImageFormat::GetFormatNeedsGammaSpace( FImageCoreUtils::ConvertToRawImageFormat(ImportImageFormat) ) )
			{
				// if the imported image supported gamma (eg. U8)
				//	then we will set SRGB=false if it was U8-linear (can only happen with DDS U8 linear import, very rare)
				// note that texture SRGB flag in this case affects both the source interpretation and the platform encoding
				return ImportImageSRGB;
			}
			else
			{
				// counter-intuitively, U16 and F32 always want SRGB *on*
				//	the source will be treated as linear no matter what we set SRGB to
				//  SRGB will only affect the Platform encoding, so we prefer that to be sRGB color space
				return true;
			}
		}
		else
		{
			// TC_HDR, NormalMap, etc. we want SRGB off

			// TC_Grayscale we would prefer to have SRGB on, but default to off because
			//	G8 + SRGB is not supported well

			return false;
		}
	}

} // TextureUtilitiesCommon
