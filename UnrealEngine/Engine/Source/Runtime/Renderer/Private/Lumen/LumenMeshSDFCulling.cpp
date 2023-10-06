// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LumenMeshSDFCulling.cpp
=============================================================================*/

#include "RendererPrivate.h"
#include "LightRendering.h"
#include "ScenePrivate.h"
#include "SceneUtils.h"
#include "PipelineStateCache.h"
#include "ShaderParameterStruct.h"
#include "PixelShaderUtils.h"
#include "DistanceFieldLightingShared.h"
#include "LumenMeshCards.h"
#include "LumenTracingUtils.h"

int32 GMeshSDFAverageCulledCount = 512;
FAutoConsoleVariableRef CVarMeshSDFAverageCulledCount(
	TEXT("r.Lumen.DiffuseIndirect.MeshSDF.AverageCulledCount"),
	GMeshSDFAverageCulledCount,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GMeshSDFRadiusThreshold = 30;
FAutoConsoleVariableRef CVarMeshSDFRadiusThreshold(
	TEXT("r.Lumen.DiffuseIndirect.MeshSDF.RadiusThreshold"),
	GMeshSDFRadiusThreshold,
	TEXT(""),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GMeshSDFNotCoveredExpandSurfaceScale = .6f;
FAutoConsoleVariableRef CVarMeshSDFNotCoveredExpandSurfaceScale(
	TEXT("r.Lumen.DiffuseIndirect.MeshSDF.NotCoveredExpandSurfaceScale"),
	GMeshSDFNotCoveredExpandSurfaceScale,
	TEXT("Scales the surface expand used for Mesh SDFs that contain mostly two sided materials (foliage)"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GMeshSDFNotCoveredMinStepScale = 32;
FAutoConsoleVariableRef CVarMeshSDFNotCoveredMinStepScale(
	TEXT("r.Lumen.DiffuseIndirect.MeshSDF.NotCoveredMinStepScale"),
	GMeshSDFNotCoveredMinStepScale,
	TEXT("Scales the min step size to improve performance, for Mesh SDFs that contain mostly two sided materials (foliage)"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

float GMeshSDFDitheredTransparencyStepThreshold = .1f;
FAutoConsoleVariableRef CVarMeshSDFDitheredTransparencyStepThreshold(
	TEXT("r.Lumen.DiffuseIndirect.MeshSDF.DitheredTransparencyStepThreshold"),
	GMeshSDFDitheredTransparencyStepThreshold,
	TEXT("Per-step stochastic semi-transparency threshold, for tracing users that have dithered transparency enabled, for Mesh SDFs that contain mostly two sided materials (foliage)"),
	ECVF_Scalability | ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenSceneHeightfieldCullForView(
	TEXT("r.LumenScene.Heightfield.CullForView"),
	1,
	TEXT("Enables Heightfield culling (default = 1)"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<int32> CVarLumenSceneHeightfieldFroxelCulling(
	TEXT("r.LumenScene.Heightfield.FroxelCulling"),
	1,
	TEXT("Enables Heightfield froxel view culling (default = 1)"),
	ECVF_RenderThreadSafe
);

void SetupLumenMeshSDFTracingParameters(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View, 
	FLumenMeshSDFTracingParameters& OutParameters)
{
	const FDistanceFieldSceneData& DistanceFieldSceneData = Scene->DistanceFieldSceneData;
	OutParameters.DistanceFieldObjectBuffers = DistanceField::SetupObjectBufferParameters(GraphBuilder, DistanceFieldSceneData);
	OutParameters.DistanceFieldAtlas = DistanceField::SetupAtlasParameters(GraphBuilder, DistanceFieldSceneData);
	OutParameters.MeshSDFNotCoveredExpandSurfaceScale = GMeshSDFNotCoveredExpandSurfaceScale;
	OutParameters.MeshSDFNotCoveredMinStepScale = GMeshSDFNotCoveredMinStepScale;
	OutParameters.MeshSDFDitheredTransparencyStepThreshold = GMeshSDFDitheredTransparencyStepThreshold;
}

uint32 CullMeshSDFObjectsForViewGroupSize = 64;

class FCullMeshSDFObjectsForViewCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCullMeshSDFObjectsForViewCS)
	SHADER_USE_PARAMETER_STRUCT(FCullMeshSDFObjectsForViewCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWNumCulledObjects)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWObjectIndexBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWObjectIndirectArguments)
		SHADER_PARAMETER_STRUCT_INCLUDE(FDistanceFieldObjectBufferParameters, DistanceFieldObjectBuffers)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(uint32, NumConvexHullPlanes)
		SHADER_PARAMETER_ARRAY(FVector4f, ViewFrustumConvexHull, [6])
		SHADER_PARAMETER(uint32, ObjectBoundingGeometryIndexCount)
		SHADER_PARAMETER(float, CardTraceEndDistanceFromCamera)
		SHADER_PARAMETER(float, MaxMeshSDFInfluenceRadius)
		SHADER_PARAMETER(float, MeshSDFRadiusThreshold)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), CullMeshSDFObjectsForViewGroupSize);
	}
};

IMPLEMENT_GLOBAL_SHADER(FCullMeshSDFObjectsForViewCS, "/Engine/Private/Lumen/LumenMeshSDFCulling.usf", "CullMeshSDFObjectsForViewCS", SF_Compute);

class FCombineObjectIndexBuffersCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCombineObjectIndexBuffersCS);
	SHADER_USE_PARAMETER_STRUCT(FCombineObjectIndexBuffersCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint32>, MeshSDFIndexBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint32>, HeightfieldIndexBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint32>, NumCulledMeshSDFObjects)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint32>, NumCulledHeightfieldObjects)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint32>, RWCombinedObjectIndexBuffer)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), CullMeshSDFObjectsForViewGroupSize);
	}
};

IMPLEMENT_GLOBAL_SHADER(FCombineObjectIndexBuffersCS, "/Engine/Private/Lumen/LumenMeshSDFCulling.usf", "CombineObjectIndexBuffersCS", SF_Compute);

