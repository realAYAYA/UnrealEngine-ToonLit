// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualTextureSpace.h"
#include "VirtualTexturePhysicalSpace.h"
#include "VirtualTextureSystem.h"
#include "SpriteIndexBuffer.h"
#include "PostProcess/SceneFilterRendering.h"
#include "RenderTargetPool.h"
#include "VisualizeTexture.h"
#include "CommonRenderResources.h"
#include "GlobalShader.h"
#include "GlobalRenderResources.h"
#include "PipelineStateCache.h"
#include "RHIStaticStates.h"
#include "HAL/IConsoleManager.h"
#include "SceneUtils.h"
#include "RenderGraph.h"

#include "VT/AllocatedVirtualTexture.h"

DEFINE_LOG_CATEGORY_STATIC(LogVirtualTextureSpace, Log, All);

static TAutoConsoleVariable<int32> CVarVTRefreshEntirePageTable(
	TEXT("r.VT.RefreshEntirePageTable"),
	0,
	TEXT("Refreshes the entire page table texture every frame"),
	ECVF_RenderThreadSafe
	);

static TAutoConsoleVariable<int32> CVarVTMaskedPageTableUpdates(
	TEXT("r.VT.MaskedPageTableUpdates"),
	1,
	TEXT("Masks the page table update quads to reduce pixel fill costs"),
	ECVF_RenderThreadSafe
	);

static EPixelFormat GetFormatForNumLayers(uint32 NumLayers, EVTPageTableFormat Format)
{
	const bool bUse16Bits = (Format == EVTPageTableFormat::UInt16);
	switch (NumLayers)
	{
	case 1u: return bUse16Bits ? PF_R16_UINT : PF_R32_UINT;
	case 2u: return bUse16Bits ? PF_R16G16_UINT : PF_R32G32_UINT;
	case 3u:
	case 4u: return bUse16Bits ? PF_R16G16B16A16_UINT : PF_R32G32B32A32_UINT;
	default: checkNoEntry(); return PF_Unknown;
	}
}

FVirtualTextureSpace::FVirtualTextureSpace(FVirtualTextureSystem* InSystem, uint8 InID, const FVTSpaceDescription& InDesc, uint32 InSizeNeeded)
	: Description(InDesc)
	, Allocator(InDesc.Dimensions)
	, NumRefs(0u)
	, ID(InID)
	, CachedPageTableWidth(0u)
	, CachedPageTableHeight(0u)
	, CachedNumPageTableLevels(0u)
	, bNeedToAllocatePageTable(true)
	, bForceEntireUpdate(false)
{
	// Initialize page map with large enough capacity to handle largest possible physical texture
	const uint32 PhysicalTileSize = InDesc.TileSize + InDesc.TileBorderSize * 2u;
	const uint32 MaxSizeInTiles = GetMax2DTextureDimension() / PhysicalTileSize;
	const uint32 MaxNumTiles = MaxSizeInTiles * MaxSizeInTiles;
	for (uint32 LayerIndex = 0u; LayerIndex < InDesc.NumPageTableLayers; ++LayerIndex)
	{
		PhysicalPageMap[LayerIndex].Initialize(MaxNumTiles, LayerIndex, InDesc.Dimensions);
	}

	uint32 NumLayersToAllocate = InDesc.NumPageTableLayers;
	uint32 PageTableIndex = 0u;
	FMemory::Memzero(TexturePixelFormat);
	while (NumLayersToAllocate > 0u)
	{
		const uint32 NumLayersForTexture = FMath::Min(NumLayersToAllocate, LayersPerPageTableTexture);
		const EPixelFormat PixelFormat = GetFormatForNumLayers(NumLayersForTexture, InDesc.PageTableFormat);
		TexturePixelFormat[PageTableIndex] = PixelFormat;
		NumLayersToAllocate -= NumLayersForTexture;
		++PageTableIndex;
	}

	Allocator.Initialize(Description.MaxSpaceSize);

	bNeedToAllocatePageTableIndirection = InDesc.IndirectionTextureSize > 0;
}

