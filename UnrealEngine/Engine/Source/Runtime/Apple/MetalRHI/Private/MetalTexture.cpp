// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 MetalTexture.cpp: Metal texture RHI implementation.
 =============================================================================*/

#include "MetalRHIPrivate.h"
#include "MetalProfiler.h" // for STAT_MetalTexturePageOffTime
#include "MetalCommandBuffer.h"
#include "RenderUtils.h"
#include "Containers/ResourceArray.h"
#include "Misc/ScopeRWLock.h"
#include "MetalLLM.h"

volatile int64 FMetalSurface::ActiveUploads = 0;

int32 GMetalMaxOutstandingAsyncTexUploads = 100 * 1024 * 1024;
FAutoConsoleVariableRef CVarMetalMaxOutstandingAsyncTexUploads(
															   TEXT("rhi.Metal.MaxOutstandingAsyncTexUploads"),
															   GMetalMaxOutstandingAsyncTexUploads,
															   TEXT("The maximum number of outstanding asynchronous texture uploads allowed to be pending in Metal. After the limit is reached the next upload will wait for all outstanding operations to complete and purge the waiting free-lists in order to reduce peak memory consumption. Defaults to 0 (infinite), set to a value > 0 limit the number."),
															   ECVF_ReadOnly|ECVF_RenderThreadSafe
															   );

int32 GMetalForceIOSTexturesShared = 1;
FAutoConsoleVariableRef CVarMetalForceIOSTexturesShared(
														TEXT("rhi.Metal.ForceIOSTexturesShared"),
														GMetalForceIOSTexturesShared,
														TEXT("If true, forces all textures to be Shared on iOS"),
														ECVF_RenderThreadSafe);

/** Given a pointer to a RHI texture that was created by the Metal RHI, returns a pointer to the FMetalTextureBase it encapsulates. */
FMetalSurface* GetMetalSurfaceFromRHITexture(FRHITexture* Texture)
{
	return Texture
		? static_cast<FMetalSurface*>(Texture->GetTextureBaseRHI())
		: nullptr;
}

static bool IsRenderTarget(ETextureCreateFlags Flags)
{
	return EnumHasAnyFlags(Flags, TexCreate_RenderTargetable | TexCreate_ResolveTargetable | TexCreate_DepthStencilTargetable | TexCreate_DepthStencilResolveTarget);
}

static mtlpp::TextureUsage ConvertFlagsToUsage(ETextureCreateFlags Flags)
{
	NSUInteger Usage = mtlpp::TextureUsage::Unknown;
    if (EnumHasAnyFlags(Flags, TexCreate_ShaderResource|TexCreate_ResolveTargetable|TexCreate_DepthStencilTargetable))
	{
		Usage |= mtlpp::TextureUsage::ShaderRead;
		Usage |= mtlpp::TextureUsage::PixelFormatView;
	}
	
	if (EnumHasAnyFlags(Flags, TexCreate_UAV))
	{
		Usage |= mtlpp::TextureUsage::ShaderRead;
		Usage |= mtlpp::TextureUsage::ShaderWrite;
		Usage |= mtlpp::TextureUsage::PixelFormatView;
	}
	
	// offline textures are normal shader read textures
	if (EnumHasAnyFlags(Flags, TexCreate_OfflineProcessed))
	{
		Usage |= mtlpp::TextureUsage::ShaderRead;
	}

	//if the high level is doing manual resolves then the textures specifically markes as resolve targets
	//are likely to be used in a manual shader resolve by the high level and must be bindable as rendertargets.
	const bool bSeparateResolveTargets = FMetalCommandQueue::SupportsSeparateMSAAAndResolveTarget();
	const bool bResolveTarget = EnumHasAnyFlags(Flags, TexCreate_ResolveTargetable);
	if (EnumHasAnyFlags(Flags, TexCreate_RenderTargetable|TexCreate_DepthStencilTargetable|TexCreate_DepthStencilResolveTarget) || (bResolveTarget && bSeparateResolveTargets))
	{
		Usage |= mtlpp::TextureUsage::RenderTarget;
		Usage |= mtlpp::TextureUsage::ShaderRead;
	}
	return (mtlpp::TextureUsage)Usage;
}

static bool IsPixelFormatCompressed(EPixelFormat Format)
{
	switch (Format)
	{
		case PF_DXT1:
		case PF_DXT3:
		case PF_DXT5:
		case PF_PVRTC2:
		case PF_PVRTC4:
		case PF_BC4:
		case PF_BC5:
		case PF_ETC2_RGB:
		case PF_ETC2_RGBA:
		case PF_ASTC_4x4:
		case PF_ASTC_6x6:
		case PF_ASTC_8x8:
		case PF_ASTC_10x10:
		case PF_ASTC_12x12:
		case PF_BC6H:
		case PF_BC7:
			return true;
		default:
			return false;
	}
}

static bool IsPixelFormatASTCCompressed(EPixelFormat Format)
{
	switch (Format)
	{
		case PF_ASTC_4x4:
		case PF_ASTC_6x6:
		case PF_ASTC_8x8:
		case PF_ASTC_10x10:
		case PF_ASTC_12x12:
			return true;
		default:
			return false;
	}
}

static bool IsPixelFormatPVRTCCompressed(EPixelFormat Format)
{
	switch (Format)
	{
		case PF_PVRTC2:
		case PF_PVRTC4:
		case PF_ETC2_RGB:
		case PF_ETC2_RGBA:
			return true;
		default:
			return false;
	}
}

void SafeReleaseMetalTexture(FMetalSurface* Surface, FMetalTexture& Texture, bool bAVFoundationTexture)
{
	if(GIsMetalInitialized && GDynamicRHI)
	{
		if (!bAVFoundationTexture)
		{
			GetMetalDeviceContext().ReleaseTexture(Surface, Texture);
		}
		else
		{
			SafeReleaseMetalObject([Texture.GetPtr() retain]);
		}
	}
}

void SafeReleaseMetalTexture(FMetalSurface* Surface, FMetalTexture& Texture)
{
	if(GIsMetalInitialized && GDynamicRHI)
	{
		GetMetalDeviceContext().ReleaseTexture(Surface, Texture);
	}
}

#if PLATFORM_MAC
mtlpp::PixelFormat ToSRGBFormat_NonAppleMacGPU(mtlpp::PixelFormat MTLFormat)
{
	// Expand as R8_sRGB is Apple Silicon only.
	if (MTLFormat == mtlpp::PixelFormat::R8Unorm)
	{
		MTLFormat = mtlpp::PixelFormat::RGBA8Unorm;
	}

	switch (MTLFormat)
	{
		case mtlpp::PixelFormat::RGBA8Unorm:
			MTLFormat = mtlpp::PixelFormat::RGBA8Unorm_sRGB;
			break;
		case mtlpp::PixelFormat::BGRA8Unorm:
			MTLFormat = mtlpp::PixelFormat::BGRA8Unorm_sRGB;
			break;
		case mtlpp::PixelFormat::BC1_RGBA:
			MTLFormat = mtlpp::PixelFormat::BC1_RGBA_sRGB;
			break;
		case mtlpp::PixelFormat::BC2_RGBA:
			MTLFormat = mtlpp::PixelFormat::BC2_RGBA_sRGB;
			break;
		case mtlpp::PixelFormat::BC3_RGBA:
			MTLFormat = mtlpp::PixelFormat::BC3_RGBA_sRGB;
			break;
		case mtlpp::PixelFormat::BC7_RGBAUnorm:
			MTLFormat = mtlpp::PixelFormat::BC7_RGBAUnorm_sRGB;
			break;
		default:
			break;
	}
	return MTLFormat;
}
#endif

mtlpp::PixelFormat ToSRGBFormat_AppleGPU(mtlpp::PixelFormat MTLFormat)
{
	switch (MTLFormat)
	{
		case mtlpp::PixelFormat::RGBA8Unorm:
			MTLFormat = mtlpp::PixelFormat::RGBA8Unorm_sRGB;
			break;
		case mtlpp::PixelFormat::BGRA8Unorm:
			MTLFormat = mtlpp::PixelFormat::BGRA8Unorm_sRGB;
			break;
		case mtlpp::PixelFormat::R8Unorm:
			MTLFormat = mtlpp::PixelFormat::R8Unorm_sRGB;
			break;
		case mtlpp::PixelFormat::PVRTC_RGBA_2BPP:
			MTLFormat = mtlpp::PixelFormat::PVRTC_RGBA_2BPP_sRGB;
			break;
		case mtlpp::PixelFormat::PVRTC_RGBA_4BPP:
			MTLFormat = mtlpp::PixelFormat::PVRTC_RGBA_4BPP_sRGB;
			break;
		case mtlpp::PixelFormat::ASTC_4x4_LDR:
			MTLFormat = mtlpp::PixelFormat::ASTC_4x4_sRGB;
			break;
		case mtlpp::PixelFormat::ASTC_6x6_LDR:
			MTLFormat = mtlpp::PixelFormat::ASTC_6x6_sRGB;
			break;
		case mtlpp::PixelFormat::ASTC_8x8_LDR:
			MTLFormat = mtlpp::PixelFormat::ASTC_8x8_sRGB;
			break;
		case mtlpp::PixelFormat::ASTC_10x10_LDR:
			MTLFormat = mtlpp::PixelFormat::ASTC_10x10_sRGB;
			break;
		case mtlpp::PixelFormat::ASTC_12x12_LDR:
			MTLFormat = mtlpp::PixelFormat::ASTC_12x12_sRGB;
			break;
#if PLATFORM_MAC
		// Fix for Apple silicon M1 macs that can support BC pixel formats even though they are Apple family GPUs.
		case mtlpp::PixelFormat::BC1_RGBA:
			MTLFormat = mtlpp::PixelFormat::BC1_RGBA_sRGB;
			break;
		case mtlpp::PixelFormat::BC2_RGBA:
			MTLFormat = mtlpp::PixelFormat::BC2_RGBA_sRGB;
			break;
		case mtlpp::PixelFormat::BC3_RGBA:
			MTLFormat = mtlpp::PixelFormat::BC3_RGBA_sRGB;
			break;
		case mtlpp::PixelFormat::BC7_RGBAUnorm:
			MTLFormat = mtlpp::PixelFormat::BC7_RGBAUnorm_sRGB;
			break;
#endif
		default:
			break;
	}
	return MTLFormat;
}

mtlpp::PixelFormat ToSRGBFormat(mtlpp::PixelFormat MTLFormat)
{
	if([GetMetalDeviceContext().GetDevice().GetPtr() supportsFamily:MTLGPUFamilyApple1])
	{
		MTLFormat = ToSRGBFormat_AppleGPU(MTLFormat);
	}
#if PLATFORM_MAC
	else if([GetMetalDeviceContext().GetDevice().GetPtr() supportsFamily:MTLGPUFamilyMac1])
	{
		MTLFormat = ToSRGBFormat_NonAppleMacGPU(MTLFormat);
	}
#endif
	
	return MTLFormat;
}

static inline uint32 ComputeLockIndex(uint32 MipIndex, uint32 ArrayIndex)
{
	check(MipIndex < MAX_uint16);
	check(ArrayIndex < MAX_uint16);
	return (MipIndex & MAX_uint16) | ((ArrayIndex & MAX_uint16) << 16);
}

void FMetalSurface::PrepareTextureView()
{
	// Recreate the texture to enable MTLTextureUsagePixelFormatView which must be off unless we definitely use this feature or we are throwing ~4% performance vs. Windows on the floor.
	mtlpp::TextureUsage Usage = (mtlpp::TextureUsage)Texture.GetUsage();
	bool bMemoryLess = false;
#if PLATFORM_IOS
	bMemoryLess = (Texture.GetStorageMode() == mtlpp::StorageMode::Memoryless);
#endif
	if(!(Usage & mtlpp::TextureUsage::PixelFormatView) && !bMemoryLess)
	{
		check(ImageSurfaceRef == nullptr);
		
		check(Texture);
		const bool bMSAATextureIsTexture = MSAATexture == Texture;
		const bool bMSAAResolveTextureIsTexture = MSAAResolveTexture == Texture;
		if (MSAATexture && !bMSAATextureIsTexture)
		{
			FMetalTexture OldMSAATexture = MSAATexture;
			MSAATexture = Reallocate(MSAATexture, mtlpp::TextureUsage::PixelFormatView);
			SafeReleaseMetalTexture(this, OldMSAATexture, ImageSurfaceRef != nullptr);
		}
		if (MSAAResolveTexture && !bMSAAResolveTextureIsTexture)
		{
			FMetalTexture OldMSAAResolveTexture = MSAAResolveTexture;
			MSAAResolveTexture = Reallocate(MSAAResolveTexture, mtlpp::TextureUsage::PixelFormatView);
			SafeReleaseMetalTexture(this, OldMSAAResolveTexture, ImageSurfaceRef != nullptr);
		}
		
		FMetalTexture OldTexture = Texture;
		Texture = Reallocate(Texture, mtlpp::TextureUsage::PixelFormatView);
		SafeReleaseMetalTexture(this, OldTexture, ImageSurfaceRef != nullptr);
		
		if (bMSAATextureIsTexture)
		{
			MSAATexture = Texture;
		}
		if (bMSAAResolveTextureIsTexture)
		{
			MSAAResolveTexture = Texture;
		}
	}
}

