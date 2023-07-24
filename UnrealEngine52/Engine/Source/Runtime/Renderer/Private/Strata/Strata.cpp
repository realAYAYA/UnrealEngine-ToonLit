// Copyright Epic Games, Inc. All Rights Reserved.

#include "Strata.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "HAL/IConsoleManager.h"
#include "PixelShaderUtils.h"
#include "SceneView.h"
#include "ScenePrivate.h"
#include "SceneRendering.h"
#include "RendererInterface.h"
#include "UniformBuffer.h"
#include "PostProcess/SceneRenderTargets.h"
#include "SceneTextureParameters.h"
#include "ShaderCompiler.h"
#include "Lumen/Lumen.h"
#include "RendererUtils.h"
#include "EngineAnalytics.h"
#include "SystemTextures.h"
#include "DBufferTextures.h"

//UE_DISABLE_OPTIMIZATION

// The project setting for Strata
static TAutoConsoleVariable<int32> CVarUseCmaskClear(
	TEXT("r.Substrate.UseCmaskClear"),
	0,
	TEXT("TEST."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarSubstrateTileOverflow(
	TEXT("r.Substrate.TileOverflow"),
	1.f,
	TEXT("Scale the number of Substrate tile for overflowing tiles containing multi-BSDFs pixels. (0: 0%, 1: 100%. Default 1.0f)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSubstrateTileOverflowFromMaterial(
	TEXT("r.Substrate.TileOverflowFromMaterial"),
	1,
	TEXT("When enable, scale the number of Substrate tile for overflowing tiles containing multi-BSDFs pixels based on material data. Otherwise use global overflow scale defined by r.Substrate.TileOverflow."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSubstrateDebugPeelLayersAboveDepth(
	TEXT("r.Substrate.Debug.PeelLayersAboveDepth"),
	0,
	TEXT("Substrate debug control to progressively peel off materials layer by layer."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSubstrateDebugRoughnessTracking(
	TEXT("r.Substrate.Debug.RoughnessTracking"),
	1,
	TEXT("Substrate debug control to disable roughness tracking, e.g. top layer roughness affecting bottom layer roughness to simulate light scattering."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSubstrateAsyncClassification(
	TEXT("r.Substrate.AsyncClassification"),
	1,
	TEXT("Run Substrate material classification in async (with shadow)."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarSubstrateDBufferPassDedicatedTiles(
	TEXT("r.Substrate.DBufferPass.DedicatedTiles"),
	0,
	TEXT("Use dedicated tile for DBuffer application when DBuffer pass is enabled."),
	ECVF_RenderThreadSafe);

IMPLEMENT_GLOBAL_SHADER_PARAMETER_STRUCT(FStrataGlobalUniformParameters, "Strata");

void FStrataViewData::Reset()
{
	// Preserve old max BSDF count which is set prior to the reset operation
	const uint32 OldMaxBSDFCount = MaxBSDFCount;
	const uint32 OldMaxBytePerPixel = MaxBytePerPixel;
	*this = FStrataViewData();
	MaxBSDFCount = OldMaxBSDFCount;
	MaxBytePerPixel = OldMaxBytePerPixel;
	for (uint32 i = 0; i < EStrataTileType::ECount; ++i)
	{
		ClassificationTileListBuffer[i] = nullptr;
		ClassificationTileListBufferUAV[i] = nullptr;
		ClassificationTileListBufferSRV[i] = nullptr;
	}
}

const TCHAR* ToString(EStrataTileType Type)
{
	switch (Type)
	{
	case EStrataTileType::ESimple:							return TEXT("Simple");
	case EStrataTileType::ESingle:							return TEXT("Single");
	case EStrataTileType::EComplex:							return TEXT("Complex");
	case EStrataTileType::EOpaqueRoughRefraction:			return TEXT("Opaque/RoughRefraction");
	case EStrataTileType::EOpaqueRoughRefractionSSSWithout:	return TEXT("Opaque/RoughRefraction/SSSWithout");
	case EStrataTileType::EDecalSimple:						return TEXT("Decal/Simple");
	case EStrataTileType::EDecalSingle:						return TEXT("Decal/Single");
	case EStrataTileType::EDecalComplex:					return TEXT("Decal/Complex");

	}
	return TEXT("Unknown");
}


namespace Strata
{

enum EStrataTileSpace
{
	StrataTileSpace_Primary = 1u,
	StrataTileSpace_Overflow = 2u
};

bool DoesStrataTileOverflowUseMaterialData() 
{
	return CVarSubstrateTileOverflowFromMaterial.GetValueOnRenderThread() > 0;
}

float GetStrataTileOverflowRatio(const FViewInfo& View)
{
	if (DoesStrataTileOverflowUseMaterialData())
	{
		const uint32 MaxBDFCount = FMath::Max(View.StrataViewData.MaxBSDFCount,1u);
		return FMath::Clamp(MaxBDFCount-1, 0.f, 4.0f);
	}
	else
	{
		return FMath::Clamp(CVarSubstrateTileOverflow.GetValueOnRenderThread(), 0.f, 4.0f);
	}
}

static FIntPoint GetStrataTextureTileResolution(const FViewInfo& View, const FIntPoint& InResolution, uint32 InSpace)
{
	FIntPoint Out = InResolution;
	Out.X = FMath::DivideAndRoundUp(Out.X, STRATA_TILE_SIZE);
	Out.Y = 0;
	if (InSpace & EStrataTileSpace::StrataTileSpace_Primary)
	{
		Out.Y += FMath::DivideAndRoundUp(InResolution.Y, STRATA_TILE_SIZE);
	}
	if (InSpace & EStrataTileSpace::StrataTileSpace_Overflow)
	{
		const float OverflowRatio = GetStrataTileOverflowRatio(View);
		Out.Y += FMath::CeilToInt(FMath::DivideAndRoundUp(InResolution.Y, STRATA_TILE_SIZE) * OverflowRatio);
	}
	return Out;
}

FIntPoint GetStrataTextureResolution(const FViewInfo& View, const FIntPoint& InResolution)
{
	if (Strata::IsStrataEnabled())
	{
		return GetStrataTextureTileResolution(View, InResolution, EStrataTileSpace::StrataTileSpace_Primary | EStrataTileSpace::StrataTileSpace_Overflow) * STRATA_TILE_SIZE;
	}
	{
		return InResolution;
	}
}

static void BindStrataGlobalUniformParameters(FRDGBuilder& GraphBuilder, FStrataViewData* StrataViewData, FStrataGlobalUniformParameters& OutStrataUniformParameters);

bool SupportsCMask(const FStaticShaderPlatform InPlatform)
{
	return CVarUseCmaskClear.GetValueOnRenderThread() > 0 && FDataDrivenShaderPlatformInfo::GetSupportsRenderTargetWriteMask(InPlatform);
}

bool IsClassificationAsync()
{
	return CVarSubstrateAsyncClassification.GetValueOnRenderThread() > 0;
}

static EPixelFormat GetClassificationTileFormat(const FIntPoint& InResolution)
{
	// For platform which whose resolution is never above 1080p, use 8bit tile format for performance.
	const bool bRequest8bit = Is8bitTileCoordEnabled();
	if (bRequest8bit)
	{
		check(InResolution.X <= 2048 && InResolution.Y <= 2048);
	}
	return bRequest8bit ? PF_R16_UINT : PF_R32_UINT;
}

static void InitialiseStrataViewData(FRDGBuilder& GraphBuilder, FViewInfo& View, const FSceneTexturesConfig& SceneTexturesConfig, bool bNeedBSDFOffets, FStrataSceneData& SceneData)
{
	// Sanity check: the scene data should already exist 
	check(SceneData.MaterialTextureArray != nullptr);

	FStrataViewData& Out = View.StrataViewData;
	Out.Reset();
	Out.SceneData = &SceneData;

	const FIntPoint ViewResolution(View.ViewRect.Width(), View.ViewRect.Height());
	if (IsStrataEnabled())
	{
		const FIntPoint TileResolution(FMath::DivideAndRoundUp(ViewResolution.X, STRATA_TILE_SIZE), FMath::DivideAndRoundUp(ViewResolution.Y, STRATA_TILE_SIZE));

		const TCHAR* StrataTileListBufferNames[EStrataTileType::ECount] =
		{
			TEXT("Substrate.StrataTileListBuffer(Simple)"),
			TEXT("Substrate.StrataTileListBuffer(Single)"),
			TEXT("Substrate.StrataTileListBuffer(Complex)"),
			TEXT("Substrate.StrataTileListBuffer(OpaqueRoughRefraction)"),
			TEXT("Substrate.StrataTileListBuffer(OpaqueRoughRefractionSSSWithout)"),
			TEXT("Substrate.StrataTileListBuffer(DecalSimple)"),
			TEXT("Substrate.StrataTileListBuffer(DecalSingle)"),
			TEXT("Substrate.StrataTileListBuffer(DecalComplex)"),
		};

		// Tile classification buffers
		{
			// Indirect draw
			Out.ClassificationTileDrawIndirectBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDrawIndirectParameters>(EStrataTileType::ECount), TEXT("Substrate.StrataTileDrawIndirectBuffer"));
			Out.ClassificationTileDrawIndirectBufferUAV = GraphBuilder.CreateUAV(Out.ClassificationTileDrawIndirectBuffer, PF_R32_UINT);
			AddClearUAVPass(GraphBuilder, Out.ClassificationTileDrawIndirectBufferUAV, 0);

			// Indirect dispatch
			Out.ClassificationTileDispatchIndirectBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(EStrataTileType::ECount), TEXT("Substrate.StrataTileDispatchIndirectBuffer"));
			Out.ClassificationTileDispatchIndirectBufferUAV = GraphBuilder.CreateUAV(Out.ClassificationTileDispatchIndirectBuffer, PF_R32_UINT);
			AddClearUAVPass(GraphBuilder, Out.ClassificationTileDispatchIndirectBufferUAV, 0);

			// Separated subsurface & rough refraction textures (tile data)
			const uint32 RoughTileCount = IsOpaqueRoughRefractionEnabled() ? TileResolution.X * TileResolution.Y : 4;
			const uint32 DecalTileCount = IsDBufferPassEnabled(View.GetShaderPlatform()) ? TileResolution.X * TileResolution.Y : 4;
			const uint32 RegularTileCount = TileResolution.X * TileResolution.Y;

			const EPixelFormat ClassificationTileFormat = GetClassificationTileFormat(ViewResolution);
			for (uint32 i = 0; i < EStrataTileType::ECount; ++i)
			{
				const EStrataTileType TileType = EStrataTileType(i);
				const uint32 FormatBytes = ClassificationTileFormat == PF_R16_UINT ? sizeof(uint16) : sizeof(uint32);

				uint32 TileCount = 0;
				switch (TileType)
				{
				case EStrataTileType::ESimple:
				case EStrataTileType::ESingle:
				case EStrataTileType::EComplex:
					TileCount = RegularTileCount; break;
				case EStrataTileType::EOpaqueRoughRefraction:
				case EStrataTileType::EOpaqueRoughRefractionSSSWithout:
					TileCount = RoughTileCount; break;
				case EStrataTileType::EDecalSimple:
				case EStrataTileType::EDecalSingle:
				case EStrataTileType::EDecalComplex:
					TileCount = DecalTileCount; break;
				}
				check(TileCount > 0);

				Out.ClassificationTileListBuffer[i] = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(FormatBytes, TileCount), StrataTileListBufferNames[i]);
				Out.ClassificationTileListBufferSRV[i] = GraphBuilder.CreateSRV(Out.ClassificationTileListBuffer[i], ClassificationTileFormat);
				Out.ClassificationTileListBufferUAV[i] = GraphBuilder.CreateUAV(Out.ClassificationTileListBuffer[i], ClassificationTileFormat);
			}
		}

		// BSDF tiles
		if (bNeedBSDFOffets)
		{			
			const FIntPoint BufferSize = SceneTexturesConfig.Extent;
			const FIntPoint BufferSize_Extended = GetStrataTextureTileResolution(View, SceneTexturesConfig.Extent, EStrataTileSpace::StrataTileSpace_Primary | EStrataTileSpace::StrataTileSpace_Overflow);

			const FIntPoint BaseOverflowTileOffset = FIntPoint(0, FMath::DivideAndRoundUp(BufferSize.Y, STRATA_TILE_SIZE));

			Out.TileCount	= GetStrataTextureTileResolution(View, ViewResolution, EStrataTileSpace::StrataTileSpace_Primary);
			Out.TileOffset  = FIntPoint(FMath::DivideAndRoundUp(View.ViewRect.Min.X, STRATA_TILE_SIZE), FMath::DivideAndRoundUp(View.ViewRect.Min.Y, STRATA_TILE_SIZE));

			Out.OverflowTileCount = GetStrataTextureTileResolution(View, ViewResolution, EStrataTileSpace::StrataTileSpace_Overflow);
			Out.OverflowTileOffset = GetStrataTextureTileResolution(View, Out.TileOffset * STRATA_TILE_SIZE, EStrataTileSpace::StrataTileSpace_Overflow) + BaseOverflowTileOffset;

			Out.BSDFTileTexture = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(BufferSize_Extended, PF_R32_UINT, FClearValueBinding::None, TexCreate_UAV | TexCreate_ShaderResource), TEXT("Substrate.BSDFTiles"));
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Out.BSDFTileTexture), 0u);

			Out.BSDFTilePerThreadDispatchIndirectBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Substrate.StrataBSDFTilePerThreadDispatchIndirectBuffer"));
			Out.BSDFTileDispatchIndirectBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Substrate.StrataBSDFTileDispatchIndirectBuffer"));
			Out.BSDFTileCountBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(4, 1), TEXT("Substrate.BSDFTileCount"));
		}
		else
		{
			Out.TileCount = GetStrataTextureTileResolution(View, ViewResolution, EStrataTileSpace::StrataTileSpace_Primary);
			Out.TileOffset = FIntPoint(0,0);
			Out.OverflowTileCount = FIntPoint(0, 0);
			Out.OverflowTileOffset = FIntPoint(0, 0);
			Out.BSDFTileTexture = nullptr;
			Out.BSDFTilePerThreadDispatchIndirectBuffer = nullptr;
			Out.BSDFTileDispatchIndirectBuffer = nullptr;
			Out.BSDFTileCountBuffer = nullptr;
		}
	}

	// Create the readable uniform buffers
	if (IsStrataEnabled())
	{
		FStrataGlobalUniformParameters* StrataUniformParameters = GraphBuilder.AllocParameters<FStrataGlobalUniformParameters>();
		BindStrataGlobalUniformParameters(GraphBuilder, &Out, *StrataUniformParameters);
		Out.StrataGlobalUniformParameters = GraphBuilder.CreateUniformBuffer(StrataUniformParameters);
	}
}

static bool NeedBSDFOffsets(const FScene* Scene, const FViewInfo& View)
{
	return  ShouldRenderLumenDiffuseGI(Scene, View) || ShouldRenderLumenReflections(View) || Strata::ShouldRenderStrataDebugPasses(View);
}

static void RecordStrataAnalytics(EShaderPlatform InPlatform)
{
	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> EventAttributes;
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("Enabled"), 1));
		EventAttributes.Add(FAnalyticsEventAttribute(TEXT("BytesPerPixel"), GetBytePerPixel(InPlatform)));

		FString OutStr(TEXT("Substrate.Usage.ProjectSettings"));
		FEngineAnalytics::GetProvider().RecordEvent(OutStr, EventAttributes);
	}
}

static EPixelFormat GetTopLayerTextureFormat(bool bUseDBufferPass)
{
	const bool bStrataHighQualityNormal = GetNormalQuality() > 0;

	// High quality normal is not supported on platforms that do not support R32G32 UAV load.
	// This is dues to the way Strata account for decals. See FStrataDBufferPassCS, updating TopLayerTexture this way.
	// If you encounter this check, you must disable high quality normal for Strata (material shaders must be recompiled to account for that).
	if (bUseDBufferPass)
	{
		check(!bStrataHighQualityNormal || (bStrataHighQualityNormal && UE::PixelFormat::HasCapabilities(PF_R32G32_UINT, EPixelFormatCapabilities::TypedUAVLoad)));
	}

	return bStrataHighQualityNormal ? PF_R32G32_UINT : PF_R32_UINT;
}

void InitialiseStrataFrameSceneData(FRDGBuilder& GraphBuilder, FSceneRenderer& SceneRenderer)
{
	FStrataSceneData& Out = SceneRenderer.Scene->StrataSceneData;
	Out = FStrataSceneData();

	auto UpdateMaterialBufferToTiledResolution = [](FIntPoint InBufferSizeXY, FIntPoint& OutMaterialBufferSizeXY)
	{
		// We need to allocate enough for the tiled memory addressing to always work
		OutMaterialBufferSizeXY.X = FMath::DivideAndRoundUp(InBufferSizeXY.X, STRATA_TILE_SIZE) * STRATA_TILE_SIZE;
		OutMaterialBufferSizeXY.Y = FMath::DivideAndRoundUp(InBufferSizeXY.Y, STRATA_TILE_SIZE) * STRATA_TILE_SIZE;
	};

	bool bNeedBSDFOffsets = false;
	bool bNeedUAV = false;
	uint32 MaxBytePerPixel = 0;
	bool bUseDBufferPass = false;
	for (const FViewInfo& View : SceneRenderer.Views)
	{
		bNeedBSDFOffsets = bNeedBSDFOffsets || NeedBSDFOffsets(SceneRenderer.Scene, View);
		bNeedUAV = bNeedUAV || IsDBufferPassEnabled(View.GetShaderPlatform());
		MaxBytePerPixel = FMath::Max(MaxBytePerPixel, View.StrataViewData.MaxBytePerPixel);
		bUseDBufferPass = bUseDBufferPass || IsDBufferPassEnabled(View.GetShaderPlatform());
	}
	MaxBytePerPixel = FMath::Clamp(MaxBytePerPixel, 4u * STRATA_BASE_PASS_MRT_OUTPUT_COUNT, GetBytePerPixel(SceneRenderer.ShaderPlatform));

	FIntPoint MaterialBufferSizeXY;
	UpdateMaterialBufferToTiledResolution(FIntPoint(1, 1), MaterialBufferSizeXY);
	if (IsStrataEnabled())
	{
		// Analytics for tracking Strata usage
		static bool bAnalyticsInitialized = false;
		if (!bAnalyticsInitialized)
		{
			RecordStrataAnalytics(SceneRenderer.ShaderPlatform);
			bAnalyticsInitialized = true;
		}

		FIntPoint SceneTextureExtent = SceneRenderer.GetActiveSceneTexturesConfig().Extent;
		
		// We need to allocate enough for the tiled memory addressing of material data to always work
		UpdateMaterialBufferToTiledResolution(SceneTextureExtent, MaterialBufferSizeXY);

		const uint32 RoundToValue = 4u;
		Out.MaxBytesPerPixel = FMath::DivideAndRoundUp(MaxBytePerPixel, RoundToValue) * RoundToValue;

		// Top layer texture
		{
			Out.TopLayerTexture = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(SceneTextureExtent, GetTopLayerTextureFormat(bUseDBufferPass), FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_FastVRAM | (bNeedUAV ? TexCreate_UAV : TexCreate_None)), TEXT("Substrate.TopLayerTexture"));
		}

		// Separated subsurface and rough refraction textures
		{
			const bool bIsStrataOpaqueMaterialRoughRefractionEnabled = IsOpaqueRoughRefractionEnabled();
			const FIntPoint OpaqueRoughRefractionSceneExtent		 = bIsStrataOpaqueMaterialRoughRefractionEnabled ? SceneTextureExtent : FIntPoint(4, 4);
			
			Out.OpaqueRoughRefractionTexture = GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(OpaqueRoughRefractionSceneExtent, PF_FloatR11G11B10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable), TEXT("Substrate.OpaqueRoughRefractionTexture"));
			Out.OpaqueRoughRefractionTextureUAV = GraphBuilder.CreateUAV(Out.OpaqueRoughRefractionTexture);
			
			Out.SeparatedSubSurfaceSceneColor			= GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(OpaqueRoughRefractionSceneExtent, PF_FloatR11G11B10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable), TEXT("Substrate.SeparatedSubSurfaceSceneColor"));
			Out.SeparatedOpaqueRoughRefractionSceneColor= GraphBuilder.CreateTexture(
				FRDGTextureDesc::Create2D(OpaqueRoughRefractionSceneExtent, PF_FloatR11G11B10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable), TEXT("Substrate.SeparatedOpaqueRoughRefractionSceneColor"));

			if (bIsStrataOpaqueMaterialRoughRefractionEnabled)
			{
				// Fast clears
				AddClearRenderTargetPass(GraphBuilder, Out.OpaqueRoughRefractionTexture, Out.OpaqueRoughRefractionTexture->Desc.ClearValue.GetClearColor());
				AddClearRenderTargetPass(GraphBuilder, Out.SeparatedSubSurfaceSceneColor, Out.SeparatedSubSurfaceSceneColor->Desc.ClearValue.GetClearColor());
				AddClearRenderTargetPass(GraphBuilder, Out.SeparatedOpaqueRoughRefractionSceneColor, Out.SeparatedOpaqueRoughRefractionSceneColor->Desc.ClearValue.GetClearColor());
			}
		}

		// BSDF offsets
		if (bNeedBSDFOffsets)
		{
			Out.BSDFOffsetTexture = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(SceneTextureExtent, PF_R32_UINT, FClearValueBinding::None, TexCreate_UAV | TexCreate_ShaderResource), TEXT("Substrate.BSDFOffsets"));
			AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Out.BSDFOffsetTexture), 0u);
		}
	}
	else
	{
		Out.MaxBytesPerPixel = 4u * STRATA_BASE_PASS_MRT_OUTPUT_COUNT;
	}

	// Create the material data container
	FIntPoint SceneTextureExtent = IsStrataEnabled() ? SceneRenderer.GetActiveSceneTexturesConfig().Extent : FIntPoint(2, 2);

	const uint32 SliceCountSSS = STRATA_SSS_DATA_UINT_COUNT;
	const uint32 SliceCountAdvDebug = IsAdvancedVisualizationEnabled() ? 1 : 0;
	const uint32 SliceCount = FMath::DivideAndRoundUp(Out.MaxBytesPerPixel, 4u) + SliceCountSSS + SliceCountAdvDebug;
	FRDGTextureDesc MaterialTextureDesc = FRDGTextureDesc::Create2DArray(SceneTextureExtent, PF_R32_UINT, FClearValueBinding::Transparent, TexCreate_TargetArraySlicesIndependently | TexCreate_DisableDCC | TexCreate_NoFastClear | TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV | TexCreate_FastVRAM, SliceCount, 1, 1);
	MaterialTextureDesc.FastVRAMPercentage = (1.0f / SliceCount) * 0xFF; // Only allocate the first slice into ESRAM

	Out.MaterialTextureArray = GraphBuilder.CreateTexture(MaterialTextureDesc, TEXT("Substrate.Material"));
	Out.MaterialTextureArraySRV = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::Create(Out.MaterialTextureArray));
	Out.MaterialTextureArrayUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Out.MaterialTextureArray, 0));

	// See AppendStrataMRTs
	check(STRATA_BASE_PASS_MRT_OUTPUT_COUNT <= (SliceCount - SliceCountSSS - SliceCountAdvDebug)); // We want enough slice for MRTs but also do not want the SSSData to be a MRT.
	Out.MaterialTextureArrayUAVWithoutRTs = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(Out.MaterialTextureArray, 0, PF_Unknown, STRATA_BASE_PASS_MRT_OUTPUT_COUNT, SliceCount - STRATA_BASE_PASS_MRT_OUTPUT_COUNT));

	// Rough diffuse model
	Out.bRoughDiffuse = IsRoughDiffuseEnabled();

	Out.PeelLayersAboveDepth = FMath::Max(CVarSubstrateDebugPeelLayersAboveDepth.GetValueOnRenderThread(), 0);
	Out.bRoughnessTracking = CVarSubstrateDebugRoughnessTracking.GetValueOnRenderThread() > 0 ? 1 : 0;

	// STRATA_TODO allocate a slice for StoringDebugStrata only if STRATA_ADVANCED_DEBUG_ENABLED is enabled 
	Out.SliceStoringDebugStrataTreeData				= SliceCount - SliceCountAdvDebug;										// When we read, there is no slices excluded
	Out.SliceStoringDebugStrataTreeDataWithoutMRT	= SliceCount - SliceCountAdvDebug - STRATA_BASE_PASS_MRT_OUTPUT_COUNT;	// The UAV skips the first slices set as render target

	Out.FirstSliceStoringStrataSSSData				= SliceCount - SliceCountSSS - SliceCountAdvDebug;										// When we read, there is no slices excluded
	Out.FirstSliceStoringStrataSSSDataWithoutMRT	= SliceCount - SliceCountSSS - SliceCountAdvDebug - STRATA_BASE_PASS_MRT_OUTPUT_COUNT;	// The UAV skips the first slices set as render target

	// Initialized view data
	for (int32 ViewIndex = 0; ViewIndex < SceneRenderer.Views.Num(); ViewIndex++)
	{
		Strata::InitialiseStrataViewData(GraphBuilder, SceneRenderer.Views[ViewIndex], SceneRenderer.GetActiveSceneTexturesConfig(), bNeedBSDFOffsets, Out);
	}

	if (IsStrataEnabled())
	{
		Out.StrataPublicGlobalUniformParameters = ::Strata::CreatePublicGlobalUniformBuffer(GraphBuilder, &Out);
	}
}

void BindStrataBasePassUniformParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, FStrataBasePassUniformParameters& OutStrataUniformParameters)
{
	const FStrataSceneData* StrataSceneData = View.StrataViewData.SceneData;
	if (IsStrataEnabled() && StrataSceneData)
	{
		OutStrataUniformParameters.bRoughDiffuse = StrataSceneData->bRoughDiffuse ? 1u : 0u;
		OutStrataUniformParameters.MaxBytesPerPixel = StrataSceneData->MaxBytesPerPixel;
		OutStrataUniformParameters.PeelLayersAboveDepth = StrataSceneData->PeelLayersAboveDepth;
		OutStrataUniformParameters.bRoughnessTracking = StrataSceneData->bRoughnessTracking ? 1u : 0u;
		OutStrataUniformParameters.SliceStoringDebugStrataTreeDataWithoutMRT = StrataSceneData->SliceStoringDebugStrataTreeDataWithoutMRT;
		OutStrataUniformParameters.FirstSliceStoringStrataSSSDataWithoutMRT = StrataSceneData->FirstSliceStoringStrataSSSDataWithoutMRT;
		OutStrataUniformParameters.MaterialTextureArrayUAVWithoutRTs = StrataSceneData->MaterialTextureArrayUAVWithoutRTs;
		OutStrataUniformParameters.OpaqueRoughRefractionTextureUAV = StrataSceneData->OpaqueRoughRefractionTextureUAV;
	}
	else
	{
		FRDGTextureRef DummyWritableRefracTexture = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(FIntPoint(1, 1), PF_R8, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV), TEXT("Substrate.DummyWritableTexture"));
		FRDGTextureUAVRef DummyWritableRefracTextureUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(DummyWritableRefracTexture));

		FRDGTextureRef DummyWritableTextureArray = GraphBuilder.CreateTexture(FRDGTextureDesc::Create2DArray(FIntPoint(1, 1), PF_R32_UINT, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV, 1), TEXT("Substrate.DummyWritableTexture"));
		FRDGTextureUAVRef DummyWritableTextureArrayUAV = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(DummyWritableTextureArray));

		const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
		OutStrataUniformParameters.bRoughDiffuse = 0u;
		OutStrataUniformParameters.MaxBytesPerPixel = 0;
		OutStrataUniformParameters.PeelLayersAboveDepth = 0;
		OutStrataUniformParameters.bRoughnessTracking = 0;
		OutStrataUniformParameters.SliceStoringDebugStrataTreeDataWithoutMRT = -1;
		OutStrataUniformParameters.FirstSliceStoringStrataSSSDataWithoutMRT = -1;
		OutStrataUniformParameters.MaterialTextureArrayUAVWithoutRTs = DummyWritableTextureArrayUAV;
		OutStrataUniformParameters.OpaqueRoughRefractionTextureUAV = DummyWritableRefracTextureUAV;
	}
}

