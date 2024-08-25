// Copyright Epic Games, Inc. All Rights Reserved.

#include "VulkanRHIPrivate.h"
#include "VulkanContext.h"
#include "VulkanDescriptorSets.h"
#include "VulkanLLM.h"
#include "ClearReplacementShaders.h"

#if VULKAN_RHI_RAYTRACING
#include "VulkanRayTracing.h"
#endif // VULKAN_RHI_RAYTRACING


FVulkanView::FVulkanView(FVulkanDevice& InDevice, VkDescriptorType InDescriptorType)
	: Device(InDevice)
{
	BindlessHandle = Device.GetBindlessDescriptorManager()->ReserveDescriptor(InDescriptorType);
}

FVulkanView::~FVulkanView()
{
	Invalidate();

	if (BindlessHandle.IsValid())
	{
		Device.GetDeferredDeletionQueue().EnqueueBindlessHandle(BindlessHandle);
		BindlessHandle = FRHIDescriptorHandle();
	}
}

void FVulkanView::Invalidate()
{
	// Carry forward its initialized state
	const bool bIsInitialized = IsInitialized();

	switch (GetViewType())
	{
	default: checkNoEntry(); [[fallthrough]];
	case EType::Null:
		break;

	case EType::TypedBuffer:
		DEC_DWORD_STAT(STAT_VulkanNumBufferViews);
		Device.GetDeferredDeletionQueue().EnqueueResource(FDeferredDeletionQueue2::EType::BufferView, Storage.Get<FTypedBufferView>().View);
		break;

	case EType::Texture:
		DEC_DWORD_STAT(STAT_VulkanNumImageViews);
		Device.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue2::EType::ImageView, Storage.Get<FTextureView>().View);
		break;

	case EType::StructuredBuffer:
		// Nothing to do
		break;

#if VULKAN_RHI_RAYTRACING
	case EType::AccelerationStructure:
		Device.GetDeferredDeletionQueue().EnqueueResource(VulkanRHI::FDeferredDeletionQueue2::EType::AccelerationStructure, Storage.Get<FAccelerationStructureView>().Handle);
		break;
#endif
	}

	Storage.Emplace<FInvalidatedState>();
	Storage.Get<FInvalidatedState>().bInitialized = bIsInitialized;
}

FVulkanView* FVulkanView::InitAsTypedBufferView(FVulkanResourceMultiBuffer* Buffer, EPixelFormat UEFormat, uint32 InOffset, uint32 InSize)
{
	// We will need a deferred update if the descriptor was already in use
	const bool bImmediateUpdate = !IsInitialized();

	check(GetViewType() == EType::Null);
	Storage.Emplace<FTypedBufferView>();
	FTypedBufferView& TBV = Storage.Get<FTypedBufferView>();

	const uint32 TotalOffset = Buffer->GetOffset() + InOffset;

	check(UEFormat != PF_Unknown);
	VkFormat Format = GVulkanBufferFormat[UEFormat];
	check(Format != VK_FORMAT_UNDEFINED);

	VkBufferViewCreateInfo ViewInfo;
	ZeroVulkanStruct(ViewInfo, VK_STRUCTURE_TYPE_BUFFER_VIEW_CREATE_INFO);
	ViewInfo.buffer = Buffer->GetHandle();
	ViewInfo.offset = TotalOffset;
	ViewInfo.format = Format;

	// :todo-jn: Volatile buffers use temporary allocations that can be smaller than the buffer creation size.  Check if the savings are still worth it.
	if (Buffer->IsVolatile())
	{
		InSize = FMath::Min<uint64>(InSize, Buffer->GetCurrentSize());
	}

	const uint32 TypeSize = GetNumBitsPerPixel(Format) / 8u;
	// View size has to be a multiple of element size
	// Commented out because there are multiple places in the high level rendering code which re-purpose buffers for a new format while there are still
	// views with the old format lying around, and then lock them with a size computed based on the new stride, triggering this assert when the old views
	// are re-created. These places need to be fixed before re-enabling this check (UE-211785).
	//check(IsAligned(InSize, TypeSize));

	//#todo-rco: Revisit this if buffer views become VK_BUFFER_USAGE_STORAGE_BUFFER_BIT instead of VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT
	const VkPhysicalDeviceLimits& Limits = Device.GetLimits();
	const uint64 MaxSize = (uint64)Limits.maxTexelBufferElements * TypeSize;
	ViewInfo.range = FMath::Min<uint64>(InSize, MaxSize);
	// TODO: add a check() for exceeding MaxSize, to catch code which blindly makes views without checking the platform limits.

	check(Buffer->GetBufferUsageFlags() & (VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT));
	check(IsAligned(InOffset, Limits.minTexelBufferOffsetAlignment));

	VERIFYVULKANRESULT(VulkanRHI::vkCreateBufferView(Device.GetInstanceHandle(), &ViewInfo, VULKAN_CPU_ALLOCATOR, &TBV.View));

	TBV.bVolatile = Buffer->IsVolatile();
	if (!TBV.bVolatile && UseVulkanDescriptorCache())
	{
		TBV.ViewId = ++GVulkanBufferViewHandleIdCounter;
	}

	INC_DWORD_STAT(STAT_VulkanNumBufferViews);
	// :todo-jn: the buffer view is actually not needed in bindless anymore

	Device.GetBindlessDescriptorManager()->UpdateTexelBuffer(BindlessHandle, ViewInfo, bImmediateUpdate);

	return this;
}

