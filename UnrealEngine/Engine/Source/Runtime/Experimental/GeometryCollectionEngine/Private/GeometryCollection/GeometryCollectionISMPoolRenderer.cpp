// Copyright Epic Games, Inc. All Rights Reserved.
#include "GeometryCollection/GeometryCollectionISMPoolRenderer.h"

#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GeometryCollection/Facades/CollectionInstancedMeshFacade.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionISMPoolActor.h"
#include "GeometryCollection/GeometryCollectionISMPoolComponent.h"
#include "GeometryCollection/GeometryCollectionISMPoolSubSystem.h"
#include "GeometryCollection/GeometryCollectionObject.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GeometryCollectionISMPoolRenderer)

void UGeometryCollectionISMPoolRenderer::OnRegisterGeometryCollection(UGeometryCollectionComponent const& InComponent)
{
}

void UGeometryCollectionISMPoolRenderer::OnUnregisterGeometryCollection()
{
	ReleaseGroup(MergedMeshGroup);
	ReleaseGroup(InstancesGroup);

	ISMPoolActor = nullptr;
}

void UGeometryCollectionISMPoolRenderer::UpdateState(UGeometryCollection const& InGeometryCollection, FTransform const& InComponentTransform, bool bIsBroken, bool bIsVisible)
{
	ComponentTransform = InComponentTransform;

	if (bIsVisible == false)
	{
		ReleaseGroup(InstancesGroup);
		ReleaseGroup(MergedMeshGroup);
	}
	else
	{
		if (!bIsBroken && MergedMeshGroup.GroupIndex == INDEX_NONE)
		{
			// Remove broken primitives.
			ReleaseGroup(InstancesGroup);

			// Add merged mesh.
			InitMergedMeshFromGeometryCollection(InGeometryCollection);
		}

		if (bIsBroken && InstancesGroup.GroupIndex == INDEX_NONE)
		{
			// Remove merged mesh.
			ReleaseGroup(MergedMeshGroup);

			// Add broken primitives.
			InitInstancesFromGeometryCollection(InGeometryCollection);
		}
	}
}

void UGeometryCollectionISMPoolRenderer::UpdateRootTransform(UGeometryCollection const& InGeometryCollection, FTransform const& InRootTransform)
{
	UpdateMergedMeshTransforms(InRootTransform * ComponentTransform);
}

void UGeometryCollectionISMPoolRenderer::UpdateTransforms(UGeometryCollection const& InGeometryCollection, TArrayView<const FTransform> InTransforms)
{
	UpdateInstanceTransforms(InGeometryCollection, ComponentTransform, InTransforms);
}

UGeometryCollectionISMPoolComponent* UGeometryCollectionISMPoolRenderer::GetOrCreateISMPoolComponent()
{
	if (ISMPoolActor == nullptr)
	{
		if (UGeometryCollectionISMPoolSubSystem* ISMPoolSubSystem = UWorld::GetSubsystem<UGeometryCollectionISMPoolSubSystem>(GetWorld()))
		{
			ISMPoolActor = ISMPoolSubSystem->FindISMPoolActor();
		}
	}
	return ISMPoolActor != nullptr ? ISMPoolActor->GetISMPoolComp() : nullptr;
}

void UGeometryCollectionISMPoolRenderer::InitMergedMeshFromGeometryCollection(UGeometryCollection const& InGeometryCollection)
{
	if (InGeometryCollection.RootProxyData.ProxyMeshes.Num() == 0)
	{
		return;
	}

	UGeometryCollectionISMPoolComponent* ISMPoolComponent = GetOrCreateISMPoolComponent();
	MergedMeshGroup.GroupIndex = ISMPoolComponent != nullptr ? ISMPoolComponent->CreateMeshGroup() : INDEX_NONE;

	if (MergedMeshGroup.GroupIndex == INDEX_NONE)
	{
		return;
	}

	for (UStaticMesh* StaticMesh : InGeometryCollection.RootProxyData.ProxyMeshes)
	{
		if (StaticMesh == nullptr)
		{
			continue;
		}

		FGeometryCollectionStaticMeshInstance StaticMeshInstance;
		StaticMeshInstance.StaticMesh = StaticMesh;
		StaticMeshInstance.Desc.bUseHISM = true;

		TArray<float> DummyCustomData;
		MergedMeshGroup.MeshIds.Add(ISMPoolComponent->AddMeshToGroup(MergedMeshGroup.GroupIndex, StaticMeshInstance, 1, DummyCustomData));
	}
}

