// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFramework/LightWeightInstanceStaticMeshManager.h"

#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Elements/SMInstance/SMInstanceElementId.h"
#include "GameFramework/LightWeightInstanceSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Templates/Greater.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(LightWeightInstanceStaticMeshManager)

#if WITH_EDITOR
#include "Editor.h"
#else
#include "Engine/World.h"
#include "TimerManager.h"
#endif // WITH_EDITOR

ALightWeightInstanceStaticMeshManager::ALightWeightInstanceStaticMeshManager(const FObjectInitializer& ObjectInitializer)
	:Super(ObjectInitializer)
{
	InstancedStaticMeshComponent = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("InstancedStaticMeshComponent0"));
	SetRootComponent(InstancedStaticMeshComponent);
	AddInstanceComponent(InstancedStaticMeshComponent);

	if (StaticMesh.IsValid())
	{
		OnStaticMeshSet();
	}

	SetInstancedStaticMeshParams();
}

void ALightWeightInstanceStaticMeshManager::SetRepresentedClass(UClass* ActorClass)
{
	Super::SetRepresentedClass(ActorClass);

	AActor* ActorCDO = RepresentedClass ? Cast<AActor>(RepresentedClass->GetDefaultObject()) : nullptr;
	if (ActorCDO)
	{
		BaseInstanceName = ActorCDO->GetName();
		SetStaticMeshFromActor(ActorCDO);
	}
	else
	{
		BaseInstanceName.Reset();
		ClearStaticMesh();
	}

	if (InstancedStaticMeshComponent)
	{
		InstancedStaticMeshComponent->OnPostLoadPerInstanceData();
	}
}

int32 ALightWeightInstanceStaticMeshManager::ConvertCollisionIndexToLightWeightIndex(int32 InIndex) const
{
	if (ensureMsgf(RenderingIndicesToDataIndices.IsValidIndex(InIndex), TEXT("Invalid index [ %d ]"), InIndex))
	{
		return RenderingIndicesToDataIndices[InIndex];
	}

	return InIndex;
}

int32 ALightWeightInstanceStaticMeshManager::ConvertLightWeightIndexToCollisionIndex(int32 InIndex) const
{
	if (ensureMsgf(DataIndicesToRenderingIndices.IsValidIndex(InIndex), TEXT("Invalid index [ %d ]"), InIndex))
	{
		return DataIndicesToRenderingIndices[InIndex];
	}

	return InIndex;
}

void ALightWeightInstanceStaticMeshManager::AddNewInstanceAt(FLWIData* InitData, int32 Index)
{
	Super::AddNewInstanceAt(InitData, Index);
	AddInstanceToRendering(Index);
}

void ALightWeightInstanceStaticMeshManager::RemoveInstance(const int32 Index)
{
#if WITH_EDITOR
	Modify();
#endif

	RemoveInstanceFromRendering(Index);
	Super::RemoveInstance(Index);
}

void ALightWeightInstanceStaticMeshManager::AddInstanceToRendering(int32 DataIndex)
{
	if (!ensureMsgf(RenderingIndicesToDataIndices.Contains(DataIndex) == false, TEXT("LWI rendering instance added more than once. Index: %d"), DataIndex))
	{
		return;
	}

	//cancel any pending deletes
	const bool bPendingDeleteCancelled = DataIndicesToBeDeleted.RemoveSingle(DataIndex) > 0;

	// The rendering indices are tightly packed so we know it's going on the end of the array
	const int32 RenderingIdx = RenderingIndicesToDataIndices.Add(DataIndex);

	UE_LOG(LogLightWeightInstance, Verbose,
		TEXT("ALightWeightInstanceStaticMeshManager::AddInstanceToRendering - manager [ %s ] adding instance at data index [ %d ] rendering index [ %d ], did cancel pending delete? [ %s ]"),
		*GetActorNameOrLabel(),
		DataIndex,
		RenderingIdx,
		bPendingDeleteCancelled ? TEXT("true") : TEXT("false"));

	// Now that we know the rendering index we can fill in the other side of the map
	if (DataIndex >= DataIndicesToRenderingIndices.Num())
	{
		ensure(DataIndex == DataIndicesToRenderingIndices.Add(RenderingIdx));
	}
	else
	{
		DataIndicesToRenderingIndices[DataIndex] = RenderingIdx;
	}

	// Update the HISMC
	if (InstancedStaticMeshComponent)
	{
		ensure(RenderingIdx == InstancedStaticMeshComponent->AddInstance(InstanceTransforms[DataIndex], /*bWorldSpace*/false));
	}
}