class FMeshSDFObjectCullVS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMeshSDFObjectCullVS);
	SHADER_USE_PARAMETER_STRUCT(FMeshSDFObjectCullVS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint32>, ObjectIndexBuffer)
		// SDF parameters
		SHADER_PARAMETER_STRUCT_INCLUDE(FDistanceFieldObjectBufferParameters, DistanceFieldObjectBuffers)
		// Heightfield parameters
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(float, ConservativeRadiusScale)
		SHADER_PARAMETER(float, MaxMeshSDFInfluenceRadius)
	END_SHADER_PARAMETER_STRUCT()

	class FCullMeshTypeSDF : SHADER_PERMUTATION_BOOL("CULL_MESH_SDF");
	class FCullMeshTypeHeightfield : SHADER_PERMUTATION_BOOL("CULL_MESH_HEIGHTFIELD");

	using FPermutationDomain = TShaderPermutationDomain<FCullMeshTypeSDF, FCullMeshTypeHeightfield>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMeshSDFObjectCullVS, "/Engine/Private/Lumen/LumenMeshSDFCulling.usf", "MeshSDFObjectCullVS", SF_Vertex);

class FMeshSDFObjectCullPS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FMeshSDFObjectCullPS);
	SHADER_USE_PARAMETER_STRUCT(FMeshSDFObjectCullPS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWNumGridCulledMeshSDFObjects)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWNumGridCulledHeightfieldObjects)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWNumCulledObjectsToCompact)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWCulledObjectsToCompactArray)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, GridCulledMeshSDFObjectStartOffsetArray)
		// SDF parameters
		SHADER_PARAMETER_STRUCT_INCLUDE(FDistanceFieldObjectBufferParameters, DFObjectBufferParameters)
		SHADER_PARAMETER_STRUCT_INCLUDE(FDistanceFieldAtlasParameters, DistanceFieldAtlas)
		// Heightfield parameters
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER(float, MaxMeshSDFInfluenceRadius)
		SHADER_PARAMETER(FVector3f, CardGridZParams)
		SHADER_PARAMETER(uint32, CardGridPixelSizeShift)
		SHADER_PARAMETER(FIntVector, CullGridSize)
		SHADER_PARAMETER(float, CardTraceEndDistanceFromCamera)
		SHADER_PARAMETER(uint32, MaxNumberOfCulledObjects)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, ClosestHZBTexture)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, FurthestHZBTexture)
		SHADER_PARAMETER(float, HZBMipLevel)
		SHADER_PARAMETER(uint32, HaveClosestHZB)
		SHADER_PARAMETER(FVector2f, ViewportUVToHZBBufferUV)
	END_SHADER_PARAMETER_STRUCT()

	class FCullToFroxelGrid : SHADER_PERMUTATION_BOOL("CULL_TO_FROXEL_GRID");
	class FCullMeshTypeSDF : SHADER_PERMUTATION_BOOL("CULL_MESH_SDF");
	class FCullMeshTypeHeightfield : SHADER_PERMUTATION_BOOL("CULL_MESH_HEIGHTFIELD");
	class FOffsetDataStructure : SHADER_PERMUTATION_INT("OFFSET_DATA_STRUCT", 3);
	
	using FPermutationDomain = TShaderPermutationDomain<FCullToFroxelGrid, FCullMeshTypeSDF, FCullMeshTypeHeightfield, FOffsetDataStructure>;

	static FPermutationDomain RemapPermutation(FPermutationDomain PermutationVector)
	{
		// FOffsetDataStructure is only used for mesh SDFs
		if (!PermutationVector.Get<FCullMeshTypeSDF>())
		{
			PermutationVector.Set<FOffsetDataStructure>(0);
		}

		return PermutationVector;
	}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);

		if (RemapPermutation(PermutationVector) != PermutationVector)
		{
			return false;
		}

		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMeshSDFObjectCullPS, "/Engine/Private/Lumen/LumenMeshSDFCulling.usf", "MeshSDFObjectCullPS", SF_Pixel);

BEGIN_SHADER_PARAMETER_STRUCT(FMeshSDFObjectCull, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FMeshSDFObjectCullVS::FParameters, VS)
	SHADER_PARAMETER_STRUCT_INCLUDE(FMeshSDFObjectCullPS::FParameters, PS)
	RDG_BUFFER_ACCESS(MeshSDFIndirectArgs, ERHIAccess::IndirectArgs)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FCompactCulledObjectsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCompactCulledObjectsCS);
	SHADER_USE_PARAMETER_STRUCT(FCompactCulledObjectsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Mesh SDF
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, GridCulledMeshSDFObjectStartOffsetArray)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWNumGridCulledMeshSDFObjects)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWGridCulledMeshSDFObjectIndicesArray)
		// Heightfield
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, GridCulledHeightfieldObjectStartOffsetArray)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWNumGridCulledHeightfieldObjects)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWGridCulledHeightfieldObjectIndicesArray)
		// Type-agnostic data
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, NumCulledObjectsToCompact)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, CulledObjectsToCompactArray)

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		RDG_BUFFER_ACCESS(CompactCulledObjectsIndirectArguments, ERHIAccess::IndirectArgs)
		SHADER_PARAMETER(uint32, MaxNumberOfCulledObjects)
	END_SHADER_PARAMETER_STRUCT()

	class FCullMeshTypeSDF : SHADER_PERMUTATION_BOOL("CULL_MESH_SDF");
	class FCullMeshTypeHeightfield : SHADER_PERMUTATION_BOOL("CULL_MESH_HEIGHTFIELD");
	using FPermutationDomain = TShaderPermutationDomain<FCullMeshTypeSDF, FCullMeshTypeHeightfield>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static int32 GetGroupSize()
	{
		return 64;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), GetGroupSize());
	}
};

IMPLEMENT_GLOBAL_SHADER(FCompactCulledObjectsCS, "/Engine/Private/Lumen/LumenMeshSDFCulling.usf", "CompactCulledObjectsCS", SF_Compute);


uint32 ComputeCulledMeshSDFObjectsStartOffsetGroupSize = 64;

class FComputeCulledObjectsStartOffsetCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FComputeCulledObjectsStartOffsetCS)
	SHADER_USE_PARAMETER_STRUCT(FComputeCulledObjectsStartOffsetCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		// Mesh SDF
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, NumGridCulledMeshSDFObjects)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWGridCulledMeshSDFObjectStartOffsetArray)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWCulledMeshSDFObjectAllocator)
		// Heightfield
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, NumGridCulledHeightfieldObjects)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWGridCulledHeightfieldObjectStartOffsetArray)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWCulledHeightfieldObjectAllocator)
		// Type-agnostic
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWCompactCulledObjectsIndirectArguments)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, NumCulledObjectsToCompact)
		SHADER_PARAMETER(uint32, NumCullGridCells)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), ComputeCulledMeshSDFObjectsStartOffsetGroupSize);
	}
};

IMPLEMENT_GLOBAL_SHADER(FComputeCulledObjectsStartOffsetCS, "/Engine/Private/Lumen/LumenMeshSDFCulling.usf", "ComputeCulledObjectsStartOffsetCS", SF_Compute);

class FObjectCullingContext
{
public:
	uint32 NumCullGridCells = 0;
	uint32 MaxNumberOfCulledObjects = 0;

	// View culled object data
	FRDGBufferRef NumMeshSDFCulledObjects = nullptr;
	FRDGBufferRef MeshSDFObjectIndexBuffer = nullptr;

	FRDGBufferRef NumHeightfieldCulledObjects = nullptr;
	FRDGBufferRef HeightfieldObjectIndexBuffer = nullptr;

	// Froxel-culled object data
	FRDGBufferRef NumGridCulledMeshSDFObjects = nullptr;
	FRDGBufferRef GridCulledMeshSDFObjectStartOffsetArray = nullptr;
	FRDGBufferRef GridCulledMeshSDFObjectIndicesArray = nullptr;

	FRDGBufferRef NumGridCulledHeightfieldObjects = nullptr;
	FRDGBufferRef GridCulledHeightfieldObjectStartOffsetArray = nullptr;
	FRDGBufferRef GridCulledHeightfieldObjectIndicesArray = nullptr;

	// Intermediary buffers
	FRDGBufferRef ObjectIndirectArguments = nullptr;
	FRDGBufferRef NumCulledObjectsToCompact = nullptr;
	FRDGBufferRef CulledObjectsToCompactArray = nullptr;
};

void InitObjectCullingContext(
	FRDGBuilder& GraphBuilder,
	uint32 NumCullGridCells,
	FObjectCullingContext& Context)
{
	Context.NumCullGridCells = NumCullGridCells;
	Context.MaxNumberOfCulledObjects = NumCullGridCells * GMeshSDFAverageCulledCount;

	Context.NumGridCulledMeshSDFObjects = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), NumCullGridCells), TEXT("Lumen.NumGridCulledMeshSDFObjects"));
	Context.NumGridCulledHeightfieldObjects = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), NumCullGridCells), TEXT("Lumen.NumGridCulledHeightfieldObjects"));

	Context.GridCulledMeshSDFObjectIndicesArray = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), Context.MaxNumberOfCulledObjects), TEXT("Lumen.GridCulledMeshSDFObjectIndicesArray"));
	Context.GridCulledHeightfieldObjectIndicesArray = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), Context.MaxNumberOfCulledObjects), TEXT("Lumen.GridCulledHeightfieldObjectIndicesArray"));

	Context.NumCulledObjectsToCompact = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("Lumen.NumCulledObjectsToCompact"));
	Context.CulledObjectsToCompactArray = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 2 * Context.MaxNumberOfCulledObjects), TEXT("Lumen.CulledObjectsToCompactArray"));

	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Context.NumGridCulledMeshSDFObjects, PF_R32_UINT), 0);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Context.NumGridCulledHeightfieldObjects, PF_R32_UINT), 0);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Context.NumCulledObjectsToCompact, PF_R32_UINT), 0);
}

void FillGridParameters(
	FRDGBuilder& GraphBuilder, 
	const FScene* Scene,
	const FViewInfo& View,
	const FObjectCullingContext* Context,
	FLumenMeshSDFGridParameters& OutGridParameters)
{
	const FDistanceFieldSceneData& DistanceFieldSceneData = Scene->DistanceFieldSceneData;

	SetupLumenMeshSDFTracingParameters(GraphBuilder, Scene, View, OutGridParameters.TracingParameters);

	if (Context)
	{
		bool bCullMeshSDFObjects = DistanceFieldSceneData.NumObjectsInBuffer > 0;
		if (bCullMeshSDFObjects)
		{
			// Froxel-culled data
			OutGridParameters.NumGridCulledMeshSDFObjects = GraphBuilder.CreateSRV(Context->NumGridCulledMeshSDFObjects, PF_R32_UINT);
			OutGridParameters.GridCulledMeshSDFObjectStartOffsetArray = GraphBuilder.CreateSRV(Context->GridCulledMeshSDFObjectStartOffsetArray, PF_R32_UINT);
			OutGridParameters.GridCulledMeshSDFObjectIndicesArray = GraphBuilder.CreateSRV(Context->GridCulledMeshSDFObjectIndicesArray, PF_R32_UINT);
		}

		bool bCullHeightfieldObjects = Lumen::UseHeightfieldTracing(*View.Family, *Scene->GetLumenSceneData(View));
		if (bCullHeightfieldObjects)
		{
			// View-culled heightfield objects
			OutGridParameters.NumCulledHeightfieldObjects = GraphBuilder.CreateSRV(Context->NumHeightfieldCulledObjects, PF_R32_UINT);
			OutGridParameters.CulledHeightfieldObjectIndexBuffer = GraphBuilder.CreateSRV(Context->HeightfieldObjectIndexBuffer, PF_R32_UINT);

			// Froxel-culled heightfield objects are optionally set, depending on the method
			if (Context->NumGridCulledHeightfieldObjects)
			{
				OutGridParameters.NumGridCulledHeightfieldObjects = GraphBuilder.CreateSRV(Context->NumGridCulledHeightfieldObjects, PF_R32_UINT);
				OutGridParameters.GridCulledHeightfieldObjectStartOffsetArray = GraphBuilder.CreateSRV(Context->GridCulledHeightfieldObjectStartOffsetArray, PF_R32_UINT);
				OutGridParameters.GridCulledHeightfieldObjectIndicesArray = GraphBuilder.CreateSRV(Context->GridCulledHeightfieldObjectIndicesArray, PF_R32_UINT);
			}
		}
	}
	else
	{
		OutGridParameters.NumGridCulledMeshSDFObjects = nullptr;
		OutGridParameters.GridCulledMeshSDFObjectStartOffsetArray = nullptr;
		OutGridParameters.GridCulledMeshSDFObjectIndicesArray = nullptr;

		OutGridParameters.NumGridCulledHeightfieldObjects = nullptr;
		OutGridParameters.GridCulledHeightfieldObjectStartOffsetArray = nullptr;
		OutGridParameters.GridCulledHeightfieldObjectIndicesArray = nullptr;

		OutGridParameters.NumCulledHeightfieldObjects = nullptr;
		OutGridParameters.CulledHeightfieldObjectIndexBuffer = nullptr;
	}
}

class FCullHeightfieldObjectsForViewCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FCullHeightfieldObjectsForViewCS)
	SHADER_USE_PARAMETER_STRUCT(FCullHeightfieldObjectsForViewCS, FGlobalShader)

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
		SHADER_PARAMETER_RDG_UNIFORM_BUFFER(FLumenCardScene, LumenCardScene)
		SHADER_PARAMETER(float, CardTraceEndDistanceFromCamera)
		SHADER_PARAMETER(float, MaxMeshSDFInfluenceRadius)
		SHADER_PARAMETER(int, MaxNumObjects)
		SHADER_PARAMETER(int, bShouldCull)
		SHADER_PARAMETER(uint32, ObjectBoundingGeometryIndexCount)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWNumCulledObjects)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWCulledObjectIndexBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RWObjectIndirectArguments)
	END_SHADER_PARAMETER_STRUCT()

	using FPermutationDomain = TShaderPermutationDomain<>;
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return DoesPlatformSupportLumenGI(Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), CullMeshSDFObjectsForViewGroupSize);
	}
};

IMPLEMENT_GLOBAL_SHADER(FCullHeightfieldObjectsForViewCS, "/Engine/Private/Lumen/LumenMeshSDFCulling.usf", "CullHeightfieldObjectsForViewCS", SF_Compute);

void CombineObjectIndexBuffers(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	bool bCullMeshSDFObjects,
	bool bCullHeightfieldObjects,
	FObjectCullingContext& Context,
	FRDGBufferRef& CombinedObjectIndexBuffer
)
{
	const FDistanceFieldSceneData& DistanceFieldSceneData = Scene->DistanceFieldSceneData;
	const FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(View);

	if (bCullMeshSDFObjects && bCullHeightfieldObjects)
	{
		uint32 NumDistanceFields = DistanceFieldSceneData.NumObjectsInBuffer;
		uint32 NumHeightfields = LumenSceneData.Heightfields.Num();
		uint32 MaxNumObjects = FMath::RoundUpToPowerOfTwo(NumDistanceFields + NumHeightfields);
		CombinedObjectIndexBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), MaxNumObjects), TEXT("Lumen.CombinedObjectIndexBuffer"));

		FCombineObjectIndexBuffersCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCombineObjectIndexBuffersCS::FParameters>();
		{
			PassParameters->MeshSDFIndexBuffer = GraphBuilder.CreateSRV(Context.MeshSDFObjectIndexBuffer, PF_R32_UINT);
			PassParameters->HeightfieldIndexBuffer = GraphBuilder.CreateSRV(Context.HeightfieldObjectIndexBuffer, PF_R32_UINT);
			PassParameters->NumCulledMeshSDFObjects = GraphBuilder.CreateSRV(Context.NumMeshSDFCulledObjects, PF_R32_UINT);
			PassParameters->NumCulledHeightfieldObjects = GraphBuilder.CreateSRV(Context.NumHeightfieldCulledObjects, PF_R32_UINT);

			PassParameters->RWCombinedObjectIndexBuffer = GraphBuilder.CreateUAV(CombinedObjectIndexBuffer, PF_R32_UINT);
		}

		auto ComputeShader = View.ShaderMap->GetShader<FCombineObjectIndexBuffersCS>();
		const int32 GroupSize = FMath::DivideAndRoundUp<int32>(NumDistanceFields + NumHeightfields, CullMeshSDFObjectsForViewGroupSize);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CombineObjectIndexBuffers"),
			ComputeShader,
			PassParameters,
			FIntVector(GroupSize, 1, 1));
	}
	else if (bCullHeightfieldObjects)
	{
		CombinedObjectIndexBuffer = Context.HeightfieldObjectIndexBuffer;
	}
	else //if (bCullMeshSDFObjects)
	{
		CombinedObjectIndexBuffer = Context.MeshSDFObjectIndexBuffer;
	}
}

