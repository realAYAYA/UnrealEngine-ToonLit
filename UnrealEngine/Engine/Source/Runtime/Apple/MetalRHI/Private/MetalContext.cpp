// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetalRHIPrivate.h"
#include "MetalRHIRenderQuery.h"
#include "MetalVertexDeclaration.h"
#include "MetalShaderTypes.h"
#include "MetalGraphicsPipelineState.h"
#include "Misc/App.h"
#if PLATFORM_IOS
#include "IOS/IOSAppDelegate.h"
#endif
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformFramePacer.h"
#include "Runtime/HeadMountedDisplay/Public/IHeadMountedDisplayModule.h"

#include "MetalContext.h"
#include "MetalProfiler.h"
#include "MetalCommandBuffer.h"

#include "MetalBindlessDescriptors.h"
#include "MetalFrameAllocator.h"

int32 GMetalSupportsIntermediateBackBuffer = 0;
static FAutoConsoleVariableRef CVarMetalSupportsIntermediateBackBuffer(
	TEXT("rhi.Metal.SupportsIntermediateBackBuffer"),
	GMetalSupportsIntermediateBackBuffer,
	TEXT("When enabled (> 0) allocate an intermediate texture to use as the back-buffer & blit from there into the actual device back-buffer, this is required if we use the experimental separate presentation thread. (Off by default (0))"), ECVF_ReadOnly);

int32 GMetalSeparatePresentThread = 0;
static FAutoConsoleVariableRef CVarMetalSeparatePresentThread(
	TEXT("rhi.Metal.SeparatePresentThread"),
	GMetalSeparatePresentThread,
	TEXT("When enabled (> 0) requires rhi.Metal.SupportsIntermediateBackBuffer be enabled and will cause two intermediate back-buffers be allocated so that the presentation of frames to the screen can be run on a separate thread.\n")
	TEXT("This option uncouples the Render/RHI thread from calls to -[CAMetalLayer nextDrawable] and will run arbitrarily fast by rendering but not waiting to present all frames. This is equivalent to running without V-Sync, but without the screen tearing.\n")
	TEXT("On iOS/tvOS this is the only way to run without locking the CPU to V-Sync somewhere - this shouldn't be used in a shipping title without understanding the power/heat implications.\n")
	TEXT("(Off by default (0))"), ECVF_ReadOnly);

int32 GMetalNonBlockingPresent = 0;
static FAutoConsoleVariableRef CVarMetalNonBlockingPresent(
	TEXT("rhi.Metal.NonBlockingPresent"),
	GMetalNonBlockingPresent,
	TEXT("When enabled (> 0) this will force MetalRHI to query if a back-buffer is available to present and if not will skip the frame. Only functions on macOS, it is ignored on iOS/tvOS.\n")
	TEXT("(Off by default (0))"));

#if PLATFORM_MAC
static int32 GMetalCommandQueueSize = 5120; // This number is large due to texture streaming - currently each texture is its own command-buffer.
// The whole MetalRHI needs to be changed to use MTLHeaps/MTLFences & reworked so that operations with the same synchronisation requirements are collapsed into a single blit command-encoder/buffer.
#else
static int32 GMetalCommandQueueSize = 0;
#endif

static FAutoConsoleVariableRef CVarMetalCommandQueueSize(
	TEXT("rhi.Metal.CommandQueueSize"),
	GMetalCommandQueueSize,
	TEXT("The maximum number of command-buffers that can be allocated from each command-queue. (Default: 5120 Mac, 64 iOS/tvOS)"), ECVF_ReadOnly);

int32 GMetalBufferZeroFill = 0; // Deliberately not static
static FAutoConsoleVariableRef CVarMetalBufferZeroFill(
	TEXT("rhi.Metal.BufferZeroFill"),
	GMetalBufferZeroFill,
	TEXT("Debug option: when enabled will fill the buffer contents with 0 when allocating buffer objects, or regions thereof. (Default: 0, Off)"));

#if METAL_DEBUG_OPTIONS
int32 GMetalBufferScribble = 0; // Deliberately not static, see InitFrame_UniformBufferPoolCleanup
static FAutoConsoleVariableRef CVarMetalBufferScribble(
	TEXT("rhi.Metal.BufferScribble"),
	GMetalBufferScribble,
	TEXT("Debug option: when enabled will scribble over the buffer contents with a single value when releasing buffer objects, or regions thereof. (Default: 0, Off)"));

static int32 GMetalResourcePurgeOnDelete = 0;
static FAutoConsoleVariableRef CVarMetalResourcePurgeOnDelete(
	TEXT("rhi.Metal.ResourcePurgeOnDelete"),
	GMetalResourcePurgeOnDelete,
	TEXT("Debug option: when enabled all MTLResource objects will have their backing stores purged on release - any subsequent access will be invalid and cause a command-buffer failure. Useful for making intermittent resource lifetime errors more common and easier to track. (Default: 0, Off)"));

static int32 GMetalResourceDeferDeleteNumFrames = 0;
static FAutoConsoleVariableRef CVarMetalResourceDeferDeleteNumFrames(
	TEXT("rhi.Metal.ResourceDeferDeleteNumFrames"),
	GMetalResourceDeferDeleteNumFrames,
	TEXT("Debug option: set to the number of frames that must have passed before resource free-lists are processed and resources disposed of. (Default: 0, Off)"));
#endif

#if UE_BUILD_SHIPPING
int32 GMetalRuntimeDebugLevel = 0;
#else
int32 GMetalRuntimeDebugLevel = 1;
#endif
static FAutoConsoleVariableRef CVarMetalRuntimeDebugLevel(
	TEXT("rhi.Metal.RuntimeDebugLevel"),
	GMetalRuntimeDebugLevel,
	TEXT("The level of debug validation performed by MetalRHI in addition to the underlying Metal API & validation layer.\n")
	TEXT("Each subsequent level adds more tests and reporting in addition to the previous level.\n")
	TEXT("*LEVELS >= 3 ARE IGNORED IN SHIPPING AND TEST BUILDS*. (Default: 1 (Debug, Development), 0 (Test, Shipping))\n")
	TEXT("\t0: Off,\n")
	TEXT("\t1: Enable light-weight validation of resource bindings & API usage,\n")
	TEXT("\t2: Reset resource bindings when binding a PSO/Compute-Shader to simplify GPU debugging,\n")
	TEXT("\t3: Allow rhi.Metal.CommandBufferCommitThreshold to break command-encoders (except when MSAA is enabled),\n")
	TEXT("\t4: Enable slower, more extensive validation checks for resource types & encoder usage,\n")
    TEXT("\t5: Wait for each command-buffer to complete immediately after submission."));

float GMetalPresentFramePacing = 0.0f;
#if !PLATFORM_MAC
static FAutoConsoleVariableRef CVarMetalPresentFramePacing(
	TEXT("rhi.Metal.PresentFramePacing"),
	GMetalPresentFramePacing,
	TEXT("Specify the desired frame rate for presentation (iOS 10.3+ only, default: 0.0f, off"));
#endif

#if PLATFORM_MAC
static int32 GMetalDefaultUniformBufferAllocation = 1024*1024;
#else
static int32 GMetalDefaultUniformBufferAllocation = 1024*32;
#endif
static FAutoConsoleVariableRef CVarMetalDefaultUniformBufferAllocation(
    TEXT("rhi.Metal.DefaultUniformBufferAllocation"),
    GMetalDefaultUniformBufferAllocation,
    TEXT("Default size of a uniform buffer allocation."));

#if PLATFORM_MAC
static int32 GMetalTargetUniformAllocationLimit = 1024 * 1024 * 50;
#else
static int32 GMetalTargetUniformAllocationLimit = 1024 * 1024 * 5;
#endif
static FAutoConsoleVariableRef CVarMetalTargetUniformAllocationLimit(
     TEXT("rhi.Metal.TargetUniformAllocationLimit"),
     GMetalTargetUniformAllocationLimit,
     TEXT("Target Allocation limit for the uniform buffer pool."));

#if PLATFORM_MAC
static int32 GMetalTargetTransferAllocatorLimit = 1024*1024*50;
#else
static int32 GMetalTargetTransferAllocatorLimit = 1024*1024*2;
#endif
static FAutoConsoleVariableRef CVarMetalTargetTransferAllocationLimit(
	TEXT("rhi.Metal.TargetTransferAllocationLimit"),
	GMetalTargetTransferAllocatorLimit,
	TEXT("Target Allocation limit for the upload staging buffer pool."));

