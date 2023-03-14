// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerMorphModel.h"
#include "MLDeformerMorphModelInstance.h"
#include "MLDeformerModelInstance.h"
#include "MLDeformerComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Rendering/MorphTargetVertexInfoBuffers.h"

#define LOCTEXT_NAMESPACE "MLDeformerMorphModel"

UMLDeformerMorphModel::UMLDeformerMorphModel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MorphTargetSet = MakeShared<FExternalMorphSet>();
	MorphTargetSet->Name = GetClass()->GetFName();
}

void UMLDeformerMorphModel::Serialize(FArchive& Archive)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UMLDeformerMorphModel::Serialize)

	Super::Serialize(Archive);

	// Check if we have initialized our compressed morph buffers.
	bool bHasMorphData = false;
	if (Archive.IsSaving())
	{
		bHasMorphData = MorphTargetSet.IsValid() ? MorphTargetSet->MorphBuffers.IsMorphCPUDataValid() : false;
	}
	Archive << bHasMorphData;

	// Load or save the compressed morph buffers, if they exist.
	if (bHasMorphData)
	{
		check(MorphTargetSet.IsValid());
		Archive << MorphTargetSet->MorphBuffers;
	}
}

UMLDeformerModelInstance* UMLDeformerMorphModel::CreateModelInstance(UMLDeformerComponent* Component)
{
	return NewObject<UMLDeformerMorphModelInstance>(Component);
}

void UMLDeformerMorphModel::SetMorphTargetDeltaFloats(const TArray<float>& Deltas)
{
	FloatArrayToVector3Array(Deltas, MorphTargetDeltas);
}

void UMLDeformerMorphModel::SetMorphTargetDeltas(const TArray<FVector3f>& Deltas)
{
	MorphTargetDeltas = Deltas;
}

int32 UMLDeformerMorphModel::GetMorphTargetDeltaStartIndex(int32 BlendShapeIndex) const
{
	if (MorphTargetDeltas.Num() == 0)
	{
		return INDEX_NONE;
	}

	return GetNumBaseMeshVerts() * BlendShapeIndex;
}

void UMLDeformerMorphModel::BeginDestroy()
{
	if (MorphTargetSet.IsValid())
	{
		// Release and flush, waiting for the release to have completed, 
		// If we don't do this we can get an error that we destroy a render resource that is still initialized,
		// as the release happens in another thread.
		ReleaseResourceAndFlush(&MorphTargetSet->MorphBuffers);
		MorphTargetSet.Reset();
	}
	Super::BeginDestroy();
}

#undef LOCTEXT_NAMESPACE
