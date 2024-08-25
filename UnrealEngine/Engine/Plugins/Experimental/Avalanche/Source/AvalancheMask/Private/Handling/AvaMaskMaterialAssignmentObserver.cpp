// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaMaskMaterialAssignmentObserver.h"

#include "Components/ActorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

FAvaMaskMaterialAssignmentObserver::FAvaMaskMaterialAssignmentObserver(UPrimitiveComponent* InComponent)
	: ComponentWeak(InComponent)
{
	check(InComponent);
	
	SlotCount = InComponent->GetNumMaterials();
	Materials.AddDefaulted(InComponent->GetNumMaterials());
	
	for (int32 MaterialIdx = 0; MaterialIdx < InComponent->GetNumMaterials(); ++MaterialIdx)
	{
		Materials[MaterialIdx] = InComponent->GetMaterial(MaterialIdx);
	}
	
	if (!UPrimitiveComponent::MarkRenderStateDirtyEvent.IsBoundToObject(this))
	{
		RenderStateDirtyHandle = UPrimitiveComponent::MarkRenderStateDirtyEvent.AddRaw(this, &FAvaMaskMaterialAssignmentObserver::OnComponentRenderStateDirty);
	}
}

FAvaMaskMaterialAssignmentObserver::~FAvaMaskMaterialAssignmentObserver()
{
	if (RenderStateDirtyHandle.IsValid())
	{
		UPrimitiveComponent::MarkRenderStateDirtyEvent.Remove(RenderStateDirtyHandle);
	}
}

FAvaMaskMaterialAssignmentObserver::FOnMaterialAssignedDelegate& FAvaMaskMaterialAssignmentObserver::OnMaterialAssigned()
{
	return OnMaterialAssignedDelegate;
}

void FAvaMaskMaterialAssignmentObserver::OnComponentRenderStateDirty(UActorComponent& InComponent)
{
	// @note: this is global delegate, so we need to check that we care about the component
	if (UPrimitiveComponent* Component = ComponentWeak.Get())
	{
		if (&InComponent == Component)
		{
			const int32 PreviousSlotCount = SlotCount;
			const int32 NewSlotCount = Component->GetNumMaterials();
			SlotCount = NewSlotCount;

			TArray<int32> ChangedMaterialSlots;
			ChangedMaterialSlots.Reserve(NewSlotCount);

			Materials.SetNum(NewSlotCount);
			for (int32 MaterialIdx = 0; MaterialIdx < Component->GetNumMaterials(); ++MaterialIdx)
			{
				if (UMaterialInterface* NewMaterial = Component->GetMaterial(MaterialIdx))
				{
					// Compare actual material
					if (Materials[MaterialIdx] != NewMaterial)
					{
						ChangedMaterialSlots.Add(MaterialIdx);
					}
				}
				
				Materials[MaterialIdx] = Component->GetMaterial(MaterialIdx);
			}

			if (!ChangedMaterialSlots.IsEmpty())
			{
				OnMaterialAssigned().Broadcast(Component, PreviousSlotCount, NewSlotCount, ChangedMaterialSlots);
			}
		}
	}
}
