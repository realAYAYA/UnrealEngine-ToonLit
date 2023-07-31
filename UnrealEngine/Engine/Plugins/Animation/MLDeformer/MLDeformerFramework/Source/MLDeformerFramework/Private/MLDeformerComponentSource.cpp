// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerComponentSource.h"

#include "MLDeformerComponent.h"
#include "MLDeformerModelInstance.h"

#include "Components/SkeletalMeshComponent.h"
#include "SkeletalRenderPublic.h"


#define LOCTEXT_NAMESPACE "MLDeformerComponentSource"


FName UMLDeformerComponentSource::Domains::Vertex("Vertex");


FText UMLDeformerComponentSource::GetDisplayName() const
{
	return LOCTEXT("MLDeformerComponent", "ML Deformer Component");
}


TSubclassOf<UActorComponent> UMLDeformerComponentSource::GetComponentClass() const
{
	return UMLDeformerComponent::StaticClass();
}


TArray<FName> UMLDeformerComponentSource::GetExecutionDomains() const
{
	return {Domains::Vertex};
}


int32 UMLDeformerComponentSource::GetLodIndex(const UActorComponent* InComponent) const
{
	const UMLDeformerComponent* DeformerComponent = Cast<UMLDeformerComponent>(InComponent);
	const UMLDeformerModelInstance* ModelInstance = DeformerComponent ? DeformerComponent->GetModelInstance() : nullptr;
	const USkeletalMeshComponent* SkeletalMeshComponent = ModelInstance ? ModelInstance->GetSkeletalMeshComponent() : nullptr;
	const FSkeletalMeshObject* SkeletalMeshObject = SkeletalMeshComponent ? SkeletalMeshComponent->MeshObject : nullptr;
	return SkeletalMeshObject ? SkeletalMeshObject->GetLOD() : 0;
}


bool UMLDeformerComponentSource::GetComponentElementCountsForExecutionDomain(
	FName InDomainName,
	const UActorComponent* InComponent,
	int32 InLodIndex,
	TArray<int32>& OutElementCounts
	) const
{
	if (InDomainName != Domains::Vertex)
	{
		return false;
	}

	const UMLDeformerComponent* DeformerComponent = Cast<UMLDeformerComponent>(InComponent);
	if (!DeformerComponent)
	{
		return false;
	}
	
	const UMLDeformerModelInstance* ModelInstance = DeformerComponent->GetModelInstance();
	if (!ModelInstance)
	{
		return false;
	}
		
	const FSkeletalMeshObject* SkeletalMeshObject = ModelInstance->GetSkeletalMeshComponent()->MeshObject;
	if (!SkeletalMeshObject)
	{
		return false;
	}
	FSkeletalMeshRenderData const& SkeletalMeshRenderData = SkeletalMeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = &SkeletalMeshRenderData.LODRenderData[InLodIndex];
		
	OutElementCounts.Reset(LodRenderData->RenderSections.Num());
	for (FSkelMeshRenderSection const& RenderSection: LodRenderData->RenderSections)
	{
		OutElementCounts.Add(RenderSection.NumVertices);
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
