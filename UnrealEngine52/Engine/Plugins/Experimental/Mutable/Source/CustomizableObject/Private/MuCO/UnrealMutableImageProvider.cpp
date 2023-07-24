// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/UnrealMutableImageProvider.h"

#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObject.h"
#include "TextureResource.h"


//-------------------------------------------------------------------------------------------------
namespace
{

	mu::ImagePtr ConvertTextureUnrealToMutable(UTexture2D* Texture, uint8 MipmapsToSkip)
	{
		mu::ImagePtr pResult;

#if WITH_EDITOR
		int LODs = 1;
		int SizeX = Texture->Source.GetSizeX() >> MipmapsToSkip;
		int SizeY = Texture->Source.GetSizeY() >> MipmapsToSkip;
		check(SizeX > 0 && SizeY > 0);

		ETextureSourceFormat Format = Texture->Source.GetFormat();
		mu::EImageFormat MutableFormat = mu::EImageFormat::IF_NONE;

		switch (Format)
		{
		case ETextureSourceFormat::TSF_BGRA8: MutableFormat = mu::EImageFormat::IF_BGRA_UBYTE; break;
		// This format is deprecated and using the enum fails to compile in some cases.
		//case ETextureSourceFormat::TSF_RGBA8: MutableFormat = mu::EImageFormat::IF_RGBA_UBYTE; break;
		case ETextureSourceFormat::TSF_G8: MutableFormat = mu::EImageFormat::IF_L_UBYTE; break;
		default:
			break;
		}

		pResult = new mu::Image(SizeX, SizeY, LODs, MutableFormat);

		// If not locked ReadOnly the Texture Source's FGuid can change, invalidating the texture's caching/shaders
		// making shader compile and cook times increase
		const uint8* pSource = Texture->Source.LockMipReadOnly(MipmapsToSkip);
		if (pSource)
		{
			FMemory::Memcpy(pResult->GetData(), pSource, pResult->GetDataSize());
			Texture->Source.UnlockMip(MipmapsToSkip);
		}
		else
		{
			check(false);
		}


#else
		check(Texture->GetPlatformData()->Mips[MipmapsToSkip].BulkData.IsBulkDataLoaded());

		int32 LODs = 1;
		int32 SizeX = Texture->GetSizeX() >> MipmapsToSkip;
		int32 SizeY = Texture->GetSizeY() >> MipmapsToSkip;
		check(SizeX > 0 && SizeY > 0);

		EPixelFormat Format = Texture->GetPlatformData()->PixelFormat;
		mu::EImageFormat MutableFormat = mu::EImageFormat::IF_NONE;

		switch (Format)
		{
		case EPixelFormat::PF_B8G8R8A8: MutableFormat = mu::EImageFormat::IF_BGRA_UBYTE; break;
			// This format is deprecated and using the enum fails to compile in some cases.
			//case ETextureSourceFormat::TSF_RGBA8: MutableFormat = mu::EImageFormat::IF_RGBA_UBYTE; break;
		case EPixelFormat::PF_G8: MutableFormat = mu::EImageFormat::IF_L_UBYTE; break;
		default:
			break;
		}

		pResult = new mu::Image(SizeX, SizeY, LODs, MutableFormat);

		// If not locked ReadOnly the Texture Source's FGuid can change, invalidating the texture's caching/shaders
		// making shader compile and cook times increase
		const void* pSource = Texture->GetPlatformData()->Mips[MipmapsToSkip].BulkData.LockReadOnly();

		if (pSource)
		{
			FMemory::Memcpy(pResult->GetData(), pSource, pResult->GetDataSize());
			Texture->GetPlatformData()->Mips[MipmapsToSkip].BulkData.Unlock();
		}
		else
		{
			check(false);
		}

#endif

		return pResult;
	}
}


