// Copyright Epic Games, Inc. All Rights Reserved.

#include "SPCGEditorGraphDebugObjectWidget.h"

#include "Framework/Views/TableViewMetadata.h"
#include "PCGComponent.h"
#include "PCGEditor.h"
#include "PCGEditorGraph.h"

#include "Editor/UnrealEdEngine.h"
#include "PropertyCustomizationHelpers.h"
#include "UnrealEdGlobals.h"
#include "Widgets/Input/SComboBox.h"

#define LOCTEXT_NAMESPACE "PCGEditorGraphDebugObjectWidget"

namespace PCGEditorGraphDebugObjectWidget
{
	const FString NoObjectString = TEXT("No debug object selected");
	const FString SelectionString = TEXT(" (selected)");
	const FString SeparatorString = TEXT(" / ");
}

void SPCGEditorGraphDebugObjectWidget::Construct(const FArguments& InArgs, TSharedPtr<FPCGEditor> InPCGEditor)
{
	PCGEditorPtr = InPCGEditor;

	DebugObjects.Add(MakeShared<FPCGEditorGraphDebugObjectInstance>(PCGEditorGraphDebugObjectWidget::NoObjectString));

	const TSharedRef<SWidget> BrowseButton = PropertyCustomizationHelpers::MakeBrowseButton(
		FSimpleDelegate::CreateSP(this, &SPCGEditorGraphDebugObjectWidget::SelectedDebugObject_OnClicked),
		LOCTEXT("DebugSelectActor", "Select and frame the debug actor in the Level Editor."),
		TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &SPCGEditorGraphDebugObjectWidget::IsSelectDebugObjectButtonEnabled))
	);
	
	DebugObjectsComboBox = SNew(SComboBox<TSharedPtr<FPCGEditorGraphDebugObjectInstance>>)
		.OptionsSource(&DebugObjects)
		.InitiallySelectedItem(DebugObjects[0])
		.OnComboBoxOpening(this, &SPCGEditorGraphDebugObjectWidget::OnComboBoxOpening)
		.OnGenerateWidget(this, &SPCGEditorGraphDebugObjectWidget::OnGenerateWidget)
		.OnSelectionChanged(this, &SPCGEditorGraphDebugObjectWidget::OnSelectionChanged)
		.Content()
		[
			SNew(STextBlock)
			.Text(this, &SPCGEditorGraphDebugObjectWidget::GetSelectedDebugObjectText)
		];

	this->ChildSlot
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			DebugObjectsComboBox.ToSharedRef()
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.0f)
		[
			BrowseButton
		]
	];
}

