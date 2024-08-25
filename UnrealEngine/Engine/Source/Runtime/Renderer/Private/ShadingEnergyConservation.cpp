// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DeferredShadingRenderer.cpp: Top level rendering loop for deferred shading
=============================================================================*/

#include "ShadingEnergyConservation.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "RenderGraphUtils.h"
#include "SceneRendering.h"
#include "ScenePrivate.h"
#include "PixelShaderUtils.h"
#include "Substrate/Substrate.h"

static TAutoConsoleVariable<int32> CVarShadingEnergyConservation(
	TEXT("r.Shading.EnergyConservation"),
	1,
	TEXT("0 to disable energy conservation on shading models.\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarShadingEnergyConservation_Preservation(
	TEXT("r.Shading.EnergyPreservation"),
	1,
	TEXT("0 to disable energy preservation on shading models, i.e. the energy attenuation on diffuse lighting caused by the specular reflection. Require energy conservation to be enabled\n")
	TEXT(" 0: off\n")
	TEXT(" 1: on"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarShadingFurnaceTest(
	TEXT("r.Shading.FurnaceTest"),
	0,
	TEXT("Enable/disable furnace for shading validation."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarShadingFurnaceTest_SampleCount(
	TEXT("r.Shading.FurnaceTest.SampleCount"),
	64,
	TEXT("Number of sampler per pixel used for furnace tests."),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarShadingFurnaceTest_TableFormat(
	TEXT("r.Shading.EnergyConservation.Format"),
	1,
	TEXT("Energy conservation table format 0: 16bits, 1: 32bits."),
	ECVF_RenderThreadSafe);

// Transition render settings that will disapear when Substrate gets enabled

static TAutoConsoleVariable<int32> CVarMaterialEnergyConservation(
	TEXT("r.Material.EnergyConservation"),
	0,
	TEXT("Enable energy conservation for legacy materials (project settings, read only). Please note that when Substrate is enabled, energy conservation is forced to enabled."),
	ECVF_ReadOnly | ECVF_RenderThreadSafe);

#define SHADING_ENERGY_CONSERVATION_TABLE_RESOLUTION 32

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace ShadingEnergyConservationData
{
	#include "ShadingEnergyConservationData.h"

	template<typename TDataType>
	void LockCopyTexture2D(FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, const TDataType* InSrcBuffer, uint32 NumComponents)
	{
		const uint8* SrcBuffer = (const uint8*)InSrcBuffer;
		const uint32 SrcBytesPerPixel = sizeof(TDataType) * NumComponents;

		uint32 SrcStride = SrcBytesPerPixel * SHADING_ENERGY_CONSERVATION_TABLE_RESOLUTION;
		uint32 DstStride;
		uint8* DstBuffer = (uint8*)RHICmdList.LockTexture2D(Texture, 0, RLM_WriteOnly, DstStride, false);
		for (uint32 y = 0; y < SHADING_ENERGY_CONSERVATION_TABLE_RESOLUTION; ++y)
		{
			const uint8* Src = SrcBuffer + y * SrcStride;
			uint8* Dst = DstBuffer + y * DstStride;
			FMemory::Memcpy(Dst, Src, SrcStride);
		}
		RHICmdList.UnlockTexture2D(Texture, 0, false);
	}

	template<typename TDataType>
	void LockCopyTexture3D(FRHICommandListImmediate& RHICmdList, FRHITexture* Texture, const TDataType* InSrcBuffer, uint32 NumComponents)
	{
		const uint8* SrcBuffer = (const uint8*)InSrcBuffer;
		const uint32 SrcBytesPerPixel = sizeof(TDataType) * NumComponents;

		const FIntVector Extent(SHADING_ENERGY_CONSERVATION_TABLE_RESOLUTION, SHADING_ENERGY_CONSERVATION_TABLE_RESOLUTION, SHADING_ENERGY_CONSERVATION_TABLE_RESOLUTION);
		FUpdateTextureRegion3D Region(0, 0, 0, 0, 0, 0, Extent.X, Extent.Y, Extent.Z);
		RHICmdList.UpdateTexture3D(Texture, 0, Region, Extent.X * SrcBytesPerPixel, Extent.X * Extent.Y * SrcBytesPerPixel, SrcBuffer);

		// UpdateTexture3D before and after state is currently undefined
		RHICmdList.Transition(FRHITransitionInfo(Texture, ERHIAccess::Unknown, ERHIAccess::SRVMask));
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FShadingFurnaceTestPassPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FShadingFurnaceTestPassPS);
	SHADER_USE_PARAMETER_STRUCT(FShadingFurnaceTestPassPS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSubstrateGlobalUniformParameters, Substrate)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FSceneTextureUniformParameters, SceneTexturesStruct)
		SHADER_PARAMETER(uint32, NumSamplesPerSet)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()

		static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return GetMaxSupportedFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_FURNACE_ANALYTIC"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FShadingFurnaceTestPassPS, "/Engine/Private/ShadingFurnaceTest.usf", "MainPS", SF_Pixel);

static void AddShadingFurnacePass(
	FRDGBuilder& GraphBuilder, 
	const FViewInfo& View, 
	TRDGUniformBufferRef<FSceneTextureUniformParameters>& SceneTexturesUniformBuffer,
	FRDGTextureRef OutTexture)
{
	TShaderMapRef<FShadingFurnaceTestPassPS> PixelShader(View.ShaderMap);
	FShadingFurnaceTestPassPS::FParameters* Parameters = GraphBuilder.AllocParameters<FShadingFurnaceTestPassPS::FParameters>();
	Parameters->ViewUniformBuffer				= View.ViewUniformBuffer;
	Parameters->SceneTexturesStruct				= SceneTexturesUniformBuffer;
	Parameters->NumSamplesPerSet				= FMath::Clamp(CVarShadingFurnaceTest_SampleCount.GetValueOnAnyThread(), 16, 2048);
	Parameters->RenderTargets[0]				= FRenderTargetBinding(OutTexture, ERenderTargetLoadAction::ELoad);
	if (Substrate::IsSubstrateEnabled())
	{
		Parameters->Substrate = Substrate::BindSubstrateGlobalUniformParameters(View);
	}

	ShaderPrint::SetParameters(GraphBuilder, View.ShaderPrintData, Parameters->ShaderPrintUniformBuffer);

	FPixelShaderUtils::AddFullscreenPass<FShadingFurnaceTestPassPS>(
		GraphBuilder,
		View.ShaderMap,
		RDG_EVENT_NAME("ShadingEnergyConservation::FurnaceTest"),
		PixelShader,
		Parameters,
		View.ViewRect);
}

////////////////////////////////////////////////////////////////////////////////////////////////

class FBuildShadingEnergyConservationTableCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FBuildShadingEnergyConservationTableCS)
	SHADER_USE_PARAMETER_STRUCT(FBuildShadingEnergyConservationTableCS, FGlobalShader)

	enum class EEnergyTableType 
	{
		GGXSpecular = 0,
		GGXGlass = 1,
		Cloth = 2,
		Diffuse = 3,
		MAX
	};

	class FEnergyTableDim : SHADER_PERMUTATION_ENUM_CLASS("BUILD_ENERGY_TABLE", EEnergyTableType);

	using FPermutationDomain = TShaderPermutationDomain<FEnergyTableDim>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return GetMaxSupportedFeatureLevel(Parameters.Platform) >= ERHIFeatureLevel::SM5;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), FComputeShaderUtils::kGolden2DGroupSize);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), FComputeShaderUtils::kGolden2DGroupSize);
		OutEnvironment.SetDefine(TEXT("ENERGY_TABLE_RESOLUTION"), SHADING_ENERGY_CONSERVATION_TABLE_RESOLUTION);
	}

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumSamples)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, Output1Texture2D)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, Output2Texture2D)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture3D, OutputTexture3D)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_SHADER_TYPE(, FBuildShadingEnergyConservationTableCS, TEXT("/Engine/Private/ShadingEnergyConservationTable.usf"), TEXT("BuildEnergyTableCS"), SF_Compute);

