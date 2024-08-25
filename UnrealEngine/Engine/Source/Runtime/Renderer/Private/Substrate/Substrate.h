// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ShaderParameterMacros.h"
#include "RenderGraphResources.h"
#include "MeshPassProcessor.h"
#include "UnifiedBuffer.h"
#include "RHIUtilities.h"
#include "SubstrateDefinitions.h"
#include "GBufferInfo.h"
#include "RendererUtils.h"
#include "ShaderParameterStruct.h"

// Forward declarations.
class FScene;
class FSceneRenderer;
class FRDGBuilder;
struct FDBufferTextures;
struct FMinimalSceneTextures;
struct FScreenPassTexture;
struct FTextureRenderTargetBinding;

BEGIN_SHADER_PARAMETER_STRUCT(FSubstrateCommonParameters, )
	SHADER_PARAMETER(uint32, MaxBytesPerPixel)
	SHADER_PARAMETER(uint32, MaxClosurePerPixel)
	SHADER_PARAMETER(uint32, bRoughDiffuse)
	SHADER_PARAMETER(uint32, PeelLayersAboveDepth)
	SHADER_PARAMETER(uint32, bRoughnessTracking)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FSubstrateBasePassUniformParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FSubstrateCommonParameters, Common)
	SHADER_PARAMETER(int32, SliceStoringDebugSubstrateTreeDataWithoutMRT)
	SHADER_PARAMETER(int32, FirstSliceStoringSubstrateSSSDataWithoutMRT)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<uint>, MaterialTextureArrayUAVWithoutRTs)
	SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float3>, OpaqueRoughRefractionTextureUAV)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FSubstrateForwardPassUniformParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FSubstrateCommonParameters, Common)
	SHADER_PARAMETER(int32, FirstSliceStoringSubstrateSSSData)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<uint>, MaterialTextureArray)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<SUBSTRATE_TOP_LAYER_TYPE>, TopLayerTexture)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FSubstrateMobileForwardPassUniformParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FSubstrateCommonParameters, Common)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FSubstrateTileParameter, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, TileListBuffer)
	SHADER_PARAMETER(uint32, TileListBufferOffset)
	SHADER_PARAMETER(uint32, TileEncoding)
	RDG_BUFFER_ACCESS(TileIndirectBuffer, ERHIAccess::IndirectArgs)
END_SHADER_PARAMETER_STRUCT()

// This parameter struct is declared with RENDERER_API even though it is not public. This is
// to workaround other modules doing 'private include' of the Renderer module
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FSubstrateGlobalUniformParameters, RENDERER_API)
	SHADER_PARAMETER_STRUCT_INCLUDE(FSubstrateCommonParameters, Common)
	SHADER_PARAMETER(int32,  SliceStoringDebugSubstrateTreeData)
	SHADER_PARAMETER(int32,  FirstSliceStoringSubstrateSSSData)
	SHADER_PARAMETER(uint32, TileSize)
	SHADER_PARAMETER(uint32, TileSizeLog2)
	SHADER_PARAMETER(FIntPoint, TileCount)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray<uint>, MaterialTextureArray)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<SUBSTRATE_TOP_LAYER_TYPE>, TopLayerTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float3>, OpaqueRoughRefractionTexture)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, ClosureOffsetTexture)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ClosureTileBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ClosureTileCountBuffer)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FSubstratePublicGlobalUniformParameters, RENDERER_API)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D<SUBSTRATE_TOP_LAYER_TYPE>, TopLayerTexture)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

// This must map to the SUBSTRATE_TILE_TYPE defines.
enum ESubstrateTileType : uint32
{
	ESimple								= SUBSTRATE_TILE_TYPE_SIMPLE,
	ESingle								= SUBSTRATE_TILE_TYPE_SINGLE,
	EComplex							= SUBSTRATE_TILE_TYPE_COMPLEX,
	EComplexSpecial						= SUBSTRATE_TILE_TYPE_COMPLEX_SPECIAL,
	EOpaqueRoughRefraction				= SUBSTRATE_TILE_TYPE_ROUGH_REFRACT,
	EOpaqueRoughRefractionSSSWithout	= SUBSTRATE_TILE_TYPE_ROUGH_REFRACT_SSS_WITHOUT,
	EDecalSimple						= SUBSTRATE_TILE_TYPE_DECAL_SIMPLE,
	EDecalSingle						= SUBSTRATE_TILE_TYPE_DECAL_SINGLE,
	EDecalComplex						= SUBSTRATE_TILE_TYPE_DECAL_COMPLEX,
	ECount
};

