// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorToolkit.h"

#include "AdvancedPreviewScene.h"
#include "Animation/Skeleton.h"
#include "Dataflow/DataflowCore.h"
#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowContent.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowEditorMode.h"
#include "Dataflow/DataflowEditorModeToolkit.h"
#include "Dataflow/DataflowEditorModule.h"
#include "Dataflow/DataflowEditorModeUILayer.h"
#include "Dataflow/DataflowEditorViewport.h"
#include "Dataflow/DataflowEditorViewportClient.h"
#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowGraphEditor.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "Dataflow/DataflowPreviewScene.h"
#include "Dataflow/DataflowSchema.h"
#include "DynamicMeshBuilder.h"
#include "EditorModeManager.h"
#include "EditorStyleSet.h"
#include "EditorViewportTabContent.h"
#include "EditorViewportLayout.h"
#include "EditorViewportCommands.h"
#include "EdModeInteractiveToolsContext.h"
#include "Engine/SkeletalMesh.h"
#include "GraphEditorActions.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Styling/SlateStyleRegistry.h"
#include "Framework/Commands/GenericCommands.h"
#include "Widgets/Docking/SDockTab.h"
#include "ISkeletonTree.h"
#include "AssetEditorModeManager.h"
#include "ISkeletonEditorModule.h"
#include "IStructureDetailsView.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "DataflowEditorToolkit"

//DEFINE_LOG_CATEGORY_STATIC(EditorToolkitLog, Log, All);

const FName FDataflowEditorToolkit::GraphCanvasTabId(TEXT("DataflowEditor_GraphCanvas"));
const FName FDataflowEditorToolkit::NodeDetailsTabId(TEXT("DataflowEditor_NodeDetails"));
const FName FDataflowEditorToolkit::SkeletalTabId(TEXT("DataflowEditor_Skeletal"));
const FName FDataflowEditorToolkit::SelectionViewTabId_1(TEXT("DataflowEditor_SelectionView_1"));
const FName FDataflowEditorToolkit::SelectionViewTabId_2(TEXT("DataflowEditor_SelectionView_2"));
const FName FDataflowEditorToolkit::SelectionViewTabId_3(TEXT("DataflowEditor_SelectionView_3"));
const FName FDataflowEditorToolkit::SelectionViewTabId_4(TEXT("DataflowEditor_SelectionView_4"));
const FName FDataflowEditorToolkit::CollectionSpreadSheetTabId_1(TEXT("DataflowEditor_CollectionSpreadSheet_1"));
const FName FDataflowEditorToolkit::CollectionSpreadSheetTabId_2(TEXT("DataflowEditor_CollectionSpreadSheet_2"));
const FName FDataflowEditorToolkit::CollectionSpreadSheetTabId_3(TEXT("DataflowEditor_CollectionSpreadSheet_3"));
const FName FDataflowEditorToolkit::CollectionSpreadSheetTabId_4(TEXT("DataflowEditor_CollectionSpreadSheet_4"));
const FName FDataflowEditorToolkit::SimulationViewportTabId(TEXT("DataflowEditor_SimulationViewport"));

FDataflowEditorToolkit::FDataflowEditorToolkit(UAssetEditor* InOwningAssetEditor)
	: FBaseCharacterFXEditorToolkit(InOwningAssetEditor, FName("DataflowEditor"))
{
	check(Cast<UDataflowEditor>(InOwningAssetEditor));

	StandaloneDefaultLayout = FTabManager::NewLayout(FName("DataflowEditorLayout03"))
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.8f)	// Relative width of (Tools Panel, Construction Viewport, Preview Viewport, Dataflow Graph Editor, Outliner) vs (Asset Details, Preview Scene Details, Dataflow Node Details)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
					->SetSizeCoefficient(0.60f)	// Relative height of (Tools Panel, Construction Viewport, Preview Viewport) vs (Dataflow Graph Editor, Outliner)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.1f)		// Relative width of (Tools Panel) vs (Construction Viewport, Preview Viewport)
						->SetExtensionId(UDataflowEditorUISubsystem::EditorSidePanelAreaName)
						->SetHideTabWell(true)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.9f)		// Relative width of (Construction Viewport) vs (Tools Panel, Preview Viewport)
						->AddTab(ViewportTabID, ETabState::OpenedTab)
						->SetExtensionId("RestSpaceViewportArea")
						->SetHideTabWell(false)
					)
				)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
					->SetSizeCoefficient(0.40f)	// Relative height of (Dataflow Node Details) vs (Asset Details, Preview Scene Details)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.2f)
						->AddTab(CollectionSpreadSheetTabId_1, ETabState::OpenedTab)
						->SetExtensionId("CollectionSpreadSheetArea")
						->SetHideTabWell(false)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.8f)	// Relative height of (Dataflow Graph Editor, Outliner) vs (Tools Panel, Construction Viewport, Preview Viewport)
						->AddTab(GraphCanvasTabId, ETabState::OpenedTab)
						->SetExtensionId("GraphEditorArea")
						->SetHideTabWell(false)
						->SetForegroundTab(GraphCanvasTabId)
					)
				)
			)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.2f)	// Relative width of (Asset Details, Preview Scene Details, Dataflow Node Details) vs (Tools Panel, Construction Viewport, Preview Viewport, Dataflow Graph Editor, Outliner)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.65f)	// Relative height of (Asset Details, Preview Scene Details) vs (Dataflow Node Details)
					->AddTab(DetailsTabID, ETabState::OpenedTab)
					->SetExtensionId("DetailsArea")
					->SetHideTabWell(false)
					->SetForegroundTab(DetailsTabID)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.2f)	// Relative height of (Dataflow Node Details) vs (Asset Details, Preview Scene Details)
					->AddTab(NodeDetailsTabId, ETabState::OpenedTab)
					->SetExtensionId("NodeDetailsArea")
					->SetHideTabWell(false)
				)
			)
		);

	// Add any extenders specified by the UISubsystem
	// The extenders provide defined locations for FModeToolkit to attach
	// tool palette tabs and detail panel tabs
	LayoutExtender = MakeShared<FLayoutExtender>();
	FDataflowEditorModule* Module = &FModuleManager::LoadModuleChecked<FDataflowEditorModule>("DataflowEditor");
	Module->OnRegisterLayoutExtensions().Broadcast(*LayoutExtender);
	StandaloneDefaultLayout->ProcessExtensions(*LayoutExtender);

	FAdvancedPreviewScene::ConstructionValues PreviewSceneArgs;
	PreviewSceneArgs.bShouldSimulatePhysics = 1;
	PreviewSceneArgs.bCreatePhysicsScene = 1;
	
	ObjectScene = MakeUnique<FDataflowConstructionScene>(PreviewSceneArgs, GetDataflowContent());
	SimulationScene = MakeShared<FDataflowSimulationScene>(PreviewSceneArgs, GetDataflowContent());
}

