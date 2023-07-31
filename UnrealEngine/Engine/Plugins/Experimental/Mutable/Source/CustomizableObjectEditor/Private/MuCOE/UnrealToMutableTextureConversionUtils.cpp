// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/UnrealToMutableTextureConversionUtils.h"

#include "Containers/ArrayView.h"
#include "Containers/ContainersFwd.h"
#include "Containers/EnumAsByte.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureDefines.h"
#include "HAL/PlatformCrt.h"
#include "HAL/UnrealMemory.h"
#include "ImageCore.h"
#include "ImageCoreUtils.h"
//#include "MuCO/CustomizableObject.h"	// For the LogMutable logging category
#include "ImageUtils.h"
#include "Math/Color.h"
#include "Math/UnrealMathSSE.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "Templates/UnrealTemplate.h"

namespace UnrealToMutableImageConversion_Interanl
{

FORCEINLINE ERawImageFormat::Type ConvertFormatSourceToRaw(const ETextureSourceFormat SourceFormat) 
{
	return FImageCoreUtils::ConvertToRawImageFormat(SourceFormat);
}

EUnrealToMutableConversionError ApplyCompositeTexture(
        FImage& Image, 
        UTexture* CompositeTexture,
        const ECompositeTextureMode CompositeTextureMode,
        const float CompositePower)
{
    const int32 SizeX = CompositeTexture->Source.GetSizeX();
    const int32 SizeY = CompositeTexture->Source.GetSizeY();
    const ETextureSourceFormat SourceFormat = CompositeTexture->Source.GetFormat();

    const ERawImageFormat::Type RawFormat = ConvertFormatSourceToRaw(SourceFormat);

    if (RawFormat == ERawImageFormat::RGBA32F)
    {
        return EUnrealToMutableConversionError::CompositeUnsupportedFormat;
    }   

    FImage CompositeImage(SizeX, SizeY, 1, RawFormat, EGammaSpace::Linear);
    
    if(!CompositeTexture->Source.GetMipData(CompositeImage.RawData, 0))
    {
        return EUnrealToMutableConversionError::Unknown;
    }

    // Convert Composite Image to RGBA32F format and resize so both images have
    // the source image size.
    const bool bHaveSimilarAspect = FMath::IsNearlyEqual(
                float(SizeX) / float(SizeY), 
                float(Image.SizeX) / float(Image.SizeY), 
                KINDA_SMALL_NUMBER);
   
    if (SizeX < Image.SizeX || SizeY < Image.SizeY || !bHaveSimilarAspect)
    {
        return EUnrealToMutableConversionError::CompositeImageDimensionMissmatch;
    }

    {
        FImage TempImage;
        CompositeImage.ResizeTo(
            TempImage, Image.SizeX, Image.SizeY, ERawImageFormat::RGBA32F, EGammaSpace::Linear);
        
        Exchange(CompositeImage, TempImage);
    }

    TArrayView64<FLinearColor> ImageView = Image.AsRGBA32F();
    TArrayView64<FLinearColor> CompositeImageView = CompositeImage.AsRGBA32F();
   
    const size_t OutChannelOffset = [CompositeTextureMode]() -> size_t
        {
            switch(CompositeTextureMode)
            {
                case CTM_NormalRoughnessToRed:   return offsetof(FLinearColor, R);
                case CTM_NormalRoughnessToGreen: return offsetof(FLinearColor, G);
                case CTM_NormalRoughnessToBlue:  return offsetof(FLinearColor, B);
                case CTM_NormalRoughnessToAlpha: return offsetof(FLinearColor, A);
            }

            check(false);
            return 0;
        }(); 

    const int64 NumPixels = ImageView.Num();
    for (int64 I = 0; I < NumPixels; ++I)
    {
        const FVector Normal = FVector( 
                CompositeImageView[I].R * 2.0f - 1.0f,
                CompositeImageView[I].G * 2.0f - 1.0f,
                CompositeImageView[I].B * 2.0f - 1.0f); 

        // Is that C++ undefined behaviour?
        float* Value = reinterpret_cast<float*>(
                reinterpret_cast<uint8*>(&ImageView[I]) + OutChannelOffset);

        // See TextureCompressorModule.cpp:1924 for details. 
        // Toksvig estimation of variance
        float LengthN = FMath::Min( Normal.Size(), 1.0f );
        float Variance = ( 1.0f - LengthN ) / LengthN;
        Variance = FMath::Max( 0.0f, Variance - 0.00004f );

        Variance *= CompositePower;
        
        float Roughness = *Value;

        float a = Roughness * Roughness;
        float a2 = a * a;
        float B = 2.0f * Variance * (a2 - 1.0f);
        a2 = ( B - a2 ) / ( B - 1.0f );
        Roughness = FMath::Pow( a2, 0.25f );
        
        *Value = Roughness;
    }

    return EUnrealToMutableConversionError::Success;
}

void FlipGreenChannel(FImage& Image)
{
    TArrayView64<FLinearColor> ImageView = Image.AsRGBA32F();

    for (FLinearColor& Color : ImageView)
    {
        Color.G = 1.0f - FMath::Clamp(Color.G, 0.0f, 1.0f); 
    }
}

void Normalize(FImage& Image)
{
	TArrayView64<FLinearColor> ImageView = Image.AsRGBA32F();

	for (FLinearColor& Color : ImageView)
	{
		FVector3f Normal = (FVector3f(Color.R, Color.G, Color.B) * 2.0f - 1.0f).GetUnsafeNormal();

		Color.R = Normal.X * 0.5f + 0.5f;
		Color.G = Normal.Y * 0.5f + 0.5f;
		Color.B = Normal.Z * 0.5f + 0.5f;
	}
}

void BlurNormalForComposite(FImage& Image)
{
	TArrayView64<FLinearColor> ImageView = Image.AsRGBA32F();

	const int32 SizeX = Image.SizeX;
	const int32 SizeY = Image.SizeY;

	for (int32 Y = 0; Y < SizeY - 1; ++Y)
	{
		for (int32 X = 0; X < SizeX - 1; ++X)
		{
			const int64 Idx0 = Y * SizeX + X;
			const int64 Idx1 = Y * SizeX + (X + 1);
			const int64 Idx2 = (Y + 1) * SizeX + X;
			const int64 Idx3 = (Y + 1) * SizeX + (X + 1);

			// Simple 2x2 box filter in place to gather info about the top mip normals variance.
			ImageView[Idx0] = (ImageView[Idx0] + ImageView[Idx1] + ImageView[Idx2] + ImageView[Idx3]) * 0.25f;
		}
	}
}

} //namespace UnrealToMutableImageConversion_Interanl