static void BindStrataGlobalUniformParameters(FRDGBuilder& GraphBuilder, FStrataViewData* StrataViewData, FStrataGlobalUniformParameters& OutStrataUniformParameters)
{
	FStrataSceneData* StrataSceneData = StrataViewData->SceneData;
	if (IsStrataEnabled() && StrataSceneData)
	{
		OutStrataUniformParameters.bRoughDiffuse = StrataSceneData->bRoughDiffuse ? 1u : 0u;
		OutStrataUniformParameters.MaxBytesPerPixel = StrataSceneData->MaxBytesPerPixel;
		OutStrataUniformParameters.PeelLayersAboveDepth = StrataSceneData->PeelLayersAboveDepth;
		OutStrataUniformParameters.bRoughnessTracking = StrataSceneData->bRoughnessTracking ? 1u : 0u;
		OutStrataUniformParameters.SliceStoringDebugStrataTreeData = StrataSceneData->SliceStoringDebugStrataTreeData;
		OutStrataUniformParameters.FirstSliceStoringStrataSSSData = StrataSceneData->FirstSliceStoringStrataSSSData;
		OutStrataUniformParameters.TileSize = STRATA_TILE_SIZE;
		OutStrataUniformParameters.TileSizeLog2 = STRATA_TILE_SIZE_DIV_AS_SHIFT;
		OutStrataUniformParameters.TileCount = StrataViewData->TileCount;
		OutStrataUniformParameters.TileOffset = StrataViewData->TileOffset;
		OutStrataUniformParameters.OverflowTileCount = StrataViewData->OverflowTileCount;
		OutStrataUniformParameters.OverflowTileOffset = StrataViewData->OverflowTileOffset;
		OutStrataUniformParameters.MaterialTextureArray = StrataSceneData->MaterialTextureArray;
		OutStrataUniformParameters.TopLayerTexture = StrataSceneData->TopLayerTexture;
		OutStrataUniformParameters.OpaqueRoughRefractionTexture = StrataSceneData->OpaqueRoughRefractionTexture;
		OutStrataUniformParameters.BSDFTileTexture = StrataViewData->BSDFTileTexture;
		OutStrataUniformParameters.BSDFOffsetTexture = StrataSceneData->BSDFOffsetTexture;
		OutStrataUniformParameters.BSDFTileCountBuffer = StrataViewData->BSDFTileCountBuffer ? GraphBuilder.CreateSRV(StrataViewData->BSDFTileCountBuffer, PF_R32_UINT) : nullptr;

		if (OutStrataUniformParameters.BSDFOffsetTexture == nullptr)
		{
			const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
			FRDGBufferSRVRef DefaultBuffer = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultBuffer(GraphBuilder, 4, 0u), PF_R32_UINT);
			OutStrataUniformParameters.BSDFOffsetTexture = SystemTextures.Black;
			OutStrataUniformParameters.BSDFTileTexture = SystemTextures.Black;
			OutStrataUniformParameters.BSDFTileCountBuffer = DefaultBuffer;
		}
	}
	else
	{
		const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
		FRDGTextureRef DefaultTextureArray = GSystemTextures.GetDefaultTexture(GraphBuilder, ETextureDimension::Texture2DArray, EPixelFormat::PF_R32_UINT, FClearValueBinding::Transparent);
		FRDGBufferSRVRef DefaultBuffer = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultBuffer(GraphBuilder, 4, 0u), PF_R32_UINT);
		OutStrataUniformParameters.bRoughDiffuse = 0;
		OutStrataUniformParameters.MaxBytesPerPixel = 0;
		OutStrataUniformParameters.PeelLayersAboveDepth = 0;
		OutStrataUniformParameters.bRoughnessTracking = 0;
		OutStrataUniformParameters.SliceStoringDebugStrataTreeData = -1;
		OutStrataUniformParameters.FirstSliceStoringStrataSSSData = -1;
		OutStrataUniformParameters.TileSize = 0;
		OutStrataUniformParameters.TileSizeLog2 = 0;
		OutStrataUniformParameters.TileCount = 0;
		OutStrataUniformParameters.TileOffset = 0;
		OutStrataUniformParameters.OverflowTileCount = 0;
		OutStrataUniformParameters.OverflowTileOffset = 0;
		OutStrataUniformParameters.MaterialTextureArray = DefaultTextureArray;
		OutStrataUniformParameters.TopLayerTexture = SystemTextures.DefaultNormal8Bit;
		OutStrataUniformParameters.OpaqueRoughRefractionTexture = SystemTextures.Black;
		OutStrataUniformParameters.BSDFTileTexture = SystemTextures.Black;
		OutStrataUniformParameters.BSDFOffsetTexture = SystemTextures.Black;
		OutStrataUniformParameters.BSDFTileCountBuffer = DefaultBuffer;
	}
}

void BindStrataForwardPasslUniformParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, FStrataForwardPassUniformParameters& OutStrataUniformParameters)
{
	FStrataSceneData* StrataSceneData = View.StrataViewData.SceneData;
	if (IsStrataEnabled() && StrataSceneData)
	{
		OutStrataUniformParameters.MaxBytesPerPixel = StrataSceneData->MaxBytesPerPixel;
		OutStrataUniformParameters.bRoughDiffuse = StrataSceneData->bRoughDiffuse ? 1u : 0u;
		OutStrataUniformParameters.PeelLayersAboveDepth = StrataSceneData->PeelLayersAboveDepth;
		OutStrataUniformParameters.bRoughnessTracking = StrataSceneData->bRoughnessTracking ? 1u : 0u;
		OutStrataUniformParameters.FirstSliceStoringStrataSSSData = StrataSceneData->FirstSliceStoringStrataSSSData;
		OutStrataUniformParameters.MaterialTextureArray = StrataSceneData->MaterialTextureArray;
		OutStrataUniformParameters.TopLayerTexture = StrataSceneData->TopLayerTexture;
	}
	else
	{
		const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
		FRDGTextureRef DefaultTextureArray = GSystemTextures.GetDefaultTexture(GraphBuilder, ETextureDimension::Texture2DArray, EPixelFormat::PF_R32_UINT, FClearValueBinding::Transparent);
		OutStrataUniformParameters.MaxBytesPerPixel = 0;
		OutStrataUniformParameters.bRoughDiffuse = 0;
		OutStrataUniformParameters.PeelLayersAboveDepth = 0;
		OutStrataUniformParameters.bRoughnessTracking = 0;
		OutStrataUniformParameters.FirstSliceStoringStrataSSSData = -1;
		OutStrataUniformParameters.MaterialTextureArray = DefaultTextureArray;
		OutStrataUniformParameters.TopLayerTexture = SystemTextures.DefaultNormal8Bit;
	}
}

void BindStrataMobileForwardPasslUniformParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, FStrataMobileForwardPassUniformParameters& OutStrataUniformParameters)
{
	FStrataSceneData* StrataSceneData = View.StrataViewData.SceneData;
	if (IsStrataEnabled() && StrataSceneData)
	{
		OutStrataUniformParameters.MaxBytesPerPixel = StrataSceneData->MaxBytesPerPixel;
		OutStrataUniformParameters.bRoughDiffuse = StrataSceneData->bRoughDiffuse ? 1u : 0u;
		OutStrataUniformParameters.PeelLayersAboveDepth = StrataSceneData->PeelLayersAboveDepth;
		OutStrataUniformParameters.bRoughnessTracking = StrataSceneData->bRoughnessTracking ? 1u : 0u;
	}
	else
	{
		OutStrataUniformParameters.MaxBytesPerPixel = 0;
		OutStrataUniformParameters.bRoughDiffuse = 0;
		OutStrataUniformParameters.PeelLayersAboveDepth = 0;
		OutStrataUniformParameters.bRoughnessTracking = 0;
	}
}

TRDGUniformBufferRef<FStrataGlobalUniformParameters> BindStrataGlobalUniformParameters(const FViewInfo& View)
{
	check(View.StrataViewData.StrataGlobalUniformParameters != nullptr || !IsStrataEnabled());
	return View.StrataViewData.StrataGlobalUniformParameters;
}

static void BindStrataPublicGlobalUniformParameters(FRDGBuilder& GraphBuilder, FStrataSceneData* StrataSceneData, FStrataPublicGlobalUniformParameters& OutStrataUniformParameters)
{
	const FRDGSystemTextures& SystemTextures = FRDGSystemTextures::Get(GraphBuilder);
	if (StrataSceneData && StrataSceneData->TopLayerTexture)
	{
		OutStrataUniformParameters.TopLayerTexture = StrataSceneData->TopLayerTexture;
	}
	else
	{
		OutStrataUniformParameters.TopLayerTexture = SystemTextures.Black;
	}

	//TODO: Other Strata scene textures or other globals.
}

