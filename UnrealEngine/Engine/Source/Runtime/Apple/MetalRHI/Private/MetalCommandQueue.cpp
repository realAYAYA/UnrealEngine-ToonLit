// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MetalCommandQueue.cpp: Metal command queue wrapper..
=============================================================================*/

#include "MetalRHIPrivate.h"

#include "MetalCommandQueue.h"
#include "MetalCommandBuffer.h"
#include "MetalCommandList.h"
#include "MetalProfiler.h"
#include "Misc/ConfigCacheIni.h"

#if !UE_BUILD_SHIPPING
#import <Metal/Metal.h>
#endif

#pragma mark - Private C++ Statics -
NS::UInteger FMetalCommandQueue::PermittedOptions = 0;
uint64 FMetalCommandQueue::Features = 0;
extern MTL::VertexFormat GMetalFColorVertexFormat;

bool GMetalCommandBufferDebuggingEnabled = 0;

static int32 GForceNoMetalHeap = 1;
static FAutoConsoleVariableRef CVarMetalForceNoHeap(
	TEXT("rhi.Metal.ForceNoHeap"),
	GForceNoMetalHeap,
	TEXT("[IOS] When enabled, act as if -nometalheap was on the commandline\n")
	TEXT("(On by default (1))"));

static int32 GForceNoMetalFence = 1;
static FAutoConsoleVariableRef CVarMetalForceNoFence(
	TEXT("rhi.Metal.ForceNoFence"),
	GForceNoMetalFence,
	TEXT("[IOS] When enabled, act as if -nometalfence was on the commandline\n")
	TEXT("(On by default (1))"));



#pragma mark - Public C++ Boilerplate -

