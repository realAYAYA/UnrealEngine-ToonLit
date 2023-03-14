// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTreeEditor.h"
#include "Customizations/StateTreeBindingExtension.h"
#include "IDetailsView.h"
#include "IMessageLogListing.h"
#include "MessageLogInitializationOptions.h"
#include "MessageLogModule.h"
#include "Misc/UObjectToken.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SStateTreeView.h"
#include "StateTree.h"
#include "StateTreeCompiler.h"
#include "StateTreeDelegates.h"
#include "StateTreeEditorCommands.h"
#include "StateTreeEditorData.h"
#include "StateTreeEditorModule.h"
#include "StateTreeObjectHash.h"
#include "StateTreeState.h"
#include "StateTreeTaskBase.h"
#include "StateTreeToolMenuContext.h"
#include "StateTreeTypes.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

#define LOCTEXT_NAMESPACE "StateTreeEditor"

const FName StateTreeEditorAppName(TEXT("StateTreeEditorApp"));

const FName FStateTreeEditor::SelectionDetailsTabId(TEXT("StateTreeEditor_SelectionDetails"));
const FName FStateTreeEditor::AssetDetailsTabId(TEXT("StateTreeEditor_AssetDetails"));
const FName FStateTreeEditor::StateTreeViewTabId(TEXT("StateTreeEditor_StateTreeView"));
const FName FStateTreeEditor::StateTreeStatisticsTabId(TEXT("StateTreeEditor_StateTreeStatistics"));
const FName FStateTreeEditor::CompilerResultsTabId(TEXT("StateTreeEditor_CompilerResults"));

void FStateTreeEditor::PostUndo(bool bSuccess)
{
}

void FStateTreeEditor::PostRedo(bool bSuccess)
{
}

void FStateTreeEditor::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (StateTree != nullptr)
	{
		Collector.AddReferencedObject(StateTree);
	}
}

void FStateTreeEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_StateTreeEditor", "StateTree Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(SelectionDetailsTabId, FOnSpawnTab::CreateSP(this, &FStateTreeEditor::SpawnTab_SelectionDetails) )
		.SetDisplayName( NSLOCTEXT("StateTreeEditor", "SelectionDetailsTab", "Details" ) )
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(AssetDetailsTabId, FOnSpawnTab::CreateSP(this, &FStateTreeEditor::SpawnTab_AssetDetails))
		.SetDisplayName(NSLOCTEXT("StateTreeEditor", "AssetDetailsTab", "Asset Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(StateTreeViewTabId, FOnSpawnTab::CreateSP(this, &FStateTreeEditor::SpawnTab_StateTreeView))
		.SetDisplayName(NSLOCTEXT("StateTreeEditor", "StateTreeViewTab", "StateTree"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
	InTabManager->RegisterTabSpawner(StateTreeStatisticsTabId, FOnSpawnTab::CreateSP(this, &FStateTreeEditor::SpawnTab_StateTreeStatistics))
		.SetDisplayName(NSLOCTEXT("StateTreeEditor", "StatisticsTab", "StateTree Statistics"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
	InTabManager->RegisterTabSpawner(CompilerResultsTabId, FOnSpawnTab::CreateSP(this, &FStateTreeEditor::SpawnTab_CompilerResults))
		.SetDisplayName(NSLOCTEXT("StateTreeEditor", "CompilerResultsTab", "Compiler Results"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
}


void FStateTreeEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(SelectionDetailsTabId);
	InTabManager->UnregisterTabSpawner(AssetDetailsTabId);
	InTabManager->UnregisterTabSpawner(StateTreeViewTabId);
	InTabManager->UnregisterTabSpawner(StateTreeStatisticsTabId);
	InTabManager->UnregisterTabSpawner(CompilerResultsTabId);
}

void FStateTreeEditor::InitEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UStateTree* InStateTree)
{
	StateTree = InStateTree;
	check(StateTree != NULL);

	UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(StateTree->EditorData);
	if (EditorData == NULL)
	{
		EditorData = NewObject<UStateTreeEditorData>(StateTree, FName(), RF_Transactional);
		EditorData->AddRootState();
		StateTree->EditorData = EditorData;
		Compile();
	}

	EditorDataHash = UE::StateTree::Editor::CalcAssetHash(*StateTree);
	
	// @todo: Temporary fix
	// Make sure all states are transactional
	for (UStateTreeState* SubTree : EditorData->SubTrees)
	{
		TArray<UStateTreeState*> Stack;

		Stack.Add(SubTree);
		while (!Stack.IsEmpty())
		{
			if (UStateTreeState* State = Stack.Pop())
			{
				State->SetFlags(RF_Transactional);
				
				for (UStateTreeState* ChildState : State->Children)
				{
					Stack.Add(ChildState);
				}
			}
		}
	}


	StateTreeViewModel = MakeShareable(new FStateTreeViewModel());
	StateTreeViewModel->Init(EditorData);

	StateTreeViewModel->GetOnAssetChanged().AddSP(this, &FStateTreeEditor::HandleModelAssetChanged);
	StateTreeViewModel->GetOnSelectionChanged().AddSP(this, &FStateTreeEditor::HandleModelSelectionChanged);

	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	FMessageLogInitializationOptions LogOptions;
	// Show Pages so that user is never allowed to clear log messages
	LogOptions.bShowPages = false;
	LogOptions.bShowFilters = false;
	LogOptions.bAllowClear = false;
	LogOptions.MaxPageCount = 1;
	CompilerResultsListing = MessageLogModule.CreateLogListing("StateTreeCompiler", LogOptions);
	CompilerResults = MessageLogModule.CreateLogListingWidget(CompilerResultsListing.ToSharedRef());

	CompilerResultsListing->OnMessageTokenClicked().AddSP(this, &FStateTreeEditor::HandleMessageTokenClicked);

	
	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_StateTree_Layout_v3")
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewSplitter() ->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.2f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.65f)
					->AddTab(AssetDetailsTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.35f)
					->AddTab(StateTreeStatisticsTabId, ETabState::OpenedTab)
				)
			)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.5f)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.75f)
					->AddTab(StateTreeViewTabId, ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.25f)
					->AddTab(CompilerResultsTabId, ETabState::ClosedTab)
				)
			)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.3f)
				->AddTab(SelectionDetailsTabId, ETabState::OpenedTab)
				->SetForegroundTab(SelectionDetailsTabId)
			)
		)
	);


	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, StateTreeEditorAppName, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, StateTree);

	BindCommands();
	RegisterToolbar();
	
	FStateTreeEditorModule& StateTreeEditorModule = FModuleManager::LoadModuleChecked<FStateTreeEditorModule>("StateTreeEditorModule");
	AddMenuExtender(StateTreeEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	RegenerateMenusAndToolbars();

	UE::StateTree::Delegates::OnIdentifierChanged.AddSP(this, &FStateTreeEditor::OnIdentifierChanged);
	UE::StateTree::Delegates::OnSchemaChanged.AddSP(this, &FStateTreeEditor::OnSchemaChanged);
	UE::StateTree::Delegates::OnParametersChanged.AddSP(this, &FStateTreeEditor::OnParametersChanged);
	UE::StateTree::Delegates::OnStateParametersChanged.AddSP(this, &FStateTreeEditor::OnStateParametersChanged);
}

FName FStateTreeEditor::GetToolkitFName() const
{
	return FName("StateTreeEditor");
}

FText FStateTreeEditor::GetBaseToolkitName() const
{
	return NSLOCTEXT("StateTreeEditor", "AppLabel", "State Tree");
}

FString FStateTreeEditor::GetWorldCentricTabPrefix() const
{
	return NSLOCTEXT("StateTreeEditor", "WorldCentricTabPrefix", "State Tree").ToString();
}

FLinearColor FStateTreeEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.0f, 0.0f, 0.2f, 0.5f );
}

void FStateTreeEditor::InitToolMenuContext(FToolMenuContext& MenuContext)
{
	UStateTreeToolMenuContext* Context = NewObject<UStateTreeToolMenuContext>();
	Context->StateTreeEditor = SharedThis(this);
	MenuContext.AddObject(Context);
}

void FStateTreeEditor::HandleMessageTokenClicked(const TSharedRef<IMessageToken>& InMessageToken)
{
	if (InMessageToken->GetType() == EMessageToken::Object)
	{
		const TSharedRef<FUObjectToken> ObjectToken = StaticCastSharedRef<FUObjectToken>(InMessageToken);
		UStateTreeState* State = Cast<UStateTreeState>(ObjectToken->GetObject().Get());
		if (State)
		{
			StateTreeViewModel->SetSelection(State);
		}
	}
}

TSharedRef<SDockTab> FStateTreeEditor::SpawnTab_StateTreeView(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == StateTreeViewTabId);

	return SNew(SDockTab)
		.Label(NSLOCTEXT("StateTreeEditor", "StateTreeViewTab", "StateTree"))
		.TabColorScale(GetTabColorScale())
		[
			SAssignNew(StateTreeView, SStateTreeView, StateTreeViewModel.ToSharedRef())
		];
}