FMetalTexture FMetalSurface::Reallocate(FMetalTexture InTexture, mtlpp::TextureUsage UsageModifier)
{
	mtlpp::TextureDescriptor Desc;
	Desc.SetTextureType(InTexture.GetTextureType());
	Desc.SetPixelFormat(InTexture.GetPixelFormat());
	Desc.SetWidth(InTexture.GetWidth());
	Desc.SetHeight(InTexture.GetHeight());
	Desc.SetDepth(InTexture.GetDepth());
	Desc.SetMipmapLevelCount(InTexture.GetMipmapLevelCount());
	Desc.SetSampleCount(InTexture.GetSampleCount());
	Desc.SetArrayLength(InTexture.GetArrayLength());
	
	static mtlpp::ResourceOptions GeneralResourceOption = (mtlpp::ResourceOptions)FMetalCommandQueue::GetCompatibleResourceOptions(mtlpp::ResourceOptions::HazardTrackingModeUntracked);
	
	Desc.SetResourceOptions(mtlpp::ResourceOptions(((NSUInteger)InTexture.GetCpuCacheMode() << mtlpp::ResourceCpuCacheModeShift) | ((NSUInteger)Texture.GetStorageMode() << mtlpp::ResourceStorageModeShift) | GeneralResourceOption));
	Desc.SetCpuCacheMode(InTexture.GetCpuCacheMode());
	Desc.SetStorageMode(InTexture.GetStorageMode());
	Desc.SetUsage(mtlpp::TextureUsage(InTexture.GetUsage() | UsageModifier));
	
	FMetalTexture NewTex = GetMetalDeviceContext().CreateTexture(this, Desc);
	check(NewTex);
	return NewTex;
}

void FMetalSurface::MakeAliasable(void)
{
	check(ImageSurfaceRef == nullptr);
	
	static bool bSupportsHeaps = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesHeaps);
	if (bSupportsHeaps && Texture.GetStorageMode() == mtlpp::StorageMode::Private && Texture.GetHeap())
	{
		if (MSAATexture && (MSAATexture != Texture) && !MSAATexture.IsAliasable())
		{
			MSAATexture.MakeAliasable();
#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
			MetalLLM::LogAliasTexture(MSAATexture);
#endif
		}
		if (!Texture.IsAliasable())
		{
			Texture.MakeAliasable();
#if STATS || ENABLE_LOW_LEVEL_MEM_TRACKER
			MetalLLM::LogAliasTexture(Texture);
#endif
		}
	}
}

uint8 GetMetalPixelFormatKey(mtlpp::PixelFormat Format)
{
	struct FMetalPixelFormatKeyMap
	{
		FRWLock Mutex;
		uint8 NextKey = 1; // 0 is reserved for mtlpp::PixelFormat::Invalid
		TMap<uint64, uint8> Map;

		uint8 Get(mtlpp::PixelFormat Format)
		{
			FRWScopeLock Lock(Mutex, SLT_ReadOnly);
			uint8* Key = Map.Find((uint64)Format);
			if (Key == nullptr)
			{
				Lock.ReleaseReadOnlyLockAndAcquireWriteLock_USE_WITH_CAUTION();
				Key = Map.Find((uint64)Format);
				if (Key == nullptr)
				{
					Key = &Map.Add((uint64)Format, NextKey++);
					// only giving 6 bits to the key
					checkf(NextKey < 64, TEXT("Too many unique pixel formats to fit into the PipelineStateHash"));
				}
			}
			return *Key;
		}

		FMetalPixelFormatKeyMap()
		{
			// Add depth stencil formats first, so we don't have to use 6 bits for them in the pipeline hash
			Get(mtlpp::PixelFormat::Depth32Float);
			Get(mtlpp::PixelFormat::Stencil8);
			Get(mtlpp::PixelFormat::Depth32Float_Stencil8);
	#if PLATFORM_MAC
			Get(mtlpp::PixelFormat::Depth24Unorm_Stencil8);
			Get(mtlpp::PixelFormat::Depth16Unorm);
	#endif
		}
	} static PixelFormatKeyMap;

	return PixelFormatKeyMap.Get(Format);
}

FMetalTextureCreateDesc::FMetalTextureCreateDesc(FRHITextureCreateDesc const& InDesc)
	: FRHITextureCreateDesc(InDesc)
	, bIsRenderTarget(IsRenderTarget(InDesc.Flags))
{
	MTLFormat = (mtlpp::PixelFormat)GPixelFormats[InDesc.Format].PlatformFormat;
	if (EnumHasAnyFlags(InDesc.Flags, TexCreate_SRGB))
	{
		MTLFormat = ToSRGBFormat(MTLFormat);
	}

	// get a unique key for this surface's format
	FormatKey = GetMetalPixelFormatKey(MTLFormat);

	if (InDesc.IsTextureCube())
	{
		Desc = mtlpp::TextureDescriptor::TextureCubeDescriptor(MTLFormat, InDesc.Extent.X, (InDesc.NumMips > 1));
	}
	else if (InDesc.IsTexture3D())
	{
		Desc.SetTextureType(mtlpp::TextureType::Texture3D);
		Desc.SetWidth(InDesc.Extent.X);
		Desc.SetHeight(InDesc.Extent.Y);
		Desc.SetDepth(InDesc.Depth);
		Desc.SetPixelFormat(MTLFormat);
		Desc.SetArrayLength(1);
		Desc.SetMipmapLevelCount(1);
		Desc.SetSampleCount(1);
	}
	else
	{
		Desc = mtlpp::TextureDescriptor::Texture2DDescriptor(MTLFormat, InDesc.Extent.X, InDesc.Extent.Y, (InDesc.NumMips > 1));
		Desc.SetArrayLength(InDesc.ArraySize);
	}

	// flesh out the descriptor
	if (InDesc.IsTextureArray())
	{
		Desc.SetArrayLength(InDesc.ArraySize);
		if (InDesc.IsTextureCube())
		{
			if (FMetalCommandQueue::SupportsFeature(EMetalFeaturesCubemapArrays))
			{
				Desc.SetTextureType(mtlpp::TextureType::TextureCubeArray);
			}
			else
			{
				Desc.SetTextureType(mtlpp::TextureType::Texture2DArray);
				Desc.SetArrayLength(InDesc.ArraySize * 6);
			}
		}
		else
		{
			Desc.SetTextureType(mtlpp::TextureType::Texture2DArray);
		}
	}
	Desc.SetMipmapLevelCount(InDesc.NumMips);

	{
		Desc.SetUsage(ConvertFlagsToUsage(InDesc.Flags));
		
		const bool bAppleGPU = [GetMetalDeviceContext().GetDevice().GetPtr() supportsFamily:MTLGPUFamilyApple1];

		if (EnumHasAnyFlags(InDesc.Flags, TexCreate_CPUReadback) && !EnumHasAnyFlags(InDesc.Flags, TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable | TexCreate_FastVRAM))
		{
			Desc.SetCpuCacheMode(mtlpp::CpuCacheMode::DefaultCache);
			
			if(bAppleGPU)
			{
				Desc.SetStorageMode(mtlpp::StorageMode::Shared);
				Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache | mtlpp::ResourceOptions::StorageModeShared));
			}
#if PLATFORM_MAC
			else
			{
				Desc.SetStorageMode(mtlpp::StorageMode::Managed);
				Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache | mtlpp::ResourceOptions::StorageModeManaged));
			}
#endif
		}
		else if (EnumHasAnyFlags(InDesc.Flags, TexCreate_NoTiling) && !EnumHasAnyFlags(InDesc.Flags, TexCreate_FastVRAM | TexCreate_DepthStencilTargetable | TexCreate_RenderTargetable | TexCreate_UAV))
		{
			Desc.SetCpuCacheMode(mtlpp::CpuCacheMode::DefaultCache);
			
			if(bAppleGPU)
			{
				Desc.SetStorageMode(mtlpp::StorageMode::Shared);
				Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache | mtlpp::ResourceOptions::StorageModeShared));
			}
#if PLATFORM_MAC
			else
			{
				Desc.SetStorageMode(mtlpp::StorageMode::Managed);
				Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache | mtlpp::ResourceOptions::StorageModeManaged));
			}
#endif
		}
		else if (EnumHasAnyFlags(InDesc.Flags, TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable | TexCreate_ResolveTargetable | TexCreate_DepthStencilResolveTarget))
		{
			check(!(InDesc.Flags & TexCreate_CPUReadback));
			Desc.SetCpuCacheMode(mtlpp::CpuCacheMode::DefaultCache);
#if PLATFORM_MAC
			Desc.SetStorageMode(mtlpp::StorageMode::Private);
			Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache | mtlpp::ResourceOptions::StorageModePrivate));
#else
			if (GMetalForceIOSTexturesShared)
			{
				Desc.SetStorageMode(mtlpp::StorageMode::Shared);
				Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache | mtlpp::ResourceOptions::StorageModeShared));
			}
			else
			{
				Desc.SetStorageMode(mtlpp::StorageMode::Private);
				Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache | mtlpp::ResourceOptions::StorageModePrivate));
			}
#endif
		}
		else
		{
			check(!(InDesc.Flags & TexCreate_CPUReadback));
			Desc.SetCpuCacheMode(mtlpp::CpuCacheMode::DefaultCache);
#if PLATFORM_MAC
			Desc.SetStorageMode(mtlpp::StorageMode::Private);
			Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache | mtlpp::ResourceOptions::StorageModePrivate));
#else
			if (GMetalForceIOSTexturesShared)
			{
				Desc.SetStorageMode(mtlpp::StorageMode::Shared);
				Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache | mtlpp::ResourceOptions::StorageModeShared));
			}
			// No private storage for PVRTC as it messes up the blit-encoder usage.
			// note: this is set to always be on and will be re-addressed in a future release
			else
			{
				if (IsPixelFormatPVRTCCompressed(InDesc.Format))
				{
					Desc.SetStorageMode(mtlpp::StorageMode::Shared);
					Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache | mtlpp::ResourceOptions::StorageModeShared));
				}
				else
				{
					Desc.SetStorageMode(mtlpp::StorageMode::Private);
					Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache | mtlpp::ResourceOptions::StorageModePrivate));
				}
			}
#endif
		}

#if PLATFORM_IOS
		if (EnumHasAnyFlags(InDesc.Flags, TexCreate_Memoryless))
		{
			ensure(EnumHasAnyFlags(InDesc.Flags, (TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable)));
			ensure(!EnumHasAnyFlags(InDesc.Flags, (TexCreate_CPUReadback | TexCreate_CPUWritable)));
			ensure(!EnumHasAnyFlags(InDesc.Flags, TexCreate_UAV));
			Desc.SetStorageMode(mtlpp::StorageMode::Memoryless);
			Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache | mtlpp::ResourceOptions::StorageModeMemoryless));
		}
#endif

		static mtlpp::ResourceOptions GeneralResourceOption = FMetalCommandQueue::GetCompatibleResourceOptions(mtlpp::ResourceOptions::HazardTrackingModeUntracked);
		Desc.SetResourceOptions((mtlpp::ResourceOptions)(Desc.GetResourceOptions() | GeneralResourceOption));
	}
}