const TCHAR* ToString(ESubstrateTileType Type);

struct FSubstrateSceneData
{
	// Track max BytesPerPixel / ClosurePerPixel amoung all views
	uint32 ViewsMaxBytesPerPixel = 0;
	uint32 ViewsMaxClosurePerPixel = 0;

	// Track max BytesPerPixel / ClosurePerPixel ever encountered since the scene was created
	uint32 PersistentMaxBytesPerPixel = 0;
	uint32 PersistentMaxClosurePerPixel = 0;
	bool bUsesComplexSpecialRenderPath = false;

	// Current max BytesPerPixel / ClosurePerPixel
	uint32 EffectiveMaxBytesPerPixel = 0;
	uint32 EffectiveMaxClosurePerPixel = 0;

	int32 PeelLayersAboveDepth = -1;
	bool bRoughDiffuse = false;
	bool bRoughnessTracking = false;

	int32 SliceStoringDebugSubstrateTreeDataWithoutMRT = -1;
	int32 SliceStoringDebugSubstrateTreeData = -1;
	int32 FirstSliceStoringSubstrateSSSDataWithoutMRT = -1;
	int32 FirstSliceStoringSubstrateSSSData = -1;

	// Resources allocated and updated each frame

	FRDGTextureRef MaterialTextureArray = nullptr;
	FRDGTextureUAVRef MaterialTextureArrayUAVWithoutRTs = nullptr;
	FRDGTextureUAVRef MaterialTextureArrayUAV = nullptr;
	FRDGTextureSRVRef MaterialTextureArraySRV = nullptr;

	FRDGTextureRef TopLayerTexture = nullptr;
	FRDGTextureRef OpaqueRoughRefractionTexture = nullptr;

	FRDGTextureUAVRef TopLayerTextureUAV = nullptr;
	FRDGTextureUAVRef OpaqueRoughRefractionTextureUAV = nullptr;

	FRDGTextureRef ClosureOffsetTexture = nullptr;

	// Used when the subsurface luminance is separated from the scene color
	FRDGTextureRef SeparatedSubSurfaceSceneColor = nullptr;

	// Used for Luminance that should go through opaque rough refraction (when under a top layer interface)
	FRDGTextureRef SeparatedOpaqueRoughRefractionSceneColor = nullptr;

	//Public facing minimal uniform data.
	TRDGUniformBufferRef<FSubstratePublicGlobalUniformParameters> SubstratePublicGlobalUniformParameters{};
};

struct FSubstrateViewData
{
	// Max BytePerPixel & ClosurePerPixel count among all visible materials
	uint32 MaxClosurePerPixel = 0;
	uint32 MaxBytesPerPixel = 0;

	// True if any material requires the complex special path (e.g. glints or SpecularLUT)
	bool bUsesComplexSpecialRenderPath = 0;

	FIntPoint TileCount  = FIntPoint(0, 0);
	uint32    TileEncoding = SUBSTRATE_TILE_ENCODING_16BITS;
	uint32    LayerCount = 0;

	FRDGBufferRef    ClassificationTileListBuffer = nullptr;
	FRDGBufferSRVRef ClassificationTileListBufferSRV = nullptr;
	FRDGBufferUAVRef ClassificationTileListBufferUAV = nullptr;
	uint32			 ClassificationTileListBufferOffset[SUBSTRATE_TILE_TYPE_COUNT];

	FRDGBufferRef    ClassificationTileDrawIndirectBuffer = nullptr;
	FRDGBufferUAVRef ClassificationTileDrawIndirectBufferUAV = nullptr;

	FRDGBufferRef    ClassificationTileDispatchIndirectBuffer = nullptr;
	FRDGBufferUAVRef ClassificationTileDispatchIndirectBufferUAV = nullptr;

	FRDGBufferRef  ClosureTileBuffer = nullptr;
	FRDGBufferRef  ClosureTileCountBuffer = nullptr;
	FRDGBufferRef  ClosureTileDispatchIndirectBuffer = nullptr;
	FRDGBufferRef  ClosureTilePerThreadDispatchIndirectBuffer = nullptr;

	FSubstrateSceneData* SceneData = nullptr;

	TRDGUniformBufferRef<FSubstrateGlobalUniformParameters> SubstrateGlobalUniformParameters{};

	void Reset();
};

