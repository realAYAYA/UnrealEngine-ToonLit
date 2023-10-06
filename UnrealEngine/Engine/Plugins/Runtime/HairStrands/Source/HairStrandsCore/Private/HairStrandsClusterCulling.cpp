// Copyright Epic Games, Inc. All Rights Reserved.

#include "HairStrandsClusterCulling.h"
#include "RendererInterface.h"
#include "Shader.h"
#include "RenderGraph.h"
#include "GlobalShader.h"
#include "ShaderParameters.h"
#include "ShaderParameterStruct.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "GroomVisualizationData.h"
#include "GroomInstance.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

class FHairIndBufferClearCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHairIndBufferClearCS);
	SHADER_USE_PARAMETER_STRUCT(FHairIndBufferClearCS, FGlobalShader);

	class FSetIndirectDraw : SHADER_PERMUTATION_BOOL("PERMUTATION_SETINDIRECTDRAW");
	using FPermutationDomain = TShaderPermutationDomain<FSetIndirectDraw>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, DispatchIndirectParametersClusterCount)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, DrawIndirectParameters)
		SHADER_PARAMETER(uint32, VertexCountPerInstance)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Strands, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_CLUSTERCULLINGINDCLEAR"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairIndBufferClearCS, "/Engine/Private/HairStrands/HairStrandsClusterCulling.usf", "MainClusterCullingIndClearCS", SF_Compute);

///////////////////////////////////////////////////////////////////////////////////////////////////

class FHairClusterCullCS: public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHairClusterCullCS);
	SHADER_USE_PARAMETER_STRUCT(FHairClusterCullCS, FGlobalShader);

	class FPointPerCurve : SHADER_PERMUTATION_SPARSE_INT("PERMUTATION_POINT_PER_CURVE", 4, 8, 16, 32, 64);
	using FPermutationDomain = TShaderPermutationDomain<FPointPerCurve>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER(float, LODIndex)
		SHADER_PARAMETER(float, LODBias)
		SHADER_PARAMETER(uint32, CurveCount)
		SHADER_PARAMETER(uint32, DispatchCountX)
		SHADER_PARAMETER(FVector4f, ClusterInfoParameters)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, CurveBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, CurveToClusterIdBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, PointLODBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FPackedHairClusterInfo>, ClusterInfoBuffer)
	
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutPointCounter)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutIndexBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, OutRadiusScaleBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, OutCulledCurveBuffer)

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
		END_SHADER_PARAMETER_STRUCT()

