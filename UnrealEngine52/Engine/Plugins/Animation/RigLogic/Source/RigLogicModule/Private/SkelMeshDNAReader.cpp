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

FSkelMeshDNAReader::FSkelMeshDNAReader(UDNAAsset* DNAAsset) : GeometryReader{nullptr}
{
	BehaviorReader = DNAAsset->GetBehaviorReader();
#if WITH_EDITORONLY_DATA
	GeometryReader = DNAAsset->GetGeometryReader();
#endif
}

dna::Reader* FSkelMeshDNAReader::Unwrap() const
{
	return nullptr;  // Unused in SkelMeshDNAReader
}

/** HEADER READER **/
uint16 FSkelMeshDNAReader::GetFileFormatGeneration() const
{
	return BehaviorReader->GetFileFormatGeneration();
}

uint16 FSkelMeshDNAReader::GetFileFormatVersion() const
{
	return BehaviorReader->GetFileFormatVersion();
}

/** DESCRIPTOR READER **/

FString FSkelMeshDNAReader::GetName() const
{
	return BehaviorReader->GetName();
}

EArchetype FSkelMeshDNAReader::GetArchetype() const
{
	return BehaviorReader->GetArchetype();
}

EGender FSkelMeshDNAReader::GetGender() const
{
	return BehaviorReader->GetGender();
}

uint16 FSkelMeshDNAReader::GetAge() const
{
	return BehaviorReader->GetAge();
}

uint32 FSkelMeshDNAReader::GetMetaDataCount() const
{
	return BehaviorReader->GetMetaDataCount();
}

FString FSkelMeshDNAReader::GetMetaDataKey(uint32 Index) const
{
	return BehaviorReader->GetMetaDataKey(Index);
}

FString FSkelMeshDNAReader::GetMetaDataValue(const FString& Key) const
{
	return BehaviorReader->GetMetaDataValue(Key);
}

ETranslationUnit FSkelMeshDNAReader::GetTranslationUnit() const
{
	return BehaviorReader->GetTranslationUnit();
}

ERotationUnit FSkelMeshDNAReader::GetRotationUnit() const
{
	return BehaviorReader->GetRotationUnit();
}

FCoordinateSystem FSkelMeshDNAReader::GetCoordinateSystem() const
{
	return BehaviorReader->GetCoordinateSystem();
}

uint16 FSkelMeshDNAReader::GetLODCount() const
{
	return BehaviorReader->GetLODCount();
}

uint16 FSkelMeshDNAReader::GetDBMaxLOD() const
{
	return BehaviorReader->GetDBMaxLOD();
}

FString FSkelMeshDNAReader::GetDBComplexity() const
{
	return BehaviorReader->GetDBComplexity();
}

FString FSkelMeshDNAReader::GetDBName() const
{
	return BehaviorReader->GetDBName();
}

/** DEFINITION READER **/

uint16 FSkelMeshDNAReader::GetGUIControlCount() const
{
	return BehaviorReader->GetGUIControlCount();
}

FString FSkelMeshDNAReader::GetGUIControlName(uint16 Index) const
{
	return BehaviorReader->GetGUIControlName(Index);
}

uint16 FSkelMeshDNAReader::GetRawControlCount() const
{
	return BehaviorReader->GetRawControlCount();
}

FString FSkelMeshDNAReader::GetRawControlName(uint16 Index) const
{
	return BehaviorReader->GetRawControlName(Index);
}

uint16 FSkelMeshDNAReader::GetJointCount() const
{
	return BehaviorReader->GetJointCount();
}

FString FSkelMeshDNAReader::GetJointName(uint16 Index) const
{
	return BehaviorReader->GetJointName(Index);
}

