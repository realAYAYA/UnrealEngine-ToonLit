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
		TextureShareObject->GetData().ResourceRequests.Add(TextureShareSample::Receive::Texture1::Desc);
		TextureShareObject->GetData().ResourceRequests.Add(TextureShareSample::Receive::Texture2::Desc);
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
	return -1;
}

//////////////////////////////////////////////////////////////////////////////////////////////
void FD3D12TextureShareSample::BeginFrame(const FTextureShareDeviceContextD3D12& InDeviceContext, ID3D12Resource* InBackBufferTexture)
{
	if (TextureShareObject && TextureShareObject->BeginFrame())
	{
	}
}

void FD3D12TextureShareSample::EndFrame(const FTextureShareDeviceContextD3D12& InDeviceContext, ID3D12Resource* InBackBufferTexture)
{
	if (TextureShareObject)
	{
		// Copt to backbuffer rects
		const FTextureShareImageD3D12 DestBackbufferTexture(InBackBufferTexture, D3D12_RESOURCE_STATE_RENDER_TARGET);

		// Receive directly to backbuffer surface
		FTextureShareTextureCopyParameters LeftRect;
		FTextureShareTextureCopyParameters RightRect;

		const FIntPoint& BBSize = D3D12AppSetup::Backbuffer::Size;
		LeftRect.Rect = RightRect.Rect = FIntPoint(BBSize.X / 2, BBSize.Y);
		RightRect.Dest.X = RightRect.Rect.X;

		TextureShareObject->ReceiveTexture(InDeviceContext, TextureShareSample::Receive::Texture1::Desc, DestBackbufferTexture, LeftRect);
		TextureShareObject->ReceiveTexture(InDeviceContext, TextureShareSample::Receive::Texture2::Desc, DestBackbufferTexture, RightRect);

		TextureShareObject->EndFrame();
	}
}