void CullHeightfieldObjectsForView(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	float MaxMeshSDFInfluenceRadius,
	float CardTraceEndDistanceFromCamera,
	FRDGBufferRef& NumHeightfieldCulledObjects,
	FRDGBufferRef& HeightfieldObjectIndexBuffer,
	FRDGBufferRef& HeightfieldObjectIndirectArguments)
{
	const FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(View);

	// We don't want any heightfield overhead if there are no heightfields in the scene
	check(Lumen::UseHeightfieldTracing(*View.Family, LumenSceneData));

	uint32 NumHeightfields = LumenSceneData.Heightfields.Num();
	uint32 MaxNumHeightfields = FMath::RoundUpToPowerOfTwo(LumenSceneData.Heightfields.Num());

	NumHeightfieldCulledObjects = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 2), TEXT("Lumen.NumCulledHeightfieldObjects"));
	HeightfieldObjectIndexBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), MaxNumHeightfields), TEXT("Lumen.CulledHeightfieldObjectIndices"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(NumHeightfieldCulledObjects, PF_R32_UINT), 0);

	FCullHeightfieldObjectsForViewCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCullHeightfieldObjectsForViewCS::FParameters>();
	{
		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->LumenCardScene = FrameTemporaries.LumenCardSceneUniformBuffer;
		PassParameters->CardTraceEndDistanceFromCamera = CardTraceEndDistanceFromCamera;
		PassParameters->MaxMeshSDFInfluenceRadius = MaxMeshSDFInfluenceRadius;
		PassParameters->MaxNumObjects = NumHeightfields;
		PassParameters->bShouldCull = CVarLumenSceneHeightfieldCullForView.GetValueOnRenderThread() != 0;
		PassParameters->ObjectBoundingGeometryIndexCount = StencilingGeometry::GLowPolyStencilSphereIndexBuffer.GetIndexCount();

		PassParameters->RWNumCulledObjects = GraphBuilder.CreateUAV(NumHeightfieldCulledObjects, PF_R32_UINT);
		PassParameters->RWCulledObjectIndexBuffer = GraphBuilder.CreateUAV(HeightfieldObjectIndexBuffer, PF_R32_UINT);
		PassParameters->RWObjectIndirectArguments = GraphBuilder.CreateUAV(HeightfieldObjectIndirectArguments, PF_R32_UINT);
	}

	auto ComputeShader = View.ShaderMap->GetShader<FCullHeightfieldObjectsForViewCS>();
	const int32 GroupSize = FMath::DivideAndRoundUp<int32>(NumHeightfields, CullMeshSDFObjectsForViewGroupSize);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("CullHeightfieldsForView"),
		ComputeShader,
		PassParameters,
		FIntVector(GroupSize, 1, 1));
}

void CullMeshSDFObjectsForView(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	float MaxMeshSDFInfluenceRadius,
	float CardTraceEndDistanceFromCamera,
	FObjectCullingContext& Context)
{
	const FLumenSceneData& LumenSceneData = *Scene->GetLumenSceneData(View);
	const FDistanceFieldSceneData& DistanceFieldSceneData = Scene->DistanceFieldSceneData;

	int32 MaxSDFMeshObjects = FMath::RoundUpToPowerOfTwo(DistanceFieldSceneData.NumObjectsInBuffer);
	MaxSDFMeshObjects = FMath::DivideAndRoundUp(MaxSDFMeshObjects, 128) * 128;

	Context.NumMeshSDFCulledObjects = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("Lumen.NumMeshSDFCulledObjects"));
	Context.MeshSDFObjectIndexBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), MaxSDFMeshObjects), TEXT("Lumen.MeshSDFObjectIndexBuffer"));

	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Context.NumMeshSDFCulledObjects, PF_R32_UINT), 0);

	{
		FCullMeshSDFObjectsForViewCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCullMeshSDFObjectsForViewCS::FParameters>();
		PassParameters->RWNumCulledObjects = GraphBuilder.CreateUAV(Context.NumMeshSDFCulledObjects, PF_R32_UINT);
		PassParameters->RWObjectIndexBuffer = GraphBuilder.CreateUAV(Context.MeshSDFObjectIndexBuffer, PF_R32_UINT);
		PassParameters->RWObjectIndirectArguments = GraphBuilder.CreateUAV(Context.ObjectIndirectArguments, PF_R32_UINT);
		PassParameters->DistanceFieldObjectBuffers = DistanceField::SetupObjectBufferParameters(GraphBuilder, DistanceFieldSceneData);

		PassParameters->View = View.ViewUniformBuffer;
		PassParameters->NumConvexHullPlanes = View.ViewFrustum.Planes.Num();

		for (int32 i = 0; i < View.ViewFrustum.Planes.Num(); i++)
		{
			PassParameters->ViewFrustumConvexHull[i] = FVector4f((FVector3f)View.ViewFrustum.Planes[i], View.ViewFrustum.Planes[i].W);
		}

		PassParameters->ObjectBoundingGeometryIndexCount = StencilingGeometry::GLowPolyStencilSphereIndexBuffer.GetIndexCount();
		PassParameters->CardTraceEndDistanceFromCamera = CardTraceEndDistanceFromCamera;
		PassParameters->MaxMeshSDFInfluenceRadius = MaxMeshSDFInfluenceRadius;
		PassParameters->MeshSDFRadiusThreshold = GMeshSDFRadiusThreshold / FMath::Clamp(View.FinalPostProcessSettings.LumenSceneDetail, .01f, 100.0f);

		auto ComputeShader = View.ShaderMap->GetShader<FCullMeshSDFObjectsForViewCS>();

		const int32 GroupSize = FMath::DivideAndRoundUp<int32>(DistanceFieldSceneData.NumObjectsInBuffer, CullMeshSDFObjectsForViewGroupSize);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CullMeshSDFObjectsForView"),
			ComputeShader,
			PassParameters,
			FIntVector(GroupSize, 1, 1));
	}
}

