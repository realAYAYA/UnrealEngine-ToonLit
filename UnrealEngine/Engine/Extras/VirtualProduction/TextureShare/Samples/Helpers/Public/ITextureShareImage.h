// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/TextureShareSDKContainers.h"

/**
 * Abstract Texture Container
 */
struct ITextureShareImage
{
	virtual ~ITextureShareImage() = default;

	virtual ETextureShareDeviceType GetDeviceType() const = 0;

	virtual bool IsValid() const = 0;

	// return pointer to class implementation
	virtual const void* Ptr() const = 0;
	virtual void* Ptr() = 0;
};
