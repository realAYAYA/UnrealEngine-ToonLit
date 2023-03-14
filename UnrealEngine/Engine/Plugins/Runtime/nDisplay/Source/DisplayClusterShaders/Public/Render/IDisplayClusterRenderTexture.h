// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "RHI.h"
#include "RHIResources.h"
#include "RenderResource.h"

// Runtime texture resoure
class IDisplayClusterRenderTexture
{
public:
	virtual ~IDisplayClusterRenderTexture() = default;

public:
	/**
	* Create texture from memory data:
	*
	* @param InPixelFormat          - texture pixel format
	* @param InWidth                - Texture width
	* @param InHeight               - Texture height
	* @param InTextureData          - Texture input data
	* @param bInHasCPUAccess        - has CPU access to texture data, dont release from memory
	*
	* @return - true if the mesh linked and ready to warp
	*/
	virtual void CreateTexture(EPixelFormat InPixelFormat, uint32_t InWidth, uint32_t InHeight, const void* InTextureData, bool bInHasCPUAccess = false) = 0;
	virtual void* GetData() const = 0;
	virtual uint32_t GetWidth() const = 0;
	virtual uint32_t GetHeight() const = 0;
	virtual uint32_t GetTotalPoints() const = 0;
	virtual EPixelFormat GetPixelFormat() const = 0;
	virtual bool IsEnabled() const = 0;

	virtual FRHITexture* GetRHITexture() const = 0;
};
