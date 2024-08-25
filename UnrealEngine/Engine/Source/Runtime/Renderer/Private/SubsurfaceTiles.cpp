// Copyright Epic Games, Inc. All Rights Reserved.

#include "SubsurfaceTiles.h"
#include "SceneRendering.h"
#include "DataDrivenShaderPlatformInfo.h"

bool FSubsurfaceTilePassVS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

void FSubsurfaceTilePassVS::ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
{
	FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	OutEnvironment.SetDefine(TEXT("SHADER_TILE_VS"), 1);
	OutEnvironment.SetDefine(TEXT("SUBSURFACE_TILE_SIZE"), FSubsurfaceTiles::TileSize);
}

bool FSubsurfaceTileFallbackScreenPassVS::ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
{
	return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
}

FSubsurfaceTilePassVS::FParameters GetSubsurfaceTileParameters(const FScreenPassTextureViewport& TileViewport, const FSubsurfaceTiles& InTile, FSubsurfaceTiles::ETileType TileType)
{
	FSubsurfaceTilePassVS::FParameters Out;
	Out.TileType = uint32(TileType);
	Out.bRectPrimitive = InTile.bRectPrimitive ? 1 : 0;
	Out.ViewMin = TileViewport.Rect.Min;
	Out.ViewMax = TileViewport.Rect.Max;
	Out.ExtentInverse = FVector2f(1.f / TileViewport.Extent.X, 1.f / TileViewport.Extent.Y);
	Out.TileDataBuffer = InTile.GetTileBufferSRV(TileType);
	Out.TileIndirectBuffer = InTile.TileIndirectDrawBuffer;
	return Out;
}


class FClearUAVBuildIndirectDispatchBufferCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClearUAVBuildIndirectDispatchBufferCS)
	SHADER_USE_PARAMETER_STRUCT(FClearUAVBuildIndirectDispatchBufferCS, FGlobalShader)

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntPoint, ViewportSize)
		SHADER_PARAMETER(uint32, Offset)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint32>, ConditionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint32>, RWIndirectDispatchArgsBuffer)
		END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FShaderPermutationParameters&, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("SUBSURFACE_COMPUTE"), 1);
		OutEnvironment.SetDefine(TEXT("SUBSURFACE_TILE_SIZE"), FSubsurfaceTiles::TileSize);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearUAVBuildIndirectDispatchBufferCS, "/Engine/Private/PostprocessSubsurfaceTile.usf", "BuildIndirectDispatchArgsCS", SF_Compute);

class FClearUAVCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FClearUAVCS)
	SHADER_USE_PARAMETER_STRUCT(FClearUAVCS, FGlobalShader)

		BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FIntPoint, TextureExtent)
		SHADER_PARAMETER(FIntPoint, ViewportMin)
		RDG_BUFFER_ACCESS(IndirectDispatchArgsBuffer, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>,TextureOutput)
		END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FShaderPermutationParameters&, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("SUBSURFACE_COMPUTE"), 1);
		OutEnvironment.SetDefine(TEXT("SUBSURFACE_TILE_SIZE"), FSubsurfaceTiles::TileSize);
	}
};

IMPLEMENT_GLOBAL_SHADER(FClearUAVCS, "/Engine/Private/PostprocessSubsurfaceTile.usf", "ClearUAV", SF_Compute);


void AddConditionalClearBlackUAVPass(FRDGBuilder& GraphBuilder, FRDGEventName&& Name, 
	FRDGTextureRef Texture, const FScreenPassTextureViewport& ScreenPassViewport, FRDGBufferRef ConditionBuffer, uint32 Offset)
{

	FRDGBufferRef IndirectDispatchArgsBuffer = GraphBuilder.CreateBuffer(
		FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1),
		TEXT("IndirectDispatchArgsBuffer"));

	{
		// build the indirect dispatch arguments buffer ( compute the group count on GPU conditionally)
		FClearUAVBuildIndirectDispatchBufferCS::FParameters* PassParameters =
			GraphBuilder.AllocParameters<FClearUAVBuildIndirectDispatchBufferCS::FParameters>();
		PassParameters->ViewportSize = FIntPoint(ScreenPassViewport.Rect.Max.X - ScreenPassViewport.Rect.Min.X + 1,
												ScreenPassViewport.Rect.Max.Y - ScreenPassViewport.Rect.Min.Y + 1);
		PassParameters->Offset = Offset;
		PassParameters->ConditionBuffer = 
			GraphBuilder.CreateSRV(ConditionBuffer, EPixelFormat::PF_R32_UINT);
		PassParameters->RWIndirectDispatchArgsBuffer = 
			GraphBuilder.CreateUAV(IndirectDispatchArgsBuffer, EPixelFormat::PF_R32_UINT);

		TShaderMapRef<FClearUAVBuildIndirectDispatchBufferCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SSS::ClearUAV(BuildIndirectDispatchBuffer)"),
			ComputeShader,
			PassParameters,
			FIntVector(1, 1, 1));
	}

	FClearUAVCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FClearUAVCS::FParameters>();
	PassParameters->TextureExtent = Texture->Desc.Extent;
	PassParameters->ViewportMin = ScreenPassViewport.Rect.Min;
	PassParameters->TextureOutput = GraphBuilder.CreateUAV(Texture);
	PassParameters->IndirectDispatchArgsBuffer = IndirectDispatchArgsBuffer;

	TShaderMapRef<FClearUAVCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		Forward<FRDGEventName>(Name),
		ComputeShader,
		PassParameters,
		IndirectDispatchArgsBuffer, 0);
}

IMPLEMENT_GLOBAL_SHADER(FSubsurfaceTilePassVS, "/Engine/Private/PostProcessSubsurfaceTile.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FSubsurfaceTileFallbackScreenPassVS, "/Engine/Private/PostProcessSubsurfaceTile.usf", "SubsurfaceTileFallbackScreenPassVS", SF_Vertex);

const TCHAR* ToString(FSubsurfaceTiles::ETileType Type)
{
	switch (Type)
	{
	case FSubsurfaceTiles::ETileType::All:			return TEXT("SSS(All)");
	case FSubsurfaceTiles::ETileType::AFIS:		return TEXT("SSS(AFIS)");
	case FSubsurfaceTiles::ETileType::SEPARABLE:		return TEXT("SSS(Separable)");
	default:											return TEXT("Unknown");
	}
}