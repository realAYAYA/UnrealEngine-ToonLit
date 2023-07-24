// Copyright Epic Games, Inc. All Rights Reserved.

#include "RHITextureReference.h"
#include "RHI.h"

FRHITextureReference::FRHITextureReference()
	: FRHITexture(RRT_TextureReference)
{
	check(DefaultTexture);
	ReferencedTexture = DefaultTexture;
	UpdateBindlessShaderResourceView();
}

FRHITextureReference::~FRHITextureReference() = default;

FRHITextureReference* FRHITextureReference::GetTextureReference()
{
	return this;
}

FRHIDescriptorHandle FRHITextureReference::GetDefaultBindlessHandle() const
{ 
	check(ReferencedTexture);

	// If an SRV has been created, return its handle
	return BindlessShaderResourceViewRHI.IsValid() ?
		BindlessShaderResourceViewRHI->GetBindlessHandle() : 
		ReferencedTexture->GetDefaultBindlessHandle();
}

void* FRHITextureReference::GetNativeResource() const
{
	check(ReferencedTexture);
	return ReferencedTexture->GetNativeResource();
}

void* FRHITextureReference::GetNativeShaderResourceView() const
{
	check(ReferencedTexture);
	return ReferencedTexture->GetNativeShaderResourceView();
}

void* FRHITextureReference::GetTextureBaseRHI()
{
	check(ReferencedTexture);
	return ReferencedTexture->GetTextureBaseRHI();
}

void FRHITextureReference::GetWriteMaskProperties(void*& OutData, uint32& OutSize)
{
	check(ReferencedTexture);
	return ReferencedTexture->GetWriteMaskProperties(OutData, OutSize);
}

#if ENABLE_RHI_VALIDATION
RHIValidation::FResource* FRHITextureReference::GetTrackerResource()
{
	check(ReferencedTexture);
	return ReferencedTexture->GetTrackerResource();
}
#endif

const FRHITextureDesc& FRHITextureReference::GetDesc() const
{
	check(ReferencedTexture);
	return ReferencedTexture->GetDesc();
}

void FRHITextureReference::UpdateBindlessShaderResourceView()
{
	if (RHIGetBindlessResourcesConfiguration(GMaxRHIShaderPlatform) != ERHIBindlessConfiguration::Disabled)
	{
		BindlessShaderResourceViewRHI = RHICreateShaderResourceView(ReferencedTexture.GetReference(), 0u);
	}
}
