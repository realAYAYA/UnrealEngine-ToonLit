// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"
#include "ClearReplacementShaders.h"

FD3D12UnorderedAccessView::FD3D12UnorderedAccessView(FD3D12Device* InParent, FRHIViewableResource* InParentResource)
	: FRHIUnorderedAccessView(InParentResource)
	, FD3D12View(InParent, ERHIDescriptorHeapType::Standard, ViewSubresourceSubsetFlags_None)
{
}

FD3D12UnorderedAccessView::FD3D12UnorderedAccessView(FD3D12Device* InParent, FRHIViewableResource* InParentResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC& InDesc, FD3D12BaseShaderResource* InBaseShaderResource, FD3D12Resource* InCounterResource)
	: FRHIUnorderedAccessView(InParentResource)
	, FD3D12View(InParent, ERHIDescriptorHeapType::Standard, ViewSubresourceSubsetFlags_None)
	, CounterResource(InCounterResource)
{
	SetDesc(InDesc);
	CreateView(InBaseShaderResource, InBaseShaderResource->ResourceLocation, InCounterResource, ED3D12DescriptorCreateReason::InitialCreate);
}

void FD3D12UnorderedAccessView::RecreateView()
{
	check(CounterResource == nullptr);
	check(ResourceLocation->GetOffsetFromBaseOfResource() == 0);
	CreateView(BaseShaderResource, *ResourceLocation, nullptr, ED3D12DescriptorCreateReason::UpdateOrRename);
}

void FD3D12UnorderedAccessView::CreateView(FD3D12BaseShaderResource* InBaseShaderResource, FD3D12ResourceLocation& InResourceLocation, FD3D12Resource* InCounterResource, ED3D12DescriptorCreateReason Reason)
{
	InitializeInternal(InBaseShaderResource, InResourceLocation);

	if (Resource)
	{
		ID3D12Resource* D3DResource = Resource->GetUAVAccessResource() ? Resource->GetUAVAccessResource() : Resource->GetResource();
		ID3D12Resource* D3DCounterResource = InCounterResource ? InCounterResource->GetResource() : nullptr;
		Descriptor.CreateView(Desc, D3DResource, D3DCounterResource, Reason);
	}
}

template<typename ResourceType>
inline FD3D12UnorderedAccessView* CreateUAV(D3D12_UNORDERED_ACCESS_VIEW_DESC& Desc, ResourceType* Resource, bool bNeedsCounterResource)
{
	if (Resource == nullptr)
	{
		return nullptr;
	}

	FD3D12Adapter* Adapter = Resource->GetParentDevice()->GetParentAdapter();

	return Adapter->CreateLinkedViews<ResourceType, FD3D12UnorderedAccessView>(Resource, [bNeedsCounterResource, &Desc](ResourceType* Resource)
	{
		FD3D12Device* Device = Resource->GetParentDevice();
		FD3D12Resource* CounterResource = nullptr;

		if (bNeedsCounterResource)
		{
			const FRHIGPUMask Node = Device->GetGPUMask();
			const D3D12_HEAP_PROPERTIES HeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT, Node.GetNative(), Node.GetNative());
			const D3D12_RESOURCE_STATES InitialState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
			Device->GetParentAdapter()->CreateBuffer(HeapProps, Node, InitialState, ED3D12ResourceStateMode::MultiState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 4, &CounterResource,  TEXT("Counter"), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		}

		return new FD3D12UnorderedAccessView(Device, Resource, Desc, Resource, CounterResource);
	});
}

