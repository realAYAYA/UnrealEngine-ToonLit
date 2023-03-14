// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/Platform.h"
#include "MuR/Image.h"
#include "MuR/System.h"
#include "MuR/Types.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "UObject/GCObject.h"


/** Implementation of a mutable core provider for image parameters that are application-specific. */
class FUnrealMutableImageProvider : public mu::ImageParameterGenerator, public FGCObject
{
public:

	// mu::ImageParameterGenerator interface
	// Thread: Mutable
	mu::ImagePtr GetImage(mu::EXTERNAL_IMAGE_ID id) override;

	// Own interface
	// Thread: Game
	void CacheImage(mu::EXTERNAL_IMAGE_ID id);
	void UnCacheImage(mu::EXTERNAL_IMAGE_ID id);
	void CacheAllImagesInAllProviders(bool bClearPreviousCacheImages);
	void ClearCache();

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
		FUnrealMutableImageInfo(mu::ImagePtr InImage, class UTexture2D* InTextureToLoad) : Image(InImage), TextureToLoad(InTextureToLoad) { }

		mu::ImagePtr Image;

		/** If the above Image has not been loaded in the game thread, the TextureToLoad bulk data will be loaded
		* from the Mutable thread when it's needed
		*/
		UTexture2D* TextureToLoad = nullptr;
	};

	/** This will be called if an image Id has been requested by Mutable core but it has not been provided by any provider. */
	mu::ImagePtr CreateDummy();

	/** Map of Ids to external textures that may be required for any instance or Mutable texture mip under construction.
	* This is only safely written from the game thread protected by the following critical section, and it
	* is safely read from the mutable thread during the update of the instance or texture mips
	*/
	TMap<uint64, FUnrealMutableImageInfo> GlobalExternalImages;
	
	/** Access to GlobalExternalImages must be protected with this because it may be accessed concurrently from the 
	Game thread to modify it and from the Mutable thread to read it. */
	FCriticalSection ExternalImagesLock;

};