TRDGUniformBufferRef<FStrataPublicGlobalUniformParameters> CreatePublicGlobalUniformBuffer(FRDGBuilder& GraphBuilder, FStrataSceneData* StrataScene)
{
	FStrataPublicGlobalUniformParameters* StrataPublicUniformParameters = GraphBuilder.AllocParameters<FStrataPublicGlobalUniformParameters>();
	check(StrataPublicUniformParameters);
	BindStrataPublicGlobalUniformParameters(GraphBuilder, StrataScene, *StrataPublicUniformParameters);
	return GraphBuilder.CreateUniformBuffer(StrataPublicUniformParameters);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FStrataBSDFTilePassCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FStrataBSDFTilePassCS);
	SHADER_USE_PARAMETER_STRUCT(FStrataBSDFTilePassCS, FGlobalShader);

	class FWaveOps : SHADER_PERMUTATION_BOOL("PERMUTATION_WAVE_OPS");
	using FPermutationDomain = TShaderPermutationDomain<FWaveOps>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER(int32, bRectPrimitive)
		SHADER_PARAMETER(int32, TileSizeLog2)
		SHADER_PARAMETER(FIntPoint, TileCount_Primary)
		SHADER_PARAMETER(FIntPoint, TileOffset_Primary)
		SHADER_PARAMETER(FIntPoint, OverflowTileCount)
		SHADER_PARAMETER(FIntPoint, OverflowTileOffset)
		SHADER_PARAMETER(FIntPoint, ViewResolution)
		SHADER_PARAMETER(uint32, MaxBytesPerPixel)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TopLayerTexture)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2DArray<uint>, MaterialTextureArray)

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWBSDFTileTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RWBSDFOffsetTexture)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWBSDFTileCountBuffer)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, TileListBuffer)
		RDG_BUFFER_ACCESS(TileIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const bool bUseWaveIntrinsics = FDataDrivenShaderPlatformInfo::GetSupportsWaveOperations(Parameters.Platform) != ERHIFeatureSupport::Unsupported;
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FWaveOps>() && !bUseWaveIntrinsics)
		{
			return false;
		}
		return GetMaxSupportedFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5 && Strata::IsStrataEnabled();
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_BSDF_TILE"), 1);

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FWaveOps>())
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
		}
	}
};
IMPLEMENT_GLOBAL_SHADER(FStrataBSDFTilePassCS, "/Engine/Private/Strata/StrataMaterialClassification.usf", "BSDFTileMainCS", SF_Compute);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FStrataMaterialTileClassificationPassCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FStrataMaterialTileClassificationPassCS);
	SHADER_USE_PARAMETER_STRUCT(FStrataMaterialTileClassificationPassCS, FGlobalShader);

	class FCmask : SHADER_PERMUTATION_BOOL("PERMUTATION_CMASK");
	class FWaveOps : SHADER_PERMUTATION_BOOL("PERMUTATION_WAVE_OPS");
	class FDecal : SHADER_PERMUTATION_BOOL("PERMUTATION_DECAL"); 
	using FPermutationDomain = TShaderPermutationDomain<FCmask, FWaveOps, FDecal>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER(int32, FirstSliceStoringStrataSSSData)
		SHADER_PARAMETER(int32, bRectPrimitive)
		SHADER_PARAMETER(FIntPoint, ViewResolution)
		SHADER_PARAMETER(uint32, MaxBytesPerPixel)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TopLayerTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, TopLayerCmaskTexture)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, TileDrawIndirectDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, SimpleTileListDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, SingleTileListDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, ComplexTileListDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, OpaqueRoughRefractionTileListDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, OpaqueRoughRefractionSSSWithoutTileListDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, DecalSimpleTileListDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, DecalSingleTileListDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, DecalComplexTileListDataBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<uint>, MaterialTextureArrayUAV)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, OpaqueRoughRefractionTexture)
		SHADER_PARAMETER_STRUCT_INCLUDE(FDBufferParameters, DBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, SceneStencilTexture)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const bool bUseWaveIntrinsics = FDataDrivenShaderPlatformInfo::GetSupportsWaveOperations(Parameters.Platform) != ERHIFeatureSupport::Unsupported;
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FWaveOps>() && !bUseWaveIntrinsics)
		{
			return false;
		}
		if (PermutationVector.Get<FDecal>() && !IsConsolePlatform(Parameters.Platform))
		{
			return false;
		}		
		return GetMaxSupportedFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5 && Strata::IsStrataEnabled();
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_TILE_CATEGORIZATION"), 1);

		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FWaveOps>())
		{
			OutEnvironment.CompilerFlags.Add(CFLAG_WaveOperations);
		}
	}
};
IMPLEMENT_GLOBAL_SHADER(FStrataMaterialTileClassificationPassCS, "/Engine/Private/Strata/StrataMaterialClassification.usf", "TileMainCS", SF_Compute);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FStrataDBufferPassCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FStrataDBufferPassCS);
	SHADER_USE_PARAMETER_STRUCT(FStrataDBufferPassCS, FGlobalShader);

	class FTileType : SHADER_PERMUTATION_INT("PERMUTATION_TILETYPE", 3);
	using FPermutationDomain = TShaderPermutationDomain<FTileType>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntPoint, ViewResolution)
		SHADER_PARAMETER(uint32, MaxBytesPerPixel)
		SHADER_PARAMETER(uint32, FirstSliceStoringStrataSSSData)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_INCLUDE(FDBufferParameters, DBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, TopLayerTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<uint>, MaterialTextureArrayUAV)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, TileListBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<float>, SceneStencilTexture)
		RDG_BUFFER_ACCESS(TileIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return GetMaxSupportedFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5 && Strata::IsStrataEnabled() && IsUsingDBuffers(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		const uint32 StrataStencilDbufferMask 
			= GET_STENCIL_BIT_MASK(STRATA_RECEIVE_DBUFFER_NORMAL, 1) 
			| GET_STENCIL_BIT_MASK(STRATA_RECEIVE_DBUFFER_DIFFUSE, 1) 
			| GET_STENCIL_BIT_MASK(STRATA_RECEIVE_DBUFFER_ROUGHNESS, 1);

		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_DBUFFER"), 1);
		OutEnvironment.SetDefine(TEXT("STRATA_STENCIL_DBUFFER_MASK"), StrataStencilDbufferMask);
		OutEnvironment.SetDefine(TEXT("STENCIL_STRATA_RECEIVE_DBUFFER_NORMAL_BIT_ID"), STENCIL_STRATA_RECEIVE_DBUFFER_NORMAL_BIT_ID);
		OutEnvironment.SetDefine(TEXT("STENCIL_STRATA_RECEIVE_DBUFFER_DIFFUSE_BIT_ID"), STENCIL_STRATA_RECEIVE_DBUFFER_DIFFUSE_BIT_ID);
		OutEnvironment.SetDefine(TEXT("STENCIL_STRATA_RECEIVE_DBUFFER_ROUGHNESS_BIT_ID"), STENCIL_STRATA_RECEIVE_DBUFFER_ROUGHNESS_BIT_ID);

		// Needed as top layer texture can be a uint2
		OutEnvironment.CompilerFlags.Add(CFLAG_AllowTypedUAVLoads);
	}
};
IMPLEMENT_GLOBAL_SHADER(FStrataDBufferPassCS, "/Engine/Private/Strata/StrataDBuffer.usf", "MainCS", SF_Compute);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FStrataMaterialTilePrepareArgsPassCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FStrataMaterialTilePrepareArgsPassCS);
	SHADER_USE_PARAMETER_STRUCT(FStrataMaterialTilePrepareArgsPassCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer,   TileDrawIndirectDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, TileDispatchIndirectDataBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return GetMaxSupportedFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5 && Strata::IsStrataEnabled();
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_MATERIAL_TILE_PREPARE_ARGS"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FStrataMaterialTilePrepareArgsPassCS, "/Engine/Private/Strata/StrataMaterialClassification.usf", "ArgsMainCS", SF_Compute);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FStrataBSDFTilePrepareArgsPassCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FStrataBSDFTilePrepareArgsPassCS);
	SHADER_USE_PARAMETER_STRUCT(FStrataBSDFTilePrepareArgsPassCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntPoint, TileCount_Primary)
		SHADER_PARAMETER(FIntPoint, TileOffset_Primary)
		SHADER_PARAMETER(FIntPoint, OverflowTileCount)
		SHADER_PARAMETER(FIntPoint, OverflowTileOffset)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, TileDrawIndirectDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, TileDispatchIndirectDataBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, TileDispatchPerThreadIndirectDataBuffer)
		END_SHADER_PARAMETER_STRUCT()

		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return GetMaxSupportedFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5 && Strata::IsStrataEnabled();
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_BSDF_TILE_PREPARE_ARGS"), 1);
	}
};
IMPLEMENT_GLOBAL_SHADER(FStrataBSDFTilePrepareArgsPassCS, "/Engine/Private/Strata/StrataMaterialClassification.usf", "ArgsMainCS", SF_Compute);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

bool FStrataTilePassVS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return GetMaxSupportedFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5; // We do not skip the compilation because we have some conditional when tiling a pass and the shader must be fetch once before hand.
}

void FStrataTilePassVS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("SHADER_TILE_VS"), 1);
}

class FStrataMaterialStencilTaggingPassPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FStrataMaterialStencilTaggingPassPS);
	SHADER_USE_PARAMETER_STRUCT(FStrataMaterialStencilTaggingPassPS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(Strata::FStrataTilePassVS::FParameters, VS)
		SHADER_PARAMETER(FVector4f, DebugTileColor)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return GetMaxSupportedFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5 && Strata::IsStrataEnabled();
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_STENCIL_TAGGING_PS"), 1); 
	}
};

