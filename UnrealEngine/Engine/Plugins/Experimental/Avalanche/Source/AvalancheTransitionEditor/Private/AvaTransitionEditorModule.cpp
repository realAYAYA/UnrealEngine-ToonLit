// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTransitionEditorModule.h"
#include "AvaTransitionCommands.h"
#include "AvaTransitionEditorEnums.h"
#include "AvaTransitionEditorLog.h"
#include "AvaTransitionEditorStyle.h"
#include "AvaTransitionEditorUtils.h"
#include "AvaTransitionTree.h"
#include "AvaTransitionTreeEditorData.h"
#include "AvaTransitionTreeSchema.h"
#include "Behavior/IAvaTransitionBehavior.h"
#include "IAvaTransitionModule.h"
#include "StateTreeDelegates.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "ViewModels/AvaTransitionEditorViewModel.h"
#include "Widgets/Layout/SBox.h"

#if WITH_STATETREE_DEBUGGER
#include "Debugger/AvaTransitionTraceModule.h"
#endif

#define LOCTEXT_NAMESPACE "AvaTransitionEditorModule"

DEFINE_LOG_CATEGORY(LogAvaEditorTransition)

IMPLEMENT_MODULE(FAvaTransitionEditorModule, AvalancheTransitionEditor)

void FAvaTransitionEditorModule::StartupModule()
{
#if WITH_STATETREE_DEBUGGER
	FAvaTransitionTraceModule::Startup();
#endif

	FAvaTransitionEditorCommands::Register();

	IAvaTransitionModule::Get().GetOnValidateTransitionTree().BindRaw(this, &FAvaTransitionEditorModule::ValidateStateTree);

	OnPostCompileHandle = UE::StateTree::Delegates::OnPostCompile.AddRaw(this, &FAvaTransitionEditorModule::OnPostCompile);
}

void FAvaTransitionEditorModule::ShutdownModule()
{
	FAvaTransitionEditorCommands::Unregister();

#if WITH_STATETREE_DEBUGGER
	FAvaTransitionTraceModule::Shutdown();
#endif

	IAvaTransitionModule::Get().GetOnValidateTransitionTree().Unbind();

	UE::StateTree::Delegates::OnPostCompile.Remove(OnPostCompileHandle);
	OnPostCompileHandle.Reset();
}

IAvaTransitionEditorModule::FOnBuildDefaultTransitionTree& FAvaTransitionEditorModule::GetOnBuildDefaultTransitionTree()
{
	return OnBuildDefaultTransitionTree;
}

IAvaTransitionEditorModule::FOnCompileTransitionTree& FAvaTransitionEditorModule::GetOnCompileTransitionTree()
{
	return OnCompileTransitionTree;
}

void FAvaTransitionEditorModule::GenerateTransitionTreeOptionsMenu(UToolMenu* InMenu, IAvaTransitionBehavior* InTransitionBehavior)
{
	if (!InMenu || !InTransitionBehavior)
	{
		return;
	}

	UAvaTransitionTree* TransitionTree = InTransitionBehavior->GetTransitionTree();
	if (!TransitionTree)
	{
		return;
	}

	TWeakObjectPtr<UAvaTransitionTree> TransitionTreeWeak = TransitionTree;

	FToolMenuSection& GeneralSection = InMenu->FindOrAddSection(TEXT("GeneralSection"), LOCTEXT("GeneralSectionLabel", "General"));

	GeneralSection.AddMenuEntry(TEXT("TransitionTreeEnabled")
		, LOCTEXT("TransitionTreeEnabledLabel", "Enabled")
		, LOCTEXT("TransitionTreeEnabledTooltip", "Determines whether Transition Tree is Enabled")
		, TAttribute<FSlateIcon>()
		, FUIAction(FExecuteAction::CreateStatic(&UE::AvaTransitionEditor::ToggleTransitionTreeEnabled, TransitionTreeWeak)
			, FCanExecuteAction()
			, FIsActionChecked::CreateStatic(&UE::AvaTransitionEditor::IsTransitionTreeEnabled, TransitionTreeWeak))
		, EUserInterfaceActionType::ToggleButton);

	if (TSharedPtr<SWidget> LayerPicker = UE::AvaTransitionEditor::CreateTransitionLayerPicker(Cast<UAvaTransitionTreeEditorData>(TransitionTree->EditorData), /*bInCompileOnLayerPicked*/true))
	{
		FToolMenuSection& LayerSection = InMenu->FindOrAddSection(TEXT("LayerSection"), LOCTEXT("LayerSectionLabel", "Layer"));

		LayerSection.AddEntry(FToolMenuEntry::InitWidget(TEXT("TransitionLayerPicker")
			, SNew(SBox)
				.HeightOverride(40.f)
				[
					LayerPicker.ToSharedRef()
				]
			, FText::GetEmpty()
			, /*bNoIndent*/true
			, /*bSearchable*/false
			, /*bNoPadding*/true));
	}
}

void FAvaTransitionEditorModule::ValidateStateTree(UAvaTransitionTree* InTransitionTree)
{
	// Return early if Editor Data is already valid
	if (!InTransitionTree || InTransitionTree->EditorData)
	{
		return;
	}

	// Disable Tree by default if being set up for the first time
	InTransitionTree->SetEnabled(false);

	UAvaTransitionTreeEditorData* const EditorData = NewObject<UAvaTransitionTreeEditorData>(InTransitionTree, NAME_None, RF_Transactional);
	check(EditorData);

	EditorData->Schema = NewObject<UAvaTransitionTreeSchema>(EditorData);

	InTransitionTree->EditorData = EditorData;

	// Build the Default Tree
	if (OnBuildDefaultTransitionTree.IsBound())
	{
		OnBuildDefaultTransitionTree.Execute(*EditorData);
	}
	else
	{
		EditorData->AddRootState();	
	}

	// Compile in Advanced Mode here so that no new nodes are generated from outside
	FAvaTransitionCompiler Compiler;
	Compiler.SetTransitionTree(InTransitionTree);
	Compiler.Compile(EAvaTransitionEditorMode::Advanced);
}

void FAvaTransitionEditorModule::OnPostCompile(const UStateTree& InStateTree)
{
	// TODO: Currently State Tree Compiler does not have any hooks to compile additional data,
	// so this is where we inject the additional properties
	if (const UAvaTransitionTree* TransitionTree = Cast<UAvaTransitionTree>(&InStateTree))
	{
		UAvaTransitionTree* TransitionTreeMutable = const_cast<UAvaTransitionTree*>(TransitionTree);

		const UAvaTransitionTreeEditorData* EditorData = CastChecked<UAvaTransitionTreeEditorData>(InStateTree.EditorData);

		TransitionTreeMutable->SetTransitionLayer(EditorData->GetTransitionLayer());
	}
}

#undef LOCTEXT_NAMESPACE
