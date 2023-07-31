// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
ImageUtils.cpp: Image utility functions.
=============================================================================*/

#include "ImageUtils.h"

#include "CubemapUnwrapUtils.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureCubeArray.h"
#include "Engine/VolumeTexture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Logging/MessageLog.h"
#include "Misc/FileHelper.h"
#include "Misc/ObjectThumbnail.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ImageCoreUtils.h"
#include "DDSFile.h"

DEFINE_LOG_CATEGORY_STATIC(LogImageUtils, Log, All);

#define LOCTEXT_NAMESPACE "ImageUtils"


static IImageWrapperModule * GetOrLoadImageWrapperModule()
{
	static FName ImageWrapperName("ImageWrapper");

	if ( IsInGameThread() )
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(ImageWrapperName);
		return &ImageWrapperModule;
	}
	else
	{
		IImageWrapperModule * ImageWrapperModule = FModuleManager::GetModulePtr<IImageWrapperModule>(ImageWrapperName);

		if ( ImageWrapperModule == nullptr )
		{
			UE_LOG(LogImageUtils,Warning,TEXT("Not on GameThread, cannot load ImageWrapper.  Do on main thread in startup."));

			// LoadModule needs to be done on the Game thread as part of your initialization
			//   before any thread tries to run this code
			// Engine does this.  If you are making a stand-alone app and hit this error,
			//   add this to your initialization on the Game thread :
			// FModuleManager::LoadModuleChecked<IImageWrapperModule>("ImageWrapper");
		}

		return ImageWrapperModule;
	}
}


bool FImageUtils::SaveImageAutoFormat(const TCHAR * FileName, const FImageView & InImage, int32 Quality)
{
	IImageWrapperModule * ImageWrapperModule = GetOrLoadImageWrapperModule();
	if ( ImageWrapperModule == nullptr )
	{
		return false;
	}

	EImageFormat ToFormat = ImageWrapperModule->GetDefaultOutputFormat(InImage.Format);
	if ( ToFormat == EImageFormat::Invalid )
	{
		return false;
	}
	
	FString Scratch;
	if ( ImageWrapperModule->GetImageFormatFromExtension(FileName) != ToFormat )
	{
		const TCHAR * Extension = ImageWrapperModule->GetExtension(ToFormat);
		
		Scratch = FPaths::GetBaseFilename(FileName, false);
		Scratch += TEXT('.');
		Scratch += Extension;
		FileName = *Scratch;
	}

	TArray64<uint8> Buffer;
	if ( ! ImageWrapperModule->CompressImage(Buffer,ToFormat,InImage,Quality) )
	{
		return false;
	}

	return FFileHelper::SaveArrayToFile(Buffer,FileName);
}

bool FImageUtils::LoadImage(const TCHAR * Filename, FImage & OutImage)
{
	TArray64<uint8> Buffer;
	if ( ! FFileHelper::LoadFileToArray(Buffer, Filename) )
	{
		return false;
	}

	return DecompressImage(Buffer.GetData(),Buffer.Num(),OutImage);
}

bool FImageUtils::SaveImageByExtension(const TCHAR * Filename, const FImageView & InImage, int32 Quality)
{
	TArray64<uint8> Buffer;
	if ( !  CompressImage(Buffer, Filename, InImage,Quality) )
	{
		return false;
	}

	return FFileHelper::SaveArrayToFile(Buffer,Filename);
}

// OutImage is allocated and filled, any existing contents are discarded
bool FImageUtils::DecompressImage(const void* InCompressedData, int64 InCompressedSize, FImage & OutImage)
{
	IImageWrapperModule * ImageWrapperModule = GetOrLoadImageWrapperModule();
	if ( ImageWrapperModule == nullptr )
	{
		return false;
	}

	return ImageWrapperModule->DecompressImage(InCompressedData,InCompressedSize,OutImage);
}

// OutData is filled with the file format encoded data
//	 lossy conversion may be done if necessary
bool FImageUtils::CompressImage(TArray64<uint8> & OutData, const TCHAR * ToFormatExtension, const FImageView & InImage, int32 Quality)
{
	IImageWrapperModule * ImageWrapperModule = GetOrLoadImageWrapperModule();
	if ( ImageWrapperModule == nullptr )
	{
		return false;
	}

	
	EImageFormat ToFormat;
	
	if ( ToFormatExtension == nullptr )
	{
		ToFormat = ImageWrapperModule->GetDefaultOutputFormat(InImage.Format);
	}
	else
	{
		ToFormat = ImageWrapperModule->GetImageFormatFromExtension(ToFormatExtension);
	}

	if ( ToFormat == EImageFormat::Invalid )
	{
		return false;
	}

	return ImageWrapperModule->CompressImage(OutData,ToFormat,InImage,Quality);
}