FVulkanView* FVulkanView::InitAsTextureView(
	  VkImage InImage
	, VkImageViewType ViewType
	, VkImageAspectFlags AspectFlags
	, EPixelFormat UEFormat
	, VkFormat Format
	, uint32 FirstMip
	, uint32 NumMips
	, uint32 ArraySliceIndex
	, uint32 NumArraySlices
	, bool bUseIdentitySwizzle
	, VkImageUsageFlags ImageUsageFlags)
{
	// We will need a deferred update if the descriptor was already in use
	const bool bImmediateUpdate = !IsInitialized();

	check(GetViewType() == EType::Null);
	Storage.Emplace<FTextureView>();
	FTextureView& TV = Storage.Get<FTextureView>();

	LLM_SCOPE_VULKAN(ELLMTagVulkan::VulkanTextures);

	VkImageViewCreateInfo ViewInfo;
	ZeroVulkanStruct(ViewInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
	ViewInfo.image = InImage;
	ViewInfo.viewType = ViewType;
	ViewInfo.format = Format;

#if VULKAN_SUPPORTS_ASTC_DECODE_MODE
	VkImageViewASTCDecodeModeEXT DecodeMode;
	if (Device.GetOptionalExtensions().HasEXTASTCDecodeMode && IsAstcLdrFormat(Format) && !IsAstcSrgbFormat(Format))
	{
		ZeroVulkanStruct(DecodeMode, VK_STRUCTURE_TYPE_IMAGE_VIEW_ASTC_DECODE_MODE_EXT);
		DecodeMode.decodeMode = VK_FORMAT_R8G8B8A8_UNORM;
		DecodeMode.pNext = ViewInfo.pNext;
		ViewInfo.pNext = &DecodeMode;
	}
#endif

	if (bUseIdentitySwizzle)
	{
		ViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		ViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		ViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		ViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	}
	else
	{
		ViewInfo.components = Device.GetFormatComponentMapping(UEFormat);
	}

	ViewInfo.subresourceRange.aspectMask = AspectFlags;
	ViewInfo.subresourceRange.baseMipLevel = FirstMip;
	ensure(NumMips != 0xFFFFFFFF);
	ViewInfo.subresourceRange.levelCount = NumMips;

	ensure(ArraySliceIndex != 0xFFFFFFFF);
	ensure(NumArraySlices != 0xFFFFFFFF);
	ViewInfo.subresourceRange.baseArrayLayer = ArraySliceIndex;
	ViewInfo.subresourceRange.layerCount = NumArraySlices;

	//HACK.  DX11 on PC currently uses a D24S8 depthbuffer and so needs an X24_G8 SRV to visualize stencil.
	//So take that as our cue to visualize stencil.  In the future, the platform independent code will have a real format
	//instead of PF_DepthStencil, so the cross-platform code could figure out the proper format to pass in for this.
	if (UEFormat == PF_X24_G8)
	{
		ensure((ViewInfo.format == (VkFormat)GPixelFormats[PF_DepthStencil].PlatformFormat) && (ViewInfo.format != VK_FORMAT_UNDEFINED));
		ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	// Inform the driver the view will only be used with a subset of usage flags (to help performance and/or compatibility)
	VkImageViewUsageCreateInfo ImageViewUsageCreateInfo;
	if (ImageUsageFlags != 0)
	{
		ZeroVulkanStruct(ImageViewUsageCreateInfo, VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO);
		ImageViewUsageCreateInfo.usage = ImageUsageFlags;

		ImageViewUsageCreateInfo.pNext = (void*)ViewInfo.pNext;
		ViewInfo.pNext = &ImageViewUsageCreateInfo;
	}

	INC_DWORD_STAT(STAT_VulkanNumImageViews);
	VERIFYVULKANRESULT(VulkanRHI::vkCreateImageView(Device.GetInstanceHandle(), &ViewInfo, VULKAN_CPU_ALLOCATOR, &TV.View));

	TV.Image = InImage;

	if (UseVulkanDescriptorCache())
	{
		TV.ViewId = ++GVulkanImageViewHandleIdCounter;
	}

	const bool bDepthOrStencilAspect = (AspectFlags & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) != 0;
	Device.GetBindlessDescriptorManager()->UpdateImage(BindlessHandle, TV.View, bDepthOrStencilAspect, bImmediateUpdate);

	return this;
}

FVulkanView* FVulkanView::InitAsStructuredBufferView(FVulkanResourceMultiBuffer* Buffer, uint32 InOffset, uint32 InSize)
{
	// We will need a deferred update if the descriptor was already in use
	const bool bImmediateUpdate = !IsInitialized();

	check(GetViewType() == EType::Null);
	Storage.Emplace<FStructuredBufferView>();
	FStructuredBufferView& SBV = Storage.Get<FStructuredBufferView>();

	const uint32 TotalOffset = Buffer->GetOffset() + InOffset;

	SBV.Buffer = Buffer->GetHandle();
	SBV.HandleId = Buffer->GetCurrentAllocation().HandleId;
	SBV.Offset = TotalOffset;

	// :todo-jn: Volatile buffers use temporary allocations that can be smaller than the buffer creation size.  Check if the savings are still worth it.
	if (Buffer->IsVolatile())
	{
		InSize = FMath::Min<uint64>(InSize, Buffer->GetCurrentSize());
	}

	SBV.Size = InSize;

	Device.GetBindlessDescriptorManager()->UpdateBuffer(BindlessHandle, Buffer->GetHandle(), TotalOffset, InSize, bImmediateUpdate);

	return this;
}

#if VULKAN_RHI_RAYTRACING
FVulkanView* FVulkanView::InitAsAccelerationStructureView(FVulkanResourceMultiBuffer* Buffer, uint32 Offset, uint32 Size)
{
	check(GetViewType() == EType::Null);
	Storage.Emplace<FAccelerationStructureView>();
	FAccelerationStructureView& ASV = Storage.Get<FAccelerationStructureView>();

	VkAccelerationStructureCreateInfoKHR CreateInfo;
	ZeroVulkanStruct(CreateInfo, VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR);
	CreateInfo.buffer = Buffer->GetHandle();
	CreateInfo.offset = Buffer->GetOffset() + Offset;
	CreateInfo.size = Size;
	CreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

	VERIFYVULKANRESULT(VulkanDynamicAPI::vkCreateAccelerationStructureKHR(Device.GetInstanceHandle(), &CreateInfo, VULKAN_CPU_ALLOCATOR, &ASV.Handle));

	Device.GetBindlessDescriptorManager()->UpdateAccelerationStructure(BindlessHandle, ASV.Handle);

	return this;
}
#endif




void FVulkanViewableResource::UpdateLinkedViews()
{
	for (FVulkanLinkedView* View = LinkedViews; View; View = View->Next())
	{
		View->UpdateView();
	}
}


static VkImageViewType GetVkImageViewTypeForDimensionSRV(FRHIViewDesc::EDimension DescDimension, VkImageViewType TextureViewType)
{
	switch (DescDimension)
	{
	case FRHIViewDesc::EDimension::Texture2D:			return VK_IMAGE_VIEW_TYPE_2D; break;
	case FRHIViewDesc::EDimension::Texture2DArray:		return VK_IMAGE_VIEW_TYPE_2D_ARRAY; break;
	case FRHIViewDesc::EDimension::Texture3D:			return VK_IMAGE_VIEW_TYPE_3D; break;
	case FRHIViewDesc::EDimension::TextureCube:			return VK_IMAGE_VIEW_TYPE_CUBE; break;
	case FRHIViewDesc::EDimension::TextureCubeArray:	return VK_IMAGE_VIEW_TYPE_CUBE_ARRAY; break;
	case FRHIViewDesc::EDimension::Unknown:				return TextureViewType; break;
	default: break;
	}

	checkf(false, TEXT("Unknown texture dimension value!"));
	return VK_IMAGE_VIEW_TYPE_MAX_ENUM;
}

static VkImageViewType GetVkImageViewTypeForDimensionUAV(FRHIViewDesc::EDimension DescDimension, VkImageViewType TextureViewType)
{
	switch (DescDimension)
	{
	case FRHIViewDesc::EDimension::Texture2D:			return VK_IMAGE_VIEW_TYPE_2D; break;
	case FRHIViewDesc::EDimension::Texture2DArray:		return VK_IMAGE_VIEW_TYPE_2D_ARRAY; break;
	case FRHIViewDesc::EDimension::Texture3D:			return VK_IMAGE_VIEW_TYPE_3D; break;
	case FRHIViewDesc::EDimension::TextureCube:			return VK_IMAGE_VIEW_TYPE_2D_ARRAY; break;
	case FRHIViewDesc::EDimension::TextureCubeArray:	return VK_IMAGE_VIEW_TYPE_2D_ARRAY; break;
	case FRHIViewDesc::EDimension::Unknown:				return TextureViewType; break;
	default: break;
	}

	checkf(false, TEXT("Unknown texture dimension value!"));
	return VK_IMAGE_VIEW_TYPE_MAX_ENUM;

}
static VkDescriptorType GetDescriptorTypeForViewDesc(FRHIViewDesc const& ViewDesc)
{
	if (ViewDesc.IsBuffer())
	{
		if (ViewDesc.IsSRV())
		{
			switch (ViewDesc.Buffer.SRV.BufferType)
			{
			case FRHIViewDesc::EBufferType::Raw:
			case FRHIViewDesc::EBufferType::Structured:
				return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

			case FRHIViewDesc::EBufferType::Typed:
				return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;

			case FRHIViewDesc::EBufferType::AccelerationStructure:
				return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

			default:
				checkNoEntry();
				return VK_DESCRIPTOR_TYPE_MAX_ENUM;
			}
		}
		else
		{
			switch (ViewDesc.Buffer.UAV.BufferType)
			{
			case FRHIViewDesc::EBufferType::Raw:
			case FRHIViewDesc::EBufferType::Structured:
				return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

			case FRHIViewDesc::EBufferType::Typed:
				return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;

			case FRHIViewDesc::EBufferType::AccelerationStructure:
				return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

			default:
				checkNoEntry();
				return VK_DESCRIPTOR_TYPE_MAX_ENUM;
			}
		}
	}
	else
	{
		if (ViewDesc.IsSRV())
		{
			// Sampled images aren't supported in R64, shadercompiler patches them to storage image
			if (ViewDesc.Texture.SRV.Format == PF_R64_UINT)
			{
				return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			}

			return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		}
		else
		{
			return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		}
	}
}


FVulkanShaderResourceView::FVulkanShaderResourceView(FRHICommandListBase& RHICmdList, FVulkanDevice& InDevice, FRHIViewableResource* InResource, FRHIViewDesc const& InViewDesc)
	: FRHIShaderResourceView(InResource, InViewDesc)
	, FVulkanLinkedView(InDevice, GetDescriptorTypeForViewDesc(InViewDesc))
{
	RHICmdList.EnqueueLambda([this](FRHICommandListBase&)
	{
		LinkHead(GetBaseResource()->LinkedViews);
		UpdateView();
	});
}

FVulkanViewableResource* FVulkanShaderResourceView::GetBaseResource() const
{
	return IsBuffer()
		? static_cast<FVulkanViewableResource*>(ResourceCast(GetBuffer()))
		: static_cast<FVulkanViewableResource*>(ResourceCast(GetTexture()));
}

void FVulkanShaderResourceView::UpdateView()
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanSRVUpdateTime);
#endif

	Invalidate();

	if (IsBuffer())
	{
		FVulkanResourceMultiBuffer* Buffer = ResourceCast(GetBuffer());
		auto const Info = ViewDesc.Buffer.SRV.GetViewInfo(Buffer);

		if (!Info.bNullView)
		{
			switch (Info.BufferType)
			{
			case FRHIViewDesc::EBufferType::Raw:
			case FRHIViewDesc::EBufferType::Structured:
				InitAsStructuredBufferView(Buffer, Info.OffsetInBytes, Info.SizeInBytes);
				break;

			case FRHIViewDesc::EBufferType::Typed:
				check(VKHasAllFlags(Buffer->GetBufferUsageFlags(), VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT));
				InitAsTypedBufferView(Buffer, Info.Format, Info.OffsetInBytes, Info.SizeInBytes);
				break;

#if VULKAN_RHI_RAYTRACING
			case FRHIViewDesc::EBufferType::AccelerationStructure:
				InitAsAccelerationStructureView(Buffer, Info.OffsetInBytes, Info.SizeInBytes);
				break;
#endif

			default:
				checkNoEntry();
				break;
			}
		}
	}
	else
	{
		FVulkanTexture* Texture = ResourceCast(GetTexture());
		auto const Info = ViewDesc.Texture.SRV.GetViewInfo(Texture);

		uint32 ArrayFirst = Info.ArrayRange.First;
		uint32 ArrayNum = Info.ArrayRange.Num;
		if (Info.Dimension == FRHIViewDesc::EDimension::TextureCube || Info.Dimension == FRHIViewDesc::EDimension::TextureCubeArray)
		{
			ArrayFirst *= 6;
			ArrayNum *= 6;
			checkf((ArrayFirst + ArrayNum) <= Texture->GetNumberOfArrayLevels(), TEXT("View extends beyond original cube texture level count!"));
		}

		InitAsTextureView(
			  Texture->Image
			, GetVkImageViewTypeForDimensionSRV(Info.Dimension, Texture->GetViewType())
			, Texture->GetPartialAspectMask()
			, Info.Format
			, UEToVkTextureFormat(Info.Format, Info.bSRGB)
			, Info.MipRange.First
			, Info.MipRange.Num
			, ArrayFirst
			, ArrayNum
			, false
		);
	}
}