uint16 FSkelMeshDNAReader::GetJointIndexListCount() const
{
	return BehaviorReader->GetJointIndexListCount();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetJointIndicesForLOD(uint16 LOD) const
{
	return BehaviorReader->GetJointIndicesForLOD(LOD);
}

uint16 FSkelMeshDNAReader::GetBlendShapeChannelCount() const
{
	return BehaviorReader->GetBlendShapeChannelCount();
}

FString FSkelMeshDNAReader::GetBlendShapeChannelName(uint16 Index) const
{
	return BehaviorReader->GetBlendShapeChannelName(Index);
}

uint16 FSkelMeshDNAReader::GetBlendShapeChannelIndexListCount() const
{
	return BehaviorReader->GetBlendShapeChannelIndexListCount();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetBlendShapeChannelIndicesForLOD(uint16 LOD) const
{
	return BehaviorReader->GetBlendShapeChannelIndicesForLOD(LOD);
}

uint16 FSkelMeshDNAReader::GetAnimatedMapCount() const
{
	return BehaviorReader->GetAnimatedMapCount();
}

FString FSkelMeshDNAReader::GetAnimatedMapName(uint16 Index) const
{
	return BehaviorReader->GetAnimatedMapName(Index);
}

uint16 FSkelMeshDNAReader::GetAnimatedMapIndexListCount() const
{
	return BehaviorReader->GetAnimatedMapIndexListCount();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetAnimatedMapIndicesForLOD(uint16 LOD) const
{
	return BehaviorReader->GetAnimatedMapIndicesForLOD(LOD);
}

uint16 FSkelMeshDNAReader::GetMeshCount() const
{
	return BehaviorReader->GetMeshCount();
}

FString FSkelMeshDNAReader::GetMeshName(uint16 Index) const
{
	return BehaviorReader->GetMeshName(Index);
}

uint16 FSkelMeshDNAReader::GetMeshIndexListCount() const
{
	return BehaviorReader->GetMeshIndexListCount();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetMeshIndicesForLOD(uint16 LOD) const
{
	return BehaviorReader->GetMeshIndicesForLOD(LOD);
}

uint16 FSkelMeshDNAReader::GetMeshBlendShapeChannelMappingCount() const
{
	return BehaviorReader->GetMeshBlendShapeChannelMappingCount();
}

FMeshBlendShapeChannelMapping FSkelMeshDNAReader::GetMeshBlendShapeChannelMapping(uint16 Index) const
{
	return BehaviorReader->GetMeshBlendShapeChannelMapping(Index);
}

TArrayView<const uint16> FSkelMeshDNAReader::GetMeshBlendShapeChannelMappingIndicesForLOD(uint16 LOD) const
{
	return BehaviorReader->GetMeshBlendShapeChannelMappingIndicesForLOD(LOD);
}

FVector FSkelMeshDNAReader::GetNeutralJointTranslation(uint16 Index) const
{
	return BehaviorReader->GetNeutralJointTranslation(Index);
}

FVector FSkelMeshDNAReader::GetNeutralJointRotation(uint16 Index) const
{
	return BehaviorReader->GetNeutralJointRotation(Index);
}

uint16 FSkelMeshDNAReader::GetJointParentIndex(uint16 Index) const
{
	return BehaviorReader->GetJointParentIndex(Index);
}

/** BEHAVIOR READER **/

TArrayView<const uint16> FSkelMeshDNAReader::GetGUIToRawInputIndices() const
{
	return BehaviorReader->GetGUIToRawInputIndices();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetGUIToRawOutputIndices() const
{
	return BehaviorReader->GetGUIToRawOutputIndices();
}

TArrayView<const float> FSkelMeshDNAReader::GetGUIToRawFromValues() const
{
	return BehaviorReader->GetGUIToRawFromValues();
}

TArrayView<const float> FSkelMeshDNAReader::GetGUIToRawToValues() const
{
	return  BehaviorReader->GetGUIToRawToValues();
}

TArrayView<const float> FSkelMeshDNAReader::GetGUIToRawSlopeValues() const
{
	return BehaviorReader->GetGUIToRawSlopeValues();
}

TArrayView<const float> FSkelMeshDNAReader::GetGUIToRawCutValues() const
{
	return BehaviorReader->GetGUIToRawCutValues();
}

uint16 FSkelMeshDNAReader::GetPSDCount() const
{
	return BehaviorReader->GetPSDCount();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetPSDRowIndices() const
{
	return BehaviorReader->GetPSDRowIndices();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetPSDColumnIndices() const
{
	return BehaviorReader->GetPSDColumnIndices();
}

TArrayView<const float> FSkelMeshDNAReader::GetPSDValues() const
{
	return BehaviorReader->GetPSDValues();
}

uint16 FSkelMeshDNAReader::GetJointRowCount() const
{
	return BehaviorReader->GetJointRowCount();
}

uint16 FSkelMeshDNAReader::GetJointColumnCount() const
{
	return BehaviorReader->GetJointColumnCount();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetJointGroupJointIndices(uint16 JointGroupIndex) const
{
	return BehaviorReader->GetJointGroupJointIndices(JointGroupIndex);
}

TArrayView<const uint16> FSkelMeshDNAReader::GetJointVariableAttributeIndices(uint16 LOD) const
{
	return BehaviorReader->GetJointVariableAttributeIndices(LOD);
}

uint16 FSkelMeshDNAReader::GetJointGroupCount() const
{
	return BehaviorReader->GetJointGroupCount();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetJointGroupLODs(uint16 JointGroupIndex) const
{
	return BehaviorReader->GetJointGroupLODs(JointGroupIndex);
}

TArrayView<const uint16> FSkelMeshDNAReader::GetJointGroupInputIndices(uint16 JointGroupIndex) const
{
	return BehaviorReader->GetJointGroupInputIndices(JointGroupIndex);
}

TArrayView<const uint16> FSkelMeshDNAReader::GetJointGroupOutputIndices(uint16 JointGroupIndex) const
{
	return BehaviorReader->GetJointGroupOutputIndices(JointGroupIndex);
}

TArrayView<const float> FSkelMeshDNAReader::GetJointGroupValues(uint16 JointGroupIndex) const
{
	return BehaviorReader->GetJointGroupValues(JointGroupIndex);
}

TArrayView<const uint16> FSkelMeshDNAReader::GetBlendShapeChannelLODs() const
{
	return BehaviorReader->GetBlendShapeChannelLODs();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetBlendShapeChannelInputIndices() const
{
	return BehaviorReader->GetBlendShapeChannelInputIndices();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetBlendShapeChannelOutputIndices() const
{
	return BehaviorReader->GetBlendShapeChannelOutputIndices();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetAnimatedMapLODs() const
{
	return BehaviorReader->GetAnimatedMapLODs();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetAnimatedMapInputIndices() const
{
	return BehaviorReader->GetAnimatedMapInputIndices();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetAnimatedMapOutputIndices() const
{
	return BehaviorReader->GetAnimatedMapOutputIndices();
}

TArrayView<const float> FSkelMeshDNAReader::GetAnimatedMapFromValues() const
{
	return BehaviorReader->GetAnimatedMapFromValues();
}

TArrayView<const float> FSkelMeshDNAReader::GetAnimatedMapToValues() const
{
	return BehaviorReader->GetAnimatedMapToValues();
}

TArrayView<const float> FSkelMeshDNAReader::GetAnimatedMapSlopeValues() const
{
	return BehaviorReader->GetAnimatedMapSlopeValues();
}

TArrayView<const float> FSkelMeshDNAReader::GetAnimatedMapCutValues() const
{
	return BehaviorReader->GetAnimatedMapCutValues();
}

/** GEOMETRY READER **/
uint32 FSkelMeshDNAReader::GetVertexPositionCount(uint16 MeshIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetVertexPositionCount(MeshIndex);
	}
	return {};
}

FVector FSkelMeshDNAReader::GetVertexPosition(uint16 MeshIndex, uint32 PositionIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetVertexPosition(MeshIndex, PositionIndex);
	}
	return {};
}

TArrayView<const float> FSkelMeshDNAReader::GetVertexPositionXs(uint16 MeshIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetVertexPositionXs(MeshIndex);
	}
	return {};
}

TArrayView<const float> FSkelMeshDNAReader::GetVertexPositionYs(uint16 MeshIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetVertexPositionYs(MeshIndex);
	}
	return {};
}

TArrayView<const float> FSkelMeshDNAReader::GetVertexPositionZs(uint16 MeshIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetVertexPositionZs(MeshIndex);
	}
	return {};
}

uint32 FSkelMeshDNAReader::GetVertexTextureCoordinateCount(uint16 MeshIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetVertexTextureCoordinateCount(MeshIndex);
	}
	return {};
}

FTextureCoordinate FSkelMeshDNAReader::GetVertexTextureCoordinate(uint16 MeshIndex, uint32 TextureCoordinateIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetVertexTextureCoordinate(MeshIndex, TextureCoordinateIndex);
	}
	return {};
}

uint32 FSkelMeshDNAReader::GetVertexNormalCount(uint16 MeshIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetVertexNormalCount(MeshIndex);
	}
	return {};
}

FVector FSkelMeshDNAReader::GetVertexNormal(uint16 MeshIndex, uint32 NormalIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetVertexNormal(MeshIndex, NormalIndex);
	}
	return {};
}

/* not needed for gene splicer */
uint32 FSkelMeshDNAReader::GetVertexLayoutCount(uint16 MeshIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetVertexLayoutCount(MeshIndex);
	}
	return {};
}

/* not needed for gene splicer */
FVertexLayout FSkelMeshDNAReader::GetVertexLayout(uint16 MeshIndex, uint32 VertexIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetVertexLayout(MeshIndex, VertexIndex);
	}
	return {};
}

/* not needed for gene splicer */
uint32 FSkelMeshDNAReader::GetFaceCount(uint16 MeshIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetFaceCount(MeshIndex);
	}
	return {};
}

/* not needed for gene splicer */
TArrayView<const uint32> FSkelMeshDNAReader::GetFaceVertexLayoutIndices(uint16 MeshIndex, uint32 FaceIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetFaceVertexLayoutIndices(MeshIndex, FaceIndex);
	}
	return {};
}

uint16 FSkelMeshDNAReader::GetMaximumInfluencePerVertex(uint16 MeshIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetMaximumInfluencePerVertex(MeshIndex);
	}
	return {};
}

