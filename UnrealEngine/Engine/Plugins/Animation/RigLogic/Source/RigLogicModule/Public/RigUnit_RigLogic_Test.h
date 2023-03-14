// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "CoreMinimal.h"
#include "UObject/StrongObjectPtr.h"
#include "Rigs/RigHierarchy.h"

#include "DNAReader.h"

#if WITH_DEV_AUTOMATION_TESTS
	#include "RigUnit_RigLogic.h"
#endif

struct FRigUnit_RigLogic_JointUpdateParams;

//** A unit test-only helper class to prevent FString from being deleted as it is converted from FString*//

class TestBehaviorReader: public IBehaviorReader
{
public:
	void Unload(EDNADataLayer Layer) override
	{

	}

	// DescriptorReader methods start
	FString GetName() const override
	{
		return {};
	}

	EArchetype GetArchetype() const override
	{
		return {};
	}

	EGender GetGender() const override
	{
		return {};
	}

	uint16 GetAge() const override
	{
		return {};
	}

	uint32 GetMetaDataCount() const override
	{
		return {};
	}

	FString GetMetaDataKey(uint32 Index) const override
	{
		return {};
	}

	FString GetMetaDataValue(const FString& Key) const override
	{
		return {};
	}

	ETranslationUnit GetTranslationUnit() const override
	{
		return {};
	}

	ERotationUnit GetRotationUnit() const override
	{
		return {};
	}

	FCoordinateSystem GetCoordinateSystem() const override
	{
		return {};
	}

	uint16 GetLODCount() const override
	{
		return LODCount;
	}

	uint16 GetDBMaxLOD() const override
	{
		return {};
	}

	FString GetDBComplexity() const override
	{
		return {};
	}

	FString GetDBName() const override
	{
		return {};
	}

	// DefinitionReader methods start
	uint16 GetGUIControlCount() const override
	{
		return {};
	}

	FString GetGUIControlName(uint16 Index) const override
	{
		return {};
	}

	uint16 GetRawControlCount() const override
	{
		return rawControls.Num();
	}

	FString GetRawControlName(uint16 Index) const override
	{
		return rawControls[Index];
	}

	uint16 GetAnimatedMapCount() const override
	{
		return animatedMaps.Num();
	}

	FString GetAnimatedMapName(uint16 Index) const override
	{
		return animatedMaps[Index];
	}

	TArrayView<const uint16> GetAnimatedMapIndicesForLOD(uint16 lod) const override
	{
		if (lod == 1)
		{
			return TArrayView<const uint16_t>(animatedMapIndicesLOD1.GetData(), animatedMapIndicesLOD1.Num());
		}
		else
		{
			return TArrayView<const uint16_t>(animatedMapIndicesLOD0.GetData(), animatedMapIndicesLOD0.Num());
		}
	}

	uint16 GetJointCount() const override
	{
		return static_cast<uint16>(jointNames.Num());
	}

	FString GetJointName(uint16 Index) const override
	{
		return jointNames[Index];
	}

	TArrayView<const uint16> GetJointIndicesForLOD(uint16 lod) const override
	{
		if (lod == 1)
		{
			return TArrayView<const uint16_t>(jointIndicesLOD1.GetData(), jointIndicesLOD1.Num());
		}
		else
		{
			return TArrayView<const uint16_t>(jointIndicesLOD0.GetData(), jointIndicesLOD0.Num());
		}
	}

	uint16 GetJointParentIndex(uint16 Index) const override
	{
		return {};
	}

	uint16 GetBlendShapeChannelCount() const override
	{
		return static_cast<uint16>(blendShapeChannelNames.Num());
	}

	FString GetBlendShapeChannelName(uint16 Index) const override
	{
		return blendShapeChannelNames[Index];
	}

	TArrayView<const uint16> GetBlendShapeChannelIndicesForLOD(uint16 lod) const override
	{
		if (lod == 0)
		{
			return TArrayView<const uint16_t>(blendShapeIndicesForLOD0.GetData(), blendShapeIndicesForLOD0.Num());
		}
		else
		{
			return TArrayView<const uint16_t>(blendShapeIndicesForLOD1.GetData(), blendShapeIndicesForLOD1.Num());
		}
	}

	uint16 GetMeshCount() const override
	{
		return {};
	}

	FString GetMeshName(uint16 Index) const override
	{		
		return meshNames[Index];
	}

	uint16 GetMeshBlendShapeChannelMappingCount() const override
	{
		return meshBlendShapeChannelMappings.Num();
	}

	FMeshBlendShapeChannelMapping GetMeshBlendShapeChannelMapping(uint16 Index) const override
	{
		return meshBlendShapeChannelMappings[Index];
	}

	TArrayView<const uint16> GetMeshBlendShapeChannelMappingIndicesForLOD(uint16 lod) const override
	{
		if (lod == 0)
		{
			return TArrayView<const uint16_t>(blendShapeMappingIndicesLOD0.GetData(), blendShapeMappingIndicesLOD0.Num());
		}
		else
		{
			return TArrayView<const uint16_t>(blendShapeMappingIndicesLOD1.GetData(), blendShapeMappingIndicesLOD1.Num());
		}
	}

