// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanViewport.h: Vulkan viewport RHI definitions.
=============================================================================*/

#pragma once

#include "VulkanResources.h"
#include "HAL/CriticalSection.h"

class FVulkanDynamicRHI;
class FVulkanSwapChain;
class FVulkanQueue;
class FVulkanViewport;

namespace VulkanRHI
{
	class FSemaphore;
}

class FVulkanBackBuffer : public FVulkanTexture
{
public:
	FVulkanBackBuffer(FVulkanDevice& Device, FVulkanViewport* InViewport, EPixelFormat Format, uint32 SizeX, uint32 SizeY, ETextureCreateFlags UEFlags);
	virtual ~FVulkanBackBuffer();
	
	void OnLayoutTransition(FVulkanCommandListContext& Context, VkImageLayout NewLayout) override final;

	void OnGetBackBufferImage(FRHICommandListImmediate& RHICmdList);
	void OnAdvanceBackBufferFrame(FRHICommandListImmediate& RHICmdList);

	void ReleaseViewport();
	void ReleaseAcquiredImage();

private:
	void AcquireBackBufferImage(FVulkanCommandListContext& Context);

private:
	FVulkanViewport* Viewport;
};


class FVulkanViewport : public FRHIViewport, public VulkanRHI::FDeviceChild
{
public:
	enum { NUM_BUFFERS = 3 };

	FVulkanViewport(FVulkanDevice* InDevice, void* InWindowHandle, uint32 InSizeX,uint32 InSizeY,bool bInIsFullscreen, EPixelFormat InPreferredPixelFormat);
	~FVulkanViewport();

	FTexture2DRHIRef GetBackBuffer(FRHICommandListImmediate& RHICmdList);
	void AdvanceBackBufferFrame(FRHICommandListImmediate& RHICmdList);

	virtual void WaitForFrameEventCompletion() override;

	virtual void IssueFrameEvent() override;

	inline FIntPoint GetSizeXY() const
	{
		return FIntPoint(SizeX, SizeY);
	}

	virtual void SetCustomPresent(FRHICustomPresent* InCustomPresent) override final
	{
		CustomPresent = InCustomPresent;
	}

	virtual FRHICustomPresent* GetCustomPresent() const override final
	{
		return CustomPresent;
	}

	virtual void Tick(float DeltaTime) override final;
	
	bool Present(FVulkanCommandListContext* Context, FVulkanCmdBuffer* CmdBuffer, FVulkanQueue* Queue, FVulkanQueue* PresentQueue, bool bLockToVsync);

	inline uint32 GetPresentCount() const
	{
		return PresentCount;
	}

	inline bool IsFullscreen() const
	{
		return bIsFullscreen;
	}

	inline uint32 GetBackBufferImageCount()
	{
		return (uint32)BackBufferImages.Num();
	}

	inline VkImage GetBackBufferImage(uint32 Index)
	{
		if (BackBufferImages.Num() > 0)
		{
			return BackBufferImages[Index];
		}
		else
		{
			return VK_NULL_HANDLE;
		}
	}

	inline FVulkanSwapChain* GetSwapChain()
	{
		return SwapChain;
	}

	VkSurfaceTransformFlagBitsKHR GetSwapchainQCOMRenderPassTransform() const;
	VkFormat GetSwapchainImageFormat() const;

protected:
	// NUM_BUFFERS don't have to match exactly as the driver can require a minimum number larger than NUM_BUFFERS. Provide some slack
	TArray<VkImage, TInlineAllocator<NUM_BUFFERS*2>> BackBufferImages;
	TArray<VulkanRHI::FSemaphore*, TInlineAllocator<NUM_BUFFERS*2>> RenderingDoneSemaphores;
	TIndirectArray<FVulkanView, TInlineAllocator<NUM_BUFFERS*2>> TextureViews;
	TRefCountPtr<FVulkanBackBuffer> RHIBackBuffer;

	// 'Dummy' back buffer
	TRefCountPtr<FVulkanTexture>	RenderingBackBuffer;
	
	/** narrow-scoped section that locks access to back buffer during its recreation*/
	FCriticalSection RecreatingSwapchain;

	uint32 SizeX;
	uint32 SizeY;
	bool bIsFullscreen;
	EPixelFormat PixelFormat;
	int32 AcquiredImageIndex;
	FVulkanSwapChain* SwapChain;
	void* WindowHandle;
	uint32 PresentCount;
	bool bRenderOffscreen;

	int8 LockToVsync;

	// Just a pointer, not owned by this class
	VulkanRHI::FSemaphore* AcquiredSemaphore;

	FCustomPresentRHIRef CustomPresent;

	FVulkanCmdBuffer* LastFrameCommandBuffer = nullptr;
	uint64 LastFrameFenceCounter = 0;

	void CreateSwapchain(struct FVulkanSwapChainRecreateInfo* RecreateInfo);
	void DestroySwapchain(struct FVulkanSwapChainRecreateInfo* RecreateInfo);
	bool TryAcquireImageIndex();

	void RecreateSwapchain(void* NewNativeWindow);
	void RecreateSwapchainFromRT(EPixelFormat PreferredPixelFormat);
	void Resize(uint32 InSizeX, uint32 InSizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat);

	bool DoCheckedSwapChainJob(TFunction<int32(FVulkanViewport*)> SwapChainJob);
	bool SupportsStandardSwapchain();
	bool RequiresRenderingBackBuffer();
	EPixelFormat GetPixelFormatForNonDefaultSwapchain();

	friend class FVulkanDynamicRHI;
	friend class FVulkanCommandListContext;
	friend struct FRHICommandAcquireBackBuffer;
	friend class FVulkanBackBuffer;
};

template<>
struct TVulkanResourceTraits<FRHIViewport>
{
	typedef FVulkanViewport TConcreteType;
};
