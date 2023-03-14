// Copyright Epic Games, Inc. All Rights Reserved.

#include "HairStrandsMeshProjection.h"
#include "MeshMaterialShader.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "CommonRenderResources.h"
#include "SkeletalRenderPublic.h"
#include "RayTracingDynamicGeometryCollection.h"
#include "RenderGraphUtils.h"
#include "GroomResources.h"

static int32 GHairProjectionMaxTrianglePerProjectionIteration = 8;
static FAutoConsoleVariableRef CVarHairProjectionMaxTrianglePerProjectionIteration(TEXT("r.HairStrands.Projection.MaxTrianglePerIteration"), GHairProjectionMaxTrianglePerProjectionIteration, TEXT("Change the number of triangles which are iterated over during one projection iteration step. In kilo triangle (e.g., 8 == 8000 triangles). Default is 8."));

static int32 GHairStrandsUseGPUPositionOffset = 1;
static FAutoConsoleVariableRef CVarHairStrandsUseGPUPositionOffset(TEXT("r.HairStrands.UseGPUPositionOffset"), GHairStrandsUseGPUPositionOffset, TEXT("Use GPU position offset to improve hair strands position precision."));

///////////////////////////////////////////////////////////////////////////////////////////////////

#define MAX_HAIRSTRANDS_SECTION_COUNT 255
#define MAX_HAIRSTRANDS_SECTION_BITOFFSET 24

uint32 GetHairStrandsMaxSectionCount()
{
	return MAX_HAIRSTRANDS_SECTION_COUNT;
}

uint32 GetHairStrandsMaxTriangleCount()
{
	return (1 << MAX_HAIRSTRANDS_SECTION_BITOFFSET) - 1;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

FHairStrandsProjectionMeshData ExtractMeshData(FSkeletalMeshRenderData* RenderData)
{
	FHairStrandsProjectionMeshData MeshData;
	uint32 LODIndex = 0;
	for (FSkeletalMeshLODRenderData& LODRenderData : RenderData->LODRenderData)
	{
		FHairStrandsProjectionMeshData::LOD& LOD = MeshData.LODs.AddDefaulted_GetRef();
		uint32 SectionIndex = 0;
		for (FSkelMeshRenderSection& InSection : LODRenderData.RenderSections)
		{
			// Pick between float and halt
			const uint32 UVSizeInByte = (LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetUseFullPrecisionUVs() ? 4 : 2) * 2;

			FHairStrandsProjectionMeshData::Section& OutSection = LOD.Sections.AddDefaulted_GetRef();
			OutSection.UVsChannelOffset = 0; // Assume that we needs to pair meshes based on UVs 0
			OutSection.UVsChannelCount = LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
			OutSection.UVsBuffer = LODRenderData.StaticVertexBuffers.StaticMeshVertexBuffer.GetTexCoordsSRV();
			OutSection.PositionBuffer = LODRenderData.StaticVertexBuffers.PositionVertexBuffer.GetSRV();
			OutSection.IndexBuffer = LODRenderData.MultiSizeIndexContainer.GetIndexBuffer()->GetSRV();
			OutSection.TotalVertexCount = LODRenderData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices();
			OutSection.TotalIndexCount = LODRenderData.MultiSizeIndexContainer.GetIndexBuffer()->Num();
			OutSection.NumPrimitives = InSection.NumTriangles;
			OutSection.NumVertices = InSection.NumVertices;
			OutSection.VertexBaseIndex = InSection.BaseVertexIndex;
			OutSection.IndexBaseIndex = InSection.BaseIndex;
			OutSection.SectionIndex = SectionIndex;
			OutSection.LODIndex = LODIndex;

			++SectionIndex;
		}
		++LODIndex;
	}

	return MeshData;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
class FSkinUpdateCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FSkinUpdateCS);
	SHADER_USE_PARAMETER_STRUCT(FSkinUpdateCS, FGlobalShader);

	
	class FPrevious : SHADER_PERMUTATION_BOOL("PERMUTATION_PREV");
	class FUnlimitedBoneInfluence : SHADER_PERMUTATION_BOOL("GPUSKIN_UNLIMITED_BONE_INFLUENCE");
	class FUseExtraInfluence : SHADER_PERMUTATION_BOOL("GPUSKIN_USE_EXTRA_INFLUENCES");
	class FIndexUint16 : SHADER_PERMUTATION_BOOL("GPUSKIN_BONE_INDEX_UINT16");
	using FPermutationDomain = TShaderPermutationDomain<FUnlimitedBoneInfluence, FUseExtraInfluence, FIndexUint16, FPrevious>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, NumTotalVertices)
		SHADER_PARAMETER(uint32, SectionVertexBaseIndex)
		SHADER_PARAMETER(uint32, IndexSize)
		SHADER_PARAMETER(uint32, WeightStride)
		SHADER_PARAMETER(uint32, BonesOffset)
		SHADER_PARAMETER_SRV(Buffer<uint>, WeightLookup)
		SHADER_PARAMETER_SRV(Buffer<float4>, BoneMatrices)
		SHADER_PARAMETER_SRV(Buffer<float4>, PrevBoneMatrices)
		SHADER_PARAMETER_SRV(Buffer<uint>, VertexWeights)
		SHADER_PARAMETER_SRV(Buffer<float>, RestPositions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, DeformedPositions)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, PrevDeformedPositions)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::All, Parameters.Platform); }
};

IMPLEMENT_GLOBAL_SHADER(FSkinUpdateCS, "/Engine/Private/HairStrands/HairStrandsSkinUpdate.usf", "UpdateSkinPositionCS", SF_Compute);

