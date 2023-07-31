// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryScript/TextureMapFunctions.h"

#include "Async/ParallelFor.h"
#include "AssetUtils/Texture2DUtil.h"
#include "Spatial/SampledScalarField2.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(TextureMapFunctions)

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "UGeometryScriptLibrary_TextureMapFunctions"

void UGeometryScriptLibrary_TextureMapFunctions::SampleTexture2DAtUVPositions(
	FGeometryScriptUVList UVList,
	UTexture2D* TextureAsset,
	FGeometryScriptSampleTextureOptions SampleOptions,
	FGeometryScriptColorList& ColorList,
	UGeometryScriptDebug* Debug)
{
	ColorList.Reset();
	if (UVList.List.IsValid() == false || UVList.List->Num() == 0)
	{
		return;
	}
	
	if (TextureAsset == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("SampleTexture2DAtUVPositions_InvalidInput2", "SampleTexture2DAtUVPositions: Texture is Null"));
		return;
	}

	TImageBuilder<FVector4f> ImageData;
	if (UE::AssetUtils::ReadTexture(TextureAsset, ImageData, false) == false)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::OperationFailed, LOCTEXT("SampleTexture2DAtUVPositions_TexReadFailed", "SampleTexture2DAtUVPositions: Error reading source texture data. If using this function at Runtime, The Compression Settings type on the UTexture2D Asset must be set to VectorDisplacementmap (RGBA8)."));
		return;
	}

	const TArray<FVector2D>& UVs = *UVList.List;
	int32 NumUVs = UVs.Num();

	TArray<FLinearColor>& Colors = *ColorList.List;
	Colors.SetNumUninitialized(NumUVs);

	for (int32 k = 0; k < NumUVs; ++k)
	{
		FVector2d UV = UVs[k];

		// Adjust UV value. 
		UV = UV * SampleOptions.UVScale + SampleOptions.UVOffset;

		FVector4f InterpValue;
		if (SampleOptions.bWrap)
		{
			constexpr EImageTilingMethod TilingMethod = EImageTilingMethod::Wrap;
			InterpValue = (SampleOptions.SamplingMethod == EGeometryScriptPixelSamplingMethod::Bilinear) ?
				ImageData.BilinearSampleUV<double, TilingMethod>(UV, FVector4f::Zero()) : ImageData.NearestSampleUV<TilingMethod>(UV);
		}
		else
		{
			constexpr EImageTilingMethod TilingMethod = EImageTilingMethod::Clamp;
			InterpValue = (SampleOptions.SamplingMethod == EGeometryScriptPixelSamplingMethod::Bilinear) ?
				ImageData.BilinearSampleUV<double, TilingMethod>(UV, FVector4f::Zero()) : ImageData.NearestSampleUV<TilingMethod>(UV);
		}

		Colors[k] = (FLinearColor)InterpValue;
	}


}


#undef LOCTEXT_NAMESPACE
