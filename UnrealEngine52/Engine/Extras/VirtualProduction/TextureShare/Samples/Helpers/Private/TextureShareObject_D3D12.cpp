// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureShareObject.h"
#include <d3dx12.h>

#include "TextureShareObject.h"

//////////////////////////////////////////////////////////////////////////////////////////////
namespace TextureShareD3D12Helpers
{
	static bool IsTexturesSizeFormatEqual(ID3D12Resource* InTexture1, ID3D12Resource* InTexture2)
	{
		if (InTexture1 && InTexture2)
		{
			D3D12_RESOURCE_DESC Desc1 = InTexture1->GetDesc();
			D3D12_RESOURCE_DESC Desc2 = InTexture2->GetDesc();

			return (Desc1.Width == Desc2.Width) && (Desc1.Height == Desc2.Height) && (Desc1.Format == Desc2.Format);
		}

		return false;
	}

	static bool CreateShaderResourceView(const FTextureShareDeviceContextD3D12& InDeviceContext, FTextureShareResourceD3D12& InOutResource)
	{
		if (InDeviceContext.IsValid() && InOutResource.IsValid())
		{
			const D3D12_RESOURCE_DESC InTextureDesc = InOutResource.Texture->GetDesc();

			// Describe and create a SRV for the texture.
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = InTextureDesc.Format;
			srvDesc.Texture2D.MipLevels = InTextureDesc.MipLevels;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

			D3D12_CPU_DESCRIPTOR_HANDLE handle = InDeviceContext.pD3D12HeapSRV->GetCPUDescriptorHandleForHeapStart();
			uint32 DescriptorSize = InDeviceContext.pD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			uint32 DescriptorOffset = InOutResource.SRVIndex * DescriptorSize;
			handle.ptr += DescriptorOffset;

			InDeviceContext.pD3D12Device->CreateShaderResourceView(InOutResource.Texture, &srvDesc, handle);

			return true;
		}

		return false;
	}

	static bool CreateResource(const FTextureShareDeviceContextD3D12& InDeviceContext, ID3D12Resource* InSharedTexture, FTextureShareResourceD3D12& InOutResource)
	{
		InOutResource.Release();

		// Create copiable texture format&size must be equal
		D3D12_RESOURCE_DESC SharedTextureDesc = InSharedTexture->GetDesc();
		D3D12_RESOURCE_DESC SRVTextureDesc = {};
		{
			SRVTextureDesc.Format = SharedTextureDesc.Format;
			SRVTextureDesc.Width = SharedTextureDesc.Width;
			SRVTextureDesc.Height = SharedTextureDesc.Height;
			SRVTextureDesc.MipLevels = SharedTextureDesc.MipLevels;

			SRVTextureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

			SRVTextureDesc.DepthOrArraySize = 1;
			SRVTextureDesc.SampleDesc.Count = 1;
			SRVTextureDesc.SampleDesc.Quality = 0;
			SRVTextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		}

		D3D12_HEAP_PROPERTIES HeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		HRESULT hResult = InDeviceContext.pD3D12Device->CreateCommittedResource(
			&HeapProp,
			D3D12_HEAP_FLAG_NONE,
			&SRVTextureDesc,
			D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
			nullptr,
			__uuidof(ID3D12Resource), (void**)(&InOutResource.Texture));

		if (SUCCEEDED(hResult))
		{
			if (InOutResource.bCreateSRV)
			{
				return CreateShaderResourceView(InDeviceContext, InOutResource);
			}

			return true;
		}

		return false;
	}

	static void TransitionBarrier(const FTextureShareDeviceContextD3D12& InDeviceContext, ID3D12Resource* InResource, const D3D12_RESOURCE_STATES InSourceState, const D3D12_RESOURCE_STATES InDescState)
	{
		if (InDeviceContext.IsValid() && InResource)
		{
			D3D12_RESOURCE_BARRIER ResourceBarrier = {};
			ResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			ResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			ResourceBarrier.Transition.pResource = InResource;
			ResourceBarrier.Transition.StateBefore = InSourceState;
			ResourceBarrier.Transition.StateAfter = InDescState;
			ResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

			InDeviceContext.pCmdList->ResourceBarrier(1, &ResourceBarrier);
		}
	}