void AddSkinUpdatePass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	uint32 SectionIndex,
	uint32 BonesOffset,
	FSkinWeightVertexBuffer* SkinWeight,
	FSkeletalMeshLODRenderData& RenderData,
	FRHIShaderResourceView* BoneMatrices,
	FRHIShaderResourceView* PrevBoneMatrices,
	FRDGBufferRef OutDeformedPosition,
	FRDGBufferRef OutPrevDeformedPosition)
{
	check(BoneMatrices);

	const FSkelMeshRenderSection& Section = RenderData.RenderSections[SectionIndex];
	const uint32 NumVertexToProcess = Section.NumVertices;
	const uint32 SectionVertexBaseIndex = Section.BaseVertexIndex;
	const uint32 NumTotalVertices = RenderData.StaticVertexBuffers.PositionVertexBuffer.GetNumVertices();

	const bool bPrevPosition = OutPrevDeformedPosition != nullptr && PrevBoneMatrices != nullptr;
	
	FSkinUpdateCS::FParameters* Parameters = GraphBuilder.AllocParameters<FSkinUpdateCS::FParameters>();
	Parameters->IndexSize = SkinWeight->GetBoneIndexByteSize();
	Parameters->NumTotalVertices = NumTotalVertices;
	Parameters->SectionVertexBaseIndex = SectionVertexBaseIndex;
	Parameters->WeightStride = SkinWeight->GetConstantInfluencesVertexStride();
	Parameters->WeightLookup = SkinWeight->GetLookupVertexBuffer()->GetSRV();
	Parameters->BonesOffset = BonesOffset;
	Parameters->BoneMatrices = BoneMatrices;
	Parameters->VertexWeights = SkinWeight->GetDataVertexBuffer()->GetSRV();
	Parameters->RestPositions = RenderData.StaticVertexBuffers.PositionVertexBuffer.GetSRV();
	Parameters->DeformedPositions = GraphBuilder.CreateUAV(OutDeformedPosition, PF_R32_FLOAT);
	if (bPrevPosition)
	{
		check(PrevBoneMatrices);
		Parameters->PrevBoneMatrices = BoneMatrices;
		Parameters->PrevDeformedPositions = GraphBuilder.CreateUAV(OutPrevDeformedPosition, PF_R32_FLOAT);
	}

	FSkinUpdateCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FSkinUpdateCS::FUnlimitedBoneInfluence>(SkinWeight->GetBoneInfluenceType() == GPUSkinBoneInfluenceType::UnlimitedBoneInfluence);
	PermutationVector.Set<FSkinUpdateCS::FUseExtraInfluence>(SkinWeight->GetMaxBoneInfluences() > MAX_INFLUENCES_PER_STREAM);
	PermutationVector.Set<FSkinUpdateCS::FIndexUint16>(SkinWeight->Use16BitBoneIndex());
	PermutationVector.Set<FSkinUpdateCS::FPrevious>(bPrevPosition);

	const FIntVector DispatchGroupCount = FComputeShaderUtils::GetGroupCount(NumVertexToProcess, 64);
	check(DispatchGroupCount.X < 65536);
	TShaderMapRef<FSkinUpdateCS> ComputeShader(ShaderMap, PermutationVector);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("HairStrands::UpdateSkinPosition(%s,Section:%d)", bPrevPosition ? TEXT("Curr,Prev") : TEXT("Curr"), SectionIndex),
		ComputeShader,
		Parameters,
		DispatchGroupCount);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class FHairUpdateMeshTriangleCS : public FGlobalShader
{
public:
	const static uint32 SectionArrayCount = 16; // This defines the number of sections managed for each iteration pass
private:
	DECLARE_GLOBAL_SHADER(FHairUpdateMeshTriangleCS);
	SHADER_USE_PARAMETER_STRUCT(FHairUpdateMeshTriangleCS, FGlobalShader);

	class FUpdateUVs : SHADER_PERMUTATION_BOOL("PERMUTATION_WITHUV");
	class FPositionType : SHADER_PERMUTATION_BOOL("PERMUTATION_POSITION_TYPE");
	class FPrevious : SHADER_PERMUTATION_BOOL("PERMUTATION_PREVIOUS"); 
	using FPermutationDomain = TShaderPermutationDomain<FUpdateUVs, FPositionType, FPrevious>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MaxUniqueTriangleCount)
		SHADER_PARAMETER(uint32, MaxSectionCount)
		SHADER_PARAMETER(uint32, Pass_SectionStart)
		SHADER_PARAMETER(uint32, Pass_SectionCount)
		
		SHADER_PARAMETER_ARRAY(FUintVector4, PackedMeshSectionScalars1, [SectionArrayCount])
		SHADER_PARAMETER_ARRAY(FUintVector4, PackedMeshSectionScalars2, [SectionArrayCount])

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPositionBuffer0)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPositionBuffer1)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPositionBuffer2)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPositionBuffer3)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPositionBuffer4)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPositionBuffer5)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPositionBuffer6)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPositionBuffer7)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPreviousPositionBuffer0)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPreviousPositionBuffer1)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPreviousPositionBuffer2)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPreviousPositionBuffer3)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPreviousPositionBuffer4)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPreviousPositionBuffer5)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPreviousPositionBuffer6)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGMeshPreviousPositionBuffer7)

		SHADER_PARAMETER_SRV(Buffer, MeshPositionBuffer0)
		SHADER_PARAMETER_SRV(Buffer, MeshPositionBuffer1)
		SHADER_PARAMETER_SRV(Buffer, MeshPositionBuffer2)
		SHADER_PARAMETER_SRV(Buffer, MeshPositionBuffer3)
		SHADER_PARAMETER_SRV(Buffer, MeshPositionBuffer4)
		SHADER_PARAMETER_SRV(Buffer, MeshPositionBuffer5)
		SHADER_PARAMETER_SRV(Buffer, MeshPositionBuffer6)
		SHADER_PARAMETER_SRV(Buffer, MeshPositionBuffer7)

		SHADER_PARAMETER_SRV(Buffer, MeshPreviousPositionBuffer0)
		SHADER_PARAMETER_SRV(Buffer, MeshPreviousPositionBuffer1)
		SHADER_PARAMETER_SRV(Buffer, MeshPreviousPositionBuffer2)
		SHADER_PARAMETER_SRV(Buffer, MeshPreviousPositionBuffer3)
		SHADER_PARAMETER_SRV(Buffer, MeshPreviousPositionBuffer4)
		SHADER_PARAMETER_SRV(Buffer, MeshPreviousPositionBuffer5)
		SHADER_PARAMETER_SRV(Buffer, MeshPreviousPositionBuffer6)
		SHADER_PARAMETER_SRV(Buffer, MeshPreviousPositionBuffer7)

		SHADER_PARAMETER_SRV(Buffer, MeshIndexBuffer0)
		SHADER_PARAMETER_SRV(Buffer, MeshIndexBuffer1)
		SHADER_PARAMETER_SRV(Buffer, MeshIndexBuffer2)
		SHADER_PARAMETER_SRV(Buffer, MeshIndexBuffer3)
		SHADER_PARAMETER_SRV(Buffer, MeshIndexBuffer4)
		SHADER_PARAMETER_SRV(Buffer, MeshIndexBuffer5)
		SHADER_PARAMETER_SRV(Buffer, MeshIndexBuffer6)
		SHADER_PARAMETER_SRV(Buffer, MeshIndexBuffer7)
		
		SHADER_PARAMETER_SRV(Buffer, MeshUVsBuffer0)
		SHADER_PARAMETER_SRV(Buffer, MeshUVsBuffer1)
		SHADER_PARAMETER_SRV(Buffer, MeshUVsBuffer2)
		SHADER_PARAMETER_SRV(Buffer, MeshUVsBuffer3)
		SHADER_PARAMETER_SRV(Buffer, MeshUVsBuffer4)
		SHADER_PARAMETER_SRV(Buffer, MeshUVsBuffer5)
		SHADER_PARAMETER_SRV(Buffer, MeshUVsBuffer6)
		SHADER_PARAMETER_SRV(Buffer, MeshUVsBuffer7)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, UniqueTriangleIndices)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, OutUniqueTrianglePrevPosition0)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, OutUniqueTrianglePrevPosition1)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, OutUniqueTrianglePrevPosition2)

		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, OutUniqueTriangleCurrPosition0)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, OutUniqueTriangleCurrPosition1)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, OutUniqueTriangleCurrPosition2)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::All, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("MAX_SECTION_COUNT"), SectionArrayCount);
		OutEnvironment.SetDefine(TEXT("SHADER_MESH_UPDATE"), SectionArrayCount);		
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairUpdateMeshTriangleCS, "/Engine/Private/HairStrands/HairStrandsMesh.usf", "MainCS", SF_Compute);

// Manual packing for mesh section scalars.  MUST MATCH WITH HairStrandsMesh.usf
// PackedMeshSectionScalars1: MeshSectionIndices, MeshMaxIndexCount, MeshMaxVertexCount, MeshIndexOffset
// PackedMeshSectionScalars2: MeshUVsChannelOffset, MeshUVsChannelCount, MeshSectionBufferIndex, *free*
inline void SetMeshSectionIndices(FHairUpdateMeshTriangleCS::FParameters& Params, uint32 SectionIndex, uint32 MeshSectionIndices)         { Params.PackedMeshSectionScalars1[SectionIndex].X = MeshSectionIndices; }
inline void SetMeshMaxIndexCount(FHairUpdateMeshTriangleCS::FParameters& Params, uint32 SectionIndex, uint32 MeshMaxIndexCount)           { Params.PackedMeshSectionScalars1[SectionIndex].Y = MeshMaxIndexCount; }
inline void SetMeshMaxVertexCount(FHairUpdateMeshTriangleCS::FParameters& Params, uint32 SectionIndex, uint32 MeshMaxVertexCount)         { Params.PackedMeshSectionScalars1[SectionIndex].Z = MeshMaxVertexCount; }
inline void SetMeshIndexOffset(FHairUpdateMeshTriangleCS::FParameters& Params, uint32 SectionIndex, uint32 MeshIndexOffset)               { Params.PackedMeshSectionScalars1[SectionIndex].W = MeshIndexOffset; }
inline void SetMeshUVsChannelOffset(FHairUpdateMeshTriangleCS::FParameters& Params, uint32 SectionIndex, uint32 MeshUVsChannelOffset)     { Params.PackedMeshSectionScalars2[SectionIndex].X = MeshUVsChannelOffset; }
inline void SetMeshUVsChannelCount(FHairUpdateMeshTriangleCS::FParameters& Params, uint32 SectionIndex, uint32 MeshUVsChannelCount)       { Params.PackedMeshSectionScalars2[SectionIndex].Y = MeshUVsChannelCount; }
inline void SetMeshSectionBufferIndex(FHairUpdateMeshTriangleCS::FParameters& Params, uint32 SectionIndex, uint32 MeshSectionBufferIndex) { Params.PackedMeshSectionScalars2[SectionIndex].Z = MeshSectionBufferIndex; }


void AddHairStrandUpdateMeshTrianglesPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const int32 LODIndex,
	const HairStrandsTriangleType Type,
	const FHairStrandsProjectionMeshData::LOD& MeshData,
	FHairStrandsRestRootResource* RestResources,
	FHairStrandsDeformedRootResource* DeformedResources)
{
	const uint32 RootCount = RestResources->BulkData.RootCount;
	if (RootCount == 0 || LODIndex < 0)
	{
		return;
	}

	if (Type == HairStrandsTriangleType::RestPose && LODIndex >= RestResources->LODs.Num())
	{
		return;
	}

	if (Type == HairStrandsTriangleType::DeformedPose && (LODIndex >= RestResources->LODs.Num() || LODIndex >= DeformedResources->LODs.Num()))
	{
		return;
	}

	FHairStrandsRestRootResource::FLOD& RestLODData = RestResources->LODs[LODIndex];
	check(RestLODData.LODIndex == LODIndex);

	const uint32 SectionCount = MeshData.Sections.Num();
	const uint32 MaxSupportedSectionCount = GetHairStrandsMaxSectionCount();
	check(SectionCount < MaxSupportedSectionCount);
	if (SectionCount == 0 || SectionCount >= MaxSupportedSectionCount)
	{
		return;
	}

	// Update the last known mesh LOD for which the root resources has been updated
	DeformedResources->MeshLODIndex = LODIndex;

	// When the number of section of a mesh is above FHairUpdateMeshTriangleCS::SectionArrayCount, the update is split into several passes
	const TArray<uint32>& ValidSectionIndices = RestResources->BulkData.GetValidSectionIndices(LODIndex);
	const uint32 ValidSectionCount = ValidSectionIndices.Num();
	const uint32 PassCount = FMath::DivideAndRoundUp(ValidSectionCount, FHairUpdateMeshTriangleCS::SectionArrayCount);
	const bool bComputePreviousDeformedPosition = IsHairStrandContinuousDecimationReorderingEnabled() && Type == HairStrandsTriangleType::DeformedPose;

	const uint32 MaxUniqueTriangleCount = RestLODData.RestUniqueTrianglePosition0Buffer.Buffer->Desc.NumElements;

	FHairUpdateMeshTriangleCS::FParameters CommonParameters;

	FRDGImportedBuffer OutputCurrBuffers[3];
	FRDGImportedBuffer OutputPrevBuffers[3];
	const bool bEnableUAVOverlap = true;
	const ERDGUnorderedAccessViewFlags UAVFlags = bEnableUAVOverlap ? ERDGUnorderedAccessViewFlags::SkipBarrier : ERDGUnorderedAccessViewFlags::None;
	CommonParameters.UniqueTriangleIndices = RegisterAsSRV(GraphBuilder, RestLODData.UniqueTriangleIndexBuffer);
	if (Type == HairStrandsTriangleType::RestPose)
	{
		OutputCurrBuffers[0] = Register(GraphBuilder, RestLODData.RestUniqueTrianglePosition0Buffer, ERDGImportedBufferFlags::CreateUAV, UAVFlags);
		OutputCurrBuffers[1] = Register(GraphBuilder, RestLODData.RestUniqueTrianglePosition1Buffer, ERDGImportedBufferFlags::CreateUAV, UAVFlags);
		OutputCurrBuffers[2] = Register(GraphBuilder, RestLODData.RestUniqueTrianglePosition2Buffer, ERDGImportedBufferFlags::CreateUAV, UAVFlags);
	}
	else if (Type == HairStrandsTriangleType::DeformedPose)
	{
		FHairStrandsDeformedRootResource::FLOD& DeformedLODData = DeformedResources->LODs[LODIndex];
		OutputCurrBuffers[0] = Register(GraphBuilder, DeformedLODData.GetDeformedUniqueTrianglePosition0Buffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateUAV, UAVFlags);
		OutputCurrBuffers[1] = Register(GraphBuilder, DeformedLODData.GetDeformedUniqueTrianglePosition1Buffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateUAV, UAVFlags);
		OutputCurrBuffers[2] = Register(GraphBuilder, DeformedLODData.GetDeformedUniqueTrianglePosition2Buffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateUAV, UAVFlags);

		if (bComputePreviousDeformedPosition)
		{
			OutputPrevBuffers[0] = Register(GraphBuilder, DeformedLODData.GetDeformedUniqueTrianglePosition0Buffer(FHairStrandsDeformedRootResource::FLOD::Previous), ERDGImportedBufferFlags::CreateUAV, UAVFlags);
			OutputPrevBuffers[1] = Register(GraphBuilder, DeformedLODData.GetDeformedUniqueTrianglePosition1Buffer(FHairStrandsDeformedRootResource::FLOD::Previous), ERDGImportedBufferFlags::CreateUAV, UAVFlags);
			OutputPrevBuffers[2] = Register(GraphBuilder, DeformedLODData.GetDeformedUniqueTrianglePosition2Buffer(FHairStrandsDeformedRootResource::FLOD::Previous), ERDGImportedBufferFlags::CreateUAV, UAVFlags);
		}

		DeformedLODData.Status = FHairStrandsDeformedRootResource::FLOD::EStatus::Completed;
	}
	else
	{
		// error
		return;
	}

	CommonParameters.OutUniqueTriangleCurrPosition0 = OutputCurrBuffers[0].UAV;
	CommonParameters.OutUniqueTriangleCurrPosition1 = OutputCurrBuffers[1].UAV;
	CommonParameters.OutUniqueTriangleCurrPosition2 = OutputCurrBuffers[2].UAV;

	if (bComputePreviousDeformedPosition)
	{
		CommonParameters.OutUniqueTrianglePrevPosition0 = OutputPrevBuffers[0].UAV;
		CommonParameters.OutUniqueTrianglePrevPosition1 = OutputPrevBuffers[1].UAV;
		CommonParameters.OutUniqueTrianglePrevPosition2 = OutputPrevBuffers[2].UAV;
	}

	for (uint32 PassIt = 0; PassIt < PassCount; ++PassIt)
	{
		FHairUpdateMeshTriangleCS::FParameters* Parameters = &CommonParameters;
		Parameters->MaxUniqueTriangleCount = MaxUniqueTriangleCount;
		Parameters->MaxSectionCount = MeshData.Sections.Num();
		Parameters->Pass_SectionStart = PassIt * FHairUpdateMeshTriangleCS::SectionArrayCount;
		Parameters->Pass_SectionCount = FMath::Min(ValidSectionCount - Parameters->Pass_SectionStart, FHairUpdateMeshTriangleCS::SectionArrayCount);

		// Most often, a skeletal mesh will have many sections, but *most* of will share the same vertex buffers. 
		// So NumSection >> NumBuffer, and the limitation of MaxSectionBufferCount = 8;
		const uint32 MaxSectionBufferCount = 8;
		struct FMeshSectionBuffers
		{
			uint32 MeshSectionBufferIndex = 0;
			FRDGBufferSRVRef RDGPositionBuffer = nullptr;
			FRDGBufferSRVRef RDGPreviousPositionBuffer = nullptr;
			FRHIShaderResourceView* PositionBuffer = nullptr;
			FRHIShaderResourceView* PreviousPositionBuffer = nullptr;
			FRHIShaderResourceView* IndexBuffer = nullptr;
			FRHIShaderResourceView* UVsBuffer = nullptr;
		};
		TMap<FRHIShaderResourceView*, FMeshSectionBuffers>	UniqueMeshSectionBuffers;
		TMap<FRDGBufferSRVRef, FMeshSectionBuffers>			UniqueMeshSectionBuffersRDG;
		uint32 UniqueMeshSectionBufferIndex = 0;

		#define SETPARAMETERS(OutParameters, InMeshSectionData, Index) \
				OutParameters->RDGMeshPositionBuffer##Index			= InMeshSectionData.RDGPositionBuffer; \
				OutParameters->RDGMeshPreviousPositionBuffer##Index = InMeshSectionData.RDGPreviousPositionBuffer; \
				OutParameters->MeshPositionBuffer##Index			= InMeshSectionData.PositionBuffer; \
				OutParameters->MeshPreviousPositionBuffer##Index	= InMeshSectionData.PreviousPositionBuffer; \
				OutParameters->MeshIndexBuffer##Index				= InMeshSectionData.IndexBuffer; \
				OutParameters->MeshUVsBuffer##Index					= InMeshSectionData.UVsBuffer;

		auto SetMeshSectionBuffers = [Parameters](uint32 UniqueIndex, const FHairStrandsProjectionMeshData::Section& MeshSectionData)
		{
			switch (UniqueIndex)
			{
			case 0: SETPARAMETERS(Parameters, MeshSectionData, 0); break;
			case 1: SETPARAMETERS(Parameters, MeshSectionData, 1); break;
			case 2: SETPARAMETERS(Parameters, MeshSectionData, 2); break;
			case 3: SETPARAMETERS(Parameters, MeshSectionData, 3); break;
			case 4: SETPARAMETERS(Parameters, MeshSectionData, 4); break;
			case 5: SETPARAMETERS(Parameters, MeshSectionData, 5); break;
			case 6: SETPARAMETERS(Parameters, MeshSectionData, 6); break;
			case 7: SETPARAMETERS(Parameters, MeshSectionData, 7); break;
			}
		};
		#undef SETPARAMETERS

		bool bUseRDGPositionBuffer = false;
		for (int32 SectionStartIt = Parameters->Pass_SectionStart, SectionItEnd = Parameters->Pass_SectionStart + Parameters->Pass_SectionCount; SectionStartIt < SectionItEnd; ++SectionStartIt)
		{
			const int32 SectionIt = SectionStartIt - Parameters->Pass_SectionStart;
			const int32 SectionIndex = ValidSectionIndices[SectionStartIt];

			if (SectionIndex < 0 || SectionIndex >= MeshData.Sections.Num())
			{
				continue;
			}

			const FHairStrandsProjectionMeshData::Section& MeshSectionData = MeshData.Sections[SectionIndex];

			// If a skel. mesh is not streaming yet, its SRV will be null
			if (MeshSectionData.IndexBuffer == nullptr)
			{
				continue;
			}

			const FMeshSectionBuffers* Buffers = nullptr;
			if (MeshSectionData.PositionBuffer)
			{
				Buffers = UniqueMeshSectionBuffers.Find(MeshSectionData.PositionBuffer);
			}
			else if (MeshSectionData.RDGPositionBuffer)
			{
				Buffers = UniqueMeshSectionBuffersRDG.Find(MeshSectionData.RDGPositionBuffer);
			}
			else
			{
				check(false); // Should never happen
				continue;
			}

			if (Buffers != nullptr)
			{
				// Insure that all buffers actually match
				check(Buffers->RDGPositionBuffer == MeshSectionData.RDGPositionBuffer);
				check(Buffers->RDGPreviousPositionBuffer == MeshSectionData.RDGPreviousPositionBuffer);
				check(Buffers->PositionBuffer == MeshSectionData.PositionBuffer);
				check(Buffers->PreviousPositionBuffer == MeshSectionData.PreviousPositionBuffer);
				check(Buffers->IndexBuffer == MeshSectionData.IndexBuffer);
				check(Buffers->UVsBuffer == MeshSectionData.UVsBuffer);

				SetMeshSectionBufferIndex(*Parameters, SectionIt, Buffers->MeshSectionBufferIndex);
			}
			else
			{
				// Only support 8 unique different buffer at the moment
				check(UniqueMeshSectionBufferIndex < MaxSectionBufferCount);
				SetMeshSectionBuffers(UniqueMeshSectionBufferIndex, MeshSectionData);

				FMeshSectionBuffers Entry;
				Entry.MeshSectionBufferIndex = UniqueMeshSectionBufferIndex;
				Entry.RDGPositionBuffer = MeshSectionData.RDGPositionBuffer;
				Entry.RDGPreviousPositionBuffer = MeshSectionData.RDGPreviousPositionBuffer;
				Entry.PositionBuffer = MeshSectionData.PositionBuffer;
				Entry.PreviousPositionBuffer = MeshSectionData.PreviousPositionBuffer;
				Entry.IndexBuffer = MeshSectionData.IndexBuffer;
				Entry.UVsBuffer = MeshSectionData.UVsBuffer;

				if (MeshSectionData.PositionBuffer)
				{
					UniqueMeshSectionBuffers.Add(MeshSectionData.PositionBuffer, Entry);
				}
				else if (MeshSectionData.RDGPositionBuffer)
				{
					UniqueMeshSectionBuffersRDG.Add(MeshSectionData.RDGPositionBuffer, Entry);
				}

				SetMeshSectionBufferIndex(*Parameters, SectionIt, UniqueMeshSectionBufferIndex);
				++UniqueMeshSectionBufferIndex;
			}

			SetMeshSectionIndices(*Parameters, SectionIt, MeshSectionData.SectionIndex);
			SetMeshMaxIndexCount(*Parameters, SectionIt, MeshSectionData.TotalIndexCount);
			SetMeshMaxVertexCount(*Parameters, SectionIt, MeshSectionData.TotalVertexCount);
			SetMeshIndexOffset(*Parameters, SectionIt, MeshSectionData.IndexBaseIndex);
			SetMeshUVsChannelOffset(*Parameters, SectionIt, MeshSectionData.UVsChannelOffset);
			SetMeshUVsChannelCount(*Parameters, SectionIt, MeshSectionData.UVsChannelCount);

			// Sanity check
			// If one of the input is using RDG position, we expect all mesh sections to use RDG input
			if (bUseRDGPositionBuffer)
			{
				check(MeshSectionData.RDGPositionBuffer != nullptr);
			}
			else if (MeshSectionData.RDGPositionBuffer != nullptr)
			{
				bUseRDGPositionBuffer = true;
			}
		}

		if (MeshData.Sections.Num() > 0)
		{
			for (uint32 Index = UniqueMeshSectionBufferIndex; Index < MaxSectionBufferCount; ++Index)
			{
				SetMeshSectionBuffers(Index, MeshData.Sections[0]);
			}
		}

		if (UniqueMeshSectionBufferIndex == 0 || Parameters->MeshUVsBuffer0 == nullptr)
		{
			return;
		}

		FHairUpdateMeshTriangleCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FHairUpdateMeshTriangleCS::FParameters>();
		*PassParameters = *Parameters;

		FHairUpdateMeshTriangleCS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FHairUpdateMeshTriangleCS::FUpdateUVs>(1);
		PermutationVector.Set<FHairUpdateMeshTriangleCS::FPrevious>(bComputePreviousDeformedPosition);
		PermutationVector.Set<FHairUpdateMeshTriangleCS::FPositionType>(bUseRDGPositionBuffer ? 1 : 0);

		const FIntVector DispatchGroupCount = FComputeShaderUtils::GetGroupCount(RootCount, 128);
		check(DispatchGroupCount.X < 65536);
		TShaderMapRef<FHairUpdateMeshTriangleCS> ComputeShader(ShaderMap, PermutationVector);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("HairStrands::TriangleMeshUpdate(%s,%s)", bComputePreviousDeformedPosition ? TEXT("Previous/Current, Current") : TEXT("Current"), bUseRDGPositionBuffer ? TEXT("RDGBuffer") : TEXT("SkinCacheBuffer")),
			ComputeShader,
			PassParameters,
			DispatchGroupCount);
	}

	GraphBuilder.SetBufferAccessFinal(OutputCurrBuffers[0].Buffer, ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(OutputCurrBuffers[1].Buffer, ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(OutputCurrBuffers[2].Buffer, ERHIAccess::SRVMask);

	if (bComputePreviousDeformedPosition)
	{
		GraphBuilder.SetBufferAccessFinal(OutputPrevBuffers[0].Buffer, ERHIAccess::SRVMask);
		GraphBuilder.SetBufferAccessFinal(OutputPrevBuffers[1].Buffer, ERHIAccess::SRVMask);
		GraphBuilder.SetBufferAccessFinal(OutputPrevBuffers[2].Buffer, ERHIAccess::SRVMask);
	}

}

///////////////////////////////////////////////////////////////////////////////////////////////////

class FHairMeshesInterpolateCS : public FGlobalShader
{
private:
	DECLARE_GLOBAL_SHADER(FHairMeshesInterpolateCS);
	SHADER_USE_PARAMETER_STRUCT(FHairMeshesInterpolateCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, VertexCount)
		SHADER_PARAMETER(uint32, MaxSampleCount)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RestSamplePositionsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, MeshSampleWeightsBuffer)

		SHADER_PARAMETER_SRV(Buffer, RestPositionBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, OutDeformedPositionBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::All, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_HAIRMESHES"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairMeshesInterpolateCS, "/Engine/Private/HairStrands/HairStrandsMesh.usf", "MainHairMeshesCS", SF_Compute);

template<typename TRestResource, typename TDeformedResource>
void InternalAddHairRBFInterpolationPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const bool bCards,
	const int32 MeshLODIndex,
	TRestResource* RestResources,
	TDeformedResource* DeformedResources,
	FHairStrandsRestRootResource* RestRootResources,
	FHairStrandsDeformedRootResource* DeformedRootResources)
{
	const uint32 VertexCount = RestResources ? RestResources->GetVertexCount() : 0;
	if (!RestResources || !DeformedResources || !RestRootResources || !DeformedRootResources || VertexCount == 0 || MeshLODIndex >= RestRootResources->LODs.Num() || MeshLODIndex < 0)
	{
		return;
	}

	// Copy current to previous position before update the current position. This allows to have correct motion vectors.
	FRDGImportedBuffer DeformedPositionBuffer_Curr = Register(GraphBuilder, DeformedResources->GetBuffer(TDeformedResource::EFrameType::Current), ERDGImportedBufferFlags::CreateUAV);
	FRDGImportedBuffer DeformedPositionBuffer_Prev = Register(GraphBuilder, DeformedResources->GetBuffer(TDeformedResource::EFrameType::Previous), ERDGImportedBufferFlags::CreateUAV);
	AddCopyBufferPass(GraphBuilder, DeformedPositionBuffer_Prev.Buffer, DeformedPositionBuffer_Curr.Buffer);

	FHairStrandsRestRootResource::FLOD& RestLODData = RestRootResources->LODs[MeshLODIndex];
	FHairStrandsDeformedRootResource::FLOD& DeformedLODData = DeformedRootResources->LODs[MeshLODIndex];
	FHairMeshesInterpolateCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairMeshesInterpolateCS::FParameters>();
	Parameters->VertexCount = VertexCount;
	Parameters->MaxSampleCount = RestLODData.SampleCount;

	Parameters->RestPositionBuffer			= RestResources->RestPositionBuffer.ShaderResourceViewRHI;
	Parameters->OutDeformedPositionBuffer	= DeformedPositionBuffer_Curr.UAV;

	Parameters->RestSamplePositionsBuffer	= RegisterAsSRV(GraphBuilder, RestLODData.RestSamplePositionsBuffer);
	Parameters->MeshSampleWeightsBuffer		= RegisterAsSRV(GraphBuilder, DeformedLODData.GetMeshSampleWeightsBuffer(FHairStrandsDeformedRootResource::FLOD::Current));

	const FIntVector DispatchGroupCount = FComputeShaderUtils::GetGroupCount(VertexCount, 128);
	check(DispatchGroupCount.X < 65536);
	TShaderMapRef<FHairMeshesInterpolateCS> ComputeShader(ShaderMap);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("HairStrands::HairInterpolationRBF(%s)", bCards ? TEXT("Cards") : TEXT("Meshes")),
		ComputeShader,
		Parameters,
		DispatchGroupCount);

	GraphBuilder.SetBufferAccessFinal(DeformedPositionBuffer_Curr.Buffer, ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(DeformedPositionBuffer_Prev.Buffer, ERHIAccess::SRVMask);
	
}

void AddHairMeshesRBFInterpolationPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const int32 MeshLODIndex,
	FHairMeshesRestResource* RestResources,
	FHairMeshesDeformedResource* DeformedResources,
	FHairStrandsRestRootResource* RestRootResources,
	FHairStrandsDeformedRootResource* DeformedRootResources)
{
	InternalAddHairRBFInterpolationPass(
		GraphBuilder,
		ShaderMap,
		false,
		MeshLODIndex,
		RestResources,
		DeformedResources,
		RestRootResources,
		DeformedRootResources);
}

void AddHairCardsRBFInterpolationPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const int32 MeshLODIndex,
	FHairCardsRestResource* RestResources,
	FHairCardsDeformedResource* DeformedResources,
	FHairStrandsRestRootResource* RestRootResources,
	FHairStrandsDeformedRootResource* DeformedRootResources)
{
	InternalAddHairRBFInterpolationPass(
		GraphBuilder,
		ShaderMap,
		true,
		MeshLODIndex,
		RestResources,
		DeformedResources,
		RestRootResources,
		DeformedRootResources);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
class FHairInitMeshSamplesCS : public FGlobalShader
{
public:
	const static uint32 SectionArrayCount = 16; // This defines the number of sections managed for each iteration pass
private:
	DECLARE_GLOBAL_SHADER(FHairInitMeshSamplesCS);
	SHADER_USE_PARAMETER_STRUCT(FHairInitMeshSamplesCS, FGlobalShader);

	class FPositionType : SHADER_PERMUTATION_INT("PERMUTATION_POSITION_TYPE", 2);
	using FPermutationDomain = TShaderPermutationDomain<FPositionType>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MaxSampleCount)
		SHADER_PARAMETER(uint32, MaxVertexCount)
		SHADER_PARAMETER(uint32, PassSectionCount)

		SHADER_PARAMETER_ARRAY(FUintVector4, PackedSectionScalars, [SectionArrayCount])

		SHADER_PARAMETER_SRV(Buffer, VertexPositionsBuffer0)
		SHADER_PARAMETER_SRV(Buffer, VertexPositionsBuffer1)
		SHADER_PARAMETER_SRV(Buffer, VertexPositionsBuffer2)
		SHADER_PARAMETER_SRV(Buffer, VertexPositionsBuffer3)
		SHADER_PARAMETER_SRV(Buffer, VertexPositionsBuffer4)
		SHADER_PARAMETER_SRV(Buffer, VertexPositionsBuffer5)
		SHADER_PARAMETER_SRV(Buffer, VertexPositionsBuffer6)
		SHADER_PARAMETER_SRV(Buffer, VertexPositionsBuffer7)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGVertexPositionsBuffer0)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGVertexPositionsBuffer1)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGVertexPositionsBuffer2)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGVertexPositionsBuffer3)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGVertexPositionsBuffer4)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGVertexPositionsBuffer5)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGVertexPositionsBuffer6)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RDGVertexPositionsBuffer7)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, SampleIndicesBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, OutSamplePositionsBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::All, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("MAX_SECTION_COUNT"), SectionArrayCount);
		OutEnvironment.SetDefine(TEXT("SHADER_SAMPLE_INIT"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairInitMeshSamplesCS, "/Engine/Private/HairStrands/HairStrandsMesh.usf", "MainCS", SF_Compute);

// Manual packing for mesh section scalars.  MUST MATCH WITH HairStrandsMesh.usf
// PackedSectionScalars: SectionVertexOffset, SectionVertexCount, SectionBufferIndex, *free*
inline void SetSectionVertexOffset(FHairInitMeshSamplesCS::FParameters& Params, uint32 SectionIndex, uint32 SectionVertexOffset) { Params.PackedSectionScalars[SectionIndex].X = SectionVertexOffset; }
inline void SetSectionVertexCount(FHairInitMeshSamplesCS::FParameters& Params, uint32 SectionIndex, uint32 SectionVertexCount)   { Params.PackedSectionScalars[SectionIndex].Y = SectionVertexCount; }
inline void SetSectionBufferIndex(FHairInitMeshSamplesCS::FParameters& Params, uint32 SectionIndex, uint32 SectionBufferIndex)   { Params.PackedSectionScalars[SectionIndex].Z = SectionBufferIndex; }



void AddHairStrandInitMeshSamplesPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const int32 LODIndex,
	const HairStrandsTriangleType Type,
	const FHairStrandsProjectionMeshData::LOD& MeshData,
	FHairStrandsRestRootResource* RestResources,
	FHairStrandsDeformedRootResource* DeformedResources)
{
	if (LODIndex < 0)
	{
		return;
	}

	if (Type == HairStrandsTriangleType::RestPose && LODIndex >= RestResources->LODs.Num())
	{
		return;
	}

	if (Type == HairStrandsTriangleType::DeformedPose && (LODIndex >= RestResources->LODs.Num() || LODIndex >= DeformedResources->LODs.Num()))
	{
		return;
	}

	FHairStrandsRestRootResource::FLOD& RestLODData = RestResources->LODs[LODIndex];
	check(RestLODData.LODIndex == LODIndex);

	const uint32 SectionCount = MeshData.Sections.Num();
	const uint32 MaxSupportedSectionCount = GetHairStrandsMaxSectionCount();
	check(SectionCount < MaxSupportedSectionCount);
	if (SectionCount == 0 || SectionCount >= MaxSupportedSectionCount)
	{
		return;
	}

	// When the number of section of a mesh is above FHairUpdateMeshTriangleCS::SectionArrayCount, the update is split into several passes
	const TArray<uint32>& ValidSectionIndices = RestResources->BulkData.GetValidSectionIndices(LODIndex);
	const uint32 ValidSectionCount = ValidSectionIndices.Num();
	const uint32 PassCount = FMath::DivideAndRoundUp(ValidSectionCount, FHairInitMeshSamplesCS::SectionArrayCount);

	if (SectionCount > 0 && RestLODData.SampleCount > 0)
	{
		FRDGImportedBuffer OutBuffer;
		if (Type == HairStrandsTriangleType::RestPose)
		{
			OutBuffer = Register(GraphBuilder, RestLODData.RestSamplePositionsBuffer, ERDGImportedBufferFlags::CreateUAV);
		}
		else if (Type == HairStrandsTriangleType::DeformedPose)
		{
			FHairStrandsDeformedRootResource::FLOD& DeformedLODData = DeformedResources->LODs[LODIndex];
			check(DeformedLODData.LODIndex == LODIndex);

			OutBuffer = Register(GraphBuilder, DeformedLODData.GetDeformedSamplePositionsBuffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateUAV);
		}
		else
		{
			return;
		}
		for (uint32 PassIt = 0; PassIt < PassCount; ++PassIt)
		{
			FHairInitMeshSamplesCS::FParameters Parameters;

			for (uint32 DataIndex = 0; DataIndex < FHairInitMeshSamplesCS::SectionArrayCount; ++DataIndex)
			{
				SetSectionBufferIndex(Parameters, DataIndex, 0);
				SetSectionVertexOffset(Parameters, DataIndex, 0);
				SetSectionVertexCount(Parameters, DataIndex, 0);
			}

			const int32 PassSectionStart = PassIt * FHairInitMeshSamplesCS::SectionArrayCount;
			const int32 PassSectionCount = FMath::Min(ValidSectionCount - PassSectionStart, FHairInitMeshSamplesCS::SectionArrayCount);
			Parameters.PassSectionCount = PassSectionCount;

			struct FMeshSectionBuffers
			{
				uint32 SectionBufferIndex = 0;
				FRDGBufferSRVRef RDGPositionBuffer = nullptr;
				FRDGBufferSRVRef RDGPreviousPositionBuffer = nullptr;
				FRHIShaderResourceView* PositionBuffer = nullptr;
				FRHIShaderResourceView* PreviousPositionBuffer = nullptr;
			};
			TMap<FRHIShaderResourceView*, FMeshSectionBuffers>	UniqueMeshSectionBuffers;
			TMap<FRDGBufferSRVRef, FMeshSectionBuffers>		UniqueMeshSectionBuffersRDG;

			#define SETPARAMETERS(OutParameters, InMeshSectionData, Index) \
				OutParameters.RDGVertexPositionsBuffer##Index = InMeshSectionData.RDGPositionBuffer; \
				OutParameters.VertexPositionsBuffer##Index = InMeshSectionData.PositionBuffer;

			auto SetMeshSectionBuffers = [&Parameters](uint32 UniqueIndex, const FHairStrandsProjectionMeshData::Section& MeshSectionData)
			{
				switch (UniqueIndex)
				{
				case 0: SETPARAMETERS(Parameters, MeshSectionData, 0); break;
				case 1: SETPARAMETERS(Parameters, MeshSectionData, 1); break;
				case 2: SETPARAMETERS(Parameters, MeshSectionData, 2); break;
				case 3: SETPARAMETERS(Parameters, MeshSectionData, 3); break;
				case 4: SETPARAMETERS(Parameters, MeshSectionData, 4); break;
				case 5: SETPARAMETERS(Parameters, MeshSectionData, 5); break;
				case 6: SETPARAMETERS(Parameters, MeshSectionData, 6); break;
				case 7: SETPARAMETERS(Parameters, MeshSectionData, 7); break;
				}
			};

			#undef SETPARAMETERS

			uint32 UniqueMeshSectionBufferIndex = 0;
			bool bUseRDGPositionBuffer = false;
			for (int32 SectionStartIt = PassSectionStart, SectionItEnd = PassSectionStart + PassSectionCount; SectionStartIt < SectionItEnd; ++SectionStartIt)
			{
				const int32 SectionIt = SectionStartIt - PassSectionStart;
				const int32 SectionIndex = ValidSectionIndices[SectionStartIt];

				const FHairStrandsProjectionMeshData::Section& MeshSectionData = MeshData.Sections[SectionIndex];

				SetSectionVertexOffset(Parameters, SectionIt, MeshSectionData.VertexBaseIndex);
				SetSectionVertexCount(Parameters, SectionIt, MeshSectionData.NumVertices);

				const FMeshSectionBuffers* Buffers = nullptr;
				if (MeshSectionData.PositionBuffer)
				{
					Buffers = UniqueMeshSectionBuffers.Find(MeshSectionData.PositionBuffer);
				}
				else if (MeshSectionData.RDGPositionBuffer)
				{
					Buffers = UniqueMeshSectionBuffersRDG.Find(MeshSectionData.RDGPositionBuffer);
				}
				else
				{
					check(false); // Should never happen
					continue;
				}
				if (Buffers != nullptr)
				{
					SetSectionBufferIndex(Parameters, SectionIt, Buffers->SectionBufferIndex);
				}
				else
				{
					// Only support 8 unique different buffer at the moment
					check(UniqueMeshSectionBufferIndex < 8);
					SetMeshSectionBuffers(UniqueMeshSectionBufferIndex, MeshSectionData);

					FMeshSectionBuffers Entry;
					Entry.SectionBufferIndex = UniqueMeshSectionBufferIndex;
					Entry.RDGPositionBuffer = MeshSectionData.RDGPositionBuffer;
					Entry.PositionBuffer = MeshSectionData.PositionBuffer;

					if (MeshSectionData.PositionBuffer)
					{
						UniqueMeshSectionBuffers.Add(MeshSectionData.PositionBuffer, Entry);
					}
					else if (MeshSectionData.RDGPositionBuffer)
					{
						UniqueMeshSectionBuffersRDG.Add(MeshSectionData.RDGPositionBuffer, Entry);
					}

					SetSectionBufferIndex(Parameters, SectionIt, UniqueMeshSectionBufferIndex);
					++UniqueMeshSectionBufferIndex;
				}

				// Sanity check
				// If one of the input is using RDG position, we expect all mesh sections to use RDG input
				if (bUseRDGPositionBuffer)
				{
					check(MeshSectionData.RDGPositionBuffer != nullptr);
				}
				else if (MeshSectionData.RDGPositionBuffer != nullptr)
				{
					bUseRDGPositionBuffer = true;
				}
			}

			if (MeshData.Sections.Num() > 0)
			{
				for (uint32 Index = UniqueMeshSectionBufferIndex; Index < 8; ++Index)
				{
					SetMeshSectionBuffers(Index, MeshData.Sections[0]);
				}
			}

			if (UniqueMeshSectionBufferIndex == 0)
			{
				return;
			}
			Parameters.MaxVertexCount = MeshData.Sections[0].TotalVertexCount;
			Parameters.MaxSampleCount = RestLODData.SampleCount;
			Parameters.SampleIndicesBuffer = RegisterAsSRV(GraphBuilder, RestLODData.MeshSampleIndicesBuffer);
			Parameters.OutSamplePositionsBuffer = OutBuffer.UAV;

			FHairInitMeshSamplesCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FHairInitMeshSamplesCS::FParameters>();
			*PassParameters = Parameters;

			FHairInitMeshSamplesCS::FPermutationDomain PermutationVector;
			PermutationVector.Set<FHairInitMeshSamplesCS::FPositionType>(bUseRDGPositionBuffer ? 1 : 0);

			const FIntVector DispatchGroupCount = FComputeShaderUtils::GetGroupCount(RestLODData.SampleCount, 128);
			check(DispatchGroupCount.X < 65536);
			TShaderMapRef<FHairInitMeshSamplesCS> ComputeShader(ShaderMap, PermutationVector);

			FComputeShaderUtils::AddPass(
				GraphBuilder,
				RDG_EVENT_NAME("HairStrandsInitMeshSamples"),
				ComputeShader,
				PassParameters,
				DispatchGroupCount);
		}
		GraphBuilder.SetBufferAccessFinal(OutBuffer.Buffer, ERHIAccess::SRVMask);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
//
class FHairUpdateMeshSamplesCS : public FGlobalShader
{
private:
	DECLARE_GLOBAL_SHADER(FHairUpdateMeshSamplesCS);
	SHADER_USE_PARAMETER_STRUCT(FHairUpdateMeshSamplesCS, FGlobalShader);

	using FPermutationDomain = TShaderPermutationDomain<>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, MaxSampleCount)

		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, SampleIndicesBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, InterpolationWeightsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, SampleRestPositionsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, SampleDeformedPositionsBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer, OutSampleDeformationsBuffer)
		END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::All, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_SAMPLE_UPDATE"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairUpdateMeshSamplesCS, "/Engine/Private/HairStrands/HairStrandsMesh.usf", "MainCS", SF_Compute);

void AddHairStrandUpdateMeshSamplesPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const int32 LODIndex,
	const FHairStrandsProjectionMeshData::LOD& MeshData,
	FHairStrandsRestRootResource* RestResources,
	FHairStrandsDeformedRootResource* DeformedResources)
{
	if (LODIndex < 0 || LODIndex >= RestResources->LODs.Num() || LODIndex >= DeformedResources->LODs.Num())
	{
		return;
	}

	FHairStrandsRestRootResource::FLOD& RestLODData = RestResources->LODs[LODIndex];
	FHairStrandsDeformedRootResource::FLOD& DeformedLODData = DeformedResources->LODs[LODIndex];
	check(RestLODData.LODIndex == LODIndex);
	check(DeformedLODData.LODIndex == LODIndex);

	const uint32 SectionCount = MeshData.Sections.Num();
	if (SectionCount > 0 && RestLODData.SampleCount > 0)
	{
		FHairUpdateMeshSamplesCS::FParameters* Parameters = GraphBuilder.AllocParameters<FHairUpdateMeshSamplesCS::FParameters>();

		FRDGImportedBuffer OutWeightsBuffer = Register(GraphBuilder, DeformedLODData.GetMeshSampleWeightsBuffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateUAV);

		Parameters->MaxSampleCount					= RestLODData.SampleCount;
		Parameters->SampleIndicesBuffer				= RegisterAsSRV(GraphBuilder, RestLODData.MeshSampleIndicesBuffer);
		Parameters->InterpolationWeightsBuffer		= RegisterAsSRV(GraphBuilder, RestLODData.MeshInterpolationWeightsBuffer);
		Parameters->SampleRestPositionsBuffer		= RegisterAsSRV(GraphBuilder, RestLODData.RestSamplePositionsBuffer);
		Parameters->SampleDeformedPositionsBuffer	= RegisterAsSRV(GraphBuilder, DeformedLODData.GetDeformedSamplePositionsBuffer(FHairStrandsDeformedRootResource::FLOD::Current));
		Parameters->OutSampleDeformationsBuffer		= OutWeightsBuffer.UAV;

		const FIntVector DispatchGroupCount = FComputeShaderUtils::GetGroupCount(RestLODData.SampleCount+4, 128);
		check(DispatchGroupCount.X < 65536);
		TShaderMapRef<FHairUpdateMeshSamplesCS> ComputeShader(ShaderMap);
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("HairStrands::UpdateMeshSamples"),
			ComputeShader,
			Parameters,
			DispatchGroupCount);

		GraphBuilder.SetBufferAccessFinal(OutWeightsBuffer.Buffer, ERHIAccess::SRVMask);
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////

// Generate follicle mask texture
BEGIN_SHADER_PARAMETER_STRUCT(FHairFollicleMaskParameters, )
	SHADER_PARAMETER(FVector2f, OutputResolution)
	SHADER_PARAMETER(uint32, MaxRootCount)
	SHADER_PARAMETER(uint32, MaxUniqueTriangleIndex)
	SHADER_PARAMETER(uint32, Channel)
	SHADER_PARAMETER(uint32, KernelSizeInPixels)

	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, UniqueTrianglePosition0Buffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, UniqueTrianglePosition1Buffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, UniqueTrianglePosition2Buffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RootToUniqueTriangleIndexBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RootBarycentricBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer, RootUVsBuffer)

	SHADER_PARAMETER_STRUCT_REF(FViewUniformShaderParameters, ViewUniformBuffer)
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FHairFollicleMask : public FGlobalShader
{
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsHairStrandsSupported(EHairStrandsShaderType::Tool, Parameters.Platform);
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_FOLLICLE_MASK"), 1);
	}

	FHairFollicleMask() = default;
	FHairFollicleMask(const CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer) {}
};

