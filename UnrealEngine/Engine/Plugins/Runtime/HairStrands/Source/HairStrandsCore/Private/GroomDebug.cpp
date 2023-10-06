// Copyright Epic Games, Inc. All Rights Reserved. 

#include "GeometryCacheComponent.h"
#include "GroomInstance.h"
#include "GroomManager.h"
#include "HairStrandsMeshProjection.h"
#include "HairStrandsInterface.h"
#include "CommonRenderResources.h"
#include "CachedGeometry.h"
#include "GroomGeometryCache.h"
#include "Components/SkeletalMeshComponent.h"
#include "RHIStaticStates.h"
#include "SkeletalRenderPublic.h"
#include "UnrealEngine.h"
#include "SystemTextures.h"
#include "CanvasTypes.h"
#include "ShaderCompilerCore.h"
#include "GroomVisualizationData.h"
#include "GroomCacheData.h"
#include "HairStrandsClusterCulling.h"
#include "ShaderPrintParameters.h"
#include "GroomComponent.h"

///////////////////////////////////////////////////////////////////////////////////////////////////

static int32 GHairDebugMeshProjection_SkinCacheMesh = 0;
static int32 GHairDebugMeshProjection_SkinCacheMeshInUVsSpace = 0;

static int32 GHairDebugMeshProjection_Sim_HairRestTriangles = 0;
static int32 GHairDebugMeshProjection_Sim_HairRestFrames = 0;
static int32 GHairDebugMeshProjection_Sim_HairRestSamples = 0;
static int32 GHairDebugMeshProjection_Sim_HairDeformedTriangles = 0;
static int32 GHairDebugMeshProjection_Sim_HairDeformedFrames = 0;
static int32 GHairDebugMeshProjection_Sim_HairDeformedSamples = 0;

static int32 GHairDebugMeshProjection_Render_HairRestTriangles = 0;
static int32 GHairDebugMeshProjection_Render_HairRestFrames = 0;
static int32 GHairDebugMeshProjection_Render_HairRestSamples = 0;
static int32 GHairDebugMeshProjection_Render_HairDeformedTriangles = 0;
static int32 GHairDebugMeshProjection_Render_HairDeformedFrames = 1;
static int32 GHairDebugMeshProjection_Render_HairDeformedSamples = 0;


static FAutoConsoleVariableRef CVarHairDebugMeshProjection_SkinCacheMeshInUVsSpace(TEXT("r.HairStrands.MeshProjection.DebugInUVsSpace"), GHairDebugMeshProjection_SkinCacheMeshInUVsSpace, TEXT("Render debug mes projection in UVs space"));
static FAutoConsoleVariableRef CVarHairDebugMeshProjection_SkinCacheMesh(TEXT("r.HairStrands.MeshProjection.DebugSkinCache"), GHairDebugMeshProjection_SkinCacheMesh, TEXT("Render the skin cache used in projection"));

static FAutoConsoleVariableRef CVarHairDebugMeshProjection_Render_HairRestTriangles(TEXT("r.HairStrands.MeshProjection.Render.Rest.Triangles"), GHairDebugMeshProjection_Render_HairRestTriangles, TEXT("Render strands rest triangles"));
static FAutoConsoleVariableRef CVarHairDebugMeshProjection_Render_HairRestFrames(TEXT("r.HairStrands.MeshProjection.Render.Rest.Frames"), GHairDebugMeshProjection_Render_HairRestFrames, TEXT("Render strands rest frames"));
static FAutoConsoleVariableRef CVarHairDebugMeshProjection_Render_HairRestSamples(TEXT("r.HairStrands.MeshProjection.Render.Rest.Samples"), GHairDebugMeshProjection_Render_HairRestSamples, TEXT("Render strands rest samples"));
static FAutoConsoleVariableRef CVarHairDebugMeshProjection_Render_HairDeformedTriangles(TEXT("r.HairStrands.MeshProjection.Render.Deformed.Triangles"), GHairDebugMeshProjection_Render_HairDeformedTriangles, TEXT("Render strands deformed triangles"));
static FAutoConsoleVariableRef CVarHairDebugMeshProjection_Render_HairDeformedFrames(TEXT("r.HairStrands.MeshProjection.Render.Deformed.Frames"), GHairDebugMeshProjection_Render_HairDeformedFrames, TEXT("Render strands deformed frames"));
static FAutoConsoleVariableRef CVarHairDebugMeshProjection_Render_HairDeformedSamples(TEXT("r.HairStrands.MeshProjection.Render.Deformed.Samples"), GHairDebugMeshProjection_Render_HairDeformedSamples, TEXT("Render strands deformed samples"));

static FAutoConsoleVariableRef CVarHairDebugMeshProjection_Sim_HairRestTriangles(TEXT("r.HairStrands.MeshProjection.Sim.Rest.Triangles"), GHairDebugMeshProjection_Sim_HairRestTriangles, TEXT("Render guides rest triangles"));
static FAutoConsoleVariableRef CVarHairDebugMeshProjection_Sim_HairRestFrames(TEXT("r.HairStrands.MeshProjection.Sim.Rest.Frames"), GHairDebugMeshProjection_Sim_HairRestFrames, TEXT("Render guides rest frames"));
static FAutoConsoleVariableRef CVarHairDebugMeshProjection_Sim_HairRestSamples(TEXT("r.HairStrands.MeshProjection.Sim.Rest.Samples"), GHairDebugMeshProjection_Sim_HairRestSamples, TEXT("Render guides rest samples"));
static FAutoConsoleVariableRef CVarHairDebugMeshProjection_Sim_HairDeformedTriangles(TEXT("r.HairStrands.MeshProjection.Sim.Deformed.Triangles"), GHairDebugMeshProjection_Sim_HairDeformedTriangles, TEXT("Render guides deformed triangles"));
static FAutoConsoleVariableRef CVarHairDebugMeshProjection_Sim_HairDeformedFrames(TEXT("r.HairStrands.MeshProjection.Sim.Deformed.Frames"), GHairDebugMeshProjection_Sim_HairDeformedFrames, TEXT("Render guides deformed frames"));
static FAutoConsoleVariableRef CVarHairDebugMeshProjection_Sim_HairDeformedSamples(TEXT("r.HairStrands.MeshProjection.Sim.Deformed.Samples"), GHairDebugMeshProjection_Sim_HairDeformedSamples, TEXT("Render guides deformed samples"));

static int32 GHairCardsAtlasDebug = 0;
static FAutoConsoleVariableRef CVarHairCardsAtlasDebug(TEXT("r.HairStrands.Cards.DebugAtlas"), GHairCardsAtlasDebug, TEXT("Draw debug hair cards atlas."));

///////////////////////////////////////////////////////////////////////////////////////////////////

FCachedGeometry GetCacheGeometryForHair(
	FRDGBuilder& GraphBuilder,
	FSceneInterface* Scene,
	FHairGroupInstance* Instance,
	FGlobalShaderMap* ShaderMap,
	const bool bOutputTriangleData);

