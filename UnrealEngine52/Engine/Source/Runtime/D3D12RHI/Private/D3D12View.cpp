// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FD3D12ViewDescriptorHandle

FD3D12ViewDescriptorHandle::FD3D12ViewDescriptorHandle(FD3D12Device* InParentDevice, ERHIDescriptorHeapType InHeapType)
	: FD3D12DeviceChild(InParentDevice)
	, HeapType(InHeapType)
{
	AllocateDescriptorSlot();
}

FD3D12ViewDescriptorHandle::~FD3D12ViewDescriptorHandle()
{
	FreeDescriptorSlot();
	check(OfflineCpuHandle.ptr == 0);
	check(OfflineHeapIndex == UINT_MAX);
	check(!BindlessHandle.IsValid());
}

void FD3D12ViewDescriptorHandle::SetParentDevice(FD3D12Device* InParent)
{
	check(Parent == nullptr);
	Parent = InParent;

	AllocateDescriptorSlot();
}

void FD3D12ViewDescriptorHandle::CreateView(const D3D12_RENDER_TARGET_VIEW_DESC& Desc, ID3D12Resource* Resource)
{
	check(HeapType == ERHIDescriptorHeapType::RenderTarget);

	GetParentDevice()->GetDevice()->CreateRenderTargetView(Resource, &Desc, OfflineCpuHandle);
}

void FD3D12ViewDescriptorHandle::CreateView(const D3D12_DEPTH_STENCIL_VIEW_DESC& Desc, ID3D12Resource* Resource)
{
	check(HeapType == ERHIDescriptorHeapType::DepthStencil);

	GetParentDevice()->GetDevice()->CreateDepthStencilView(Resource, &Desc, OfflineCpuHandle);
}

void FD3D12ViewDescriptorHandle::CreateView(const D3D12_CONSTANT_BUFFER_VIEW_DESC& Desc)
{
	check(HeapType == ERHIDescriptorHeapType::Standard);

	GetParentDevice()->GetDevice()->CreateConstantBufferView(&Desc, OfflineCpuHandle);
}

void FD3D12ViewDescriptorHandle::CreateView(const D3D12_SHADER_RESOURCE_VIEW_DESC& Desc, ID3D12Resource* Resource, ED3D12DescriptorCreateReason Reason)
{
	check(HeapType == ERHIDescriptorHeapType::Standard);

#if D3D12_RHI_RAYTRACING
	// NOTE (from D3D Debug runtime): When ViewDimension is D3D12_SRV_DIMENSION_RAYTRACING_ACCELLERATION_STRUCTURE, pResource must be NULL, since the resource location comes from a GPUVA in pDesc
	if (Desc.ViewDimension == D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE)
	{
		Resource = nullptr;
	}
#endif // D3D12_RHI_RAYTRACING

	GetParentDevice()->GetDevice()->CreateShaderResourceView(Resource, &Desc, OfflineCpuHandle);

	UpdateBindlessSlot(Reason);
}

void FD3D12ViewDescriptorHandle::CreateView(const D3D12_UNORDERED_ACCESS_VIEW_DESC& Desc, ID3D12Resource* Resource, ID3D12Resource* CounterResource, ED3D12DescriptorCreateReason Reason)
{
	check(HeapType == ERHIDescriptorHeapType::Standard);

	GetParentDevice()->GetDevice()->CreateUnorderedAccessView(Resource, CounterResource, &Desc, OfflineCpuHandle);

	UpdateBindlessSlot(Reason);
}

void FD3D12ViewDescriptorHandle::AllocateDescriptorSlot()
{
	if (FD3D12Device* Device = GetParentDevice_Unsafe())
	{
		OfflineCpuHandle = Device->GetOfflineDescriptorManager(HeapType).AllocateHeapSlot(OfflineHeapIndex);
		check(OfflineCpuHandle.ptr != 0);

		if (HeapType == ERHIDescriptorHeapType::Standard)
		{
			BindlessHandle = Device->GetBindlessDescriptorManager().Allocate(ERHIDescriptorHeapType::Standard);
		}
	}
}