FVirtualTextureSpace::~FVirtualTextureSpace()
{
}

uint32 FVirtualTextureSpace::AllocateVirtualTexture(FAllocatedVirtualTexture* VirtualTexture)
{
	const uint32 vAddress = Allocator.Alloc(VirtualTexture);
	
	// After allocation, check if we need to reallocate the page table texture.
	const FUintPoint RequiredPageTableSize = GetRequiredPageTableAllocationSize(); 
	if (RequiredPageTableSize.X > CachedPageTableWidth || RequiredPageTableSize.Y > CachedPageTableHeight)
	{
		bNeedToAllocatePageTable = true;
	}

	return vAddress;
}

void FVirtualTextureSpace::FreeVirtualTexture(FAllocatedVirtualTexture* VirtualTexture)
{
	Allocator.Free(VirtualTexture);
}

void FVirtualTextureSpace::InitRHI(FRHICommandListBase& RHICmdList)
{
	for (uint32 TextureIndex = 0u; TextureIndex < GetNumPageTableTextures(); ++TextureIndex)
	{
		FTextureEntry& TextureEntry = PageTable[TextureIndex];
		TextureEntry.TextureReferenceRHI = RHICmdList.CreateTextureReference();
	}
	PageTableIndirection.TextureReferenceRHI = RHICmdList.CreateTextureReference();
	RHICmdList.UpdateTextureReference(PageTableIndirection.TextureReferenceRHI, GBlackUintTexture->TextureRHI);
}

void FVirtualTextureSpace::ReleaseRHI()
{
	for (uint32 i = 0u; i < TextureCapacity; ++i)
	{
		FTextureEntry& TextureEntry = PageTable[i];
		TextureEntry.TextureReferenceRHI.SafeRelease();
		GRenderTargetPool.FreeUnusedResource(TextureEntry.RenderTarget);
	}

	PageTableIndirection.TextureReferenceRHI.SafeRelease();
	GRenderTargetPool.FreeUnusedResource(PageTableIndirection.RenderTarget);

	UpdateBuffer.SafeRelease();
	UpdateBufferSRV.SafeRelease();
}

FUintPoint FVirtualTextureSpace::GetRequiredPageTableAllocationSize() const
{
	// Private spaces should allocate the full page table texture up front.
	const uint32 Width = Description.bPrivateSpace ? Description.MaxSpaceSize : Allocator.GetAllocatedWidth();
	const uint32 Height = Description.bPrivateSpace ? Description.MaxSpaceSize : Allocator.GetAllocatedHeight();
	// We align on some minimum size. Maybe minimum, and align sizes should be different? But OK for now.
	const uint32 WidthAligned = Align(Width, VIRTUALTEXTURE_MIN_PAGETABLE_SIZE);
	const uint32 HeightAligned = Align(Height, VIRTUALTEXTURE_MIN_PAGETABLE_SIZE);
	return FUintPoint(WidthAligned, HeightAligned);
}

uint32 FVirtualTextureSpace::GetSizeInBytes() const
{
	const FUintPoint RequiredPageTableSize = GetRequiredPageTableAllocationSize();
	const uint32 NumPageTableLevels = FMath::FloorLog2(FMath::Max(RequiredPageTableSize.X, RequiredPageTableSize.Y)) + 1u;

	uint32 TotalSize = 0u;
	for (uint32 TextureIndex = 0u; TextureIndex < GetNumPageTableTextures(); ++TextureIndex)
	{
		const SIZE_T TextureSize = CalcTextureSize(RequiredPageTableSize.X, RequiredPageTableSize.Y, TexturePixelFormat[TextureIndex], NumPageTableLevels);
		TotalSize += TextureSize;
	}
	
	TotalSize += CalculateImageBytes(Description.IndirectionTextureSize, Description.IndirectionTextureSize, 0, PF_R32_UINT);

	return TotalSize;
}

