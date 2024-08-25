// Copyright Epic Games, Inc. All Rights Reserved.

#include "VT/VirtualTextureRecreate.h"

#include "Components/RuntimeVirtualTextureComponent.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "VT/RuntimeVirtualTexture.h"
#include "UObject/UObjectIterator.h"

namespace VirtualTexture
{
	// Release all VT render resources here.
	// Assuming all virtual textures are released, then virtual texture pools will reach a zero ref count and release, which is needed for any pool size scale to be effective.
	// Note that for pool size scale changes, there will be a transition period (with high memory watermark) where new pools are created before old pools are released.
	void Recreate()
	{
		// Reinit streaming virtual textures.
		for (TObjectIterator<UTexture2D> It(RF_ClassDefaultObject, true, EInternalObjectFlags::Garbage); It; ++It)
		{
			if (It->IsCurrentlyVirtualTextured())
			{
				if (FVirtualTexture2DResource* Resource = static_cast<FVirtualTexture2DResource*>(It->GetResource()))
				{
					BeginUpdateResourceRHI(Resource);
				}
			}
		}

		// Reinit runtime virtual textures.
		for (TObjectIterator<URuntimeVirtualTextureComponent> It(RF_ClassDefaultObject, true, EInternalObjectFlags::Garbage); It; ++It)
		{
			It->MarkRenderStateDirty();
		}
	}

	void Recreate(TConstArrayView < TEnumAsByte<EPixelFormat> > InFormat)
	{
		// Reinit streaming virtual textures that match one of our passed in format arrays.
		for (TObjectIterator<UTexture2D> It(RF_ClassDefaultObject, true, EInternalObjectFlags::Garbage); It; ++It)
		{
			if (It->IsCurrentlyVirtualTextured())
			{
				if (FVirtualTexture2DResource* Resource = static_cast<FVirtualTexture2DResource*>(It->GetResource()))
				{
					bool bIsMatchingFormat = Resource->GetNumLayers() == InFormat.Num();
					for (int32 LayerIndex = 0; bIsMatchingFormat && LayerIndex < InFormat.Num(); LayerIndex++)
					{
						if (Resource->GetFormat(LayerIndex) != InFormat[LayerIndex])
						{
							bIsMatchingFormat = false;
							break;
						}
					}

					if (bIsMatchingFormat)
					{
						BeginUpdateResourceRHI(Resource);
					}
				}
			}
		}

		// Reinit runtime virtual textures that match one of our passed in format arrays.
		for (TObjectIterator<URuntimeVirtualTextureComponent> It(RF_ClassDefaultObject, true, EInternalObjectFlags::Garbage); It; ++It)
		{
			if (URuntimeVirtualTexture* VirtualTexture = It->GetVirtualTexture())
			{
				bool bIsMatchingFormat = VirtualTexture->GetLayerCount() == InFormat.Num();
				for (int32 LayerIndex = 0; bIsMatchingFormat && LayerIndex < InFormat.Num(); LayerIndex++)
				{
					if (VirtualTexture->GetLayerFormat(LayerIndex) != InFormat[LayerIndex])
					{
						bIsMatchingFormat = false;
						break;
					}
				}

				if (bIsMatchingFormat)
				{
					It->MarkRenderStateDirty();
				}
			}
		}
	}
}