#if PLATFORM_MAC
static int32 GMetalDefaultTransferAllocation = 1024*1024*10;
#else
static int32 GMetalDefaultTransferAllocation = 1024*1024*1;
#endif
static FAutoConsoleVariableRef CVarMetalDefaultTransferAllocation(
	TEXT("rhi.Metal.DefaultTransferAllocation"),
	GMetalDefaultTransferAllocation,
	TEXT("Default size of a single entry in the upload pool."));

uint32 SafeGetRuntimeDebuggingLevel()
{
    return GIsRHIInitialized ? GetMetalDeviceContext().GetCommandQueue().GetRuntimeDebuggingLevel() : GMetalRuntimeDebugLevel;
}

#if PLATFORM_MAC
static NS::Object* GMetalDeviceObserver;
static MTL::Device* GetMTLDevice(uint32& DeviceIndex)
{
#if PLATFORM_MAC_ARM64
    return MTL::CreateSystemDefaultDevice();
#else
    MTL_SCOPED_AUTORELEASE_POOL;
	
	DeviceIndex = 0;
	
	NS::Array* DeviceList;
	
    DeviceList = MTL::CopyAllDevicesWithObserver(&GMetalDeviceObserver, [](const MTL::Device* Device, const NS::String* Notification)
    {
        if (Notification->isEqualToString(MTL::DeviceWasAddedNotification))
        {
            FPlatformMisc::GPUChangeNotification(Device->registryID(), FPlatformMisc::EMacGPUNotification::Added);
        }
        else if (Notification->isEqualToString(MTL::DeviceRemovalRequestedNotification))
        {
            FPlatformMisc::GPUChangeNotification(Device->registryID(), FPlatformMisc::EMacGPUNotification::RemovalRequested);
        }
        else if (Notification->isEqualToString(MTL::DeviceWasRemovedNotification))
        {
            FPlatformMisc::GPUChangeNotification(Device->registryID(), FPlatformMisc::EMacGPUNotification::Removed);
        }
    });
	
	const int32 NumDevices = DeviceList->count();
	
	TArray<FMacPlatformMisc::FGPUDescriptor> const& GPUs = FPlatformMisc::GetGPUDescriptors();
	check(GPUs.Num() > 0);

	// @TODO  here, GetGraphicsAdapterLuid() is used as a device index (how the function "GetGraphicsAdapter" used to work)
	//        eventually we want the HMD module to return the MTLDevice's registryID, but we cannot fully handle that until
	//        we drop support for 10.12
	//  NOTE: this means any implementation of GetGraphicsAdapterLuid() for Mac should return an index, and use -1 as a 
	//        sentinel value representing "no device" (instead of 0, which is used in the LUID case)
	int32 HmdGraphicsAdapter  = IHeadMountedDisplayModule::IsAvailable() ? (int32)IHeadMountedDisplayModule::Get().GetGraphicsAdapterLuid() : -1;
 	int32 OverrideRendererId = FPlatformMisc::GetExplicitRendererIndex();
	
	int32 ExplicitRendererId = OverrideRendererId >= 0 ? OverrideRendererId : HmdGraphicsAdapter;
	if(ExplicitRendererId < 0 && GPUs.Num() > 1)
	{
		OverrideRendererId = -1;
		bool bForceExplicitRendererId = false;
		for(uint32 i = 0; i < GPUs.Num(); i++)
		{
			FMacPlatformMisc::FGPUDescriptor const& GPU = GPUs[i];
			if(!GPU.GPUHeadless && GPU.GPUVendorId != (uint32)EGpuVendorId::Intel)
			{
				OverrideRendererId = i;
			}
		}
		if (bForceExplicitRendererId)
		{
			ExplicitRendererId = OverrideRendererId;
		}
	}
	
	MTL::Device* SelectedDevice = nullptr;
	if (ExplicitRendererId >= 0 && ExplicitRendererId < GPUs.Num())
	{
		FMacPlatformMisc::FGPUDescriptor const& GPU = GPUs[ExplicitRendererId];
		TArray<FString> NameComponents;
		FString(GPU.GPUName).TrimStart().ParseIntoArray(NameComponents, TEXT(" "));	
		for (uint32 index = 0; index < NumDevices; index++)
		{
			MTL::Device* Device = (MTL::Device*)DeviceList->object(index);
			
            FString DeviceName = NSStringToFString(Device->name());
            
            if((Device->registryID() == GPU.RegistryID))
            {
                DeviceIndex = ExplicitRendererId;
                SelectedDevice = Device;
            }
			else if((DeviceName.Find(TEXT("AMD"), ESearchCase::IgnoreCase) != -1 && GPU.GPUVendorId == (uint32)EGpuVendorId::Amd)
			   || (DeviceName.Find(TEXT("Intel"), ESearchCase::IgnoreCase) != -1 && GPU.GPUVendorId == (uint32)EGpuVendorId::Intel))
			{
				bool bMatchesName = (NameComponents.Num() > 0);
				for (FString& Component : NameComponents)
				{
					bMatchesName &= DeviceName.Contains(Component);
				}
				if((Device->isHeadless() == GPU.GPUHeadless || GPU.GPUVendorId != (uint32)EGpuVendorId::Amd) && bMatchesName)
                {
					DeviceIndex = ExplicitRendererId;
					SelectedDevice = Device;
					break;
				}
			}
		}
		if(!SelectedDevice)
		{
			UE_LOG(LogMetal, Warning,  TEXT("Couldn't find Metal device to match GPU descriptor (%s) from IORegistry - using default device."), *FString(GPU.GPUName));
		}
	}
	if (SelectedDevice == nullptr)
	{
		TArray<FString> NameComponents;
		SelectedDevice = MTL::CreateSystemDefaultDevice();
		bool bFoundDefault = false;
		for (uint32 i = 0; i < GPUs.Num(); i++)
		{
			FMacPlatformMisc::FGPUDescriptor const& GPU = GPUs[i];
            FString DeviceName = NSStringToFString(SelectedDevice->name());

            if((SelectedDevice->registryID() == GPU.RegistryID))
            {
                DeviceIndex = i;
                bFoundDefault = true;
                break;
            }
            else if((DeviceName.Find(TEXT("AMD"), ESearchCase::IgnoreCase) != -1 && GPU.GPUVendorId == (uint32)EGpuVendorId::Amd)
                   || (DeviceName.Find(TEXT("Intel"), ESearchCase::IgnoreCase) != -1 && GPU.GPUVendorId == (uint32)EGpuVendorId::Intel))
			{
				NameComponents.Empty();
				bool bMatchesName = FString(GPU.GPUName).TrimStart().ParseIntoArray(NameComponents, TEXT(" ")) > 0;
				for (FString& Component : NameComponents)
				{
					bMatchesName &= DeviceName.Contains(Component);
				}
				if((SelectedDevice->isHeadless() == GPU.GPUHeadless || GPU.GPUVendorId != (uint32)EGpuVendorId::Amd) && bMatchesName)
                {
					DeviceIndex = i;
					bFoundDefault = true;
					break;
				}
			}
		}
		if(!bFoundDefault)
		{
			UE_LOG(LogMetal, Warning,  TEXT("Couldn't find Metal device %s in GPU descriptors from IORegistry - capability reporting may be wrong."), *NSStringToFString(SelectedDevice->name()));
		}
	}
	return SelectedDevice;
#endif // PLATFORM_MAC_ARM64
}

MTL::PrimitiveTopologyClass TranslatePrimitiveTopology(uint32 PrimitiveType)
{
	switch (PrimitiveType)
	{
		case PT_TriangleList:
		case PT_TriangleStrip:
			return MTL::PrimitiveTopologyClassTriangle;
		case PT_LineList:
			return MTL::PrimitiveTopologyClassLine;
		case PT_PointList:
			return MTL::PrimitiveTopologyClassPoint;
		default:
			UE_LOG(LogMetal, Fatal, TEXT("Unsupported primitive topology %d"), (int32)PrimitiveType);
			return MTL::PrimitiveTopologyClassTriangle;
	}
}
#endif