static void GetGroomInterpolationData(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	FSceneInterface* Scene,
	const FHairStrandsInstances& Instances,
	const EHairStrandsProjectionMeshType MeshType,
	FHairStrandsProjectionMeshData::LOD& OutGeometries)
{
	for (FHairStrandsInstance* AbstractInstance : Instances)
	{
		FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);

		if (!Instance)
			continue;

		const FCachedGeometry CachedGeometry = GetCacheGeometryForHair(GraphBuilder, Scene, Instance, ShaderMap, true);
		if (CachedGeometry.Sections.Num() == 0)
			continue;

		if (MeshType == EHairStrandsProjectionMeshType::DeformedMesh || MeshType == EHairStrandsProjectionMeshType::RestMesh)
		{
			for (int32 SectionIndex = 0; SectionIndex < CachedGeometry.Sections.Num(); ++SectionIndex)
			{
				FHairStrandsProjectionMeshData::Section OutSection = ConvertMeshSection(CachedGeometry, SectionIndex);
				if (MeshType == EHairStrandsProjectionMeshType::RestMesh)
				{					
					// If the mesh has some mesh-tranferred data, we display that otherwise we use the rest data
					const int32 SectionLodIndex = CachedGeometry.Sections[SectionIndex].LODIndex;
					const bool bHasTransferData = SectionLodIndex < Instance->Debug.TransferredPositions.Num();
					if (bHasTransferData)
					{
						OutSection.PositionBuffer = Instance->Debug.TransferredPositions[SectionLodIndex].SRV;
					}
					else if (Instance->Debug.TargetMeshData.LODs.Num() > 0)
					{
						OutGeometries = Instance->Debug.TargetMeshData.LODs[0];
					}
				}
				OutGeometries.Sections.Add(OutSection);
			}
		}

		if (MeshType == EHairStrandsProjectionMeshType::TargetMesh && Instance->Debug.TargetMeshData.LODs.Num() > 0)
		{
			OutGeometries = Instance->Debug.TargetMeshData.LODs[0];
		}

		if (MeshType == EHairStrandsProjectionMeshType::SourceMesh && Instance->Debug.SourceMeshData.LODs.Num() > 0)
		{
			OutGeometries = Instance->Debug.SourceMeshData.LODs[0];
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class FHairProjectionMeshDebugCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHairProjectionMeshDebugCS);
	SHADER_USE_PARAMETER_STRUCT(FHairProjectionMeshDebugCS, FGlobalShader);

	class FInputType : SHADER_PERMUTATION_INT("PERMUTATION_INPUT_TYPE", 2);
	using FPermutationDomain = TShaderPermutationDomain<FInputType>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		SHADER_PARAMETER(uint32, VertexOffset)
		SHADER_PARAMETER(uint32, IndexOffset)
		SHADER_PARAMETER(uint32, MaxIndexCount)
		SHADER_PARAMETER(uint32, MaxVertexCount)
		SHADER_PARAMETER(uint32, MeshUVsChannelOffset)
		SHADER_PARAMETER(uint32, MeshUVsChannelCount)
		SHADER_PARAMETER(uint32, bOutputInUVsSpace)
		SHADER_PARAMETER(uint32, MeshType)
		SHADER_PARAMETER(uint32, SectionIndex)
		SHADER_PARAMETER(FVector2f, OutputResolution)
		SHADER_PARAMETER_SRV(StructuredBuffer, InputIndexBuffer)
		SHADER_PARAMETER_SRV(StructuredBuffer, InputVertexPositionBuffer)
		SHADER_PARAMETER_SRV(StructuredBuffer, InputVertexUVsBuffer)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsHairStrandsSupported(EHairStrandsShaderType::Tool, Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_MESH_PROJECTION_SKIN_CACHE"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairProjectionMeshDebugCS, "/Engine/Private/HairStrands/HairStrandsDebug.usf", "MainCS", SF_Compute);

static void AddDebugProjectionMeshPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const FShaderPrintData* ShaderPrintData,
	const FIntRect Viewport,
	const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
	const EHairStrandsProjectionMeshType MeshType,
	FHairStrandsProjectionMeshData::Section& MeshSectionData)
{
	// Force ShaderPrint on.
	ShaderPrint::SetEnabled(true);

	const bool bHasIndexBuffer = MeshSectionData.IndexBuffer != nullptr;
	const uint32 PrimitiveCount = MeshSectionData.NumPrimitives;

	if (!MeshSectionData.PositionBuffer || PrimitiveCount == 0)
		return;

	if (ShaderPrintData == nullptr || !ShaderPrint::IsEnabled(*ShaderPrintData))
	{
		return;
	}

	ShaderPrint::RequestSpaceForLines(PrimitiveCount);
	ShaderPrint::RequestSpaceForTriangles(PrimitiveCount);

	const FIntPoint Resolution(Viewport.Width(), Viewport.Height());

	FHairProjectionMeshDebugCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairProjectionMeshDebugCS::FParameters>();
	Parameters->LocalToWorld = FMatrix44f(MeshSectionData.LocalToWorld.ToMatrixWithScale());		// LWC_TODO: Precision loss
	Parameters->OutputResolution = Resolution;
	Parameters->MeshType = uint32(MeshType);
	Parameters->bOutputInUVsSpace = GHairDebugMeshProjection_SkinCacheMeshInUVsSpace ? 1 : 0;
	Parameters->VertexOffset = MeshSectionData.VertexBaseIndex;
	Parameters->IndexOffset = MeshSectionData.IndexBaseIndex;
	Parameters->MaxIndexCount = MeshSectionData.TotalIndexCount;
	Parameters->MaxVertexCount = MeshSectionData.TotalVertexCount;
	Parameters->MeshUVsChannelOffset = MeshSectionData.UVsChannelOffset;
	Parameters->MeshUVsChannelCount = MeshSectionData.UVsChannelCount;
	Parameters->InputIndexBuffer = MeshSectionData.IndexBuffer;
	Parameters->InputVertexPositionBuffer = MeshSectionData.PositionBuffer;
	Parameters->InputVertexUVsBuffer = MeshSectionData.UVsBuffer;
	Parameters->SectionIndex = MeshSectionData.SectionIndex;
	Parameters->ViewUniformBuffer = ViewUniformBuffer;
	ShaderPrint::SetParameters(GraphBuilder, *ShaderPrintData, Parameters->ShaderPrintUniformBuffer);

	FHairProjectionMeshDebugCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FHairProjectionMeshDebugCS::FInputType>(bHasIndexBuffer ? 1 : 0);
	TShaderMapRef<FHairProjectionMeshDebugCS> ComputeShader(ShaderMap, PermutationVector);
	ClearUnusedGraphResources(ComputeShader, Parameters);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("HairStrands::MeshProjectionDebug(Mesh)"),
		ComputeShader,
		Parameters,
		FIntVector(FMath::DivideAndRoundUp(PrimitiveCount, 256u), 1, 1));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class FHairProjectionHairDebugCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHairProjectionHairDebugCS);
	SHADER_USE_PARAMETER_STRUCT(FHairProjectionHairDebugCS, FGlobalShader);

	class FInputType : SHADER_PERMUTATION_INT("PERMUTATION_INPUT_TYPE", 3);
	using FPermutationDomain = TShaderPermutationDomain<FInputType>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector2f, OutputResolution)
		SHADER_PARAMETER(uint32, MaxRootCount)
		SHADER_PARAMETER(uint32, DeformedFrameEnable)
		SHADER_PARAMETER(FMatrix44f, RootLocalToWorld)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RestPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, DeformedPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RestSamplePositionsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, DeformedSamplePositionsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RootBarycentricBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RootToUniqueTriangleIndexBuffer)

		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsHairStrandsSupported(EHairStrandsShaderType::Tool, Parameters.Platform);
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_MESH_PROJECTION_HAIR"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairProjectionHairDebugCS, "/Engine/Private/HairStrands/HairStrandsDebug.usf", "MainCS", SF_Compute);

enum class EDebugProjectionHairType
{
	HairFrame,
	HairTriangle,
	HairSamples,
};

static void AddDebugProjectionHairPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const FShaderPrintData* ShaderPrintData,
	FIntRect Viewport,
	const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer,
	const int32 MeshLODIndex,
	const FHairStrandsRestRootResource* RestRootResources,
	const FHairStrandsDeformedRootResource* DeformedRootResources,
	const FTransform& LocalToWorld,
	const EDebugProjectionHairType GeometryType,
	const HairStrandsTriangleType PoseType)
{
	// Force ShaderPrint on.
	ShaderPrint::SetEnabled(true);

	if (MeshLODIndex < 0 || MeshLODIndex >= RestRootResources->LODs.Num() || MeshLODIndex >= DeformedRootResources->LODs.Num())
	{
		return;
	}
	
	const EPrimitiveType PrimitiveType = GeometryType == EDebugProjectionHairType::HairFrame ? PT_LineList : GeometryType == EDebugProjectionHairType::HairTriangle ? PT_TriangleList : PT_LineList;
	const uint32 RootCount = EDebugProjectionHairType::HairSamples == GeometryType ? 3 * RestRootResources->LODs[MeshLODIndex].SampleCount : RestRootResources->GetRootCount();
	const uint32 PrimitiveCount = RootCount;

	if (PrimitiveCount == 0)
	{
		return;
	}

	if (ShaderPrintData == nullptr || !ShaderPrint::IsEnabled(*ShaderPrintData))
	{
		return;
	}

	ShaderPrint::RequestSpaceForLines(PrimitiveCount * 4 /* 1 hair root + 3 triangles */);
	ShaderPrint::RequestSpaceForTriangles(PrimitiveCount * 3);

	if (EDebugProjectionHairType::HairFrame == GeometryType &&
		!RestRootResources->LODs[MeshLODIndex].RootBarycentricBuffer.Buffer)
	{
		return;
	}

	if (EDebugProjectionHairType::HairSamples == GeometryType &&
		!RestRootResources->LODs[MeshLODIndex].RestSamplePositionsBuffer.Buffer)
	{
			return;
	}

	const FHairStrandsRestRootResource::FLOD& RestLODDatas = RestRootResources->LODs[MeshLODIndex];
	const FHairStrandsDeformedRootResource::FLOD& DeformedLODDatas = DeformedRootResources->LODs[MeshLODIndex];

	if (!RestLODDatas.RestUniqueTrianglePositionBuffer.Buffer || !DeformedLODDatas.DeformedUniqueTrianglePositionBuffer[0].Buffer)
	{
		return;
	}

	// Double buffering is disabled by default unless the read-only cvar r.HairStrands.ContinuousDecimationReordering is set
	if (IsHairStrandContinuousDecimationReorderingEnabled() && !DeformedLODDatas.DeformedUniqueTrianglePositionBuffer[1].Buffer)
	{
		return;
	}

	const FIntPoint Resolution(Viewport.Width(), Viewport.Height());

	FHairProjectionHairDebugCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairProjectionHairDebugCS::FParameters>();
	Parameters->OutputResolution = Resolution;
	Parameters->MaxRootCount = RootCount;
	Parameters->RootLocalToWorld = FMatrix44f(LocalToWorld.ToMatrixWithScale());	// LWC_TODO: Precision loss
	Parameters->DeformedFrameEnable = PoseType == HairStrandsTriangleType::DeformedPose;

	if (EDebugProjectionHairType::HairFrame == GeometryType)
	{
		Parameters->RootBarycentricBuffer = RegisterAsSRV(GraphBuilder, RestLODDatas.RootBarycentricBuffer);
	}

	Parameters->RootToUniqueTriangleIndexBuffer = RegisterAsSRV(GraphBuilder, RestLODDatas.RootToUniqueTriangleIndexBuffer);

	Parameters->RestPositionBuffer = RegisterAsSRV(GraphBuilder, RestLODDatas.RestUniqueTrianglePositionBuffer);
	Parameters->DeformedPositionBuffer = RegisterAsSRV(GraphBuilder, DeformedLODDatas.GetDeformedUniqueTrianglePositionBuffer(FHairStrandsDeformedRootResource::FLOD::Current));

	Parameters->RestSamplePositionsBuffer = RegisterAsSRV(GraphBuilder, RestLODDatas.RestSamplePositionsBuffer);
	Parameters->DeformedSamplePositionsBuffer = RegisterAsSRV(GraphBuilder, DeformedLODDatas.GetDeformedSamplePositionsBuffer(FHairStrandsDeformedRootResource::FLOD::Current));

	Parameters->ViewUniformBuffer = ViewUniformBuffer;
	ShaderPrint::SetParameters(GraphBuilder, *ShaderPrintData, Parameters->ShaderPrintUniformBuffer);

	FHairProjectionHairDebugCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FHairProjectionHairDebugCS::FInputType>(GeometryType == EDebugProjectionHairType::HairFrame ? 0 : GeometryType == EDebugProjectionHairType::HairTriangle ? 1 : 2);

	TShaderMapRef<FHairProjectionHairDebugCS> ComputeShader(ShaderMap, PermutationVector);
	ClearUnusedGraphResources(ComputeShader, Parameters);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("HairStrands::MeshProjectionDebug(Hair, %s)", GeometryType == EDebugProjectionHairType::HairFrame ? TEXT("Frame") : GeometryType == EDebugProjectionHairType::HairTriangle ? TEXT("Triangle") : TEXT("Samples")),
		ComputeShader,
		Parameters,
		FIntVector(FMath::DivideAndRoundUp(PrimitiveCount, 256u), 1, 1));
}	