TSharedRef<SDockTab> FStateTreeEditor::SpawnTab_SelectionDetails(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == SelectionDetailsTabId);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	SelectionDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	SelectionDetailsView->SetObject(nullptr);
	SelectionDetailsView->OnFinishedChangingProperties().AddSP(this, &FStateTreeEditor::OnSelectionFinishedChangingProperties);

	SelectionDetailsView->SetExtensionHandler(MakeShared<FStateTreeBindingExtension>());

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(NSLOCTEXT("StateTreeEditor", "SelectionDetailsTab", "Details"))
		[
			SelectionDetailsView.ToSharedRef()
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FStateTreeEditor::SpawnTab_AssetDetails(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == AssetDetailsTabId);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	AssetDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	AssetDetailsView->SetObject(StateTree ? StateTree->EditorData : nullptr);
	AssetDetailsView->OnFinishedChangingProperties().AddSP(this, &FStateTreeEditor::OnAssetFinishedChangingProperties);

	AssetDetailsView->SetExtensionHandler(MakeShared<FStateTreeBindingExtension>());

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(NSLOCTEXT("StateTreeEditor", "AssetDetailsTabLabel", "StateTree"))
		[
			AssetDetailsView.ToSharedRef()
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FStateTreeEditor::SpawnTab_StateTreeStatistics(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == StateTreeStatisticsTabId);
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("StatisticsTitle", "StateTree Statistics"))
		[
			SNew(SMultiLineEditableTextBox)
			.Padding(10.0f)
			.Style(FAppStyle::Get(), "Log.TextBox")
			.Font(FCoreStyle::GetDefaultFontStyle("Mono", 9))
			.ForegroundColor(FLinearColor::Gray)
			.IsReadOnly(true)
			.Text(this, &FStateTreeEditor::GetStatisticsText)
		];
	return SpawnedTab;
}

TSharedRef<SDockTab> FStateTreeEditor::SpawnTab_CompilerResults(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == CompilerResultsTabId);
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("CompilerResultsTitle", "Compiler Results"))
		[
			SNew(SBox)
			[
				CompilerResults.ToSharedRef()
			]
		];
	return SpawnedTab;
}

FText FStateTreeEditor::GetStatisticsText() const
{
	if (!StateTree)
	{
		return FText::GetEmpty();
	}


	TArray<FStateTreeMemoryUsage> MemoryUsages = StateTree->CalculateEstimatedMemoryUsage();
	if (MemoryUsages.IsEmpty())
	{
		return FText::GetEmpty();
	}

	TArray<FText> Rows;

	for (const FStateTreeMemoryUsage& Usage : MemoryUsages)
	{
		const FText SizeText = FText::AsMemory(Usage.EstimatedMemoryUsage);
		const FText NumNodesText = FText::AsNumber(Usage.NodeCount);
		Rows.Add(FText::Format(LOCTEXT("UsageRow", "{0}: {1}, {2} nodes"), FText::FromString(Usage.Name), SizeText, NumNodesText));
	}

	return FText::Join(FText::FromString(TEXT("\n")), Rows);
}

void FStateTreeEditor::HandleModelAssetChanged()
{
	UpdateAsset();
}

void FStateTreeEditor::HandleModelSelectionChanged(const TArray<TWeakObjectPtr<UStateTreeState>>& SelectedStates)
{
	if (SelectionDetailsView)
	{
		TArray<UObject*> Selected;
		for (const TWeakObjectPtr<UStateTreeState>& WeakState : SelectedStates)
		{
			if (UStateTreeState* State = WeakState.Get())
			{
				Selected.Add(State);
			}
		}
		SelectionDetailsView->SetObjects(Selected);
	}
}

void FStateTreeEditor::SaveAsset_Execute()
{
	// Remember the treeview expansion state
	if (StateTreeView)
	{
		StateTreeView->SavePersistentExpandedStates();
	}

	UpdateAsset();

	// save it
	FAssetEditorToolkit::SaveAsset_Execute();
}

void FStateTreeEditor::OnIdentifierChanged(const UStateTree& InStateTree)
{
	if (StateTree == &InStateTree)
	{
		UpdateAsset();
	}
}