void ALightWeightInstanceStaticMeshManager::RemoveInstanceFromRendering(int32 DataIndex)
{
	if (IsIndexValid(DataIndex))
	{
		if (DataIndicesToBeDeleted.IsEmpty())
		{
			// We can't remove the indices right away because we could have a situation where we receive multiple requests to convert an instance to an actor in the same frame	
#if WITH_EDITOR
			if (GEditor)
			{
				GEditor->GetTimerManager()->SetTimerForNextTick(this, &ALightWeightInstanceStaticMeshManager::PostRemoveInstanceFromRendering);
			}
			else
#endif // WITH_EDITOR
			{
				GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ALightWeightInstanceStaticMeshManager::PostRemoveInstanceFromRendering);
			}
		}

		DataIndicesToBeDeleted.AddUnique(DataIndex);

		UE_LOG(LogLightWeightInstance, Verbose,
			TEXT("ALightWeightInstanceStaticMeshManager::RemoveInstanceFromRendering - manager [ %s ] removing instance at data index [ %d ]"),
			*GetActorNameOrLabel(),
			DataIndex);
	}
}

void ALightWeightInstanceStaticMeshManager::PostRemoveInstanceFromRendering()
{
	TArray<int32> RenderingIndicesToBeDeleted;
	for (int32 DataIndex : DataIndicesToBeDeleted)
	{
		RenderingIndicesToBeDeleted.Add(DataIndicesToRenderingIndices[DataIndex]);

		UE_LOG(LogLightWeightInstance, Verbose,
			TEXT("ALightWeightInstanceStaticMeshManager::PostRemoveInstanceFromRendering - manager [ %s ] removing instance at data index [ %d ] rendering index [ %d ]"),
			*GetActorNameOrLabel(),
			DataIndex,
			DataIndicesToRenderingIndices[DataIndex]);
	}

	RenderingIndicesToBeDeleted.Sort();
	int32 Count = RenderingIndicesToBeDeleted.Num();
	for (int32 Idx = Count-1; Idx >= 0; Idx--)
	{
		int32 RenderingIndex = RenderingIndicesToBeDeleted[Idx];
		if (RenderingIndex != INDEX_NONE)
		{
			if (InstancedStaticMeshComponent)
			{
				InstancedStaticMeshComponent->RemoveInstance(RenderingIndex);
			}
			RenderingIndicesToDataIndices.RemoveAtSwap(RenderingIndex);

			// fix up the other side of the map to match the change we just made
			// if we removed the last element than nothing was moved so we're done
			if (RenderingIndex < RenderingIndicesToDataIndices.Num())
			{
				// find the data index that corresponds with the changed rendering index
				const int32 ShiftedDataIndex = RenderingIndicesToDataIndices[RenderingIndex];

				DataIndicesToRenderingIndices[ShiftedDataIndex] = RenderingIndex;
			}
		}
	}

	DataIndicesToBeDeleted.Reset();
}

void ALightWeightInstanceStaticMeshManager::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALightWeightInstanceStaticMeshManager, StaticMesh);
	DOREPLIFETIME(ALightWeightInstanceStaticMeshManager, RenderingIndicesToDataIndices);
	DOREPLIFETIME(ALightWeightInstanceStaticMeshManager, DataIndicesToRenderingIndices);
}

