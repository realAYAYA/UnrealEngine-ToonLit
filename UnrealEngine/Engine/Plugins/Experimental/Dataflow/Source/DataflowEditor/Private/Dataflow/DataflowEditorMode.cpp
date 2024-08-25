// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowEditorMode.h"

#include "AssetEditorModeManager.h"
#include "AttributeEditorTool.h"
#include "BaseGizmos/TransformGizmoUtil.h"
#include "Components/DynamicMeshComponent.h"
#include "ContextObjectStore.h"
#include "DataflowEditorTools/DataflowEditorWeightMapPaintTool.h"
#include "Dataflow/DataflowComponentToolTarget.h"
#include "Dataflow/DataflowCollectionAddScalarVertexPropertyNode.h"
#include "Dataflow/DataflowEditor.h"
#include "Dataflow/DataflowContent.h"
#include "Dataflow/DataflowEditorCommands.h"
#include "Dataflow/DataflowEditorModeToolkit.h"
#include "Dataflow/DataflowEditorViewportClient.h"
#include "Dataflow/DataflowGraphEditor.h"
#include "Dataflow/DataflowPreviewScene.h"
#include "Dataflow/DataflowSNode.h"
#include "Dataflow/DataflowToolTarget.h"
#include "EditorModeManager.h"
#include "EdModeInteractiveToolsContext.h"
#include "Elements/Framework/EngineElementsLibrary.h"
#include "MeshSelectionTool.h"
#include "MeshVertexPaintTool.h"
#include "MeshAttributePaintTool.h"
#include "ModelingToolTargetUtil.h"
#include "TargetInterfaces/MaterialProvider.h"
#include "TargetInterfaces/DynamicMeshCommitter.h"
#include "TargetInterfaces/DynamicMeshProvider.h"
#include "ToolTargetManager.h"
#include "ToolTargets/DynamicMeshComponentToolTarget.h"
#include "ToolTargets/StaticMeshComponentToolTarget.h"
#include "ToolTargets/StaticMeshToolTarget.h"
#include "ToolTargets/SkeletalMeshComponentToolTarget.h"
#include "ToolTargets/SkeletalMeshToolTarget.h"
#include "Tools/UEdMode.h"
#include "Selection.h"
#include "UnrealClient.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DataflowEditorMode)

#define LOCTEXT_NAMESPACE "UDataflowEditorMode"

const FEditorModeID UDataflowEditorMode::EM_DataflowEditorModeId = TEXT("EM_DataflowAssetEditorMode");

UDataflowEditorMode::UDataflowEditorMode()
{
	Info = FEditorModeInfo(
		EM_DataflowEditorModeId,
		LOCTEXT("DataflowEditorModeName", "Dataflow"),
		FSlateIcon(),
		false);
}

const FToolTargetTypeRequirements& UDataflowEditorMode::GetToolTargetRequirements()
{
	static const FToolTargetTypeRequirements ToolTargetRequirements =
		FToolTargetTypeRequirements({
			UMaterialProvider::StaticClass(),
			UDynamicMeshCommitter::StaticClass(),
			UDynamicMeshProvider::StaticClass()
			});

	return ToolTargetRequirements;
}

void UDataflowEditorMode::Enter()
{
	UBaseCharacterFXEditorMode::Enter();

	// Register gizmo ContextObject for use inside interactive tools
	UE::TransformGizmoUtil::RegisterTransformGizmoContextObject(GetInteractiveToolsContext());
}

void UDataflowEditorMode::AddToolTargetFactories()
{
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<UDynamicMeshComponentToolTargetFactory>(GetToolManager()));
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<UStaticMeshComponentToolTargetFactory>(GetToolManager()));
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<UStaticMeshToolTargetFactory>(GetToolManager()));
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<USkeletalMeshComponentToolTargetFactory>(GetToolManager()));
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<USkeletalMeshToolTargetFactory>(GetToolManager()));
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<UDataflowComponentToolTargetFactory>(GetToolManager()));
	GetInteractiveToolsContext()->TargetManager->AddTargetFactory(NewObject<UDataflowToolTargetFactory>(GetToolManager()));
}

