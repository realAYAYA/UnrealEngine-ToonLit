// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D11TextureShareSample.h"
#include "D3D11TextureShareSampleSetup.h"
#include "Misc/TextureShareLog.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FD3D11TextureShareSample
//////////////////////////////////////////////////////////////////////////////////////////////
FD3D11TextureShareSample::FD3D11TextureShareSample(ID3D11Texture2D* InBackBufferTexture)
{
	// Create texture share object
	if(TextureShareObject = ITextureShareObject::CreateInstance(TextureShareSample::ObjectDesc))
	{
		// Request resources for receive
		TextureShareObject->GetData().ResourceRequests.Add(FTextureShareResourceRequest(TextureShareSample::Receive::Texture1::Desc, TextureShareSample::Receive::Texture1::Resource));
		TextureShareObject->GetData().ResourceRequests.Add(FTextureShareResourceRequest(TextureShareSample::Receive::Texture2::Desc, TextureShareSample::Receive::Texture2::Resource));
	}
}

FD3D11TextureShareSample::~FD3D11TextureShareSample()
{
	if (TextureShareObject)
	{
		delete TextureShareObject;
		TextureShareObject = nullptr;
	}
}

ID3D11ShaderResourceView* FD3D11TextureShareSample::GetReceiveTextureSRV(int32 InReceiveTextureIndex) const
{
	switch (InReceiveTextureIndex)
	{
	case 0: return TextureShareSample::Receive::Texture1::Resource.GetTextureSRV();
	case 1: return TextureShareSample::Receive::Texture2::Resource.GetTextureSRV();
	default:
		break;
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////////
void FD3D11TextureShareSample::BeginFrame(const FTextureShareDeviceContextD3D11& InDeviceContext, ID3D11Texture2D* InBackBufferTexture)
{
	if (TextureShareObject && TextureShareObject->BeginFrame())
	{
		// Receive remote textures
		TextureShareObject->ReceiveResource(InDeviceContext, TextureShareSample::Receive::Texture1::Desc, TextureShareSample::Receive::Texture1::Resource);
		TextureShareObject->ReceiveResource(InDeviceContext, TextureShareSample::Receive::Texture2::Desc, TextureShareSample::Receive::Texture2::Resource);

		// Request resource for sending
		if (ID3D11Texture2D* Texture = TextureShareSample::Receive::Texture1::Resource.Texture)
		{
			TextureShareObject->GetData().ResourceRequests.AddUnique(ITextureShareObject::GetResourceRequest(TextureShareSample::Send::Texture1::Desc, FTextureShareImageD3D11(Texture)));
			
			// first frame skipped, resource created on next frame
			TextureShareObject->SendTexture(InDeviceContext, TextureShareSample::Send::Texture1::Desc, FTextureShareImageD3D11(Texture));
		}
		if (ID3D11Texture2D* Texture = TextureShareSample::Receive::Texture2::Resource.Texture)
		{
			TextureShareObject->GetData().ResourceRequests.AddUnique(ITextureShareObject::GetResourceRequest(TextureShareSample::Send::Texture2::Desc, FTextureShareImageD3D11(Texture)));

			// first frame skipped, resource created on next frame
			TextureShareObject->SendTexture(InDeviceContext, TextureShareSample::Send::Texture2::Desc, FTextureShareImageD3D11(Texture));
		}
	}
}

void FD3D11TextureShareSample::EndFrame(const FTextureShareDeviceContextD3D11& InDeviceContext, ID3D11Texture2D* InBackBufferTexture)
{
	if (TextureShareObject)
	{
		TextureShareObject->EndFrame();
	}
}