FDataflowEditorToolkit::~FDataflowEditorToolkit()
{
	if (GraphEditor)
	{
		GraphEditor->OnSelectionChangedMulticast.Remove(OnSelectionChangedMulticastDelegateHandle);
		GraphEditor->OnNodeDeletedMulticast.Remove(OnNodeDeletedMulticastDelegateHandle);
	}

	if (NodeDetailsEditor)
	{
		NodeDetailsEditor->GetOnFinishedChangingPropertiesDelegate().Remove(OnFinishedChangingPropertiesDelegateHandle);
	}

	if (AssetDetailsEditor)
	{
		AssetDetailsEditor->OnFinishedChangingProperties().Remove(OnFinishedChangingAssetPropertiesDelegateHandle);
	}

	// We need to force the dataflow editor mode deletion now because otherwise the preview and rest-space worlds
	// will end up getting destroyed before the mode's Exit() function gets to run, and we'll get some
	// warnings when we destroy any mode actors.
	EditorModeManager->DestroyMode(UDataflowEditorMode::EM_DataflowEditorModeId);
	SimulationModeManager->DestroyMode(UDataflowEditorMode::EM_DataflowEditorModeId);
}

void FDataflowEditorToolkit::CreateEditorModeManager()
{
	// Setup the construction manager / scene
	FBaseCharacterFXEditorToolkit::CreateEditorModeManager();
	static_cast<FDataflowPreviewScene*>(ObjectScene.Get())->GetDataflowModeManager()
		= StaticCastSharedPtr<FAssetEditorModeManager>(EditorModeManager);

	// Setup the simulation manager / scene
	SimulationModeManager = MakeShared<FAssetEditorModeManager>();
	StaticCastSharedPtr<FAssetEditorModeManager>(SimulationModeManager)->SetPreviewScene(
		SimulationScene.Get());

	static_cast<FDataflowPreviewScene*>(SimulationScene.Get())->GetDataflowModeManager()
		= StaticCastSharedPtr<FAssetEditorModeManager>(SimulationModeManager);
}

bool FDataflowEditorToolkit::CanOpenDataflowEditor(UObject* ObjectToEdit)
{
	if (const UClass* Class = ObjectToEdit->GetClass())
	{
		return (Class->FindPropertyByName(FName("DataflowAsset")) != nullptr) ;
	}
	return false;
}

bool FDataflowEditorToolkit::HasDataflowAsset(UObject* ObjectToEdit)
{
	if (const UClass* Class = ObjectToEdit->GetClass())
	{
		if (FProperty* Property = Class->FindPropertyByName(FName("DataflowAsset")))
		{
			return *Property->ContainerPtrToValuePtr<UDataflow*>(ObjectToEdit) != nullptr;
		}
	}
	return false;
}

UDataflow* FDataflowEditorToolkit::GetDataflowAsset(UObject* ObjectToEdit)
{
	UDataflow* DataflowObject = Cast<UDataflow>(ObjectToEdit);

	if (!DataflowObject)
	{
		if (const UClass* Class = ObjectToEdit->GetClass())
		{
			if (FProperty* Property = Class->FindPropertyByName(FName("DataflowAsset")))
			{
				DataflowObject = *Property->ContainerPtrToValuePtr<UDataflow*>(ObjectToEdit);
			}
		}
	}
	return DataflowObject;
}

const UDataflow* FDataflowEditorToolkit::GetDataflowAsset(const UObject* ObjectToEdit)
{
	const UDataflow* DataflowObject = Cast<UDataflow>(ObjectToEdit);

	if (!DataflowObject)
	{
		if (const UClass* Class = ObjectToEdit->GetClass())
		{
			if (const FProperty* Property = Class->FindPropertyByName(FName("DataflowAsset")))
			{
				DataflowObject = *Property->ContainerPtrToValuePtr<const UDataflow*>(ObjectToEdit);
			}
		}
	}
	return DataflowObject;
}

//~ Begin FBaseCharacterFXEditorToolkit overrides

FEditorModeID FDataflowEditorToolkit::GetEditorModeId() const
{
	return UDataflowEditorMode::EM_DataflowEditorModeId;
}

TObjectPtr<UDataflowBaseContent> FDataflowEditorToolkit::GetDataflowContent()
{
	return Cast<UDataflowEditor>(OwningAssetEditor)->DataflowContent;
}

const TObjectPtr<UDataflowBaseContent> FDataflowEditorToolkit::GetDataflowContent() const
{
	return Cast<UDataflowEditor>(OwningAssetEditor)->DataflowContent;
}

bool FDataflowEditorToolkit::OnRequestClose(EAssetEditorCloseReason InCloseReason)
{
	// Note: This needs a bit of adjusting, because currently OnRequestClose seems to be 
	// called multiple times when the editor itself is being closed. We can take the route 
	// of NiagaraScriptToolkit and remember when changes are discarded, but this can cause
	// issues if the editor close sequence is interrupted due to some other asset editor.

	UDataflowEditorMode* DataflowEdMode = Cast<UDataflowEditorMode>(EditorModeManager->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId));
	if (!DataflowEdMode) {
		// If we don't have a valid mode, because the OnRequestClose is currently being called multiple times,
		// simply return true because there's nothing left to do.
		return true;
	}

	// Give any active modes a chance to shutdown while the toolkit host is still alive
	// This is super important to do, otherwise currently opened tabs won't be marked as "closed".
	// This results in tabs not being properly recycled upon reopening the editor and tab
	// duplication for each opening event.
	GetEditorModeManager().ActivateDefaultMode();

	return FAssetEditorToolkit::OnRequestClose(InCloseReason);
}

