// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/MutableMeshBufferUtils.h"

#include "Math/Color.h"
#include "Misc/AssertionMacros.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/Types.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "Templates/UnrealTemplate.h"


void MutableMeshBufferUtils::SetupVertexPositionsBuffer(const int32& InCurrentVertexBuffer, mu::FMeshBufferSet& OutTargetVertexBuffers)
{
	using namespace mu;
	const int32 ElementSize = sizeof(FPositionVertex);
	constexpr int32 ChannelCount = 1;
	const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = {MBS_POSITION};
	const int32 SemanticIndices[ChannelCount] = {0};
	const MESH_BUFFER_FORMAT Formats[ChannelCount] = {MBF_FLOAT32};
	const int32 Components[ChannelCount] = {3};
	const int32 Offsets[ChannelCount] =
	{
		STRUCT_OFFSET(FPositionVertex, Position)
	};

	OutTargetVertexBuffers.SetBuffer(InCurrentVertexBuffer, ElementSize, ChannelCount, Semantics, SemanticIndices,
									  Formats, Components, Offsets);
}


void MutableMeshBufferUtils::SetupTangentBuffer(const int32& InCurrentVertexBuffer,
                                                mu::FMeshBufferSet& OutTargetVertexBuffers)
{
	// \todo: support for high precision?
	typedef TStaticMeshVertexTangentDatum<TStaticMeshVertexTangentTypeSelector<
		EStaticMeshVertexTangentBasisType::Default>::TangentTypeT> TangentType;

	using namespace mu;
	const int32 ElementSize = sizeof(TangentType);
	constexpr int32 ChannelCount = 2;
	const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = {MBS_TANGENT, MBS_NORMAL};
	const int32 SemanticIndices[ChannelCount] = {0, 0};
	const MESH_BUFFER_FORMAT Formats[ChannelCount] = {MBF_PACKEDDIRS8, MBF_PACKEDDIRS8_W_TANGENTSIGN};
	const int32 Components[ChannelCount] = {4, 4};
	const int32 Offsets[ChannelCount] =
	{
		STRUCT_OFFSET(TangentType, TangentX),
		STRUCT_OFFSET(TangentType, TangentZ)
	};

	OutTargetVertexBuffers.SetBuffer(InCurrentVertexBuffer, ElementSize, ChannelCount, Semantics, SemanticIndices,
	                                  Formats, Components, Offsets);
}


void MutableMeshBufferUtils::SetupTexCoordinatesBuffer(const int32& InCurrentVertexBuffer, const int32& InChannelCount,
                                                       mu::FMeshBufferSet& OutTargetVertexBuffers,const int32* InTextureSemanticIndicesOverride /* = nullptr */)
{
	// \todo: support for half precision?
	typedef TStaticMeshVertexUVsDatum<TStaticMeshVertexUVsTypeSelector<
		EStaticMeshVertexUVType::HighPrecision>::UVsTypeT> UVType;

	using namespace mu;
	const int32 ElementSize = sizeof(UVType) * InChannelCount;
	constexpr int32 MaxChannelCount = MaxTexCordChannelCount;
	const MESH_BUFFER_SEMANTIC Semantics[MaxChannelCount] = {
		MBS_TEXCOORDS, MBS_TEXCOORDS, MBS_TEXCOORDS, MBS_TEXCOORDS
	};
	const MESH_BUFFER_FORMAT Formats[MaxChannelCount] = {MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32, MBF_FLOAT32};
	const int32 Components[MaxChannelCount] = {2, 2, 2, 2};
	const int32 Offsets[MaxChannelCount] =
	{
		STRUCT_OFFSET(UVType, UVs) + 0 * sizeof(UVType),
		STRUCT_OFFSET(UVType, UVs) + 1 * sizeof(UVType),
		STRUCT_OFFSET(UVType, UVs) + 2 * sizeof(UVType),
		STRUCT_OFFSET(UVType, UVs) + 3 * sizeof(UVType),
	};

	if (InTextureSemanticIndicesOverride)
	{
		OutTargetVertexBuffers.SetBuffer(InCurrentVertexBuffer, ElementSize, InChannelCount, Semantics,
		                                  InTextureSemanticIndicesOverride, Formats, Components, Offsets);
	}
	else
	{
		const int32 SemanticIndices[MaxChannelCount] = {0, 1, 2, 3};
		OutTargetVertexBuffers.SetBuffer(InCurrentVertexBuffer, ElementSize, InChannelCount, Semantics,
		                                  SemanticIndices, Formats, Components, Offsets);
	}
}