bool FImageUtils::ExportTextureSourceToDDS(TArray64<uint8> & OutData, UTexture * Texture, int BlockIndex, int LayerIndex)
{
#if WITH_EDITORONLY_DATA	

	const FTextureSource & Source = Texture->Source;

	if ( ! Source.IsValid() )
	{
		UE_LOG(LogImageUtils,Warning,TEXT("ExportTextureSourceToDDS: Texture does not have source (%s)"),*Texture->GetName());
		return false;
	}

	check( BlockIndex < Source.GetNumBlocks() );
	check( LayerIndex < Source.GetNumLayers() );
	
	FTextureSourceBlock Block;
	Source.GetBlock(BlockIndex, Block);

	int32 SizeX = Block.SizeX;
	int32 SizeY = Block.SizeY;
	int32 NumSlices = Block.NumSlices;
	int32 NumMips = Block.NumMips;

	int32 Dimension;
	int32 SizeZ;
	int32 ArraySize;
	uint32 CreateFlags = 0;
	ETextureClass TextureClass = Texture->GetTextureClass();
	if ( TextureClass == ETextureClass::TwoD )
	{
		Dimension = 2;
		SizeZ = 1;
		ArraySize = NumSlices;
	}
	else if ( TextureClass == ETextureClass::Cube )
	{
		Dimension = 2;
		SizeZ = 1;

		// IsLongLatCubemap() is not right
		// just guess from NumSlices

		//bool bIsLongLatCubemap = Source.IsLongLatCubemap();
		bool bIsLongLatCubemap = ( NumSlices == 1 ); 

		if ( bIsLongLatCubemap )
		{
			// just export as 2d texture
			check( NumSlices == 1 );
			ArraySize = 1;
		}
		else
		{
			check( NumSlices == 6 );
			ArraySize = 6;
			CreateFlags = UE::DDS::FDDSFile::CREATE_FLAG_CUBEMAP;
		}
	}
	else if ( TextureClass == ETextureClass::CubeArray )
	{
		Dimension = 2;
		SizeZ = 1;

		// IsLongLatCubemap() should be right for CubeArrays
		bool bIsLongLatCubemap = Source.IsLongLatCubemap() || (NumSlices%6) != 0; 

		if ( bIsLongLatCubemap )
		{
			// just export as 2d texture array
			ArraySize = NumSlices;
		}
		else
		{
			check( (NumSlices%6) == 0 );
			ArraySize = NumSlices;
			CreateFlags = UE::DDS::FDDSFile::CREATE_FLAG_CUBEMAP;
		}
	}
	else if ( TextureClass == ETextureClass::Volume )
	{
		Dimension = 3;
		SizeZ = NumSlices;
		ArraySize = 1;
	}
	else
	{
		checkf(false, TEXT("unexpected TextureClass"));
		return false;
	}
	
	ERawImageFormat::Type RawFormat = FImageCoreUtils::ConvertToRawImageFormat(Source.GetFormat(LayerIndex));
	EGammaSpace GammaSpace = Source.GetGammaSpace(LayerIndex);
	UE::DDS::EDXGIFormat DXGIFormat = UE::DDS::DXGIFormatFromRawFormat(RawFormat,GammaSpace);
	
	if ( RawFormat == ERawImageFormat::BGRE8 )
	{
		// convert BGRE to Linear float? or just export the bytes as they are in BGRA ?
		UE_LOG(LogImageUtils,Warning,TEXT("DDS export to BGRE8 will write the bytes as they were in BGRA, not converted to float linear"));
	}

	UE_LOG(LogImageUtils,Display,TEXT("Exporting DDS Dimension=%d SizeX=%d SizeY=%d SizeZ=%d NumMips=%d ArraySize=%d"),Dimension, SizeX,SizeY,SizeZ,NumMips,ArraySize);	
	
	UE::DDS::EDDSError Error;
	UE::DDS::FDDSFile * DDS = UE::DDS::FDDSFile::CreateEmpty(Dimension, SizeX,SizeY,SizeZ,NumMips,ArraySize, DXGIFormat,CreateFlags, &Error);
	if ( DDS == nullptr || Error != UE::DDS::EDDSError::OK )
	{
		UE_LOG(LogImageUtils,Warning,TEXT("FDDSFile::CreateEmpty (Error=%d)"),(int)Error);			
		return false;
	}
	
	// delete DDS at scope exit :
	TUniquePtr<UE::DDS::FDDSFile> DDSPtr(DDS);

	check( DDS->Validate() == UE::DDS::EDDSError::OK );
	
	// blit into the mips:
	
	for(int MipIndex=0;MipIndex<NumMips;MipIndex++)
	{
		FTextureSource::FMipLock MipLock(FTextureSource::ELockState::ReadOnly,const_cast<FTextureSource *>(&Source),BlockIndex,LayerIndex,MipIndex);

		if ( DDS->Dimension == 3 )
		{
			check( DDS->Mips.Num() == DDS->MipCount );
			check( DDS->Mips[MipIndex].Depth == MipLock.Image.NumSlices );

			DDS->FillMip( MipLock.Image, MipIndex );
		}
		else
		{
			// DDS->Mips[] contains both mips and arrays
			check( DDS->Mips.Num() == DDS->MipCount * NumSlices );

			for(int SliceIndex=0;SliceIndex<NumSlices;SliceIndex++)
			{
				FImageView MipSlice = MipLock.Image.GetSlice(SliceIndex);
				
				// DDS Mips[] array has whole mip chain of each slice, then next slice
				// we have the opposite (all slices of top mip first, then next mip)
				int DDSMipIndex = SliceIndex * DDS->MipCount + MipIndex;
				
				DDS->FillMip( MipSlice, DDSMipIndex );
			}
		}
	}

	Error = DDS->WriteDDS(OutData);
	if ( Error != UE::DDS::EDDSError::OK )
	{
		UE_LOG(LogImageUtils,Warning,TEXT("FDDSFile::WriteDDS failed (Error=%d)"),(int)Error);			
		return false;
	}

	return true;
#else
	return false;
#endif
}

/**
 * Returns data containing the pixmap of the passed in rendertarget.
 * @param TexRT - The 2D rendertarget from which to read pixmap data.
 * @param RawData - an array to be filled with pixel data.
 * @return true if RawData has been successfully filled.
 */
bool FImageUtils::GetRawData(UTextureRenderTarget2D* TexRT, TArray64<uint8>& RawData)
{
	FRenderTarget* RenderTarget = TexRT->GameThread_GetRenderTargetResource();
	EPixelFormat Format = TexRT->GetFormat();

	int32 ImageBytes = CalculateImageBytes(TexRT->SizeX, TexRT->SizeY, 0, Format);
	RawData.AddUninitialized(ImageBytes);
	bool bReadSuccess = false;
	switch (Format)
	{
	case PF_FloatRGBA:
		{
		TArray<FFloat16Color> FloatColors;
		bReadSuccess = RenderTarget->ReadFloat16Pixels(FloatColors);
		FMemory::Memcpy(RawData.GetData(), FloatColors.GetData(), ImageBytes);
		}
		break;
	case PF_B8G8R8A8:
		bReadSuccess = RenderTarget->ReadPixelsPtr((FColor*)RawData.GetData());
		break;
	default:
		// bReadSuccess == false
		UE_LOG(LogImageUtils,Warning,TEXT("RenderTarget GetRawData PixelFormat no supported : %s") , *(StaticEnum<EPixelFormat>()->GetDisplayNameTextByValue(Format).ToString()) );
		break;
	}
	if (bReadSuccess == false)
	{
		RawData.Empty();
	}
	return bReadSuccess;
}

bool FImageUtils::GetRenderTargetImage(UTextureRenderTarget2D* TexRT, FImage & Image)
{
	Image = FImage();
	
	// see ETextureRenderTargetFormat
	// for allowed formats

	TArray64<uint8> RawData;
	if ( ! GetRawData(TexRT,RawData) )
	{
		return false;
	}
	
	// @todo Oodle : in theory the RenderTarget knows its gamma
	//   but from what I've seen it's usually wrong?
	// TexRT->GetDisplayGamma() or TexRT->SRGB or TexRT->IsSRGB() , OMG

	Image.RawData = MoveTemp(RawData);
	Image.SizeX = TexRT->SizeX;
	Image.SizeY = TexRT->SizeY;
	Image.NumSlices = 1;
	
	EPixelFormat PixelFormat = TexRT->GetFormat();
	switch(PixelFormat)
	{
	case PF_FloatRGBA:
		Image.Format = ERawImageFormat::RGBA16F;
		Image.GammaSpace = EGammaSpace::Linear;
		break;	
	case PF_B8G8R8A8:
		Image.Format = ERawImageFormat::BGRA8;
		Image.GammaSpace = EGammaSpace::sRGB;
		break;
	default:
		// should not get here because GetRawData would have returned false
		check(0);
		return false;
	}

	return true;
}


