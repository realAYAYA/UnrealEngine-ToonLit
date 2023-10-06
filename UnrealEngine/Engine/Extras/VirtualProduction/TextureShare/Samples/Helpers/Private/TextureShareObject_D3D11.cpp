// Copyright Epic Games, Inc. All Rights Reserved.

#include "TextureShareObject.h"

//////////////////////////////////////////////////////////////////////////////////////////////
namespace TextureShareD3D11Helpers
{
	static bool IsTexturesSizeFormatEqual(ID3D11Texture2D* Texture1, ID3D11Texture2D* Texture2)
	{
		if (Texture1 && Texture2)
		{
			D3D11_TEXTURE2D_DESC Desc1, Desc2;
			Texture1->GetDesc(&Desc1);
			Texture2->GetDesc(&Desc2);

			return (Desc1.Width == Desc2.Width) && (Desc1.Height == Desc2.Height) && (Desc1.Format == Desc2.Format);
		}

		return false;
	}

	static bool GetOrCreate(const FTextureShareDeviceContextD3D11& InDeviceContext, ID3D11Texture2D* InSharedTexture, FTextureShareResourceD3D11& InOutD3D11Resource)
	{
		if (IsTexturesSizeFormatEqual(InOutD3D11Resource.Texture, InSharedTexture))
		{
			// equal, continue use exist texture
			return true;
		}

		if (InSharedTexture && InDeviceContext.IsValid())
		{
			InOutD3D11Resource.Release();

			D3D11_TEXTURE2D_DESC SharedTextureDesc;
			InSharedTexture->GetDesc(&SharedTextureDesc);

			D3D11_TEXTURE2D_DESC SRVTextureDesc = {};

			// Use size&format from shared texture
			SRVTextureDesc.Format = SharedTextureDesc.Format;
			SRVTextureDesc.Width = SharedTextureDesc.Width;
			SRVTextureDesc.Height = SharedTextureDesc.Height;

			SRVTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

			SRVTextureDesc.MipLevels = 1;
			SRVTextureDesc.ArraySize = 1;
			SRVTextureDesc.SampleDesc.Count = 1;
			SRVTextureDesc.SampleDesc.Quality = 0;
			SRVTextureDesc.CPUAccessFlags = 0;
			SRVTextureDesc.MiscFlags = 0;
			SRVTextureDesc.Usage = D3D11_USAGE_DEFAULT;

			// Create texture for SRV
			HRESULT hResult = InDeviceContext.D3D11Device->CreateTexture2D(&SRVTextureDesc, nullptr, &InOutD3D11Resource.Texture);

			if (SUCCEEDED(hResult) && InOutD3D11Resource.bCreateSRV)
			{
				// Create SRV
				D3D11_SHADER_RESOURCE_VIEW_DESC srDesc;

				srDesc.Format = SharedTextureDesc.Format;

				srDesc.Texture2D.MostDetailedMip = 0;
				srDesc.Texture2D.MipLevels = 1;
				srDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;

				hResult = InDeviceContext.D3D11Device->CreateShaderResourceView(InOutD3D11Resource.Texture, &srDesc, &InOutD3D11Resource.TextureSRV);
			}

			return SUCCEEDED(hResult);
		}

		return false;
	}

	static bool CopyResource(const FTextureShareDeviceContextD3D11& InDeviceContext, const FTextureShareImageD3D11& InSrcTexture, ID3D11Resource* DestTexture2D)
	{
		if (InDeviceContext.IsValid() && InSrcTexture.IsValid() && DestTexture2D)
		{
			InDeviceContext.D3D11DeviceContext->CopyResource(DestTexture2D, InSrcTexture.Texture);
			return true;
		}

		return false;
	}