FUnorderedAccessViewRHIRef FD3D12DynamicRHI::RHICreateUnorderedAccessView(FRHIBuffer* BufferRHI, bool bUseUAVCounter, bool bAppendBuffer)
{
	FD3D12Buffer* Buffer = FD3D12DynamicRHI::ResourceCast(BufferRHI);

	FD3D12ResourceLocation& Location = Buffer->ResourceLocation;

	const EBufferUsageFlags BufferUsage = Buffer->GetUsage();
	const bool bByteAccessBuffer = EnumHasAnyFlags(BufferUsage, BUF_ByteAddressBuffer);
	const bool bStructuredBuffer = !bByteAccessBuffer;
	check(bByteAccessBuffer != bStructuredBuffer); // You can't have a structured buffer that allows raw views

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc{};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	UAVDesc.Format = DXGI_FORMAT_UNKNOWN;

	uint32 EffectiveStride = Buffer->GetStride();

	if (bByteAccessBuffer)
	{
		UAVDesc.Format  = DXGI_FORMAT_R32_TYPELESS;
		UAVDesc.Buffer.Flags |= D3D12_BUFFER_UAV_FLAG_RAW;
		EffectiveStride = 4;
	}
	else if (EnumHasAnyFlags(BufferUsage, BUF_DrawIndirect))
	{
		UAVDesc.Format  = DXGI_FORMAT_R32_UINT;
		EffectiveStride = 4;
	}

	UAVDesc.Buffer.FirstElement = Location.GetOffsetFromBaseOfResource() / EffectiveStride;
	UAVDesc.Buffer.NumElements  = Location.GetSize() / EffectiveStride;
	UAVDesc.Buffer.StructureByteStride = bStructuredBuffer ? EffectiveStride : 0;

	const bool bNeedsCounterResource = bAppendBuffer | bUseUAVCounter;
	return CreateUAV(UAVDesc, Buffer, bNeedsCounterResource);
}

FUnorderedAccessViewRHIRef FD3D12DynamicRHI::RHICreateUnorderedAccessView(FRHITexture* TextureRHI, uint32 MipLevel, uint8 Format, uint16 FirstArraySlice, uint16 NumArraySlices)
{
	FD3D12Texture* Texture = GetD3D12TextureFromRHITexture(TextureRHI);
	ETextureDimension Dimension = TextureRHI->GetDesc().Dimension;

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};

	const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;
	UAVDesc.Format = FindShaderResourceDXGIFormat(PlatformResourceFormat, false);

	switch (Dimension)
	{
	case ETextureDimension::Texture3D:
	{
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		UAVDesc.Texture3D.MipSlice = MipLevel;
		UAVDesc.Texture3D.FirstWSlice = 0;
		UAVDesc.Texture3D.WSize = Texture->GetDesc().Depth >> MipLevel;

		return CreateUAV(UAVDesc, Texture, false);
	}
	case ETextureDimension::Texture2DArray:
	{
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		UAVDesc.Texture2DArray.MipSlice = MipLevel;
		UAVDesc.Texture2DArray.FirstArraySlice = NumArraySlices == 0 ? 0 : FirstArraySlice;
		UAVDesc.Texture2DArray.ArraySize = NumArraySlices == 0 ? Texture->GetDesc().ArraySize : NumArraySlices;
		UAVDesc.Texture2DArray.PlaneSlice = GetPlaneSliceFromViewFormat(PlatformResourceFormat, UAVDesc.Format);

		return CreateUAV(UAVDesc, Texture, false);
	}
	case ETextureDimension::TextureCube:
	{
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		UAVDesc.Texture2DArray.MipSlice = MipLevel;
		UAVDesc.Texture2DArray.FirstArraySlice = 0;
		UAVDesc.Texture2DArray.ArraySize = 6;
		UAVDesc.Texture2DArray.PlaneSlice = GetPlaneSliceFromViewFormat(PlatformResourceFormat, UAVDesc.Format);

		return CreateUAV(UAVDesc, Texture, false);
	}
	case ETextureDimension::TextureCubeArray:
	{
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		UAVDesc.Texture2DArray.MipSlice = MipLevel;
		UAVDesc.Texture2DArray.FirstArraySlice = NumArraySlices == 0 ? 0 : FirstArraySlice * 6;
		UAVDesc.Texture2DArray.ArraySize = NumArraySlices == 0 ? Texture->GetDesc().ArraySize * 6 : NumArraySlices * 6;
		UAVDesc.Texture2DArray.PlaneSlice = GetPlaneSliceFromViewFormat(PlatformResourceFormat, UAVDesc.Format);

		return CreateUAV(UAVDesc, Texture, false);
	}
	default:
	{
		UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		UAVDesc.Texture2D.MipSlice = MipLevel;
		UAVDesc.Texture2D.PlaneSlice = GetPlaneSliceFromViewFormat(PlatformResourceFormat, UAVDesc.Format);

		return CreateUAV(UAVDesc, Texture, false);
	}
	}
}

