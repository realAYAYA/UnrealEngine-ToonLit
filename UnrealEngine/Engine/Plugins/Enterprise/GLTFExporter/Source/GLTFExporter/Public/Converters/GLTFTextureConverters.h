// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Json/GLTFJsonCore.h"
#include "Converters/GLTFConverter.h"
#include "Converters/GLTFBuilderContext.h"
#include "Engine/LightMapTexture2D.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTargetCube.h"
#include "RHIDefinitions.h"

// TODO: generalize parameter bToSRGB to SamplerType (including normalmap unpacking)

// TODO: remove lightmap-specific converter (since it won't work in runtime)

typedef TGLTFConverter<FGLTFJsonTexture*, const UTexture2D*, bool> IGLTFTexture2DConverter;
typedef TGLTFConverter<FGLTFJsonTexture*, const UTextureCube*, ECubeFace, bool> IGLTFTextureCubeConverter;
typedef TGLTFConverter<FGLTFJsonTexture*, const UTextureRenderTarget2D*, bool> IGLTFTextureRenderTarget2DConverter;
typedef TGLTFConverter<FGLTFJsonTexture*, const UTextureRenderTargetCube*, ECubeFace, bool> IGLTFTextureRenderTargetCubeConverter;
typedef TGLTFConverter<FGLTFJsonTexture*, const ULightMapTexture2D*> IGLTFTextureLightMapConverter;

class GLTFEXPORTER_API FGLTFTexture2DConverter : public FGLTFBuilderContext, public IGLTFTexture2DConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual void Sanitize(const UTexture2D*& Texture2D, bool& bToSRGB) override;

	virtual FGLTFJsonTexture* Convert(const UTexture2D* Texture2D, bool bToSRGB) override;
};

class GLTFEXPORTER_API FGLTFTextureCubeConverter : public FGLTFBuilderContext, public IGLTFTextureCubeConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual void Sanitize(const UTextureCube*& TextureCube, ECubeFace& CubeFace, bool& bToSRGB) override;

	virtual FGLTFJsonTexture* Convert(const UTextureCube* TextureCube, ECubeFace CubeFace, bool bToSRGB) override;
};

class GLTFEXPORTER_API FGLTFTextureRenderTarget2DConverter : public FGLTFBuilderContext, public IGLTFTextureRenderTarget2DConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual void Sanitize(const UTextureRenderTarget2D*& RenderTarget2D, bool& bToSRGB) override;

	virtual FGLTFJsonTexture* Convert(const UTextureRenderTarget2D* RenderTarget2D, bool bToSRGB) override;
};

class GLTFEXPORTER_API FGLTFTextureRenderTargetCubeConverter : public FGLTFBuilderContext, public IGLTFTextureRenderTargetCubeConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual void Sanitize(const UTextureRenderTargetCube*& RenderTargetCube, ECubeFace& CubeFace, bool& bToSRGB) override;

	virtual FGLTFJsonTexture* Convert(const UTextureRenderTargetCube* RenderTargetCube, ECubeFace CubeFace, bool bToSRGB) override;
};

class GLTFEXPORTER_API FGLTFTextureLightMapConverter : public FGLTFBuilderContext, public IGLTFTextureLightMapConverter
{
public:

	using FGLTFBuilderContext::FGLTFBuilderContext;

protected:

	virtual FGLTFJsonTexture* Convert(const ULightMapTexture2D* LightMap) override;
};