void FVirtualTextureSpace::QueueUpdate(uint8 Layer, uint8 vLogSize, uint32 vAddress, uint8 vLevel, const FPhysicalTileLocation& pTileLocation)
{
	FPageTableUpdate Update;
	Update.vAddress = vAddress;
	Update.pTileLocation = pTileLocation;
	Update.vLevel = vLevel;
	Update.vLogSize = vLogSize;
	Update.Check( Description.Dimensions );
	PageTableUpdates[Layer].Add( Update );
}


TGlobalResource< FSpriteIndexBuffer<8> > GQuadIndexBuffer;

class FPageTableUpdateVS : public FGlobalShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FPageTableUpdateVS, NonVirtual);
protected:
	FPageTableUpdateVS() {}
	
public:
	FPageTableUpdateVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{
		PageTableSize.Bind( Initializer.ParameterMap, TEXT("PageTableSize") );
		FirstUpdate.Bind( Initializer.ParameterMap, TEXT("FirstUpdate") );
		NumUpdates.Bind( Initializer.ParameterMap, TEXT("NumUpdates") );
		UpdateBuffer.Bind( Initializer.ParameterMap, TEXT("UpdateBuffer") );
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}

	void SetParameters(FRHIBatchedShaderParameters& BatchedParameters, FIntPoint InPageTableSize, uint32 InFirstUpdate, uint32 InNumUpdates, FRHIShaderResourceView* InUpdateBuffer)
	{
		SetShaderValue(BatchedParameters, PageTableSize, InPageTableSize);
		SetShaderValue(BatchedParameters, FirstUpdate, InFirstUpdate);
		SetShaderValue(BatchedParameters, NumUpdates, InNumUpdates);
		SetSRVParameter(BatchedParameters, UpdateBuffer, InUpdateBuffer);
	}

	LAYOUT_FIELD(FShaderParameter, PageTableSize);
	LAYOUT_FIELD(FShaderParameter, FirstUpdate);
	LAYOUT_FIELD(FShaderParameter, NumUpdates);
	LAYOUT_FIELD(FShaderResourceParameter, UpdateBuffer);
};

class FPageTableUpdatePS : public FGlobalShader
{
	DECLARE_INLINE_TYPE_LAYOUT(FPageTableUpdatePS, NonVirtual);
protected:
	FPageTableUpdatePS() {}

public:
	FPageTableUpdatePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FGlobalShader(Initializer)
	{}
	
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return true;
	}
};

template<bool Use16Bits>
class TPageTableUpdateVS : public FPageTableUpdateVS
{
	DECLARE_SHADER_TYPE(TPageTableUpdateVS,Global);

	TPageTableUpdateVS() {}

public:
	TPageTableUpdateVS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FPageTableUpdateVS(Initializer)
	{}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
		OutEnvironment.SetDefine(TEXT("USE_16BIT"), Use16Bits);
	}
};

template<EPixelFormat TargetFormat>
class TPageTableUpdatePS : public FPageTableUpdatePS
{
	DECLARE_SHADER_TYPE(TPageTableUpdatePS, Global);

	TPageTableUpdatePS() {}

public:
	TPageTableUpdatePS(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FPageTableUpdatePS(Initializer)
	{}
	
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment( Parameters, OutEnvironment );
		OutEnvironment.SetRenderTargetOutputFormat(0u, TargetFormat);
	}
};

