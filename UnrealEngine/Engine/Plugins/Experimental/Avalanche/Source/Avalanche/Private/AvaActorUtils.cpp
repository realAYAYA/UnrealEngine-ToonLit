// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaActorUtils.h"
#include "AvaDefs.h"
#include "AvaSceneItem.h"
#include "AvaSceneSubsystem.h"
#include "AvaSceneTree.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/AvaGameInstance.h"
#include "GameFramework/Actor.h"
#include "IAvaSceneInterface.h"
#include "Math/MathFwd.h"
#include "Math/OrientedBox.h"

#if WITH_EDITOR
#include "AvaOutlinerDefines.h"
#include "AvaOutlinerSubsystem.h"
#include "AvaOutlinerUtils.h"
#include "IAvaOutliner.h"
#endif

FOrientedBox FAvaActorUtils::MakeOrientedBox(const FBox& InLocalBox, const FTransform& InWorldTransform)
{
	FOrientedBox OutOrientedBox;

	OutOrientedBox.Center = InWorldTransform.TransformPosition(InLocalBox.GetCenter());

	OutOrientedBox.AxisX = InWorldTransform.TransformVector(FVector::UnitX());
	OutOrientedBox.AxisY = InWorldTransform.TransformVector(FVector::UnitY());
	OutOrientedBox.AxisZ = InWorldTransform.TransformVector(FVector::UnitZ());

	OutOrientedBox.ExtentX = (InLocalBox.Max.X - InLocalBox.Min.X) / 2.f;
	OutOrientedBox.ExtentY = (InLocalBox.Max.Y - InLocalBox.Min.Y) / 2.f;
	OutOrientedBox.ExtentZ = (InLocalBox.Max.Z - InLocalBox.Min.Z) / 2.f;

	return OutOrientedBox;
}

FBox FAvaActorUtils::GetActorLocalBoundingBox(const AActor* InActor, bool bIncludeFromChildActors, bool bMustBeRegistered)
{
	FBox Box(ForceInit);
	Box.IsValid = 0;

	if (!InActor || !InActor->GetRootComponent())
	{
		return Box;
	}

	FTransform ActorToWorld = InActor->GetTransform();
	ActorToWorld.SetScale3D(FVector::OneVector);
	const FTransform WorldToActor = ActorToWorld.Inverse();

	uint32 FailedComponentCount = 0;
	InActor->ForEachComponent<UPrimitiveComponent>(bIncludeFromChildActors, [&FailedComponentCount, bMustBeRegistered, &WorldToActor, &Box](const UPrimitiveComponent* InPrimComp)
		{
#if WITH_EDITOR
			// Ignore Visualization Components, but don't consider them as failed components.
			if (InPrimComp->IsVisualizationComponent())
			{
				return;
			}
#endif

	if (InPrimComp->IsRegistered() || !bMustBeRegistered)
	{
		const FTransform ComponentToActor = InPrimComp->GetComponentTransform() * WorldToActor;
		Box += InPrimComp->CalcBounds(ComponentToActor).GetBox();
	}
	else
	{
		FailedComponentCount++;
	}
		});

	// Actors with no Failed Primitives should still return a valid Box with no Extents and 0,0,0 origin (local).
	if (FailedComponentCount == 0)
	{
		Box.IsValid = 1;
	}

	return Box;
}

FBox FAvaActorUtils::GetComponentLocalBoundingBox(const USceneComponent* InComponent)
{
	FBox Box(ForceInit);
	Box.IsValid = 0;

	if (!InComponent)
	{
		return Box;
	}

	if (!InComponent->IsRegistered())
	{
		return Box;
	}

#if WITH_EDITOR
	if (InComponent->IsVisualizationComponent())
	{
		return Box;
	}
#endif

	// Pre-scale component to be consistent with actor bounding boxes
	const FTransform ComponentTransform = FTransform(FRotator::ZeroRotator, FVector::ZeroVector, InComponent->GetComponentScale());
	const FBoxSphereBounds BoxSphereBounds = InComponent->CalcBounds(ComponentTransform);
	Box = BoxSphereBounds.GetBox();
	Box.IsValid = 1;

	return Box;
}

IAvaSceneInterface* FAvaActorUtils::GetSceneInterfaceFromActor(const AActor* InActor)
{
	if (!InActor)
	{
		return nullptr;
	}

	const UWorld* World = InActor->GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const UAvaSceneSubsystem* SceneSubsystem = World->GetSubsystem<UAvaSceneSubsystem>();
	if (!SceneSubsystem)
	{
		return nullptr;
	}

	return SceneSubsystem->GetSceneInterface(InActor->GetLevel());
}