FVulkanUnorderedAccessView::FVulkanUnorderedAccessView(FRHICommandListBase& RHICmdList, FVulkanDevice& InDevice, FRHIViewableResource* InResource, FRHIViewDesc const& InViewDesc)
	: FRHIUnorderedAccessView(InResource, InViewDesc)
	, FVulkanLinkedView(InDevice, GetDescriptorTypeForViewDesc(InViewDesc))
{
	RHICmdList.EnqueueLambda([this](FRHICommandListBase&)
	{
		LinkHead(GetBaseResource()->LinkedViews);
		UpdateView();
	});
}

FVulkanViewableResource* FVulkanUnorderedAccessView::GetBaseResource() const
{
	return IsBuffer()
		? static_cast<FVulkanViewableResource*>(ResourceCast(GetBuffer()))
		: static_cast<FVulkanViewableResource*>(ResourceCast(GetTexture()));
}

void FVulkanUnorderedAccessView::UpdateView()
{
#if VULKAN_ENABLE_AGGRESSIVE_STATS
	SCOPE_CYCLE_COUNTER(STAT_VulkanUAVUpdateTime);
#endif

	Invalidate();

	if (IsBuffer())
	{
		FVulkanResourceMultiBuffer* Buffer = ResourceCast(GetBuffer());
		auto const Info = ViewDesc.Buffer.UAV.GetViewInfo(Buffer);

		checkf(!Info.bAppendBuffer && !Info.bAtomicCounter, TEXT("UAV counters not implemented in Vulkan RHI."));

		if (!Info.bNullView)
		{
			switch (Info.BufferType)
			{
			case FRHIViewDesc::EBufferType::Raw:
			case FRHIViewDesc::EBufferType::Structured:
				InitAsStructuredBufferView(Buffer, Info.OffsetInBytes, Info.SizeInBytes);
				break;

			case FRHIViewDesc::EBufferType::Typed:
				check(VKHasAllFlags(Buffer->GetBufferUsageFlags(), VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT));
				InitAsTypedBufferView(Buffer, Info.Format, Info.OffsetInBytes, Info.SizeInBytes);
				break;

#if VULKAN_RHI_RAYTRACING
			case FRHIViewDesc::EBufferType::AccelerationStructure:
				checkNoEntry(); // @todo implement
				break;
#endif

			default:
				checkNoEntry();
				break;
			}
		}
	}
	else
	{
		FVulkanTexture* Texture = ResourceCast(GetTexture());
		auto const Info = ViewDesc.Texture.UAV.GetViewInfo(Texture);

		uint32 ArrayFirst = Info.ArrayRange.First;
		uint32 ArrayNum = Info.ArrayRange.Num;
		if (Info.Dimension == FRHIViewDesc::EDimension::TextureCube || Info.Dimension == FRHIViewDesc::EDimension::TextureCubeArray)
		{
			ArrayFirst *= 6;
			ArrayNum *= 6;
			checkf((ArrayFirst + ArrayNum) <= Texture->GetNumberOfArrayLevels(), TEXT("View extends beyond original cube texture level count!"));
		}

		InitAsTextureView(
			  Texture->Image
			, GetVkImageViewTypeForDimensionUAV(Info.Dimension, Texture->GetViewType())
			, Texture->GetPartialAspectMask()
			, Info.Format
			, UEToVkTextureFormat(Info.Format, false)
			, Info.MipLevel
			, 1
			, ArrayFirst
			, ArrayNum
			, true
		);
	}
}

