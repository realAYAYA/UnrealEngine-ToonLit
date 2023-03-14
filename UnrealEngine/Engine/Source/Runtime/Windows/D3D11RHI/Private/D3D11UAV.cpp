// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D11RHIPrivate.h"
#include "ClearReplacementShaders.h"

static D3D11_UNORDERED_ACCESS_VIEW_DESC InitializeCommonUAVDesc(const D3D11_BUFFER_DESC& BufferDesc, uint32 Offset, uint32 Size, EPixelFormat Format, bool bForStructured)
{
	const bool bByteAccessBuffer = (BufferDesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS) != 0;

	D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc{};
	UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;

	uint32 EffectiveStride;
	if (bByteAccessBuffer)
	{
		UAVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		UAVDesc.Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_RAW;
		EffectiveStride = 4;
	}
	else if (bForStructured)
	{
		UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
		// For byte access buffers and indirect draw argument buffers, GetDesc returns a StructureByteStride of 0 even though we created it with 4
		EffectiveStride = BufferDesc.StructureByteStride == 0 ? 4 : BufferDesc.StructureByteStride;
	}
	else
	{
		UAVDesc.Format = FindUnorderedAccessDXGIFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat);
		EffectiveStride = GPixelFormats[Format].BlockBytes;
	}

	UAVDesc.Buffer.FirstElement = Offset / EffectiveStride;
	UAVDesc.Buffer.NumElements = Size / EffectiveStride;

	return UAVDesc;
}

FUnorderedAccessViewRHIRef FD3D11DynamicRHI::RHICreateUnorderedAccessView(FRHIBuffer* BufferRHI, bool bUseUAVCounter, bool bAppendBuffer)
{
	FD3D11Buffer* Buffer = ResourceCast(BufferRHI);

	D3D11_BUFFER_DESC BufferDesc;
	Buffer->Resource->GetDesc(&BufferDesc);

	D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = InitializeCommonUAVDesc(BufferDesc, 0, BufferDesc.ByteWidth, PF_Unknown, true);

	if (BufferDesc.MiscFlags & D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS)
	{
		UAVDesc.Format = DXGI_FORMAT_R32_UINT;
	}
		
	if (bUseUAVCounter)
	{
		UAVDesc.Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_COUNTER;
	}

	if (bAppendBuffer)
	{
		UAVDesc.Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_APPEND;
	}

	TRefCountPtr<ID3D11UnorderedAccessView> UnorderedAccessView;
	VERIFYD3D11CREATEVIEWRESULT(Direct3DDevice->CreateUnorderedAccessView(Buffer->Resource, &UAVDesc, (ID3D11UnorderedAccessView**)UnorderedAccessView.GetInitReference()), Direct3DDevice, BufferRHI, UAVDesc);

	return new FD3D11UnorderedAccessView(UnorderedAccessView, Buffer, Buffer);
}

FUnorderedAccessViewRHIRef FD3D11DynamicRHI::RHICreateUnorderedAccessView_RenderThread(
	class FRHICommandListImmediate& RHICmdList,
	FRHIBuffer* Buffer,
	bool bUseUAVCounter,
	bool bAppendBuffer)
{
	return RHICreateUnorderedAccessView(Buffer, bUseUAVCounter, bAppendBuffer);
}

FUnorderedAccessViewRHIRef FD3D11DynamicRHI::RHICreateUnorderedAccessView(FRHITexture* TextureRHI, uint32 MipLevel, uint16 FirstArraySlice, uint16 NumArraySlices)
{
	FD3D11Texture* Texture = GetD3D11TextureFromRHITexture(TextureRHI);
	
	D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc{};

	if (TextureRHI->GetTexture3D() != NULL)
	{
		UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		UAVDesc.Texture3D.MipSlice = MipLevel;
		UAVDesc.Texture3D.FirstWSlice = 0;
		UAVDesc.Texture3D.WSize = Texture->GetDesc().Depth >> MipLevel;
	}
	else if (TextureRHI->GetTexture2DArray() != NULL)
	{
		UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		UAVDesc.Texture2DArray.MipSlice = MipLevel;
		UAVDesc.Texture2DArray.FirstArraySlice = NumArraySlices == 0 ? 0 : FirstArraySlice;
		UAVDesc.Texture2DArray.ArraySize = NumArraySlices == 0 ? Texture->GetDesc().ArraySize : NumArraySlices;
	}
	else if (TextureRHI->GetTextureCube() != NULL)
	{
		UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		UAVDesc.Texture2DArray.MipSlice = MipLevel;
		UAVDesc.Texture2DArray.FirstArraySlice = NumArraySlices == 0 ? 0 : FirstArraySlice * 6;
		UAVDesc.Texture2DArray.ArraySize = NumArraySlices == 0 ? Texture->GetDesc().ArraySize * 6 : NumArraySlices * 6;
	}
	else
	{
		UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		UAVDesc.Texture2D.MipSlice = MipLevel;
	}
	
	UAVDesc.Format = FindShaderResourceDXGIFormat((DXGI_FORMAT)GPixelFormats[TextureRHI->GetFormat()].PlatformFormat, false);

	TRefCountPtr<ID3D11UnorderedAccessView> UnorderedAccessView;
	VERIFYD3D11CREATEVIEWRESULT(Direct3DDevice->CreateUnorderedAccessView(Texture->GetResource(),&UAVDesc,(ID3D11UnorderedAccessView**)UnorderedAccessView.GetInitReference()), Direct3DDevice, TextureRHI, UAVDesc);

	return new FD3D11UnorderedAccessView(UnorderedAccessView, Texture, Texture);
}