FMetalSurface::FMetalSurface(FMetalTextureCreateDesc const& CreateDesc)
	: FRHITexture       (CreateDesc)
	, FormatKey         (CreateDesc.FormatKey)
	, Texture           (nil)
	, MSAATexture       (nil)
	, MSAAResolveTexture(nil)
	, TotalTextureSize  (0)
	, Viewport          (nullptr)
	, ImageSurfaceRef   (nullptr)
{
	FPlatformAtomics::InterlockedExchange(&Written, 0);

	check(CreateDesc.Extent.X > 0 && CreateDesc.Extent.Y > 0 && CreateDesc.NumMips > 0);

	// the special back buffer surface will be updated in GetMetalDeviceContext().BeginDrawingViewport - no need to set the texture here
	if (EnumHasAnyFlags(CreateDesc.Flags, TexCreate_Presentable))
	{
		return;
	}
    
    bool bIsMSAARequired = CreateDesc.NumSamples > 1 && !FParse::Param(FCommandLine::Get(), TEXT("nomsaa"));

	FResourceBulkDataInterface* BulkData = CreateDesc.BulkData;

	// The bulk data interface can be used to create external textures for VR and media player.
	// Handle these first.
	if (BulkData != nullptr)
	{
		switch (BulkData->GetResourceType())
		{
		case FResourceBulkDataInterface::EBulkDataType::MediaTexture:
			{
				checkf(CreateDesc.NumMips == 1 && CreateDesc.ArraySize == 1, TEXT("Only handling bulk data with 1 mip and 1 array length"));
				ImageSurfaceRef = (CFTypeRef)BulkData->GetResourceBulkData();
				CFRetain(ImageSurfaceRef);
				
#if !COREVIDEO_SUPPORTS_METAL
				Texture = MTLPP_VALIDATE(mtlpp::Device, GetMetalDeviceContext().GetDevice(), SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, NewTexture(CreateDesc.Desc, CVPixelBufferGetIOSurface((CVPixelBufferRef)ImageSurfaceRef), 0));
#else
				Texture = CVMetalTextureGetTexture((CVMetalTextureRef)ImageSurfaceRef);
#endif
				METAL_FATAL_ASSERT(Texture, TEXT("Failed to create texture, desc %s"), *FString([CreateDesc.Desc description]));

				BulkData->Discard();
				BulkData = nullptr;
			}
			break;

#if PLATFORM_MAC
		case FResourceBulkDataInterface::EBulkDataType::VREyeBuffer:
			{
				ImageSurfaceRef = (CFTypeRef)BulkData->GetResourceBulkData();
				CFRetain(ImageSurfaceRef);

				mtlpp::TextureDescriptor DescCopy = CreateDesc.Desc;
				DescCopy.SetStorageMode(mtlpp::StorageMode::Managed);
				DescCopy.SetResourceOptions((mtlpp::ResourceOptions)((DescCopy.GetResourceOptions() & ~(mtlpp::ResourceStorageModeMask)) | mtlpp::ResourceOptions::StorageModeManaged));

				Texture = [GetMetalDeviceContext().GetDevice() newTextureWithDescriptor:DescCopy iosurface : (IOSurfaceRef)ImageSurfaceRef plane : 0];

				METAL_FATAL_ASSERT(Texture, TEXT("Failed to create texture, desc %s"), *FString([DescCopy description]));

				BulkData->Discard();
				BulkData = nullptr;
			}
			break;
#endif
		}
	}

	if (!Texture)
	{
		// Non VR/media texture case (i.e. a regular texture
		// Create the actual texture resource. Decide if we need to create from buffer backing
		const bool bBufferCompatibleOption = 	(CreateDesc.Desc.GetTextureType() == mtlpp::TextureType::Texture2D || CreateDesc.Desc.GetTextureType() == mtlpp::TextureType::TextureBuffer) &&
												CreateDesc.NumMips == 1 && CreateDesc.ArraySize == 1 && CreateDesc.NumSamples == 1 && CreateDesc.Desc.GetDepth() == 1;

        FMetalTextureCreateDesc NewCreateDesc = CreateDesc;
        
#if PLATFORM_IOS
        // If we are attempting to create an MSAA texture the texture cannot be memoryless unless we are creating a depth texture
        if(bIsMSAARequired && CreateDesc.Format != PF_DepthStencil && EnumHasAllFlags(CreateDesc.Flags, TexCreate_Memoryless))
        {
            NewCreateDesc.Flags &= ~TexCreate_Memoryless;
            
            if (GMetalForceIOSTexturesShared)
            {
                NewCreateDesc.Desc.SetStorageMode(mtlpp::StorageMode::Shared);
                NewCreateDesc.Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache | mtlpp::ResourceOptions::StorageModeShared));
            }
            else
            {
                NewCreateDesc.Desc.SetStorageMode(mtlpp::StorageMode::Private);
                NewCreateDesc.Desc.SetResourceOptions((mtlpp::ResourceOptions)(mtlpp::ResourceOptions::CpuCacheModeDefaultCache | mtlpp::ResourceOptions::StorageModePrivate));
            }
        }
#endif
        
		if(bBufferCompatibleOption && (EnumHasAllFlags(CreateDesc.Flags, TexCreate_UAV | TexCreate_NoTiling) || EnumHasAllFlags(CreateDesc.Flags, TexCreate_AtomicCompatible)))
		{
			mtlpp::Device Device = GetMetalDeviceContext().GetDevice();

			const uint32 MinimumByteAlignment = Device.GetMinimumLinearTextureAlignmentForPixelFormat(CreateDesc.MTLFormat);
			const NSUInteger BytesPerRow = Align(NewCreateDesc.Desc.GetWidth() * GPixelFormats[NewCreateDesc.Format].BlockBytes, MinimumByteAlignment);

			// Backing buffer resource options must match the texture we are going to create from it
			FMetalPooledBufferArgs Args(Device, BytesPerRow * NewCreateDesc.Desc.GetHeight(), BUF_Dynamic, mtlpp::StorageMode::Private, NewCreateDesc.Desc.GetCpuCacheMode());
			FMetalBuffer Buffer = GetMetalDeviceContext().CreatePooledBuffer(Args);

			Texture = Buffer.NewTexture(NewCreateDesc.Desc, 0, BytesPerRow);
		}
		else
		{
			// If we are in here then either the texture description is not buffer compatable or these flags were not set
			// assert that these flag combinations are not set as they require a buffer backed texture and the texture description is not compatible with that
			checkf(!EnumHasAllFlags(CreateDesc.Flags, TexCreate_AtomicCompatible), TEXT("Requested buffer backed texture that breaks Metal linear texture limitations: %s"), *FString([NewCreateDesc.Desc description]));
			Texture = GetMetalDeviceContext().CreateTexture(this, NewCreateDesc.Desc);
		}
		
		METAL_FATAL_ASSERT(Texture, TEXT("Failed to create texture, desc %s"), *FString([CreateDesc.Desc description]));
	}

	if (BulkData)
	{
		// Regular texture has some bulk data to handle
		UE_LOG(LogMetal, Display, TEXT("Got a bulk data texture, with %d mips"), CreateDesc.NumMips);
		checkf(CreateDesc.NumMips == 1, TEXT("Only handling bulk data with 1 mip and 1 array length"));

		check(IsInRenderingThread());
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

		// lock, copy, unlock
		uint32 Stride;
		void* LockedData = FMetalDynamicRHI::Get().LockTexture2D_RenderThread(RHICmdList, this, 0, RLM_WriteOnly, Stride, false);
		check(LockedData);
		FMemory::Memcpy(LockedData, BulkData->GetResourceBulkData(), BulkData->GetResourceBulkDataSize());
		FMetalDynamicRHI::Get().UnlockTexture2D_RenderThread(RHICmdList, this, 0, false);

		// bulk data can be unloaded now
		BulkData->Discard();
		BulkData = nullptr;
	}

	// calculate size of the texture
	TotalTextureSize = GetMemorySize();

	if (bIsMSAARequired)
	{
		mtlpp::TextureDescriptor Desc = CreateDesc.Desc;
		check(CreateDesc.bIsRenderTarget);
		Desc.SetTextureType(mtlpp::TextureType::Texture2DMultisample);

		// allow commandline to override
		uint32 NewNumSamples;
		if (FParse::Value(FCommandLine::Get(), TEXT("msaa="), NewNumSamples))
		{
			Desc.SetSampleCount(NewNumSamples);
		}
		else
		{
			Desc.SetSampleCount(CreateDesc.NumSamples);
		}

		bool bMemoryless = false;
        
#if PLATFORM_IOS
		if (EnumHasAllFlags(CreateDesc.Flags, TexCreate_Memoryless))
		{
			bMemoryless = true;
			Desc.SetStorageMode(mtlpp::StorageMode::Memoryless);
			Desc.SetResourceOptions(mtlpp::ResourceOptions::StorageModeMemoryless);
		}
#endif

		MSAATexture = GetMetalDeviceContext().CreateTexture(this, Desc);
			
		//device doesn't support HW depth resolve.  This case only valid on mobile renderer or
		//on Mac where RHISupportsSeparateMSAAAndResolveTextures is true.
		const bool bSupportsMSAADepthResolve = GetMetalDeviceContext().SupportsFeature(EMetalFeaturesMSAADepthResolve);
		const bool bDepthButNoResolveSupported = CreateDesc.Format == PF_DepthStencil && !bSupportsMSAADepthResolve;
		if (bDepthButNoResolveSupported)
		{
			Texture = MSAATexture;
				
			// we don't have the resolve texture, so we just update the memory size with the MSAA size
			TotalTextureSize = TotalTextureSize * CreateDesc.NumSamples;
		}
		else if (!bMemoryless)
		{
			// an MSAA render target takes NumSamples more space, in addition to the resolve texture
			TotalTextureSize += TotalTextureSize * CreateDesc.NumSamples;
		}
			
		if (MSAATexture != Texture)
		{
			check(!MSAAResolveTexture);
				
			//if bSupportsSeparateMSAAAndResolve then the high level expect to binds the MSAA when binding shader params.
			const bool bSupportsSeparateMSAAAndResolve = FMetalCommandQueue::SupportsSeparateMSAAAndResolveTarget();
			if (bSupportsSeparateMSAAAndResolve)
			{
				MSAAResolveTexture = Texture;
					
				Texture = MSAATexture;
			}
			else
			{
				MSAAResolveTexture = Texture;
			}
		}
			
		//we always require an MSAAResolveTexture if MSAATexture is active.
		check(!MSAATexture || MSAAResolveTexture || bDepthButNoResolveSupported);
			
		NSLog(@"Creating %dx MSAA %d x %d %s surface", (int32)CreateDesc.Desc.GetSampleCount(), CreateDesc.Extent.X, CreateDesc.Extent.Y, EnumHasAnyFlags(CreateDesc.Flags, TexCreate_RenderTargetable) ? "Color" : "Depth");
		if (MSAATexture.GetPtr() == nil)
		{
			NSLog(@"Failed to create texture, desc  %@", CreateDesc.Desc.GetPtr());
		}
	}
	
	// create a stencil buffer if needed
	if (CreateDesc.Format == PF_DepthStencil)
	{
		// 1 byte per texel
		TotalTextureSize += CreateDesc.Extent.X * CreateDesc.Extent.Y;
	}
	
	// track memory usage
	
	if (CreateDesc.bIsRenderTarget)
	{
		GCurrentRendertargetMemorySize += Align(TotalTextureSize, 1024) / 1024;
	}
	else
	{
		GCurrentTextureMemorySize += Align(TotalTextureSize, 1024) / 1024;
	}
	
#if STATS
	if (CreateDesc.IsTextureCube())
	{
		if (CreateDesc.bIsRenderTarget)
		{
			INC_MEMORY_STAT_BY(STAT_RenderTargetMemoryCube, TotalTextureSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_TextureMemoryCube, TotalTextureSize);
		}
	}
	else if (CreateDesc.IsTexture3D())
	{
		if (CreateDesc.bIsRenderTarget)
		{
			INC_MEMORY_STAT_BY(STAT_RenderTargetMemory3D, TotalTextureSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_TextureMemory3D, TotalTextureSize);
		}
	}
	else
	{
		if (CreateDesc.bIsRenderTarget)
		{
			INC_MEMORY_STAT_BY(STAT_RenderTargetMemory2D, TotalTextureSize);
		}
		else
		{
			INC_MEMORY_STAT_BY(STAT_TextureMemory2D, TotalTextureSize);
		}
	}
#endif
}

@interface FMetalDeferredStats : FApplePlatformObject
{
@public
	uint64 TextureSize;
	ETextureDimension Dimension;
	bool bIsRenderTarget;
}
@end

@implementation FMetalDeferredStats
APPLE_PLATFORM_OBJECT_ALLOC_OVERRIDES(FMetalDeferredStats)
-(void)dealloc
{
#if STATS
	if (Dimension == ETextureDimension::TextureCube || Dimension == ETextureDimension::TextureCubeArray)
	{
		if (bIsRenderTarget)
		{
			DEC_MEMORY_STAT_BY(STAT_RenderTargetMemoryCube, TextureSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_TextureMemoryCube, TextureSize);
		}
	}
	else if (Dimension == ETextureDimension::Texture3D)
	{
		if (bIsRenderTarget)
		{
			DEC_MEMORY_STAT_BY(STAT_RenderTargetMemory3D, TextureSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_TextureMemory3D, TextureSize);
		}
	}
	else
	{
		if (bIsRenderTarget)
		{
			DEC_MEMORY_STAT_BY(STAT_RenderTargetMemory2D, TextureSize);
		}
		else
		{
			DEC_MEMORY_STAT_BY(STAT_TextureMemory2D, TextureSize);
		}
	}
#endif
	if (bIsRenderTarget)
	{
		GCurrentRendertargetMemorySize -= Align(TextureSize, 1024) / 1024;
	}
	else
	{
		GCurrentTextureMemorySize -= Align(TextureSize, 1024) / 1024;
	}
	[super dealloc];
}
@end

