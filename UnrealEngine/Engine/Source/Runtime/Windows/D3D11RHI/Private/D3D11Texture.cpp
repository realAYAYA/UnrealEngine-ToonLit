// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	D3D11Texture.cpp: D3D texture RHI implementation.
=============================================================================*/

#include "D3D11RHIPrivate.h"

// For Depth Bounds Test interface
#include "Windows/AllowWindowsPlatformTypes.h"
#if WITH_NVAPI
	#include "nvapi.h"
#endif
#if WITH_AMD_AGS
	#include "amd_ags.h"
#endif
#include "Windows/HideWindowsPlatformTypes.h"

#include "HAL/LowLevelMemTracker.h"
#include "ProfilingDebugging/MemoryTrace.h"
#include "RHICoreStats.h"

int64 FD3D11GlobalStats::GDedicatedVideoMemory = 0;
int64 FD3D11GlobalStats::GDedicatedSystemMemory = 0;
int64 FD3D11GlobalStats::GSharedSystemMemory = 0;
int64 FD3D11GlobalStats::GTotalGraphicsMemory = 0;


/*-----------------------------------------------------------------------------
	Texture allocator support.
-----------------------------------------------------------------------------*/

// Note: This function can be called from many different threads
// @param TextureSize >0 to allocate, <0 to deallocate
// @param b3D true:3D, false:2D or cube map
void UpdateD3D11TextureStats(FD3D11Texture& Texture, bool bAllocating)
{
	const FRHITextureDesc& TextureDesc = Texture.GetDesc();

	uint32 BindFlags;
	if (TextureDesc.IsTexture3D())
	{
		D3D11_TEXTURE3D_DESC Desc;
		Texture.GetD3D11Texture3D()->GetDesc(&Desc);
		BindFlags = Desc.BindFlags;
	}
	else
	{
		D3D11_TEXTURE2D_DESC Desc;
		Texture.GetD3D11Texture2D()->GetDesc(&Desc);
		BindFlags = Desc.BindFlags;
	}

	const uint64 TextureSize = Texture.GetMemorySize();

	const bool bOnlyStreamableTexturesInTexturePool = false;
	UE::RHICore::UpdateGlobalTextureStats(TextureDesc, TextureSize, bOnlyStreamableTexturesInTexturePool, bAllocating);

	if (bAllocating)
	{
		INC_DWORD_STAT(STAT_D3D11TexturesAllocated);
	}
	else
	{
		INC_DWORD_STAT(STAT_D3D11TexturesReleased);
	}

	// On Windows there is no way to hook into the low level d3d allocations and frees.
	// This means that we must manually add the tracking here.
	if (bAllocating)
	{
		LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, Texture.GetResource(), TextureSize, ELLMTag::GraphicsPlatform));
		LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Default , Texture.GetResource(), TextureSize, ELLMTag::Textures));
		{
			LLM(UE_MEMSCOPE_DEFAULT(ELLMTag::Textures));
			MemoryTrace_Alloc((uint64)Texture.GetResource(), TextureSize, 1024, EMemoryTraceRootHeap::VideoMemory);
		}
	}
	else
	{
		LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, Texture.GetResource()));
		LLM_IF_ENABLED(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Default , Texture.GetResource()));
		MemoryTrace_Free((uint64)Texture.GetResource(), EMemoryTraceRootHeap::VideoMemory);
	}
}

FDynamicRHI::FRHICalcTextureSizeResult FD3D11DynamicRHI::RHICalcTexturePlatformSize(FRHITextureDesc const& Desc, uint32 FirstMipIndex)
{
	// D3D11 does not provide a way to compute the actual driver/GPU specific in-memory size of a texture.
	// Fallback to the estimate based on the texture's dimensions / format etc.
	FDynamicRHI::FRHICalcTextureSizeResult Result;
	Result.Size = Desc.CalcMemorySizeEstimate(FirstMipIndex);
	Result.Align = 1;
	return Result;
}

/**
 * Retrieves texture memory stats. 
 */
