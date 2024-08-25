// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaOperatorStackExtension.h"
#include "AvaEditorCommands.h"
#include "AvaOperatorStackTabSpawner.h"
#include "Components/PropertyAnimatorCoreComponent.h"
#include "EditorModeManager.h"
#include "Framework/Docking/LayoutExtender.h"
#include "LevelEditor.h"
#include "Modifiers/ActorModifierCoreBase.h"
#include "Modifiers/ActorModifierCoreComponent.h"
#include "Selection.h"
#include "Subsystems/OperatorStackEditorSubsystem.h"
#include "Subsystems/PropertyAnimatorCoreSubsystem.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "ToolMenuSection.h"
#include "Widgets/SAvaOperatorStackTab.h"
#include "Widgets/SOperatorStackEditorWidget.h"

#define LOCTEXT_NAMESPACE "AvaOperatorStackExtension"

FAvaOperatorStackExtension::FAvaOperatorStackExtension()
	: AnimatorCommands(MakeShared<FUICommandList>())
{
}

TSharedPtr<SDockTab> FAvaOperatorStackExtension::OpenTab()
{
	const TSharedPtr<IAvaEditor>& Editor = GetEditor();

	if (!Editor.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<FTabManager> TabManager = Editor->GetTabManager();

	if (!TabManager.IsValid())
	{
		return nullptr;
	}

	return TabManager->TryInvokeTab(FAvaOperatorStackTabSpawner::GetTabID());
}

void FAvaOperatorStackExtension::Activate()
{
	FAvaEditorExtension::Activate();

	USelection::SelectionChangedEvent.AddSP(this, &FAvaOperatorStackExtension::OnSelectionChanged);
}

void FAvaOperatorStackExtension::Deactivate()
{
	FAvaEditorExtension::Deactivate();

	USelection::SelectionChangedEvent.RemoveAll(this);
}

void FAvaOperatorStackExtension::RegisterTabSpawners(const TSharedRef<IAvaEditor>& InEditor) const
{
	InEditor->AddTabSpawner<FAvaOperatorStackTabSpawner>(InEditor);
}

void FAvaOperatorStackExtension::ExtendLevelEditorLayout(FLayoutExtender& InExtender) const
{
	InExtender.ExtendLayout(LevelEditorTabIds::LevelEditorSceneOutliner
		, ELayoutExtensionPosition::After
		, FTabManager::FTab(FAvaOperatorStackTabSpawner::GetTabID(), ETabState::ClosedTab));
}

void FAvaOperatorStackExtension::ExtendToolbarMenu(UToolMenu& InMenu)
{
	FToolMenuSection& Section = InMenu.FindOrAddSection(DefaultSectionName);

	FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(
		TEXT("OpenOperatorStackButton"),
		FExecuteAction::CreateSPLambda(this, [this]() { OpenTab(); }),
		LOCTEXT("OpenOperatorStackLabel", "Operator Stack"),
		LOCTEXT("OpenOperatorStackTooltip", "Open the operator stack tab."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.UserDefinedStruct")
	));

	Entry.StyleNameOverride = "CalloutToolbar";
}

void FAvaOperatorStackExtension::BindCommands(const TSharedRef<FUICommandList>& InCommandList)
{
	InCommandList->Append(AnimatorCommands);

	const FAvaEditorCommands& EditorCommands = FAvaEditorCommands::Get();

	AnimatorCommands->MapAction(EditorCommands.DisableAnimators
		, FExecuteAction::CreateSP(this, &FAvaOperatorStackExtension::EnableAnimators, false));

	AnimatorCommands->MapAction(EditorCommands.EnableAnimators
		, FExecuteAction::CreateSP(this, &FAvaOperatorStackExtension::EnableAnimators, true));
}

void FAvaOperatorStackExtension::OnSelectionChanged(UObject* InSelection)
{
	const USelection* Selection = Cast<USelection>(InSelection);

	if (!Selection || Selection->Num() == 0)
	{
		return;
	}

	const UObject* LastSelectedObject = Selection->GetSelectedObject(Selection->Num() - 1);

	if (!LastSelectedObject)
	{
		return;
	}

	const UOperatorStackEditorSubsystem* OperatorStackEditorSubsystem = UOperatorStackEditorSubsystem::Get();

	if (!OperatorStackEditorSubsystem)
	{
		return;
	}

	FName ActiveCustomization;
	if (LastSelectedObject->IsA<UActorModifierCoreBase>()
		|| LastSelectedObject->IsA<UActorModifierCoreComponent>())
	{
		ActiveCustomization = TEXT("Modifiers");
	}
	else if (LastSelectedObject->IsA<UPropertyAnimatorCoreBase>()
		|| LastSelectedObject->IsA<UPropertyAnimatorCoreComponent>())
	{
		ActiveCustomization = TEXT("Animators");
	}

	if (ActiveCustomization.IsNone())
	{
		return;
	}

	// Lets invoke the operator stack tab
	const TSharedPtr<SDockTab> InvokedTab = OpenTab();

	if (!InvokedTab.IsValid())
	{
		return;
	}

	// Find the avalanche operator stack and set the active customization
	OperatorStackEditorSubsystem->ForEachCustomizationWidget([ActiveCustomization](TSharedRef<SOperatorStackEditorWidget> InWidget)->bool
	{
		if (InWidget->GetPanelTag() == SAvaOperatorStackTab::PanelTag)
		{
			InWidget->SetActiveCustomization(ActiveCustomization);

			// Stop looping for customization widgets
			return false;
		}

		return true;
	});
}

void FAvaOperatorStackExtension::EnableAnimators(bool bInEnable) const
{
	const UWorld* const World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const FEditorModeTools* ModeTools = GetEditorModeTools();
	if (!ModeTools)
	{
		return;
	}

	const UTypedElementSelectionSet* SelectionSet = ModeTools->GetEditorSelectionSet();
	if (!SelectionSet)
	{
		return;
	}

	const TSet<AActor*> SelectedActors(SelectionSet->GetSelectedObjects<AActor>());

	UPropertyAnimatorCoreSubsystem* AnimatorSubsystem = UPropertyAnimatorCoreSubsystem::Get();

	if (!AnimatorSubsystem)
	{
		return;
	}

	// If nothing selected, target all animators in level
	if (!SelectedActors.IsEmpty())
	{
		AnimatorSubsystem->SetActorAnimatorsEnabled(SelectedActors, bInEnable, true);
	}
	else
	{
		AnimatorSubsystem->SetLevelAnimatorsEnabled(World, bInEnable, true);
	}
}

#undef LOCTEXT_NAMESPACE