void FStateTreeEditor::OnSchemaChanged(const UStateTree& InStateTree)
{
	if (StateTree == &InStateTree)
	{
		UpdateAsset();
		
		if (StateTreeViewModel)
		{
			StateTreeViewModel->NotifyAssetChangedExternally();
		}

		if (SelectionDetailsView.IsValid())
		{
			SelectionDetailsView->ForceRefresh();
		}
	}
}

void FStateTreeEditor::OnParametersChanged(const UStateTree& InStateTree)
{
	if (StateTree == &InStateTree)
	{
		// Accessible structs might be different after modifying parameters so forcing refresh
		// so the FStateTreeBindingExtension can rebuild the list of bindable structs
		if (SelectionDetailsView.IsValid())
		{
			SelectionDetailsView->ForceRefresh();
		}
	}
}

void FStateTreeEditor::OnStateParametersChanged(const UStateTree& InStateTree, const FGuid ChangedStateID)
{
	if (StateTree == &InStateTree)
	{
		if (const UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(StateTree->EditorData))
		{
			TreeData->VisitHierarchy([&ChangedStateID](UStateTreeState& State, UStateTreeState* /*ParentState*/)
			{
				if (State.Type == EStateTreeStateType::Linked && State.LinkedSubtree.ID == ChangedStateID)
				{
					State.UpdateParametersFromLinkedSubtree();
				}
				return EStateTreeVisitor::Continue;
			});
		}

		// Accessible structs might be different after modifying parameters so forcing refresh
		// so the FStateTreeBindingExtension can rebuild the list of bindable structs
		if (SelectionDetailsView.IsValid())
		{
			SelectionDetailsView->ForceRefresh();
		}
	}
}

void FStateTreeEditor::OnAssetFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	// Make sure nodes get updates when properties are changed.
	if (StateTreeViewModel)
	{
		StateTreeViewModel->NotifyAssetChangedExternally();
	}
}

void FStateTreeEditor::OnSelectionFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	// Make sure nodes get updates when properties are changed.
	if (StateTreeViewModel)
	{
		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = SelectionDetailsView->GetSelectedObjects();
		TSet<UStateTreeState*> ChangedStates;
		for (const TWeakObjectPtr<UObject>& WeakObject : SelectedObjects)
		{
			if (UObject* Object = WeakObject.Get())
			{
				if (UStateTreeState* State = Cast<UStateTreeState>(Object))
				{
					ChangedStates.Add(State);
				}
			}
		}
		if (ChangedStates.Num() > 0)
		{
			StateTreeViewModel->NotifyStatesChangedExternally(ChangedStates, PropertyChangedEvent);
			UpdateAsset();
		}
	}
}

namespace UE::StateTree::Editor::Internal
{
	bool FixChangedStateLinkName(FStateTreeStateLink& StateLink, const TMap<FGuid, FName>& IDToName)
	{
		if (StateLink.ID.IsValid())
		{
			const FName* Name = IDToName.Find(StateLink.ID);
			if (Name == nullptr)
			{
				// Missing link, we'll show these in the UI
				return false;
			}
			if (StateLink.Name != *Name)
			{
				// Name changed, fix!
				StateLink.Name = *Name;
				return true;
			}
		}
		return false;
	}

	void ValidateLinkedStates(UStateTree& StateTree)
	{
		UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(StateTree.EditorData);
		if (!TreeData)
		{
			return;
		}

		// Make sure all state links are valid and update the names if needed.

		// Create ID to state name map.
		TMap<FGuid, FName> IDToName;

		TreeData->VisitHierarchy([&IDToName](const UStateTreeState& State, UStateTreeState* /*ParentState*/)
		{
			IDToName.Add(State.ID, State.Name);
			return EStateTreeVisitor::Continue;
		});
		
		// Fix changed names.
		TreeData->VisitHierarchy([&IDToName](UStateTreeState& State, UStateTreeState* /*ParentState*/)
		{
			if (State.Type == EStateTreeStateType::Linked)
			{
				FixChangedStateLinkName(State.LinkedSubtree, IDToName);
			}
					
			for (FStateTreeTransition& Transition : State.Transitions)
			{
				FixChangedStateLinkName(Transition.State, IDToName);
			}

			return EStateTreeVisitor::Continue;
		});
	}

	void UpdateParents(UStateTree& StateTree)
	{
		UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(StateTree.EditorData);
		if (!TreeData)
		{
			return;
		}

		TreeData->VisitHierarchy([](UStateTreeState& State, UStateTreeState* ParentState)
		{
			State.Parent = ParentState;
			return EStateTreeVisitor::Continue;
		});
	}