void FD3D11DynamicRHI::RHIGetTextureMemoryStats(FTextureMemoryStats& OutStats)
{
	UE::RHICore::FillBaselineTextureMemoryStats(OutStats);

	OutStats.DedicatedVideoMemory = FD3D11GlobalStats::GDedicatedVideoMemory;
    OutStats.DedicatedSystemMemory = FD3D11GlobalStats::GDedicatedSystemMemory;
    OutStats.SharedSystemMemory = FD3D11GlobalStats::GSharedSystemMemory;
	OutStats.TotalGraphicsMemory = FD3D11GlobalStats::GTotalGraphicsMemory ? FD3D11GlobalStats::GTotalGraphicsMemory : -1;

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
bool FD3D11DynamicRHI::RHIGetTextureMemoryVisualizeData( FColor* /*TextureData*/, int32 /*SizeX*/, int32 /*SizeY*/, int32 /*Pitch*/, int32 /*PixelSize*/ )
{
	// currently only implemented for console (Note: Keep this function for further extension. Talk to NiklasS for more info.)
	return false;
}

// Work around an issue with the WARP device & BC7
// Creating two views with different formats (DXGI_FORMAT_BC7_UNORM vs DXGI_FORMAT_BC7_UNORM_SRGB)
// will result in a crash inside d3d10warp.dll when creating the second view
void ApplyBC7SoftwareAdapterWorkaround(bool bSoftwareAdapter, D3D11_TEXTURE2D_DESC& Desc)
{
	if (bSoftwareAdapter)
	{
		bool bApplyWorkaround =	Desc.Format == DXGI_FORMAT_BC7_TYPELESS
							 && Desc.Usage == D3D11_USAGE_DEFAULT
							 && Desc.MipLevels == 1
							 && Desc.ArraySize == 1
							 && Desc.CPUAccessFlags == 0;

		if (bApplyWorkaround)
		{
			Desc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
		}
	}
}

/** If true, guard texture creates with SEH to log more information about a driver crash we are seeing during texture streaming. */
#define GUARDED_TEXTURE_CREATES (!(UE_BUILD_SHIPPING || UE_BUILD_TEST || PLATFORM_COMPILER_CLANG))

/**
 * Creates a 2D texture optionally guarded by a structured exception handler.
 */
static void SafeCreateTexture2D(ID3D11Device* Direct3DDevice, int32 UEFormat, const D3D11_TEXTURE2D_DESC* TextureDesc, const D3D11_SUBRESOURCE_DATA* SubResourceData, ID3D11Texture2D** OutTexture2D, const TCHAR* DebugName)
{
#if GUARDED_TEXTURE_CREATES
	bool bDriverCrash = true;
	__try
	{
#endif // #if GUARDED_TEXTURE_CREATES
		VERIFYD3D11CREATETEXTURERESULT(
			Direct3DDevice->CreateTexture2D(TextureDesc,SubResourceData,OutTexture2D),
			UEFormat,
			TextureDesc->Width,
			TextureDesc->Height,
			TextureDesc->ArraySize,
			TextureDesc->Format,
			TextureDesc->MipLevels,
			TextureDesc->BindFlags,
			TextureDesc->Usage,
			TextureDesc->CPUAccessFlags,
			TextureDesc->MiscFlags,			
			TextureDesc->SampleDesc.Count,
			TextureDesc->SampleDesc.Quality,
			SubResourceData ? SubResourceData->pSysMem : nullptr,
			SubResourceData ? SubResourceData->SysMemPitch : 0,
			SubResourceData ? SubResourceData->SysMemSlicePitch : 0,
			Direct3DDevice,
			DebugName
			);
#if GUARDED_TEXTURE_CREATES
		bDriverCrash = false;
	}
	__finally
	{
		if (bDriverCrash)
		{
			UE_LOG(LogD3D11RHI,Error,
				TEXT("Driver crashed while creating texture: %ux%ux%u %s(0x%08x) with %u mips, PF_ %d"),
				TextureDesc->Width,
				TextureDesc->Height,
				TextureDesc->ArraySize,
				UE::DXGIUtilities::GetFormatString(TextureDesc->Format),
				(uint32)TextureDesc->Format,
				TextureDesc->MipLevels,
				UEFormat
				);
		}
	}
#endif // #if GUARDED_TEXTURE_CREATES
}

FD3D11Texture* FD3D11DynamicRHI::CreateD3D11Texture2D(FRHITextureCreateDesc const& CreateDesc, TConstArrayView<D3D11_SUBRESOURCE_DATA> InitialData)
{
	check(!CreateDesc.IsTexture3D());

	const bool                bTextureArray = CreateDesc.IsTextureArray();
	const bool                bCubeTexture  = CreateDesc.IsTextureCube();
	const uint32              SizeX         = CreateDesc.Extent.X;
	const uint32              SizeY         = CreateDesc.Extent.Y;
	const uint32              SizeZ         = bCubeTexture ? CreateDesc.ArraySize * 6 : CreateDesc.ArraySize;
	const EPixelFormat        Format        = CreateDesc.Format;
	const uint32              NumMips       = CreateDesc.NumMips;
	const uint32              NumSamples    = CreateDesc.NumSamples;
	const ETextureCreateFlags Flags         = CreateDesc.Flags;

	check(SizeX > 0 && SizeY > 0 && NumMips > 0);

	if (bCubeTexture)
	{
		checkf(SizeX <= GetMaxCubeTextureDimension(), TEXT("Requested cube texture size too large: %i, Max: %i, DebugName: '%s'"), SizeX, GetMaxCubeTextureDimension(), CreateDesc.DebugName ? CreateDesc.DebugName : TEXT(""));
		check(SizeX == SizeY);
	}
	else
	{
		checkf(SizeX <= GetMax2DTextureDimension(), TEXT("Requested texture2d x size too large: %i, Max: %i, DebugName: '%s'"), SizeX, GetMax2DTextureDimension(), CreateDesc.DebugName ? CreateDesc.DebugName : TEXT(""));
		checkf(SizeY <= GetMax2DTextureDimension(), TEXT("Requested texture2d y size too large: %i, Max: %i, DebugName: '%s'"), SizeY, GetMax2DTextureDimension(), CreateDesc.DebugName ? CreateDesc.DebugName : TEXT(""));
	}

	if (bTextureArray)
	{
		checkf(SizeZ <= GetMaxTextureArrayLayers(), TEXT("Requested texture array size too large: %i, Max: %i, DebugName: '%s'"), SizeZ, GetMaxTextureArrayLayers(), CreateDesc.DebugName ? CreateDesc.DebugName : TEXT(""));
	}

	SCOPE_CYCLE_COUNTER(STAT_D3D11CreateTextureTime);

	const bool bSRGB = EnumHasAnyFlags(Flags, TexCreate_SRGB);

	const DXGI_FORMAT PlatformResourceFormat = UE::DXGIUtilities::GetPlatformTextureResourceFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat, Flags);
	const DXGI_FORMAT PlatformShaderResourceFormat = UE::DXGIUtilities::FindShaderResourceFormat(PlatformResourceFormat, bSRGB);
	const DXGI_FORMAT PlatformRenderTargetFormat = UE::DXGIUtilities::FindShaderResourceFormat(PlatformResourceFormat, bSRGB);
	
	uint32 CPUAccessFlags = 0;
	D3D11_USAGE TextureUsage = D3D11_USAGE_DEFAULT;
	bool bCreateShaderResource = true;

	uint32 ActualMSAAQuality = GetMaxMSAAQuality(NumSamples);
	check(ActualMSAAQuality != 0xffffffff);
	check(NumSamples == 1 || !EnumHasAnyFlags(Flags, TexCreate_Shared));

	const bool bIsMultisampled = NumSamples > 1;

	if (EnumHasAnyFlags(Flags, TexCreate_CPUReadback))
	{
		check(!EnumHasAnyFlags(Flags, TexCreate_RenderTargetable| TexCreate_DepthStencilTargetable| TexCreate_ShaderResource));

		CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		TextureUsage = D3D11_USAGE_STAGING;
		bCreateShaderResource = false;
	}

	if (EnumHasAnyFlags(Flags, TexCreate_CPUWritable))
	{
		CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		TextureUsage = D3D11_USAGE_STAGING;
		bCreateShaderResource = false;
	}

	// Describe the texture.
	D3D11_TEXTURE2D_DESC TextureDesc;
	ZeroMemory( &TextureDesc, sizeof( D3D11_TEXTURE2D_DESC ) );
	TextureDesc.Width = SizeX;
	TextureDesc.Height = SizeY;
	TextureDesc.MipLevels = NumMips;
	TextureDesc.ArraySize = SizeZ;
	TextureDesc.Format = PlatformResourceFormat;
	TextureDesc.SampleDesc.Count = NumSamples;
	TextureDesc.SampleDesc.Quality = ActualMSAAQuality;
	TextureDesc.Usage = TextureUsage;
	TextureDesc.BindFlags = bCreateShaderResource? D3D11_BIND_SHADER_RESOURCE : 0;
	TextureDesc.CPUAccessFlags = CPUAccessFlags;
	TextureDesc.MiscFlags = bCubeTexture ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;

	ApplyBC7SoftwareAdapterWorkaround(Adapter.bSoftwareAdapter, TextureDesc);

	// NV12/P010 doesn't support SRV in NV12 format so don't create SRV for it.
	// Todo: add support for SRVs of underneath luminance & chrominance textures.
	if (Format == PF_NV12 || Format == PF_P010)
	{
		// JoeG - I moved this to below the bind flags because it is valid to bind R8 or B8G8 to this
		// And creating a SRV afterward would fail because of the missing bind flags
		bCreateShaderResource = false;
	}

	if (EnumHasAnyFlags(Flags, TexCreate_DisableSRVCreation))
	{
		bCreateShaderResource = false;
	}

	if (EnumHasAnyFlags(Flags, TexCreate_Shared))
	{
		if (GCVarUseSharedKeyedMutex->GetInt() != 0)
		{
			TextureDesc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
		}
		else
		{
			TextureDesc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
		}
	}

	if (EnumHasAnyFlags(Flags, TexCreate_GenerateMipCapable))
	{
		// Set the flag that allows us to call GenerateMips on this texture later
		TextureDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
	}

	// Set up the texture bind flags.
	bool bCreateRTV = false;
	bool bCreateDSV = false;
	bool bCreatedRTVPerSlice = false;

	if(EnumHasAnyFlags(Flags, TexCreate_RenderTargetable))
	{
		check(!EnumHasAnyFlags(Flags, TexCreate_DepthStencilTargetable| TexCreate_ResolveTargetable));
		TextureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
		bCreateRTV = true;		
	}
	else if(EnumHasAnyFlags(Flags, TexCreate_DepthStencilTargetable))
	{
		check(!EnumHasAnyFlags(Flags, TexCreate_RenderTargetable| TexCreate_ResolveTargetable));
		TextureDesc.BindFlags |= D3D11_BIND_DEPTH_STENCIL; 
		bCreateDSV = true;
	}
	else if(EnumHasAnyFlags(Flags, TexCreate_ResolveTargetable))
	{
		check(!EnumHasAnyFlags(Flags, TexCreate_RenderTargetable| TexCreate_DepthStencilTargetable));
		if(Format == PF_DepthStencil || Format == PF_ShadowDepth || Format == PF_D24)
		{
			TextureDesc.BindFlags |= D3D11_BIND_DEPTH_STENCIL; 
			bCreateDSV = true;
		}
		else
		{
			TextureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
			bCreateRTV = true;
		}
	}
	// NV12 doesn't support RTV in NV12 format so don't create RTV for it.
	// Todo: add support for RTVs of underneath luminance & chrominance textures.
	if (Format == PF_NV12 || Format == PF_P010)
	{
		bCreateRTV = false;
	}

	if (EnumHasAnyFlags(Flags, TexCreate_UAV))
	{
		TextureDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}

	if (bCreateDSV && !EnumHasAnyFlags(Flags, TexCreate_ShaderResource))
	{
		TextureDesc.BindFlags &= ~D3D11_BIND_SHADER_RESOURCE;
		bCreateShaderResource = false;
	}

	TRefCountPtr<ID3D11Texture2D> TextureResource;
	TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;
	TArray<TRefCountPtr<ID3D11RenderTargetView> > RenderTargetViews;
	TRefCountPtr<ID3D11DepthStencilView> DepthStencilViews[FExclusiveDepthStencil::MaxIndex];

	TArray<D3D11_SUBRESOURCE_DATA> SubResourceData;
	D3D11_SUBRESOURCE_DATA const* pSubresourceData = nullptr;

	if (InitialData.Num())
	{
		// Caller provided initla data structs.
		check(InitialData.Num() == NumMips * SizeZ);
		pSubresourceData = InitialData.GetData();
	}
	else if (CreateDesc.BulkData)
	{
		uint8* Data = (uint8*)CreateDesc.BulkData->GetResourceBulkData();

		// each mip of each array slice counts as a subresource
		SubResourceData.AddZeroed(NumMips * SizeZ);

		uint32 SliceOffset = 0;
		for (uint32 ArraySliceIndex = 0; ArraySliceIndex < SizeZ; ++ArraySliceIndex)
		{			
			uint32 MipOffset = 0;
			for(uint32 MipIndex = 0;MipIndex < NumMips;++MipIndex)
			{
				uint32 DataOffset = SliceOffset + MipOffset;
				uint32 SubResourceIndex = ArraySliceIndex * NumMips + MipIndex;

				uint32 NumBlocksX = FMath::Max<uint32>(1,((SizeX >> MipIndex) + GPixelFormats[Format].BlockSizeX-1) / GPixelFormats[Format].BlockSizeX);
				uint32 NumBlocksY = FMath::Max<uint32>(1,((SizeY >> MipIndex) + GPixelFormats[Format].BlockSizeY-1) / GPixelFormats[Format].BlockSizeY);

				SubResourceData[SubResourceIndex].pSysMem = &Data[DataOffset];
				SubResourceData[SubResourceIndex].SysMemPitch      =  NumBlocksX * GPixelFormats[Format].BlockBytes;
				SubResourceData[SubResourceIndex].SysMemSlicePitch =  NumBlocksX * NumBlocksY * SubResourceData[MipIndex].SysMemPitch;

				MipOffset                                  += NumBlocksY * SubResourceData[MipIndex].SysMemPitch;
			}
			SliceOffset += MipOffset;
		}

		pSubresourceData = SubResourceData.GetData();
	}

#if INTEL_EXTENSIONS
	if (EnumHasAnyFlags(Flags, ETextureCreateFlags::Atomic64Compatible) && IsRHIDeviceIntel() && GRHISupportsAtomicUInt64)
	{
		INTC_D3D11_TEXTURE2D_DESC IntelDesc{};
		IntelDesc.EmulatedTyped64bitAtomics = true;
		IntelDesc.pD3D11Desc = &TextureDesc;

		VERIFYD3D11RESULT(INTC_D3D11_CreateTexture2D(IntelExtensionContext, &IntelDesc, pSubresourceData, TextureResource.GetInitReference()));
	}
	else
#endif
	{
		SafeCreateTexture2D(Direct3DDevice, Format, &TextureDesc, pSubresourceData, TextureResource.GetInitReference(), CreateDesc.DebugName);
	}

	if (bCreateRTV)
	{
		// Create a render target view for each mip
		for (uint32 MipIndex = 0; MipIndex < NumMips; MipIndex++)
		{
			if (EnumHasAnyFlags(Flags, TexCreate_TargetArraySlicesIndependently) && (bTextureArray || bCubeTexture))
			{
				bCreatedRTVPerSlice = true;

				for (uint32 SliceIndex = 0; SliceIndex < TextureDesc.ArraySize; SliceIndex++)
				{
					D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
					FMemory::Memzero(RTVDesc);

					RTVDesc.Format = PlatformRenderTargetFormat;

					if (bIsMultisampled)
					{
						RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
						RTVDesc.Texture2DMSArray.FirstArraySlice = SliceIndex;
						RTVDesc.Texture2DMSArray.ArraySize = 1;
					}
					else
					{
						RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
						RTVDesc.Texture2DArray.FirstArraySlice = SliceIndex;
						RTVDesc.Texture2DArray.ArraySize = 1;
						RTVDesc.Texture2DArray.MipSlice = MipIndex;
					}

					TRefCountPtr<ID3D11RenderTargetView> RenderTargetView;
					VERIFYD3D11RESULT_EX(Direct3DDevice->CreateRenderTargetView(TextureResource,&RTVDesc,RenderTargetView.GetInitReference()), Direct3DDevice);
					RenderTargetViews.Add(RenderTargetView);
				}
			}
			else
			{
				D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
				FMemory::Memzero(RTVDesc);

				RTVDesc.Format = PlatformRenderTargetFormat;

				if (bTextureArray || bCubeTexture)
				{
					if (bIsMultisampled)
					{
						RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
						RTVDesc.Texture2DMSArray.FirstArraySlice = 0;
						RTVDesc.Texture2DMSArray.ArraySize = TextureDesc.ArraySize;
					}
					else
					{
						RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
						RTVDesc.Texture2DArray.FirstArraySlice = 0;
						RTVDesc.Texture2DArray.ArraySize = TextureDesc.ArraySize;
						RTVDesc.Texture2DArray.MipSlice = MipIndex;
					}
				}
				else
				{
					if (bIsMultisampled)
					{
						RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
						// Nothing to set
					}
					else
					{
						RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
						RTVDesc.Texture2D.MipSlice = MipIndex;
					}
				}

				TRefCountPtr<ID3D11RenderTargetView> RenderTargetView;
				VERIFYD3D11RESULT_EX(Direct3DDevice->CreateRenderTargetView(TextureResource,&RTVDesc,RenderTargetView.GetInitReference()), Direct3DDevice);
				RenderTargetViews.Add(RenderTargetView);
			}
		}
	}
	
	if (bCreateDSV)
	{
		// Create a depth-stencil-view for the texture.
		D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc;
		FMemory::Memzero(DSVDesc);

		DSVDesc.Format = UE::DXGIUtilities::FindDepthStencilFormat(PlatformResourceFormat);

		if (bTextureArray || bCubeTexture)
		{
			if (bIsMultisampled)
			{
				DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
				DSVDesc.Texture2DMSArray.FirstArraySlice = 0;
				DSVDesc.Texture2DMSArray.ArraySize = TextureDesc.ArraySize;
			}
			else
			{
				DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
				DSVDesc.Texture2DArray.FirstArraySlice = 0;
				DSVDesc.Texture2DArray.ArraySize = TextureDesc.ArraySize;
				DSVDesc.Texture2DArray.MipSlice = 0;
			}
		}
		else
		{
			if (bIsMultisampled)
			{
				DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
				// Nothing to set
			}
			else
			{
				DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
				DSVDesc.Texture2D.MipSlice = 0;
			}
		}

		for (uint32 AccessType = 0; AccessType < FExclusiveDepthStencil::MaxIndex; ++AccessType)
		{
			// Create a read-only access views for the texture.
			// Read-only DSVs are not supported in Feature Level 10 so 
			// a dummy DSV is created in order reduce logic complexity at a higher-level.
			DSVDesc.Flags = (AccessType & FExclusiveDepthStencil::DepthRead_StencilWrite) ? D3D11_DSV_READ_ONLY_DEPTH : 0;
			if(UE::DXGIUtilities::HasStencilBits(DSVDesc.Format))
			{
				DSVDesc.Flags |= (AccessType & FExclusiveDepthStencil::DepthWrite_StencilRead) ? D3D11_DSV_READ_ONLY_STENCIL : 0;
			}
			VERIFYD3D11RESULT_EX(Direct3DDevice->CreateDepthStencilView(TextureResource,&DSVDesc,DepthStencilViews[AccessType].GetInitReference()), Direct3DDevice);
		}
	}
	check(IsValidRef(TextureResource));

	// Create a shader resource view for the texture.
	if (bCreateShaderResource)
	{
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			FMemory::Memzero(SRVDesc);

			SRVDesc.Format = PlatformShaderResourceFormat;

			if (bCubeTexture && bTextureArray)
			{
				SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
				SRVDesc.TextureCubeArray.MostDetailedMip = 0;
				SRVDesc.TextureCubeArray.MipLevels = NumMips;
				SRVDesc.TextureCubeArray.First2DArrayFace = 0;
				SRVDesc.TextureCubeArray.NumCubes = SizeZ / 6;
			}
			else if(bCubeTexture)
			{
				SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
				SRVDesc.TextureCube.MostDetailedMip = 0;
				SRVDesc.TextureCube.MipLevels = NumMips;
			}
			else if(bTextureArray)
			{
				if (bIsMultisampled)
				{
					SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
					SRVDesc.Texture2DMSArray.FirstArraySlice = 0;
					SRVDesc.Texture2DMSArray.ArraySize = TextureDesc.ArraySize;
				}
				else
				{
					SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
					SRVDesc.Texture2DArray.MostDetailedMip = 0;
					SRVDesc.Texture2DArray.MipLevels = NumMips;
					SRVDesc.Texture2DArray.FirstArraySlice = 0;
					SRVDesc.Texture2DArray.ArraySize = TextureDesc.ArraySize;
				}
			}
			else
			{
				if (bIsMultisampled)
				{
					SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
					// Nothing to set
				}
				else
				{
					SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
					SRVDesc.Texture2D.MostDetailedMip = 0;
					SRVDesc.Texture2D.MipLevels = NumMips;
				}
			}
			VERIFYD3D11RESULT_EX(Direct3DDevice->CreateShaderResourceView(TextureResource,&SRVDesc,ShaderResourceView.GetInitReference()), Direct3DDevice);
		}

		check(IsValidRef(ShaderResourceView));
	}

	FD3D11Texture* Texture2D = new FD3D11Texture(
		CreateDesc,
		TextureResource,
		ShaderResourceView,
		TextureDesc.ArraySize,
		bCreatedRTVPerSlice,
		RenderTargetViews,
		DepthStencilViews
	);

	if (CreateDesc.BulkData)
	{
		CreateDesc.BulkData->Discard();
	}

	return Texture2D;
}

