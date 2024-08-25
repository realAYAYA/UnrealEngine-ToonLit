// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OpenGLTexture.cpp: OpenGL texture RHI implementation.
=============================================================================*/

#include "CoreMinimal.h"
#include "Containers/ResourceArray.h"
#include "Stats/Stats.h"
#include "RHI.h"
#include "RenderUtils.h"
#include "OpenGLDrv.h"
#include "OpenGLDrvPrivate.h"
#include "HAL/LowLevelMemTracker.h"
#include "Engine/Texture.h"
#include "RHICoreStats.h"

#if PLATFORM_ANDROID
#include "ThirdParty/Android/detex/AndroidETC.h"
#endif //PLATFORM_ANDROID

static TAutoConsoleVariable<int32> CVarDeferTextureCreation(
	TEXT("r.OpenGL.DeferTextureCreation"),
	0,
	TEXT("0: OpenGL textures are sent to the driver to be created immediately. (default)\n")
	TEXT("1: Where possible OpenGL textures are stored in system memory and created only when required for rendering.\n")
	TEXT("   This can avoid memory overhead seen in some GL drivers."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDeferTextureCreationExcludeMask(
	TEXT("r.OpenGL.DeferTextureCreationExcludeFlags"),
	static_cast<int32>(~(TexCreate_ShaderResource | TexCreate_SRGB | TexCreate_Streamable | TexCreate_OfflineProcessed)),
	TEXT("Deferred texture creation exclusion mask, any texture requested with flags in this mask will be excluded from deferred creation."),
	ECVF_RenderThreadSafe);

static int32 GOGLDeferTextureCreationKeepLowerMipCount = -1;
static FAutoConsoleVariableRef CVarDeferTextureCreationKeepLowerMipCount(
	TEXT("r.OpenGL.DeferTextureCreationKeepLowerMipCount"),
	GOGLDeferTextureCreationKeepLowerMipCount,
	TEXT("Maximum number of texture mips to retain in CPU memory after a deferred texture has been sent to the driver for GPU memory creation.\n")
	TEXT("-1: to match the number of mips kept resident by the texture streamer (default).\n")
	TEXT(" 0: to disable texture eviction and discard CPU mips after sending them to the driver.\n")
	TEXT(" 16: keep all mips around.\n"),
	ECVF_RenderThreadSafe);

static int32 GOGLTextureEvictFramesToLive = 500;
static FAutoConsoleVariableRef CVarTextureEvictionFrameCount(
	TEXT("r.OpenGL.TextureEvictionFrameCount"),
	GOGLTextureEvictFramesToLive,
	TEXT("The number of frames since a texture was last referenced before it will considered for eviction.\n")
	TEXT("Textures can only be evicted after creation if all their mips are resident, ie its mip count <= r.OpenGL.DeferTextureCreationKeepLowerMipCount."),
	ECVF_RenderThreadSafe
);

int32 GOGLTexturesToEvictPerFrame = 10;
static FAutoConsoleVariableRef CVarTexturesToEvictPerFrame(
	TEXT("r.OpenGL.TextureEvictsPerFrame"),
	GOGLTexturesToEvictPerFrame,
	TEXT("The maximum number of evictable textures to evict per frame, limited to avoid potential driver CPU spikes.\n")
	TEXT("Textures can only be evicted after creation if all their mips are resident, ie its mip count <= r.OpenGL.DeferTextureCreationKeepLowerMipCount."),
	ECVF_RenderThreadSafe
);

static int32 GOGLTextureEvictLogging = 0;
static FAutoConsoleVariableRef CVarTextureEvictionLogging(
	TEXT("r.OpenGL.TextureEvictionLogging"),
	GOGLTextureEvictLogging,
	TEXT("Enables debug logging for texture eviction."),
	ECVF_RenderThreadSafe
);

static int32 GOGLTextureMinLRUCapacity = 0;
static FAutoConsoleVariableRef CVarTextureEvictionMinLRUCapacity(
	TEXT("r.OpenGL.TextureEvictionMinLRUCapacity"),
	GOGLTextureMinLRUCapacity,
	TEXT("Keep a minimum number of textures resident in GL When using the texture LRU\n")
	TEXT("This can reduce LRU restore times when resuming from static scenes.\n")
	TEXT("0: (default)")
	,
	ECVF_RenderThreadSafe
);

/*-----------------------------------------------------------------------------
	Texture allocator support.
-----------------------------------------------------------------------------*/

/** Caching it here, to avoid getting it every time we create a texture. 0 is no multisampling. */
GLint GMaxOpenGLColorSamples = 0;
GLint GMaxOpenGLDepthSamples = 0;
GLint GMaxOpenGLIntegerSamples = 0;

// in bytes, never change after RHI, needed to scale game features
int64 GOpenGLDedicatedVideoMemory = 0;
// In bytes. Never changed after RHI init. Our estimate of the amount of memory that we can use for graphics resources in total.
int64 GOpenGLTotalGraphicsMemory = 0;

void FOpenGLTexture::UpdateTextureStats(FOpenGLTexture* Texture, bool bAllocating)
{
	const FRHITextureDesc& Desc = Texture->GetDesc();

	const uint64 TextureSize = Texture->MemorySize;

	const bool bOnlyStreamableTexturesInTexturePool = false;
	UE::RHICore::UpdateGlobalTextureStats(Desc, TextureSize, bOnlyStreamableTexturesInTexturePool, bAllocating);

	const int64 TextureSizeDelta = bAllocating ? int64(TextureSize) : -int64(TextureSize);

#if ENABLE_LOW_LEVEL_MEM_TRACKER
	const ELLMTag TextureTag = EnumHasAnyFlags(Desc.Flags, ETextureCreateFlags::RenderTargetable | ETextureCreateFlags::ResolveTargetable | ETextureCreateFlags::DepthStencilTargetable)
		? ELLMTag::RenderTargets
		: ELLMTag::Textures;

	LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT(ELLMTag::GraphicsPlatform, TextureSizeDelta, ELLMTracker::Platform, ELLMAllocType::None);
	LLM_SCOPED_PAUSE_TRACKING_WITH_ENUM_AND_AMOUNT(TextureTag               , TextureSizeDelta, ELLMTracker::Default , ELLMAllocType::None);
#endif // ENABLE_LOW_LEVEL_MEM_TRACKER
}

FDynamicRHI::FRHICalcTextureSizeResult FOpenGLDynamicRHI::RHICalcTexturePlatformSize(FRHITextureDesc const& Desc, uint32 FirstMipIndex)
{
	FDynamicRHI::FRHICalcTextureSizeResult Result;
	Result.Size = Desc.CalcMemorySizeEstimate(FirstMipIndex);
	Result.Align = 1;
	return Result;
}

/**
 * Retrieves texture memory stats. Unsupported with this allocator.
 *
 * @return false, indicating that out variables were left unchanged.
 */
void FOpenGLDynamicRHI::RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats)
{
	UE::RHICore::FillBaselineTextureMemoryStats(OutStats);

	OutStats.DedicatedVideoMemory = GOpenGLDedicatedVideoMemory;
	OutStats.TotalGraphicsMemory = GOpenGLTotalGraphicsMemory ? GOpenGLTotalGraphicsMemory : -1;

	OutStats.LargestContiguousAllocation = OutStats.StreamingMemorySize;
}


/**
 * Fills a texture with to visualize the texture pool memory.
 *
 * @param	TextureData		Start address
 * @param	SizeX			Number of pixels along X
 * @param	SizeY			Number of pixels along Y
 * @param	Pitch			Number of bytes between each row
 * @param	PixelSize		Number of bytes each pixel represents
 *
 * @return true if successful, false otherwise
 */
bool FOpenGLDynamicRHI::RHIGetTextureMemoryVisualizeData( FColor* /*TextureData*/, int32 /*SizeX*/, int32 /*SizeY*/, int32 /*Pitch*/, int32 /*PixelSize*/ )
{
	return false;
}

FOpenGLTextureDesc::FOpenGLTextureDesc(FRHITextureDesc const& InDesc)
	: bCubemap           (InDesc.IsTextureCube())
	, bArrayTexture      (InDesc.IsTextureArray())
	, bStreamable        (EnumHasAnyFlags(InDesc.Flags, TexCreate_Streamable))
	, bDepthStencil      (EnumHasAnyFlags(InDesc.Flags, TexCreate_DepthStencilTargetable))
	, bCanCreateAsEvicted(false)
	, bIsPowerOfTwo      (false)
	, bMultisampleRenderbuffer(EnumHasAnyFlags(InDesc.Flags, TexCreate_Memoryless) && InDesc.NumSamples > 1)
{
	checkf(!bCubemap || InDesc.NumSamples == 1, TEXT("Texture cubes cannot be multisampled."));
	checkf(FOpenGL::SupportsTexture3D() || (!InDesc.IsTexture3D() && !InDesc.IsTextureArray()), TEXT("Texture3D / Texture2DArray support requires FOpenGL::SupportsTexture3D()."));
	checkf(!bMultisampleRenderbuffer || EnumHasAnyFlags(InDesc.Flags, TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable), TEXT("Only render targets can be memoryless"));

	// Special case for multiview MSAA depth target. It has to be a non-MSAA texture with multisample rendering
	const bool bMultiviewMSAADepthTarget = (bDepthStencil && InDesc.NumSamples > 1 && InDesc.Dimension == ETextureDimension::Texture2DArray);
	if (bMultiviewMSAADepthTarget || FOpenGL::GetMaxMSAASamplesTileMem() == 1)
	{
		bMultisampleRenderbuffer = false;
	}
			
	// Select an appropriate texture target
	if (bMultisampleRenderbuffer)
	{
		// Special case for multisample memoryless render targets
		Target = GL_RENDERBUFFER;
	}
	else 
	{
		if (bMultiviewMSAADepthTarget)
		{
			Target = GL_TEXTURE_2D_ARRAY;
		}
		else if (EnumHasAnyFlags(InDesc.Flags, TexCreate_External))
		{
			check(InDesc.IsTexture2D());
			check(!InDesc.IsTextureArray());

			Target = FOpenGL::SupportsImageExternal()
				? GL_TEXTURE_EXTERNAL_OES
				// Fall back to a regular 2d texture if we don't have support.
				// Texture samplers in the shader will also fall back to a regular sampler2D.
				: GL_TEXTURE_2D;
		}
		else if (EnumHasAnyFlags(InDesc.Flags, TexCreate_Presentable))
		{
			check(InDesc.Dimension == ETextureDimension::Texture2D);
			Target = GL_RENDERBUFFER;
		}
		else
		{
			switch (InDesc.Dimension)
			{
			default: checkNoEntry();
			case ETextureDimension::Texture2D:		  Target = (InDesc.NumSamples > 1) ? GL_TEXTURE_2D_MULTISAMPLE       : GL_TEXTURE_2D;       break;
			case ETextureDimension::Texture2DArray:	  Target = (InDesc.NumSamples > 1) ? GL_TEXTURE_2D_MULTISAMPLE_ARRAY : GL_TEXTURE_2D_ARRAY; break;
			case ETextureDimension::TextureCubeArray: Target = GL_TEXTURE_CUBE_MAP_ARRAY; break;
			case ETextureDimension::TextureCube:	  Target = GL_TEXTURE_CUBE_MAP;       break;
			case ETextureDimension::Texture3D:		  Target = GL_TEXTURE_3D;             break;
			}
		}
	}
	check(Target != GL_NONE);

	// can run on RT.
	bCanCreateAsEvicted =
		CanDeferTextureCreation()
		&& InDesc.Flags != TexCreate_None // ignore TexCreate_None
		&& !EnumHasAnyFlags((ETextureCreateFlags)CVarDeferTextureCreationExcludeMask.GetValueOnAnyThread(), InDesc.Flags)  // Anything outside of these flags cannot be evicted.
		&& Target == GL_TEXTURE_2D
		&& InDesc.IsTexture2D(); // 2d only.

	if (GOGLTextureEvictLogging)
	{
		UE_CLOG(!bCanCreateAsEvicted, LogRHI, Warning, TEXT("CanDeferTextureCreation:%d, Flags:%llx Mask:%x, Target:%x"),
						bCanCreateAsEvicted, int(InDesc.Flags), CVarDeferTextureCreationExcludeMask.GetValueOnAnyThread(), Target);
	}

	bIsPowerOfTwo = 
		   FMath::IsPowerOfTwo(InDesc.Extent.X)
		&& FMath::IsPowerOfTwo(InDesc.Extent.Y)
		&& FMath::IsPowerOfTwo(InDesc.Depth);

	MemorySize = InDesc.CalcMemorySizeEstimate();

	// Determine the attachment point for the texture.	
	if (EnumHasAnyFlags(InDesc.Flags, TexCreate_RenderTargetable | TexCreate_CPUReadback))
	{
		Attachment = GL_COLOR_ATTACHMENT0;
	}
	else if (EnumHasAnyFlags(InDesc.Flags, TexCreate_DepthStencilTargetable))
	{
		Attachment = (InDesc.Format == PF_DepthStencil) ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
	}
	else if (EnumHasAnyFlags(InDesc.Flags, TexCreate_ResolveTargetable))
	{
		Attachment = (InDesc.Format == PF_DepthStencil)
			? GL_DEPTH_STENCIL_ATTACHMENT
			: ((InDesc.Format == PF_ShadowDepth || InDesc.Format == PF_D24)
				? GL_DEPTH_ATTACHMENT
				: GL_COLOR_ATTACHMENT0);
	}
	else
	{
		Attachment = GL_NONE;
	}

	switch (Attachment)
	{
	case GL_COLOR_ATTACHMENT0:
		check(GMaxOpenGLColorSamples >= (GLint)InDesc.NumSamples);
		break;
	case GL_DEPTH_ATTACHMENT:
	case GL_DEPTH_STENCIL_ATTACHMENT:
		check(GMaxOpenGLDepthSamples >= (GLint)InDesc.NumSamples);
		break;
	default:
		break;
	}
}

// Constructor for RHICreateAliasedTexture
FOpenGLTexture::FOpenGLTexture(FOpenGLTexture& Other, const FString& Name, EAliasConstructorParam)
	: FRHITexture(FRHITextureCreateDesc(Other.GetDesc(), ERHIAccess::SRVMask, *Name))
	, Target             (Other.Target)
	, Attachment         (Other.Attachment)
	, MemorySize         (0)
	, bIsPowerOfTwo      (Other.bIsPowerOfTwo)
	, bCanCreateAsEvicted(false)
	, bStreamable        (Other.bStreamable)
	, bCubemap           (Other.bCubemap)
	, bArrayTexture      (Other.bArrayTexture)
	, bDepthStencil      (Other.bDepthStencil)
	, bAlias             (true)
	, bMultisampleRenderbuffer(Other.bMultisampleRenderbuffer)
{
	RunOnGLRenderContextThread([&]()
	{
		AliasResources(Other);
	});
}

void FOpenGLTexture::AliasResources(FOpenGLTexture& Texture)
{
	VERIFY_GL_SCOPE();
	check(bAlias && !Texture.bAlias);

	// restore the source texture, do not allow the texture to become evicted, the aliasing texture cannot re-create the resource.
	if (Texture.IsEvicted())
	{
		Texture.RestoreEvictedGLResource(false);
	}

	Resource = Texture.Resource;
}

// Constructor for external resources (RHICreateTexture2DFromResource etc).
FOpenGLTexture::FOpenGLTexture(FOpenGLTextureCreateDesc const& CreateDesc, GLuint InResource)
	: FRHITexture        (CreateDesc)
	, Resource           (InResource)
	, Target             (CreateDesc.Target)
	, Attachment         (CreateDesc.Attachment)
	, MemorySize         (CreateDesc.MemorySize)
	, bIsPowerOfTwo      (CreateDesc.bIsPowerOfTwo)
	, bCanCreateAsEvicted(false)
	, bStreamable        (CreateDesc.bStreamable)
	, bCubemap           (CreateDesc.bCubemap)
	, bArrayTexture      (CreateDesc.bArrayTexture)
	, bDepthStencil      (CreateDesc.bDepthStencil)
	, bAlias             (true)
	, bMultisampleRenderbuffer(CreateDesc.bMultisampleRenderbuffer)
{}

// Standard constructor.
FOpenGLTexture::FOpenGLTexture(FRHICommandListBase& RHICmdList, FOpenGLTextureCreateDesc const& CreateDesc)
	: FRHITexture        (CreateDesc)
	, Target             (CreateDesc.Target)
	, Attachment         (CreateDesc.Attachment)
	, MemorySize         (CreateDesc.MemorySize)
	, bIsPowerOfTwo      (CreateDesc.bIsPowerOfTwo)
	, bCanCreateAsEvicted(CreateDesc.bCanCreateAsEvicted)
	, bStreamable        (CreateDesc.bStreamable)
	, bCubemap           (CreateDesc.bCubemap)
	, bArrayTexture      (CreateDesc.bArrayTexture)
	, bDepthStencil      (CreateDesc.bDepthStencil)
	, bAlias             (false)
	, bMultisampleRenderbuffer(CreateDesc.bMultisampleRenderbuffer)
{
	SCOPE_CYCLE_COUNTER(STAT_OpenGLCreateTextureTime);

	if (bCanCreateAsEvicted)
	{
		EvictionParamsPtr = MakeUnique<FTextureEvictionParams>(CreateDesc.NumMips);
	}

	void* BulkDataPtr = nullptr;
	uint64 BulkDataSize = 0;
	bool bFreeBulkData = false;

	if (CreateDesc.BulkData)
	{
// FORT-672412: speculative and temporary fix for create texture/lock happening out of order.
		if (!ShouldRunGLRenderContextOpOnThisThread(RHICmdList))
// 		if (RHICmdList.IsTopOfPipe())
		{
			// If bulk data is provided, and texture initialization is done by the RHI thread, it needs to be copied out of the FResourceBulkDataInterface.
			// It is not safe to pass this pointer to the RHI thread, as the interface may have been stack allocated in the renderer.
			// @todo: remove this memcpy when the new resource creation API is available.
			BulkDataSize = CreateDesc.BulkData->GetResourceBulkDataSize();
			BulkDataPtr = FMemory::Malloc(BulkDataSize);

			FMemory::Memcpy(BulkDataPtr, CreateDesc.BulkData->GetResourceBulkData(), BulkDataSize);
			bFreeBulkData = true;
		}
		else
		{
			// Otherwise, initialization will be done on this thread.
			// Just use the raw pointer / size as-is.
			BulkDataSize = CreateDesc.BulkData->GetResourceBulkDataSize();
			BulkDataPtr = const_cast<void*>(CreateDesc.BulkData->GetResourceBulkData());
		}
	}
// 	FORT-672412: speculative and temporary fix for create texture/lock happening out of order.
	RunOnGLRenderContextThread([this, BulkDataPtr, BulkDataSize, bFreeBulkData]()
// 	RHICmdList.EnqueueLambda([this, BulkDataPtr, BulkDataSize, bFreeBulkData](FRHICommandListBase&)
	{
		FOpenGLDynamicRHI::Get().InitializeGLTexture(this, BulkDataPtr, BulkDataSize);
		if (bFreeBulkData)
		{
			FMemory::Free(BulkDataPtr);
		}
	});

	UpdateTextureStats(this, true);

	PixelBuffers.AddZeroed(CreateDesc.NumMips * (bCubemap ? 6 : 1) * GetEffectiveSizeZ());

	if (CreateDesc.BulkData)
	{
		CreateDesc.BulkData->Discard();
	}
}

FOpenGLTexture::~FOpenGLTexture()
{
	VERIFY_GL_SCOPE();

	FTextureEvictionLRU::Get().Remove(this);

	if (!bCanCreateAsEvicted)
	{
		ReleaseOpenGLFramebuffers(this);
	}

	DeleteGLResource();
	UpdateTextureStats(this, false);
}

void FOpenGLTexture::DeleteGLResource()
{
	VERIFY_GL_SCOPE();
	QUICK_SCOPE_CYCLE_COUNTER(STAT_OpenGLDeleteGLTextureTime);

	if (Resource != 0)
	{
		switch (Target)
		{
		case GL_TEXTURE_2D:
		case GL_TEXTURE_2D_MULTISAMPLE:
		case GL_TEXTURE_3D:
		case GL_TEXTURE_CUBE_MAP:
		case GL_TEXTURE_2D_ARRAY:
		case GL_TEXTURE_CUBE_MAP_ARRAY:
		case GL_TEXTURE_EXTERNAL_OES:
			FOpenGLDynamicRHI::Get().InvalidateTextureResourceInCache(Resource);
			if (!bAlias)
			{
				FOpenGL::DeleteTextures(1, &Resource);
			}
			break;

		case GL_RENDERBUFFER:
			if (!bAlias)
			{
				glDeleteRenderbuffers(1, &Resource);
			}
			break;

		default:
			checkNoEntry();
			break;
		}
	}
}

static inline bool IsAstcLdrRGBAFormat(GLenum Format)
{
	return Format >= GL_COMPRESSED_RGBA_ASTC_4x4_KHR && Format <= GL_COMPRESSED_RGBA_ASTC_12x12_KHR;
}

uint32 GTotalTexStorageSkipped = 0;
uint32 GTotalCompressedTexStorageSkipped = 0;

void FOpenGLDynamicRHI::InitializeGLTexture(FOpenGLTexture* Texture, const void* BulkDataPtr, uint64 BulkDataSize)
{
	VERIFY_GL_SCOPE();

	if (EnumHasAnyFlags(Texture->GetDesc().Flags, TexCreate_Presentable))
		return;

	// Allocate the GL resource ID
	GLuint TextureID;
	if (Texture->bMultisampleRenderbuffer)
	{
		check(Texture->Target == GL_RENDERBUFFER);
		glGenRenderbuffers(1, &TextureID);
	}
	else
	{
		check(Texture->Target != GL_RENDERBUFFER);
		glGenTextures(1, &TextureID);
	}
	Texture->SetResource(TextureID);

	if (!Texture->IsEvicted())
	{
		InitializeGLTextureInternal(Texture, BulkDataPtr, BulkDataSize);
	}
	else
	{
		// creating this as 'evicted'.
		GTotalTexStorageSkipped++;

		EPixelFormat PixelFormat = Texture->GetFormat();
		const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[PixelFormat];
		bool bIsCompressed = GLFormat.bCompressed;
		GTotalCompressedTexStorageSkipped += bIsCompressed ? 1 : 0;

		if (BulkDataPtr)
		{
			check(!GLFormat.bCompressed);
			const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
			const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;

			uint8* Data = (uint8*)BulkDataPtr;
			uint32 MipOffset = 0;

			const FRHITextureDesc& Desc = Texture->GetDesc();

			// copy bulk data to evicted mip store:
			for (uint32 MipIndex = 0; MipIndex < Desc.NumMips; MipIndex++)
			{
				uint32 NumBlocksX = AlignArbitrary(FMath::Max<uint32>(1, (Desc.Extent.X >> MipIndex)), BlockSizeX) / BlockSizeX;
				uint32 NumBlocksY = AlignArbitrary(FMath::Max<uint32>(1, (Desc.Extent.Y >> MipIndex)), BlockSizeY) / BlockSizeY;
				uint32 NumLayers = FMath::Max<uint32>(1, Desc.ArraySize);
				uint32 MipDataSize = NumBlocksX * NumBlocksY * NumLayers * GPixelFormats[PixelFormat].BlockBytes;

				Texture->EvictionParamsPtr->SetMipData(MipIndex, &Data[MipOffset], MipDataSize);
				MipOffset += MipDataSize;
			}
		}
	}
}

void FOpenGLDynamicRHI::InitializeGLTextureInternal(FOpenGLTexture* Texture, void const* BulkDataPtr, uint64 BulkDataSize)
{
	VERIFY_GL_SCOPE();

	GLuint const TextureID = Texture->GetRawResourceName();

	const FRHITextureDesc& Desc = Texture->GetDesc();
	const GLenum Target = Texture->Target;

	const bool bSRGB = EnumHasAnyFlags(Desc.Flags, TexCreate_SRGB);
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[Desc.Format];
	if (GLFormat.InternalFormat[bSRGB] == GL_NONE)
	{
		UE_LOG(LogRHI, Fatal,TEXT("Texture format '%s' not supported (sRGB=%d)."), GPixelFormats[Desc.Format].Name, bSRGB);
	}
	
	const bool bMultiviewMSAADepthTarget = (Texture->bDepthStencil && Desc.NumSamples > 1 && Desc.Dimension == ETextureDimension::Texture2DArray);

	FOpenGLContextState& ContextState = GetContextStateForCurrentContext();

	// Make sure PBO is disabled
	CachedBindPixelUnpackBuffer(ContextState, 0);

	bool bAllocatedStorage = false;
	if (Texture->bMultisampleRenderbuffer)
	{
		check(Texture->IsMultisampled());
		check(Target == GL_RENDERBUFFER);
		// Multisample Renderbuffers will be allocated on first use. See ConditionallyAllocateRenderbufferStorage
	}
	else
	{
		// Use a texture stage that's not likely to be used for draws, to avoid waiting
		CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, TextureID, 0, Desc.NumMips);

		if((GLFormat.bBGRA && !EnumHasAnyFlags(Desc.Flags, TexCreate_RenderTargetable)) 
#if !PLATFORM_ANDROID
			|| (GLFormat.InternalFormat[0] == GL_RGB5_A1)
#endif
			)
		{
			glTexParameteri(Target, GL_TEXTURE_SWIZZLE_R, GL_BLUE);
			glTexParameteri(Target, GL_TEXTURE_SWIZZLE_B, GL_RED);
		}

		if (!Texture->IsMultisampled())
		{
			if (Target == GL_TEXTURE_EXTERNAL_OES || !FMath::IsPowerOfTwo(Desc.Extent.X) || !FMath::IsPowerOfTwo(Desc.Extent.Y))
			{
				glTexParameteri(Target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(Target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
				if (FOpenGL::SupportsTexture3D())
				{
					glTexParameteri(Target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
				}
			}
			else
			{
				glTexParameteri(Target, GL_TEXTURE_WRAP_S, GL_REPEAT);
				glTexParameteri(Target, GL_TEXTURE_WRAP_T, GL_REPEAT);
				if (FOpenGL::SupportsTexture3D())
				{
					glTexParameteri(Target, GL_TEXTURE_WRAP_R, GL_REPEAT);
				}
			}

			glTexParameteri(Target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(Target, GL_TEXTURE_MIN_FILTER, Desc.NumMips > 1 ? GL_NEAREST_MIPMAP_NEAREST : GL_NEAREST);

			if (FOpenGL::SupportsTextureFilterAnisotropic())
			{
				glTexParameteri(Target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1);
			}
		}

		glTexParameteri(Target, GL_TEXTURE_BASE_LEVEL, 0);

		TextureMipLimits.Add(TextureID, TPair<GLenum, GLenum>(0, Desc.NumMips - 1));

		if (FOpenGL::SupportsASTCDecodeMode())
		{
			if (IsAstcLdrRGBAFormat(GLFormat.InternalFormat[bSRGB]))
			{
				glTexParameteri(Target, TEXTURE_ASTC_DECODE_PRECISION_EXT, GL_RGBA8);
			}
		}

		if (Target != GL_TEXTURE_EXTERNAL_OES)
		{
			auto EnumerateSubresources = [&](void const* Data, TFunctionRef<bool(GLenum Target, uint32 SizeX, uint32 SizeY, uint32 SizeZ, uint32 MipIndex, uint32 ArraySlice, void const* MipSliceData, uint32 MipSliceSize)> Callback)
			{
				struct FScopedPackAlignment
				{
					 FScopedPackAlignment() { glPixelStorei(GL_UNPACK_ALIGNMENT, 1); }
					~FScopedPackAlignment() { glPixelStorei(GL_UNPACK_ALIGNMENT, 4); }
				} PackAlignment;

				uint64 DataOffset = 0;

				for (uint32 MipIndex = 0; MipIndex < Desc.NumMips; ++MipIndex)
				{
					for (uint32 ArraySlice = 0; ArraySlice < Desc.ArraySize; ++ArraySlice)
					{
						for (uint32 FaceIndex = 0; FaceIndex < (Desc.IsTextureCube() ? 6u : 1u); ++FaceIndex)
						{
							const uint32 MipPixelSizeX = FMath::Max<uint32>(1u, Desc.Extent.X >> MipIndex);
							const uint32 MipPixelSizeY = FMath::Max<uint32>(1u, Desc.Extent.Y >> MipIndex);
							const uint32 MipPixelSizeZ = FMath::Max<uint32>(1u, Desc.Depth >> MipIndex);

							const GLenum CurrentTarget = Target == GL_TEXTURE_CUBE_MAP
								? GL_TEXTURE_CUBE_MAP_POSITIVE_X + FaceIndex
								: Target;

							if (Data)
							{
								const uint32 MipBlockSizeX = FMath::DivideAndRoundUp<uint32>(MipPixelSizeX, GPixelFormats[Desc.Format].BlockSizeX);
								const uint32 MipBlockSizeY = FMath::DivideAndRoundUp<uint32>(MipPixelSizeY, GPixelFormats[Desc.Format].BlockSizeY);
								const uint32 MipBlockSizeZ = FMath::DivideAndRoundUp<uint32>(MipPixelSizeZ, GPixelFormats[Desc.Format].BlockSizeZ);

								const uint32 MipNumBlocks = MipBlockSizeX * MipBlockSizeY * MipBlockSizeZ;
								const uint32 MipSize = MipNumBlocks * GPixelFormats[Desc.Format].BlockBytes * Desc.ArraySize;

								if (!Callback(CurrentTarget, MipPixelSizeX, MipPixelSizeY, MipPixelSizeZ, MipIndex, ArraySlice, Data, MipSize))
									return;

								DataOffset += MipSize;
								if (DataOffset >= BulkDataSize)
								{
									// Reach the end of bulk data. Only pass nullptr to the callback for any subsequent mips / slices
									Data = nullptr;
								}
							}
							else
							{
								if (!Callback(CurrentTarget, MipPixelSizeX, MipPixelSizeY, MipPixelSizeZ, MipIndex, ArraySlice, nullptr, 0))
									return;
							}
						}
					}
				}
			};

			// Create the texture resource
			switch (Target)
			{
			default: checkNoEntry();
			case GL_RENDERBUFFER:
			case GL_TEXTURE_2D:
			case GL_TEXTURE_CUBE_MAP:
				{
					// Try to create the texture using immutable storage
					FOpenGL::TexStorage2D(Target, Desc.NumMips, GLFormat.InternalFormat[bSRGB], Desc.Extent.X, Desc.Extent.Y, GLFormat.Format, GLFormat.Type, Desc.Flags);

					// Texture created with immutable storage. Now fill in the bulk data.
					bAllocatedStorage = true;

					if (BulkDataPtr)
					{
						EnumerateSubresources(BulkDataPtr, [&](GLenum CurrentTarget, uint32 MipSizeX, uint32 MipSizeY, uint32 MipSizeZ, uint32 MipIndex, uint32 ArraySlice, void const* MipSliceData, uint32 MipSliceSize)
						{
							// Stop when there's no more bulk data
							if (MipSliceData == nullptr)
								return false;

							if (GLFormat.bCompressed)
							{
								glCompressedTexSubImage2D(
									CurrentTarget,
									MipIndex,
									0, 0, // X/Y offset
									MipSizeX, MipSizeY,
									GLFormat.Format,
									MipSliceSize,
									MipSliceData);
							}
							else
							{
								glTexSubImage2D(
									CurrentTarget,
									MipIndex,
									0, 0, // X/Y offset
									MipSizeX, MipSizeY,
									GLFormat.Format,
									GLFormat.Type,
									MipSliceData);
							}

							return true;
						});
					}
				}
				break;

			case GL_TEXTURE_2D_ARRAY:
			case GL_TEXTURE_CUBE_MAP_ARRAY:
			case GL_TEXTURE_3D:
				{
					bAllocatedStorage = true; // Always supported if 3D textures are supported.

					const uint32 SizeZ =
						Target == GL_TEXTURE_3D ? Desc.Depth :
						Target == GL_TEXTURE_CUBE_MAP_ARRAY ? Desc.ArraySize * 6 :
						Desc.ArraySize;

					FOpenGL::TexStorage3D(Target, Desc.NumMips, GLFormat.InternalFormat[bSRGB], Desc.Extent.X, Desc.Extent.Y, SizeZ, GLFormat.Format, GLFormat.Type);

					// Texture created with immutable storage. Now fill in the bulk data.
					if (BulkDataPtr)
					{
						EnumerateSubresources(BulkDataPtr, [&](GLenum CurrentTarget, uint32 MipSizeX, uint32 MipSizeY, uint32 MipSizeZ, uint32 MipIndex, uint32 ArraySlice, void const* MipSliceData, uint32 MipSliceSize)
						{
							// Stop when there's no more bulk data
							if (MipSliceData == nullptr)
								return false;

							if (GLFormat.bCompressed)
							{
								glCompressedTexSubImage3D(
									CurrentTarget,
									MipIndex,
									0, 0, ArraySlice, // X/Y/Z offset
									MipSizeX, MipSizeY, MipSizeZ,
									GLFormat.Format,
									MipSliceSize,
									MipSliceData);
							}
							else
							{
								glTexSubImage3D(
									CurrentTarget,
									MipIndex,
									0, 0, ArraySlice, // X/Y/Z offset
									MipSizeX, MipSizeY, MipSizeZ,
									GLFormat.Format,
									GLFormat.Type,
									MipSliceData);
							}

							return true;
						});
					}
				}
				break;

			case GL_TEXTURE_2D_MULTISAMPLE:
				{
					checkf(BulkDataPtr == nullptr, TEXT("Multisample textures cannot be created with initial bulk data."));

					// Try to create an immutable storage texture and fallback if it fails
					const int32 NumSamples = Texture->GetDesc().NumSamples;
					const bool FixedSampleLocations = true;
					FOpenGL::TexStorage2DMultisample(Target, NumSamples, GLFormat.InternalFormat[bSRGB], Desc.Extent.X, Desc.Extent.Y, FixedSampleLocations);
					bAllocatedStorage = true;
				}
				break;
			}
		}
	}

	// @todo: If integer pixel format
	//check(GMaxOpenGLIntegerSamples>=NumSamples);
	Texture->SetAllocatedStorage(bAllocatedStorage);

	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.
}


void FOpenGLTexture::Resolve(uint32 MipIndex,uint32 ArrayIndex)
{
	VERIFY_GL_SCOPE();
	check(!GetTexture2D() || GetNumSamples() == 1);
	
	// Calculate the dimensions of the mip-map.
	EPixelFormat PixelFormat = this->GetFormat();
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	const uint32 MipSizeX = FMath::Max(this->GetSizeX() >> MipIndex,BlockSizeX);
	const uint32 MipSizeY = FMath::Max(this->GetSizeY() >> MipIndex,BlockSizeY);
	uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	uint32 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;
	const uint32 MipBytes = NumBlocksX * NumBlocksY * BlockBytes;
	
	const int32 BufferIndex = MipIndex * (bCubemap ? 6 : 1) * this->GetEffectiveSizeZ() + ArrayIndex;
	
	// Standard path with a PBO mirroring ever slice of a texture to allow multiple simulataneous maps
	if (!IsValidRef(PixelBuffers[BufferIndex]))
	{
		PixelBuffers[BufferIndex] = new FOpenGLPixelBuffer(nullptr, GL_PIXEL_UNPACK_BUFFER, FRHIBufferDesc(MipBytes, 0, BUF_Dynamic), nullptr);
	}
	
	TRefCountPtr<FOpenGLPixelBuffer> PixelBuffer = PixelBuffers[BufferIndex];
	check(PixelBuffer->GetSize() == MipBytes);
	check(!PixelBuffer->IsLocked());
	
	// Transfer data from texture to pixel buffer.
	// This may be further optimized by caching information if surface content was changed since last lock.
	
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[PixelFormat];
	const bool bSRGB = EnumHasAnyFlags(this->GetFlags(), TexCreate_SRGB);
	
	// Use a texture stage that's not likely to be used for draws, to avoid waiting
	FOpenGLContextState& ContextState = FOpenGLDynamicRHI::Get().GetContextStateForCurrentContext();

	FOpenGLDynamicRHI::Get().CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, this->GetResource(), -1, this->GetNumMips());
	
	glBindBuffer( GL_PIXEL_PACK_BUFFER, PixelBuffer->Resource );

	{
		if (GetDesc().IsTextureArray() || GetDesc().IsTexture3D())
		{
			// apparently it's not possible to retrieve compressed image from GL_TEXTURE_2D_ARRAY in OpenGL for compressed images
			// and for uncompressed ones it's not possible to specify the image index
			check(0);
		}
		else
		{
			if (GLFormat.bCompressed)
			{
				FOpenGL::GetCompressedTexImage(
											   bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
											   MipIndex,
											   0);	// offset into PBO
			}
			else
			{
				// Get framebuffer for texture
				FOpenGLTexture* Texture = this;
				GLuint SourceFramebuffer = FOpenGLDynamicRHI::Get().GetOpenGLFramebuffer(1, &Texture, (bCubemap ? &ArrayIndex : nullptr), &MipIndex, nullptr);
				// Bind the framebuffer
				glBindFramebuffer(UGL_READ_FRAMEBUFFER, SourceFramebuffer);
				FOpenGL::ReadBuffer(GL_COLOR_ATTACHMENT0);

				glPixelStorei(GL_PACK_ALIGNMENT, 1);
				glReadPixels(0, 0, MipSizeX, MipSizeY, GLFormat.Format, GLFormat.Type, 0);
				glPixelStorei(GL_PACK_ALIGNMENT, 4);

				FOpenGLDynamicRHI::Get().GetContextStateForCurrentContext().Framebuffer = (GLuint)-1;
			}
		}
	}
	
	glBindBuffer( GL_PIXEL_PACK_BUFFER, 0 );
	
	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.
}

uint32 FOpenGLTexture::GetLockSize(uint32 InMipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride)
{
	// Calculate the dimensions of the mip-map.
	EPixelFormat PixelFormat = this->GetFormat();
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	const uint32 MipSizeX = FMath::Max(this->GetSizeX() >> InMipIndex, BlockSizeX);
	const uint32 MipSizeY = FMath::Max(this->GetSizeY() >> InMipIndex, BlockSizeY);
	uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	uint32 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;
	const uint32 MipBytes = NumBlocksX * NumBlocksY * BlockBytes;
	DestStride = NumBlocksX * BlockBytes;
	return MipBytes;
}

void FOpenGLTexture::Fill2DGLTextureImage(const FOpenGLTextureFormat& GLFormat, const bool bSRGB, uint32 MipIndex, const void* BufferOrPBOOffset, uint32 ImageSize, uint32 ArrayIndex)
{
	if (GLFormat.bCompressed)
	{
		if (GetAllocatedStorageForMip(MipIndex,ArrayIndex))
		{
			glCompressedTexSubImage2D(
				bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
				MipIndex,
				0,
				0,
				FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
				FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
				GLFormat.InternalFormat[bSRGB],
				ImageSize,
				BufferOrPBOOffset);	// offset into PBO
		}
		else
		{
			glCompressedTexImage2D(
				bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
				MipIndex,
				GLFormat.InternalFormat[bSRGB],
				FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
				FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
				0,
				ImageSize,
				BufferOrPBOOffset);	// offset into PBO
			SetAllocatedStorageForMip(MipIndex,ArrayIndex);
		}
	}
	else
	{
		// All construction paths should have called TexStorage2D or TexImage2D. So we will
		// always call TexSubImage2D.
		check(GetAllocatedStorageForMip(MipIndex,ArrayIndex) == true);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexSubImage2D(
			bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
			MipIndex,
			0,
			0,
			FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
			FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
			GLFormat.Format,
			GLFormat.Type,
			BufferOrPBOOffset);	// offset into PBO
		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	}
}

void* FOpenGLTexture::Lock(uint32 InMipIndex,uint32 ArrayIndex,EResourceLockMode LockMode,uint32& DestStride)
{
	VERIFY_GL_SCOPE();
	check(!GetTexture2D() || GetNumSamples() == 1);

	SCOPE_CYCLE_COUNTER(STAT_OpenGLLockTextureTime);
	
	const uint32 MipBytes = GetLockSize(InMipIndex, ArrayIndex, LockMode, DestStride);

	check(!IsEvicted() || ArrayIndex==0);
	void* result = NULL;

	const int32 BufferIndex = InMipIndex * (bCubemap ? 6 : 1) * this->GetEffectiveSizeZ() + ArrayIndex;
	EPixelFormat PixelFormat = this->GetFormat();

	// Should we use client-storage to improve update time on platforms that require it
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[PixelFormat];
	if (IsEvicted())
	{
		check(ArrayIndex == 0);
		// check there's nothing already here?
		ensure(InMipIndex >= (uint32)EvictionParamsPtr->MipImageData.Num() || EvictionParamsPtr->MipImageData[InMipIndex].Num() == 0);
		EvictionParamsPtr->SetMipData(InMipIndex, 0, MipBytes);
		return EvictionParamsPtr->MipImageData[InMipIndex].GetData();
	}
	else
	{
		// Standard path with a PBO mirroring ever slice of a texture to allow multiple simulataneous maps
		bool bBufferExists = true;
		if (!IsValidRef(PixelBuffers[BufferIndex]))
		{
			bBufferExists = false;
			PixelBuffers[BufferIndex] = new FOpenGLPixelBuffer(nullptr, GL_PIXEL_UNPACK_BUFFER, FRHIBufferDesc(MipBytes, 0, BUF_Dynamic), nullptr);
		}

		TRefCountPtr<FOpenGLPixelBuffer> PixelBuffer = PixelBuffers[BufferIndex];
		check(PixelBuffer->GetSize() == MipBytes);
		check(!PixelBuffer->IsLocked());
		
		// If the buffer already exists & the flags are such that the texture cannot be rendered to & is CPU accessible then we can skip the internal resolve for read locks. This makes HZB occlusion faster.
		const bool bCPUTexResolved = bBufferExists && EnumHasAnyFlags(this->GetFlags(), TexCreate_CPUReadback) && !EnumHasAnyFlags(this->GetFlags(), TexCreate_RenderTargetable|TexCreate_DepthStencilTargetable);

		if (LockMode != RLM_WriteOnly && !bCPUTexResolved)
		{
			Resolve(InMipIndex, ArrayIndex);
		}

		result = PixelBuffer->Lock(0, PixelBuffer->GetSize(), LockMode == RLM_ReadOnly, LockMode != RLM_ReadOnly);
	}
	
	return result;
}

// Copied from OpenGLDebugFrameDump.
inline uint32 HalfFloatToFloatInteger(uint16 HalfFloat)
{
	uint32 Sign = (HalfFloat >> 15) & 0x00000001;
	uint32 Exponent = (HalfFloat >> 10) & 0x0000001f;
	uint32 Mantiss = HalfFloat & 0x000003ff;

	if (Exponent == 0)
	{
		if (Mantiss == 0) // Plus or minus zero
		{
			return Sign << 31;
		}
		else // Denormalized number -- renormalize it
		{
			while ((Mantiss & 0x00000400) == 0)
			{
				Mantiss <<= 1;
				Exponent -= 1;
			}

			Exponent += 1;
			Mantiss &= ~0x00000400;
		}
	}
	else if (Exponent == 31)
	{
		if (Mantiss == 0) // Inf
			return (Sign << 31) | 0x7f800000;
		else // NaN
			return (Sign << 31) | 0x7f800000 | (Mantiss << 13);
	}

	Exponent = Exponent + (127 - 15);
	Mantiss = Mantiss << 13;

	return (Sign << 31) | (Exponent << 23) | Mantiss;
}

inline float HalfFloatToFloat(uint16 HalfFloat)
{
	union
	{
		float F;
		uint32 I;
	} Convert;

	Convert.I = HalfFloatToFloatInteger(HalfFloat);
	return Convert.F;
}

void FOpenGLTexture::Unlock(uint32 MipIndex,uint32 ArrayIndex)
{
	VERIFY_GL_SCOPE();
	SCOPE_CYCLE_COUNTER(STAT_OpenGLUnlockTextureTime);

	if (IsEvicted())
	{
		// evicted textures didn't actually perform a lock, so we can bail out early
		check(ArrayIndex == 0);
		// check the space was allocated
		ensure(MipIndex < (uint32)EvictionParamsPtr->MipImageData.Num() && EvictionParamsPtr->MipImageData[MipIndex].Num());
		return;
	}

	const int32 BufferIndex = MipIndex * (bCubemap ? 6 : 1) * this->GetEffectiveSizeZ() + ArrayIndex;
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[this->GetFormat()];
	const bool bSRGB = EnumHasAnyFlags(this->GetFlags(), TexCreate_SRGB);
	TRefCountPtr<FOpenGLPixelBuffer> PixelBuffer = PixelBuffers[BufferIndex];

	check(IsValidRef(PixelBuffer));
	
#if PLATFORM_ANDROID
	// check for FloatRGBA to RGBA8 conversion needed
	if (this->GetFormat() == PF_FloatRGBA && GLFormat.Type == GL_UNSIGNED_BYTE)
	{
		UE_LOG(LogRHI, Warning, TEXT("Converting texture from PF_FloatRGBA to RGBA8!  Only supported for limited cases of 0.0 to 1.0 values (clamped)"));

		// Code path for non-PBO: and always uncompressed!
		// Volume/array textures are currently only supported if PixelBufferObjects are also supported.
		check(this->GetSizeZ() == 0);

		// Use a texture stage that's not likely to be used for draws, to avoid waiting
		FOpenGLContextState& ContextState = FOpenGLDynamicRHI::Get().GetContextStateForCurrentContext();
		FOpenGLDynamicRHI::Get().CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, GetResource(), -1, this->GetNumMips());

		CachedBindPixelUnpackBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

		// get the source data and size
		uint16* floatData = (uint16*)PixelBuffer->GetLockedBuffer();
		int32 texWidth = FMath::Max<uint32>(1, (this->GetSizeX() >> MipIndex));
		int32 texHeight = FMath::Max<uint32>(1, (this->GetSizeY() >> MipIndex));

		// always RGBA8 so 4 bytes / pixel
		int nValues = texWidth * texHeight * 4;
		uint8* rgbaData = (uint8*)FMemory::Malloc(nValues);

		// convert to GL_BYTE (saturate)
		uint8* outPtr = rgbaData;
		while (nValues--)
		{
			int32 pixelValue = (int32)(HalfFloatToFloat(*floatData++) * 255.0f);
			*outPtr++ = (uint8)(pixelValue < 0 ? 0 : (pixelValue < 256 ? pixelValue : 255));
		}

		// All construction paths should have called TexStorage2D or TexImage2D. So we will
		// always call TexSubImage2D.
		check(GetAllocatedStorageForMip(MipIndex, ArrayIndex) == true);
		glTexSubImage2D(
			bCubemap ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + ArrayIndex : Target,
			MipIndex,
			0,
			0,
			texWidth,
			texHeight,
			GLFormat.Format,
			GLFormat.Type,
			rgbaData);

		// free temporary conversion buffer
		FMemory::Free(rgbaData);

		// Unlock "PixelBuffer" and free the temp memory after the texture upload.
		PixelBuffer->Unlock();

		// No need to restore texture stage; leave it like this,
		// and the next draw will take care of cleaning it up; or
		// next operation that needs the stage will switch something else in on it.

		CachedBindPixelUnpackBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

		return;
	}
#endif

	// Code path for PBO per slice
	check(IsValidRef(PixelBuffers[BufferIndex]));
			
	PixelBuffer->Unlock();

	// Modify permission?
	if (!PixelBuffer->IsLockReadOnly())
	{
		// Use a texture stage that's not likely to be used for draws, to avoid waiting
		FOpenGLContextState& ContextState = FOpenGLDynamicRHI::Get().GetContextStateForCurrentContext();
		FOpenGLDynamicRHI::Get().CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Target, GetResource(), -1, this->GetNumMips());

		if (GetDesc().IsTextureArray() || GetDesc().IsTexture3D())
		{
			if (GLFormat.bCompressed)
			{
				FOpenGL::CompressedTexSubImage3D(
					Target,
					MipIndex,
					0,
					0,
					ArrayIndex,
					FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
					FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
					1,
					GLFormat.InternalFormat[bSRGB],
					PixelBuffer->GetSize(),
					0);
			}
			else
			{
				glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
				check( FOpenGL::SupportsTexture3D() );
				FOpenGL::TexSubImage3D(
					Target,
					MipIndex,
					0,
					0,
					ArrayIndex,
					FMath::Max<uint32>(1,(this->GetSizeX() >> MipIndex)),
					FMath::Max<uint32>(1,(this->GetSizeY() >> MipIndex)),
					1,
					GLFormat.Format,
					GLFormat.Type,
					0);	// offset into PBO
				glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
			}
		}
		else
		{
			Fill2DGLTextureImage(GLFormat, bSRGB, MipIndex, 0, PixelBuffer->GetSize(), ArrayIndex);
		}
	}

	//need to free PBO if we aren't keeping shadow copies
	PixelBuffers[BufferIndex] = NULL;

	// No need to restore texture stage; leave it like this,
	// and the next draw will take care of cleaning it up; or
	// next operation that needs the stage will switch something else in on it.

	CachedBindPixelUnpackBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}


uint32 GTotalEvictedMipMemStored = 0;
uint32 GTotalEvictedMipMemDuplicated = 0;
uint32 GTotalMipStoredCount = 0;
uint32 GTotalMipRestores = 0;

extern uint32 GTotalEvictedMipMemStored;
extern uint32 GTotalTexStorageSkipped;

float GMaxRestoreTime = 0.0f;
float GAvgRestoreTime = 0.0f;
uint32 GAvgRestoreCount = 0;

void FOpenGLTexture::RestoreEvictedGLResource(bool bAttemptToRetainMips)
{
//	double StartTime = FPlatformTime::Seconds();

	QUICK_SCOPE_CYCLE_COUNTER(STAT_OpenGLRestoreEvictedTextureTime);

	check(!EvictionParamsPtr->bHasRestored);
	EvictionParamsPtr->bHasRestored = true;

	const FClearValueBinding ClearBinding = this->GetClearBinding();
	FOpenGLDynamicRHI::Get().InitializeGLTextureInternal(this, nullptr, 0);

	EPixelFormat PixelFormat = this->GetFormat();
	const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[PixelFormat];
	const bool bSRGB = EnumHasAnyFlags(this->GetFlags(), TexCreate_SRGB);
	checkf(EvictionParamsPtr->MipImageData.Num() == this->GetNumMips(), TEXT("EvictionParamsPtr->MipImageData.Num() =%d, this->GetNumMips() = %d"), EvictionParamsPtr->MipImageData.Num(), this->GetNumMips());

	CachedBindPixelUnpackBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

	for (int i = EvictionParamsPtr->MipImageData.Num() - 1; i >= 0; i--)
	{
		auto& MipMem = EvictionParamsPtr->MipImageData[i];
		if(MipMem.Num())
		{
			Fill2DGLTextureImage(GLFormat, bSRGB, i, MipMem.GetData(), MipMem.Num(), 0);
		}
	}

	// Use the resident streaming mips if our cvar is -1.
	uint32 DeferTextureCreationKeepLowerMipCount = (uint32)(GOGLDeferTextureCreationKeepLowerMipCount >= 0 ? GOGLDeferTextureCreationKeepLowerMipCount : UTexture::GetStaticMinTextureResidentMipCount());

	uint32 RetainMips = bAttemptToRetainMips && EnumHasAnyFlags(this->GetFlags(), TexCreate_Streamable) && this->GetNumMips() > 1 && !bAlias
		? DeferTextureCreationKeepLowerMipCount
		: 0;

	if (CanBeEvicted())
	{
		if (FTextureEvictionLRU::Get().Add(this) == false)
		{
			// could not store this in the LRU. Deleting all backup mips, as this texture will never be evicted.
			RetainMips = 0;
		}
	}

	// keep the mips for streamable textures
	EvictionParamsPtr->ReleaseMipData(RetainMips);
#if GLDEBUG_LABELS_ENABLED
	FAnsiCharArray& TextureDebugName = EvictionParamsPtr->GetDebugLabelName();
	if(TextureDebugName.Num())
	{
		FOpenGL::LabelObject(GL_TEXTURE, this->GetRawResourceName(), TextureDebugName.GetData());
		if (RetainMips == 0)
		{
			TextureDebugName.Empty();
		}
	}
#endif

	GTotalEvictedMipMemDuplicated += EvictionParamsPtr->GetTotalAllocated();
// 	float ThisTime = (float)(FPlatformTime::Seconds() - StartTime);
// 	GAvgRestoreCount++;
// 	GMaxRestoreTime = FMath::Max(GMaxRestoreTime, ThisTime);
// 	GAvgRestoreTime += ThisTime;
}

void FOpenGLTexture::TryEvictGLResource()
{
	VERIFY_GL_SCOPE();
	if (bCanCreateAsEvicted && EvictionParamsPtr->bHasRestored)
	{
		if (CanBeEvicted())
		{
			QUICK_SCOPE_CYCLE_COUNTER(STAT_OpenGLTryEvictGLResource);
			DeleteGLResource();

			// create a new texture id.
			EvictionParamsPtr->bHasRestored = false;
			const FClearValueBinding ClearBinding = this->GetClearBinding();
			// recreate the GL tex resource name (but not allocate the memory)
			FOpenGLDynamicRHI::Get().InitializeGLTexture(this, nullptr, 0);
			GTotalEvictedMipMemDuplicated -= EvictionParamsPtr->GetTotalAllocated();
		}
	}
}

bool FOpenGLTextureDesc::CanDeferTextureCreation()
{
	bool bCanDeferTextureCreation = CVarDeferTextureCreation.GetValueOnAnyThread() != 0;
#if PLATFORM_ANDROID
	static bool bDeferTextureCreationConfigRulesChecked = false;
	static TOptional<bool> bConfigRulesCanDeferTextureCreation;
	if (!bDeferTextureCreationConfigRulesChecked)
	{
		const FString* ConfigRulesDeferOpenGLTextureCreationStr = FAndroidMisc::GetConfigRulesVariable(TEXT("DeferOpenGLTextureCreation"));
		if (ConfigRulesDeferOpenGLTextureCreationStr)
		{
			bConfigRulesCanDeferTextureCreation = ConfigRulesDeferOpenGLTextureCreationStr->Equals("true", ESearchCase::IgnoreCase);
			UE_LOG(LogRHI, Log, TEXT("OpenGL deferred texture creation, set by config rules: %d"), (int)bConfigRulesCanDeferTextureCreation.GetValue());
		}
		else
		{
			UE_LOG(LogRHI, Log, TEXT("OpenGL deferred texture creation, no config rule set: %d"), (int)bCanDeferTextureCreation);
		}
		bDeferTextureCreationConfigRulesChecked = true;
	}

	if (bConfigRulesCanDeferTextureCreation.IsSet())
	{
		bCanDeferTextureCreation = bConfigRulesCanDeferTextureCreation.GetValue();
	}
#endif
	return bCanDeferTextureCreation;
}

bool FOpenGLTexture::CanBeEvicted()
{
	VERIFY_GL_SCOPE();
	checkf(!bCanCreateAsEvicted || EvictionParamsPtr.IsValid(), TEXT("%p, bCanCreateAsEvicted %d, EvictionParamsPtr.IsValid() %d"), this, bCanCreateAsEvicted, EvictionParamsPtr.IsValid());

	// if we're aliased check that there's no eviction data.
	check(!bCanCreateAsEvicted || !bAlias || (EvictionParamsPtr->MipImageData.Num() == 0 && EvictionParamsPtr->MipImageData.Num() != this->GetNumMips()));

	// cant evict if we're aliased, or there are mips are not backed by stored data.
	bool bRet = bCanCreateAsEvicted && EvictionParamsPtr->MipImageData.Num() == this->GetNumMips() && EvictionParamsPtr->AreAllMipsPresent();

	return bRet;
}

void FOpenGLTexture::CloneViaCopyImage(FOpenGLTexture* Src, uint32 InNumMips, int32 SrcOffset, int32 DstOffset)
{
	VERIFY_GL_SCOPE();

	check(Src->bCanCreateAsEvicted == bCanCreateAsEvicted);
	if (bCanCreateAsEvicted)
	{
		// Copy all mips that are present.
		if (!(!Src->IsEvicted() || Src->EvictionParamsPtr->AreAllMipsPresent()))
		{
			UE_LOG(LogRHI, Warning, TEXT("IsEvicted %d, MipsPresent %d, InNumMips %d, SrcOffset %d, DstOffset %d"), Src->IsEvicted(), Src->EvictionParamsPtr->AreAllMipsPresent(), InNumMips, SrcOffset, DstOffset);
			int MessageCount = 0;
			for (const auto& MipData : Src->EvictionParamsPtr->MipImageData)
			{
				UE_LOG(LogRHI, Warning, TEXT("SrcMipData[%d].Num() == %d"), MessageCount++, MipData.Num());
			}	
		}
		check(!Src->IsEvicted() || Src->EvictionParamsPtr->AreAllMipsPresent() );
		EvictionParamsPtr->CloneMipData(*Src->EvictionParamsPtr, InNumMips, SrcOffset, DstOffset);

		// the dest texture can remain evicted if: the src was also evicted or has all of the resident mips available or the dest texture has all mips already evicted.
		if(IsEvicted() && (Src->IsEvicted() || Src->EvictionParamsPtr->AreAllMipsPresent() || EvictionParamsPtr->AreAllMipsPresent()))
		{
			return;
		}
	}

	for (uint32 ArrayIndex = 0; ArrayIndex < this->GetEffectiveSizeZ(); ArrayIndex++)
	{
		// use the Copy Image functionality to copy mip level by mip level
		for(uint32 MipIndex = 0;MipIndex < InNumMips;++MipIndex)
		{
			// Calculate the dimensions of the mip-map.
			const uint32 DstMipIndex = MipIndex + DstOffset;
			const uint32 SrcMipIndex = MipIndex + SrcOffset;
			const uint32 MipSizeX = FMath::Max(this->GetSizeX() >> DstMipIndex,uint32(1));
			const uint32 MipSizeY = FMath::Max(this->GetSizeY() >> DstMipIndex,uint32(1));
			
			if(FOpenGL::AmdWorkaround() && ((MipSizeX < 4) || (MipSizeY < 4))) break;

			// copy the texture data
			FOpenGL::CopyImageSubData(Src->GetResource(), Src->Target, SrcMipIndex, 0, 0, ArrayIndex,
				GetResource(), Target, DstMipIndex, 0, 0, ArrayIndex, MipSizeX, MipSizeY, 1);
		}
	}
	
}

/*-----------------------------------------------------------------------------
	2D texture support.
-----------------------------------------------------------------------------*/

FTextureRHIRef FOpenGLDynamicRHI::RHICreateTexture(FRHICommandListBase& RHICmdList, const FRHITextureCreateDesc& CreateDesc)
{
	return new FOpenGLTexture(RHICmdList, CreateDesc);
}

FTextureRHIRef FOpenGLDynamicRHI::RHIAsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ERHIAccess InResourceState, void** InitialMipData, uint32 NumInitialMips, const TCHAR* DebugName, FGraphEventRef& OutCompletionEvent)
{
	check(0);
	return FTexture2DRHIRef();
}

