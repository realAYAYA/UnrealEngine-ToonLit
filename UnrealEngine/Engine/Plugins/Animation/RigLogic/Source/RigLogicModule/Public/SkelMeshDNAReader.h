// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DNAReader.h"

#include "CoreMinimal.h"
#include "Engine/AssetUserData.h"
#include "Interfaces/Interface_AssetUserData.h"
#include "HAL/FileManager.h"
#include "Serialization/Archive.h"
#include "UObject/NoExportTypes.h"

class UDNAAsset;

DECLARE_LOG_CATEGORY_EXTERN(LogSkelMeshDNAReader, Log, All);

class RIGLOGICMODULE_API FSkelMeshDNAReader: public IDNAReader
{
public:
	explicit FSkelMeshDNAReader(UDNAAsset* DNAAsset);

	// Descriptor
	FString GetName() const override;
	EArchetype GetArchetype() const override;
	EGender GetGender() const override;
	uint16 GetAge() const override;
	uint32 GetMetaDataCount() const override;
	FString GetMetaDataKey(uint32 Index) const override;
	FString GetMetaDataValue(const FString& Key) const override;
	ETranslationUnit GetTranslationUnit() const override;
	ERotationUnit GetRotationUnit() const override;
	FCoordinateSystem GetCoordinateSystem() const override;
	uint16 GetLODCount() const override;
	uint16 GetDBMaxLOD() const override;
	FString GetDBComplexity() const override;
	FString GetDBName() const override;
	// Definition
	uint16 GetGUIControlCount() const override;
	FString GetGUIControlName(uint16 Index) const override;
	uint16 GetRawControlCount() const override;
	FString GetRawControlName(uint16 Index) const override;
	uint16 GetJointCount() const override;
	FString GetJointName(uint16 Index) const override;
	uint16 GetJointIndexListCount() const override;
	TArrayView<const uint16> GetJointIndicesForLOD(uint16 LOD) const override;
	uint16 GetJointParentIndex(uint16 Index) const override;
	uint16 GetBlendShapeChannelCount() const override;
	FString GetBlendShapeChannelName(uint16 Index) const override;
	uint16 GetBlendShapeChannelIndexListCount() const override;
	TArrayView<const uint16> GetBlendShapeChannelIndicesForLOD(uint16 LOD) const override;
	uint16 GetAnimatedMapCount() const override;
	FString GetAnimatedMapName(uint16 Index) const override;
	uint16 GetAnimatedMapIndexListCount() const override;
	TArrayView<const uint16> GetAnimatedMapIndicesForLOD(uint16 LOD) const override;
	uint16 GetMeshCount() const override;
	FString GetMeshName(uint16 Index) const override;
	uint16 GetMeshIndexListCount() const override;
	TArrayView<const uint16> GetMeshIndicesForLOD(uint16 LOD) const override;
	uint16 GetMeshBlendShapeChannelMappingCount() const override;
	FMeshBlendShapeChannelMapping GetMeshBlendShapeChannelMapping(uint16 Index) const override;
	TArrayView<const uint16> GetMeshBlendShapeChannelMappingIndicesForLOD(uint16 LOD) const override;
	FVector GetNeutralJointTranslation(uint16 Index) const override;
	FVector GetNeutralJointRotation(uint16 Index) const override;
	// Behavior
	TArrayView<const uint16> GetGUIToRawInputIndices() const override;
	TArrayView<const uint16> GetGUIToRawOutputIndices() const override;
	TArrayView<const float> GetGUIToRawFromValues() const override;
	TArrayView<const float> GetGUIToRawToValues() const override;
	TArrayView<const float> GetGUIToRawSlopeValues() const override;
	TArrayView<const float> GetGUIToRawCutValues() const override;
	uint16 GetPSDCount() const override;
	TArrayView<const uint16> GetPSDRowIndices() const override;
	TArrayView<const uint16> GetPSDColumnIndices() const override;
	TArrayView<const float> GetPSDValues() const override;
	uint16 GetJointRowCount() const override;
	uint16 GetJointColumnCount() const override;
	TArrayView<const uint16> GetJointVariableAttributeIndices(uint16 LOD) const override;
	uint16 GetJointGroupCount() const override;
	TArrayView<const uint16> GetJointGroupLODs(uint16 JointGroupIndex) const override;
	TArrayView<const uint16> GetJointGroupInputIndices(uint16 JointGroupIndex) const override;
	TArrayView<const uint16> GetJointGroupOutputIndices(uint16 JointGroupIndex) const override;
	TArrayView<const float> GetJointGroupValues(uint16 JointGroupIndex) const override;
	TArrayView<const uint16> GetJointGroupJointIndices(uint16 JointGroupIndex) const override;
	TArrayView<const uint16> GetBlendShapeChannelLODs() const override;
	TArrayView<const uint16> GetBlendShapeChannelOutputIndices() const override;
	TArrayView<const uint16> GetBlendShapeChannelInputIndices() const override;
	TArrayView<const uint16> GetAnimatedMapLODs() const override;
	TArrayView<const uint16> GetAnimatedMapInputIndices() const override;
	TArrayView<const uint16> GetAnimatedMapOutputIndices() const override;
	TArrayView<const float> GetAnimatedMapFromValues() const override;
	TArrayView<const float> GetAnimatedMapToValues() const override;
	TArrayView<const float> GetAnimatedMapSlopeValues() const override;
	TArrayView<const float> GetAnimatedMapCutValues() const override;
	// Geometry
	uint32 GetVertexPositionCount(uint16 MeshIndex) const override;
	FVector GetVertexPosition(uint16 MeshIndex, uint32 VertexIndex) const override;
	TArrayView<const float> GetVertexPositionXs(uint16 MeshIndex) const override;
	TArrayView<const float> GetVertexPositionYs(uint16 MeshIndex) const override;
	TArrayView<const float> GetVertexPositionZs(uint16 MeshIndex) const override;
	uint32 GetVertexTextureCoordinateCount(uint16 MeshIndex) const override;
	FTextureCoordinate GetVertexTextureCoordinate(uint16 MeshIndex, uint32 TextureCoordinateIndex) const override;
	uint32 GetVertexNormalCount(uint16 MeshIndex) const override;
	FVector GetVertexNormal(uint16 MeshIndex, uint32 NormalIndex) const override;
	uint32 GetFaceCount(uint16 MeshIndex) const override;
	TArrayView<const uint32> GetFaceVertexLayoutIndices(uint16 MeshIndex, uint32 FaceIndex) const override;
	uint32 GetVertexLayoutCount(uint16 MeshIndex) const override;
	FVertexLayout GetVertexLayout(uint16 MeshIndex, uint32 LayoutIndex) const override;
	uint16 GetMaximumInfluencePerVertex(uint16 MeshIndex) const override;
	uint32 GetSkinWeightsCount(uint16 MeshIndex) const override;
	TArrayView<const float> GetSkinWeightsValues(uint16 MeshIndex, uint32 VertexIndex) const override;
	TArrayView<const uint16> GetSkinWeightsJointIndices(uint16 MeshIndex, uint32 VertexIndex) const override;
	uint16 GetBlendShapeTargetCount(uint16 MeshIndex) const override;
	uint16 GetBlendShapeChannelIndex(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const override;
	uint32 GetBlendShapeTargetDeltaCount(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const override;
	FVector GetBlendShapeTargetDelta(uint16 MeshIndex, uint16 BlendShapeTargetIndex, uint32 DeltaIndex) const override;
	TArrayView<const float> GetBlendShapeTargetDeltaXs(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const override;
	TArrayView<const float> GetBlendShapeTargetDeltaYs(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const override;
	TArrayView<const float> GetBlendShapeTargetDeltaZs(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const override;
	TArrayView<const uint32> GetBlendShapeTargetVertexIndices(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const override;
	void Unload(EDNADataLayer /*unused*/) override;

private:
	dna::Reader* Unwrap() const override;

private:
	/** Both BehaviorReader and DesignDataStreamReader are StreamReaders from DNAAsset 	   
	  * split out into run-time and in-editor parts from a full DNA that is either:
	  * 1) imported manually into SkeletalMesh asset through ContentBrowser
	  *	2) overwritten by GeneSplicer (GeneSplicerDNAReader) in a transient SkeletalMesh copy
	  * They both just borrow DNAAsset's readers and are not owned by SkelMeshDNAReader
	 **/
	IBehaviorReader* BehaviorStreamReader = nullptr;
	IGeometryReader* GeometryStreamReader = nullptr;
};
