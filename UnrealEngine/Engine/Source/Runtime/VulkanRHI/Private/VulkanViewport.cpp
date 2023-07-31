// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	VulkanViewport.cpp: Vulkan viewport RHI implementation.
=============================================================================*/

#include "VulkanRHIPrivate.h"
#include "VulkanSwapChain.h"
#include "VulkanPendingState.h"
#include "VulkanContext.h"
#include "VulkanBarriers.h"
#include "GlobalShader.h"
#include "HAL/PlatformAtomics.h"
#include "Engine/RendererSettings.h"


FVulkanBackBuffer::FVulkanBackBuffer(FVulkanDevice& Device, FVulkanViewport* InViewport, EPixelFormat Format, uint32 SizeX, uint32 SizeY, ETextureCreateFlags UEFlags)
	: FVulkanTexture(Device, FRHITextureCreateDesc::Create2D(TEXT("FVulkanBackBuffer"), SizeX, SizeY, Format).SetFlags(UEFlags).DetermineInititialState(), VK_NULL_HANDLE, false)
	, Viewport(InViewport)
{
}

void FVulkanBackBuffer::ReleaseAcquiredImage()
{
	DefaultView.View = VK_NULL_HANDLE;
	DefaultView.ViewId = 0;
	Image = VK_NULL_HANDLE;
}

void FVulkanBackBuffer::ReleaseViewport()
{
	Viewport = nullptr;
	ReleaseAcquiredImage();
}

void FVulkanBackBuffer::OnGetBackBufferImage(FRHICommandListImmediate& RHICmdList)
{
	check(Viewport);
	if (GVulkanDelayAcquireImage == EDelayAcquireImageType::None)
	{
		FVulkanCommandListContext& Context = (FVulkanCommandListContext&)RHICmdList.GetContext().GetLowestLevelContext();
		AcquireBackBufferImage(Context);
	}
}

void FVulkanBackBuffer::OnAdvanceBackBufferFrame(FRHICommandListImmediate& RHICmdList)
{
	check(Viewport);
	ReleaseAcquiredImage();
}

void FVulkanBackBuffer::OnLayoutTransition(FVulkanCommandListContext& Context, VkImageLayout NewLayout)
{
	if (GVulkanDelayAcquireImage == EDelayAcquireImageType::LazyAcquire)
	{
		AcquireBackBufferImage(Context);
	}
}

void FVulkanBackBuffer::AcquireBackBufferImage(FVulkanCommandListContext& Context)
{
	check(Viewport);
	
	if (Image == VK_NULL_HANDLE)
	{
		if (Viewport->TryAcquireImageIndex())
		{
			int32 AcquiredImageIndex = Viewport->AcquiredImageIndex;
			check(AcquiredImageIndex >= 0 && AcquiredImageIndex < Viewport->TextureViews.Num());

			FVulkanTextureView& ImageView = Viewport->TextureViews[AcquiredImageIndex];

			Image = ImageView.Image;
			DefaultView.View = ImageView.View;
			DefaultView.ViewId = ImageView.ViewId;

			// right after acquiring image is in undefined state
			FVulkanLayoutManager& LayoutMgr = Context.GetLayoutManager();
			VkImageLayout& CurrentLayout = LayoutMgr.FindOrAddLayoutRW(ImageView.Image, VK_IMAGE_LAYOUT_UNDEFINED, 1, 1);
			CurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			FVulkanCommandBufferManager* CmdBufferManager = Context.GetCommandBufferManager();
			FVulkanCmdBuffer* CmdBuffer = CmdBufferManager->GetActiveCmdBuffer();
			check(!CmdBuffer->IsInsideRenderPass());

			// Wait for semaphore signal before writing to backbuffer image
			CmdBuffer->AddWaitSemaphore(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, Viewport->AcquiredSemaphore);
		}
		else
		{
			// fallback to a 'dummy' backbuffer
			check(Viewport->RenderingBackBuffer);
			const FVulkanTextureView& DummyView = Viewport->RenderingBackBuffer->DefaultView;
			Image = DummyView.Image;
			DefaultView.View = DummyView.View;
			DefaultView.ViewId = DummyView.ViewId;
		}
	}
}

FVulkanBackBuffer::~FVulkanBackBuffer()
{
	check(IsImageOwner() == false);
	// Clear ImageOwnerType so ~FVulkanTexture2D() doesn't try to re-destroy it
	ImageOwnerType = EImageOwnerType::None;
	ReleaseAcquiredImage();
}

FVulkanViewport::FVulkanViewport(FVulkanDynamicRHI* InRHI, FVulkanDevice* InDevice, void* InWindowHandle, uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen, EPixelFormat InPreferredPixelFormat)
	: VulkanRHI::FDeviceChild(InDevice)
	, RHI(InRHI)
	, SizeX(InSizeX)
	, SizeY(InSizeY)
	, bIsFullscreen(bInIsFullscreen)
	, PixelFormat(InPreferredPixelFormat)
	, AcquiredImageIndex(-1)
	, SwapChain(nullptr)
	, WindowHandle(InWindowHandle)
	, PresentCount(0)
	, bRenderOffscreen(false)
	, LockToVsync(1)
	, AcquiredSemaphore(nullptr)
{
	check(IsInGameThread());
	FMemory::Memzero(BackBufferImages);
	RHI->Viewports.Add(this);

	// Make sure Instance is created
	RHI->InitInstance();

	bRenderOffscreen = FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen"));
	CreateSwapchain(nullptr);

	if (SupportsStandardSwapchain())
	{
		for (int32 Index = 0, NumBuffers = RenderingDoneSemaphores.Num(); Index < NumBuffers; ++Index)
		{
			RenderingDoneSemaphores[Index] = new VulkanRHI::FSemaphore(*InDevice);
			RenderingDoneSemaphores[Index]->AddRef();
		}
	}
}

