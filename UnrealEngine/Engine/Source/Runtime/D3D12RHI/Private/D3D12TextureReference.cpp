// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12TextureReference.h"

FD3D12RHITextureReference::FD3D12RHITextureReference(FD3D12Device* InDevice, FD3D12Texture* InReferencedTexture)
	: FD3D12DeviceChild(InDevice)
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	, FRHITextureReference(InReferencedTexture, InDevice->GetBindlessDescriptorManager().AllocateResourceHandle())
#else
	, FRHITextureReference(InReferencedTexture)
#endif
{
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	if (BindlessHandle.IsValid())
	{
		InReferencedTexture->AddRenameListener(this);

		InDevice->GetBindlessDescriptorManager().UpdateDescriptorImmediately(BindlessHandle, InReferencedTexture->GetShaderResourceView());
	}
#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING
}

FD3D12RHITextureReference::~FD3D12RHITextureReference()
{
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	if (BindlessHandle.IsValid())
	{
		FD3D12DynamicRHI::ResourceCast(GetReferencedTexture())->RemoveRenameListener(this);

		GetParentDevice()->GetBindlessDescriptorManager().DeferredFreeFromDestructor(BindlessHandle);
	}
#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING
}

void FD3D12RHITextureReference::SwitchToNewTexture(FRHICommandListBase& RHICmdList, FD3D12Texture* InNewTexture)
{
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
	if (BindlessHandle.IsValid())
	{
		FD3D12Texture* NewTexture = InNewTexture ? InNewTexture : FD3D12DynamicRHI::ResourceCast(FRHITextureReference::GetDefaultTexture());

		FD3D12Texture* CurrentTexture = FD3D12DynamicRHI::ResourceCast(GetReferencedTexture());

		if (CurrentTexture != NewTexture)
		{
			if (CurrentTexture)
			{
				CurrentTexture->RemoveRenameListener(this);
			}

			NewTexture->AddRenameListener(this);

			GetParentDevice()->GetBindlessDescriptorManager().UpdateDescriptor(RHICmdList, BindlessHandle, NewTexture->GetShaderResourceView());
		}
	}
#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING

	SetReferencedTexture(InNewTexture);
}

#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
void FD3D12RHITextureReference::ResourceRenamed(FRHICommandListBase& RHICmdList, FD3D12BaseShaderResource* InRenamedResource, FD3D12ResourceLocation* InNewResourceLocation)
{
	if (ensure(BindlessHandle.IsValid()))
	{
		FD3D12Texture* RenamedTexture = static_cast<FD3D12Texture*>(InRenamedResource);
		checkSlow(RenamedTexture == ReferencedTexture);

		GetParentDevice()->GetBindlessDescriptorManager().UpdateDescriptor(RHICmdList, BindlessHandle, RenamedTexture->GetShaderResourceView());
	}
}
#endif // PLATFORM_SUPPORTS_BINDLESS_RENDERING

FTextureReferenceRHIRef FD3D12DynamicRHI::RHICreateTextureReference(FRHICommandListBase& RHICmdList, FRHITexture* InReferencedTexture)
{
	FRHITexture* ReferencedTexture = InReferencedTexture ? InReferencedTexture : FRHITextureReference::GetDefaultTexture();

	FD3D12Adapter* Adapter = &GetAdapter();
	return Adapter->CreateLinkedObject<FD3D12RHITextureReference>(FRHIGPUMask::All(), [ReferencedTexture](FD3D12Device* Device)
	{
		return new FD3D12RHITextureReference(Device, ResourceCast(ReferencedTexture, Device->GetGPUIndex()));
	});
}

void FD3D12DynamicRHI::RHIUpdateTextureReference(FRHICommandListBase& RHICmdList, FRHITextureReference* TextureRef, FRHITexture* InNewTexture)
{
	FRHITexture* NewTexture = InNewTexture ? InNewTexture : FRHITextureReference::GetDefaultTexture();

	for (TD3D12DualLinkedObjectIterator<FD3D12RHITextureReference, FD3D12Texture> It(ResourceCast(TextureRef), ResourceCast(NewTexture)); It; ++It)
	{
		It.GetFirst()->SwitchToNewTexture(RHICmdList, It.GetSecond());
	}
}