public:
		static uint32 GetGroupSize() { return 64u; }
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Strands, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_CLUSTER_CULL"), 1);
		OutEnvironment.SetDefine(TEXT("GROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairClusterCullCS, "/Engine/Private/HairStrands/HairStrandsClusterCulling.usf", "Main", SF_Compute);

///////////////////////////////////////////////////////////////////////////////////////////////////

class FHairClusterCullArgsCS: public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHairClusterCullArgsCS);
	SHADER_USE_PARAMETER_STRUCT(FHairClusterCullArgsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, PointCounterBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectDrawArgsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWIndirectDispatchArgsBuffer)
		END_SHADER_PARAMETER_STRUCT()

public:
		static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Strands, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_CLUSTER_CULL_ARGS"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairClusterCullArgsCS, "/Engine/Private/HairStrands/HairStrandsClusterCulling.usf", "Main", SF_Compute);

///////////////////////////////////////////////////////////////////////////////////////////////////
static void AddClusterCullingPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const FSceneView* View,
	const FShaderPrintData* ShaderPrintData,
	FHairStrandClusterData::FHairGroup& ClusterData)
{
	check(View);

	ShaderPrint::SetEnabled(true);
	ShaderPrint::RequestSpaceForCharacters(2048);
	ShaderPrint::RequestSpaceForLines(2048);

	// 0. Glogal counter for visible points
	FRDGBufferRef PointCounter 	= GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("Hair.ClusterPointCounter"));
	FRDGBufferUAVRef PointCounterUAV = GraphBuilder.CreateUAV(PointCounter, PF_R32_UINT);
	AddClearUAVPass(GraphBuilder, PointCounterUAV, 0);

	// 1. Build culled index buffer
	{
		FHairClusterCullCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairClusterCullCS::FParameters>();
		Parameters->LODIndex 			 = ClusterData.LODIndex;
		Parameters->LODBias 			 = ClusterData.LODBias;
		Parameters->CurveCount 			 = ClusterData.HairGroupPublicPtr->GetActiveStrandsCurveCount();	
		Parameters->CurveBuffer 		 = RegisterAsSRV(GraphBuilder, *ClusterData.CurveBuffer);
		Parameters->CurveToClusterIdBuffer=RegisterAsSRV(GraphBuilder, *ClusterData.CurveToClusterIdBuffer);
		Parameters->PointLODBuffer 		 = RegisterAsSRV(GraphBuilder, *ClusterData.PointLODBuffer);
		Parameters->ClusterInfoBuffer 	 = RegisterAsSRV(GraphBuilder, *ClusterData.ClusterInfoBuffer);
		Parameters->ClusterInfoParameters= ClusterData.ClusterInfoParameters;
		Parameters->ViewUniformBuffer	 = View->ViewUniformBuffer;
		Parameters->OutPointCounter 	 = PointCounterUAV;
		Parameters->OutIndexBuffer 		 = RegisterAsUAV(GraphBuilder, *ClusterData.GetCulledVertexIdBuffer());
		Parameters->OutRadiusScaleBuffer = RegisterAsUAV(GraphBuilder, *ClusterData.GetCulledVertexRadiusScaleBuffer());
		Parameters->OutCulledCurveBuffer = RegisterAsUAV(GraphBuilder, *ClusterData.GetCulledCurveBuffer()); // TODO: this could be changed to be transient buffer

		if (ShaderPrintData)
		{
			ShaderPrint::SetParameters(GraphBuilder, *ShaderPrintData, Parameters->ShaderPrintUniformBuffer);
		}

		// Pick permutation based on groom max number of point per curve
		//const uint32 PointPerCurve = FMath::Clamp(uint32(FMath::Pow( FMath::RoundFromZero(FMath::Log2(float(ClusterData.MaxPointPerCurve))),2u)), 4u, 32u);
		const uint32 PointPerCurve = FMath::Clamp(uint32(FMath::Pow(2u, FMath::RoundFromZero(FMath::Log2(float(ClusterData.MaxPointPerCurve))))), 4u, 64u);
		check(FMath::IsPowerOfTwo(PointPerCurve));

		const uint32 CurvePerGroup = FHairClusterCullCS::GetGroupSize() / PointPerCurve;
		const uint32 LinearGroupCount = FMath::DivideAndRoundUp(Parameters->CurveCount, CurvePerGroup);

		FIntVector DispatchCount(LinearGroupCount, 1, 1);
		if (DispatchCount.X > 0xFFFFu)
		{
			DispatchCount.X = 64;
			DispatchCount.Y = FMath::DivideAndRoundUp(LinearGroupCount, uint32(DispatchCount.X));
		}
		Parameters->DispatchCountX = DispatchCount.X;
		check(DispatchCount.X <= 0xFFFFu);
		check(DispatchCount.Y <= 0xFFFFu);

		FHairClusterCullCS::FPermutationDomain Permutation;
		Permutation.Set<FHairClusterCullCS::FPointPerCurve>(PointPerCurve);
		TShaderMapRef<FHairClusterCullCS> ComputeShader(ShaderMap, Permutation);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("HairStrands::ClusterCullPass2"),
			ComputeShader,
			Parameters,
			DispatchCount);
	}

	// 2. Prepare indirect draw/dispatch args buffers
	{
		FRDGImportedBuffer DrawIndirectParametersBuffer = Register(GraphBuilder, ClusterData.HairGroupPublicPtr->GetDrawIndirectBuffer(), ERDGImportedBufferFlags::CreateViews);
		FRDGImportedBuffer DrawIndirectParametersRasterComputeBuffer = Register(GraphBuilder, ClusterData.HairGroupPublicPtr->GetDrawIndirectRasterComputeBuffer(), ERDGImportedBufferFlags::CreateViews);

		FHairClusterCullArgsCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairClusterCullArgsCS::FParameters>();
		Parameters->PointCounterBuffer = GraphBuilder.CreateSRV(PointCounter, PF_R32_UINT);
		Parameters->RWIndirectDrawArgsBuffer = DrawIndirectParametersBuffer.UAV;
		Parameters->RWIndirectDispatchArgsBuffer = DrawIndirectParametersRasterComputeBuffer.UAV;

		TShaderMapRef<FHairClusterCullArgsCS> ComputeShader(ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("HairStrands::ClusterCullPrepareArgs"),
			ComputeShader,
			Parameters,
			FIntVector(1, 1, 1));

		GraphBuilder.SetBufferAccessFinal(DrawIndirectParametersBuffer.Buffer, ERHIAccess::IndirectArgs | ERHIAccess::SRVMask);
		GraphBuilder.SetBufferAccessFinal(DrawIndirectParametersRasterComputeBuffer.Buffer, ERHIAccess::IndirectArgs | ERHIAccess::SRVMask);
	}

	ClusterData.SetCullingResultAvailable(true);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