void UDataflowEditorMode::RegisterDataflowTool(TSharedPtr<FUICommandInfo> UICommand,
	FString ToolIdentifier,
	UInteractiveToolBuilder* Builder,
	const IDataflowEditorToolBuilder* DataflowToolBuilder,
	UEditorInteractiveToolsContext* const ToolsContext,
	EToolsContextScope ToolScope)
{
	if (!Toolkit.IsValid())
	{
		return;
	}

	if (!ToolsContext)
	{
		return;
	}

	if (ToolScope == EToolsContextScope::Default)
	{
		ToolScope = GetDefaultToolScope();
	}
	ensure(ToolScope != EToolsContextScope::Editor);

	ToolsContext->ToolManager->RegisterToolType(ToolIdentifier, Builder);

	const TSharedRef<FUICommandList>& CommandList = Toolkit->GetToolkitCommands();

	CommandList->MapAction(UICommand, 
		FExecuteAction::CreateWeakLambda(ToolsContext, [this, ToolsContext, ToolIdentifier, DataflowToolBuilder]()
		{
			// Check if we need to switch view modes before starting the tool
			TArray<Dataflow::EDataflowPatternVertexType> SupportedModes;
			DataflowToolBuilder->GetSupportedViewModes(SupportedModes);

			if (SupportedModes.Num() > 0 && !SupportedModes.Contains(this->GetConstructionViewMode()))
			{
				if (!bShouldRestoreSavedConstructionViewMode)
				{
					// remember the current view mode so we can restore it later
					SavedConstructionViewMode = this->GetConstructionViewMode();
					bShouldRestoreSavedConstructionViewMode = true;
				}

				// switch to the preferred view mode for the tool that's about to start
				this->SetConstructionViewMode(SupportedModes[0]);
			}

			// Check if we need to disable wireframe mode before starting tool.
			const bool bCanSetWireframeActive = DataflowToolBuilder->CanSetConstructionViewWireframeActive();
			if (!bCanSetWireframeActive)
			{
				if (!bShouldRestoreConstructionViewWireframe)
				{
					bShouldRestoreConstructionViewWireframe = bConstructionViewWireframe;
				}
				bConstructionViewWireframe = false;
			}

			ActiveToolsContext = ToolsContext;
			ToolsContext->StartTool(ToolIdentifier);
		}),
		FCanExecuteAction::CreateWeakLambda(ToolsContext, [this, ToolIdentifier, ToolsContext]()
		{
			return ShouldToolStartBeAllowed(ToolIdentifier) &&
			ToolsContext->ToolManager->CanActivateTool(EToolSide::Mouse, ToolIdentifier);
		}),
		FIsActionChecked::CreateUObject(ToolsContext, &UEdModeInteractiveToolsContext::IsToolActive, EToolSide::Mouse, ToolIdentifier),
		EUIActionRepeatMode::RepeatDisabled
	);
}

void UDataflowEditorMode::RegisterAddNodeCommand(TSharedPtr<FUICommandInfo> AddNodeCommand, const FName& NewNodeType, TSharedPtr<FUICommandInfo> StartToolCommand)
{
	auto AddNode = [this](const FName& NewNodeType)
	{
		const FName ConnectionType = FManagedArrayCollection::StaticType();
		const FName ConnectionName("Collection");

		UEdGraphNode* const CurrentlySelectedNode = GetSingleSelectedNodeWithOutputType(ConnectionType);
		checkf(CurrentlySelectedNode, TEXT("No node with FManagedArrayCollection output is currently selected in the Dataflow graph"));

		const UEdGraphNode* const NewNode = CreateAndConnectNewNode(NewNodeType, *CurrentlySelectedNode, ConnectionType, ConnectionName);
		verifyf(NewNode, TEXT("Failed to create a new node: %s"), *NewNodeType.ToString());

		StartToolForSelectedNode(NewNode);
	};

	auto CanAddNode = [this](const FName& NewNodeType) -> bool
	{
		const UEdGraphNode* const CurrentlySelectedNode = GetSingleSelectedNodeWithOutputType(FManagedArrayCollection::StaticType());
		return (CurrentlySelectedNode != nullptr);
	};

	const TSharedRef<FUICommandList>& CommandList = Toolkit->GetToolkitCommands();

	CommandList->MapAction(AddNodeCommand,
		FExecuteAction::CreateWeakLambda(this, AddNode, NewNodeType),
		FCanExecuteAction::CreateWeakLambda(this, CanAddNode, NewNodeType)
	);

	NodeTypeToToolCommandMap.Add(NewNodeType, StartToolCommand);
}