FD3D11Texture* FD3D11DynamicRHI::CreateD3D11Texture3D(FRHITextureCreateDesc const& CreateDesc)
{
	check(CreateDesc.IsTexture3D());
	check(CreateDesc.ArraySize == 1);
	const uint32              SizeX   = CreateDesc.Extent.X;
	const uint32              SizeY   = CreateDesc.Extent.Y;
	const uint32              SizeZ   = CreateDesc.Depth;
	const EPixelFormat        Format  = CreateDesc.Format;
	const uint32              NumMips = CreateDesc.NumMips;
	const ETextureCreateFlags Flags   = CreateDesc.Flags;

	SCOPE_CYCLE_COUNTER(STAT_D3D11CreateTextureTime);
	
	const bool bSRGB = EnumHasAnyFlags(Flags, TexCreate_SRGB);

	const DXGI_FORMAT PlatformResourceFormat = (DXGI_FORMAT)GPixelFormats[Format].PlatformFormat;
	const DXGI_FORMAT PlatformShaderResourceFormat = UE::DXGIUtilities::FindShaderResourceFormat(PlatformResourceFormat, bSRGB);
	const DXGI_FORMAT PlatformRenderTargetFormat = UE::DXGIUtilities::FindShaderResourceFormat(PlatformResourceFormat, bSRGB);

	// Describe the texture.
	D3D11_TEXTURE3D_DESC TextureDesc;
	ZeroMemory( &TextureDesc, sizeof( D3D11_TEXTURE3D_DESC ) );
	TextureDesc.Width = SizeX;
	TextureDesc.Height = SizeY;
	TextureDesc.Depth = SizeZ;
	TextureDesc.MipLevels = NumMips;
	TextureDesc.Format = PlatformResourceFormat;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	TextureDesc.CPUAccessFlags = 0;
	TextureDesc.MiscFlags = 0;

	if (EnumHasAnyFlags(Flags, TexCreate_Shared))
	{
		if (GCVarUseSharedKeyedMutex->GetInt() != 0)
		{
			TextureDesc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
		}
		else
		{
			TextureDesc.MiscFlags |= D3D11_RESOURCE_MISC_SHARED;
		}
	}

	if (EnumHasAnyFlags(Flags, TexCreate_GenerateMipCapable))
	{
		// Set the flag that allows us to call GenerateMips on this texture later
		TextureDesc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
	}

	if (EnumHasAnyFlags(Flags, TexCreate_UAV))
	{
		TextureDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}

	bool bCreateRTV = false;

	if(EnumHasAnyFlags(Flags, TexCreate_RenderTargetable))
	{
		TextureDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;		
		bCreateRTV = true;
	}

	bool bCreateShaderResource = true;

	if (EnumHasAnyFlags(Flags, TexCreate_CPUReadback))
	{
		check(!EnumHasAnyFlags(Flags, TexCreate_RenderTargetable | TexCreate_DepthStencilTargetable | TexCreate_ShaderResource));

		TextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		TextureDesc.Usage = D3D11_USAGE_STAGING;
		TextureDesc.BindFlags = 0;
		bCreateShaderResource = false;
	}

	if (EnumHasAnyFlags(Flags, TexCreate_DisableSRVCreation))
	{
		bCreateShaderResource = false;
	}

	// Set up the texture bind flags.
	check(!EnumHasAnyFlags(Flags, TexCreate_DepthStencilTargetable | TexCreate_ResolveTargetable));

	TArray<D3D11_SUBRESOURCE_DATA> SubResourceData;

	if (CreateDesc.BulkData)
	{
		uint8* Data = (uint8*)CreateDesc.BulkData->GetResourceBulkData();
		SubResourceData.AddZeroed(NumMips);
		uint32 MipOffset = 0;
		for(uint32 MipIndex = 0;MipIndex < NumMips;++MipIndex)
		{
			SubResourceData[MipIndex].pSysMem = &Data[MipOffset];
			SubResourceData[MipIndex].SysMemPitch      =  FMath::Max<uint32>(1,SizeX >> MipIndex) * GPixelFormats[Format].BlockBytes;
			SubResourceData[MipIndex].SysMemSlicePitch =  FMath::Max<uint32>(1,SizeY >> MipIndex) * SubResourceData[MipIndex].SysMemPitch;
			MipOffset                                  += FMath::Max<uint32>(1,SizeZ >> MipIndex) * SubResourceData[MipIndex].SysMemSlicePitch;
		}
	}

	TRefCountPtr<ID3D11Texture3D> TextureResource;
	const D3D11_SUBRESOURCE_DATA* SubResData = CreateDesc.BulkData != nullptr ? (const D3D11_SUBRESOURCE_DATA*)SubResourceData.GetData() : nullptr;
	VERIFYD3D11CREATETEXTURERESULT(
		Direct3DDevice->CreateTexture3D(&TextureDesc, SubResData,TextureResource.GetInitReference()),
		Format,
		SizeX,
		SizeY,
		SizeZ,
		PlatformShaderResourceFormat,
		NumMips,
		TextureDesc.BindFlags,
		TextureDesc.Usage,
		TextureDesc.CPUAccessFlags,
		TextureDesc.MiscFlags,
		0,
		0,
		SubResData ? SubResData->pSysMem : nullptr,
		SubResData ? SubResData->SysMemPitch : 0,
		SubResData ? SubResData->SysMemSlicePitch : 0,
		Direct3DDevice,
		CreateDesc.DebugName
		);

	// Create a shader resource view for the texture.
	TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;
	if(bCreateShaderResource)
	{
		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		SRVDesc.Format = PlatformShaderResourceFormat;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		SRVDesc.Texture3D.MipLevels = NumMips;
		SRVDesc.Texture3D.MostDetailedMip = 0;
		VERIFYD3D11RESULT_EX(Direct3DDevice->CreateShaderResourceView(TextureResource,&SRVDesc,ShaderResourceView.GetInitReference()), Direct3DDevice);
	}

	TRefCountPtr<ID3D11RenderTargetView> RenderTargetView;
	if(bCreateRTV)
	{
		// Create a render-target-view for the texture.
		D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
		FMemory::Memzero(&RTVDesc,sizeof(RTVDesc));
		RTVDesc.Format = PlatformRenderTargetFormat;
		RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
		RTVDesc.Texture3D.MipSlice = 0;
		RTVDesc.Texture3D.FirstWSlice = 0;
		RTVDesc.Texture3D.WSize = SizeZ;

		VERIFYD3D11RESULT_EX(Direct3DDevice->CreateRenderTargetView(TextureResource,&RTVDesc,RenderTargetView.GetInitReference()), Direct3DDevice);
	}

	FD3D11Texture* Texture3D = new FD3D11Texture(
		CreateDesc,
		TextureResource,
		ShaderResourceView,
		1,     // InRTVArraySize
		false, // bInCreatedRTVsPerSlice
		{ RenderTargetView },
		{}
	);

	if (CreateDesc.BulkData)
	{
		CreateDesc.BulkData->Discard();
	}

	return Texture3D;
}

