// Copyright Epic Games, Inc. All Rights Reserved.

#include "Synchronization/DisplayClusterFrameQueueItem.h"

#include "DisplayClusterConfigurationTypes.h"
#include "DisplayClusterMediaLog.h"
#include "DisplayClusterRootActor.h"

#include "IDisplayCluster.h"
#include "Render/IDisplayClusterRenderManager.h"
#include "Render/Viewport/IDisplayClusterViewportManager.h"
#include "Render/Viewport/IDisplayClusterViewportManagerProxy.h"

#include "RHICommandList.h"


FDisplayClusterFrameQueueItem::FDisplayClusterFrameQueueItem(const FDisplayClusterFrameQueueItem& Other)
{
	for (const TPair<FString, FDisplayClusterFrameQueueItemView>& View : Other.Views)
	{
		FDisplayClusterFrameQueueItemView& NewItem = this->Views.Emplace(View.Key, View.Value);
		if (View.Value.Texture.IsValid())
		{
			NewItem.Texture = CreateTexture(View.Value.Texture);
			FRHICommandListImmediate& RHICmdList = GetImmediateCommandList_ForRenderCommand();
			RHICmdList.CopyTexture(View.Value.Texture.GetReference(), NewItem.Texture, FRHICopyTextureInfo());
		}
	}
}

void FDisplayClusterFrameQueueItem::SaveView(FRHICommandListImmediate& RHICmdList, const FString& ViewportId, FRHITexture* Texture)
{
	checkSlow(Texture);

	// Find proper view and copy texture data
	FDisplayClusterFrameQueueItemView* View = Views.Find(ViewportId);
	if (!View)
	{
		View = &Views.Emplace(ViewportId);
	}

	// Create texture if not yet available. Or re-create if the source texture has been updated (re-sized, other format, etc.)
	if (!View->Texture.IsValid() ||
		Texture->GetFormat() != View->Texture.GetReference()->GetFormat() ||
		Texture->GetDesc().Extent != View->Texture.GetReference()->GetDesc().Extent)
	{
		View->Texture = CreateTexture(Texture);
		check(View->Texture.IsValid());
	}

	// Copy texture data
	if (View->Texture.IsValid())
	{
		RHICmdList.CopyTexture(Texture, View->Texture.GetReference(), FRHICopyTextureInfo());
	}
}

void FDisplayClusterFrameQueueItem::LoadView(FRHICommandListImmediate& RHICmdList, const FString& ViewportId, FRHITexture* Texture)
{
	checkSlow(Texture);

	// Find proper view and copy texture data
	const FDisplayClusterFrameQueueItemView* const View = Views.Find(ViewportId);
	if (View && View->Texture.IsValid())
	{
		RHICmdList.CopyTexture(View->Texture.GetReference(), Texture, FRHICopyTextureInfo());
	}
}

void FDisplayClusterFrameQueueItem::SaveData(FRHICommandListImmediate& RHICmdList, const FString& ViewportId, FDisplayClusterShaderParameters_WarpBlend& WarpBlendParameters, FDisplayClusterShaderParameters_ICVFX& ICVFXParameters)
{
	if (FDisplayClusterFrameQueueItemView* View = Views.Find(ViewportId))
	{
		// Warp/blend - cherry pick necessary parameters only
		View->WarpBlendData.bRenderAlphaChannel = WarpBlendParameters.bRenderAlphaChannel;
		View->WarpBlendData.Context = WarpBlendParameters.Context;

		// ICVFX - cherry pick necessary parameters only
		View->IcvfxData.Cameras = ICVFXParameters.Cameras;
	}
}

