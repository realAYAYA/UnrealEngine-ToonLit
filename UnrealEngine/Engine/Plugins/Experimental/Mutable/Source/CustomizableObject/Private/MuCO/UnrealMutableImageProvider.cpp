// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/UnrealMutableImageProvider.h"

#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/UnrealToMutableTextureConversionUtils.h"
#include "TextureResource.h"
#include "MuR/Parameters.h"
#include "MuR/ImageTypes.h"


//-------------------------------------------------------------------------------------------------
namespace
{

	void ConvertTextureUnrealToMutable(mu::Image* OutResult, UTexture2D* Texture, bool bIsNormalComposite, uint8 MipmapsToSkip)
	{		
#if WITH_EDITOR

		EUnrealToMutableConversionError Error = ConvertTextureUnrealSourceToMutable(OutResult, Texture, bIsNormalComposite, MipmapsToSkip);
		check(Error==EUnrealToMutableConversionError::Success);

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

		// If not locked ReadOnly the Texture Source's FGuid can change, invalidating the texture's caching/shaders
		// making shader compile and cook times increase
		const void* pSource = Texture->GetPlatformData()->Mips[MipmapsToSkip].BulkData.LockReadOnly();

		if (pSource)
		{
			OutResult->Init(SizeX, SizeY, LODs, MutableFormat, mu::EInitializationType::NotInitialized);
			FMemory::Memcpy(OutResult->GetLODData(0), pSource, OutResult->GetLODDataSize(0));
			Texture->GetPlatformData()->Mips[MipmapsToSkip].BulkData.Unlock();
		}
		else
		{
			check(false);
			OutResult->Init(SizeX, SizeY, LODs, MutableFormat, mu::EInitializationType::Black);
		}

#endif
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
	case PF_ASTC_6x6: return mu::EImageFormat::IF_ASTC_6x6_RGBA_LDR;
	case PF_ASTC_8x8: return mu::EImageFormat::IF_ASTC_8x8_RGBA_LDR;
	case PF_ASTC_10x10: return mu::EImageFormat::IF_ASTC_10x10_RGBA_LDR;
	case PF_ASTC_12x12: return mu::EImageFormat::IF_ASTC_12x12_RGBA_LDR;
	default: return mu::EImageFormat::IF_NONE;
	}
}


#if WITH_EDITOR

bool FUnrealMutableImageProvider::Tick()
{
	FReferencedImageRequest* Request=nullptr;

	// Process only one request per tick, or zero.
	if (!QueuedReferencedImageRequests.Dequeue(Request))
	{
		return true;
	}

	// Find the CO for this model.
	UCustomizableObject* CO = nullptr;
	for (TObjectIterator<UCustomizableObject> It; It; ++It)
	{
		if (IsValid(*It) && It->GetPrivate()->GetModel().Get() == Request->ModelPtr)
		{
			CO = *It;
			break;
		}
	}

	if (!CO)
	{
		// The CO for this request has been unloaded!
		check(false);
		return true;
	}

	const FModelResources& ModelResources = CO->GetPrivate()->GetModelResources();
	if (!ModelResources.PassThroughTextures.IsValidIndex(Request->Id))
	{
		// The id is not valid for this CO
		check(false);
		return true;
	}

	// Find the texture id
	TSoftObjectPtr<UTexture> TexturePtr = ModelResources.PassThroughTextures[Request->Id];

	// This can cause a stall because of loading the asset.
	UTexture2D* Texture = Cast<UTexture2D>( TexturePtr.LoadSynchronous() );
	if (!Texture)
	{
		// Failed to load the texture
		check(false);
		return true;
	}

	// In the editor the src data can be directly accessed
	int32 MipIndex = (Request->MipmapsToSkip < Texture->GetPlatformData()->Mips.Num()) ? Request->MipmapsToSkip : Texture->GetPlatformData()->Mips.Num() - 1;
	check(MipIndex >= 0);
	bool bIsNormalComposite = false; // TODO?
	ConvertTextureUnrealToMutable(Request->ResultImage.get(), Texture, bIsNormalComposite, MipIndex);
	Request->CompletionEvent.Trigger();

	return true;
}

#endif