/*-----------------------------------------------------------------------------
	2D texture support.
-----------------------------------------------------------------------------*/

FTextureRHIRef FD3D11DynamicRHI::RHICreateTexture(FRHICommandListBase&, const FRHITextureCreateDesc& CreateDesc)
{
	return CreateDesc.IsTexture3D()
		? CreateD3D11Texture3D(CreateDesc)
		: CreateD3D11Texture2D(CreateDesc);
}

FTextureRHIRef FD3D11DynamicRHI::RHIAsyncCreateTexture2D(uint32 SizeX, uint32 SizeY, uint8 Format, uint32 NumMips, ETextureCreateFlags Flags, ERHIAccess InResourceState, void** InitialMipData, uint32 NumInitialMips, const TCHAR* DebugName, FGraphEventRef& OutCompletionEvent)
{
	TArray<D3D11_SUBRESOURCE_DATA, TInlineAllocator<12>> SubresourceData;
	SubresourceData.SetNumUninitialized(NumMips);

	for (uint32 MipIndex = 0; MipIndex < NumInitialMips; ++MipIndex)
	{
		uint32 NumBlocksX = FMath::Max<uint32>(1, ((SizeX >> MipIndex) +GPixelFormats[Format].BlockSizeX-1) / GPixelFormats[Format].BlockSizeX);
		uint32 NumBlocksY = FMath::Max<uint32>(1, ((SizeY >> MipIndex) +GPixelFormats[Format].BlockSizeY-1) / GPixelFormats[Format].BlockSizeY);

		SubresourceData[MipIndex].pSysMem = InitialMipData[MipIndex];
		SubresourceData[MipIndex].SysMemPitch = NumBlocksX * GPixelFormats[Format].BlockBytes;
		SubresourceData[MipIndex].SysMemSlicePitch = NumBlocksX * NumBlocksY * GPixelFormats[Format].BlockBytes;
	}

	void* TempBuffer = ZeroBuffer;
	uint32 TempBufferSize = ZeroBufferSize;
	for (uint32 MipIndex = NumInitialMips; MipIndex < NumMips; ++MipIndex)
	{
		uint32 NumBlocksX = FMath::Max<uint32>(1, ((SizeX >> MipIndex) +GPixelFormats[Format].BlockSizeX-1) / GPixelFormats[Format].BlockSizeX);
		uint32 NumBlocksY = FMath::Max<uint32>(1, ((SizeY >> MipIndex) +GPixelFormats[Format].BlockSizeY-1) / GPixelFormats[Format].BlockSizeY);
		uint32 MipSize = NumBlocksX * NumBlocksY * GPixelFormats[Format].BlockBytes;

		if (MipSize > TempBufferSize)
		{
			UE_LOG(LogD3D11RHI, Display, TEXT("Temp texture streaming buffer not large enough, needed %d bytes"), MipSize);
			check(TempBufferSize == ZeroBufferSize);
			TempBufferSize = MipSize;
			TempBuffer = FMemory::Malloc(TempBufferSize);
			FMemory::Memzero(TempBuffer, TempBufferSize);
		}

		SubresourceData[MipIndex].pSysMem = TempBuffer;
		SubresourceData[MipIndex].SysMemPitch = NumBlocksX * GPixelFormats[Format].BlockBytes;
		SubresourceData[MipIndex].SysMemSlicePitch = MipSize;
	}

	const FRHITextureCreateDesc Desc =
		FRHITextureCreateDesc::Create2D(DebugName, SizeX, SizeY, (EPixelFormat)Format)
		.SetClearValue(FClearValueBinding::None)
		.SetFlags(Flags)
		.SetNumMips(NumMips)
		.DetermineInititialState();

	FTextureRHIRef Texture = CreateD3D11Texture2D(Desc, SubresourceData);

	if (TempBufferSize != ZeroBufferSize)
	{
		FMemory::Free(TempBuffer);
	}

	OutCompletionEvent = nullptr;

	return Texture;
}

/** Generates mip maps for the surface. */
void FD3D11DynamicRHI::RHIGenerateMips(FRHITexture* TextureRHI)
{
	FD3D11Texture* Texture = ResourceCast(TextureRHI);
	// Surface must have been created with D3D11_BIND_RENDER_TARGET for GenerateMips to work
	check(Texture->GetShaderResourceView() && Texture->GetRenderTargetView(0, -1));
	Direct3DDeviceIMContext->GenerateMips(Texture->GetShaderResourceView());

	GPUProfilingData.RegisterGPUWork(0);
}

/**
 * Computes the size in memory required by a given texture.
 *
 * @param	TextureRHI		- Texture we want to know the size of
 * @return					- Size in Bytes
 */
uint32 FD3D11DynamicRHI::RHIComputeMemorySize(FRHITexture* TextureRHI)
{
	if(!TextureRHI)
	{
		return 0;
	}
	
	FD3D11Texture* Texture = ResourceCast(TextureRHI);
	return Texture->GetMemorySize();
}

/**
 * Asynchronous texture copy helper
 *
 * @param NewTexture2DRHI		- Texture to reallocate
 * @param Texture2DRHI			- Texture to reallocate
 * @param NewMipCount			- New number of mip-levels
 * @param NewSizeX				- New width, in pixels
 * @param NewSizeY				- New height, in pixels
 * @param RequestStatus			- Will be decremented by 1 when the reallocation is complete (success or failure).
 * @return						- New reference to the texture, or an invalid reference upon failure
 */
void FD3D11DynamicRHI::RHIAsyncCopyTexture2DCopy(FRHITexture2D* NewTexture2DRHI, FRHITexture2D* Texture2DRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	FD3D11Texture* Texture2D = ResourceCast(Texture2DRHI);
	FD3D11Texture* NewTexture2D = ResourceCast(NewTexture2DRHI);

	// Use the GPU to asynchronously copy the old mip-maps into the new texture.
	const uint32 NumSharedMips = FMath::Min(Texture2D->GetNumMips(), NewTexture2D->GetNumMips());
	const uint32 SourceMipOffset = Texture2D->GetNumMips() - NumSharedMips;
	const uint32 DestMipOffset = NewTexture2D->GetNumMips() - NumSharedMips;
	for (uint32 MipIndex = 0; MipIndex < NumSharedMips; ++MipIndex)
	{
		// Use the GPU to copy between mip-maps.
		// This is serialized with other D3D commands, so it isn't necessary to increment Counter to signal a pending asynchronous copy.
		Direct3DDeviceIMContext->CopySubresourceRegion(
			NewTexture2D->GetResource(),
			D3D11CalcSubresource(MipIndex + DestMipOffset, 0, NewTexture2D->GetNumMips()),
			0,
			0,
			0,
			Texture2D->GetResource(),
			D3D11CalcSubresource(MipIndex + SourceMipOffset, 0, Texture2D->GetNumMips()),
			NULL
		);
	}

	// Decrement the thread-safe counter used to track the completion of the reallocation, since D3D handles sequencing the
	// async mip copies with other D3D calls.
	RequestStatus->Decrement();
}


/**
 * Starts an asynchronous texture reallocation. It may complete immediately if the reallocation
 * could be performed without any reshuffling of texture memory, or if there isn't enough memory.
 * The specified status counter will be decremented by 1 when the reallocation is complete (success or failure).
 *
 * Returns a new reference to the texture, which will represent the new mip count when the reallocation is complete.
 * RHIGetAsyncReallocateTexture2DStatus() can be used to check the status of an ongoing or completed reallocation.
 *
 * @param Texture2D		- Texture to reallocate
 * @param NewMipCount	- New number of mip-levels
 * @param NewSizeX		- New width, in pixels
 * @param NewSizeY		- New height, in pixels
 * @param RequestStatus	- Will be decremented by 1 when the reallocation is complete (success or failure).
 * @return				- New reference to the texture, or an invalid reference upon failure
 */