void FDataflowEditorToolkit::PostInitAssetEditor()
{
	// @todo(DataflowMode) : Enable base class functionality when the mode is ready.
	FBaseCharacterFXEditorToolkit::PostInitAssetEditor();
	
	// @todo(DataflowMode) : Access to mode in Toolkit construction
	//UDataflowEditorMode* const DataflowEditorMode = CastChecked<UDataflowEditorMode>(EditorModeManager->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId));
	
	auto SetCommonViewportClientOptions = [](FEditorViewportClient* Client)
	{
		// Normally the bIsRealtime flag is determined by whether the connection is remote, but our
		// tools require always being ticked.
		Client->SetRealtime(true);

		// Disable motion blur effects that cause our renders to "fade in" as things are moved
		Client->EngineShowFlags.SetTemporalAA(false);
		Client->EngineShowFlags.SetAntiAliasing(true);
		Client->EngineShowFlags.SetMotionBlur(false);

		// Disable the dithering of occluded portions of gizmos.
		Client->EngineShowFlags.SetOpaqueCompositeEditorPrimitives(true);

		// Disable hardware occlusion queries, which make it harder to use vertex shaders to pull materials
		// toward camera for z ordering because non-translucent materials start occluding themselves (once
		// the component bounds are behind the displaced geometry).
		Client->EngineShowFlags.SetDisableOcclusionQueries(true);

		// Default FOV of 90 degrees causes a fair bit of lens distortion, especially noticeable with smaller viewports
		Client->ViewFOV = 45.0;

		// Ortho has too many problems with rendering things, unfortunately, so we should use perspective.
		Client->SetViewportType(ELevelViewportType::LVT_Perspective);

		// Lit gives us the most options in terms of the materials we can use.
		Client->SetViewMode(EViewModeIndex::VMI_Lit);

		// If exposure isn't set to fixed, it will flash as we stare into the void
		Client->ExposureSettings.bFixed = true;

		// We need the viewport client to start out focused, or else it won't get ticked until
		// we click inside it.
		if(Client->Viewport)
		{
			Client->ReceivedFocus(Client->Viewport);
		}
	};
	SetCommonViewportClientOptions(ViewportClient.Get());
	SetCommonViewportClientOptions(SimulationViewportClient.Get());

	//@todo(brice) : FChaosClothAssetEditorToolkit::PostInitAssetEditor() has alot more going on. 

	//@todo(brice) : can we remove this coupling of the viewport and mode
	UDataflowEditorMode* const DataflowMode = CastChecked<UDataflowEditorMode>(EditorModeManager->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId));
	const TWeakPtr<FViewportClient> WeakViewportClient(ViewportClient);
	DataflowMode->SetRestSpaceViewportClient(StaticCastWeakPtr<FDataflowEditorViewportClient>(WeakViewportClient));
}

void FDataflowEditorToolkit::InitializeEdMode(UBaseCharacterFXEditorMode* EdMode)
{
	UDataflowEditorMode* DataflowMode = Cast<UDataflowEditorMode>(EdMode);
	check(DataflowMode);

	// We first set the preview scene in order to store the dynamic mesh elements
	// generated by the tools
	DataflowMode->SetDataflowConstructionScene(static_cast<FDataflowConstructionScene*>(ObjectScene.Get()));

	// Set of the graph editor to be able to add nodes
	DataflowMode->SetDataflowGraphEditor(GraphEditor);
	
	TArray<TObjectPtr<UObject>> ObjectsToEdit;
	OwningAssetEditor->GetObjectsToEdit(MutableView(ObjectsToEdit));
	DataflowMode->InitializeTargets(ObjectsToEdit);

	if (TSharedPtr<FModeToolkit> ModeToolkit = DataflowMode->GetToolkit().Pin())
	{
		FDataflowEditorModeToolkit* DataflowModeToolkit = static_cast<FDataflowEditorModeToolkit*>(ModeToolkit.Get());
		DataflowModeToolkit->SetRestSpaceViewportWidget(DataflowEditorViewport);

		FName ParentToolbarName;
		const FName ToolBarName = GetToolMenuToolbarName(ParentToolbarName);
		DataflowModeToolkit->BuildEditorToolBar(ToolBarName);
	}

	// @todo(brice) : This used to crash when comnmented out. 
	FBaseCharacterFXEditorToolkit::InitializeEdMode(EdMode);
}

void FDataflowEditorToolkit::CreateEditorModeUILayer()
{
	FBaseCharacterFXEditorToolkit::CreateEditorModeUILayer();
}

void FDataflowEditorToolkit::GetSaveableObjects(TArray<UObject*>& OutObjects) const
{
	FBaseCharacterFXEditorToolkit::GetSaveableObjects(OutObjects);

	if (ensure(GetDataflowContent()))
	{
		if (UDataflow* DataflowAsset = GetDataflowContent()->DataflowAsset)
		{
			check(DataflowAsset->IsAsset());
			OutObjects.Add(DataflowAsset);
		}
	}
}

//~ End FBaseCharacterFXEditorToolkit overrides


//~ Begin FBaseAssetToolkit overrides

void FDataflowEditorToolkit::CreateWidgets()
{
	FBaseCharacterFXEditorToolkit::CreateWidgets();

	if (TObjectPtr<UDataflowBaseContent> DataflowContent = GetDataflowContent())
	{
		if (UDataflow* DataflowAsset = DataflowContent->GetDataflowAsset())
		{
			NodeDetailsEditor = CreateNodeDetailsEditorWidget(DataflowContent->GetDataflowOwner());
			AssetDetailsEditor = CreateAssetDetailsEditorWidget(DataflowContent->GetDataflowOwner());
			GraphEditor = CreateGraphEditorWidget(DataflowAsset, NodeDetailsEditor);
			SkeletalEditor = CreateSkeletalEditorWidget();

			CreateSimulationViewportClient();
		}
	}
}

// Called from FBaseAssetToolkit::CreateWidgets. The delegate call path goes through FAssetEditorToolkit::InitAssetEditor
// and FBaseAssetToolkit::SpawnTab_Viewport.
AssetEditorViewportFactoryFunction FDataflowEditorToolkit::GetViewportDelegate()
{
	AssetEditorViewportFactoryFunction TempViewportDelegate = [this](FAssetEditorViewportConstructionArgs InArgs)
	{
		return SAssignNew(DataflowEditorViewport, SDataflowEditorViewport, InArgs)
			.ViewportClient(StaticCastSharedPtr<FDataflowEditorViewportClient>(ViewportClient));
	};

	return TempViewportDelegate;
}

