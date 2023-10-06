// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RHIResources.h"

class FRHITextureReference final : public FRHITexture
{
public:
	UE_DEPRECATED(5.3, "FRHITextureReference now requires a texture reference on construction")
	RHI_API FRHITextureReference();
	RHI_API FRHITextureReference(FRHITexture* InReferencedTexture, FRHIShaderResourceView* InBindlessView = nullptr);
	RHI_API ~FRHITextureReference();

	RHI_API virtual class FRHITextureReference* GetTextureReference() override;
	RHI_API virtual FRHIDescriptorHandle GetDefaultBindlessHandle() const override;

	RHI_API virtual void* GetNativeResource() const override;
	RHI_API virtual void* GetNativeShaderResourceView() const override;
	RHI_API virtual void* GetTextureBaseRHI() override;
	RHI_API virtual void GetWriteMaskProperties(void*& OutData, uint32& OutSize) override;
	RHI_API virtual const FRHITextureDesc& GetDesc() const override;

#if ENABLE_RHI_VALIDATION
	// Implement RHIValidation::FTextureResource::GetTrackerResource to use the tracker info
	// for the referenced texture.
	RHI_API virtual RHIValidation::FResource* GetTrackerResource() final override;
#endif

	inline FRHITexture* GetReferencedTexture() const { return ReferencedTexture.GetReference(); }
	inline FRHIShaderResourceView* GetBindlessView() const { return BindlessView.GetReference(); }

	static inline FRHITexture* GetDefaultTexture() { return DefaultTexture; }

private:
	friend class FRHICommandListImmediate;
	friend class FDynamicRHI;

	// Called only from FDynamicRHI::RHIUpdateTextureReference
	void SetReferencedTexture(FRHITexture* InTexture)
	{
		ReferencedTexture = InTexture;
	}

	TRefCountPtr<FRHITexture> ReferencedTexture;

	// SRV to create a reusable slot for bindless handles for this texture reference
	FShaderResourceViewRHIRef BindlessView;

	// This pointer is set by the InitRHI() function on the FBlackTextureWithSRV global resource,
	// to allow FRHITextureReference to use the global black texture when the reference is nullptr.
	// A pointer is required since FBlackTextureWithSRV is defined in RenderCore.
	friend class FBlackTextureWithSRV;
	RHI_API static TRefCountPtr<FRHITexture> DefaultTexture;
};