//-------------------------------------------------------------------------------------------------
TTuple<UE::Tasks::FTask, TFunction<void()>> FUnrealMutableImageProvider::GetImageAsync(FName Id, uint8 MipmapsToSkip, TFunction<void(mu::Ptr<mu::Image>)>& ResultCallback)
{
	// Thread: worker
	MUTABLE_CPUPROFILER_SCOPE(FUnrealMutableImageProvider::GetImage);

	// Some data that may have to be copied from the GlobalExternalImages while it's locked
	IBulkDataIORequest* IORequest = nullptr;
	const int32 LODs = 1;

	EPixelFormat Format = EPixelFormat::PF_Unknown;
	int32 BulkDataSize = 0;

	mu::EImageFormat MutImageFormat = mu::EImageFormat::IF_NONE;
	int32 MutImageDataSize = 0;

	auto TrivialReturn = []() -> TTuple<UE::Tasks::FTask, TFunction<void()>>
	{
		return MakeTuple(UE::Tasks::MakeCompletedTask<void>(), []() -> void {});
	};

	{
		FScopeLock Lock(&ExternalImagesLock);
		// Inside this scope it's safe to access GlobalExternalImages


		if (!GlobalExternalImages.Contains(Id))
		{
			// Null case, no image was provided
			UE_LOG(LogMutable, Warning, TEXT("Failed to get external image [%s]. GlobalExternalImage not found."), *Id.ToString());

			ResultCallback(CreateDummy());
			return Invoke(TrivialReturn);
		}

		const FUnrealMutableImageInfo& ImageInfo = GlobalExternalImages[Id];

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

#if WITH_EDITOR
			if (!TextureToLoad->IsAsyncCacheComplete())
			{
				// If the UE texture is being compiled/processed in a background process, it's not safe to access
				FString TextureName;
				TextureToLoad->GetName(TextureName);
				UE_LOG(LogMutable, Warning, TEXT("Failed to get the external texture [%s] because it's still being async cached. Wait until all background processed finish and retry again."), *TextureName);
				ResultCallback(CreateDummy());
				return Invoke(TrivialReturn);
			}
#endif

			int32 MipIndex = MipmapsToSkip < TextureToLoad->GetPlatformData()->Mips.Num() ? MipmapsToSkip : TextureToLoad->GetPlatformData()->Mips.Num() - 1;
			check (MipIndex >= 0);

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
			mu::Ptr<mu::Image> Image = new mu::Image();
			bool bIsNormalComposite = false; // TODO?
			ConvertTextureUnrealToMutable(Image.get(), TextureToLoad, bIsNormalComposite,  MipIndex);
			ResultCallback(Image);
			return Invoke(TrivialReturn);
#else
			// Texture format and the equivalent mutable format
			Format = TextureToLoad->GetPlatformData()->PixelFormat;
			MutImageFormat = GetMutablePixelFormat(Format);

			// Check if it's a format we support
			if (MutImageFormat == mu::EImageFormat::IF_NONE)
			{
				UE_LOG(LogMutable, Warning, TEXT("Failed to get external image [%s]. Unexpected image format. EImageFormat [%s]."), *Id.ToString(), GetPixelFormatString(Format));
				ResultCallback(CreateDummy());
				return Invoke(TrivialReturn);
			}

			int32 SizeX = TextureToLoad->GetSizeX() >> MipIndex;
			int32 SizeY = TextureToLoad->GetSizeY() >> MipIndex;

			check(LODs == 1);
			mu::Ptr<mu::Image> Image = new mu::Image(SizeX, SizeY, LODs, MutImageFormat, mu::EInitializationType::NotInitialized);
			TArrayView<uint8> MutImageDataView = Image->DataStorage.GetLOD(0);

			// In a packaged game the bulk data has to be loaded
			// Get the actual file to read the mip 0 data, do not keep any reference to TextureToLoad because once outside of the lock
			// it may be GCed or changed. Just keep the actual file handle and some sizes instead of the texture
			FByteBulkData& BulkData = TextureToLoad->GetPlatformData()->Mips[MipIndex].BulkData;
			BulkDataSize = BulkData.GetBulkDataSize();
			check(BulkDataSize > 0);

			if (BulkDataSize != MutImageDataView.Num())
			{
				UE_LOG(LogMutable, Warning, TEXT("Failed to get external image [%s]. Bulk data size is different than the expected size. BulkData size [%d]. Mutable image data size [%d]."),
					*Id.ToString(),	BulkDataSize, MutImageDataSize);

				ResultCallback(CreateDummy());
				return Invoke(TrivialReturn);
			}

			// Create a streaming request if the data is not loaded or copy the mip data
			if (!BulkData.IsBulkDataLoaded())
			{
				UE::Tasks::FTaskEvent IORequestCompletionEvent(TEXT("Mutable_IORequestCompletionEvent"));

				TFunction<void(bool, IBulkDataIORequest*)> IOCallback =
					[
						MutImageDataView,
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
						UE::Tasks::FTaskEvent EventCopy = IORequestCompletionEvent;
						EventCopy.Trigger();
					};
					
					// Should we do someting different than returning a dummy image if cancelled?
					if (bWasCancelled)
					{
						UE_LOG(LogMutable, Warning, TEXT("Failed to get external image. Cancelled IO Request"));
						ResultCallback(CreateDummy());
						return;
					}

					uint8* Results = IORequest->GetReadResults(); // required?

					if (Results && MutImageDataView.Num() == (int32)IORequest->GetSize())
					{
						check(BulkDataSize == (int32)IORequest->GetSize());
						check(Results == MutImageDataView.GetData());

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
					else if (MutImageDataView.Num() != (int32)IORequest->GetSize())
					{
						UE_LOG(LogMutable, Warning, TEXT("Failed to get external image. Requested size is different than the expected size. RequestSize: [%lld]. ExpectedSize: [%d]. Format: [%s]. MutableFormat: [%d]."),
							IORequest->GetSize(),
							MutImageDataView.Num(),
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
				IORequest = BulkData.CreateStreamingRequest(EAsyncIOPriorityAndFlags::AIOP_High, &IOCallback, MutImageDataView.GetData());

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

					IORequestCompletionEvent.Trigger();
					
					ResultCallback(CreateDummy());
					return Invoke(TrivialReturn);
				}
			}
			else
			{
				// Bulk data already loaded
				const void* Data = (!BulkData.IsLocked()) ? BulkData.LockReadOnly() : nullptr; // TODO: Retry if it fails?
				
				if (Data)
				{
					FMemory::Memcpy(MutImageDataView.GetData(), Data, BulkDataSize);

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


//-------------------------------------------------------------------------------------------------
TTuple<UE::Tasks::FTask, TFunction<void()>> FUnrealMutableImageProvider::GetReferencedImageAsync(const void* ModelPtr, int32 Id, uint8 MipmapsToSkip, TFunction<void(mu::Ptr<mu::Image>)>& ResultCallback)
{
	// Thread: worker
	MUTABLE_CPUPROFILER_SCOPE(FUnrealMutableImageProvider::GetReferencedImageAsync);

	auto TrivialReturn = []() -> TTuple<UE::Tasks::FTask, TFunction<void()>>
	{
		return MakeTuple(UE::Tasks::MakeCompletedTask<void>(), []() -> void {});
	};


#if WITH_EDITOR

	TUniquePtr<FReferencedImageRequest> Request( new FReferencedImageRequest(UE::Tasks::FTaskEvent(TEXT("GetReferencedImageCompletion"))) );
	Request->ModelPtr = ModelPtr;
	Request->Id = Id;
	Request->MipmapsToSkip = MipmapsToSkip;
	Request->ResultImage = new mu::Image();

	QueuedReferencedImageRequests.Enqueue(Request.Get());

	if (IsInGameThread())
	{
		// This may happen in the mutable debugger.
		while (!Request->CompletionEvent.IsCompleted())
		{
			Tick();
		}
	}
	else
	{
		Request->CompletionEvent.BusyWait();
	}

	ResultCallback(Request->ResultImage);
	return Invoke(TrivialReturn);

#else // WITH_EDITOR

	// Not supported outside editor yet.
	UE_LOG(LogMutable, Warning, TEXT("Failed to get reference image. Only supported in editor."));

	ResultCallback(CreateDummy());
	return Invoke(TrivialReturn);

#endif
}


// This should mantain parity with the descriptor of the images generated by GetImageAsync 
mu::FImageDesc FUnrealMutableImageProvider::GetImageDesc(FName Id, uint8 MipmapsToSkip)
{
	MUTABLE_CPUPROFILER_SCOPE(FUnrealMutableImageProvider::GetImageDesc);

	mu::FImageDesc Result;	
	{
		FScopeLock Lock(&ExternalImagesLock);
		// Inside this scope it's safe to access GlobalExternalImages

		if (!GlobalExternalImages.Contains(Id))
		{
			// Null case, no image was provided
			return CreateDummyDesc();
		}

		const FUnrealMutableImageInfo& ImageInfo = GlobalExternalImages[Id];

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

			int32 MipIndex = MipmapsToSkip < TextureToLoad->GetPlatformData()->Mips.Num() ? MipmapsToSkip : TextureToLoad->GetPlatformData()->Mips.Num() - 1;

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


void FUnrealMutableImageProvider::CacheImage(FName Id, bool bUser)
{
	if (Id == NAME_None)
	{
		return;	
	}	

	{
		FScopeLock Lock(&ExternalImagesLock);

		if (FUnrealMutableImageInfo* Result = GlobalExternalImages.Find(Id))
		{
			if (bUser)
			{
				Result->ReferencesUser = true;			
			}
			else
			{
				Result->ReferencesSystem++;							
			}
		}
		else
		{
			mu::ImagePtr pResult;
			UTexture2D* UnrealDeferredTexture = nullptr;

			// See if any provider provides this id.
			for (int32 p = 0; !pResult && p < ImageProviders.Num(); ++p)
			{
				TWeakObjectPtr<UCustomizableSystemImageProvider> Provider = ImageProviders[p];

				if (Provider.IsValid())
				{
					// \TODO: all these queries could probably be optimized into a single call.
					switch (Provider->HasTextureParameterValue(Id))
					{

					case UCustomizableSystemImageProvider::ValueType::Raw:
					{
						FIntVector desc = Provider->GetTextureParameterValueSize(Id);
						pResult = new mu::Image(desc[0], desc[1], 1, mu::EImageFormat::IF_RGBA_UBYTE, mu::EInitializationType::Black);
						Provider->GetTextureParameterValueData(Id, pResult->GetLODData(0));
						break;
					}

					case UCustomizableSystemImageProvider::ValueType::Unreal:
					{
						UTexture2D* UnrealTexture = Provider->GetTextureParameterValue(Id);
						pResult = new mu::Image();
						bool bIsNormalComposite = false;
						ConvertTextureUnrealToMutable(pResult.get(), UnrealTexture, bIsNormalComposite, 0);
						break;
					}

					case UCustomizableSystemImageProvider::ValueType::Unreal_Deferred:
					{
						UnrealDeferredTexture = Provider->GetTextureParameterValue(Id);
						break;
					}

					default:
						break;
					}
				}
			}

			if (!pResult && !UnrealDeferredTexture)
			{
				UE_LOG(LogMutable, Warning, TEXT("Failed to cache external image %s. Missing result or source texture. Result [%d]. Unreal texture [%d]. Num providers [%d]"),
					*Id.ToString(),
					pResult ? 1 : 0,
					UnrealDeferredTexture ? 1 : 0,
					ImageProviders.Num());

				return;
			}
			
			FUnrealMutableImageInfo ImageInfo(pResult, UnrealDeferredTexture);

			if (bUser)
			{
				ImageInfo.ReferencesUser = true;			
			}
			else
			{
				ImageInfo.ReferencesSystem++;							
			}
			
			GlobalExternalImages.Add(Id, ImageInfo);
		}
	}
}


void FUnrealMutableImageProvider::UnCacheImage(FName Id, bool bUser)
{
	if (Id == NAME_None)
	{
		return;	
	}
	
	FScopeLock Lock(&ExternalImagesLock);
	if (FUnrealMutableImageInfo* Result = GlobalExternalImages.Find(Id))
	{
		if (bUser)
		{
			Result->ReferencesUser = false;			
		}
		else
		{
			--Result->ReferencesSystem;
			check(Result->ReferencesSystem >= 0); // Something went wrong. The image has been uncached more times than it has been cached.
		}
		
		if (Result->ReferencesUser + Result->ReferencesSystem == 0)
		{
			GlobalExternalImages.Remove(Id);		
		}
	}
	else
	{
		UE_LOG(LogMutable, Warning, TEXT("Failed to uncache external image %s. Possible double free!"), *Id.ToString());
	}
}


void FUnrealMutableImageProvider::ClearCache(bool bUser)
{
	FScopeLock Lock(&ExternalImagesLock);
	for (TTuple<FName, FUnrealMutableImageInfo> Tuple : GlobalExternalImages)
	{
		UnCacheImage(Tuple.Key, bUser);
	}	
}


void FUnrealMutableImageProvider::CacheImages(const mu::Parameters& Parameters)
{
	const int32 NumParams = Parameters.GetCount();
	for (int32 ParamIndex = 0; ParamIndex < NumParams; ++ParamIndex)
	{
		if (Parameters.GetType(ParamIndex) != mu::PARAMETER_TYPE::T_IMAGE)
		{
			continue;
		}
	
		{
			const FName TextureId = Parameters.GetImageValue(ParamIndex);
			CacheImage(TextureId, false);
		}

		const int32 NumValues = Parameters.GetValueCount(ParamIndex);
		for (int32 ValueIndex = 0; ValueIndex < NumValues; ++ValueIndex)
		{
			mu::Ptr<const mu::RangeIndex> Range = Parameters.GetValueIndex(ParamIndex, ValueIndex);
			const FName TextureId = Parameters.GetImageValue(ParamIndex, Range);

			CacheImage(TextureId, false);
		}
	}
}


void FUnrealMutableImageProvider::UnCacheImages(const mu::Parameters& Parameters)
{
	const int32 NumParams = Parameters.GetCount();
	for (int32 ParamIndex = 0; ParamIndex < NumParams; ++ParamIndex)
	{
		if (Parameters.GetType(ParamIndex) != mu::PARAMETER_TYPE::T_IMAGE)
		{
			continue;
		}
	
		{
			const FName TextureId = Parameters.GetImageValue(ParamIndex);
			UnCacheImage(TextureId, false);				
		}

		const int32 NumValues = Parameters.GetValueCount(ParamIndex);
		for (int32 ValueIndex = 0; ValueIndex < NumValues; ++ValueIndex)
		{
			mu::Ptr<const mu::RangeIndex> Range = Parameters.GetValueIndex(ParamIndex, ValueIndex);
			const FName TextureId = Parameters.GetImageValue(ParamIndex, Range);

			UnCacheImage(TextureId, false);
		}
	}
}


mu::ImagePtr FUnrealMutableImageProvider::CreateDummy()
{
	// Create a dummy image
	const int32 Size = DUMMY_IMAGE_DESC.m_size[0];
	const int32 CheckerSize = 4;
	constexpr int32 CheckerTileCount = 2;
	
#if !UE_BUILD_SHIPPING
	uint8 Colors[CheckerTileCount][4] = {{255, 255, 0, 255}, {0, 0, 255, 255}};
#else
	uint8 Colors[CheckerTileCount][4] = {{255, 255, 0, 0}, {0, 0, 255, 0}};
#endif

	mu::ImagePtr pResult = new mu::Image(Size, Size, DUMMY_IMAGE_DESC.m_lods, DUMMY_IMAGE_DESC.m_format, mu::EInitializationType::NotInitialized);

	check(pResult->GetLODCount() == 1);
	check(pResult->GetFormat() == mu::EImageFormat::IF_RGBA_UBYTE || pResult->GetFormat() == mu::EImageFormat::IF_BGRA_UBYTE);
	uint8* pData = pResult->GetLODData(0);
	for (int32 X = 0; X < Size; ++X)
	{
		for (int32 Y = 0; Y < Size; ++Y)
		{
			int32 CheckerIndex = ((X / CheckerSize) + (Y / CheckerSize)) % CheckerTileCount;
			pData[0] = Colors[CheckerIndex][0];
			pData[1] = Colors[CheckerIndex][1];
			pData[2] = Colors[CheckerIndex][2];
			pData[3] = Colors[CheckerIndex][3];
			pData += 4;
		}
	}

	return pResult;
}


mu::FImageDesc FUnrealMutableImageProvider::CreateDummyDesc()
{
	return DUMMY_IMAGE_DESC;
}


TAutoConsoleVariable<bool> CVarMutableLockExternalImagesDuringGC(
	TEXT("Mutable.LockExternalImagesDuringGC"),
	true,
	TEXT("If true, GlobalExternalImages where all texture parameters are stored will be locked from concurrent access during the AddReferencedObjects phase of GC."),
	ECVF_Default);


void FUnrealMutableImageProvider::AddReferencedObjects(FReferenceCollector& Collector)
{
	bool bDoLock = CVarMutableLockExternalImagesDuringGC.GetValueOnAnyThread();

	if (bDoLock)
	{
		ExternalImagesLock.Lock();
	}

	for (TPair<FName, FUnrealMutableImageInfo>& Image : GlobalExternalImages)
	{
		if (Image.Value.TextureToLoad)
		{
			Collector.AddReferencedObject(Image.Value.TextureToLoad);
		}
	}

	if (bDoLock)
	{
		ExternalImagesLock.Unlock();
	}
}
