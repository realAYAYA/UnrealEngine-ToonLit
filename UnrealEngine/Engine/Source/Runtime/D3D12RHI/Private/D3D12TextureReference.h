// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "D3D12RHIPrivate.h"
#include "RHITextureReference.h"

class FD3D12RHITextureReference : public FD3D12DeviceChild, public FRHITextureReference, public FD3D12LinkedAdapterObject<FD3D12RHITextureReference>
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	, public FD3D12ShaderResourceRenameListener
#endif
{
public:
	FD3D12RHITextureReference() = delete;
	FD3D12RHITextureReference(FD3D12Device* InDevice, FD3D12Texture* InReferencedTexture);
	~FD3D12RHITextureReference();

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	// FD3D12ShaderResourceRenameListener
	virtual void ResourceRenamed(FRHICommandListBase& RHICmdList, FD3D12BaseShaderResource* InRenamedResource, FD3D12ResourceLocation* InNewResourceLocation) final override;
#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING

	void SwitchToNewTexture(FRHICommandListBase& RHICmdList, FD3D12Texture* InNewTexture);
};

template<>
struct TD3D12ResourceTraits<FRHITextureReference>
{
	using TConcreteType = FD3D12RHITextureReference;
};
