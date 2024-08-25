// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/RefCounting.h"

#include "AVContext.h"
#include "Video/VideoResource.h"

#pragma warning(push)
#pragma warning(disable: 4005)

THIRD_PARTY_INCLUDES_START
#include "Windows/AllowWindowsPlatformTypes.h"
#include <d3d11.h>
#include <mfobjects.h>
#include <mftransform.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <codecapi.h>
#include <shlwapi.h>
#include <mfreadwrite.h>
#include <d3d11_1.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include "Windows/HideWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_END

/**
 * D3D11 and D3D12 platform video context and resource.
 */

class AVCODECSCORE_API FVideoContextD3D11 : public FAVContext
{
public:
	TRefCountPtr<ID3D11Device> Device;

	FVideoContextD3D11(TRefCountPtr<ID3D11Device> const& Device);
};

class AVCODECSCORE_API FVideoContextD3D12 : public FAVContext
{
public:
	TRefCountPtr<ID3D12Device> Device;

	FVideoContextD3D12(TRefCountPtr<ID3D12Device> const& Device);
};

class AVCODECSCORE_API FVideoResourceD3D11 : public TVideoResource<FVideoContextD3D11>
{
private:
	TRefCountPtr<ID3D11Texture2D> Raw;
	HANDLE SharedHandle = nullptr;

public:
	static FVideoDescriptor GetDescriptorFrom(TSharedRef<FAVDevice> const& Device, TRefCountPtr<ID3D11Texture2D> const& Raw);

	FORCEINLINE TRefCountPtr<ID3D11Texture2D> const& GetRaw() const { return Raw; }
	FORCEINLINE HANDLE const& GetSharedHandle() const { return SharedHandle; }

	FVideoResourceD3D11(TSharedRef<FAVDevice> const& Device, TRefCountPtr<ID3D11Texture2D> const& Raw, FAVLayout const& Layout);

	virtual FAVResult Validate() const override;
};

class AVCODECSCORE_API FVideoResourceD3D12 : public TVideoResource<FVideoContextD3D12>
{
public:
	struct FRawD3D12
	{
		TRefCountPtr<ID3D12Resource> D3DResource;
		HANDLE D3DResourceShared = nullptr;
		TRefCountPtr<ID3D12Heap> D3DHeap;
		HANDLE D3DHeapShared = nullptr;
		TRefCountPtr<ID3D12Fence> D3DFence;
		HANDLE D3DFenceShared = nullptr;
		uint64 FenceValue;
	};

private:
	FRawD3D12 Raw;

public:
	static FVideoDescriptor GetDescriptorFrom(TSharedRef<FAVDevice> const& Device, FRawD3D12 const& Raw);

	FORCEINLINE TRefCountPtr<ID3D12Resource> const& GetResource() const { return Raw.D3DResource; }
	FORCEINLINE HANDLE const& GetResourceSharedHandle() const { return Raw.D3DResourceShared; }
	
	FORCEINLINE TRefCountPtr<ID3D12Heap> const& GetHeap() const { return Raw.D3DHeap; }
	FORCEINLINE HANDLE const& GetHeapSharedHandle() const { return Raw.D3DHeapShared; }

	FORCEINLINE TRefCountPtr<ID3D12Fence> const& GetFence() const { return Raw.D3DFence; };
	FORCEINLINE HANDLE const& GetFenceSharedHandle() const { return Raw.D3DFenceShared; }
	FORCEINLINE uint64 const& GetFenceValue() const { return Raw.FenceValue; }

	uint64 GetSizeInBytes();

	FVideoResourceD3D12(TSharedRef<FAVDevice> const& Device, FRawD3D12 const& Raw, FAVLayout const& Layout);
	FVideoResourceD3D12(TSharedRef<FAVDevice> const& Device, FRawD3D12 const& Raw, FAVLayout const& Layout, FVideoDescriptor const& Descriptor);
	virtual ~FVideoResourceD3D12() override;

	virtual FAVResult Validate() const override;
};

DECLARE_TYPEID(FVideoContextD3D11, AVCODECSCORE_API);
DECLARE_TYPEID(FVideoContextD3D12, AVCODECSCORE_API);
DECLARE_TYPEID(FVideoResourceD3D11, AVCODECSCORE_API);
DECLARE_TYPEID(FVideoResourceD3D12, AVCODECSCORE_API);