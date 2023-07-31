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
		TextureShareObject->GetData().ResourceRequests.Add(FTextureShareResourceRequest(TextureShareSample::Receive::Texture1::Desc, D3D11AppSetup::Backbuffer::Format));
		TextureShareObject->GetData().ResourceRequests.Add(FTextureShareResourceRequest(TextureShareSample::Receive::Texture2::Desc, D3D11AppSetup::Backbuffer::Format));
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
	return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////////
void FD3D11TextureShareSample::BeginFrame(const FTextureShareDeviceContextD3D11& InDeviceContext, ID3D11Texture2D* InBackBufferTexture)
{
	if (TextureShareObject && TextureShareObject->BeginFrame())
	{
	}
}

void FD3D11TextureShareSample::EndFrame(const FTextureShareDeviceContextD3D11& InDeviceContext, ID3D11Texture2D* InBackBufferTexture)
{
	if (TextureShareObject)
	{
		// Copt to backbuffer rects
		const FTextureShareImageD3D11 DestBackbufferTexture(InBackBufferTexture);

		// Receive directly to backbuffer surface
		FTextureShareTextureCopyParameters LeftRect;
		FTextureShareTextureCopyParameters RightRect;

		const FIntPoint& BBSize = D3D11AppSetup::Backbuffer::Size;
		LeftRect.Rect = RightRect.Rect = FIntPoint(BBSize.X / 2, BBSize.Y);
		RightRect.Dest.X = RightRect.Rect.X;

		TextureShareObject->ReceiveTexture(InDeviceContext, TextureShareSample::Receive::Texture1::Desc, DestBackbufferTexture, LeftRect);
		TextureShareObject->ReceiveTexture(InDeviceContext, TextureShareSample::Receive::Texture2::Desc, DestBackbufferTexture, RightRect);

		TextureShareObject->EndFrame();
	}
}
