// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureSourceDataUtils.h"

#if WITH_EDITOR

#include "ImageCoreUtils.h"
#include "Engine/Texture.h"
#include "HAL/UnrealMemory.h"
#include "EngineLogs.h"

namespace UE::TextureUtilitiesCommon::Experimental
{

namespace Private
{
	bool ResizeTexture2D(UTexture* Texture, int32 MaxSize, const ITargetPlatform* TargetPlatform)
	{
		// Protect the code from an async build of the texture
		Texture->PreEditChange(nullptr);

		// We want to reduce the asset size so ignore the imported mip(s)
		const int32 MipIndex = 0;
		FImage SourceMip0;
		if (!Texture->Source.GetMipImage(SourceMip0, MipIndex))
		{
			return false;
		}

		const int32 LayerIndex = 0;
		if ( ! Texture->DownsizeImageUsingTextureSettings(TargetPlatform, SourceMip0, MaxSize, LayerIndex) )
		{
			return false;
		}
		FImage ResizedImage = MoveTemp(SourceMip0);

		UE::Serialization::FEditorBulkData::FSharedBufferWithID ResizedImageBufferWithID = MakeSharedBufferFromArray(MoveTemp(ResizedImage.RawData));

		const int32 NumMips = 1;
		Texture->Source.Init(ResizedImage.SizeX
			, ResizedImage.SizeY
			, ResizedImage.NumSlices
			, NumMips
			, FImageCoreUtils::ConvertToTextureSourceFormat(ResizedImage.Format)
			, MoveTemp(ResizedImageBufferWithID));

		// if gamma was Pow22 it is now sRGB
		Texture->bUseLegacyGamma = false;

		// PostEditChange is called outside by our caller

		return true;
	}

	bool ResizeTextureSlicedBy2DLayers(UTexture* Texture, int32 MaxSize, const ITargetPlatform* TargetPlatform)
	{
		// Protect the code from an async build of the texture
		Texture->PreEditChange(nullptr);

		FImage ResizedImage;
		{
			TArray<FImage> ResizedSlices;
			ResizedSlices.Reserve(Texture->Source.GetNumSlices());

			ERawImageFormat::Type FormatUsed;
			EGammaSpace GammaSpaceUsed;

			{
				// We want to reduce the asset size so ignore the imported mip(s)
				TArray<FImage> Slices;
				Slices.Reserve(Texture->Source.GetNumSlices());

				{
					// We could probably avoid a copy here but for the simplicity of the code keep it for now.
					FImage SourceMip0;
					if (!Texture->Source.GetMipImage(SourceMip0, 0))
					{
						return false;
					}

					FormatUsed = SourceMip0.Format;
					GammaSpaceUsed = SourceMip0.GammaSpace;

					for (int32 Index = 0; Index < SourceMip0.NumSlices; ++Index)
					{
						FImage& Slice = Slices.AddDefaulted_GetRef();
						FImageView SliceView = SourceMip0.GetSlice(Index);

						FImageCore::CopyImage(SliceView, Slice);
					}
				}

				for (FImage& Slice : Slices)
				{
					FImage& ResizedSlice = ResizedSlices.AddDefaulted_GetRef();

					const int32 LayerIndex = 0;
					Texture->DownsizeImageUsingTextureSettings(TargetPlatform, Slice, MaxSize, LayerIndex);
					ResizedSlice = MoveTemp(Slice);
				}
			}

			// Move the resized slices into the resized image
			ResizedImage.Init(ResizedSlices[0].SizeX,ResizedSlices[0].SizeY, ResizedSlices.Num(), FormatUsed, GammaSpaceUsed);

			for (int32 Index = 0; Index < ResizedImage.NumSlices; ++Index)
			{
				FImageCore::CopyImage(ResizedSlices[Index], ResizedImage.GetSlice(Index));
			}
		}


		UE::Serialization::FEditorBulkData::FSharedBufferWithID ResizedImageBufferWithID = MakeSharedBufferFromArray(MoveTemp(ResizedImage.RawData));

		const int32 NumMips = 1;
		Texture->Source.Init(ResizedImage.SizeX
			, ResizedImage.SizeY
			, ResizedImage.NumSlices
			, NumMips
			, FImageCoreUtils::ConvertToTextureSourceFormat(ResizedImage.Format)
			, MoveTemp(ResizedImageBufferWithID));

		return true;
	}