/**
 * Resizes the given image using a simple average filter and stores it in the destination array.
 *
 * @param SrcWidth		Source image width.
 * @param SrcHeight		Source image height.
 * @param SrcData		Source image data.
 * @param DstWidth		Destination image width.
 * @param DstHeight		Destination image height.
 * @param DstData		Destination image data.
 * @param bLinearSpace	If true, convert colors into linear space before interpolating (slower but more accurate)
 */
void FImageUtils::ImageResize(int32 SrcWidth, int32 SrcHeight, const TArray<FColor> &SrcData, int32 DstWidth, int32 DstHeight, TArray<FColor> &DstData, bool bLinearSpace, bool bForceOpaqueOutput)
{
	DstData.Empty(DstWidth*DstHeight);
	DstData.AddZeroed(DstWidth*DstHeight);

	ImageResize(SrcWidth, SrcHeight, TArrayView<const FColor>(SrcData), DstWidth, DstHeight, TArrayView<FColor>(DstData), bLinearSpace, bForceOpaqueOutput);
}

/**
 * Resizes the given image using a simple average filter and stores it in the destination array.  This version constrains aspect ratio.
 * Accepts TArrayViews but requires that DstData be pre-sized appropriately
 *
 * @param SrcWidth	Source image width.
 * @param SrcHeight	Source image height.
 * @param SrcData	Source image data.
 * @param DstWidth	Destination image width.
 * @param DstHeight Destination image height.
 * @param DstData	Destination image data. (must already be sized to DstWidth*DstHeight)
 */
void FImageUtils::ImageResize(int32 SrcWidth, int32 SrcHeight, const TArrayView<const FColor> &SrcData, int32 DstWidth, int32 DstHeight, const TArrayView<FColor> &DstData, bool bLinearSpace, bool bForceOpaqueOutput)
{
	//@todo Oodle : this could be better ; investigate how widely it's used

	check(SrcData.Num() >= SrcWidth * SrcHeight);
	check(DstData.Num() >= DstWidth * DstHeight);

	float SrcX = 0;
	float SrcY = 0;

	const float StepSizeX = SrcWidth / (float)DstWidth;
	const float StepSizeY = SrcHeight / (float)DstHeight;

	for(int32 Y=0; Y<DstHeight;Y++)
	{
		int32 PixelPos = Y * DstWidth;
		SrcX = 0.0f;	
	
		for(int32 X=0; X<DstWidth; X++)
		{
			int32 PixelCount = 0;
			float EndX = SrcX + StepSizeX;
			float EndY = SrcY + StepSizeY;
			
			// Generate a rectangular region of pixels and then find the average color of the region.
			int32 PosY = FMath::TruncToInt(SrcY+0.5f);
			PosY = FMath::Clamp<int32>(PosY, 0, (SrcHeight - 1));

			int32 PosX = FMath::TruncToInt(SrcX+0.5f);
			PosX = FMath::Clamp<int32>(PosX, 0, (SrcWidth - 1));

			int32 EndPosY = FMath::TruncToInt(EndY+0.5f);
			EndPosY = FMath::Clamp<int32>(EndPosY, 0, (SrcHeight - 1));

			int32 EndPosX = FMath::TruncToInt(EndX+0.5f);
			EndPosX = FMath::Clamp<int32>(EndPosX, 0, (SrcWidth - 1));

			FColor FinalColor;
			if(bLinearSpace)
			{
				FLinearColor LinearStepColor(0.0f,0.0f,0.0f,0.0f);
				for(int32 PixelX = PosX; PixelX <= EndPosX; PixelX++)
				{
					for(int32 PixelY = PosY; PixelY <= EndPosY; PixelY++)
					{
						int32 StartPixel =  PixelX + PixelY * SrcWidth;

						// Convert from gamma space to linear space before the addition.
						LinearStepColor += SrcData[StartPixel];
						PixelCount++;
					}
				}
				LinearStepColor /= (float)PixelCount;

				// Convert back from linear space to gamma space.
				FinalColor = LinearStepColor.ToFColor(true);
			}
			else
			{
				FVector4 StepColor(0,0,0,0);
				for(int32 PixelX = PosX; PixelX <= EndPosX; PixelX++)
				{
					for(int32 PixelY = PosY; PixelY <= EndPosY; PixelY++)
					{
						int32 StartPixel =  PixelX + PixelY * SrcWidth;
						StepColor.X += (float)SrcData[StartPixel].R;
						StepColor.Y += (float)SrcData[StartPixel].G;
						StepColor.Z += (float)SrcData[StartPixel].B;
						StepColor.W += (float)SrcData[StartPixel].A;
						PixelCount++;
					}
				}
				uint8 FinalR = FMath::Clamp(FMath::TruncToInt32(StepColor.X / (float)PixelCount), 0, 255);
				uint8 FinalG = FMath::Clamp(FMath::TruncToInt32(StepColor.Y / (float)PixelCount), 0, 255);
				uint8 FinalB = FMath::Clamp(FMath::TruncToInt32(StepColor.Z / (float)PixelCount), 0, 255);
				uint8 FinalA = FMath::Clamp(FMath::TruncToInt32(StepColor.W / (float)PixelCount), 0, 255);
				FinalColor = FColor(FinalR, FinalG, FinalB, FinalA);
			}

			if ( bForceOpaqueOutput )
			{
				FinalColor.A = 255;
			}

			// Store the final averaged pixel color value.
			DstData[PixelPos] = FinalColor;

			SrcX = EndX;	
			PixelPos++;
		}

		SrcY += StepSizeY;
	}
}

/**
 * Resizes the given image using a simple average filter and stores it in the destination array.
 *
 * @param SrcWidth		Source image width.
 * @param SrcHeight		Source image height.
 * @param SrcData		Source image data.
 * @param DstWidth		Destination image width.
 * @param DstHeight		Destination image height.
 * @param DstData		Destination image data.
 */
void FImageUtils::ImageResize(int32 SrcWidth, int32 SrcHeight, const TArray64<FLinearColor>& SrcData, int32 DstWidth, int32 DstHeight, TArray64<FLinearColor>& DstData)
{
	DstData.Empty(DstWidth * DstHeight);
	DstData.AddZeroed(DstWidth * DstHeight);

	ImageResize(SrcWidth, SrcHeight, TArrayView64<const FLinearColor>(SrcData), DstWidth, DstHeight, TArrayView64<FLinearColor>(DstData));
}