FUnorderedAccessViewRHIRef FD3D12DynamicRHI::RHICreateUnorderedAccessView(FRHITexture* TextureRHI, uint32 MipLevel, uint16 FirstArraySlice, uint16 NumArraySlices)
{
	return RHICreateUnorderedAccessView(TextureRHI, MipLevel, TextureRHI->GetFormat(), FirstArraySlice, NumArraySlices);
}

FUnorderedAccessViewRHIRef FD3D12DynamicRHI::RHICreateUnorderedAccessView(FRHIBuffer* BufferRHI, uint8 Format)
{
	FD3D12Buffer* Buffer = FD3D12DynamicRHI::ResourceCast(BufferRHI);
	FD3D12ResourceLocation& Location = Buffer->ResourceLocation;

	D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc{};
	UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

	uint32 EffectiveStride;
	if (EnumHasAnyFlags(Buffer->GetUsage(), BUF_ByteAddressBuffer))
	{
		UAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		UAVDesc.Buffer.Flags |= D3D12_BUFFER_UAV_FLAG_RAW;
		EffectiveStride = 4;
	}
	else
	{
		UAVDesc.Format = FindUnorderedAccessDXGIFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat);
		EffectiveStride = GPixelFormats[Format].BlockBytes;
	}

	UAVDesc.Buffer.FirstElement = Location.GetOffsetFromBaseOfResource() / EffectiveStride;
	UAVDesc.Buffer.NumElements = Location.GetSize() / EffectiveStride;

	return CreateUAV(UAVDesc, Buffer, false);
}

