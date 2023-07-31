// Copyright Epic Games, Inc. All Rights Reserved.

#include "D3D12TextureShareSample.h"
#include "D3D12TextureShareSampleSetup.h"
#include "Misc/TextureShareLog.h"

//////////////////////////////////////////////////////////////////////////////////////////////
// FD3D12TextureShareSample
//////////////////////////////////////////////////////////////////////////////////////////////
FD3D12TextureShareSample::FD3D12TextureShareSample(ID3D12Resource* InBackBufferTexture)
{
	// Create texture share object
	if(TextureShareObject = ITextureShareObject::CreateInstance(TextureShareSample::ObjectDesc))
	{
		// Request resources for receive
		TextureShareObject->GetData().ResourceRequests.Add(FTextureShareResourceRequest(TextureShareSample::Receive::Texture1::Desc, TextureShareSample::Receive::Texture1::Resource));
		TextureShareObject->GetData().ResourceRequests.Add(FTextureShareResourceRequest(TextureShareSample::Receive::Texture2::Desc, TextureShareSample::Receive::Texture2::Resource));
	}
}

FD3D12TextureShareSample::~FD3D12TextureShareSample()
{
	if (TextureShareObject)
	{
		delete TextureShareObject;
		TextureShareObject = nullptr;
	}
}

int32 FD3D12TextureShareSample::GetReceiveTextureSRV(int32 InReceiveTextureIndex) const
{
	switch (InReceiveTextureIndex)
	{
	case 0: return TextureShareSample::Receive::Texture1::Resource.IsValid() ? TextureShareSample::Receive::Texture1::Resource.SRVIndex : -1;
	case 1: return TextureShareSample::Receive::Texture2::Resource.IsValid() ? TextureShareSample::Receive::Texture2::Resource.SRVIndex : -1;
	default:
		break;
	}

	return -1;
}


//////////////////////////////////////////////////////////////////////////////////////////////
void FD3D12TextureShareSample::BeginFrame(const FTextureShareDeviceContextD3D12& InDeviceContext, ID3D12Resource* InBackBufferTexture)
{
	if (TextureShareObject && TextureShareObject->BeginFrame())
	{
		// Receive remote textures
		TextureShareObject->ReceiveResource(InDeviceContext, TextureShareSample::Receive::Texture1::Desc, TextureShareSample::Receive::Texture1::Resource);
		TextureShareObject->ReceiveResource(InDeviceContext, TextureShareSample::Receive::Texture2::Desc, TextureShareSample::Receive::Texture2::Resource);
	}
}

void FD3D12TextureShareSample::EndFrame(const FTextureShareDeviceContextD3D12& InDeviceContext, ID3D12Resource* InBackBufferTexture)
{
	if (TextureShareObject)
	{
		TextureShareObject->EndFrame();
	}
}