void UDataflowEditorMode::RegisterTools()
{
	const FDataflowEditorCommandsImpl& CommandInfos = FDataflowEditorCommands::Get();

	UEditorInteractiveToolsContext* const ConstructionViewportToolsContext = GetInteractiveToolsContext();

	UDataflowEditorWeightMapPaintToolBuilder* WeightMapPaintToolBuilder = NewObject<UDataflowEditorWeightMapPaintToolBuilder>();
	RegisterDataflowTool(CommandInfos.BeginWeightMapPaintTool, FDataflowEditorCommandsImpl::BeginWeightMapPaintToolIdentifier, WeightMapPaintToolBuilder, WeightMapPaintToolBuilder, ConstructionViewportToolsContext);
	RegisterAddNodeCommand(CommandInfos.AddWeightMapNode, FDataflowCollectionAddScalarVertexPropertyNode::StaticType(), CommandInfos.BeginWeightMapPaintTool);

	// @todo(brice) Remove Example Tools
	//RegisterTool(CommandInfos.BeginAttributeEditorTool, FDataflowEditorCommandsImpl::BeginAttributeEditorToolIdentifier, NewObject<UAttributeEditorToolBuilder>());
	//RegisterTool(CommandInfos.BeginMeshSelectionTool, FDataflowEditorCommandsImpl::BeginMeshSelectionToolIdentifier, NewObject<UMeshSelectionToolBuilder>());
	//RegisterTool(CommandInfos.BeginMeshSelectionTool, FDataflowEditorCommandsImpl::BeginMeshSelectionToolIdentifier, NewObject<UMeshVertexPaintToolBuilder>());
	//RegisterTool(CommandInfos.BeginMeshSelectionTool, FDataflowEditorCommandsImpl::BeginMeshSelectionToolIdentifier, NewObject<UMeshAttributePaintToolBuilder>());
}

bool UDataflowEditorMode::ShouldToolStartBeAllowed(const FString& ToolIdentifier) const
{
	// Allow switching away from tool if no changes have been made in the tool yet (which we infer from the CanAccept status)
	if (GetInteractiveToolsContext()->CanAcceptActiveTool())
	{
		return false;
	}

	if (PreviewScene && PreviewScene->GetDataflowModeManager() && PreviewScene->GetDataflowModeManager()->GetInteractiveToolsContext())
	{
		if (PreviewScene->GetDataflowModeManager()->GetInteractiveToolsContext()->HasActiveTool())
		{
			return false;
		}
	}

	return Super::ShouldToolStartBeAllowed(ToolIdentifier);
}

void UDataflowEditorMode::CreateToolkit()
{
	Toolkit = MakeShared<FDataflowEditorModeToolkit>();
}

void UDataflowEditorMode::OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	FDataflowEditorCommandsImpl::UpdateToolCommandBinding(Tool, ToolCommandList, false);
}

void UDataflowEditorMode::OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	FDataflowEditorCommandsImpl::UpdateToolCommandBinding(Tool, ToolCommandList, true);

	if (bShouldRestoreConstructionViewWireframe)
	{
		bConstructionViewWireframe = true;
		bShouldRestoreConstructionViewWireframe = false;
	}

	if (bShouldRestoreSavedConstructionViewMode)
	{
		SetConstructionViewMode(SavedConstructionViewMode);
		bShouldRestoreSavedConstructionViewMode = false;
	}
	else
	{
		PreviewScene->ResetConstructionScene();
	}

	if (TSharedPtr<SDataflowGraphEditor> GraphEditor = DataflowGraphEditor.Pin())
	{
		GraphEditor->SetEnabled(true);
	}
}

void UDataflowEditorMode::BindCommands()
{
	const FDataflowEditorCommandsImpl& CommandInfos = FDataflowEditorCommands::Get();
	const TSharedRef<FUICommandList>& CommandList = Toolkit->GetToolkitCommands();

	// Hook up to Enter/Esc key presses
	CommandList->MapAction(
		CommandInfos.AcceptOrCompleteActiveTool,
		FExecuteAction::CreateLambda([this](){ AcceptActiveToolActionOrTool();}),
		FCanExecuteAction::CreateLambda([this]() {
				return GetInteractiveToolsContext()->CanAcceptActiveTool() || GetInteractiveToolsContext()->CanCompleteActiveTool();
			}),
		FGetActionCheckState(),
		FIsActionButtonVisible(),
		EUIActionRepeatMode::RepeatDisabled);

	CommandList->MapAction(
		CommandInfos.CancelOrCompleteActiveTool,
		FExecuteAction::CreateLambda([this]() { CancelActiveToolActionOrTool(); }),
		FCanExecuteAction::CreateLambda([this]() {
			return GetInteractiveToolsContext()->CanCompleteActiveTool() || GetInteractiveToolsContext()->CanCancelActiveTool();
			}),
		FGetActionCheckState(),
		FIsActionButtonVisible(),
		EUIActionRepeatMode::RepeatDisabled);
}