	static bool Send(const FTextureShareDeviceContextD3D12& InDeviceContext, const FTextureShareImageD3D12& InSrcTexture, ID3D12Resource* DestSharedResource)
	{
		if (InDeviceContext.IsValid() && DestSharedResource && InSrcTexture.IsValid())
		{
			TransitionBarrier(InDeviceContext, InSrcTexture.Texture, InSrcTexture.TextureState, D3D12_RESOURCE_STATE_COPY_SOURCE);

			InDeviceContext.pCmdList->CopyResource(DestSharedResource, InSrcTexture.Texture);

			TransitionBarrier(InDeviceContext, InSrcTexture.Texture, D3D12_RESOURCE_STATE_COPY_SOURCE, InSrcTexture.TextureState);

			return true;
		}

		return false;
	}

	static bool Receive(const FTextureShareDeviceContextD3D12& InDeviceContext, ID3D12Resource* SrcSharedResource, FTextureShareResourceD3D12& InOutResource)
	{
		if (InDeviceContext.IsValid() && SrcSharedResource && InOutResource.IsValid())
		{
			const D3D12_RESOURCE_STATES SrcResourceState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

			TransitionBarrier(InDeviceContext, InOutResource.Texture, SrcResourceState, D3D12_RESOURCE_STATE_COPY_DEST);

			InDeviceContext.pCmdList->CopyResource(InOutResource.Texture, SrcSharedResource);

			TransitionBarrier(InDeviceContext, InOutResource.Texture, D3D12_RESOURCE_STATE_COPY_DEST, SrcResourceState);

			return true;
		}

		return false;
	}

	static bool ReceiveToTexture(const FTextureShareDeviceContextD3D12& InDeviceContext, ID3D12Resource* SrcSharedResource, const FTextureShareImageD3D12& InDestTexture, const FTextureShareTextureCopyParameters& InCopyParameters)
	{
		if (InDeviceContext.IsValid() && SrcSharedResource && InDestTexture.IsValid())
		{
			// Copy region
			if (InDeviceContext.IsValid() && SrcSharedResource && InDestTexture.IsValid())
			{
				D3D12_RESOURCE_DESC SrcDesc = SrcSharedResource->GetDesc();
				D3D12_RESOURCE_DESC DestDesc = InDestTexture.Texture->GetDesc();

				const FTextureShareTextureCopyParameters CopyParameters = InCopyParameters.FindValidRect(FIntPoint((int)SrcDesc.Width, (int)SrcDesc.Height), FIntPoint((int)DestDesc.Width, (int)DestDesc.Height));
				if (CopyParameters.IsValid())
				{
					TransitionBarrier(InDeviceContext, InDestTexture.Texture, InDestTexture.TextureState, D3D12_RESOURCE_STATE_COPY_DEST);

					CD3DX12_TEXTURE_COPY_LOCATION Dst(InDestTexture.Texture, 0);
					CD3DX12_TEXTURE_COPY_LOCATION Src(SrcSharedResource, 0);

					D3D12_BOX SrcRegion = {};
					SrcRegion.left = CopyParameters.Src.X;
					SrcRegion.right = CopyParameters.Src.X + CopyParameters.Rect.X;
					
					SrcRegion.top = CopyParameters.Src.Y;
					SrcRegion.bottom = CopyParameters.Src.Y + CopyParameters.Rect.Y;

					SrcRegion.front = 0;
					SrcRegion.back = 1;

					InDeviceContext.pCmdList->CopyTextureRegion(&Dst, CopyParameters.Dest.X, CopyParameters.Dest.Y, 0, &Src, &SrcRegion);

					TransitionBarrier(InDeviceContext, InDestTexture.Texture, D3D12_RESOURCE_STATE_COPY_DEST, InDestTexture.TextureState);

					return true;
				}
			}
		}

		return false;
	}