FMetalCommandQueue::FMetalCommandQueue(MTL::Device* InDevice, uint32 const MaxNumCommandBuffers /* = 0 */)
	: Device(InDevice)
	, RuntimeDebuggingLevel(EMetalDebugLevelOff)
{
	int32 IndirectArgumentTier = 0;
    int32 MetalShaderVersion = 0;
#if PLATFORM_MAC
	const TCHAR* const Settings = TEXT("/Script/MacTargetPlatform.MacTargetSettings");
#else
	const TCHAR* const Settings = TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings");
#endif
    GConfig->GetInt(Settings, TEXT("MetalLanguageVersion"), MetalShaderVersion, GEngineIni);
	
    if(!GConfig->GetInt(Settings, TEXT("IndirectArgumentTier"), IndirectArgumentTier, GEngineIni))
	{
		IndirectArgumentTier = 0;
	}
	ValidateVersion(MetalShaderVersion);

	if(MaxNumCommandBuffers == 0)
	{
		CommandQueue = Device->newCommandQueue();
	}
	else
	{
		CommandQueue = Device->newCommandQueue(MaxNumCommandBuffers);
	}
	check(CommandQueue);
#if PLATFORM_IOS
	NS::OperatingSystemVersion Vers = NS::ProcessInfo::processInfo()->operatingSystemVersion();
	Features = EMetalFeaturesSetBufferOffset | EMetalFeaturesSetBytes;

#if PLATFORM_TVOS
	Features &= ~(EMetalFeaturesSetBytes);
		
    if(Device->supportsFeatureSet(MTL::FeatureSet_tvOS_GPUFamily2_v1))
	{
		Features |= EMetalFeaturesCountingQueries | EMetalFeaturesBaseVertexInstance | EMetalFeaturesIndirectBuffer | EMetalFeaturesMSAADepthResolve | EMetalFeaturesMSAAStoreAndResolve;
	}

	Features |= EMetalFeaturesPrivateBufferSubAllocation;

	Features |= EMetalFeaturesGPUCaptureManager | EMetalFeaturesBufferSubAllocation | EMetalFeaturesParallelRenderEncoders | EMetalFeaturesPipelineBufferMutability;

	GMetalFColorVertexFormat = MTL::VertexFormatUChar4Normalized_BGRA;

	Features |= EMetalFeaturesMaxThreadsPerThreadgroup;

	if (FParse::Param(FCommandLine::Get(), TEXT("metalfence")))
	{
		Features |= EMetalFeaturesFences;
	}
					
    if (FParse::Param(FCommandLine::Get(),TEXT("metalheap")))
	{
		Features |= EMetalFeaturesHeaps;
	}

	Features |= EMetalFeaturesTextureBuffers;
#else
	if (Device->supportsFeatureSet(MTL::FeatureSet_iOS_GPUFamily3_v1))
	{
		Features |= EMetalFeaturesCountingQueries | EMetalFeaturesBaseVertexInstance | EMetalFeaturesIndirectBuffer | EMetalFeaturesMSAADepthResolve;
	}
		
    if(Device->supportsFeatureSet(MTL::FeatureSet_iOS_GPUFamily3_v2) || Device->supportsFeatureSet(MTL::FeatureSet_iOS_GPUFamily2_v3) || Device->supportsFeatureSet(MTL::FeatureSet_iOS_GPUFamily1_v3))
	{
        if (FParse::Param(FCommandLine::Get(),TEXT("metalfence")))
		{
			Features |= EMetalFeaturesFences;
		}
			
		if (FParse::Param(FCommandLine::Get(),TEXT("metalheap")))
		{
			Features |= EMetalFeaturesHeaps;
		}
	}
		
    if(Device->supportsFeatureSet(MTL::FeatureSet_iOS_GPUFamily3_v2))
	{
		Features |= EMetalFeaturesMSAAStoreAndResolve;
	}

	// Turning the below option on will allocate more buffer memory which isn't generally desirable on iOS
	// Features |= EMetalFeaturesEfficientBufferBlits;
			
	// These options are fine however as thye just change how we allocate small buffers
	Features |= EMetalFeaturesBufferSubAllocation;
	Features |= EMetalFeaturesPrivateBufferSubAllocation;

	GMetalFColorVertexFormat = MTL::VertexFormatUChar4Normalized_BGRA;

	Features |= EMetalFeaturesPresentMinDuration | EMetalFeaturesGPUCaptureManager | EMetalFeaturesBufferSubAllocation | EMetalFeaturesParallelRenderEncoders | EMetalFeaturesPipelineBufferMutability;

	Features |= EMetalFeaturesMaxThreadsPerThreadgroup;
	if (!GForceNoMetalFence && !FParse::Param(FCommandLine::Get(), TEXT("nometalfence")))
	{
		Features |= EMetalFeaturesFences;
	}
                    
    if (!GForceNoMetalHeap && !FParse::Param(FCommandLine::Get(),TEXT("nometalheap")))
	{
		Features |= EMetalFeaturesHeaps;
	}

	Features |= EMetalFeaturesTextureBuffers;

	if (Device->supportsFeatureSet(MTL::FeatureSet_iOS_GPUFamily4_v1))
	{
		Features |= EMetalFeaturesTileShaders;
                        
		// The below implies tile shaders which are necessary to order the draw calls and generate a buffer that shows what PSOs/draws ran on each tile.
		IConsoleVariable* GPUCrashDebuggingCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUCrashDebugging"));
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
		GMetalCommandBufferDebuggingEnabled = (GPUCrashDebuggingCVar && GPUCrashDebuggingCVar->GetInt() != 0) || FParse::Param(FCommandLine::Get(), TEXT("metalgpudebug"));
#else
        GMetalCommandBufferDebuggingEnabled = true;
#endif
	}
                    
	if (Device->supportsFeatureSet(MTL::FeatureSet_iOS_GPUFamily5_v1))
	{
		Features |= EMetalFeaturesLayeredRendering;
	}
#endif
#else // Assume that Mac & other platforms all support these from the start. They can diverge later.
	Features = EMetalFeaturesCountingQueries | EMetalFeaturesBaseVertexInstance | EMetalFeaturesIndirectBuffer |
                EMetalFeaturesLayeredRendering | EMetalFeaturesCubemapArrays | EMetalFeaturesSetBufferOffset;

    FString DeviceName(Device->name()->cString(NS::UTF8StringEncoding));
    
	if (Device->supportsFeatureSet(MTL::FeatureSet_macOS_GPUFamily1_v2))
	{
		Features |= EMetalFeaturesMSAADepthResolve | EMetalFeaturesMSAAStoreAndResolve;
        
		// Assume that set*Bytes only works on macOS Sierra and above as no-one has tested it anywhere else.
		Features |= EMetalFeaturesSetBytes;
		
		// On earlier OS versions Intel Broadwell couldn't suballocate properly
		if (!(DeviceName.Contains(TEXT("Intel")) && (DeviceName.Contains(TEXT("5300")) || DeviceName.Contains(TEXT("6000")) || DeviceName.Contains(TEXT("6100")))))
		{
			// Using Private Memory & BlitEncoders for Vertex & Index data should be *much* faster.
			Features |= EMetalFeaturesEfficientBufferBlits;
        	
			Features |= EMetalFeaturesBufferSubAllocation;
					
			// On earlier OS versions Vega didn't like non-zero blit offsets
	        if (!DeviceName.Contains(TEXT("Vega")))
			{
				Features |= EMetalFeaturesPrivateBufferSubAllocation;
			}
		}
		
		GMetalFColorVertexFormat = MTL::VertexFormatUChar4Normalized_BGRA;
		if (!FParse::Param(FCommandLine::Get(), TEXT("nometalparallelencoder")))
		{
			Features |= EMetalFeaturesParallelRenderEncoders;
		}
		Features |= EMetalFeaturesTextureBuffers;
		if (IndirectArgumentTier >= 1)
		{
			Features |= EMetalFeaturesIABs;
				
			if (IndirectArgumentTier >= 2)
			{
				Features |= EMetalFeaturesTier2IABs;
			}
		}
            
		IConsoleVariable* GPUCrashDebuggingCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUCrashDebugging"));
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
        GMetalCommandBufferDebuggingEnabled = (GPUCrashDebuggingCVar && GPUCrashDebuggingCVar->GetInt() != 0) || FParse::Param(FCommandLine::Get(), TEXT("metalgpudebug"));
#else
        GMetalCommandBufferDebuggingEnabled = true;
#endif
		
		// The editor spawns so many viewports and preview icons that we can run out of hardware fences!
		// Need to figure out a way to safely flush the rendering and reuse the fences when that happens.
#if WITH_EDITORONLY_DATA
		if (!GIsEditor)
#endif
		{
            if (FParse::Param(FCommandLine::Get(),TEXT("metalfence")))
			{
				Features |= EMetalFeaturesFences;
			}
		}
	}
    
	// Temporarily only support heaps for devices with unified memory
	// Disable this by default code while we work on metal heaps
	if (!DeviceName.Contains(TEXT("Intel")) &&
          Device->hasUnifiedMemory() &&
          FParse::Param(FCommandLine::Get(),TEXT("metalheap")))
	{
		Features |= EMetalFeaturesHeaps;
	}
	
    if(Device->supportsFeatureSet(MTL::FeatureSet_macOS_GPUFamily1_v3))
	{
		Features |= EMetalFeaturesMultipleViewports | EMetalFeaturesPipelineBufferMutability | EMetalFeaturesGPUCaptureManager;
		
		if (FParse::Param(FCommandLine::Get(),TEXT("metalfence")))
		{
			Features |= EMetalFeaturesFences;
		}
		
		if (FParse::Param(FCommandLine::Get(),TEXT("metaliabs")))
		{
			Features |= EMetalFeaturesIABs;
		}
	}
#endif
	
#if !UE_BUILD_SHIPPING
	Class MTLDebugDevice = NSClassFromString(@"MTLDebugDevice");
    id<MTLDevice> ObjCDevice = (__bridge id<MTLDevice>)InDevice;
	if ([ObjCDevice isKindOfClass:MTLDebugDevice])
	{
		Features |= EMetalFeaturesValidation;
	}
#endif

	static const auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shaders.Optimize"));
	if (CVar->GetInt() == 0 || FParse::Param(FCommandLine::Get(),TEXT("metalshaderdebug")))
	{
		Features |= EMetalFeaturesGPUTrace;
	}

	PermittedOptions = 0;
	PermittedOptions |= MTL::ResourceCPUCacheModeDefaultCache;
	PermittedOptions |= MTL::ResourceCPUCacheModeWriteCombined;
	{
	PermittedOptions |= MTL::ResourceStorageModeShared;
	PermittedOptions |= MTL::ResourceStorageModePrivate;
#if PLATFORM_MAC
	PermittedOptions |= MTL::ResourceStorageModeManaged;
#else
	PermittedOptions |= MTL::ResourceStorageModeMemoryless;
#endif
	// You can't use HazardUntracked under the validation layer due to bugs in the layer when trying to create linear-textures/texture-buffers
	if ((Features & EMetalFeaturesFences) && !(Features & EMetalFeaturesValidation))
	{
		PermittedOptions |= MTL::ResourceHazardTrackingModeUntracked;
	}
	}
}

