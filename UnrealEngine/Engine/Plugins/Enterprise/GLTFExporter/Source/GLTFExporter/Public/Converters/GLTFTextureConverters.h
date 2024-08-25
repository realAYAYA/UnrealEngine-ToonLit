// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"
#include "Converters/GLTFConverter.h"
#include "Converters/GLTFBuilderContext.h"

#include "Engine/TextureDefines.h"

class UTexture2D;
class UTextureRenderTarget2D;

// TODO: generalize parameter bToSRGB to SamplerType (including normalmap unpacking)

typedef TGLTFConverter<FGLTFJsonTexture*, const UTexture2D*, bool, TextureAddress, TextureAddress> IGLTFTexture2DConverter;
typedef TGLTFConverter<FGLTFJsonTexture*, const UTextureRenderTarget2D*, bool> IGLTFTextureRenderTarget2DConverter;

class GLTFEXPORTER_API FGLTFTexture2DConverter : public FGLTFBuilderContext, public IGLTFTexture2DConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonTexture* Convert(const UTexture2D* Texture2D, bool bToSRGB, TextureAddress WrapS, TextureAddress WrapT) override;
};

class GLTFEXPORTER_API FGLTFTextureRenderTarget2DConverter : public FGLTFBuilderContext, public IGLTFTextureRenderTarget2DConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonTexture* Convert(const UTextureRenderTarget2D* RenderTarget2D, bool bToSRGB) override;
};