///////////////////////////////////////////////////////////////////////////////////////////////////

class FDrawDebugClusterAABBCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDrawDebugClusterAABBCS);
	SHADER_USE_PARAMETER_STRUCT(FDrawDebugClusterAABBCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters,)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_SRV(Buffer, ClusterAABBBuffer)
		SHADER_PARAMETER_SRV(Buffer, GroupAABBBuffer)
		SHADER_PARAMETER(uint32, ClusterCount)
		SHADER_PARAMETER(uint32, PointCount)
		SHADER_PARAMETER(uint32, CurveCount)
		SHADER_PARAMETER(uint32, HairGroupId)
		SHADER_PARAMETER(uint32, bDrawAABB)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintParameters)
		END_SHADER_PARAMETER_STRUCT()

public:
	static uint32 GetGroupSize() { return 64; }
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Tool, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_CLUSTERAABB"), 1);
		OutEnvironment.SetDefine(TEXT("GROUP_SIZE"), GetGroupSize());
		// Skip optimization for avoiding long compilation time due to large UAV writes
		OutEnvironment.CompilerFlags.Add(CFLAG_Debug);
	}
};

IMPLEMENT_GLOBAL_SHADER(FDrawDebugClusterAABBCS, "/Engine/Private/HairStrands/HairStrandsDebug.usf", "MainDrawDebugAABBCS", SF_Compute);

void AddDrawDebugClusterPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FGlobalShaderMap* ShaderMap,
	const FShaderPrintData* ShaderPrintData,
	EGroomViewMode ViewMode,
	FHairStrandClusterData& HairClusterData)
{
	ShaderPrint::SetEnabled(true);
	ShaderPrint::RequestSpaceForLines(64000u);
	ShaderPrint::RequestSpaceForCharacters(2000);
	if (!ShaderPrintData) { return; }

	const bool bDebugAABB = ViewMode == EGroomViewMode::ClusterAABB;

	uint32 DataIndex = 0;
	for (const FHairStrandClusterData::FHairGroup& HairGroupClusters : HairClusterData.HairGroups)
	{
		TShaderMapRef<FDrawDebugClusterAABBCS> ComputeShader(ShaderMap);

		FDrawDebugClusterAABBCS::FParameters* Parameters = GraphBuilder.AllocParameters<FDrawDebugClusterAABBCS::FParameters>();
		Parameters->ViewUniformBuffer = View.ViewUniformBuffer;
		Parameters->ClusterCount = HairGroupClusters.ClusterCount;
		Parameters->PointCount = HairGroupClusters.HairGroupPublicPtr->GetActiveStrandsPointCount();
		Parameters->CurveCount = HairGroupClusters.HairGroupPublicPtr->GetActiveStrandsCurveCount();
		Parameters->HairGroupId = DataIndex++;
		Parameters->bDrawAABB = ViewMode == EGroomViewMode::ClusterAABB ? 1 : 0;
		Parameters->ClusterAABBBuffer = HairGroupClusters.ClusterAABBBuffer->SRV;
		Parameters->GroupAABBBuffer = HairGroupClusters.GroupAABBBuffer->SRV;
		ShaderPrint::SetParameters(GraphBuilder, *ShaderPrintData, Parameters->ShaderPrintParameters);

		const FIntVector DispatchCount = DispatchCount.DivideAndRoundUp(FIntVector(Parameters->ClusterCount, 1, 1), FIntVector(FDrawDebugClusterAABBCS::GetGroupSize(), 1, 1));
		FComputeShaderUtils::AddPass(
			GraphBuilder, RDG_EVENT_NAME("HairStrands::DrawDebugClusterAABB"),
			ComputeShader, Parameters, DispatchCount);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class FDrawDebugCardAtlasCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDrawDebugCardAtlasCS);
	SHADER_USE_PARAMETER_STRUCT(FDrawDebugCardAtlasCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_TEXTURE(Texture2D, AtlasTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutputTexture)
		SHADER_PARAMETER(FIntPoint, OutputResolution)
		SHADER_PARAMETER(FIntPoint, AtlasResolution)
		SHADER_PARAMETER(int32, DebugMode)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearSampler)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintParameters)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Tool, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_CARDS_ATLAS"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FDrawDebugCardAtlasCS, "/Engine/Private/HairStrands/HairStrandsDebug.usf", "MainCS", SF_Compute);

static void AddDrawDebugCardsAtlasPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FGlobalShaderMap* ShaderMap,
	const FHairGroupInstance* Instance,
	const FShaderPrintData* ShaderPrintData,
	FRDGTextureRef SceneColorTexture)
{
	if (Instance->HairGroupPublicData->VFInput.GeometryType != EHairGeometryType::Cards || ShaderPrintData == nullptr)
	{
		return;
	}

	const int32 LODIndex = Instance->HairGroupPublicData->GetIntLODIndex();
	if (!Instance->Cards.IsValid(LODIndex))
	{
		return;
	}

	FTextureReferenceRHIRef AtlasTexture = nullptr;

	const int32 DebugMode = FMath::Clamp(GHairCardsAtlasDebug, 1, 6);
	switch (DebugMode)
	{
	case 1: AtlasTexture = Instance->Cards.LODs[LODIndex].RestResource->DepthTexture; break;
	case 2: AtlasTexture = Instance->Cards.LODs[LODIndex].RestResource->CoverageTexture; break;
	case 3: AtlasTexture = Instance->Cards.LODs[LODIndex].RestResource->TangentTexture; break;
	case 4:
	case 5:
	case 6: AtlasTexture = Instance->Cards.LODs[LODIndex].RestResource->AttributeTexture; break;
	}

	if (AtlasTexture != nullptr)
	{
		TShaderMapRef<FDrawDebugCardAtlasCS> ComputeShader(ShaderMap);

		FDrawDebugCardAtlasCS::FParameters* Parameters = GraphBuilder.AllocParameters<FDrawDebugCardAtlasCS::FParameters>();
		Parameters->ViewUniformBuffer = View.ViewUniformBuffer;
		Parameters->OutputResolution = SceneColorTexture->Desc.Extent;
		Parameters->AtlasResolution = FIntPoint(AtlasTexture->GetSizeXYZ().X, AtlasTexture->GetSizeXYZ().Y);
		Parameters->AtlasTexture = AtlasTexture;
		Parameters->DebugMode = DebugMode;
		Parameters->LinearSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
		Parameters->OutputTexture = GraphBuilder.CreateUAV(SceneColorTexture);

		ShaderPrint::SetParameters(GraphBuilder, *ShaderPrintData, Parameters->ShaderPrintParameters);

		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("HairStrands::DrawDebugCardsAtlas"), ComputeShader, Parameters,
		FIntVector::DivideAndRoundUp(FIntVector(Parameters->OutputResolution.X, Parameters->OutputResolution.Y, 1), FIntVector(8, 8, 1)));

	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class FDrawDebugStrandsCVsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDrawDebugStrandsCVsCS);
	SHADER_USE_PARAMETER_STRUCT(FDrawDebugStrandsCVsCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER(uint32, MaxVertexCount)
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearSampler)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, DepthTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, ColorTexture)
		SHADER_PARAMETER_STRUCT_REF(FHairStrandsVertexFactoryUniformShaderParameters, HairStrandsVF)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Tool, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_CVS"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FDrawDebugStrandsCVsCS, "/Engine/Private/HairStrands/HairStrandsDebug.usf", "MainCS", SF_Compute);

static void AddDrawDebugStrandsCVsPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FGlobalShaderMap* ShaderMap,
	const FHairGroupInstance* Instance,
	const FShaderPrintData* ShaderPrintData,
	FRDGTextureRef ColorTexture,
	FRDGTextureRef DepthTexture)
{
	if (Instance->HairGroupPublicData->VFInput.GeometryType != EHairGeometryType::Strands || ShaderPrintData == nullptr)
	{
		return;
	}

	if (!Instance->Strands.IsValid())
	{
		return;
	}

	TShaderMapRef<FDrawDebugStrandsCVsCS> ComputeShader(ShaderMap);
	FDrawDebugStrandsCVsCS::FParameters* Parameters = GraphBuilder.AllocParameters<FDrawDebugStrandsCVsCS::FParameters>();
	Parameters->ViewUniformBuffer = View.ViewUniformBuffer;
	Parameters->HairStrandsVF = Instance->Strands.UniformBuffer;
	Parameters->LocalToWorld = FMatrix44f(Instance->LocalToWorld.ToMatrixWithScale());		// LWC_TODO: Precision loss // TODO change this to Uniform buffer parameters..
	Parameters->MaxVertexCount = Instance->Strands.Data->GetNumPoints();
	Parameters->ColorTexture = GraphBuilder.CreateUAV(ColorTexture);
	Parameters->DepthTexture = DepthTexture;
	Parameters->LinearSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	const uint32 PointCount = Instance->HairGroupPublicData->VFInput.Strands.Common.PointCount;
	FComputeShaderUtils::AddPass(
		GraphBuilder, 
		RDG_EVENT_NAME("HairStrands::DrawCVs"), 
		ComputeShader, 
		Parameters,
		FIntVector::DivideAndRoundUp(FIntVector(PointCount, 1, 1), FIntVector(256, 1, 1)));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class FDrawDebugCardGuidesCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FDrawDebugCardGuidesCS);
	SHADER_USE_PARAMETER_STRUCT(FDrawDebugCardGuidesCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER(uint32,  DebugMode)
		SHADER_PARAMETER(FMatrix44f, LocalToWorld)
		
		SHADER_PARAMETER(uint32,  RenVertexCount)
		SHADER_PARAMETER(FVector3f, RenRestOffset)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RenDeformedOffset)
		
		SHADER_PARAMETER(uint32,  SimVertexCount)
		SHADER_PARAMETER(FVector3f, SimRestOffset)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, SimDeformedOffset)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RenRestPosition)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RenDeformedPosition)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, SimRestPosition)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, SimDeformedPosition)

		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintParameters)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Tool, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_CARDS_GUIDE"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FDrawDebugCardGuidesCS, "/Engine/Private/HairStrands/HairStrandsDebug.usf", "MainCS", SF_Compute);

static void AddDrawDebugCardsGuidesPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FGlobalShaderMap* ShaderMap,
	const FHairGroupInstance* Instance,
	const FShaderPrintData* ShaderPrintData,
	const bool bDeformed, 
	const bool bRen)
{
	// Force ShaderPrint on.
	ShaderPrint::SetEnabled(true);

	if (ShaderPrintData == nullptr || !ShaderPrint::IsEnabled(*ShaderPrintData))
	{
		return;
	}

	if (Instance->HairGroupPublicData->VFInput.GeometryType != EHairGeometryType::Cards)
	{
		return;
	}

	const int32 HairLODIndex = Instance->HairGroupPublicData->GetIntLODIndex();
	if (!Instance->Cards.IsValid(HairLODIndex))
	{
		return;
	}

	const FHairGroupInstance::FCards::FLOD& LOD = Instance->Cards.LODs[HairLODIndex];
	if (!LOD.Guides.Data)
	{
		return;
	}
	
	const uint32 MaxCount = FMath::Max(
		Instance->Guides.RestResource ? Instance->Guides.RestResource->GetPointCount() * 2 : 0,
		LOD.Guides.RestResource ? LOD.Guides.RestResource->GetPointCount() * 2 : 0);
	ShaderPrint::RequestSpaceForLines(MaxCount);

	TShaderMapRef<FDrawDebugCardGuidesCS> ComputeShader(ShaderMap);
	const bool bGuideValid			= Instance->Guides.RestResource != nullptr;
	const bool bGuideDeformValid	= Instance->Guides.DeformedResource != nullptr;
	const bool bRenderValid			= LOD.Guides.RestResource != nullptr;
	const bool bRenderDeformValid	= LOD.Guides.DeformedResource != nullptr;
	if (bRen  && !bRenderValid)						{ return; }
	if (bRen  && bDeformed && !bRenderDeformValid)	{ return; }
	if (!bRen && !bGuideValid)						{ return; }
	if (!bRen && bDeformed && !bGuideDeformValid)	{ return; }

	FRDGBufferSRVRef DefaultBuffer = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultBuffer(GraphBuilder, 8, 0u), PF_R16G16B16A16_UINT);

	FDrawDebugCardGuidesCS::FParameters* Parameters = GraphBuilder.AllocParameters<FDrawDebugCardGuidesCS::FParameters>();
	Parameters->ViewUniformBuffer = View.ViewUniformBuffer;

	Parameters->RenVertexCount = 0;
	Parameters->RenRestOffset = FVector3f::ZeroVector;
	Parameters->RenRestPosition = DefaultBuffer;
	Parameters->RenDeformedOffset = DefaultBuffer;
	Parameters->RenDeformedPosition = DefaultBuffer;

	Parameters->SimVertexCount = 0;
	Parameters->SimRestOffset = FVector3f::ZeroVector;
	Parameters->SimRestPosition = DefaultBuffer;
	Parameters->SimDeformedOffset = DefaultBuffer;
	Parameters->SimDeformedPosition = DefaultBuffer;

	if (bRen)
	{
		Parameters->RenVertexCount = LOD.Guides.RestResource->GetPointCount();
		Parameters->RenRestOffset = (FVector3f)LOD.Guides.RestResource->GetPositionOffset();
		Parameters->RenRestPosition = RegisterAsSRV(GraphBuilder, LOD.Guides.RestResource->PositionBuffer);
		if (bDeformed)
		{
			Parameters->RenDeformedOffset = RegisterAsSRV(GraphBuilder, LOD.Guides.DeformedResource->GetPositionOffsetBuffer(FHairStrandsDeformedResource::Current));
			Parameters->RenDeformedPosition = RegisterAsSRV(GraphBuilder, LOD.Guides.DeformedResource->GetBuffer(FHairStrandsDeformedResource::Current));
		}
	}

	if (!bRen)
	{
		Parameters->SimVertexCount = Instance->Guides.RestResource->GetPointCount();
		Parameters->SimRestOffset = (FVector3f)Instance->Guides.RestResource->GetPositionOffset();
		Parameters->SimRestPosition = RegisterAsSRV(GraphBuilder, Instance->Guides.RestResource->PositionBuffer);
		if (bDeformed)
		{
			Parameters->SimDeformedOffset = RegisterAsSRV(GraphBuilder, Instance->Guides.DeformedResource->GetPositionOffsetBuffer(FHairStrandsDeformedResource::Current));
			Parameters->SimDeformedPosition = RegisterAsSRV(GraphBuilder, Instance->Guides.DeformedResource->GetBuffer(FHairStrandsDeformedResource::Current));
		}
	}

	Parameters->LocalToWorld = FMatrix44f(Instance->LocalToWorld.ToMatrixWithScale());		// LWC_TODO: Precision loss

	const TCHAR* DebugName = TEXT("Unknown");
	if (!bDeformed &&  bRen) { Parameters->DebugMode = 1; DebugName = TEXT("Ren, Rest"); } 
	if ( bDeformed &&  bRen) { Parameters->DebugMode = 2; DebugName = TEXT("Ren, Deformed"); } 
	if (!bDeformed && !bRen) { Parameters->DebugMode = 3; DebugName = TEXT("Sim, Rest"); } 
	if ( bDeformed && !bRen) { Parameters->DebugMode = 4; DebugName = TEXT("Sim, Deformed"); } 

	ShaderPrint::SetParameters(GraphBuilder, *ShaderPrintData, Parameters->ShaderPrintParameters);

	const uint32 VertexCount = Parameters->DebugMode <= 2 ? Parameters->RenVertexCount : Parameters->SimVertexCount;
	FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("HairStrands::DebugCards(%s)", DebugName), ComputeShader, Parameters,
	FIntVector::DivideAndRoundUp(FIntVector(VertexCount, 1, 1), FIntVector(32, 1, 1)));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

BEGIN_SHADER_PARAMETER_STRUCT(FHairDebugCanvasParameter, )
	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, View)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

static const TCHAR* ToString(EHairGeometryType In)
{
	switch (In)
	{
	case EHairGeometryType::NoneGeometry:	return TEXT("None");
	case EHairGeometryType::Strands:		return TEXT("Strands");
	case EHairGeometryType::Cards:			return TEXT("Cards");
	case EHairGeometryType::Meshes:			return TEXT("Meshes");
	}
	return TEXT("None");
}