FMetalDeviceContext* FMetalDeviceContext::CreateDeviceContext()
{
	uint32 DeviceIndex = 0;
#if PLATFORM_VISIONOS && UE_USE_SWIFT_UI_MAIN
	// get the device from the compositor layer
	MTL::Device* Device = (__bridge MTL::Device*)cp_layer_renderer_get_device([IOSAppDelegate GetDelegate].SwiftLayer);
#elif PLATFORM_IOS
	MTL::Device* Device = [IOSAppDelegate GetDelegate].IOSView->MetalDevice;
#else
	MTL::Device* Device = GetMTLDevice(DeviceIndex);
	if (!Device)
	{
		FPlatformMisc::MessageBoxExt(EAppMsgType::Ok, TEXT("The graphics card in this Mac appears to erroneously report support for Metal graphics technology, which is required to run this application, but failed to create a Metal device. The application will now exit."), TEXT("Failed to initialize Metal"));
		exit(0);
	}
#endif
	
	uint32 MetalDebug = GMetalRuntimeDebugLevel;
	const bool bOverridesMetalDebug = FParse::Value( FCommandLine::Get(), TEXT( "MetalRuntimeDebugLevel=" ), MetalDebug );
	if (bOverridesMetalDebug)
	{
		GMetalRuntimeDebugLevel = MetalDebug;
	}
	
	//MTLPP_VALIDATION(MTL::ValidatedDevice::Register(Device));
	
	FMetalCommandQueue* Queue = new FMetalCommandQueue(Device, GMetalCommandQueueSize);
	check(Queue);
	
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesFences))
	{
		FMetalFencePool::Get().Initialise(Device);
	}
	
	return new FMetalDeviceContext(Device, DeviceIndex, Queue);
}

FMetalDeviceContext::FMetalDeviceContext(MTL::Device* MetalDevice, uint32 InDeviceIndex, FMetalCommandQueue* Queue)
	: FMetalContext(MetalDevice, *Queue)
	, DeviceIndex(InDeviceIndex)
	, CaptureManager(MetalDevice, *Queue)
	, SceneFrameCounter(0)
	, FrameCounter(0)
	, PSOManager(0)
	, FrameNumberRHIThread(0)
{
	CommandQueue.SetRuntimeDebuggingLevel(GMetalRuntimeDebugLevel);
	
	// If the separate present thread is enabled then an intermediate backbuffer is required
	check(!GMetalSeparatePresentThread || GMetalSupportsIntermediateBackBuffer);
	
	// Hook into the ios framepacer, if it's enabled for this platform.
	FrameReadyEvent = NULL;
	if( FPlatformRHIFramePacer::IsEnabled() || GMetalSeparatePresentThread )
	{
		FrameReadyEvent = FPlatformProcess::GetSynchEventFromPool();
		FPlatformRHIFramePacer::InitWithEvent( FrameReadyEvent );
		
		// A bit dirty - this allows the present frame pacing to match the CPU pacing by default unless you've overridden it with the CVar
		// In all likelihood the CVar is only useful for debugging.
		if (GMetalPresentFramePacing <= 0.0f)
		{
			FString FrameRateLockAsEnum;
			GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("FrameRateLock"), FrameRateLockAsEnum, GEngineIni);
	
			uint32 FrameRateLock = 0;
			FParse::Value(*FrameRateLockAsEnum, TEXT("PUFRL_"), FrameRateLock);
			if (FrameRateLock > 0)
			{
				GMetalPresentFramePacing = (float)FrameRateLock;
			}
		}
	}
	
    const bool bIsVisionOS = PLATFORM_VISIONOS;
	if (bIsVisionOS || FParse::Param(FCommandLine::Get(), TEXT("MetalIntermediateBackBuffer")) || FParse::Param(FCommandLine::Get(), TEXT("MetalOffscreenOnly")))
	{
		GMetalSupportsIntermediateBackBuffer = 1;
	}
    
    // initialize uniform allocator
    UniformBufferAllocator = new FMetalFrameAllocator(MetalDevice);
    UniformBufferAllocator->SetTargetAllocationLimitInBytes(GMetalTargetUniformAllocationLimit);
    UniformBufferAllocator->SetDefaultAllocationSizeInBytes(GMetalDefaultUniformBufferAllocation);
    UniformBufferAllocator->SetStatIds(GET_STATID(STAT_MetalUniformAllocatedMemory), GET_STATID(STAT_MetalUniformMemoryInFlight), GET_STATID(STAT_MetalUniformBytesPerFrame));
	
	TransferBufferAllocator = new FMetalFrameAllocator(MetalDevice);
	TransferBufferAllocator->SetTargetAllocationLimitInBytes(GMetalTargetTransferAllocatorLimit);
	TransferBufferAllocator->SetDefaultAllocationSizeInBytes(GMetalDefaultTransferAllocation);
	// We won't set StatIds here so it goes to the default frame allocator stats
	
	PSOManager = new FMetalPipelineStateCacheManager();
	
#if METAL_RHI_RAYTRACING
	InitializeRayTracing();
#endif
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
    BindlessDescriptorManager = new FMetalBindlessDescriptorManager();
#endif

	METAL_GPUPROFILE(FMetalProfiler::CreateProfiler(this));
	
	InitFrame();
}

FMetalDeviceContext::~FMetalDeviceContext()
{
	SubmitCommandsHint(EMetalSubmitFlagsWaitOnCommandBuffer);
	delete &(GetCommandQueue());
	
	delete PSOManager;
    
    delete UniformBufferAllocator;

    ShutdownPipelineCache();
    
#if METAL_RHI_RAYTRACING
	CleanUpRayTracing();
#endif
#if PLATFORM_SUPPORTS_BINDLESS_RENDERING
    delete BindlessDescriptorManager;
#endif
	
#if PLATFORM_MAC
    MTL::RemoveDeviceObserver(GMetalDeviceObserver);
#endif
}

void FMetalDeviceContext::Init(void)
{
	Heap.Init(GetCommandQueue());
}

void FMetalDeviceContext::BeginFrame()
{
#if ENABLE_METAL_GPUPROFILE
	FPlatformTLS::SetTlsValue(CurrentContextTLSSlot, this);
#endif
	
	// Wait for the frame semaphore on the immediate context.
	dispatch_semaphore_wait(CommandBufferSemaphore, DISPATCH_TIME_FOREVER);

#if METAL_RHI_RAYTRACING
	UpdateRayTracing();
#endif // METAL_RHI_RAYTRACING
}

#if METAL_DEBUG_OPTIONS
void FMetalDeviceContext::ScribbleBuffer(MTL::Buffer* Buffer, const NS::Range& Range)
{
	static uint8 Fill = 0;
	if (Buffer->storageMode() != MTL::StorageModePrivate)
	{
		FMemory::Memset((uint8_t*)Buffer->contents() + Range.location, Fill++, Range.length);
#if PLATFORM_MAC
		if (Buffer->storageMode() == MTL::StorageModeManaged)
		{
			Buffer->didModifyRange(Range);
		}
#endif
	}
	else
	{
		FillBuffer(Buffer, NS::Range(0, Range.length), Fill++);
	}
}
#endif

void FMetalDeviceContext::ClearFreeList()
{
	uint32 Index = 0;
	while(Index < DelayedFreeLists.Num())
	{
		FMetalDelayedFreeList* Pair = DelayedFreeLists[Index];
		if(METAL_DEBUG_OPTION(Pair->DeferCount-- <= 0 &&) Pair->IsComplete())
		{
			for(NS::Object* Entry : Pair->ObjectFreeList )
			{
				Entry->release();
			}
			for (FMetalBufferPtr Buffer : Pair->UsedBuffers)
			{
#if METAL_DEBUG_OPTIONS
                MTL::Buffer* MTLBuffer = Buffer->GetMTLBuffer().get();
				if (GMetalBufferScribble)
				{
					ScribbleBuffer(MTLBuffer, Buffer->GetRange());
				}
				if (GMetalResourcePurgeOnDelete && !MTLBuffer->heap() &&
                    Buffer->GetOffset() == 0 && Buffer->GetLength() == MTLBuffer->length())
				{
                    MTLBuffer->setPurgeableState(MTL::PurgeableStateEmpty);
				}
#endif
                Heap.ReleaseBuffer(Buffer);
			}
			for (MTLTexturePtr Texture : Pair->UsedTextures)
			{
                if (!Texture->buffer() && !Texture->parentTexture())
				{
#if METAL_DEBUG_OPTIONS
					if (GMetalResourcePurgeOnDelete && !Texture->heap())
					{
						Texture->setPurgeableState(MTL::PurgeableStateEmpty);
					}
#endif
					Heap.ReleaseTexture(nullptr, Texture);
				}
			}
			for (FMetalFence* Fence : Pair->FenceFreeList)
			{
				FMetalFencePool::Get().ReleaseFence(Fence);
			}
            for (TFunction<void()>& Function : Pair->FunctionFreeList)
            {
                Function();
            }
			delete Pair;
			DelayedFreeLists.RemoveAt(Index, 1, EAllowShrinking::No);
		}
		else
		{
			Index++;
		}
	}
}