mu::EImageFormat GetMutablePixelFormat(EPixelFormat InTextureFormat)
{
	switch (InTextureFormat)
	{
	case PF_B8G8R8A8: return mu::EImageFormat::IF_BGRA_UBYTE;
	case PF_R8G8B8A8: return mu::EImageFormat::IF_RGBA_UBYTE;
	case PF_DXT1: return mu::EImageFormat::IF_BC1;
	case PF_DXT3: return mu::EImageFormat::IF_BC2;
	case PF_DXT5: return mu::EImageFormat::IF_BC3;
	case PF_BC4: return mu::EImageFormat::IF_BC4;
	case PF_BC5: return mu::EImageFormat::IF_BC5;
	case PF_G8: return mu::EImageFormat::IF_L_UBYTE;
	case PF_ASTC_4x4: return mu::EImageFormat::IF_ASTC_4x4_RGBA_LDR;
	case PF_ASTC_8x8: return mu::EImageFormat::IF_ASTC_8x8_RGBA_LDR;
	case PF_ASTC_12x12: return mu::EImageFormat::IF_ASTC_12x12_RGBA_LDR;
	default: return mu::EImageFormat::IF_NONE;
	}
}


//-------------------------------------------------------------------------------------------------
TTuple<FGraphEventRef, TFunction<void()>> FUnrealMutableImageProvider::GetImageAsync(mu::EXTERNAL_IMAGE_ID id, uint8 MipmapsToSkip, TFunction<void (mu::Ptr<mu::Image>)>& ResultCallback)
{
	// Thread: worker
	MUTABLE_CPUPROFILER_SCOPE(FUnrealMutableImageProvider::GetImage);

	// Out Texture
	mu::ImagePtr Image;

	// Some data that may have to be copied from the GlobalExternalImages while it's locked
	IBulkDataIORequest* IORequest = nullptr;
	const int32 LODs = 1;

	EPixelFormat Format = EPixelFormat::PF_Unknown;
	int32 BulkDataSize = 0;

	mu::EImageFormat MutImageFormat = mu::EImageFormat::IF_NONE;
	int32 MutImageDataSize = 0;

	auto TrivialReturn = []() -> TTuple<FGraphEventRef, TFunction<void()>>
	{
		FGraphEventRef CompletionEvent = FGraphEvent::CreateGraphEvent();
		CompletionEvent->DispatchSubsequents();

		return MakeTuple(CompletionEvent, []() -> void {});
	};

	{
		FScopeLock Lock(&ExternalImagesLock);
		// Inside this scope it's safe to access GlobalExternalImages


		if (!GlobalExternalImages.Contains(id))
		{
			// Null case, no image was provided
			ResultCallback(CreateDummy());
			return Invoke(TrivialReturn);
		}

		const FUnrealMutableImageInfo& ImageInfo = GlobalExternalImages[id];

		if (ImageInfo.Image)
		{
			// Easy case where the image was directly provided
			ResultCallback(ImageInfo.Image);
			return Invoke(TrivialReturn);
		}
		else if (UTexture2D* TextureToLoad = ImageInfo.TextureToLoad)
		{
			// It's safe to access TextureToLoad because ExternalImagesLock guarantees that the data in GlobalExternalImages is valid,
			// not being modified by the game thread at the moment and the texture cannot be GCed because of the AddReferencedObjects
			// in the FUnrealMutableImageProvider

			uint8 MipIndex = MipmapsToSkip < TextureToLoad->GetPlatformData()->Mips.Num() ? MipmapsToSkip : TextureToLoad->GetPlatformData()->Mips.Num() - 1;

			// Mips in the mip tail are inlined and can't be streamed, find the smallest mip available.
			for (; MipIndex > 0; --MipIndex)
			{
				if (TextureToLoad->GetPlatformData()->Mips[MipIndex].BulkData.CanLoadFromDisk())
				{
					break;
				}
			}

#if WITH_EDITOR
			// In the editor the src data can be directly accessed
			ResultCallback(ConvertTextureUnrealToMutable(TextureToLoad, MipIndex));
			return Invoke(TrivialReturn);
#else
			// Texture format and the equivalent mutable format
			Format = TextureToLoad->GetPlatformData()->PixelFormat;
			MutImageFormat = GetMutablePixelFormat(Format);

			// Check if it's a format we support
			if (MutImageFormat == mu::EImageFormat::IF_NONE)
			{
				UE_LOG(LogMutable, Warning, TEXT("Failed to get external image. Unexpected image format. EImageFormat [%s]."), GetPixelFormatString(Format));
				ResultCallback(CreateDummy());
				return Invoke(TrivialReturn);
			}

			int SizeX = TextureToLoad->GetSizeX() >> MipIndex;
			int SizeY = TextureToLoad->GetSizeY() >> MipIndex;

			Image = new mu::Image(SizeX, SizeY, LODs, MutImageFormat);
			MutImageDataSize = Image->GetDataSize();

			// In a packaged game the bulk data has to be loaded
			// Get the actual file to read the mip 0 data, do not keep any reference to TextureToLoad because once outside of the lock
			// it may be GCed or changed. Just keep the actual file handle and some sizes instead of the texture
			FByteBulkData& BulkData = TextureToLoad->GetPlatformData()->Mips[MipIndex].BulkData;
			BulkDataSize = BulkData.GetBulkDataSize();
			check(BulkDataSize > 0);

			if (BulkDataSize != MutImageDataSize)
			{
				UE_LOG(LogMutable, Warning, TEXT("Failed to get external image. Bulk data size is different than the expected size. BulkData size [%d]. Mutable image data size [%d]."),
					BulkDataSize, MutImageDataSize);

				ResultCallback(CreateDummy());
				return Invoke(TrivialReturn);
			}

			// Create a streaming request if the data is not loaded or copy the mip data
			if (!BulkData.IsBulkDataLoaded())
			{
				FGraphEventRef IORequestCompletionEvent = FGraphEvent::CreateGraphEvent();

				TFunction<void(bool, IBulkDataIORequest*)> IOCallback =
					[
						MutImageDataSize,
						MutImageFormat,
						Format,
						Image,
						BulkDataSize,	
						ResultCallback, // Notice ResultCallback is captured by copy
						IORequestCompletionEvent
					](bool bWasCancelled, IBulkDataIORequest* IORequest)
				{
					ON_SCOPE_EXIT
					{
						if (IORequestCompletionEvent.IsValid())
						{
							IORequestCompletionEvent->DispatchSubsequents();
						}
					};
					
					// Should we do someting different than returning a dummy image if cancelled?
					if (bWasCancelled)
					{
						ResultCallback(CreateDummy());
						return;
					}

					uint8* Results = IORequest->GetReadResults(); // required?

					if (Results && Image->GetDataSize() == (int32)IORequest->GetSize())
					{
						check(BulkDataSize == (int32)IORequest->GetSize());
						check(Results == Image->GetData());

						ResultCallback(Image);
						return;
					}

					if (!Results)
					{
						UE_LOG(LogMutable, Warning, TEXT("Failed to get external image. IO Request failed. Request results [%hhd]. Format: [%s]. MutableFormat: [%d]."),
							(Results != nullptr),
							GetPixelFormatString(Format),
							(int32)MutImageFormat);
					}
					else if (MutImageDataSize != (int32)IORequest->GetSize())
					{
						UE_LOG(LogMutable, Warning, TEXT("Failed to get external image. Requested size is different than the expected size. RequestSize: [%lld]. ExpectedSize: [%d]. Format: [%s]. MutableFormat: [%d]."),
							IORequest->GetSize(),
							Image->GetDataSize(),
							GetPixelFormatString(Format),
							(int32)MutImageFormat);
					}
					else
					{
						UE_LOG(LogMutable, Warning, TEXT("Failed to get external image."));
					}

					// Something failed when loading the bulk data, just return a dummy
					ResultCallback(CreateDummy());
				};
			
				// Is the resposability of the CreateStreamingRequest caller to delete the IORequest. 
				// This can *not* be done in the IOCallback because it would cause a deadlock so it is deferred to the returned
				// cleanup function. Another solution could be to spwan a new task that depends on the 
				// IORequestComplitionEvent which deletes it.
				IORequest = BulkData.CreateStreamingRequest(EAsyncIOPriorityAndFlags::AIOP_High, &IOCallback, Image->GetData());

				if (IORequest)
				{
					// Make the lambda mutable and set the IORequest pointer to null when deleted so it is safer 
					// agains multiple calls.
					const auto DeleteIORequest = [IORequest]() mutable -> void
					{
						if (IORequest)
						{
							delete IORequest;
						}
						
						IORequest = nullptr;
					};

					return MakeTuple(IORequestCompletionEvent, DeleteIORequest);

				}
				else
				{
					UE_LOG(LogMutable, Warning, TEXT("Failed to create an IORequest for a UTexture2D BulkData for an application-specific image parameter."));

					ResultCallback(CreateDummy());
					return Invoke(TrivialReturn);
				}
			}
			else
			{
				// Bulk data already loaded
				const void* Data = BulkData.IsUnlocked() ? BulkData.LockReadOnly() : nullptr; // TODO: Retry if it fails?
				
				if (Data)
				{
					FMemory::Memcpy(Image->GetData(), Data, BulkDataSize);

					BulkData.Unlock();
					ResultCallback(Image);
					return Invoke(TrivialReturn);
				}
				else
				{
					UE_LOG(LogMutable, Warning, TEXT("Failed to get external image. Bulk data already locked or null."));
					ResultCallback(CreateDummy());
					return Invoke(TrivialReturn);
				}
			}
#endif
		}
		else
		{
			// No UTexture2D was provided, cannot do anything, just provide a dummy texture
			UE_LOG(LogMutable, Warning, TEXT("No UTexture2D was provided for an application-specific image parameter."));
			ResultCallback(CreateDummy());
			return Invoke(TrivialReturn);
		}
	}

	// Make sure the returned event is dispatched at some point for all code paths, 
	// in this case returning Invoke(TrivialReturn) or through the IORequest callback.
}