namespace Substrate
{
constexpr uint32 StencilBit_Fast			= 0x10; // In sync with SceneRenderTargets.h - GET_STENCIL_BIT_MASK(STENCIL_SUBSTRATE_FASTPATH)
constexpr uint32 StencilBit_Single			= 0x20; // In sync with SceneRenderTargets.h - GET_STENCIL_BIT_MASK(STENCIL_SUBSTRATE_SINGLEPATH)
constexpr uint32 StencilBit_Complex			= 0x40; // In sync with SceneRenderTargets.h - GET_STENCIL_BIT_MASK(STENCIL_SUBSTRATE_COMPLEX)
constexpr uint32 StencilBit_ComplexSpecial	= 0x80; // In sync with SceneRenderTargets.h - GET_STENCIL_BIT_MASK(STENCIL_SUBSTRATE_COMPLEX_SPECIAL)	

FIntPoint GetSubstrateTextureResolution(const FViewInfo& View, const FIntPoint& InResolution);
uint32 GetSubstrateMaxClosureCount(const FViewInfo& View);
bool GetSubstrateUsesComplexSpecialPath(const FViewInfo& View);

void InitialiseSubstrateFrameSceneData(FRDGBuilder& GraphBuilder, FSceneRenderer& SceneRenderer);

void BindSubstrateBasePassUniformParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, FSubstrateBasePassUniformParameters& OutSubstrateUniformParameters);
void BindSubstrateForwardPasslUniformParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, FSubstrateForwardPassUniformParameters& OutSubstrateUniformParameters);
void BindSubstrateMobileForwardPasslUniformParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, FSubstrateMobileForwardPassUniformParameters& OutSubstrateUniformParameters);
TRDGUniformBufferRef<FSubstrateGlobalUniformParameters> BindSubstrateGlobalUniformParameters(const FViewInfo& View);

void AppendSubstrateMRTs(const FSceneRenderer& SceneRenderer, uint32& BasePassTextureCount, TArrayView<FTextureRenderTargetBinding> BasePassTextures);
void SetBasePassRenderTargetOutputFormat(const EShaderPlatform Platform, const FMaterialShaderParameters& MaterialParameters, FShaderCompilerEnvironment& OutEnvironment, EGBufferLayout GBufferLayout);

TRDGUniformBufferRef<FSubstratePublicGlobalUniformParameters> CreatePublicGlobalUniformBuffer(FRDGBuilder& GraphBuilder, FSubstrateSceneData* SubstrateScene);

void AddSubstrateMaterialClassificationPass(FRDGBuilder& GraphBuilder, const FMinimalSceneTextures& SceneTextures, const FDBufferTextures& DBufferTextures, const TArray<FViewInfo>& Views);
void AddSubstrateDBufferPass(FRDGBuilder& GraphBuilder, const FMinimalSceneTextures& SceneTextures, const FDBufferTextures& DBufferTextures, const TArray<FViewInfo>& Views);

void AddSubstrateStencilPass(FRDGBuilder& GraphBuilder, const TArray<FViewInfo>& Views, const FMinimalSceneTextures& SceneTextures);

void AddSubstrateOpaqueRoughRefractionPasses(
	FRDGBuilder& GraphBuilder,
	FSceneTextures& SceneTextures,
	TArrayView<const FViewInfo> Views);

bool ShouldRenderSubstrateDebugPasses(const FViewInfo& View);
FScreenPassTexture AddSubstrateDebugPasses(FRDGBuilder& GraphBuilder, const FViewInfo& View, FScreenPassTexture& ScreenPassSceneColor);

class FSubstrateTilePassVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSubstrateTilePassVS);
	SHADER_USE_PARAMETER_STRUCT(FSubstrateTilePassVS, FGlobalShader);

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
		SHADER_PARAMETER(uint32, TileListBufferOffset)
		SHADER_PARAMETER(uint32, TileEncoding)
		RDG_BUFFER_ACCESS(TileIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters);
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
};

FSubstrateTileParameter SetTileParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, const ESubstrateTileType Type);
FSubstrateTilePassVS::FParameters SetTileParameters(FRDGBuilder& GraphBuilder, const FViewInfo& View, const ESubstrateTileType Type, EPrimitiveType& PrimitiveType);
FSubstrateTilePassVS::FParameters SetTileParameters(const FViewInfo& View, const ESubstrateTileType Type, EPrimitiveType& PrimitiveType);
uint32 TileTypeDrawIndirectArgOffset(const ESubstrateTileType Type);
uint32 TileTypeDispatchIndirectArgOffset(const ESubstrateTileType Type);
};
