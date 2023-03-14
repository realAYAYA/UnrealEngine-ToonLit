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
#include "SkeletalRenderPublic.h"
#include "UnrealEngine.h"
#include "SystemTextures.h"
#include "CanvasTypes.h"
#include "ShaderCompilerCore.h"

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
static int32 GHairDebugMeshProjection_Render_HairDeformedFrames = 0;
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

static int32 GHairCardsVoxelDebug = 0;
static FAutoConsoleVariableRef CVarHairCardsVoxelDebug(TEXT("r.HairStrands.Cards.DebugVoxel"), GHairCardsVoxelDebug, TEXT("Draw debug hair cards voxel datas."));

static int32 GHairCardsGuidesDebug_Ren = 0;
static int32 GHairCardsGuidesDebug_Sim = 0;
static FAutoConsoleVariableRef CVarHairCardsGuidesDebug_Ren(TEXT("r.HairStrands.Cards.DebugGuides.Render"), GHairCardsGuidesDebug_Ren, TEXT("Draw debug hair cards guides (1: Rest, 2: Deformed)."));
static FAutoConsoleVariableRef CVarHairCardsGuidesDebug_Sim(TEXT("r.HairStrands.Cards.DebugGuides.Sim"), GHairCardsGuidesDebug_Sim, TEXT("Draw debug hair sim guides (1: Rest, 2: Deformed)."));

static int32 GHairStrandsControlPointDebug = 0;
static FAutoConsoleVariableRef CVarHairStrandsControlPointDebug(TEXT("r.HairStrands.Strands.DebugControlPoint"), GHairStrandsControlPointDebug, TEXT("Draw debug hair strands control points)."));

///////////////////////////////////////////////////////////////////////////////////////////////////

FCachedGeometry GetCacheGeometryForHair(
	FRDGBuilder& GraphBuilder,
	FHairGroupInstance* Instance,
	FGlobalShaderMap* ShaderMap,
	const bool bOutputTriangleData);