// This should mantain parity with the descriptor of the images generated by GetImageAsync 
mu::FImageDesc FUnrealMutableImageProvider::GetImageDesc(mu::EXTERNAL_IMAGE_ID id, uint8 MipmapsToSkip) 
{
	MUTABLE_CPUPROFILER_SCOPE(FUnrealMutableImageProvider::GetImageDesc);

	mu::FImageDesc Result;	
	{
		FScopeLock Lock(&ExternalImagesLock);
		// Inside this scope it's safe to access GlobalExternalImages

		if (!GlobalExternalImages.Contains(id))
		{
			// Null case, no image was provided
			return CreateDummyDesc();
		}

		const FUnrealMutableImageInfo& ImageInfo = GlobalExternalImages[id];

		if (ImageInfo.Image)
		{
			// Easy case where the image was directly provided
			Result = mu::FImageDesc(ImageInfo.Image->GetSize(), ImageInfo.Image->GetFormat(), ImageInfo.Image->GetLODCount());
		}
		else if (UTexture2D* TextureToLoad = ImageInfo.TextureToLoad)
		{
			// It's safe to access TextureToLoad because ExternalImagesLock guarantees that the data in GlobalExternalImages is valid,
			// not being modified by the game thread at the moment and the texture cannot be GCed because of the AddReferencedObjects
			// in the FUnrealMutableImageProvider

			uint8 MipIndex = MipmapsToSkip < TextureToLoad->GetPlatformData()->Mips.Num() ? MipmapsToSkip : TextureToLoad->GetPlatformData()->Mips.Num() - 1;

			// Mips in the mip tail are inlined and can't be streamed, find the smallest mip available.
			for (; MipIndex > 0; --MipIndex)
			{
				if (TextureToLoad->GetPlatformData()->Mips[MipIndex].BulkData.CanLoadFromDisk())
				{
					break;
				}
			}

			// Texture format and the equivalent mutable format
			const EPixelFormat Format = TextureToLoad->GetPlatformData()->PixelFormat;
			const mu::EImageFormat MutableFormat = GetMutablePixelFormat(Format);

			// Check if it's a format we support
			if (MutableFormat == mu::EImageFormat::IF_NONE)
			{
				UE_LOG(LogMutable, Warning, TEXT("Failed to get external image descriptor. Unexpected image format. EImageFormat [%s]."), GetPixelFormatString(Format));
				return CreateDummyDesc();
			}

			const mu::FImageSize ImageSize = mu::FImageSize(
					TextureToLoad->GetSizeX() >> MipIndex,
					TextureToLoad->GetSizeY() >> MipIndex);

			const int32 Lods = 1;

			Result = mu::FImageDesc(ImageSize, MutableFormat, Lods);
		}
		else
		{
			// No UTexture2D was provided, cannot do anything, just provide a dummy texture
			UE_LOG(LogMutable, Warning, TEXT("No UTexture2D was provided for an application-specific image parameter descriptor."));
			return CreateDummyDesc();
		}

		return Result;
	}
}


