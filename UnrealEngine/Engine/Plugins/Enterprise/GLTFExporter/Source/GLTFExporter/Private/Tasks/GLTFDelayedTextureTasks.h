// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tasks/GLTFDelayedTask.h"
#include "Builders/GLTFConvertBuilder.h"

class FGLTFDelayedTexture2DTask : public FGLTFDelayedTask
{
public:

	FGLTFDelayedTexture2DTask(FGLTFConvertBuilder& Builder, const UTexture2D* Texture2D, bool bToSRGB, FGLTFJsonTexture* JsonTexture)
		: FGLTFDelayedTask(EGLTFTaskPriority::Texture)
		, Builder(Builder)
		, Texture2D(Texture2D)
		, bToSRGB(bToSRGB)
		, JsonTexture(JsonTexture)
	{
	}

	virtual FString GetName() override;

	virtual void Process() override;

private:

	FGLTFConvertBuilder& Builder;
	const UTexture2D* Texture2D;
	bool bToSRGB;
	FGLTFJsonTexture* JsonTexture;
};

class FGLTFDelayedTextureCubeTask : public FGLTFDelayedTask
{
public:

	FGLTFDelayedTextureCubeTask(FGLTFConvertBuilder& Builder, const UTextureCube* TextureCube, ECubeFace CubeFace, bool bToSRGB, FGLTFJsonTexture* JsonTexture)
		: FGLTFDelayedTask(EGLTFTaskPriority::Texture)
		, Builder(Builder)
		, TextureCube(TextureCube)
		, CubeFace(CubeFace)
		, bToSRGB(bToSRGB)
		, JsonTexture(JsonTexture)
	{
	}

	virtual FString GetName() override;

	virtual void Process() override;

private:

	FGLTFConvertBuilder& Builder;
	const UTextureCube* TextureCube;
	ECubeFace CubeFace;
	bool bToSRGB;
	FGLTFJsonTexture* JsonTexture;
};

class FGLTFDelayedTextureRenderTarget2DTask : public FGLTFDelayedTask
{
public:

	FGLTFDelayedTextureRenderTarget2DTask(FGLTFConvertBuilder& Builder, const UTextureRenderTarget2D* RenderTarget2D, bool bToSRGB, FGLTFJsonTexture* JsonTexture)
		: FGLTFDelayedTask(EGLTFTaskPriority::Texture)
		, Builder(Builder)
		, RenderTarget2D(RenderTarget2D)
		, bToSRGB(bToSRGB)
		, JsonTexture(JsonTexture)
	{
	}

	virtual FString GetName() override;

	virtual void Process() override;

private:

	FGLTFConvertBuilder& Builder;
	const UTextureRenderTarget2D* RenderTarget2D;
	bool bToSRGB;
	FGLTFJsonTexture* JsonTexture;
};

class FGLTFDelayedTextureRenderTargetCubeTask : public FGLTFDelayedTask
{
public:

	FGLTFDelayedTextureRenderTargetCubeTask(FGLTFConvertBuilder& Builder, const UTextureRenderTargetCube* RenderTargetCube, ECubeFace CubeFace, bool bToSRGB, FGLTFJsonTexture* JsonTexture)
		: FGLTFDelayedTask(EGLTFTaskPriority::Texture)
		, Builder(Builder)
		, RenderTargetCube(RenderTargetCube)
		, CubeFace(CubeFace)
		, bToSRGB(bToSRGB)
		, JsonTexture(JsonTexture)
	{
	}

	virtual FString GetName() override;

	virtual void Process() override;

private:

	FGLTFConvertBuilder& Builder;
	const UTextureRenderTargetCube* RenderTargetCube;
	ECubeFace CubeFace;
	bool bToSRGB;
	FGLTFJsonTexture* JsonTexture;
};

#if WITH_EDITOR

class FGLTFDelayedTextureLightMapTask : public FGLTFDelayedTask
{
public:

	FGLTFDelayedTextureLightMapTask(FGLTFConvertBuilder& Builder, const ULightMapTexture2D* LightMap, FGLTFJsonTexture* JsonTexture)
		: FGLTFDelayedTask(EGLTFTaskPriority::Texture)
		, Builder(Builder)
		, LightMap(LightMap)
		, JsonTexture(JsonTexture)
	{
	}

	virtual FString GetName() override;

	virtual void Process() override;

private:

	FGLTFConvertBuilder& Builder;
	const ULightMapTexture2D* LightMap;
	FGLTFJsonTexture* JsonTexture;
};

#endif
