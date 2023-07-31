// Copyright Epic Games, Inc. All Rights Reserved.

#include "HairStrandsClusters.h"
#include "HairStrandsUtils.h"
#include "SceneRendering.h"
#include "SceneManagement.h"
#include "RendererInterface.h"
#include "Shader.h"
#include "GlobalShader.h"
#include "ShaderParameters.h"
#include "ShaderParameterStruct.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

static int32 GHairStrandsClusterForceLOD = -1;
static FAutoConsoleVariableRef CVarHairClusterCullingLodMode(TEXT("r.HairStrands.Cluster.ForceLOD"), GHairStrandsClusterForceLOD, TEXT("Force a specific hair LOD."));

static int32 GHairStrandsClusterCullingFreezeCamera = 0;
static FAutoConsoleVariableRef CVarHairStrandsClusterCullingFreezeCamera(TEXT("r.HairStrands.Cluster.CullingFreezeCamera"), GHairStrandsClusterCullingFreezeCamera, TEXT("Freeze camera when enabled. It will disable HZB culling because hzb buffer is not frozen."));

bool IsHairStrandsClusterCullingEnable()
{
	// At the moment it is not possible to disable cluster culling, as this pass is in charge of LOD selection, 
	// and preparing the buffer which will be need for the cluster AABB pass (used later on by the voxelization pass)
	return true;
}

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

class FHairClusterCullingCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHairClusterCullingCS);
	SHADER_USE_PARAMETER_STRUCT(FHairClusterCullingCS, FGlobalShader);

	class FDebugAABBBuffer : SHADER_PERMUTATION_INT("PERMUTATION_DEBUGAABBBUFFER", 2);
	using FPermutationDomain = TShaderPermutationDomain<FDebugAABBBuffer>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector3f, CameraWorldPos)
		SHADER_PARAMETER(FMatrix44f, WorldToClipMatrix)
		SHADER_PARAMETER(FMatrix44f, ProjectionMatrix)
		SHADER_PARAMETER(uint32, ClusterCount)
		SHADER_PARAMETER(float, LODForcedIndex)
		SHADER_PARAMETER(int32, bIsHairGroupVisible)
		SHADER_PARAMETER(uint32, NumConvexHullPlanes)
		SHADER_PARAMETER(float, LODBias)
		SHADER_PARAMETER_ARRAY(FVector4f, ViewFrustumConvexHull, [6])
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, ClusterAABBBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, ClusterInfoBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, ClusterLODInfoBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, GlobalClusterIdBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, GlobalIndexStartBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, GlobalIndexCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, GlobalRadiusScaleBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, ClusterDebugInfoBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, DispatchIndirectParametersClusterCount)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, DrawIndirectParameters)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Strands, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_CLUSTERCULLING"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairClusterCullingCS, "/Engine/Private/HairStrands/HairStrandsClusterCulling.usf", "MainClusterCullingCS", SF_Compute);

///////////////////////////////////////////////////////////////////////////////////////////////////

class FMainClusterCullingPrepareIndirectDrawsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMainClusterCullingPrepareIndirectDrawsCS);
	SHADER_USE_PARAMETER_STRUCT(FMainClusterCullingPrepareIndirectDrawsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, GroupSize1D)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer,   DispatchIndirectParametersClusterCount)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, DispatchIndirectParametersClusterCount1D)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, DispatchIndirectParametersClusterCount2D)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, DispatchIndirectParametersClusterCountDiv512)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, DispatchIndirectParametersClusterCountDiv512Div512)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Strands, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_PREPAREINDIRECTDRAW"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMainClusterCullingPrepareIndirectDrawsCS, "/Engine/Private/HairStrands/HairStrandsClusterCulling.usf", "MainClusterCullingPrepareIndirectDrawsCS", SF_Compute);

///////////////////////////////////////////////////////////////////////////////////////////////////