TTuple<mu::ImagePtr, EUnrealToMutableConversionError> ConvertTextureUnrealToMutable(UTexture2D* Texture, bool bIsNormalComposite)
{
    using namespace UnrealToMutableImageConversion_Interanl;

    const int32 LODs = 1;
    const int32 SizeX = Texture->Source.GetSizeX();
    const int32 SizeY = Texture->Source.GetSizeY();
    ETextureSourceFormat Format = Texture->Source.GetFormat();
 
    const ERawImageFormat::Type RawFormat = ConvertFormatSourceToRaw(Format);

    if (RawFormat == ERawImageFormat::RGBA32F)
    {
        return MakeTuple(nullptr, EUnrealToMutableConversionError::UnsupportedFormat);
    }


    FImage TempImage0(SizeX, SizeY, 1, RawFormat, EGammaSpace::Linear);
    FImage TempImage1;
    
    if (!Texture->Source.GetMipData(TempImage0.RawData, 0))
    {
        return MakeTuple(nullptr, EUnrealToMutableConversionError::Unknown);
    }

    const bool bFlipGreenChannel = Texture->bFlipGreenChannel;
    
	//const bool bApplyCompositeTexture =
    //        static_cast<bool>(Texture->CompositeTexture) &&
    //        Texture->CompositeTextureMode != ECompositeTextureMode::CTM_Disabled;

    // If any post processes of the image is needed, convert to RGBA32F
    if (bFlipGreenChannel || bIsNormalComposite)
    { 

        TempImage0.CopyTo(TempImage1, ERawImageFormat::RGBA32F, EGammaSpace::Linear);

        if (bFlipGreenChannel)
        {
            FlipGreenChannel(TempImage1);
        }

		// Prepare texture for use as normal composite.
		if (bIsNormalComposite)
		{
			Normalize(TempImage1);
			BlurNormalForComposite(TempImage1);
		}

		// Don't do this here as the result depends on the generation of the mips  
        //if (bApplyCompositeTexture)
        //{
        //    EUnrealToMutableConversionError CompositeTextureError = 
        //        ApplyCompositeTexture(
        //            TempImage1, 
        //            Texture->CompositeTexture, 
        //            Texture->CompositeTextureMode,
        //            Texture->CompositePower);

        //    // This should probably only be a warning indicating the composite could not
        //    // be applied.
        //    if (CompositeTextureError != EUnrealToMutableConversionError::Success)
        //    {
        //        return MakeTuple(nullptr, CompositeTextureError);
        //    }
        //}

        // The result is needed to TempImage0
        // Swap internals so the memory allocations is potentially reused.
        Exchange(TempImage1, TempImage0); 
    }
   
    const ERawImageFormat::Type MutableCompatibleFormat = Format == TSF_G8 
            ? ERawImageFormat::G8 
            : ERawImageFormat::BGRA8;

    TempImage0.CopyTo(TempImage1, MutableCompatibleFormat, EGammaSpace::Linear);

    // Convert to RGBA8 in place if needed 
    if (MutableCompatibleFormat == ERawImageFormat::BGRA8)
    {
        TArrayView64<FColor> ImageDataView = TempImage1.AsBGRA8();

        for (FColor& Color : ImageDataView)
        {
            Color = FColor(Color.ToPackedABGR()); 
        } 
    }

	mu::ImagePtr Image;
	switch (MutableCompatibleFormat)
	{
	case ERawImageFormat::G8:
		Image = new mu::Image(SizeX, SizeY, LODs, mu::EImageFormat::IF_L_UBYTE);
		FMemory::Memcpy(Image->GetData(), TempImage1.RawData.GetData(), Image->GetDataSize());
		break;

	case ERawImageFormat::BGRA8:
	{
		// Try to find out if the texture has and actually makes use of the alpha channel
		bool bHasAlphaChannel = true;
		FImage SourceImage;
		if (FImageUtils::GetTexture2DSourceImage(Texture, SourceImage))
		{
			bHasAlphaChannel = Texture->AdjustMinAlpha != Texture->AdjustMaxAlpha
				&& Texture->CompressionSettings != TextureCompressionSettings::TC_Normalmap
				&& !Texture->CompressionNoAlpha
				&& (Texture->CompressionForceAlpha
					|| FImageCore::DetectAlphaChannel(SourceImage));
		}

		// TODO: If we ever manage to get Pixel Format data on cook compilation time, remove the code that sets bHasAlphaChannel and just use Texture->HasAlphaChannel() here. Currently unreliable, it always returns EPixelFormat::PF_Unknown when cooking, which returns always "false" to HasAlphaChannel().
		if (bHasAlphaChannel)
		{
			Image = new mu::Image(SizeX, SizeY, LODs, mu::EImageFormat::IF_RGBA_UBYTE);
			FMemory::Memcpy(Image->GetData(), TempImage1.RawData.GetData(), Image->GetDataSize());
		}
		else
		{
			Image = new mu::Image(SizeX, SizeY, LODs, mu::EImageFormat::IF_RGB_UBYTE);
			// Manual copy
			const uint8* DataSource = TempImage1.RawData.GetData();
			uint8* DataDest = Image->GetData();
			uint8* DataDestEnd = DataDest + Image->GetDataSize();
			for (int32 Pixel=0; DataDest!= DataDestEnd; )
			{
				DataDest[0] = DataSource[0];
				DataDest[1] = DataSource[1];
				DataDest[2] = DataSource[2];
				DataDest += 3;
				DataSource += 4;
			}
		}

		//FString Msg = FString::Printf(TEXT("Alpha channel is %s for %s"), bHasAlphaChannel ? TEXT("enabled") : TEXT("disabled"), *Texture->GetName());
		//UE_LOG(LogMutable, VeryVerbose, TEXT("%s"), *Msg);

		break;
	}

	default:
		// Format not supported yet?
		check(false);
		break;
	}

    return MakeTuple(Image, EUnrealToMutableConversionError::Success);
}