FVulkanViewport::~FVulkanViewport()
{
	RenderingBackBuffer = nullptr;
	
	if (RHIBackBuffer)
	{
		RHIBackBuffer->ReleaseViewport();
		RHIBackBuffer = nullptr;
	}
	
	if (SupportsStandardSwapchain())
	{
		for (int32 Index = 0, NumBuffers = RenderingDoneSemaphores.Num(); Index < NumBuffers; ++Index)
		{
			RenderingDoneSemaphores[Index]->Release();

			TextureViews[Index].Destroy(*Device);

			// FIXME: race condition on TransitionAndLayoutManager, could this be called from RT while RHIT is active?
			Device->NotifyDeletedImage(BackBufferImages[Index], true);
			BackBufferImages[Index] = VK_NULL_HANDLE;
		}

		SwapChain->Destroy(nullptr);
		delete SwapChain;
		SwapChain = nullptr;
	}

	RHI->Viewports.Remove(this);
}

bool FVulkanViewport::DoCheckedSwapChainJob(TFunction<int32(FVulkanViewport*)> SwapChainJob)
{
	int32 AttemptsPending = FVulkanPlatform::RecreateSwapchainOnFail() ? 4 : 0;
	int32 Status = SwapChainJob(this);

	while (Status < 0 && AttemptsPending > 0)
	{
		if (Status == (int32)FVulkanSwapChain::EStatus::OutOfDate)
		{
			UE_LOG(LogVulkanRHI, Verbose, TEXT("Swapchain is out of date! Trying to recreate the swapchain."));
		}
		else if (Status == (int32)FVulkanSwapChain::EStatus::SurfaceLost)
		{
			UE_LOG(LogVulkanRHI, Warning, TEXT("Swapchain surface lost! Trying to recreate the swapchain."));
		}
		else
		{
			check(0);
		}

		RecreateSwapchain(WindowHandle);

		// Swapchain creation pushes some commands - flush the command buffers now to begin with a fresh state
		Device->SubmitCommandsAndFlushGPU();
		Device->WaitUntilIdle();

		Status = SwapChainJob(this);

		--AttemptsPending;
	}

	return Status >= 0;
}

bool FVulkanViewport::TryAcquireImageIndex()
{
	if (SwapChain)
	{
		int32 Result = SwapChain->AcquireImageIndex(&AcquiredSemaphore);
		if (Result >= 0)
		{
			AcquiredImageIndex = Result;
			return true;
		}
	}
	return false;
}

FTexture2DRHIRef FVulkanViewport::GetBackBuffer(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());

	// make sure we aren't in the middle of swapchain recreation (which can happen on e.g. RHI thread)
	FScopeLock LockSwapchain(&RecreatingSwapchain);

	if (SupportsStandardSwapchain() && GVulkanDelayAcquireImage != EDelayAcquireImageType::DelayAcquire)
	{
		check(RHICmdList.IsImmediate());
		check(RHIBackBuffer);
		
		RHICmdList.EnqueueLambda([this](FRHICommandListImmediate& CmdList)
		{
			this->RHIBackBuffer->OnGetBackBufferImage(CmdList);
		});

		return RHIBackBuffer.GetReference();
	}
	
	return RenderingBackBuffer.GetReference();
}

void FVulkanViewport::AdvanceBackBufferFrame(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());

	if (SupportsStandardSwapchain() && GVulkanDelayAcquireImage != EDelayAcquireImageType::DelayAcquire)
	{
		check(RHIBackBuffer);
		
		RHICmdList.EnqueueLambda([this](FRHICommandListImmediate& CmdList)
		{
			this->RHIBackBuffer->OnAdvanceBackBufferFrame(CmdList);
		});
	}
}

void FVulkanViewport::WaitForFrameEventCompletion()
{
	if (FVulkanPlatform::RequiresWaitingForFrameCompletionEvent())
	{
		static FCriticalSection CS;
		FScopeLock ScopeLock(&CS);
		if (LastFrameCommandBuffer && LastFrameCommandBuffer->IsSubmitted())
		{
			// If last frame's fence hasn't been signaled already, wait for it here
			if (LastFrameFenceCounter == LastFrameCommandBuffer->GetFenceSignaledCounter())
			{
				if (!GWaitForIdleOnSubmit)
				{
					// The wait has already happened if GWaitForIdleOnSubmit is set
					LastFrameCommandBuffer->GetOwner()->GetMgr().WaitForCmdBuffer(LastFrameCommandBuffer);
				}
			}
		}
	}
}

void FVulkanViewport::IssueFrameEvent()
{
	if (FVulkanPlatform::RequiresWaitingForFrameCompletionEvent())
	{
		// The fence we need to wait on next frame is already there in the command buffer
		// that was just submitted in this frame's Present. Just grab that command buffer's
		// info to use next frame in WaitForFrameEventCompletion.
		FVulkanQueue* Queue = Device->GetGraphicsQueue();
		Queue->GetLastSubmittedInfo(LastFrameCommandBuffer, LastFrameFenceCounter);
	}
}