class FMainClusterCullingPrepareIndirectDispatchCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMainClusterCullingPrepareIndirectDispatchCS);
	SHADER_USE_PARAMETER_STRUCT(FMainClusterCullingPrepareIndirectDispatchCS, FGlobalShader);

	class FGroupSize : SHADER_PERMUTATION_SPARSE_INT("PERMUTATION_GROUP_SIZE", 32, 64);
	using FPermutationDomain = TShaderPermutationDomain<FGroupSize>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, DrawIndirectBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, DispatchIndirectBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Strands, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_PREPAREINDIRECTDISPATCH"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMainClusterCullingPrepareIndirectDispatchCS, "/Engine/Private/HairStrands/HairStrandsClusterCulling.usf", "MainClusterCullingPrepareIndirectDispatchCS", SF_Compute);

///////////////////////////////////////////////////////////////////////////////////////////////////

class FHairClusterCullingLocalBlockPreFixSumCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHairClusterCullingLocalBlockPreFixSumCS);
	SHADER_USE_PARAMETER_STRUCT(FHairClusterCullingLocalBlockPreFixSumCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(DispatchIndirectParametersClusterCountDiv512, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, DispatchIndirectParametersClusterCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, GlobalIndexCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, PerBlocklIndexCountPreFixSumBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, PerBlocklTotalIndexCountBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Strands, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_PERBLOCKPREFIXSUM"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairClusterCullingLocalBlockPreFixSumCS, "/Engine/Private/HairStrands/HairStrandsClusterCulling.usf", "MainPerBlockPreFixSumCS", SF_Compute);

///////////////////////////////////////////////////////////////////////////////////////////////////

class FHairClusterCullingCompactVertexIdsLocalBlockCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHairClusterCullingCompactVertexIdsLocalBlockCS);
	SHADER_USE_PARAMETER_STRUCT(FHairClusterCullingCompactVertexIdsLocalBlockCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(DispatchIndirectParametersBuffer, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, DispatchIndirectParametersClusterCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, DispatchIndirectParametersClusterCount2D)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, DispatchIndirectParametersClusterCountDiv512)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, PerBlocklIndexCountPreFixSumBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, PerBlocklTotalIndexCountPreFixSumBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, GlobalIndexStartBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, GlobalIndexCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, GlobalRadiusScaleBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, ClusterVertexIdBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, CulledCompactedIndexBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, CulledCompactedRadiusScaleBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Strands, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_CLUSTERCULLINGCOMPACTVERTEXIDLOCALBLOCK"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairClusterCullingCompactVertexIdsLocalBlockCS, "/Engine/Private/HairStrands/HairStrandsClusterCulling.usf", "MainClusterCullingCompactVertexIdsLocalBlockCS", SF_Compute);

///////////////////////////////////////////////////////////////////////////////////////////////////

class FHairClusterCullingPreFixSumCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHairClusterCullingPreFixSumCS);
	SHADER_USE_PARAMETER_STRUCT(FHairClusterCullingPreFixSumCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		RDG_BUFFER_ACCESS(DispatchIndirectParameters, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, DispatchIndirectParametersClusterCount)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, GlobalIndexCountBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, GlobalIndexCountPreFixSumBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Strands, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_CLUSTERCULLINGPREFIXSUM"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairClusterCullingPreFixSumCS, "/Engine/Private/HairStrands/HairStrandsClusterCulling.usf", "MainClusterCullingPreFixSumCS", SF_Compute);

///////////////////////////////////////////////////////////////////////////////////////////////////

static FVector CapturedCameraWorldPos;
static FMatrix CapturedWorldToClipMatrix;
static FMatrix CapturedProjMatrix;

bool IsHairStrandsClusterDebugEnable();
bool IsHairStrandsClusterDebugAABBEnable();

static void AddClusterCullingPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const FViewInfo& View,
	const FHairCullingParams& CullingParameters,
	FHairStrandClusterData::FHairGroup& ClusterData)
{
	FRDGBufferRef DispatchIndirectParametersClusterCount = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(), TEXT("Hair.DispatchIndirectParametersClusterCount"));
	FRDGBufferRef DispatchIndirectParametersClusterCount1D = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(), TEXT("Hair.DispatchIndirectParametersClusterCount1D"));
	FRDGBufferRef DispatchIndirectParametersClusterCount2D = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(), TEXT("Hair.DispatchIndirectParametersClusterCount2D"));
	FRDGBufferRef DispatchIndirectParametersClusterCountDiv512 = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(), TEXT("Hair.DispatchIndirectParametersClusterCountDiv512"));
	FRDGBufferRef DispatchIndirectParametersClusterCountDiv512Div512 = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(), TEXT("Hair.DispatchIndirectParametersClusterCountDiv512Div512"));
	
	FRDGBufferRef GlobalClusterIdBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), ClusterData.ClusterCount), TEXT("Hair.GlobalClusterIdBuffer"));
	FRDGBufferRef GlobalIndexStartBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), ClusterData.ClusterCount), TEXT("Hair.GlobalIndexStartBuffer"));
	FRDGBufferRef GlobalIndexCountBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), ClusterData.ClusterCount), TEXT("Hair.GlobalIndexCountBuffer"));
	FRDGBufferRef GlobalRadiusScaleBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(float), ClusterData.ClusterCount), TEXT("Hair.GlobalRadiusScaleBuffer"));

	const uint32 PrefixGroupSize = 512;
	const uint32 ClusterCountRoundUpToGroupSize = FMath::DivideAndRoundUp(ClusterData.ClusterCount, PrefixGroupSize) * PrefixGroupSize;
	FRDGBufferRef PerBlocklTotalIndexCountBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), ClusterCountRoundUpToGroupSize), TEXT("Hair.PerBlocklTotalIndexCountBuffer"));
	FRDGBufferRef PerBlocklTotalIndexCountPreFixSumBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32) * 2, ClusterCountRoundUpToGroupSize), TEXT("Hair.PerBlocklTotalIndexCountPreFixSumBuffer"));
	FRDGBufferRef PerBlocklIndexCountPreFixSumBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32) * 2, ClusterCountRoundUpToGroupSize), TEXT("Hair.PerBlocklIndexCountPreFixSumBuffer"));

	FRDGImportedBuffer DrawIndirectParametersBuffer = Register(GraphBuilder, ClusterData.HairGroupPublicPtr->GetDrawIndirectBuffer(), ERDGImportedBufferFlags::CreateViews);
	FRDGImportedBuffer DrawIndirectParametersRasterComputeBuffer = Register(GraphBuilder, ClusterData.HairGroupPublicPtr->GetDrawIndirectRasterComputeBuffer(), ERDGImportedBufferFlags::CreateViews);
	
	bool bClusterDebugAABBBuffer = false;
	bool bClusterDebug = false;
