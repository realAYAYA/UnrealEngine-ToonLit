// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ShaderParameterMacros.h"
#include "RenderGraphResources.h"
#include "MeshPassProcessor.h"
#include "UnifiedBuffer.h"
#include "RHIUtilities.h"
#include "StrataDefinitions.h"
#include "RenderGraphUtils.h"
#include "GBufferInfo.h"
#include "RendererUtils.h"

// Forward declarations.
class FScene;
class FSceneRenderer;
class FRDGBuilder;
struct FMinimalSceneTextures;
struct FScreenPassTexture;
struct FDBufferTextures;

BEGIN_SHADER_PARAMETER_STRUCT(FStrataBasePassUniformParameters, )
	SHADER_PARAMETER(uint32, MaxBytesPerPixel)
	SHADER_PARAMETER(uint32, bRoughDiffuse)
	SHADER_PARAMETER(uint32, PeelLayersAboveDepth)
	SHADER_PARAMETER(uint32, bRoughnessTracking)
	SHADER_PARAMETER(int32, SliceStoringDebugStrataTreeDataWithoutMRT)
	SHADER_PARAMETER(int32, FirstSliceStoringStrataSSSDataWithoutMRT)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<uint>, MaterialTextureArrayUAVWithoutRTs)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, OpaqueRoughRefractionTextureUAV)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FStrataForwardPassUniformParameters, )
	SHADER_PARAMETER(uint32, MaxBytesPerPixel)
	SHADER_PARAMETER(uint32, bRoughDiffuse)
	SHADER_PARAMETER(uint32, PeelLayersAboveDepth)
	SHADER_PARAMETER(uint32, bRoughnessTracking)
	SHADER_PARAMETER(int32, FirstSliceStoringStrataSSSData)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<uint>, MaterialTextureArray)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint2>, TopLayerTexture)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FStrataMobileForwardPassUniformParameters, )
	SHADER_PARAMETER(uint32, MaxBytesPerPixel)
	SHADER_PARAMETER(uint32, bRoughDiffuse)
	SHADER_PARAMETER(uint32, PeelLayersAboveDepth)
	SHADER_PARAMETER(uint32, bRoughnessTracking)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FStrataTileParameter, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, TileListBuffer)
	RDG_BUFFER_ACCESS(TileIndirectBuffer, ERHIAccess::IndirectArgs)
END_SHADER_PARAMETER_STRUCT()

// This paramater struct is declared with RENDERER_API even though it is not public. This is
// to workaround other modules doing 'private include' of the Renderer module
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FStrataGlobalUniformParameters, RENDERER_API)
	SHADER_PARAMETER(uint32, MaxBytesPerPixel)
	SHADER_PARAMETER(uint32, bRoughDiffuse)
	SHADER_PARAMETER(uint32, PeelLayersAboveDepth)
	SHADER_PARAMETER(uint32, bRoughnessTracking)
	SHADER_PARAMETER(int32,  SliceStoringDebugStrataTreeData)
	SHADER_PARAMETER(int32,  FirstSliceStoringStrataSSSData)
	SHADER_PARAMETER(uint32, TileSize)
	SHADER_PARAMETER(uint32, TileSizeLog2)
	SHADER_PARAMETER(FIntPoint, TileCount)
	SHADER_PARAMETER(FIntPoint, TileOffset)
	SHADER_PARAMETER(FIntPoint, OverflowTileCount)
	SHADER_PARAMETER(FIntPoint, OverflowTileOffset)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<uint>, MaterialTextureArray)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint2>, TopLayerTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, OpaqueRoughRefractionTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, BSDFOffsetTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, BSDFTileTexture)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, BSDFTileCountBuffer)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

// This must map to the STRATA_TILE_TYPE defines.
enum EStrataTileType : uint32
{
	ESimple								= STRATA_TILE_TYPE_SIMPLE,
	ESingle								= STRATA_TILE_TYPE_SINGLE,
	EComplex							= STRATA_TILE_TYPE_COMPLEX,
	EOpaqueRoughRefraction				= STRATA_TILE_TYPE_ROUGH_REFRACT,
	EOpaqueRoughRefractionSSSWithout	= STRATA_TILE_TYPE_ROUGH_REFRACT_SSS_WITHOUT,
	EDecalSimple						= STRATA_TILE_TYPE_DECAL_SIMPLE,
	EDecalSingle						= STRATA_TILE_TYPE_DECAL_SINGLE,
	EDecalComplex						= STRATA_TILE_TYPE_DECAL_COMPLEX,
	ECount
};

