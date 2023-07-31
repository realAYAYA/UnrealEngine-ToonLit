// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareDeviceContext.h"
#include <d3d11.h>
#include <d3d11_1.h>

/**
 * D3D11 device context
 */
struct FTextureShareDeviceContextD3D11
	: public ITextureShareDeviceContext
{
	FTextureShareDeviceContextD3D11(ID3D11Device* const InD3D11Device, ID3D11DeviceContext* const InD3D11DeviceContext)
		: D3D11Device(InD3D11Device)
		, D3D11DeviceContext(InD3D11DeviceContext)
	{ }

	virtual ~FTextureShareDeviceContextD3D11() = default;

	virtual ETextureShareDeviceType GetDeviceType() const override
	{
		return ETextureShareDeviceType::D3D11;
	}

	virtual bool IsValid() const override
	{
		return D3D11Device && D3D11DeviceContext;
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
	ID3D11Device*        const D3D11Device = nullptr;
	ID3D11DeviceContext* const D3D11DeviceContext = nullptr;
};