void FD3D12CommandContext::ClearUAV(TRHICommandList_RecursiveHazardous<FD3D12CommandContext>& RHICmdList, FD3D12UnorderedAccessView* UnorderedAccessView, const void* ClearValues, bool bFloat)
{
	const D3D12_RESOURCE_DESC& ResourceDesc = UnorderedAccessView->GetResource()->GetDesc();
	const D3D12_UNORDERED_ACCESS_VIEW_DESC& UAVDesc = UnorderedAccessView->GetDesc();

	// Only structured buffers can have an unknown format
	check(UAVDesc.ViewDimension == D3D12_UAV_DIMENSION_BUFFER || UAVDesc.Format != DXGI_FORMAT_UNKNOWN);

	EClearReplacementValueType ValueType = bFloat ? EClearReplacementValueType::Float : EClearReplacementValueType::Uint32;
	switch (UAVDesc.Format)
	{
	case DXGI_FORMAT_R32G32B32A32_SINT:
	case DXGI_FORMAT_R32G32B32_SINT:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R32G32_SINT:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R8G8_SINT:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_R8_SINT:
		ValueType = EClearReplacementValueType::Int32;
		break;

	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
	case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
	case DXGI_FORMAT_R10G10B10A2_UINT:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R8_UINT:
		ValueType = EClearReplacementValueType::Uint32;
		break;
	}

	ensureMsgf((UAVDesc.Format == DXGI_FORMAT_UNKNOWN) || (bFloat == (ValueType == EClearReplacementValueType::Float)), TEXT("Attempt to clear a UAV using the wrong RHIClearUAV function. Float vs Integer mismatch."));

	if (UAVDesc.ViewDimension == D3D12_UAV_DIMENSION_BUFFER)
	{
		if (UAVDesc.Format == DXGI_FORMAT_UNKNOWN || (UAVDesc.Buffer.Flags & D3D12_BUFFER_UAV_FLAG_RAW) != 0)
		{
			// Structured buffer.
			RHICmdList.RunOnContext([UnorderedAccessView, ClearValues, UAVDesc](auto& Context)
			{
				// Alias the structured buffer with an R32_UINT UAV to perform the clear.
				// We construct a temporary UAV on the offline heap, copy it to the online heap, and then call ClearUnorderedAccessViewUint.

				FD3D12Device* ParentDevice = Context.GetParentDevice();
				ID3D12Device* Device = ParentDevice->GetDevice();
				ID3D12Resource* Resource = UnorderedAccessView->GetResource()->GetResource();

				D3D12_UNORDERED_ACCESS_VIEW_DESC R32UAVDesc{};
				if ((UAVDesc.Buffer.Flags & D3D12_BUFFER_UAV_FLAG_RAW) != 0)
				{
					// Raw UAVs will already be setup correctly for us.
					check(UAVDesc.Format == DXGI_FORMAT_R32_TYPELESS);
					R32UAVDesc = UAVDesc;
				}
				else
				{
					// Structured buffer stride must be a multiple of sizeof(uint32)
					check(UAVDesc.Buffer.StructureByteStride % sizeof(uint32) == 0);
					uint32 DwordsPerElement = UAVDesc.Buffer.StructureByteStride / sizeof(uint32);

					R32UAVDesc.Format = DXGI_FORMAT_R32_UINT;
					R32UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
					R32UAVDesc.Buffer.FirstElement = UAVDesc.Buffer.FirstElement * DwordsPerElement;
					R32UAVDesc.Buffer.NumElements = UAVDesc.Buffer.NumElements * DwordsPerElement;
				}

				// Scoped descriptor handle will free the offline CPU handle once we return
				FD3D12ViewDescriptorHandle UAVHandle(ParentDevice, ERHIDescriptorHeapType::Standard);
				UAVHandle.CreateView(R32UAVDesc, Resource, nullptr, ED3D12DescriptorCreateReason::InitialCreate);

				// Check if the view heap is full and needs to rollover.
				if (!Context.StateCache.GetDescriptorCache()->GetCurrentViewHeap()->CanReserveSlots(1))
				{
					Context.StateCache.GetDescriptorCache()->GetCurrentViewHeap()->RollOver();
				}

				uint32 ReservedSlot = Context.StateCache.GetDescriptorCache()->GetCurrentViewHeap()->ReserveSlots(1);
				D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = UAVHandle.GetOfflineCpuHandle();
				D3D12_CPU_DESCRIPTOR_HANDLE DestSlot = Context.StateCache.GetDescriptorCache()->GetCurrentViewHeap()->GetCPUSlotHandle(ReservedSlot);
				D3D12_GPU_DESCRIPTOR_HANDLE GPUHandle = Context.StateCache.GetDescriptorCache()->GetCurrentViewHeap()->GetGPUSlotHandle(ReservedSlot);

				Device->CopyDescriptorsSimple(1, DestSlot, CPUHandle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

				Context.TransitionResource(UnorderedAccessView, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

				Context.FlushResourceBarriers();
				Context.GraphicsCommandList()->ClearUnorderedAccessViewUint(GPUHandle, CPUHandle, Resource, *reinterpret_cast<const UINT(*)[4]>(ClearValues), 0, nullptr);
				Context.UpdateResidency(UnorderedAccessView->GetResource());
				Context.ConditionalSplitCommandList();

				if (Context.IsDefaultContext())
				{
					ParentDevice->RegisterGPUWork(1);
				}
			});
		}
		else
		{
			ClearUAVShader_T<EClearReplacementResourceType::Buffer, 4, false>(RHICmdList, UnorderedAccessView, UAVDesc.Buffer.NumElements, 1, 1, ClearValues, ValueType);
		}
	}
	else
	{
		if (UAVDesc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2D)
		{
			uint32 Width = ResourceDesc.Width >> UAVDesc.Texture2D.MipSlice;
			uint32 Height = ResourceDesc.Height >> UAVDesc.Texture2D.MipSlice;
			ClearUAVShader_T<EClearReplacementResourceType::Texture2D, 4, false>(RHICmdList, UnorderedAccessView, Width, Height, 1, ClearValues, ValueType);
		}
		else if (UAVDesc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE2DARRAY)
		{
			uint32 Width = ResourceDesc.Width >> UAVDesc.Texture2DArray.MipSlice;
			uint32 Height = ResourceDesc.Height >> UAVDesc.Texture2DArray.MipSlice;
			ClearUAVShader_T<EClearReplacementResourceType::Texture2DArray, 4, false>(RHICmdList, UnorderedAccessView, Width, Height, UAVDesc.Texture2DArray.ArraySize, ClearValues, ValueType);
		}
		else if (UAVDesc.ViewDimension == D3D12_UAV_DIMENSION_TEXTURE3D)
		{
			// @todo - is WSize / mip index handling here correct?
			uint32 Width = ResourceDesc.Width >> UAVDesc.Texture2DArray.MipSlice;
			uint32 Height = ResourceDesc.Height >> UAVDesc.Texture2DArray.MipSlice;
			ClearUAVShader_T<EClearReplacementResourceType::Texture3D, 4, false>(RHICmdList, UnorderedAccessView, Width, Height, UAVDesc.Texture3D.WSize, ClearValues, ValueType);
		}
		else
		{
			ensure(0);
		}
	}
}

void FD3D12CommandContext::RHIClearUAVFloat(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FVector4f& Values)
{
	TRHICommandList_RecursiveHazardous<FD3D12CommandContext> RHICmdList(this);
	ClearUAV(RHICmdList, RetrieveObject<FD3D12UnorderedAccessView>(UnorderedAccessViewRHI), &Values, true);
}

void FD3D12CommandContext::RHIClearUAVUint(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FUintVector4& Values)
{
	TRHICommandList_RecursiveHazardous<FD3D12CommandContext> RHICmdList(this);
	ClearUAV(RHICmdList, RetrieveObject<FD3D12UnorderedAccessView>(UnorderedAccessViewRHI), &Values, false);
}

FUnorderedAccessViewRHIRef FD3D12DynamicRHI::RHICreateUnorderedAccessView_RenderThread(FRHICommandListImmediate& RHICmdList, FRHIBuffer* BufferRHI, bool bUseUAVCounter, bool bAppendBuffer)
{
	FD3D12Buffer* Buffer = FD3D12DynamicRHI::ResourceCast(BufferRHI);
	// TODO: we have to stall the RHI thread when creating SRVs of dynamic buffers because they get renamed.
	// perhaps we could do a deferred operation?
	if (EnumHasAnyFlags(Buffer->GetUsage(), BUF_AnyDynamic))
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return RHICreateUnorderedAccessView(BufferRHI, bUseUAVCounter, bAppendBuffer);
	}
	return RHICreateUnorderedAccessView(BufferRHI, bUseUAVCounter, bAppendBuffer);
}

FUnorderedAccessViewRHIRef FD3D12DynamicRHI::RHICreateUnorderedAccessView_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, uint32 MipLevel, uint16 FirstArraySlice, uint16 NumArraySlices)
{
	return FD3D12DynamicRHI::RHICreateUnorderedAccessView(Texture, MipLevel, FirstArraySlice, NumArraySlices);
}