/**
 * Resizes the given image using a simple average filter and stores it in the destination array.  This version constrains aspect ratio.
 * Accepts TArrayViews but requires that DstData be pre-sized appropriately
 *
 * @param SrcWidth	Source image width.
 * @param SrcHeight	Source image height.
 * @param SrcData	Source image data.
 * @param DstWidth	Destination image width.
 * @param DstHeight Destination image height.
 * @param DstData	Destination image data. (must already be sized to DstWidth*DstHeight)
 */
void FImageUtils::ImageResize(int32 SrcWidth, int32 SrcHeight, const TArrayView64<const FLinearColor>& SrcData, int32 DstWidth, int32 DstHeight, const TArrayView64<FLinearColor>& DstData)
{
	//@todo Oodle : this could be better ; investigate how widely it's used

	check(SrcData.Num() >= SrcWidth * SrcHeight);
	check(DstData.Num() >= DstWidth * DstHeight);

	float SrcX = 0;
	float SrcY = 0;
	const float StepSizeX = SrcWidth / (float)DstWidth;
	const float StepSizeY = SrcHeight / (float)DstHeight;

	for (int32 Y = 0; Y < DstHeight; Y++)
	{
		int32 PixelPos = Y * DstWidth;
		SrcX = 0.0f;

		for (int32 X = 0; X < DstWidth; X++)
		{
			int32 PixelCount = 0;
			float EndX = SrcX + StepSizeX;
			float EndY = SrcY + StepSizeY;

			// Generate a rectangular region of pixels and then find the average color of the region.
			int32 PosY = FMath::TruncToInt(SrcY + 0.5f);
			PosY = FMath::Clamp<int32>(PosY, 0, (SrcHeight - 1));

			int32 PosX = FMath::TruncToInt(SrcX + 0.5f);
			PosX = FMath::Clamp<int32>(PosX, 0, (SrcWidth - 1));

			int32 EndPosY = FMath::TruncToInt(EndY + 0.5f);
			EndPosY = FMath::Clamp<int32>(EndPosY, 0, (SrcHeight - 1));

			int32 EndPosX = FMath::TruncToInt(EndX + 0.5f);
			EndPosX = FMath::Clamp<int32>(EndPosX, 0, (SrcWidth - 1));

			FLinearColor FinalColor(0.0f, 0.0f, 0.0f, 0.0f);
			for (int32 PixelX = PosX; PixelX <= EndPosX; PixelX++)
			{
				for (int32 PixelY = PosY; PixelY <= EndPosY; PixelY++)
				{
					int32 StartPixel = PixelX + PixelY * SrcWidth;
					FinalColor += SrcData[StartPixel];
					PixelCount++;
				}
			}
			FinalColor /= (float)PixelCount;

			// Store the final averaged pixel color value.
			DstData[PixelPos] = FinalColor;
			SrcX = EndX;
			PixelPos++;
		}
		SrcY += StepSizeY;
	}
}

/**
 * Creates a 2D texture from a array of raw color data.
 *
 * @param SrcWidth		Source image width.
 * @param SrcHeight		Source image height.
 * @param SrcData		Source image data.
 * @param Outer			Outer for the texture object.
 * @param Name			Name for the texture object.
 * @param Flags			Object flags for the texture object.
 * @param InParams		Params about how to set up the texture.
 * @return				Returns a pointer to the constructed 2D texture object.
 *
 */
UTexture2D* FImageUtils::CreateTexture2D(int32 SrcWidth, int32 SrcHeight, const TArray<FColor> &SrcData, UObject* Outer, const FString& Name, const EObjectFlags &Flags, const FCreateTexture2DParameters& InParams)
{
#if WITH_EDITOR
	UTexture2D* Tex2D;

	Tex2D = NewObject<UTexture2D>(Outer, FName(*Name), Flags);
	Tex2D->Source.Init(SrcWidth, SrcHeight, /*NumSlices=*/ 1, /*NumMips=*/ 1, TSF_BGRA8);
	
	// if bUseAlpha is off, alpha is changed to 255
	uint8 AlphaOr = InParams.bUseAlpha ? 0 : 0xFF;

	// Create base mip for the texture we created.
	uint8* MipData = Tex2D->Source.LockMip(0);
	for( int32 y=0; y<SrcHeight; y++ )
	{
		uint8* DestPtr = &MipData[(SrcHeight - 1 - y) * SrcWidth * sizeof(FColor)];
		const FColor* SrcPtr = &SrcData[(SrcHeight - 1 - y) * SrcWidth];
		for( int32 x=0; x<SrcWidth; x++ )
		{
			*DestPtr++ = SrcPtr->B;
			*DestPtr++ = SrcPtr->G;
			*DestPtr++ = SrcPtr->R;
			*DestPtr++ = SrcPtr->A | AlphaOr;
			SrcPtr++;
		}
	}
	Tex2D->Source.UnlockMip(0);

	// Set the Source Guid/Hash if specified
	if (InParams.SourceGuidHash.IsValid())
	{
		Tex2D->Source.SetId(InParams.SourceGuidHash, true);
	}

	// Set compression options.
	Tex2D->SRGB = InParams.bSRGB;
	Tex2D->CompressionSettings = InParams.CompressionSettings;
	Tex2D->MipGenSettings = InParams.MipGenSettings;
	if( !InParams.bUseAlpha )
	{
		Tex2D->CompressionNoAlpha = true;
	}
	Tex2D->DeferCompression	= InParams.bDeferCompression;
	if (InParams.TextureGroup != TEXTUREGROUP_MAX)
	{
		Tex2D->LODGroup = InParams.TextureGroup;
	}

	Tex2D->VirtualTextureStreaming = InParams.bVirtualTexture;

	Tex2D->PostEditChange();
	return Tex2D;
#else
	UE_LOG(LogImageUtils, Fatal,TEXT("ConstructTexture2D not supported on console."));
	return NULL;
#endif
}