static void GetGroomInterpolationData(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const FHairStrandsInstances& Instances,
	const EHairStrandsProjectionMeshType MeshType,
	FHairStrandsProjectionMeshData::LOD& OutGeometries)
{
	for (FHairStrandsInstance* AbstractInstance : Instances)
	{
		FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);

		if (!Instance || !Instance->Debug.MeshComponent)
			continue;

		const FCachedGeometry CachedGeometry = GetCacheGeometryForHair(GraphBuilder, Instance, ShaderMap, true);
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

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RestPosition0Buffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RestPosition1Buffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RestPosition2Buffer)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, DeformedPosition0Buffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, DeformedPosition1Buffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, DeformedPosition2Buffer)

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
		return;
	
	const EPrimitiveType PrimitiveType = GeometryType == EDebugProjectionHairType::HairFrame ? PT_LineList : GeometryType == EDebugProjectionHairType::HairTriangle ? PT_TriangleList : PT_LineList;
	const uint32 RootCount = EDebugProjectionHairType::HairSamples == GeometryType ? 3 * RestRootResources->LODs[MeshLODIndex].SampleCount : RestRootResources->BulkData.RootCount;
	const uint32 PrimitiveCount = RootCount;

	if (PrimitiveCount == 0)
		return;

	if (ShaderPrintData == nullptr || !ShaderPrint::IsEnabled(*ShaderPrintData))
	{
		return;
	}

	ShaderPrint::RequestSpaceForLines(PrimitiveCount);
	ShaderPrint::RequestSpaceForTriangles(PrimitiveCount);

	if (EDebugProjectionHairType::HairFrame == GeometryType &&
		!RestRootResources->LODs[MeshLODIndex].RootBarycentricBuffer.Buffer)
		return;

	if (EDebugProjectionHairType::HairSamples == GeometryType &&
		!RestRootResources->LODs[MeshLODIndex].RestSamplePositionsBuffer.Buffer)
			return;

	const FHairStrandsRestRootResource::FLOD& RestLODDatas = RestRootResources->LODs[MeshLODIndex];
	const FHairStrandsDeformedRootResource::FLOD& DeformedLODDatas = DeformedRootResources->LODs[MeshLODIndex];

	if (!RestLODDatas.RestUniqueTrianglePosition0Buffer.Buffer ||
		!RestLODDatas.RestUniqueTrianglePosition1Buffer.Buffer ||
		!RestLODDatas.RestUniqueTrianglePosition2Buffer.Buffer ||
		!DeformedLODDatas.DeformedUniqueTrianglePosition0Buffer[0].Buffer ||
		!DeformedLODDatas.DeformedUniqueTrianglePosition1Buffer[0].Buffer ||
		!DeformedLODDatas.DeformedUniqueTrianglePosition2Buffer[0].Buffer	)
		return;

	// Double buffering is disabled by default unless the read-only cvar r.HairStrands.ContinuousDecimationReordering is set
	if (IsHairStrandContinuousDecimationReorderingEnabled() && (!DeformedLODDatas.DeformedUniqueTrianglePosition0Buffer[1].Buffer || !DeformedLODDatas.DeformedUniqueTrianglePosition1Buffer[1].Buffer || !DeformedLODDatas.DeformedUniqueTrianglePosition2Buffer[1].Buffer))
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
		Parameters->RootBarycentricBuffer	= RegisterAsSRV(GraphBuilder, RestLODDatas.RootBarycentricBuffer);
	}

	Parameters->RootToUniqueTriangleIndexBuffer = RegisterAsSRV(GraphBuilder, RestLODDatas.RootToUniqueTriangleIndexBuffer);

	Parameters->RestPosition0Buffer = RegisterAsSRV(GraphBuilder, RestLODDatas.RestUniqueTrianglePosition0Buffer);
	Parameters->RestPosition1Buffer = RegisterAsSRV(GraphBuilder, RestLODDatas.RestUniqueTrianglePosition1Buffer);
	Parameters->RestPosition2Buffer = RegisterAsSRV(GraphBuilder, RestLODDatas.RestUniqueTrianglePosition2Buffer);

	Parameters->DeformedPosition0Buffer = RegisterAsSRV(GraphBuilder, DeformedLODDatas.GetDeformedUniqueTrianglePosition0Buffer(FHairStrandsDeformedRootResource::FLOD::Current));
	Parameters->DeformedPosition1Buffer = RegisterAsSRV(GraphBuilder, DeformedLODDatas.GetDeformedUniqueTrianglePosition1Buffer(FHairStrandsDeformedRootResource::FLOD::Current));
	Parameters->DeformedPosition2Buffer = RegisterAsSRV(GraphBuilder, DeformedLODDatas.GetDeformedUniqueTrianglePosition2Buffer(FHairStrandsDeformedRootResource::FLOD::Current));

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
class FVoxelPlainRaymarchingCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FVoxelPlainRaymarchingCS);
	SHADER_USE_PARAMETER_STRUCT(FVoxelPlainRaymarchingCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(ShaderPrint::FShaderParameters, ShaderPrintParameters)
		SHADER_PARAMETER(FVector2f, OutputResolution)		
		SHADER_PARAMETER(FIntVector, Voxel_Resolution)
		SHADER_PARAMETER(float, Voxel_VoxelSize)
		SHADER_PARAMETER(FVector3f, Voxel_MinBound)
		SHADER_PARAMETER(FVector3f, Voxel_MaxBound)
		SHADER_PARAMETER_SRV(Buffer, Voxel_TangentBuffer)
		SHADER_PARAMETER_SRV(Buffer, Voxel_NormalBuffer)
		SHADER_PARAMETER_SRV(Buffer, Voxel_DensityBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, Voxel_ProcessedDensityBuffer)
		SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutputTexture)
		END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Strands, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_CARDS_VOXEL"), 1);
	}	
};

IMPLEMENT_GLOBAL_SHADER(FVoxelPlainRaymarchingCS, "/Engine/Private/HairStrands/HairStrandsDebug.usf", "MainCS", SF_Compute);