FMetalSurface::~FMetalSurface()
{
	bool const bIsRenderTarget = IsRenderTarget(GetDesc().Flags);
	
	if (MSAATexture.GetPtr())
	{
		if (Texture.GetPtr() != MSAATexture.GetPtr())
		{
			SafeReleaseMetalTexture(this, MSAATexture, false);
		}
	}
	
	
	//do the same as above.  only do a [release] if it's the same as texture.
	if (MSAAResolveTexture.GetPtr())
	{
		if (Texture.GetPtr() != MSAAResolveTexture.GetPtr())
		{
			SafeReleaseMetalTexture(this, MSAAResolveTexture, false);
		}
	}
	
	if (!(GetDesc().Flags & TexCreate_Presentable) && Texture.GetPtr())
	{
		SafeReleaseMetalTexture(this, Texture, (ImageSurfaceRef != nullptr));
	}
	
	MSAATexture = nil;
	MSAAResolveTexture = nil;
	Texture = nil;
	
	// track memory usage
	FMetalDeferredStats* Block = [FMetalDeferredStats new];
	Block->Dimension = GetDesc().Dimension;
	Block->TextureSize = TotalTextureSize;
	Block->bIsRenderTarget = bIsRenderTarget;
	SafeReleaseMetalObject(Block);
	
	if(ImageSurfaceRef)
	{
		// CFArray can contain CFType objects and is toll-free bridged with NSArray
		CFArrayRef Temp = CFArrayCreate(kCFAllocatorSystemDefault, &ImageSurfaceRef, 1, &kCFTypeArrayCallBacks);
		SafeReleaseMetalObject((NSArray*)Temp);
		CFRelease(ImageSurfaceRef);
	}
	
	ImageSurfaceRef = nullptr;
}

id <MTLBuffer> FMetalSurface::AllocSurface(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride, bool SingleLayer /*= false*/)
{
	check(IsInRenderingThread());

	// get size and stride
	uint32 MipBytes = GetMipSize(MipIndex, &DestStride, SingleLayer);
	
	// allocate some temporary memory
	// This should really be pooled and texture transfers should be their own pool
	id <MTLDevice> Device = GetMetalDeviceContext().GetDevice();
	id <MTLBuffer> Buffer = [Device newBufferWithLength:MipBytes options:MTLResourceStorageModeShared];
	Buffer.label = @"Temporary Surface Backing";
	
	// Note: while the lock is active, this map owns the backing store.
	const uint32 LockIndex = ComputeLockIndex(MipIndex, ArrayIndex);
	GRHILockTracker.Lock(this, Buffer, LockIndex, MipBytes, LockMode, false);
	
#if PLATFORM_MAC
	// Expand R8_sRGB into RGBA8_sRGB for non Apple Silicon Mac.
	if (   GetDesc().Format == PF_G8 
		&& GetDesc().Dimension == ETextureDimension::Texture2D
		&& EnumHasAnyFlags(GetDesc().Flags, TexCreate_SRGB) 
		&& LockMode == RLM_WriteOnly 
		&& Texture.GetPixelFormat() == mtlpp::PixelFormat::RGBA8Unorm_sRGB)
	{
		DestStride = FMath::Max<uint32>(GetDesc().Extent.X >> MipIndex, 1);
	}
#endif
	
	check(Buffer);
	
	return Buffer;
}

void FMetalSurface::UpdateSurfaceAndDestroySourceBuffer(id <MTLBuffer> SourceBuffer, uint32 MipIndex, uint32 ArrayIndex)
{
#if STATS
	uint64 Start = FPlatformTime::Cycles64();
#endif
	check(SourceBuffer);
	
	uint32 Stride;
	uint32 BytesPerImage = GetMipSize(MipIndex, &Stride, true);
	
	mtlpp::Region Region;
	if (GetDesc().IsTexture3D())
	{
		// upload the texture to the texture slice
		Region = mtlpp::Region(
			0, 0, 0,
			FMath::Max<uint32>(GetDesc().Extent.X >> MipIndex, 1),
			FMath::Max<uint32>(GetDesc().Extent.Y >> MipIndex, 1),
			FMath::Max<uint32>(GetDesc().Depth    >> MipIndex, 1)
		);
	}
	else
	{
		// upload the texture to the texture slice
		Region = mtlpp::Region(
			0, 0,
			FMath::Max<uint32>(GetDesc().Extent.X >> MipIndex, 1),
			FMath::Max<uint32>(GetDesc().Extent.Y >> MipIndex, 1)
		);
	}

#if PLATFORM_MAC
	// Expand R8_sRGB into RGBA8_sRGB for non Apple Silicon Mac.
	if (   GetDesc().Format == PF_G8 
		&& GetDesc().Dimension == ETextureDimension::Texture2D
		&& EnumHasAnyFlags(GetDesc().Flags, TexCreate_SRGB) 
		&& Texture.GetPixelFormat() == mtlpp::PixelFormat::RGBA8Unorm_sRGB)
	{
		TArray<uint8> Data;
		uint8* ExpandedMem = (uint8*) SourceBuffer.contents;
		check(ExpandedMem);
		Data.Append(ExpandedMem, BytesPerImage);
		uint32 SrcStride = FMath::Max<uint32>(GetDesc().Extent.X >> MipIndex, 1);
		for(uint y = 0; y < FMath::Max<uint32>(GetDesc().Extent.Y >> MipIndex, 1); y++)
		{
			uint8* RowDest = ExpandedMem;
			for(uint x = 0; x < FMath::Max<uint32>(GetDesc().Extent.X >> MipIndex, 1); x++)
			{
				*(RowDest++) = Data[(y * SrcStride) + x];
				*(RowDest++) = Data[(y * SrcStride) + x];
				*(RowDest++) = Data[(y * SrcStride) + x];
				*(RowDest++) = Data[(y * SrcStride) + x];
			}
			ExpandedMem = (ExpandedMem + Stride);
		}
	}
#endif
	
	if(Texture.GetStorageMode() == mtlpp::StorageMode::Private)
	{
		SCOPED_AUTORELEASE_POOL;
		
		FMetalBuffer Buffer(SourceBuffer);
		
		int64 Size = BytesPerImage * Region.size.depth * FMath::Max(1u, ArrayIndex);
		
		int64 Count = FPlatformAtomics::InterlockedAdd(&ActiveUploads, Size);
		
		bool const bWait = ((GetMetalDeviceContext().GetNumActiveContexts() == 1) && (GMetalMaxOutstandingAsyncTexUploads > 0) && (Count >= GMetalMaxOutstandingAsyncTexUploads));
		
		mtlpp::BlitOption Options = mtlpp::BlitOption::None;
#if !PLATFORM_MAC
		if (Texture.GetPixelFormat() >= mtlpp::PixelFormat::PVRTC_RGB_2BPP && Texture.GetPixelFormat() <= mtlpp::PixelFormat::PVRTC_RGBA_4BPP_sRGB)
		{
			Options = mtlpp::BlitOption::RowLinearPVRTC;
		}
#endif
		
		if(GetMetalDeviceContext().AsyncCopyFromBufferToTexture(Buffer, 0, Stride, BytesPerImage, Region.size, Texture, ArrayIndex, MipIndex, Region.origin, Options))
		{
			mtlpp::CommandBufferHandler ScheduledHandler = nil;
	#if STATS
			int64* Cycles = new int64;
			FPlatformAtomics::InterlockedExchange(Cycles, 0);
			ScheduledHandler = [Cycles](mtlpp::CommandBuffer const&)
			{
				FPlatformAtomics::InterlockedExchange(Cycles, FPlatformTime::Cycles64());
			};
			mtlpp::CommandBufferHandler CompletionHandler = [SourceBuffer, Size, Cycles](mtlpp::CommandBuffer const&)
	#else
			mtlpp::CommandBufferHandler CompletionHandler = [SourceBuffer, Size](mtlpp::CommandBuffer const&)
	#endif
			{
				FPlatformAtomics::InterlockedAdd(&ActiveUploads, -Size);
	#if STATS
				int64 Taken = FPlatformTime::Cycles64() - *Cycles;
				delete Cycles;
				FPlatformAtomics::InterlockedAdd(&GMetalTexturePageOnTime, Taken);
	#endif
				[SourceBuffer release];
			};
			GetMetalDeviceContext().SubmitAsyncCommands(ScheduledHandler, CompletionHandler, bWait);
			
		}
		else
		{
			mtlpp::CommandBufferHandler CompletionHandler = [SourceBuffer, Size](mtlpp::CommandBuffer const&)
			{
				FPlatformAtomics::InterlockedAdd(&ActiveUploads, -Size);
				[SourceBuffer release];
			};
			GetMetalDeviceContext().GetCurrentRenderPass().AddCompletionHandler(CompletionHandler);
		}
		
		INC_DWORD_STAT_BY(STAT_MetalTextureMemUpdate, Size);
		
		if (bWait)
		{
			GetMetalDeviceContext().ClearFreeList();
		}
	}
	else
	{
#if !PLATFORM_MAC
		if (Texture.GetPixelFormat() >= mtlpp::PixelFormat::PVRTC_RGB_2BPP && Texture.GetPixelFormat() <= mtlpp::PixelFormat::PVRTC_RGBA_4BPP_sRGB) // @todo Calculate correct strides and byte-counts
		{
			Stride = 0;
			BytesPerImage = 0;
		}
#endif
		
		MTLPP_VALIDATE(mtlpp::Texture, Texture, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Replace(Region, MipIndex, ArrayIndex, SourceBuffer.contents, Stride, BytesPerImage));
		[SourceBuffer release];
		
		INC_DWORD_STAT_BY(STAT_MetalTextureMemUpdate, BytesPerImage);
	}
	
	FPlatformAtomics::InterlockedExchange(&Written, 1);
	
#if STATS
	GMetalTexturePageOnTime += (FPlatformTime::Cycles64() - Start);
#endif
}