	static bool GetOrCreate(const FTextureShareDeviceContextD3D12& InDeviceContext, ID3D12Resource* InSharedTexture, FTextureShareResourceD3D12& InOutResource)
	{
		if (InDeviceContext.IsValid() && InSharedTexture)
		{
			if (InOutResource.IsValid() && IsTexturesSizeFormatEqual(InOutResource.Texture, InSharedTexture))
			{
				return true;
			}

			// Create new resource
			InOutResource.Release();

			return CreateResource(InDeviceContext, InSharedTexture, InOutResource);
		}

		return false;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareObject
//////////////////////////////////////////////////////////////////////////////////////////////
EResourceState FTextureShareObject::D3D12SendTexture(const FTextureShareDeviceContextD3D12* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageD3D12* InSrcTexture)
{
	if (InDeviceContext && InSrcTexture)
	{
		if (ID3D12Resource* D3D12SharedTexture = TextureShareSDKObject->OpenSharedResourceD3D12(InDeviceContext->pD3D12Device, TDataInput<FTextureShareCoreResourceDesc>(InResourceDesc)))
		{
			if (TextureShareD3D12Helpers::IsTexturesSizeFormatEqual(InSrcTexture->Texture, D3D12SharedTexture))
			{
				// Copy [InTexture] >>>  [D3D11SharedTexture]
				return TextureShareD3D12Helpers::Send(*InDeviceContext, *InSrcTexture, D3D12SharedTexture) ? EResourceState::Success : EResourceState::E_UnknownError;
			}

			// UE side texture different, so can't copied
			return EResourceState::E_SharedResourceSizeFormatNotEqual;
		}

		return EResourceState::E_SharedResourceOpenFailed;
	}

	return EResourceState::E_INVALID_ARGS_TYPECAST;
}

EResourceState FTextureShareObject::D3D12ReceiveResource(const FTextureShareDeviceContextD3D12* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, FTextureShareResourceD3D12* InDestResource)
{
	if (InDeviceContext)
	{
		if (ID3D12Resource* D3D12SharedTexture = TextureShareSDKObject->OpenSharedResourceD3D12(InDeviceContext->pD3D12Device, TDataInput<FTextureShareCoreResourceDesc>(InResourceDesc)))
		{
			if (TextureShareD3D12Helpers::GetOrCreate(*InDeviceContext, D3D12SharedTexture, *InDestResource))
			{
				// Copy [D3D11SharedTexture] >>>  [InOutResource]
				return TextureShareD3D12Helpers::Receive(*InDeviceContext, D3D12SharedTexture, *InDestResource) ? EResourceState::Success : EResourceState::E_UnknownError;
			}

			// UE side texture different, so can't copied
			return EResourceState::E_SharedResourceSizeFormatNotEqual;
		}

		return EResourceState::E_SharedResourceOpenFailed;
	}

	return EResourceState::E_INVALID_ARGS_TYPECAST;
}

EResourceState FTextureShareObject::D3D12ReceiveTexture(const FTextureShareDeviceContextD3D12* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageD3D12* InDestTexture, const FTextureShareTextureCopyParameters& InCopyParameters)
{
	if (InDeviceContext)
	{
		if (ID3D12Resource* D3D12SharedTexture = TextureShareSDKObject->OpenSharedResourceD3D12(InDeviceContext->pD3D12Device, TDataInput<FTextureShareCoreResourceDesc>(InResourceDesc)))
		{
			return TextureShareD3D12Helpers::ReceiveToTexture(*InDeviceContext, D3D12SharedTexture, *InDestTexture, InCopyParameters) ? EResourceState::Success : EResourceState::E_UnknownError;
		}

		return EResourceState::E_SharedResourceOpenFailed;
	}

	return EResourceState::E_INVALID_ARGS_TYPECAST;
}

FTextureShareCoreResourceRequest FTextureShareObject::GetResourceRequestD3D12(const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageD3D12* InTexture)
{
	FTextureShareCoreResourceRequest Result(InResourceDesc);

	if (InTexture && InTexture->IsValid())
	{
		D3D12_RESOURCE_DESC Desc = InTexture->Texture->GetDesc();

		Result.Size = FIntPoint((int32)Desc.Width, (int32)Desc.Height);
		Result.Format = Desc.Format;
	}

	return Result;
}