IMPLEMENT_GLOBAL_SHADER(FStrataTilePassVS, "/Engine/Private/Strata/StrataTile.usf", "StrataTilePassVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FStrataMaterialStencilTaggingPassPS, "/Engine/Private/Strata/StrataTile.usf", "StencilTaggingMainPS", SF_Pixel);

static FStrataTileParameter InternalSetTileParameters(FRDGBuilder* GraphBuilder, const FViewInfo& View, const EStrataTileType TileType)
{
	FStrataTileParameter Out;
	if (TileType != EStrataTileType::ECount)
	{
		Out.TileListBuffer = View.StrataViewData.ClassificationTileListBufferSRV[TileType];
		Out.TileIndirectBuffer = View.StrataViewData.ClassificationTileDrawIndirectBuffer;
	}
	else if (GraphBuilder)
	{
		FRDGBufferRef BufferDummy = GSystemTextures.GetDefaultBuffer(*GraphBuilder, 4, 0u);
		FRDGBufferSRVRef BufferDummySRV = GraphBuilder->CreateSRV(BufferDummy, PF_R32_UINT);
		Out.TileListBuffer = BufferDummySRV;
		Out.TileIndirectBuffer = BufferDummy;
	}
	return Out;
}

FStrataTilePassVS::FParameters SetTileParameters(
	const FViewInfo& View,
	const EStrataTileType TileType,
	EPrimitiveType& PrimitiveType)
{
	FStrataTileParameter Temp = InternalSetTileParameters(nullptr, View, TileType);
	PrimitiveType = GRHISupportsRectTopology ? PT_RectList : PT_TriangleList;

	FStrataTilePassVS::FParameters Out;
	Out.OutputViewMinRect = FVector2f(View.CachedViewUniformShaderParameters->ViewRectMin.X, View.CachedViewUniformShaderParameters->ViewRectMin.Y);
	Out.OutputViewSizeAndInvSize = View.CachedViewUniformShaderParameters->ViewSizeAndInvSize;
	Out.OutputBufferSizeAndInvSize = View.CachedViewUniformShaderParameters->BufferSizeAndInvSize;
	Out.ViewScreenToTranslatedWorld = View.CachedViewUniformShaderParameters->ScreenToTranslatedWorld;
	Out.TileListBuffer = Temp.TileListBuffer;
	Out.TileIndirectBuffer = Temp.TileIndirectBuffer;
	return Out;
}

FStrataTilePassVS::FParameters SetTileParameters(
	FRDGBuilder& GraphBuilder, 
	const FViewInfo& View, 
	const EStrataTileType TileType,
	EPrimitiveType& PrimitiveType)
{
	FStrataTileParameter Temp = InternalSetTileParameters(&GraphBuilder, View, TileType);
	PrimitiveType = GRHISupportsRectTopology ? PT_RectList : PT_TriangleList;

	FStrataTilePassVS::FParameters Out;
	Out.OutputViewMinRect = FVector2f(View.CachedViewUniformShaderParameters->ViewRectMin.X, View.CachedViewUniformShaderParameters->ViewRectMin.Y);
	Out.OutputViewSizeAndInvSize = View.CachedViewUniformShaderParameters->ViewSizeAndInvSize;
	Out.OutputBufferSizeAndInvSize = View.CachedViewUniformShaderParameters->BufferSizeAndInvSize;
	Out.ViewScreenToTranslatedWorld = View.CachedViewUniformShaderParameters->ScreenToTranslatedWorld;
	Out.TileListBuffer = Temp.TileListBuffer;
	Out.TileIndirectBuffer = Temp.TileIndirectBuffer;
	return Out;
}

FStrataTileParameter SetTileParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, const EStrataTileType TileType)
{
	return InternalSetTileParameters(&GraphBuilder, View, TileType);
}

uint32 TileTypeDrawIndirectArgOffset(const EStrataTileType Type)
{
	check(Type >= 0 && Type < EStrataTileType::ECount);
	return GetStrataTileTypeDrawIndirectArgOffset_Byte(Type);
}

uint32 TileTypeDispatchIndirectArgOffset(const EStrataTileType Type)
{
	check(Type >= 0 && Type < EStrataTileType::ECount);
	return GetStrataTileTypeDispatchIndirectArgOffset_Byte(Type);
}

// Add additionnaly bits for filling/clearing stencil to ensure that the 'Strata' bits are not corrupted by the stencil shadows 
// when generating shadow mask. Withouth these 'trailing' bits, the incr./decr. operation would change/corrupt the 'Strata' bits
constexpr uint32 StencilBit_Fast_1	  = StencilBit_Fast;
constexpr uint32 StencilBit_Single_1  = StencilBit_Single;
constexpr uint32 StencilBit_Complex_1 = StencilBit_Complex; 

void AddStrataInternalClassificationTilePass(
	FRDGBuilder& GraphBuilder,
	const FViewInfo& View,
	const FRDGTextureRef* DepthTexture,
	const FRDGTextureRef* ColorTexture,
	EStrataTileType TileMaterialType,
	const bool bDebug = false)
{
	EPrimitiveType StrataTilePrimitiveType = PT_TriangleList;
	FIntPoint DebugOutputResolution = FIntPoint(View.UnscaledViewRect.Width(), View.UnscaledViewRect.Height());
	const FIntRect ViewRect = View.ViewRect;

	FStrataMaterialStencilTaggingPassPS::FParameters* ParametersPS = GraphBuilder.AllocParameters<FStrataMaterialStencilTaggingPassPS::FParameters>();
	ParametersPS->VS = Strata::SetTileParameters(GraphBuilder, View, TileMaterialType, StrataTilePrimitiveType);

	FStrataTilePassVS::FPermutationDomain VSPermutationVector;
	VSPermutationVector.Set< FStrataTilePassVS::FEnableDebug >(bDebug);
	VSPermutationVector.Set< FStrataTilePassVS::FEnableTexCoordScreenVector >(false);
	TShaderMapRef<FStrataTilePassVS> VertexShader(View.ShaderMap, VSPermutationVector);
	TShaderMapRef<FStrataMaterialStencilTaggingPassPS> PixelShader(View.ShaderMap);

	// For debug purpose
	if (bDebug)
	{
		// ViewRect contains the scaled resolution according to TSR screen percentage.
		// The ColorTexture can be larger than the screen resolution if the screen percentage has be manipulated to be >100%.
		// So we simply re-use the previously computed ViewResolutionFraction to recover the targeted resolution in the editor.
		// TODO fix this for split screen.
		const float InvViewResolutionFraction = 1.0f / View.CachedViewUniformShaderParameters->ViewResolutionFraction;
		DebugOutputResolution = FIntPoint(float(ViewRect.Width()) * InvViewResolutionFraction, float(ViewRect.Height()) * InvViewResolutionFraction);

		check(ColorTexture);
		ParametersPS->RenderTargets[0] = FRenderTargetBinding(*ColorTexture, ERenderTargetLoadAction::ELoad);
		switch (TileMaterialType)
		{
		case EStrataTileType::ESimple:							ParametersPS->DebugTileColor = FVector4f(0.0f, 1.0f, 0.0f, 1.0); break;
		case EStrataTileType::ESingle:							ParametersPS->DebugTileColor = FVector4f(1.0f, 1.0f, 0.0f, 1.0); break;
		case EStrataTileType::EComplex:							ParametersPS->DebugTileColor = FVector4f(1.0f, 0.0f, 0.0f, 1.0); break;

		case EStrataTileType::EOpaqueRoughRefraction:			ParametersPS->DebugTileColor = FVector4f(0.0f, 1.0f, 1.0f, 1.0); break;
		case EStrataTileType::EOpaqueRoughRefractionSSSWithout:	ParametersPS->DebugTileColor = FVector4f(0.0f, 0.0f, 1.0f, 1.0); break;

		case EStrataTileType::EDecalSingle:						ParametersPS->DebugTileColor = FVector4f(0.0f, 1.0f, 0.0f, 1.0); break;
		case EStrataTileType::EDecalSimple:						ParametersPS->DebugTileColor = FVector4f(1.0f, 1.0f, 0.0f, 1.0); break;
		case EStrataTileType::EDecalComplex:					ParametersPS->DebugTileColor = FVector4f(1.0f, 0.0f, 0.0f, 1.0); break;
		default: check(false);
		}
	}
	else
	{
		check(DepthTexture);
		ParametersPS->RenderTargets.DepthStencil = FDepthStencilBinding(
			*DepthTexture,
			ERenderTargetLoadAction::ELoad,
			ERenderTargetLoadAction::ELoad,
			FExclusiveDepthStencil::DepthNop_StencilWrite);
		ParametersPS->DebugTileColor = FVector4f(ForceInitToZero);
	}
	
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("Substrate::%sClassificationPass(%s)", bDebug ? TEXT("Debug") : TEXT("Stencil"), ToString(TileMaterialType)),
		ParametersPS,
		ERDGPassFlags::Raster,
		[ParametersPS, VertexShader, PixelShader, ViewRect, DebugOutputResolution, StrataTilePrimitiveType, TileMaterialType, bDebug](FRHICommandList& RHICmdList)
		{
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			uint32 StencilRef = 0xFF;
			if (bDebug)
			{
				// Use premultiplied alpha blending, pixel shader and depth/stencil is off
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
				GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Add, BF_One, BF_InverseSourceAlpha, BO_Add, BF_Zero, BF_One>::GetRHI();
				GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			}
			else
			{
				check(TileMaterialType != EStrataTileType::ECount && TileMaterialType != EStrataTileType::EOpaqueRoughRefraction && TileMaterialType != EStrataTileType::EOpaqueRoughRefractionSSSWithout);

				// No blending and no pixel shader required. Stencil will be written to.
				GraphicsPSOInit.BoundShaderState.PixelShaderRHI = nullptr;
				GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
				switch (TileMaterialType)
				{
				case EStrataTileType::ESimple:
				{
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
						false, CF_Always,
						true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
						false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
						0xFF, StencilBit_Fast_1>::GetRHI();
					StencilRef = StencilBit_Fast_1;
				}
				break;
				case EStrataTileType::ESingle:
				{
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
						false, CF_Always,
						true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
						false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
						0xFF, StencilBit_Single_1>::GetRHI();
					StencilRef = StencilBit_Single_1;
				}
				break;
				case EStrataTileType::EComplex:
				{
					GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<
						false, CF_Always,
						true, CF_Always, SO_Keep, SO_Keep, SO_Replace,
						false, CF_Always, SO_Keep, SO_Keep, SO_Keep,
						0xFF, StencilBit_Complex_1>::GetRHI();
					StencilRef = StencilBit_Complex_1;
				}
				break;
				}
			}
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GFilterVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.PrimitiveType = StrataTilePrimitiveType;
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);
			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), ParametersPS->VS);
			if (bDebug)
			{
				// Debug rendering is aways done during the post-processing stage, which has an ViewMinRect set to (0,0)
				SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), *ParametersPS);
				RHICmdList.SetViewport(0, 0, 0.0f, DebugOutputResolution.X, DebugOutputResolution.Y, 1.0f);
			}
			else
			{
				RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, 0.0f, ViewRect.Max.X, ViewRect.Max.Y, 1.0f);
			}
			RHICmdList.SetStreamSource(0, nullptr, 0);
			RHICmdList.DrawPrimitiveIndirect(ParametersPS->VS.TileIndirectBuffer->GetIndirectRHICallBuffer(), TileTypeDrawIndirectArgOffset(TileMaterialType));
		});
}