FTexture2DRHIRef FD3D11DynamicRHI::RHIAsyncReallocateTexture2D(FRHITexture2D* Texture2DRHI, int32 NewMipCount, int32 NewSizeX, int32 NewSizeY, FThreadSafeCounter* RequestStatus)
{
	FD3D11Texture* Texture2D = ResourceCast(Texture2DRHI);

	FRHITextureCreateDesc CreateDesc(
		Texture2D->GetDesc(),
		RHIGetDefaultResourceState(Texture2D->GetDesc().Flags, false),
		TEXT("RHIAsyncReallocateTexture2D")
	);
	CreateDesc.Extent = FIntPoint(NewSizeX, NewSizeY);
	CreateDesc.NumMips = NewMipCount;

	// Allocate a new texture.
	FD3D11Texture* NewTexture2D = CreateD3D11Texture2D(CreateDesc);

	RHIAsyncCopyTexture2DCopy(NewTexture2D, Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus);

	return NewTexture2D;
}

FTexture2DRHIRef FD3D11DynamicRHI::AsyncReallocateTexture2D_RenderThread(
	class FRHICommandListImmediate& RHICmdList,
	FRHITexture2D* Texture2D,
	int32 NewMipCount,
	int32 NewSizeX,
	int32 NewSizeY,
	FThreadSafeCounter* RequestStatus)
{
	FTexture2DRHIRef NewTexture2D;

	if (ShouldNotEnqueueRHICommand())
	{
		NewTexture2D = RHIAsyncReallocateTexture2D(Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
	}
	else
	{
		// Allocate a new texture.
		FRHITextureCreateDesc CreateDesc(
			Texture2D->GetDesc(),
			RHIGetDefaultResourceState(Texture2D->GetDesc().Flags, false),
			TEXT("AsyncReallocateTexture2D_RenderThread")
		);
		CreateDesc.Extent = FIntPoint(NewSizeX, NewSizeY);
		CreateDesc.NumMips = NewMipCount;

		NewTexture2D = CreateD3D11Texture2D(CreateDesc);

		RunOnRHIThread([this, NewTexture2D, Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus]()
		{
			RHIAsyncCopyTexture2DCopy(NewTexture2D, Texture2D, NewMipCount, NewSizeX, NewSizeY, RequestStatus);
		});
	}
	return NewTexture2D;
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
ETextureReallocationStatus FD3D11DynamicRHI::RHIFinalizeAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Succeeded;
}

ETextureReallocationStatus FD3D11DynamicRHI::FinalizeAsyncReallocateTexture2D_RenderThread(
	class FRHICommandListImmediate& RHICmdList,
	FRHITexture2D* Texture2D,
	bool bBlockUntilCompleted)
{
	return RHIFinalizeAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

/**
 * Cancels an async reallocation for the specified texture.
 * This should be called for the new texture, not the original.
 *
 * @param Texture				Texture to cancel
 * @param bBlockUntilCompleted	If true, blocks until the cancellation is fully completed
 * @return						Reallocation status
 */
ETextureReallocationStatus FD3D11DynamicRHI::RHICancelAsyncReallocateTexture2D(FRHITexture2D* Texture2D, bool bBlockUntilCompleted )
{
	return TexRealloc_Succeeded;
}

ETextureReallocationStatus FD3D11DynamicRHI::CancelAsyncReallocateTexture2D_RenderThread(
	class FRHICommandListImmediate& RHICmdList,
	FRHITexture2D* Texture2D,
	bool bBlockUntilCompleted)
{
	return RHICancelAsyncReallocateTexture2D(Texture2D, bBlockUntilCompleted);
}

void* FD3D11Texture::Lock(FD3D11DynamicRHI* D3DRHI, uint32 MipIndex, uint32 ArrayIndex, EResourceLockMode LockMode, uint32& DestStride, bool bForceLockDeferred, uint64* OutLockedByteCount)
{
	check(!IsTexture3D()); // Only 2D texture locks are implemented

	SCOPE_CYCLE_COUNTER(STAT_D3D11LockTextureTime);

	// Calculate the subresource index corresponding to the specified mip-map.
	const uint32 Subresource = D3D11CalcSubresource(MipIndex,ArrayIndex,this->GetNumMips());

	// Calculate the dimensions of the mip-map.
	const uint32 BlockSizeX = GPixelFormats[this->GetFormat()].BlockSizeX;
	const uint32 BlockSizeY = GPixelFormats[this->GetFormat()].BlockSizeY;
	const uint32 BlockBytes = GPixelFormats[this->GetFormat()].BlockBytes;
	const uint32 MipSizeX = FMath::Max(this->GetSizeX() >> MipIndex,BlockSizeX);
	const uint32 MipSizeY = FMath::Max(this->GetSizeY() >> MipIndex,BlockSizeY);
	const uint32 NumBlocksX = (MipSizeX + BlockSizeX - 1) / BlockSizeX;
	const uint32 NumBlocksY = (MipSizeY + BlockSizeY - 1) / BlockSizeY;
	const uint32 MipBytes = NumBlocksX * NumBlocksY * BlockBytes;

	if (OutLockedByteCount)
	{
		*OutLockedByteCount = MipBytes;
	}

	FD3D11LockedData LockedData;
	if( LockMode == RLM_WriteOnly )
	{
		if (!bForceLockDeferred && EnumHasAnyFlags(GetDesc().Flags, TexCreate_CPUWritable))
		{
			D3D11_MAPPED_SUBRESOURCE MappedTexture;
			VERIFYD3D11RESULT_EX(D3DRHI->GetDeviceContext()->Map(GetResource(), Subresource, D3D11_MAP_WRITE, 0, &MappedTexture), D3DRHI->GetDevice());
			LockedData.SetData(MappedTexture.pData);
			LockedData.Pitch = DestStride = MappedTexture.RowPitch;
		}
		else
		{
			// If we're writing to the texture, allocate a system memory buffer to receive the new contents.
			LockedData.AllocData(MipBytes);
			LockedData.Pitch = DestStride = NumBlocksX * BlockBytes;
			LockedData.bLockDeferred = true;
		}
	}
	else
	{
		check(!bForceLockDeferred);
		// If we're reading from the texture, we create a staging resource, copy the texture contents to it, and map it.

		// Create the staging texture.
		D3D11_TEXTURE2D_DESC StagingTextureDesc;
		GetD3D11Texture2D()->GetDesc(&StagingTextureDesc);
		StagingTextureDesc.Width = MipSizeX;
		StagingTextureDesc.Height = MipSizeY;
		StagingTextureDesc.MipLevels = 1;
		StagingTextureDesc.ArraySize = 1;
		StagingTextureDesc.Usage = D3D11_USAGE_STAGING;
		StagingTextureDesc.BindFlags = 0;
		StagingTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		StagingTextureDesc.MiscFlags = 0;
		TRefCountPtr<ID3D11Texture2D> StagingTexture;
		VERIFYD3D11CREATETEXTURERESULT(
			D3DRHI->GetDevice()->CreateTexture2D(&StagingTextureDesc,NULL,StagingTexture.GetInitReference()),
			GetDesc().Format,
			this->GetSizeX(),
			this->GetSizeY(),
			this->GetSizeZ(),
			StagingTextureDesc.Format,
			1,
			0,
			StagingTextureDesc.Usage,
			StagingTextureDesc.CPUAccessFlags,
			StagingTextureDesc.MiscFlags,
			StagingTextureDesc.SampleDesc.Count,
			StagingTextureDesc.SampleDesc.Quality,
			nullptr,
			0,
			0,
			D3DRHI->GetDevice(),
			*FString::Printf(TEXT("%s_Staging"), *GetName().ToString())
			);
		LockedData.StagingResource = StagingTexture;

		// Copy the mip-map data from the real resource into the staging resource
		D3DRHI->GetDeviceContext()->CopySubresourceRegion(StagingTexture,0,0,0,0,GetResource(),Subresource,NULL);

		// Map the staging resource, and return the mapped address.
		D3D11_MAPPED_SUBRESOURCE MappedTexture;
		VERIFYD3D11RESULT_EX(D3DRHI->GetDeviceContext()->Map(StagingTexture,0,D3D11_MAP_READ,0,&MappedTexture), D3DRHI->GetDevice());
		LockedData.SetData(MappedTexture.pData);
		LockedData.Pitch = DestStride = MappedTexture.RowPitch;
	}

	// Add the lock to the outstanding lock list.
	if (!bForceLockDeferred)
	{
		D3DRHI->AddLockedData(FD3D11LockedKey(GetResource(), Subresource), LockedData);
	}
	else
	{
		RunOnRHIThread([this, Subresource, LockedData, D3DRHI]()
		{
			D3DRHI->AddLockedData(FD3D11LockedKey(GetResource(), Subresource), LockedData);
		});
	}

	return (void*)LockedData.GetData();
}

void FD3D11Texture::Unlock(FD3D11DynamicRHI* D3DRHI, uint32 MipIndex, uint32 ArrayIndex)
{
	check(!IsTexture3D()); // Only 2D texture locks are implemented

	SCOPE_CYCLE_COUNTER(STAT_D3D11UnlockTextureTime);

	// Calculate the subresource index corresponding to the specified mip-map.
	const uint32 Subresource = D3D11CalcSubresource(MipIndex,ArrayIndex,this->GetNumMips());

	// Find the object that is tracking this lock and remove it from outstanding list
	FD3D11LockedData LockedData;
	verifyf(D3DRHI->RemoveLockedData(FD3D11LockedKey(GetResource(), Subresource), LockedData), TEXT("Texture is not locked"));

	if (!LockedData.bLockDeferred && EnumHasAnyFlags(GetDesc().Flags, TexCreate_CPUWritable))
	{
		D3DRHI->GetDeviceContext()->Unmap(GetResource(), 0);
	}
	else if(!LockedData.StagingResource)
	{
		// If we're writing, we need to update the subresource
		D3DRHI->GetDeviceContext()->UpdateSubresource(GetResource(), Subresource, NULL, LockedData.GetData(), LockedData.Pitch, 0);
		LockedData.FreeData();
	}
	else
	{
		D3DRHI->GetDeviceContext()->Unmap(LockedData.StagingResource, 0);
	}
}

void* FD3D11DynamicRHI::RHILockTexture2D(FRHITexture2D* TextureRHI, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail, uint64* OutLockedByteCount)
{
	check(TextureRHI);
	FD3D11Texture* Texture = ResourceCast(TextureRHI);
	ConditionalClearShaderResource(Texture, false);
	return Texture->Lock(this, MipIndex, 0, LockMode, DestStride, false /* bForceLockDeferred */, OutLockedByteCount);
}

void* FD3D11DynamicRHI::LockTexture2D_RenderThread(
	class FRHICommandListImmediate& RHICmdList,
	FRHITexture2D* Texture,
	uint32 MipIndex,
	EResourceLockMode LockMode,
	uint32& DestStride,
	bool bLockWithinMiptail,
	bool bNeedsDefaultRHIFlush,
	uint64* OutLockedByteCount)
{
	void *LockedTexture = nullptr;

	if (ShouldNotEnqueueRHICommand())
	{
		LockedTexture = RHILockTexture2D(Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail, OutLockedByteCount);
	}
	else if (LockMode == RLM_ReadOnly)
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		LockedTexture = RHILockTexture2D(Texture, MipIndex, LockMode, DestStride, bLockWithinMiptail, OutLockedByteCount);
	}
	else
	{
		FD3D11Texture* TextureD3D11 = ResourceCast(Texture);
		LockedTexture = TextureD3D11->Lock(this, MipIndex, 0, LockMode, DestStride, true, OutLockedByteCount);
	}
	return LockedTexture;
}

void FD3D11DynamicRHI::RHIUnlockTexture2D(FRHITexture2D* TextureRHI, uint32 MipIndex, bool bLockWithinMiptail)
{
	check(TextureRHI);
	FD3D11Texture* Texture = ResourceCast(TextureRHI);
	Texture->Unlock(this, MipIndex, 0);
}

void FD3D11DynamicRHI::UnlockTexture2D_RenderThread(
	class FRHICommandListImmediate& RHICmdList,
	FRHITexture2D* Texture,
	uint32 MipIndex,
	bool bLockWithinMiptail,
	bool bNeedsDefaultRHIFlush)
{
	if (ShouldNotEnqueueRHICommand())
	{
		RHIUnlockTexture2D(Texture, MipIndex, bLockWithinMiptail);
	}
	else
	{
		RunOnRHIThread([this, Texture, MipIndex, bLockWithinMiptail]()
		{
			RHIUnlockTexture2D(Texture, MipIndex, bLockWithinMiptail);
		});
	}
}

void* FD3D11DynamicRHI::RHILockTexture2DArray(FRHITexture2DArray* TextureRHI, uint32 TextureIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	FD3D11Texture* Texture = ResourceCast(TextureRHI);
	ConditionalClearShaderResource(Texture, false);
	return Texture->Lock(this, MipIndex, TextureIndex, LockMode, DestStride);
}

void FD3D11DynamicRHI::RHIUnlockTexture2DArray(FRHITexture2DArray* TextureRHI, uint32 TextureIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	FD3D11Texture* Texture = ResourceCast(TextureRHI);
	Texture->Unlock(this, MipIndex, TextureIndex);
}

void FD3D11DynamicRHI::RHIUpdateTexture2D(FRHICommandListBase& RHICmdList, FRHITexture2D* TextureRHI, uint32 MipIndex, const FUpdateTextureRegion2D& UpdateRegion, uint32 SourcePitch, const uint8* SourceData)
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

	const bool bNeedStagingMemory = RHICmdList.IsTopOfPipe();
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

	RHICmdList.EnqueueLambda([this, TextureRHI, MipIndex, UpdateRegion, UpdatePitch, UpdateMemory, bNeedStagingMemory] (FRHICommandListBase&)
	{
		FD3D11Texture* Texture = ResourceCast(TextureRHI);

		D3D11_BOX DestBox =
		{
			UpdateRegion.DestX,                      UpdateRegion.DestY,                       0,
			UpdateRegion.DestX + UpdateRegion.Width, UpdateRegion.DestY + UpdateRegion.Height, 1
		};

		Direct3DDeviceIMContext->UpdateSubresource(Texture->GetResource(), MipIndex, &DestBox, UpdateMemory, UpdatePitch, 0);

		if (bNeedStagingMemory)
		{
			FMemory::Free(const_cast<void*>(UpdateMemory));
		}
	});
}