FVulkanFramebuffer::FVulkanFramebuffer(FVulkanDevice& Device, const FRHISetRenderTargetsInfo& InRTInfo, const FVulkanRenderTargetLayout& RTLayout, const FVulkanRenderPass& RenderPass)
	: Framebuffer(VK_NULL_HANDLE)
	, NumColorRenderTargets(InRTInfo.NumColorRenderTargets)
	, NumColorAttachments(0)
	, DepthStencilRenderTargetImage(VK_NULL_HANDLE)
	, FragmentDensityImage(VK_NULL_HANDLE)
{
	FMemory::Memzero(ColorRenderTargetImages);
	FMemory::Memzero(ColorResolveTargetImages);
		
	AttachmentTextureViews.Empty(RTLayout.GetNumAttachmentDescriptions());
	uint32 MipIndex = 0;

	const VkExtent3D& RTExtents = RTLayout.GetExtent3D();
	// Adreno does not like zero size RTs
	check(RTExtents.width != 0 && RTExtents.height != 0);
	uint32 NumLayers = RTExtents.depth;

	for (int32 Index = 0; Index < InRTInfo.NumColorRenderTargets; ++Index)
	{
		FRHITexture* RHITexture = InRTInfo.ColorRenderTarget[Index].Texture;
		if (!RHITexture)
		{
			continue;
		}

		FVulkanTexture* Texture = FVulkanTexture::Cast(RHITexture);
		const FRHITextureDesc& Desc = Texture->GetDesc();

		// this could fire in case one of the textures is FVulkanBackBuffer and it has not acquired an image
		// with EDelayAcquireImageType::LazyAcquire acquire happens when texture transition to Writeable state
		// make sure you call TransitionResource(Writable, Tex) before using this texture as a render-target
		check(Texture->Image != VK_NULL_HANDLE);

		ColorRenderTargetImages[Index] = Texture->Image;
		MipIndex = InRTInfo.ColorRenderTarget[Index].MipIndex;

		FVulkanTextureView RTView;
		if (Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D || Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
		{
			uint32 ArraySliceIndex, NumArraySlices;
			if (InRTInfo.ColorRenderTarget[Index].ArraySliceIndex == -1)
			{
				ArraySliceIndex = 0;
				NumArraySlices = Texture->GetNumberOfArrayLevels();
			}
			else
			{
				ArraySliceIndex = InRTInfo.ColorRenderTarget[Index].ArraySliceIndex;
				NumArraySlices = 1;
				check(ArraySliceIndex < Texture->GetNumberOfArrayLevels());
			}
			RTView.Create(*Texture->Device, Texture->Image, Texture->GetViewType(), Texture->GetFullAspectMask(), Desc.Format, Texture->ViewFormat, MipIndex, 1, ArraySliceIndex, NumArraySlices, true);
		}
		else if (Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_CUBE)
		{
			// Cube always renders one face at a time
			INC_DWORD_STAT(STAT_VulkanNumImageViews);
			RTView.Create(*Texture->Device, Texture->Image, VK_IMAGE_VIEW_TYPE_2D, Texture->GetFullAspectMask(), Desc.Format, Texture->ViewFormat, MipIndex, 1, InRTInfo.ColorRenderTarget[Index].ArraySliceIndex, 1, true);
		}
		else if (Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_3D)
		{
			RTView.Create(*Texture->Device, Texture->Image, VK_IMAGE_VIEW_TYPE_2D_ARRAY, Texture->GetFullAspectMask(), Desc.Format, Texture->ViewFormat, MipIndex, 1, 0, Desc.Depth, true, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
		}
		else
		{
			ensure(0);
		}

		AttachmentTextureViews.Add(RTView);
		AttachmentViewsToDelete.Add(RTView.View);

		++NumColorAttachments;

		// Check the RTLayout as well to make sure the resolve attachment is needed (Vulkan and Feature level specific)
		// See: FVulkanRenderTargetLayout constructor with FRHIRenderPassInfo
		if (InRTInfo.bHasResolveAttachments && RTLayout.GetHasResolveAttachments() && RTLayout.GetResolveAttachmentReferences()[Index].layout != VK_IMAGE_LAYOUT_UNDEFINED)
		{
			FRHITexture* ResolveRHITexture = InRTInfo.ColorResolveRenderTarget[Index].Texture;
			FVulkanTexture* ResolveTexture = FVulkanTexture::Cast(ResolveRHITexture);
			ColorResolveTargetImages[Index] = ResolveTexture->Image;

			//resolve attachments only supported for 2d/2d array textures
			FVulkanTextureView ResolveRTView;
			if (ResolveTexture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D || ResolveTexture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
			{
				ResolveRTView.Create(*ResolveTexture->Device, ResolveTexture->Image, ResolveTexture->GetViewType(), ResolveTexture->GetFullAspectMask(), ResolveTexture->GetDesc().Format, ResolveTexture->ViewFormat,
					MipIndex, 1, FMath::Max(0, (int32)InRTInfo.ColorRenderTarget[Index].ArraySliceIndex), ResolveTexture->GetNumberOfArrayLevels(), true);
			}

			AttachmentTextureViews.Add(ResolveRTView);
			AttachmentViewsToDelete.Add(ResolveRTView.View);
		}
	}

	VkSurfaceTransformFlagBitsKHR QCOMRenderPassTransform = RTLayout.GetQCOMRenderPassTransform();

	if (RTLayout.GetHasDepthStencil())
	{
		FVulkanTexture* Texture = FVulkanTexture::Cast(InRTInfo.DepthStencilRenderTarget.Texture);
		const FRHITextureDesc& Desc = Texture->GetDesc();
		DepthStencilRenderTargetImage = Texture->Image;
		bool bHasStencil = (Texture->GetDesc().Format == PF_DepthStencil || Texture->GetDesc().Format == PF_X24_G8);
		check(Texture->PartialView);
		PartialDepthTextureView = *Texture->PartialView;

		FVulkanTextureView RTView;
		ensure(Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D || Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY || Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_CUBE);
		if (NumColorAttachments == 0 && Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_CUBE)
		{
			RTView.Create(*Texture->Device, Texture->Image, VK_IMAGE_VIEW_TYPE_2D_ARRAY, Texture->GetFullAspectMask(), Texture->GetDesc().Format, Texture->ViewFormat, MipIndex, 1, 0, 6, true);
			NumLayers = 6;
			AttachmentTextureViews.Add(RTView);
			AttachmentViewsToDelete.Add(RTView.View);
		}
		else if (Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D  || Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY)
		{
			// depth attachments need a separate view to have no swizzle components, for validation correctness
			RTView.Create(*Texture->Device, Texture->Image, Texture->GetViewType(), Texture->GetFullAspectMask(), Texture->GetDesc().Format, Texture->ViewFormat, MipIndex, 1, 0, Texture->GetNumberOfArrayLevels(), true);
			AttachmentTextureViews.Add(RTView);
			AttachmentViewsToDelete.Add(RTView.View);
		}
		else if (QCOMRenderPassTransform != VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR &&
			Desc.Extent.X == RTExtents.width && Desc.Extent.Y == RTExtents.height)
		{
			FVulkanSwapChain* SwapChain = Device.GetImmediateContext().GetSwapChain();
			PartialDepthTextureView = *SwapChain->GetOrCreateQCOMDepthView(*Texture);
			AttachmentTextureViews.Add(*SwapChain->GetOrCreateQCOMDepthStencilView(*Texture));
		}
		else
		{
			AttachmentTextureViews.Add(Texture->DefaultView);
		}
	}

	if (GRHISupportsAttachmentVariableRateShading && GRHIVariableRateShadingEnabled && GRHIAttachmentVariableRateShadingEnabled && RTLayout.GetHasFragmentDensityAttachment())
	{
		FVulkanTexture* Texture = FVulkanTexture::Cast(InRTInfo.ShadingRateTexture);
		FragmentDensityImage = Texture->Image;

		ensure(Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D || Texture->GetViewType() == VK_IMAGE_VIEW_TYPE_2D_ARRAY);

		FVulkanTextureView RTView;
		RTView.Create(*Texture->Device, Texture->Image, Texture->GetViewType(), Texture->GetFullAspectMask(), Texture->GetDesc().Format, Texture->ViewFormat, MipIndex, 1, 0, Texture->GetNumberOfArrayLevels(), true);

		AttachmentTextureViews.Add(RTView);
		AttachmentViewsToDelete.Add(RTView.View);
	}

	TArray<VkImageView> AttachmentViews;
	AttachmentViews.Empty(AttachmentTextureViews.Num());
	for (auto& TextureView : AttachmentTextureViews)
	{
		AttachmentViews.Add(TextureView.View);
	}

	VkFramebufferCreateInfo CreateInfo;
	ZeroVulkanStruct(CreateInfo, VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
	CreateInfo.renderPass = RenderPass.GetHandle();
	CreateInfo.attachmentCount = AttachmentViews.Num();
	CreateInfo.pAttachments = AttachmentViews.GetData();
	CreateInfo.width  = RTExtents.width;
	CreateInfo.height = RTExtents.height;
	CreateInfo.layers = NumLayers;

	if (QCOMRenderPassTransform == VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR ||
		QCOMRenderPassTransform == VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)
	{
		Swap(CreateInfo.width, CreateInfo.height);
	}

	VERIFYVULKANRESULT_EXPANDED(VulkanRHI::vkCreateFramebuffer(Device.GetInstanceHandle(), &CreateInfo, VULKAN_CPU_ALLOCATOR, &Framebuffer));

	RenderArea.offset.x = 0;
	RenderArea.offset.y = 0;
	RenderArea.extent.width = RTExtents.width;
	RenderArea.extent.height = RTExtents.height;

	INC_DWORD_STAT(STAT_VulkanNumFrameBuffers);
}

FVulkanFramebuffer::~FVulkanFramebuffer()
{
	ensure(Framebuffer == VK_NULL_HANDLE);
}

void FVulkanFramebuffer::Destroy(FVulkanDevice& Device)
{
	VulkanRHI::FDeferredDeletionQueue2& Queue = Device.GetDeferredDeletionQueue();
	
	// will be deleted in reverse order
	Queue.EnqueueResource(VulkanRHI::FDeferredDeletionQueue2::EType::Framebuffer, Framebuffer);
	Framebuffer = VK_NULL_HANDLE;

	for (int32 Index = 0; Index < AttachmentViewsToDelete.Num(); ++Index)
	{
		DEC_DWORD_STAT(STAT_VulkanNumImageViews);
		Queue.EnqueueResource(VulkanRHI::FDeferredDeletionQueue2::EType::ImageView, AttachmentViewsToDelete[Index]);
	}

	DEC_DWORD_STAT(STAT_VulkanNumFrameBuffers);
}

bool FVulkanFramebuffer::Matches(const FRHISetRenderTargetsInfo& InRTInfo) const
{
	if (NumColorRenderTargets != InRTInfo.NumColorRenderTargets)
	{
		return false;
	}

	{
		const FRHIDepthRenderTargetView& B = InRTInfo.DepthStencilRenderTarget;
		if (B.Texture)
		{
			VkImage AImage = DepthStencilRenderTargetImage;
			VkImage BImage = FVulkanTexture::Cast(B.Texture)->Image;
			if (AImage != BImage)
			{
				return false;
			}
		}
	}

	{
		FRHITexture* Texture = InRTInfo.ShadingRateTexture;
		if (Texture)
		{
			VkImage AImage = FragmentDensityImage;
			VkImage BImage = FVulkanTexture::Cast(Texture)->Image;
			if (AImage != BImage)
			{
				return false;
			}
		}
	}

	int32 AttachementIndex = 0;
	for (int32 Index = 0; Index < InRTInfo.NumColorRenderTargets; ++Index)
	{
		if (InRTInfo.bHasResolveAttachments)
		{
			const FRHIRenderTargetView& R = InRTInfo.ColorResolveRenderTarget[Index];
			if (R.Texture)
			{
				VkImage AImage = ColorResolveTargetImages[AttachementIndex];
				VkImage BImage = FVulkanTexture::Cast(R.Texture)->Image;
				if (AImage != BImage)
				{
					return false;
				}
			}
		}

		const FRHIRenderTargetView& B = InRTInfo.ColorRenderTarget[Index];
		if (B.Texture)
		{
			VkImage AImage = ColorRenderTargetImages[AttachementIndex];
			VkImage BImage = FVulkanTexture::Cast(B.Texture)->Image;
			if (AImage != BImage)
			{
				return false;
			}
			AttachementIndex++;
		}
	}

	return true;
}

// Tear down and recreate swapchain and related resources.
void FVulkanViewport::RecreateSwapchain(void* NewNativeWindow)
{
	FScopeLock LockSwapchain(&RecreatingSwapchain);

	FVulkanSwapChainRecreateInfo RecreateInfo = { VK_NULL_HANDLE, VK_NULL_HANDLE };
	DestroySwapchain(&RecreateInfo);
	WindowHandle = NewNativeWindow;
	CreateSwapchain(&RecreateInfo);
	check(RecreateInfo.Surface == VK_NULL_HANDLE);
	check(RecreateInfo.SwapChain == VK_NULL_HANDLE);
}

void FVulkanViewport::Tick(float DeltaTime)
{
	check(IsInGameThread());

	if (SwapChain && FPlatformAtomics::AtomicRead(&LockToVsync) != SwapChain->DoesLockToVsync())
	{
		FlushRenderingCommands();
		ENQUEUE_RENDER_COMMAND(UpdateVsync)(
			[this](FRHICommandListImmediate& RHICmdList)
		{
			RecreateSwapchainFromRT(PixelFormat);
		});
		FlushRenderingCommands();
	}
}

void FVulkanViewport::Resize(uint32 InSizeX, uint32 InSizeY, bool bInIsFullscreen, EPixelFormat PreferredPixelFormat)
{
	SizeX = InSizeX;
	SizeY = InSizeY;
	bIsFullscreen = bInIsFullscreen;

	RecreateSwapchainFromRT(PreferredPixelFormat);
}

void FVulkanViewport::RecreateSwapchainFromRT(EPixelFormat PreferredPixelFormat)
{
	check(IsInRenderingThread());

	// TODO: should flush RHIT commands here?
	
	FVulkanSwapChainRecreateInfo RecreateInfo = { VK_NULL_HANDLE, VK_NULL_HANDLE};
	DestroySwapchain(&RecreateInfo);
	PixelFormat = PreferredPixelFormat;
	CreateSwapchain(&RecreateInfo);
	check(RecreateInfo.Surface == VK_NULL_HANDLE);
	check(RecreateInfo.SwapChain == VK_NULL_HANDLE);
}

void FVulkanViewport::CreateSwapchain(FVulkanSwapChainRecreateInfo* RecreateInfo)
{
	// Release a previous swapchain 'dummy' and a real backbuffer if any
	RenderingBackBuffer = nullptr;
	RHIBackBuffer = nullptr;

	if (SupportsStandardSwapchain())
	{
		uint32 DesiredNumBackBuffers = NUM_BUFFERS;

		TArray<VkImage> Images;
		SwapChain = new FVulkanSwapChain(
			RHI->Instance, *Device, WindowHandle,
			PixelFormat, SizeX, SizeY, bIsFullscreen,
			&DesiredNumBackBuffers,
			Images,
			LockToVsync,
			RecreateInfo
		);

		checkf(Images.Num() >= NUM_BUFFERS, TEXT("We wanted at least %i images, actual Num: %i"), NUM_BUFFERS, Images.Num());
		BackBufferImages.SetNum(Images.Num());
		RenderingDoneSemaphores.SetNum(Images.Num());
		TextureViews.SetNum(Images.Num());

		FVulkanCmdBuffer* CmdBuffer = Device->GetImmediateContext().GetCommandBufferManager()->GetUploadCmdBuffer();
		ensure(CmdBuffer->IsOutsideRenderPass());

		for (int32 Index = 0; Index < Images.Num(); ++Index)
		{
			BackBufferImages[Index] = Images[Index];
			TextureViews[Index].Create(*Device, Images[Index], VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_ASPECT_COLOR_BIT, PixelFormat, UEToVkTextureFormat(PixelFormat, false), 0, 1, 0, 1, false);

			// Clear the swapchain to avoid a validation warning, and transition to ColorAttachment
			{
				VkImageSubresourceRange Range = FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT);

				VkClearColorValue Color;
				FMemory::Memzero(Color);
				VulkanSetImageLayout(CmdBuffer->GetHandle(), Images[Index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Range);
				VulkanRHI::vkCmdClearColorImage(CmdBuffer->GetHandle(), Images[Index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &Color, 1, &Range);
				VulkanSetImageLayout(CmdBuffer->GetHandle(), Images[Index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, Range);
			}

#if VULKAN_ENABLE_DRAW_MARKERS
			if (Device->GetDebugMarkerSetObjectName())
			{
				VulkanRHI::SetDebugMarkerName(Device->GetDebugMarkerSetObjectName(), Device->GetInstanceHandle(), BackBufferImages[Index], "RenderingBackBuffer");
			}
#endif
		}
		
		Device->GetImmediateContext().GetCommandBufferManager()->SubmitUploadCmdBuffer();

		RHIBackBuffer = new FVulkanBackBuffer(*Device, this, PixelFormat, SizeX, SizeY, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_ResolveTargetable);
	}
	else
	{
		PixelFormat = GetPixelFormatForNonDefaultSwapchain();
		if (RecreateInfo != nullptr)
		{
			if(RecreateInfo->SwapChain)
			{
				FVulkanPlatform::DestroySwapchainKHR(Device->GetInstanceHandle(), RecreateInfo->SwapChain, VULKAN_CPU_ALLOCATOR);
				RecreateInfo->SwapChain = VK_NULL_HANDLE;
			}
			if (RecreateInfo->Surface)
			{
				VulkanRHI::vkDestroySurfaceKHR(RHI->Instance, RecreateInfo->Surface, VULKAN_CPU_ALLOCATOR);
				RecreateInfo->Surface = VK_NULL_HANDLE;
			}
		}
	}

	// We always create a 'dummy' backbuffer to gracefully handle SurfaceLost cases
	{
		uint32 BackBufferSizeX = RequiresRenderingBackBuffer() ? SizeX : 1;
		uint32 BackBufferSizeY = RequiresRenderingBackBuffer() ? SizeY : 1;

		const FRHITextureCreateDesc Desc =
			FRHITextureCreateDesc::Create2D(TEXT("RenderingBackBuffer"), BackBufferSizeX, BackBufferSizeY, PixelFormat)
			.SetClearValue(FClearValueBinding::None)
			.SetFlags(ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ShaderResource | ETextureCreateFlags::ResolveTargetable)
			.SetInitialState(ERHIAccess::Present);

		RenderingBackBuffer = new FVulkanTexture(*Device, Desc, nullptr);
#if VULKAN_ENABLE_DRAW_MARKERS
		if (Device->GetDebugMarkerSetObjectName())
		{
			VulkanRHI::SetDebugMarkerName(Device->GetDebugMarkerSetObjectName(), Device->GetInstanceHandle(), RenderingBackBuffer->Image, "RenderingBackBuffer");
		}
#endif
	}

	AcquiredImageIndex = -1;
}

void FVulkanViewport::DestroySwapchain(FVulkanSwapChainRecreateInfo* RecreateInfo)
{
	// Submit all command buffers here
	Device->SubmitCommandsAndFlushGPU();
	Device->WaitUntilIdle();
	
	// Intentionally leave RenderingBackBuffer alive, so it can be used a dummy backbuffer while we don't have swapchain images
	// RenderingBackBuffer = nullptr;
	
	if (RHIBackBuffer)
	{
		RHIBackBuffer->ReleaseAcquiredImage();
		// We release this RHIBackBuffer when we create a new swapchain
	}
		
	if (SupportsStandardSwapchain() && SwapChain)
	{
		for (int32 Index = 0, NumBuffers = BackBufferImages.Num(); Index < NumBuffers; ++Index)
		{
			TextureViews[Index].Destroy(*Device);
			Device->NotifyDeletedImage(BackBufferImages[Index], true);
			BackBufferImages[Index] = VK_NULL_HANDLE;
		}
		
		Device->GetDeferredDeletionQueue().ReleaseResources(true);

		SwapChain->Destroy(RecreateInfo);
		delete SwapChain;
		SwapChain = nullptr;

		Device->GetDeferredDeletionQueue().ReleaseResources(true);
	}

	AcquiredImageIndex = -1;
}

inline static void CopyImageToBackBuffer(FVulkanCommandListContext* Context, FVulkanCmdBuffer* CmdBuffer, VkImage SrcSurface, VkImage DstSurface, int32 SizeX, int32 SizeY, int32 WindowSizeX, int32 WindowSizeY)
{
	FVulkanLayoutManager& LayoutManager = Context->GetLayoutManager();
	VkImageLayout SrcLayout = LayoutManager.FindLayoutChecked(SrcSurface);

	{
		FVulkanPipelineBarrier Barrier;
		Barrier.AddImageLayoutTransition(SrcSurface, SrcLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1));
		Barrier.AddImageLayoutTransition(DstSurface, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1));
		Barrier.Execute(CmdBuffer->GetHandle());
	}

	VulkanRHI::DebugHeavyWeightBarrier(CmdBuffer->GetHandle(), 32);

	if (SizeX != WindowSizeX || SizeY != WindowSizeY)
	{
		VkImageBlit Region;
		FMemory::Memzero(Region);
		Region.srcOffsets[0].x = 0;
		Region.srcOffsets[0].y = 0;
		Region.srcOffsets[0].z = 0;
		Region.srcOffsets[1].x = SizeX;
		Region.srcOffsets[1].y = SizeY;
		Region.srcOffsets[1].z = 1;
		Region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Region.srcSubresource.layerCount = 1;
		Region.dstOffsets[0].x = 0;
		Region.dstOffsets[0].y = 0;
		Region.dstOffsets[0].z = 0;
		Region.dstOffsets[1].x = WindowSizeX;
		Region.dstOffsets[1].y = WindowSizeY;
		Region.dstOffsets[1].z = 1;
		Region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		Region.dstSubresource.baseArrayLayer = 0;
		Region.dstSubresource.layerCount = 1;
		VulkanRHI::vkCmdBlitImage(CmdBuffer->GetHandle(),
			SrcSurface, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			DstSurface, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &Region, VK_FILTER_LINEAR);
	}
	else
	{
		VkImageCopy Region;
		FMemory::Memzero(Region);
		Region.extent.width = SizeX;
		Region.extent.height = SizeY;
		Region.extent.depth = 1;
		Region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		//Region.srcSubresource.baseArrayLayer = 0;
		Region.srcSubresource.layerCount = 1;
		//Region.srcSubresource.mipLevel = 0;
		Region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		//Region.dstSubresource.baseArrayLayer = 0;
		Region.dstSubresource.layerCount = 1;
		//Region.dstSubresource.mipLevel = 0;
		VulkanRHI::vkCmdCopyImage(CmdBuffer->GetHandle(),
			SrcSurface, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			DstSurface, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &Region);
	}

	{
		FVulkanPipelineBarrier Barrier;
		Barrier.AddImageLayoutTransition(SrcSurface, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, SrcLayout, FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1));
		Barrier.AddImageLayoutTransition(DstSurface, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT, 0, 1));
		Barrier.Execute(CmdBuffer->GetHandle());
	}
}

bool FVulkanViewport::Present(FVulkanCommandListContext* Context, FVulkanCmdBuffer* CmdBuffer, FVulkanQueue* Queue, FVulkanQueue* PresentQueue, bool bLockToVsync)
{
	FPlatformAtomics::AtomicStore(&LockToVsync, bLockToVsync ? 1 : 0);
	bool bFailedToDelayAcquireBackbuffer = false;

	//Transition back buffer to presentable and submit that command
	check(CmdBuffer->IsOutsideRenderPass());

	if (SupportsStandardSwapchain())
	{
		if (GVulkanDelayAcquireImage == EDelayAcquireImageType::DelayAcquire && RenderingBackBuffer)
		{
			SCOPE_CYCLE_COUNTER(STAT_VulkanAcquireBackBuffer);
			// swapchain can go out of date, do not crash at this point
			if (LIKELY(TryAcquireImageIndex()))
			{
				uint32 WindowSizeX = FMath::Min(SizeX, SwapChain->InternalWidth);
				uint32 WindowSizeY = FMath::Min(SizeY, SwapChain->InternalHeight);

				Context->RHIPushEvent(TEXT("CopyImageToBackBuffer"), FColor::Blue);
				CopyImageToBackBuffer(Context, CmdBuffer, RenderingBackBuffer->Image, BackBufferImages[AcquiredImageIndex], SizeX, SizeY, WindowSizeX, WindowSizeY);
				Context->RHIPopEvent();
			}
			else
			{
				bFailedToDelayAcquireBackbuffer = true;
			}
		}
		else
		{
			if (AcquiredImageIndex != -1)
			{
				check(RHIBackBuffer != nullptr && RHIBackBuffer->Image == BackBufferImages[AcquiredImageIndex]);
				VkImageLayout& Layout = Context->GetLayoutManager().FindOrAddLayoutRW(BackBufferImages[AcquiredImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, 1, 1);
				VulkanSetImageLayout(CmdBuffer->GetHandle(), BackBufferImages[AcquiredImageIndex], Layout, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, FVulkanPipelineBarrier::MakeSubresourceRange(VK_IMAGE_ASPECT_COLOR_BIT));
				Layout = VK_IMAGE_LAYOUT_UNDEFINED;
			}
			else
			{
				// When we have failed to acquire backbuffer image we fallback to using 'dummy' backbuffer
				check(RHIBackBuffer != nullptr && RHIBackBuffer->Image == RenderingBackBuffer->Image);
			}
		}
	}

	CmdBuffer->End();
	FVulkanCommandBufferManager* ImmediateCmdBufMgr = Device->GetImmediateContext().GetCommandBufferManager();
	ImmediateCmdBufMgr->FlushResetQueryPools();
	checkf(ImmediateCmdBufMgr->GetActiveCmdBufferDirect() == CmdBuffer, TEXT("Present() is submitting something else than the active command buffer"));
	if (SupportsStandardSwapchain())
	{
		if (LIKELY(!bFailedToDelayAcquireBackbuffer))
		{
			if (GVulkanDelayAcquireImage == EDelayAcquireImageType::DelayAcquire)
			{
				CmdBuffer->AddWaitSemaphore(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, AcquiredSemaphore);
			}
			
			VulkanRHI::FSemaphore* SignalSemaphore = (AcquiredImageIndex >= 0 ? RenderingDoneSemaphores[AcquiredImageIndex] : nullptr);
			// submit through the CommandBufferManager as it will add the proper semaphore
			ImmediateCmdBufMgr->SubmitActiveCmdBufferFromPresent(SignalSemaphore);
		}
		else
		{
			// failing to do the delayacquire can only happen if we were in this mode to begin with
			check(GVulkanDelayAcquireImage == EDelayAcquireImageType::DelayAcquire);

			UE_LOG(LogVulkanRHI, Log, TEXT("AcquireNextImage() failed due to the outdated swapchain, not even attempting to present."));

			// cannot just throw out this command buffer (needs to be submitted or other checks fail)
			Queue->Submit(CmdBuffer);
			RecreateSwapchain(WindowHandle);

			// Swapchain creation pushes some commands - flush the command buffers now to begin with a fresh state
			Device->SubmitCommandsAndFlushGPU();
			Device->WaitUntilIdle();

			// early exit
			return (int32)FVulkanSwapChain::EStatus::Healthy;
		}
	}
	else
	{
		// Submit active command buffer if not supporting standard swapchain (e.g. XR devices).
		ImmediateCmdBufMgr->SubmitActiveCmdBufferFromPresent(nullptr);
	}

	//Flush all commands
	//check(0);

	//#todo-rco: Proper SyncInterval bLockToVsync ? RHIConsoleVariables::SyncInterval : 0
	int32 SyncInterval = 0;
	bool bNeedNativePresent = true;

	const bool bHasCustomPresent = IsValidRef(CustomPresent);
	if (bHasCustomPresent)
	{
		SCOPE_CYCLE_COUNTER(STAT_VulkanCustomPresentTime);
		bNeedNativePresent = CustomPresent->Present(SyncInterval);
	}

	bool bResult = false;
	if (bNeedNativePresent && (!SupportsStandardSwapchain() || GVulkanDelayAcquireImage == EDelayAcquireImageType::DelayAcquire || RHIBackBuffer != nullptr))
	{
		// Present the back buffer to the viewport window.
		auto SwapChainJob = [Queue, PresentQueue](FVulkanViewport* Viewport)
		{
			// May happend if swapchain was recreated in DoCheckedSwapChainJob()
			if (Viewport->AcquiredImageIndex == -1)
			{
				// Skip present silently if image has not been acquired
				return (int32)FVulkanSwapChain::EStatus::Healthy;
			}
			return (int32)Viewport->SwapChain->Present(Queue, PresentQueue, Viewport->RenderingDoneSemaphores[Viewport->AcquiredImageIndex]);
		};
		if (SupportsStandardSwapchain() && !DoCheckedSwapChainJob(SwapChainJob))
		{
			UE_LOG(LogVulkanRHI, Error, TEXT("Swapchain present failed!"));
			bResult = false;
		}
		else
		{
			bResult = true;
		}

		if (bHasCustomPresent)
		{
			CustomPresent->PostPresent();
		}
	}

	if (FVulkanPlatform::RequiresWaitingForFrameCompletionEvent() && !bHasCustomPresent)
	{
		// Wait for the GPU to finish rendering the previous frame before finishing this frame.
		WaitForFrameEventCompletion();
		IssueFrameEvent();
	}

	// If the input latency timer has been triggered, block until the GPU is completely
	// finished displaying this frame and calculate the delta time.
	//if (GInputLatencyTimer.RenderThreadTrigger)
	//{
	//	WaitForFrameEventCompletion();
	//	uint32 EndTime = FPlatformTime::Cycles();
	//	GInputLatencyTimer.DeltaTime = EndTime - GInputLatencyTimer.StartTime;
	//	GInputLatencyTimer.RenderThreadTrigger = false;
	//}

	// PrepareForNewActiveCommandBuffer might be called by swapchain re-creation routine. Skip prepare if we already have an open active buffer.
	if (ImmediateCmdBufMgr->GetActiveCmdBuffer() && !ImmediateCmdBufMgr->GetActiveCmdBuffer()->HasBegun())
	{
		ImmediateCmdBufMgr->PrepareForNewActiveCommandBuffer();
	}

	AcquiredImageIndex = -1;

	++PresentCount;
	++GVulkanRHI->TotalPresentCount;

	return bResult;
}

VkSurfaceTransformFlagBitsKHR FVulkanViewport::GetSwapchainQCOMRenderPassTransform() const
{
	return SwapChain->QCOMRenderPassTransform;
}

VkFormat FVulkanViewport::GetSwapchainImageFormat() const
{
	return SwapChain->ImageFormat;
}

bool FVulkanViewport::SupportsStandardSwapchain()
{
	return !bRenderOffscreen && !RHI->bIsStandaloneStereoDevice;
}

bool FVulkanViewport::RequiresRenderingBackBuffer()
{
	return !RHI->bIsStandaloneStereoDevice;
}

EPixelFormat FVulkanViewport::GetPixelFormatForNonDefaultSwapchain()
{
	if (bRenderOffscreen || RHI->bIsStandaloneStereoDevice)
	{
		return PF_R8G8B8A8;
	}
	else
	{
		checkf(0, TEXT("Platform Requires Standard Swapchain!"));
		return PF_Unknown;
	}
}

/*=============================================================================
 *	The following RHI functions must be called from the main thread.
 *=============================================================================*/
FViewportRHIRef FVulkanDynamicRHI::RHICreateViewport(void* WindowHandle, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
{
	check( IsInGameThread() );

	// Use a default pixel format if none was specified	
	if (PreferredPixelFormat == PF_Unknown)
	{
		static const auto* CVarDefaultBackBufferPixelFormat = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultBackBufferPixelFormat"));
		PreferredPixelFormat = EDefaultBackBufferPixelFormat::Convert2PixelFormat(EDefaultBackBufferPixelFormat::FromInt(CVarDefaultBackBufferPixelFormat->GetValueOnAnyThread()));
	}

	return new FVulkanViewport(this, Device, WindowHandle, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat);
}

void FVulkanDynamicRHI::RHIResizeViewport(FRHIViewport* ViewportRHI, uint32 SizeX, uint32 SizeY, bool bIsFullscreen, EPixelFormat PreferredPixelFormat)
{
	check(IsInGameThread());
	FVulkanViewport* Viewport = ResourceCast(ViewportRHI);

	// Use a default pixel format if none was specified	
	if (PreferredPixelFormat == PF_Unknown)
	{
		static const auto* CVarDefaultBackBufferPixelFormat = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DefaultBackBufferPixelFormat"));
		PreferredPixelFormat = EDefaultBackBufferPixelFormat::Convert2PixelFormat(EDefaultBackBufferPixelFormat::FromInt(CVarDefaultBackBufferPixelFormat->GetValueOnAnyThread()));
	}

	if (Viewport->GetSizeXY() != FIntPoint(SizeX, SizeY) || Viewport->IsFullscreen() != bIsFullscreen)
	{
		FlushRenderingCommands();

		ENQUEUE_RENDER_COMMAND(ResizeViewport)(
			[Viewport, SizeX, SizeY, bIsFullscreen, PreferredPixelFormat](FRHICommandListImmediate& RHICmdList)
			{
				Viewport->Resize(SizeX, SizeY, bIsFullscreen, PreferredPixelFormat);
			});
		FlushRenderingCommands();
	}
}

void FVulkanDynamicRHI::RHIResizeViewport(FRHIViewport* ViewportRHI, uint32 SizeX, uint32 SizeY, bool bIsFullscreen)
{
	check(IsInGameThread());
	FVulkanViewport* Viewport = ResourceCast(ViewportRHI);

	if (Viewport->GetSizeXY() != FIntPoint(SizeX, SizeY))
	{
		FlushRenderingCommands();

		ENQUEUE_RENDER_COMMAND(ResizeViewport)(
			[Viewport, SizeX, SizeY, bIsFullscreen](FRHICommandListImmediate& RHICmdList)
			{
				Viewport->Resize(SizeX, SizeY, bIsFullscreen, PF_Unknown);
			});
		FlushRenderingCommands();
	}
}

void FVulkanDynamicRHI::RHITick(float DeltaTime)
{
	check(IsInGameThread());
	FVulkanDevice* VulkanDevice = GetDevice();
	static bool bRequestNULLPixelShader = true;
	bool bRequested = bRequestNULLPixelShader;
	ENQUEUE_RENDER_COMMAND(TempFrameReset)(
		[VulkanDevice, bRequested](FRHICommandListImmediate& RHICmdList)
		{
			if (bRequested)
			{
				//work around layering violation
				TShaderMapRef<FNULLPS>(GetGlobalShaderMap(GMaxRHIFeatureLevel)).GetPixelShader();
			}

			VulkanDevice->GetImmediateContext().GetTempFrameAllocationBuffer().Reset();
		});

	if (bRequestNULLPixelShader)
	{
		bRequestNULLPixelShader = false;
	}
}

FTexture2DRHIRef FVulkanDynamicRHI::RHIGetViewportBackBuffer(FRHIViewport* ViewportRHI)
{
	check(IsInRenderingThread());
	check(ViewportRHI);
	FVulkanViewport* Viewport = ResourceCast(ViewportRHI);

	if (Viewport->SwapChain)
	{
		Viewport->SwapChain->RenderThreadPacing();
	}

	return Viewport->GetBackBuffer(FRHICommandListExecutor::GetImmediateCommandList());
}

void FVulkanDynamicRHI::RHIAdvanceFrameForGetViewportBackBuffer(FRHIViewport* ViewportRHI)
{
	check(IsInRenderingThread());
	check(ViewportRHI);
	FVulkanViewport* Viewport = ResourceCast(ViewportRHI);
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	Viewport->AdvanceBackBufferFrame(RHICmdList);
}

void FVulkanCommandListContext::RHISetViewport(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ)
{
	PendingGfxState->SetViewport(MinX, MinY, MinZ, MaxX, MaxY, MaxZ);
}

void FVulkanCommandListContext::RHISetStereoViewport(float LeftMinX, float RightMinX, float LeftMinY, float RightMinY, float MinZ, float LeftMaxX, float RightMaxX, float LeftMaxY, float RightMaxY, float MaxZ)
{
	TStaticArray<VkViewport, 2> Viewports;

	Viewports[0].x = FMath::FloorToInt(LeftMinX);
	Viewports[0].y = FMath::FloorToInt(LeftMinY);
	Viewports[0].width = FMath::CeilToInt(LeftMaxX - LeftMinX);
	Viewports[0].height = FMath::CeilToInt(LeftMaxY - LeftMinY);
	Viewports[0].minDepth = MinZ;
	Viewports[0].maxDepth = MaxZ;

	Viewports[1].x = FMath::FloorToInt(RightMinX);
	Viewports[1].y = FMath::FloorToInt(RightMinY);
	Viewports[1].width = FMath::CeilToInt(RightMaxX - RightMinX);
	Viewports[1].height = FMath::CeilToInt(RightMaxY - RightMinY);
	Viewports[1].minDepth = MinZ;
	Viewports[1].maxDepth = MaxZ;

	PendingGfxState->SetMultiViewport(Viewports);
}

void FVulkanCommandListContext::RHISetMultipleViewports(uint32 Count, const FViewportBounds* Data)
{
	VULKAN_SIGNAL_UNIMPLEMENTED();
}

void FVulkanCommandListContext::RHISetScissorRect(bool bEnable, uint32 MinX, uint32 MinY, uint32 MaxX, uint32 MaxY)
{
	PendingGfxState->SetScissor(bEnable, MinX, MinY, MaxX, MaxY);
}