void FImageUtils::CropAndScaleImage( int32 SrcWidth, int32 SrcHeight, int32 DesiredWidth, int32 DesiredHeight, const TArray<FColor> &SrcData, TArray<FColor> &DstData  )
{
	// Get the aspect ratio, and calculate the dimension of the image to crop
	float DesiredAspectRatio = (float)DesiredWidth/(float)DesiredHeight;

	float MaxHeight = (float)SrcWidth / DesiredAspectRatio;
	float MaxWidth = (float)SrcWidth;

	if ( MaxHeight > (float)SrcHeight)
	{
		MaxHeight = (float)SrcHeight;
		MaxWidth = MaxHeight * DesiredAspectRatio;
	}

	// Store crop width and height as ints for convenience
	int32 CropWidth = FMath::FloorToInt(MaxWidth);
	int32 CropHeight = FMath::FloorToInt(MaxHeight);

	// Array holding the cropped image
	TArray<FColor> CroppedData;
	CroppedData.AddUninitialized(CropWidth*CropHeight);

	int32 CroppedSrcTop  = 0;
	int32 CroppedSrcLeft = 0;

	// Set top pixel if we are cropping height
	if ( CropHeight < SrcHeight )
	{
		CroppedSrcTop = ( SrcHeight - CropHeight ) / 2;
	}

	// Set width pixel if cropping width
	if ( CropWidth < SrcWidth )
	{
		CroppedSrcLeft = ( SrcWidth - CropWidth ) / 2;
	}

	const int32 DataSize = sizeof(FColor);

	//Crop the image
	for (int32 Row = 0; Row < CropHeight; Row++)
	{
	 	//Row*Side of a row*byte per color
	 	int32 SrcPixelIndex = (CroppedSrcTop+Row)*SrcWidth + CroppedSrcLeft;
	 	const void* SrcPtr = &(SrcData[SrcPixelIndex]);
	 	void* DstPtr = &(CroppedData[Row*CropWidth]);
	 	FMemory::Memcpy(DstPtr, SrcPtr, CropWidth*DataSize);
	}

	// Scale the image
	DstData.AddUninitialized(DesiredWidth*DesiredHeight);

	// Resize the image
	FImageUtils::ImageResize( MaxWidth, MaxHeight, CroppedData, DesiredWidth, DesiredHeight, DstData, true );
}

void FImageUtils::ThumbnailCompressImageArray( int32 ImageWidth, int32 ImageHeight, const TArray<FColor> &SrcData, TArray<uint8> &DstData )
{
	FObjectThumbnail TempThumbnail;
	TempThumbnail.SetImageSize( ImageWidth, ImageHeight );
	TArray<uint8>& ThumbnailByteArray = TempThumbnail.AccessImageData();

	// Copy image into destination thumb
	int32 MemorySize = ImageWidth*ImageHeight*sizeof(FColor);
	ThumbnailByteArray.AddUninitialized(MemorySize);
	FMemory::Memcpy(ThumbnailByteArray.GetData(), SrcData.GetData(), MemorySize);
	
	FColor * MutableSrcData = (FColor *)ThumbnailByteArray.GetData();

	// Thumbnails are saved as RGBA but FColors are stored as BGRA. An option to swap the order upon compression may be added at 
	// some point. At the moment, manually swapping Red and Blue 
	for (int32 Index = 0; Index < ImageWidth*ImageHeight; Index++ )
	{
		uint8 TempRed = MutableSrcData[Index].R;
		MutableSrcData[Index].R = MutableSrcData[Index].B;
		MutableSrcData[Index].B = TempRed;
	}

	// Compress data - convert into thumbnail current format
	TempThumbnail.CompressImageData();
	DstData = TempThumbnail.AccessCompressedImageData();
}

void FImageUtils::PNGCompressImageArray(int32 ImageWidth, int32 ImageHeight, const TArrayView64<const FColor>& SrcData, TArray64<uint8>& DstData)
{
	DstData.Reset();

	if (SrcData.Num() > 0 && ImageWidth > 0 && ImageHeight > 0)
	{
		FImageView Image((void *)SrcData.GetData(),ImageWidth,ImageHeight, ERawImageFormat::BGRA8);

		CompressImage(DstData,TEXT("png"),Image);
	}
}

UTexture2D* FImageUtils::CreateCheckerboardTexture(FColor ColorOne, FColor ColorTwo, int32 CheckerSize)
{
	CheckerSize = FMath::Min<uint32>( FMath::RoundUpToPowerOfTwo(CheckerSize), 4096 );
	const int32 HalfPixelNum = CheckerSize >> 1;

	// Create the texture
	UTexture2D* CheckerboardTexture = UTexture2D::CreateTransient(CheckerSize, CheckerSize, PF_B8G8R8A8);

	// Lock the checkerboard texture so it can be modified
	FColor* MipData = reinterpret_cast<FColor*>( CheckerboardTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE) );

	// Fill in the colors in a checkerboard pattern
	for ( int32 RowNum = 0; RowNum < CheckerSize; ++RowNum )
	{
		for ( int32 ColNum = 0; ColNum < CheckerSize; ++ColNum )
		{
			FColor& CurColor = MipData[( ColNum + ( RowNum * CheckerSize ) )];

			if ( ColNum < HalfPixelNum )
			{
				CurColor = ( RowNum < HalfPixelNum ) ? ColorOne : ColorTwo;
			}
			else
			{
				CurColor = ( RowNum < HalfPixelNum ) ? ColorTwo : ColorOne;
			}
		}
	}

	// Unlock the texture
	CheckerboardTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	CheckerboardTexture->UpdateResource();

	return CheckerboardTexture;
}