static void AddClusterResetLod0(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	FHairStrandClusterData::FHairGroup& ClusterData)
{
	// Set as culling result not available
	ClusterData.SetCullingResultAvailable(false);

	FRDGImportedBuffer IndirectBuffer = Register(GraphBuilder, ClusterData.HairGroupPublicPtr->GetDrawIndirectBuffer(), ERDGImportedBufferFlags::CreateViews);

	// Initialise indirect buffers to entire lod 0 dispatch
	FHairIndBufferClearCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairIndBufferClearCS::FParameters>();
	Parameters->DrawIndirectParameters = IndirectBuffer.UAV;
	Parameters->VertexCountPerInstance = ClusterData.HairGroupPublicPtr->GetActiveStrandsPointCount() * HAIR_POINT_TO_VERTEX;

	FHairIndBufferClearCS::FPermutationDomain Permutation;
	Permutation.Set<FHairIndBufferClearCS::FSetIndirectDraw>(true);
	TShaderMapRef<FHairIndBufferClearCS> ComputeShader(ShaderMap, Permutation);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("BufferClearCS"),
		ComputeShader,
		Parameters,
		FIntVector(1, 1, 1));

	GraphBuilder.SetBufferAccessFinal(IndirectBuffer.Buffer, ERHIAccess::IndirectArgs);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void ComputeHairStrandsClustersCulling(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap& ShaderMap,
	const TArray<const FSceneView*>& Views, 
	const FShaderPrintData* ShaderPrintData,
	FHairStrandClusterData& ClusterDatas)
{
	DECLARE_GPU_STAT(HairStrandsClusterCulling);
	RDG_EVENT_SCOPE(GraphBuilder, "HairStrandsClusterCulling");
	TRACE_CPUPROFILER_EVENT_SCOPE(ComputeHairStrandsClustersCulling);
	RDG_GPU_STAT_SCOPE(GraphBuilder, HairStrandsClusterCulling);

	for (const FSceneView* View : Views)
	{
		// TODO use compute overlap (will need to split AddClusterCullingPass)
		for (FHairStrandClusterData::FHairGroup& ClusterData : ClusterDatas.HairGroups)
		{
			AddClusterResetLod0(GraphBuilder, &ShaderMap, ClusterData);
		}

		for (FHairStrandClusterData::FHairGroup& ClusterData : ClusterDatas.HairGroups)		
		{
			AddClusterCullingPass(
				GraphBuilder,
				&ShaderMap,
				View,
				ShaderPrintData,
				ClusterData);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void AddInstanceToClusterData(FHairGroupInstance* In, FHairStrandClusterData& Out)
{
	// Initialize group cluster data for culling by the renderer
	const int32 ClusterDataGroupIndex = Out.HairGroups.Num();
	FHairStrandClusterData::FHairGroup& HairGroupCluster = Out.HairGroups.Emplace_GetRef();
	HairGroupCluster.MaxPointPerCurve = In->Strands.Data ? In->Strands.Data->Header.MaxPointPerCurve : 0;
	HairGroupCluster.ClusterScale = In->HairGroupPublicData->GetClusterScale();
	HairGroupCluster.ClusterCount = In->HairGroupPublicData->GetClusterCount();
	HairGroupCluster.GroupAABBBuffer = &In->HairGroupPublicData->GetGroupAABBBuffer();
	HairGroupCluster.ClusterAABBBuffer = &In->HairGroupPublicData->GetClusterAABBBuffer();

	HairGroupCluster.CurveBuffer = &In->Strands.RestResource->CurveBuffer;
	HairGroupCluster.PointLODBuffer = &In->Strands.ClusterResource->PointLODBuffer;

	HairGroupCluster.ClusterInfoParameters = In->Strands.ClusterResource->BulkData.Header.ClusterInfoParameters;
	HairGroupCluster.ClusterInfoBuffer = &In->Strands.ClusterResource->ClusterInfoBuffer;
	HairGroupCluster.CurveToClusterIdBuffer = &In->Strands.ClusterResource->CurveToClusterIdBuffer;

	HairGroupCluster.HairGroupPublicPtr = In->HairGroupPublicData;
	HairGroupCluster.LODBias  = In->HairGroupPublicData->GetLODBias();
	HairGroupCluster.LODIndex = In->HairGroupPublicData->GetLODIndex();
	HairGroupCluster.bVisible = In->HairGroupPublicData->GetLODVisibility();

	// These buffer are create during the culling pass
	// HairGroupCluster.ClusterIdBuffer = nullptr;
	// HairGroupCluster.ClusterIndexOffsetBuffer = nullptr;
	// HairGroupCluster.ClusterIndexCountBuffer = nullptr;

	HairGroupCluster.HairGroupPublicPtr->ClusterDataIndex = ClusterDataGroupIndex;
}