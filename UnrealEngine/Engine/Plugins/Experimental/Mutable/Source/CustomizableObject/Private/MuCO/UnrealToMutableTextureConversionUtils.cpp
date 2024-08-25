// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/UnrealToMutableTextureConversionUtils.h"

#include "MuR/MutableTrace.h"
#include "MuR/Image.h"
#include "Engine/Texture2D.h"
#include "ImageCoreUtils.h"
#include "ImageUtils.h"
#include "Async/ParallelFor.h"

#if WITH_EDITOR

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
        return EUnrealToMutableConversionError::CompositeImageDimensionMismatch;
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

void FlipGreenChannelRGBA32F(FImage& Image)
{
	TArrayView64<FLinearColor> ImageDataView = Image.AsRGBA32F();
	ParallelFor(ImageDataView.Num(),
		[&ImageDataView](uint32 p)
		{
			ImageDataView[p].G = 1.0f - FMath::Clamp(ImageDataView[p].G, 0.0f, 1.0f);
		});
}

void FlipGreenChannelBGRA8(FImage& Image)
{
	TArrayView64<FColor> ImageDataView = Image.AsBGRA8();
	ParallelFor(ImageDataView.Num(),
		[&ImageDataView](uint32 p)
		{
			ImageDataView[p].G = 255 - ImageDataView[p].G;
		});
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

} //namespace UnrealToMutableImageConversion_Internal


EUnrealToMutableConversionError ConvertTextureUnrealSourceToMutable(mu::Image* OutResult, UTexture2D* Texture, bool bIsNormalComposite, uint8 MipmapsToSkip)
{
	MUTABLE_CPUPROFILER_SCOPE(ConvertTextureUnrealToMutableTuple);

    using namespace UnrealToMutableImageConversion_Interanl;

	// Correct mips to skip to fit source data
	MipmapsToSkip = FMath::Clamp(MipmapsToSkip, 0, Texture->Source.GetNumMips()-1);

    const int32 LODs = 1;
    const int32 SizeX = Texture->Source.GetSizeX() >> MipmapsToSkip;
    const int32 SizeY = Texture->Source.GetSizeY() >> MipmapsToSkip;
	check(SizeX > 0 && SizeY > 0);

    ETextureSourceFormat Format = Texture->Source.GetFormat();
 
    ERawImageFormat::Type RawFormat = ConvertFormatSourceToRaw(Format);

	// Not true, we will convert it.
	// \TODO: Warn?
    //if (RawFormat == ERawImageFormat::RGBA32F)
    //{
    //    return MakeTuple(nullptr, EUnrealToMutableConversionError::UnsupportedFormat);
    //}

	// What if source data is not linear?
    FImage TempImage(SizeX, SizeY, 1, RawFormat, EGammaSpace::Linear);
	FImage TempImage2;

    if (!Texture->Source.GetMipData(TempImage.RawData, MipmapsToSkip))
    {
        return EUnrealToMutableConversionError::Unknown;
    }

    bool bFlipGreenChannel = Texture->bFlipGreenChannel;

    // If any post processes of the image is needed, convert to RGBA32F
    if (bIsNormalComposite)
    { 
		MUTABLE_CPUPROFILER_SCOPE(FlipOrComposite);

        TempImage.CopyTo(TempImage2, ERawImageFormat::RGBA32F, EGammaSpace::Linear);
		RawFormat = ERawImageFormat::RGBA32F;

        if (bFlipGreenChannel)
        {
			FlipGreenChannelRGBA32F(TempImage2);
			// Don't flip again below.
			bFlipGreenChannel = false;
		}

		// Prepare texture for use as normal composite.
		Normalize(TempImage2);
		BlurNormalForComposite(TempImage2);

        // The result is needed to TempImage
        // Swap internals so the memory allocations is potentially reused.
		TempImage2.Swap(TempImage);
    }
   
    const ERawImageFormat::Type MutableCompatibleFormat = Format == TSF_G8 
            ? ERawImageFormat::G8 
            : ERawImageFormat::BGRA8;

	if (MutableCompatibleFormat != RawFormat)
	{
		MUTABLE_CPUPROFILER_SCOPE(ToCompatibleFormat);
		TempImage.CopyTo(TempImage2, MutableCompatibleFormat, EGammaSpace::Linear);
		TempImage2.Swap(TempImage);
	}
	
	if (bFlipGreenChannel)
	{
		FlipGreenChannelBGRA8(TempImage);
	}

	switch (MutableCompatibleFormat)
	{
	case ERawImageFormat::G8:
	{
		MUTABLE_CPUPROFILER_SCOPE(NoConvert);
        check(LODs == 1);

		OutResult->Init(SizeX, SizeY, LODs, mu::EImageFormat::IF_L_UBYTE, mu::EInitializationType::NotInitialized);
		OutResult->DataStorage.GetInternalArray(0) = MoveTemp(TempImage.RawData);
		break;
	}

	case ERawImageFormat::BGRA8:
	{
		// Try to find out if the texture has and actually makes use of the alpha channel
		bool bHasAlphaChannel = 
			Texture->AdjustMinAlpha != Texture->AdjustMaxAlpha
			&& Texture->CompressionSettings != TextureCompressionSettings::TC_Normalmap
			&& !Texture->CompressionNoAlpha
			&& (Texture->CompressionForceAlpha 
				||
				FImageCore::DetectAlphaChannel(TempImage));

		// TODO: If we ever manage to get Pixel Format data on cook compilation time, remove the code that sets bHasAlphaChannel and just use Texture->HasAlphaChannel() here. Currently unreliable, it always returns EPixelFormat::PF_Unknown when cooking, which returns always "false" to HasAlphaChannel().
		if (bHasAlphaChannel)
		{
			MUTABLE_CPUPROFILER_SCOPE(ToRGBA);
			OutResult->Init(SizeX, SizeY, LODs, mu::EImageFormat::IF_RGBA_UBYTE, mu::EInitializationType::NotInitialized);
            check(LODs == 1);
            uint8* DataDest = OutResult->GetLODData(0);

			// Convert to RGBA8 while copying
			TArrayView64<FColor> ImageDataView = TempImage.AsBGRA8();
			ParallelFor(TEXT("MutableToRGBA"), ImageDataView.Num(), 16*1024,
				[DataDest, &ImageDataView](uint32 p)
				{
					DataDest[4 * p + 0] = ImageDataView[p].R;
					DataDest[4 * p + 1] = ImageDataView[p].G;
					DataDest[4 * p + 2] = ImageDataView[p].B;
					DataDest[4 * p + 3] = ImageDataView[p].A;
				});
		}
		else
		{
			MUTABLE_CPUPROFILER_SCOPE(ToRGB);

			// TODO: add support for a mu::IF_RGBX_UBYTE?			
			OutResult->Init(SizeX, SizeY, LODs, mu::EImageFormat::IF_RGB_UBYTE, mu::EInitializationType::NotInitialized);
			check(LODs == 1);
            uint8* DataDest = OutResult->GetLODData(0);

			// Convert to RGB8 while copying
			TArrayView64<FColor> ImageDataView = TempImage.AsBGRA8();
			ParallelFor(TEXT("MutableToRGB"), ImageDataView.Num(), 16 * 1024,
				[DataDest, &ImageDataView](uint32 p)
				{
					DataDest[3 * p + 0] = ImageDataView[p].R;
					DataDest[3 * p + 1] = ImageDataView[p].G;
					DataDest[3 * p + 2] = ImageDataView[p].B;
				});
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

    return EUnrealToMutableConversionError::Success;
}

#endif // WITH_EDITOR