	TArrayView<const uint16> GetMeshIndicesForLOD(uint16 lod) const override
	{
		if (lod == 0)
		{
			return TArrayView<const uint16_t>(meshIndicesLOD0.GetData(), meshIndicesLOD0.Num());
		}
		else
		{
			return TArrayView<const uint16_t>(meshIndicesLOD1.GetData(), meshIndicesLOD1.Num());
		}
	}

	uint16 GetJointIndexListCount() const override
	{
		return {};
	}

	uint16 GetBlendShapeChannelIndexListCount() const override
	{
		return {};
	}

	uint16 GetAnimatedMapIndexListCount() const override
	{
		return {};
	}

	uint16 GetMeshIndexListCount() const override
	{
		return {};
	}

	FVector GetNeutralJointTranslation(uint16 Index) const override
	{
		return {};
	}

	FVector GetNeutralJointRotation(uint16 Index) const override
	{
		return {};
	}

	// BehaviorReader methods start
	TArrayView<const uint16> GetGUIToRawInputIndices() const override
	{
		return {};
	}

	TArrayView<const uint16> GetGUIToRawOutputIndices() const override
	{
		return {};
	}

	TArrayView<const float> GetGUIToRawFromValues() const override
	{
		return {};
	}

	TArrayView<const float> GetGUIToRawToValues() const override
	{
		return {};
	}

	TArrayView<const float> GetGUIToRawSlopeValues() const override
	{
		return {};
	}

	TArrayView<const float> GetGUIToRawCutValues() const override
	{
		return {};
	}

	uint16 GetPSDCount() const override
	{
		return {};
	}

	TArrayView<const uint16> GetPSDRowIndices() const override
	{
		return {};
	}

	TArrayView<const uint16> GetPSDColumnIndices() const override
	{
		return {};
	}

	TArrayView<const float> GetPSDValues() const override
	{
		return {};
	}

	TArrayView<const uint16> GetJointVariableAttributeIndices(uint16 lod) const override
	{
		return {};
	}

	uint16 GetJointRowCount() const override
	{
		return 0;
	}

	uint16 GetJointColumnCount() const override
	{
		return 0;
	}

	uint16 GetJointGroupCount() const override
	{
		return 0;
	}

	TArrayView<const uint16> GetJointGroupLODs(uint16 jointGroupIndex) const override
	{
		return {};
	}

	TArrayView<const uint16> GetJointGroupInputIndices(uint16 jointGroupIndex) const override
	{
		return {};
	}

	TArrayView<const uint16> GetJointGroupOutputIndices(uint16 jointGroupIndex) const override
	{
		return {};
	}

	TArrayView<const float> GetJointGroupValues(uint16 jointGroupIndex) const override
	{
		return {};
	}

	TArrayView<const uint16> GetJointGroupJointIndices(uint16 jointGroupIndex) const override
	{
		return {};
	}

	TArrayView<const uint16> GetBlendShapeChannelLODs() const override
	{
		return {};
	}

	TArrayView<const uint16> GetBlendShapeChannelInputIndices() const override
	{
		return {};
	}

	TArrayView<const uint16> GetBlendShapeChannelOutputIndices() const override
	{
		return {};
	}

	TArrayView<const uint16> GetAnimatedMapLODs() const override
	{
		return {};
	}

	TArrayView<const uint16> GetAnimatedMapInputIndices() const override
	{
		return {};
	}

	TArrayView<const uint16> GetAnimatedMapOutputIndices() const override
	{
		return {};
	}

	TArrayView<const float> GetAnimatedMapFromValues() const override
	{
		return {};
	}

	TArrayView<const float> GetAnimatedMapToValues() const override
	{
		return {};
	}

	TArrayView<const float> GetAnimatedMapSlopeValues() const override
	{
		return {};
	}

	TArrayView<const float> GetAnimatedMapCutValues() const override
	{
		return {};
	}

	uint16 LODCount;

	TArray<FString> rawControls;

	TArray<FString> jointNames;
	TArray<uint16> jointIndicesLOD0;
	TArray<uint16> jointIndicesLOD1;

	TArray<FString> meshNames;
	TArray<uint16> meshIndicesLOD0; //indices into previous array
	TArray<uint16> meshIndicesLOD1; //indices into previous array

	TArray<FString> blendShapeChannelNames;
	TArray<uint16> blendShapeIndicesForLOD0; //indices into previous array
	TArray<uint16> blendShapeIndicesForLOD1;

	TArray<FMeshBlendShapeChannelMapping> meshBlendShapeChannelMappings; //contains all mappings from blendshapes to meshes
	TArray<uint16> blendShapeMappingIndicesLOD0; //indices into previous array
	TArray<uint16> blendShapeMappingIndicesLOD1; 

	TArray<FString> animatedMaps;
	TArray<uint16> animatedMapIndicesLOD0;
	TArray<uint16> animatedMapIndicesLOD1;

