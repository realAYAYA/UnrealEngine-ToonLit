// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvalancheEffectorsModule.h"

#include "AvaActorUtils.h"
#include "AvaSceneTree.h"
#include "IAvaSceneInterface.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/CEClonerSubsystem.h"

#if WITH_EDITOR
#include "IAvaOutliner.h"
#include "AvaOutlinerDefines.h"
#include "AvaOutlinerSubsystem.h"
#include "Item/AvaOutlinerActor.h"
#endif

void FAvalancheEffectorsModule::StartupModule()
{
	UCEClonerSubsystem::OnSubsystemInitialized().AddRaw(this, &FAvalancheEffectorsModule::RegisterCustomActorResolver);
}

void FAvalancheEffectorsModule::ShutdownModule()
{
	UCEClonerSubsystem::OnSubsystemInitialized().RemoveAll(this);

	UnregisterCustomActorResolver();
}

void FAvalancheEffectorsModule::RegisterCustomActorResolver()
{
	if (UCEClonerSubsystem* Subsystem = UCEClonerSubsystem::Get())
	{
		Subsystem->RegisterCustomActorResolver(UCEClonerSubsystem::FOnGetOrderedActors::CreateRaw(this, &FAvalancheEffectorsModule::GetOrderedChildrenActors));
	}
}

void FAvalancheEffectorsModule::UnregisterCustomActorResolver()
{
	if (UObjectInitialized())
	{
		if (UCEClonerSubsystem* Subsystem = UCEClonerSubsystem::Get())
		{
			Subsystem->UnregisterCustomActorResolver();
		}
	}
}

TArray<AActor*> FAvalancheEffectorsModule::GetOrderedChildrenActors(const AActor* InParentActor)
{
	TArray<AActor*> ChildrenActors;

	if (!InParentActor)
	{
		return ChildrenActors;
	}

	UWorld* ClonerWorld = InParentActor->GetTypedOuter<UWorld>();
	if (!ClonerWorld)
	{
		return ChildrenActors;
	}

#if WITH_EDITOR
	if (const UAvaOutlinerSubsystem* const OutlinerSubsystem = ClonerWorld->GetSubsystem<UAvaOutlinerSubsystem>())
	{
		if (const TSharedPtr<IAvaOutliner> AvaOutliner = OutlinerSubsystem->GetOutliner())
		{
			const FAvaOutlinerItemPtr OutlinerClonerItem = AvaOutliner->FindItem(InParentActor);
			if (OutlinerClonerItem.IsValid())
			{
				for (FAvaOutlinerItemPtr OutlinerChild : OutlinerClonerItem->GetChildren())
				{
					if (!OutlinerChild.IsValid() || !OutlinerChild->IsA<FAvaOutlinerActor>())
					{
						continue;
					}

					if (const FAvaOutlinerActor* OutlinerActor = OutlinerChild->CastTo<FAvaOutlinerActor>())
					{
						ChildrenActors.Add(OutlinerActor->GetActor());
					}
				}
			}

			return ChildrenActors;
		}
	}
#endif

	if (const IAvaSceneInterface* SceneInterface = FAvaActorUtils::GetSceneInterfaceFromActor(InParentActor))
	{
		TArray<AActor*> AttachedActors;
		InParentActor->GetAttachedActors(AttachedActors, true, false);
		ChildrenActors.SetNumUninitialized(AttachedActors.Num(), EAllowShrinking::No);
		const FAvaSceneTree& SceneTree = SceneInterface->GetSceneTree();

		for (AActor* ChildActor : AttachedActors)
		{
			if (const FAvaSceneTreeNode* ChildNode = SceneTree.FindTreeNode(FAvaSceneItem(ChildActor, ClonerWorld)))
			{
				const int32 ChildIndex = ChildNode->GetLocalIndex();

				if (ChildrenActors.IsValidIndex(ChildIndex))
				{
					ChildrenActors[ChildIndex] = ChildActor;
				}
			}
		}
	}
	else
	{
		InParentActor->GetAttachedActors(ChildrenActors, true, false);
	}

	return ChildrenActors;
}

IMPLEMENT_MODULE(FAvalancheEffectorsModule, AvalancheEffectors)
