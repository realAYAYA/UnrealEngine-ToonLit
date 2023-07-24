// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/CustomizableObjectSystem.h"

#include "DefaultImageProvider.generated.h"

class UTexture2D;
class UCustomizableObjectInstance;


/** Simple image provider that translates UTexture2D to Mutable IDs.
 *
 * Allows the reuse of UTexture2D.
 * IDs will be garbage collected if they are referenced inside an Instance (GetTextureParameters()) or are not mark explicitly to keep (Keep(...)).
 */
UCLASS()
class CUSTOMIZABLEOBJECT_API UDefaultImageProvider : public UCustomizableSystemImageProvider
{
	GENERATED_BODY()

public:
	// UCustomizableSystemImageProvider interface
	virtual ValueType HasTextureParameterValue(int64 ID) override;
	virtual UTexture2D* GetTextureParameterValue(int64 ID) override;
	virtual void GetTextureParameterValues(TArray<FCustomizableObjectExternalTexture>& OutValues) override;

	// Own interface

	UTexture2D* Get(uint64 TextureId) const;

	int64 Get(UTexture2D* Texture) const;

	/** If the TextureId does not get assigned to an Instance Texture Parameter it will be GC on the next Instance update. */
	uint64 GetOrAdd(UTexture2D* Texture); 

	void CacheTextures(UCustomizableObjectInstance& Instance);

	/** Keep the TextureId from being collected by the GC. */
	void Keep(uint64 TextureId, bool bKeep);
	
	void GarbageCollectTextureIds();

private:
	int32 ToIndex(uint64 TextureId) const;

	uint64 ToTextureId(int32 TextureIndex) const;
	
	/** Change it to avoid TextureId collisions with other Image Providers. */
	const int32 BASE_ID = 1;
	const int32 MAX_IDS = 1000;

	UPROPERTY()
	TArray<TObjectPtr<UTexture2D>> Textures;

	UPROPERTY()
	TArray<bool> KeepTextures;
};