/** Generates mip maps for the surface. */
void FOpenGLDynamicRHI::RHIGenerateMips(FRHITexture* SurfaceRHI)
{
	if (FOpenGL::SupportsGenerateMipmap())
	{
		RunOnGLRenderContextThread([this, SurfaceRHI]()
		{
			VERIFY_GL_SCOPE();
			GPUProfilingData.RegisterGPUWork(0);

			FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
			FOpenGLTexture* Texture = ResourceCast(SurfaceRHI);
			// Setup the texture on a disused unit
			// need to figure out how to setup mips properly in no views case
			CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Texture->Target, Texture->GetResource(), -1, Texture->GetNumMips());
			FOpenGL::GenerateMipmap(Texture->Target);
		});
	}
	else
	{
		UE_LOG( LogRHI, Fatal, TEXT("Generate Mipmaps unsupported on this OpenGL version"));
	}
}



/**
 * Computes the size in memory required by a given texture.
 *
 * @param	TextureRHI		- Texture we want to know the size of
 * @return					- Size in Bytes
 */
uint32 FOpenGLDynamicRHI::RHIComputeMemorySize(FRHITexture* TextureRHI)
{
	if (!TextureRHI)
	{
		return 0;
	}

	FOpenGLTexture* Texture = ResourceCast(TextureRHI);
	return Texture->MemorySize;
}

