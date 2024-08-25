// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosVDDetailsView.h"

#include "ChaosVDParticleActor.h"
#include "GameFramework/Actor.h"
#include "PropertyEditorModule.h"
#include "SSubobjectInstanceEditor.h"
#include "SSubobjectEditor.h"
#include "SSubobjectEditorModule.h"
#include "Visualizers/IChaosVDParticleVisualizationDataProvider.h"


void SChaosVDDetailsView::Construct(const FArguments& InArgs)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = false;
	DetailsViewArgs.bLockable = true;
	DetailsViewArgs.bAllowFavoriteSystem = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ObjectsUseNameArea | FDetailsViewArgs::ComponentsAndActorsUseNameArea;
	DetailsViewArgs.bCustomNameAreaLocation = true;
	DetailsViewArgs.bCustomFilterAreaLocation = false;
	DetailsViewArgs.bShowSectionSelector = false;
	DetailsViewArgs.bShowScrollBar = false;

	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	FModuleManager::LoadModuleChecked<FSubobjectEditorModule>("SubobjectEditor");

	SubobjectEditor = SNew(SSubobjectInstanceEditor)
		.ObjectContext(this, &SChaosVDDetailsView::GetRootContextObject)
		.AllowEditing(false)
		.OnSelectionUpdated(this, &SChaosVDDetailsView::OnSelectedSubobjectsChanged);

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(10.f, 4.f, 0.f, 0.f)
		.AutoHeight()
		[
			DetailsView->GetNameAreaWidget().ToSharedRef()
		]
		+SVerticalBox::Slot()
		[
			SNew(SSplitter)
			.MinimumSlotHeight(40.0f)
			.Orientation(Orient_Vertical)
			.Style(FAppStyle::Get(), "SplitterDark")
			.PhysicalSplitterHandleSize(2.0f)
			+SSplitter::Slot()
			.Value(0.2f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.Padding(FMargin(2.0f, 0.0f, 2.0f, 0.0f))
				[
					SubobjectEditor.ToSharedRef()
				]
			]
			+SSplitter::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				[
					DetailsView.ToSharedRef()
				]
			]
		]
	];
}

void SChaosVDDetailsView::SetSelectedObject(UObject* NewObject)
{
	if (DetailsView->IsLocked())
	{
		return;
	}

	CurrentObjectInView = NewObject;

	DetailsView->SetObject(NewObject, true);
	SubobjectEditor->UpdateTree();
}

void SChaosVDDetailsView::OnSelectedSubobjectsChanged(const TArray<TSharedPtr<FSubobjectEditorTreeNode>>& SelectedNodes)
{
	if (SelectedNodes.Num() == 0)
	{
		return;
	}

	if (!DetailsView->IsLocked())
	{
		const bool bContainsRootActor = SelectedNodes.ContainsByPredicate([](const FSubobjectEditorTreeNodePtrType& Node)
		{
			if (Node.IsValid())
			{
				if (FSubobjectData* Data = Node->GetDataSource())
				{
					return Data->IsRootActor();
				}
			}

			return false;
		});

		if (bContainsRootActor)
		{
			DetailsView->SetObject(GetRootContextObject());
		}
		else
		{
			TArray<UObject*> Components;
		
			if (AActor* ContextAsActor = Cast<AActor>(GetRootContextObject()))
			{
				Components.Reserve(SelectedNodes.Num());
                for (const FSubobjectEditorTreeNodePtrType& Node : SelectedNodes)
                {
                	if (Node.IsValid())
                	{
                		if (FSubobjectData* Data = Node->GetDataSource())
                		{
                			if (Data->IsComponent())
                			{
                				if (const UActorComponent* Component = Data->FindComponentInstanceInActor(ContextAsActor))
                				{
                					if (Component != ContextAsActor->GetRootComponent())
                					{
                						Components.Add(const_cast<UActorComponent*>(Component));
                					}
                				}
                			}
                		}
                	}
                }
			}

			DetailsView->SetObjects(Components);
		}
	}
}