class FHairFollicleMaskVS : public FHairFollicleMask
{
	DECLARE_GLOBAL_SHADER(FHairFollicleMaskVS);
	SHADER_USE_PARAMETER_STRUCT(FHairFollicleMaskVS, FHairFollicleMask);

	class FUVType : SHADER_PERMUTATION_INT("PERMUTATION_UV_TYPE", 2);
	using FPermutationDomain = TShaderPermutationDomain<FUVType>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FHairFollicleMaskParameters, Pass)
	END_SHADER_PARAMETER_STRUCT()
};

class FHairFollicleMaskPS : public FHairFollicleMask
{
	DECLARE_GLOBAL_SHADER(FHairFollicleMaskPS);
	SHADER_USE_PARAMETER_STRUCT(FHairFollicleMaskPS, FHairFollicleMask);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FHairFollicleMaskParameters, Pass)
	END_SHADER_PARAMETER_STRUCT()
};

IMPLEMENT_GLOBAL_SHADER(FHairFollicleMaskPS, "/Engine/Private/HairStrands/HairStrandsFollicleMask.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FHairFollicleMaskVS, "/Engine/Private/HairStrands/HairStrandsFollicleMask.usf", "MainVS", SF_Vertex);

static void AddFollicleMaskPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const bool bNeedClear,
	const uint32 KernelSizeInPixels,
	const uint32 Channel,
	const uint32 LODIndex,
	FHairStrandsRestRootResource* RestResources,
	FRDGTextureRef OutTexture)
{
	const uint32 RootCount = RestResources->BulkData.RootCount;
	if (LODIndex >= uint32(RestResources->LODs.Num()) || RootCount == 0)
		return;

	FHairStrandsRestRootResource::FLOD& LODData = RestResources->LODs[LODIndex];
	if (!LODData.RootBarycentricBuffer.Buffer ||
		!LODData.RestUniqueTrianglePosition0Buffer.Buffer ||
		!LODData.RestUniqueTrianglePosition1Buffer.Buffer ||
		!LODData.RestUniqueTrianglePosition2Buffer.Buffer)
		return;

	const FIntPoint OutputResolution = OutTexture->Desc.Extent;
	FHairFollicleMaskParameters* Parameters = GraphBuilder.AllocParameters<FHairFollicleMaskParameters>();
	Parameters->UniqueTrianglePosition0Buffer = RegisterAsSRV(GraphBuilder, LODData.RestUniqueTrianglePosition0Buffer);
	Parameters->UniqueTrianglePosition1Buffer = RegisterAsSRV(GraphBuilder, LODData.RestUniqueTrianglePosition1Buffer);
	Parameters->UniqueTrianglePosition2Buffer = RegisterAsSRV(GraphBuilder, LODData.RestUniqueTrianglePosition2Buffer);
	Parameters->RootToUniqueTriangleIndexBuffer = RegisterAsSRV(GraphBuilder, LODData.RootToUniqueTriangleIndexBuffer);
	Parameters->RootBarycentricBuffer   = RegisterAsSRV(GraphBuilder, LODData.RootBarycentricBuffer);
	Parameters->RootUVsBuffer = nullptr;
	Parameters->OutputResolution = OutputResolution;
	Parameters->MaxRootCount = RootCount;
	Parameters->MaxUniqueTriangleIndex = RestResources->BulkData.MeshProjectionLODs[LODIndex].UniqueTriangleCount;
	Parameters->Channel = FMath::Min(Channel, 3u);
	Parameters->KernelSizeInPixels = FMath::Clamp(KernelSizeInPixels, 2u, 200u);
	Parameters->RenderTargets[0] = FRenderTargetBinding(OutTexture, bNeedClear ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad, 0);

	FHairFollicleMaskVS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FHairFollicleMaskVS::FUVType>(0); // Mesh UVs

	TShaderMapRef<FHairFollicleMaskVS> VertexShader(ShaderMap, PermutationVector);
	TShaderMapRef<FHairFollicleMaskPS> PixelShader(ShaderMap);
	FHairFollicleMaskVS::FParameters ParametersVS;
	FHairFollicleMaskPS::FParameters ParametersPS;
	ParametersVS.Pass = *Parameters;
	ParametersPS.Pass = *Parameters;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("HairStrands::FollicleMask"),
		Parameters,
		ERDGPassFlags::Raster,
		[Parameters, ParametersVS, ParametersPS, VertexShader, PixelShader, OutputResolution](FRHICommandList& RHICmdList)
	{

		RHICmdList.SetViewport(0, 0, 0.0f, OutputResolution.X, OutputResolution.Y, 1.0f);

		// Apply additive blending pipeline state.
		FGraphicsPipelineStateInitializer GraphicsPSOInit;
		RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
		GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Max, BF_SourceColor, BF_DestColor, BO_Max, BF_One, BF_One>::GetRHI();
		GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
		GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
		GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
		GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
		GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
		GraphicsPSOInit.PrimitiveType = PT_TriangleList;
		SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

		SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), ParametersVS);
		SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), ParametersPS);

		// Emit an instanced quad draw call on the order of the number of pixels on the screen.	
		RHICmdList.DrawPrimitive(0, Parameters->MaxRootCount, 1);
	});
}