void FDisplayClusterFrameQueueItem::LoadData(FRHICommandListImmediate& RHICmdList, const FString& ViewportId, FDisplayClusterShaderParameters_WarpBlend& WarpBlendParameters, FDisplayClusterShaderParameters_ICVFX& ICVFXParameters)
{
	if (const FDisplayClusterFrameQueueItemView* const View = Views.Find(ViewportId))
	{
		// Warp/blend - cherry pick necessary parameters only
		WarpBlendParameters.bRenderAlphaChannel = View->WarpBlendData.bRenderAlphaChannel;
		WarpBlendParameters.Context = View->WarpBlendData.Context;

		// ICVFX - cherry pick necessary parameters only
		for (const FDisplayClusterShaderParameters_ICVFX::FCameraSettings& CameraSettings : View->IcvfxData.Cameras)
		{
			// Look up for proper camera settings object. Here we use ViewportId as a key to compare.
			FDisplayClusterShaderParameters_ICVFX::FCameraSettings* TargetCameraSettings = ICVFXParameters.Cameras.FindByPredicate([CameraSettings](const FDisplayClusterShaderParameters_ICVFX::FCameraSettings& Item)
				{
					return Item.Resource.ViewportId.Equals(CameraSettings.Resource.ViewportId, ESearchCase::IgnoreCase);
				});

			// Update camera settings
			if (TargetCameraSettings)
			{
				TargetCameraSettings->SoftEdge                 = CameraSettings.SoftEdge;
				TargetCameraSettings->InnerCameraBorderColor      = CameraSettings.InnerCameraBorderColor;
				TargetCameraSettings->InnerCameraBorderThickness  = CameraSettings.InnerCameraBorderThickness;
				TargetCameraSettings->InnerCameraFrameAspectRatio = CameraSettings.InnerCameraFrameAspectRatio;
				TargetCameraSettings->Local2WorldTransform     = CameraSettings.Local2WorldTransform;
				TargetCameraSettings->CameraViewRotation       = CameraSettings.CameraViewRotation;
				TargetCameraSettings->CameraViewLocation       = CameraSettings.CameraViewLocation;
				TargetCameraSettings->CameraPrjMatrix          = CameraSettings.CameraPrjMatrix;
				TargetCameraSettings->ChromakeySource          = CameraSettings.ChromakeySource;
				TargetCameraSettings->ChromakeyColor           = CameraSettings.ChromakeyColor;
				TargetCameraSettings->ChromakeyMarkersColor    = CameraSettings.ChromakeyMarkersColor;
				TargetCameraSettings->ChromakeyMarkersScale    = CameraSettings.ChromakeyMarkersScale;
				TargetCameraSettings->ChromakeyMarkersDistance = CameraSettings.ChromakeyMarkersDistance;
				TargetCameraSettings->ChromakeyMarkersOffset   = CameraSettings.ChromakeyMarkersOffset;
				TargetCameraSettings->RenderOrder              = CameraSettings.RenderOrder;
			}
		}
	}
}

FTextureRHIRef FDisplayClusterFrameQueueItem::CreateTexture(FRHITexture* ReferenceTexture)
{
	if (ReferenceTexture)
	{
		// Use original format and size
		const int32 SizeX = ReferenceTexture->GetDesc().Extent.X;
		const int32 SizeY = ReferenceTexture->GetDesc().Extent.Y;
		const EPixelFormat Format = ReferenceTexture->GetFormat();

		// Prepare description
		FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("DisplayClusterFrameQueueCacheTexture"), SizeX, SizeY, Format)
			.SetClearValue(FClearValueBinding::Black)
			.SetNumMips(1)
			.SetFlags(ETextureCreateFlags::Dynamic)
			.AddFlags(ETextureCreateFlags::MultiGPUGraphIgnore)
			.SetInitialState(ERHIAccess::SRVMask);

		// Leave original flags, but make sure it's ResolveTargetable but not RenderTargetable
		ETextureCreateFlags Flags = ReferenceTexture->GetFlags();
		Flags &= ~ETextureCreateFlags::RenderTargetable;
		Flags |= ETextureCreateFlags::ResolveTargetable;
		Desc.SetFlags(Flags);

		// Create texture
		return RHICreateTexture(Desc);
	}

	return nullptr;
}