FTexture2DRHIRef FOpenGLDynamicRHI::AsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2DRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	return this->RHIAsyncReallocateTexture2D(Texture2DRHI, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
}

FTexture2DRHIRef FOpenGLDynamicRHI::RHIAsyncReallocateTexture2D(FRHITexture2D* Texture2DRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	check(IsInRenderingThread());
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

	FOpenGLTexture* OldTexture = ResourceCast(Texture2DRHI);

	FRHITextureDesc Desc = OldTexture->GetDesc();
	int32 SourceMipCount = Desc.NumMips;

	Desc.Extent = FIntPoint(NewSizeX, NewSizeY);
	Desc.NumMips = NewMipCount;

	FRHITextureCreateDesc CreateDesc(
		Desc,
		RHIGetDefaultResourceState(Desc.Flags, false),
		TEXT("RHIAsyncReallocateTexture2D")
	);

	FOpenGLTexture* NewTexture = new FOpenGLTexture(RHICmdList, CreateDesc);

	RHICmdList.EnqueueLambda([OldTexture, NewTexture, SourceMipCount, NewMipCount, RequestStatus](FRHICommandListImmediate& RHICmdList)
	{
		VERIFY_GL_SCOPE();

		// Use the GPU to asynchronously copy the old mip-maps into the new texture.
		const uint32 NumSharedMips = FMath::Min(SourceMipCount, NewMipCount);
		const uint32 SourceMipOffset = SourceMipCount - NumSharedMips;
		const uint32 DestMipOffset = NewMipCount - NumSharedMips;

		NewTexture->CloneViaCopyImage(OldTexture, NumSharedMips, SourceMipOffset, DestMipOffset);

		RequestStatus->Decrement();
	});

	return NewTexture;
}

