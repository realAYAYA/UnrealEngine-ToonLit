// Copyright Epic Games, Inc. All Rights Reserved.

#include "OpenXRHMD_Layer.h"
#include "IStereoLayers.h"
#include "OpenXRHMD_Swapchain.h"
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
TArray<FXrCompositionLayerUnion> FOpenXRLayer::CreateOpenXRLayer(FTransform InvTrackingToWorld, float WorldToMeters, XrSpace Space) const
{
	TArray<FXrCompositionLayerUnion> Headers;

	const bool bNoAlpha = Desc.Flags & IStereoLayers::LAYER_FLAG_TEX_NO_ALPHA_CHANNEL;
	const bool bIsStereo = Desc.LeftTexture.IsValid();
	FTransform PositionTransform = Desc.PositionType == IStereoLayers::ELayerType::WorldLocked ?
		InvTrackingToWorld : FTransform::Identity;

	if (Desc.HasShape<FQuadLayer>())
	{
		CreateOpenXRQuadLayer(bIsStereo, bNoAlpha, PositionTransform, WorldToMeters, Space, Headers);
	}
	else if (Desc.HasShape<FCylinderLayer>())
	{
		CreateOpenXRCylinderLayer(bIsStereo, bNoAlpha, PositionTransform, WorldToMeters, Space, Headers);
	}
	else if (Desc.HasShape<FEquirectLayer>())
	{
		CreateOpenXREquirectLayer(bIsStereo, bNoAlpha, PositionTransform, WorldToMeters, Space, Headers);
	}

	return Headers;
}

void FOpenXRLayer::CreateOpenXRCylinderLayer(bool bIsStereo, bool bNoAlpha, FTransform PositionTransform, float WorldToMeters, XrSpace Space, TArray<FXrCompositionLayerUnion>& Headers) const
{
	XrCompositionLayerCylinderKHR Cylinder = { XR_TYPE_COMPOSITION_LAYER_CYLINDER_KHR, /*next*/ nullptr };
	Cylinder.layerFlags = bNoAlpha ? 0 : XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT |
		XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
	Cylinder.space = Space;
	Cylinder.subImage.imageArrayIndex = 0;
	Cylinder.pose = ToXrPose(Desc.Transform * PositionTransform, WorldToMeters);

	const FCylinderLayer& CylinderProps = Desc.GetShape<FCylinderLayer>();
	Cylinder.radius = FMath::Abs(CylinderProps.Radius / WorldToMeters);
	Cylinder.centralAngle = FMath::Min((float)(2.0f * PI), FMath::Abs(CylinderProps.OverlayArc / CylinderProps.Radius));
	Cylinder.aspectRatio = FMath::Abs(CylinderProps.OverlayArc / CylinderProps.Height);

	FXrCompositionLayerUnion LayerUnion;
	LayerUnion.Cylinder = Cylinder;

	if (RightEye.Swapchain.IsValid())
	{
		LayerUnion.Cylinder.eyeVisibility = bIsStereo ? XR_EYE_VISIBILITY_RIGHT : XR_EYE_VISIBILITY_BOTH;
		LayerUnion.Cylinder.subImage.imageRect = ToXrRect(GetRightViewportSize());
		LayerUnion.Cylinder.subImage.swapchain = static_cast<FOpenXRSwapchain*>(RightEye.Swapchain.Get())->GetHandle();
		Headers.Add(LayerUnion);
	}
	if (LeftEye.Swapchain.IsValid())
	{
		LayerUnion.Cylinder.eyeVisibility = XR_EYE_VISIBILITY_LEFT;
		LayerUnion.Cylinder.subImage.imageRect = ToXrRect(GetLeftViewportSize());
		LayerUnion.Cylinder.subImage.swapchain = static_cast<FOpenXRSwapchain*>(LeftEye.Swapchain.Get())->GetHandle();
		Headers.Add(LayerUnion);
	}
}