uint32 FSkelMeshDNAReader::GetSkinWeightsCount(uint16 MeshIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetSkinWeightsCount(MeshIndex);
	}
	return {};
}

TArrayView<const float> FSkelMeshDNAReader::GetSkinWeightsValues(uint16 MeshIndex, uint32 VertexIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetSkinWeightsValues(MeshIndex, VertexIndex);
	}
	return {};
}

TArrayView<const uint16> FSkelMeshDNAReader::GetSkinWeightsJointIndices(uint16 MeshIndex, uint32 VertexIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetSkinWeightsJointIndices(MeshIndex, VertexIndex);
	}
	return {};
}

uint16 FSkelMeshDNAReader::GetBlendShapeTargetCount(uint16 MeshIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetBlendShapeTargetCount(MeshIndex);
	}
	return {};
}

uint16 FSkelMeshDNAReader::GetBlendShapeChannelIndex(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetBlendShapeChannelIndex(MeshIndex, BlendShapeTargetIndex);
	}
	return {};
}

uint32 FSkelMeshDNAReader::GetBlendShapeTargetDeltaCount(uint16 MeshIndex, uint16 BlendShapeIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetBlendShapeTargetDeltaCount(MeshIndex, BlendShapeIndex);
	}
	return {};
}

FVector FSkelMeshDNAReader::GetBlendShapeTargetDelta(uint16 MeshIndex, uint16 BlendShapeIndex, uint32 DeltaIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetBlendShapeTargetDelta(MeshIndex, BlendShapeIndex, DeltaIndex);
	}
	return {};
}