void FUnrealMutableImageProvider::CacheImage(mu::EXTERNAL_IMAGE_ID id)
{
	check( IsInGameThread() );

	mu::ImagePtr pResult;
	UTexture2D* UnrealDeferredTexture = nullptr;

	// See if any provider provides this id.
	for (int32 p = 0; !pResult && p < ImageProviders.Num(); ++p)
	{
		TWeakObjectPtr<UCustomizableSystemImageProvider> Provider = ImageProviders[p];

		if (Provider.IsValid())
		{
			switch (Provider->HasTextureParameterValue(id))
			{

			case UCustomizableSystemImageProvider::ValueType::Raw:
			{
				FIntVector desc = Provider->GetTextureParameterValueSize(id);
				pResult = new mu::Image(desc[0], desc[1], 1, mu::EImageFormat::IF_RGBA_UBYTE);
				Provider->GetTextureParameterValueData(id, pResult->GetData());
				break;
			}

			case UCustomizableSystemImageProvider::ValueType::Unreal:
			{
				UTexture2D* UnrealTexture = Provider->GetTextureParameterValue(id);
				pResult = ConvertTextureUnrealToMutable(UnrealTexture, 0);
				break;
			}

			case UCustomizableSystemImageProvider::ValueType::Unreal_Deferred:
			{
				UnrealDeferredTexture = Provider->GetTextureParameterValue(id);
				break;
			}

			default:
				break;
			}
		}
	}

	if (!pResult && !UnrealDeferredTexture)
	{
		pResult = CreateDummy();
	}

	{
		FScopeLock Lock(&ExternalImagesLock);

		GlobalExternalImages.Add(id, FUnrealMutableImageInfo(pResult, UnrealDeferredTexture));
	}
}