void FVulkanUnorderedAccessView::Clear(TRHICommandList_RecursiveHazardous<FVulkanCommandListContext>& RHICmdList, const void* ClearValue, bool bFloat)
{
	auto GetValueType = [&](EPixelFormat Format)
	{
		if (bFloat)
			return EClearReplacementValueType::Float;

		switch (Format)
		{
		case PF_R32_SINT:
		case PF_R16_SINT:
		case PF_R16G16B16A16_SINT:
			return EClearReplacementValueType::Int32;
		}

		return EClearReplacementValueType::Uint32;
	};

	if (IsBuffer())
	{
		FVulkanResourceMultiBuffer* Buffer = ResourceCast(GetBuffer());
		auto const Info = ViewDesc.Buffer.UAV.GetViewInfo(Buffer);

		switch (Info.BufferType)
		{
		case FRHIViewDesc::EBufferType::Raw:
		case FRHIViewDesc::EBufferType::Structured:
			RHICmdList.RunOnContext([this, Buffer, Info, ClearValue = *static_cast<const uint32*>(ClearValue)](FVulkanCommandListContext& Context)
			{
				FVulkanCmdBuffer* CmdBuffer = Context.GetCommandBufferManager()->GetActiveCmdBuffer();

				// vkCmdFillBuffer is treated as a transfer operation for the purposes of synchronization barriers.
				{
					FVulkanPipelineBarrier BeforeBarrier;
					BeforeBarrier.AddMemoryBarrier(VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
					BeforeBarrier.Execute(CmdBuffer);
				}

				VulkanRHI::vkCmdFillBuffer(
					  CmdBuffer->GetHandle()
					, Buffer->GetHandle()
					, Buffer->GetOffset() + Info.OffsetInBytes
					, Info.SizeInBytes
					, ClearValue
				);

				{
					FVulkanPipelineBarrier AfterBarrier;
					AfterBarrier.AddMemoryBarrier(VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
					AfterBarrier.Execute(CmdBuffer);
				}
			});
			break;

		case FRHIViewDesc::EBufferType::Typed:
			{
				const uint32 ComputeWorkGroupCount = FMath::DivideAndRoundUp(Info.NumElements, (uint32)ClearReplacementCS::TThreadGroupSize<EClearReplacementResourceType::Buffer>::X);

				FVulkanDevice* TargetDevice = FVulkanCommandListContext::GetVulkanContext(RHICmdList.GetContext()).GetDevice();
				const bool bOversizedBuffer = (ComputeWorkGroupCount > TargetDevice->GetLimits().maxComputeWorkGroupCount[0]);

				if (bOversizedBuffer)
				{
					ClearUAVShader_T<EClearReplacementResourceType::LargeBuffer, 4, false>(RHICmdList, this, Info.NumElements, 1, 1, ClearValue, GetValueType(Info.Format));
				}
				else
				{
					ClearUAVShader_T<EClearReplacementResourceType::Buffer, 4, false>(RHICmdList, this, Info.NumElements, 1, 1, ClearValue, GetValueType(Info.Format));
				}
			}
			break;

		default:
			checkNoEntry();
			break;
		}
	}
	else
	{
		FVulkanTexture* Texture = ResourceCast(GetTexture());
		auto const Info = ViewDesc.Texture.UAV.GetViewInfo(Texture);

		FIntVector SizeXYZ = Texture->GetMipDimensions(Info.MipLevel);

		switch (Texture->GetDesc().Dimension)
		{
		case ETextureDimension::Texture2D:
			ClearUAVShader_T<EClearReplacementResourceType::Texture2D, 4, false>(RHICmdList, this, SizeXYZ.X, SizeXYZ.Y, SizeXYZ.Z, ClearValue, GetValueType(Info.Format));
			break;

		case ETextureDimension::Texture2DArray:
			ClearUAVShader_T<EClearReplacementResourceType::Texture2DArray, 4, false>(RHICmdList, this, SizeXYZ.X, SizeXYZ.Y, Info.ArrayRange.Num, ClearValue, GetValueType(Info.Format));
			break;

		case ETextureDimension::TextureCube:
		case ETextureDimension::TextureCubeArray:
			ClearUAVShader_T<EClearReplacementResourceType::Texture2DArray, 4, false>(RHICmdList, this, SizeXYZ.X, SizeXYZ.Y, Info.ArrayRange.Num * 6, ClearValue, GetValueType(Info.Format));
			break;

		case ETextureDimension::Texture3D:
			ClearUAVShader_T<EClearReplacementResourceType::Texture3D, 4, false>(RHICmdList, this, SizeXYZ.X, SizeXYZ.Y, SizeXYZ.Z, ClearValue, GetValueType(Info.Format));
			break;

		default:
			checkNoEntry();
			break;
		}
	}
}

void FVulkanCommandListContext::RHIClearUAVFloat(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FVector4f& Values)
{
	TRHICommandList_RecursiveHazardous<FVulkanCommandListContext> RHICmdList(this);
	ResourceCast(UnorderedAccessViewRHI)->Clear(RHICmdList, &Values, true);
}

void FVulkanCommandListContext::RHIClearUAVUint(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FUintVector4& Values)
{
	TRHICommandList_RecursiveHazardous<FVulkanCommandListContext> RHICmdList(this);
	ResourceCast(UnorderedAccessViewRHI)->Clear(RHICmdList, &Values, false);
}

FShaderResourceViewRHIRef  FVulkanDynamicRHI::RHICreateShaderResourceView(class FRHICommandListBase& RHICmdList, FRHIViewableResource* Resource, FRHIViewDesc const& ViewDesc)
{
	return new FVulkanShaderResourceView(RHICmdList, *Device, Resource, ViewDesc);
}

FUnorderedAccessViewRHIRef FVulkanDynamicRHI::RHICreateUnorderedAccessView(class FRHICommandListBase& RHICmdList, FRHIViewableResource* Resource, FRHIViewDesc const& ViewDesc)
{
	return new FVulkanUnorderedAccessView(RHICmdList, *Device, Resource, ViewDesc);
}
