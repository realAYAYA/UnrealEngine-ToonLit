// Copyright Epic Games, Inc. All Rights Reserved.

#include "IMeshPaintComponentAdapter.h"

#include "Engine/Texture.h"
#include "Engine/World.h"
#include "Components/MeshComponent.h"
#include "MeshPaintingToolsetTypes.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter.h"
#include "MaterialShared.h"
#include "TexturePaintToolset.h"

//////////////////////////////////////////////////////////////////////////
// IMeshPaintGeometryAdapter

void IMeshPaintComponentAdapter::DefaultApplyOrRemoveTextureOverride(UMeshComponent* InMeshComponent, UTexture* SourceTexture, UTexture* OverrideTexture)
{
	const ERHIFeatureLevel::Type FeatureLevel = InMeshComponent->GetWorld()->GetFeatureLevel();

	// Check all the materials on the mesh to see if the user texture is there
	int32 MaterialIndex = 0;
	UMaterialInterface* MaterialToCheck = InMeshComponent->GetMaterial(MaterialIndex);
	while (MaterialToCheck != nullptr)
	{
		const bool bIsTextureUsed = DoesMaterialUseTexture(MaterialToCheck, SourceTexture);
		if (bIsTextureUsed)
		{
			MaterialToCheck->OverrideTexture(SourceTexture, OverrideTexture, FeatureLevel);
		}

		++MaterialIndex;
		MaterialToCheck = InMeshComponent->GetMaterial(MaterialIndex);
	}
}

static bool IsTextureSuitableForTexturePainting(const TWeakObjectPtr<UTexture> TexturePtr)
{
	return (TexturePtr.Get() != nullptr &&
		!TexturePtr->IsNormalMap() &&
		!TexturePtr->VirtualTextureStreaming &&
		!TexturePtr->HasHDRSource() && // Currently HDR textures are not supported to paint on.
		TexturePtr->Source.IsValid() &&
		TexturePtr->Source.GetBytesPerPixel() > 0 && // Textures' sources must have a known count of bytes per pixel
		(TexturePtr->Source.GetBytesPerPixel() <= UTexturePaintToolset::GetMaxSupportedBytesPerPixelForPainting())); // Textures' sources must fit in FColor struct to be supported.
}

void IMeshPaintComponentAdapter::DefaultQueryPaintableTextures(int32 MaterialIndex, const UMeshComponent* MeshComponent, int32& OutDefaultIndex, TArray<struct FPaintableTexture>& InOutTextureList)
{
	OutDefaultIndex = INDEX_NONE;

	// We already know the material we are painting on, take it off the static mesh component
	UMaterialInterface* Material = MeshComponent->GetMaterial(MaterialIndex);

	while (Material != nullptr)
	{
		if (Material != NULL)
		{
			FPaintableTexture PaintableTexture;
			// Find all the unique textures used in the top material level of the selected actor materials

			// Only grab the textures from the top level of samples
			for (UMaterialExpression* Expression : Material->GetMaterial()->GetExpressions())
			{
				UMaterialExpressionTextureBase* TextureBase = Cast<UMaterialExpressionTextureBase>(Expression);
				if (TextureBase != NULL &&
					IsTextureSuitableForTexturePainting(TextureBase->Texture))
				{
					// Default UV channel to index 0. 
					PaintableTexture = FPaintableTexture(TextureBase->Texture, 0);

					// Texture Samples can have UV's specified, check the first node for whether it has a custom UV channel set. 
					// We only check the first as the Mesh paint mode does not support painting with UV's modified in the shader.
					UMaterialExpressionTextureSample* TextureSample = Cast<UMaterialExpressionTextureSample>(Expression);
					if (TextureSample != NULL)
					{
						UMaterialExpressionTextureCoordinate* TextureCoords = Cast<UMaterialExpressionTextureCoordinate>(TextureSample->Coordinates.Expression);
						if (TextureCoords != NULL)
						{
							// Store the uv channel, this is set when the texture is selected. 
							PaintableTexture.UVChannelIndex = TextureCoords->CoordinateIndex;
						}

						// Handle texture parameter expressions
						UMaterialExpressionTextureSampleParameter* TextureSampleParameter = Cast<UMaterialExpressionTextureSampleParameter>(TextureSample);
						if (TextureSampleParameter != NULL)
						{
							// Grab the overridden texture if it exists.  
							Material->GetTextureParameterValue(TextureSampleParameter->ParameterName, PaintableTexture.Texture);
						}
					}

					// note that the same texture will be added again if its UV channel differs. 
					int32 TextureIndex = InOutTextureList.AddUnique(PaintableTexture);

					// cache the first default index, if there is no previous info this will be used as the selected texture
					if ((OutDefaultIndex == INDEX_NONE) && TextureBase->IsDefaultMeshpaintTexture)
					{
						OutDefaultIndex = TextureIndex;
					}
				}
			}
		}
		// Make sure to include all texture parameters, this will include all of the texture parameters from internal material functions
		TMap<FMaterialParameterInfo, FMaterialParameterMetadata> ParameterValues;
		Material->GetAllParametersOfType(EMaterialParameterType::Texture, ParameterValues);

		for (auto& ParameterElem : ParameterValues)
		{
			const TWeakObjectPtr<UTexture> TexturePtr = ParameterElem.Value.Value.Texture;

			if (IsTextureSuitableForTexturePainting(TexturePtr))
			{
				FPaintableTexture PaintableTexture;

				// Default UV channel to index 0.
				PaintableTexture = FPaintableTexture(TexturePtr.Get(), 0);
				InOutTextureList.AddUnique(PaintableTexture);
			}
		}

		UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(Material);
		if (MaterialInstance)
		{
			Material = MaterialInstance->Parent ? Cast<UMaterialInstance>(MaterialInstance->Parent) : nullptr;
		}
		else
		{
			// This prevents an infinite loop when `Material` isn't a material instance.
			break;
		}
	}
}