void MutableMeshBufferUtils::SetupSkinBuffer(const int32& InCurrentVertexBuffer,
	const int32& MaxBoneIndexTypeSizeBytes,
	const int32& MaxBoneWeightTypeSizeBytes,
	const int32& MaxNumBonesPerVertex,
	mu::FMeshBufferSet& OutTargetVertexBuffers)
{
	using namespace mu;
	const int32 ElementSize = (MaxBoneWeightTypeSizeBytes + MaxBoneIndexTypeSizeBytes) * MaxNumBonesPerVertex;
	constexpr int32 ChannelCount = 2;
	const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = {MBS_BONEINDICES, MBS_BONEWEIGHTS};
	const int32 SemanticIndices[ChannelCount] = {0, 0};

	MESH_BUFFER_FORMAT Formats[ChannelCount] = {MBF_UINT8, MBF_NUINT8};
	switch (MaxBoneIndexTypeSizeBytes)
	{
	case 0: // Fallback to something in this case.
	case 1: Formats[0] = mu::MBF_UINT8;
		break;
	case 2: Formats[0] = mu::MBF_UINT16;
		break;
	case 4: Formats[0] = mu::MBF_UINT32;
		break;
	default:
		// unsupported bone index type
		check(false);
		Formats[0] = mu::MBF_NONE;
		break;
	}

	switch (MaxBoneWeightTypeSizeBytes)
	{
	case 0: // Fallback to something in this case.
	case 1: Formats[1] = mu::MBF_NUINT8;
		break;
	case 2: Formats[1] = mu::MBF_NUINT16;
		break;
	case 4: Formats[1] = mu::MBF_NUINT32;
		break;
	default:
		// unsupported bone weight type
		check(false);
		Formats[1] = mu::MBF_NONE;
		break;
	}

	int32 Components[ChannelCount];
	Components[0] = Components[1] = MaxNumBonesPerVertex;

	int32 Offsets[ChannelCount];
	Offsets[0] = 0;
	Offsets[1] = MaxBoneIndexTypeSizeBytes * MaxNumBonesPerVertex;

	OutTargetVertexBuffers.SetBuffer(InCurrentVertexBuffer, ElementSize, ChannelCount, Semantics, SemanticIndices,
	                                  Formats, Components, Offsets);
}


void MutableMeshBufferUtils::SetupVertexColorBuffer(const int32& InCurrentVertexBuffer,
                                                    mu::FMeshBufferSet& OutTargetVertexBuffers)
{
	using namespace mu;
	const int32 ElementSize = sizeof(FColor);
	constexpr int32 ChannelCount = 1;
	const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = {MBS_COLOUR};
	const int32 SemanticIndices[ChannelCount] = {0};
	const MESH_BUFFER_FORMAT Formats[ChannelCount] = {MBF_NUINT8};
	const int32 Components[ChannelCount] = {4};
	const int32 Offsets[ChannelCount] = {0};
	check(ElementSize == 4);

	OutTargetVertexBuffers.SetBuffer(InCurrentVertexBuffer, ElementSize, ChannelCount, Semantics, SemanticIndices,
	                                  Formats, Components, Offsets);
}


void MutableMeshBufferUtils::SetupIndexBuffer(mu::FMeshBufferSet& OutTargetIndexBuffers)
{
	OutTargetIndexBuffers.SetBufferCount(1);

	using namespace mu;
	const int32 ElementSize = sizeof(uint32);
	//SkeletalMesh->GetImportedResource()->LODModels[LOD].MultiSizeIndexContainer.GetDataTypeSize();
	constexpr int32 ChannelCount = 1;
	const MESH_BUFFER_SEMANTIC Semantics[ChannelCount] = {MBS_VERTEXINDEX};
	const int32 SemanticIndices[ChannelCount] = {0};
	// We force 32 bit indices, since merging meshes may create vertex buffers bigger than the initial mesh
	// and for now the mutable runtime doesn't handle it.
	// \TODO: go back to 16-bit indices when possible.
	MESH_BUFFER_FORMAT Formats[ChannelCount] = {MBF_UINT32};
	const int32 Components[ChannelCount] = {1};
	const int32 Offsets[ChannelCount] = {0};

	OutTargetIndexBuffers.SetBuffer(0, ElementSize, ChannelCount, Semantics, SemanticIndices, Formats, Components,
	                                 Offsets);
}