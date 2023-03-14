// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/UnrealMutableImageProvider.h"

#include "CoreGlobals.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureDefines.h"
#include "HAL/UnrealMemory.h"
#include "Math/IntVector.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObject.h"
#include "MuR/Ptr.h"
#include "UObject/WeakObjectPtr.h"


//-------------------------------------------------------------------------------------------------
namespace
{

	mu::ImagePtr ConvertTextureUnrealToMutable(UTexture2D* Texture)
	{
		mu::ImagePtr pResult;

#if WITH_EDITOR
		int LODs = 1;
		int SizeX = Texture->Source.GetSizeX();
		int SizeY = Texture->Source.GetSizeY();
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
		const uint8* pSource = Texture->Source.LockMipReadOnly(0);
		if (pSource)
		{
			FMemory::Memcpy(pResult->GetData(), pSource, pResult->GetDataSize());
		}

		Texture->Source.UnlockMip(0);
#else
		check(Texture->GetPlatformData()->Mips[0].BulkData.IsBulkDataLoaded());

		int32 LODs = 1;
		int32 SizeX = Texture->GetSizeX();
		int32 SizeY = Texture->GetSizeY();
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
		const void* pSource = Texture->GetPlatformData()->Mips[0].BulkData.LockReadOnly();

		if (pSource)
		{
			FMemory::Memcpy(pResult->GetData(), pSource, pResult->GetDataSize());
		}

		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
#endif

		return pResult;
	}
}


//-------------------------------------------------------------------------------------------------

mu::ImagePtr FUnrealMutableImageProvider::GetImage(mu::EXTERNAL_IMAGE_ID id)
{
	// Thread: Mutable worker

	// Some data that may have to be copied from the GlobalExternalImages while it's locked
	TUniquePtr<IAsyncReadFileHandle> FileHandle;
	int32 LODs = 1;
	int32 SizeX = 0;
	int32 SizeY = 0;
	EPixelFormat Format = EPixelFormat::PF_Unknown;
	int32 BulkDataSize = 0;
	
	{
		FScopeLock Lock(&ExternalImagesLock);
		// Inside this scope it's safe to access GlobalExternalImages

		if (!GlobalExternalImages.Contains(id))
		{
			// Null case, no image was provided
			return CreateDummy();
		}

		const FUnrealMutableImageInfo& ImageInfo = GlobalExternalImages[id];

		if (ImageInfo.Image)
		{
			// Easy case where the image was directly provided
			return ImageInfo.Image;
		}
		else if (UTexture2D* TextureToLoad = ImageInfo.TextureToLoad)
		{
			// It's safe to access TextureToLoad because ExternalImagesLock guarantees that the data in GlobalExternalImages is valid,
			// not being modified by the game thread at the moment and the texture cannot be GCed because of the AddReferencedObjects
			// in the FUnrealMutableImageProvider

#if WITH_EDITOR
			// In the editor the src data can be directly accessed
			return ConvertTextureUnrealToMutable(TextureToLoad);
#else
			// In a packaged game the bulk data has to be loaded
			// Get the actual file to read the mip 0 data, do not keep any reference to TextureToLoad because once outside of the lock
			// it may be GCed or changed. Just keep the actual file handle and some sizes instead of the texture
			FByteBulkData& BulkData = TextureToLoad->GetPlatformData()->Mips[0].BulkData;
			check(BulkData.GetBulkDataSize() > 0);
			FileHandle.Reset(BulkData.OpenAsyncReadHandle());

			SizeX = TextureToLoad->GetSizeX();
			SizeY = TextureToLoad->GetSizeY();
			BulkDataSize = BulkData.GetBulkDataSize();
			Format = TextureToLoad->GetPlatformData()->PixelFormat;
#endif
		}
		else
		{
			// No UTexture2D was provided, cannot do anything, just provide a dummy texture
			UE_LOG(LogMutable, Warning, TEXT("No UTexture2D was provided for an application-specific image parameter."));
			return CreateDummy();
		}
	}

	// Case where the bulk data has to be loaded from disk. The ExternalImagesLock has been unlocked because the file access
	// must not be done while the lock is on as it could stall the game thread for too long
	if (FileHandle)
	{
		mu::ImagePtr Image;
		IAsyncReadRequest* AsyncRequest = FileHandle->ReadRequest(0, BulkDataSize,
			AIOP_FLAG_DONTCACHE | AIOP_BelowNormal, nullptr, nullptr);

		if (AsyncRequest)
		{
			// It's OK to wait since this must run on the Mutable thread
			bool bComplete = AsyncRequest->WaitCompletion();
			uint8* Results = AsyncRequest->GetReadResults();

			if (bComplete && Results)
			{
				// TODO: For the moment only this uncompressed format is supported in packaged games
				check(Format == EPixelFormat::PF_B8G8R8A8);
				mu::EImageFormat MutableFormat = mu::EImageFormat::IF_BGRA_UBYTE;

				check(BulkDataSize == AsyncRequest->GetSizeResults());
				check(Image->GetDataSize() == BulkDataSize);
				Image = new mu::Image(SizeX, SizeY, LODs, MutableFormat);
				FMemory::Memcpy(Image->GetData(), Results, Image->GetDataSize());

				FMemory::Free(Results);
				Results = nullptr;
			}
		}

		delete AsyncRequest;

		if (!Image)
		{
			// Something failed in the loading of the bulk data, just return a dummy
			UE_LOG(LogMutable, Warning, TEXT("UTexture2D BulkData failed loading for an application-specific image parameter."));
			CreateDummy();
		}

		return Image;
	}
	else
	{
		UE_LOG(LogMutable, Warning, TEXT("Failed to get a FileHandle for a UTexture2D BulkData for an application-specific image parameter."));
		return CreateDummy();
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
				pResult = ConvertTextureUnrealToMutable(UnrealTexture);
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
	const int size = 32;
	const int checkerSize = 4;
	constexpr int checkerTileCount = 2;
	uint8_t colours[checkerTileCount][4] = { { 255,255,0,255 },{ 0,0,255,255 } };

	mu::ImagePtr pResult = new mu::Image(size, size, 1, mu::EImageFormat::IF_RGBA_UBYTE);

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