void AddStrataStencilPass(
	FRDGBuilder& GraphBuilder,
	const TArray<FViewInfo>& Views,
	const FMinimalSceneTextures& SceneTextures)
{
	for (int32 i = 0; i < Views.Num(); ++i)
	{
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", i);

		const FViewInfo& View = Views[i];
		AddStrataInternalClassificationTilePass(GraphBuilder, View, &SceneTextures.Depth.Target, nullptr, EStrataTileType::EComplex);
		AddStrataInternalClassificationTilePass(GraphBuilder, View, &SceneTextures.Depth.Target, nullptr, EStrataTileType::ESingle);
		AddStrataInternalClassificationTilePass(GraphBuilder, View, &SceneTextures.Depth.Target, nullptr, EStrataTileType::ESimple);
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void AppendStrataMRTs(const FSceneRenderer& SceneRenderer, uint32& RenderTargetCount, TStaticArray<FTextureRenderTargetBinding, MaxSimultaneousRenderTargets>& RenderTargets)
{
	if (Strata::IsStrataEnabled() && SceneRenderer.Scene)
	{
		// If this function changes, update Strata::SetBasePassRenderTargetOutputFormat()
		 
		// Add 2 uint for Strata fast path. 
		// - We must clear the first uint to 0 to identify pixels that have not been written to.
		// - We must never clear the second uint, it will only be written/read if needed.
		auto AddStrataOutputTarget = [&](int16 StrataMaterialArraySlice, bool bNeverClear = false)
		{
			RenderTargets[RenderTargetCount] = FTextureRenderTargetBinding(SceneRenderer.Scene->StrataSceneData.MaterialTextureArray, StrataMaterialArraySlice, bNeverClear);
			RenderTargetCount++;
		};
		const bool bSupportCMask = SupportsCMask(GMaxRHIShaderPlatform);
		for (int i = 0; i < STRATA_BASE_PASS_MRT_OUTPUT_COUNT; ++i)
		{
			const bool bNeverClear = bSupportCMask || i != 0; // Only allow clearing the first slice containing the header
			AddStrataOutputTarget(i, bNeverClear);
		}

		// Add another MRT for Strata top layer information. We want to follow the usual clear process which can leverage fast clear.
		{
			RenderTargets[RenderTargetCount] = FTextureRenderTargetBinding(SceneRenderer.Scene->StrataSceneData.TopLayerTexture);
			RenderTargetCount++;
		};
	}
}

void SetBasePassRenderTargetOutputFormat(const EShaderPlatform Platform, const FMaterialShaderParameters& MaterialParameters, FShaderCompilerEnvironment& OutEnvironment, EGBufferLayout GBufferLayout)
{
	if (Strata::IsStrataEnabled())
	{
		FGBufferParams GBufferParams = FShaderCompileUtilities::FetchGBufferParamsRuntime(Platform, GBufferLayout);

		// If it is not a water material, we force bHasSingleLayerWaterSeparatedMainLight to false, in order to 
		// ensure non-used MRTs are not inserted in BufferInfo. Otherwise this would offset Strata MRTs, causing 
		// MRTs' format to be incorrect
		if (!MaterialParameters.bIsUsedWithWater)
		{
			GBufferParams.bHasSingleLayerWaterSeparatedMainLight = false;
		}
		const FGBufferInfo BufferInfo = FetchFullGBufferInfo(GBufferParams);

		// Add N uint for Strata fast path
		for (int i = 0; i < STRATA_BASE_PASS_MRT_OUTPUT_COUNT; ++i)
		{
			OutEnvironment.SetRenderTargetOutputFormat(BufferInfo.NumTargets + i, PF_R32_UINT);
		}

		// Add another MRT for Strata top layer information
		OutEnvironment.SetRenderTargetOutputFormat(BufferInfo.NumTargets + STRATA_BASE_PASS_MRT_OUTPUT_COUNT, GetTopLayerTextureFormat(IsDBufferPassEnabled(Platform)));
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void AddStrataMaterialClassificationPass(FRDGBuilder& GraphBuilder, const FMinimalSceneTextures& SceneTextures, const FDBufferTextures& DBufferTextures, const TArray<FViewInfo>& Views)
{
	RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, IsStrataEnabled() && Views.Num() > 0, "Substrate::MaterialClassification");
	if (!IsStrataEnabled())
	{
		return;
	}

	// Optionally run tile classification in async compute
	const ERDGPassFlags PassFlags = IsClassificationAsync() ? ERDGPassFlags::AsyncCompute : ERDGPassFlags::Compute;

	for (int32 i = 0; i < Views.Num(); ++i)
	{
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", i);

		const FViewInfo& View = Views[i];
		bool bWaveOps = GRHISupportsWaveOperations && GRHIMaximumWaveSize >= 64 && FDataDrivenShaderPlatformInfo::GetSupportsWaveOperations(View.GetShaderPlatform()) != ERHIFeatureSupport::Unsupported;
		
		const FStrataViewData* StrataViewData = &View.StrataViewData;
		const FStrataSceneData* StrataSceneData = View.StrataViewData.SceneData;

		// Tile reduction
		{
			// When the platform support explicit CMask texture, we disable material data bufferclear. Material buffer buffer clear (the header part) is done during the classification pass.  
			// To reduce the reading bandwidth, we rely on TopLayerData CMask to 'drive' the clearing process. This allows to clear quickly empty tiles.
			const bool bSupportCMask = SupportsCMask(View.GetShaderPlatform());
			FRDGTextureRef TopLayerCmaskTexture = StrataSceneData->TopLayerTexture;			
			if (bSupportCMask)
			{
				// Combine DBuffer RTWriteMasks; will end up in one texture we can load from in the base pass PS and decide whether to do the actual work or not.
				FRDGTextureRef SourceCMaskTextures[] = { StrataSceneData->TopLayerTexture };
				FRenderTargetWriteMask::Decode(GraphBuilder, View.ShaderMap, MakeArrayView(SourceCMaskTextures), TopLayerCmaskTexture, GFastVRamConfig.DBufferMask, TEXT("Substrate::TopLayerCmask"));
			}

			// If Dbuffer pass (i.e. apply DBuffer data after the base-pass) is enabled, run special classification for outputing tile with/without tiles
			const bool bDBufferTiles = IsDBufferPassEnabled(View.GetShaderPlatform()) && CVarSubstrateDBufferPassDedicatedTiles.GetValueOnRenderThread() > 0 && DBufferTextures.IsValid() && IsConsolePlatform(View.GetShaderPlatform());

			FStrataMaterialTileClassificationPassCS::FPermutationDomain PermutationVector;
			PermutationVector.Set< FStrataMaterialTileClassificationPassCS::FCmask >(bSupportCMask);
			PermutationVector.Set< FStrataMaterialTileClassificationPassCS::FWaveOps >(bWaveOps);
			PermutationVector.Set< FStrataMaterialTileClassificationPassCS::FDecal>(bDBufferTiles);
			TShaderMapRef<FStrataMaterialTileClassificationPassCS> ComputeShader(View.ShaderMap, PermutationVector);
			FStrataMaterialTileClassificationPassCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStrataMaterialTileClassificationPassCS::FParameters>();
			PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
			PassParameters->bRectPrimitive = GRHISupportsRectTopology ? 1 : 0;
			PassParameters->ViewResolution = View.ViewRect.Size();
			PassParameters->MaxBytesPerPixel = StrataSceneData->MaxBytesPerPixel;
			PassParameters->FirstSliceStoringStrataSSSData = StrataSceneData->FirstSliceStoringStrataSSSData;
			PassParameters->TopLayerTexture = StrataSceneData->TopLayerTexture;
			PassParameters->TopLayerCmaskTexture = TopLayerCmaskTexture;
			PassParameters->MaterialTextureArrayUAV = StrataSceneData->MaterialTextureArrayUAV;
			PassParameters->OpaqueRoughRefractionTexture = StrataSceneData->OpaqueRoughRefractionTexture;
			PassParameters->TileDrawIndirectDataBuffer = StrataViewData->ClassificationTileDrawIndirectBufferUAV;
			PassParameters->DBuffer = GetDBufferParameters(GraphBuilder, DBufferTextures, View.GetShaderPlatform());
			PassParameters->SceneStencilTexture = SceneTextures.Stencil;
			// Regular tiles
			PassParameters->SimpleTileListDataBuffer = StrataViewData->ClassificationTileListBufferUAV[EStrataTileType::ESimple];
			PassParameters->SingleTileListDataBuffer = StrataViewData->ClassificationTileListBufferUAV[EStrataTileType::ESingle];
			PassParameters->ComplexTileListDataBuffer = StrataViewData->ClassificationTileListBufferUAV[EStrataTileType::EComplex];
			// Decal tiles
			PassParameters->DecalSimpleTileListDataBuffer = StrataViewData->ClassificationTileListBufferUAV[EStrataTileType::EDecalSimple];
			PassParameters->DecalSingleTileListDataBuffer = StrataViewData->ClassificationTileListBufferUAV[EStrataTileType::EDecalSingle];
			PassParameters->DecalComplexTileListDataBuffer = StrataViewData->ClassificationTileListBufferUAV[EStrataTileType::EDecalComplex];
			// Rough refraction tiles
			PassParameters->OpaqueRoughRefractionTileListDataBuffer = StrataViewData->ClassificationTileListBufferUAV[EStrataTileType::EOpaqueRoughRefraction];
			PassParameters->OpaqueRoughRefractionSSSWithoutTileListDataBuffer = StrataViewData->ClassificationTileListBufferUAV[EStrataTileType::EOpaqueRoughRefractionSSSWithout];

			const uint32 GroupSize = 8;
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Substrate::MaterialTileClassification(%s%s)", bWaveOps ? TEXT("Wave") : TEXT("SharedMemory"), bSupportCMask ? TEXT(", CMask") : TEXT("")),
				PassFlags,
				ComputeShader,
				PassParameters,
				FComputeShaderUtils::GetGroupCount(PassParameters->ViewResolution, GroupSize));
		}

		// Tile indirect dispatch args conversion
		{
			TShaderMapRef<FStrataMaterialTilePrepareArgsPassCS> ComputeShader(View.ShaderMap);
			FStrataMaterialTilePrepareArgsPassCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStrataMaterialTilePrepareArgsPassCS::FParameters>();
			PassParameters->TileDrawIndirectDataBuffer = GraphBuilder.CreateSRV(StrataViewData->ClassificationTileDrawIndirectBuffer, PF_R32_UINT);
			PassParameters->TileDispatchIndirectDataBuffer = StrataViewData->ClassificationTileDispatchIndirectBufferUAV;

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Substrate::MaterialTilePrepareArgs"),
				PassFlags,
				ComputeShader,
				PassParameters,
				FIntVector(1,1,1));
		}

		// Compute BSDF tile index and material read offset
		if (StrataSceneData->BSDFOffsetTexture)
		{
			FRDGBufferUAVRef RWBSDFTileCountBuffer = GraphBuilder.CreateUAV(StrataViewData->BSDFTileCountBuffer, PF_R32_UINT);
			AddClearUAVPass(GraphBuilder, RWBSDFTileCountBuffer, 0u);

			FStrataBSDFTilePassCS::FPermutationDomain PermutationVector;
			PermutationVector.Set< FStrataBSDFTilePassCS::FWaveOps >(bWaveOps);
			TShaderMapRef<FStrataBSDFTilePassCS> ComputeShader(View.ShaderMap, PermutationVector);
			FStrataBSDFTilePassCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStrataBSDFTilePassCS::FParameters>();
			PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
			PassParameters->TileSizeLog2 = STRATA_TILE_SIZE_DIV_AS_SHIFT;
			PassParameters->TileCount_Primary = StrataViewData->TileCount;
			PassParameters->TileOffset_Primary = StrataViewData->TileOffset;
			PassParameters->OverflowTileCount = StrataViewData->OverflowTileCount;
			PassParameters->OverflowTileOffset = StrataViewData->OverflowTileOffset;
			PassParameters->ViewResolution = View.ViewRect.Size();
			PassParameters->MaxBytesPerPixel = StrataSceneData->MaxBytesPerPixel;
			PassParameters->TopLayerTexture = StrataSceneData->TopLayerTexture;
			PassParameters->MaterialTextureArray = StrataSceneData->MaterialTextureArraySRV;
			PassParameters->TileListBuffer = StrataViewData->ClassificationTileListBufferSRV[EStrataTileType::EComplex];
			PassParameters->TileIndirectBuffer = StrataViewData->ClassificationTileDispatchIndirectBuffer;

			PassParameters->RWBSDFOffsetTexture = GraphBuilder.CreateUAV(StrataSceneData->BSDFOffsetTexture);
			PassParameters->RWBSDFTileTexture = GraphBuilder.CreateUAV(StrataViewData->BSDFTileTexture);
			PassParameters->RWBSDFTileCountBuffer = RWBSDFTileCountBuffer;

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Substrate::BSDFTileAndOffsets(%s)", bWaveOps ? TEXT("Wave") : TEXT("SharedMemory")),
				PassFlags,
				ComputeShader,
				PassParameters,
				PassParameters->TileIndirectBuffer,
				TileTypeDispatchIndirectArgOffset(EStrataTileType::EComplex));
		}

		// Tile indirect dispatch args conversion
		if (StrataSceneData->BSDFOffsetTexture)
		{
			TShaderMapRef<FStrataBSDFTilePrepareArgsPassCS> ComputeShader(View.ShaderMap);
			FStrataBSDFTilePrepareArgsPassCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStrataBSDFTilePrepareArgsPassCS::FParameters>();
			PassParameters->TileCount_Primary = StrataViewData->TileCount;
			PassParameters->TileOffset_Primary = StrataViewData->TileOffset;
			PassParameters->OverflowTileCount = StrataViewData->OverflowTileCount;
			PassParameters->OverflowTileOffset = StrataViewData->OverflowTileOffset;
			PassParameters->TileDrawIndirectDataBuffer = GraphBuilder.CreateSRV(StrataViewData->BSDFTileCountBuffer, PF_R32_UINT);
			PassParameters->TileDispatchIndirectDataBuffer = GraphBuilder.CreateUAV(StrataViewData->BSDFTileDispatchIndirectBuffer, PF_R32_UINT);
			PassParameters->TileDispatchPerThreadIndirectDataBuffer = GraphBuilder.CreateUAV(StrataViewData->BSDFTilePerThreadDispatchIndirectBuffer, PF_R32_UINT);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Substrate::BSDFTilePrepareArgs"),
				PassFlags,
				ComputeShader,
				PassParameters,
				FIntVector(1, 1, 1));
		}
	}
}