void FD3D12ViewDescriptorHandle::FreeDescriptorSlot()
{
	if (FD3D12Device* Device = GetParentDevice_Unsafe())
	{
		Device->GetOfflineDescriptorManager(HeapType).FreeHeapSlot(OfflineCpuHandle, OfflineHeapIndex);
		OfflineHeapIndex = UINT_MAX;
		OfflineCpuHandle.ptr = 0;

		if (BindlessHandle.IsValid())
		{
			Device->GetBindlessDescriptorManager().DeferredFreeFromDestructor(BindlessHandle);
			BindlessHandle = FRHIDescriptorHandle();
		}
	}
	check(OfflineCpuHandle.ptr == 0);
}

void FD3D12ViewDescriptorHandle::UpdateBindlessSlot(ED3D12DescriptorCreateReason Reason)
{
	if (BindlessHandle.IsValid())
	{
		FD3D12BindlessDescriptorManager& BindlessManager = GetParentDevice()->GetBindlessDescriptorManager();
		if (Reason == ED3D12DescriptorCreateReason::InitialCreate)
		{
			BindlessManager.UpdateImmediately(BindlessHandle, GetOfflineCpuHandle());
		}
		else
		{
			BindlessManager.UpdateDeferred(BindlessHandle, GetOfflineCpuHandle());
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FD3D12ShaderResourceView::FD3D12ShaderResourceView(FD3D12Device* InParent)
	: FRHIShaderResourceView(nullptr)
	, FD3D12View(InParent, ERHIDescriptorHeapType::Standard, ViewSubresourceSubsetFlags_None)
	, bContainsDepthPlane(false)
	, bContainsStencilPlane(false)
	, bSkipFastClearFinalize(false)
	, bRequiresResourceStateTracking(false)
{
}

FD3D12ShaderResourceView::FD3D12ShaderResourceView(FD3D12Buffer* InBuffer, const D3D12_SHADER_RESOURCE_VIEW_DESC& InDesc, uint32 InStride, uint32 InStartOffsetBytes)
	: FD3D12ShaderResourceView(InBuffer->GetParentDevice())
{
	InitializeAfterCreate(InDesc, InBuffer, InBuffer->ResourceLocation, InStride, InStartOffsetBytes);
}

FD3D12ShaderResourceView::FD3D12ShaderResourceView(FD3D12Texture* InTexture, const D3D12_SHADER_RESOURCE_VIEW_DESC& InDesc, ETextureCreateFlags InTextureCreateFlags)
	: FD3D12ShaderResourceView(InTexture->GetParentDevice())
{
	InitializeAfterCreate(InDesc, InTexture, InTexture->ResourceLocation, -1, 0, EnumHasAnyFlags(InTextureCreateFlags, ETextureCreateFlags::NoFastClearFinalize));
}

FD3D12ShaderResourceView::~FD3D12ShaderResourceView()
{
}

void FD3D12ShaderResourceView::InitializeAfterCreate(const D3D12_SHADER_RESOURCE_VIEW_DESC& InDesc, FD3D12BaseShaderResource* InBaseShaderResource, FD3D12ResourceLocation& InResourceLocation, uint32 InStride, uint32 InStartOffsetBytes, bool InSkipFastClearFinalize)
{
	SetDesc(InDesc);

	PreCreateView(InBaseShaderResource->ResourceLocation, InStride, InStartOffsetBytes, InSkipFastClearFinalize);
	CreateView(InBaseShaderResource, InBaseShaderResource->ResourceLocation, ED3D12DescriptorCreateReason::InitialCreate);
}

void FD3D12ShaderResourceView::Update(FD3D12Buffer* InBuffer, const D3D12_SHADER_RESOURCE_VIEW_DESC& InDesc, uint32 InStride)
{
	FD3D12Device* InParent = InBuffer->GetParentDevice();
	if (!this->GetParentDevice_Unsafe())
	{
		// This is a null SRV created without viewing on any resource
		// We need to set its device and allocate a descriptor slot before moving forward
		this->SetParentDevice(InParent);
	}
	check(GetParentDevice() == InParent);

	SetDesc(InDesc);

	PreCreateView(InBuffer->ResourceLocation, InStride, 0, false);
	CreateView(InBuffer, InBuffer->ResourceLocation, ED3D12DescriptorCreateReason::UpdateOrRename);
}

void FD3D12ShaderResourceView::UpdateMinLODClamp(float ResourceMinLODClamp)
{
	check(bInitialized);
	check(ResourceLocation);
	check(Desc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D);

	// Update the LODClamp, the reinitialize the SRV
	Desc.Texture2D.ResourceMinLODClamp = ResourceMinLODClamp;
	CreateView(BaseShaderResource, *ResourceLocation, ED3D12DescriptorCreateReason::UpdateOrRename);
}

void FD3D12ShaderResourceView::RecreateView()
{
	// Update the first element index, then reinitialize the SRV
	if (Desc.ViewDimension == D3D12_SRV_DIMENSION_BUFFER)
	{
		uint32 StartElement = StartOffsetBytes / Stride;
		Desc.Buffer.FirstElement = ResourceLocation->GetOffsetFromBaseOfResource() / Stride + StartElement;
	}

	PreCreateView(BaseShaderResource->ResourceLocation, Stride, StartOffsetBytes, bSkipFastClearFinalize);
	CreateView(BaseShaderResource, BaseShaderResource->ResourceLocation, ED3D12DescriptorCreateReason::UpdateOrRename);
}

void FD3D12ShaderResourceView::PreCreateView(const FD3D12ResourceLocation& InResourceLocation, uint32 InStride, uint32 InStartOffsetBytes, bool InSkipFastClearFinalize)
{
	Stride = InStride;
	StartOffsetBytes = InStartOffsetBytes;
	bSkipFastClearFinalize = InSkipFastClearFinalize;

	if (FD3D12Resource* NewResource = InResourceLocation.GetResource())
	{
		bContainsDepthPlane = NewResource->IsDepthStencilResource() && GetPlaneSliceFromViewFormat(NewResource->GetDesc().Format, Desc.Format) == 0;
		bContainsStencilPlane = NewResource->IsDepthStencilResource() && GetPlaneSliceFromViewFormat(NewResource->GetDesc().Format, Desc.Format) == 1;
		bRequiresResourceStateTracking = NewResource->RequiresResourceStateTracking();

#if DO_CHECK
		// Check the plane slice of the SRV matches the texture format
		// Texture2DMS does not have explicit plane index (it's implied by the format)
		if (Desc.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D)
		{
			check(GetPlaneSliceFromViewFormat(NewResource->GetDesc().Format, Desc.Format) == Desc.Texture2D.PlaneSlice);
		}
#endif
	}
	else
	{
		bContainsDepthPlane = false;
		bContainsStencilPlane = false;
		bRequiresResourceStateTracking = false;
	}
}

void FD3D12ShaderResourceView::CreateView(FD3D12BaseShaderResource* InBaseShaderResource, FD3D12ResourceLocation& InResourceLocation, ED3D12DescriptorCreateReason Reason)
{
	InitializeInternal(InBaseShaderResource, InResourceLocation);

	if (ResourceLocation->GetResource())
	{
		ID3D12Resource* D3DResource = ResourceLocation->GetResource()->GetResource();
		Descriptor.CreateView(Desc, D3DResource, Reason);
	}
}

static D3D12_SHADER_RESOURCE_VIEW_DESC GetRawBufferSRVDesc(FD3D12Buffer* Buffer, uint32 StartOffsetBytes, uint32 NumElements)
{
	const uint32 Stride = 4;
	checkf(StartOffsetBytes % Stride == 0, TEXT("Raw buffer offset must be DWORD-aligned"));

	const uint32 Width = Buffer->GetSize();
	const uint32 MaxElements = Width / Stride;
	const uint32 StartElement = FMath::Min(StartOffsetBytes, Width) / Stride;
	const FD3D12ResourceLocation& Location = Buffer->ResourceLocation;

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	SRVDesc.Buffer.NumElements = FMath::Min(MaxElements - StartElement, NumElements);

	if (Location.GetResource())
	{
		// Create a Shader Resource View
		SRVDesc.Buffer.FirstElement = Location.GetOffsetFromBaseOfResource() / 4 + StartElement;
	}
	else
	{
		// Null underlying D3D12 resource should only be the case for dynamic resources
		check(EnumHasAnyFlags(Buffer->GetUsage(), BUF_AnyDynamic));
	}

	return SRVDesc;
}

static FORCEINLINE D3D12_SHADER_RESOURCE_VIEW_DESC GetVertexBufferSRVDesc(const FD3D12Buffer* VertexBuffer, uint32& CreationStride, uint8 Format, uint32 StartOffsetBytes, uint32 NumElements)
{
	const uint32 BufferSize = VertexBuffer->GetSize();
	const uint64 BufferOffset = VertexBuffer->ResourceLocation.GetOffsetFromBaseOfResource();

	const uint32 FormatStride = GPixelFormats[Format].BlockBytes;

	const uint32 NumRequestedBytes = NumElements * FormatStride;
	const uint32 OffsetBytes = FMath::Min(StartOffsetBytes, BufferSize);
	const uint32 NumBytes = FMath::Min(NumRequestedBytes, BufferSize - OffsetBytes);

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc{};

	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

	if (EnumHasAnyFlags(VertexBuffer->GetUsage(), EBufferUsageFlags::ByteAddressBuffer))
	{
		SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		SRVDesc.Buffer.Flags |= D3D12_BUFFER_SRV_FLAG_RAW;
		CreationStride = 4;
	}
	else
	{
		SRVDesc.Format = FindShaderResourceDXGIFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat, false);
		CreationStride = GPixelFormats[Format].BlockBytes;
	}

	SRVDesc.Buffer.FirstElement = (BufferOffset + OffsetBytes) / CreationStride;
	SRVDesc.Buffer.NumElements = NumBytes / CreationStride;

	return SRVDesc;
}

static FORCEINLINE D3D12_SHADER_RESOURCE_VIEW_DESC GetStructuredBufferSRVDesc(const FD3D12Buffer* StructuredBuffer, uint32& CreationStride, uint8 Format, uint32 StartOffsetBytes, uint32 NumElements)
{
	const uint32 BufferSize = StructuredBuffer->ResourceLocation.GetSize();
	const uint32 BufferStride = StructuredBuffer->GetStride();
	const uint32 BufferOffset = StructuredBuffer->ResourceLocation.GetOffsetFromBaseOfResource();

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc{};

	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

	if (EnumHasAnyFlags(StructuredBuffer->GetUsage(), EBufferUsageFlags::ByteAddressBuffer))
	{
		SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		SRVDesc.Buffer.Flags |= D3D12_BUFFER_SRV_FLAG_RAW;
		CreationStride = 4;
	}
	else
	{
		SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		SRVDesc.Buffer.StructureByteStride = BufferStride;
		CreationStride = BufferStride;
	}

	const uint32 MaxElements = BufferSize / CreationStride;
	const uint32 StartElement = FMath::Min<uint32>(StartOffsetBytes, BufferSize) / CreationStride;

	SRVDesc.Buffer.NumElements = FMath::Min<uint32>(MaxElements - StartElement, NumElements);
	SRVDesc.Buffer.FirstElement = (BufferOffset / CreationStride) + StartElement;

	return SRVDesc;
}

static FORCEINLINE D3D12_SHADER_RESOURCE_VIEW_DESC GetIndexBufferSRVDesc(FD3D12Buffer* IndexBuffer, uint32 StartOffsetBytes, uint32 NumElements)
{
	const EBufferUsageFlags Usage = IndexBuffer->GetUsage();
	const uint32 Width = IndexBuffer->GetSize();
	const uint32 CreationStride = IndexBuffer->GetStride();
	const uint32 MaxElements = Width / CreationStride;
	const uint32 StartElement = FMath::Min(StartOffsetBytes, Width) / CreationStride;
	const FD3D12ResourceLocation& Location = IndexBuffer->ResourceLocation;

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc{};
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

	if (EnumHasAnyFlags(Usage, EBufferUsageFlags::ByteAddressBuffer))
	{
		check(CreationStride == 4u);
		SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
	}
	else
	{
		check(CreationStride == 2u || CreationStride == 4u);
		SRVDesc.Format = CreationStride == 2u ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	}
	SRVDesc.Buffer.NumElements = FMath::Min(MaxElements - StartElement, NumElements);

	if (Location.GetResource())
	{
		// Create a Shader Resource View
		SRVDesc.Buffer.FirstElement = Location.GetOffsetFromBaseOfResource() / CreationStride + StartElement;
	}
	else
	{
		// Null underlying D3D12 resource should only be the case for dynamic resources
		check(EnumHasAnyFlags(Usage, BUF_AnyDynamic));
	}
	return SRVDesc;
}

template<typename TextureType>
FD3D12ShaderResourceView* CreateSRV(TextureType* Texture, const D3D12_SHADER_RESOURCE_VIEW_DESC& Desc)
{
	if (Texture == nullptr)
	{
		return nullptr;
	}

	FD3D12Adapter* Adapter = Texture->GetParentDevice()->GetParentAdapter();

	return Adapter->CreateLinkedViews<TextureType, FD3D12ShaderResourceView>(Texture, [&Desc](TextureType* Texture)
	{
		return new FD3D12ShaderResourceView(Texture, Desc);
	});
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView(FRHITexture* RHITexture, const FRHITextureSRVCreateInfo& CreateInfo)
{
	FD3D12Texture* Texture = GetD3D12TextureFromRHITexture(RHITexture);
	ETextureDimension Dimension = RHITexture->GetDesc().Dimension;

	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	const D3D12_RESOURCE_DESC& TextureDesc = Texture->GetResource()->GetDesc();
	DXGI_FORMAT BaseTextureFormat = TextureDesc.Format;

	switch (Dimension)
	{
	case ETextureDimension::Texture3D:
	{
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		SRVDesc.Texture3D.MipLevels = CreateInfo.NumMipLevels;
		SRVDesc.Texture3D.MostDetailedMip = CreateInfo.MipLevel;
		break;
	}
	case ETextureDimension::Texture2DArray:
	{
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		SRVDesc.Texture2DArray.ArraySize = (CreateInfo.NumArraySlices == 0 ? TextureDesc.DepthOrArraySize : CreateInfo.NumArraySlices);
		SRVDesc.Texture2DArray.FirstArraySlice = CreateInfo.FirstArraySlice;
		SRVDesc.Texture2DArray.MipLevels = CreateInfo.NumMipLevels;
		SRVDesc.Texture2DArray.MostDetailedMip = CreateInfo.MipLevel;
		break;
	}
	case ETextureDimension::TextureCube:
	{
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		SRVDesc.TextureCube.MipLevels = CreateInfo.NumMipLevels;
		SRVDesc.TextureCube.MostDetailedMip = CreateInfo.MipLevel;
		break;
	}
	case ETextureDimension::TextureCubeArray:
	{
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
		SRVDesc.TextureCubeArray.First2DArrayFace = CreateInfo.FirstArraySlice;
		SRVDesc.TextureCubeArray.NumCubes = (CreateInfo.NumArraySlices == 0 ? TextureDesc.DepthOrArraySize / 6 : CreateInfo.NumArraySlices / 6);
		SRVDesc.TextureCubeArray.MipLevels = CreateInfo.NumMipLevels;
		SRVDesc.TextureCubeArray.MostDetailedMip = CreateInfo.MipLevel;
		break;
	}
	default:
	{		
		if (TextureDesc.SampleDesc.Count > 1)
		{
			// MS textures can't have mips apparently, so nothing else to set.
			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
		}
		else
		{
			SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			SRVDesc.Texture2D.MipLevels = CreateInfo.NumMipLevels;
			SRVDesc.Texture2D.MostDetailedMip = CreateInfo.MipLevel;
		}
		break;
	}
	}

	// Allow input CreateInfo to override SRGB and/or format
	const bool bBaseSRGB = EnumHasAnyFlags(RHITexture->GetFlags(), TexCreate_SRGB);
	const bool bSRGB = CreateInfo.SRGBOverride != SRGBO_ForceDisable && bBaseSRGB;
	const DXGI_FORMAT ViewTextureFormat = (CreateInfo.Format == PF_Unknown) ? BaseTextureFormat : (DXGI_FORMAT)GPixelFormats[CreateInfo.Format].PlatformFormat;
	SRVDesc.Format = FindShaderResourceDXGIFormat(ViewTextureFormat, bSRGB);

	switch (SRVDesc.ViewDimension)
	{
	case D3D12_SRV_DIMENSION_TEXTURE2D: SRVDesc.Texture2D.PlaneSlice = GetPlaneSliceFromViewFormat(BaseTextureFormat, SRVDesc.Format); break;
	case D3D12_SRV_DIMENSION_TEXTURE2DARRAY: SRVDesc.Texture2DArray.PlaneSlice = GetPlaneSliceFromViewFormat(BaseTextureFormat, SRVDesc.Format); break;
	default: break; // other view types don't support PlaneSlice
	}

	check(Texture);
	return CreateSRV(Texture, SRVDesc);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView(FRHIBuffer* BufferRHI)
{
	return FD3D12DynamicRHI::RHICreateShaderResourceView(FShaderResourceViewInitializer(BufferRHI));
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView(FRHIBuffer* BufferRHI, uint32 Stride, uint8 Format)
{
	ensureMsgf(Stride == GPixelFormats[Format].BlockBytes, TEXT("provided stride: %i was not consitent with Pixelformat: %s"), Stride, GPixelFormats[Format].Name);
	return FD3D12DynamicRHI::RHICreateShaderResourceView(FShaderResourceViewInitializer(BufferRHI, EPixelFormat(Format)));
}

uint64 FD3D12DynamicRHI::RHIGetMinimumAlignmentForBufferBackedSRV(EPixelFormat Format)
{
	return GPixelFormats[Format].BlockBytes;
}

FRHICOMMAND_MACRO(FD3D12InitializeBufferSRVRHICommand)
{
	const FShaderResourceViewInitializer Initializer;
	FD3D12ShaderResourceView* const View;
	uint32 GPUIndex;

	FD3D12InitializeBufferSRVRHICommand(const FShaderResourceViewInitializer& InInitializer, FD3D12ShaderResourceView* InView, uint32 InGPUIndex)
		: Initializer(InInitializer)
		, View(InView)
		, GPUIndex(InGPUIndex)
	{
	}

	void Execute(FRHICommandListBase& RHICmdList)
	{
		const FShaderResourceViewInitializer::FBufferShaderResourceViewInitializer& Desc = Initializer.AsBufferSRV();
		FD3D12Buffer* const Buffer = FD3D12DynamicRHI::ResourceCast(Desc.Buffer, GPUIndex);

		if (Initializer.GetType() == FShaderResourceViewInitializer::EType::VertexBufferSRV)
		{
			uint32 CreationStride = 0;
			const D3D12_SHADER_RESOURCE_VIEW_DESC ViewDesc = GetVertexBufferSRVDesc(Buffer, CreationStride, Desc.Format, Desc.StartOffsetBytes, Desc.NumElements);

			View->InitializeAfterCreate(ViewDesc, Buffer, Buffer->ResourceLocation, CreationStride, Desc.StartOffsetBytes);
		}
		else if (Initializer.GetType() == FShaderResourceViewInitializer::EType::StructuredBufferSRV)
		{
			uint32 CreationStride = 0;
			const D3D12_SHADER_RESOURCE_VIEW_DESC ViewDesc = GetStructuredBufferSRVDesc(Buffer, CreationStride, Desc.Format, Desc.StartOffsetBytes, Desc.NumElements);

			View->InitializeAfterCreate(ViewDesc, Buffer, Buffer->ResourceLocation, CreationStride, Desc.StartOffsetBytes);
		}
		else
		{
			checkNoEntry();
		}
	}
};

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView(const FShaderResourceViewInitializer& Initializer)
{
	FShaderResourceViewInitializer::FBufferShaderResourceViewInitializer Desc = Initializer.AsBufferSRV();

	if (!Desc.Buffer)
	{
		return GetAdapter().CreateLinkedObject<FD3D12ShaderResourceView>(FRHIGPUMask::All(), [](FD3D12Device* Device)
			{
				return new FD3D12ShaderResourceView(nullptr);
			});
	}

	FD3D12Buffer* Buffer = FD3D12DynamicRHI::ResourceCast(Desc.Buffer);

	switch (Initializer.GetType())
	{
		case FShaderResourceViewInitializer::EType::VertexBufferSRV:
		case FShaderResourceViewInitializer::EType::StructuredBufferSRV:
			return GetAdapter().CreateLinkedViews<FD3D12Buffer, FD3D12ShaderResourceView>(Buffer,
				[Initializer](FD3D12Buffer* Buffer)
				{
					check(Buffer);

					FD3D12ShaderResourceView* ShaderResourceView = new FD3D12ShaderResourceView(Buffer->GetParentDevice());
					
					FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
					if (ShouldDeferBufferLockOperation(&RHICmdList) && EnumHasAnyFlags(Buffer->GetUsage(), EBufferUsageFlags::AnyDynamic))
					{
						// We have to defer the SRV initialization to the RHI thread if the buffer is dynamic (and RHI threading is enabled), as dynamic buffers can be renamed.
						// Also insert an RHI thread fence to prevent parallel translate tasks running until this command has completed.
						ALLOC_COMMAND_CL(RHICmdList, FD3D12InitializeBufferSRVRHICommand)(Initializer, ShaderResourceView, Buffer->GetParentGPUIndex());
						RHICmdList.RHIThreadFence(true);
					}
					else
					{
						// Run the command directly if we're bypassing RHI command list recording, or the buffer is not dynamic.
						FD3D12InitializeBufferSRVRHICommand Command(Initializer, ShaderResourceView, Buffer->GetParentGPUIndex());
						Command.Execute(RHICmdList);
					}

					return ShaderResourceView;
				});

		case FShaderResourceViewInitializer::EType::IndexBufferSRV:
			return GetAdapter().CreateLinkedViews<FD3D12Buffer, FD3D12ShaderResourceView>(Buffer,
				[Desc](FD3D12Buffer* Buffer)
				{
					check(Buffer);

					const uint32 CreationStride = Buffer->GetStride();
					const D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = GetIndexBufferSRVDesc(Buffer, Desc.StartOffsetBytes, Desc.NumElements);

					FD3D12ShaderResourceView* ShaderResourceView = new FD3D12ShaderResourceView(Buffer, SRVDesc, CreationStride);
					return ShaderResourceView;
				});

		case FShaderResourceViewInitializer::EType::RawBufferSRV:
			return GetAdapter().CreateLinkedViews<FD3D12Buffer, FD3D12ShaderResourceView>(Buffer,
				[Desc](FD3D12Buffer* Buffer)
				{
					check(Buffer);
					const D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = GetRawBufferSRVDesc(Buffer, Desc.StartOffsetBytes, Desc.NumElements);
					FD3D12ShaderResourceView* ShaderResourceView = new FD3D12ShaderResourceView(Buffer, SRVDesc, 4);
					return ShaderResourceView;
				});

	#if D3D12_RHI_RAYTRACING
		case FShaderResourceViewInitializer::EType::AccelerationStructureSRV:
			return GetAdapter().CreateLinkedViews<FD3D12Buffer, FD3D12ShaderResourceView>(Buffer,
				[Desc](FD3D12Buffer* Buffer)
				{
					check(Buffer);

					D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};

					SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
					SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
					SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					SRVDesc.RaytracingAccelerationStructure.Location = Buffer->ResourceLocation.GetGPUVirtualAddress() + Desc.StartOffsetBytes;

					FD3D12ShaderResourceView* ShaderResourceView = new FD3D12ShaderResourceView(Buffer, SRVDesc, 4);
					return ShaderResourceView;
				});
	#endif // D3D12_RHI_RAYTRACING
	}

	checkNoEntry();
	return nullptr;
}

void FD3D12DynamicRHI::RHIUpdateShaderResourceView(FRHIShaderResourceView* SRV, FRHIBuffer* BufferRHI, uint32 Stride, uint8 Format)
{
	check(SRV);
	if (BufferRHI)
	{
		FD3D12Buffer* Buffer = ResourceCast(BufferRHI);
		FD3D12ShaderResourceView* SRVD3D12 = ResourceCast(SRV);
		const D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = GetVertexBufferSRVDesc(Buffer, Stride, Format, 0, UINT32_MAX);

		// Rename the SRV to view on the new vertex buffer
		for (auto It = MakeDualLinkedObjectIterator(Buffer, SRVD3D12); It; ++It)
		{
			Buffer = It.GetFirst();
			SRVD3D12 = It.GetSecond();

			SRVD3D12->Update(Buffer, SRVDesc, Stride);
		}
	}
}

void FD3D12DynamicRHI::RHIUpdateShaderResourceView(FRHIShaderResourceView* SRV, FRHIBuffer* BufferRHI)
{
	check(SRV);
	if (BufferRHI)
	{
		FD3D12Buffer* Buffer = ResourceCast(BufferRHI);
		FD3D12ShaderResourceView* SRVD3D12 = ResourceCast(SRV);
		const D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = GetIndexBufferSRVDesc(Buffer, 0, UINT32_MAX);
		const uint32 Stride = Buffer->GetStride();

		// Rename the SRV to view on the new index buffer
		for (auto It = MakeDualLinkedObjectIterator(Buffer, SRVD3D12); It; ++It)
		{
			Buffer = It.GetFirst();
			SRVD3D12 = It.GetSecond();

			SRVD3D12->Update(Buffer, SRVDesc, Stride);
		}
	}
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView_RenderThread(FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, const FRHITextureSRVCreateInfo& CreateInfo)
{
	return RHICreateShaderResourceView(Texture, CreateInfo);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView_RenderThread(FRHICommandListImmediate& RHICmdList, FRHIBuffer* BufferRHI, uint32 Stride, uint8 Format)
{
	return RHICreateShaderResourceView(BufferRHI, Stride, Format);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView_RenderThread(FRHICommandListImmediate& RHICmdList, const FShaderResourceViewInitializer& Initializer)
{
	return RHICreateShaderResourceView(Initializer);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::CreateShaderResourceView_RenderThread(FRHICommandListImmediate& RHICmdList, FRHIBuffer* BufferRHI, uint32 Stride, uint8 Format)
{
	return RHICreateShaderResourceView_RenderThread(RHICmdList, BufferRHI, Stride, Format);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::CreateShaderResourceView_RenderThread(FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer)
{
	return RHICreateShaderResourceView(Buffer);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::CreateShaderResourceView_RenderThread(FRHICommandListImmediate& RHICmdList, const FShaderResourceViewInitializer& Initializer)
{
	return RHICreateShaderResourceView_RenderThread(RHICmdList, Initializer);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceView_RenderThread(FRHICommandListImmediate& RHICmdList, FRHIBuffer* BufferRHI)
{
	return RHICreateShaderResourceView(BufferRHI);
}

FShaderResourceViewRHIRef FD3D12DynamicRHI::RHICreateShaderResourceViewWriteMask_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D)
{
	return RHICreateShaderResourceViewWriteMask(Texture2D);
}

FD3D12ConstantBufferView::FD3D12ConstantBufferView(FD3D12Device* InParent)
	: FD3D12DeviceChild(InParent)
	, Descriptor(InParent, ERHIDescriptorHeapType::Standard)
{
}

void FD3D12ConstantBufferView::Create(D3D12_GPU_VIRTUAL_ADDRESS GPUAddress, const uint32 AlignedSize)
{
	Desc.BufferLocation = GPUAddress;
	Desc.SizeInBytes = AlignedSize;
	Descriptor.CreateView(Desc);
}