void UGeometryCollectionISMPoolRenderer::InitInstancesFromGeometryCollection(UGeometryCollection const& InGeometryCollection)
{
	const int32 NumMeshes = InGeometryCollection.AutoInstanceMeshes.Num();
	if (NumMeshes == 0)
	{
		return;
	}

	UGeometryCollectionISMPoolComponent* ISMPoolComponent = GetOrCreateISMPoolComponent();
	InstancesGroup.GroupIndex = ISMPoolComponent != nullptr ? ISMPoolComponent->CreateMeshGroup() : INDEX_NONE;

	if (InstancesGroup.GroupIndex == INDEX_NONE)
	{
		return;
	}

	InstancesGroup.MeshIds.Reserve(NumMeshes);

	for (const FGeometryCollectionAutoInstanceMesh& AutoInstanceMesh : InGeometryCollection.AutoInstanceMeshes)
	{
		if (const UStaticMesh* StaticMesh = AutoInstanceMesh.Mesh)
		{
			bool bMaterialOverride = false;
			for (int32 MatIndex = 0; MatIndex < AutoInstanceMesh.Materials.Num(); MatIndex++)
			{
				const UMaterialInterface* OriginalMaterial = StaticMesh->GetMaterial(MatIndex);
				if (OriginalMaterial != AutoInstanceMesh.Materials[MatIndex])
				{
					bMaterialOverride = true;
					break;
				}
			}
			FGeometryCollectionStaticMeshInstance StaticMeshInstance;
			StaticMeshInstance.StaticMesh = const_cast<UStaticMesh*>(StaticMesh);
			if (bMaterialOverride)
			{
				StaticMeshInstance.MaterialsOverrides = AutoInstanceMesh.Materials;
			}
			StaticMeshInstance.Desc.bUseHISM = true;

			TArray<float> DummyCustomData;
			InstancesGroup.MeshIds.Add(ISMPoolComponent->AddMeshToGroup(InstancesGroup.GroupIndex, StaticMeshInstance, AutoInstanceMesh.NumInstances, DummyCustomData));
		}
	}
}

void UGeometryCollectionISMPoolRenderer::UpdateMergedMeshTransforms(FTransform const& InBaseTransform)
{
	if (MergedMeshGroup.GroupIndex == INDEX_NONE)
	{
		return;
	}

	UGeometryCollectionISMPoolComponent* ISMPoolComponent = GetOrCreateISMPoolComponent();
	if (ISMPoolComponent == nullptr)
	{
		return;
	}

	TArrayView<const FTransform> InstanceTransforms(&InBaseTransform, 1);
	for (int32 MeshIndex = 0; MeshIndex < MergedMeshGroup.MeshIds.Num(); MeshIndex++)
	{
		ISMPoolComponent->BatchUpdateInstancesTransforms(MergedMeshGroup.GroupIndex, MergedMeshGroup.MeshIds[MeshIndex], 0, InstanceTransforms, true/*bWorldSpace*/, false/*bMarkRenderStateDirty*/, false/*bTeleport*/);
	}
}

void UGeometryCollectionISMPoolRenderer::UpdateInstanceTransforms(UGeometryCollection const& InGeometryCollection, FTransform const& InBaseTransform, TArrayView<const FTransform> InTransforms)
{
	if (InstancesGroup.GroupIndex == INDEX_NONE)
	{
		return;
	}

	UGeometryCollectionISMPoolComponent* ISMPoolComponent = GetOrCreateISMPoolComponent();
	if (ISMPoolComponent == nullptr)
	{
		return;
	}

	const GeometryCollection::Facades::FCollectionInstancedMeshFacade InstancedMeshFacade(*InGeometryCollection.GetGeometryCollection());
	if (!InstancedMeshFacade.IsValid())
	{
		return;
	}

	const int32 NumTransforms = InGeometryCollection.NumElements(FGeometryCollection::TransformAttribute);
	const TManagedArray<TSet<int32>>& Children = InGeometryCollection.GetGeometryCollection()->Children;

	TArray<FTransform> InstanceTransforms;
	for (int32 MeshIndex = 0; MeshIndex < InstancesGroup.MeshIds.Num(); MeshIndex++)
	{
		InstanceTransforms.Reset(NumTransforms); // Allocate for worst case
		for (int32 TransformIndex = 0; TransformIndex < NumTransforms; TransformIndex++)
		{
			const int32 AutoInstanceMeshIndex = InstancedMeshFacade.GetIndex(TransformIndex);
			if (AutoInstanceMeshIndex == MeshIndex && Children[TransformIndex].Num() == 0)
			{
				InstanceTransforms.Add(InTransforms[TransformIndex] * InBaseTransform);
			}
		}
		
		ISMPoolComponent->BatchUpdateInstancesTransforms(InstancesGroup.GroupIndex, InstancesGroup.MeshIds[MeshIndex], 0, MakeArrayView(InstanceTransforms), true/*bWorldSpace*/, false/*bMarkRenderStateDirty*/, false/*bTeleport*/);
	}
}

void UGeometryCollectionISMPoolRenderer::ReleaseGroup(FISMPoolGroup& InOutGroup)
{
	UGeometryCollectionISMPoolComponent* ISMPoolComponent = GetOrCreateISMPoolComponent();
	if (ISMPoolComponent != nullptr)
	{
		ISMPoolComponent->DestroyMeshGroup(InOutGroup.GroupIndex);
	}

	InOutGroup.GroupIndex = INDEX_NONE;
	InOutGroup.MeshIds.Empty();
}
