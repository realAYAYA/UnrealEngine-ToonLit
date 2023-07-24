// Copyright Epic Games, Inc.All Rights Reserved.

/*=============================================================================
StereoRenderTargetManager.h: Abstract interface returned from IStereoRendering to support rendering into a texture
=============================================================================*/

#pragma once
#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "PixelFormat.h"

class FRHITexture;
enum class ETextureCreateFlags : uint64;
enum EShaderPlatform : uint16;
typedef TRefCountPtr<FRHITexture> FTexture2DRHIRef;

/** 
 * The IStereoRenderTargetManager can be returned from IStereoRendering::GetRenderTargetManager() implementations.
 * Implement this interface if a stereo rendering device requires all output to be rendered into separate render targets and/or to customize how
 * separate render targets are allocated.
 */
class IStereoRenderTargetManager
{
public:
	/** 
	 * Whether a separate render target should be used or not.
	 * In case the stereo rendering implementation does not require special handling of separate render targets 
	 * at all, it can leave out implementing this interface completely and simply let the default implementation 
	 * of IStereoRendering::GetRenderTargetManager() return nullptr.
	 */
	virtual bool ShouldUseSeparateRenderTarget() const = 0;

	/**
	 * Updates viewport for direct rendering of distortion. Should be called on a game thread.
	 *
	 * @param bUseSeparateRenderTarget	Set to true if a separate render target will be used. Can potentiallt be true even if ShouldUseSeparateRenderTarget() returned false earlier.
	 * @param Viewport					The Viewport instance calling this method.
	 * @param ViewportWidget			(optional) The Viewport widget containing the view. Can be used to access SWindow object.
	 */
	virtual void UpdateViewport(bool bUseSeparateRenderTarget, const class FViewport& Viewport, class SViewport* ViewportWidget = nullptr) = 0;

	/**
	 * Calculates dimensions of the render target texture for direct rendering of distortion.
	 */
	virtual void CalculateRenderTargetSize(const class FViewport& Viewport, uint32& InOutSizeX, uint32& InOutSizeY) = 0;

	/**
	 * Returns true, if render target texture must be re-calculated.
	 */
	virtual bool NeedReAllocateViewportRenderTarget(const class FViewport& Viewport) = 0;

	/**
	* Returns true, if render target texture must be re-calculated.
	*/
	UE_DEPRECATED(5.2, "Return true in NeedReAllocateViewportRenderTarget instead")
	virtual bool NeedReAllocateDepthTexture(const TRefCountPtr<struct IPooledRenderTarget>& DepthTarget) { return false; }

	/**
	* Returns true, if shading rate texture must be re-calculated.
	*/
	virtual bool NeedReAllocateShadingRateTexture(const TRefCountPtr<struct IPooledRenderTarget>& ShadingRateTarget) { return false; }

	/**
	 * Returns number of required buffered frames.
	 */
	UE_DEPRECATED(5.2, "Inferred from the array size returned in AllocateRenderTargetTextures")
	virtual uint32 GetNumberOfBufferedFrames() const { return 1; }

	/**
	 * Allocates a render target texture.
	 * The default implementation always return false to indicate that the default texture allocation should be used instead.
	 *
	 * @param Index			(in) index of the buffer, changing from 0 to GetNumberOfBufferedFrames()
	 * @return				true, if texture was allocated; false, if the default texture allocation should be used.
	 */
	UE_DEPRECATED(5.2, "Implement AllocateRenderTargetTextures to allocate all textures at once")
	virtual bool AllocateRenderTargetTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) { return false; }

	/**
	 * Allocates the render target textures, which includes the color textures and optionally other textures like depth.
	 * The default implementation always returns false to indicate that the default texture allocation should be used instead.
	 *
	 * @return				true, if textures were allocated; false, if the default texture allocation should be used.
	 */
	virtual bool AllocateRenderTargetTextures(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumLayers, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, TArray<FTexture2DRHIRef>& OutTargetableTextures, TArray<FTexture2DRHIRef>& OutShaderResourceTextures, uint32 NumSamples = 1) { return false; }

	/**
	 * Returns pixel format that the device created its swapchain with (which can be different than what was requested in AllocateRenderTargetTexture)
	 */
	virtual EPixelFormat GetActualColorSwapchainFormat() const { return PF_Unknown; }

	/**
	 * Acquires the next available color texture.
	 * 
	 * @return				the index of the texture in the array returned by AllocateRenderTargetTexture.
	 */
	virtual int32 AcquireColorTexture() { return -1; }

	/**
	 * Allocates a depth texture.
	 *
	 * @param Index			(in) index of the buffer, changing from 0 to GetNumberOfBufferedFrames()
	 * @return				true, if texture was allocated; false, if the default texture allocation should be used.
	 */
	virtual bool AllocateDepthTexture(uint32 Index, uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTargetableTexture, FTexture2DRHIRef& OutShaderResourceTexture, uint32 NumSamples = 1) { return false; }

	/**
	 * Allocates a shading rate texture.
	 * The default implementation always returns false to indicate that the default texture allocation should be used instead.
	 *
	 * @param Index			(in) index of the buffer, changing from 0 to GetNumberOfBufferedFrames()
	 * @return				true, if texture was allocated; false, if the default texture allocation should be used.
	 */
	virtual bool AllocateShadingRateTexture(uint32 Index, uint32 RenderSizeX, uint32 RenderSizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ETextureCreateFlags TargetableTextureFlags, FTexture2DRHIRef& OutTexture, FIntPoint& OutTextureSize) { return false; }

	/**
	 * Retrieves HDR information about the stereo device, if any is available.
	 * The default implementation always returns false to indicate that the information from the monitor can be used instead.
	 *
	 * @param OutDisplayOutputFormat	(out) encoding used by the stereo device
	 * @param OutDisplayColorGamut		(out) color space used by the stereo device
	 * @param OutbHDRSupported			(out) whether HDR is supported by the stereo device
	 * @return							true, if HDR information is available for the stereo device
	 */
	virtual bool HDRGetMetaDataForStereo(EDisplayOutputFormat& OutDisplayOutputFormat, EDisplayColorGamut& OutDisplayColorGamut, bool& OutbHDRSupported) { return false; }

	/**
	 * In the editor, we may switch between preview shader platforms, which support various single-pass rendering methods
	 * (e.g. InstancedStereo or MobileMultiView). Sometimes RT managers have their own state that depends on the method, so this
	 * API provides them with the possibility to reconfigure it
	 */
	virtual bool ReconfigureForShaderPlatform(EShaderPlatform NewShaderPlatform) { return false; };

	static EPixelFormat GetStereoLayerPixelFormat() { return PF_B8G8R8A8; }
};