FUnorderedAccessViewRHIRef FD3D11DynamicRHI::RHICreateUnorderedAccessView_RenderThread(
	class FRHICommandListImmediate& RHICmdList,
	FRHITexture* Texture,
	uint32 MipLevel, 
	uint16 FirstArraySlice, 
	uint16 NumArraySlices)
{
	return RHICreateUnorderedAccessView(Texture, MipLevel, FirstArraySlice, NumArraySlices);
}

FUnorderedAccessViewRHIRef FD3D11DynamicRHI::RHICreateUnorderedAccessView(FRHIBuffer* BufferRHI, uint8 Format)
{
	FD3D11Buffer* Buffer = ResourceCast(BufferRHI);

	D3D11_BUFFER_DESC BufferDesc;
	Buffer->Resource->GetDesc(&BufferDesc);

	D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = InitializeCommonUAVDesc(BufferDesc, 0, BufferDesc.ByteWidth, (EPixelFormat)Format, false);

	TRefCountPtr<ID3D11UnorderedAccessView> UnorderedAccessView;
	VERIFYD3D11CREATEVIEWRESULT(Direct3DDevice->CreateUnorderedAccessView(Buffer->Resource, &UAVDesc, (ID3D11UnorderedAccessView**)UnorderedAccessView.GetInitReference()), Direct3DDevice, BufferRHI, UAVDesc);

	return new FD3D11UnorderedAccessView(UnorderedAccessView, Buffer, Buffer);
}

FUnorderedAccessViewRHIRef FD3D11DynamicRHI::RHICreateUnorderedAccessView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer, uint8 Format)
{
	return RHICreateUnorderedAccessView(Buffer, Format);
}

FShaderResourceViewRHIRef FD3D11DynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer)
{
	return RHICreateShaderResourceView(Buffer);
}

FShaderResourceViewRHIRef FD3D11DynamicRHI::RHICreateShaderResourceView(FRHIBuffer* BufferRHI)
{
	return FD3D11DynamicRHI::RHICreateShaderResourceView(FShaderResourceViewInitializer(BufferRHI));
}

uint64 FD3D11DynamicRHI::RHIGetMinimumAlignmentForBufferBackedSRV(EPixelFormat Format)
{
	return GPixelFormats[Format].BlockBytes;
}

static void CreateD3D11ShaderResourceViewOnBuffer(ID3D11Device* Direct3DDevice, ID3D11Buffer* Buffer, uint32 StartOffsetBytes, uint32 NumElements, uint8 Format, ID3D11ShaderResourceView** OutSRV)
{
	D3D11_BUFFER_DESC BufferDesc;
	Buffer->GetDesc(&BufferDesc);

	const uint32 FormatStride = GPixelFormats[Format].BlockBytes;
	const uint32 NumRequestedBytes = NumElements * FormatStride;
	const uint32 OffsetBytes = FMath::Min(StartOffsetBytes, BufferDesc.ByteWidth);
	const uint32 NumBytes = FMath::Min(NumRequestedBytes, BufferDesc.ByteWidth - OffsetBytes);

	const bool bByteAccessBuffer = (BufferDesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS) != 0;

	// Create a Shader Resource View
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc{};

	if (bByteAccessBuffer)
	{
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
		SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;

		SRVDesc.BufferEx.Flags |= D3D11_BUFFEREX_SRV_FLAG_RAW;
		SRVDesc.BufferEx.FirstElement = OffsetBytes / 4;
		SRVDesc.BufferEx.NumElements = NumBytes / 4;
	}
	else
	{
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
		SRVDesc.Format = FindShaderResourceDXGIFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat, false);

		SRVDesc.Buffer.FirstElement = OffsetBytes / GPixelFormats[Format].BlockBytes;
		SRVDesc.Buffer.NumElements = NumBytes / GPixelFormats[Format].BlockBytes;
	}

	HRESULT hr = Direct3DDevice->CreateShaderResourceView(Buffer, &SRVDesc, OutSRV);
	if (FAILED(hr))
	{
		if (hr == E_OUTOFMEMORY)
		{
			// There appears to be a driver bug that causes SRV creation to fail with an OOM error and then succeed on the next call.
			hr = Direct3DDevice->CreateShaderResourceView(Buffer, &SRVDesc, OutSRV);
		}
		if (FAILED(hr))
		{
			UE_LOG(LogD3D11RHI, Error, TEXT("Failed to create shader resource view for buffer: ByteWidth=%d NumElements=%d Format=%s"), BufferDesc.ByteWidth, BufferDesc.ByteWidth / GPixelFormats[Format].BlockBytes, GPixelFormats[Format].Name);
			VerifyD3D11Result(hr, "Direct3DDevice->CreateShaderResourceView", __FILE__, __LINE__, Direct3DDevice);
		}
	}
}