void SPCGEditorGraphDebugObjectWidget::OnComboBoxOpening()
{
	DebugObjects.Empty();
	DebugObjectsComboBox->RefreshOptions();
	
	if (!PCGEditorPtr.IsValid())
	{
		return;
	}

	UPCGEditorGraph* PCGEditorGraph = PCGEditorPtr.Pin()->GetPCGEditorGraph();
	if (!PCGEditorGraph)
	{
		return;
	}

	const UPCGGraph* PCGGraph = PCGEditorGraph->GetPCGGraph();
	if (!PCGGraph)
	{
		return;
	}

	TSharedPtr<FPCGEditorGraphDebugObjectInstance> SelectedItem = DebugObjectsComboBox->GetSelectedItem();

	DebugObjects.Add(MakeShared<FPCGEditorGraphDebugObjectInstance>(PCGEditorGraphDebugObjectWidget::NoObjectString));

	if (!SelectedItem.IsValid() || SelectedItem->PCGComponent == nullptr)
	{
		DebugObjectsComboBox->SetSelectedItem(DebugObjects[0]);
	}
	
	TArray<UObject*> PCGComponents;
	GetObjectsOfClass(UPCGComponent::StaticClass(), PCGComponents, true);
	for (UObject* PCGComponentObject : PCGComponents)
	{
		if (!IsValid(PCGComponentObject))
		{
			continue;
		}
		
		UPCGComponent* PCGComponent = Cast<UPCGComponent>(PCGComponentObject);
		if (!PCGComponent)
		{
			continue;
		}
		
		const AActor* Actor = PCGComponent->GetOwner();
		if (!Actor)
		{
			continue;
		}

		const UPCGGraph* PCGComponentGraph = PCGComponent->GetGraph();
		if (!PCGComponentGraph)
		{
			continue;
		}

		if (PCGComponentGraph == PCGGraph)
		{
			FString ActorNameOrLabel = Actor->GetActorNameOrLabel();
			if (Actor->IsSelected())
			{
				ActorNameOrLabel.Append(PCGEditorGraphDebugObjectWidget::SelectionString);
			}
			
			FString ComponentName = PCGComponent->GetFName().ToString();
			if (PCGComponent->IsSelected())
			{
				ComponentName.Append(PCGEditorGraphDebugObjectWidget::SelectionString);
			}

			const FString Label = ActorNameOrLabel + PCGEditorGraphDebugObjectWidget::SeparatorString + ComponentName;
			TSharedPtr<FPCGEditorGraphDebugObjectInstance> DebugInstance = MakeShared<FPCGEditorGraphDebugObjectInstance>(PCGComponent, Label);
			DebugObjects.Add(DebugInstance);

			if (SelectedItem.IsValid() && SelectedItem->PCGComponent == PCGComponent)
			{
				DebugObjectsComboBox->SetSelectedItem(DebugInstance);
			}
		}
	}
}

void SPCGEditorGraphDebugObjectWidget::OnSelectionChanged(TSharedPtr<FPCGEditorGraphDebugObjectInstance> NewSelection, ESelectInfo::Type SelectInfo) const
{
	if (NewSelection.IsValid())
	{
		UPCGComponent* PCGComponent = NewSelection->PCGComponent.Get();
		PCGEditorPtr.Pin()->SetPCGComponentBeingDebugged(PCGComponent);
	}
}

TSharedRef<SWidget> SPCGEditorGraphDebugObjectWidget::OnGenerateWidget(TSharedPtr<FPCGEditorGraphDebugObjectInstance> InDebugObjectInstance) const
{
	const FString ItemString = InDebugObjectInstance->Label;

	return SNew(STextBlock)
		.Text(FText::FromString(*ItemString));
}

FText SPCGEditorGraphDebugObjectWidget::GetSelectedDebugObjectText() const
{
	if (const TSharedPtr<FPCGEditorGraphDebugObjectInstance> SelectedItem = DebugObjectsComboBox->GetSelectedItem())
	{
		return SelectedItem->GetDebugObjectText();
	}

	return FText::GetEmpty();
}

void SPCGEditorGraphDebugObjectWidget::SelectedDebugObject_OnClicked() const
{
	if (UPCGComponent* PCGComponent = PCGEditorPtr.Pin()->GetPCGComponentBeingDebugged())
	{
		if (AActor* Actor = PCGComponent->GetOwner())
		{
			GEditor->SelectNone(/*bNoteSelectionChange=*/false, /*bDeselectBSPSurfs=*/true, /*WarnAboutManyActors=*/false);
			GEditor->SelectActor(Actor, /*bInSelected=*/true, /*bNotify=*/true, /*bSelectEvenIfHidden=*/true);
			GUnrealEd->Exec(Actor->GetWorld(), TEXT("CAMERA ALIGN ACTIVEVIEWPORTONLY"));
			GEditor->SelectComponent(PCGComponent, /*bInSelected=*/true, /*bNotify=*/true, /*bSelectEvenIfHidden=*/true);
		}
	}
}

bool SPCGEditorGraphDebugObjectWidget::IsSelectDebugObjectButtonEnabled() const
{
	return PCGEditorPtr.IsValid() && (PCGEditorPtr.Pin()->GetPCGComponentBeingDebugged() != nullptr);
}

#undef LOCTEXT_NAMESPACE