void* FMetalSurface::Lock(uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride, bool SingleLayer /*= false*/)
{
	// get size and stride
	uint32 MipBytes = GetMipSize(MipIndex, &DestStride, false);
	
	// allocate some temporary memory
	id <MTLBuffer> Buffer = AllocSurface(MipIndex, ArrayIndex, LockMode, DestStride, SingleLayer);
	FMetalBuffer SourceData(Buffer);
	
	switch(LockMode)
	{
		case RLM_ReadOnly:
		{
			SCOPE_CYCLE_COUNTER(STAT_MetalTexturePageOffTime);
			
			FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
			const bool bIssueImmediateCommands = RHICmdList.Bypass() || IsInRHIThread();
			
			mtlpp::Region Region;
			if (GetDesc().IsTexture3D())
			{
				// upload the texture to the texture slice
				Region = mtlpp::Region(
					0, 0, 0,
					FMath::Max<uint32>(GetDesc().Extent.X >> MipIndex, 1),
					FMath::Max<uint32>(GetDesc().Extent.Y >> MipIndex, 1),
					FMath::Max<uint32>(GetDesc().Depth    >> MipIndex, 1));
			}
			else
			{
				// upload the texture to the texture slice
				Region = mtlpp::Region(
					0, 0,
					FMath::Max<uint32>(GetDesc().Extent.X >> MipIndex, 1),
					FMath::Max<uint32>(GetDesc().Extent.Y >> MipIndex, 1)
				);
			}
			
			if (Texture.GetStorageMode() == mtlpp::StorageMode::Private)
			{
				// If we are running with command lists or the RHI thread is enabled we have to execute GFX commands in that context.
				auto CopyTexToBuf =
				[this, &ArrayIndex, &MipIndex, &Region, &SourceData, &DestStride, &MipBytes](FRHICommandListImmediate& RHICmdList)
				{
					GetMetalDeviceContext().CopyFromTextureToBuffer(this->Texture, ArrayIndex, MipIndex, Region.origin, Region.size, SourceData, 0, DestStride, MipBytes, mtlpp::BlitOption::None);
					//kick the current command buffer.
					GetMetalDeviceContext().SubmitCommandBufferAndWait();
				};
				
				if (bIssueImmediateCommands)
				{
					CopyTexToBuf(RHICmdList);
				}
				else
				{
					RHICmdList.EnqueueLambda(MoveTemp(CopyTexToBuf));
					RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
				}
			}
			else
			{
#if PLATFORM_MAC
				if(this->Texture.GetStorageMode() == mtlpp::StorageMode::Managed)
				{
					// Managed texture - need to sync GPU -> CPU before access as it could have been written to by the GPU
					auto SyncReadbackToCPU =
					[this, &ArrayIndex, &MipIndex](FRHICommandListImmediate& RHICmdList)
					{
						GetMetalDeviceContext().SynchronizeTexture(this->Texture, ArrayIndex, MipIndex);
						GetMetalDeviceContext().SubmitCommandBufferAndWait();
					};
					
					// Similar to above. If we are in a context where we have command lists or the RHI thread we must execute
					// commands there. Otherwise we can just do this directly.
					if (bIssueImmediateCommands)
					{
						SyncReadbackToCPU(RHICmdList);
					}
					else
					{
						RHICmdList.EnqueueLambda(MoveTemp(SyncReadbackToCPU));
						RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
					}
				}
#endif
				
				// This block breaks the texture atlas system in Ocean, which depends on nonzero strides coming back from compressed textures. Turning off.
#if 0
				if (GetDesc().Format == PF_PVRTC2 || GetDesc().Format == PF_PVRTC4)
				{
					// for compressed textures metal debug RT expects 0 for rowBytes and imageBytes.
					DestStride = 0;
					MipBytes = 0;
				}
#endif
				uint32 BytesPerRow = DestStride;
				if (GetDesc().Format == PF_PVRTC2 || GetDesc().Format == PF_PVRTC4)
				{
					// for compressed textures metal debug RT expects 0 for rowBytes and imageBytes.
					BytesPerRow = 0;
					MipBytes = 0;
				}
				MTLPP_VALIDATE(mtlpp::Texture, Texture, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, GetBytes(MTLPP_VALIDATE(mtlpp::Buffer, SourceData, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, GetContents()), BytesPerRow, MipBytes, Region, MipIndex, ArrayIndex));
			}
			
#if PLATFORM_MAC
			// Pack RGBA8_sRGB into R8_sRGB for non Apple Silicon Mac.
			if (   GetDesc().Format == PF_G8 
				&& GetDesc().Dimension == ETextureDimension::Texture2D
				&& EnumHasAnyFlags(GetDesc().Flags, TexCreate_SRGB) 
				&& Texture.GetPixelFormat() == mtlpp::PixelFormat::RGBA8Unorm_sRGB)
			{
				TArray<uint8> Data;
				uint8* ExpandedMem = (uint8*)SourceData.GetContents();
				Data.Append(ExpandedMem, MipBytes);
				uint32 SrcStride = DestStride;
				DestStride = FMath::Max<uint32>(GetDesc().Extent.X >> MipIndex, 1);
				for(uint y = 0; y < FMath::Max<uint32>(GetDesc().Extent.Y >> MipIndex, 1); y++)
				{
					uint8* RowDest = ExpandedMem;
					for(uint x = 0; x < FMath::Max<uint32>(GetDesc().Extent.X >> MipIndex, 1); x++)
					{
						*(RowDest++) = Data[(y * SrcStride) + (x * 4)];
					}
					ExpandedMem = (ExpandedMem + DestStride);
				}
			}
#endif
			
			break;
		}
		case RLM_WriteOnly:
		{
			break;
		}
		default:
			check(false);
			break;
	}
	
	return SourceData.GetContents();
}

void FMetalSurface::Unlock(uint32 MipIndex, uint32 ArrayIndex, bool bTryAsync)
{
	check(IsInRenderingThread());
	
	const uint32 LockIndex = ComputeLockIndex(MipIndex, ArrayIndex);
	FRHILockTracker::FLockParams Params = GRHILockTracker.Unlock(this, LockIndex);
	
	id <MTLBuffer> SourceData = (id <MTLBuffer>) Params.Buffer;
	if(bTryAsync)
	{
		AsyncUnlock(SourceData, MipIndex, ArrayIndex);
	}
	else
	{
		UpdateSurfaceAndDestroySourceBuffer(SourceData, MipIndex, ArrayIndex);
	}
}

void* FMetalSurface::AsyncLock(class FRHICommandListImmediate& RHICmdList, uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride, bool bNeedsDefaultRHIFlush)
{
	bool bDirectLock = (LockMode == RLM_ReadOnly || !GIsRHIInitialized);
	
	void* BufferData = nullptr;
	
	// Never flush for writing, it is unnecessary
	if (bDirectLock)
	{
		if (bNeedsDefaultRHIFlush)
		{
			// @todo Not all read locks need to flush either, but that'll require resource use tracking
			QUICK_SCOPE_CYCLE_COUNTER(STAT_RHIMETHOD_LockTexture2D_Flush);
			RHICmdList.ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		}
		BufferData = Lock(MipIndex, ArrayIndex, LockMode, DestStride);
	}
	else
	{
		id <MTLBuffer> Buffer = AllocSurface(MipIndex, ArrayIndex, LockMode, DestStride);
		check(Buffer);
		
		BufferData = Buffer.contents;
	}
	
	check(BufferData);
	
	return BufferData;
}

struct FMetalRHICommandUnlockTextureUpdate final : public FRHICommand<FMetalRHICommandUnlockTextureUpdate>
{
	FMetalSurface* Surface;
	id <MTLBuffer> UpdateData;
	uint32 MipIndex;
	
	FORCEINLINE_DEBUGGABLE FMetalRHICommandUnlockTextureUpdate(FMetalSurface* InSurface, id <MTLBuffer> InUpdateData, uint32 InMipIndex)
	: Surface(InSurface)
	, UpdateData(InUpdateData)
	, MipIndex(InMipIndex)
	{
		[UpdateData retain];
	}
	
	void Execute(FRHICommandListBase& CmdList)
	{
		Surface->UpdateSurfaceAndDestroySourceBuffer(UpdateData, MipIndex, 0);
	}
	
	virtual ~FMetalRHICommandUnlockTextureUpdate()
	{
		[UpdateData release];
	}
};

void FMetalSurface::AsyncUnlock(id <MTLBuffer> SourceData, uint32 MipIndex, uint32 ArrayIndex)
{
	FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		UpdateSurfaceAndDestroySourceBuffer(SourceData, MipIndex, ArrayIndex);
	}
	else
	{
		new (RHICmdList.AllocCommand<FMetalRHICommandUnlockTextureUpdate>()) FMetalRHICommandUnlockTextureUpdate(this, SourceData, MipIndex);
	}
}

uint32 FMetalSurface::GetMipSize(uint32 MipIndex, uint32* Stride, bool bSingleLayer)
{
	EPixelFormat PixelFormat = GetDesc().Format;

	// DXT/BC formats on Mac actually do have mip-tails that are smaller than the block size, they end up being uncompressed.
	bool const bPixelFormatASTC = IsPixelFormatASTCCompressed(PixelFormat);
	
	// Calculate the dimensions of the mip-map.
	const uint32 BlockSizeX = GPixelFormats[PixelFormat].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[PixelFormat].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[PixelFormat].BlockBytes;
	const uint32 Alignment = 1u; // Apparently we always want natural row alignment (tightly-packed) even though the docs say iOS doesn't support it - this may be because we don't upload texture data from one contiguous buffer.
	const uint32 UnalignedMipSizeX = FMath::Max<uint32>(GetDesc().Extent.X >> MipIndex, BlockSizeX);
	const uint32 UnalignedMipSizeY = FMath::Max<uint32>(GetDesc().Extent.Y >> MipIndex, BlockSizeY);
	const uint32 MipSizeX = (bPixelFormatASTC) ? AlignArbitrary(UnalignedMipSizeX, BlockSizeX) : UnalignedMipSizeX;
	const uint32 MipSizeY = (bPixelFormatASTC) ? AlignArbitrary(UnalignedMipSizeY, BlockSizeY) : UnalignedMipSizeY;
	
	const uint32 MipSizeZ = bSingleLayer ? 1 : FMath::Max<uint32>(GetDesc().Depth >> MipIndex, 1u);
	uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	uint32 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;
	if (PixelFormat == PF_PVRTC2 || PixelFormat == PF_PVRTC4)
	{
		// PVRTC has minimum 2 blocks width and height
		NumBlocksX = FMath::Max<uint32>(NumBlocksX, 2);
		NumBlocksY = FMath::Max<uint32>(NumBlocksY, 2);
	}
#if PLATFORM_MAC
	else if (PixelFormat == PF_G8 && EnumHasAnyFlags(GetDesc().Flags, TexCreate_SRGB) && Texture.GetPixelFormat() == mtlpp::PixelFormat::RGBA8Unorm_sRGB)
	{
		// RGBA_sRGB is the closest match - so expand the data.
		NumBlocksX *= 4;
	}
#endif
	
	const uint32 MipStride = NumBlocksX * BlockBytes;
	const uint32 AlignedStride = ((MipStride - 1) & ~(Alignment - 1)) + Alignment;
	
	const uint32 MipBytes = AlignedStride * NumBlocksY * MipSizeZ;
	
	if (Stride)
	{
		*Stride = AlignedStride;
	}
	
	return MipBytes;
}

uint32 FMetalSurface::GetMemorySize()
{
	// if already calculated, no need to do it again
	if (TotalTextureSize != 0)
	{
		return TotalTextureSize;
	}
	
	if (Texture.GetPtr() == nil)
	{
		return 0;
	}
	
	uint32 TotalSize = 0;
	for (uint32 MipIndex = 0; MipIndex < Texture.GetMipmapLevelCount(); MipIndex++)
	{
		TotalSize += GetMipSize(MipIndex, NULL, false);
	}
	
	return TotalSize;
}

uint32 FMetalSurface::GetNumFaces()
{
	// UE <= 5.0 for Cube Texture FMetalSurface::SizeZ was set to 6
	// UE >= 5.1 FMetalSurface::SizeZ does not exist and extent.Depth from the create descriptor is set to 1 for cube textures
	return GetDesc().Depth * GetDesc().ArraySize * (GetDesc().IsTextureCube() ? 6 : 1);
}

FMetalTexture FMetalSurface::GetDrawableTexture()
{
	if (!Texture && EnumHasAnyFlags(GetDesc().Flags, TexCreate_Presentable))
	{
		check(Viewport);
		Texture = Viewport->GetDrawableTexture(EMetalViewportAccessRHI);
	}
	return Texture;
}

ns::AutoReleased<FMetalTexture> FMetalSurface::GetCurrentTexture()
{
	ns::AutoReleased<FMetalTexture> Tex;
	if (Viewport && EnumHasAnyFlags(GetDesc().Flags, TexCreate_Presentable))
	{
		check(Viewport);
		Tex = Viewport->GetCurrentTexture(EMetalViewportAccessRHI);
	}
	return Tex;
}


/*-----------------------------------------------------------------------------
 Texture allocator support.
 -----------------------------------------------------------------------------*/

void FMetalDynamicRHI::RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats)
{
	if(MemoryStats.TotalGraphicsMemory > 0)
	{
		OutStats.DedicatedVideoMemory = MemoryStats.DedicatedVideoMemory;
		OutStats.DedicatedSystemMemory = MemoryStats.DedicatedSystemMemory;
		OutStats.SharedSystemMemory = MemoryStats.SharedSystemMemory;
		OutStats.TotalGraphicsMemory = MemoryStats.TotalGraphicsMemory;
	}
	else
	{
		OutStats.DedicatedVideoMemory = 0;
		OutStats.DedicatedSystemMemory = 0;
		OutStats.SharedSystemMemory = 0;
		OutStats.TotalGraphicsMemory = 0;
	}
	
	OutStats.AllocatedMemorySize = int64(GCurrentTextureMemorySize) * 1024;
	OutStats.LargestContiguousAllocation = OutStats.AllocatedMemorySize;
	OutStats.TexturePoolSize = GTexturePoolSize;
	OutStats.PendingMemoryAdjustment = 0;
}

bool FMetalDynamicRHI::RHIGetTextureMemoryVisualizeData( FColor* /*TextureData*/, int32 /*SizeX*/, int32 /*SizeY*/, int32 /*Pitch*/, int32 /*PixelSize*/ )
{
	NOT_SUPPORTED("RHIGetTextureMemoryVisualizeData");
	return false;
}

uint32 FMetalDynamicRHI::RHIComputeMemorySize(FRHITexture* TextureRHI)
{
	@autoreleasepool {
		if(!TextureRHI)
		{
			return 0;
		}
		
		return GetMetalSurfaceFromRHITexture(TextureRHI)->GetMemorySize();
	}
}

