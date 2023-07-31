// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFSamplerConverters.h"
#include "Builders/GLTFContainerBuilder.h"
#include "Utilities/GLTFCoreUtilities.h"
#include "Converters/GLTFTextureUtility.h"

FGLTFJsonSampler* FGLTFSamplerConverter::Convert(const UTexture* Texture)
{
	// TODO: reuse existing samplers, don't create a unique sampler per texture

	FGLTFJsonSampler* JsonSampler = Builder.AddSampler();
	Texture->GetName(JsonSampler->Name);

	if (Texture->IsA<ULightMapTexture2D>())
	{
		// Override default filter settings for LightMap textures (which otherwise is "nearest")
		JsonSampler->MinFilter = EGLTFJsonTextureFilter::LinearMipmapLinear;
		JsonSampler->MagFilter = EGLTFJsonTextureFilter::Linear;
	}
	else
	{
		JsonSampler->MinFilter = FGLTFCoreUtilities::ConvertMinFilter(Texture->Filter, Texture->LODGroup);
		JsonSampler->MagFilter = FGLTFCoreUtilities::ConvertMagFilter(Texture->Filter, Texture->LODGroup);
	}

	if (FGLTFTextureUtility::IsCubemap(Texture))
	{
		JsonSampler->WrapS = EGLTFJsonTextureWrap::ClampToEdge;
		JsonSampler->WrapT = EGLTFJsonTextureWrap::ClampToEdge;
	}
	else
	{
		const TTuple<TextureAddress, TextureAddress> AddressXY = FGLTFTextureUtility::GetAddressXY(Texture);
		// TODO: report error if AddressX == TA_MAX or AddressY == TA_MAX

		JsonSampler->WrapS = FGLTFCoreUtilities::ConvertWrap(AddressXY.Get<0>());
		JsonSampler->WrapT = FGLTFCoreUtilities::ConvertWrap(AddressXY.Get<1>());
	}

	return JsonSampler;
}