	void ApplySchema(UStateTree& StateTree)
	{
		UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(StateTree.EditorData);
		if (!TreeData)
		{
			return;
		}
		
		const UStateTreeSchema* Schema = TreeData->Schema;
		if (!Schema)
		{
			return;
		}

		// Clear evaluators if not allowed.
		if (Schema->AllowEvaluators() == false && TreeData->Evaluators.Num() > 0)
		{
			UE_LOG(LogStateTreeEditor, Warning, TEXT("%s: Resetting Evaluators due to current schema restrictions."), *GetNameSafe(&StateTree));
			TreeData->Evaluators.Reset();
		}


		TreeData->VisitHierarchy([&StateTree, Schema](UStateTreeState& State, UStateTreeState* /*ParentState*/)
		{
			// Clear enter conditions if not allowed.
			if (Schema->AllowEnterConditions() == false && State.EnterConditions.Num() > 0)
			{
				UE_LOG(LogStateTreeEditor, Warning, TEXT("%s: Resetting Enter Conditions in state %s due to current schema restrictions."), *GetNameSafe(&StateTree), *GetNameSafe(&State));
				State.EnterConditions.Reset();
			}

			// Keep single and many tasks based on what is allowed.
			if (Schema->AllowMultipleTasks() == false)
			{
				if (State.Tasks.Num() > 0)
				{
					State.Tasks.Reset();
					UE_LOG(LogStateTreeEditor, Warning, TEXT("%s: Resetting Tasks in state %s due to current schema restrictions."), *GetNameSafe(&StateTree), *GetNameSafe(&State));
				}
				
				// Task name is the same as state name.
				if (FStateTreeTaskBase* Task = State.SingleTask.Node.GetMutablePtr<FStateTreeTaskBase>())
				{
					Task->Name = State.Name;
				}
			}
			else
			{
				if (State.SingleTask.Node.IsValid())
				{
					State.SingleTask.Reset();
					UE_LOG(LogStateTreeEditor, Warning, TEXT("%s: Resetting Single Task in state %s due to current schema restrictions."), *GetNameSafe(&StateTree), *GetNameSafe(&State));
				}
			}
			
			return EStateTreeVisitor::Continue;
		});
	}

	void RemoveUnusedBindings(UStateTree& StateTree)
	{
		UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(StateTree.EditorData);
		if (!TreeData)
		{
			return;
		}

		TMap<FGuid, const UStruct*> AllStructIDs;
		TreeData->GetAllStructIDs(AllStructIDs);
		TreeData->GetPropertyEditorBindings()->RemoveUnusedBindings(AllStructIDs);
	}

	void UpdateLinkedStateParameters(UStateTree& StateTree)
	{
		UStateTreeEditorData* TreeData = Cast<UStateTreeEditorData>(StateTree.EditorData);
		if (!TreeData)
		{
			return;
		}

		TreeData->VisitHierarchy([](UStateTreeState& State, UStateTreeState* /*ParentState*/)
		{
			if (State.Type == EStateTreeStateType::Linked)
			{
				State.UpdateParametersFromLinkedSubtree();
			}
			return EStateTreeVisitor::Continue;
		});
	}

}

namespace UE::StateTree::Editor
{
	void ValidateAsset(UStateTree& StateTree)
	{
		UE::StateTree::Editor::Internal::UpdateParents(StateTree);
		UE::StateTree::Editor::Internal::ApplySchema(StateTree);
		UE::StateTree::Editor::Internal::RemoveUnusedBindings(StateTree);
		UE::StateTree::Editor::Internal::ValidateLinkedStates(StateTree);
		UE::StateTree::Editor::Internal::UpdateLinkedStateParameters(StateTree);
	}

	uint32 CalcAssetHash(const UStateTree& StateTree)
	{
		uint32 EditorDataHash = 0;
		if (StateTree.EditorData != nullptr)
		{
			FStateTreeObjectCRC32 Archive;
			EditorDataHash = Archive.Crc32(StateTree.EditorData, 0);
		}

		return EditorDataHash;
	}

};

void FStateTreeEditor::BindCommands()
{
	const FStateTreeEditorCommands& Commands = FStateTreeEditorCommands::Get();

	ToolkitCommands->MapAction(
		Commands.Compile,
		FExecuteAction::CreateSP(this, &FStateTreeEditor::Compile),
		FCanExecuteAction::CreateSP(this, &FStateTreeEditor::CanCompile));
}