/**
 * Returns the status of an ongoing or completed texture reallocation:
 *	TexRealloc_Succeeded	- The texture is ok, reallocation is not in progress.
 *	TexRealloc_Failed		- The texture is bad, reallocation is not in progress.
 *	TexRealloc_InProgress	- The texture is currently being reallocated async.
 *
 * @param Texture2D		- Texture to check the reallocation status for
 * @return				- Current reallocation status
 */
ETextureReallocationStatus FOpenGLDynamicRHI::RHIFinalizeAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Succeeded;
}

/**
 * Cancels an async reallocation for the specified texture.
 * This should be called for the new texture, not the original.
 *
 * @param Texture				Texture to cancel
 * @param bBlockUntilCompleted	If true, blocks until the cancellation is fully completed
 * @return						Reallocation status
 */
ETextureReallocationStatus FOpenGLDynamicRHI::RHICancelAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Succeeded;
}

void* FOpenGLDynamicRHI::RHILockTexture2D(FRHITexture2D* TextureRHI,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail, uint64* OutLockedByteCount)
{
	FOpenGLTexture* Texture = ResourceCast(TextureRHI);
	if (OutLockedByteCount)
	{
		*OutLockedByteCount = Texture->GetLockSize(MipIndex, 0, LockMode, DestStride);
	}

	return Texture->Lock(MipIndex, 0, LockMode, DestStride);
}