void ALightWeightInstanceStaticMeshManager::OnRep_StaticMesh()
{
	OnStaticMeshSet();
}

void ALightWeightInstanceStaticMeshManager::OnRep_Transforms()
{
	Super::OnRep_Transforms();

	if (InstancedStaticMeshComponent)
	{
		InstancedStaticMeshComponent->AddInstance(InstanceTransforms.Last(), /*bWorldSpace*/false);
	}
}

void ALightWeightInstanceStaticMeshManager::PostActorSpawn(const FActorInstanceHandle& Handle)
{
	Super::PostActorSpawn(Handle);

	// remove the rendered instance from the HISMC
	RemoveInstanceFromRendering(Handle.GetInstanceIndex());
}

void ALightWeightInstanceStaticMeshManager::SetInstancedStaticMeshParams()
{
	FName CollisionProfileName(TEXT("LightWeightInstancedStaticMeshPhysics"));
	InstancedStaticMeshComponent->SetCollisionProfileName(CollisionProfileName);

	InstancedStaticMeshComponent->CanCharacterStepUpOn = ECB_Owner;
	InstancedStaticMeshComponent->CastShadow = true;
	InstancedStaticMeshComponent->bCastDynamicShadow = true;
	InstancedStaticMeshComponent->bCastStaticShadow = true;
	InstancedStaticMeshComponent->PrimaryComponentTick.bCanEverTick = false;
	// Allows updating in game, while optimizing rendering for the case that it is not modified
	InstancedStaticMeshComponent->Mobility = EComponentMobility::Movable;
	// Allows per-instance selection in the editor
	InstancedStaticMeshComponent->bHasPerInstanceHitProxies = true;

	while(InstancedStaticMeshComponent->InstancingRandomSeed == 0)
	{
		InstancedStaticMeshComponent->InstancingRandomSeed = FMath::Rand();
	}
} 

void ALightWeightInstanceStaticMeshManager::SetStaticMeshFromActor(AActor* InActor)
{
	ensureMsgf(false, TEXT("ALightWeightInstanceManager::SetStaticMeshFromActor was called on %s. Projects should override this function in subclasses."), *GetNameSafe(InActor));
	ClearStaticMesh();
}

void ALightWeightInstanceStaticMeshManager::OnStaticMeshSet()
{
#if WITH_EDITOR
	Modify();
#endif
	
	if (InstancedStaticMeshComponent)
	{
		EComponentMobility::Type Mobility = InstancedStaticMeshComponent->Mobility;
		if (Mobility == EComponentMobility::Static)
		{
			InstancedStaticMeshComponent->SetMobility(EComponentMobility::Stationary);
			InstancedStaticMeshComponent->SetStaticMesh(StaticMesh.Get());
			InstancedStaticMeshComponent->SetMobility(Mobility);
		}
		else
		{
			InstancedStaticMeshComponent->SetStaticMesh(StaticMesh.Get());
		}

		if (UStaticMesh* Mesh = StaticMesh.Get())
		{
			for (int32 Idx = 0; Idx < Mesh->GetStaticMaterials().Num(); ++Idx)
			{
				InstancedStaticMeshComponent->SetMaterial(Idx, Mesh->GetMaterial(Idx));
			}
		}
	}
}

void ALightWeightInstanceStaticMeshManager::ClearStaticMesh()
{
#if WITH_EDITOR
	Modify();
#endif
	
	StaticMesh = nullptr;
	OnStaticMeshSet();
}

FText ALightWeightInstanceStaticMeshManager::GetSMInstanceDisplayName(const FSMInstanceId& InstanceId) const
{
	check(InstanceId.ISMComponent == InstancedStaticMeshComponent);

	const FText OwnerDisplayName = FText::FromString(RepresentedClass ? RepresentedClass->GetName() : AActor::GetName());
	return FText::Format(NSLOCTEXT("LightWeightInstanceStaticMeshManager", "DisplayNameFmt", "{0} - Instance {1}"), OwnerDisplayName, InstanceId.InstanceIndex);
}