void FMetalDeviceContext::DrainHeap()
{
	Heap.Compact(&RenderPass, false);
}

void FMetalDeviceContext::EndFrame()
{
	check(MetalIsSafeToUseRHIThreadResources());
	
	// A 'frame' in this context is from the beginning of encoding on the CPU
	// to the end of all rendering operations on the GPU. So the semaphore is
	// signalled when the last command buffer finishes GPU execution.
	{
		dispatch_semaphore_t CmdBufferSemaphore = CommandBufferSemaphore;
		dispatch_retain(CmdBufferSemaphore);
		
        FMetalCommandBufferCompletionHandler Handler;
        Handler.BindLambda([CmdBufferSemaphore](MTL::CommandBuffer* CommandBuffer)
        {
             dispatch_semaphore_signal(CmdBufferSemaphore);
             dispatch_release(CmdBufferSemaphore);
        });
		RenderPass.AddCompletionHandler(Handler);
	}
	
	if (bPresented)
	{
		CaptureManager.PresentFrame(FrameCounter++);
		bPresented = false;
	}
	
	// Force submission so the completion handler that signals CommandBufferSemaphore fires.
	uint32 SubmitFlags = EMetalSubmitFlagsResetState | EMetalSubmitFlagsForce | EMetalSubmitFlagsLastCommandBuffer;
#if METAL_DEBUG_OPTIONS
	// Latched update of whether to use runtime debugging features
	if (GMetalRuntimeDebugLevel != CommandQueue.GetRuntimeDebuggingLevel())
	{
		CommandQueue.SetRuntimeDebuggingLevel(GMetalRuntimeDebugLevel);
		
		// After change the debug features level wait on commit
		SubmitFlags |= EMetalSubmitFlagsWaitOnCommandBuffer;
	}
#endif
    
	SubmitCommandsHint((uint32)SubmitFlags);
    
    // increment the internal frame counter
    FrameNumberRHIThread++;
	
    FlushFreeList();
    
    ClearFreeList();
    
	DrainHeap();
    
	InitFrame();
}

void FMetalDeviceContext::BeginScene()
{
#if ENABLE_METAL_GPUPROFILE
	FPlatformTLS::SetTlsValue(CurrentContextTLSSlot, this);
#endif
	
	// Increment the frame counter. INDEX_NONE is a special value meaning "uninitialized", so if
	// we hit it just wrap around to zero.
	SceneFrameCounter++;
	if (SceneFrameCounter == INDEX_NONE)
	{
		SceneFrameCounter++;
	}
}

void FMetalDeviceContext::EndScene()
{
}

void FMetalDeviceContext::BeginDrawingViewport(FMetalViewport* Viewport)
{
#if ENABLE_METAL_GPUPROFILE
	FPlatformTLS::SetTlsValue(CurrentContextTLSSlot, this);
#endif
}

bool FMetalDeviceContext::FMetalDelayedFreeList::IsComplete() const
{
	bool bFinished = true;
	for (TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe> Fence : Fences)
	{
		bFinished &= Fence->Wait(0);

		if (!bFinished)
			break;
	}
	return bFinished;
}

void FMetalDeviceContext::FlushFreeList(bool const bFlushFences)
{
	FMetalDelayedFreeList* NewList = new FMetalDelayedFreeList;
	
	// Get the committed command buffer fences and clear the array in the command-queue
	GetCommandQueue().GetCommittedCommandBufferFences(NewList->Fences);
	
	METAL_DEBUG_OPTION(NewList->DeferCount = GMetalResourceDeferDeleteNumFrames);
	FreeListMutex.Lock();
	NewList->UsedBuffers = MoveTemp(UsedBuffers);
	NewList->UsedTextures = MoveTemp(UsedTextures);
	NewList->ObjectFreeList = ObjectFreeList;
	if (bFlushFences)
	{
		TArray<FMetalFence*> Fences;
		FenceFreeList.PopAll(Fences);
		for (FMetalFence* Fence : Fences)
		{
			if(!UsedFences.Contains(Fence))
			{
				UsedFences.Add(Fence);
			}
		}
		NewList->FenceFreeList = MoveTemp(UsedFences);
        
	}
    NewList->FunctionFreeList = MoveTemp(FunctionFreeList);
    
#if METAL_DEBUG_OPTIONS
	if (FrameFences.Num())
	{
		FrameFences.Empty();
	}
#endif
	ObjectFreeList.Empty(ObjectFreeList.Num());
	FreeListMutex.Unlock();
	
	DelayedFreeLists.Add(NewList);
}

void FMetalDeviceContext::EndDrawingViewport(FMetalViewport* Viewport, bool bPresent, bool bLockToVsync)
{
	// enqueue a present if desired
	static bool const bOffscreenOnly = FParse::Param(FCommandLine::Get(), TEXT("MetalOffscreenOnly"));
	if (bPresent && !bOffscreenOnly)
	{
		
        bool bNeedNativePresent = true;
#if PLATFORM_MAC || PLATFORM_VISIONOS
		// Handle custom present
		FRHICustomPresent* const CustomPresent = Viewport->GetCustomPresent();
		if (CustomPresent != nullptr)
		{
			int32 SyncInterval = 0;
			{
				SCOPE_CYCLE_COUNTER(STAT_MetalCustomPresentTime);
                FMetalRHICommandContext* RHICommandContext = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
                RHICommandContext->SetCustomPresentViewport(Viewport);
                bNeedNativePresent = CustomPresent->Present(SyncInterval);
                RHICommandContext->SetCustomPresentViewport(nullptr);
			}
			
            FMetalCommandBuffer* CurrentCommandBuffer = GetCurrentCommandBuffer();
            check(CurrentCommandBuffer && CurrentCommandBuffer->GetMTLCmdBuffer());
			
            MTL::HandlerFunction Handler = [CustomPresent](MTL::CommandBuffer*) {
                CustomPresent->PostPresent();
            };
            
			CurrentCommandBuffer->GetMTLCmdBuffer()->addScheduledHandler(Handler);
		}
#endif
		
		RenderPass.End();
		if (bNeedNativePresent)
        {
            SubmitCommandsHint(EMetalSubmitFlagsForce|EMetalSubmitFlagsCreateCommandBuffer);
            
            Viewport->Present(GetCommandQueue(), bLockToVsync);
        }
	}
	
	bPresented = bPresent;
	
	// We may be limiting our framerate to the display link
	if( FrameReadyEvent != nullptr && !GMetalSeparatePresentThread )
	{
		bool bIgnoreThreadIdleStats = true; // Idle time is already counted by the caller
		FrameReadyEvent->Wait(MAX_uint32, bIgnoreThreadIdleStats);
	}
	
	Viewport->ReleaseDrawable();
}

void FMetalDeviceContext::ReleaseObject(NS::Object* obj)
{
	if (GIsMetalInitialized) // @todo zebra: there seems to be some race condition at exit when the framerate is very low
	{
		check(obj);
		FreeListMutex.Lock();
		if(!ObjectFreeList.Contains(obj))
        {
            ObjectFreeList.Add(obj);
        }
        else
        {
            obj->release();
        }
		FreeListMutex.Unlock();
	}
}

void FMetalDeviceContext::ReleaseTexture(FMetalSurface* Surface, MTLTexturePtr Texture)
{
	if (GIsMetalInitialized) // @todo zebra: there seems to be some race condition at exit when the framerate is very low
	{
		check(Surface && Texture);
		ReleaseTexture(Texture);
	}
}