FShaderResourceViewRHIRef FD3D11DynamicRHI::RHICreateShaderResourceView(FRHIBuffer* BufferRHI, uint32 Stride, uint8 Format)
{
	ensureMsgf(Stride == GPixelFormats[Format].BlockBytes, TEXT("provided stride: %i was not consitent with Pixelformat: %s"), Stride, GPixelFormats[Format].Name);
	return FD3D11DynamicRHI::RHICreateShaderResourceView(FShaderResourceViewInitializer(BufferRHI, EPixelFormat(Format)));
}

FShaderResourceViewRHIRef FD3D11DynamicRHI::RHICreateShaderResourceView(const FShaderResourceViewInitializer& Initializer)
{
	FShaderResourceViewInitializer::FBufferShaderResourceViewInitializer Desc = Initializer.AsBufferSRV();

	FD3D11Buffer* Buffer = ResourceCast(Desc.Buffer);

	if (!Desc.Buffer || !Buffer->Resource)
	{
		return new FD3D11ShaderResourceView(nullptr, nullptr, nullptr);
	}

	TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;

	switch (Initializer.GetType())
	{
		case FShaderResourceViewInitializer::EType::VertexBufferSRV:
		{
			CreateD3D11ShaderResourceViewOnBuffer(Direct3DDevice, Buffer->Resource, Desc.StartOffsetBytes, Desc.NumElements, Desc.Format, ShaderResourceView.GetInitReference());

			return new FD3D11ShaderResourceView(ShaderResourceView, Buffer, Buffer);
		}

		case FShaderResourceViewInitializer::EType::StructuredBufferSRV:
		{
			D3D11_BUFFER_DESC BufferDesc;
			Buffer->Resource->GetDesc(&BufferDesc);

			const bool bByteAccessBuffer = (BufferDesc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS) != 0;

			// Create a Shader Resource View
			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc{};

			if (bByteAccessBuffer)
			{
				uint32 MaxElements = BufferDesc.ByteWidth / 4;
				uint32 StartOffsetBytes = FMath::Min(Desc.StartOffsetBytes, BufferDesc.ByteWidth);
				uint32 StartElement = StartOffsetBytes / 4;

				SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
				SRVDesc.BufferEx.NumElements = FMath::Min(Desc.NumElements, MaxElements - StartElement);
				SRVDesc.BufferEx.FirstElement = StartElement;
				SRVDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
				SRVDesc.Format = DXGI_FORMAT_R32_TYPELESS;
			}
			else
			{
				uint32 MaxElements = BufferDesc.ByteWidth / BufferDesc.StructureByteStride;
				uint32 StartOffsetBytes = FMath::Min(Desc.StartOffsetBytes, BufferDesc.ByteWidth);
				uint32 StartElement = StartOffsetBytes / BufferDesc.StructureByteStride;

				SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
				SRVDesc.Buffer.FirstElement = StartElement;
				SRVDesc.Buffer.NumElements = FMath::Min(Desc.NumElements, MaxElements - StartElement);
				SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
			}

			VERIFYD3D11RESULT_EX(Direct3DDevice->CreateShaderResourceView(Buffer->Resource, &SRVDesc, (ID3D11ShaderResourceView**)ShaderResourceView.GetInitReference()), Direct3DDevice);

			return new FD3D11ShaderResourceView(ShaderResourceView, Buffer, Buffer);
		}

		case FShaderResourceViewInitializer::EType::IndexBufferSRV:
		{
			// The stride in bytes of the index buffer; must be 2 or 4
			const uint32 Stride = Desc.Buffer->GetStride();
			check(Stride == 2 || Stride == 4);
			const EPixelFormat Format = (Stride == 2) ? PF_R16_UINT : PF_R32_UINT;

			CreateD3D11ShaderResourceViewOnBuffer(Direct3DDevice, Buffer->Resource, Desc.StartOffsetBytes, Desc.NumElements, Format, ShaderResourceView.GetInitReference());

			return new FD3D11ShaderResourceView(ShaderResourceView, Buffer, Buffer);
		}
	}

	checkNoEntry();
	return nullptr;
}