void FOpenXRLayer::CreateOpenXRQuadLayer(bool bIsStereo, bool bNoAlpha, FTransform PositionTransform, float WorldToMeters, XrSpace Space, TArray<FXrCompositionLayerUnion>& Headers) const
{
	XrCompositionLayerQuad Quad = { XR_TYPE_COMPOSITION_LAYER_QUAD, nullptr };
	Quad.layerFlags = bNoAlpha ? 0 : XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT |
		XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
	Quad.space = Space;
	Quad.subImage.imageArrayIndex = 0;
	Quad.pose = ToXrPose(Desc.Transform * PositionTransform, WorldToMeters);

	// The layer pose doesn't take the transform scale into consideration, so we need to manually apply it the quad size.
	const FVector2D LayerComponentScaler(Desc.Transform.GetScale3D().Y, Desc.Transform.GetScale3D().Z);

	FXrCompositionLayerUnion LayerUnion;
	LayerUnion.Quad = Quad;
	
	// We need to copy each layer into an OpenXR swapchain so they can be displayed by the compositor
	if (RightEye.Swapchain.IsValid())
	{
		LayerUnion.Quad.eyeVisibility = bIsStereo ? XR_EYE_VISIBILITY_RIGHT : XR_EYE_VISIBILITY_BOTH;
		LayerUnion.Quad.subImage.imageRect = ToXrRect(GetRightViewportSize());
		LayerUnion.Quad.subImage.swapchain = static_cast<FOpenXRSwapchain*>(RightEye.Swapchain.Get())->GetHandle();
		LayerUnion.Quad.size = ToXrExtent2D(GetRightQuadSize() * LayerComponentScaler, WorldToMeters);
		Headers.Add(LayerUnion);
	}
	if (LeftEye.Swapchain.IsValid())
	{
		LayerUnion.Quad.eyeVisibility = XR_EYE_VISIBILITY_LEFT;
		LayerUnion.Quad.subImage.imageRect = ToXrRect(GetLeftViewportSize());
		LayerUnion.Quad.subImage.swapchain = static_cast<FOpenXRSwapchain*>(LeftEye.Swapchain.Get())->GetHandle();
		LayerUnion.Quad.size = ToXrExtent2D(GetLeftQuadSize() * LayerComponentScaler, WorldToMeters);
		Headers.Add(LayerUnion);
	}
}

void FOpenXRLayer::CreateOpenXREquirectLayer(bool bIsStereo, bool bNoAlpha, FTransform PositionTransform, float WorldToMeters, XrSpace Space, TArray<FXrCompositionLayerUnion>& Headers) const
{
	XrCompositionLayerEquirectKHR Equirect = { XR_TYPE_COMPOSITION_LAYER_EQUIRECT_KHR, nullptr };
	Equirect.layerFlags = bNoAlpha ? 0 : XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT |
		XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
	Equirect.space = Space;
	Equirect.subImage.imageArrayIndex = 0;
	Equirect.pose = ToXrPose(Desc.Transform * PositionTransform, WorldToMeters);

	const FEquirectLayer& EquirectProps = Desc.GetShape<FEquirectLayer>();

	// An equirect layer with a radius of 0 is an infinite sphere.
	// As of UE 5.3, equirect layers are supported only by the Oculus OpenXR runtime and 
	// only with a radius of 0. Other radius values will be ignored.
	Equirect.radius = FMath::Abs(EquirectProps.Radius / WorldToMeters);

	FXrCompositionLayerUnion LayerUnion;
	LayerUnion.Equirect = Equirect;

	// We need to copy each layer into an OpenXR swapchain so they can be displayed by the compositor
	if (RightEye.Swapchain.IsValid())
	{
		LayerUnion.Equirect.eyeVisibility = bIsStereo ? XR_EYE_VISIBILITY_RIGHT : XR_EYE_VISIBILITY_BOTH;
		LayerUnion.Equirect.subImage.imageRect = ToXrRect(GetRightViewportSize());
		LayerUnion.Equirect.subImage.swapchain = static_cast<FOpenXRSwapchain*>(RightEye.Swapchain.Get())->GetHandle();
		LayerUnion.Equirect.scale = ToXrVector2f(EquirectProps.RightScale);
		LayerUnion.Equirect.bias = ToXrVector2f(EquirectProps.RightBias);
		Headers.Add(LayerUnion);
	}
	if (LeftEye.Swapchain.IsValid())
	{
		LayerUnion.Equirect.eyeVisibility = XR_EYE_VISIBILITY_LEFT;
		LayerUnion.Equirect.subImage.imageRect = ToXrRect(GetLeftViewportSize());
		LayerUnion.Equirect.subImage.swapchain = static_cast<FOpenXRSwapchain*>(LeftEye.Swapchain.Get())->GetHandle();
		LayerUnion.Equirect.scale = ToXrVector2f(EquirectProps.LeftScale);
		LayerUnion.Equirect.bias = ToXrVector2f(EquirectProps.LeftBias);
		Headers.Add(LayerUnion);
	}
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