void UDataflowEditorMode::Exit()
{
	UActorComponent::MarkRenderStateDirtyEvent.RemoveAll(this);

	PreviewScene->ResetConstructionScene();
	PreviewScene = nullptr;

	Super::Exit();
}

void UDataflowEditorMode::SetDataflowConstructionScene(FDataflowConstructionScene* InPreviewScene)
{
	PreviewScene = InPreviewScene;

	UEditorInteractiveToolsContext* const PreviewToolsContext = PreviewScene->GetDataflowModeManager()->GetInteractiveToolsContext();
	UInteractiveToolManager* const PreviewToolManager = PreviewToolsContext->ToolManager;
	//#todo(brice): Make sure AddToolTargetFactories has been called. 
	//PreviewToolsContext->TargetManager->AddTargetFactory(NewObject<UClothComponentToolTargetFactory>(PreviewToolManager));
	//PreviewToolsContext->TargetManager->AddTargetFactory(NewObject<USkeletalMeshComponentToolTargetFactory>(PreviewToolManager));

	PreviewToolManager->OnToolStarted.AddUObject(this, &UDataflowEditorMode::OnToolStarted);
	PreviewToolManager->OnToolEnded.AddUObject(this, &UDataflowEditorMode::OnToolEnded);

	check(Toolkit.IsValid());

	// FBaseToolkit's OnToolStarted and OnToolEnded are protected, so we use the subclass to get at them
	FDataflowEditorModeToolkit* const DataflowModeToolkit = static_cast<FDataflowEditorModeToolkit*>(Toolkit.Get());

	PreviewToolManager->OnToolStarted.AddSP(DataflowModeToolkit, &FDataflowEditorModeToolkit::OnToolStarted);
	PreviewToolManager->OnToolEnded.AddSP(DataflowModeToolkit, &FDataflowEditorModeToolkit::OnToolEnded);
}

void UDataflowEditorMode::CreateToolTargets(const TArray<TObjectPtr<UObject>>& AssetsIn)
{
	ToolTargets.Reset();
	if (TObjectPtr<UDataflowBaseContent> EditorContent = PreviewScene->GetDataflowContent())
	{
		if (UToolTarget* Target = GetInteractiveToolsContext()->TargetManager->BuildTarget(EditorContent, GetToolTargetRequirements()))
		{
			ToolTargets.Add(Target);
		}
	}
}

bool UDataflowEditorMode::IsComponentSelected(const UPrimitiveComponent* InComponent)
{
	if (const FEditorModeTools* const ModeManager = GetModeManager())
	{
		if (const UTypedElementSelectionSet* const TypedElementSelectionSet = ModeManager->GetEditorSelectionSet())
		{
			if (const FTypedElementHandle ComponentElement = UEngineElementsLibrary::AcquireEditorComponentElementHandle(InComponent))
			{
				const bool bElementSelected = TypedElementSelectionSet->IsElementSelected(ComponentElement, FTypedElementIsSelectedOptions());
				return bElementSelected;
			}
		}
	}

	return false;
}

void UDataflowEditorMode::RefocusRestSpaceViewportClient()
{
	TSharedPtr<FDataflowEditorViewportClient, ESPMode::ThreadSafe> PinnedVC = ConstructionViewportClient.Pin();
	if (PinnedVC.IsValid())
	{
		// This will happen in FocusViewportOnBox anyways; do it now to get a consistent end result
		PinnedVC->ToggleOrbitCamera(false);

		const FBox SceneBounds = SceneBoundingBox();
		const bool bPattern2DMode = (ConstructionViewMode == Dataflow::EDataflowPatternVertexType::Sim2D);
		if (bPattern2DMode)
		{
			// 2D pattern
			PinnedVC->SetInitialViewTransform(ELevelViewportType::LVT_Perspective, FVector(0, 0, -100), FRotator(90, -90, 0), DEFAULT_ORTHOZOOM);
		}
		else
		{
			// 3D rest space
			PinnedVC->SetInitialViewTransform(ELevelViewportType::LVT_Perspective, FVector(0, 150, 200), FRotator(0, 0, 0), DEFAULT_ORTHOZOOM);
		}

		constexpr bool bInstant = true;
		PinnedVC->FocusViewportOnBox(SceneBounds, bInstant);

		// Recompute near/far clip planes
		PinnedVC->SetConstructionViewMode(ConstructionViewMode);
	}
}