IMPLEMENT_SHADER_TYPE(template<>, TPageTableUpdateVS<false>, TEXT("/Engine/Private/PageTableUpdate.usf"), TEXT("PageTableUpdateVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<>, TPageTableUpdateVS<true>, TEXT("/Engine/Private/PageTableUpdate.usf"), TEXT("PageTableUpdateVS"), SF_Vertex);
IMPLEMENT_SHADER_TYPE(template<>, TPageTableUpdatePS<PF_R16_UINT>, TEXT("/Engine/Private/PageTableUpdate.usf"), TEXT("PageTableUpdatePS_1"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TPageTableUpdatePS<PF_R16G16_UINT>, TEXT("/Engine/Private/PageTableUpdate.usf"), TEXT("PageTableUpdatePS_2"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TPageTableUpdatePS<PF_R16G16B16A16_UINT>, TEXT("/Engine/Private/PageTableUpdate.usf"), TEXT("PageTableUpdatePS_4"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TPageTableUpdatePS<PF_R32_UINT>, TEXT("/Engine/Private/PageTableUpdate.usf"), TEXT("PageTableUpdatePS_1"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TPageTableUpdatePS<PF_R32G32_UINT>, TEXT("/Engine/Private/PageTableUpdate.usf"), TEXT("PageTableUpdatePS_2"), SF_Pixel);
IMPLEMENT_SHADER_TYPE(template<>, TPageTableUpdatePS<PF_R32G32B32A32_UINT>, TEXT("/Engine/Private/PageTableUpdate.usf"), TEXT("PageTableUpdatePS_4"), SF_Pixel);

void FVirtualTextureSpace::QueueUpdateEntirePageTable()
{
	bForceEntireUpdate = true;
}

void FVirtualTextureSpace::AllocateTextures(FRDGBuilder& GraphBuilder)
{
	if (bNeedToAllocatePageTable)
	{
		RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

		const TCHAR* TextureNames[] = { TEXT("VirtualPageTable_0"), TEXT("VirtualPageTable_1") };
		static_assert(UE_ARRAY_COUNT(TextureNames) == TextureCapacity, "");

		const FUintPoint RequiredPageTableSize = GetRequiredPageTableAllocationSize();
		CachedPageTableWidth = RequiredPageTableSize.X;
		CachedPageTableHeight = RequiredPageTableSize.Y;
		CachedNumPageTableLevels = FMath::FloorLog2(FMath::Max(CachedPageTableWidth, CachedPageTableHeight)) + 1u;

		for (uint32 TextureIndex = 0u; TextureIndex < GetNumPageTableTextures(); ++TextureIndex)
		{
			// Page Table
			FTextureEntry& TextureEntry = PageTable[TextureIndex];
			const FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				FIntPoint(CachedPageTableWidth, CachedPageTableHeight),
				TexturePixelFormat[TextureIndex],
				FClearValueBinding::None,
				TexCreate_RenderTargetable | TexCreate_ShaderResource,
				CachedNumPageTableLevels);

			FRDGTextureRef DstTexture = GraphBuilder.CreateTexture(Desc, TextureNames[TextureIndex]);

			if (TextureEntry.RenderTarget)
			{
				FRDGTextureRef SrcTexture = GraphBuilder.RegisterExternalTexture(TextureEntry.RenderTarget);
				const FRDGTextureDesc& SrcDesc = SrcTexture->Desc;

				// Copy previously allocated page table to new texture
				FRHICopyTextureInfo CopyInfo;
				CopyInfo.Size.X = FMath::Min(Desc.Extent.X, SrcDesc.Extent.X);
				CopyInfo.Size.Y = FMath::Min(Desc.Extent.Y, SrcDesc.Extent.Y);
				CopyInfo.Size.Z = 1;
				CopyInfo.NumMips = FMath::Min(Desc.NumMips, SrcDesc.NumMips);

				AddCopyTexturePass(GraphBuilder, SrcTexture, DstTexture, CopyInfo);
			}

			TextureEntry.RenderTarget = GraphBuilder.ConvertToExternalTexture(DstTexture);
			RHIUpdateTextureReference(TextureEntry.TextureReferenceRHI, TextureEntry.RenderTarget->GetRHI());
		}

		bNeedToAllocatePageTable = false;
	}

	if (bNeedToAllocatePageTableIndirection)
	{
		RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

		if (Description.IndirectionTextureSize > 0)
		{
			const FRDGTextureDesc Desc = FRDGTextureDesc::Create2D(
				FIntPoint(Description.IndirectionTextureSize, Description.IndirectionTextureSize),
				PF_R32_UINT,
				FClearValueBinding::None,
				TexCreate_UAV | TexCreate_ShaderResource);

			FRDGTextureRef PageTableIndirectionTexture = GraphBuilder.CreateTexture(Desc, TEXT("PageTableIndirection"));
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(PageTableIndirectionTexture), FUintVector4(ForceInitToZero));
			PageTableIndirection.RenderTarget = GraphBuilder.ConvertToExternalTexture(PageTableIndirectionTexture);
			RHIUpdateTextureReference(PageTableIndirection.TextureReferenceRHI, PageTableIndirection.RenderTarget->GetRHI());
		}

		bNeedToAllocatePageTableIndirection = false;
	}
}


void FVirtualTextureSpace::ApplyUpdates(FVirtualTextureSystem* System, FRDGBuilder& GraphBuilder)
{
	ON_SCOPE_EXIT
	{
		FinalizeTextures(GraphBuilder);
	};

	static TArray<FPageTableUpdate> ExpandedUpdates[VIRTUALTEXTURE_SPACE_MAXLAYERS][16];

	if (bNeedToAllocatePageTable)
	{
		// Defer updates until next frame if page table texture needs to be re-allocated
		// We can't update the page table texture at this point in frame, as RHIUpdateTextureReference can't be called during RHIBegin/EndScene
		// Note that the virtual texture system doesn't account for page table updates being deferred. So this can potentially lead to sampling invalid page table addresses.
		// This could cause a glitch if we sample a VT on the first frame it is allocated. That's usually not the case (we usually sample some time after loading).
		// But it can be the case for Adaptive Virtual Texture which does a lot of dynamic page table allocation during the texture life.
		// However Adaptive Virtual Texture is OK because it always sets bPrivateSpace which gives fixed allocation of the actual page table texture.
		return;
	}

	// Multi-GPU support
	RDG_GPU_MASK_SCOPE(GraphBuilder, FRHIGPUMask::All());

	for (uint32 LayerIndex = 0u; LayerIndex < Description.NumPageTableLayers; ++LayerIndex)
	{
		FTexturePageMap& PageMap = PhysicalPageMap[LayerIndex];
		if (bForceEntireUpdate || CVarVTRefreshEntirePageTable.GetValueOnRenderThread())
		{
			PageMap.RefreshEntirePageTable(System, ExpandedUpdates[LayerIndex]);
		}
		else
		{
			for (const FPageTableUpdate& Update : PageTableUpdates[LayerIndex])
			{
				if (CVarVTMaskedPageTableUpdates.GetValueOnRenderThread())
				{
					PageMap.ExpandPageTableUpdateMasked(System, Update, ExpandedUpdates[LayerIndex]);
				}
				else
				{
					PageMap.ExpandPageTableUpdatePainters(System, Update, ExpandedUpdates[LayerIndex]);
				}
			}
		}
		PageTableUpdates[LayerIndex].Reset();
	}
	bForceEntireUpdate = false;

	// TODO Expand 3D updates for slices of volume texture

	uint32 TotalNumUpdates = 0;
	for (uint32 LayerIndex = 0u; LayerIndex < Description.NumPageTableLayers; ++LayerIndex)
	{
		for (uint32 Mip = 0; Mip < CachedNumPageTableLevels; Mip++)
		{
			TotalNumUpdates += ExpandedUpdates[LayerIndex][Mip].Num();
		}
	}

	if (TotalNumUpdates == 0u)
	{
		return;
	}

	FRHICommandListBase& RHICmdList = GraphBuilder.RHICmdList;

	if (UpdateBuffer == nullptr || TotalNumUpdates * sizeof(FPageTableUpdate) > UpdateBuffer->GetSize())
	{
		// Resize Update Buffer
		const uint32 MaxUpdates = FMath::RoundUpToPowerOfTwo(TotalNumUpdates);
		uint32 NewBufferSize = MaxUpdates * sizeof(FPageTableUpdate);
		if (UpdateBuffer)
		{
			NewBufferSize = FMath::Max(NewBufferSize, UpdateBuffer->GetSize() * 2u);
		}

		FRHIResourceCreateInfo CreateInfo(TEXT("FVirtualTextureSpace_UpdateBuffer"));
		UpdateBuffer = RHICmdList.CreateVertexBuffer(NewBufferSize, BUF_ShaderResource | BUF_Volatile, CreateInfo);
		UpdateBufferSRV = RHICmdList.CreateShaderResourceView(UpdateBuffer, sizeof(FPageTableUpdate), PF_R16G16B16A16_UINT);
	}

	// This flushes the RHI thread!
	{
		uint8* Buffer = (uint8*)RHICmdList.LockBuffer(UpdateBuffer, 0, TotalNumUpdates * sizeof(FPageTableUpdate), RLM_WriteOnly);
		for (uint32 LayerIndex = 0u; LayerIndex < Description.NumPageTableLayers; ++LayerIndex)
		{
			for (uint32 Mip = 0; Mip < CachedNumPageTableLevels; Mip++)
			{
				const uint32 NumUpdates = ExpandedUpdates[LayerIndex][Mip].Num();
				if (NumUpdates)
				{
					size_t UploadSize = NumUpdates * sizeof(FPageTableUpdate);
					FMemory::Memcpy(Buffer, ExpandedUpdates[LayerIndex][Mip].GetData(), UploadSize);
					Buffer += UploadSize;
				}
			}
		}
		RHICmdList.UnlockBuffer(UpdateBuffer);
	}

	// Draw
	RDG_EVENT_SCOPE(GraphBuilder, "PageTableUpdate");

	auto ShaderMap = GetGlobalShaderMap(GetFeatureLevel());
	TShaderRef<FPageTableUpdateVS> VertexShader;
	if (Description.PageTableFormat == EVTPageTableFormat::UInt16)
	{
		VertexShader = ShaderMap->GetShader< TPageTableUpdateVS<true> >();
	}
	else
	{
		VertexShader = ShaderMap->GetShader< TPageTableUpdateVS<false> >();
	}
	check(VertexShader.IsValid());

	uint32 FirstUpdate = 0;
	for (uint32 LayerIndex = 0u; LayerIndex < Description.NumPageTableLayers; ++LayerIndex)
	{
		const uint32 TextureIndex = LayerIndex / LayersPerPageTableTexture;
		const uint32 LayerInTexture = LayerIndex % LayersPerPageTableTexture;

		FTextureEntry& PageTableEntry = PageTable[TextureIndex];
		FRDGTextureRef PageTableTexture = GraphBuilder.RegisterExternalTexture(PageTableEntry.RenderTarget);

		// Use color write mask to update the proper page table entry for this layer
		FRHIBlendState* BlendStateRHI = nullptr;
		switch (LayerInTexture)
		{
		case 0u: BlendStateRHI = TStaticBlendState<CW_RED>::GetRHI(); break;
		case 1u: BlendStateRHI = TStaticBlendState<CW_GREEN>::GetRHI(); break;
		case 2u: BlendStateRHI = TStaticBlendState<CW_BLUE>::GetRHI(); break;
		case 3u: BlendStateRHI = TStaticBlendState<CW_ALPHA>::GetRHI(); break;
		default: check(false); break;
		}

		TShaderRef<FPageTableUpdatePS> PixelShader;
		switch (TexturePixelFormat[TextureIndex])
		{
		case PF_R16_UINT: PixelShader = ShaderMap->GetShader< TPageTableUpdatePS<PF_R16_UINT> >(); break;
		case PF_R16G16_UINT: PixelShader = ShaderMap->GetShader< TPageTableUpdatePS<PF_R16G16_UINT> >(); break;
		case PF_R16G16B16A16_UINT: PixelShader = ShaderMap->GetShader< TPageTableUpdatePS<PF_R16G16B16A16_UINT> >(); break;
		case PF_R32_UINT: PixelShader = ShaderMap->GetShader< TPageTableUpdatePS<PF_R32_UINT> >(); break;
		case PF_R32G32_UINT: PixelShader = ShaderMap->GetShader< TPageTableUpdatePS<PF_R32G32_UINT> >(); break;
		case PF_R32G32B32A32_UINT: PixelShader = ShaderMap->GetShader< TPageTableUpdatePS<PF_R32G32B32A32_UINT> >(); break;
		default: checkNoEntry(); break;
		}
		check(PixelShader.IsValid());

		uint32 MipWidth = CachedPageTableWidth;
		uint32 MipHeight = CachedPageTableHeight;
		for (uint32 Mip = 0; Mip < CachedNumPageTableLevels; Mip++)
		{
			const uint32 NumUpdates = ExpandedUpdates[LayerIndex][Mip].Num();
			if (NumUpdates)
			{
				auto* PassParameters = GraphBuilder.AllocParameters<FRenderTargetParameters>();
				PassParameters->RenderTargets[0] = FRenderTargetBinding(PageTableTexture, ERenderTargetLoadAction::ELoad, Mip);

				GraphBuilder.AddPass(
					RDG_EVENT_NAME("PageTableUpdate (Mip: %d)", Mip),
					PassParameters,
					ERDGPassFlags::Raster,
					[this, VertexShader, PixelShader, BlendStateRHI, FirstUpdate, NumUpdates, MipWidth, MipHeight](FRHICommandList& RHICmdList)
				{
					RHICmdList.SetViewport(0, 0, 0.0f, MipWidth, MipHeight, 1.0f);

					FGraphicsPipelineStateInitializer GraphicsPSOInit;
					RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

					GraphicsPSOInit.BlendState = BlendStateRHI;
					GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
					GraphicsPSOInit.PrimitiveType = PT_TriangleList;

					GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
					GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
					GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

					SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

					SetShaderParametersLegacyVS(RHICmdList, VertexShader, FIntPoint(CachedPageTableWidth, CachedPageTableHeight), FirstUpdate, NumUpdates, UpdateBufferSRV);

					// needs to be the same on shader side (faster on NVIDIA and AMD)
					uint32 QuadsPerInstance = 8;

					RHICmdList.SetStreamSource(0, NULL, 0);
					RHICmdList.DrawIndexedPrimitive(GQuadIndexBuffer.IndexBufferRHI, 0, 0, 32, 0, 2 * QuadsPerInstance, FMath::DivideAndRoundUp(NumUpdates, QuadsPerInstance));
				});

				ExpandedUpdates[LayerIndex][Mip].Reset();
			}

			FirstUpdate += NumUpdates;
			MipWidth = FMath::Max(MipWidth / 2u, 1u);
			MipHeight = FMath::Max(MipHeight / 2u, 1u);
		}

		PageTableEntry.RenderTarget = GraphBuilder.ConvertToExternalTexture(PageTableTexture);
	}
}

void FVirtualTextureSpace::FinalizeTextures(FRDGBuilder& GraphBuilder)
{
	for (uint32 LayerIndex = 0u; LayerIndex < Description.NumPageTableLayers; ++LayerIndex)
	{
		const uint32 TextureIndex = LayerIndex / LayersPerPageTableTexture;
		FTextureEntry& PageTableEntry = PageTable[TextureIndex];
		if (PageTableEntry.RenderTarget)
		{
			// It's only necessary to enable external access mode on textures modified by RDG this frame.
			if (FRDGTexture* Texture = GraphBuilder.FindExternalTexture(PageTableEntry.RenderTarget))
			{
				GraphBuilder.UseExternalAccessMode(Texture, ERHIAccess::SRVMask);
			}
		}
	}
}

void FVirtualTextureSpace::DumpToConsole(bool verbose)
{
	UE_LOG(LogConsoleResponse, Display, TEXT("-= Space ID %i =-"), ID);
	Allocator.DumpToConsole(verbose);
}

#if WITH_EDITOR
void FVirtualTextureSpace::SaveAllocatorDebugImage() const
{
	const FString ImageName = FString::Printf(TEXT("Space%dAllocator.png"), ID);
	Allocator.SaveDebugImage(*ImageName);
}
#endif
