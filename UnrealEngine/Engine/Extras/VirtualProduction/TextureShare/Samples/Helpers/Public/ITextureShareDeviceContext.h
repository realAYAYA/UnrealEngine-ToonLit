// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/TextureShareSDKContainers.h"

/**
 * Abstract Render Device Context Container
 */
struct ITextureShareDeviceContext
{
	virtual ~ITextureShareDeviceContext() = default;

	virtual ETextureShareDeviceType GetDeviceType() const = 0;

	virtual bool IsValid() const = 0;

	// return pointer to class implementation
	virtual const void* Ptr() const = 0;
	virtual void* Ptr() = 0;
};
