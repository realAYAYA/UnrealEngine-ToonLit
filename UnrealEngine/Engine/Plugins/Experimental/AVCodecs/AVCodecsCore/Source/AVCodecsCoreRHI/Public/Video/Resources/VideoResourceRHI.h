// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AVContext.h"
#include "AVExtension.h"
#include "Video/VideoResource.h"
#include "RHI.h"

#if AVCODECS_USE_D3D
THIRD_PARTY_INCLUDES_START
#include "Windows/WindowsHWrapper.h"
#include "d3d12.h"
THIRD_PARTY_INCLUDES_END
#endif

class AVCODECSCORERHI_API FVideoContextRHI : public FAVContext
{
};

class AVCODECSCORERHI_API FVideoResourceRHI : public TVideoResource<FVideoContextRHI>
{
public:
	struct FRawData
	{
		FTextureRHIRef Texture;
#if AVCODECS_USE_D3D
		TRefCountPtr<ID3D12Fence> Fence; // TODO replace with an RHI fence and convert in a transform function
#else 
		FGPUFenceRHIRef Fence;
#endif
		uint64 FenceValue;
	};

private:
	FRawData Raw;

public:
	static FAVLayout GetLayoutFrom(TSharedRef<FAVDevice> const& Device, FTextureRHIRef const& Raw);
	static FVideoDescriptor GetDescriptorFrom(TSharedRef<FAVDevice> const& Device, FTextureRHIRef const& Raw);

	static TSharedPtr<FVideoResourceRHI> Create(TSharedPtr<FAVDevice> const& Device, FVideoDescriptor const& Descriptor, ETextureCreateFlags AdditionalFlags = ETextureCreateFlags::None, bool bIsSRGB = true);


	FORCEINLINE FRawData const& GetRaw() const { return Raw; }

	FVideoResourceRHI(TSharedRef<FAVDevice> const& Device, FRawData const& Raw, FVideoDescriptor const& OverrideDescriptor);
	FVideoResourceRHI(TSharedRef<FAVDevice> const& Device, FRawData const& Raw);
	virtual ~FVideoResourceRHI() override = default;

	virtual FAVResult Validate() const override;

	virtual void Lock() override;
	virtual FScopeLock LockScope() override;

	void CopyFrom(TArrayView64<uint8> const& From);
	void CopyFrom(TArrayView<uint8> const& From);
	void CopyFrom(TArray64<uint8> const& From);
	void CopyFrom(TArray<uint8> const& From);
	void CopyFrom(FTextureRHIRef const& From);

	TSharedPtr<FVideoResourceRHI> TransformResource(FVideoDescriptor const& OutDescriptor);
	void TransformResourceTo(FRHICommandListImmediate& RHICmdList, FTextureRHIRef Target, FVideoDescriptor const& OutDescriptor);
};

class AVCODECSCORERHI_API FResolvableVideoResourceRHI : public TResolvableVideoResource<FVideoResourceRHI>
{
public:
	ETextureCreateFlags AdditionalFlags = ETextureCreateFlags::None;
	
protected:
	virtual TSharedPtr<FVideoResourceRHI> TryResolve(TSharedPtr<FAVDevice> const& Device, FVideoDescriptor const& Descriptor) override;
};

/*
* Handles pixel format or colorspace transformations
*/
template <>
FAVResult FAVExtension::TransformResource(TSharedPtr<FVideoResourceRHI>& OutResource, TSharedPtr<class FVideoResourceRHI> const& InResource);

template <>
FAVResult FAVExtension::TransformResource(TSharedPtr<FVideoResourceRHI>& OutResource, TSharedPtr<class FVideoResourceCPU> const& InResource);

#if AVCODECS_USE_D3D

template <>
FAVResult FAVExtension::TransformResource(TSharedPtr<class FVideoResourceD3D11>& OutResource, TSharedPtr<FVideoResourceRHI> const& InResource);

template <>
FAVResult FAVExtension::TransformResource(TSharedPtr<class FVideoResourceD3D12>& OutResource, TSharedPtr<FVideoResourceRHI> const& InResource);

#endif

#if AVCODECS_USE_VULKAN

template <>
FAVResult FAVExtension::TransformResource(TSharedPtr<class FVideoResourceVulkan>& OutResource, TSharedPtr<FVideoResourceRHI> const& InResource);

#endif

#if AVCODECS_USE_METAL

template <>
FAVResult FAVExtension::TransformResource(TSharedPtr<class FVideoResourceMetal>& OutResource, TSharedPtr<FVideoResourceRHI> const& InResource);

#endif

DECLARE_TYPEID(FVideoContextRHI, AVCODECSCORERHI_API);
DECLARE_TYPEID(FVideoResourceRHI, AVCODECSCORERHI_API);