////////////////////////////////////////////////////////////////////////////////////////////////

namespace ShadingEnergyConservation
{

bool IsEnable()
{
	return CVarShadingEnergyConservation.GetValueOnAnyThread() > 0;
}

void Init(FRDGBuilder& GraphBuilder, FViewInfo& View)
{
	FRDGTextureRef GGXSpecEnergyTexture = nullptr;
	FRDGTextureRef GGXGlassEnergyTexture = nullptr;
	FRDGTextureRef ClothEnergyTexture = nullptr;
	FRDGTextureRef DiffuseEnergyTexture = nullptr;

	// Enabled based on settings
	const bool bMaterialEnergyConservationEnabled = CVarMaterialEnergyConservation.GetValueOnRenderThread() > 0;
	const bool bIsEnergyConservationEnabled = CVarShadingEnergyConservation.GetValueOnRenderThread() > 0;
	const bool bIsEnergyPreservationEnabled = CVarShadingEnergyConservation_Preservation.GetValueOnRenderThread() > 0;	

	// Build/bind table if energy conservation is enabled or if Substrate is enabled in order to have 
	// the correct tables built & bound. Even if we are not using energy conservation, we want to 
	// have access to directional albedo information for env. lighting for instance)
	const bool bBindEnergyData = (View.ViewState != nullptr) && (bMaterialEnergyConservationEnabled || Substrate::IsSubstrateEnabled() || (View.Family->EngineShowFlags.PathTracing && RHI_RAYTRACING)) && (bIsEnergyPreservationEnabled || bIsEnergyConservationEnabled);
	if (bBindEnergyData)
	{
		// Change this to true in order to regenerate the energy tables, and manually copy the coefficients into ShadingEnergyConservationData.h
		const bool bRuntimeGeneration = false;

		const int Size = SHADING_ENERGY_CONSERVATION_TABLE_RESOLUTION;
		const EPixelFormat SpecFormat = bRuntimeGeneration && CVarShadingFurnaceTest_TableFormat.GetValueOnRenderThread() > 0 ? PF_G32R32F : PF_G16R16;
		const EPixelFormat DiffFormat = PF_G16;
		const bool bBuildTable = 
			View.ViewState->ShadingEnergyConservationData.Format != SpecFormat ||
			View.ViewState->ShadingEnergyConservationData.GGXSpecEnergyTexture == nullptr ||
			View.ViewState->ShadingEnergyConservationData.GGXGlassEnergyTexture == nullptr ||
			View.ViewState->ShadingEnergyConservationData.ClothEnergyTexture ==  nullptr ||
			View.ViewState->ShadingEnergyConservationData.DiffuseEnergyTexture == nullptr;

		if (bBuildTable)
		{
			View.ViewState->ShadingEnergyConservationData.Format = SpecFormat;

			if (bRuntimeGeneration)
			{			
				GGXSpecEnergyTexture	= GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(FIntPoint(Size, Size),        SpecFormat, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV), TEXT("Shading.GGXSpecEnergy"),   ERDGTextureFlags::MultiFrame);
				GGXGlassEnergyTexture	= GraphBuilder.CreateTexture(FRDGTextureDesc::Create3D(FIntVector(Size, Size, Size), SpecFormat, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV), TEXT("Shading.GGXGlassEnergy"),  ERDGTextureFlags::MultiFrame);
				ClothEnergyTexture		= GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(FIntPoint(Size, Size),        SpecFormat, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV), TEXT("Shading.ClothSpecEnergy"), ERDGTextureFlags::MultiFrame);
				DiffuseEnergyTexture	= GraphBuilder.CreateTexture(FRDGTextureDesc::Create2D(FIntPoint(Size, Size),        DiffFormat, FClearValueBinding::None, TexCreate_ShaderResource | TexCreate_UAV), TEXT("Shading.DiffuseEnergy"),   ERDGTextureFlags::MultiFrame);
			
				const uint32 NumSamples = 1u << 14u;

				// GGX
				{
					FBuildShadingEnergyConservationTableCS::FPermutationDomain PermutationVector;
					PermutationVector.Set<FBuildShadingEnergyConservationTableCS::FEnergyTableDim>(FBuildShadingEnergyConservationTableCS::EEnergyTableType::GGXSpecular);
					TShaderMapRef<FBuildShadingEnergyConservationTableCS> ComputeShader(View.ShaderMap, PermutationVector);
					FBuildShadingEnergyConservationTableCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FBuildShadingEnergyConservationTableCS::FParameters>();
					PassParameters->NumSamples = NumSamples;
					PassParameters->Output2Texture2D = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(GGXSpecEnergyTexture, 0));
					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("ShadingEnergyConservation::BuildTable(GGXSpec)"),
						ComputeShader,
						PassParameters,
						FComputeShaderUtils::GetGroupCount(FIntPoint(Size, Size), FComputeShaderUtils::kGolden2DGroupSize));
				}