#if WITH_EDITOR
	FRDGBufferRef ClusterDebugInfoBuffer = nullptr;
	{
		// Defined in HairStrandsClusterCommon.ush
		struct FHairClusterDebugInfo
		{
			uint32 GroupIndex;
			float LOD;
			float VertexCount;
			float CurveCount;
		};

		static IConsoleVariable* CVarClusterDebug = IConsoleManager::Get().FindConsoleVariable(TEXT("r.HairStrands.Cluster.Debug"));
		bClusterDebugAABBBuffer = IsHairStrandsClusterDebugAABBEnable();
		bClusterDebug = IsHairStrandsClusterDebugEnable();
		ClusterDebugInfoBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(sizeof(FHairClusterDebugInfo), ClusterData.ClusterCount), TEXT("Hair.CulledCompactedIndexBuffer"));
	}
#endif

	/// Initialise indirect buffers to be setup during the culling process
	{
		FHairIndBufferClearCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairIndBufferClearCS::FParameters>();
		Parameters->DispatchIndirectParametersClusterCount = GraphBuilder.CreateUAV(DispatchIndirectParametersClusterCount);
		Parameters->DrawIndirectParameters = DrawIndirectParametersBuffer.UAV;

		FHairIndBufferClearCS::FPermutationDomain Permutation;
		Permutation.Set<FHairIndBufferClearCS::FSetIndirectDraw>(false);
		TShaderMapRef<FHairIndBufferClearCS> ComputeShader(ShaderMap, Permutation);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("BufferClearCS"),
			ComputeShader,
			Parameters,
			FIntVector(1, 1, 1));
	}

	/// Cull cluster, generate indirect dispatch and prepare data to expand index buffer
	{
		const bool bClusterCullingFrozenCamera = GHairStrandsClusterCullingFreezeCamera > 0;
		if (!bClusterCullingFrozenCamera)
		{
			CapturedCameraWorldPos = View.ViewMatrices.GetViewOrigin();
			CapturedWorldToClipMatrix = View.ViewMatrices.GetViewProjectionMatrix();
			CapturedProjMatrix = View.ViewMatrices.GetProjectionMatrix();
		}

		float ForceLOD = -1;
		bool bIsVisible = true;
		if (GHairStrandsClusterForceLOD >= 0)
		{
			ForceLOD = GHairStrandsClusterForceLOD;
		}
		else if (ClusterData.LODIndex >= 0) // CPU-driven LOD selection
		{
			ForceLOD = ClusterData.LODIndex;
			bIsVisible = ClusterData.bVisible;
		}

		FHairClusterCullingCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairClusterCullingCS::FParameters>();
		Parameters->ProjectionMatrix = FMatrix44f(CapturedProjMatrix);			// LWC_TODO: Precision loss
		Parameters->CameraWorldPos = FVector3f(CapturedCameraWorldPos);
		Parameters->WorldToClipMatrix = FMatrix44f(CapturedWorldToClipMatrix);
		Parameters->ClusterCount = ClusterData.ClusterCount;
		Parameters->LODForcedIndex = ForceLOD;
		Parameters->LODBias = ClusterData.LODBias;
		Parameters->bIsHairGroupVisible = bIsVisible ? 1 : 0;

		Parameters->NumConvexHullPlanes = View.ViewFrustum.Planes.Num();
		check(Parameters->NumConvexHullPlanes <= 6);
		for (uint32 i = 0; i < Parameters->NumConvexHullPlanes; ++i)
		{
			const FPlane& Plane = View.ViewFrustum.Planes[i];
			Parameters->ViewFrustumConvexHull[i] = FVector4f(Plane.X, Plane.Y, Plane.Z, Plane.W); // LWC_TODO: precision loss
		}

		Parameters->ClusterAABBBuffer = RegisterAsSRV(GraphBuilder, *ClusterData.ClusterAABBBuffer);
		Parameters->ClusterInfoBuffer = RegisterAsSRV(GraphBuilder, *ClusterData.ClusterInfoBuffer);
		Parameters->ClusterLODInfoBuffer = RegisterAsSRV(GraphBuilder, *ClusterData.ClusterLODInfoBuffer);

		Parameters->GlobalClusterIdBuffer = GraphBuilder.CreateUAV(GlobalClusterIdBuffer, PF_R32_UINT);
		Parameters->GlobalIndexStartBuffer = GraphBuilder.CreateUAV(GlobalIndexStartBuffer, PF_R32_UINT);
		Parameters->GlobalIndexCountBuffer = GraphBuilder.CreateUAV(GlobalIndexCountBuffer, PF_R32_UINT);
		Parameters->GlobalRadiusScaleBuffer = GraphBuilder.CreateUAV(GlobalRadiusScaleBuffer, PF_R32_FLOAT);

		Parameters->DispatchIndirectParametersClusterCount = GraphBuilder.CreateUAV(DispatchIndirectParametersClusterCount);
		Parameters->DrawIndirectParameters = DrawIndirectParametersBuffer.UAV;

#if WITH_EDITOR
		Parameters->ClusterDebugInfoBuffer = GraphBuilder.CreateUAV(ClusterDebugInfoBuffer, PF_R32_SINT);
#endif

		FHairClusterCullingCS::FPermutationDomain Permutation;
		Permutation.Set<FHairClusterCullingCS::FDebugAABBBuffer>(bClusterDebugAABBBuffer ? 1 : 0);
		TShaderMapRef<FHairClusterCullingCS> ComputeShader(ShaderMap, Permutation);
		const FIntVector DispatchCount = DispatchCount.DivideAndRoundUp(FIntVector(ClusterData.ClusterCount, 1, 1), FIntVector(64, 1, 1));
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ClusterCullingCS"),
			ComputeShader,
			Parameters,
			DispatchCount);
	}

	/// Prepare some indirect draw buffers for specific compute group size
	const uint32 GroupSize1D = GetVendorOptimalGroupSize1D();
	{
		FMainClusterCullingPrepareIndirectDrawsCS::FParameters* Parameters = GraphBuilder.AllocParameters<FMainClusterCullingPrepareIndirectDrawsCS::FParameters>();
		Parameters->GroupSize1D = GroupSize1D;
		Parameters->DispatchIndirectParametersClusterCount = GraphBuilder.CreateSRV(DispatchIndirectParametersClusterCount, PF_R32_UINT);
		Parameters->DispatchIndirectParametersClusterCount1D = GraphBuilder.CreateUAV(DispatchIndirectParametersClusterCount1D, PF_R32_UINT);
		Parameters->DispatchIndirectParametersClusterCount2D = GraphBuilder.CreateUAV(DispatchIndirectParametersClusterCount2D, PF_R32_UINT);
		Parameters->DispatchIndirectParametersClusterCountDiv512 = GraphBuilder.CreateUAV(DispatchIndirectParametersClusterCountDiv512, PF_R32_UINT);
		Parameters->DispatchIndirectParametersClusterCountDiv512Div512 = GraphBuilder.CreateUAV(DispatchIndirectParametersClusterCountDiv512Div512, PF_R32_UINT);

		TShaderMapRef<FMainClusterCullingPrepareIndirectDrawsCS> ComputeShader(ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("PrepareIndirectDrawsCS"),
			ComputeShader,
			Parameters,
			FIntVector(2, 1, 1));
	}

	/// local prefix sum per 512 block
	{
		FHairClusterCullingLocalBlockPreFixSumCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairClusterCullingLocalBlockPreFixSumCS::FParameters>();
		Parameters->DispatchIndirectParametersClusterCountDiv512 = DispatchIndirectParametersClusterCountDiv512;
		Parameters->DispatchIndirectParametersClusterCount = GraphBuilder.CreateSRV(DispatchIndirectParametersClusterCount);
		Parameters->GlobalIndexCountBuffer = GraphBuilder.CreateSRV(GlobalIndexCountBuffer, PF_R32_UINT);
		Parameters->PerBlocklIndexCountPreFixSumBuffer = GraphBuilder.CreateUAV(PerBlocklIndexCountPreFixSumBuffer, PF_R32G32_UINT);
		Parameters->PerBlocklTotalIndexCountBuffer = GraphBuilder.CreateUAV(PerBlocklTotalIndexCountBuffer, PF_R32_UINT);

		TShaderMapRef<FHairClusterCullingLocalBlockPreFixSumCS> ComputeShader(ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("WithinBlockIndexCountPreFixSumCS"),
			ComputeShader,
			Parameters,
			DispatchIndirectParametersClusterCountDiv512, 0); // FIX ME, this could get over 65535
		check(ClusterData.ClusterCount / 512 <= 65535);
	}

	/// Prefix sum on the total index count per block of 512
	{
		FHairClusterCullingPreFixSumCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairClusterCullingPreFixSumCS::FParameters>();
		Parameters->DispatchIndirectParameters = DispatchIndirectParametersClusterCountDiv512Div512;
		Parameters->DispatchIndirectParametersClusterCount = GraphBuilder.CreateSRV(DispatchIndirectParametersClusterCountDiv512, PF_R32_UINT);
		Parameters->GlobalIndexCountBuffer = GraphBuilder.CreateSRV(PerBlocklTotalIndexCountBuffer, PF_R32_UINT);
		Parameters->GlobalIndexCountPreFixSumBuffer = GraphBuilder.CreateUAV(PerBlocklTotalIndexCountPreFixSumBuffer, PF_R32G32_UINT);

		TShaderMapRef<FHairClusterCullingPreFixSumCS> ComputeShader(ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("BlockIndexCountPreFixSumCS"),
			ComputeShader,
			Parameters,
			DispatchIndirectParametersClusterCountDiv512Div512, 0); // FIX ME, this could get over 65535
		check(ClusterData.ClusterCount / (512*512) <= 65535);
	}

	/// Compact to VertexId buffer using hierarchical binary search or splatting
	{
		check(ClusterData.GetCulledVertexIdBuffer());
		check(ClusterData.GetCulledVertexRadiusScaleBuffer());
		
		FRDGImportedBuffer ClusterVertexIdBuffer			= Register(GraphBuilder, *ClusterData.ClusterVertexIdBuffer, ERDGImportedBufferFlags::CreateSRV);
		FRDGImportedBuffer CulledCompactedIndexBuffer		= Register(GraphBuilder, *ClusterData.GetCulledVertexIdBuffer(), ERDGImportedBufferFlags::CreateUAV);
		FRDGImportedBuffer CulledCompactedRadiusScaleBuffer	= Register(GraphBuilder, *ClusterData.GetCulledVertexRadiusScaleBuffer(), ERDGImportedBufferFlags::CreateUAV);

		FHairClusterCullingCompactVertexIdsLocalBlockCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairClusterCullingCompactVertexIdsLocalBlockCS::FParameters>();

		Parameters->DispatchIndirectParametersClusterCount = GraphBuilder.CreateSRV(DispatchIndirectParametersClusterCount, PF_R32_UINT);
		Parameters->DispatchIndirectParametersClusterCount2D = GraphBuilder.CreateSRV(DispatchIndirectParametersClusterCount2D, PF_R32_UINT);
		Parameters->DispatchIndirectParametersClusterCountDiv512 = GraphBuilder.CreateSRV(DispatchIndirectParametersClusterCountDiv512, PF_R32_UINT);

		Parameters->PerBlocklIndexCountPreFixSumBuffer = GraphBuilder.CreateSRV(PerBlocklIndexCountPreFixSumBuffer, PF_R32G32_UINT);
		Parameters->PerBlocklTotalIndexCountPreFixSumBuffer = GraphBuilder.CreateSRV(PerBlocklTotalIndexCountPreFixSumBuffer, PF_R32G32_UINT);

		Parameters->GlobalIndexStartBuffer = GraphBuilder.CreateSRV(GlobalIndexStartBuffer, PF_R32_UINT);
		Parameters->GlobalIndexCountBuffer = GraphBuilder.CreateSRV(GlobalIndexCountBuffer, PF_R32_UINT);
		Parameters->GlobalRadiusScaleBuffer = GraphBuilder.CreateSRV(GlobalRadiusScaleBuffer, PF_R32_FLOAT);
		Parameters->ClusterVertexIdBuffer = ClusterVertexIdBuffer.SRV;

		
		Parameters->CulledCompactedIndexBuffer = CulledCompactedIndexBuffer.UAV;
		Parameters->CulledCompactedRadiusScaleBuffer = CulledCompactedRadiusScaleBuffer.UAV;

		Parameters->DispatchIndirectParametersBuffer = DispatchIndirectParametersClusterCount2D;
		TShaderMapRef<FHairClusterCullingCompactVertexIdsLocalBlockCS> ComputeShader(ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("SplatCompactVertexIdsCS"),
			ComputeShader,
			Parameters,
			DispatchIndirectParametersClusterCount2D, 0); // DispatchIndirectParametersClusterCount2D is used to avoid having any dispatch dimension going above 65535.

		GraphBuilder.SetBufferAccessFinal(CulledCompactedIndexBuffer.Buffer, ERHIAccess::SRVMask);
		GraphBuilder.SetBufferAccessFinal(CulledCompactedRadiusScaleBuffer.Buffer, ERHIAccess::SRVMask);
	}

	{
		ClusterData.ClusterIdBuffer = GraphBuilder.ConvertToExternalBuffer(GlobalClusterIdBuffer);
		ClusterData.ClusterIndexOffsetBuffer = GraphBuilder.ConvertToExternalBuffer(GlobalIndexStartBuffer);
		ClusterData.ClusterIndexCountBuffer = GraphBuilder.ConvertToExternalBuffer(GlobalIndexCountBuffer);
		ClusterData.CulledCluster2DIndirectArgsBuffer = DispatchIndirectParametersClusterCount2D;
		ClusterData.CulledCluster1DIndirectArgsBuffer = DispatchIndirectParametersClusterCount1D;
		ClusterData.CulledClusterCountBuffer = DispatchIndirectParametersClusterCount;
		ClusterData.GroupSize1D = GroupSize1D;
	}

	/// Prepare some indirect dispatch for compute raster visibility buffers
	{
		FMainClusterCullingPrepareIndirectDispatchCS::FParameters* Parameters = GraphBuilder.AllocParameters<FMainClusterCullingPrepareIndirectDispatchCS::FParameters>();
		Parameters->DrawIndirectBuffer = DrawIndirectParametersBuffer.UAV;
		Parameters->DispatchIndirectBuffer = DrawIndirectParametersRasterComputeBuffer.UAV;
		
		FMainClusterCullingPrepareIndirectDispatchCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FMainClusterCullingPrepareIndirectDispatchCS::FGroupSize>(GetVendorOptimalGroupSize1D());
		TShaderMapRef<FMainClusterCullingPrepareIndirectDispatchCS> ComputeShader(ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("PrepareIndirectDispatchCS"),
			ComputeShader,
			Parameters,
			FIntVector(1, 1, 1));
	}

	// Should this be move onto the culling result?