void FMetalDeviceContext::ReleaseTexture(MTLTexturePtr Texture)
{
	if(GIsMetalInitialized)
	{
		check(Texture);
		FreeListMutex.Lock();
		if(!UsedTextures.Contains(Texture))
		{
			UsedTextures.Add(Texture);
		}
		FreeListMutex.Unlock();
	}
}

void FMetalDeviceContext::ReleaseFence(FMetalFence* Fence)
{
#if METAL_DEBUG_OPTIONS
	if(GetCommandList().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation)
	{
		FScopeLock Lock(&FreeListMutex);
		FrameFences.Add(Fence);
	}
#endif
	
	if (GIsMetalInitialized) // @todo zebra: there seems to be some race condition at exit when the framerate is very low
	{
		check(Fence);
		FenceFreeList.Push(Fence);
	}
}

void FMetalDeviceContext::ReleaseFunction(TFunction<void()> Func)
{
    if (GIsMetalInitialized)
    {
        FunctionFreeList.Push(Func);
    }
}

MTLTexturePtr FMetalDeviceContext::CreateTexture(FMetalSurface* Surface, MTL::TextureDescriptor* Descriptor)
{
	MTLTexturePtr Tex = Heap.CreateTexture(Descriptor, Surface);
#if METAL_DEBUG_OPTIONS
	if (GMetalResourcePurgeOnDelete && !Tex->heap())
	{
		Tex->setPurgeableState(MTL::PurgeableStateNonVolatile);
	}
#endif
	
	return Tex;
}

FMetalBufferPtr FMetalDeviceContext::CreatePooledBuffer(FMetalPooledBufferArgs const& Args)
{   
	NS::UInteger CpuResourceOption = ((NS::UInteger)Args.CpuCacheMode) << MTL::ResourceCpuCacheModeShift;
	
	uint32 RequestedBufferOffsetAlignment = BufferOffsetAlignment;
	
	if(EnumHasAnyFlags(Args.Flags, BUF_UnorderedAccess | BUF_ShaderResource))
	{
		// Buffer backed linear textures have specific align requirements
		// We don't know upfront the pixel format that may be requested for an SRV so we can't use minimumLinearTextureAlignmentForPixelFormat:
		RequestedBufferOffsetAlignment = BufferBackedLinearTextureOffsetAlignment;
	}
	
	MTL::ResourceOptions HazardTrackingMode = MTL::ResourceHazardTrackingModeUntracked;
	static bool bSupportsHeaps = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesHeaps);
	if(bSupportsHeaps)
	{
		HazardTrackingMode = MTL::ResourceHazardTrackingModeTracked;
	}
	
    FMetalBufferPtr Buffer = Heap.CreateBuffer(Args.Size, RequestedBufferOffsetAlignment, Args.Flags, FMetalCommandQueue::GetCompatibleResourceOptions((MTL::ResourceOptions)(CpuResourceOption | HazardTrackingMode | ((NS::UInteger)Args.Storage << MTL::ResourceStorageModeShift))));
	
    check(Buffer);
#if METAL_DEBUG_OPTIONS
    MTL::Buffer* MTLBuffer = Buffer->GetMTLBuffer().get();
	if (GMetalResourcePurgeOnDelete && !MTLBuffer->heap())
	{
        MTLBuffer->setPurgeableState(MTL::PurgeableStateNonVolatile);
	}
#endif
	
	return Buffer;
}

void FMetalDeviceContext::ReleaseBuffer(FMetalBufferPtr Buffer)
{
	if(GIsMetalInitialized)
	{
		check(Buffer);
		FreeListMutex.Lock();
		if(!UsedBuffers.Contains(Buffer))
		{
			UsedBuffers.Add(MoveTemp(Buffer));
		}
		FreeListMutex.Unlock();
	}
}

uint32 FMetalDeviceContext::GetDeviceIndex(void) const
{
	return DeviceIndex;
}

void FMetalDeviceContext::NewLock(FMetalRHIBuffer* Buffer, FMetalFrameAllocator::AllocationEntry& Allocation)
{
	check(!OutstandingLocks.Contains(Buffer));
	OutstandingLocks.Add(Buffer, Allocation);
}

FMetalFrameAllocator::AllocationEntry FMetalDeviceContext::FetchAndRemoveLock(FMetalRHIBuffer* Buffer)
{
	FMetalFrameAllocator::AllocationEntry Backing = OutstandingLocks.FindAndRemoveChecked(Buffer);
	return Backing;
}

#if METAL_DEBUG_OPTIONS
void FMetalDeviceContext::AddActiveBuffer(MTL::Buffer* Buffer, const NS::Range& Range)
{
    if(GetCommandList().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation)
    {
        FScopeLock Lock(&ActiveBuffersMutex);
        
        NS::Range DestRange = NS::Range::Make(Range.location, Range.length);
        TArray<NS::Range>* Ranges = ActiveBuffers.Find(Buffer);
        if (!Ranges)
        {
            ActiveBuffers.Add(Buffer, TArray<NS::Range>());
            Ranges = ActiveBuffers.Find(Buffer);
        }
        Ranges->Add(DestRange);
    }
}

static bool operator==(NSRange const& A, NSRange const& B)
{
    return NSEqualRanges(A, B);
}

void FMetalDeviceContext::RemoveActiveBuffer(MTL::Buffer* Buffer, const NS::Range& Range)
{
    if(GetCommandList().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation)
    {
        FScopeLock Lock(&ActiveBuffersMutex);
        
        TArray<NS::Range>& Ranges = ActiveBuffers.FindChecked(Buffer);
        int32 i = Ranges.RemoveSingle(Range);
        check(i > 0);
    }
}

bool FMetalDeviceContext::ValidateIsInactiveBuffer(MTL::Buffer* Buffer, const NS::Range& DestRange)
{
    if(GetCommandList().GetCommandQueue().GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation)
    {
        FScopeLock Lock(&ActiveBuffersMutex);
        
        TArray<NS::Range>* Ranges = ActiveBuffers.Find(Buffer);
        if (Ranges)
        {
            for (NS::Range Range : *Ranges)
            {
                if(DestRange.location < Range.location + Range.length ||
                   Range.location < DestRange.location + DestRange.length)
                {
                    continue;
                }
                
                UE_LOG(LogMetal, Error, TEXT("ValidateIsInactiveBuffer failed on overlapping ranges ({%d, %d} vs {%d, %d}) of buffer %p."), (uint32)Range.location, (uint32)Range.length, (uint32)DestRange.location, (uint32)DestRange.length, Buffer);
                return false;
            }
        }
    }
    return true;
}
#endif


#if ENABLE_METAL_GPUPROFILE
uint32 FMetalContext::CurrentContextTLSSlot = FPlatformTLS::AllocTlsSlot();
#endif

FMetalContext::FMetalContext(MTL::Device* InDevice, FMetalCommandQueue& Queue)
	: Device(InDevice)
	, CommandQueue(Queue)
	, CommandList(Queue)
	, StateCache(InDevice, true)
	, RenderPass(CommandList, StateCache)
	, QueryBuffer(new FMetalQueryBufferPool(this))
{
	// create a semaphore for multi-buffering the command buffer
	CommandBufferSemaphore = dispatch_semaphore_create(FParse::Param(FCommandLine::Get(),TEXT("gpulockstep")) ? 1 : 3);
    Device->retain();
}

FMetalContext::~FMetalContext()
{
	SubmitCommandsHint(EMetalSubmitFlagsWaitOnCommandBuffer);
    Device->release();
}

MTL::Device* FMetalContext::GetDevice()
{
	return Device;
}

FMetalCommandQueue& FMetalContext::GetCommandQueue()
{
	return CommandQueue;
}

FMetalCommandList& FMetalContext::GetCommandList()
{
	return CommandList;
}

FMetalCommandBuffer* FMetalContext::GetCurrentCommandBuffer()
{
	return RenderPass.GetCurrentCommandBuffer();
}

void FMetalContext::InsertCommandBufferFence(TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe>& Fence, FMetalCommandBufferCompletionHandler Handler)
{
	check(GetCurrentCommandBuffer());
	
	RenderPass.InsertCommandBufferFence(Fence, Handler);
}

