// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetalLLM.h"
#include "MetalProfiler.h"
#include "RenderUtils.h"
#include "HAL/LowLevelMemStats.h"

#include <objc/runtime.h>

#if ENABLE_LOW_LEVEL_MEM_TRACKER

struct FLLMTagInfoMetal
{
	const TCHAR* Name;
	FName StatName;				// shows in the LLMFULL stat group
	FName SummaryStatName;		// shows in the LLM summary stat group
};

DECLARE_LLM_MEMORY_STAT(TEXT("Metal Buffers"), STAT_MetalBuffersLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("Metal Textures"), STAT_MetalTexturesLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("Metal Heaps"), STAT_MetalHeapsLLM, STATGROUP_LLMPlatform);
DECLARE_LLM_MEMORY_STAT(TEXT("Metal RenderTargets"), STAT_MetalRenderTargetsLLM, STATGROUP_LLMPlatform);

// *** order must match ELLMTagMetal enum ***
const FLLMTagInfoMetal ELLMTagNamesMetal[] =
{
	// csv name									// stat name										// summary stat name						// enum value
	{ TEXT("Metal Buffers"),		GET_STATFNAME(STAT_MetalBuffersLLM),		GET_STATFNAME(STAT_EngineSummaryLLM) },		// ELLMTagMetal::Buffers
	{ TEXT("Metal Textures"),		GET_STATFNAME(STAT_MetalTexturesLLM),		GET_STATFNAME(STAT_EngineSummaryLLM) },		// ELLMTagMetal::Textures
	{ TEXT("Metal Heaps"),			GET_STATFNAME(STAT_MetalHeapsLLM),			GET_STATFNAME(STAT_EngineSummaryLLM) },		// ELLMTagMetal::Heaps
	{ TEXT("Metal Render Targets"),	GET_STATFNAME(STAT_MetalRenderTargetsLLM),	GET_STATFNAME(STAT_EngineSummaryLLM) },		// ELLMTagMetal::RenderTargets
};

/*
 * Register Metal tags with LLM
 */
void MetalLLM::Initialise()
{
	int32 TagCount = sizeof(ELLMTagNamesMetal) / sizeof(FLLMTagInfoMetal);

	for (int32 Index = 0; Index < TagCount; ++Index)
	{
		int32 Tag = (int32)ELLMTagApple::AppleMetalTagsStart + Index;
		const FLLMTagInfoMetal& TagInfo = ELLMTagNamesMetal[Index];

		FLowLevelMemTracker::Get().RegisterPlatformTag(Tag, TagInfo.Name, TagInfo.StatName, TagInfo.SummaryStatName);
	}
}

#endif		// #if ENABLE_LOW_LEVEL_MEM_TRACKER

@implementation FMetalDeallocHandler

APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDeallocHandler)

-(instancetype)initWithBlock:(dispatch_block_t)InBlock
{
	id Self = [super init];
	if (Self)
	{
		self->Block = Block_copy(InBlock);
	}
	return Self;
}
-(void)dealloc
{
	self->Block();
	Block_release(self->Block);
	[super dealloc];
}
@end