	bool ResizeTexture2DBlocked(UTexture* Texture, int32 MaxSize, const ITargetPlatform* TargetPlatform)
	{
		// Protect the code from an async build of the texture
		Texture->PreEditChange(nullptr);

		FIntPoint LogicalSourceSize = Texture->Source.GetLogicalSize();
		double RatioX = double(MaxSize) / LogicalSourceSize.X;
		double RatioY = double(MaxSize) / LogicalSourceSize.Y;

		TArray<FTextureSourceBlock > ResizedSourceBlocks;
		ResizedSourceBlocks.Reserve(Texture->Source.GetNumBlocks());
	
		TArray<FImage> ResizedBlocks;
		ResizedBlocks.Reserve(Texture->Source.GetNumBlocks());

		for (int32 BlockIndex = 0; BlockIndex < Texture->Source.GetNumBlocks(); ++BlockIndex)
		{
			// We want to reduce the asset size so ignore the imported mip(s)
			FImage SourceMip0;
			const int32 MipIndex = 0;
			const int32 LayerIndex = 0;
			if (!Texture->Source.GetMipImage(SourceMip0, BlockIndex, LayerIndex, MipIndex))
			{
				return false;
			}

			FTextureSourceBlock& ResizedSourceBlock = ResizedSourceBlocks.AddDefaulted_GetRef();
			Texture->Source.GetBlock(BlockIndex, ResizedSourceBlock);
		
			FImage& ResizedBlock = ResizedBlocks.AddDefaulted_GetRef();

			int32 BlockMaxSize = FMath::RoundToInt32(FMath::Min(ResizedSourceBlock.SizeX * RatioX, ResizedSourceBlock.SizeY * RatioY));

			Texture->DownsizeImageUsingTextureSettings(TargetPlatform, SourceMip0, MaxSize, LayerIndex);
			ResizedBlock = MoveTemp(SourceMip0);
			
			ResizedSourceBlock.SizeX = ResizedBlock.SizeX;
			ResizedSourceBlock.SizeY = ResizedBlock.SizeY;
			ResizedSourceBlock.NumSlices = 1;
		}

		int64 SizeNeededInBytes = 0;
		for (const FImage& Block : ResizedBlocks)
		{
			SizeNeededInBytes += Block.RawData.Num();
		}
		FUniqueBuffer WriteImageBuffer = FUniqueBuffer::Alloc(SizeNeededInBytes);

		uint8* CurrentAddress = static_cast<uint8*>(WriteImageBuffer.GetData());
		for (FImage& Block : ResizedBlocks)
		{
			FMemory::Memcpy(CurrentAddress, static_cast<uint8*>(Block.RawData.GetData()), Block.RawData.Num());
			CurrentAddress += Block.RawData.Num();
		}

		UE::Serialization::FEditorBulkData::FSharedBufferWithID ResizedImageBufferWithID = WriteImageBuffer.MoveToShared();

		const ETextureSourceFormat SourceFormat = Texture->Source.GetFormat();
		int32 NumLayers = 1;
		Texture->Source.InitBlocked(
			&SourceFormat,
			ResizedSourceBlocks.GetData(),
			NumLayers,
			ResizedSourceBlocks.Num(),
			MoveTemp(ResizedImageBufferWithID)
		);

		return true;
	}
}


bool DownsizeTextureSourceData(UTexture* Texture, int32 TargetSizeInGame, const ITargetPlatform* TargetPlatform)
{
	check( Texture->Source.IsValid() );

	// Check if we don't know how to resize that texture
	if (Texture->Source.GetNumLayers() != 1)
	{
		return false;
	}

	if (!(Texture->Source.GetTextureClass() == ETextureClass::Cube || Texture->Source.GetTextureClass() == ETextureClass::TwoD))
	{
		return false;
	}

	if (Texture->Source.GetTextureClass() == ETextureClass::TwoD && Texture->Source.GetNumSlices() != 1)
	{
		return false;
	}

	if (Texture->Source.GetTextureClass() == ETextureClass::Cube)
	{
		if (Texture->Source.IsLongLatCubemap())
		{
			if (Texture->Source.GetNumSlices() != 1)
			{
				return false;
			}
		}
		else if (Texture->Source.GetNumSlices() != 6)
		{
			return false;
		}
	}

	int32 TargetSourceSize = TargetSizeInGame;
	if (Texture->Source.IsLongLatCubemap())
	{
		// The function return the max size of the generated cube from the source long lat
		// this should be kept in sync with the implementation details of ComputeLongLatCubemapExtents() or refactored
		TargetSourceSize = (1U << FMath::FloorLog2(TargetSizeInGame)) * 2;
	}

	FIntPoint SourceSize = Texture->Source.GetLogicalSize();
	if (SourceSize.X <= TargetSourceSize && SourceSize.Y <= TargetSourceSize)
	{
		return false;
	}

	if (Texture->Source.GetTextureClass() == ETextureClass::TwoD)
	{
		if (Texture->Source.GetNumBlocks() == 1)
		{
			return Private::ResizeTexture2D(Texture, TargetSourceSize, TargetPlatform);
		}
		else
		{
			// UDIM(s)
			return Private::ResizeTexture2DBlocked(Texture, TargetSourceSize, TargetPlatform);
		}
	}
	else if (Texture->Source.GetTextureClass() == ETextureClass::Cube)
	{
		if (Texture->Source.IsLongLatCubemap())
		{
			return Private::ResizeTexture2D(Texture, TargetSourceSize, TargetPlatform);
		}
		else
		{
			return Private::ResizeTextureSlicedBy2DLayers(Texture, TargetSourceSize, TargetPlatform);
		}
	}
	
	return false;
}

bool DownsizeTexureSourceDataNearRenderingSize(UTexture* Texture, const ITargetPlatform* TargetPlatform)
{
	if ( ! Texture->Source.IsValid() )
	{
		return false;
	}

	int32 BeforeSizeX;
	int32 BeforeSizeY;
	Texture->GetBuiltTextureSize(TargetPlatform, BeforeSizeX, BeforeSizeY);

	int32 TargetSize = FMath::Max(BeforeSizeX, BeforeSizeY);
	if (DownsizeTextureSourceData(Texture, TargetSize, TargetPlatform))
	{
		Texture->LODBias = 0;
		Texture->PostEditChange();
		
		// check that GetBuiltTextureSize was preserved :
		int32 AfterSizeX;
		int32 AfterSizeY;
		Texture->GetBuiltTextureSize(TargetPlatform, AfterSizeX, AfterSizeY);

		if ( BeforeSizeX != AfterSizeX ||
			 BeforeSizeY != AfterSizeY )
		{
			UE_LOG(LogTexture,Warning,TEXT("DownsizeTexureSourceDataNearRenderingSize failed to preserve built size; was: %dx%d now: %dx%d on [%s]"),
				BeforeSizeX,BeforeSizeY,
				AfterSizeX,AfterSizeY,
				*Texture->GetFullName());
		}

		return true;
	}

	return false;
}

} // End namespace UE::TextureUtilitiesCommon::Experimental

#endif //WITH_EDITOR