TArrayView<const float> FSkelMeshDNAReader::GetBlendShapeTargetDeltaXs(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetBlendShapeTargetDeltaXs(MeshIndex, BlendShapeTargetIndex);
	}
	return {};
}

TArrayView<const float> FSkelMeshDNAReader::GetBlendShapeTargetDeltaYs(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetBlendShapeTargetDeltaYs(MeshIndex, BlendShapeTargetIndex);
	}
	return {};
}

TArrayView<const float> FSkelMeshDNAReader::GetBlendShapeTargetDeltaZs(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetBlendShapeTargetDeltaZs(MeshIndex, BlendShapeTargetIndex);
	}
	return {};
}

TArrayView<const uint32> FSkelMeshDNAReader::GetBlendShapeTargetVertexIndices(uint16 MeshIndex, uint16 BlendShapeIndex) const
{
	if (GeometryReader)
	{
		return GeometryReader->GetBlendShapeTargetVertexIndices(MeshIndex, BlendShapeIndex);
	}
	return {};
}

/** MACHINE LEARNED BEHAVIOR READER **/
uint16 FSkelMeshDNAReader::GetMLControlCount() const
{
	return BehaviorReader->GetMLControlCount();
}

FString FSkelMeshDNAReader::GetMLControlName(uint16 Index) const
{
	return BehaviorReader->GetMLControlName(Index);
}