static void AddVoxelPlainRaymarchingPass(
	FRDGBuilder& GraphBuilder,
	const FSceneView& View,
	FGlobalShaderMap* ShaderMap,
	const FHairGroupInstance* Instance,
	const FShaderPrintData* ShaderPrintData,
	FRDGTextureRef OutputTexture)
{
#if 0 // #hair_todo: renable if needed
	const FHairStrandClusterData::FHairGroup& HairGroupClusters = HairClusterData.HairGroups[DataIndex];

	FViewInfo& View = Views[ViewIndex];
	if (ShaderPrint::IsEnabled(View.ShaderPrintData))
	{
		if (Instance->HairGroupPublicData->VFInput.GeometryType != EHairGeometryType::Cards)
			return;

		FSceneTextureParameters SceneTextures;
		SetupSceneTextureParameters(GraphBuilder, &SceneTextures);

		const FIntPoint OutputResolution(OutputTexture->Desc.Extent);
		const FHairCardsVoxel& CardsVoxel = Instance->HairGroupPublicData->VFInput.Cards.Voxel;

		FRDGBufferRef VoxelDensityBuffer2 = nullptr;
		AddVoxelProcessPass(GraphBuilder, View, CardsVoxel, VoxelDensityBuffer2);

		FVoxelPlainRaymarchingCS::FParameters* Parameters = GraphBuilder.AllocParameters<FVoxelPlainRaymarchingCS::FParameters>();
		Parameters->ViewUniformBuffer	= View.ViewUniformBuffer;
		Parameters->OutputResolution	= OutputResolution;
		Parameters->Voxel_Resolution	= CardsVoxel.Resolution;
		Parameters->Voxel_VoxelSize		= CardsVoxel.VoxelSize;
		Parameters->Voxel_MinBound		= CardsVoxel.MinBound;
		Parameters->Voxel_MaxBound		= CardsVoxel.MaxBound;
		Parameters->Voxel_TangentBuffer	= CardsVoxel.TangentBuffer.SRV;
		Parameters->Voxel_NormalBuffer	= CardsVoxel.NormalBuffer.SRV;
		Parameters->Voxel_DensityBuffer = CardsVoxel.DensityBuffer.SRV;
		Parameters->Voxel_ProcessedDensityBuffer = GraphBuilder.CreateSRV(VoxelDensityBuffer2, PF_R32_UINT);

		ShaderPrint::SetParameters(GraphBuilder, ShaderPrintData, Parameters->ShaderPrintParameters);
		//ShaderPrint::SetParameters(View, Parameters->ShaderPrintParameters);
		Parameters->OutputTexture = GraphBuilder.CreateUAV(OutputTexture);

		TShaderMapRef<FVoxelPlainRaymarchingCS> ComputeShader(ShaderMap);
		const FIntVector DispatchCount = DispatchCount.DivideAndRoundUp(FIntVector(OutputTexture->Desc.Extent.X, OutputTexture->Desc.Extent.Y, 1), FIntVector(8, 8, 1));
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("HairStrands::VoxelPlainRaymarching"), ComputeShader, Parameters, DispatchCount);
	}
#endif
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
	Parameters->LocalToWorld = FMatrix44f(Instance->LocalToWorld.ToMatrixWithScale());		// LWC_TODO: Precision loss
	Parameters->MaxVertexCount = Instance->Strands.Data->PointCount;
	Parameters->ColorTexture = GraphBuilder.CreateUAV(ColorTexture);
	Parameters->DepthTexture = DepthTexture;
	Parameters->LinearSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

	const uint32 VertexCount = Instance->HairGroupPublicData->VFInput.Strands.VertexCount;
	FComputeShaderUtils::AddPass(
		GraphBuilder, 
		RDG_EVENT_NAME("HairStrands::DrawCVs"), 
		ComputeShader, 
		Parameters,
		FIntVector::DivideAndRoundUp(FIntVector(VertexCount, 1, 1), FIntVector(256, 1, 1)));
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
		Instance->Guides.RestResource ? Instance->Guides.RestResource->GetVertexCount() * 2 : 0,
		LOD.Guides.RestResource ? LOD.Guides.RestResource->GetVertexCount() * 2 : 0);
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
		Parameters->RenVertexCount = LOD.Guides.RestResource->GetVertexCount();
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
		Parameters->SimVertexCount = Instance->Guides.RestResource->GetVertexCount();
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
		SHADER_PARAMETER(uint32, NameInfoCount)
		SHADER_PARAMETER(uint32, NameCharacterCount)
		SHADER_PARAMETER(uint32, InstanceCount_StrandsPrimaryView)
		SHADER_PARAMETER(uint32, InstanceCount_StrandsShadowView)
		SHADER_PARAMETER(uint32, InstanceCount_CardsOrMeshes)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<uint2>, NameInfos)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint8>, Names)
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

