// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkelMeshDNAReader.h"

#include "DNAAsset.h"
#include "DNAReader.h"

#if WITH_EDITORONLY_DATA
#include "EditorFramework/AssetImportData.h"
#endif
#include "Engine/AssetUserData.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY(LogSkelMeshDNAReader);

FSkelMeshDNAReader::FSkelMeshDNAReader(UDNAAsset* DNAAsset) : GeometryStreamReader{ nullptr }
{
	BehaviorStreamReader = DNAAsset->GetBehaviorReader().Get();
#if WITH_EDITORONLY_DATA
	GeometryStreamReader = DNAAsset->GetGeometryReader().Get();
#endif
}

dna::Reader* FSkelMeshDNAReader::Unwrap() const
{
	return nullptr;  // Unused in SkelMeshDNAReader
}

/** DESCRIPTOR READER **/

FString FSkelMeshDNAReader::GetName() const
{
	return BehaviorStreamReader->GetName();
}

EArchetype FSkelMeshDNAReader::GetArchetype() const
{
	return BehaviorStreamReader->GetArchetype();
}

EGender FSkelMeshDNAReader::GetGender() const
{
	return BehaviorStreamReader->GetGender();
}

uint16 FSkelMeshDNAReader::GetAge() const
{
	return BehaviorStreamReader->GetAge();
}

uint32 FSkelMeshDNAReader::GetMetaDataCount() const
{
	return BehaviorStreamReader->GetMetaDataCount();
}

FString FSkelMeshDNAReader::GetMetaDataKey(uint32 Index) const
{
	return BehaviorStreamReader->GetMetaDataKey(Index);
}

FString FSkelMeshDNAReader::GetMetaDataValue(const FString& Key) const
{
	return BehaviorStreamReader->GetMetaDataValue(Key);
}

ETranslationUnit FSkelMeshDNAReader::GetTranslationUnit() const
{
	return BehaviorStreamReader->GetTranslationUnit();
}

ERotationUnit FSkelMeshDNAReader::GetRotationUnit() const
{
	return BehaviorStreamReader->GetRotationUnit();
}

FCoordinateSystem FSkelMeshDNAReader::GetCoordinateSystem() const
{
	return BehaviorStreamReader->GetCoordinateSystem();
}

uint16 FSkelMeshDNAReader::GetLODCount() const
{
	return BehaviorStreamReader->GetLODCount();
}

uint16 FSkelMeshDNAReader::GetDBMaxLOD() const
{
	return BehaviorStreamReader->GetDBMaxLOD();
}

FString FSkelMeshDNAReader::GetDBComplexity() const
{
	return BehaviorStreamReader->GetDBComplexity();
}

FString FSkelMeshDNAReader::GetDBName() const
{
	return BehaviorStreamReader->GetDBName();
}

/** DEFINITION READER **/

uint16 FSkelMeshDNAReader::GetGUIControlCount() const
{
	return BehaviorStreamReader->GetGUIControlCount();
}

FString FSkelMeshDNAReader::GetGUIControlName(uint16 Index) const
{
	return BehaviorStreamReader->GetGUIControlName(Index);
}

uint16 FSkelMeshDNAReader::GetRawControlCount() const
{
	return BehaviorStreamReader->GetRawControlCount();
}

FString FSkelMeshDNAReader::GetRawControlName(uint16 Index) const
{
	return BehaviorStreamReader->GetRawControlName(Index);
}

uint16 FSkelMeshDNAReader::GetJointCount() const
{
	return BehaviorStreamReader->GetJointCount();
}

FString FSkelMeshDNAReader::GetJointName(uint16 Index) const
{
	return BehaviorStreamReader->GetJointName(Index);
}

