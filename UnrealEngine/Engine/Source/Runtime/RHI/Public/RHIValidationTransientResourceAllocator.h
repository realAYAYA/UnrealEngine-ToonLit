// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "RHITransientResourceAllocator.h"

#if ENABLE_RHI_VALIDATION

class FValidationTransientResourceAllocator : public IRHITransientResourceAllocator
{
public:
	FValidationTransientResourceAllocator(IRHITransientResourceAllocator* InRHIAllocator)
		: RHIAllocator(InRHIAllocator)
	{}

	virtual ~FValidationTransientResourceAllocator();

	// Implementation of FRHITransientResourceAllocator interface
	virtual bool SupportsResourceType(ERHITransientResourceType InType) const override final { return RHIAllocator->SupportsResourceType(InType); }
	virtual FRHITransientTexture* CreateTexture(const FRHITextureCreateInfo& InCreateInfo, const TCHAR* InDebugName, uint32 InPassIndex) override final;
	virtual FRHITransientBuffer* CreateBuffer(const FRHIBufferCreateInfo& InCreateInfo, const TCHAR* InDebugName, uint32 InPassIndex) override final;
	virtual void DeallocateMemory(FRHITransientTexture* InTexture, uint32 InPassIndex) override final;
	virtual void DeallocateMemory(FRHITransientBuffer* InBuffer, uint32 InPassIndex) override final;
	virtual void Flush(FRHICommandListImmediate&, FRHITransientAllocationStats*) override final;
	virtual void Release(FRHICommandListImmediate&) override final;

private:
	// Actual RHI transient allocator which will get all functions forwarded
	IRHITransientResourceAllocator* RHIAllocator = nullptr;

	// All the allocated resources on the transient allocator
	struct FAllocatedResourceData
	{
		enum class EType
		{
			Texture,
			Buffer,
		};

		FString DebugName;
		EType ResourceType = EType::Texture;

		struct FTexture
		{
			ETextureCreateFlags Flags = TexCreate_None;
			EPixelFormat Format = PF_Unknown;
			uint16 ArraySize = 0;
			uint8 NumMips = 0;
		} Texture;
	};

	using FAllocatedResourceDataMap = TMap<FRHIResource*, FAllocatedResourceData>;
	using FAllocatedResourceDataArray = TArray<TPair<FRHIResource*, FAllocatedResourceData>>;

	friend class FValidationContext;
	static void InitBarrierTracking(const FAllocatedResourceDataArray& AllocatedResourcesToInit);

	FAllocatedResourceDataMap AllocatedResourceMap;
	FAllocatedResourceDataArray AllocatedResourcesToInit;
};

#endif	// ENABLE_RHI_VALIDATION
