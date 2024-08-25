// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "D3D12ThirdParty.h"
#include "RHI.h"
#include "Templates/Function.h"

struct FD3D12MinimalAdapterDesc
{
	DXGI_ADAPTER_DESC Desc{};
	uint32 NumDeviceNodes{};
};

enum class ED3D12RHIRunOnQueueType
{
	Graphics = 0,
	Copy,
};

struct ID3D12DynamicRHI : public FDynamicRHI
{
	virtual ERHIInterfaceType     GetInterfaceType() const override final { return ERHIInterfaceType::D3D12; }

	virtual TArray<FD3D12MinimalAdapterDesc> RHIGetAdapterDescs() const = 0;
	virtual bool                             RHIIsPixEnabled() const = 0;

	virtual ID3D12CommandQueue*        RHIGetCommandQueue() const = 0;
	virtual ID3D12Device*              RHIGetDevice(uint32 InIndex) const = 0;
	virtual uint32                     RHIGetDeviceNodeMask(uint32 InIndex) const = 0;
	virtual ID3D12GraphicsCommandList* RHIGetGraphicsCommandList(uint32 InDeviceIndex) const = 0;
	virtual DXGI_FORMAT                RHIGetSwapChainFormat(EPixelFormat InFormat) const = 0;

	virtual FTexture2DRHIRef      RHICreateTexture2DFromResource(EPixelFormat Format, ETextureCreateFlags TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D12Resource* Resource) = 0;
	virtual FTexture2DArrayRHIRef RHICreateTexture2DArrayFromResource(EPixelFormat Format, ETextureCreateFlags TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D12Resource* Resource) = 0;
	virtual FTextureCubeRHIRef    RHICreateTextureCubeFromResource(EPixelFormat Format, ETextureCreateFlags TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D12Resource* Resource) = 0;

	virtual ID3D12Resource*       RHIGetResource(FRHIBuffer* InBuffer) const = 0;
	virtual uint32                RHIGetResourceDeviceIndex(FRHIBuffer* InBuffer) const = 0;
	virtual int64                 RHIGetResourceMemorySize(FRHIBuffer* InBuffer) const = 0;
	virtual bool                  RHIIsResourcePlaced(FRHIBuffer* InBuffer) const = 0;
	virtual ID3D12Resource*       RHIGetResource(FRHITexture* InTexture) const = 0;
	virtual uint32                RHIGetResourceDeviceIndex(FRHITexture* InTexture) const = 0;
	virtual int64                 RHIGetResourceMemorySize(FRHITexture* InTexture) const = 0;
	virtual bool                  RHIIsResourcePlaced(FRHITexture* InTexture) const = 0;

	virtual D3D12_CPU_DESCRIPTOR_HANDLE RHIGetRenderTargetView(FRHITexture* InTexture, int32 InMipIndex = 0, int32 InArraySliceIndex = 0) const = 0;

	virtual void                  RHIFinishExternalComputeWork(uint32 InDeviceIndex, ID3D12GraphicsCommandList* InCommandList) = 0;
	virtual void                  RHITransitionResource(FRHICommandList& RHICmdList, FRHITexture* InTexture, D3D12_RESOURCE_STATES InState, uint32 InSubResource) = 0;

	virtual void                  RHISignalManualFence(FRHICommandList& RHICmdList, ID3D12Fence* Fence, uint64 Value) = 0;
	virtual void                  RHIWaitManualFence(FRHICommandList& RHICmdList, ID3D12Fence* Fence, uint64 Value) = 0;

	virtual void                  RHIVerifyResult(ID3D12Device* Device, HRESULT Result, const ANSICHAR* Code, const ANSICHAR* Filename, uint32 Line, FString Message = FString()) const = 0;

	virtual void				  RHIRunOnQueue(ED3D12RHIRunOnQueueType QueueType, TFunction<void(ID3D12CommandQueue*)>&& CodeToRun, bool bWaitForSubmission) = 0;
};

inline bool IsRHID3D12()
{
	return GDynamicRHI != nullptr && GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D12;
}

inline ID3D12DynamicRHI* GetID3D12DynamicRHI()
{
	check(GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D12);
	return GetDynamicRHI<ID3D12DynamicRHI>();
}

#if D3D12RHI_PLATFORM_HAS_CUSTOM_INTERFACE
	#include "ID3D12PlatformDynamicRHI.h"
#else
	using ID3D12PlatformDynamicRHI = ID3D12DynamicRHI;
#endif

inline ID3D12PlatformDynamicRHI* GetID3D12PlatformDynamicRHI()
{
	check(GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D12);
	return GetDynamicRHI<ID3D12PlatformDynamicRHI>();
}