static void AddFollicleMaskPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const bool bNeedClear,
	const uint32 KernelSizeInPixels,
	const uint32 Channel,
	const uint32 RootCount,
	const FRDGBufferRef RootUVBuffer,
	FRDGTextureRef OutTexture)
{
	const FIntPoint OutputResolution = OutTexture->Desc.Extent;
	FHairFollicleMaskParameters* Parameters = GraphBuilder.AllocParameters<FHairFollicleMaskParameters>();
	Parameters->UniqueTrianglePosition0Buffer = nullptr;
	Parameters->UniqueTrianglePosition1Buffer = nullptr;
	Parameters->UniqueTrianglePosition2Buffer = nullptr;
	Parameters->RootBarycentricBuffer = nullptr;
	Parameters->RootUVsBuffer = GraphBuilder.CreateSRV(RootUVBuffer, PF_G32R32F);
	Parameters->OutputResolution = OutputResolution;
	Parameters->MaxRootCount = RootCount;
	Parameters->MaxUniqueTriangleIndex = 0;
	Parameters->Channel = FMath::Min(Channel, 3u);
	Parameters->KernelSizeInPixels = FMath::Clamp(KernelSizeInPixels, 2u, 200u);
	Parameters->RenderTargets[0] = FRenderTargetBinding(OutTexture, bNeedClear ? ERenderTargetLoadAction::EClear : ERenderTargetLoadAction::ELoad, 0);

	FHairFollicleMaskVS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FHairFollicleMaskVS::FUVType>(1); // Groom root's UV

	TShaderMapRef<FHairFollicleMaskVS> VertexShader(ShaderMap, PermutationVector);
	TShaderMapRef<FHairFollicleMaskPS> PixelShader(ShaderMap);
	FHairFollicleMaskVS::FParameters ParametersVS;
	FHairFollicleMaskPS::FParameters ParametersPS;
	ParametersVS.Pass = *Parameters;
	ParametersPS.Pass = *Parameters;

	GraphBuilder.AddPass(
		RDG_EVENT_NAME("HairStrands::FollicleMask"),
		Parameters,
		ERDGPassFlags::Raster,
		[Parameters, ParametersVS, ParametersPS, VertexShader, PixelShader, OutputResolution](FRHICommandList& RHICmdList)
		{

			RHICmdList.SetViewport(0, 0, 0.0f, OutputResolution.X, OutputResolution.Y, 1.0f);

			// Apply additive blending pipeline state.
			FGraphicsPipelineStateInitializer GraphicsPSOInit;
			RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);
			GraphicsPSOInit.BlendState = TStaticBlendState<CW_RGBA, BO_Max, BF_SourceColor, BF_DestColor, BO_Max, BF_One, BF_One>::GetRHI();
			GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
			GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
			GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
			GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
			GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
			GraphicsPSOInit.PrimitiveType = PT_TriangleList;
			SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, 0);

			SetShaderParameters(RHICmdList, VertexShader, VertexShader.GetVertexShader(), ParametersVS);
			SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), ParametersPS);

			// Emit an instanced quad draw call on the order of the number of pixels on the screen.	
			RHICmdList.DrawPrimitive(0, Parameters->MaxRootCount, 1);
		});
}