void FStateTreeEditor::RegisterToolbar()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	UToolMenu* ToolBar;
	FName ParentName;
	const FName MenuName = GetToolMenuToolbarName(ParentName);
	if (ToolMenus->IsMenuRegistered(MenuName))
	{
		ToolBar = ToolMenus->ExtendMenu(MenuName);
	}
	else
	{
		ToolBar = UToolMenus::Get()->RegisterMenu(MenuName, ParentName, EMultiBoxType::ToolBar);
	}

	const FToolMenuInsert InsertAfterAssetSection("Asset", EToolMenuInsertType::After);

	FToolMenuSection& Section = ToolBar->AddSection("Compile", TAttribute<FText>(), InsertAfterAssetSection);

	Section.AddDynamicEntry("CompileCommands", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
	{
		const UStateTreeToolMenuContext* Context = InSection.FindContext<UStateTreeToolMenuContext>();
		if (Context && Context->StateTreeEditor.IsValid())
		{
			TSharedPtr<FStateTreeEditor> StateTreeEditor = Context->StateTreeEditor.Pin();
			if (StateTreeEditor.IsValid())
			{
				const FStateTreeEditorCommands& Commands = FStateTreeEditorCommands::Get();

				FToolMenuEntry CompileButton = FToolMenuEntry::InitToolBarButton(
					Commands.Compile,
					TAttribute<FText>(),
					TAttribute<FText>(),
					TAttribute<FSlateIcon>(StateTreeEditor.ToSharedRef(), &FStateTreeEditor::GetCompileStatusImage)
					);

				InSection.AddEntry(CompileButton);
			}
		}
	}));
}

void FStateTreeEditor::Compile()
{
	if (!StateTree)
	{
		return;
	}

	// Note: If the compilation process changes, also update UStateTreeCompileAllCommandlet and UStateTreeFactory::FactoryCreateNew.
	
	UpdateAsset();
	
	if (CompilerResultsListing.IsValid())
	{
		CompilerResultsListing->ClearMessages();
	}

	FStateTreeCompilerLog Log;
	FStateTreeCompiler Compiler(Log);

	bLastCompileSucceeded = Compiler.Compile(*StateTree);

	if (CompilerResultsListing.IsValid())
	{
		Log.AppendToLog(CompilerResultsListing.Get());
	}

	if (bLastCompileSucceeded)
	{
		// Success
		StateTree->LastCompiledEditorDataHash = EditorDataHash;

		UE::StateTree::Delegates::OnPostCompile.Broadcast(*StateTree);
	}
	else
	{
		// Make sure not to leave stale data on failed compile.
		StateTree->ResetCompiled();
		StateTree->LastCompiledEditorDataHash = 0;

		// Show log
		TabManager->TryInvokeTab(CompilerResultsTabId);
	}
}

bool FStateTreeEditor::CanCompile() const
{
	if (StateTree == nullptr)
	{
		return false;
	}
	
	// We can't recompile while in PIE
	if (GEditor->IsPlaySessionInProgress())
	{
		return false;
	}

	return true;
}

FSlateIcon FStateTreeEditor::GetCompileStatusImage() const
{
	static const FName CompileStatusBackground("Blueprint.CompileStatus.Background");
	static const FName CompileStatusUnknown("Blueprint.CompileStatus.Overlay.Unknown");
	static const FName CompileStatusError("Blueprint.CompileStatus.Overlay.Error");
	static const FName CompileStatusGood("Blueprint.CompileStatus.Overlay.Good");
	static const FName CompileStatusWarning("Blueprint.CompileStatus.Overlay.Warning");

	if (StateTree == nullptr)
	{
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusUnknown);
	}
	
	const bool bCompiledDataResetDuringLoad = StateTree->LastCompiledEditorDataHash == EditorDataHash && !StateTree->IsReadyToRun();

	if (!bLastCompileSucceeded || bCompiledDataResetDuringLoad)
	{
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusError);
	}
	
	if (StateTree->LastCompiledEditorDataHash != EditorDataHash)
	{
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusUnknown);
	}
	
	return FSlateIcon(FAppStyle::GetAppStyleSetName(), CompileStatusBackground, NAME_None, CompileStatusGood);
}

void FStateTreeEditor::UpdateAsset()
{
	if (!StateTree)
	{
		return;
	}

	UE::StateTree::Editor::ValidateAsset(*StateTree);
	EditorDataHash = UE::StateTree::Editor::CalcAssetHash(*StateTree);
}


#undef LOCTEXT_NAMESPACE
