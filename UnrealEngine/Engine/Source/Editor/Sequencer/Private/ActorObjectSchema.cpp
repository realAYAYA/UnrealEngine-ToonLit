// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorObjectSchema.h"
#include "GameFramework/Actor.h"
#include "Modules/ModuleManager.h"
#include "ISequencer.h"
#include "ISequencerModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "SubobjectData.h"
#include "SubobjectDataSubsystem.h"

#include "MVVM/ObjectBindingModelStorageExtension.h"
#include "MVVM/Selection/SequencerOutlinerSelection.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/ViewModels/SequenceModel.h"
#include "MVVM/ViewModels/ObjectBindingModel.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "FActorSchema"

namespace UE::Sequencer
{

FText FActorSchema::GetPrettyName(const UObject* Object) const
{
	if (const AActor* Actor = Cast<const AActor>(Object))
	{
		return FText::FromString(Actor->GetActorLabel());
	}
	return FText::FromString(Object->GetName());
}

UObject* FActorSchema::GetParentObject(UObject* Object) const
{
	if (UActorComponent* Component = Cast<UActorComponent>(Object))
	{
		return Component->GetOwner();
	}

	return nullptr;
}

FObjectSchemaRelevancy FActorSchema::GetRelevancy(const UObject* InObject) const
{
	if (InObject->IsA<AActor>())
	{
		return AActor::StaticClass();
	}
	else if (InObject->IsA<UActorComponent>())
	{
		return UActorComponent::StaticClass();
	}
	return FObjectSchemaRelevancy();
}

TSharedPtr<FExtender> FActorSchema::ExtendObjectBindingMenu(TSharedRef<FUICommandList> CommandList, TWeakPtr<ISequencer> WeakSequencer, TArrayView<UObject* const> ContextSensitiveObjects) const
{
	TArray<AActor*> Actors;
	for (UObject* Object : ContextSensitiveObjects)
	{
		if (AActor* Actor = Cast<AActor>(Object))
		{
			Actors.Add(Actor);
		}
	}

	if (Actors.Num() > 0)
	{
		TSharedRef<FExtender> AddTrackMenuExtender = MakeShared<FExtender>();
		AddTrackMenuExtender->AddMenuExtension(
			SequencerMenuExtensionPoints::AddTrackMenu_PropertiesSection,
			EExtensionHook::Before,
			CommandList,
			FMenuExtensionDelegate::CreateRaw(this, &FActorSchema::HandleTrackMenuExtensionAddTrack, WeakSequencer, Actors));
		return AddTrackMenuExtender;
	}

	return nullptr;
}

void FActorSchema::HandleTrackMenuExtensionAddTrack(FMenuBuilder& AddTrackMenuBuilder, TWeakPtr<ISequencer> WeakSequencer, TArray<AActor*> Actors) const
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer)
	{
		return;
	}

	struct FComponentData
	{
		FString DisplayString;
		FString FullDisplayString;
		FText ComponentName;
		UActorComponent* ActorComponent;

		bool operator>(const FComponentData& Other) const
		{
			return DisplayString.Compare(Other.DisplayString) > 0;
		}
		bool operator<(const FComponentData& Other) const
		{
			return DisplayString.Compare(Other.DisplayString) < 0;
		}
	};

	TArray<FComponentData> ComponentData; 

	AddTrackMenuBuilder.BeginSection("Components", LOCTEXT("ComponentsSection", "Components"));
	{
		USubobjectDataSubsystem* DataSubsystem = USubobjectDataSubsystem::Get();
		check(DataSubsystem);

		for (AActor* Actor : Actors)
		{
			// Prefer to gather the components from the SubobjectDataSubsystem which is where the Details panel gets the list of components.
			// But FN character parts list is not visible through those means, so for now, just gather all of the components
			/*
			TArray<FSubobjectDataHandle> SubobjectData;
			DataSubsystem->GatherSubobjectData(Actor, SubobjectData);

			for (const FSubobjectDataHandle& Handle : SubobjectData)
			{
				if (UActorComponent* ActorComponent = const_cast<UActorComponent*>(Handle.GetData()->FindComponentInstanceInActor(Actor)))
				{
					if (Sequencer->GetHandleToObject(ActorComponent, false).IsValid())
					{
						continue;
					}

					FComponentData Data;
					Data.DisplayString = Handle.GetData()->GetDisplayString(false);
					Data.FullDisplayString = Handle.GetData()->GetDisplayString(true);
					Data.ComponentName = Handle.GetData()->GetDisplayName();
					Data.ActorComponent = ActorComponent;
					ComponentData.Add(Data);
				}
			}
			*/

			auto ComponentDataContainsComponent = [ComponentData](UActorComponent* ActorComponent)
			{
				for (const FComponentData& Data : ComponentData)
				{
					if (Data.ActorComponent == ActorComponent)
					{
						return true;
					}
				}

				return false;
			};

			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (!Component || 
					Component->IsVisualizationComponent() ||
					Sequencer->GetHandleToObject(Component, false).IsValid() ||
					ComponentDataContainsComponent(Component))
				{
					continue;
				}

				// Hack - forcibly allow USkeletalMeshComponentBudgeted until FORT-527888
				//static const FName SkeletalMeshComponentBudgetedClassName(TEXT("SkeletalMeshComponentBudgeted"));
				//if (Component->GetClass()->GetName() == SkeletalMeshComponentBudgetedClassName)
				{
					FComponentData Data;
					Data.DisplayString = Component->GetName();
					Data.FullDisplayString = Component->GetName();
					Data.ComponentName = FText::FromString(Component->GetName());
					ComponentData.Add(Data);
				}
			}
		}

		ComponentData.Sort();

		for (const FComponentData& Data : ComponentData)
		{
			FString DisplayString = Data.DisplayString;
			FString FullDisplayString = Data.FullDisplayString;
			FText ComponentName = Data.ComponentName;

			FUIAction AddComponentAction(FExecuteAction::CreateSP(this, &FActorSchema::HandleAddComponentActionExecute, ComponentName, WeakSequencer, Actors));
			FText AddComponentLabel = FText::FromString(DisplayString);
			FText AddComponentToolTip = FText::Format(LOCTEXT("ComponentToolTipFormat", "Add {0}"), FText::FromString(FullDisplayString));
			
			AddTrackMenuBuilder.AddMenuEntry(AddComponentLabel, AddComponentToolTip, FSlateIcon(), AddComponentAction);
		}
	}
	AddTrackMenuBuilder.EndSection();
}