FText ALightWeightInstanceStaticMeshManager::GetSMInstanceTooltip(const FSMInstanceId& InstanceId) const
{
	check(InstanceId.ISMComponent == InstancedStaticMeshComponent);
	
	const FText OwnerDisplayPath = FText::FromString(GetPathName(GetWorld())); // stops the path at the level of the world the object is in
	return FText::Format(NSLOCTEXT("LightWeightInstanceStaticMeshManager", "TooltipFmt", "Instance {0} on {1}"), InstanceId.InstanceIndex, OwnerDisplayPath);
}

bool ALightWeightInstanceStaticMeshManager::CanEditSMInstance(const FSMInstanceId& InstanceId) const
{
	check(InstanceId.ISMComponent == InstancedStaticMeshComponent);

	ISMInstanceManager* InstanceManager = InstancedStaticMeshComponent;
	return InstanceManager->CanEditSMInstance(InstanceId);
}

bool ALightWeightInstanceStaticMeshManager::CanMoveSMInstance(const FSMInstanceId& InstanceId, const ETypedElementWorldType InWorldType) const
{
	check(InstanceId.ISMComponent == InstancedStaticMeshComponent);

	ISMInstanceManager* InstanceManager = InstancedStaticMeshComponent;
	return InstanceManager->CanMoveSMInstance(InstanceId, InWorldType);
}

bool ALightWeightInstanceStaticMeshManager::GetSMInstanceTransform(const FSMInstanceId& InstanceId, FTransform& OutInstanceTransform, bool bWorldSpace) const
{
	check(InstanceId.ISMComponent == InstancedStaticMeshComponent);

	if (RenderingIndicesToDataIndices.IsValidIndex(InstanceId.InstanceIndex))
	{
		const int32 DataIndex = RenderingIndicesToDataIndices[InstanceId.InstanceIndex];
		OutInstanceTransform = bWorldSpace ? InstanceTransforms[DataIndex] * GetActorTransform() : InstanceTransforms[DataIndex];
		return true;
	}
	
	return false;
}

bool ALightWeightInstanceStaticMeshManager::SetSMInstanceTransform(const FSMInstanceId& InstanceId, const FTransform& InstanceTransform, bool bWorldSpace, bool bMarkRenderStateDirty, bool bTeleport)
{
	check(InstanceId.ISMComponent == InstancedStaticMeshComponent);

	if (RenderingIndicesToDataIndices.IsValidIndex(InstanceId.InstanceIndex) && InstancedStaticMeshComponent->UpdateInstanceTransform(InstanceId.InstanceIndex, InstanceTransform, bWorldSpace, bMarkRenderStateDirty, bTeleport))
	{
		const int32 DataIndex = RenderingIndicesToDataIndices[InstanceId.InstanceIndex];
		InstancedStaticMeshComponent->GetInstanceTransform(InstanceId.InstanceIndex, InstanceTransforms[DataIndex], /*bWorldSpace*/false);
		return true;
	}

	return false;
}

void ALightWeightInstanceStaticMeshManager::NotifySMInstanceMovementStarted(const FSMInstanceId& InstanceId)
{
	check(InstanceId.ISMComponent == InstancedStaticMeshComponent);

	ISMInstanceManager* InstanceManager = InstancedStaticMeshComponent;
	InstanceManager->NotifySMInstanceMovementStarted(InstanceId);
}

void ALightWeightInstanceStaticMeshManager::NotifySMInstanceMovementOngoing(const FSMInstanceId& InstanceId)
{
	check(InstanceId.ISMComponent == InstancedStaticMeshComponent);

	ISMInstanceManager* InstanceManager = InstancedStaticMeshComponent;
	InstanceManager->NotifySMInstanceMovementOngoing(InstanceId);
}