void UDataflowEditorMode::FirstTimeFocusRestSpaceViewport()
{
	// If this is the first time seeing a valid 2D or 3D mesh, refocus the camera on it.
	const bool bIsValid = (PreviewScene->HasRenderableGeometry());
	const bool bIs2D = ConstructionViewMode == Dataflow::EDataflowPatternVertexType::Sim3D;

	if (bIsValid)
	{
		if (bIs2D && bFirstValid2DMesh)
		{
			bFirstValid2DMesh = false;
			RefocusRestSpaceViewportClient();
		}
		else if (!bIs2D && bFirstValid3DMesh)
		{
			bFirstValid3DMesh = false;
			RefocusRestSpaceViewportClient();
		}
	}
}

void UDataflowEditorMode::InitializeTargets(const TArray<TObjectPtr<UObject>>& ObjectsToEdit)
{
	UBaseCharacterFXEditorMode::InitializeTargets(ObjectsToEdit);

	// @todo(brice) : Consider initializing the Content here from the ObjectsToEdit

	// @todo(brice) : What are the ToolTargets storing?
	// ... for(ToolTarget& : ToolTargets){
	// ... UE::ToolTarget::GetDynamicMeshCopy(Target)
	// ... UE::ToolTarget::GetMaterialSet(Target).Materials for PreviewScene->AddDynamicMeshComponent
	// ... }

	// @todo(michael) : do we need to update the construction scene?
	PreviewScene->UpdateConstructionScene();
}

void UDataflowEditorMode::ModeTick(float DeltaTime)
{
	Super::ModeTick(DeltaTime);

	if (TSharedPtr<SDataflowGraphEditor> GraphEditor = DataflowGraphEditor.Pin())
	{
		// For now don't allow selection change once the tool has uncommitted changes
		// TODO: We might want to auto-accept unsaved changes and allow switching between nodes
		if (GetInteractiveToolsContext()->CanAcceptActiveTool())
		{
			GraphEditor->SetEnabled(false);
		}
		else
		{
			GraphEditor->SetEnabled(true);
		}
	}

	if (!NodeTypeForPendingToolStart.IsNone() && !GetToolManager()->HasActiveTool(EToolSide::Left))
	{
		const TSharedRef<FUICommandList> CommandList = Toolkit->GetToolkitCommands();
		const FDataflowEditorCommandsImpl& CommandInfos = FDataflowEditorCommandsImpl::Get();

		if (const TSharedPtr<const FUICommandInfo>* const Command = NodeTypeToToolCommandMap.Find(NodeTypeForPendingToolStart))
		{
			CommandList->TryExecuteAction(Command->ToSharedRef());
		}

		NodeTypeForPendingToolStart = FName();
	}
}

void UDataflowEditorMode::RestSpaceViewportResized(FViewport* RestspaceViewport, uint32 /*Unused*/)
{
	// We'd like to call RefocusRestSpaceViewportClient() when the viewport is first created, however in Ortho mode the
	// viewport needs to have non-zero size for FocusViewportOnBox() to work properly. So we wait until the viewport is resized here.
	if (bShouldFocusRestSpaceView && RestspaceViewport && RestspaceViewport->GetSizeXY().X > 0 && RestspaceViewport->GetSizeXY().Y > 0)
	{
		RefocusRestSpaceViewportClient();
		bShouldFocusRestSpaceView = false;
	}
}

FBox UDataflowEditorMode::SceneBoundingBox() const
{
	return PreviewScene->GetBoundingBox();
}

FBox UDataflowEditorMode::SelectionBoundingBox() const
{
	// If the selection is on the GetBoundingBox is automatically computing the selection one
	FBox Bounds = PreviewScene->GetBoundingBox();
	if (Bounds.IsValid)
	{
		return Bounds;
	}

	// Nothing selected, return the whole scene
	return SceneBoundingBox();
}