void FOpenGLDynamicRHI::RHIUnlockTexture2D(FRHITexture2D* TextureRHI, uint32 MipIndex, bool bLockWithinMiptail)
{
	ResourceCast(TextureRHI)->Unlock(MipIndex, 0);
}

void* FOpenGLDynamicRHI::RHILockTexture2DArray(FRHITexture2DArray* TextureRHI, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	return ResourceCast(TextureRHI)->Lock(MipIndex, TextureIndex, LockMode, DestStride);
}

void FOpenGLDynamicRHI::RHIUnlockTexture2DArray(FRHITexture2DArray* TextureRHI, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	ResourceCast(TextureRHI)->Unlock(MipIndex, TextureIndex);
}

void FOpenGLDynamicRHI::RHIUpdateTexture2D(FRHICommandListBase& RHICmdList, FRHITexture2D* TextureRHI, uint32 MipIndex, const FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{
	const FPixelFormatInfo& FormatInfo = GPixelFormats[TextureRHI->GetFormat()];

	check(UpdateRegion.Width  % FormatInfo.BlockSizeX == 0);
	check(UpdateRegion.Height % FormatInfo.BlockSizeY == 0);
	check(UpdateRegion.DestX  % FormatInfo.BlockSizeX == 0);
	check(UpdateRegion.DestY  % FormatInfo.BlockSizeY == 0);
	check(UpdateRegion.SrcX   % FormatInfo.BlockSizeX == 0);
	check(UpdateRegion.SrcY   % FormatInfo.BlockSizeY == 0);

	const uint32 SrcXInBlocks   = FMath::DivideAndRoundUp<uint32>(UpdateRegion.SrcX,   FormatInfo.BlockSizeX);
	const uint32 SrcYInBlocks   = FMath::DivideAndRoundUp<uint32>(UpdateRegion.SrcY,   FormatInfo.BlockSizeY);
	const uint32 WidthInBlocks  = FMath::DivideAndRoundUp<uint32>(UpdateRegion.Width,  FormatInfo.BlockSizeX);
	const uint32 HeightInBlocks = FMath::DivideAndRoundUp<uint32>(UpdateRegion.Height, FormatInfo.BlockSizeY);

	const void* UpdateMemory = SourceData + FormatInfo.BlockBytes * SrcXInBlocks + SourcePitch * SrcYInBlocks * FormatInfo.BlockSizeY;
	uint32 UpdatePitch = SourcePitch;

	const bool bNeedStagingMemory = !ShouldRunGLRenderContextOpOnThisThread(RHICmdList);
	if (bNeedStagingMemory)
	{
		const size_t SourceDataSizeInBlocks = static_cast<size_t>(WidthInBlocks) * static_cast<size_t>(HeightInBlocks);
		const size_t SourceDataSize = SourceDataSizeInBlocks * FormatInfo.BlockBytes;

		uint8* const StagingMemory = (uint8*)FMemory::Malloc(SourceDataSize);
		const size_t StagingPitch = static_cast<size_t>(WidthInBlocks) * FormatInfo.BlockBytes;

		const uint8* CopySrc = (const uint8*)UpdateMemory;
		uint8* CopyDst = (uint8*)StagingMemory;
		for (uint32 BlockRow = 0; BlockRow < HeightInBlocks; BlockRow++)
		{
			FMemory::Memcpy(CopyDst, CopySrc, WidthInBlocks * FormatInfo.BlockBytes);
			CopySrc += SourcePitch;
			CopyDst += StagingPitch;
		}

		UpdateMemory = StagingMemory;
		UpdatePitch = StagingPitch;
	}

	RHICmdList.EnqueueLambda([this, TextureRHI, MipIndex, UpdateRegion, UpdatePitch, UpdateMemory, bNeedStagingMemory](FRHICommandListBase&)
	{
		VERIFY_GL_SCOPE();

		FOpenGLTexture* Texture = ResourceCast(TextureRHI);
		const EPixelFormat PixelFormat = TextureRHI->GetFormat();

		const FPixelFormatInfo& FormatInfo = GPixelFormats[PixelFormat];
		const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[PixelFormat];

		// Use a texture stage that's not likely to be used for draws, to avoid waiting
		FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
		CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Texture->Target, Texture->GetResource(), 0, Texture->GetNumMips());
		CachedBindPixelUnpackBuffer(ContextState, 0);

		glPixelStorei(GL_UNPACK_ROW_LENGTH, UpdatePitch / FormatInfo.BlockBytes);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		if (GLFormat.bCompressed)
		{
			glCompressedTexSubImage2D(
				Texture->Target,
				MipIndex,
				UpdateRegion.DestX, UpdateRegion.DestY,
				UpdateRegion.Width, UpdateRegion.Height,
				GLFormat.Format,
				UpdatePitch * FMath::DivideAndRoundUp<uint32>(UpdateRegion.Height, FormatInfo.BlockSizeY),
				UpdateMemory);
		}
		else
		{
			glTexSubImage2D(
				Texture->Target,
				MipIndex,
				UpdateRegion.DestX, UpdateRegion.DestY,
				UpdateRegion.Width, UpdateRegion.Height,
				GLFormat.Format,
				GLFormat.Type,
				UpdateMemory);
		}

		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

		// No need to restore texture stage; leave it like this,
		// and the next draw will take care of cleaning it up; or
		// next operation that needs the stage will switch something else in on it.

		// free source data if we're on RHIT
		if (bNeedStagingMemory)
		{
			FMemory::Free(const_cast<void*>(UpdateMemory));
		}
	});
}