/*-----------------------------------------------------------------------------
 2D texture support.
 -----------------------------------------------------------------------------*/

FTextureRHIRef FMetalDynamicRHI::RHICreateTexture_RenderThread(class FRHICommandListImmediate& RHICmdList, const FRHITextureCreateDesc& CreateDesc)
{
	@autoreleasepool{
		return this->RHICreateTexture(CreateDesc);
	}
}

FTextureRHIRef FMetalDynamicRHI::RHICreateTexture(const FRHITextureCreateDesc& CreateDesc)
{
	@autoreleasepool{
		return new FMetalSurface(CreateDesc);
	}
}

FTexture2DRHIRef FMetalDynamicRHI::RHIAsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ERHIAccess InResourceState, void** InitialMipData, uint32 NumInitialMips)
{
	UE_LOG(LogMetal, Fatal, TEXT("RHIAsyncCreateTexture2D is not supported"));
	return FTexture2DRHIRef();
}

void FMetalDynamicRHI::RHICopySharedMips(FRHITexture2D* DestTexture2D, FRHITexture2D* SrcTexture2D)
{
	NOT_SUPPORTED("RHICopySharedMips");
}

void FMetalDynamicRHI::RHIGenerateMips(FRHITexture* SourceSurfaceRHI)
{
	@autoreleasepool {
		FMetalSurface* Surf = GetMetalSurfaceFromRHITexture(SourceSurfaceRHI);
		if (Surf && Surf->Texture)
		{
			ImmediateContext.GetInternalContext().AsyncGenerateMipmapsForTexture(Surf->Texture);
		}
	}
}

FTexture2DRHIRef FMetalDynamicRHI::AsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	return this->RHIAsyncReallocateTexture2D(Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
}

ETextureReallocationStatus FMetalDynamicRHI::FinalizeAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, bool bBlockUntilCompleted)
{
	// No need to flush - does nothing
	return this->RHIFinalizeAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

ETextureReallocationStatus FMetalDynamicRHI::CancelAsyncReallocateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture2D, bool bBlockUntilCompleted)
{
	// No need to flush - does nothing
	return this->RHICancelAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

FTexture2DRHIRef FMetalDynamicRHI::RHIAsyncReallocateTexture2D(FRHITexture2D* OldTextureRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	@autoreleasepool {

		check(IsInRenderingThread());
		FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

		FMetalSurface* OldTexture = ResourceCast(OldTextureRHI);

		FRHITextureDesc Desc = OldTexture->GetDesc();
		Desc.Extent = FIntPoint(NewSizeX, NewSizeY);
		Desc.NumMips = NewMipCount;

		FRHITextureCreateDesc CreateDesc(
			Desc,
			RHIGetDefaultResourceState(Desc.Flags, false),
			TEXT("RHIAsyncReallocateTexture2D")
		);
		
		FMetalSurface* NewTexture = new FMetalSurface(CreateDesc);

		// Copy shared mips
		RHICmdList.EnqueueLambda([this, OldTexture, NewSizeX, NewSizeY, NewTexture, RequestStatus](FRHICommandListImmediate& RHICmdList)
		{
			FMetalContext& Context = ImmediateContext.GetInternalContext();

			// figure out what mips to schedule
			const uint32 NumSharedMips = FMath::Min(OldTexture->GetNumMips(), NewTexture->GetNumMips());
			const uint32 SourceMipOffset = OldTexture->GetNumMips() - NumSharedMips;
			const uint32 DestMipOffset = NewTexture->GetNumMips() - NumSharedMips;

			const uint32 BlockSizeX = GPixelFormats[OldTexture->GetFormat()].BlockSizeX;
			const uint32 BlockSizeY = GPixelFormats[OldTexture->GetFormat()].BlockSizeY;

			// only handling straight 2D textures here
			uint32 SliceIndex = 0;
			mtlpp::Origin Origin(0, 0, 0);

			FMetalTexture Tex = OldTexture->Texture;

			// DXT/BC formats on Mac actually do have mip-tails that are smaller than the block size, they end up being uncompressed.
			bool const bPixelFormatASTC = IsPixelFormatASTCCompressed(OldTexture->GetFormat());

			bool bAsync = true;
			for (uint32 MipIndex = 0; MipIndex < NumSharedMips; ++MipIndex)
			{
				const uint32 UnalignedMipSizeX = FMath::Max<uint32>(1, NewSizeX >> (MipIndex + DestMipOffset));
				const uint32 UnalignedMipSizeY = FMath::Max<uint32>(1, NewSizeY >> (MipIndex + DestMipOffset));
				const uint32 MipSizeX = FMath::Max<uint32>(1, NewSizeX >> (MipIndex + DestMipOffset));
				const uint32 MipSizeY = FMath::Max<uint32>(1, NewSizeY >> (MipIndex + DestMipOffset));

				bAsync &= Context.AsyncCopyFromTextureToTexture(OldTexture->Texture, SliceIndex, MipIndex + SourceMipOffset, Origin, mtlpp::Size(MipSizeX, MipSizeY, 1), NewTexture->Texture, SliceIndex, MipIndex + DestMipOffset, Origin);
			}

			// when done, decrement the counter to indicate it's safe
			mtlpp::CommandBufferHandler CompletionHandler = [Tex](mtlpp::CommandBuffer const&)
			{
			};

			if (bAsync)
			{
				// kck it off!
				Context.SubmitAsyncCommands(nil, CompletionHandler, false);
			}
			else
			{
				Context.GetCurrentRenderPass().AddCompletionHandler(CompletionHandler);
			}

			// Like D3D mark this as complete immediately.
			RequestStatus->Decrement();

			FMetalSurface* Source = GetMetalSurfaceFromRHITexture(OldTexture);
			Source->MakeAliasable();
		});
		
		return NewTexture;
	}
}

ETextureReallocationStatus FMetalDynamicRHI::RHIFinalizeAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Succeeded;
}

ETextureReallocationStatus FMetalDynamicRHI::RHICancelAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Failed;
}

void* FMetalDynamicRHI::LockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush)
{
	@autoreleasepool {
		check(IsInRenderingThread());
		
		FMetalSurface* TextureMTL = ResourceCast(Texture);
		
		void* BufferData = TextureMTL->AsyncLock(RHICmdList, MipIndex, 0, LockMode, DestStride, bNeedsDefaultRHIFlush);
		
		return BufferData;
	}
}

void FMetalDynamicRHI::UnlockTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, bool bLockWithinMiptail, bool bNeedsDefaultRHIFlush)
{
	@autoreleasepool
	{
		check(IsInRenderingThread());
		
		FMetalSurface* TextureMTL = ResourceCast(Texture);
		TextureMTL->Unlock(MipIndex, 0, true);
	}
}


void* FMetalDynamicRHI::RHILockTexture2D(FRHITexture2D* TextureRHI,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	@autoreleasepool {
		FMetalSurface* Texture = ResourceCast(TextureRHI);
		return Texture->Lock(MipIndex, 0, LockMode, DestStride);
	}
}

void FMetalDynamicRHI::RHIUnlockTexture2D(FRHITexture2D* TextureRHI,uint32 MipIndex,bool bLockWithinMiptail)
{
	@autoreleasepool {
		FMetalSurface* Texture = ResourceCast(TextureRHI);
		Texture->Unlock(MipIndex, 0, false);
	}
}

void* FMetalDynamicRHI::RHILockTexture2DArray(FRHITexture2DArray* TextureRHI,uint32 TextureIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	@autoreleasepool {
		FMetalSurface* Texture = ResourceCast(TextureRHI);
		return Texture->Lock(MipIndex, TextureIndex, LockMode, DestStride);
	}
}

void FMetalDynamicRHI::RHIUnlockTexture2DArray(FRHITexture2DArray* TextureRHI,uint32 TextureIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	@autoreleasepool {
		FMetalSurface* Texture = ResourceCast(TextureRHI);
		Texture->Unlock(MipIndex, TextureIndex, false);
	}
}

#if PLATFORM_MAC
static void InternalExpandR8ToStandardRGBA(uint32* pDest, const struct FUpdateTextureRegion2D& UpdateRegion, uint32& InOutSourcePitch, const uint8* pSrc)
{
	// Should only be required for non Apple Silicon Macs
	const uint32 ExpandedPitch = UpdateRegion.Width * sizeof(uint32);
	
	for(uint y = 0; y < UpdateRegion.Height; y++)
	{
		for(uint x = 0; x < UpdateRegion.Width; x++)
		{
			uint8 Value = pSrc[(y * InOutSourcePitch) + x];
			*(pDest++) = (Value | (Value << 8) | (Value << 16) | (Value << 24));
		}
	}
	
	InOutSourcePitch = ExpandedPitch;
}
#endif

static FMetalBuffer Internal_CreateBufferAndCopyTexture2DUpdateRegionData(FRHITexture2D* TextureRHI, const struct FUpdateTextureRegion2D& UpdateRegion, uint32& InOutSourcePitch, const uint8* SourceData)
{
	FMetalBuffer OutBuffer;

	FMetalSurface* Texture = ResourceCast(TextureRHI);

#if PLATFORM_MAC
	// Expand R8_sRGB into RGBA8_sRGB for non Apple Silicon Mac.
	if (   Texture->GetFormat() == PF_G8
		&& EnumHasAnyFlags(Texture->GetFlags(), TexCreate_SRGB)
		&& Texture->Texture.GetPixelFormat() == mtlpp::PixelFormat::RGBA8Unorm_sRGB)
	{
		const uint32 ExpandedBufferSize = UpdateRegion.Height * UpdateRegion.Width * sizeof(uint32);
		OutBuffer = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetMetalDeviceContext().GetDevice(), ExpandedBufferSize, BUF_Static, mtlpp::StorageMode::Shared));
		InternalExpandR8ToStandardRGBA((uint32*)OutBuffer.GetContents(), UpdateRegion, InOutSourcePitch, SourceData);
	}
	else
#endif
	{
		const FPixelFormatInfo& FormatInfo = GPixelFormats[TextureRHI->GetFormat()];
		
		const uint32 BufferSize = UpdateRegion.Height * InOutSourcePitch;
		OutBuffer = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetMetalDeviceContext().GetDevice(), BufferSize, BUF_Static, mtlpp::StorageMode::Shared));

		uint32 CopyPitch = FMath::DivideAndRoundUp(UpdateRegion.Width, (uint32)FormatInfo.BlockSizeX) * FormatInfo.BlockBytes;
		check(CopyPitch <= InOutSourcePitch);
		
		uint8* pDestRow = (uint8*)OutBuffer.GetContents();
		uint8* pSourceRow = (uint8*)SourceData;
		const uint32 NumRows = FMath::DivideAndRoundUp(UpdateRegion.Height, (uint32)FormatInfo.BlockSizeY);
		
		// Limit copy to line by line by update region pitch otherwise we can go off the end of source data on the last row
		for (uint32 i = 0;i < NumRows;++i)
		{
			FMemory::Memcpy(pDestRow, pSourceRow, CopyPitch);
			pSourceRow += InOutSourcePitch;
			pDestRow += InOutSourcePitch;
		}
	}

	return OutBuffer;
}

static void InternalUpdateTexture2D(FMetalContext& Context, FRHITexture2D* TextureRHI, uint32 MipIndex, FUpdateTextureRegion2D const& UpdateRegion, uint32 SourcePitch, FMetalBuffer Buffer)
{
	FMetalSurface* Texture = ResourceCast(TextureRHI);
	FMetalTexture Tex = Texture->Texture;
	
	mtlpp::Region Region = mtlpp::Region(UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.Width, UpdateRegion.Height);
	
	if(Tex.GetStorageMode() == mtlpp::StorageMode::Private)
	{
		SCOPED_AUTORELEASE_POOL;
		
		const FPixelFormatInfo& FormatInfo = GPixelFormats[TextureRHI->GetFormat()];
		const uint32 NumRows = FMath::DivideAndRoundUp(UpdateRegion.Height, (uint32)FormatInfo.BlockSizeY);
		uint32 BytesPerImage = SourcePitch * NumRows;
		
		mtlpp::BlitOption Options = mtlpp::BlitOption::None;
#if !PLATFORM_MAC
		if (Tex.GetPixelFormat() >= mtlpp::PixelFormat::PVRTC_RGB_2BPP && Tex.GetPixelFormat() <= mtlpp::PixelFormat::PVRTC_RGBA_4BPP_sRGB)
		{
			Options = mtlpp::BlitOption::RowLinearPVRTC;
		}
#endif
		if(Context.AsyncCopyFromBufferToTexture(Buffer, 0, SourcePitch, BytesPerImage, Region.size, Tex, 0, MipIndex, Region.origin, Options))
		{
			Context.SubmitAsyncCommands(nil, nil, false);
		}
	}
	else
	{
		MTLPP_VALIDATE(mtlpp::Texture, Tex, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Replace(Region, MipIndex, 0, MTLPP_VALIDATE(mtlpp::Buffer, Buffer, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, GetContents()), SourcePitch, 0));
	}

	FPlatformAtomics::InterlockedExchange(&Texture->Written, 1);
}