// Compact list of {ObjectIndex, GridCellIndex} into a continuous array
void CompactCulledObjectArray(
	FRDGBuilder& GraphBuilder,
	const FScene* Scene,
	const FViewInfo& View,
	FObjectCullingContext& Context,
	ERDGPassFlags ComputePassFlags)
{
	Context.GridCulledMeshSDFObjectStartOffsetArray = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), Context.NumCullGridCells), TEXT("Lumen.GridCulledMeshSDFObjectStartOffsetArray"));
	Context.GridCulledHeightfieldObjectStartOffsetArray = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), Context.NumCullGridCells), TEXT("Lumen.GridCulledHeightfieldObjectStartOffsetArray"));

	FRDGBufferRef CulledMeshSDFObjectAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("Lumen.CulledMeshSDFObjectAllocator"));
	FRDGBufferRef CulledHeightfieldObjectAllocator = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 1), TEXT("Lumen.CulledHeightfieldObjectAllocator"));
	FRDGBufferRef CompactCulledObjectsIndirectArguments = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDispatchIndirectParameters>(1), TEXT("Lumen.CompactCulledObjectsIndirectArguments"));

	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CulledMeshSDFObjectAllocator, PF_R32_UINT), 0, ComputePassFlags);
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(CulledHeightfieldObjectAllocator, PF_R32_UINT), 0, ComputePassFlags);

	{
		FComputeCulledObjectsStartOffsetCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FComputeCulledObjectsStartOffsetCS::FParameters>();
		{
			// Mesh SDF
			PassParameters->NumGridCulledMeshSDFObjects = GraphBuilder.CreateSRV(Context.NumGridCulledMeshSDFObjects, PF_R32_UINT);
			PassParameters->RWGridCulledMeshSDFObjectStartOffsetArray = GraphBuilder.CreateUAV(Context.GridCulledMeshSDFObjectStartOffsetArray, PF_R32_UINT);
			PassParameters->RWCulledMeshSDFObjectAllocator = GraphBuilder.CreateUAV(CulledMeshSDFObjectAllocator, PF_R32_UINT);
			// Heightfield
			PassParameters->NumGridCulledHeightfieldObjects = GraphBuilder.CreateSRV(Context.NumGridCulledHeightfieldObjects, PF_R32_UINT);
			PassParameters->RWGridCulledHeightfieldObjectStartOffsetArray = GraphBuilder.CreateUAV(Context.GridCulledHeightfieldObjectStartOffsetArray, PF_R32_UINT);
			PassParameters->RWCulledHeightfieldObjectAllocator = GraphBuilder.CreateUAV(CulledHeightfieldObjectAllocator, PF_R32_UINT);
			// Type-agnostic
			PassParameters->RWCompactCulledObjectsIndirectArguments = GraphBuilder.CreateUAV(CompactCulledObjectsIndirectArguments, PF_R32_UINT);
			PassParameters->NumCulledObjectsToCompact = GraphBuilder.CreateSRV(Context.NumCulledObjectsToCompact, PF_R32_UINT);
			PassParameters->NumCullGridCells = Context.NumCullGridCells;
		}

		auto ComputeShader = View.ShaderMap->GetShader<FComputeCulledObjectsStartOffsetCS>();

		int32 GroupSize = FMath::DivideAndRoundUp(Context.NumCullGridCells, ComputeCulledMeshSDFObjectsStartOffsetGroupSize);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("ComputeCulledObjectsStartOffsetCS"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			FIntVector(GroupSize, 1, 1));
	}

	FRDGBufferUAVRef NumGridCulledMeshSDFObjectsUAV = GraphBuilder.CreateUAV(Context.NumGridCulledMeshSDFObjects, PF_R32_UINT);
	FRDGBufferUAVRef NumGridCulledHeightfieldObjectsUAV = GraphBuilder.CreateUAV(Context.NumGridCulledHeightfieldObjects, PF_R32_UINT);

	AddClearUAVPass(GraphBuilder, NumGridCulledMeshSDFObjectsUAV, 0, ComputePassFlags);
	AddClearUAVPass(GraphBuilder, NumGridCulledHeightfieldObjectsUAV, 0, ComputePassFlags);

	{
		FCompactCulledObjectsCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FCompactCulledObjectsCS::FParameters>();
		{
			// Mesh SDF
			PassParameters->GridCulledMeshSDFObjectStartOffsetArray = GraphBuilder.CreateSRV(Context.GridCulledMeshSDFObjectStartOffsetArray, PF_R32_UINT);
			PassParameters->RWNumGridCulledMeshSDFObjects = NumGridCulledMeshSDFObjectsUAV;
			PassParameters->RWGridCulledMeshSDFObjectIndicesArray = GraphBuilder.CreateUAV(Context.GridCulledMeshSDFObjectIndicesArray, PF_R32_UINT);
			// Heightfield
			PassParameters->GridCulledHeightfieldObjectStartOffsetArray = GraphBuilder.CreateSRV(Context.GridCulledHeightfieldObjectStartOffsetArray, PF_R32_UINT);
			PassParameters->RWNumGridCulledHeightfieldObjects = NumGridCulledHeightfieldObjectsUAV;
			PassParameters->RWGridCulledHeightfieldObjectIndicesArray = GraphBuilder.CreateUAV(Context.GridCulledHeightfieldObjectIndicesArray, PF_R32_UINT);
			// Type-agnostic
			PassParameters->NumCulledObjectsToCompact = GraphBuilder.CreateSRV(Context.NumCulledObjectsToCompact, PF_R32_UINT);
			PassParameters->CulledObjectsToCompactArray = GraphBuilder.CreateSRV(Context.CulledObjectsToCompactArray, PF_R32_UINT);
			PassParameters->View = View.ViewUniformBuffer;
			PassParameters->CompactCulledObjectsIndirectArguments = CompactCulledObjectsIndirectArguments;
			PassParameters->MaxNumberOfCulledObjects = Context.MaxNumberOfCulledObjects;
		}

		FCompactCulledObjectsCS::FPermutationDomain PermutationVector;
		PermutationVector.Set< FCompactCulledObjectsCS::FCullMeshTypeSDF >(Scene->DistanceFieldSceneData.NumObjectsInBuffer > 0);
		PermutationVector.Set< FCompactCulledObjectsCS::FCullMeshTypeHeightfield >(Lumen::UseHeightfieldTracing(*View.Family, *Scene->GetLumenSceneData(View)));
		auto ComputeShader = View.ShaderMap->GetShader< FCompactCulledObjectsCS >(PermutationVector);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("CompactCulledObjects"),
			ComputePassFlags,
			ComputeShader,
			PassParameters,
			CompactCulledObjectsIndirectArguments,
			0);
	}
}