void FOpenGLDynamicRHI::RHIUpdateTexture3D(FRHICommandListBase& RHICmdList, FRHITexture3D* TextureRHI, uint32 MipIndex, const FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData)
{
	uint8* RHITSourceData = nullptr;
	if (!ShouldRunGLRenderContextOpOnThisThread(RHICmdList))
	{
		uint32 DataSize = SourceDepthPitch * UpdateRegion.Depth;
		RHITSourceData = (uint8*)FMemory::Malloc(DataSize, 16);
		FMemory::Memcpy(RHITSourceData, SourceData, DataSize);
		SourceData = RHITSourceData;
	}

	RHICmdList.EnqueueLambda([this, TextureRHI, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData, RHITSourceData](FRHICommandListBase&)
	{
		VERIFY_GL_SCOPE();
		check(FOpenGL::SupportsTexture3D());
		FOpenGLTexture* Texture = ResourceCast(TextureRHI);

		// Use a texture stage that's not likely to be used for draws, to avoid waiting
		FOpenGLContextState& ContextState = GetContextStateForCurrentContext();
		CachedSetupTextureStage(ContextState, FOpenGL::GetMaxCombinedTextureImageUnits() - 1, Texture->Target, Texture->GetResource(), 0, Texture->GetNumMips());
		CachedBindPixelUnpackBuffer(ContextState, 0);

		EPixelFormat PixelFormat = Texture->GetFormat();
		const FOpenGLTextureFormat& GLFormat = GOpenGLTextureFormats[PixelFormat];
		const FPixelFormatInfo& FormatInfo = GPixelFormats[PixelFormat];
		const uint32 FormatBPP = FormatInfo.BlockBytes;

		check(FOpenGL::SupportsTexture3D());
		// TO DO - add appropriate offsets to source data when necessary
		check(UpdateRegion.SrcX == 0);
		check(UpdateRegion.SrcY == 0);
		check(UpdateRegion.SrcZ == 0);

		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

		const bool bSRGB = EnumHasAnyFlags(Texture->GetFlags(), TexCreate_SRGB);

		if (GLFormat.bCompressed)
		{
			FOpenGL::CompressedTexSubImage3D(
				Texture->Target,
				MipIndex,
				UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.DestZ,
				UpdateRegion.Width, UpdateRegion.Height, UpdateRegion.Depth,
				GLFormat.InternalFormat[bSRGB],
				SourceDepthPitch * UpdateRegion.Depth,
				SourceData);
		}
		else
		{
			glPixelStorei(GL_UNPACK_ROW_LENGTH, UpdateRegion.Width / FormatInfo.BlockSizeX);
			glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, UpdateRegion.Height / FormatInfo.BlockSizeY);

			FOpenGL::TexSubImage3D(Texture->Target, MipIndex, UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.DestZ, UpdateRegion.Width, UpdateRegion.Height, UpdateRegion.Depth, GLFormat.Format, GLFormat.Type, SourceData);
		}

		glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
		glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
		glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

		// free source data if we're on RHIT
		if (RHITSourceData)
		{
			FMemory::Free(RHITSourceData);
		}

		// No need to restore texture stage; leave it like this,
		// and the next draw will take care of cleaning it up; or
		// next operation that needs the stage will switch something else in on it.
	});
}

void FOpenGLDynamicRHI::InvalidateTextureResourceInCache(GLuint Resource)
{
	VERIFY_GL_SCOPE();
	auto InvalidateContextTextureResources = [&Resource](TArray<FTextureStage>& Textures)
	{
		for (FTextureStage& TextureStage : Textures)
		{
			if (TextureStage.Resource == Resource)
			{
				TextureStage.Target = GL_NONE;
				TextureStage.Resource = 0;
			}
		}
	};
	InvalidateContextTextureResources(SharedContextState.Textures);
	InvalidateContextTextureResources(RenderingContextState.Textures);
	InvalidateContextTextureResources(PendingState.Textures);
	
	TextureMipLimits.Remove(Resource);
	
	if (PendingState.DepthStencil && PendingState.DepthStencil->GetResource() == Resource)
	{
		PendingState.DepthStencil = nullptr;
	}
}

void FOpenGLDynamicRHI::InvalidateUAVResourceInCache(GLuint Resource)
{
	VERIFY_GL_SCOPE();
	int32 NumUAVs = RenderingContextState.UAVs.Num();
	
	for (int32 UAVIndex = 0; UAVIndex < NumUAVs; ++UAVIndex)
	{
		if (SharedContextState.UAVs[UAVIndex].Resource == Resource)
		{
			SharedContextState.UAVs[UAVIndex].Format = GL_NONE;
			SharedContextState.UAVs[UAVIndex].Resource = 0;
		}

		if (RenderingContextState.UAVs[UAVIndex].Resource == Resource)
		{
			RenderingContextState.UAVs[UAVIndex].Format = GL_NONE;
			RenderingContextState.UAVs[UAVIndex].Resource = 0;
		}

		if (PendingState.UAVs[UAVIndex].Resource == Resource)
		{
			PendingState.UAVs[UAVIndex].Format = GL_NONE;
			PendingState.UAVs[UAVIndex].Resource = 0;
		}
	}
}