				// GGX (Reflection + Transmission) indexed by IOR
				{
					FBuildShadingEnergyConservationTableCS::FPermutationDomain PermutationVector;
					PermutationVector.Set<FBuildShadingEnergyConservationTableCS::FEnergyTableDim>(FBuildShadingEnergyConservationTableCS::EEnergyTableType::GGXGlass);
					TShaderMapRef<FBuildShadingEnergyConservationTableCS> ComputeShader(View.ShaderMap, PermutationVector);
					FBuildShadingEnergyConservationTableCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FBuildShadingEnergyConservationTableCS::FParameters>();
					PassParameters->NumSamples = NumSamples;
					PassParameters->OutputTexture3D = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(GGXGlassEnergyTexture, 0));
					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("ShadingEnergyConservation::BuildTable(GGXGlass)"),
						ComputeShader,
						PassParameters,
						FComputeShaderUtils::GetGroupCount(FIntVector(Size, Size, Size), FIntVector(FComputeShaderUtils::kGolden2DGroupSize, FComputeShaderUtils::kGolden2DGroupSize, 1)));
				}

				// Cloth
				{
					FBuildShadingEnergyConservationTableCS::FPermutationDomain PermutationVector;
					PermutationVector.Set<FBuildShadingEnergyConservationTableCS::FEnergyTableDim>(FBuildShadingEnergyConservationTableCS::EEnergyTableType::Cloth);
					TShaderMapRef<FBuildShadingEnergyConservationTableCS> ComputeShader(View.ShaderMap, PermutationVector);
					FBuildShadingEnergyConservationTableCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FBuildShadingEnergyConservationTableCS::FParameters>();
					PassParameters->NumSamples = NumSamples;
					PassParameters->Output2Texture2D = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(ClothEnergyTexture, 0));
					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("ShadingEnergyConservation::BuildTable(Cloth)"),
						ComputeShader,
						PassParameters,
						FComputeShaderUtils::GetGroupCount(FIntPoint(Size, Size), FComputeShaderUtils::kGolden2DGroupSize));
				}

				// Diffuse
				{
					FBuildShadingEnergyConservationTableCS::FPermutationDomain PermutationVector;
					PermutationVector.Set<FBuildShadingEnergyConservationTableCS::FEnergyTableDim>(FBuildShadingEnergyConservationTableCS::EEnergyTableType::Diffuse);
					TShaderMapRef<FBuildShadingEnergyConservationTableCS> ComputeShader(View.ShaderMap, PermutationVector);
					FBuildShadingEnergyConservationTableCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FBuildShadingEnergyConservationTableCS::FParameters>();
					PassParameters->NumSamples = NumSamples;
					PassParameters->Output1Texture2D = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(DiffuseEnergyTexture, 0));
					FComputeShaderUtils::AddPass(
						GraphBuilder,
						RDG_EVENT_NAME("ShadingEnergyConservation::BuildTable(Diffuse)"),
						ComputeShader,
						PassParameters,
						FComputeShaderUtils::GetGroupCount(FIntPoint(Size, Size), FComputeShaderUtils::kGolden2DGroupSize));
				}

				GraphBuilder.QueueTextureExtraction(GGXSpecEnergyTexture,	&View.ViewState->ShadingEnergyConservationData.GGXSpecEnergyTexture);
				GraphBuilder.QueueTextureExtraction(GGXGlassEnergyTexture,	&View.ViewState->ShadingEnergyConservationData.GGXGlassEnergyTexture);
				GraphBuilder.QueueTextureExtraction(ClothEnergyTexture,		&View.ViewState->ShadingEnergyConservationData.ClothEnergyTexture);
				GraphBuilder.QueueTextureExtraction(DiffuseEnergyTexture,	&View.ViewState->ShadingEnergyConservationData.DiffuseEnergyTexture);
			}
			else
			{
				// Precomputed data are stored as float16
				check(SpecFormat == PF_G16R16);

				View.ViewState->ShadingEnergyConservationData.GGXSpecEnergyTexture  = GRenderTargetPool.FindFreeElement(FRDGTextureDesc::Create2D(FIntPoint(Size, Size),        SpecFormat, FClearValueBinding::None, TexCreate_ShaderResource), TEXT("Shading.GGXSpecEnergy"));
				View.ViewState->ShadingEnergyConservationData.GGXGlassEnergyTexture = GRenderTargetPool.FindFreeElement(FRDGTextureDesc::Create3D(FIntVector(Size, Size, Size), SpecFormat, FClearValueBinding::None, TexCreate_ShaderResource), TEXT("Shading.GGXGlassEnergy"));
				View.ViewState->ShadingEnergyConservationData.ClothEnergyTexture    = GRenderTargetPool.FindFreeElement(FRDGTextureDesc::Create2D(FIntPoint(Size, Size),        SpecFormat, FClearValueBinding::None, TexCreate_ShaderResource), TEXT("Shading.ClothSpecEnergy"));
				View.ViewState->ShadingEnergyConservationData.DiffuseEnergyTexture  = GRenderTargetPool.FindFreeElement(FRDGTextureDesc::Create2D(FIntPoint(Size, Size),        DiffFormat,	FClearValueBinding::None, TexCreate_ShaderResource), TEXT("Shading.DiffuseEnergy"));

				ShadingEnergyConservationData::LockCopyTexture2D(GraphBuilder.RHICmdList, View.ViewState->ShadingEnergyConservationData.GGXSpecEnergyTexture->GetRHI(),  ShadingEnergyConservationData::GGXSpecValues, 2);
				ShadingEnergyConservationData::LockCopyTexture3D(GraphBuilder.RHICmdList, View.ViewState->ShadingEnergyConservationData.GGXGlassEnergyTexture->GetRHI(), ShadingEnergyConservationData::GGXGlassValues, 2);
				ShadingEnergyConservationData::LockCopyTexture2D(GraphBuilder.RHICmdList, View.ViewState->ShadingEnergyConservationData.ClothEnergyTexture->GetRHI(),    Substrate::IsSubstrateEnabled() ? ShadingEnergyConservationData::SubstrateClothSpecValues : ShadingEnergyConservationData::ClothSpecValues, 2);
				ShadingEnergyConservationData::LockCopyTexture2D(GraphBuilder.RHICmdList, View.ViewState->ShadingEnergyConservationData.DiffuseEnergyTexture->GetRHI(),  ShadingEnergyConservationData::DiffuseValues, 1);

				GGXSpecEnergyTexture  = GraphBuilder.RegisterExternalTexture(View.ViewState->ShadingEnergyConservationData.GGXSpecEnergyTexture);
				GGXGlassEnergyTexture = GraphBuilder.RegisterExternalTexture(View.ViewState->ShadingEnergyConservationData.GGXGlassEnergyTexture);
				ClothEnergyTexture    = GraphBuilder.RegisterExternalTexture(View.ViewState->ShadingEnergyConservationData.ClothEnergyTexture);
				DiffuseEnergyTexture  = GraphBuilder.RegisterExternalTexture(View.ViewState->ShadingEnergyConservationData.DiffuseEnergyTexture);
			}
		}
		else
		{
			GGXSpecEnergyTexture	= GraphBuilder.RegisterExternalTexture(View.ViewState->ShadingEnergyConservationData.GGXSpecEnergyTexture);
			GGXGlassEnergyTexture	= GraphBuilder.RegisterExternalTexture(View.ViewState->ShadingEnergyConservationData.GGXGlassEnergyTexture);
			ClothEnergyTexture		= GraphBuilder.RegisterExternalTexture(View.ViewState->ShadingEnergyConservationData.ClothEnergyTexture);
			DiffuseEnergyTexture	= GraphBuilder.RegisterExternalTexture(View.ViewState->ShadingEnergyConservationData.DiffuseEnergyTexture);
		}

		View.ViewState->ShadingEnergyConservationData.bEnergyConservation = bIsEnergyConservationEnabled;
		View.ViewState->ShadingEnergyConservationData.bEnergyPreservation = bIsEnergyPreservationEnabled;
	}
	else if (View.ViewState)
	{
		View.ViewState->ShadingEnergyConservationData = FShadingEnergyConservationStateData();
	}
}


void Debug(FRDGBuilder& GraphBuilder, const FViewInfo& View, FSceneTextures& SceneTextures)
{
	if (CVarShadingFurnaceTest.GetValueOnAnyThread() > 0)
	{
		RDG_EVENT_SCOPE(GraphBuilder, "ShadingEnergyConservation::FurnaceTest");
		AddShadingFurnacePass(GraphBuilder, View, SceneTextures.UniformBuffer, SceneTextures.Color.Target);
	}
}

}