void GenerateFolliculeMask(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const EPixelFormat Format,
	const FIntPoint Resolution,
	const uint32 MipCount,
	const uint32 KernelSizeInPixels,
	const uint32 Channel,
	const int32 LODIndex,
	FHairStrandsRestRootResource* RestResources,
	FRDGTextureRef& OutTexture)
{
	const FLinearColor ClearColor(0.0f, 0.f, 0.f, 0.f);

	bool bClear = OutTexture == nullptr;
	if (OutTexture == nullptr)
	{
		FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(Resolution, Format, FClearValueBinding(ClearColor), TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, MipCount);
		OutTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("Hair.FollicleMask"));
	}

	AddFollicleMaskPass(GraphBuilder, ShaderMap, bClear, KernelSizeInPixels, Channel, LODIndex, RestResources, OutTexture);
}

void GenerateFolliculeMask(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const EPixelFormat Format,
	const FIntPoint Resolution,
	const uint32 MipCount,
	const uint32 KernelSizeInPixels,
	const uint32 Channel,
	const TArray<FRDGBufferRef>& RootUVBuffers,
	FRDGTextureRef& OutTexture)
{
	const FLinearColor ClearColor(0.0f, 0.f, 0.f, 0.f);

	bool bClear = OutTexture == nullptr;
	if (OutTexture == nullptr)
	{
		FRDGTextureDesc OutputDesc = FRDGTextureDesc::Create2D(Resolution, Format, FClearValueBinding(ClearColor), TexCreate_ShaderResource | TexCreate_RenderTargetable | TexCreate_UAV, MipCount);
		OutTexture = GraphBuilder.CreateTexture(OutputDesc, TEXT("Hair.FollicleMask"));
	}

	for (const FRDGBufferRef& RootUVBuffer : RootUVBuffers)
	{
		const uint32 RootCount = RootUVBuffer->Desc.NumElements;
		AddFollicleMaskPass(GraphBuilder, ShaderMap, bClear, KernelSizeInPixels, Channel, RootCount, RootUVBuffer, OutTexture);
		bClear = false;
	}
}

class FGenerateMipCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FGenerateMipCS);
	SHADER_USE_PARAMETER_STRUCT(FGenerateMipCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(uint32, Resolution)
		SHADER_PARAMETER(uint32, SourceMip)
		SHADER_PARAMETER(uint32, TargetMip)

		SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D, InTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, OutTexture)
		SHADER_PARAMETER_SAMPLER(SamplerState, LinearSampler)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) { return IsHairStrandsSupported(EHairStrandsShaderType::Tool, Parameters.Platform); }
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_GENERATE_MIPS"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FGenerateMipCS, "/Engine/Private/HairStrands/HairStrandsFollicleMask.usf", "MainCS", SF_Compute);

void AddComputeMipsPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	FRDGTextureRef& OutTexture)
{
	check(OutTexture->Desc.Extent.X == OutTexture->Desc.Extent.Y);
	const uint32 Resolution = OutTexture->Desc.Extent.X;
	const uint32 MipCount = OutTexture->Desc.NumMips;
	for (uint32 MipIt = 0; MipIt < MipCount - 1; ++MipIt)
	{
		const uint32 SourceMipIndex = MipIt;
		const uint32 TargetMipIndex = MipIt + 1;
		const uint32 TargetResolution = Resolution << TargetMipIndex;

		FGenerateMipCS::FParameters* Parameters = GraphBuilder.AllocParameters<FGenerateMipCS::FParameters>();
		Parameters->InTexture = GraphBuilder.CreateSRV(FRDGTextureSRVDesc::CreateForMipLevel(OutTexture, SourceMipIndex));
		Parameters->OutTexture = GraphBuilder.CreateUAV(FRDGTextureUAVDesc(OutTexture, TargetMipIndex));
		Parameters->Resolution = Resolution;
		Parameters->SourceMip = SourceMipIndex;
		Parameters->TargetMip = TargetMipIndex;
		Parameters->LinearSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();

		TShaderMapRef<FGenerateMipCS> ComputeShader(ShaderMap);
		ClearUnusedGraphResources(ComputeShader, Parameters);
		GraphBuilder.AddPass(
			RDG_EVENT_NAME("HairStrandsComputeVoxelMip"),
			Parameters,
			ERDGPassFlags::Compute,
			[Parameters, ComputeShader, TargetResolution](FRHICommandList& RHICmdList)
		{
			const FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(FIntPoint(TargetResolution, TargetResolution), FIntPoint(8, 8));
			FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *Parameters, GroupCount);
		});
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////

class FHairUpdatePositionOffsetCS : public FGlobalShader
{
public:
private:
	DECLARE_GLOBAL_SHADER(FHairUpdatePositionOffsetCS);
	SHADER_USE_PARAMETER_STRUCT(FHairUpdatePositionOffsetCS, FGlobalShader);

	class FUseGPUOffset : SHADER_PERMUTATION_BOOL("PERMUTATION_USE_GPU_OFFSET");
	class FPrevious : SHADER_PERMUTATION_BOOL("PERMUTATION_PREVIOUS");
	using FPermutationDomain = TShaderPermutationDomain<FUseGPUOffset, FPrevious>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER(FVector3f, CPUCurrPositionOffset)
		SHADER_PARAMETER(FVector3f, CPUPrevPositionOffset)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, RootTriangleCurrPosition0Buffer)
		SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, RootTrianglePrevPosition0Buffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, OutCurrOffsetBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer, OutPrevOffsetBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsHairStrandsSupported(EHairStrandsShaderType::All, Parameters.Platform);
	}
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("SHADER_OFFSET_UPDATE"), 1);
	}
};

IMPLEMENT_GLOBAL_SHADER(FHairUpdatePositionOffsetCS, "/Engine/Private/HairStrands/HairStrandsMesh.usf", "MainCS", SF_Compute);

void AddHairStrandUpdatePositionOffsetPass(
	FRDGBuilder& GraphBuilder,
	FGlobalShaderMap* ShaderMap,
	const int32 LODIndex,
	FHairStrandsDeformedRootResource* DeformedRootResources,
	FHairStrandsDeformedResource* DeformedResources)
{
	if ((DeformedRootResources && LODIndex < 0) || DeformedResources == nullptr)
	{
		return;
	}

	const bool bComputePreviousDeformedPosition = IsHairStrandContinuousDecimationReorderingEnabled();

	FRDGImportedBuffer OutCurrPositionOffsetBuffer = Register(GraphBuilder, DeformedResources->GetPositionOffsetBuffer(FHairStrandsDeformedResource::Current), ERDGImportedBufferFlags::CreateUAV);
	FRDGImportedBuffer OutPrevPositionOffsetBuffer = Register(GraphBuilder, DeformedResources->GetPositionOffsetBuffer(FHairStrandsDeformedResource::Previous), ERDGImportedBufferFlags::CreateUAV);

	FRDGImportedBuffer RootTriangleCurrPositionBuffer;
	FRDGImportedBuffer RootTrianglePrevPositionBuffer;
	if (DeformedRootResources)
	{
		RootTriangleCurrPositionBuffer = Register(GraphBuilder, DeformedRootResources->LODs[LODIndex].GetDeformedUniqueTrianglePosition0Buffer(FHairStrandsDeformedRootResource::FLOD::Current), ERDGImportedBufferFlags::CreateSRV);
		RootTrianglePrevPositionBuffer = Register(GraphBuilder, DeformedRootResources->LODs[LODIndex].GetDeformedUniqueTrianglePosition0Buffer(FHairStrandsDeformedRootResource::FLOD::Previous), ERDGImportedBufferFlags::CreateSRV);
	}

	const bool bUseGPUOffset = DeformedRootResources != nullptr && DeformedRootResources->IsValid() && DeformedRootResources->IsInitialized() && GHairStrandsUseGPUPositionOffset > 0;
	const uint32 CurrOffsetIndex = DeformedResources->GetIndex(FHairStrandsDeformedResource::Current);
	const uint32 PrevOffsetIndex = DeformedResources->GetIndex(FHairStrandsDeformedResource::Previous);

	FHairUpdatePositionOffsetCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FHairUpdatePositionOffsetCS::FParameters>();
	PassParameters->CPUCurrPositionOffset			= (FVector3f)DeformedResources->PositionOffset[CurrOffsetIndex];
	PassParameters->CPUPrevPositionOffset			= (FVector3f)DeformedResources->PositionOffset[PrevOffsetIndex];
	PassParameters->RootTriangleCurrPosition0Buffer	= RootTriangleCurrPositionBuffer.SRV;
	PassParameters->RootTrianglePrevPosition0Buffer = RootTrianglePrevPositionBuffer.SRV;
	PassParameters->OutCurrOffsetBuffer				= OutCurrPositionOffsetBuffer.UAV;
	PassParameters->OutPrevOffsetBuffer				= OutPrevPositionOffsetBuffer.UAV;

	FHairUpdatePositionOffsetCS::FPermutationDomain PermutationVector;
	PermutationVector.Set<FHairUpdatePositionOffsetCS::FUseGPUOffset>(bUseGPUOffset);
	PermutationVector.Set<FHairUpdatePositionOffsetCS::FPrevious>(bComputePreviousDeformedPosition);
	TShaderMapRef<FHairUpdatePositionOffsetCS> ComputeShader(ShaderMap, PermutationVector);
	FComputeShaderUtils::AddPass(
		GraphBuilder,
		RDG_EVENT_NAME("HairStrands::UpdatePositionOffset(%s,%s)", bUseGPUOffset ? TEXT("GPU") : TEXT("CPU"), bComputePreviousDeformedPosition ? TEXT("Current/Previous") : TEXT("Current")),
		ComputeShader,
		PassParameters,
		FIntVector(1, 1, 1));

	GraphBuilder.SetBufferAccessFinal(OutCurrPositionOffsetBuffer.Buffer, ERHIAccess::SRVMask);
	GraphBuilder.SetBufferAccessFinal(OutPrevPositionOffsetBuffer.Buffer, ERHIAccess::SRVMask);
}