void FD3D11DynamicRHI::RHIUpdateTexture3D(FRHICommandListBase& RHICmdList, FRHITexture3D* TextureRHI,uint32 MipIndex,const FUpdateTextureRegion3D& UpdateRegion,uint32 SourceRowPitch,uint32 SourceDepthPitch,const uint8* SourceData)
{
	const SIZE_T SourceDataSize = static_cast<SIZE_T>(SourceDepthPitch) * UpdateRegion.Depth;
	uint8* SourceDataCopy = (uint8*)FMemory::Malloc(SourceDataSize);
	FMemory::Memcpy(SourceDataCopy, SourceData, SourceDataSize);
	SourceData = SourceDataCopy;

	RHICmdList.EnqueueLambda([this, TextureRHI, MipIndex, UpdateRegion, SourceRowPitch, SourceDepthPitch, SourceData] (FRHICommandListBase&)
	{
		FD3D11Texture* Texture = ResourceCast(TextureRHI);

		// The engine calls this with the texture size in the region. 
		// Some platforms like D3D11 needs that to be rounded up to the block size.
		const FPixelFormatInfo& Format = GPixelFormats[Texture->GetFormat()];
		const int32 NumBlockX = FMath::DivideAndRoundUp<int32>(UpdateRegion.Width, Format.BlockSizeX);
		const int32 NumBlockY = FMath::DivideAndRoundUp<int32>(UpdateRegion.Height, Format.BlockSizeY);

		D3D11_BOX DestBox =
		{
			UpdateRegion.DestX,                                 UpdateRegion.DestY,                                 UpdateRegion.DestZ,
			UpdateRegion.DestX + NumBlockX * Format.BlockSizeX, UpdateRegion.DestY + NumBlockY * Format.BlockSizeY, UpdateRegion.DestZ + UpdateRegion.Depth
		};

		Direct3DDeviceIMContext->UpdateSubresource(Texture->GetResource(), MipIndex, &DestBox, SourceData, SourceRowPitch, SourceDepthPitch);

		FMemory::Free((void*)SourceData);
	});
}

void FD3D11DynamicRHI::RHIEndUpdateTexture3D(FRHICommandListBase& RHICmdList, FUpdateTexture3DData& UpdateData)
{
	RHIUpdateTexture3D(RHICmdList, UpdateData.Texture, UpdateData.MipIndex, UpdateData.UpdateRegion, UpdateData.RowPitch, UpdateData.DepthPitch, UpdateData.Data);
	FMemory::Free(UpdateData.Data);
	UpdateData.Data = nullptr;
}

/*-----------------------------------------------------------------------------
	Cubemap texture support.
-----------------------------------------------------------------------------*/

void* FD3D11DynamicRHI::RHILockTextureCubeFace(FRHITextureCube* TextureCubeRHI, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, EResourceLockMode LockMode, uint32& DestStride, bool bLockWithinMiptail)
{
	FD3D11Texture* TextureCube = ResourceCast(TextureCubeRHI);
	ConditionalClearShaderResource(TextureCube, false);
	uint32 D3DFace = GetD3D11CubeFace((ECubeFace)FaceIndex);
	return TextureCube->Lock(this, MipIndex, D3DFace + ArrayIndex * 6, LockMode, DestStride);
}

void* FD3D11DynamicRHI::RHILockTextureCubeFace_RenderThread(
	class FRHICommandListImmediate& RHICmdList,
	FRHITextureCube* Texture,
	uint32 FaceIndex,
	uint32 ArrayIndex,
	uint32 MipIndex,
	EResourceLockMode LockMode,
	uint32& DestStride,
	bool bLockWithinMiptail)
{
	void *LockedTexture = nullptr;

	if (ShouldNotEnqueueRHICommand())
	{
		LockedTexture = RHILockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
	}
	else if (LockMode == RLM_ReadOnly)
	{
		FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThread);
		LockedTexture = RHILockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, LockMode, DestStride, bLockWithinMiptail);
	}
	else
	{
		FD3D11Texture* TextureCube = ResourceCast(Texture);
		const uint32 D3DFace = GetD3D11CubeFace((ECubeFace)FaceIndex);
		LockedTexture = TextureCube->Lock(this, MipIndex, D3DFace + ArrayIndex * 6, LockMode, DestStride, true);
	}
	return LockedTexture;
}

void FD3D11DynamicRHI::RHIUnlockTextureCubeFace(FRHITextureCube* TextureCubeRHI, uint32 FaceIndex, uint32 ArrayIndex, uint32 MipIndex, bool bLockWithinMiptail)
{
	FD3D11Texture* TextureCube = ResourceCast(TextureCubeRHI);
	uint32 D3DFace = GetD3D11CubeFace((ECubeFace)FaceIndex);
	TextureCube->Unlock(this, MipIndex, D3DFace + ArrayIndex * 6);
}

void FD3D11DynamicRHI::RHIUnlockTextureCubeFace_RenderThread(
	class FRHICommandListImmediate& RHICmdList,
	FRHITextureCube* Texture,
	uint32 FaceIndex,
	uint32 ArrayIndex,
	uint32 MipIndex,
	bool bLockWithinMiptail)
{
	if (ShouldNotEnqueueRHICommand())
	{
		RHIUnlockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, bLockWithinMiptail);
	}
	else
	{
		RunOnRHIThread([this, Texture, FaceIndex, ArrayIndex, MipIndex, bLockWithinMiptail]()
		{
			RHIUnlockTextureCubeFace(Texture, FaceIndex, ArrayIndex, MipIndex, bLockWithinMiptail);
		});
	}
}

void FD3D11DynamicRHI::RHIBindDebugLabelName(FRHICommandListBase& RHICmdList, FRHITexture* TextureRHI, const TCHAR* Name)
{
	FD3D11Texture* Texture = ResourceCast(TextureRHI);

	//todo: require names at texture creation time.
	FName DebugName(Name);
	Texture->SetName(DebugName);
#if UE_BUILD_DEBUG || UE_BUILD_DEVELOPMENT
	ID3D11Resource* ResourceD3D = Texture->GetResource();
	if (ResourceD3D != nullptr)
	{
		ResourceD3D->SetPrivateData(WKPDID_D3DDebugObjectName, FCString::Strlen(Name) + 1, TCHAR_TO_ANSI(Name));
	}
#endif
}