// Called from FBaseAssetToolkit::CreateWidgets to populate ViewportClient, but otherwise only used 
// in our own viewport delegate.
TSharedPtr<FEditorViewportClient> FDataflowEditorToolkit::CreateEditorViewportClient() const
{
	// Note that we can't reliably adjust the viewport client here because we will be passing it
	// into the viewport created by the viewport delegate we get from GetViewportDelegate(), and
	// that delegate may (will) affect the settings based on FAssetEditorViewportConstructionArgs,
	// namely ViewportType.
	// Instead, we do viewport client adjustment in PostInitAssetEditor().
	check(EditorModeManager.IsValid());
	TSharedPtr<FDataflowEditorViewportClient> LocalConstructionClient = MakeShared<FDataflowEditorViewportClient>(
	EditorModeManager.Get(), ObjectScene.Get(), true);
	LocalConstructionClient->SetDataflowEditorToolkit(StaticCastSharedRef<FDataflowEditorToolkit>(
		const_cast<FDataflowEditorToolkit*>(this)->AsShared()));
	return LocalConstructionClient;
}

void FDataflowEditorToolkit::CreateSimulationViewportClient()
{
	SimulationTabContent = MakeShareable(new FEditorViewportTabContent());
	SimulationViewportClient = MakeShared<FDataflowEditorViewportClient>(SimulationModeManager.Get(),
		SimulationScene.Get(), false);
	
	SimulationViewportClient->SetDataflowEditorToolkit(StaticCastSharedRef<FDataflowEditorToolkit>(this->AsShared()));

	SimulationViewportDelegate = [this](FAssetEditorViewportConstructionArgs InArgs)
	{
		return SAssignNew(DataflowSimulationViewport, SDataflowEditorViewport, InArgs)
			.ViewportClient(StaticCastSharedPtr<FDataflowEditorViewportClient>(SimulationViewportClient));
	};
}

//~ End FBaseAssetToolkit overrides

void FDataflowEditorToolkit::OnPropertyValueChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (TObjectPtr<UDataflowBaseContent> EditorContent = GetDataflowContent())
	{
		ensure(EditorContent);
		if (UDataflow* DataflowAsset = EditorContent->GetDataflowAsset())
		{
			TSharedPtr<Dataflow::FEngineContext> DataflowContext = EditorContent->GetDataflowContext();
			Dataflow::FTimestamp LastNodeTimestamp = EditorContent->GetLastModifiedTimestamp();
			
			FDataflowEditorCommands::OnPropertyValueChanged(DataflowAsset, DataflowContext, LastNodeTimestamp, PropertyChangedEvent, SelectedDataflowNodes);

			EditorContent->SetDataflowContext(DataflowContext);
			EditorContent->SetLastModifiedTimestamp(LastNodeTimestamp);
		}
	}
}

void FDataflowEditorToolkit::OnAssetPropertyValueChanged(const FPropertyChangedEvent& PropertyChangedEvent)
{
	if (TObjectPtr<UDataflowBaseContent> EditorContent = GetDataflowContent())
	{
		ensure(EditorContent);	FDataflowEditorCommands::OnAssetPropertyValueChanged(EditorContent, PropertyChangedEvent);
	}
}

bool FDataflowEditorToolkit::OnNodeVerifyTitleCommit(const FText& NewText, UEdGraphNode* GraphNode, FText& OutErrorMessage)
{
	return FDataflowEditorCommands::OnNodeVerifyTitleCommit(NewText, GraphNode, OutErrorMessage);
}

void FDataflowEditorToolkit::OnNodeTitleCommitted(const FText& InNewText, ETextCommit::Type InCommitType, UEdGraphNode* GraphNode)
{
	FDataflowEditorCommands::OnNodeTitleCommitted(InNewText, InCommitType, GraphNode);
}

void FDataflowEditorToolkit::OnNodeSelectionChanged(const TSet<UObject*>& InNewSelection)
{
	auto FindDataflowNodesInSet = [](const TSet<UObject*>& InSet) {
		TSet<UObject*> Results;
		for (UObject* Item : InSet)
		{
			if (Cast<UDataflowEdNode>(Item))
			{
				Results.Add(Item);
			}
		}
		return Results;
	};

	auto ResetListeners = [&ViewListeners = ViewListeners](UDataflowEdNode* Node = nullptr)
	{
		for (IDataflowViewListener* Listener : ViewListeners)
		{
			Listener->OnSelectedNodeChanged(nullptr);
		}
		if (Node)
		{
			for (IDataflowViewListener* Listener : ViewListeners)
			{
				Listener->OnSelectedNodeChanged(Node);
			}
		}
	};

	// Despite this function's name, we might not have actually changed which node is selected
	bool bPrimarySelectionChanged = false;

	if (TObjectPtr<UDataflowBaseContent> EditorContent = GetDataflowContent(); EditorContent->DataflowAsset)
	{
		// Only keep UDataflowEdNode from NewSelection
		TSet<UObject*> NodeSelection = FindDataflowNodesInSet(InNewSelection);

		if (!NodeSelection.Num())
		{
			// The selection is empty. 
			ResetListeners();
			SelectedDataflowNodes = TSet<UObject*>();
			if (PrimarySelection) bPrimarySelectionChanged = true;
			PrimarySelection = nullptr;
		}
		else
		{
			TSet<UObject*> DeselectedNodes = SelectedDataflowNodes.Difference(NodeSelection);
			TSet<UObject*> StillSelectedNodes = SelectedDataflowNodes.Intersect(NodeSelection);
			TSet<UObject*> NewlySelectedNodes = NodeSelection.Difference(SelectedDataflowNodes);

			// Something has been removed
			if (DeselectedNodes.Num())
			{
				if (DeselectedNodes.Contains(PrimarySelection))
				{
					ResetListeners();

					if (PrimarySelection) bPrimarySelectionChanged = true;
					PrimarySelection = nullptr;

					// pick a new primary if nothing new was selected
					if (!NewlySelectedNodes.Num() && StillSelectedNodes.Num())
					{
						PrimarySelection = Cast< UDataflowEdNode>(StillSelectedNodes.Array()[0]);
						ResetListeners(PrimarySelection);
						bPrimarySelectionChanged = true;
					}
				}
			}

			// Something new has been selected.
			if (NewlySelectedNodes.Num() == 1)
			{
				PrimarySelection = Cast< UDataflowEdNode>(NewlySelectedNodes.Array()[0]);
				ResetListeners(PrimarySelection);
				bPrimarySelectionChanged = true;
			}

			SelectedDataflowNodes = NodeSelection;
		}

		if (bPrimarySelectionChanged)
		{
			EditorContent->SetPrimarySelectedNode(nullptr);

			UDataflowEditorMode* const DataflowMode = CastChecked<UDataflowEditorMode>(EditorModeManager->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId));
			if (DataflowMode)
			{
				// Close any running tool. OnNodeSingleClicked() will start a new tool if a new node was clicked.
				UEditorInteractiveToolsContext* const ToolsContext = DataflowMode->GetInteractiveToolsContext();
				checkf(ToolsContext, TEXT("No valid ToolsContext found for FDataflowEditorToolkit"));
				if (ToolsContext->HasActiveTool())
				{
					ToolsContext->EndTool(EToolShutdownType::Completed);
				}

				EditorContent->SetPrimarySelectedNode(PrimarySelection);
			}
			EditorContent->SetIsDirty(true);
		}
	}
}