	void addJoint(FString newJointName)
	{
		jointNames.Add(newJointName);
	}

	void addBlendShapeChannelName( FString newBlendShapeName )
	{
		blendShapeChannelNames.Add(newBlendShapeName);
	}

	void addMeshName(FString newMeshName)
	{
		meshNames.Add(newMeshName);
	}

	void addBlendShapeMapping( uint16 meshIndex, uint16 blendShapeChannelIndex )
	{
		//{ (0,0), (0, 1), (0, 2), (1, 3), (1, 4), .... (5, 666) }
		//  ^^^^^^^^^^^^^^^^^^^^^   ^^^^^^^^^^^^^       ^^^^^^^^^
		//     mesh 1                 mesh 2             mesh n
		FMeshBlendShapeChannelMapping mapping;
		mapping.MeshIndex = meshIndex;
		mapping.BlendShapeChannelIndex = blendShapeChannelIndex;
		meshBlendShapeChannelMappings.Add(mapping);
	}

	void addBlendShapeMappingIndicesToLOD(uint16 MappingIndex, uint16 LODIndex)
	{
		if (LODIndex == 0)
		{
			blendShapeMappingIndicesLOD0.Add(MappingIndex);
		}
		else 
		{
			blendShapeMappingIndicesLOD1.Add(MappingIndex);
		}
	}

	void addAnimatedMapIndicesToLOD(uint16 MappingIndex, uint16 LODIndex)
	{
		if (LODIndex == 0)
		{
			animatedMapIndicesLOD0.Add(MappingIndex);
		}
		else
		{
			animatedMapIndicesLOD1.Add(MappingIndex);
		}
	}

private:
	dna::Reader* Unwrap() const
	{
		return nullptr;  // Unused
	}
};

struct FRigUnit_RigLogic::TestAccessor
{
	TestAccessor(FRigUnit_RigLogic* Unit);

	 /** MapInputCurve Tests **/

	TSharedPtr<TestBehaviorReader> CreateBehaviorReaderEmpty();
	TSharedPtr<TestBehaviorReader> CreateBehaviorReaderOneCurve(FString ControlNameStr);

	TStrongObjectPtr<URigHierarchy> CreateCurveContainerEmpty();
	TStrongObjectPtr<URigHierarchy> CreateCurveContainerOneCurve(FString CurveNameStr);

	void Exec_MapInputCurve(URigHierarchy* TestHierarchy);

	/** MapJoints Tests **/

	TSharedPtr<TestBehaviorReader> CreateBehaviorReaderTwoJoints(FString Joint1NameStr, FString Joint2NameStr);
	TStrongObjectPtr<URigHierarchy> CreateBoneHierarchyEmpty();
	TStrongObjectPtr<URigHierarchy> CreateBoneHierarchyTwoBones(FString Bone1NameStr, FString Bone2NameStr);

	void Exec_MapJoints(URigHierarchy* TestHierarchy);

	/** MapBlendShapes Tests **/

	TSharedPtr<TestBehaviorReader> CreateBehaviorReaderNoBlendshapes(FString MeshNameStr);
	TSharedPtr<TestBehaviorReader> CreateBehaviorReaderOneBlendShape(FString MeshNameStr, FString BlendShapeNameStr);
	TStrongObjectPtr<URigHierarchy> CreateCurveContainerOneMorphTarget(FString MorphTargetStr);
	TSharedPtr<TestBehaviorReader> CreateBehaviorReaderTwoBlendShapes(FString MeshNameStr, FString BlendShape1Str, FString BlendShape2Str);
	TStrongObjectPtr<URigHierarchy> CreateCurveContainerTwoMorphTargets(FString MorphTarget1Str, FString MorphTarget2Str);

	void Exec_MapMorphTargets(URigHierarchy* TestHierarchy);

	/** MapMaskMultipliers Tests **/
	TSharedPtr<TestBehaviorReader> CreateBehaviorReaderOneAnimatedMap(FString AnimatedMapNameStr);

	void Exec_MapMaskMultipliers(URigHierarchy* TestHierarchy);

	static const uint8 MAX_ATTRS_PER_JOINT;

	/** UpdateJoints Tests **/
	void AddToTransformArray(float* InArray, FTransform& Transform);
	FTransformArrayView CreateTwoJointNeutralTransforms(float *InValueArray);
	TArrayView<const uint16> CreateTwoJointVariableAttributes(uint16* InVariableAttributeIndices, uint8 LOD);

	void Exec_UpdateJoints(URigHierarchy* TestHierarchy, TArrayView<const float> NeutralJointValues, TArrayView<const float> DeltaJointValues);

	TSharedPtr<FSharedRigRuntimeContext> GetSharedRigRuntimeContext(USkeletalMesh* SkelMesh);

private:
	FName RawCtrlName;
	TStrongObjectPtr<URigHierarchy> Hierarchy;
	FRigUnit_RigLogic* Unit;

public:
	FRigUnit_RigLogic_Data* GetData() { return &(Unit->Data); };
};