struct FHairDebugNameInfo
{
	uint32 PrimitiveID;
	uint16 Offset;
	uint8  Length;
	uint8  Pad0;
};

static void AddHairDebugPrintInstancePass(
	FRDGBuilder& GraphBuilder, 
	FGlobalShaderMap* ShaderMap,
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

	const uint32 MaxPrimitiveNameCount = 128u;
	check(sizeof(FHairDebugNameInfo) == 8);

	TArray<FHairDebugNameInfo> NameInfos;
	TArray<uint8> Names;
	Names.Reserve(MaxPrimitiveNameCount * 30u);

	struct FInstanceInfos
	{
		FUintVector4 Data0;
		FUintVector4 Data1;
		FUintVector4 Data2;
	};
	TArray<FInstanceInfos> Infos;
	Infos.Reserve(InstanceCount);
	for (uint32 InstanceIndex = 0; InstanceIndex < InstanceCount; ++InstanceIndex)
	{
		const FHairStrandsInstance* AbstractInstance = Instances[InstanceIndex];
		const FHairGroupInstance* Instance = static_cast<const FHairGroupInstance*>(AbstractInstance);

		// Collect Names
		if (InstanceIndex < MaxPrimitiveNameCount)
		{
			const FString Name = *Instance->Debug.GroomAssetName;
			const uint32 NameOffset = Names.Num();
			const uint32 NameLength = Name.Len();
			for (TCHAR C : Name)
			{
				Names.Add(uint8(C));
			}

			FHairDebugNameInfo& NameInfo = NameInfos.AddDefaulted_GetRef();
			NameInfo.PrimitiveID = InstanceIndex;
			NameInfo.Length = NameLength;
			NameInfo.Offset = NameOffset;
		}

		const float LODIndex = Instance->HairGroupPublicData->LODIndex;
		const uint32 IntLODIndex = Instance->HairGroupPublicData->LODIndex;
		const uint32 LODCount = Instance->HairGroupPublicData->GetLODScreenSizes().Num();

		FUintVector4 Data0 = { 0,0,0,0 };
		FUintVector4 Data1 = { 0,0,0,0 };
		FUintVector4 Data2 = { 0,0,0,0 };
		Data0.X =
			((Instance->Debug.GroupIndex & 0xFF)) |
			((Instance->Debug.GroupCount & 0xFF) << 8) |
			((LODCount & 0xFF) << 16) |
			((uint32(Instance->GeometryType) & 0x7)<<24) |
			((uint32(Instance->BindingType) & 0x7)<<27) |
			((Instance->Guides.bIsSimulationEnable ? 0x1 : 0x0) << 30) |
			((Instance->Guides.bHasGlobalInterpolation ? 0x1 : 0x0) << 31);

		Data0.Y =
			(FFloat16(LODIndex).Encoded) |
			(FFloat16(Instance->Strands.Modifier.HairLengthScale_Override ? Instance->Strands.Modifier.HairLengthScale : -1.f).Encoded << 16);
		
		switch (Instance->GeometryType)
		{
		case EHairGeometryType::Strands:
			if (Instance->Strands.IsValid())
			{
				Data0.Z = Instance->Strands.Data->GetNumCurves(); // Change this later on for having dynamic value
				Data0.W = Instance->Strands.Data->GetNumPoints(); // Change this later on for having dynamic value
				const int32 MeshLODIndex = Instance->HairGroupPublicData->MeshLODIndex;
				if (MeshLODIndex>=0)
				{
					Data1.X = Instance->Strands.RestRootResource->BulkData.MeshProjectionLODs[MeshLODIndex].UniqueSectionIndices.Num();
					Data1.Y = Instance->Strands.RestRootResource->BulkData.MeshProjectionLODs[MeshLODIndex].UniqueTriangleCount;
					Data1.Z = Instance->Strands.RestRootResource->BulkData.RootCount;
					Data1.W = Instance->Strands.RestRootResource->BulkData.PointCount;
				}

				{
					Data2.X = InstanceIndex < InstanceCountPerType[HairInstanceCount_StrandsPrimaryView] ? 3 : 2; // Visible in primary & shadow (3) or only in shadow (2)
					Data2.Y = 0;
					Data2.Z = 0;
					Data2.W = 0;
				}
			}
			break;
		case EHairGeometryType::Cards:
			if (Instance->Cards.IsValid(IntLODIndex))
			{
				Data0.Z = Instance->Cards.LODs[IntLODIndex].Guides.IsValid() ? Instance->Cards.LODs[IntLODIndex].Guides.Data->GetNumCurves() : 0;
				Data0.W = Instance->Cards.LODs[IntLODIndex].Data->GetNumVertices();
				Data2.X = 3; // Visible in primary & shadow
			}
			break;
		case EHairGeometryType::Meshes:
			if (Instance->Meshes.IsValid(IntLODIndex))
			{
				Data0.Z = 0;
				Data0.W = Instance->Meshes.LODs[IntLODIndex].Data->GetNumVertices();
				Data2.X = 3; // Visible in primary & shadow
			}
			break;
		}
		Infos.Add({Data0, Data1, Data2 });
	}

	if (NameInfos.IsEmpty())
	{
		FHairDebugNameInfo& NameInfo = NameInfos.AddDefaulted_GetRef();
		NameInfo.PrimitiveID = ~0;
		NameInfo.Length = 4;
		NameInfo.Offset = 0;
		Names.Add(uint8('N'));
		Names.Add(uint8('o'));
		Names.Add(uint8('n'));
		Names.Add(uint8('e'));
	}	

	const uint32 InfoInBytes  = sizeof(FInstanceInfos);
	const uint32 InfoIn4Bytes = sizeof(FInstanceInfos) / sizeof(uint8);
	FRDGBufferRef NameBuffer = CreateVertexBuffer(GraphBuilder, TEXT("Hair.Debug.InstanceNames"), FRDGBufferDesc::CreateBufferDesc(1, Names.Num()), Names.GetData(), Names.Num());
	FRDGBufferRef NameInfoBuffer = CreateStructuredBuffer(GraphBuilder, TEXT("Hair.Debug.InstanceNameInfos"), NameInfos);	
	FRDGBufferRef InfoBuffer = CreateVertexBuffer(GraphBuilder, TEXT("Hair.Debug.InstanceInfos"), FRDGBufferDesc::CreateBufferDesc(4, InfoIn4Bytes * Infos.Num()), Infos.GetData(), InfoInBytes * Infos.Num());

	// Draw general information for all instances (one pass for all instances)
	{
		FHairDebugPrintInstanceCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairDebugPrintInstanceCS::FParameters>();
		Parameters->InstanceCount = InstanceCount;
		Parameters->InstanceCount_StrandsPrimaryView = InstanceCountPerType[HairInstanceCount_StrandsPrimaryView];
		Parameters->InstanceCount_StrandsShadowView = InstanceCountPerType[HairInstanceCount_StrandsShadowView];
		Parameters->InstanceCount_CardsOrMeshes = InstanceCountPerType[HairInstanceCount_CardsOrMeshes];
		Parameters->NameInfoCount = NameInfos.Num();
		Parameters->NameCharacterCount = Names.Num();
		Parameters->Names = GraphBuilder.CreateSRV(NameBuffer, PF_R8_UINT);
		Parameters->NameInfos = GraphBuilder.CreateSRV(NameInfoBuffer);
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
			FHairDebugPrintInstanceCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairDebugPrintInstanceCS::FParameters>();
			Parameters->InstanceAABB = Register(GraphBuilder, Instance->HairGroupPublicData->GroupAABBBuffer, ERDGImportedBufferFlags::CreateSRV).SRV;
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

void RunHairStrandsDebug(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const FSceneView& View,
	const FHairStrandsInstances& Instances,
	const FUintVector4& InstanceCountPerType,
	const FShaderPrintData* ShaderPrintData,
	FRDGTextureRef SceneColorTexture,
	FRDGTextureRef SceneDepthTexture,
	FIntRect Viewport,
	const TUniformBufferRef<FViewUniformShaderParameters>& ViewUniformBuffer)
{
	const EHairDebugMode HairDebugMode = GetHairStrandsDebugMode();

	if (HairDebugMode == EHairDebugMode::MacroGroups)
	{
		AddHairDebugPrintInstancePass(GraphBuilder, ShaderMap, ShaderPrintData, Instances, InstanceCountPerType);
	}

	if (HairDebugMode == EHairDebugMode::MeshProjection)
	{
		{
			if (GHairDebugMeshProjection_SkinCacheMesh > 0)
			{
				auto RenderMeshProjection = [ShaderMap, ShaderPrintData, Viewport, &ViewUniformBuffer, Instances, &GraphBuilder](FRDGBuilder& LocalGraphBuilder, EHairStrandsProjectionMeshType MeshType)
				{
					FHairStrandsProjectionMeshData::LOD MeshProjectionLODData;
					GetGroomInterpolationData(GraphBuilder, ShaderMap, Instances, MeshType, MeshProjectionLODData);
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

			auto RenderProjectionData = [&GraphBuilder, ShaderMap, Viewport, &ViewUniformBuffer, Instances, ShaderPrintData](EHairStrandsInterpolationType StrandType, bool bRestTriangle, bool bRestFrame, bool bRestSamples, bool bDeformedTriangle, bool bDeformedFrame, bool bDeformedSamples)
			{
				TArray<int32> HairLODIndices;
				for (FHairStrandsInstance* AbstractInstance : Instances)
				{
					FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);
					if (!Instance->HairGroupPublicData || Instance->BindingType != EHairBindingType::Skinning)
						continue;

					const bool bRenderStrands = StrandType == EHairStrandsInterpolationType::RenderStrands;
					FHairStrandsRestRootResource* RestRootResource = bRenderStrands ? Instance->Strands.RestRootResource : Instance->Guides.RestRootResource;
					FHairStrandsDeformedRootResource* DeformedRootResource = bRenderStrands ? Instance->Strands.DeformedRootResource : Instance->Guides.DeformedRootResource;
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
					EHairStrandsInterpolationType::RenderStrands,
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
					EHairStrandsInterpolationType::SimulationStrands,
					GHairDebugMeshProjection_Sim_HairRestTriangles > 0,
					GHairDebugMeshProjection_Sim_HairRestFrames > 0,
					GHairDebugMeshProjection_Sim_HairRestSamples > 0,
					GHairDebugMeshProjection_Sim_HairDeformedTriangles > 0,
					GHairDebugMeshProjection_Sim_HairDeformedFrames > 0,
					GHairDebugMeshProjection_Sim_HairDeformedSamples > 0);
			}
		}
	}

	if (GHairCardsVoxelDebug > 0)
	{
		for (FHairStrandsInstance* AbstractInstance : Instances)
		{
			FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);

			AddVoxelPlainRaymarchingPass(GraphBuilder, View, ShaderMap, Instance, ShaderPrintData, SceneColorTexture);
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

	for (FHairStrandsInstance* AbstractInstance : Instances)
	{
		FHairGroupInstance* Instance = static_cast<FHairGroupInstance*>(AbstractInstance);

		if (GHairCardsGuidesDebug_Ren > 0 || Instance->Debug.bDrawCardsGuides)
		{
			AddDrawDebugCardsGuidesPass(GraphBuilder, View, ShaderMap, Instance, ShaderPrintData, Instance->Debug.bDrawCardsGuides ? false : GHairCardsGuidesDebug_Ren == 1, true);
		}

		if (GHairCardsGuidesDebug_Sim > 0)
		{
			AddDrawDebugCardsGuidesPass(GraphBuilder, View, ShaderMap, Instance, ShaderPrintData, GHairCardsGuidesDebug_Sim == 1, false);
		}

		if (GHairStrandsControlPointDebug || Instance->HairGroupPublicData->DebugMode == EHairStrandsDebugMode::RenderHairControlPoints)
		{
			AddDrawDebugStrandsCVsPass(GraphBuilder, View, ShaderMap, Instance, ShaderPrintData, SceneColorTexture, SceneDepthTexture);
		}
	}
}