#if ENABLE_METAL_GPUPROFILE
FMetalContext* FMetalContext::GetCurrentContext()
{
	FMetalContext* Current = (FMetalContext*)FPlatformTLS::GetTlsValue(CurrentContextTLSSlot);
	
	if (!Current)
	{
		// If we are executing this outside of a pass we'll return the default.
		// TODO This needs further investigation. We should fix all the cases that call this without
		// a context set.
		FMetalRHICommandContext* CmdContext = static_cast<FMetalRHICommandContext*>(RHIGetDefaultContext());
		check(CmdContext);
		Current = &CmdContext->GetInternalContext();
	}
	
	check(Current);
	return Current;
}

void FMetalContext::MakeCurrent(FMetalContext* Context)
{
	FPlatformTLS::SetTlsValue(CurrentContextTLSSlot, Context);
}
#endif

void FMetalContext::InitFrame()
{
#if ENABLE_METAL_GPUPROFILE
	FPlatformTLS::SetTlsValue(CurrentContextTLSSlot, this);
#endif
	
	// Reset cached state in the encoder
	StateCache.Reset();
	
	// Reallocate if necessary to ensure >= 80% usage, otherwise we're just too wasteful
	RenderPass.ShrinkRingBuffers();
	
	// Begin the render pass frame.
	RenderPass.Begin();
	
	// make sure first SetRenderTarget goes through
	StateCache.InvalidateRenderTargets();
}

void FMetalContext::FinishFrame(bool const bImmediateContext)
{
	// End the render pass
	RenderPass.End();
	
	// Issue any outstanding commands.
	SubmitCommandsHint(EMetalSubmitFlagsNone);
	
	// make sure first SetRenderTarget goes through
	StateCache.InvalidateRenderTargets();
	
	if (!bImmediateContext)
	{
		StateCache.Reset();
	}

#if ENABLE_METAL_GPUPROFILE
	FPlatformTLS::SetTlsValue(CurrentContextTLSSlot, nullptr);
#endif
}

void FMetalContext::TransitionResource(FRHIUnorderedAccessView* InResource)
{
	FMetalUnorderedAccessView* UAV = ResourceCast(InResource);

	if (UAV->IsTexture())
	{
		FMetalSurface* Surface = ResourceCast(UAV->GetTexture());
		if (Surface->Texture)
		{
			RenderPass.TransitionResources(Surface->Texture.get());
			if (Surface->MSAATexture)
			{
				RenderPass.TransitionResources(Surface->MSAATexture.get());
			}
		}
	}
	else
	{
		FMetalRHIBuffer* Buffer = ResourceCast(UAV->GetBuffer());
		RenderPass.TransitionResources(Buffer->GetCurrentBuffer()->GetMTLBuffer().get());
	}
}

void FMetalContext::TransitionResource(FRHITexture* InResource)
{
	FMetalSurface* Surface = GetMetalSurfaceFromRHITexture(InResource);

	if ((Surface != nullptr) && Surface->Texture)
	{
		RenderPass.TransitionResources(Surface->Texture.get());
		if (Surface->MSAATexture)
		{
			RenderPass.TransitionResources(Surface->MSAATexture.get());
		}
	}
}

void FMetalContext::SubmitCommandsHint(uint32 const Flags)
{
#if !PLATFORM_MAC
    if (RenderPass.IsWithinRenderPass() && !StateCache.CanRestartRenderPass())
    {
		// Make sure we don't try to end render-passes that can't be restarted (eg. render-passes with a memoryless targets)
		return;
    }
#endif
    
    // When the command-buffer is submitted for a reason other than a break of a logical command-buffer (where one high-level command-sequence becomes more than one command-buffer).
	if (!(Flags & EMetalSubmitFlagsBreakCommandBuffer))
	{
		// Release the current query buffer if there are outstanding writes so that it isn't transitioned by a future encoder that will cause a resource access conflict and lifetime error.
		GetQueryBufferPool()->ReleaseCurrentQueryBuffer();
	}
	
	RenderPass.Submit((EMetalSubmitFlags)Flags);
}

void FMetalContext::SubmitCommandBufferAndWait()
{
	// kick the whole buffer
	// Commit to hand the commandbuffer off to the gpu
	// Wait for completion as requested.
	SubmitCommandsHint((EMetalSubmitFlagsCreateCommandBuffer | EMetalSubmitFlagsBreakCommandBuffer | EMetalSubmitFlagsWaitOnCommandBuffer));
}

void FMetalContext::ResetRenderCommandEncoder()
{
	SubmitCommandsHint();
	
	StateCache.InvalidateRenderTargets();
	
	SetRenderPassInfo(StateCache.GetRenderPassInfo(), true);
}