void FActorSchema::HandleAddComponentActionExecute(FText ComponentName, TWeakPtr<ISequencer> WeakSequencer, TArray<AActor*> Actors) const
{
	const FScopedTransaction Transaction(LOCTEXT("AddComponent", "Add Component"));

	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	if (!Sequencer)
	{
		return;
	}

	FObjectBindingModelStorageExtension* ObjectStorage = Sequencer->GetViewModel()->GetRootModel()->CastDynamic<FObjectBindingModelStorageExtension>();
	check(ObjectStorage);

	TSharedPtr<FSequencerSelection> Selection = Sequencer->GetViewModel()->GetSelection();

	FSelectionEventSuppressor SupressEvents = Selection->SuppressEvents();
	Selection->Outliner.Empty();

	USubobjectDataSubsystem* DataSubsystem = USubobjectDataSubsystem::Get();
	check(DataSubsystem);

	for (AActor* Actor : Actors)
	{
		// See comment above: 
		// Prefer to gather the components from the SubobjectDataSubsystem which is where the Details panel gets the list of components.
		// But FN character parts list is not visible through those means, so for now, just gather all of the components
		/*
		TArray<FSubobjectDataHandle> SubobjectData;
		DataSubsystem->GatherSubobjectData(Actor, SubobjectData);

		for (const FSubobjectDataHandle& Handle : SubobjectData)
		{
			if (Handle.GetData()->GetDisplayName().EqualTo(ComponentName))
			{
				if (UActorComponent* ActorComponent = const_cast<UActorComponent*>(Handle.GetData()->FindComponentInstanceInActor(Actor)))
				{
					FGuid ObjectId = Sequencer->GetHandleToObject(ActorComponent);

					TSharedPtr<FObjectBindingModel> Model = ObjectStorage->FindModelForObjectBinding(ObjectId);
					if (Model)
					{
						Selection->Outliner.Select(Model);
					}
				}
				break;
			}
		}
		*/

		for (UActorComponent* Component : Actor->GetComponents())
		{
			if (Component && Component->GetFName() == ComponentName.ToString())
			{
				FGuid ObjectId = Sequencer->GetHandleToObject(Component);

				TSharedPtr<FObjectBindingModel> Model = ObjectStorage->FindModelForObjectBinding(ObjectId);
				if (Model)
				{
					Selection->Outliner.Select(Model);
				}
				break;
			}
		}
	}
}

} // namespace UE::Sequencer

#undef LOCTEXT_NAMESPACE