FD3D11Texture* FD3D11DynamicRHI::CreateTextureFromResource(bool bTextureArray, bool bCubeTexture, EPixelFormat Format, ETextureCreateFlags TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D11Texture2D* TextureResource)
{
	D3D11_TEXTURE2D_DESC TextureDesc;
	TextureResource->GetDesc(&TextureDesc);

	const bool bSRGB = EnumHasAnyFlags(TexCreateFlags, TexCreate_SRGB);

	const DXGI_FORMAT PlatformResourceFormat = UE::DXGIUtilities::GetPlatformTextureResourceFormat((DXGI_FORMAT)GPixelFormats[Format].PlatformFormat, TexCreateFlags);
	const DXGI_FORMAT PlatformShaderResourceFormat = UE::DXGIUtilities::FindShaderResourceFormat(PlatformResourceFormat, bSRGB);
	const DXGI_FORMAT PlatformRenderTargetFormat = UE::DXGIUtilities::FindShaderResourceFormat(PlatformResourceFormat, bSRGB);

	const bool bIsMultisampled = TextureDesc.SampleDesc.Count > 1;

	TRefCountPtr<ID3D11ShaderResourceView> ShaderResourceView;
	TArray<TRefCountPtr<ID3D11RenderTargetView> > RenderTargetViews;
	TRefCountPtr<ID3D11DepthStencilView> DepthStencilViews[FExclusiveDepthStencil::MaxIndex];

	bool bCreateRTV = (TextureDesc.BindFlags & D3D11_BIND_RENDER_TARGET) != 0;
	bool bCreateDSV = (TextureDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL) != 0;
	bool bCreateShaderResource = (TextureDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0;

	// DXGI_FORMAT_NV12 allows us to create RTV and SRV but only with other formats, so we should block creation here.
	// @todo: Should this be a check? Seems wrong to just silently change what the caller asked for.
	if (Format == PF_NV12 || Format == PF_P010)
	{
		bCreateRTV = false;
		bCreateShaderResource = false;
	}

	bool bCreatedRTVPerSlice = false;

	if(bCreateRTV)
	{
		// Create a render target view for each mip
		for (uint32 MipIndex = 0; MipIndex < TextureDesc.MipLevels; MipIndex++)
		{
			if (EnumHasAnyFlags(TexCreateFlags, TexCreate_TargetArraySlicesIndependently) && (bTextureArray || bCubeTexture))
			{
				bCreatedRTVPerSlice = true;

				for (uint32 SliceIndex = 0; SliceIndex < TextureDesc.ArraySize; SliceIndex++)
				{
					D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
					FMemory::Memzero(RTVDesc);

					RTVDesc.Format = PlatformRenderTargetFormat;

					if (bIsMultisampled)
					{
						RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
						RTVDesc.Texture2DMSArray.FirstArraySlice = SliceIndex;
						RTVDesc.Texture2DMSArray.ArraySize = 1;
					}
					else
					{
						RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
						RTVDesc.Texture2DArray.FirstArraySlice = SliceIndex;
						RTVDesc.Texture2DArray.ArraySize = 1;
						RTVDesc.Texture2DArray.MipSlice = MipIndex;
					}

					TRefCountPtr<ID3D11RenderTargetView> RenderTargetView;
					VERIFYD3D11RESULT_EX(Direct3DDevice->CreateRenderTargetView(TextureResource,&RTVDesc,RenderTargetView.GetInitReference()), Direct3DDevice);
					RenderTargetViews.Add(RenderTargetView);
				}
			}
			else
			{
				D3D11_RENDER_TARGET_VIEW_DESC RTVDesc;
				FMemory::Memzero(RTVDesc);

				RTVDesc.Format = PlatformRenderTargetFormat;

				if (bTextureArray || bCubeTexture)
				{
					if (bIsMultisampled)
					{
						RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
						RTVDesc.Texture2DMSArray.FirstArraySlice = 0;
						RTVDesc.Texture2DMSArray.ArraySize = TextureDesc.ArraySize;
					}
					else
					{
						RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
						RTVDesc.Texture2DArray.FirstArraySlice = 0;
						RTVDesc.Texture2DArray.ArraySize = TextureDesc.ArraySize;
						RTVDesc.Texture2DArray.MipSlice = MipIndex;
					}
				}
				else
				{
					if (bIsMultisampled)
					{
						RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
						// Nothing to set
					}
					else
					{
						RTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
						RTVDesc.Texture2D.MipSlice = MipIndex;
					}
				}

				TRefCountPtr<ID3D11RenderTargetView> RenderTargetView;
				VERIFYD3D11RESULT_EX(Direct3DDevice->CreateRenderTargetView(TextureResource,&RTVDesc,RenderTargetView.GetInitReference()), Direct3DDevice);
				RenderTargetViews.Add(RenderTargetView);
			}
		}
	}

	if(bCreateDSV)
	{
		// Create a depth-stencil-view for the texture.
		D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc;
		FMemory::Memzero(DSVDesc);

		DSVDesc.Format = UE::DXGIUtilities::FindDepthStencilFormat(PlatformResourceFormat);

		if (bTextureArray || bCubeTexture)
		{
			if (bIsMultisampled)
			{
				DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
				DSVDesc.Texture2DMSArray.FirstArraySlice = 0;
				DSVDesc.Texture2DMSArray.ArraySize = TextureDesc.ArraySize;
			}
			else
			{
				DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
				DSVDesc.Texture2DArray.FirstArraySlice = 0;
				DSVDesc.Texture2DArray.ArraySize = TextureDesc.ArraySize;
				DSVDesc.Texture2DArray.MipSlice = 0;
			}
		}
		else
		{
			if (bIsMultisampled)
			{
				DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
				// Nothing to set
			}
			else
			{
				DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
				DSVDesc.Texture2D.MipSlice = 0;
			}
		}

		for (uint32 AccessType = 0; AccessType < FExclusiveDepthStencil::MaxIndex; ++AccessType)
		{
			// Create a read-only access views for the texture.
			// Read-only DSVs are not supported in Feature Level 10 so 
			// a dummy DSV is created in order reduce logic complexity at a higher-level.
			DSVDesc.Flags = (AccessType & FExclusiveDepthStencil::DepthRead_StencilWrite) ? D3D11_DSV_READ_ONLY_DEPTH : 0;
			if (UE::DXGIUtilities::HasStencilBits(DSVDesc.Format))
			{
				DSVDesc.Flags |= (AccessType & FExclusiveDepthStencil::DepthWrite_StencilRead) ? D3D11_DSV_READ_ONLY_STENCIL : 0;
			}
			VERIFYD3D11RESULT_EX(Direct3DDevice->CreateDepthStencilView(TextureResource,&DSVDesc,DepthStencilViews[AccessType].GetInitReference()), Direct3DDevice);
		}
	}

	// Create a shader resource view for the texture.
	if (bCreateShaderResource)
	{
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
			FMemory::Memzero(SRVDesc);

			SRVDesc.Format = PlatformShaderResourceFormat;

			if (bCubeTexture && bTextureArray)
			{
				SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
				SRVDesc.TextureCubeArray.MostDetailedMip = 0;
				SRVDesc.TextureCubeArray.MipLevels = TextureDesc.MipLevels;
				SRVDesc.TextureCubeArray.First2DArrayFace = 0;
				SRVDesc.TextureCubeArray.NumCubes = TextureDesc.ArraySize / 6;
			}
			else if (bCubeTexture)
			{
				SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
				SRVDesc.TextureCube.MostDetailedMip = 0;
				SRVDesc.TextureCube.MipLevels = TextureDesc.MipLevels;
			}
			else if (bTextureArray)
			{
				if (bIsMultisampled)
				{
					SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
					SRVDesc.Texture2DMSArray.FirstArraySlice = 0;
					SRVDesc.Texture2DMSArray.ArraySize = TextureDesc.ArraySize;
				}
				else
				{
					SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
					SRVDesc.Texture2DArray.MostDetailedMip = 0;
					SRVDesc.Texture2DArray.MipLevels = TextureDesc.MipLevels;
					SRVDesc.Texture2DArray.FirstArraySlice = 0;
					SRVDesc.Texture2DArray.ArraySize = TextureDesc.ArraySize;
				}
			}
			else 
			{
				if (bIsMultisampled)
				{
					SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
					// Nothing to set
				}
				else
				{
					SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
					SRVDesc.Texture2D.MostDetailedMip = 0;
					SRVDesc.Texture2D.MipLevels = TextureDesc.MipLevels;
				}
			}
			VERIFYD3D11RESULT_EX(Direct3DDevice->CreateShaderResourceView(TextureResource,&SRVDesc,ShaderResourceView.GetInitReference()), Direct3DDevice);
		}

		check(IsValidRef(ShaderResourceView));
	}

	const ETextureDimension Dimension = bTextureArray
		? (bCubeTexture ? ETextureDimension::TextureCubeArray : ETextureDimension::Texture2DArray)
		: (bCubeTexture ? ETextureDimension::TextureCube      : ETextureDimension::Texture2D     );

	const FRHITextureCreateDesc RHITextureDesc =
		FRHITextureCreateDesc::Create(TEXT("FD3D11DynamicRHI::CreateTextureFromResource"), Dimension)
		.SetExtent(TextureDesc.Width, TextureDesc.Height)
		.SetFormat((EPixelFormat)Format)
		.SetClearValue(ClearValueBinding)
		.SetArraySize(TextureDesc.ArraySize)
		.SetFlags(TexCreateFlags)
		.SetNumMips(TextureDesc.MipLevels)
		.SetNumSamples(TextureDesc.SampleDesc.Count)
		.DetermineInititialState();

	FD3D11Texture* Texture2D = new FD3D11Texture(
		RHITextureDesc,
		TextureResource,
		ShaderResourceView,
		TextureDesc.ArraySize,
		bCreatedRTVPerSlice,
		RenderTargetViews,
		DepthStencilViews
	);

	return Texture2D;
}