uint16 FSkelMeshDNAReader::GetJointIndexListCount() const
{
	return BehaviorStreamReader->GetJointIndexListCount();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetJointIndicesForLOD(uint16 LOD) const
{
	return BehaviorStreamReader->GetJointIndicesForLOD(LOD);
}

uint16 FSkelMeshDNAReader::GetBlendShapeChannelCount() const
{
	return BehaviorStreamReader->GetBlendShapeChannelCount();
}

FString FSkelMeshDNAReader::GetBlendShapeChannelName(uint16 Index) const
{
	return BehaviorStreamReader->GetBlendShapeChannelName(Index);
}

uint16 FSkelMeshDNAReader::GetBlendShapeChannelIndexListCount() const
{
	return BehaviorStreamReader->GetBlendShapeChannelIndexListCount();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetBlendShapeChannelIndicesForLOD(uint16 LOD) const
{
	return BehaviorStreamReader->GetBlendShapeChannelIndicesForLOD(LOD);
}

uint16 FSkelMeshDNAReader::GetAnimatedMapCount() const
{
	return BehaviorStreamReader->GetAnimatedMapCount();
}

FString FSkelMeshDNAReader::GetAnimatedMapName(uint16 Index) const
{
	return BehaviorStreamReader->GetAnimatedMapName(Index);
}

uint16 FSkelMeshDNAReader::GetAnimatedMapIndexListCount() const
{
	return BehaviorStreamReader->GetAnimatedMapIndexListCount();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetAnimatedMapIndicesForLOD(uint16 LOD) const
{
	return BehaviorStreamReader->GetAnimatedMapIndicesForLOD(LOD);
}

uint16 FSkelMeshDNAReader::GetMeshCount() const
{
	return BehaviorStreamReader->GetMeshCount();
}

FString FSkelMeshDNAReader::GetMeshName(uint16 Index) const
{
	return BehaviorStreamReader->GetMeshName(Index);
}

uint16 FSkelMeshDNAReader::GetMeshIndexListCount() const
{
	return BehaviorStreamReader->GetMeshIndexListCount();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetMeshIndicesForLOD(uint16 LOD) const
{
	return BehaviorStreamReader->GetMeshIndicesForLOD(LOD);
}

uint16 FSkelMeshDNAReader::GetMeshBlendShapeChannelMappingCount() const
{
	return BehaviorStreamReader->GetMeshBlendShapeChannelMappingCount();
}

FMeshBlendShapeChannelMapping FSkelMeshDNAReader::GetMeshBlendShapeChannelMapping(uint16 Index) const
{
	return BehaviorStreamReader->GetMeshBlendShapeChannelMapping(Index);
}

TArrayView<const uint16> FSkelMeshDNAReader::GetMeshBlendShapeChannelMappingIndicesForLOD(uint16 LOD) const
{
	return BehaviorStreamReader->GetMeshBlendShapeChannelMappingIndicesForLOD(LOD);
}

FVector FSkelMeshDNAReader::GetNeutralJointTranslation(uint16 Index) const
{
	return BehaviorStreamReader->GetNeutralJointTranslation(Index);
}

FVector FSkelMeshDNAReader::GetNeutralJointRotation(uint16 Index) const
{
	return BehaviorStreamReader->GetNeutralJointRotation(Index);
}

uint16 FSkelMeshDNAReader::GetJointParentIndex(uint16 Index) const
{
	return BehaviorStreamReader->GetJointParentIndex(Index);
}

/** BEHAVIOR READER **/

TArrayView<const uint16> FSkelMeshDNAReader::GetGUIToRawInputIndices() const
{
	return BehaviorStreamReader->GetGUIToRawInputIndices();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetGUIToRawOutputIndices() const
{
	return BehaviorStreamReader->GetGUIToRawOutputIndices();
}

TArrayView<const float> FSkelMeshDNAReader::GetGUIToRawFromValues() const
{
	return BehaviorStreamReader->GetGUIToRawFromValues();
}

TArrayView<const float> FSkelMeshDNAReader::GetGUIToRawToValues() const
{
	return  BehaviorStreamReader->GetGUIToRawToValues();
}

TArrayView<const float> FSkelMeshDNAReader::GetGUIToRawSlopeValues() const
{
	return BehaviorStreamReader->GetGUIToRawSlopeValues();
}

TArrayView<const float> FSkelMeshDNAReader::GetGUIToRawCutValues() const
{
	return BehaviorStreamReader->GetGUIToRawCutValues();
}

uint16 FSkelMeshDNAReader::GetPSDCount() const
{
	return BehaviorStreamReader->GetPSDCount();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetPSDRowIndices() const
{
	return BehaviorStreamReader->GetPSDRowIndices();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetPSDColumnIndices() const
{
	return BehaviorStreamReader->GetPSDColumnIndices();
}

TArrayView<const float> FSkelMeshDNAReader::GetPSDValues() const
{
	return BehaviorStreamReader->GetPSDValues();
}

uint16 FSkelMeshDNAReader::GetJointRowCount() const
{
	return BehaviorStreamReader->GetJointRowCount();
}

uint16 FSkelMeshDNAReader::GetJointColumnCount() const
{
	return BehaviorStreamReader->GetJointColumnCount();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetJointGroupJointIndices(uint16 JointGroupIndex) const
{
	return BehaviorStreamReader->GetJointGroupJointIndices(JointGroupIndex);
}

TArrayView<const uint16> FSkelMeshDNAReader::GetJointVariableAttributeIndices(uint16 LOD) const
{
	return BehaviorStreamReader->GetJointVariableAttributeIndices(LOD);
}

uint16 FSkelMeshDNAReader::GetJointGroupCount() const
{
	return BehaviorStreamReader->GetJointGroupCount();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetJointGroupLODs(uint16 JointGroupIndex) const
{
	return BehaviorStreamReader->GetJointGroupLODs(JointGroupIndex);
}

TArrayView<const uint16> FSkelMeshDNAReader::GetJointGroupInputIndices(uint16 JointGroupIndex) const
{
	return BehaviorStreamReader->GetJointGroupInputIndices(JointGroupIndex);
}

TArrayView<const uint16> FSkelMeshDNAReader::GetJointGroupOutputIndices(uint16 JointGroupIndex) const
{
	return BehaviorStreamReader->GetJointGroupOutputIndices(JointGroupIndex);
}

TArrayView<const float> FSkelMeshDNAReader::GetJointGroupValues(uint16 JointGroupIndex) const
{
	return BehaviorStreamReader->GetJointGroupValues(JointGroupIndex);
}

TArrayView<const uint16> FSkelMeshDNAReader::GetBlendShapeChannelLODs() const
{
	return BehaviorStreamReader->GetBlendShapeChannelLODs();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetBlendShapeChannelInputIndices() const
{
	return BehaviorStreamReader->GetBlendShapeChannelInputIndices();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetBlendShapeChannelOutputIndices() const
{
	return BehaviorStreamReader->GetBlendShapeChannelOutputIndices();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetAnimatedMapLODs() const
{
	return BehaviorStreamReader->GetAnimatedMapLODs();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetAnimatedMapInputIndices() const
{
	return BehaviorStreamReader->GetAnimatedMapInputIndices();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetAnimatedMapOutputIndices() const
{
	return BehaviorStreamReader->GetAnimatedMapOutputIndices();
}

TArrayView<const float> FSkelMeshDNAReader::GetAnimatedMapFromValues() const
{
	return BehaviorStreamReader->GetAnimatedMapFromValues();
}

TArrayView<const float> FSkelMeshDNAReader::GetAnimatedMapToValues() const
{
	return BehaviorStreamReader->GetAnimatedMapToValues();
}

TArrayView<const float> FSkelMeshDNAReader::GetAnimatedMapSlopeValues() const
{
	return BehaviorStreamReader->GetAnimatedMapSlopeValues();
}

TArrayView<const float> FSkelMeshDNAReader::GetAnimatedMapCutValues() const
{
	return BehaviorStreamReader->GetAnimatedMapCutValues();
}

/** GEOMETRY READER **/
uint32 FSkelMeshDNAReader::GetVertexPositionCount(uint16 MeshIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetVertexPositionCount(MeshIndex);
	}
	return {};
}

FVector FSkelMeshDNAReader::GetVertexPosition(uint16 MeshIndex, uint32 PositionIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetVertexPosition(MeshIndex, PositionIndex);
	}
	return {};
}

TArrayView<const float> FSkelMeshDNAReader::GetVertexPositionXs(uint16 MeshIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetVertexPositionXs(MeshIndex);
	}
	return {};
}

TArrayView<const float> FSkelMeshDNAReader::GetVertexPositionYs(uint16 MeshIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetVertexPositionYs(MeshIndex);
	}
	return {};
}

TArrayView<const float> FSkelMeshDNAReader::GetVertexPositionZs(uint16 MeshIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetVertexPositionZs(MeshIndex);
	}
	return {};
}

uint32 FSkelMeshDNAReader::GetVertexTextureCoordinateCount(uint16 MeshIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetVertexTextureCoordinateCount(MeshIndex);
	}
	return {};
}

FTextureCoordinate FSkelMeshDNAReader::GetVertexTextureCoordinate(uint16 MeshIndex, uint32 TextureCoordinateIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetVertexTextureCoordinate(MeshIndex, TextureCoordinateIndex);
	}
	return {};
}

uint32 FSkelMeshDNAReader::GetVertexNormalCount(uint16 MeshIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetVertexNormalCount(MeshIndex);
	}
	return {};
}

FVector FSkelMeshDNAReader::GetVertexNormal(uint16 MeshIndex, uint32 NormalIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetVertexNormal(MeshIndex, NormalIndex);
	}
	return {};
}

/* not needed for gene splicer */
uint32 FSkelMeshDNAReader::GetVertexLayoutCount(uint16 MeshIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetVertexLayoutCount(MeshIndex);
	}
	return {};
}

/* not needed for gene splicer */
FVertexLayout FSkelMeshDNAReader::GetVertexLayout(uint16 MeshIndex, uint32 VertexIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetVertexLayout(MeshIndex, VertexIndex);
	}
	return {};
}

/* not needed for gene splicer */
uint32 FSkelMeshDNAReader::GetFaceCount(uint16 MeshIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetFaceCount(MeshIndex);
	}
	return {};
}

/* not needed for gene splicer */
TArrayView<const uint32> FSkelMeshDNAReader::GetFaceVertexLayoutIndices(uint16 MeshIndex, uint32 FaceIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetFaceVertexLayoutIndices(MeshIndex, FaceIndex);
	}
	return {};
}

uint16 FSkelMeshDNAReader::GetMaximumInfluencePerVertex(uint16 MeshIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetMaximumInfluencePerVertex(MeshIndex);
	}
	return {};
}

