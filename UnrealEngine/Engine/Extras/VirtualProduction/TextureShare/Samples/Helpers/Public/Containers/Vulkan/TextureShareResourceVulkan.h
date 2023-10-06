// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareResource.h"
#include "Containers/Resource/TextureShareCustomResource.h"
// Not implemented

/**
 * Vulkan texture resource container
 */
struct FTextureShareResourceVulkan
	: public  ITextureShareResource
	, public FTextureShareCustomResource
{
public:
	//Not implemented
	FTextureShareResourceVulkan() = default;

	virtual ~FTextureShareResourceVulkan()
	{
		Release();
	}

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

	void Release()
	{
		// Not implemented
	}

public:
	// Not implemented
};
