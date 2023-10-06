// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanSwapChain.h: Vulkan viewport RHI definitions.
=============================================================================*/

#pragma once

class FVulkanTexture;

namespace VulkanRHI
{
	class FFence;
}

class FVulkanQueue;

struct FVulkanSwapChainRecreateInfo
{
	VkSwapchainKHR SwapChain;
	VkSurfaceKHR Surface;
};


class FVulkanSwapChain
{
public:
	FVulkanSwapChain(VkInstance InInstance, FVulkanDevice& InDevice, void* WindowHandle, EPixelFormat& InOutPixelFormat, uint32 Width, uint32 Height, bool bIsFullscreen,
		uint32* InOutDesiredNumBackBuffers, TArray<VkImage>& OutImages, int8 bLockToVsync, FVulkanSwapChainRecreateInfo* RecreateInfo);

	void Destroy(FVulkanSwapChainRecreateInfo* RecreateInfo);

	// Has to be negative as we use this also on other callbacks as the acquired image index
	enum class EStatus
	{
		Healthy = 0,
		OutOfDate = -1,
		SurfaceLost = -2,
	};
	EStatus Present(FVulkanQueue* GfxQueue, FVulkanQueue* PresentQueue, VulkanRHI::FSemaphore* BackBufferRenderingDoneSemaphore);

	void RenderThreadPacing();
	inline int8 DoesLockToVsync() { return LockToVsync; }

	const FVulkanView* GetOrCreateQCOMDepthStencilView(const FVulkanTexture& InSurface) const;
	const FVulkanView* GetOrCreateQCOMDepthView(const FVulkanTexture& InSurface) const;
	const FVulkanTexture* GetQCOMDepthStencilSurface() const;

protected:
	VkSurfaceTransformFlagBitsKHR QCOMRenderPassTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	VkFormat ImageFormat = VK_FORMAT_UNDEFINED;

	VkSwapchainKHR SwapChain;
	FVulkanDevice& Device;

	VkSurfaceKHR Surface;
	void* WindowHandle;
		
	int32 CurrentImageIndex;
	int32 SemaphoreIndex;
	uint32 NumPresentCalls;
	uint32 NumAcquireCalls;
	uint32 InternalWidth = 0;
	uint32 InternalHeight = 0;
	bool bInternalFullScreen = false;

	uint32 RTPacingSampleCount = 0;
	double RTPacingPreviousFrameCPUTime = 0;
	double RTPacingSampledDeltaTimeMS = 0;
	
	double NextPresentTargetTime = 0;

	VkInstance Instance;
	TArray<VulkanRHI::FSemaphore*> ImageAcquiredSemaphore;
#if VULKAN_USE_IMAGE_ACQUIRE_FENCES
	TArray<VulkanRHI::FFence*> ImageAcquiredFences;
#endif
	int8 LockToVsync;

	uint32 PresentID = 0;

	int32 AcquireImageIndex(VulkanRHI::FSemaphore** OutSemaphore);

	// WA: if the swapchain pass uses a depth target, it must have same size as the swapchain images.
	// For example in case if QCOMRenderPassTransform is VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR we must swap width/height of depth target.
	// But probably engine can use same depth texture for swapchain and NON swapchain passes. So it is why we have this addditional surface.
	// With this approach we should be careful in case if depth in swapchain pass is used as attachement and fetched in shader in same time
	void CreateQCOMDepthStencil(const FVulkanTexture& InSurface) const;
	mutable FVulkanTexture* QCOMDepthStencilSurface = nullptr;
	mutable FVulkanView* QCOMDepthStencilView = nullptr;
	mutable FVulkanView* QCOMDepthView = nullptr;

	friend class FVulkanViewport;
	friend class FVulkanQueue;
};