uint32 FSkelMeshDNAReader::GetSkinWeightsCount(uint16 MeshIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetSkinWeightsCount(MeshIndex);
	}
	return {};
}

TArrayView<const float> FSkelMeshDNAReader::GetSkinWeightsValues(uint16 MeshIndex, uint32 VertexIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetSkinWeightsValues(MeshIndex, VertexIndex);
	}
	return {};
}

TArrayView<const uint16> FSkelMeshDNAReader::GetSkinWeightsJointIndices(uint16 MeshIndex, uint32 VertexIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetSkinWeightsJointIndices(MeshIndex, VertexIndex);
	}
	return {};
}

uint16 FSkelMeshDNAReader::GetBlendShapeTargetCount(uint16 MeshIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetBlendShapeTargetCount(MeshIndex);
	}
	return {};
}

uint16 FSkelMeshDNAReader::GetBlendShapeChannelIndex(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetBlendShapeChannelIndex(MeshIndex, BlendShapeTargetIndex);
	}
	return {};
}

uint32 FSkelMeshDNAReader::GetBlendShapeTargetDeltaCount(uint16 MeshIndex, uint16 BlendShapeIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetBlendShapeTargetDeltaCount(MeshIndex, BlendShapeIndex);
	}
	return {};
}

FVector FSkelMeshDNAReader::GetBlendShapeTargetDelta(uint16 MeshIndex, uint16 BlendShapeIndex, uint32 DeltaIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetBlendShapeTargetDelta(MeshIndex, BlendShapeIndex, DeltaIndex);
	}
	return {};
}

TArrayView<const float> FSkelMeshDNAReader::GetBlendShapeTargetDeltaXs(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetBlendShapeTargetDeltaXs(MeshIndex, BlendShapeTargetIndex);
	}
	return {};
}

TArrayView<const float> FSkelMeshDNAReader::GetBlendShapeTargetDeltaYs(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetBlendShapeTargetDeltaYs(MeshIndex, BlendShapeTargetIndex);
	}
	return {};
}

TArrayView<const float> FSkelMeshDNAReader::GetBlendShapeTargetDeltaZs(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetBlendShapeTargetDeltaZs(MeshIndex, BlendShapeTargetIndex);
	}
	return {};
}

TArrayView<const uint32> FSkelMeshDNAReader::GetBlendShapeTargetVertexIndices(uint16 MeshIndex, uint16 BlendShapeIndex) const
{
	if (GeometryStreamReader)
	{
		return GeometryStreamReader->GetBlendShapeTargetVertexIndices(MeshIndex, BlendShapeIndex);
	}
	return {};
}

void FSkelMeshDNAReader::Unload(EDNADataLayer Layer) {
	ensureMsgf(false, TEXT("Assest are not unloadable"));
}