void UDataflowEditorMode::SetConstructionViewMode(Dataflow::EDataflowPatternVertexType InMode)
{
	// We will first check if there is an active tool. If so, we'll shut down the tool and save the results to the Node, then change view modes, then restart the tool again.
	bool bEndedActiveTool = false;
	UInteractiveToolManager* const ToolManager = GetInteractiveToolsContext()->ToolManager;
	checkf(ToolManager, TEXT("No valid ToolManager found for UDataflowEditorMode"));
	if (UInteractiveTool* const ActiveTool = ToolManager->GetActiveTool(EToolSide::Left))
	{
		// avoid switching back to the previous view mode when the tool ends here
		const bool bTempShouldRestoreVal = bShouldRestoreSavedConstructionViewMode;
		bShouldRestoreSavedConstructionViewMode = false;

		ToolManager->PostActiveToolShutdownRequest(ActiveTool, EToolShutdownType::Accept);
		bEndedActiveTool = true;

		// now we can restore the previous view mode the next time the tool ends
		bShouldRestoreSavedConstructionViewMode = bTempShouldRestoreVal;
	}

	ConstructionViewMode = InMode;
	PreviewScene->UpdateConstructionScene();

	const TSharedPtr<FDataflowEditorViewportClient> VC = ConstructionViewportClient.Pin();
	if (VC.IsValid())
	{
		VC->SetConstructionViewMode(ConstructionViewMode);
	}

	// If we are switching to a mode with a valid mesh for the first time, focus the camera on it
	FirstTimeFocusRestSpaceViewport();

	if (bEndedActiveTool)
	{
		// If we ended the active tool in order to change modes, restart it now
		if (const TSharedPtr<const SDataflowGraphEditor> PinnedGraphEditor = DataflowGraphEditor.Pin())
		{
			const FGraphPanelSelectionSet& SelectedNodes = PinnedGraphEditor->GetSelectedNodes();
			if (SelectedNodes.Num() == 1)
			{
				StartToolForSelectedNode(*SelectedNodes.CreateConstIterator());
			}
		}
	}
}

Dataflow::EDataflowPatternVertexType UDataflowEditorMode::GetConstructionViewMode() const
{
	return ConstructionViewMode;
}


bool UDataflowEditorMode::CanChangeConstructionViewModeTo(Dataflow::EDataflowPatternVertexType NewViewMode) const
{
	check(false);

	if (!GetToolManager()->HasActiveTool(EToolSide::Left))
	{
		return true;
	}

	return false;
	/*
	const UInteractiveToolBuilder* const ActiveToolBuilder = GetToolManager()->GetActiveToolBuilder(EToolSide::Left);
	checkf(ActiveToolBuilder, TEXT("No Active Tool Builder found despite having an Active Tool"));

	const IDataflowEditorToolBuilder* const DataflowToolBuilder = Cast<const IDataflowEditorToolBuilder>(ActiveToolBuilder);
	checkf(ClothToolBuilder, TEXT("Cloth Editor has an active Tool Builder that does not implement IDataflowEditorToolBuilder"));

	TArray<Dataflow::EDataflowPatternVertexType> SupportedViewModes;
	ClothToolBuilder->GetSupportedViewModes(SupportedViewModes);
	return SupportedViewModes.Contains(NewViewMode);
	*/
}

void UDataflowEditorMode::ToggleConstructionViewWireframe()
{
	check(false);
	bConstructionViewWireframe = !bConstructionViewWireframe;
	PreviewScene->UpdateConstructionScene();
}

bool UDataflowEditorMode::CanSetConstructionViewWireframeActive() const
{
	if (!GetToolManager()->HasActiveTool(EToolSide::Left))
	{
		return true;
	}

	const UInteractiveToolBuilder* const ActiveToolBuilder = GetToolManager()->GetActiveToolBuilder(EToolSide::Left);
	checkf(ActiveToolBuilder, TEXT("No Active Tool Builder found despite having an Active Tool"));

	const IDataflowEditorToolBuilder* const DataflowToolBuilder = Cast<const IDataflowEditorToolBuilder>(ActiveToolBuilder);
	checkf(DataflowToolBuilder, TEXT("Cloth Editor has an active Tool Builder that does not implement IDataflowEditorToolBuilder"));
	return DataflowToolBuilder->CanSetConstructionViewWireframeActive();
}

