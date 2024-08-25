// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaOperatorStackTab.h"

#include "Animators/PropertyAnimatorCoreBase.h"
#include "Contexts/OperatorStackEditorContext.h"
#include "DetailView/IAvaDetailsProvider.h"
#include "EditorModeManager.h"
#include "Items/OperatorStackEditorItem.h"
#include "Items/OperatorStackEditorObjectItem.h"
#include "Modifiers/ActorModifierCoreStack.h"
#include "Selection.h"
#include "Selection/AvaEditorSelection.h"
#include "Subsystems/OperatorStackEditorSubsystem.h"
#include "Widgets/SOperatorStackEditorWidget.h"

#define LOCTEXT_NAMESPACE "SAvaModifierStackTab"

void SAvaOperatorStackTab::Construct(const FArguments& InArgs
	, const TSharedPtr<IAvaDetailsProvider>& InProvider)
{
	DetailsProviderWeak = InProvider;
	
	UOperatorStackEditorSubsystem* OperatorStackSubsystem = UOperatorStackEditorSubsystem::Get();

	check(InProvider.IsValid() && OperatorStackSubsystem);

	USelection::SelectionChangedEvent.AddSP(this, &SAvaOperatorStackTab::RefreshSelection);

	// Modifiers delegates
	UActorModifierCoreStack::OnModifierAddedDelegate.AddSP(this, &SAvaOperatorStackTab::OnModifierUpdated);
	UActorModifierCoreStack::OnModifierMovedDelegate.AddSP(this, &SAvaOperatorStackTab::OnModifierUpdated);
	UActorModifierCoreStack::OnModifierRemovedDelegate.AddSP(this, &SAvaOperatorStackTab::OnModifierUpdated);

	// Property controllers delegates
	UPropertyAnimatorCoreBase::OnAnimatorCreatedDelegate.AddSP(this, &SAvaOperatorStackTab::OnControllerUpdated);
	UPropertyAnimatorCoreBase::OnAnimatorRemovedDelegate.AddSP(this, &SAvaOperatorStackTab::OnControllerUpdated);
	UPropertyAnimatorCoreBase::OnAnimatorRenamedDelegate.AddSP(this, &SAvaOperatorStackTab::OnControllerUpdated);

	const TSharedPtr<IDetailKeyframeHandler> KeyframeHandler = InProvider->GetDetailsKeyframeHandler();
	
	OperatorStack = OperatorStackSubsystem->GenerateWidget();
	OperatorStack->SetKeyframeHandler(KeyframeHandler);
	OperatorStack->SetPanelTag(SAvaOperatorStackTab::PanelTag);
	
	ChildSlot
	[
		OperatorStack.ToSharedRef()
	];

	RefreshSelection(nullptr);
}

SAvaOperatorStackTab::~SAvaOperatorStackTab()
{
	USelection::SelectionChangedEvent.RemoveAll(this);
	
	UActorModifierCoreStack::OnModifierAddedDelegate.RemoveAll(this);
	UActorModifierCoreStack::OnModifierMovedDelegate.RemoveAll(this);
	UActorModifierCoreStack::OnModifierRemovedDelegate.RemoveAll(this);
	
	UPropertyAnimatorCoreBase::OnAnimatorCreatedDelegate.RemoveAll(this);
	UPropertyAnimatorCoreBase::OnAnimatorRemovedDelegate.RemoveAll(this);
	UPropertyAnimatorCoreBase::OnAnimatorRenamedDelegate.RemoveAll(this);
}

void SAvaOperatorStackTab::RefreshSelection(UObject* InSelectionObject) const
{
	const TSharedPtr<IAvaDetailsProvider> DetailsProvider = DetailsProviderWeak.Pin();
	if (!DetailsProvider.IsValid())
	{
		return;
	}
	
	FEditorModeTools* ModeTools = DetailsProvider->GetDetailsModeTools();
	if (!ModeTools)
	{
		return;
	}

	const FAvaEditorSelection EditorSelection(*ModeTools, InSelectionObject);
	if (!EditorSelection.IsValid())
	{
		return;
	}

	const TArray<UObject*> SelectedObjects = EditorSelection.GetSelectedObjects<UObject, EAvaSelectionSource::All>();

	TArray<FOperatorStackEditorItemPtr> SelectedItems;
	Algo::Transform(SelectedObjects, SelectedItems, [](UObject* InObject)
	{
		return MakeShared<FOperatorStackEditorObjectItem>(InObject);
	});

	const FOperatorStackEditorContext Context(SelectedItems);
	OperatorStack->SetContext(Context);
}

void SAvaOperatorStackTab::OnModifierUpdated(UActorModifierCoreBase* InUpdatedItem) const
{
	RefreshCurrentSelection(InUpdatedItem);
}

void SAvaOperatorStackTab::OnControllerUpdated(UPropertyAnimatorCoreBase* InController) const
{
	RefreshCurrentSelection(InController);
}

void SAvaOperatorStackTab::RefreshCurrentSelection(const UObject* InObject) const
{
	const TSharedPtr<IAvaDetailsProvider> DetailsProvider = DetailsProviderWeak.Pin();
	if (!DetailsProvider.IsValid())
	{
		return;
	}
	
	FEditorModeTools* ModeTools = DetailsProvider->GetDetailsModeTools();
	if (!InObject || !OperatorStack.IsValid() || !ModeTools)
	{
		return;
	}

	const AActor* OwningActor = InObject->GetTypedOuter<AActor>();
	if (!OwningActor)
	{
		return;
	}

	const FOperatorStackEditorContextPtr Context = OperatorStack->GetContext();
	if (!Context.IsValid())
	{
		return;
	}

	const FAvaEditorSelection EditorSelection(*ModeTools, ModeTools->GetSelectedActors());
	if (!EditorSelection.IsValid())
	{
		return;
	}

	const TArray<AActor*> SelectedActors = EditorSelection.GetSelectedObjects<AActor, EAvaSelectionSource::All>();
	if (!SelectedActors.Contains(OwningActor))
	{
		return;
	}

	OperatorStack->RefreshContext();
}

#undef LOCTEXT_NAMESPACE