FUnorderedAccessViewRHIRef FD3D12DynamicRHI::RHICreateUnorderedAccessView_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, uint32 MipLevel, uint8 Format, uint16 FirstArraySlice, uint16 NumArraySlices)
{
	return FD3D12DynamicRHI::RHICreateUnorderedAccessView(Texture, MipLevel, Format, FirstArraySlice, NumArraySlices);
}

FUnorderedAccessViewRHIRef FD3D12DynamicRHI::RHICreateUnorderedAccessView_RenderThread(FRHICommandListImmediate& RHICmdList, FRHIBuffer* BufferRHI, uint8 Format)
{
	FD3D12Buffer* Buffer = FD3D12DynamicRHI::ResourceCast(BufferRHI);

	// TODO: we have to stall the RHI thread when creating SRVs of dynamic buffers because they get renamed.
	// perhaps we could do a deferred operation?
	if (EnumHasAnyFlags(Buffer->GetUsage(), BUF_AnyDynamic))
	{
		FScopedRHIThreadStaller StallRHIThread(RHICmdList);
		return RHICreateUnorderedAccessView(BufferRHI, Format);
	}
	return RHICreateUnorderedAccessView(BufferRHI, Format);
}

FD3D12StagingBuffer::~FD3D12StagingBuffer()
{
	ResourceLocation.Clear();
}

void* FD3D12StagingBuffer::Lock(uint32 Offset, uint32 NumBytes)
{
	check(!bIsLocked);
	bIsLocked = true;
	if (ResourceLocation.IsValid())
	{
		// readback resource are kept mapped after creation
		return reinterpret_cast<uint8*>(ResourceLocation.GetMappedBaseAddress()) + Offset;
	}
	else
	{
		return nullptr;
	}
}

void FD3D12StagingBuffer::Unlock()
{
	check(bIsLocked);
	bIsLocked = false;
}