	static bool CopyToTexture(const FTextureShareDeviceContextD3D11& InDeviceContext, ID3D11Texture2D* SourceTexture2D, ID3D11Texture2D* DestTexture2D, const FTextureShareTextureCopyParameters& InCopyParameters)
	{
		if (InDeviceContext.IsValid() && SourceTexture2D && DestTexture2D && InCopyParameters.IsValid())
		{
			D3D11_TEXTURE2D_DESC SrcDesc, DestDesc;
			SourceTexture2D->GetDesc(&SrcDesc);
			DestTexture2D->GetDesc(&DestDesc);

			const FTextureShareTextureCopyParameters CopyParameters = InCopyParameters.FindValidRect(FIntPoint((int)SrcDesc.Width, (int)SrcDesc.Height), FIntPoint((int)DestDesc.Width, (int)DestDesc.Height));
			if (CopyParameters.IsValid())
			{

				// Copy region
				UINT Width = (SrcDesc.Width < DestDesc.Width) ? SrcDesc.Width : DestDesc.Width;
				UINT Height = (SrcDesc.Height < DestDesc.Height) ? SrcDesc.Height : DestDesc.Height;

				D3D11_BOX SrcRegion = {};
				SrcRegion.left = CopyParameters.Src.X;
				SrcRegion.right = CopyParameters.Src.X + CopyParameters.Rect.X;
				
				SrcRegion.top = CopyParameters.Src.Y;
				SrcRegion.bottom = CopyParameters.Src.Y + CopyParameters.Rect.Y;

				SrcRegion.front = 0;
				SrcRegion.back = 1;

				InDeviceContext.D3D11DeviceContext->CopySubresourceRegion(DestTexture2D, 0, CopyParameters.Dest.X, CopyParameters.Dest.Y, 0, SourceTexture2D, 0, &SrcRegion);

				return true;
			}
		}

		return false;
	}
};

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareObject
//////////////////////////////////////////////////////////////////////////////////////////////
EResourceState FTextureShareObject::D3D11SendTexture(const FTextureShareDeviceContextD3D11* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageD3D11* InSrcTexture)
{
	if (InDeviceContext && InSrcTexture)
	{
		if (ID3D11Texture2D* D3D11SharedTexture = TextureShareSDKObject->OpenSharedResourceD3D11(InDeviceContext->D3D11Device, TDataInput<FTextureShareCoreResourceDesc>(InResourceDesc)))
			{
				if (TextureShareD3D11Helpers::IsTexturesSizeFormatEqual(InSrcTexture->Texture, D3D11SharedTexture))
				{
					// Copy [InTexture] >>>  [D3D11SharedTexture]
					return TextureShareD3D11Helpers::CopyResource(*InDeviceContext, *InSrcTexture, D3D11SharedTexture) ? EResourceState::Success : EResourceState::E_UnknownError;
				}

				// UE side texture different, so can't copied
				return EResourceState::E_SharedResourceSizeFormatNotEqual;
		}

		return EResourceState::E_SharedResourceOpenFailed;
	}

	return EResourceState::E_INVALID_ARGS_TYPECAST;
}

EResourceState FTextureShareObject::D3D11ReceiveResource(const FTextureShareDeviceContextD3D11* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, FTextureShareResourceD3D11* InDestResource)
{
	if (InDeviceContext)
	{
		if (ID3D11Texture2D* D3D11SharedTexture = TextureShareSDKObject->OpenSharedResourceD3D11(InDeviceContext->D3D11Device, TDataInput<FTextureShareCoreResourceDesc>(InResourceDesc)))
			{
				if (TextureShareD3D11Helpers::GetOrCreate(*InDeviceContext, D3D11SharedTexture, *InDestResource))
				{
					return TextureShareD3D11Helpers::CopyResource(*InDeviceContext, D3D11SharedTexture, InDestResource->Texture) ? EResourceState::Success : EResourceState::E_UnknownError;;
				}

				// UE side texture different, so can't copied
				return EResourceState::E_SharedResourceSizeFormatNotEqual;
		}

		return EResourceState::E_SharedResourceOpenFailed;
	}

	return EResourceState::E_INVALID_ARGS_TYPECAST;
}

EResourceState FTextureShareObject::D3D11ReceiveTexture(const FTextureShareDeviceContextD3D11* InDeviceContext, const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageD3D11* InDestTexture, const FTextureShareTextureCopyParameters& InCopyParameters)
{
	if (InDeviceContext)
	{
		if (ID3D11Texture2D* D3D11SharedTexture = TextureShareSDKObject->OpenSharedResourceD3D11(InDeviceContext->D3D11Device, TDataInput<FTextureShareCoreResourceDesc>(InResourceDesc)))
		{
			return TextureShareD3D11Helpers::CopyToTexture(*InDeviceContext, D3D11SharedTexture, InDestTexture->Texture, InCopyParameters) ? EResourceState::Success : EResourceState::E_UnknownError;
		}

		return EResourceState::E_SharedResourceOpenFailed;
	}

	return EResourceState::E_INVALID_ARGS_TYPECAST;
}

FTextureShareCoreResourceRequest FTextureShareObject::GetResourceRequestD3D11(const FTextureShareCoreResourceDesc& InResourceDesc, const FTextureShareImageD3D11* InTexture)
{
	FTextureShareCoreResourceRequest Result(InResourceDesc);
	if (InTexture && InTexture->IsValid())
	{
		D3D11_TEXTURE2D_DESC Desc;
		InTexture->Texture->GetDesc(&Desc);

		Result.Size = FIntPoint(Desc.Width, Desc.Height);
		Result.Format = Desc.Format;
	}

	return Result;
}