AActor* FAvaActorUtils::ActorFromReferenceContainer(AActor* const InActor, const EAvaReferenceContainer InReferenceContainer, const bool bInIgnoreHiddenActors)
{
	if (InReferenceContainer == EAvaReferenceContainer::Other || !IsValid(InActor))
	{
		return nullptr;
	}

	// Note: Using Typed Outer World, instead of GetWorld(), since the typed outer World could be a streamed in world
	// and GetWorld() only returns the main world.
	UWorld* const World = InActor->GetTypedOuter<UWorld>();
	if (!IsValid(World))
	{
		return nullptr;
	}

	AActor* const ParentActor = InActor->GetAttachParentActor();

	bool bIsOutlinerAttachedActors = false;
	TArray<AActor*> AttachedActors;

#if WITH_EDITOR
	UAvaOutlinerSubsystem* const OutlinerSubsystem = World->GetSubsystem<UAvaOutlinerSubsystem>();
	if (IsValid(OutlinerSubsystem))
	{
		TSharedPtr<IAvaOutliner> AvaOutliner = OutlinerSubsystem->GetOutliner();
		if (AvaOutliner.IsValid())
		{
			bIsOutlinerAttachedActors = true;
			if (IsValid(ParentActor))
			{
				AttachedActors = FAvaOutlinerUtils::EditorOutlinerChildActors(AvaOutliner, ParentActor);
			}
			else // no valid parent, use world actors instead
			{
				TArray<FAvaOutlinerItemPtr> OutlinerRootChildren = AvaOutliner->GetTreeRoot()->GetChildren();
				const bool bValidRootActors = FAvaOutlinerUtils::EditorOutlinerItemsToActors(OutlinerRootChildren, AttachedActors);
			}
		}
	}
#endif

	if (!bIsOutlinerAttachedActors)
	{
		if (const IAvaSceneInterface* SceneInterface = FAvaActorUtils::GetSceneInterfaceFromActor(InActor))
		{
			const FAvaSceneTree& SceneTree = SceneInterface->GetSceneTree();

			if (!IsValid(ParentActor))
			{
				for (const int32 ChildId : SceneTree.GetRootNode().GetChildrenIndices())
				{
					const FAvaSceneItem* Item = SceneTree.GetItemAtIndex(ChildId);
					AttachedActors.Add(Item->Resolve<AActor>(World));
				}
			}
			else
			{
				AttachedActors = SceneTree.GetChildActors(ParentActor);
			}
		}
	}

	if (AttachedActors.Num() == 0)
	{
		return nullptr;
	}

	if (bInIgnoreHiddenActors)
	{
		for (int32 ChildIndex = AttachedActors.Num() - 1; ChildIndex >= 0; --ChildIndex)
		{
			if (AttachedActors[ChildIndex]->IsHidden()
#if WITH_EDITOR
				|| AttachedActors[ChildIndex]->IsTemporarilyHiddenInEditor()
#endif
				)
			{
				AttachedActors.RemoveAt(ChildIndex);
			}
		}
	}

	int32 FromActorIndex = -1;
	if (!AttachedActors.Find(InActor, FromActorIndex))
	{
		return nullptr;
	}

	switch (InReferenceContainer)
	{
		case EAvaReferenceContainer::Previous:
		{
			const int32 PreviousActorIndex = FromActorIndex - 1;
			if (!AttachedActors.IsValidIndex(PreviousActorIndex))
			{
				return ParentActor;
			}
			return AttachedActors[PreviousActorIndex];
		}
		case EAvaReferenceContainer::Next:
		{
			const int32 NextActorIndex = FromActorIndex + 1;
			if (!AttachedActors.IsValidIndex(NextActorIndex))
			{
				return nullptr;
			}
			return AttachedActors[NextActorIndex];
		}
		case EAvaReferenceContainer::First:
		{
			if (!AttachedActors.IsValidIndex(0))
			{
				return nullptr;
			}
			return AttachedActors[0] != InActor ? AttachedActors[0] : nullptr;
		}
		case EAvaReferenceContainer::Last:
		{
			AActor* const LastChildActor = AttachedActors.Last();
			if (!IsValid(LastChildActor))
			{
				return nullptr;
			}
			return LastChildActor != InActor ? LastChildActor : nullptr;
		}
	}

	return nullptr;
}
