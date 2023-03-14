// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ITextureShareDeviceContext.h"
#include <d3d12.h>

/**
 * D3D12 device context
 */
struct FTextureShareDeviceContextD3D12
	: public ITextureShareDeviceContext
{
	FTextureShareDeviceContextD3D12(ID3D12Device* const InpD3D12Device, ID3D12GraphicsCommandList* const InpCmdList, ID3D12DescriptorHeap* const InpD3D12HeapSRV)
		: pD3D12Device(InpD3D12Device), pCmdList(InpCmdList), pD3D12HeapSRV(InpD3D12HeapSRV)
	{ }

	virtual ~FTextureShareDeviceContextD3D12() = default;

	virtual ETextureShareDeviceType GetDeviceType() const override
	{
		return ETextureShareDeviceType::D3D12;
	}

	virtual bool IsValid() const override
	{
		return pD3D12Device && pCmdList && pD3D12HeapSRV;
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
	ID3D12Device* const pD3D12Device;
	ID3D12GraphicsCommandList* const pCmdList;
	ID3D12DescriptorHeap* const pD3D12HeapSRV;
};