FShaderResourceViewRHIRef FD3D11DynamicRHI::CreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer, uint32 Stride, uint8 Format)
{
	return RHICreateShaderResourceView(Buffer, Stride, Format);
}

FShaderResourceViewRHIRef FD3D11DynamicRHI::RHICreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, const FShaderResourceViewInitializer& Initializer)
{
	return RHICreateShaderResourceView(Initializer);
}

FShaderResourceViewRHIRef FD3D11DynamicRHI::CreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, const FShaderResourceViewInitializer& Initializer)
{
	return RHICreateShaderResourceView(Initializer);
}

void FD3D11DynamicRHI::RHIUpdateShaderResourceView(FRHIShaderResourceView* SRV, FRHIBuffer* BufferRHI, uint32 Stride, uint8 Format)
{
	check(SRV);
	FD3D11ShaderResourceView* SRVD3D11 = ResourceCast(SRV);
	if (!BufferRHI)
	{
		SRVD3D11->Rename(nullptr, nullptr);
	}
	else
	{
		FD3D11Buffer* Buffer = ResourceCast(BufferRHI);
		check(Buffer->Resource);

		TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;
		CreateD3D11ShaderResourceViewOnBuffer(Direct3DDevice, Buffer->Resource, 0, UINT32_MAX, Format, ShaderResourceView.GetInitReference());

		SRVD3D11->Rename(ShaderResourceView, Buffer);
	}
}

void FD3D11DynamicRHI::RHIUpdateShaderResourceView(FRHIShaderResourceView* SRV, FRHIBuffer* BufferRHI)
{
	check(SRV);
	FD3D11ShaderResourceView* SRVD3D11 = ResourceCast(SRV);
	if (!BufferRHI)
	{
		SRVD3D11->Rename(nullptr, nullptr);
	}
	else
	{
		FD3D11Buffer* Buffer = ResourceCast(BufferRHI);
		check(Buffer->Resource);

		const uint32 Stride = Buffer->GetStride();
		const EPixelFormat Format = Stride == 2 ? PF_R16_UINT : PF_R32_UINT;
		TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;
		CreateD3D11ShaderResourceViewOnBuffer(Direct3DDevice, Buffer->Resource, 0, UINT32_MAX, Format, ShaderResourceView.GetInitReference());

		SRVD3D11->Rename(ShaderResourceView, Buffer);
	}
}

FShaderResourceViewRHIRef FD3D11DynamicRHI::CreateShaderResourceView_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHIBuffer* Buffer)
{
	return RHICreateShaderResourceView(Buffer);
}