void ALightWeightInstanceStaticMeshManager::NotifySMInstanceMovementEnded(const FSMInstanceId& InstanceId)
{
	check(InstanceId.ISMComponent == InstancedStaticMeshComponent);

	ISMInstanceManager* InstanceManager = InstancedStaticMeshComponent;
	InstanceManager->NotifySMInstanceMovementEnded(InstanceId);
}

void ALightWeightInstanceStaticMeshManager::NotifySMInstanceSelectionChanged(const FSMInstanceId& InstanceId, const bool bIsSelected)
{
	check(InstanceId.ISMComponent == InstancedStaticMeshComponent);

	ISMInstanceManager* InstanceManager = InstancedStaticMeshComponent;
	InstanceManager->NotifySMInstanceSelectionChanged(InstanceId, bIsSelected);
}

bool ALightWeightInstanceStaticMeshManager::DeleteSMInstances(TArrayView<const FSMInstanceId> InstanceIds)
{
	TArray<int32> DataIndices;
	GetLWIDataIndices(InstanceIds, DataIndices);

	// Sort so Remove doesn't alter the indices of items still to remove
	DataIndices.Sort(TGreater<int32>());

	Modify();
	InstancedStaticMeshComponent->Modify();

	for (int32 DataIndex : DataIndices)
	{
		RemoveInstance(DataIndex);
	}

	return true;
}

bool ALightWeightInstanceStaticMeshManager::DuplicateSMInstances(TArrayView<const FSMInstanceId> InstanceIds, TArray<FSMInstanceId>& OutNewInstanceIds)
{
	TArray<int32> DataIndices;
	GetLWIDataIndices(InstanceIds, DataIndices);

	Modify();
	InstancedStaticMeshComponent->Modify();

	TArray<int32> NewDataIndices;
	DuplicateLWIInstances(DataIndices, NewDataIndices);

	OutNewInstanceIds.Reset(NewDataIndices.Num());
	for (int32 NewDataIndex : NewDataIndices)
	{
		OutNewInstanceIds.Add(FSMInstanceId{ InstancedStaticMeshComponent, DataIndicesToRenderingIndices[NewDataIndex] });
	}

	return true;
}

void ALightWeightInstanceStaticMeshManager::DuplicateLWIInstances(TArrayView<const int32> DataIndices, TArray<int32>& OutNewDataIndices)
{
	OutNewDataIndices.Reset(DataIndices.Num());
	for (int32 DataIndex : DataIndices)
	{
		FLWIData InitData;
		InitData.Transform = InstanceTransforms[DataIndex];

		const int32 NewDataIndex = AddNewInstance(&InitData);
		if (DataIndex != INDEX_NONE)
		{
			OutNewDataIndices.Add(NewDataIndex);
		}
	}
}

void ALightWeightInstanceStaticMeshManager::GetLWIDataIndices(TArrayView<const FSMInstanceId> InstanceIds, TArray<int32>& OutDataIndices) const
{
	OutDataIndices.Reset(InstanceIds.Num());
	for (const FSMInstanceId& InstanceId : InstanceIds)
	{
		check(InstanceId.ISMComponent == InstancedStaticMeshComponent);
		if (RenderingIndicesToDataIndices.IsValidIndex(InstanceId.InstanceIndex))
		{
			OutDataIndices.Add(RenderingIndicesToDataIndices[InstanceId.InstanceIndex]);
		}
	}
}

int32 ALightWeightInstanceStaticMeshManager::ConvertInternalIndexToHandleIndex(int32 InInternalIndex) const
{
	return DataIndicesToRenderingIndices[InInternalIndex];
}

int32 ALightWeightInstanceStaticMeshManager::ConvertHandleIndexToInternalIndex(int32 InHandleIndex) const
{
	return RenderingIndicesToDataIndices[InHandleIndex];
}

