// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareDeviceContext.h"
// Not implemented

/**
 * Vulkan device context container
 */
struct FTextureShareDeviceContextVulkan
	: public ITextureShareDeviceContext
{
	// Not implemented
	FTextureShareDeviceContextVulkan()
	{ }

	virtual ~FTextureShareDeviceContextVulkan() = default;

	virtual ETextureShareDeviceType GetDeviceType() const override
	{
		return ETextureShareDeviceType::Vulkan;
	}

	virtual bool IsValid() const override
	{
		return false;
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
};