void FD3D11DynamicRHI::ClearUAV(TRHICommandList_RecursiveHazardous<FD3D11DynamicRHI>& RHICmdList, FD3D11UnorderedAccessView* UnorderedAccessView, const void* ClearValues, bool bFloat)
{
	D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
	UnorderedAccessView->View->GetDesc(&UAVDesc);

	// Only structured buffers can have an unknown format
	check(UAVDesc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER || UAVDesc.Format != DXGI_FORMAT_UNKNOWN);

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

	if (UAVDesc.ViewDimension == D3D11_UAV_DIMENSION_BUFFER)
	{
		const bool bByteAddressBuffer = (UAVDesc.Buffer.Flags & D3D11_BUFFER_UAV_FLAG_RAW) != 0;

		if (UAVDesc.Format == DXGI_FORMAT_UNKNOWN || bByteAddressBuffer)
		{
			// Structured buffer. Use the clear function on the immediate context, since we can't use a general purpose shader for these.
			RHICmdList.RunOnContext([UnorderedAccessView, ClearValues](auto& Context)
			{
				Context.Direct3DDeviceIMContext->ClearUnorderedAccessViewUint(UnorderedAccessView->View, *reinterpret_cast<const UINT(*)[4]>(ClearValues));
				Context.GPUProfilingData.RegisterGPUWork(1);
			});
		}
		else
		{
			if (UAVDesc.Buffer.NumElements < 65536 * 64)
			{
				ClearUAVShader_T<EClearReplacementResourceType::Buffer, 4, false>(RHICmdList, UnorderedAccessView, UAVDesc.Buffer.NumElements, 1, 1, ClearValues, ValueType);
			}
			else
			{
				ClearUAVShader_T<EClearReplacementResourceType::LargeBuffer, 4, false>(RHICmdList, UnorderedAccessView, UAVDesc.Buffer.NumElements, 1, 1, ClearValues, ValueType);
			}
			
		}
	}
	else
	{
		if (UAVDesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2D)
		{
			FD3D11Texture* Texture = static_cast<FD3D11Texture*>(UnorderedAccessView->Resource.GetReference());
			FIntVector Size = Texture->GetSizeXYZ();

			uint32 Width  = Size.X >> UAVDesc.Texture2D.MipSlice;
			uint32 Height = Size.Y >> UAVDesc.Texture2D.MipSlice;
			ClearUAVShader_T<EClearReplacementResourceType::Texture2D, 4, false>(RHICmdList, UnorderedAccessView, Width, Height, 1, ClearValues, ValueType);
		}
		else if (UAVDesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY)
		{
			FD3D11Texture* Texture = static_cast<FD3D11Texture*>(UnorderedAccessView->Resource.GetReference());
			FIntVector Size = Texture->GetSizeXYZ();

			uint32 Width = Size.X >> UAVDesc.Texture2DArray.MipSlice;
			uint32 Height = Size.Y >> UAVDesc.Texture2DArray.MipSlice;
			ClearUAVShader_T<EClearReplacementResourceType::Texture2DArray, 4, false>(RHICmdList, UnorderedAccessView, Width, Height, UAVDesc.Texture2DArray.ArraySize, ClearValues, ValueType);
		}
		else if (UAVDesc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE3D)
		{
			FD3D11Texture* Texture = static_cast<FD3D11Texture*>(UnorderedAccessView->Resource.GetReference());
			FIntVector Size = Texture->GetSizeXYZ();

			// @todo - is WSize / mip index handling here correct?
			uint32 Width = Size.X >> UAVDesc.Texture2DArray.MipSlice;
			uint32 Height = Size.Y >> UAVDesc.Texture2DArray.MipSlice;
			ClearUAVShader_T<EClearReplacementResourceType::Texture3D, 4, false>(RHICmdList, UnorderedAccessView, Width, Height, UAVDesc.Texture3D.WSize, ClearValues, ValueType);
		}
		else
		{
			ensure(0);
		}
	}
}

void FD3D11DynamicRHI::RHIClearUAVFloat(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FVector4f& Values)
{
	TRHICommandList_RecursiveHazardous<FD3D11DynamicRHI> RHICmdList(this);
	ClearUAV(RHICmdList, ResourceCast(UnorderedAccessViewRHI), &Values, true);
}

void FD3D11DynamicRHI::RHIClearUAVUint(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const FUintVector4& Values)
{
	TRHICommandList_RecursiveHazardous<FD3D11DynamicRHI> RHICmdList(this);
	ClearUAV(RHICmdList, ResourceCast(UnorderedAccessViewRHI), &Values, false);
}

void FD3D11DynamicRHI::RHIBindDebugLabelName(FRHIUnorderedAccessView* UnorderedAccessViewRHI, const TCHAR* Name)
{
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	FD3D11UnorderedAccessView* UAV = ResourceCast(UnorderedAccessViewRHI);
	if (UAV->View != nullptr)
	{
		UAV->View->SetPrivateData(WKPDID_D3DDebugObjectName, FCString::Strlen(Name) + 1, TCHAR_TO_ANSI(Name));
	}
#endif
}

FD3D11StagingBuffer::~FD3D11StagingBuffer()
{
	if (StagedRead)
	{
		StagedRead.SafeRelease();
	}
}

void* FD3D11StagingBuffer::Lock(uint32 Offset, uint32 NumBytes)
{
	check(!bIsLocked);
	bIsLocked = true;
	if (StagedRead)
	{
		// Map the staging buffer's memory for reading.
		D3D11_MAPPED_SUBRESOURCE MappedSubresource;
		VERIFYD3D11RESULT(Context->Map(StagedRead, 0, D3D11_MAP_READ, 0, &MappedSubresource));

		return (void*)((uint8*)MappedSubresource.pData + Offset);
	}
	else
	{
		return nullptr;
	}
}

void FD3D11StagingBuffer::Unlock()
{
	check(bIsLocked);
	bIsLocked = false;
	if (StagedRead)
	{
		Context->Unmap(StagedRead, 0);
	}
}