const TCHAR* ToString(EStrataTileType Type);

struct FStrataSceneData
{
	uint32 MaxBytesPerPixel;
	bool bRoughDiffuse;
	int32 PeelLayersAboveDepth;
	bool bRoughnessTracking;

	int32 SliceStoringDebugStrataTreeDataWithoutMRT;
	int32 SliceStoringDebugStrataTreeData;
	int32 FirstSliceStoringStrataSSSDataWithoutMRT;
	int32 FirstSliceStoringStrataSSSData;

	// Resources allocated and updated each frame

	FRDGTextureRef MaterialTextureArray = nullptr;
	FRDGTextureUAVRef MaterialTextureArrayUAVWithoutRTs = nullptr;
	FRDGTextureUAVRef MaterialTextureArrayUAV = nullptr;
	FRDGTextureSRVRef MaterialTextureArraySRV = nullptr;

	FRDGTextureRef TopLayerTexture = nullptr;
	FRDGTextureRef OpaqueRoughRefractionTexture = nullptr;

	FRDGTextureUAVRef TopLayerTextureUAV = nullptr;
	FRDGTextureUAVRef OpaqueRoughRefractionTextureUAV = nullptr;

	FRDGTextureRef BSDFOffsetTexture = nullptr;

	// Used when the subsurface luminance is separated from the scene color
	FRDGTextureRef SeparatedSubSurfaceSceneColor = nullptr;

	// Used for Luminance that should go through opaque rough refraction (when under a top layer interface)
	FRDGTextureRef SeparatedOpaqueRoughRefractionSceneColor = nullptr;

	//Public facing minimal uniform data.
	TRDGUniformBufferRef<FStrataPublicGlobalUniformParameters> StrataPublicGlobalUniformParameters{};
};

struct FStrataViewData
{
	// Max BSDF count among all visible materials
	uint32 MaxBSDFCount = 0;
	uint32 MaxBytePerPixel = 0;

	FIntPoint TileCount  = FIntPoint(0, 0);
	FIntPoint TileOffset = FIntPoint(0, 0);
	FIntPoint OverflowTileCount = FIntPoint(0, 0);
	FIntPoint OverflowTileOffset = FIntPoint(0, 0);

	FRDGBufferRef    ClassificationTileListBuffer[STRATA_TILE_TYPE_COUNT];
	FRDGBufferSRVRef ClassificationTileListBufferSRV[STRATA_TILE_TYPE_COUNT];
	FRDGBufferUAVRef ClassificationTileListBufferUAV[STRATA_TILE_TYPE_COUNT];

	FRDGBufferRef    ClassificationTileDrawIndirectBuffer = nullptr;
	FRDGBufferUAVRef ClassificationTileDrawIndirectBufferUAV = nullptr;

	FRDGBufferRef    ClassificationTileDispatchIndirectBuffer = nullptr;
	FRDGBufferUAVRef ClassificationTileDispatchIndirectBufferUAV = nullptr;

	FRDGTextureRef BSDFTileTexture = nullptr;
	FRDGBufferRef  BSDFTileCountBuffer = nullptr;
	FRDGBufferRef  BSDFTileDispatchIndirectBuffer = nullptr;
	FRDGBufferRef  BSDFTilePerThreadDispatchIndirectBuffer = nullptr;

	FStrataSceneData* SceneData = nullptr;

	TRDGUniformBufferRef<FStrataGlobalUniformParameters> StrataGlobalUniformParameters{};

	void Reset();
};

