// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerTestModel.h"
#include "MLDeformerComponent.h"
#include "UObject/UObjectGlobals.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MLDeformerTestModel)

UMLDeformerModelInstance* UMLDeformerTestModel::CreateModelInstance(UMLDeformerComponent* Component)
{
	return NewObject<UTestModelInstance>(Component);
}

FString UMLDeformerTestModel::GetDisplayName() const 
{ 
	return FString("Test Model");
}

#if WITH_EDITOR
	void UMLDeformerTestModel::UpdateNumTargetMeshVertices()
	{
		SetNumTargetMeshVerts(GetNumBaseMeshVerts());
	}
#endif