struct FMetalRHICommandUpdateTexture2D final : public FRHICommand<FMetalRHICommandUpdateTexture2D>
{
	FMetalContext& Context;
	FRHITexture2D* Texture;
	uint32 MipIndex;
	FUpdateTextureRegion2D UpdateRegion;
	uint32 SourcePitch;
	FMetalBuffer SourceBuffer;

	FORCEINLINE_DEBUGGABLE FMetalRHICommandUpdateTexture2D(FMetalContext& InContext, FRHITexture2D* InTexture, uint32 InMipIndex, FUpdateTextureRegion2D InUpdateRegion, uint32 InSourcePitch, const uint8* SourceData)
	: Context(InContext)
	, Texture(InTexture)
	, MipIndex(InMipIndex)
	, UpdateRegion(InUpdateRegion)
	, SourcePitch(InSourcePitch)
	{
		SourceBuffer = Internal_CreateBufferAndCopyTexture2DUpdateRegionData(Texture, UpdateRegion, SourcePitch, SourceData);
	}
	
	void Execute(FRHICommandListBase& CmdList)
	{		
		InternalUpdateTexture2D(Context, Texture, MipIndex, UpdateRegion, SourcePitch, SourceBuffer);
		GetMetalDeviceContext().ReleaseBuffer(SourceBuffer);
		INC_DWORD_STAT_BY(STAT_MetalTextureMemUpdate, UpdateRegion.Height * SourcePitch);
	}
};

void FMetalDynamicRHI::UpdateTexture2D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture2D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{
	@autoreleasepool {
		if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
		{
			this->RHIUpdateTexture2D(Texture, MipIndex, UpdateRegion, SourcePitch, SourceData);
		}
		else
		{
			new (RHICmdList.AllocCommand<FMetalRHICommandUpdateTexture2D>()) FMetalRHICommandUpdateTexture2D(ImmediateContext.GetInternalContext(), Texture, MipIndex, UpdateRegion, SourcePitch, SourceData);
		}
	}
}

void FMetalDynamicRHI::RHIUpdateTexture2D(FRHITexture2D* TextureRHI, uint32 MipIndex, const struct FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
{
	@autoreleasepool 
	{
		FMetalSurface* Texture = ResourceCast(TextureRHI);
		FMetalTexture Tex = Texture->Texture;
		
		if(Tex.GetStorageMode() == mtlpp::StorageMode::Private)
		{
			FMetalBuffer Buffer = Internal_CreateBufferAndCopyTexture2DUpdateRegionData(TextureRHI, UpdateRegion, SourcePitch, SourceData);
			InternalUpdateTexture2D(ImmediateContext.GetInternalContext(), TextureRHI, MipIndex, UpdateRegion, SourcePitch, Buffer);
			GetMetalDeviceContext().ReleaseBuffer(Buffer);
		}
		else
		{
#if PLATFORM_MAC
			// Expand R8_sRGB into RGBA8_sRGB for non Apple Silicon Mac.
			TArray<uint32> ExpandedData;
			if(Texture->GetFormat() == PF_G8 && EnumHasAnyFlags(Texture->GetFlags(), TexCreate_SRGB) && Tex.GetPixelFormat() == mtlpp::PixelFormat::RGBA8Unorm_sRGB)
			{
				ExpandedData.AddZeroed(UpdateRegion.Height * UpdateRegion.Width);
				InternalExpandR8ToStandardRGBA((uint32*)ExpandedData.GetData(), UpdateRegion, SourcePitch, SourceData);
				SourceData = (uint8*)ExpandedData.GetData();
			}
#endif
			mtlpp::Region Region = mtlpp::Region(UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.Width, UpdateRegion.Height);
			MTLPP_VALIDATE(mtlpp::Texture, Tex, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Replace(Region, MipIndex, 0, SourceData, SourcePitch, 0));

			FPlatformAtomics::InterlockedExchange(&Texture->Written, 1);
		}
		
		INC_DWORD_STAT_BY(STAT_MetalTextureMemUpdate, UpdateRegion.Height*SourcePitch);
	}
}

static FMetalBuffer Internal_CreateBufferAndCopyTexture3DUpdateRegionData(FRHITexture3D* TextureRHI, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData)
{
	FMetalSurface* Texture = ResourceCast(TextureRHI);
	
	const uint32 BufferSize = SourceDepthPitch * UpdateRegion.Depth;
	FMetalBuffer OutBuffer = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetMetalDeviceContext().GetDevice(), BufferSize, BUF_Static, mtlpp::StorageMode::Shared));

	const FPixelFormatInfo& FormatInfo = GPixelFormats[TextureRHI->GetFormat()];
	uint32 CopyPitch = FMath::DivideAndRoundUp(UpdateRegion.Width, (uint32)FormatInfo.BlockSizeX) * FormatInfo.BlockBytes;
	
	check(FormatInfo.BlockSizeZ == 1);
	check(CopyPitch <= SourceRowPitch);
		
	uint8_t* DestData = (uint8_t*)OutBuffer.GetContents();
	const uint32 NumRows = FMath::DivideAndRoundUp(UpdateRegion.Height, (uint32)FormatInfo.BlockSizeY);
		
	// Perform safe line copy
	for (uint32 i = 0;i < UpdateRegion.Depth;++i)
	{
		const uint8* pSourceRowData = SourceData + (SourceDepthPitch * i);
		uint8* pDestRowData = DestData + (SourceDepthPitch * i);

		for (uint32 j = 0;j < NumRows;++j)
		{
			FMemory::Memcpy(pDestRowData, pSourceRowData, CopyPitch);
			pSourceRowData += SourceRowPitch;
			pDestRowData += SourceRowPitch;
		}
	}
	
	return OutBuffer;
}


static void InternalUpdateTexture3D(FMetalContext& Context, FRHITexture3D* TextureRHI, uint32 MipIndex, const FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, FMetalBuffer Buffer)
{
	FMetalSurface* Texture = ResourceCast(TextureRHI);
	FMetalTexture Tex = Texture->Texture;
	
	mtlpp::Region Region = mtlpp::Region(UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.DestZ, UpdateRegion.Width, UpdateRegion.Height, UpdateRegion.Depth);
	
	if(Tex.GetStorageMode() == mtlpp::StorageMode::Private)
	{
		const FPixelFormatInfo& FormatInfo = GPixelFormats[TextureRHI->GetFormat()];
		const uint32 NumRows = FMath::DivideAndRoundUp(UpdateRegion.Height, (uint32)FormatInfo.BlockSizeY);
		const uint32 BytesPerImage = SourceRowPitch * NumRows;
		
		mtlpp::BlitOption Options = mtlpp::BlitOption::None;
#if !PLATFORM_MAC
		if (Tex.GetPixelFormat() >= mtlpp::PixelFormat::PVRTC_RGB_2BPP && Tex.GetPixelFormat() <= mtlpp::PixelFormat::PVRTC_RGBA_4BPP_sRGB)
		{
			Options = mtlpp::BlitOption::RowLinearPVRTC;
		}
#endif
		if(Context.AsyncCopyFromBufferToTexture(Buffer, 0, SourceRowPitch, BytesPerImage, Region.size, Tex, 0, MipIndex, Region.origin, Options))
		{
			Context.SubmitAsyncCommands(nil, nil, false);
		}
	}
	else
	{
		MTLPP_VALIDATE(mtlpp::Texture, Tex, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Replace(Region, MipIndex, 0, (uint8*)Buffer.GetContents(), SourceRowPitch, SourceDepthPitch));
	}

	FPlatformAtomics::InterlockedExchange(&Texture->Written, 1);
}

struct FMetalDynamicRHIUpdateTexture3DCommand final : public FRHICommand<FMetalDynamicRHIUpdateTexture3DCommand>
{
	FMetalContext& Context;
	FRHITexture3D* DestinationTexture;
	uint32 MipIndex;
	FUpdateTextureRegion3D UpdateRegion;
	uint32 SourceRowPitch;
	uint32 SourceDepthPitch;
	FMetalBuffer Buffer;
	
	FORCEINLINE_DEBUGGABLE FMetalDynamicRHIUpdateTexture3DCommand(FMetalContext& InContext, FRHITexture3D* TextureRHI, uint32 InMipIndex, const struct FUpdateTextureRegion3D& InUpdateRegion, uint32 InSourceRowPitch, uint32 InSourceDepthPitch, const uint8* SourceData)
	: Context(InContext)
	, DestinationTexture(TextureRHI)
	, MipIndex(InMipIndex)
	, UpdateRegion(InUpdateRegion)
	, SourceRowPitch(InSourceRowPitch)
	, SourceDepthPitch(InSourceDepthPitch)
	{
		Buffer = Internal_CreateBufferAndCopyTexture3DUpdateRegionData(TextureRHI, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData);
	}
	
	void Execute(FRHICommandListBase& CmdList)
	{
		InternalUpdateTexture3D(Context, DestinationTexture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, Buffer);
		GetMetalDeviceContext().ReleaseBuffer(Buffer);
		INC_DWORD_STAT_BY(STAT_MetalTextureMemUpdate, UpdateRegion.Height * UpdateRegion.Width * SourceDepthPitch);
	}
};

void FMetalDynamicRHI::UpdateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture3D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion, uint32 SourceRowPitch, uint32 SourceDepthPitch, const uint8* SourceData)
{
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		RHIUpdateTexture3D(Texture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData);
	}
	else
	{
		new (RHICmdList.AllocCommand<FMetalDynamicRHIUpdateTexture3DCommand>()) FMetalDynamicRHIUpdateTexture3DCommand(ImmediateContext.GetInternalContext(), Texture, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData);
	}
}

FUpdateTexture3DData FMetalDynamicRHI::BeginUpdateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, FRHITexture3D* Texture, uint32 MipIndex, const struct FUpdateTextureRegion3D& UpdateRegion)
{
	check(IsInRenderingThread());
	
	const int32 FormatSize = PixelFormatBlockBytes[Texture->GetFormat()];
	const int32 RowPitch = UpdateRegion.Width * FormatSize;
	const int32 DepthPitch = UpdateRegion.Width * UpdateRegion.Height * FormatSize;
	
	SIZE_T MemorySize = DepthPitch * UpdateRegion.Depth;
	uint8* Data = (uint8*)FMemory::Malloc(MemorySize);
	
	return FUpdateTexture3DData(Texture, MipIndex, UpdateRegion, RowPitch, DepthPitch, Data, MemorySize, GFrameNumberRenderThread);
}

void FMetalDynamicRHI::EndUpdateTexture3D_RenderThread(class FRHICommandListImmediate& RHICmdList, FUpdateTexture3DData& UpdateData)
{
	check(IsInRenderingThread());
	check(GFrameNumberRenderThread == UpdateData.FrameNumber);
	
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		GDynamicRHI->RHIUpdateTexture3D(UpdateData.Texture, UpdateData.MipIndex, UpdateData.UpdateRegion, UpdateData.RowPitch, UpdateData.DepthPitch, UpdateData.Data);
	}
	else
	{
		new (RHICmdList.AllocCommand<FMetalDynamicRHIUpdateTexture3DCommand>()) FMetalDynamicRHIUpdateTexture3DCommand(ImmediateContext.GetInternalContext(), UpdateData.Texture, UpdateData.MipIndex, UpdateData.UpdateRegion, UpdateData.RowPitch, UpdateData.DepthPitch, UpdateData.Data);
	}
	
	FMemory::Free(UpdateData.Data);
	UpdateData.Data = nullptr;
}