namespace Strata
{
constexpr uint32 StencilBit_Fast   = 0x08; // In sync with SceneRenderTargets.h - GET_STENCIL_BIT_MASK(STENCIL_STRATA_FASTPATH)
constexpr uint32 StencilBit_Single = 0x10; // In sync with SceneRenderTargets.h - GET_STENCIL_BIT_MASK(STENCIL_STRATA_SINGLEPATH)
constexpr uint32 StencilBit_Complex= 0x20; // In sync with SceneRenderTargets.h - GET_STENCIL_BIT_MASK(STENCIL_STRATA_COMPLEX)

FIntPoint GetStrataTextureResolution(const FViewInfo& View, const FIntPoint& InResolution);

void InitialiseStrataFrameSceneData(FRDGBuilder& GraphBuilder, FSceneRenderer& SceneRenderer);

void BindStrataBasePassUniformParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, FStrataBasePassUniformParameters& OutStrataUniformParameters);
void BindStrataForwardPasslUniformParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, FStrataForwardPassUniformParameters& OutStrataUniformParameters);
void BindStrataMobileForwardPasslUniformParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, FStrataMobileForwardPassUniformParameters& OutStrataUniformParameters);
TRDGUniformBufferRef<FStrataGlobalUniformParameters> BindStrataGlobalUniformParameters(const FViewInfo& View);

void AppendStrataMRTs(const FSceneRenderer& SceneRenderer, uint32& BasePassTextureCount, TStaticArray<FTextureRenderTargetBinding, MaxSimultaneousRenderTargets>& BasePassTextures);
void SetBasePassRenderTargetOutputFormat(const EShaderPlatform Platform, const FMaterialShaderParameters& MaterialParameters, FShaderCompilerEnvironment& OutEnvironment, EGBufferLayout GBufferLayout);

TRDGUniformBufferRef<FStrataPublicGlobalUniformParameters> CreatePublicGlobalUniformBuffer(FRDGBuilder& GraphBuilder, FStrataSceneData* StrataScene);

void AddStrataMaterialClassificationPass(FRDGBuilder& GraphBuilder, const FMinimalSceneTextures& SceneTextures, const FDBufferTextures& DBufferTextures, const TArray<FViewInfo>& Views);
void AddStrataDBufferPass(FRDGBuilder& GraphBuilder, const FMinimalSceneTextures& SceneTextures, const FDBufferTextures& DBufferTextures, const TArray<FViewInfo>& Views);

void AddStrataStencilPass(FRDGBuilder& GraphBuilder, const TArray<FViewInfo>& Views, const FMinimalSceneTextures& SceneTextures);

void AddStrataOpaqueRoughRefractionPasses(
	FRDGBuilder& GraphBuilder,
	FSceneTextures& SceneTextures,
	TArrayView<const FViewInfo> Views);

bool ShouldRenderStrataDebugPasses(const FViewInfo& View);
FScreenPassTexture AddStrataDebugPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& ScreenPassSceneColor);

class FStrataTilePassVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FStrataTilePassVS);
	SHADER_USE_PARAMETER_STRUCT(FStrataTilePassVS, FGlobalShader);

	class FEnableDebug : SHADER_PERMUTATION_BOOL("PERMUTATION_ENABLE_DEBUG");
	class FEnableTexCoordScreenVector : SHADER_PERMUTATION_BOOL("PERMUTATION_ENABLE_TEXCOORD_SCREENVECTOR");
	using FPermutationDomain = TShaderPermutationDomain<FEnableDebug, FEnableTexCoordScreenVector>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// It would be possible to use the ViewUniformBuffer instead of copying the data here, 
		// but we would have to make sure the view UB is added to all passes using this parameter structure.
		// We should not add it here to now have duplicated input UB.
		SHADER_PARAMETER(FVector2f, OutputViewMinRect)
		SHADER_PARAMETER(FVector4f, OutputViewSizeAndInvSize)
		SHADER_PARAMETER(FVector4f, OutputBufferSizeAndInvSize)
		SHADER_PARAMETER(FMatrix44f, ViewScreenToTranslatedWorld)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, TileListBuffer)
		RDG_BUFFER_ACCESS(TileIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

FStrataTileParameter SetTileParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, const EStrataTileType Type);
FStrataTilePassVS::FParameters SetTileParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, const EStrataTileType Type, EPrimitiveType& PrimitiveType);
FStrataTilePassVS::FParameters SetTileParameters(const FViewInfo& View, const EStrataTileType Type, EPrimitiveType& PrimitiveType);
uint32 TileTypeDrawIndirectArgOffset(const EStrataTileType Type);
uint32 TileTypeDispatchIndirectArgOffset(const EStrataTileType Type);
};