void CullObjectsToGrid(
	const FViewInfo& View,
	const FScene* Scene,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	float MaxMeshSDFInfluenceRadius,
	float CardTraceEndDistanceFromCamera,
	int32 GridPixelsPerCellXY,
	int32 GridSizeZ,
	FVector ZParams,
	FIntVector CullGridSize,
	FRDGBuilder& GraphBuilder,
	FRDGBufferRef ObjectIndexBuffer,
	FObjectCullingContext& Context)
{
	const FDistanceFieldSceneData& DistanceFieldSceneData = Scene->DistanceFieldSceneData;

	// Scatter mesh SDF objects into a temporary array of {ObjectIndex, GridCellIndex}
	FMeshSDFObjectCull* PassParameters = GraphBuilder.AllocParameters<FMeshSDFObjectCull>();
	{
		if (DistanceFieldSceneData.NumObjectsInBuffer > 0)
		{
			PassParameters->VS.DistanceFieldObjectBuffers = DistanceField::SetupObjectBufferParameters(GraphBuilder, DistanceFieldSceneData);
		}
		if (Lumen::UseHeightfieldTracing(*View.Family, *Scene->GetLumenSceneData(View)))
		{
			PassParameters->VS.LumenCardScene = FrameTemporaries.LumenCardSceneUniformBuffer;
		}
		PassParameters->VS.ObjectIndexBuffer = GraphBuilder.CreateSRV(ObjectIndexBuffer, PF_R32_UINT);
		PassParameters->VS.View = GetShaderBinding(View.ViewUniformBuffer);

		// Boost the effective radius so that the edges of the sphere approximation lie on the sphere, instead of the vertices
		const int32 NumRings = StencilingGeometry::GLowPolyStencilSphereVertexBuffer.GetNumRings();
		const float RadiansPerRingSegment = PI / (float)NumRings;
		PassParameters->VS.ConservativeRadiusScale = 1.0f / FMath::Cos(RadiansPerRingSegment);
		PassParameters->VS.MaxMeshSDFInfluenceRadius = MaxMeshSDFInfluenceRadius;

		PassParameters->PS.RWNumGridCulledMeshSDFObjects = GraphBuilder.CreateUAV(Context.NumGridCulledMeshSDFObjects, PF_R32_UINT);
		PassParameters->PS.RWNumGridCulledHeightfieldObjects = GraphBuilder.CreateUAV(Context.NumGridCulledHeightfieldObjects, PF_R32_UINT);
		PassParameters->PS.RWNumCulledObjectsToCompact = GraphBuilder.CreateUAV(Context.NumCulledObjectsToCompact, PF_R32_UINT);
		PassParameters->PS.RWCulledObjectsToCompactArray = GraphBuilder.CreateUAV(Context.CulledObjectsToCompactArray, PF_R32_UINT);
		if (DistanceFieldSceneData.NumObjectsInBuffer > 0)
		{
			PassParameters->PS.DFObjectBufferParameters = DistanceField::SetupObjectBufferParameters(GraphBuilder, Scene->DistanceFieldSceneData);
			PassParameters->PS.DistanceFieldAtlas = DistanceField::SetupAtlasParameters(GraphBuilder, DistanceFieldSceneData);
		}
		if (Lumen::UseHeightfieldTracing(*View.Family, *Scene->GetLumenSceneData(View)))
		{
			PassParameters->PS.LumenCardScene = FrameTemporaries.LumenCardSceneUniformBuffer;
		}
		PassParameters->PS.View = GetShaderBinding(View.ViewUniformBuffer);
		PassParameters->PS.MaxMeshSDFInfluenceRadius = MaxMeshSDFInfluenceRadius;
		PassParameters->PS.CardGridZParams = (FVector3f)ZParams;	// LWC_TODO: Precision Loss
		PassParameters->PS.CardGridPixelSizeShift = FMath::FloorLog2(GridPixelsPerCellXY);
		PassParameters->PS.CullGridSize = CullGridSize;
		PassParameters->PS.CardTraceEndDistanceFromCamera = CardTraceEndDistanceFromCamera;
		PassParameters->PS.MaxNumberOfCulledObjects = Context.MaxNumberOfCulledObjects;
		PassParameters->PS.ClosestHZBTexture = View.ClosestHZB ? View.ClosestHZB : GSystemTextures.GetBlackDummy(GraphBuilder);
		PassParameters->PS.FurthestHZBTexture = View.HZB;
		PassParameters->PS.HZBMipLevel = FMath::Max<float>((int32)FMath::FloorLog2(GridPixelsPerCellXY) - 1, 0.0f);
		PassParameters->PS.HaveClosestHZB = View.ClosestHZB ? 1 : 0;
		PassParameters->PS.ViewportUVToHZBBufferUV = FVector2f(
			float(View.ViewRect.Width()) / float(2 * View.HZBMipmap0Size.X),
			float(View.ViewRect.Height()) / float(2 * View.HZBMipmap0Size.Y)
		);

		PassParameters->MeshSDFIndirectArgs = Context.ObjectIndirectArguments;
	}

	FMeshSDFObjectCullVS::FPermutationDomain PermutationVectorVS;
	PermutationVectorVS.Set< FMeshSDFObjectCullVS::FCullMeshTypeSDF >(DistanceFieldSceneData.NumObjectsInBuffer > 0);
	PermutationVectorVS.Set< FMeshSDFObjectCullVS::FCullMeshTypeHeightfield >(Lumen::UseHeightfieldTracing(*View.Family, *Scene->GetLumenSceneData(View)));
	auto VertexShader = View.ShaderMap->GetShader< FMeshSDFObjectCullVS >(PermutationVectorVS);

	FMeshSDFObjectCullPS::FPermutationDomain PermutationVectorPS;
	PermutationVectorPS.Set< FMeshSDFObjectCullPS::FCullToFroxelGrid >(GridSizeZ > 1);
	PermutationVectorPS.Set< FMeshSDFObjectCullPS::FCullMeshTypeSDF >(DistanceFieldSceneData.NumObjectsInBuffer > 0);
	PermutationVectorPS.Set< FMeshSDFObjectCullPS::FCullMeshTypeHeightfield >(Lumen::UseHeightfieldTracing(*View.Family, *Scene->GetLumenSceneData(View)));
	extern int32 GDistanceFieldOffsetDataStructure;
	PermutationVectorPS.Set< FMeshSDFObjectCullPS::FOffsetDataStructure >(GDistanceFieldOffsetDataStructure);
	PermutationVectorPS = FMeshSDFObjectCullPS::RemapPermutation(PermutationVectorPS);
	auto PixelShader = View.ShaderMap->GetShader<FMeshSDFObjectCullPS>(PermutationVectorPS);

	ClearUnusedGraphResources(VertexShader, &PassParameters->VS);
	ClearUnusedGraphResources(PixelShader, &PassParameters->PS);
	const bool bReverseCulling = View.bReverseCulling;
	GraphBuilder.AddPass(
		RDG_EVENT_NAME("ScatterMeshSDFsToGrid"),
		PassParameters,
		ERDGPassFlags::Raster | ERDGPassFlags::SkipRenderPass,
		[CullGridSize, bReverseCulling, VertexShader, PixelShader, PassParameters](FRHICommandList& RHICmdList)
		{
			FRHIRenderPassInfo RPInfo;
			RPInfo.ResolveRect.X1 = 0;
			RPInfo.ResolveRect.Y1 = 0;
			RPInfo.ResolveRect.X2 = CullGridSize.X;
			RPInfo.ResolveRect.Y2 = CullGridSize.Y;
			RHICmdList.BeginRenderPass(RPInfo, TEXT("ScatterMeshSDFsToGrid"));

			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			RHICmdList.SetViewport(0, 0, 0.0f, CullGridSize.X, CullGridSize.Y, 1.0f);

			// Render backfaces since camera may intersect
			GraphicsPSOInit.RasterizerState = bReverseCulling ? TStaticRasterizerState<FM_Solid, CM_CW>::GetRHI() : TStaticRasterizerState<FM_Solid, CM_CCW>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;

			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GetVertexDeclarationFVector4();
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();

			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), PassParameters->VS);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), PassParameters->PS);

			RHICmdList.SetStreamSource(0, StencilingGeometry::GLowPolyStencilSphereVertexBuffer.VertexBufferRHI, 0);

			RHICmdList.DrawIndexedPrimitiveIndirect(
				StencilingGeometry::GLowPolyStencilSphereIndexBuffer.IndexBufferRHI,
				PassParameters->MeshSDFIndirectArgs->GetIndirectRHICallBuffer(),
				0);

			RHICmdList.EndRenderPass();
		});
}