void FMetalDynamicRHI::RHIUpdateTexture3D(FRHITexture3D* TextureRHI,uint32 MipIndex,const FUpdateTextureRegion3D& UpdateRegion,uint32 SourceRowPitch,uint32 SourceDepthPitch, const uint8* SourceData)
{
	@autoreleasepool {
	
		FMetalSurface* Texture = ResourceCast(TextureRHI);
		FMetalTexture Tex = Texture->Texture;
		
#if PLATFORM_MAC
		checkf(!(Texture->GetFormat() == PF_G8 && EnumHasAnyFlags(Texture->GetFlags(), TexCreate_SRGB) && Tex.GetPixelFormat() == mtlpp::PixelFormat::RGBA8Unorm_sRGB), TEXT("MetalRHI on non Apple Silicon does not support PF_G8_sRGB on 3D, array or cube textures as it requires manual, CPU-side expansion to RGBA8_sRGB which is expensive!"));
#endif
		if(Tex.GetStorageMode() == mtlpp::StorageMode::Private)
		{
			SCOPED_AUTORELEASE_POOL;

			FMetalBuffer IntermediateBuffer = Internal_CreateBufferAndCopyTexture3DUpdateRegionData(TextureRHI, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData);
			InternalUpdateTexture3D(ImmediateContext.GetInternalContext(), TextureRHI, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, IntermediateBuffer);
			GetMetalDeviceContext().ReleaseBuffer(IntermediateBuffer);
		}
		else
		{
			mtlpp::Region Region = mtlpp::Region(UpdateRegion.DestX, UpdateRegion.DestY, UpdateRegion.DestZ, UpdateRegion.Width, UpdateRegion.Height, UpdateRegion.Depth);
			MTLPP_VALIDATE(mtlpp::Texture, Tex, SafeGetRuntimeDebuggingLevel() >= EMetalDebugLevelValidation, Replace(Region, MipIndex, 0, SourceData, SourceRowPitch, SourceDepthPitch));
			FPlatformAtomics::InterlockedExchange(&Texture->Written, 1);
		}
		
		INC_DWORD_STAT_BY(STAT_MetalTextureMemUpdate, UpdateRegion.Height * UpdateRegion.Width * SourceDepthPitch);
	}
}

/*-----------------------------------------------------------------------------
 Cubemap texture support.
 -----------------------------------------------------------------------------*/
void* FMetalDynamicRHI::RHILockTextureCubeFace(FRHITextureCube* TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,EResourceLockMode LockMode,uint32& DestStride,bool bLockWithinMiptail)
{
	@autoreleasepool {
		FMetalSurface* TextureCube = ResourceCast(TextureCubeRHI);
		uint32 MetalFace = GetMetalCubeFace((ECubeFace)FaceIndex);
		return TextureCube->Lock(MipIndex, MetalFace + (6 * ArrayIndex), LockMode, DestStride, true);
	}
}

void FMetalDynamicRHI::RHIUnlockTextureCubeFace(FRHITextureCube* TextureCubeRHI,uint32 FaceIndex,uint32 ArrayIndex,uint32 MipIndex,bool bLockWithinMiptail)
{
	@autoreleasepool {
		FMetalSurface* TextureCube = ResourceCast(TextureCubeRHI);
		uint32 MetalFace = GetMetalCubeFace((ECubeFace)FaceIndex);
		TextureCube->Unlock(MipIndex, MetalFace + (ArrayIndex * 6), false);
	}
}

void FMetalDynamicRHI::RHIBindDebugLabelName(FRHITexture* TextureRHI, const TCHAR* Name)
{
	@autoreleasepool {
		FMetalSurface* Surf = GetMetalSurfaceFromRHITexture(TextureRHI);
		if(Surf->Texture)
		{
			Surf->Texture.SetLabel(FString(Name).GetNSString());
		}
		if(Surf->MSAATexture)
		{
			Surf->MSAATexture.SetLabel(FString(Name).GetNSString());
		}
	}
}

void FMetalDynamicRHI::RHIVirtualTextureSetFirstMipInMemory(FRHITexture2D* TextureRHI, uint32 FirstMip)
{
	NOT_SUPPORTED("RHIVirtualTextureSetFirstMipInMemory");
}

void FMetalDynamicRHI::RHIVirtualTextureSetFirstMipVisible(FRHITexture2D* TextureRHI, uint32 FirstMip)
{
	NOT_SUPPORTED("RHIVirtualTextureSetFirstMipVisible");
}

inline bool MetalRHICopyTexutre_IsTextureFormatCompatible(EPixelFormat SrcFmt, EPixelFormat DstFmt)
{
	//
	// For now, we only support copies between textures of mismatching
	// formats if they are of size-compatible internal formats.  This allows us
	// to copy from uncompressed to compressed textures, specifically in support
	// of the runtime virtual texture system.  Note that copies of compatible
	// formats incur the cost of an extra copy, as we must copy from the source
	// texture to a temporary buffer and finally to the destination texture.
	//
	return ((SrcFmt == DstFmt) || (GPixelFormats[SrcFmt].BlockBytes == GPixelFormats[DstFmt].BlockBytes));
}

void FMetalRHICommandContext::RHICopyTexture(FRHITexture* SourceTextureRHI, FRHITexture* DestTextureRHI, const FRHICopyTextureInfo& CopyInfo)
{
	@autoreleasepool {
		check(SourceTextureRHI);
		check(DestTextureRHI);
		
		FMetalSurface* MetalSrcTexture = GetMetalSurfaceFromRHITexture(SourceTextureRHI);
		FMetalSurface* MetalDestTexture = GetMetalSurfaceFromRHITexture(DestTextureRHI);
		
		const bool TextureFormatExactMatch = (SourceTextureRHI->GetFormat() == DestTextureRHI->GetFormat());
		const bool TextureFormatCompatible = MetalRHICopyTexutre_IsTextureFormatCompatible(SourceTextureRHI->GetFormat(), DestTextureRHI->GetFormat());
		
		if (TextureFormatExactMatch || TextureFormatCompatible)
		{
			const FIntVector Size = CopyInfo.Size == FIntVector::ZeroValue ? MetalSrcTexture->GetDesc().GetSize() >> CopyInfo.SourceMipIndex : CopyInfo.Size;

			FMetalTexture SrcTexture;

			if (TextureFormatExactMatch)
			{
				mtlpp::TextureUsage Usage = MetalSrcTexture->Texture.GetUsage();
				if (Usage & mtlpp::TextureUsage::PixelFormatView)
				{
					ns::Range Slices(0, MetalSrcTexture->Texture.GetArrayLength() * (MetalSrcTexture->GetDesc().IsTextureCube() ? 6 : 1));
					if (MetalSrcTexture->Texture.GetPixelFormat() != MetalDestTexture->Texture.GetPixelFormat())
					{
						SrcTexture = MetalSrcTexture->Texture.NewTextureView(MetalDestTexture->Texture.GetPixelFormat(), MetalSrcTexture->Texture.GetTextureType(), ns::Range(0, MetalSrcTexture->Texture.GetMipmapLevelCount()), Slices);
					}
				}
				if (!SrcTexture)
				{
					SrcTexture = MetalSrcTexture->Texture;
				}
			}
			
			for (uint32 SliceIndex = 0; SliceIndex < CopyInfo.NumSlices; ++SliceIndex)
			{
				uint32 SourceSliceIndex = CopyInfo.SourceSliceIndex + SliceIndex;
				uint32 DestSliceIndex = CopyInfo.DestSliceIndex + SliceIndex;

				for (uint32 MipIndex = 0; MipIndex < CopyInfo.NumMips; ++MipIndex)
				{
					uint32 SourceMipIndex = CopyInfo.SourceMipIndex + MipIndex;
					uint32 DestMipIndex = CopyInfo.DestMipIndex + MipIndex;
					mtlpp::Size SourceSize(FMath::Max(Size.X >> MipIndex, 1), FMath::Max(Size.Y >> MipIndex, 1), FMath::Max(Size.Z >> MipIndex, 1));
					mtlpp::Size DestSize = SourceSize;

					mtlpp::Origin SourceOrigin(CopyInfo.SourcePosition.X >> MipIndex, CopyInfo.SourcePosition.Y >> MipIndex, CopyInfo.SourcePosition.Z >> MipIndex);
					mtlpp::Origin DestinationOrigin(CopyInfo.DestPosition.X >> MipIndex, CopyInfo.DestPosition.Y >> MipIndex, CopyInfo.DestPosition.Z >> MipIndex);

					if (TextureFormatCompatible)
					{
						DestSize.width  *= GPixelFormats[MetalDestTexture->GetDesc().Format].BlockSizeX;
						DestSize.height *= GPixelFormats[MetalDestTexture->GetDesc().Format].BlockSizeY;
					}

					// Account for create with TexCreate_SRGB flag which could make these different
					if (TextureFormatExactMatch && (SrcTexture.GetPixelFormat() == MetalDestTexture->Texture.GetPixelFormat()))
					{
						GetInternalContext().CopyFromTextureToTexture(SrcTexture, SourceSliceIndex, SourceMipIndex, SourceOrigin,SourceSize,MetalDestTexture->Texture, DestSliceIndex, DestMipIndex, DestinationOrigin);
					}
					else
					{
						//
						// In the case of compatible texture formats or pixel
						// format mismatch (like linear vs. sRGB), then we must
						// achieve the copy by going through a buffer object.
						//
						const bool BlockSizeMatch = (GPixelFormats[MetalSrcTexture->GetDesc().Format].BlockSizeX == GPixelFormats[MetalDestTexture->GetDesc().Format].BlockSizeX);
						const uint32 BytesPerPixel = (MetalSrcTexture->GetDesc().Format != PF_DepthStencil) ? GPixelFormats[MetalSrcTexture->GetDesc().Format].BlockBytes : 1;
						const uint32 Stride = BytesPerPixel * SourceSize.width;
#if PLATFORM_MAC
						const uint32 Alignment = 1u;
#else
						// don't mess with alignment if we copying between formats with a different block size
						const uint32 Alignment = BlockSizeMatch ? 64u : 1u;
#endif
						const uint32 AlignedStride = ((Stride - 1) & ~(Alignment - 1)) + Alignment;
						const uint32 BytesPerImage = AlignedStride *  SourceSize.height;
						const uint32 DataSize = BytesPerImage * SourceSize.depth;
						
						FMetalBuffer Buffer = GetMetalDeviceContext().CreatePooledBuffer(FMetalPooledBufferArgs(GetInternalContext().GetDevice(), DataSize, BUF_Dynamic, mtlpp::StorageMode::Shared));
						
						check(Buffer);
						
						mtlpp::BlitOption Options = mtlpp::BlitOption::None;
#if !PLATFORM_MAC
						if (MetalSrcTexture->Texture.GetPixelFormat() >= mtlpp::PixelFormat::PVRTC_RGB_2BPP && MetalSrcTexture->Texture.GetPixelFormat() <= mtlpp::PixelFormat::PVRTC_RGBA_4BPP_sRGB)
						{
							Options = mtlpp::BlitOption::RowLinearPVRTC;
						}
#endif
						GetInternalContext().CopyFromTextureToBuffer(MetalSrcTexture->Texture, SourceSliceIndex, SourceMipIndex, SourceOrigin, SourceSize, Buffer, 0, AlignedStride, BytesPerImage, Options);
						GetInternalContext().CopyFromBufferToTexture(Buffer, 0, Stride, BytesPerImage, DestSize, MetalDestTexture->Texture, DestSliceIndex, DestMipIndex, DestinationOrigin, Options);
						
						GetMetalDeviceContext().ReleaseBuffer(Buffer);
					}
				}
			}
			
			if (SrcTexture && (SrcTexture != MetalSrcTexture->Texture))
			{
				SafeReleaseMetalTexture(SrcTexture);
			}
		}
		else
		{
			UE_LOG(LogMetal, Error, TEXT("RHICopyTexture Source (UnrealEngine %d: MTL %d) <-> Destination (UnrealEngine %d: MTL %d) texture format mismatch"), (uint32)SourceTextureRHI->GetFormat(), (uint32)MetalSrcTexture->Texture.GetPixelFormat(), (uint32)DestTextureRHI->GetFormat(), (uint32)MetalDestTexture->Texture.GetPixelFormat());
		}
	}
}

void FMetalRHICommandContext::RHICopyBufferRegion(FRHIBuffer* DstBufferRHI, uint64 DstOffset, FRHIBuffer* SrcBufferRHI, uint64 SrcOffset, uint64 NumBytes)
{
	if (!DstBufferRHI || !SrcBufferRHI || DstBufferRHI == SrcBufferRHI || !NumBytes)
	{
		return;
	}

	@autoreleasepool {
		FMetalResourceMultiBuffer* DstBuffer = ResourceCast(DstBufferRHI);
		FMetalResourceMultiBuffer* SrcBuffer = ResourceCast(SrcBufferRHI);

		check(DstBuffer && SrcBuffer);
		check(!DstBuffer->Data && !SrcBuffer->Data);
		check(DstOffset + NumBytes <= DstBufferRHI->GetSize() && SrcOffset + NumBytes <= SrcBufferRHI->GetSize());

		GetInternalContext().CopyFromBufferToBuffer(SrcBuffer->GetCurrentBuffer(), SrcOffset, DstBuffer->GetCurrentBuffer(), DstOffset, NumBytes);
	}
}
