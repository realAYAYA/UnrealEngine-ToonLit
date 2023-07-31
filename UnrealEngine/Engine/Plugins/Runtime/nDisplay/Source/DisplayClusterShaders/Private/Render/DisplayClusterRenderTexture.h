// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/IDisplayClusterRenderTexture.h"

#include "TextureResource.h"

class FDisplayClusterRenderTextureResource
	: public FTextureResource
{
public:
	FDisplayClusterRenderTextureResource(EPixelFormat InPixelFormat, uint32_t InWidth, uint32_t InHeight, const void* InTextureData, bool bInHasCPUAccess);
	virtual ~FDisplayClusterRenderTextureResource();

	void ReleaseTextureData();

	virtual void InitRHI() override;

	void* GetTextureData() const
	{
		return TextureData;
	}

	uint32_t GetWidth() const
	{
		return Width;
	}

	uint32_t GetHeight() const
	{
		return Height;
	}

	EPixelFormat GetPixelFormat() const
	{
		return PixelFormat;
	}

private:
	/** RAW data of the texture. Mips not supported now */
	void* TextureData;
	/** Width of the texture. */
	uint32_t Width;
	/** Height of the texture. */
	uint32_t Height;
	/** Format in which mip data is stored. */
	EPixelFormat PixelFormat;
	// has CPU access to texture data, dont release from memory
	bool bHasCPUAccess;
};

class FDisplayClusterRenderTexture
	: public IDisplayClusterRenderTexture
{
public:
	FDisplayClusterRenderTexture();
	virtual ~FDisplayClusterRenderTexture();

public:
	virtual void CreateTexture(EPixelFormat InPixelFormat, uint32_t InWidth, uint32_t InHeight, const void* InTextureData, bool bInHasCPUAccess = false) override;

	virtual void* GetData() const override
	{
		const FDisplayClusterRenderTextureResource* TextureResource = GetResource();
		return TextureResource ? TextureResource->GetTextureData() : nullptr;
	}

	virtual uint32_t GetWidth() const override
	{
		const FDisplayClusterRenderTextureResource* TextureResource = GetResource();
		return TextureResource ? TextureResource->GetWidth() : 0;
	}

	virtual uint32_t GetHeight() const override
	{
		const FDisplayClusterRenderTextureResource* TextureResource = GetResource();
		return TextureResource ? TextureResource->GetHeight() : 0;
	}

	virtual uint32_t GetTotalPoints() const override
	{
		const FDisplayClusterRenderTextureResource* TextureResource = GetResource();
		return TextureResource ? (TextureResource->GetWidth() * TextureResource->GetHeight()) : 0;
	}

	virtual EPixelFormat GetPixelFormat() const override
	{
		const FDisplayClusterRenderTextureResource* TextureResource = GetResource();
		return TextureResource ? TextureResource->GetPixelFormat() : PF_Unknown;
	}

	virtual bool IsEnabled() const override
	{
		const FDisplayClusterRenderTextureResource* TextureResource = GetResource();
		if (TextureResource)
		{
			return TextureResource->GetWidth() > 0 && TextureResource->GetHeight() > 0;
		}

		return false;
	}

	virtual FRHITexture* GetRHITexture() const override
	{
		const FDisplayClusterRenderTextureResource* TextureResource = GetResource();
		return TextureResource ? TextureResource->TextureRHI : nullptr;
	}

protected:
	/** Set texture's resource, can be NULL */
	void SetResource(FDisplayClusterRenderTextureResource* Resource);

	/** Get the texture's resource, can be NULL */
	FDisplayClusterRenderTextureResource* GetResource();

	/** Get the const texture's resource, can be NULL */
	const FDisplayClusterRenderTextureResource* GetResource() const;

	/**
	 * Resets the resource for the texture.
	 */
	void ReleaseResource();

private:
	/** The texture's resource, can be NULL */
	class FDisplayClusterRenderTextureResource* PrivateResource;

	/** Value updated and returned by the render-thread to allow
	  * fenceless update from the game-thread without causing
	  * potential crash in the render thread.
	  */
	class FDisplayClusterRenderTextureResource* PrivateResourceRenderThread;
};