void CullMeshObjectsToViewGrid(
	const FViewInfo& View,
	const FScene* Scene,
	const FLumenSceneFrameTemporaries& FrameTemporaries,
	float MaxMeshSDFInfluenceRadius,
	float CardTraceEndDistanceFromCamera,
	int32 GridPixelsPerCellXY,
	int32 GridSizeZ,
	FVector ZParams,
	FRDGBuilder& GraphBuilder,
	FLumenMeshSDFGridParameters& OutGridParameters,
	ERDGPassFlags ComputePassFlags)
{
	LLM_SCOPE_BYTAG(Lumen);

	const FDistanceFieldSceneData& DistanceFieldSceneData = Scene->DistanceFieldSceneData;

	const FIntPoint CardGridSizeXY = FIntPoint::DivideAndRoundUp(View.ViewRect.Size(), GridPixelsPerCellXY);
	const FIntVector CullGridSize(CardGridSizeXY.X, CardGridSizeXY.Y, GridSizeZ);
	const uint32 NumCullGridCells = CullGridSize.X * CullGridSize.Y * CullGridSize.Z;

	uint32 MaxCullGridCells;

	{
		// Allocate buffers using scene render targets size so we won't reallocate every frame with dynamic resolution
		const FIntPoint BufferSize = View.GetSceneTexturesConfig().Extent;
		const FIntPoint MaxCardGridSizeXY = FIntPoint::DivideAndRoundUp(BufferSize, GridPixelsPerCellXY);
		MaxCullGridCells = MaxCardGridSizeXY.X * MaxCardGridSizeXY.Y * GridSizeZ;
		ensure(MaxCullGridCells >= NumCullGridCells);
	}

	RDG_EVENT_SCOPE(GraphBuilder, "MeshSDFCulling %ux%ux%u cells", CullGridSize.X, CullGridSize.Y, CullGridSize.Z);

	FObjectCullingContext Context;

	InitObjectCullingContext(
		GraphBuilder,
		MaxCullGridCells,
		Context);

	Context.ObjectIndirectArguments = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc<FRHIDrawIndexedIndirectParameters>(1), TEXT("Lumen.CulledObjectIndirectArguments"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(Context.ObjectIndirectArguments), 0);

	bool bCullMeshSDFObjects = DistanceFieldSceneData.NumObjectsInBuffer > 0;
	if (bCullMeshSDFObjects)
	{
		CullMeshSDFObjectsForView(
			GraphBuilder,
			Scene,
			View,
			MaxMeshSDFInfluenceRadius,
			CardTraceEndDistanceFromCamera,
			Context);
	}

	bool bCullHeightfieldObjects = Lumen::UseHeightfieldTracing(*View.Family, *Scene->GetLumenSceneData(View));
	if (bCullHeightfieldObjects)
	{
		CullHeightfieldObjectsForView(
			GraphBuilder,
			Scene,
			View,
			FrameTemporaries,
			MaxMeshSDFInfluenceRadius,
			CardTraceEndDistanceFromCamera,
			Context.NumHeightfieldCulledObjects,
			Context.HeightfieldObjectIndexBuffer,
			Context.ObjectIndirectArguments);
	}

	FRDGBufferRef NumGridCulledHeightfieldObjects = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), Context.NumCullGridCells), TEXT("Lumen.NumGridCulledHeightfieldObjects"));
	AddClearUAVPass(GraphBuilder, GraphBuilder.CreateUAV(NumGridCulledHeightfieldObjects, PF_R32_UINT), 0);

	if (bCullMeshSDFObjects || bCullHeightfieldObjects)
	{
		FRDGBufferRef CombinedObjectIndexBuffer;
		CombineObjectIndexBuffers(
			GraphBuilder,
			Scene,
			View,
			bCullMeshSDFObjects,
			bCullHeightfieldObjects,
			Context,
			CombinedObjectIndexBuffer);

		CullObjectsToGrid(
			View,
			Scene,
			FrameTemporaries,
			MaxMeshSDFInfluenceRadius,
			CardTraceEndDistanceFromCamera,
			GridPixelsPerCellXY,
			GridSizeZ,
			ZParams,
			CullGridSize,
			GraphBuilder,
			CombinedObjectIndexBuffer,
			Context);
	}

	CompactCulledObjectArray(
		GraphBuilder,
		Scene,
		View,
		Context,
		ComputePassFlags);

	FillGridParameters(
		GraphBuilder,
		Scene,
		View,
		&Context,
		OutGridParameters);
}