FMetalCommandQueue::~FMetalCommandQueue(void)
{
	// void
}
	
#pragma mark - Public Command Buffer Mutators -

FMetalCommandBuffer* FMetalCommandQueue::CreateCommandBuffer(void)
{
#if PLATFORM_MAC
	static bool bUnretainedRefs = FParse::Param(FCommandLine::Get(),TEXT("metalunretained"))
	|| (!FParse::Param(FCommandLine::Get(),TEXT("metalretainrefs"))
			&& (Device->name()->rangeOfString(NS::String::string("Intel", NS::UTF8StringEncoding), NSCaseInsensitiveSearch).location == NSNotFound));
#else
	static bool bUnretainedRefs = !FParse::Param(FCommandLine::Get(),TEXT("metalretainrefs"));
#endif
	
    MTL::CommandBufferDescriptor* CmdBufferDesc = MTL::CommandBufferDescriptor::alloc()->init();
    check(CmdBufferDesc);
    
    CmdBufferDesc->setRetainedReferences(!bUnretainedRefs);
    CmdBufferDesc->setErrorOptions(GMetalCommandBufferDebuggingEnabled ? MTL::CommandBufferErrorOptionEncoderExecutionStatus : MTL::CommandBufferErrorOptionNone);
    
    MTL::CommandBuffer* CmdBuffer = CommandQueue->commandBuffer(CmdBufferDesc);
    
    CmdBufferDesc->release();
                                                           
    FMetalCommandBuffer* CommandBuffer = new FMetalCommandBuffer(CmdBuffer);
	CommandBufferFences.Push(CommandBuffer->GetCompletionFence());
    
	INC_DWORD_STAT(STAT_MetalCommandBufferCreatedPerFrame);
	return CommandBuffer;
}

