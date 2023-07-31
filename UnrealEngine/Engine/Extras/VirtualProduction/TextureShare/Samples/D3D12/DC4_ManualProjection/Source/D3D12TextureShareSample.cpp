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

		// Request custom size backbuffer resource for sending
		if (InBackBufferTexture)
		{
			TextureShareObject->GetData().ResourceRequests.Add(ITextureShareObject::GetResourceRequest(TextureShareSample::Send::Backbuffer::Desc, FTextureShareImageD3D12(InBackBufferTexture)));
		}
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
	if (TextureShareObject)
	{
		// Iterate frame marker values
		TextureShareObject->GetData().FrameMarker.NextFrame();
		DEBUG_LOG(TEXT("Set new Frame marker %d from process %s"), TextureShareObject->GetData().FrameMarker.CustomFrameIndex1, *TextureShareObject->GetObjectDesc().ProcessName);

		// Set Manual projections:
		{
			// Clear prev frame values
			TextureShareObject->GetData().ManualProjections.Empty();

			FTextureShareCoreManualProjection NewManualProjection;
			NewManualProjection.ViewDesc = FTextureShareCoreViewDesc(TextureShareSample::DisplayCluster::Viewport3);
			NewManualProjection.ViewRotation.Pitch = 70;
			NewManualProjection.FrustumAngles.Left = -80;
			NewManualProjection.FrustumAngles.Right = 80;

			TextureShareObject->GetData().ManualProjections.Add(NewManualProjection);
		}

		if (TextureShareObject->BeginFrame())
		{
			// Read back frame markers
			FTextureShareCoreObjectFrameMarker ObjectFrameMarker;
			if (TextureShareObject->GetReceivedProxyDataFrameMarker(ObjectFrameMarker))
			{
				DEBUG_LOG(TEXT("Reading own frame marker from remote process FrameMarker=%d, from process %s"), ObjectFrameMarker.FrameMarker.CustomFrameIndex1, *ObjectFrameMarker.ObjectDesc.ProcessDesc.ProcessId);
			}

			// Read scene view data
			{
				// Iterate over remote processes
				for (const FTextureShareCoreObjectProxyData& ObjectProxyData : TextureShareObject->GetReceivedProxyData_RenderThread())
				{
					// Iterate over views
					for (const FTextureShareCoreSceneViewData& SceneViewDataIt : ObjectProxyData.ProxyData.SceneData)
					{
						const FString& SourceProcessId = ObjectProxyData.Desc.ProcessDesc.ProcessId;
						const FRotator& ViewRotation = SceneViewDataIt.View.ViewRotation;
						const FVector& ViewLocation = SceneViewDataIt.View.ViewLocation;
						const FMatrix& PrjMatrix = SceneViewDataIt.View.ViewMatrices.ProjectionMatrix;

						DEBUG_LOG(TEXT("Receive scene view data from Process=%s, View=(%s, Eye#%d), ViewRotation=(%.2f,%.2f,%.2f), ViewLocation=(%.2f,%.2f,%.2f)"), *SourceProcessId,
							*SceneViewDataIt.ViewDesc.Id, SceneViewDataIt.ViewDesc.EyeType
							, ViewRotation.Pitch, ViewRotation.Yaw, ViewRotation.Roll
							, ViewLocation.X, ViewLocation.Y, ViewLocation.Z);
					}
				}
			}

			// Receive remote textures
			TextureShareObject->ReceiveResource(InDeviceContext, TextureShareSample::Receive::Texture1::Desc, TextureShareSample::Receive::Texture1::Resource);
			TextureShareObject->ReceiveResource(InDeviceContext, TextureShareSample::Receive::Texture2::Desc, TextureShareSample::Receive::Texture2::Resource);
		}
	}
}

void FD3D12TextureShareSample::EndFrame(const FTextureShareDeviceContextD3D12& InDeviceContext, ID3D12Resource* InBackBufferTexture)
{
	if (TextureShareObject)
	{
		// Send backbuffer texture back
		TextureShareObject->SendTexture(InDeviceContext, TextureShareSample::Send::Backbuffer::Desc, FTextureShareImageD3D12(InBackBufferTexture, D3D12_RESOURCE_STATE_RENDER_TARGET));

		TextureShareObject->EndFrame();
	}
}