static const TCHAR* ToString(EHairBindingType In)
{
	switch (In)
	{
	case EHairBindingType::NoneBinding: return TEXT("None");
	case EHairBindingType::Rigid:		return TEXT("Rigid");
	case EHairBindingType::Skinning:	return TEXT("Skinning");
	}
	return TEXT("None");
}

static const TCHAR* ToString(EHairLODSelectionType In)
{
	switch (In)
	{
	case EHairLODSelectionType::Immediate:	return TEXT("Immed");
	case EHairLODSelectionType::Predicted:	return TEXT("Predic");
	case EHairLODSelectionType::Forced:		return TEXT("Forced");
	}
	return TEXT("None");
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class FHairDebugPrintInstanceCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHairDebugPrintInstanceCS);
	SHADER_USE_PARAMETER_STRUCT(FHairDebugPrintInstanceCS, FGlobalShader);

	class FOutputType : SHADER_PERMUTATION_INT("PERMUTATION_OUTPUT_TYPE", 2);
	using FPermutationDomain = TShaderPermutationDomain<FOutputType>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, InstanceCount)

		SHADER_PARAMETER(uint32, InstanceCount_StrandsPrimaryView)
		SHADER_PARAMETER(uint32, InstanceCount_StrandsShadowView)
		SHADER_PARAMETER(uint32, InstanceCount_CardsOrMeshesPrimaryView)
		SHADER_PARAMETER(uint32, InstanceCount_CardsOrMeshesShadowView)
		SHADER_PARAMETER(FVector4f, InstanceScreenSphereBound)
		SHADER_PARAMETER_STRUCT(ShaderPrint::FStrings::FShaderParameters, InstanceNames)
		SHADER_PARAMETER_STRUCT(ShaderPrint::FStrings::FShaderParameters, AttributeNames)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint4>, Infos)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<int>, InstanceAABB)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Tool, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Skip optimization for avoiding long compilation time due to large UAV writes
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Debug);
		OutEnvironment.SetDefine(TEXT("SHADER_PRINT_INSTANCE"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairDebugPrintInstanceCS, "/Engine/Private/HairStrands/HairStrandsDebug.usf", "MainCS", SF_Compute);

bool IsHairStrandsForceAutoLODEnabled();
const TCHAR* GetHairAttributeText(EHairAttribute In, uint32 InFlags);
uint32 GetHairAttributeIndex(EHairAttribute In);
EGroomCacheType GetHairInstanceCacheType(const FHairGroupInstance* Instance);

static void AddHairDebugPrintInstancePass(
	FRDGBuilder& GraphBuilder, 
	FGlobalShaderMap* ShaderMap,
	const FSceneView& View,
	const FShaderPrintData* ShaderPrintData,
	const FHairStrandsInstances& Instances,
	const FUintVector4& InstanceCountPerType)
{
	const uint32 InstanceCount = Instances.Num();

	// Force ShaderPrint on.
	ShaderPrint::SetEnabled(true);
	// Request more drawing primitives & characters for printing if needed	
	ShaderPrint::RequestSpaceForLines(InstanceCount * 16u);
	ShaderPrint::RequestSpaceForCharacters(InstanceCount * 256 + 512);

	if (!ShaderPrintData || InstanceCount == 0) { return; }


	ShaderPrint::FStrings InstanceNames(InstanceCount);
	struct FInstanceInfos
	{
		FUintVector4 Data0 = { 0,0,0,0 };
		FUintVector4 Data1 = { 0,0,0,0 };
		FUintVector4 Data2 = { 0,0,0,0 };
		FUintVector4 Data3 = { 0,0,0,0 };
		FUintVector4 Data4 = { 0,0,0,0 };
	};
	TArray<FInstanceInfos> Infos;
	Infos.Reserve(InstanceCount);
	for (uint32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
	{
		const FHairGroupInstance* Instance = static_cast<const FHairGroupInstance*>(Instances[InstanceIndex]);

		// Collect InstanceNames
		InstanceNames.Add(*Instance->Debug.GroomAssetName, InstanceIndex);

		const float LODIndex = Instance->HairGroupPublicData->LODIndex;
		const uint32 IntLODIndex = Instance->HairGroupPublicData->LODIndex;
		const uint32 LODCount = Instance->HairGroupPublicData->GetLODScreenSizes().Num();
		const EGroomCacheType ActiveGroomCacheType = GetHairInstanceCacheType(Instance);

		FInstanceInfos& D = Infos.AddDefaulted_GetRef();
		D.Data0.X =
			((Instance->Debug.GroupIndex & 0xFF)) |
			((Instance->Debug.GroupCount & 0xFF) << 8) |
			((LODCount & 0xFF) << 16) |
			((uint32(Instance->GeometryType) & 0x7)<<24) |
			((uint32(Instance->BindingType) & 0x7)<<27) |
			((Instance->Guides.bIsSimulationEnable ? 0x1 : 0x0) << 30) |
			((Instance->Guides.bHasGlobalInterpolation ? 0x1 : 0x0) << 31);

		D.Data0.Y =
			(FFloat16(LODIndex).Encoded) |
			(FFloat16(Instance->Strands.Modifier.HairLengthScale_Override ? Instance->Strands.Modifier.HairLengthScale : -1.f).Encoded << 16);
		
		bool bHasRaytracing = false;
		switch (Instance->GeometryType) 
		{
		case EHairGeometryType::Strands:
			if (Instance->Strands.IsValid())
			{
				check(Instance->Strands.Data);
				D.Data0.Z = Instance->Strands.Data->GetNumCurves(); // Change this later on for having dynamic value
				D.Data0.W = Instance->Strands.Data->GetNumPoints(); // Change this later on for having dynamic value
				const int32 MeshLODIndex = Instance->HairGroupPublicData->MeshLODIndex;
				if (MeshLODIndex>=0 && Instance->Strands.RestRootResource)
				{
					D.Data1.X = Instance->Strands.RestRootResource->BulkData.Header.LODs[MeshLODIndex].UniqueSectionIndices.Num();
					D.Data1.Y = Instance->Strands.RestRootResource->BulkData.Header.LODs[MeshLODIndex].UniqueTriangleCount;
					D.Data1.Z = Instance->Strands.RestRootResource->BulkData.Header.RootCount;
					D.Data1.W = Instance->Strands.RestRootResource->BulkData.Header.PointCount;
				}

				{
					D.Data2 = FUintVector4(0);
					D.Data2.X |= InstanceIndex < InstanceCountPerType[uint32(EHairInstanceCount::StrandsPrimaryView)]  ? 0x1u  : 0u;
					D.Data2.X |= InstanceIndex < InstanceCountPerType[uint32(EHairInstanceCount::StrandsShadowView)]   ? 0x2u  : 0u;
					D.Data2.X |= Instance->HairGroupPublicData->VFInput.Strands.Common.bScatterSceneLighting ? 0x4u  : 0u;
					D.Data2.X |= Instance->HairGroupPublicData->VFInput.Strands.Common.bRaytracingGeometry   ? 0x8u  : 0u;
					D.Data2.X |= Instance->HairGroupPublicData->VFInput.Strands.Common.bStableRasterization  ? 0x10u : 0u;
					D.Data2.X |= Instance->HairGroupPublicData->bSupportVoxelization                         ? 0x20u : 0u;
					D.Data2.X |= ActiveGroomCacheType == EGroomCacheType::Guides                             ? 0x40u : 0u;
					D.Data2.X |= ActiveGroomCacheType == EGroomCacheType::Strands                            ? 0x80u : 0u;
					D.Data2.X |= uint32(FFloat16(Instance->HairGroupPublicData->ContinuousLODScreenSize).Encoded) << 16u;

					D.Data2.Y = Instance->HairGroupPublicData->GetActiveStrandsPointCount();
					D.Data2.Z = Instance->HairGroupPublicData->GetActiveStrandsCurveCount();

					D.Data2.W = Instance->Strands.Data->Header.ImportedAttributes;
				}
				
				D.Data3 = FUintVector4(0);
				D.Data3.X |= FFloat16(Instance->HairGroupPublicData->VFInput.Strands.Common.Radius).Encoded;
				D.Data3.X |= FFloat16(Instance->HairGroupPublicData->VFInput.Strands.Common.Density).Encoded << 16u;

				D.Data3.Y |= FFloat16(Instance->HairGroupPublicData->VFInput.Strands.Common.RootScale).Encoded;
				D.Data3.Y |= FFloat16(Instance->HairGroupPublicData->VFInput.Strands.Common.TipScale).Encoded << 16u;

				D.Data3.Z |= FFloat16(Instance->HairGroupPublicData->VFInput.Strands.Common.Length).Encoded;
				D.Data3.Z |= FFloat16(Instance->HairGroupPublicData->VFInput.Strands.Common.LengthScale).Encoded << 16u;
				D.Data3.W |= FFloat16(Instance->HairGroupPublicData->GetActiveStrandsCoverageScale()).Encoded;
				D.Data3.W |= Instance->HairGroupPublicData->bAutoLOD || IsHairStrandsForceAutoLODEnabled() ? (1u << 16u) : 0;

				#if RHI_RAYTRACING
				bHasRaytracing = Instance->Strands.RenRaytracingResource != nullptr;
				#endif
			}
			break;
		case EHairGeometryType::Cards:
			if (Instance->Cards.IsValid(IntLODIndex))
			{
				D.Data0.Z = Instance->Cards.LODs[IntLODIndex].Guides.IsValid() ? Instance->Cards.LODs[IntLODIndex].Guides.Data->GetNumCurves() : 0;
				D.Data0.W = Instance->Cards.LODs[IntLODIndex].Data->GetNumVertices();

				D.Data2 = FUintVector4(0);
				D.Data2.X |= InstanceIndex < InstanceCountPerType[uint32(EHairInstanceCount::CardsOrMeshesPrimaryView)] ? 0x1u : 0u;
				D.Data2.X |= InstanceIndex < InstanceCountPerType[uint32(EHairInstanceCount::CardsOrMeshesShadowView)]  ? 0x2u : 0u;

				#if RHI_RAYTRACING
				bHasRaytracing = Instance->Cards.LODs[IntLODIndex].RaytracingResource != nullptr;
				#endif
			}
			break;
		case EHairGeometryType::Meshes:
			if (Instance->Meshes.IsValid(IntLODIndex))
			{
				D.Data0.Z = 0;
				D.Data0.W = Instance->Meshes.LODs[IntLODIndex].Data->GetNumVertices();

				D.Data2 = FUintVector4(0);
				D.Data2.X |= InstanceIndex < InstanceCountPerType[uint32(EHairInstanceCount::CardsOrMeshesPrimaryView)] ? 0x1u : 0u;
				D.Data2.X |= InstanceIndex < InstanceCountPerType[uint32(EHairInstanceCount::CardsOrMeshesShadowView)]  ? 0x2u : 0u;

				#if RHI_RAYTRACING
				bHasRaytracing = Instance->Meshes.LODs[IntLODIndex].RaytracingResource != nullptr;
				#endif
			}
			break;
		}

		D.Data4.X = 0;
		D.Data4.X |= Instance->HairGroupPublicData->VFInput.bHasLODSwitch ? 1u : 0u;
		D.Data4.X |= bHasRaytracing ? 2u : 0u;
	}

	ShaderPrint::FStrings AttributeNames;
	for (uint32 AttributeIt = 0; AttributeIt < uint32(EHairAttribute::Count); ++AttributeIt)
	{
		// Only add valid optional attribute 
		// HAIR_ATTRIBUTE_XXX and EHairAttribute don't have a 1:1 mapping
		const EHairAttribute Attribute = (EHairAttribute)AttributeIt;
		AttributeNames.Add(GetHairAttributeText(Attribute, 0u), AttributeIt);
	}

	const uint32 InfoInBytes  = sizeof(FInstanceInfos);
	const uint32 InfoInUints = sizeof(FInstanceInfos) / sizeof(uint32);
	FRDGBufferRef InfoBuffer = CreateVertexBuffer(GraphBuilder, TEXT("Hair.Debug.InstanceInfos"), FRDGBufferDesc::CreateBufferDesc(4, InfoInUints * Infos.Num()), Infos.GetData(), InfoInBytes * Infos.Num());

	// Draw general information for all instances (one pass for all instances)
	{
		FHairDebugPrintInstanceCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairDebugPrintInstanceCS::FParameters>();
		Parameters->InstanceCount = InstanceCount;
		Parameters->InstanceCount_StrandsPrimaryView = InstanceCountPerType[uint32(EHairInstanceCount::StrandsPrimaryView)];
		Parameters->InstanceCount_StrandsShadowView = InstanceCountPerType[uint32(EHairInstanceCount::StrandsShadowView)];
		Parameters->InstanceCount_CardsOrMeshesPrimaryView = InstanceCountPerType[uint32(EHairInstanceCount::CardsOrMeshesPrimaryView)];
		Parameters->InstanceCount_CardsOrMeshesShadowView = InstanceCountPerType[uint32(EHairInstanceCount::CardsOrMeshesShadowView)];
		Parameters->InstanceNames = InstanceNames.GetParameters(GraphBuilder);
		Parameters->AttributeNames = AttributeNames.GetParameters(GraphBuilder);
		Parameters->Infos = GraphBuilder.CreateSRV(InfoBuffer, PF_R32_UINT);
		ShaderPrint::SetParameters(GraphBuilder, *ShaderPrintData, Parameters->ShaderPrintUniformBuffer);
		FHairDebugPrintInstanceCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FHairDebugPrintInstanceCS::FOutputType>(0);
		TShaderMapRef<FHairDebugPrintInstanceCS> ComputeShader(ShaderMap, PermutationVector);

		ClearUnusedGraphResources(ComputeShader, Parameters);

		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("HairStrands::DebugPrintInstance(Info,Instances:%d)", InstanceCount),
			ComputeShader,
			Parameters,
			FIntVector(1, 1, 1));
	}

	// Draw instances bound (one pass for each instance, due to separate AABB resources)
	FHairDebugPrintInstanceCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FHairDebugPrintInstanceCS::FOutputType>(1);
	TShaderMapRef<FHairDebugPrintInstanceCS> ComputeShader(ShaderMap, PermutationVector);
	for (uint32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
	{
		const FHairStrandsInstance* AbstractInstance = Instances[InstanceIndex];
		const FHairGroupInstance* Instance = static_cast<const FHairGroupInstance*>(AbstractInstance);

		if (Instance->GeometryType == EHairGeometryType::Strands)
		{
			const float MaxRectSizeInPixels = FMath::Min(View.UnscaledViewRect.Height(), View.UnscaledViewRect.Width());
			const float ContinousLODRadius = Instance->HairGroupPublicData->ContinuousLODScreenSize * MaxRectSizeInPixels * 0.5f; // Diameter->Radius

			FHairDebugPrintInstanceCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairDebugPrintInstanceCS::FParameters>();
			Parameters->InstanceAABB = Register(GraphBuilder, Instance->HairGroupPublicData->GetGroupAABBBuffer(), ERDGImportedBufferFlags::CreateSRV).SRV;
			Parameters->InstanceScreenSphereBound = FVector4f(Instance->HairGroupPublicData->ContinuousLODScreenPos.X, Instance->HairGroupPublicData->ContinuousLODScreenPos.Y, 0.f, ContinousLODRadius);
			ShaderPrint::SetParameters(GraphBuilder, *ShaderPrintData, Parameters->ShaderPrintUniformBuffer);
			ClearUnusedGraphResources(ComputeShader, Parameters);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("HairStrands::DebugPrintInstance(Bound)", InstanceCount),
				ComputeShader,
				Parameters,
				FIntVector(1, 1, 1));
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class FHairDebugPrintMemoryCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FHairDebugPrintMemoryCS);
	SHADER_USE_PARAMETER_STRUCT(FHairDebugPrintMemoryCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, ComponentCount)
		SHADER_PARAMETER(uint32, GroomCount)
		SHADER_PARAMETER(uint32, BindingCount)
		SHADER_PARAMETER_STRUCT(ShaderPrint::FStrings::FShaderParameters, ComponentNames)
		SHADER_PARAMETER_STRUCT(ShaderPrint::FStrings::FShaderParameters, GroomNames)
		SHADER_PARAMETER_STRUCT(ShaderPrint::FStrings::FShaderParameters, BindingNames)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, ComponentBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, GroomBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, BindingBuffer)
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintUniformBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Tool, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		// Skip optimization for avoiding long compilation time due to large UAV writes
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.CompilerFlags.Add(CFLAG_Debug);
		OutEnvironment.SetDefine(TEXT("SHADER_PRINT_MEMORY"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairDebugPrintMemoryCS, "/Engine/Private/HairStrands/HairStrandsDebug.usf", "MainCS", SF_Compute);

const TCHAR* GetHairAttributeText(EHairAttribute In, uint32 InFlags);
uint32 GetHairAttributeIndex(EHairAttribute In);
EGroomCacheType GetHairInstanceCacheType(const FHairGroupInstance* Instance);

static void AddHairDebugPrintMemoryPass(
	FRDGBuilder& GraphBuilder, 
	FGlobalShaderMap* ShaderMap,
	const FSceneView& View,
	const FShaderPrintData* ShaderPrintData)
{
	ShaderPrint::SetEnabled(true);
	
	if (!ShaderPrintData) { return; }

	struct FInfos
	{
		FUintVector4 Data0 = { 0,0,0,0 };
		FUintVector4 Data1 = { 0,0,0,0 };
		FUintVector4 Data2 = { 0,0,0,0 };
		FUintVector4 Data3 = { 0,0,0,0 };
		FUintVector4 Data4 = { 0,0,0,0 };
	};
	const uint32 InfoInBytes  = sizeof(FInfos);
	const uint32 InfoInUints = sizeof(FInfos) / sizeof(uint32);

	// Component
	uint32 ComponentCount = 0;
	for (TObjectIterator<UGroomComponent> ComponentIt; ComponentIt; ++ComponentIt)
	{
		if (ComponentIt)
		{
			ComponentCount += ComponentIt->GetGroupCount();
		}
	}
	ShaderPrint::FStrings ComponentNames(ComponentCount);
	TArray<FInfos> ComponentBuffer;
	ComponentBuffer.Reserve(ComponentCount);
	for (TObjectIterator<UGroomComponent> ComponentIt; ComponentIt; ++ComponentIt)
	{
		if (ComponentIt)
		{
			const uint32 GroupCount = ComponentIt->GetGroupCount();
			for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
			{
				const FHairGroupInstance* Instance = ComponentIt->GetGroupInstance(GroupIt);
				const FGroomComponentMemoryStats MemoryStats = FGroomComponentMemoryStats::Get(Instance);
		
				ComponentNames.Add(*Instance->Debug.GroomAssetName);
		
				FInfos& D = ComponentBuffer.AddDefaulted_GetRef();
				D.Data0.X = Instance->Debug.GroupIndex;
				D.Data0.Y = Instance->Debug.GroupCount;
				D.Data0.Z = Instance->HairGroupPublicData->GetActiveStrandsCurveCount();
				D.Data0.W = Instance->Strands.RestResource ? Instance->Strands.RestResource->BulkData.Header.CurveCount : 0;
		
				D.Data1.X = MemoryStats.Guides;
				D.Data1.Y = MemoryStats.Strands;
				D.Data1.Z = MemoryStats.Cards;
				D.Data1.W = MemoryStats.Meshes;
		
				D.Data2.X = MemoryStats.Guides;
				D.Data2.Y = MemoryStats.Strands;
				D.Data2.Z = MemoryStats.Cards;
				D.Data2.W = MemoryStats.Meshes;

				D.Data3.X = 0;
				D.Data3.Y = 0;
				D.Data3.Z = 0;
				D.Data3.W = 0;
			}
		}
	}

	// Groom
	uint32 GroomCount = 0;
	for (TObjectIterator<UGroomAsset> AssetIt; AssetIt; ++AssetIt)
	{
		if (AssetIt)
		{
			GroomCount += AssetIt->GetHairGroupsPlatformData().Num();
		}
	}
	ShaderPrint::FStrings GroomNames(GroomCount);
	TArray<FInfos> GroomBuffer;
	GroomBuffer.Reserve(GroomCount);
	for (uint32 It = 0; It < GroomCount; ++It)
	{
		for (TObjectIterator<UGroomAsset> AssetIt; AssetIt; ++AssetIt)
		{
			if (AssetIt)
			{
				const uint32 GroupCount = AssetIt->GetHairGroupsPlatformData().Num();
				for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
				{					
					const FHairGroupPlatformData& Data = AssetIt->GetHairGroupsPlatformData()[GroupIt];
					const FGroomAssetMemoryStats MemoryStats = FGroomAssetMemoryStats::Get(Data);

					GroomNames.Add(AssetIt->GetName());

					FInfos& D = GroomBuffer.AddDefaulted_GetRef();
					D.Data0.X = GroupIt;
					D.Data0.Y = GroupCount;
					D.Data0.Z = Data.Strands.RestResource ? Data.Strands.RestResource->MaxAvailableCurveCount : 0;
					D.Data0.W = Data.Strands.BulkData.IsValid() ? Data.Strands.BulkData.GetNumCurves() : 0;
			
					D.Data1.X = MemoryStats.CPU.Guides;
					D.Data1.Y = MemoryStats.CPU.Strands;
					D.Data1.Z = MemoryStats.CPU.Cards;
					D.Data1.W = MemoryStats.CPU.Meshes;
			
					D.Data2.X = MemoryStats.GPU.Guides;
					D.Data2.Y = MemoryStats.GPU.Strands;
					D.Data2.Z = MemoryStats.GPU.Cards;
					D.Data2.W = MemoryStats.GPU.Meshes;

					D.Data3.X = MemoryStats.Memory.Rest;
					D.Data3.Y = MemoryStats.Memory.Interpolation;
					D.Data3.Z = MemoryStats.Memory.Cluster;
					D.Data3.W = MemoryStats.Memory.Raytracing;

					D.Data4.X = MemoryStats.Curves.Rest;
					D.Data4.Y = MemoryStats.Curves.Interpolation;
					D.Data4.Z = MemoryStats.Curves.Cluster;
					D.Data4.W = MemoryStats.Curves.Raytracing;
				}
			}
		}
	}

	// Binding
	uint32 BindingCount = 0;
	for (TObjectIterator<UGroomBindingAsset> AssetIt; AssetIt; ++AssetIt)
	{
		if (AssetIt)
		{
			BindingCount += AssetIt->GetHairGroupsPlatformData().Num();
		}
	}
	ShaderPrint::FStrings BindingNames(BindingCount);
	TArray<FInfos> BindingBuffer;
	BindingBuffer.Reserve(BindingCount);
	for (uint32 It = 0; It < BindingCount; ++It)
	{
		for (TObjectIterator<UGroomBindingAsset> AssetIt; AssetIt; ++AssetIt)
		{
			if (AssetIt)
			{
				const uint32 GroupCount = AssetIt->GetHairGroupsPlatformData().Num();
				for (uint32 GroupIt = 0; GroupIt < GroupCount; ++GroupIt)
				{
					const UGroomBindingAsset::FHairGroupPlatformData& Data = AssetIt->GetHairGroupsPlatformData()[GroupIt];
					const UGroomBindingAsset::FHairGroupResource& Resource = AssetIt->GetHairGroupResources()[GroupIt];
					const FGroomBindingAssetMemoryStats MemoryStats = FGroomBindingAssetMemoryStats::Get(Data, Resource);
			

					BindingNames.Add(*AssetIt->GetName(), It);
			
					FInfos& D = BindingBuffer.AddDefaulted_GetRef();
					D.Data0.X = GroupIt;
					D.Data0.Y = GroupCount;
					D.Data0.Z = Resource.RenRootResources ? Resource.RenRootResources->MaxAvailableCurveCount : 0;
					D.Data0.W = Resource.RenRootResources ? Resource.RenRootResources->GetRootCount() : 0;
			
					D.Data1.X = MemoryStats.CPU.Guides;
					D.Data1.Y = MemoryStats.CPU.Strands;
					D.Data1.Z = MemoryStats.CPU.Cards;
					D.Data1.W = 0;
			
					D.Data2.X = MemoryStats.GPU.Guides;
					D.Data2.Y = MemoryStats.GPU.Strands;
					D.Data2.Z = MemoryStats.GPU.Cards;
					D.Data2.W = 0;
				}
			}
		}
	}

	// Empty cases
	if (ComponentCount == 0 && GroomCount == 0 && BindingCount == 0) return;
	if (ComponentBuffer.IsEmpty())	{ ComponentBuffer.AddDefaulted(); }
	if (GroomBuffer.IsEmpty())		{ GroomBuffer.AddDefaulted(); }
	if (BindingBuffer.IsEmpty())	{ BindingBuffer.AddDefaulted(); }

	// Force ShaderPrint on.
	ShaderPrint::RequestSpaceForCharacters((ComponentCount + GroomCount + BindingCount) * 256 + 512);

	FHairDebugPrintMemoryCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairDebugPrintMemoryCS::FParameters>();
	Parameters->ComponentCount  = ComponentCount;
	Parameters->ComponentNames  = ComponentNames.GetParameters(GraphBuilder);
	Parameters->ComponentBuffer = GraphBuilder.CreateSRV(CreateVertexBuffer(GraphBuilder, TEXT("Hair.Debug.Component"), FRDGBufferDesc::CreateBufferDesc(4, InfoInUints * ComponentBuffer.Num()), ComponentBuffer.GetData(), InfoInBytes * ComponentBuffer.Num()), PF_R32_UINT);

	Parameters->GroomCount		= GroomCount;
	Parameters->GroomNames  	= GroomNames.GetParameters(GraphBuilder);
	Parameters->GroomBuffer 	= GraphBuilder.CreateSRV(CreateVertexBuffer(GraphBuilder, TEXT("Hair.Debug.Groom"), FRDGBufferDesc::CreateBufferDesc(4, InfoInUints * GroomBuffer.Num()), GroomBuffer.GetData(), InfoInBytes * GroomBuffer.Num()), PF_R32_UINT);

	Parameters->BindingCount  	= BindingCount;
	Parameters->BindingNames  	= BindingNames.GetParameters(GraphBuilder);
	Parameters->BindingBuffer 	= GraphBuilder.CreateSRV(CreateVertexBuffer(GraphBuilder, TEXT("Hair.Debug.Binding"), FRDGBufferDesc::CreateBufferDesc(4, InfoInUints * BindingBuffer.Num()), BindingBuffer.GetData(), InfoInBytes * BindingBuffer.Num()), PF_R32_UINT);

	ShaderPrint::SetParameters(GraphBuilder, *ShaderPrintData, Parameters->ShaderPrintUniformBuffer);
	TShaderMapRef<FHairDebugPrintMemoryCS> ComputeShader(ShaderMap);

	ClearUnusedGraphResources(ComputeShader, Parameters);

	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("HairStrands::DebugMemory(Info,Instances:%d, Groom:%d, Binding:%d)", ComponentCount, GroomCount, BindingCount),
		ComputeShader,
		Parameters,
		FIntVector(1, 1, 1));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void RunHairStrandsDebug(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	FSceneInterface* Scene,
	const FSceneView& View,
	const FHairStrandsInstances& Instances,
	const FUintVector4& InstanceCountPerType,
	const FShaderPrintData* ShaderPrintData,
	FRDGTextureRef SceneColorTexture,
	FRDGTextureRef SceneDepthTexture,
	FIntRect Viewport,
	const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer)
{
	const EGroomViewMode ViewMode = GetGroomViewMode(View);

	if (ViewMode == EGroomViewMode::MacroGroups)
	{
		AddHairDebugPrintInstancePass(GraphBuilder, ShaderMap, View, ShaderPrintData, Instances, InstanceCountPerType);
	}

	if (ViewMode == EGroomViewMode::MeshProjection)
	{
		{
			if (GHairDebugMeshProjection_SkinCacheMesh > 0)
			{
				auto RenderMeshProjection = [ShaderMap, Scene, ShaderPrintData, Viewport, &ViewUniformBuffer, Instances, &GraphBuilder](FRDGBuilder& LocalGraphBuilder, EHairStrandsProjectionMeshType MeshType)
				{
					FHairStrandsProjectionMeshData::LOD MeshProjectionLODData;
					GetGroomInterpolationData(GraphBuilder, ShaderMap, Scene, Instances, MeshType, MeshProjectionLODData);
					for (FHairStrandsProjectionMeshData::Section& Section : MeshProjectionLODData.Sections)
					{
						AddDebugProjectionMeshPass(LocalGraphBuilder, ShaderMap, ShaderPrintData, Viewport, ViewUniformBuffer, MeshType, Section);
					}
				};

				RenderMeshProjection(GraphBuilder, EHairStrandsProjectionMeshType::DeformedMesh);
				RenderMeshProjection(GraphBuilder, EHairStrandsProjectionMeshType::RestMesh);
				RenderMeshProjection(GraphBuilder, EHairStrandsProjectionMeshType::SourceMesh);
				RenderMeshProjection(GraphBuilder, EHairStrandsProjectionMeshType::TargetMesh);
			}

			auto RenderProjectionData = [&GraphBuilder, ShaderMap, Viewport, &ViewUniformBuffer, Instances, ShaderPrintData](bool bGuide, bool bRestTriangle, bool bRestFrame, bool bRestSamples, bool bDeformedTriangle, bool bDeformedFrame, bool bDeformedSamples)
			{
				TArray<int32> HairLODIndices;
				for (FHairStrandsInstance* AbstractInstance : Instances)
				{
					FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);
					if (!Instance->HairGroupPublicData || Instance->BindingType != EHairBindingType::Skinning)
						continue;

					FHairStrandsRestRootResource* RestRootResource = nullptr;
					FHairStrandsDeformedRootResource* DeformedRootResource = nullptr;
					if (bGuide)
					{
						RestRootResource 	 = Instance->Guides.RestRootResource;
						DeformedRootResource = Instance->Guides.DeformedRootResource;
					}
					else if (Instance->GetHairGeometry() == EHairGeometryType::Strands)
					{
						RestRootResource 	 = Instance->Strands.RestRootResource;
						DeformedRootResource = Instance->Strands.DeformedRootResource;
					}
					else if (Instance->GetHairGeometry() == EHairGeometryType::Cards)
					{
						const int32 LODIndex = Instance->HairGroupPublicData->GetIntLODIndex();
						if (Instance->Cards.IsValid(LODIndex))
						{
							RestRootResource 	 = Instance->Cards.LODs[LODIndex].Guides.RestRootResource;
							DeformedRootResource = Instance->Cards.LODs[LODIndex].Guides.DeformedRootResource;
						}
					}
					if (RestRootResource == nullptr || DeformedRootResource == nullptr)
						continue;

					const int32 MeshLODIndex = Instance->Debug.MeshLODIndex;

					if (bRestTriangle)		{ AddDebugProjectionHairPass(GraphBuilder, ShaderMap, ShaderPrintData, Viewport, ViewUniformBuffer, MeshLODIndex, RestRootResource, DeformedRootResource, Instance->HairGroupPublicData->VFInput.LocalToWorldTransform, EDebugProjectionHairType::HairTriangle, HairStrandsTriangleType::RestPose); }
					if (bRestFrame)			{ AddDebugProjectionHairPass(GraphBuilder, ShaderMap, ShaderPrintData, Viewport, ViewUniformBuffer, MeshLODIndex, RestRootResource, DeformedRootResource, Instance->HairGroupPublicData->VFInput.LocalToWorldTransform, EDebugProjectionHairType::HairFrame,    HairStrandsTriangleType::RestPose); }
					if (bRestSamples)		{ AddDebugProjectionHairPass(GraphBuilder, ShaderMap, ShaderPrintData, Viewport, ViewUniformBuffer, MeshLODIndex, RestRootResource, DeformedRootResource, Instance->HairGroupPublicData->VFInput.LocalToWorldTransform, EDebugProjectionHairType::HairSamples,  HairStrandsTriangleType::RestPose); }
					if (bDeformedTriangle)	{ AddDebugProjectionHairPass(GraphBuilder, ShaderMap, ShaderPrintData, Viewport, ViewUniformBuffer, MeshLODIndex, RestRootResource, DeformedRootResource, Instance->HairGroupPublicData->VFInput.LocalToWorldTransform, EDebugProjectionHairType::HairTriangle, HairStrandsTriangleType::DeformedPose); }
					if (bDeformedFrame)		{ AddDebugProjectionHairPass(GraphBuilder, ShaderMap, ShaderPrintData, Viewport, ViewUniformBuffer, MeshLODIndex, RestRootResource, DeformedRootResource, Instance->HairGroupPublicData->VFInput.LocalToWorldTransform, EDebugProjectionHairType::HairFrame,    HairStrandsTriangleType::DeformedPose); }
					if (bDeformedSamples)	{ AddDebugProjectionHairPass(GraphBuilder, ShaderMap, ShaderPrintData, Viewport, ViewUniformBuffer, MeshLODIndex, RestRootResource, DeformedRootResource, Instance->HairGroupPublicData->VFInput.LocalToWorldTransform, EDebugProjectionHairType::HairSamples,  HairStrandsTriangleType::DeformedPose); }
				}
			};

			if (GHairDebugMeshProjection_Render_HairRestTriangles > 0 ||
				GHairDebugMeshProjection_Render_HairRestFrames > 0 ||
				GHairDebugMeshProjection_Render_HairDeformedTriangles > 0 ||
				GHairDebugMeshProjection_Render_HairDeformedFrames > 0 ||
				GHairDebugMeshProjection_Render_HairDeformedSamples > 0 ||
				GHairDebugMeshProjection_Render_HairRestSamples > 0)
			{
				RenderProjectionData(
					false,
					GHairDebugMeshProjection_Render_HairRestTriangles > 0,
					GHairDebugMeshProjection_Render_HairRestFrames > 0,
					GHairDebugMeshProjection_Render_HairRestSamples > 0,
					GHairDebugMeshProjection_Render_HairDeformedTriangles > 0,
					GHairDebugMeshProjection_Render_HairDeformedFrames > 0,
					GHairDebugMeshProjection_Render_HairDeformedSamples > 0);
			}

			if (GHairDebugMeshProjection_Sim_HairRestTriangles > 0 ||
				GHairDebugMeshProjection_Sim_HairRestFrames > 0 ||
				GHairDebugMeshProjection_Sim_HairDeformedTriangles > 0 ||
				GHairDebugMeshProjection_Sim_HairDeformedFrames > 0 ||
				GHairDebugMeshProjection_Sim_HairDeformedSamples > 0 ||
				GHairDebugMeshProjection_Sim_HairRestSamples > 0)
			{
				RenderProjectionData(
					true,
					GHairDebugMeshProjection_Sim_HairRestTriangles > 0,
					GHairDebugMeshProjection_Sim_HairRestFrames > 0,
					GHairDebugMeshProjection_Sim_HairRestSamples > 0,
					GHairDebugMeshProjection_Sim_HairDeformedTriangles > 0,
					GHairDebugMeshProjection_Sim_HairDeformedFrames > 0,
					GHairDebugMeshProjection_Sim_HairDeformedSamples > 0);
			}
		}
	}

	if (GHairCardsAtlasDebug > 0)
	{
		for (FHairStrandsInstance* AbstractInstance : Instances)
		{
			FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);

			AddDrawDebugCardsAtlasPass(GraphBuilder, View, ShaderMap, Instance, ShaderPrintData, SceneColorTexture);
		}
	}

	const bool bEnabled =
		ViewMode == EGroomViewMode::CardGuides ||
		ViewMode == EGroomViewMode::SimHairStrands ||
		ViewMode == EGroomViewMode::ControlPoints;

	if (bEnabled)
	{	
		for (FHairStrandsInstance* AbstractInstance : Instances)
		{
			FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);
	
			if (ViewMode == EGroomViewMode::CardGuides)
			{
				AddDrawDebugCardsGuidesPass(GraphBuilder, View, ShaderMap, Instance, ShaderPrintData, true /*bDeformed*/, true);
			}
	
			if (ViewMode == EGroomViewMode::SimHairStrands)
			{
				AddDrawDebugCardsGuidesPass(GraphBuilder, View, ShaderMap, Instance, ShaderPrintData, true /*bDeformed*/, false);
			}
	
			if (ViewMode == EGroomViewMode::ControlPoints)
			{
				AddDrawDebugStrandsCVsPass(GraphBuilder, View, ShaderMap, Instance, ShaderPrintData, SceneColorTexture, SceneDepthTexture);
			}
		}
	}

	if (ViewMode == EGroomViewMode::Memory)
	{
		AddHairDebugPrintMemoryPass(GraphBuilder, ShaderMap, View, ShaderPrintData);
	}
}
