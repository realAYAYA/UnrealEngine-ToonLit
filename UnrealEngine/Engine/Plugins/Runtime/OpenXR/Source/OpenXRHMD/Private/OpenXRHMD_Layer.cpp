// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenXRHMD_Layer.h"
#include "OpenXRCore.h"
#include "OpenXRPlatformRHI.h"

bool FOpenXRLayer::NeedReallocateRightTexture()
{
	if (!Desc.Texture.IsValid())
	{
		return false;
	}

	FRHITexture2D* Texture = Desc.Texture->GetTexture2D();
	if (!Texture)
	{
		return false;
	}

	if (!RightEye.Swapchain.IsValid())
	{
		return true;
	}

	return RightEye.SwapchainSize != Texture->GetSizeXY();
}

bool FOpenXRLayer::NeedReallocateLeftTexture()
{
	if (!Desc.LeftTexture.IsValid())
	{
		return false;
	}

	FRHITexture2D* Texture = Desc.LeftTexture->GetTexture2D();
	if (!Texture)
	{
		return false;
	}

	if (!LeftEye.Swapchain.IsValid())
	{
		return true;
	}

	return LeftEye.SwapchainSize != Texture->GetSizeXY();
}

FIntRect FOpenXRLayer::GetRightViewportSize() const
{
	FBox2D Viewport(RightEye.SwapchainSize * Desc.UVRect.Min, RightEye.SwapchainSize * Desc.UVRect.Max);
	return FIntRect(Viewport.Min.IntPoint(), Viewport.Max.IntPoint());
}

FIntRect FOpenXRLayer::GetLeftViewportSize() const
{
	FBox2D Viewport(LeftEye.SwapchainSize * Desc.UVRect.Min, LeftEye.SwapchainSize * Desc.UVRect.Max);
	return FIntRect(Viewport.Min.IntPoint(), Viewport.Max.IntPoint());
}

FVector2D FOpenXRLayer::GetRightQuadSize() const
{
	if (Desc.Flags & IStereoLayers::LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO)
	{
		float AspectRatio = RightEye.SwapchainSize.Y / RightEye.SwapchainSize.X;
		return FVector2D(Desc.QuadSize.X, Desc.QuadSize.X * AspectRatio);
	}
	return Desc.QuadSize;
}

FVector2D FOpenXRLayer::GetLeftQuadSize() const
{
	if (Desc.Flags & IStereoLayers::LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO)
	{
		float AspectRatio = LeftEye.SwapchainSize.Y / LeftEye.SwapchainSize.X;
		return FVector2D(Desc.QuadSize.X, Desc.QuadSize.X * AspectRatio);
	}
	return Desc.QuadSize;
}

// TStereoLayerManager helper functions

bool GetLayerDescMember(const FOpenXRLayer& Layer, IStereoLayers::FLayerDesc& OutLayerDesc)
{
	OutLayerDesc = Layer.Desc;
	return true;
}

void SetLayerDescMember(FOpenXRLayer& Layer, const IStereoLayers::FLayerDesc& Desc)
{
	Layer.Desc = Desc;
}

void MarkLayerTextureForUpdate(FOpenXRLayer& Layer)
{
	// If the swapchain is static we need to re-allocate it before it can be updated
	if (!(Layer.Desc.Flags & IStereoLayers::LAYER_FLAG_TEX_CONTINUOUS_UPDATE))
	{
		Layer.RightEye.Swapchain.Reset();
		Layer.LeftEye.Swapchain.Reset();
	}
	Layer.RightEye.bUpdateTexture = true;
	Layer.LeftEye.bUpdateTexture = true;
}