void FDataflowEditorToolkit::OnNodeSingleClicked(UObject* ClickedNode) const
{
	UDataflowEditorMode* DataflowMode = Cast<UDataflowEditorMode>(EditorModeManager->GetActiveScriptableMode(UDataflowEditorMode::EM_DataflowEditorModeId));
	if (DataflowMode)
	{
		if (GraphEditor && GraphEditor->GetSingleSelectedNode() == ClickedNode)
		{
			// Start the corresponding tool
			DataflowMode->StartToolForSelectedNode(ClickedNode);
		}
	}
}


void FDataflowEditorToolkit::OnNodeDeleted(const TSet<UObject*>& NewSelection)
{
	for (UObject* Node : NewSelection)
	{
		if (SelectedDataflowNodes.Contains(Node))
		{
			SelectedDataflowNodes.Remove(Node);
		}
	}
}

void FDataflowEditorToolkit::Tick(float DeltaTime)
{
	if (TObjectPtr<UDataflowBaseContent> EditorContent = GetDataflowContent())
	{
		if (EditorContent->GetDataflowAsset())
		{
			Dataflow::FTimestamp TimeStamp = EditorContent->GetLastModifiedTimestamp();
			if (!EditorContent->GetDataflowContext())
			{
				EditorContent->SetDataflowContext(MakeShared<Dataflow::FEngineContext>(EditorContent->GetDataflowOwner(), EditorContent->GetDataflowAsset(), Dataflow::FTimestamp::Invalid));
				TimeStamp = Dataflow::FTimestamp::Invalid;
			}

			// OnTick evaluation only pulls the termnial nodes. The other evaluations can be specific nodes. 
			FDataflowEditorCommands::EvaluateTerminalNode(*EditorContent->GetDataflowContext().Get(), TimeStamp, EditorContent->GetDataflowAsset(),
				nullptr, nullptr, EditorContent->GetDataflowOwner(), EditorContent->GetDataflowTerminal());
			EditorContent->SetLastModifiedTimestamp(TimeStamp);
		}
	}
}

TStatId FDataflowEditorToolkit::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FDataflowEditorToolkit, STATGROUP_Tickables);
}

TSharedRef<SDataflowGraphEditor> FDataflowEditorToolkit::CreateGraphEditorWidget(UDataflow* DataflowToEdit, TSharedPtr<IStructureDetailsView> InNodeDetailsEditor)
{
	ensure(DataflowToEdit);
	using namespace Dataflow;

	FDataflowEditorCommands::FGraphEvaluationCallback Evaluate = [&](FDataflowNode* Node, FDataflowOutput* Out)
	{
		if (TObjectPtr<UDataflowBaseContent> EditorContent = GetDataflowContent())
		{
			if (EditorContent->GetDataflowAsset())
			{
				if (!EditorContent->GetDataflowContext())
				{
					EditorContent->SetDataflowContext(MakeShared<Dataflow::FEngineContext>(EditorContent->GetDataflowOwner(), EditorContent->GetDataflowAsset(), Dataflow::FTimestamp::Invalid));
				}
				Node->Invalidate();
				Dataflow::FTimestamp TimeStamp = Dataflow::FTimestamp::Invalid;
				
				FDataflowEditorCommands::EvaluateTerminalNode(*EditorContent->GetDataflowContext().Get(), TimeStamp, EditorContent->GetDataflowAsset(),
					Node, Out, EditorContent->GetDataflowOwner(), EditorContent->GetDataflowTerminal());

				EditorContent->SetLastModifiedTimestamp(TimeStamp);
			}
		}
	};

	SGraphEditor::FGraphEditorEvents InEvents;
	InEvents.OnVerifyTextCommit = FOnNodeVerifyTextCommit::CreateSP(this, &FDataflowEditorToolkit::OnNodeVerifyTitleCommit);
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FDataflowEditorToolkit::OnNodeTitleCommitted);
	InEvents.OnNodeSingleClicked = SGraphEditor::FOnNodeSingleClicked::CreateSP(this, &FDataflowEditorToolkit::OnNodeSingleClicked);

	TSharedRef<SDataflowGraphEditor> NewGraphEditor = SNew(SDataflowGraphEditor, DataflowToEdit)
		.GraphToEdit(DataflowToEdit)
		.GraphEvents(InEvents)
		.DetailsView(InNodeDetailsEditor)
		.EvaluateGraph(Evaluate);

	OnSelectionChangedMulticastDelegateHandle = NewGraphEditor->OnSelectionChangedMulticast.AddSP(this, &FDataflowEditorToolkit::OnNodeSelectionChanged);
	OnNodeDeletedMulticastDelegateHandle = NewGraphEditor->OnNodeDeletedMulticast.AddSP(this, &FDataflowEditorToolkit::OnNodeDeleted);

	return NewGraphEditor;
}

TSharedPtr<IStructureDetailsView> FDataflowEditorToolkit::CreateNodeDetailsEditorWidget(UObject* ObjectToEdit)
{
	ensure(ObjectToEdit);
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bSearchInitialKeyFocus = true;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.NotifyHook = nullptr;
		DetailsViewArgs.bShowOptions = true;
		DetailsViewArgs.bShowModifiedPropertiesOption = false;
		DetailsViewArgs.bShowScrollBar = false;
	}

	FStructureDetailsViewArgs StructureViewArgs;
	{
		StructureViewArgs.bShowObjects = true;
		StructureViewArgs.bShowAssets = true;
		StructureViewArgs.bShowClasses = true;
		StructureViewArgs.bShowInterfaces = true;
	}
	TSharedPtr<IStructureDetailsView> LocalDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, nullptr);
	LocalDetailsView->GetDetailsView()->SetObject(ObjectToEdit);
	OnFinishedChangingPropertiesDelegateHandle = LocalDetailsView->GetOnFinishedChangingPropertiesDelegate().AddSP(this, &FDataflowEditorToolkit::OnPropertyValueChanged);

	return LocalDetailsView;
}

