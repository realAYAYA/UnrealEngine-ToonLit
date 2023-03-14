// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "Math/Vector.h"
#include "UObject/NoExportTypes.h"
#include "UObject/ObjectMacros.h"

#include "DNACommon.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDNAReader, Log, All);

namespace dna
{

class Reader;

}  // namespace dna

class IDescriptorReader
{
protected:
	virtual ~IDescriptorReader() = default;

public:
	virtual FString GetName() const = 0;
	virtual EArchetype GetArchetype() const = 0;
	virtual EGender GetGender() const = 0;
	virtual uint16 GetAge() const = 0;
	virtual uint32 GetMetaDataCount() const = 0;
	virtual FString GetMetaDataKey(uint32 Index) const = 0;
	virtual FString GetMetaDataValue(const FString& Key) const = 0;
	virtual ETranslationUnit GetTranslationUnit() const = 0;
	virtual ERotationUnit GetRotationUnit() const = 0;
	virtual FCoordinateSystem GetCoordinateSystem() const = 0;
	virtual uint16 GetLODCount() const = 0;
	virtual uint16 GetDBMaxLOD() const = 0;
	virtual FString GetDBComplexity() const = 0;
	virtual FString GetDBName() const = 0;

	virtual dna::Reader* Unwrap() const = 0;
	virtual void Unload(EDNADataLayer Layer) = 0;

};

class IDefinitionReader : public IDescriptorReader
{
protected:
	virtual ~IDefinitionReader() = default;

public:
	virtual uint16 GetGUIControlCount() const = 0;
	virtual FString GetGUIControlName(uint16 Index) const = 0;
	virtual uint16 GetRawControlCount() const = 0;
	virtual FString GetRawControlName(uint16 Index) const = 0;
	virtual uint16 GetJointCount() const = 0;
	virtual FString GetJointName(uint16 Index) const = 0;
	virtual uint16 GetJointIndexListCount() const = 0;
	virtual TArrayView<const uint16> GetJointIndicesForLOD(uint16 LOD) const = 0;
	virtual uint16 GetJointParentIndex(uint16 Index) const = 0;
	virtual uint16 GetBlendShapeChannelCount() const = 0;
	virtual FString GetBlendShapeChannelName(uint16 Index) const = 0;
	virtual uint16 GetBlendShapeChannelIndexListCount() const = 0;
	virtual TArrayView<const uint16> GetBlendShapeChannelIndicesForLOD(uint16 LOD) const = 0;
	virtual uint16 GetAnimatedMapCount() const = 0;
	virtual FString GetAnimatedMapName(uint16 Index) const = 0;
	virtual uint16 GetAnimatedMapIndexListCount() const = 0;
	virtual TArrayView<const uint16> GetAnimatedMapIndicesForLOD(uint16 LOD) const = 0;
	virtual uint16 GetMeshCount() const = 0;
	virtual FString GetMeshName(uint16 Index) const = 0;
	virtual uint16 GetMeshIndexListCount() const = 0;
	virtual TArrayView<const uint16> GetMeshIndicesForLOD(uint16 LOD) const = 0;
	virtual uint16 GetMeshBlendShapeChannelMappingCount() const = 0;
	virtual FMeshBlendShapeChannelMapping GetMeshBlendShapeChannelMapping(uint16 Index) const = 0;
	virtual TArrayView<const uint16> GetMeshBlendShapeChannelMappingIndicesForLOD(uint16 LOD) const = 0;
	virtual FVector GetNeutralJointTranslation(uint16 Index) const = 0;
	virtual FVector GetNeutralJointRotation(uint16 Index) const = 0;
};