void AddStrataDBufferPass(FRDGBuilder& GraphBuilder, const FMinimalSceneTextures& SceneTextures, const FDBufferTextures& DBufferTextures, const TArray<FViewInfo>& Views)
{
	RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, IsStrataEnabled() && Views.Num() > 0, "Substrate::DBuffer");
	if (!IsStrataEnabled() || !DBufferTextures.IsValid())
	{
		return;
	}

	for (int32 i = 0; i < Views.Num(); ++i)
	{
		RDG_EVENT_SCOPE_CONDITIONAL(GraphBuilder, Views.Num() > 1, "View%d", i);

		const FViewInfo& View = Views[i];
		if (!IsUsingDBuffers(View.GetShaderPlatform()) || View.Family->EngineShowFlags.Decals == 0 || !IsDBufferPassEnabled(View.GetShaderPlatform()))
		{
			continue;
		}

		const FStrataViewData* StrataViewData = &View.StrataViewData;
		const FStrataSceneData* StrataSceneData = View.StrataViewData.SceneData;

		FRDGTextureUAVRef RWMaterialTexture = GraphBuilder.CreateUAV(StrataSceneData->MaterialTextureArray, ERDGUnorderedAccessViewFlags::SkipBarrier);
		FRDGTextureUAVRef RWTopLayerTexture = GraphBuilder.CreateUAV(StrataSceneData->TopLayerTexture, ERDGUnorderedAccessViewFlags::SkipBarrier);

		auto DBufferPass = [&](EStrataTileType TileType)
		{
			// Only simple & single material are support but also dispatch complex tiles, 
			// as they can contain simple/single material pixels

			uint32 TilePermutation = 0;
			switch(TileType)
			{
			case EStrataTileType::EComplex:
			case EStrataTileType::EDecalComplex:
				TilePermutation = 2;
				break;
			case EStrataTileType::ESingle:
			case EStrataTileType::EDecalSingle:
				TilePermutation = 1;
				break;
			case EStrataTileType::ESimple:
			case EStrataTileType::EDecalSimple:
				TilePermutation = 0;
				break;
			}

			FStrataDBufferPassCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FStrataDBufferPassCS::FTileType>(TilePermutation);

			TShaderMapRef<FStrataDBufferPassCS> ComputeShader(View.ShaderMap, PermutationVector);
			FStrataDBufferPassCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FStrataDBufferPassCS::FParameters>();

			PassParameters->DBuffer = GetDBufferParameters(GraphBuilder, DBufferTextures, View.GetShaderPlatform());
			PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
			PassParameters->ViewResolution = View.ViewRect.Size();
			PassParameters->MaxBytesPerPixel = StrataSceneData->MaxBytesPerPixel;
			PassParameters->TopLayerTexture = RWTopLayerTexture;
			PassParameters->MaterialTextureArrayUAV = RWMaterialTexture;
			PassParameters->FirstSliceStoringStrataSSSData = StrataSceneData->FirstSliceStoringStrataSSSData;
			PassParameters->SceneStencilTexture = SceneTextures.Stencil;

			PassParameters->TileListBuffer = StrataViewData->ClassificationTileListBufferSRV[TileType];
			PassParameters->TileIndirectBuffer = StrataViewData->ClassificationTileDispatchIndirectBuffer;

			// Dispatch with tile data
			const uint32 GroupSize = 8;
			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("Substrate::Dbuffer(%s)", ToString(TileType)),
				ERDGPassFlags::Compute,
				ComputeShader,
				PassParameters,
				PassParameters->TileIndirectBuffer,
				TileTypeDispatchIndirectArgOffset(TileType));
		};

		const bool bDbufferTiles = CVarSubstrateDBufferPassDedicatedTiles.GetValueOnRenderThread() > 0;
		DBufferPass(bDbufferTiles ? EStrataTileType::EDecalComplex : EStrataTileType::EComplex);
		DBufferPass(bDbufferTiles ? EStrataTileType::EDecalSingle : EStrataTileType::ESingle);
		DBufferPass(bDbufferTiles ? EStrataTileType::EDecalSimple : EStrataTileType::ESimple);
	}
}

} // namespace Strata