FTexture2DRHIRef FD3D11DynamicRHI::RHICreateTexture2DFromResource(EPixelFormat Format, ETextureCreateFlags TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D11Texture2D* TextureResource)
{
	return CreateTextureFromResource(false, false, Format, TexCreateFlags, ClearValueBinding, TextureResource);
}

FTexture2DArrayRHIRef FD3D11DynamicRHI::RHICreateTexture2DArrayFromResource(EPixelFormat Format, ETextureCreateFlags TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D11Texture2D* TextureResource)
{
	return CreateTextureFromResource(true, false, Format, TexCreateFlags, ClearValueBinding, TextureResource);
}

FTextureCubeRHIRef FD3D11DynamicRHI::RHICreateTextureCubeFromResource(EPixelFormat Format, ETextureCreateFlags TexCreateFlags, const FClearValueBinding& ClearValueBinding, ID3D11Texture2D* TextureResource)
{
	return CreateTextureFromResource(false, true, Format, TexCreateFlags, ClearValueBinding, TextureResource);
}

FD3D11Texture::FD3D11Texture(
	  const FRHITextureCreateDesc& InDesc
	, ID3D11Resource* InResource
	, ID3D11ShaderResourceView* InShaderResourceView
	, int32 InRTVArraySize
	, bool bInCreatedRTVsPerSlice
	, TConstArrayView<TRefCountPtr<ID3D11RenderTargetView>> InRenderTargetViews
	, TConstArrayView<TRefCountPtr<ID3D11DepthStencilView>> InDepthStencilViews
	)
	: FRHITexture         (InDesc)
	, Resource            (InResource)
	, ShaderResourceView  (InShaderResourceView)
	, RenderTargetViews   (InRenderTargetViews)
	, RTVArraySize        (InRTVArraySize)
	, bCreatedRTVsPerSlice(bInCreatedRTVsPerSlice)
	, bAlias              (false)
{
	// Set the DSVs for all the access type combinations
	if (InDepthStencilViews.Num())
	{
		check(InDepthStencilViews.Num() == FExclusiveDepthStencil::MaxIndex);
		for (uint32 Index = 0; Index < FExclusiveDepthStencil::MaxIndex; Index++)
		{
			DepthStencilViews[Index] = InDepthStencilViews[Index];
		}
	}

	UpdateD3D11TextureStats(*this, true);
}

FD3D11Texture::FD3D11Texture(FD3D11Texture const& Other, const FString& Name, EAliasResourceParam)
	: FRHITexture(FRHITextureCreateDesc(Other.GetDesc(), ERHIAccess::SRVMask, *Name))
	, bAlias(true)
{
	AliasResource(Other);
}

FD3D11Texture::~FD3D11Texture()
{
	if (!bAlias)
	{
		UpdateD3D11TextureStats(*this, false);
	}
}

void FD3D11Texture::AliasResource(FD3D11Texture const& Other)
{
	check(bAlias);
	IHVResourceHandle    = Other.IHVResourceHandle;
	Resource             = Other.Resource;
	ShaderResourceView   = Other.ShaderResourceView;
	RenderTargetViews    = Other.RenderTargetViews;
	bCreatedRTVsPerSlice = Other.bCreatedRTVsPerSlice;
	RTVArraySize         = Other.RTVArraySize;

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(DepthStencilViews); ++Index)
	{
		DepthStencilViews[Index] = Other.DepthStencilViews[Index];
	}
}

void FD3D11DynamicRHI::RHIAliasTextureResources(FTextureRHIRef& DstTextureRHI, FTextureRHIRef& SrcTextureRHI)
{
	FD3D11Texture* DstTexture = ResourceCast(DstTextureRHI);
	FD3D11Texture* SrcTexture = ResourceCast(SrcTextureRHI);
	check(DstTexture && SrcTexture);

	DstTexture->AliasResource(*SrcTexture);
}

FTextureRHIRef FD3D11DynamicRHI::RHICreateAliasedTexture(FTextureRHIRef& SrcTextureRHI)
{
	FD3D11Texture* SrcTexture = ResourceCast(SrcTextureRHI);
	check(SrcTexture);
	const FString Name = SrcTextureRHI->GetName().ToString() + TEXT("Alias");

	return new FD3D11Texture(*SrcTexture, *Name, FD3D11Texture::CreateAlias);
}

void FD3D11DynamicRHI::RHICopyTexture(FRHITexture* SourceTextureRHI, FRHITexture* DestTextureRHI, const FRHICopyTextureInfo& CopyInfo)
{
	FRHICommandList_RecursiveHazardous RHICmdList(this);	

	FD3D11Texture* SourceTexture = ResourceCast(SourceTextureRHI);
	FD3D11Texture* DestTexture = ResourceCast(DestTextureRHI);

	check(SourceTexture && DestTexture);

	GPUProfilingData.RegisterGPUWork();

	const FRHITextureDesc& SourceDesc = SourceTextureRHI->GetDesc();
	const FRHITextureDesc& DestDesc = DestTextureRHI->GetDesc();

	const uint16 SourceArraySize = SourceDesc.ArraySize * (SourceDesc.IsTextureCube() ? 6 : 1);
	const uint16 DestArraySize   = DestDesc.ArraySize   * (DestDesc.IsTextureCube()   ? 6 : 1);

	const bool bAllPixels =
		SourceDesc.GetSize() == DestDesc.GetSize() && (CopyInfo.Size == FIntVector::ZeroValue || CopyInfo.Size == SourceDesc.GetSize());

	const bool bAllSubresources =
		SourceDesc.NumMips == DestDesc.NumMips && SourceDesc.NumMips == CopyInfo.NumMips &&
		SourceArraySize == DestArraySize && SourceArraySize == CopyInfo.NumSlices;

	if (!bAllPixels || !bAllSubresources)
	{
		const FPixelFormatInfo& PixelFormatInfo = GPixelFormats[SourceTextureRHI->GetFormat()];

		const FIntVector SourceSize = SourceDesc.GetSize();
		const FIntVector CopySize = CopyInfo.Size == FIntVector::ZeroValue ? SourceSize >> CopyInfo.SourceMipIndex : CopyInfo.Size;

		for (uint32 SliceIndex = 0; SliceIndex < CopyInfo.NumSlices; ++SliceIndex)
		{
			uint32 SourceSliceIndex = CopyInfo.SourceSliceIndex + SliceIndex;
			uint32 DestSliceIndex   = CopyInfo.DestSliceIndex   + SliceIndex;

			for (uint32 MipIndex = 0; MipIndex < CopyInfo.NumMips; ++MipIndex)
			{
				uint32 SourceMipIndex = CopyInfo.SourceMipIndex + MipIndex;
				uint32 DestMipIndex   = CopyInfo.DestMipIndex   + MipIndex;

				const uint32 SourceSubresource = D3D11CalcSubresource(SourceMipIndex, SourceSliceIndex, SourceTextureRHI->GetNumMips());
				const uint32 DestSubresource = D3D11CalcSubresource(DestMipIndex, DestSliceIndex, DestTextureRHI->GetNumMips());

				D3D11_BOX SrcBox;
				SrcBox.left   = CopyInfo.SourcePosition.X >> MipIndex;
				SrcBox.top    = CopyInfo.SourcePosition.Y >> MipIndex;
				SrcBox.front  = CopyInfo.SourcePosition.Z >> MipIndex;
				SrcBox.right  = AlignArbitrary<uint32>(FMath::Max<uint32>((CopyInfo.SourcePosition.X + CopySize.X) >> MipIndex, 1), PixelFormatInfo.BlockSizeX);
				SrcBox.bottom = AlignArbitrary<uint32>(FMath::Max<uint32>((CopyInfo.SourcePosition.Y + CopySize.Y) >> MipIndex, 1), PixelFormatInfo.BlockSizeY);
				SrcBox.back   = AlignArbitrary<uint32>(FMath::Max<uint32>((CopyInfo.SourcePosition.Z + CopySize.Z) >> MipIndex, 1), PixelFormatInfo.BlockSizeZ);

				const uint32 DestX = CopyInfo.DestPosition.X >> MipIndex;
				const uint32 DestY = CopyInfo.DestPosition.Y >> MipIndex;
				const uint32 DestZ = CopyInfo.DestPosition.Z >> MipIndex;

				Direct3DDeviceIMContext->CopySubresourceRegion(DestTexture->GetResource(), DestSubresource, DestX, DestY, DestZ, SourceTexture->GetResource(), SourceSubresource, &SrcBox);
			}
		}
	}
	else
	{
		// Make sure the params are all by default when using this case
		ensure(CopyInfo.SourceSliceIndex == 0 && CopyInfo.DestSliceIndex == 0 && CopyInfo.SourcePosition == FIntVector::ZeroValue && CopyInfo.DestPosition == FIntVector::ZeroValue && CopyInfo.SourceMipIndex == 0 && CopyInfo.DestMipIndex == 0);
		Direct3DDeviceIMContext->CopyResource(DestTexture->GetResource(), SourceTexture->GetResource());
	}
}

void FD3D11DynamicRHI::RHICopyBufferRegion(FRHIBuffer* DstBuffer, uint64 DstOffset, FRHIBuffer* SrcBuffer, uint64 SrcOffset, uint64 NumBytes)
{
	if (!DstBuffer || !SrcBuffer || DstBuffer == SrcBuffer || !NumBytes)
	{
		return;
	}

	FD3D11Buffer* DstBufferD3D11 = ResourceCast(DstBuffer);
	FD3D11Buffer* SrcBufferD3D11 = ResourceCast(SrcBuffer);

	check(DstBufferD3D11 && SrcBufferD3D11);
	check(DstOffset + NumBytes <= DstBuffer->GetSize() && SrcOffset + NumBytes <= SrcBuffer->GetSize());

	GPUProfilingData.RegisterGPUWork();

	D3D11_BOX SrcBox;
	SrcBox.left = SrcOffset;
	SrcBox.right = SrcOffset + NumBytes;
	SrcBox.top = 0;
	SrcBox.bottom = 1;
	SrcBox.front = 0;
	SrcBox.back = 1;

	ID3D11Resource* DstResource = DstBufferD3D11->Resource.GetReference();
	ID3D11Resource* SrcResource = SrcBufferD3D11->Resource.GetReference();
	Direct3DDeviceIMContext->CopySubresourceRegion(DstResource, 0, DstOffset, 0, 0, SrcResource, 0, &SrcBox);
}
