// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/Modifiers/HLODModifierMeshDestruction.h"

#include "Components/StaticMeshComponent.h"
#include "IMeshMergeExtension.h"
#include "IMeshMergeUtilities.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MeshMergeDataTracker.h"
#include "MeshMergeModule.h"
#include "Modules/ModuleManager.h"
#include "StaticMeshAttributes.h"
#include "WorldPartition/HLOD/HLODBuilder.h"
#include "WorldPartition/HLOD/HLODDestruction.h"
#include "WorldPartition/HLOD/DestructibleHLODComponent.h"


UWorldPartitionHLODModifierMeshDestruction::UWorldPartitionHLODModifierMeshDestruction(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}



bool UWorldPartitionHLODModifierMeshDestruction::CanModifyHLOD(TSubclassOf<UHLODBuilder> InHLODBuilderClass) const
{
	return true;
}


// Use vertex color attributes to store component indices
// In the material, this allows us to mask destructed building parts
class FDestructionMeshMergeExtension : public IMeshMergeExtension
{
public:
	const FHLODBuildContext& HLODBuildContext;

	TArray<AActor*> DestructibleActors;
	UMaterialInterface* DestructibleMaterial;

	FDestructionMeshMergeExtension(const FHLODBuildContext& InHLODBuildContext) : HLODBuildContext(InHLODBuildContext)
	{
		FModuleManager::LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities().RegisterExtension(this);
	}

	virtual ~FDestructionMeshMergeExtension()
	{
		FModuleManager::LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities().UnregisterExtension(this);
	}

	virtual void OnCreatedMergedRawMeshes(const TArray<UStaticMeshComponent*>& MergedComponents, const class FMeshMergeDataTracker& DataTracker, TArray<FMeshDescription>& MergedMeshLODs) override
	{
		check(DestructibleActors.IsEmpty());

		// Create component to actor map & compute components to wedges offsets.
		TMap<uint32, uint32> ComponentToActorIndex;
		TArray<uint32> ComponentToWedgeOffsets;
		ComponentToWedgeOffsets.SetNumZeroed(MergedComponents.Num());

		for (int32 ComponentIndex = 0; ComponentIndex < MergedComponents.Num(); ++ComponentIndex)
		{
			ComponentToWedgeOffsets[ComponentIndex] = DataTracker.GetComponentToWedgeMappng(ComponentIndex, /*LODIndex=*/0);

			UStaticMeshComponent* StaticMeshComponent = MergedComponents[ComponentIndex];
			if (AActor* Actor = StaticMeshComponent->GetOwner())
			{
				if (Actor->Implements<UWorldPartitionDestructibleInHLODInterface>())
				{
					uint32 ActorIndex = DestructibleActors.AddUnique(Actor);
					ComponentToActorIndex.Add(ComponentIndex, ActorIndex);
				}
			}
		}

		// Clear vertex colors
		FMeshDescription& MergedMesh = MergedMeshLODs[0];
		TVertexInstanceAttributesRef<FVector4f> VertexInstanceColors = MergedMesh.VertexInstanceAttributes().GetAttributesRef<FVector4f>(MeshAttribute::VertexInstance::Color);
		for (FVertexInstanceID VertexInstanceID : MergedMesh.VertexInstances().GetElementIDs())
		{
			VertexInstanceColors[VertexInstanceID] = FVector4f(0.0f, 0.0f, 0.0f, 0.0f);
		}

		// Store actor index + 1 in the wedge colors for each part of the merged mesh
		for (int32 ComponentIndex = 0; ComponentIndex < MergedComponents.Num(); ++ComponentIndex)
		{
			const uint32 ComponentStart = ComponentToWedgeOffsets[ComponentIndex];
			const uint32 ComponentEnd = ComponentToWedgeOffsets.IsValidIndex(ComponentIndex + 1) ? ComponentToWedgeOffsets[ComponentIndex + 1] : MergedMesh.VertexInstances().Num();
			FColor ActorColor = FColor::Black;

			const uint32* ActorIndex = ComponentToActorIndex.Find(ComponentIndex);
			if (ActorIndex)
			{
				ActorColor.DWColor() = *ActorIndex + 1;
			}

			for (uint32 Index = ComponentStart; Index < ComponentEnd; ++Index)
			{
				FVertexInstanceID VertexInstanceID(Index);
				VertexInstanceColors[FVertexInstanceID(Index)] = FLinearColor(ActorColor);
			}
		}
	}

	virtual void OnCreatedProxyMaterial(const TArray<UStaticMeshComponent*>& MergedComponents, UMaterialInterface* ProxyMaterial) override
	{
		UMaterialInstanceConstant* Instance = Cast<UMaterialInstanceConstant>(ProxyMaterial);
		if (!Instance)
		{
			return;
		}

		TSet<AActor*> DestructibleActorsSet;
		for (UStaticMeshComponent* Component : MergedComponents)
		{
			if (Component->GetOwner()->Implements<UWorldPartitionDestructibleInHLODInterface>())
			{
				DestructibleActorsSet.Add(Component->GetOwner());
			}
		}

		// Ensure a destructible material is used
		const static FName EnableInstanceDestroyingParamName(TEXT("EnableInstanceDestroying"));
		FGuid ParamGUID;
		bool bEnableInstanceDestroying = false;
		bool bFoundParam = Instance->GetStaticSwitchParameterValue(EnableInstanceDestroyingParamName, bEnableInstanceDestroying, ParamGUID);
		if (bFoundParam && bEnableInstanceDestroying)
		{
			Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo("NumInstances"), static_cast<float>(FMath::RoundUpToPowerOfTwo(DestructibleActorsSet.Num())));
			Instance->BasePropertyOverrides.TwoSided = false;
			Instance->BasePropertyOverrides.bOverride_TwoSided = true;

			FStaticParameterSet CurrentParameters = Instance->GetStaticParameters();

			Instance->UpdateStaticPermutation(CurrentParameters);
			Instance->InitStaticPermutation();

			DestructibleMaterial = Instance;
		}
	}

	UWorldPartitionDestructibleHLODMeshComponent* GetDestructibleHLODComponent()
	{
		UWorldPartitionDestructibleHLODMeshComponent* Component = nullptr;

		if (!DestructibleActors.IsEmpty())
		{
			Component = NewObject<UWorldPartitionDestructibleHLODMeshComponent>();
			Component->SetIsReplicated(true);
			Component->SetWorldLocation(HLODBuildContext.WorldPosition);
			TArray<FName> DestructibleActorsNames;
			Algo::Transform(DestructibleActors, DestructibleActorsNames, [](AActor* Actor) { return Actor->GetFName(); });
			Component->SetDestructibleActors(DestructibleActorsNames);
			Component->SetDestructibleHLODMaterial(DestructibleMaterial);
		}

		return Component;
	}
};

void UWorldPartitionHLODModifierMeshDestruction::BeginHLODBuild(const FHLODBuildContext& InHLODBuildContext)
{
	DestructionMeshMergeExtension = new FDestructionMeshMergeExtension(InHLODBuildContext);
}

void UWorldPartitionHLODModifierMeshDestruction::EndHLODBuild(TArray<UActorComponent*>& InOutComponents)
{
	UWorldPartitionDestructibleHLODMeshComponent* DestructibleHLODComponent = DestructionMeshMergeExtension->GetDestructibleHLODComponent();
	if (DestructibleHLODComponent)
	{
		InOutComponents.Add(DestructibleHLODComponent);
	}

	delete DestructionMeshMergeExtension;
}