UTexture2DArray* FImageUtils::CreateCheckerboardTexture2DArray(FColor ColorOne, FColor ColorTwo, int32 CheckerSize, int32 ArraySize)
{
	CheckerSize = FMath::Min<uint32>(FMath::RoundUpToPowerOfTwo(CheckerSize), 4096);
	const int32 HalfCheckerSize = CheckerSize >> 1;

	// Create the texture
	UTexture2DArray* CheckerboardTexture = UTexture2DArray::CreateTransient(CheckerSize, CheckerSize, ArraySize, PF_B8G8R8A8);

	// Lock the checkerboard texture so it can be modified
	FColor* MipData = reinterpret_cast<FColor*>(CheckerboardTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

	// Fill in the colors in a checkerboard pattern
	for (int32 ArrayIndex = 0; ArrayIndex < ArraySize; ArrayIndex++)
	{
		for (int32 RowIndex = 0; RowIndex < CheckerSize; RowIndex++)
		{
			for (int32 ColumnIndex = 0; ColumnIndex < CheckerSize; ColumnIndex++)
			{
				*MipData++ = ColumnIndex < HalfCheckerSize == RowIndex < HalfCheckerSize ? ColorOne : ColorTwo;
			}
		}
	}

	// Unlock the texture
	CheckerboardTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	CheckerboardTexture->UpdateResource();

	return CheckerboardTexture;
}

UTextureCube* FImageUtils::CreateCheckerboardCubeTexture(FColor ColorOne, FColor ColorTwo, int32 CheckerSize)
{
	CheckerSize = FMath::Min<uint32>(FMath::RoundUpToPowerOfTwo(CheckerSize), 4096);
	const int32 HalfPixelNum = CheckerSize >> 1;

	// Create the texture
	UTextureCube* CheckerboardTexture = UTextureCube::CreateTransient(CheckerSize, CheckerSize, PF_B8G8R8A8);

	// Lock the checkerboard texture so it can be modified
	FColor* MipData = reinterpret_cast<FColor*>(CheckerboardTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

	// Fill in the colors in a checkerboard pattern
	for (int32 Face = 0; Face < 6; ++Face)
	{
		for (int32 RowNum = 0; RowNum < CheckerSize; ++RowNum)
		{
			for (int32 ColNum = 0; ColNum < CheckerSize; ++ColNum)
			{
				FColor& CurColor = MipData[(ColNum + (RowNum * CheckerSize))];

				if (ColNum < HalfPixelNum)
				{
					CurColor = (RowNum < HalfPixelNum) ? ColorOne : ColorTwo;
				}
				else
				{
					CurColor = (RowNum < HalfPixelNum) ? ColorTwo : ColorOne;
				}
			}
		}
		MipData += CheckerSize * CheckerSize;
	}

	// Unlock the texture
	CheckerboardTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	CheckerboardTexture->UpdateResource();

	return CheckerboardTexture;
}

UTextureCubeArray* FImageUtils::CreateCheckerboardTextureCubeArray(FColor ColorOne, FColor ColorTwo, int32 CheckerSize, int32 ArraySize)
{
	CheckerSize = FMath::Min<uint32>(FMath::RoundUpToPowerOfTwo(CheckerSize), 4096);
	const int32 HalfCheckerSize = CheckerSize >> 1;

	// Create the texture
	UTextureCubeArray* CheckerboardTexture = UTextureCubeArray::CreateTransient(CheckerSize, CheckerSize, ArraySize, PF_B8G8R8A8);

	// Lock the checkerboard texture so it can be modified
	FColor* MipData = reinterpret_cast<FColor*>(CheckerboardTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

	// Fill in the colors in a checkerboard pattern
	for (int32 ArrayIndex = 0; ArrayIndex < ArraySize; ArrayIndex++)
	{
		for (int32 FaceIndex = 0; FaceIndex < 6; FaceIndex++)
		{
			for (int32 RowIndex = 0; RowIndex < CheckerSize; RowIndex++)
			{
				for (int32 ColumnIndex = 0; ColumnIndex < CheckerSize; ColumnIndex++)
				{
					*MipData++ = ColumnIndex < HalfCheckerSize == RowIndex < HalfCheckerSize ? ColorOne : ColorTwo;
				}
			}
		}
	}

	// Unlock the texture
	CheckerboardTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	CheckerboardTexture->UpdateResource();

	return CheckerboardTexture;
}

UVolumeTexture* FImageUtils::CreateCheckerboardVolumeTexture(FColor ColorOne, FColor ColorTwo, int32 CheckerSize)
{
	CheckerSize = FMath::Min<uint32>(FMath::RoundUpToPowerOfTwo(CheckerSize), 4096);
	const int32 HalfCheckerSize = CheckerSize >> 1;

	// Create the texture
	UVolumeTexture* CheckerboardTexture = UVolumeTexture::CreateTransient(CheckerSize, CheckerSize, CheckerSize, PF_B8G8R8A8);

	// Lock the checkerboard texture so it can be modified
	FColor* MipData = reinterpret_cast<FColor*>(CheckerboardTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

	// Fill in the colors in a checkerboard pattern
	for (int32 SliceIndex = 0; SliceIndex < CheckerSize; SliceIndex++)
	{
		for (int32 RowIndex = 0; RowIndex < CheckerSize; RowIndex++)
		{
			for (int32 ColumnIndex = 0; ColumnIndex < CheckerSize; ColumnIndex++)
			{
				*MipData++ = ColumnIndex < HalfCheckerSize == RowIndex < HalfCheckerSize ? ColorOne : ColorTwo;
			}
		}
	}

	// Unlock the texture
	CheckerboardTexture->GetPlatformData()->Mips[0].BulkData.Unlock();
	CheckerboardTexture->UpdateResource();

	return CheckerboardTexture;
}

/*------------------------------------------------------------------------------
HDR file format helper.
------------------------------------------------------------------------------*/
// DEPRECATED
// do not use HDR for exports, it is very lossy, use EXR instead for float output
// if you need HDR, do not use this export code
// instead use HdrImageWrapper via FImageUtils::CompressImage
class FHDRExportHelper
{
public:
	/**
	* Writes HDR format image to an FArchive
	* @param TexRT - A 2D source render target to read from.
	* @param Ar - Archive object to write HDR data to.
	* @return true on successful export.
	*/
	bool ExportHDR(UTextureRenderTarget2D* TexRT, FArchive& Ar)
	{
		check(TexRT != nullptr);
		FRenderTarget* RenderTarget = TexRT->GameThread_GetRenderTargetResource();
		Size = RenderTarget->GetSizeXY();
		Format = TexRT->GetFormat();

		TArray64<uint8> RawData;
		bool bReadSuccess = FImageUtils::GetRawData(TexRT, RawData);
		if (bReadSuccess)
		{
			WriteHDRImage(RawData, Ar);
			return true;
		}
		return false;
	}

	/**
	* Writes HDR format image to an FArchive
	* @param TexRT - A 2D source render target to read from.
	* @param Ar - Archive object to write HDR data to.
	* @return true on successful export.
	*/
	bool ExportHDR(UTexture2D* Texture, FArchive& Ar)
	{
		check(Texture != nullptr);
		bool bReadSuccess = true;
		TArray64<uint8> RawData;

#if WITH_EDITORONLY_DATA
		Size = FIntPoint(Texture->Source.GetSizeX(), Texture->Source.GetSizeY());

		bReadSuccess = Texture->Source.GetMipData(RawData, 0);
		const ETextureSourceFormat NewFormat = Texture->Source.GetFormat();

		// DEPRECATED
		// not a general purpose HDR exporter
		// can't write F32 or BGRE8
		// do not use this except for longlat cubemaps
		if (NewFormat == TSF_BGRA8)
		{
			Format = PF_B8G8R8A8;
		}
		else if (NewFormat == TSF_RGBA16F)
		{
			Format = PF_FloatRGBA;
		}
		else
		{
			bReadSuccess = false;			
			FMessageLog("ImageUtils").Warning(LOCTEXT("ExportHDRUnsupportedSourceTextureFormat", "Unsupported source texture format provided."));
		}
#else
		TArray<uint8*> RawData2;
		Size = Texture->GetImportedSize();
		RawData2.AddZeroed(Texture->GetNumMips());
		Texture->GetMipData(0, (void**)RawData2.GetData());
		const EPixelFormat NewFormat = Texture->GetPixelFormat();

		if (Texture->GetPlatformData()->Mips.Num() == 0)
		{
			bReadSuccess = false;
			FMessageLog("ImageUtils").Warning(FText::Format(LOCTEXT("ExportHDRFailedToReadMipData", "Failed to read Mip Data in: '{0}'"), FText::FromString(Texture->GetName())));
		}

		if (NewFormat == PF_B8G8R8A8)
		{
			Format = PF_B8G8R8A8;
		}
		else if (NewFormat == PF_FloatRGBA)
		{
			Format = PF_FloatRGBA;
		}
		else
		{
			bReadSuccess = false;
			FMessageLog("ImageUtils").Warning(LOCTEXT("ExportHDRUnsupportedTextureFormat", "Unsupported texture format provided."));
		}

		//Put first mip data into usable array
		if (bReadSuccess)
		{
			const uint32 TotalSize = Texture->GetPlatformData()->Mips[0].BulkData.GetBulkDataSize();
			RawData.AddZeroed(TotalSize);
			FMemory::Memcpy(RawData.GetData(), RawData2[0], TotalSize);
		}

		//Deallocate the mip data
		for (auto MipData : RawData2)
		{
			FMemory::Free(MipData);
		}

#endif // WITH_EDITORONLY_DATA

		if (bReadSuccess)
		{
			WriteHDRImage(RawData, Ar);
			return true;
		}

		return false;
	}

	/**
	* Writes HDR format image to an FArchive
	* This function unwraps the cube image on to a 2D surface.
	* @param TexCube - A cube source cube texture to read from.
	* @param Ar - Archive object to write HDR data to.
	* @return true on successful export.
	*/
	// this does GenerateLongLatUnwrap
	bool ExportHDR(UTextureCube* TexCube, FArchive& Ar)
	{
		check(TexCube != nullptr);

		// Generate 2D image.
		TArray64<uint8> RawData;
		bool bUnwrapSuccess = CubemapHelpers::GenerateLongLatUnwrap(TexCube, RawData, Size, Format);
		bool bAcceptableFormat = (Format == PF_B8G8R8A8 || Format == PF_FloatRGBA);
		if (bUnwrapSuccess == false || bAcceptableFormat == false)
		{
			return false;
		}

		WriteHDRImage(RawData, Ar);

		return true;
	}

	/**
	* Writes HDR format image to an FArchive
	* This function unwraps the cube image on to a 2D surface.
	* @param TexCube - A cube source render target cube texture to read from.
	* @param Ar - Archive object to write HDR data to.
	* @return true on successful export.
	*/
	// this does GenerateLongLatUnwrap
	bool ExportHDR(UTextureRenderTargetCube* TexCube, FArchive& Ar)
	{
		check(TexCube != nullptr);

		// Generate 2D image.
		TArray64<uint8> RawData;
		bool bUnwrapSuccess = CubemapHelpers::GenerateLongLatUnwrap(TexCube, RawData, Size, Format);
		bool bAcceptableFormat = (Format == PF_B8G8R8A8 || Format == PF_FloatRGBA);
		if (bUnwrapSuccess == false || bAcceptableFormat == false)
		{
			return false;
		}

		WriteHDRImage(RawData, Ar);

		return true;
	}

private:
	void WriteScanLine(FArchive& Ar, const TArray<uint8>& ScanLine)
	{
		const uint8* LineEnd = ScanLine.GetData() + ScanLine.Num();
		const uint8* LineSource = ScanLine.GetData();
		TArray<uint8> Output;
		Output.Reserve(ScanLine.Num() * 2);
		while (LineSource < LineEnd)
		{
			int32 CurrentPos = 0;
			int32 NextPos = 0;
			int32 CurrentRunLength = 0;
			while (CurrentRunLength <= 4 && NextPos < 128 && LineSource + NextPos < LineEnd)
			{
				CurrentPos = NextPos;
				CurrentRunLength = 0;
				while (CurrentRunLength < 127 && CurrentPos + CurrentRunLength < 128 && LineSource + NextPos < LineEnd && LineSource[CurrentPos] == LineSource[NextPos])
				{
					NextPos++;
					CurrentRunLength++;
				}
			}

			if (CurrentRunLength > 4)
			{
				// write a non run: LineSource[0] to LineSource[CurrentPos]
				if (CurrentPos > 0)
				{
					Output.Add(CurrentPos);
					for (int32 i = 0; i < CurrentPos; i++)
					{
						Output.Add(LineSource[i]);
					}
				}
				Output.Add((uint8)(128 + CurrentRunLength));
				Output.Add(LineSource[CurrentPos]);
			}
			else
			{
				// write a non run: LineSource[0] to LineSource[NextPos]
				Output.Add((uint8)(NextPos));
				for (int32 i = 0; i < NextPos; i++)
				{
					Output.Add((uint8)(LineSource[i]));
				}
			}
			LineSource += NextPos;
		}
		Ar.Serialize(Output.GetData(), Output.Num());
	}

	template<typename TSourceColorType>
	void WriteHDRBits(FArchive& Ar, TSourceColorType* SourceTexels)
	{
		const int32 NumChannels = 4;
		const int32 SizeX = Size.X;
		const int32 SizeY = Size.Y;
		TArray<uint8> ScanLine[NumChannels];
		for (int32 Channel = 0; Channel < NumChannels; Channel++)
		{
			ScanLine[Channel].Reserve(SizeX);
		}

		for (int32 y = 0; y < SizeY; y++)
		{
			// write RLE header
			uint8 RLEheader[4];
			RLEheader[0] = 2;
			RLEheader[1] = 2;
			RLEheader[2] = SizeX >> 8;
			RLEheader[3] = SizeX & 0xFF;
			Ar.Serialize(&RLEheader[0], sizeof(RLEheader));

			for (int32 Channel = 0; Channel < NumChannels; Channel++)
			{
				ScanLine[Channel].Reset();
			}

			for (int32 x = 0; x < SizeX; x++)
			{
				FLinearColor LinearColor(*SourceTexels);
				FColor RGBEColor = LinearColor.ToRGBE();

				ScanLine[0].Add(RGBEColor.R);
				ScanLine[1].Add(RGBEColor.G);
				ScanLine[2].Add(RGBEColor.B);
				ScanLine[3].Add(RGBEColor.A);
				SourceTexels++;
			}

			for (int32 Channel = 0; Channel < NumChannels; Channel++)
			{
				WriteScanLine(Ar, ScanLine[Channel]);
			}
		}
	}

	void WriteHDRHeader(FArchive& Ar)
	{
		const int32 MaxHeaderSize = 256;
		char Header[MAX_SPRINTF];
		FCStringAnsi::Sprintf(Header, "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y %d +X %d\n", Size.Y, Size.X);
		Header[MaxHeaderSize - 1] = 0;
		int32 Len = FMath::Min(FCStringAnsi::Strlen(Header), MaxHeaderSize);
		Ar.Serialize(Header, Len);
	}

	void WriteHDRImage(const TArray64<uint8>& RawData, FArchive& Ar)
	{
		WriteHDRHeader(Ar);
		if (Format == PF_FloatRGBA)
		{
			WriteHDRBits(Ar, (FFloat16Color*)RawData.GetData());
		}
		else
		{
			check( Format == PF_B8G8R8A8 );
			WriteHDRBits(Ar, (FColor*)RawData.GetData());
		}
		// can't actually write BGRE8 pixels
		// use HdrImageWrapper instead
	}

	FIntPoint Size;
	EPixelFormat Format;
};

bool FImageUtils::ExportRenderTarget2DAsHDR(UTextureRenderTarget2D* TexRT, FArchive& Ar)
{
	UE_LOG(LogImageUtils, Warning, TEXT("HDR file format is very lossy, use EXR or PNG instead.") );

	FImage Image;
	if ( ! GetRenderTargetImage(TexRT,Image) )
	{
		return false;
	}

	TArray64<uint8> CompressedData;
	if ( ! CompressImage(CompressedData,TEXT("HDR"),Image) )
	{
		return false;
	}

	Ar.Serialize((void*)CompressedData.GetData(), CompressedData.GetAllocatedSize());

	return true;
}

bool FImageUtils::ExportRenderTarget2DAsPNG(UTextureRenderTarget2D* TexRT, FArchive& Ar)
{
	FImage Image;
	if ( ! GetRenderTargetImage(TexRT,Image) )
	{
		return false;
	}

	TArray64<uint8> CompressedData;
	if ( ! CompressImage(CompressedData,TEXT("PNG"),Image) )
	{
		return false;
	}

	Ar.Serialize((void*)CompressedData.GetData(), CompressedData.GetAllocatedSize());

	return true;
}

bool FImageUtils::ExportRenderTarget2DAsEXR(UTextureRenderTarget2D* TexRT, FArchive& Ar)
{
	FImage Image;
	if ( ! GetRenderTargetImage(TexRT,Image) )
	{
		return false;
	}

	TArray64<uint8> CompressedData;
	if ( ! CompressImage(CompressedData,TEXT("EXR"),Image) )
	{
		return false;
	}

	Ar.Serialize((void*)CompressedData.GetData(), CompressedData.GetAllocatedSize());

	return true;
}

// if Texture source is available, get it as an FImage
bool FImageUtils::GetTexture2DSourceImage(UTexture2D* Texture, FImage & OutImage)
{
#if WITH_EDITORONLY_DATA	
	return Texture->Source.GetMipImage(OutImage,0);
#else

	UE_LOG(LogImageUtils,Warning,TEXT("GetTexture2DSourceImage from PlatformData not implemented yet"));

	// @todo Oodle : could export texture from platformdata
	// only a few formats would be possible to grab to an FImage
	//   (for DDS export we could do all formats)
	// eg. see ExportHDR below

	/*
	EPixelFormat PixelFormat = Texture->GetFormat();
	Texture->GetMipData(0, (void**)RawData2.GetData());
	if (Texture->GetPlatformData()->Mips.Num() == 0)
	*/
	
	//uint8* MipData = static_cast<uint8*>(NewTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
	//int64 MipDataSize = NewTexture->GetPlatformData()->Mips[0].BulkData.GetBulkDataSize();
	
	//if (NewFormat == PF_B8G8R8A8)
	//else if (NewFormat == PF_FloatRGBA)

	return false;
#endif
}

bool FImageUtils::ExportTexture2DAsHDR(UTexture2D* TexRT, FArchive& Ar)
{
	UE_LOG(LogImageUtils, Warning, TEXT("HDR file format is very lossy, use EXR or PNG instead.") );

	// could use GetTexture2DSourceImage
	// then CompressImage
	// for arbitrary format exports
	// but just leave it alone for now

	// this is only used by UKismetRenderingLibrary::ExportTexture2D
	//	so we should take filename as input to auto-detect format

	FHDRExportHelper Exporter;
	return Exporter.ExportHDR(TexRT, Ar);
}

UTexture2D* FImageUtils::ImportFileAsTexture2D(const FString& Filename)
{
	UTexture2D* NewTexture = nullptr;
	TArray64<uint8> Buffer;
	if (FFileHelper::LoadFileToArray(Buffer, *Filename))
	{
		NewTexture = FImageUtils::ImportBufferAsTexture2D(Buffer);

		if(!NewTexture)
		{
			UE_LOG(LogImageUtils, Warning, TEXT("Error creating texture. %s is not a supported file format"), *Filename)
		}	
	}
	else
	{
		UE_LOG(LogImageUtils, Warning, TEXT("Error creating texture. %s could not be found"), *Filename)
	}

	return NewTexture;
}

UTexture2D* FImageUtils::ImportBufferAsTexture2D(TArrayView64<const uint8> Buffer)
{
	FImage Image;
	if ( ! DecompressImage(Buffer.GetData(),Buffer.Num(), Image) )
	{
		UE_LOG(LogImageUtils, Warning, TEXT("Error creating texture. Couldn't determine the file format"));
		return nullptr;
	}

	return CreateTexture2DFromImage(Image);
}

UTexture2D* FImageUtils::CreateTexture2DFromImage(const FImageView & Image)
{
	ERawImageFormat::Type PixelFormatRawFormat;
	EPixelFormat PixelFormat = FImageCoreUtils::GetPixelFormatForRawImageFormat(Image.Format,&PixelFormatRawFormat);
						
	UTexture2D* NewTexture = UTexture2D::CreateTransient(Image.SizeX, Image.SizeY, PixelFormat);
	if (NewTexture == nullptr)
	{
		UE_LOG(LogImageUtils, Warning, TEXT("Error in CreateTransient"));
		return nullptr;
	}

	NewTexture->bNotOfflineProcessed = true;

	uint8* MipData = static_cast<uint8*>(NewTexture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
	check( MipData != nullptr );
	int64 MipDataSize = NewTexture->GetPlatformData()->Mips[0].BulkData.GetBulkDataSize();

	FImageView MipImage(MipData,Image.SizeX,Image.SizeY,1,PixelFormatRawFormat,Image.GammaSpace);

	// copy into texture and convert if necessary :
	FImageCore::CopyImage(Image,MipImage);
				
	NewTexture->GetPlatformData()->Mips[0].BulkData.Unlock();

	NewTexture->UpdateResource();

	return NewTexture;
}

UTexture2D* FImageUtils::ImportBufferAsTexture2D(const TArray<uint8>& Buffer)
{
	return ImportBufferAsTexture2D(TArrayView64<const uint8>(Buffer.GetData(), Buffer.Num()));
}

bool FImageUtils::ExportRenderTargetCubeAsHDR(UTextureRenderTargetCube* TexRT, FArchive& Ar)
{
	// this does GenerateLongLatUnwrap
	FHDRExportHelper Exporter;
	return Exporter.ExportHDR(TexRT, Ar);
}

bool FImageUtils::ExportTextureCubeAsHDR(UTextureCube* TexRT, FArchive& Ar)
{
	// this does GenerateLongLatUnwrap
	FHDRExportHelper Exporter;
	return Exporter.ExportHDR(TexRT, Ar);
}

#undef LOCTEXT_NAMESPACE