/*-----------------------------------------------------------------------------
	Cubemap texture support.
-----------------------------------------------------------------------------*/
void* FOpenGLDynamicRHI::RHILockTextureCubeFace(FRHITextureCube* TextureCubeRHI, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	return ResourceCast(TextureCubeRHI)->Lock(MipIndex, FaceIndex + 6 * ArrayIndex, LockMode, DestStride);
}

void FOpenGLDynamicRHI::RHIUnlockTextureCubeFace(FRHITextureCube* TextureCubeRHI, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	ResourceCast(TextureCubeRHI)->Unlock(MipIndex, FaceIndex + ArrayIndex * 6);
}

void FOpenGLDynamicRHI::RHIBindDebugLabelName(FRHICommandListBase& RHICmdList, FRHITexture* TextureRHI, const TCHAR* Name)
{
#if GLDEBUG_LABELS_ENABLED
	FAnsiCharArray TextureDebugName;
	TextureDebugName.Append(TCHAR_TO_ANSI(Name), FCString::Strlen(Name) + 1);
	RHICmdList.EnqueueLambda([TextureRHI, TextureDebugName = MoveTemp(TextureDebugName)] (FRHICommandListBase& RHICmdList)
	{
		VERIFY_GL_SCOPE();
		FOpenGLTexture* Texture = ResourceCast(TextureRHI);
		if (Texture->IsEvicted())
		{
			Texture->EvictionParamsPtr->SetDebugLabelName(TextureDebugName);
		}
		else
		{
			FOpenGL::LabelObject(GL_TEXTURE, Texture->GetResource(), TextureDebugName.GetData());
		}
	});
#endif
}

void FOpenGLDynamicRHI::RHICopyTexture(FRHITexture* SourceTextureRHI, FRHITexture* DestTextureRHI, const FRHICopyTextureInfo& CopyInfo)
{
	VERIFY_GL_SCOPE();
	FOpenGLTexture* SourceTexture = ResourceCast(SourceTextureRHI);
	FOpenGLTexture* DestTexture = ResourceCast(DestTextureRHI);


	GLsizei Width, Height, Depth;

	if (CopyInfo.Size == FIntVector::ZeroValue)
	{
		const FRHITextureDesc& SourceDesc = SourceTextureRHI->GetDesc();

		// Copy whole texture when zero vector is specified for region size.
		FIntVector SrcTexSize = SourceDesc.GetSize();
		Width = FMath::Max(1, SrcTexSize.X >> CopyInfo.SourceMipIndex);
		Height = FMath::Max(1, SrcTexSize.Y >> CopyInfo.SourceMipIndex);
		switch (SourceTexture->Target)
		{
		case GL_TEXTURE_3D:			Depth = FMath::Max(1, SrcTexSize.Z >> CopyInfo.SourceMipIndex); break;
		case GL_TEXTURE_CUBE_MAP:	Depth = 6; break;
		default:					Depth = 1; break;
		}
		ensure(CopyInfo.SourcePosition == FIntVector::ZeroValue);
	}
	else
	{
		Width = CopyInfo.Size.X;
		Height = CopyInfo.Size.Y;
		switch (SourceTexture->Target)
		{
			case GL_TEXTURE_3D:			Depth = CopyInfo.Size.Z; break;
			case GL_TEXTURE_CUBE_MAP:	Depth = CopyInfo.NumSlices; break;
			default:					Depth = 1; break;
		}
	}

	GLint SrcMip = CopyInfo.SourceMipIndex;
	GLint DestMip = CopyInfo.DestMipIndex;

	for (uint32 MipIndex = 0; MipIndex < CopyInfo.NumMips; ++MipIndex)
	{
		// X, Y, Z
		FIntVector Src, Dst;

		auto SetOffsets = [MipIndex, &CopyInfo, &Depth] (const GLenum Target, const FIntVector& Position, const uint32& SliceIndex, FIntVector& OutOffsets)
		{
			switch (Target)
			{
			case GL_TEXTURE_3D:
			case GL_TEXTURE_CUBE_MAP:
				// For cube maps, the Z offsets select the starting faces.
				OutOffsets.Z = Position.Z >> MipIndex;
				break;
			case GL_TEXTURE_1D_ARRAY:
			case GL_TEXTURE_2D_ARRAY:
				// For texture arrays, the Z offsets and depth actually refer to the range of slices to copy.
				OutOffsets.Z = SliceIndex;
				Depth = CopyInfo.NumSlices;
				break;
			default:
				OutOffsets.Z = 0;
				break;
			}

			OutOffsets.X = Position.X >> MipIndex;
			OutOffsets.Y = Position.Y >> MipIndex;
		};

		SetOffsets(SourceTexture->Target, CopyInfo.SourcePosition, CopyInfo.SourceSliceIndex, Src);
		SetOffsets(DestTexture->Target, CopyInfo.DestPosition, CopyInfo.DestSliceIndex, Dst);

		FOpenGL::CopyImageSubData(SourceTexture->GetResource(), SourceTexture->Target, SrcMip, Src.X, Src.Y, Src.Z,
			DestTexture->GetResource(), DestTexture->Target, DestMip, Dst.X, Dst.Y, Dst.Z,
			Width, Height, Depth);

		++SrcMip;
		++DestMip;

		Width = FMath::Max(1, Width >> 1);
		Height = FMath::Max(1, Height >> 1);
		if(DestTexture->Target == GL_TEXTURE_3D)
		{
			Depth = FMath::Max(1, Depth >> 1);
		}
	}
}


FTexture2DRHIRef FOpenGLDynamicRHI::RHICreateTexture2DFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, const FClearValueBinding& ClearValueBinding, GLuint Resource, ETextureCreateFlags TexCreateFlags)
{
	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(TEXT("RHICreateTexture2DFromResource"), SizeX, SizeY, Format)
		.SetClearValue(ClearValueBinding)
		.SetFlags(TexCreateFlags)
		.SetNumMips(NumMips)
		.SetNumSamples(NumSamples)
		.DetermineInititialState();

	return new FOpenGLTexture(Desc, Resource);
}

FTexture2DRHIRef FOpenGLDynamicRHI::RHICreateTexture2DArrayFromResource(EPixelFormat Format, uint32 SizeX, uint32 SizeY, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, const FClearValueBinding& ClearValueBinding, GLuint Resource, ETextureCreateFlags TexCreateFlags)
{
	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2DArray(TEXT("RHICreateTexture2DArrayFromResource"), SizeX, SizeY, ArraySize, Format)
		.SetClearValue(ClearValueBinding)
		.SetFlags(TexCreateFlags)
		.SetNumMips(NumMips)
		.SetNumSamples(NumSamples)
		.DetermineInititialState();

	return new FOpenGLTexture(Desc, Resource);
}

FTextureCubeRHIRef FOpenGLDynamicRHI::RHICreateTextureCubeFromResource(EPixelFormat Format, uint32 Size, bool bArray, uint32 ArraySize, uint32 NumMips, uint32 NumSamples, uint32 NumSamplesTileMem, const FClearValueBinding& ClearValueBinding, GLuint Resource, ETextureCreateFlags TexCreateFlags)
{
	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create(TEXT("RHICreateTextureCubeFromResource"), bArray ? ETextureDimension::TextureCube : ETextureDimension::TextureCubeArray)
		.SetExtent(Size)
		.SetArraySize(bArray ? ArraySize : 1)
		.SetFormat(Format)
		.SetClearValue(ClearValueBinding)
		.SetFlags(TexCreateFlags)
		.SetNumMips(NumMips)
		.SetNumSamples(NumSamples)
		.DetermineInititialState();

	return new FOpenGLTexture(Desc, Resource);
}

void FOpenGLDynamicRHI::RHIAliasTextureResources(FTextureRHIRef& DestRHITexture, FTextureRHIRef& SrcRHITexture)
{
	VERIFY_GL_SCOPE();
	FOpenGLTexture* DestTexture = ResourceCast(DestRHITexture);
	FOpenGLTexture* SrcTexture = ResourceCast(SrcRHITexture);

	if (DestTexture && SrcTexture)
	{
		DestTexture->AliasResources(*SrcTexture);
	}
}

FTextureRHIRef FOpenGLDynamicRHI::RHICreateAliasedTexture(FTextureRHIRef& SourceTexture)
{
	const FString Name = SourceTexture->GetName().ToString() + TEXT("Alias");
	return new FOpenGLTexture(*ResourceCast(SourceTexture), *Name, FOpenGLTexture::AliasResource);
}

void* FOpenGLDynamicRHI::LockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush, uint64* OutLockedByteCount)
{
	check(IsInRenderingThread());
	static auto* CVarRHICmdBufferWriteLocks = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RHICmdBufferWriteLocks"));
	bool bBuffer = CVarRHICmdBufferWriteLocks->GetValueOnRenderThread() > 0;
	void* Result;
	uint32 MipBytes = 0;
	if (!bBuffer || LockMode != RLM_WriteOnly || RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		RHITHREAD_GLCOMMAND_PROLOGUE();
		return this->RHILockTexture2D(Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail, OutLockedByteCount);
		RHITHREAD_GLCOMMAND_EPILOGUE_GET_RETURN(void *);
		Result = ReturnValue;
		MipBytes = ResourceCast(Texture)->GetLockSize(MipIndex, 0, LockMode, DestStride);
	}
	else
	{
		MipBytes = ResourceCast(Texture)->GetLockSize(MipIndex, 0, LockMode, DestStride);
		Result = FMemory::Malloc(MipBytes, 16);
	}
	check(Result);

	if (OutLockedByteCount)
	{
		*OutLockedByteCount = MipBytes;
	}

	GLLockTracker.Lock(Texture, Result, 0, MipIndex, DestStride, MipBytes, LockMode);
	return Result;
}

void FOpenGLDynamicRHI::UnlockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush)
{
	check(IsInRenderingThread());
	static auto* CVarRHICmdBufferWriteLocks = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RHICmdBufferWriteLocks"));
	bool bBuffer = CVarRHICmdBufferWriteLocks->GetValueOnRenderThread() > 0;
	FTextureLockTracker::FLockParams Params = GLLockTracker.Unlock(Texture, 0, MipIndex);
	if (!bBuffer || Params.LockMode != RLM_WriteOnly || RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		GLLockTracker.TotalMemoryOutstanding = 0;
		RHITHREAD_GLCOMMAND_PROLOGUE();
		this->RHIUnlockTexture2D(Texture, MipIndex, bLockWithinMiptail);
		RHITHREAD_GLCOMMAND_EPILOGUE();
	}
	else
	{
		auto GLCommand = [this, Params, Texture, MipIndex, bLockWithinMiptail]()
		{
			uint32 DestStride;
			uint64 LockedByteCount = ~0ULL;
			uint8* TexMem = (uint8*)this->RHILockTexture2D(Texture, MipIndex, Params.LockMode, DestStride, bLockWithinMiptail, &LockedByteCount);
			check(LockedByteCount != ~0ULL);
			uint8* BuffMem = (uint8*)Params.Buffer;
			check(DestStride == Params.Stride);
			check(LockedByteCount >= Params.BufferSize);
			FMemory::Memcpy(TexMem, BuffMem, Params.BufferSize);
			FMemory::Free(Params.Buffer);
			this->RHIUnlockTexture2D(Texture, MipIndex, bLockWithinMiptail);
		};
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)(MoveTemp(GLCommand));
	}
}

