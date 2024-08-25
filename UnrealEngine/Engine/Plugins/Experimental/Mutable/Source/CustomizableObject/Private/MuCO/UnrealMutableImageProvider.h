// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Image.h"
#include "MuR/System.h"
#include "UObject/GCObject.h"
#include "Containers/Ticker.h"
#include "Containers/Map.h"
#include "Containers/Queue.h"

#include "Tasks/Task.h"

class UTexture2D;


/** Implementation of a mutable core provider for image parameters that are application-specific. */
class FUnrealMutableImageProvider : public mu::ImageParameterGenerator, public FGCObject
{

public:
	// mu::ImageParameterGenerator interface
	// Thread: worker
	virtual TTuple<UE::Tasks::FTask, TFunction<void()>> GetImageAsync(FName Id, uint8 MipmapsToSkip, TFunction<void(mu::Ptr<mu::Image>)>& ResultCallback) override;
	virtual TTuple<UE::Tasks::FTask, TFunction<void()>> GetReferencedImageAsync(const void* ModelPtr, int32 Id, uint8 MipmapsToSkip, TFunction<void(mu::Ptr<mu::Image>)>& ResultCallback) override;

	virtual mu::FImageDesc GetImageDesc(FName Id, uint8 MipmapsToSkip) override;

	
	// Own interface	
	// Thread: Game
	/** Add a reference to the image. If it was not cached it caches it.
	 * @param bUser if true, adds a reference to the user reference counter. If false, adds a reference to the system reference counter. */
	void CacheImage(FName Id, bool bUser);

	/** Removes a reference to the image. If all references are removed, it uncaches the image.
	 * @param bUser if true, removes a reference from the user reference counter. If false, removes a reference from the system reference counter. */
	void UnCacheImage(FName Id, bool bUser);

	/** Removes a reference to all images. All images which no longer have references will be uncached.
	 * @param bUser if true, removes a references from the user reference counter. If false, removes a reference from the system reference counter. */
	void ClearCache(bool bUser);

	void CacheImages(const mu::Parameters& Parameters);
	void UnCacheImages(const mu::Parameters& Parameters);
	
	/** List of actual image providers that have been registered to the CustomizableObjectSystem. */
	TArray< TWeakObjectPtr<class UCustomizableSystemImageProvider> > ImageProviders;

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FUnrealMutableImageProvider");
	}

private:

	struct FUnrealMutableImageInfo
	{
		FUnrealMutableImageInfo() {}
		FUnrealMutableImageInfo(const mu::ImagePtr& InImage, class UTexture2D* InTextureToLoad) : Image(InImage), TextureToLoad(InTextureToLoad) { }

		mu::ImagePtr Image;

		/** If the above Image has not been loaded in the game thread, the TextureToLoad bulk data will be loaded
		* from the Mutable thread when it's needed
		*/
		TObjectPtr<UTexture2D> TextureToLoad = nullptr;

		/** true of the reference maintained by the user. */
		bool ReferencesUser = false;
		
		/** Number of reference maintained by the system. */
		int32 ReferencesSystem = 0;
	};

	static inline const mu::FImageDesc DUMMY_IMAGE_DESC = 
			mu::FImageDesc(mu::FImageSize(32, 32), mu::EImageFormat::IF_RGBA_UBYTE, 1);

	/** This will be called if an image Id has been requested by Mutable core but it has not been provided by any provider. */
	static mu::ImagePtr CreateDummy();
	static mu::FImageDesc CreateDummyDesc();

	/** Map of Ids to external textures that may be required for any instance or Mutable texture mip under construction.
	* This is only safely written from the game thread protected by the following critical section, and it
	* is safely read from the mutable thread during the update of the instance or texture mips
	*/
	TMap<FName, FUnrealMutableImageInfo> GlobalExternalImages;
	
	/** Access to GlobalExternalImages must be protected with this because it may be accessed concurrently from the 
	Game thread to modify it and from the Mutable thread to read it. */
	FCriticalSection ExternalImagesLock;

#if WITH_EDITOR
public:
	/** The provider may tick to support referenced images. */
	bool Tick();

private:
	struct FReferencedImageRequest
	{
		FReferencedImageRequest(const UE::Tasks::FTaskEvent&& Event) : CompletionEvent(Event) {}

		// Sync
		UE::Tasks::FTaskEvent CompletionEvent;

		// Input
		int32 Id = 0;
		uint8 MipmapsToSkip = 0;
		const void* ModelPtr = nullptr;

		// Result
		mu::Ptr<mu::Image> ResultImage;
	};
	TQueue<FReferencedImageRequest*, EQueueMode::Spsc> QueuedReferencedImageRequests;
#endif

};