static mtlpp::PixelFormat FromSRGBFormat(mtlpp::PixelFormat Format)
{
	mtlpp::PixelFormat MTLFormat = Format;
	
	switch (Format)
	{
		case mtlpp::PixelFormat::RGBA8Unorm_sRGB:
			MTLFormat = mtlpp::PixelFormat::RGBA8Unorm;
			break;
		case mtlpp::PixelFormat::BGRA8Unorm_sRGB:
			MTLFormat = mtlpp::PixelFormat::BGRA8Unorm;
			break;
#if PLATFORM_MAC
		case mtlpp::PixelFormat::BC1_RGBA_sRGB:
			MTLFormat = mtlpp::PixelFormat::BC1_RGBA;
			break;
		case mtlpp::PixelFormat::BC2_RGBA_sRGB:
			MTLFormat = mtlpp::PixelFormat::BC2_RGBA;
			break;
		case mtlpp::PixelFormat::BC3_RGBA_sRGB:
			MTLFormat = mtlpp::PixelFormat::BC3_RGBA;
			break;
		case mtlpp::PixelFormat::BC7_RGBAUnorm_sRGB:
			MTLFormat = mtlpp::PixelFormat::BC7_RGBAUnorm;
			break;
#endif //PLATFORM_MAC
#if PLATFORM_IOS
		case mtlpp::PixelFormat::R8Unorm_sRGB:
			MTLFormat = mtlpp::PixelFormat::R8Unorm;
			break;
		case mtlpp::PixelFormat::PVRTC_RGBA_2BPP_sRGB:
			MTLFormat = mtlpp::PixelFormat::PVRTC_RGBA_2BPP;
			break;
		case mtlpp::PixelFormat::PVRTC_RGBA_4BPP_sRGB:
			MTLFormat = mtlpp::PixelFormat::PVRTC_RGBA_4BPP;
			break;
		case mtlpp::PixelFormat::ASTC_4x4_sRGB:
			MTLFormat = mtlpp::PixelFormat::ASTC_4x4_LDR;
			break;
		case mtlpp::PixelFormat::ASTC_6x6_sRGB:
			MTLFormat = mtlpp::PixelFormat::ASTC_6x6_LDR;
			break;
		case mtlpp::PixelFormat::ASTC_8x8_sRGB:
			MTLFormat = mtlpp::PixelFormat::ASTC_8x8_LDR;
			break;
		case mtlpp::PixelFormat::ASTC_10x10_sRGB:
			MTLFormat = mtlpp::PixelFormat::ASTC_10x10_LDR;
			break;
		case mtlpp::PixelFormat::ASTC_12x12_sRGB:
			MTLFormat = mtlpp::PixelFormat::ASTC_12x12_LDR;
			break;
#endif //PLATFORM_IOS
		default:
			break;
	}
	
	return MTLFormat;
}

static EPixelFormat MetalToRHIPixelFormat(mtlpp::PixelFormat Format)
{
	Format = FromSRGBFormat(Format);
	for (uint32 i = 0; i < PF_MAX; i++)
	{
		if((mtlpp::PixelFormat)GPixelFormats[i].PlatformFormat == Format)
		{
			return (EPixelFormat)i;
		}
	}
	check(false);
	return PF_MAX;
}

void MetalLLM::LogAllocTexture(mtlpp::Device& Device, mtlpp::TextureDescriptor const& Desc, mtlpp::Texture const& Texture)
{
	mtlpp::SizeAndAlign SizeAlign;
	if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesGPUCaptureManager))
	{
		SizeAlign = Device.HeapTextureSizeAndAlign(Desc);
	}
	
	void* Ptr = (void*)Texture.GetPtr();
	uint64 Size = SizeAlign.Size;
	
#if PLATFORM_IOS
	bool bMemoryless = (Texture.GetStorageMode() == mtlpp::StorageMode::Memoryless);
	if (!bMemoryless)
#endif
	{
		INC_MEMORY_STAT_BY(STAT_MetalTextureMemory, Size);
	}
	INC_DWORD_STAT(STAT_MetalTextureCount);
	
	LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, Ptr, Size, ELLMTag::Untagged, ELLMAllocType::System));
	// Assign a dealloc handler to untrack the memory - but don't track the dispatch block!
	{
		LLM_SCOPED_PAUSE_TRACKING(ELLMAllocType::System);
		
		if (Desc.GetUsage() & mtlpp::TextureUsage::RenderTarget)
		{
			objc_setAssociatedObject(Texture.GetPtr(), (void*)&MetalLLM::LogAllocTexture,
			[[[FMetalDeallocHandler alloc] initWithBlock:^{
				LLM_PLATFORM_SCOPE_METAL(ELLMTagMetal::RenderTargets);
				
				LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, Ptr, ELLMAllocType::System));
				
#if PLATFORM_IOS
				if (!bMemoryless)
#endif
				{
					DEC_MEMORY_STAT_BY(STAT_MetalTextureMemory, Size);
				}
				DEC_DWORD_STAT(STAT_MetalTextureCount);
			}] autorelease],
			OBJC_ASSOCIATION_RETAIN);
		}
		else
		{
			objc_setAssociatedObject(Texture.GetPtr(), (void*)&MetalLLM::LogAllocTexture,
			[[[FMetalDeallocHandler alloc] initWithBlock:^{
				LLM_PLATFORM_SCOPE_METAL(ELLMTagMetal::Textures);
			
				LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, Ptr, ELLMAllocType::System));
			
#if PLATFORM_IOS
				if (!bMemoryless)
#endif
				{
					DEC_MEMORY_STAT_BY(STAT_MetalTextureMemory, Size);
				}
				DEC_DWORD_STAT(STAT_MetalTextureCount);
			}] autorelease],
			OBJC_ASSOCIATION_RETAIN);
		}
	}
}