void UDataflowEditorMode::SetRestSpaceViewportClient(TWeakPtr<FDataflowEditorViewportClient, ESPMode::ThreadSafe> InViewportClient)
{
	ConstructionViewportClient = InViewportClient;

	TSharedPtr<FDataflowEditorViewportClient> VC = ConstructionViewportClient.Pin();
	if (VC.IsValid())
	{
		VC->SetConstructionViewMode(ConstructionViewMode);
		VC->SetToolCommandList(ToolCommandList);

		if (VC->Viewport)
		{
			VC->Viewport->ViewportResizedEvent.AddUObject(this, &UDataflowEditorMode::RestSpaceViewportResized);
		}
	}
}

void UDataflowEditorMode::InitializeContextObject()
{
	check(PreviewScene);

	if (TObjectPtr<UDataflowBaseContent> DataflowContent = PreviewScene->GetDataflowContent())
	{
		UEditorInteractiveToolsContext* const RestSpaceToolsContext = GetInteractiveToolsContext();

		UDataflowContextObject* ContextObject = RestSpaceToolsContext->ContextObjectStore->FindContext<UDataflowContextObject>();
		if (!ContextObject)
		{
			ContextObject = DataflowContent;
			RestSpaceToolsContext->ContextObjectStore->AddContextObject(ContextObject);
		}

		check(ContextObject);

		ContextObject->SetConstructionViewMode(ConstructionViewMode);
	}
}

void UDataflowEditorMode::DeleteContextObject()
{
	UEditorInteractiveToolsContext* const RestSpaceToolsContext = GetInteractiveToolsContext();
	if (UDataflowContextObject* ContextObject = RestSpaceToolsContext->ContextObjectStore->FindContext<UDataflowContextObject>())
	{
		RestSpaceToolsContext->ContextObjectStore->RemoveContextObject(ContextObject);
	}
}

void UDataflowEditorMode::SetDataflowGraphEditor(TSharedPtr<SDataflowGraphEditor> InGraphEditor)
{
	if (InGraphEditor)
	{
		DataflowGraphEditor = InGraphEditor;
		InitializeContextObject();
	}
	else
	{
		DeleteContextObject();
	}
}

void UDataflowEditorMode::StartToolForSelectedNode(const UObject* SelectedNode)
{
	if (const UDataflowEdNode* const EdNode = Cast<UDataflowEdNode>(SelectedNode))
	{
		if (const TSharedPtr<const FDataflowNode> DataflowNode = EdNode->GetDataflowNode())
		{
			const FName DataflowNodeType = DataflowNode->GetType();
			NodeTypeForPendingToolStart = DataflowNodeType;
		}
	}
}

void UDataflowEditorMode::OnDataflowNodeDeleted(const TSet<UObject*>& DeletedNodes)
{
	UEditorInteractiveToolsContext* const ToolsContext = GetInteractiveToolsContext();
	checkf(ToolsContext, TEXT("No valid ToolsContext found for UDataflowEditorMode"));
	const bool bCanCancel = ToolsContext->CanCancelActiveTool();
	ToolsContext->EndTool(bCanCancel ? EToolShutdownType::Cancel : EToolShutdownType::Completed);
}

UEdGraphNode* UDataflowEditorMode::GetSingleSelectedNodeWithOutputType(const FName& SelectedNodeOutputTypeName) const
{
	const TSharedPtr<const SDataflowGraphEditor> PinnedDataflowGraphEditor = DataflowGraphEditor.Pin();
	if (!PinnedDataflowGraphEditor)
	{
		return nullptr;
	}

	UEdGraphNode* const SelectedNode = PinnedDataflowGraphEditor->GetSingleSelectedNode();
	if (!SelectedNode)
	{
		return nullptr;
	}

	 if (const UDataflowEdNode* const SelectedDataflowEdNode = Cast<UDataflowEdNode>(SelectedNode))
	 {
		 if (TSharedPtr<const FDataflowNode> SelectedDataflowNode = SelectedDataflowEdNode->GetDataflowNode())
		 {
			 for (const FDataflowOutput* const Output : SelectedDataflowNode->GetOutputs())
			 {
				 if (Output->GetType() == SelectedNodeOutputTypeName)
				 {
					 return SelectedNode;
				 }
			 }
		 }
	 }

	return nullptr;
}