void FUnrealMutableImageProvider::UnCacheImage(mu::EXTERNAL_IMAGE_ID id)
{
	check(IsInGameThread());

	FScopeLock Lock(&ExternalImagesLock);
	GlobalExternalImages.Remove(id);
}


void FUnrealMutableImageProvider::CacheAllImagesInAllProviders(bool bClearPreviousCacheImages)
{
	check(IsInGameThread());

	FScopeLock Lock(&ExternalImagesLock);

	if (bClearPreviousCacheImages)
	{
		ClearCache();
	}

	// See if any provider provides this id.
	for (int32 p = 0; p < ImageProviders.Num(); ++p)
	{
		TWeakObjectPtr<UCustomizableSystemImageProvider> Provider = ImageProviders[p];

		if (Provider.IsValid())
		{
			TArray<FCustomizableObjectExternalTexture> OutValues;
			Provider->GetTextureParameterValues(OutValues);

			for (const FCustomizableObjectExternalTexture& Value : OutValues)
			{
				CacheImage(Value.Value);
			}
		}
	}
}


void FUnrealMutableImageProvider::ClearCache()
{
	check(IsInGameThread());

	FScopeLock Lock(&ExternalImagesLock);
	GlobalExternalImages.Empty();
}


mu::ImagePtr FUnrealMutableImageProvider::CreateDummy()
{
	// Create a dummy image
	const int size = DUMMY_IMAGE_DESC.m_size[0];
	const int checkerSize = 4;
	constexpr int checkerTileCount = 2;
	uint8_t colours[checkerTileCount][4] = { { 255,255,0,255 },{ 0,0,255,255 } };

	mu::ImagePtr pResult = new mu::Image(size, size, DUMMY_IMAGE_DESC.m_lods, DUMMY_IMAGE_DESC.m_format);

	uint8_t* pData = pResult->GetData();
	for (int x = 0; x < size; ++x)
	{
		for (int y = 0; y < size; ++y)
		{
			int checkerIndex = ((x / checkerSize) + (y / checkerSize)) % checkerTileCount;
			pData[0] = colours[checkerIndex][0];
			pData[1] = colours[checkerIndex][1];
			pData[2] = colours[checkerIndex][2];
			pData[3] = colours[checkerIndex][3];
			pData += 4;
		}
	}

	return pResult;
}


mu::FImageDesc FUnrealMutableImageProvider::CreateDummyDesc()
{
	return DUMMY_IMAGE_DESC;
}


void FUnrealMutableImageProvider::AddReferencedObjects(FReferenceCollector& Collector)
{
	for (TPair<uint64, FUnrealMutableImageInfo>& Image : GlobalExternalImages)
	{
		if (Image.Value.TextureToLoad)
		{
			Collector.AddReferencedObject(Image.Value.TextureToLoad);
		}
	}
}
