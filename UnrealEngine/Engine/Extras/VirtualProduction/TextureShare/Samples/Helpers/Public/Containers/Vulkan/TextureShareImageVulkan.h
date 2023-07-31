// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareImage.h"
// Not implemented

/**
 * Vulkan texture ref container
 */
struct FTextureShareImageVulkan
	: public ITextureShareImage
{
public:
	FTextureShareImageVulkan(VkImage InTexture /*, ...*/)
		: Texture(InTexture)
	{ }

	virtual ~FTextureShareImageVulkan() = default;

	virtual ETextureShareDeviceType GetDeviceType() const override
	{
		return ETextureShareDeviceType::Vulkan;
	}

	virtual bool IsValid() const override
	{
		return Texture != nullptr;
	}

	virtual const void* Ptr() const override
	{
		return this;
	}

	virtual void* Ptr() override
	{
		return this;
	}

public:
	// Not implemented
	

	// Vulkan texture
	VkImage Texture = nullptr;

	// The current state of the texture barrier. Recovering after send\receive
	//srcAccessMask
	// ...
};