void FMetalCommandQueue::CommitCommandBuffer(FMetalCommandBuffer* CommandBuffer)
{
	check(CommandBuffer);
	INC_DWORD_STAT(STAT_MetalCommandBufferCommittedPerFrame);
	
	//MTLPP_VALIDATE(MTL::CommandBuffer, CommandBuffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Commit());
    CommandBuffer->GetMTLCmdBuffer()->commit();
    
	// Wait for completion when debugging command-buffers.
	if (RuntimeDebuggingLevel >= EMetalDebugLevelWaitForComplete)
	{
		CommandBuffer->GetMTLCmdBuffer()->waitUntilCompleted();
	}
    
    SafeReleaseFunction([CommandBuffer]() {
        delete CommandBuffer;
    });
}

FMetalFence* FMetalCommandQueue::CreateFence(NS::String* Label) const
{
	if ((Features & EMetalFeaturesFences) != 0)
	{
		FMetalFence* InternalFence = FMetalFencePool::Get().AllocateFence();
		{
			MTL::Fence* InnerFence = InternalFence->Get();
			NS::String* String = nullptr;
			if (GetEmitDrawEvents())
			{
                NS::String* FenceString = FStringToNSString(FString::Printf(TEXT("%p"), InnerFence));
                String = FenceString->stringByAppendingString(Label);
			}

			if(InnerFence && String)
            {
                InnerFence->setLabel(String);
            }
		}
		return InternalFence;
	}
	else
	{
		return nullptr;
	}
}

void FMetalCommandQueue::GetCommittedCommandBufferFences(TArray<TSharedPtr<FMetalCommandBufferFence, ESPMode::ThreadSafe>>& Fences)
{
    Fences = MoveTemp(CommandBufferFences);
}

#pragma mark - Public Command Queue Accessors -
	
MTL::Device* FMetalCommandQueue::GetDevice(void)
{
	return Device;
}

MTL::ResourceOptions FMetalCommandQueue::GetCompatibleResourceOptions(MTL::ResourceOptions Options)
{
	NS::UInteger NewOptions = (Options & PermittedOptions);
#if PLATFORM_IOS // Swizzle Managed to Shared for iOS - we can do this as they are equivalent, unlike Shared -> Managed on Mac.
	if ((Options & (1 /*MTL::StorageModeManaged*/ << MTL::ResourceStorageModeShift)))
	{
#if WITH_IOS_SIMULATOR
		NewOptions |= MTL::ResourceStorageModePrivate;
#else
		NewOptions |= MTL::ResourceStorageModeShared;
#endif
	}
#endif
	return (MTL::ResourceOptions)NewOptions;
}

#pragma mark - Public Debug Support -

void FMetalCommandQueue::InsertDebugCaptureBoundary(void)
{
	CommandQueue->insertDebugCaptureBoundary();
}

void FMetalCommandQueue::SetRuntimeDebuggingLevel(int32 const Level)
{
	RuntimeDebuggingLevel = Level;
}

int32 FMetalCommandQueue::GetRuntimeDebuggingLevel(void) const
{
	return RuntimeDebuggingLevel;
}