void MetalLLM::LogAllocBuffer(mtlpp::Device& Device, mtlpp::Buffer const& Buffer)
{
	void* Ptr = (void*)Buffer.GetPtr();
	uint64 Size = Buffer.GetLength();
	
	INC_MEMORY_STAT_BY(STAT_MetalBufferMemory, Size);
	INC_DWORD_STAT(STAT_MetalBufferCount);
	
	LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, Ptr, Size, ELLMTag::Untagged, ELLMAllocType::System));
	// Assign a dealloc handler to untrack the memory - but don't track the dispatch block!
	{
		LLM_SCOPED_PAUSE_TRACKING(ELLMAllocType::System);
		
		objc_setAssociatedObject(Buffer.GetPtr(), (void*)&MetalLLM::LogAllocBuffer,
		[[[FMetalDeallocHandler alloc] initWithBlock:^{
			LLM_PLATFORM_SCOPE_METAL(ELLMTagMetal::Buffers);
			
			LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, Ptr, ELLMAllocType::System));
			
			DEC_MEMORY_STAT_BY(STAT_MetalBufferMemory, Size);
			DEC_DWORD_STAT(STAT_MetalBufferCount);
		}] autorelease],
		OBJC_ASSOCIATION_RETAIN);
	}
}

void MetalLLM::LogAllocHeap(mtlpp::Device& Device, mtlpp::Heap const& Heap)
{
	void* Ptr = (void*)Heap.GetPtr();
	uint64 Size = Heap.GetSize();
	
	INC_MEMORY_STAT_BY(STAT_MetalHeapMemory, Size);
	INC_DWORD_STAT(STAT_MetalHeapCount);
	
	LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, Ptr, Size, ELLMTag::Untagged, ELLMAllocType::System));
	// Assign a dealloc handler to untrack the memory - but don't track the dispatch block!
	{
		LLM_SCOPED_PAUSE_TRACKING(ELLMAllocType::System);
		
		objc_setAssociatedObject(Heap.GetPtr(), (void*)&MetalLLM::LogAllocHeap,
								 [[[FMetalDeallocHandler alloc] initWithBlock:^{
			LLM_SCOPE_METAL(ELLMTagMetal::Heaps);
			LLM_PLATFORM_SCOPE_METAL(ELLMTagMetal::Heaps);
			
			LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, Ptr, ELLMAllocType::System));
			
			DEC_MEMORY_STAT_BY(STAT_MetalHeapMemory, Size);
			DEC_DWORD_STAT(STAT_MetalHeapCount);
		}] autorelease],
		OBJC_ASSOCIATION_RETAIN);
	}
}

void MetalLLM::LogAliasTexture(mtlpp::Texture const& Texture)
{
	objc_setAssociatedObject(Texture.GetPtr(), (void*)&MetalLLM::LogAllocTexture, nil, OBJC_ASSOCIATION_RETAIN);
}

void MetalLLM::LogAliasBuffer(mtlpp::Buffer const& Buffer)
{
	objc_setAssociatedObject(Buffer.GetPtr(), (void*)&MetalLLM::LogAllocBuffer, nil, OBJC_ASSOCIATION_RETAIN);
}