TSharedPtr<IDetailsView> FDataflowEditorToolkit::CreateAssetDetailsEditorWidget(UObject* ObjectToEdit)
{
	ensure(ObjectToEdit);
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.NotifyHook = this;
	}

	TSharedPtr<IDetailsView> LocalDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	LocalDetailsView->SetObject(ObjectToEdit);

	OnFinishedChangingAssetPropertiesDelegateHandle = LocalDetailsView->OnFinishedChangingProperties().AddSP(this, &FDataflowEditorToolkit::OnAssetPropertyValueChanged);

	return LocalDetailsView;

}

TSharedPtr<ISkeletonTree> FDataflowEditorToolkit::CreateSkeletalEditorWidget()
{
	if (const TObjectPtr<UDataflowSkeletalContent> SkeletalContent = Cast<UDataflowSkeletalContent>(GetDataflowContent()))
	{
		if (SkeletalContent->GetDataflowAsset())
		{
			const FSkeletonTreeArgs SkeletonTreeArgs;
			ISkeletonEditorModule& SkeletonEditorModule = FModuleManager::LoadModuleChecked<ISkeletonEditorModule>("SkeletonEditor");
			TSharedPtr<ISkeletonTree> SkeletonTree = SkeletonEditorModule.CreateSkeletonTree(SkeletalContent->GetSkeleton(), SkeletonTreeArgs);
			return SkeletonTree;
		}
	}
	return TSharedPtr<ISkeletonTree>(nullptr);
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_AssetDetails(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == DetailsTabID);

	return SNew(SDockTab)
		.Label(LOCTEXT("DataflowEditor_AssetDetails_TabTitle", "Asset Details"))
		[
			AssetDetailsEditor->AsShared()
		];
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_SimulationViewport(const FSpawnTabArgs& Args)
{
	TSharedRef<SDockTab> DockableTab = SNew(SDockTab);
	if(SimulationTabContent)
	{
		SimulationTabContent->Initialize(SimulationViewportDelegate, DockableTab, SimulationViewportTabId.ToString());
	}
	return DockableTab;
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_GraphCanvas(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == GraphCanvasTabId);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("DataflowEditor_Dataflow_TabTitle", "Dataflow Graph"))
		[
			GraphEditor.ToSharedRef()
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_NodeDetails(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == NodeDetailsTabId);

	return SNew(SDockTab)
		.Label(LOCTEXT("DataflowEditor_NodeDetails_TabTitle", "Node Details"))
		[
			NodeDetailsEditor->GetWidget()->AsShared()
		];
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_Skeletal(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == SkeletalTabId);
	if(SkeletalEditor)
	{
		if (const TObjectPtr<UDataflowSkeletalContent> EditorContent = Cast<UDataflowSkeletalContent>(GetDataflowContent()))
		{
			SkeletalEditor->SetSkeletalMesh(EditorContent->GetSkeletalMesh());
		}
		return SNew(SDockTab)
			.Label(LOCTEXT("DataflowEditor_Skeletal_TabTitle", "Skeletal Hierarchy"))
			[
				SkeletalEditor.ToSharedRef()
			];
	}

	return SNew(SDockTab)
		.Label(LOCTEXT("DataflowEditor_Skeletal_TabTitle", "Skeletal Hierarchy"));
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_SelectionView(const FSpawnTabArgs& Args)
{
	//	check(Args.GetTabId().TabType == SelectionViewTabId_1);

	if (Args.GetTabId() == SelectionViewTabId_1)
	{
		DataflowSelectionView_1 = MakeShared<FDataflowSelectionView>(FDataflowSelectionView());
		if (DataflowSelectionView_1.IsValid())
		{
			ViewListeners.Add(DataflowSelectionView_1.Get());
		}
	}
	else if (Args.GetTabId() == SelectionViewTabId_2)
	{
		DataflowSelectionView_2 = MakeShared<FDataflowSelectionView>(FDataflowSelectionView());
		if (DataflowSelectionView_2.IsValid())
		{
			ViewListeners.Add(DataflowSelectionView_2.Get());
		}
	}
	else if (Args.GetTabId() == SelectionViewTabId_3)
	{
		DataflowSelectionView_3 = MakeShared<FDataflowSelectionView>(FDataflowSelectionView());
		if (DataflowSelectionView_3.IsValid())
		{
			ViewListeners.Add(DataflowSelectionView_3.Get());
		}
	}
	else if (Args.GetTabId() == SelectionViewTabId_4)
	{
		DataflowSelectionView_4 = MakeShared<FDataflowSelectionView>(FDataflowSelectionView());
		if (DataflowSelectionView_4.IsValid())
		{
			ViewListeners.Add(DataflowSelectionView_4.Get());
		}
	}

	TSharedPtr<SSelectionViewWidget> SelectionViewWidget;

	TSharedRef<SDockTab> DockableTab = SNew(SDockTab)
		[
			SAssignNew(SelectionViewWidget, SSelectionViewWidget)
		];

	if (SelectionViewWidget)
	{
		if (const TObjectPtr<UDataflowBaseContent> EditorContent = GetDataflowContent())
		{
			if (Args.GetTabId() == SelectionViewTabId_1)
			{
				DataflowSelectionView_1->SetSelectionView(SelectionViewWidget);

				// Set the Context on the interface
				if (TSharedPtr<Dataflow::FContext> CurrentContext = EditorContent->GetDataflowContext())
				{
					DataflowSelectionView_1->SetContext(CurrentContext);
				}
			}
			else if (Args.GetTabId() == SelectionViewTabId_2)
			{
				DataflowSelectionView_2->SetSelectionView(SelectionViewWidget);

				// Set the Context on the interface
				if (TSharedPtr<Dataflow::FContext> CurrentContext = EditorContent->GetDataflowContext())
				{
					DataflowSelectionView_2->SetContext(CurrentContext);
				}
			}
			else if (Args.GetTabId() == SelectionViewTabId_3)
			{
				DataflowSelectionView_3->SetSelectionView(SelectionViewWidget);

				// Set the Context on the interface
				if (TSharedPtr<Dataflow::FContext> CurrentContext = EditorContent->GetDataflowContext())
				{
					DataflowSelectionView_3->SetContext(CurrentContext);
				}
			}
			else if (Args.GetTabId() == SelectionViewTabId_4)
			{
				DataflowSelectionView_4->SetSelectionView(SelectionViewWidget);

				// Set the Context on the interface
				if (TSharedPtr<Dataflow::FContext> CurrentContext = EditorContent->GetDataflowContext())
				{
					DataflowSelectionView_4->SetContext(CurrentContext);
				}
			}
		}
	}

	DockableTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &FDataflowEditorToolkit::OnTabClosed));

	return DockableTab;
}