class IBehaviorReader : public virtual IDefinitionReader
{
protected:
	virtual ~IBehaviorReader() = default;

public:
	virtual TArrayView<const uint16> GetGUIToRawInputIndices() const = 0;
	virtual TArrayView<const uint16> GetGUIToRawOutputIndices() const = 0;
	virtual TArrayView<const float> GetGUIToRawFromValues() const = 0;
	virtual TArrayView<const float> GetGUIToRawToValues() const = 0;
	virtual TArrayView<const float> GetGUIToRawSlopeValues() const = 0;
	virtual TArrayView<const float> GetGUIToRawCutValues() const = 0;
	virtual uint16 GetPSDCount() const = 0;
	virtual TArrayView<const uint16> GetPSDRowIndices() const = 0;
	virtual TArrayView<const uint16> GetPSDColumnIndices() const = 0;
	virtual TArrayView<const float> GetPSDValues() const = 0;
	virtual uint16 GetJointRowCount() const = 0;
	virtual uint16 GetJointColumnCount() const = 0;
	virtual TArrayView<const uint16> GetJointVariableAttributeIndices(uint16 LOD) const = 0;
	virtual uint16 GetJointGroupCount() const = 0;
	virtual TArrayView<const uint16> GetJointGroupLODs(uint16 JointGroupIndex) const = 0;
	virtual TArrayView<const uint16> GetJointGroupInputIndices(uint16 JointGroupIndex) const = 0;
	virtual TArrayView<const uint16> GetJointGroupOutputIndices(uint16 JointGroupIndex) const = 0;
	virtual TArrayView<const float> GetJointGroupValues(uint16 JointGroupIndex) const = 0;
	virtual TArrayView<const uint16> GetJointGroupJointIndices(uint16 JointGroupIndex) const = 0;
	virtual TArrayView<const uint16> GetBlendShapeChannelLODs() const = 0;
	virtual TArrayView<const uint16> GetBlendShapeChannelOutputIndices() const = 0;
	virtual TArrayView<const uint16> GetBlendShapeChannelInputIndices() const = 0;
	virtual TArrayView<const uint16> GetAnimatedMapLODs() const = 0;
	virtual TArrayView<const uint16> GetAnimatedMapInputIndices() const = 0;
	virtual TArrayView<const uint16> GetAnimatedMapOutputIndices() const = 0;
	virtual TArrayView<const float> GetAnimatedMapFromValues() const = 0;
	virtual TArrayView<const float> GetAnimatedMapToValues() const = 0;
	virtual TArrayView<const float> GetAnimatedMapSlopeValues() const = 0;
	virtual TArrayView<const float> GetAnimatedMapCutValues() const = 0;
};

class IGeometryReader : public virtual IDefinitionReader
{
protected:
	virtual ~IGeometryReader() = default;

public:
	virtual uint32 GetVertexPositionCount(uint16 MeshIndex) const = 0;
	virtual FVector GetVertexPosition(uint16 MeshIndex, uint32 VertexIndex) const = 0;
	virtual TArrayView<const float> GetVertexPositionXs(uint16 MeshIndex) const = 0;
	virtual TArrayView<const float> GetVertexPositionYs(uint16 MeshIndex) const = 0;
	virtual TArrayView<const float> GetVertexPositionZs(uint16 MeshIndex) const = 0;
	virtual uint32 GetVertexTextureCoordinateCount(uint16 MeshIndex) const = 0;
	virtual FTextureCoordinate GetVertexTextureCoordinate(uint16 MeshIndex, uint32 TextureCoordinateIndex) const = 0;
	virtual uint32 GetVertexNormalCount(uint16 MeshIndex) const = 0;
	virtual FVector GetVertexNormal(uint16 MeshIndex, uint32 NormalIndex) const = 0;
	virtual uint32 GetFaceCount(uint16 MeshIndex) const = 0;
	virtual TArrayView<const uint32> GetFaceVertexLayoutIndices(uint16 MeshIndex, uint32 FaceIndex) const = 0;
	virtual uint32 GetVertexLayoutCount(uint16 MeshIndex) const = 0;
	virtual FVertexLayout GetVertexLayout(uint16 MeshIndex, uint32 LayoutIndex) const = 0;
	virtual uint16 GetMaximumInfluencePerVertex(uint16 MeshIndex) const = 0;
	virtual uint32 GetSkinWeightsCount(uint16 MeshIndex) const = 0;
	virtual TArrayView<const float> GetSkinWeightsValues(uint16 MeshIndex, uint32 VertexIndex) const = 0;
	virtual TArrayView<const uint16> GetSkinWeightsJointIndices(uint16 MeshIndex, uint32 VertexIndex) const = 0;
	virtual uint16 GetBlendShapeTargetCount(uint16 MeshIndex) const = 0;
	virtual uint16 GetBlendShapeChannelIndex(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const = 0;
	virtual uint32 GetBlendShapeTargetDeltaCount(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const = 0;
	virtual FVector GetBlendShapeTargetDelta(uint16 MeshIndex, uint16 BlendShapeTargetIndex, uint32 DeltaIndex) const = 0;
	virtual TArrayView<const float> GetBlendShapeTargetDeltaXs(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const = 0;
	virtual TArrayView<const float> GetBlendShapeTargetDeltaYs(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const = 0;
	virtual TArrayView<const float> GetBlendShapeTargetDeltaZs(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const = 0;
	virtual TArrayView<const uint32> GetBlendShapeTargetVertexIndices(uint16 MeshIndex, uint16 BlendShapeTargetIndex) const = 0;
};

/**
	@brief UE interface for DNA Reader wrappers.
*/
class IDNAReader : public IBehaviorReader, public IGeometryReader
{
public:
	virtual ~IDNAReader() = default;
};