UEdGraphNode* UDataflowEditorMode::CreateNewNode(const FName& NewNodeTypeName)
{
	const TSharedPtr<const SDataflowGraphEditor> PinnedDataflowGraphEditor = DataflowGraphEditor.Pin();
	if (!PinnedDataflowGraphEditor)
	{
		return nullptr;
	}

	if (TObjectPtr<UDataflowBaseContent> EditorContent = PreviewScene->GetDataflowContent())
	{
		if (TObjectPtr<UDataflow> DataflowGraph = EditorContent->GetDataflowAsset())
		{
			const TSharedPtr<FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode> NodeAction =
				FAssetSchemaAction_Dataflow_CreateNode_DataflowEdNode::CreateAction(DataflowGraph, NewNodeTypeName);
			constexpr UEdGraphPin* FromPin = nullptr;
			constexpr bool bSelectNewNode = true;
			return NodeAction->PerformAction(DataflowGraph, FromPin, PinnedDataflowGraphEditor->GetPasteLocation(), bSelectNewNode);
		}
	}
	return nullptr;
}


UEdGraphNode* UDataflowEditorMode::CreateAndConnectNewNode(const FName& NewNodeTypeName, UEdGraphNode& UpstreamNode, const FName& ConnectionTypeName, const FName& NewNodeConnectionName)
{
	if (TObjectPtr<UDataflowBaseContent> EditorContent = PreviewScene->GetDataflowContent())
	{
		if (TObjectPtr<UDataflow> DataflowGraph = EditorContent->GetDataflowAsset())
		{
			// First find the specified output of the upstream node, plus any pins it's connected to

			UEdGraphPin* UpstreamNodeOutputPin = nullptr;
			TArray<UEdGraphPin*> ExistingNodeInputPins;

			const UDataflowEdNode* const UpstreamDataflowEdNode = CastChecked<UDataflowEdNode>(&UpstreamNode);
			const TSharedPtr<const FDataflowNode> UpstreamDataflowNode = UpstreamDataflowEdNode->GetDataflowNode();

			for (const FDataflowOutput* const Output : UpstreamDataflowNode->GetOutputs())
			{
				if (Output->GetType() == ConnectionTypeName)
				{
					UpstreamNodeOutputPin = UpstreamDataflowEdNode->FindPin(*Output->GetName().ToString(), EGPD_Output);
					ExistingNodeInputPins = UpstreamNodeOutputPin->LinkedTo;
					break;
				}
			}

			// Add the new node 

			UEdGraphNode* const NewEdNode = CreateNewNode(NewNodeTypeName);
			checkf(NewEdNode, TEXT("Failed to create a new node in the DataflowGraph"));

			UDataflowEdNode* const NewDataflowEdNode = CastChecked<UDataflowEdNode>(NewEdNode);
			const TSharedPtr<FDataflowNode> NewDataflowNode = NewDataflowEdNode->GetDataflowNode();

			// Re-wire the graph

			if (UpstreamNodeOutputPin)
			{
				UEdGraphPin* NewNodeInputPin = nullptr;
				for (const FDataflowInput* const NewNodeInput : NewDataflowNode->GetInputs())
				{
					if (NewNodeInput->GetType() == ConnectionTypeName && NewNodeInput->GetName() == NewNodeConnectionName)
					{
						NewNodeInputPin = NewDataflowEdNode->FindPin(*NewNodeInput->GetName().ToString(), EGPD_Input);
					}
				}

				UEdGraphPin* NewNodeOutputPin = nullptr;
				for (const FDataflowOutput* const NewNodeOutput : NewDataflowNode->GetOutputs())
				{
					if (NewNodeOutput->GetType() == ConnectionTypeName && NewNodeOutput->GetName() == NewNodeConnectionName)
					{
						NewNodeOutputPin = NewDataflowEdNode->FindPin(*NewNodeOutput->GetName().ToString(), EGPD_Output);
						break;
					}
				}

				check(NewNodeInputPin);
				check(NewNodeOutputPin);

				DataflowGraph->GetSchema()->TryCreateConnection(UpstreamNodeOutputPin, NewNodeInputPin);

				for (UEdGraphPin* DownstreamInputPin : ExistingNodeInputPins)
				{
					DataflowGraph->GetSchema()->TryCreateConnection(NewNodeOutputPin, DownstreamInputPin);
				}
			}

			DataflowGraph->NotifyGraphChanged();

			return NewEdNode;

		}
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE

