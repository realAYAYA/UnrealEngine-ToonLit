// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenScreenSpaceBentNormal.cpp
=============================================================================*/

#include "LumenScreenProbeGather.h"
#include "RendererPrivate.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"

float GLumenShortRangeAOSlopeCompareToleranceScale = .5f;
FAutoConsoleVariableRef CVarLumenShortRangeAOSlopeCompareToleranceScale(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.ScreenSpace.SlopeCompareToleranceScale"),
	GLumenShortRangeAOSlopeCompareToleranceScale,
	TEXT("Scales the slope threshold that screen space traces use to determine whether there was a hit."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenShortRangeAOFoliageOcclusionStrength = .7f;
FAutoConsoleVariableRef CVarLumenShortRangeAOFoliageOcclusionStrength(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.ScreenSpace.FoliageOcclusionStrength"),
	GLumenShortRangeAOFoliageOcclusionStrength,
	TEXT("Maximum strength of ScreenSpaceBentNormal occlusion on foliage and subsurface pixels.  Useful for reducing max occlusion to simulate subsurface scattering."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GLumenMaxShortRangeAOMultibounceAlbedo = .5f;
FAutoConsoleVariableRef CVarLumenMaxShortRangeAOMultibounceAlbedo(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.MaxMultibounceAlbedo"),
	GLumenMaxShortRangeAOMultibounceAlbedo,
	TEXT("Maximum albedo used for the AO multi-bounce approximation.  Useful for forcing near-white albedo to have some occlusion."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenShortRangeAOHairStrandsVoxelTrace = 1;
FAutoConsoleVariableRef GVarLumenShortRangeAOHairStrandsVoxelTrace(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.HairVoxelTrace"),
	GLumenShortRangeAOHairStrandsVoxelTrace,
	TEXT("Whether to trace against hair voxel structure for hair casting shadow onto opaques."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

int32 GLumenShortRangeAOHairStrandsScreenTrace = 0;
FAutoConsoleVariableRef GVarShortRangeAOHairStrandsScreenTrace(
	TEXT("r.Lumen.ScreenProbeGather.ShortRangeAO.HairScreenTrace"),
	GLumenShortRangeAOHairStrandsScreenTrace,
	TEXT("Whether to trace against hair depth for hair casting shadow onto opaques."),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

class FScreenSpaceShortRangeAOCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FScreenSpaceShortRangeAOCS)
	SHADER_USE_PARAMETER_STRUCT(FScreenSpaceShortRangeAOCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2DArray<float3>, RWScreenBentNormal)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_STRUCT_INCLUDE(FSceneTextureParameters, SceneTextures)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<uint>, LightingChannelsTexture)
		SHADER_PARAMETER_STRUCT_REF(FBlueNoise, BlueNoise)
		SHADER_PARAMETER(FVector4f, HZBUvFactorAndInvFactor)
		SHADER_PARAMETER(float, SlopeCompareToleranceScale)
		SHADER_PARAMETER(float, MaxScreenTraceFraction)
		SHADER_PARAMETER(float, ScreenTraceNoFallbackThicknessScale)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FurthestHZBTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, FurthestHZBTextureSampler)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FHairStrandsViewUniformParameters, HairStrands)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FVirtualVoxelParameters, HairStrandsVoxel)
		RDG_BUFFER_ACCESS(TileIndirectBuffer, ERHIAccess::IndirectArgs)
	END_SHADER_PARAMETER_STRUCT()

	class FNumPixelRays : SHADER_PERMUTATION_SPARSE_INT("NUM_PIXEL_RAYS", 4, 8, 16);
	class FOverflow : SHADER_PERMUTATION_BOOL("PERMUTATION_OVERFLOW_TILE"); 
	class FHairStrandsScreen : SHADER_PERMUTATION_BOOL("USE_HAIRSTRANDS_SCREEN");
	class FHairStrandsVoxel : SHADER_PERMUTATION_BOOL("USE_HAIRSTRANDS_VOXEL");
	using FPermutationDomain = TShaderPermutationDomain<FNumPixelRays, FOverflow, FHairStrandsScreen, FHairStrandsVoxel>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
		if (PermutationVector.Get<FOverflow>() && !Substrate::IsSubstrateEnabled())
		{
			return false;
		}
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static int32 GetGroupSize() 
	{
		// Sanity check
		static_assert(8 == SUBSTRATE_TILE_SIZE);
		return 8;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
		OutEnvironment.CompilerFlags.Add(CFLAG_Wave32);
	}
};

IMPLEMENT_GLOBAL_SHADER(FScreenSpaceShortRangeAOCS, "/Engine/Private/Lumen/LumenScreenSpaceBentNormal.usf", "ScreenSpaceShortRangeAOCS", SF_Compute);

FLumenScreenSpaceBentNormalParameters ComputeScreenSpaceShortRangeAO(
	FRDGBuilder& GraphBuilder, 
	const FScene* Scene,
	const FViewInfo& View, 
	const FSceneTextures& SceneTextures,
	FRDGTextureRef LightingChannelsTexture,
	const FBlueNoise& BlueNoise,
	float MaxScreenTraceFraction,
	float ScreenTraceNoFallbackThicknessScale,
	ERDGPassFlags ComputePassFlags)
{
	FLumenScreenSpaceBentNormalParameters OutParameters;

	const FSceneTextureParameters& SceneTextureParameters = GetSceneTextureParameters(GraphBuilder, SceneTextures);

	// When Substrate is enabled, increase the resolution for multi-layer tile overflowing (tile containing multi-BSDF data)
	FIntPoint BentNormalResolution = Substrate::GetSubstrateTextureResolution(View, View.GetSceneTexturesConfig().Extent);
	const uint32 ClosureCount = Substrate::GetSubstrateMaxClosureCount(View);
	FRDGTextureDesc ScreenBentNormalDesc(FRDGTextureDesc::Create2DArray(BentNormalResolution, PF_A2B10G10R10, FClearValueBinding::Black, TexCreate_ShaderResource | TexCreate_UAV, ClosureCount));
	FRDGTextureRef ScreenBentNormal = GraphBuilder.CreateTexture(ScreenBentNormalDesc, TEXT("Lumen.ScreenProbeGather.ScreenBentNormal"));

	int32 NumPixelRays = 4;

	if (View.FinalPostProcessSettings.LumenFinalGatherQuality >= 6.0f)
	{
		NumPixelRays = 16;
	}
	else if (View.FinalPostProcessSettings.LumenFinalGatherQuality >= 2.0f)
	{
		NumPixelRays = 8;
	}

	if (Lumen::UseHardwareRayTracedShortRangeAO(*View.Family))
	{
		RenderHardwareRayTracingShortRangeAO(
			GraphBuilder,
			Scene,
			SceneTextureParameters,
			BlueNoise,
			MaxScreenTraceFraction,
			View,
			ScreenBentNormal,
			NumPixelRays);
	}
	else
	{
		const bool bNeedTraceHairVoxel = HairStrands::HasViewHairStrandsVoxelData(View) && GLumenShortRangeAOHairStrandsVoxelTrace > 0;
		const bool bNeedTraceHairScreen = HairStrands::HasViewHairStrandsData(View) && GLumenShortRangeAOHairStrandsScreenTrace > 0;
		
		auto ScreenSpaceShortRangeAO = [&](bool bOverflow)
		{
			FScreenSpaceShortRangeAOCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FScreenSpaceShortRangeAOCS::FParameters>();
			PassParameters->RWScreenBentNormal = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ScreenBentNormal));
			PassParameters->SceneTexturesStruct = SceneTextures.UniformBuffer;
			PassParameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
			PassParameters->SceneTextures = SceneTextureParameters;

			if (!PassParameters->SceneTextures.GBufferVelocityTexture)
			{
				PassParameters->SceneTextures.GBufferVelocityTexture = GSystemTextures.GetBlackDummy(GraphBuilder);
			}

			PassParameters->MaxScreenTraceFraction = MaxScreenTraceFraction;
			PassParameters->ScreenTraceNoFallbackThicknessScale = ScreenTraceNoFallbackThicknessScale;
			PassParameters->ViewUniformBuffer = View.ViewUniformBuffer;
			PassParameters->LightingChannelsTexture = LightingChannelsTexture;
			PassParameters->BlueNoise = CreateUniformBufferImmediate(BlueNoise, EUniformBufferUsage::UniformBuffer_SingleDraw);

			const FVector2D ViewportUVToHZBBufferUV(
				float(View.ViewRect.Width()) / float(2 * View.HZBMipmap0Size.X),
				float(View.ViewRect.Height()) / float(2 * View.HZBMipmap0Size.Y)
			);

			PassParameters->HZBUvFactorAndInvFactor = FVector4f(
				ViewportUVToHZBBufferUV.X,
				ViewportUVToHZBBufferUV.Y,
				1.0f / ViewportUVToHZBBufferUV.X,
				1.0f / ViewportUVToHZBBufferUV.Y);

			PassParameters->FurthestHZBTexture = View.HZB;
			PassParameters->FurthestHZBTextureSampler = TStaticSamplerState<SF_Point>::GetRHI();
			PassParameters->SlopeCompareToleranceScale = GLumenShortRangeAOSlopeCompareToleranceScale;

			if (bNeedTraceHairScreen)
			{
				PassParameters->HairStrands = HairStrands::BindHairStrandsViewUniformParameters(View);
			}

			if (bNeedTraceHairVoxel)
			{
				PassParameters->HairStrandsVoxel = HairStrands::BindHairStrandsVoxelUniformParameters(View);
			}

			FScreenSpaceShortRangeAOCS::FPermutationDomain PermutationVector;
			PermutationVector.Set< FScreenSpaceShortRangeAOCS::FNumPixelRays >(NumPixelRays);
			PermutationVector.Set< FScreenSpaceShortRangeAOCS::FOverflow>(bOverflow);
			PermutationVector.Set< FScreenSpaceShortRangeAOCS::FHairStrandsScreen>(bNeedTraceHairScreen);
			PermutationVector.Set< FScreenSpaceShortRangeAOCS::FHairStrandsVoxel>(bNeedTraceHairVoxel);
			auto ComputeShader = View.ShaderMap->GetShader<FScreenSpaceShortRangeAOCS>(PermutationVector);

			if (bOverflow)
			{
				PassParameters->TileIndirectBuffer = View.SubstrateViewData.ClosureTileDispatchIndirectBuffer;
				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("ShortRangeAO_ScreenSpace(Rays=%u, Overflow)", NumPixelRays),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					View.SubstrateViewData.ClosureTileDispatchIndirectBuffer,
					0);
			}
			else
			{
				FComputeShaderUtils::AddPass(
					GraphBuilder,
					RDG_EVENT_NAME("ShortRangeAO_ScreenSpace(Rays=%u)", NumPixelRays),
					ComputePassFlags,
					ComputeShader,
					PassParameters,
					FComputeShaderUtils::GetGroupCount(View.ViewRect.Size(), FScreenSpaceShortRangeAOCS::GetGroupSize()));
			}
		};

		ScreenSpaceShortRangeAO(false);
		if (Substrate::IsSubstrateEnabled())
		{
			ScreenSpaceShortRangeAO(true);
		}
	}

	OutParameters.ScreenBentNormal = ScreenBentNormal;
	OutParameters.UseShortRangeAO = 1;
	return OutParameters;
}