#if WITH_EDITOR
	if (bClusterDebugAABBBuffer)
	{
		ClusterData.ClusterDebugInfoBuffer = GraphBuilder.ConvertToExternalBuffer(ClusterDebugInfoBuffer);
	}
#endif

	GraphBuilder.SetBufferAccessFinal(DrawIndirectParametersBuffer.Buffer, ERHIAccess::IndirectArgs | ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(DrawIndirectParametersRasterComputeBuffer.Buffer, ERHIAccess::IndirectArgs | ERHIAccess::SRVMask);

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
	Parameters->VertexCountPerInstance = ClusterData.HairGroupPublicPtr->GetGroupInstanceVertexCount();

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
	const TArray<FViewInfo>& Views,
	const FHairCullingParams& CullingParameters,
	FHairStrandClusterData& ClusterDatas)
{
	DECLARE_GPU_STAT(HairStrandsClusterCulling);
	RDG_EVENT_SCOPE(GraphBuilder, "HairStrandsClusterCulling");
	TRACE_CPUPROFILER_EVENT_SCOPE(ComputeHairStrandsClustersCulling);
	RDG_GPU_STAT_SCOPE(GraphBuilder, HairStrandsClusterCulling);

	const bool bClusterCulling = IsHairStrandsClusterCullingEnable();
	for (const FViewInfo& View : Views)
	{
		// TODO use compute overlap (will need to split AddClusterCullingPass)
		for (FHairStrandClusterData::FHairGroup& ClusterData : ClusterDatas.HairGroups)
		{
			AddClusterResetLod0(GraphBuilder, &ShaderMap, ClusterData);
		}

		if (bClusterCulling)
		{
			for (FHairStrandClusterData::FHairGroup& ClusterData : ClusterDatas.HairGroups)		
			{
				AddClusterCullingPass(
					GraphBuilder, 
					&ShaderMap, 
					View, 
					CullingParameters,
					ClusterData);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
