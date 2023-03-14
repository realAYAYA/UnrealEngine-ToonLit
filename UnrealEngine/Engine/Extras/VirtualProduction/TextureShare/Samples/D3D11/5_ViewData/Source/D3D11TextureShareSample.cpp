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
	if (TextureShareObject)
	{
		// Iterate frame marker values
		TextureShareObject->GetData().FrameMarker.NextFrame();
		DEBUG_LOG(TEXT("Set new Frame marker %d from process %s"), TextureShareObject->GetData().FrameMarker.CustomFrameIndex1, *TextureShareObject->GetObjectDesc().ProcessName);

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
						const FVector&  ViewLocation = SceneViewDataIt.View.ViewLocation;
						const FMatrix&     PrjMatrix = SceneViewDataIt.View.ViewMatrices.ProjectionMatrix;

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

void FD3D11TextureShareSample::EndFrame(const FTextureShareDeviceContextD3D11& InDeviceContext, ID3D11Texture2D* InBackBufferTexture)
{
	if (TextureShareObject)
	{
		TextureShareObject->EndFrame();
	}
}