void* FOpenGLDynamicRHI::RHILockTextureCubeFace_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	check(IsInRenderingThread());
	static auto* CVarRHICmdBufferWriteLocks = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RHICmdBufferWriteLocks"));
	bool bBuffer = CVarRHICmdBufferWriteLocks->GetValueOnRenderThread() > 0;
	void* Result;
	uint32 MipBytes = 0;
	if (!bBuffer || LockMode != RLM_WriteOnly || RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		RHITHREAD_GLCOMMAND_PROLOGUE();
		return this->RHILockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
		RHITHREAD_GLCOMMAND_EPILOGUE_GET_RETURN(void *);
		Result = ReturnValue;
		MipBytes = ResourceCast(Texture)->GetLockSize(MipIndex, 0, LockMode, DestStride);
	}
	else
	{
		MipBytes = ResourceCast(Texture)->GetLockSize(MipIndex, 0, LockMode, DestStride);
		Result = FMemory::Malloc(MipBytes, 16);
	}
	check(Result);
	GLLockTracker.Lock(Texture, Result, ArrayIndex, MipIndex, DestStride, MipBytes, LockMode);
	return Result;
}

void FOpenGLDynamicRHI::RHIUnlockTextureCubeFace_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITextureCube* Texture, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	check(IsInRenderingThread());
	static auto* CVarRHICmdBufferWriteLocks = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RHICmdBufferWriteLocks"));
	bool bBuffer = CVarRHICmdBufferWriteLocks->GetValueOnRenderThread() > 0;
	FTextureLockTracker::FLockParams Params = GLLockTracker.Unlock(Texture, ArrayIndex, MipIndex);
	if (!bBuffer || Params.LockMode != RLM_WriteOnly || RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		GLLockTracker.TotalMemoryOutstanding = 0;
		RHITHREAD_GLCOMMAND_PROLOGUE();
		this->RHIUnlockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, bLockWithinMiptail);
		RHITHREAD_GLCOMMAND_EPILOGUE();
	}
	else
	{
		auto GLCommand = [this, Params, Texture, FaceIndex, ArrayIndex, MipIndex, bLockWithinMiptail]()
		{
			uint32 DestStride;
			uint8* TexMem = (uint8*)this->RHILockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, RLM_WriteOnly, DestStride, bLockWithinMiptail);
			uint8* BuffMem = (uint8*)Params.Buffer;
			check(DestStride == Params.Stride);
			FMemory::Memcpy(TexMem, BuffMem, Params.BufferSize);
			FMemory::Free(Params.Buffer);
			this->RHIUnlockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, bLockWithinMiptail);
		};
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)(MoveTemp(GLCommand));
	}
}

void* FOpenGLDynamicRHI::LockTexture2DArray_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2DArray* Texture, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	check(IsInRenderingThread());
	static auto* CVarRHICmdBufferWriteLocks = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RHICmdBufferWriteLocks"));
	bool bBuffer = CVarRHICmdBufferWriteLocks->GetValueOnRenderThread() > 0;
	void* Result;
	uint32 MipBytes = 0;
	if (!bBuffer || LockMode != RLM_WriteOnly || RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		RHITHREAD_GLCOMMAND_PROLOGUE();
		return this->RHILockTexture2DArray(Texture, ArrayIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
		RHITHREAD_GLCOMMAND_EPILOGUE_GET_RETURN(void*);
		Result = ReturnValue;
		MipBytes = ResourceCast(Texture)->GetLockSize(MipIndex, ArrayIndex, LockMode, DestStride);
	}
	else
	{
		MipBytes = ResourceCast(Texture)->GetLockSize(MipIndex, ArrayIndex, LockMode, DestStride);
		Result = FMemory::Malloc(MipBytes, 16);
	}
	check(Result);

	GLLockTracker.Lock(Texture, Result, ArrayIndex, MipIndex, DestStride, MipBytes, LockMode);
	return Result;
}

void FOpenGLDynamicRHI::UnlockTexture2DArray_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2DArray* Texture, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	check(IsInRenderingThread());
	static auto* CVarRHICmdBufferWriteLocks = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.RHICmdBufferWriteLocks"));
	bool bBuffer = CVarRHICmdBufferWriteLocks->GetValueOnRenderThread() > 0;
	FTextureLockTracker::FLockParams Params = GLLockTracker.Unlock(Texture, ArrayIndex, MipIndex);
	if (!bBuffer || Params.LockMode != RLM_WriteOnly || RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		GLLockTracker.TotalMemoryOutstanding = 0;
		RHITHREAD_GLCOMMAND_PROLOGUE();
		this->RHIUnlockTexture2DArray(Texture, ArrayIndex, MipIndex, bLockWithinMiptail);
		RHITHREAD_GLCOMMAND_EPILOGUE();
	}
	else
	{
		auto GLCommand = [this, Params, Texture, ArrayIndex, MipIndex, bLockWithinMiptail]()
		{
			uint32 DestStride;
			uint8* TexMem = (uint8*)this->RHILockTexture2DArray(Texture, ArrayIndex, MipIndex, Params.LockMode, DestStride, bLockWithinMiptail);
			uint8* BuffMem = (uint8*)Params.Buffer;
			check(DestStride == Params.Stride);
			FMemory::Memcpy(TexMem, BuffMem, Params.BufferSize);
			FMemory::Free(Params.Buffer);
			this->RHIUnlockTexture2DArray(Texture, ArrayIndex, MipIndex, bLockWithinMiptail);
		};
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandGLCommand)(MoveTemp(GLCommand));
	}
}

void LogTextureEvictionDebugInfo()
{
	static int counter = 0;
	if (GOGLTextureEvictLogging && ++counter == 100)
	{
		UE_LOG(LogRHI, Warning, TEXT("txdbg: Texture mipmem %d. GTotalTexStorageSkipped %d, GTotalCompressedTexStorageSkipped %d, Total noncompressed = %d"), GTotalEvictedMipMemStored, GTotalTexStorageSkipped, GTotalCompressedTexStorageSkipped, GTotalTexStorageSkipped - GTotalCompressedTexStorageSkipped);
		UE_LOG(LogRHI, Warning, TEXT("txdbg: Texture GTotalEvictedMipMemDuplicated %d"), GTotalEvictedMipMemDuplicated);
		UE_LOG(LogRHI, Warning, TEXT("txdbg: Texture GTotalMipRestores %d, GTotalMipStoredCount %d"), GTotalMipRestores, GTotalMipStoredCount);
		UE_LOG(LogRHI, Warning, TEXT("txdbg: Texture GAvgRestoreTime %f (%d), GMaxRestoreTime %f, TotalRestoreTime %f"), GAvgRestoreCount ? (float)(GAvgRestoreTime / GAvgRestoreCount) : 0.f, GAvgRestoreCount, (float)GMaxRestoreTime, (float)GAvgRestoreTime);
		UE_LOG(LogRHI, Warning, TEXT("txdbg: Texture LRU %d"), FTextureEvictionLRU::Get().Num());

		GAvgRestoreCount = 0;
		GMaxRestoreTime = 0;
		GAvgRestoreTime = 0;

		counter = 0;
	}
}

void FTextureEvictionLRU::TickEviction()
{
#if (UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT)
	LogTextureEvictionDebugInfo();
#endif

	FScopeLock Lock(&TextureLRULock);
	FOpenGLTextureLRUContainer& TextureLRU = GetLRUContainer();

	for (int32 EvictCount = 0; 
		TextureLRU.Num() > FMath::Max(0, GOGLTextureMinLRUCapacity) && (TextureLRU.GetLeastRecent()->EvictionParamsPtr->FrameLastRendered + GOGLTextureEvictFramesToLive) < GFrameNumberRenderThread && EvictCount < GOGLTexturesToEvictPerFrame
		;EvictCount++)
	{
		FOpenGLTexture* RemovedFromLRU = TextureLRU.RemoveLeastRecent();
		RemovedFromLRU->EvictionParamsPtr->LRUNode = FSetElementId();
		RemovedFromLRU->TryEvictGLResource();
	}
}

void FTextureEvictionLRU::Remove(FOpenGLTexture* TextureBase)
{
	if( TextureBase->EvictionParamsPtr.IsValid() )
	{
		FScopeLock Lock(&TextureLRULock);

		check(!TextureBase->EvictionParamsPtr->LRUNode.IsValidId() || GetLRUContainer().Contains(TextureBase));
		check(TextureBase->EvictionParamsPtr->LRUNode.IsValidId() || !GetLRUContainer().Contains(TextureBase));
		if( TextureBase->EvictionParamsPtr->LRUNode.IsValidId())
		{
			GetLRUContainer().Remove(TextureBase);
			TextureBase->EvictionParamsPtr->LRUNode = FSetElementId();
		}
	}
}

bool FTextureEvictionLRU::Add(FOpenGLTexture* TextureBase)
{
	FScopeLock Lock(&TextureLRULock); 
	check(TextureBase->EvictionParamsPtr);
	check(!TextureBase->EvictionParamsPtr->LRUNode.IsValidId())
	FOpenGLTextureLRUContainer& TextureLRU = GetLRUContainer();
	check(!TextureLRU.Contains(TextureBase));

	if(ensure(TextureLRU.Num() != TextureLRU.Max()))
	{
		TextureBase->EvictionParamsPtr->LRUNode = TextureLRU.Add(TextureBase, TextureBase);
		TextureBase->EvictionParamsPtr->FrameLastRendered = GFrameNumberRenderThread;
		return true;
	}
	return false;
}

void FTextureEvictionLRU::Touch(FOpenGLTexture* TextureBase)
{
	FScopeLock Lock(&TextureLRULock);
	check(TextureBase->EvictionParamsPtr);
	check(TextureBase->EvictionParamsPtr->LRUNode.IsValidId())
	check(GetLRUContainer().Contains(TextureBase));
	GetLRUContainer().MarkAsRecent(TextureBase->EvictionParamsPtr->LRUNode);
	TextureBase->EvictionParamsPtr->FrameLastRendered = GFrameNumberRenderThread;
}

FOpenGLTexture* FTextureEvictionLRU::GetLeastRecent()
{
	return GetLRUContainer().GetLeastRecent();
}


FTextureEvictionParams::FTextureEvictionParams(uint32 NumMips) : bHasRestored(0), FrameLastRendered(0)
{
	MipImageData.Reserve(NumMips);
	MipImageData.SetNum(NumMips);
}

FTextureEvictionParams::~FTextureEvictionParams()
{
	VERIFY_GL_SCOPE();

	if (bHasRestored)
	{
		GTotalEvictedMipMemDuplicated -= GetTotalAllocated();
	}

	for (int i = MipImageData.Num() - 1; i >= 0; i--)
	{
		GTotalEvictedMipMemStored -= MipImageData[i].Num();
	}
	GTotalMipStoredCount -= MipImageData.Num();
}

void FTextureEvictionParams::SetMipData(uint32 MipIndex, const void* Data, uint32 Bytes)
{
	checkf(Bytes, TEXT("FTextureEvictionParams::SetMipData: MipIndex %d, Data %p, Bytes %d)"), MipIndex, Data, Bytes);

	VERIFY_GL_SCOPE();
	if (MipImageData[MipIndex].Num())
	{
		// already have data??
		checkNoEntry();
	}
	else
	{
		GTotalMipStoredCount++;
	}
	MipImageData[MipIndex].Reserve(Bytes);
	MipImageData[MipIndex].SetNumUninitialized(Bytes);
	if (Data)
	{
		FMemory::Memcpy(MipImageData[MipIndex].GetData(), Data, Bytes);
	}
	GTotalEvictedMipMemStored += Bytes;
}

void FTextureEvictionParams::CloneMipData(const FTextureEvictionParams& Src, uint32 InNumMips, int32 SrcOffset, int DstOffset)
{
	VERIFY_GL_SCOPE();

	int32 MaxMip = FMath::Min((int32)InNumMips, (int32)Src.MipImageData.Num() - SrcOffset);
	for (int32 MipIndex = 0; MipIndex < MaxMip; ++MipIndex)
	{
		if (MipImageData[MipIndex + DstOffset].Num())
		{
			checkNoEntry();
		}
		else
		{
			GTotalMipStoredCount++;
		}
		MipImageData[MipIndex + DstOffset] = Src.MipImageData[MipIndex + SrcOffset];
		GTotalEvictedMipMemStored += MipImageData[MipIndex + DstOffset].Num();
	}
}

void FTextureEvictionParams::ReleaseMipData(uint32 RetainMips)
{
	VERIFY_GL_SCOPE();

	for (int i = MipImageData.Num() - 1 - RetainMips; i >= 0; i--)
	{
		GTotalEvictedMipMemStored -= MipImageData[i].Num();
		GTotalMipStoredCount -= MipImageData[i].Num() ? 1 : 0;
		MipImageData[i].Empty();
	}

	// if we're retaining mips then keep entire MipImageData array to ensure there's no MipIndex confusion.
	if (RetainMips == 0)
	{
		MipImageData.Empty();
	}
}