bool FMetalContext::PrepareToDraw(uint32 PrimitiveType)
{
	SCOPE_CYCLE_COUNTER(STAT_MetalPrepareDrawTime);
	TRefCountPtr<FMetalGraphicsPipelineState> CurrentPSO = StateCache.GetGraphicsPSO();
	check(IsValidRef(CurrentPSO));
	
	// Enforce calls to SetRenderTarget prior to issuing draw calls.
	if (!StateCache.GetHasValidRenderTarget())
	{
		return false;
	}
	
	FMetalHashedVertexDescriptor const& VertexDesc = CurrentPSO->VertexDeclaration->Layout;
	
	// Validate the vertex layout in debug mode, or when the validation layer is enabled for development builds.
	// Other builds will just crash & burn if it is incorrect.
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	if(CommandQueue.GetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation)
	{
		MTLVertexDescriptorPtr Layout = VertexDesc.VertexDesc;
		
		if(Layout && Layout->layouts())
		{
			for (uint32 i = 0; i < MaxVertexElementCount; i++)
			{
				auto Attribute = Layout->attributes()->object(i);
				if(Attribute && Attribute->format() > MTL::VertexFormatInvalid)
				{
					auto BufferLayout = Layout->layouts()->object(Attribute->bufferIndex());
					uint32 BufferLayoutStride = BufferLayout ? BufferLayout->stride() : 0;
					
					uint32 BufferIndex = METAL_TO_UNREAL_BUFFER_INDEX(Attribute->bufferIndex());
					
					if (CurrentPSO->VertexShader->Bindings.InOutMask.IsFieldEnabled(BufferIndex))
					{
						uint64 MetalSize = StateCache.GetVertexBufferSize(BufferIndex);
						
						// If the vertex attribute is required and either no Metal buffer is bound or the size of the buffer is smaller than the stride, or the stride is explicitly specified incorrectly then the layouts don't match.
						if (BufferLayoutStride > 0 && MetalSize < BufferLayoutStride)
						{
							FString Report = FString::Printf(TEXT("Vertex Layout Mismatch: Index: %d, Len: %lld, Decl. Stride: %d"), Attribute->bufferIndex(), MetalSize, BufferLayoutStride);
							UE_LOG(LogMetal, Warning, TEXT("%s"), *Report);
						}
					}
				}
			}
		}
	}
#endif
	
	// @todo Handle the editor not setting a depth-stencil target for the material editor's tiles which render to depth even when they shouldn't.
	bool const bNeedsDepthStencilWrite = (IsValidRef(CurrentPSO->PixelShader) && (CurrentPSO->PixelShader->Bindings.InOutMask.IsFieldEnabled(CrossCompiler::FShaderBindingInOutMask::DepthStencilMaskIndex)));
	
	// @todo Improve the way we handle binding a dummy depth/stencil so we can get pure UAV raster operations...
	bool const bNeedsDepthStencilForUAVRaster = (StateCache.GetRenderPassInfo().GetNumColorRenderTargets() == 0);
	
	bool const bBindDepthStencilForWrite = bNeedsDepthStencilWrite && !StateCache.HasValidDepthStencilSurface();
	bool const bBindDepthStencilForUAVRaster = bNeedsDepthStencilForUAVRaster && !StateCache.HasValidDepthStencilSurface();
	
	if (bBindDepthStencilForWrite || bBindDepthStencilForUAVRaster)
	{
#if UE_BUILD_DEBUG
		if (bBindDepthStencilForWrite)
		{
			UE_LOG(LogMetal, Warning, TEXT("Binding a temporary depth-stencil surface as the bound shader pipeline writes to depth/stencil but no depth/stencil surface was bound!"));
		}
		else
		{
			check(bNeedsDepthStencilForUAVRaster);
			UE_LOG(LogMetal, Warning, TEXT("Binding a temporary depth-stencil surface as the bound shader pipeline needs a texture bound - even when only writing to UAVs!"));
		}
#endif
		check(StateCache.GetRenderTargetArraySize() <= 1);
		CGSize FBSize;
		if (bBindDepthStencilForWrite)
		{
			check(!bBindDepthStencilForUAVRaster);
			FBSize = StateCache.GetFrameBufferSize();
		}
		else
		{
			check(bBindDepthStencilForUAVRaster);
			FBSize = CGSizeMake(StateCache.GetViewport(0).width, StateCache.GetViewport(0).height);
		}
		
		FRHIRenderPassInfo Info = StateCache.GetRenderPassInfo();
		
		FTexture2DRHIRef FallbackDepthStencilSurface = StateCache.CreateFallbackDepthStencilSurface(FBSize.width, FBSize.height);
		check(IsValidRef(FallbackDepthStencilSurface));
		
		if (bBindDepthStencilForWrite)
		{
			check(!bBindDepthStencilForUAVRaster);
			Info.DepthStencilRenderTarget.DepthStencilTarget = FallbackDepthStencilSurface;
			Info.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthWrite_StencilWrite;
			Info.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(MakeRenderTargetActions(ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore), MakeRenderTargetActions(ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::EStore));
		}
		else
		{
			check(bBindDepthStencilForUAVRaster);
			Info.DepthStencilRenderTarget.DepthStencilTarget = FallbackDepthStencilSurface;
			Info.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthRead_StencilRead;
			Info.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(MakeRenderTargetActions(ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::ENoAction), MakeRenderTargetActions(ERenderTargetLoadAction::ELoad, ERenderTargetStoreAction::ENoAction));
		}
		
		// Ensure that we make it a Clear/Store -> Load/Store for the colour targets or we might render incorrectly
		for (uint32 i = 0; i < Info.GetNumColorRenderTargets(); i++)
		{
			if (GetLoadAction(Info.ColorRenderTargets[i].Action) != ERenderTargetLoadAction::ELoad)
			{
				check(GetStoreAction(Info.ColorRenderTargets[i].Action) == ERenderTargetStoreAction::EStore || GetStoreAction(Info.ColorRenderTargets[i].Action) == ERenderTargetStoreAction::EMultisampleResolve);
				Info.ColorRenderTargets[i].Action = MakeRenderTargetActions(ERenderTargetLoadAction::ELoad, GetStoreAction(Info.ColorRenderTargets[i].Action));
			}
		}
		
		if (StateCache.SetRenderPassInfo(Info, StateCache.GetVisibilityResultsBuffer(), true))
		{
			RenderPass.RestartRenderPass(StateCache.GetRenderPassDescriptor());
		}
		
		if (bBindDepthStencilForUAVRaster)
		{
            MTL::ScissorRect Rect = {0, 0, (NS::UInteger)FBSize.width, (NS::UInteger)FBSize.height};
			StateCache.SetScissorRect(false, Rect);
		}
		
		check(StateCache.GetHasValidRenderTarget());
	}
	else if (!bNeedsDepthStencilWrite && !bNeedsDepthStencilForUAVRaster && StateCache.GetFallbackDepthStencilBound())
	{
		FRHIRenderPassInfo Info = StateCache.GetRenderPassInfo();
		Info.DepthStencilRenderTarget.DepthStencilTarget = nullptr;
		
		RenderPass.EndRenderPass();
		
		StateCache.SetRenderTargetsActive(false);
		StateCache.SetRenderPassInfo(Info, StateCache.GetVisibilityResultsBuffer(), true);
		
		RenderPass.BeginRenderPass(StateCache.GetRenderPassDescriptor());
		
		check(StateCache.GetHasValidRenderTarget());
	}
	
	return true;
}

void FMetalContext::SetRenderPassInfo(const FRHIRenderPassInfo& RenderTargetsInfo, bool const bRestart)
{
	bool bSet = false;
	{
		// @todo Improve the way we handle binding a dummy depth/stencil so we can get pure UAV raster operations...
		const bool bNeedsDepthStencilForUAVRaster = RenderTargetsInfo.GetNumColorRenderTargets() == 0 && !RenderTargetsInfo.DepthStencilRenderTarget.DepthStencilTarget;

		if (bNeedsDepthStencilForUAVRaster)
		{
			FRHIRenderPassInfo Info = RenderTargetsInfo;
			CGSize FBSize = CGSizeMake(StateCache.GetViewport(0).width, StateCache.GetViewport(0).height);
			FTexture2DRHIRef FallbackDepthStencilSurface = StateCache.CreateFallbackDepthStencilSurface(FBSize.width, FBSize.height);
			check(IsValidRef(FallbackDepthStencilSurface));

			Info.DepthStencilRenderTarget.DepthStencilTarget = FallbackDepthStencilSurface;
			Info.DepthStencilRenderTarget.ResolveTarget = nullptr;
			Info.DepthStencilRenderTarget.ExclusiveDepthStencil = FExclusiveDepthStencil::DepthRead_StencilRead;
#if PLATFORM_MAC
			Info.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(MakeRenderTargetActions(ERenderTargetLoadAction::ENoAction, ERenderTargetStoreAction::ENoAction), MakeRenderTargetActions(ERenderTargetLoadAction::ENoAction, ERenderTargetStoreAction::ENoAction));
#else
			Info.DepthStencilRenderTarget.Action = MakeDepthStencilTargetActions(MakeRenderTargetActions(ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::ENoAction), MakeRenderTargetActions(ERenderTargetLoadAction::EClear, ERenderTargetStoreAction::ENoAction));
#endif

			if (QueryBuffer->GetCurrentQueryBuffer() != StateCache.GetVisibilityResultsBuffer())
			{
				RenderPass.EndRenderPass();
			}
			bSet = StateCache.SetRenderPassInfo(Info, QueryBuffer->GetCurrentQueryBuffer(), bRestart);
		}
		else
		{
			if (QueryBuffer->GetCurrentQueryBuffer() != StateCache.GetVisibilityResultsBuffer())
			{
				RenderPass.EndRenderPass();
			}
			bSet = StateCache.SetRenderPassInfo(RenderTargetsInfo, QueryBuffer->GetCurrentQueryBuffer(), bRestart);
		}
	}
	
	if (bSet && StateCache.GetHasValidRenderTarget())
	{
		RenderPass.EndRenderPass();
		RenderPass.BeginRenderPass(StateCache.GetRenderPassDescriptor());
	}
}

void FMetalContext::EndRenderPass()
{
    RenderPass.EndRenderPass();
}

FMetalBufferPtr FMetalContext::AllocateFromRingBuffer(uint32 Size, uint32 Alignment)
{
	return RenderPass.GetRingBuffer().NewBuffer(Size, Alignment);
}

void FMetalContext::DrawPrimitive(uint32 PrimitiveType, uint32 BaseVertexIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	// finalize any pending state
	if(!PrepareToDraw(PrimitiveType))
	{
		return;
	}
	
	RenderPass.DrawPrimitive(PrimitiveType, BaseVertexIndex, NumPrimitives, NumInstances);
}

void FMetalContext::DrawPrimitiveIndirect(uint32 PrimitiveType, FMetalRHIBuffer* VertexBuffer, uint32 ArgumentOffset)
{
	// finalize any pending state
	if(!PrepareToDraw(PrimitiveType))
	{
		return;
	}
	
	RenderPass.DrawPrimitiveIndirect(PrimitiveType, VertexBuffer, ArgumentOffset);
}

void FMetalContext::DrawIndexedPrimitive(FMetalBufferPtr IndexBuffer, uint32 IndexStride, MTL::IndexType IndexType, uint32 PrimitiveType, int32 BaseVertexIndex, uint32 FirstInstance, uint32 NumVertices, uint32 StartIndex, uint32 NumPrimitives, uint32 NumInstances)
{
	// finalize any pending state
	if(!PrepareToDraw(PrimitiveType))
	{
		return;
	}
	
	RenderPass.DrawIndexedPrimitive(IndexBuffer, IndexStride, PrimitiveType, BaseVertexIndex, FirstInstance, NumVertices, StartIndex, NumPrimitives, NumInstances);
}