uint16 FSkelMeshDNAReader::GetNeuralNetworkCount() const
{
	return BehaviorReader->GetNeuralNetworkCount();
}

uint16 FSkelMeshDNAReader::GetNeuralNetworkIndexListCount() const
{
	return BehaviorReader->GetNeuralNetworkIndexListCount();
}

TArrayView<const uint16> FSkelMeshDNAReader::GetNeuralNetworkIndicesForLOD(uint16 LOD) const
{
	return BehaviorReader->GetNeuralNetworkIndicesForLOD(LOD);
}

uint16 FSkelMeshDNAReader::GetMeshRegionCount(uint16 MeshIndex) const
{
	return BehaviorReader->GetMeshRegionCount(MeshIndex);
}

FString FSkelMeshDNAReader::GetMeshRegionName(uint16 MeshIndex, uint16 RegionIndex) const
{
	return BehaviorReader->GetMeshRegionName(MeshIndex, RegionIndex);
}

TArrayView<const uint16> FSkelMeshDNAReader::GetNeuralNetworkIndicesForMeshRegion(uint16 MeshIndex, uint16 RegionIndex) const
{
	return BehaviorReader->GetNeuralNetworkIndicesForMeshRegion(MeshIndex, RegionIndex);
}

TArrayView<const uint16> FSkelMeshDNAReader::GetNeuralNetworkInputIndices(uint16 NetIndex) const
{
	return BehaviorReader->GetNeuralNetworkInputIndices(NetIndex);
}

TArrayView<const uint16> FSkelMeshDNAReader::GetNeuralNetworkOutputIndices(uint16 NetIndex) const
{
	return BehaviorReader->GetNeuralNetworkOutputIndices(NetIndex);
}

uint16 FSkelMeshDNAReader::GetNeuralNetworkLayerCount(uint16 NetIndex) const
{
	return BehaviorReader->GetNeuralNetworkLayerCount(NetIndex);
}

EActivationFunction FSkelMeshDNAReader::GetNeuralNetworkLayerActivationFunction(uint16 NetIndex, uint16 LayerIndex) const
{
	return BehaviorReader->GetNeuralNetworkLayerActivationFunction(NetIndex, LayerIndex);
}

TArrayView<const float> FSkelMeshDNAReader::GetNeuralNetworkLayerActivationFunctionParameters(uint16 NetIndex, uint16 LayerIndex) const
{
	return BehaviorReader->GetNeuralNetworkLayerActivationFunctionParameters(NetIndex, LayerIndex);
}

TArrayView<const float> FSkelMeshDNAReader::GetNeuralNetworkLayerBiases(uint16 NetIndex, uint16 LayerIndex) const
{
	return BehaviorReader->GetNeuralNetworkLayerBiases(NetIndex, LayerIndex);
}

TArrayView<const float> FSkelMeshDNAReader::GetNeuralNetworkLayerWeights(uint16 NetIndex, uint16 LayerIndex) const
{
	return BehaviorReader->GetNeuralNetworkLayerWeights(NetIndex, LayerIndex);
}

void FSkelMeshDNAReader::Unload(EDNADataLayer Layer)
{
	ensureMsgf(false, TEXT("Assest are not unloadable"));
}