TSharedRef<SDockTab> FDataflowEditorToolkit::SpawnTab_CollectionSpreadSheet(const FSpawnTabArgs& Args)
{
	if (Args.GetTabId() == CollectionSpreadSheetTabId_1)
	{
		DataflowCollectionSpreadSheet_1 = MakeShared<FDataflowCollectionSpreadSheet>(FDataflowCollectionSpreadSheet());
		if (DataflowCollectionSpreadSheet_1.IsValid())
		{
			ViewListeners.Add(DataflowCollectionSpreadSheet_1.Get());
		}
	}
	else if (Args.GetTabId() == CollectionSpreadSheetTabId_2)
	{
		DataflowCollectionSpreadSheet_2 = MakeShared<FDataflowCollectionSpreadSheet>(FDataflowCollectionSpreadSheet());
		if (DataflowCollectionSpreadSheet_2.IsValid())
		{
			ViewListeners.Add(DataflowCollectionSpreadSheet_2.Get());
		}
	}
	else if (Args.GetTabId() == CollectionSpreadSheetTabId_3)
	{
		DataflowCollectionSpreadSheet_3 = MakeShared<FDataflowCollectionSpreadSheet>(FDataflowCollectionSpreadSheet());
		if (DataflowCollectionSpreadSheet_3.IsValid())
		{
			ViewListeners.Add(DataflowCollectionSpreadSheet_3.Get());
		}
	}
	else if (Args.GetTabId() == CollectionSpreadSheetTabId_4)
	{
		DataflowCollectionSpreadSheet_4 = MakeShared<FDataflowCollectionSpreadSheet>(FDataflowCollectionSpreadSheet());
		if (DataflowCollectionSpreadSheet_4.IsValid())
		{
			ViewListeners.Add(DataflowCollectionSpreadSheet_4.Get());
		}
	}

	TSharedPtr<SCollectionSpreadSheetWidget> CollectionSpreadSheetWidget;

	TSharedRef<SDockTab> DockableTab = SNew(SDockTab)
	[
		SAssignNew(CollectionSpreadSheetWidget, SCollectionSpreadSheetWidget)
	];

	if (CollectionSpreadSheetWidget)
	{
		if (const TObjectPtr<UDataflowBaseContent> EditorContent = GetDataflowContent())
		{
			if (Args.GetTabId() == CollectionSpreadSheetTabId_1)
			{
				DataflowCollectionSpreadSheet_1->SetCollectionSpreadSheet(CollectionSpreadSheetWidget);

				// Set the Context on the interface
				if (TSharedPtr<Dataflow::FContext> CurrentContext = EditorContent->GetDataflowContext())
				{
					DataflowCollectionSpreadSheet_1->SetContext(CurrentContext);
				}
			}
			else if (Args.GetTabId() == CollectionSpreadSheetTabId_2)
			{
				DataflowCollectionSpreadSheet_2->SetCollectionSpreadSheet(CollectionSpreadSheetWidget);

				// Set the Context on the interface
				if (TSharedPtr<Dataflow::FContext> CurrentContext = EditorContent->GetDataflowContext())
				{
					DataflowCollectionSpreadSheet_2->SetContext(CurrentContext);
				}
			}
			else if (Args.GetTabId() == CollectionSpreadSheetTabId_3)
			{
				DataflowCollectionSpreadSheet_3->SetCollectionSpreadSheet(CollectionSpreadSheetWidget);

				// Set the Context on the interface
				if (TSharedPtr<Dataflow::FContext> CurrentContext = EditorContent->GetDataflowContext())
				{
					DataflowCollectionSpreadSheet_3->SetContext(CurrentContext);
				}
			}
			else if (Args.GetTabId() == CollectionSpreadSheetTabId_4)
			{
				DataflowCollectionSpreadSheet_4->SetCollectionSpreadSheet(CollectionSpreadSheetWidget);

				// Set the Context on the interface
				if (TSharedPtr<Dataflow::FContext> CurrentContext = EditorContent->GetDataflowContext())
				{
					DataflowCollectionSpreadSheet_4->SetContext(CurrentContext);
				}
			}
		}
	}

	DockableTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &FDataflowEditorToolkit::OnTabClosed));

	return DockableTab;
}

void FDataflowEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	EditorMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_DataflowEditor", "Dataflow Editor"));
	const TSharedRef<FWorkspaceItem> SelectionViewWorkspaceMenuCategoryRef = EditorMenuCategory->AddGroup(LOCTEXT("WorkspaceMenu_SelectionView", "Selection View"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
	const TSharedRef<FWorkspaceItem> CollectionSpreadSheetWorkspaceMenuCategoryRef = EditorMenuCategory->AddGroup(LOCTEXT("WorkspaceMenu_CollectionSpreadSheet", "Collection SpreadSheet"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
	
	InTabManager->RegisterTabSpawner(ViewportTabID, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_Viewport))
		.SetDisplayName(LOCTEXT("DataflowViewportTab", "Construction Viewport"))
		.SetGroup(EditorMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(SimulationViewportTabId, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_SimulationViewport))
		.SetDisplayName(LOCTEXT("SimulationViewportTab", "Simulation Viewport"))
		.SetGroup(EditorMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(DetailsTabID, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_AssetDetails))
		.SetDisplayName(LOCTEXT("AssetDetailsTab", "Asset Details"))
		.SetGroup(EditorMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));
	
	InTabManager->RegisterTabSpawner(GraphCanvasTabId, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_GraphCanvas))
		.SetDisplayName(LOCTEXT("DataflowTab", "Dataflow Graph"))
		.SetGroup(EditorMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.EventGraph_16x"));

	InTabManager->RegisterTabSpawner(NodeDetailsTabId, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_NodeDetails))
		.SetDisplayName(LOCTEXT("NodeDetailsTab", "Node Details"))
		.SetGroup(EditorMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(SkeletalTabId, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_Skeletal))
		.SetDisplayName(LOCTEXT("DataflowSkeletalTab", "Skeletal Hierarchy"))
		.SetGroup(EditorMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.SkeletalHierarchy"));

	InTabManager->RegisterTabSpawner(SelectionViewTabId_1, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_SelectionView))
		.SetDisplayName(LOCTEXT("DataflowSelectionViewTab1", "Selection View 1"))
		.SetGroup(SelectionViewWorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(SelectionViewTabId_2, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_SelectionView))
		.SetDisplayName(LOCTEXT("DataflowSelectionViewTab2", "Selection View 2"))
		.SetGroup(SelectionViewWorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(SelectionViewTabId_3, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_SelectionView))
		.SetDisplayName(LOCTEXT("DataflowSelectionViewTab3", "Selection View 3"))
		.SetGroup(SelectionViewWorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(SelectionViewTabId_4, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_SelectionView))
		.SetDisplayName(LOCTEXT("DataflowSelectionViewTab4", "Selection View 4"))
		.SetGroup(SelectionViewWorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(CollectionSpreadSheetTabId_1, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_CollectionSpreadSheet))
		.SetDisplayName(LOCTEXT("DataflowCollectionSpreadSheetTab1", "Collection SpreadSheet 1"))
		.SetGroup(CollectionSpreadSheetWorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(CollectionSpreadSheetTabId_2, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_CollectionSpreadSheet))
		.SetDisplayName(LOCTEXT("DataflowCollectionSpreadSheetTab2", "Collection SpreadSheet 2"))
		.SetGroup(CollectionSpreadSheetWorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(CollectionSpreadSheetTabId_3, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_CollectionSpreadSheet))
		.SetDisplayName(LOCTEXT("DataflowCollectionSpreadSheetTab3", "Collection SpreadSheet 3"))
		.SetGroup(CollectionSpreadSheetWorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));

	InTabManager->RegisterTabSpawner(CollectionSpreadSheetTabId_4, FOnSpawnTab::CreateSP(this, &FDataflowEditorToolkit::SpawnTab_CollectionSpreadSheet))
		.SetDisplayName(LOCTEXT("DataflowCollectionSpreadSheetTab4", "Collection SpreadSheet 4"))
		.SetGroup(CollectionSpreadSheetWorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"));
}

void FDataflowEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FBaseAssetToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(GraphCanvasTabId);
	InTabManager->UnregisterTabSpawner(NodeDetailsTabId);
	InTabManager->UnregisterTabSpawner(SkeletalTabId);
	InTabManager->UnregisterTabSpawner(SelectionViewTabId_1);
	InTabManager->UnregisterTabSpawner(SelectionViewTabId_2);
	InTabManager->UnregisterTabSpawner(SelectionViewTabId_3);
	InTabManager->UnregisterTabSpawner(SelectionViewTabId_4);
	InTabManager->UnregisterTabSpawner(CollectionSpreadSheetTabId_1);
	InTabManager->UnregisterTabSpawner(CollectionSpreadSheetTabId_2);
	InTabManager->UnregisterTabSpawner(CollectionSpreadSheetTabId_3);
	InTabManager->UnregisterTabSpawner(CollectionSpreadSheetTabId_4);
	InTabManager->UnregisterTabSpawner(SimulationViewportTabId);
}

void FDataflowEditorToolkit::OnTabClosed(TSharedRef<SDockTab> Tab)
{
	if (Tab->GetTabLabel().EqualTo(FText::FromString("Selection View 1")))
	{
		ViewListeners.Remove(DataflowSelectionView_1.Get());
	}
	else if (Tab->GetTabLabel().EqualTo(FText::FromString("Selection View 2")))
	{
		ViewListeners.Remove(DataflowSelectionView_2.Get());
	}
	else if (Tab->GetTabLabel().EqualTo(FText::FromString("Selection View 3")))
	{
		ViewListeners.Remove(DataflowSelectionView_3.Get());
	}
	else if (Tab->GetTabLabel().EqualTo(FText::FromString("Selection View 4")))
	{
		ViewListeners.Remove(DataflowSelectionView_4.Get());
	}
	else if (Tab->GetTabLabel().EqualTo(FText::FromString("Collection SpreadSheet 1")))
	{
		ViewListeners.Remove(DataflowCollectionSpreadSheet_1.Get());
	}
	else if (Tab->GetTabLabel().EqualTo(FText::FromString("Collection SpreadSheet 2")))
	{
		ViewListeners.Remove(DataflowCollectionSpreadSheet_2.Get());
	}
	else if (Tab->GetTabLabel().EqualTo(FText::FromString("Collection SpreadSheet 3")))
	{
		ViewListeners.Remove(DataflowCollectionSpreadSheet_3.Get());
	}
	else if (Tab->GetTabLabel().EqualTo(FText::FromString("Collection SpreadSheet 4")))
	{
		ViewListeners.Remove(DataflowCollectionSpreadSheet_4.Get());
	}
}

FName FDataflowEditorToolkit::GetToolkitFName() const
{
	return FName("DataflowEditor");
}

FText FDataflowEditorToolkit::GetToolkitName() const
{
	if (const TObjectPtr<UDataflowBaseContent> EditorContent = GetDataflowContent())
	{
		if (EditorContent->GetDataflowOwner())
		{
			return  GetLabelForObject(EditorContent->GetDataflowOwner());
		}
		else if (EditorContent->GetDataflowAsset())
		{
			return  GetLabelForObject(EditorContent->GetDataflowAsset());
		}
	}
	return  LOCTEXT("ToolkitName", "Empty Dataflow Editor");
}

FText FDataflowEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Dataflow Editor");
}

FText FDataflowEditorToolkit::GetToolkitToolTipText() const
{
	return  LOCTEXT("ToolkitToolTipText", "Dataflow Editor");
}

FString FDataflowEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Dataflow").ToString();
}

FLinearColor FDataflowEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.3f, 0.2f, 0.5f, 0.5f);
}

#undef LOCTEXT_NAMESPACE