void FMetalContext::DrawIndexedIndirect(FMetalRHIBuffer* IndexBuffer, uint32 PrimitiveType, FMetalRHIBuffer* VertexBuffer, int32 DrawArgumentsIndex)
{
	// finalize any pending state
	if(!PrepareToDraw(PrimitiveType))
	{
		return;
	}
	
	RenderPass.DrawIndexedIndirect(IndexBuffer, PrimitiveType, VertexBuffer, DrawArgumentsIndex);
}

void FMetalContext::DrawIndexedPrimitiveIndirect(uint32 PrimitiveType,FMetalRHIBuffer* IndexBuffer,FMetalRHIBuffer* VertexBuffer, uint32 ArgumentOffset)
{	
	// finalize any pending state
	if(!PrepareToDraw(PrimitiveType))
	{
		return;
	}
	
	RenderPass.DrawIndexedPrimitiveIndirect(PrimitiveType, IndexBuffer, VertexBuffer, ArgumentOffset);
}

void FMetalContext::CopyFromTextureToBuffer(MTL::Texture* Texture, uint32 sourceSlice, uint32 sourceLevel, MTL::Origin sourceOrigin, MTL::Size sourceSize, FMetalBufferPtr toBuffer, uint32 destinationOffset, uint32 destinationBytesPerRow, uint32 destinationBytesPerImage, MTL::BlitOption options)
{
	RenderPass.CopyFromTextureToBuffer(Texture, sourceSlice, sourceLevel, sourceOrigin, sourceSize,
                                       toBuffer, destinationOffset, destinationBytesPerRow, destinationBytesPerImage, options);
}

void FMetalContext::CopyFromBufferToTexture(FMetalBufferPtr Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, MTL::Size sourceSize, MTL::Texture* toTexture, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin, MTL::BlitOption options)
{
	RenderPass.CopyFromBufferToTexture(Buffer, sourceOffset, sourceBytesPerRow, sourceBytesPerImage, sourceSize,
                                       toTexture, destinationSlice, destinationLevel, destinationOrigin, options);
}

void FMetalContext::CopyFromTextureToTexture(MTL::Texture* Texture, uint32 sourceSlice, uint32 sourceLevel, MTL::Origin sourceOrigin, MTL::Size sourceSize, MTL::Texture* toTexture, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin)
{
	RenderPass.CopyFromTextureToTexture(Texture, sourceSlice, sourceLevel, sourceOrigin, sourceSize,
                                        toTexture, destinationSlice, destinationLevel, destinationOrigin);
}

void FMetalContext::CopyFromBufferToBuffer(FMetalBufferPtr SourceBuffer, NS::UInteger SourceOffset, FMetalBufferPtr DestinationBuffer, NS::UInteger DestinationOffset, NS::UInteger Size)
{
	RenderPass.CopyFromBufferToBuffer(SourceBuffer, SourceOffset, DestinationBuffer, DestinationOffset, Size);
}

bool FMetalContext::AsyncCopyFromBufferToTexture(FMetalBufferPtr Buffer, uint32 sourceOffset, uint32 sourceBytesPerRow, uint32 sourceBytesPerImage, MTL::Size sourceSize, MTL::Texture* toTexture, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin, MTL::BlitOption options)
{
	return RenderPass.AsyncCopyFromBufferToTexture(Buffer, sourceOffset, sourceBytesPerRow, sourceBytesPerImage, sourceSize,
                                                   toTexture, destinationSlice, destinationLevel, destinationOrigin, options);
}

bool FMetalContext::AsyncCopyFromTextureToTexture(MTL::Texture* Texture, uint32 sourceSlice, uint32 sourceLevel, MTL::Origin sourceOrigin, MTL::Size sourceSize, MTL::Texture* toTexture, uint32 destinationSlice, uint32 destinationLevel, MTL::Origin destinationOrigin)
{
	return RenderPass.AsyncCopyFromTextureToTexture(Texture, sourceSlice, sourceLevel, sourceOrigin, sourceSize,
                                                    toTexture, destinationSlice, destinationLevel, destinationOrigin);
}

bool FMetalContext::CanAsyncCopyToBuffer(FMetalBufferPtr DestinationBuffer)
{
	return RenderPass.CanAsyncCopyToBuffer(DestinationBuffer);
}

void FMetalContext::AsyncCopyFromBufferToBuffer(FMetalBufferPtr SourceBuffer, NS::UInteger SourceOffset, FMetalBufferPtr DestinationBuffer, NS::UInteger DestinationOffset, NS::UInteger Size)
{
	RenderPass.AsyncCopyFromBufferToBuffer(SourceBuffer, SourceOffset, DestinationBuffer, DestinationOffset, Size);
}

void FMetalContext::AsyncGenerateMipmapsForTexture(MTL::Texture* Texture)
{
	RenderPass.AsyncGenerateMipmapsForTexture(Texture);
}

void FMetalContext::SubmitAsyncCommands(MTL::HandlerFunction ScheduledHandler, MTL::HandlerFunction CompletionHandler, bool const bWait)
{
	RenderPass.AddAsyncCommandBufferHandlers(ScheduledHandler, CompletionHandler);
	if (bWait)
	{
		SubmitCommandsHint((uint32)(EMetalSubmitFlagsAsyncCommandBuffer|EMetalSubmitFlagsWaitOnCommandBuffer|EMetalSubmitFlagsBreakCommandBuffer));
	}
}

void FMetalContext::SynchronizeTexture(MTL::Texture* Texture, uint32 Slice, uint32 Level)
{
	RenderPass.SynchronizeTexture(Texture, Slice, Level);
}

void FMetalContext::SynchroniseResource(MTL::Resource* Resource)
{
	RenderPass.SynchroniseResource(Resource);
}

void FMetalContext::FillBuffer(MTL::Buffer* Buffer, NS::Range Range, uint8 Value)
{
    RenderPass.FillBuffer(Buffer, Range, Value);
}

void FMetalContext::Dispatch(uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
	RenderPass.Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

void FMetalContext::DispatchIndirect(FMetalRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset)
{
	RenderPass.DispatchIndirect(ArgumentBuffer, ArgumentOffset);
}

#if PLATFORM_SUPPORTS_MESH_SHADERS
void FMetalContext::DispatchMeshShader(uint32 PrimitiveType, uint32 ThreadGroupCountX, uint32 ThreadGroupCountY, uint32 ThreadGroupCountZ)
{
    // finalize any pending state
    if(!PrepareToDraw(PrimitiveType))
    {
        return;
    }
    
    RenderPass.DispatchMeshShader(PrimitiveType, ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

void FMetalContext::DispatchIndirectMeshShader(uint32 PrimitiveType, FMetalRHIBuffer* ArgumentBuffer, uint32 ArgumentOffset)
{
    // finalize any pending state
    if(!PrepareToDraw(PrimitiveType))
    {
        return;
    }
    
    RenderPass.DispatchIndirectMeshShader(PrimitiveType, ArgumentBuffer, ArgumentOffset);
}
#endif

void FMetalContext::StartTiming(class FMetalEventNode* EventNode)
{
    FMetalCommandBufferCompletionHandler Handler;
	
	bool const bHasCurrentCommandBuffer = GetCurrentCommandBuffer();
	
	if(EventNode)
	{
		Handler = EventNode->Start();
		
		if (bHasCurrentCommandBuffer)
		{
			RenderPass.AddCompletionHandler(Handler);
		}
	}
	
	SubmitCommandsHint(EMetalSubmitFlagsCreateCommandBuffer);
	
	if (Handler.IsBound() && !bHasCurrentCommandBuffer)
	{
		GetCurrentCommandBuffer()->GetMTLCmdBuffer()->addScheduledHandler(
                        MTL::HandlerFunction([Handler](MTL::CommandBuffer* CommandBuffer)
                        {
                            Handler.Execute(CommandBuffer);
                        }));
	}
}

void FMetalContext::EndTiming(class FMetalEventNode* EventNode)
{
	bool const bWait = EventNode->Wait();
    FMetalCommandBufferCompletionHandler Handler = EventNode->Stop();
	RenderPass.AddCompletionHandler(Handler);
	
	if (!bWait)
	{
		SubmitCommandsHint(EMetalSubmitFlagsCreateCommandBuffer);
	}
	else
	{
		SubmitCommandBufferAndWait();
	}
}
