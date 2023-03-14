// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/ControlRigEditor.h"
#include "Modules/ModuleManager.h"
#include "ControlRigEditorModule.h"
#include "ControlRigBlueprint.h"
#include "SBlueprintEditorToolbar.h"
#include "Editor/ControlRigEditorMode.h"
#include "SKismetInspector.h"
#include "SEnumCombo.h"
#include "Framework/Commands/GenericCommands.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Graph/ControlRigGraph.h"
#include "BlueprintActionDatabase.h"
#include "ControlRigBlueprintCommands.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "IPersonaToolkit.h"
#include "PersonaModule.h"
#include "Editor/ControlRigEditorEditMode.h"
#include "EditMode/ControlRigEditModeSettings.h"
#include "EditorModeManager.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "AnimCustomInstanceHelper.h"
#include "Sequencer/ControlRigLayerInstance.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "IPersonaPreviewScene.h"
#include "AnimationEditorPreviewScene.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "ControlRig.h"
#include "Editor/ControlRigSkeletalMeshComponent.h"
#include "ControlRigObjectBinding.h"
#include "ControlRigBlueprintUtils.h"
#include "EditorViewportClient.h"
#include "AnimationEditorPreviewActor.h"
#include "Misc/MessageDialog.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ControlRigEditorStyle.h"
#include "EditorFontGlyphs.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Editor/SRigHierarchy.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Units/Hierarchy/RigUnit_BoneName.h"
#include "Units/Hierarchy/RigUnit_GetTransform.h"
#include "Units/Hierarchy/RigUnit_SetTransform.h"
#include "Units/Hierarchy/RigUnit_GetRelativeTransform.h"
#include "Units/Hierarchy/RigUnit_SetRelativeTransform.h"
#include "Units/Hierarchy/RigUnit_OffsetTransform.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "Units/Execution/RigUnit_InverseExecution.h"
#include "Units/Hierarchy/RigUnit_GetControlTransform.h"
#include "Units/Hierarchy/RigUnit_SetControlTransform.h"
#include "Units/Hierarchy/RigUnit_ControlChannel.h"
#include "Units/Execution/RigUnit_Collection.h"
#include "Units/Highlevel/Hierarchy/RigUnit_TransformConstraint.h"
#include "Units/Hierarchy/RigUnit_GetControlTransform.h"
#include "Units/Hierarchy/RigUnit_SetCurveValue.h"
#include "Units/Hierarchy/RigUnit_AddBoneTransform.h"
#include "Graph/NodeSpawners/ControlRigUnitNodeSpawner.h"
#include "Graph/ControlRigGraphSchema.h"
#include "ControlRigObjectVersion.h"
#include "EdGraphUtilities.h"
#include "EdGraphNode_Comment.h"
#include "HAL/PlatformApplicationMisc.h"
#include "SNodePanel.h"
#include "SMyBlueprint.h"
#include "SBlueprintEditorSelectedDebugObjectWidget.h"
#include "Exporters/Exporter.h"
#include "UnrealExporter.h"
#include "ControlRigElementDetails.h"
#include "PropertyEditorModule.h"
#include "Settings/ControlRigSettings.h"
#include "Widgets/Docking/SDockTab.h"
#include "BlueprintCompilationManager.h"
#include "AssetEditorModeManager.h"
#include "IPersonaEditorModeManager.h"
#include "BlueprintEditorTabs.h"
#include "RigVMModel/Nodes/RigVMFunctionEntryNode.h"
#include "RigVMModel/Nodes/RigVMFunctionReturnNode.h"
#include "BlueprintActionDatabaseRegistrar.h"
#include "IMessageLogListing.h"
#include "Editor/SControlRigFunctionLocalizationWidget.h"
#include "Editor/SControlRigFunctionBulkEditWidget.h"
#include "Editor/SControlRigBreakLinksWidget.h"
#include "SGraphPanel.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Units/Execution/RigUnit_SequenceExecution.h"
#include "Editor/ControlRigContextMenuContext.h"
#include "Types/ISlateMetaData.h"
#include "ControlRigGraphDetails.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/WatchedPin.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "AssetRegistry/AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "ControlRigEditor"

const FName ControlRigEditorAppName(TEXT("ControlRigEditorApp"));

const FName FControlRigEditorModes::ControlRigEditorMode("Rigging");

bool FControlRigEditor::bAreFunctionReferencesInitialized = false;

namespace ControlRigEditorTabs
{
	const FName DetailsTab(TEXT("DetailsTab"));
// 	const FName ViewportTab(TEXT("Viewport"));
// 	const FName AdvancedPreviewTab(TEXT("AdvancedPreviewTab"));
};

struct FControlRigZoomLevelsContainer : public FZoomLevelsContainer
{
	struct FControlRigZoomLevelEntry
	{
	public:
		FControlRigZoomLevelEntry(float InZoomAmount, const FText& InDisplayText, EGraphRenderingLOD::Type InLOD)
			: DisplayText(FText::Format(NSLOCTEXT("GraphEditor", "Zoom", "Zoom {0}"), InDisplayText))
		, ZoomAmount(InZoomAmount)
		, LOD(InLOD)
		{
		}

	public:
		FText DisplayText;
		float ZoomAmount;
		EGraphRenderingLOD::Type LOD;
	};
	
	FControlRigZoomLevelsContainer()
	{
		ZoomLevels.Reserve(22);
		ZoomLevels.Add(FControlRigZoomLevelEntry(0.025f, FText::FromString(TEXT("-14")), EGraphRenderingLOD::LowestDetail));
		ZoomLevels.Add(FControlRigZoomLevelEntry(0.070f, FText::FromString(TEXT("-13")), EGraphRenderingLOD::LowestDetail));
		ZoomLevels.Add(FControlRigZoomLevelEntry(0.100f, FText::FromString(TEXT("-12")), EGraphRenderingLOD::LowestDetail));
		ZoomLevels.Add(FControlRigZoomLevelEntry(0.125f, FText::FromString(TEXT("-11")), EGraphRenderingLOD::LowestDetail));
		ZoomLevels.Add(FControlRigZoomLevelEntry(0.150f, FText::FromString(TEXT("-10")), EGraphRenderingLOD::LowestDetail));
		ZoomLevels.Add(FControlRigZoomLevelEntry(0.175f, FText::FromString(TEXT("-9")), EGraphRenderingLOD::LowestDetail));
		ZoomLevels.Add(FControlRigZoomLevelEntry(0.200f, FText::FromString(TEXT("-8")), EGraphRenderingLOD::LowestDetail));
		ZoomLevels.Add(FControlRigZoomLevelEntry(0.225f, FText::FromString(TEXT("-7")), EGraphRenderingLOD::LowDetail));
		ZoomLevels.Add(FControlRigZoomLevelEntry(0.250f, FText::FromString(TEXT("-6")), EGraphRenderingLOD::LowDetail));
		ZoomLevels.Add(FControlRigZoomLevelEntry(0.375f, FText::FromString(TEXT("-5")), EGraphRenderingLOD::MediumDetail));
		ZoomLevels.Add(FControlRigZoomLevelEntry(0.500f, FText::FromString(TEXT("-4")), EGraphRenderingLOD::MediumDetail));
		ZoomLevels.Add(FControlRigZoomLevelEntry(0.675f, FText::FromString(TEXT("-3")), EGraphRenderingLOD::MediumDetail));
		ZoomLevels.Add(FControlRigZoomLevelEntry(0.750f, FText::FromString(TEXT("-2")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FControlRigZoomLevelEntry(0.875f, FText::FromString(TEXT("-1")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FControlRigZoomLevelEntry(1.000f, FText::FromString(TEXT("1:1")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FControlRigZoomLevelEntry(1.250f, FText::FromString(TEXT("+1")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FControlRigZoomLevelEntry(1.375f, FText::FromString(TEXT("+2")), EGraphRenderingLOD::DefaultDetail));
		ZoomLevels.Add(FControlRigZoomLevelEntry(1.500f, FText::FromString(TEXT("+3")), EGraphRenderingLOD::FullyZoomedIn));
		ZoomLevels.Add(FControlRigZoomLevelEntry(1.675f, FText::FromString(TEXT("+4")), EGraphRenderingLOD::FullyZoomedIn));
		ZoomLevels.Add(FControlRigZoomLevelEntry(1.750f, FText::FromString(TEXT("+5")), EGraphRenderingLOD::FullyZoomedIn));
		ZoomLevels.Add(FControlRigZoomLevelEntry(1.875f, FText::FromString(TEXT("+6")), EGraphRenderingLOD::FullyZoomedIn));
		ZoomLevels.Add(FControlRigZoomLevelEntry(2.000f, FText::FromString(TEXT("+7")), EGraphRenderingLOD::FullyZoomedIn));
	}

	float GetZoomAmount(int32 InZoomLevel) const override
	{
		checkSlow(ZoomLevels.IsValidIndex(InZoomLevel));
		return ZoomLevels[InZoomLevel].ZoomAmount;
	}

	int32 GetNearestZoomLevel(float InZoomAmount) const override
	{
		for (int32 ZoomLevelIndex=0; ZoomLevelIndex < GetNumZoomLevels(); ++ZoomLevelIndex)
		{
			if (InZoomAmount <= GetZoomAmount(ZoomLevelIndex))
			{
				return ZoomLevelIndex;
			}
		}

		return GetDefaultZoomLevel();
	}
	
	FText GetZoomText(int32 InZoomLevel) const override
	{
		checkSlow(ZoomLevels.IsValidIndex(InZoomLevel));
		return ZoomLevels[InZoomLevel].DisplayText;
	}
	
	int32 GetNumZoomLevels() const override
	{
		return ZoomLevels.Num();
	}
	
	int32 GetDefaultZoomLevel() const override
	{
		return 14;
	}

	EGraphRenderingLOD::Type GetLOD(int32 InZoomLevel) const override
	{
		checkSlow(ZoomLevels.IsValidIndex(InZoomLevel));
		return ZoomLevels[InZoomLevel].LOD;
	}

	TArray<FControlRigZoomLevelEntry> ZoomLevels;
};

const TArray<FName> FControlRigEditor::ForwardsSolveEventQueue = {FRigUnit_BeginExecution::EventName};
const TArray<FName> FControlRigEditor::BackwardsSolveEventQueue = {FRigUnit_InverseExecution::EventName};
const TArray<FName> FControlRigEditor::ConstructionEventQueue = {FRigUnit_PrepareForExecution::EventName};
const TArray<FName> FControlRigEditor::BackwardsAndForwardsSolveEventQueue = {FRigUnit_InverseExecution::EventName, FRigUnit_BeginExecution::EventName};

FControlRigEditor::FControlRigEditor()
	: ControlRig(nullptr)
	, ActiveController(nullptr)
	, bControlRigEditorInitialized(false)
	, bIsSettingObjectBeingDebugged(false)
	, bExecutionControlRig(true)
	, bIsCompilingThroughUI(false)
	, bAnyErrorsLeft(false)
	, LastEventQueue(ConstructionEventQueue)
	, ExecutionMode(EControlRigExecutionModeType::EControlRigExecutionModeType_Release)
	, LastDebuggedRig()
	, RigHierarchyTabCount(0)
	, HaltedAtNode(nullptr)
	, bSuspendDetailsPanelRefresh(false)
	, bIsConstructionEventRunning(false)
	, LastHierarchyHash(INDEX_NONE)
{
}

FControlRigEditor::~FControlRigEditor()
{
	ClearDetailObject();

	UControlRigBlueprint* RigBlueprint = GetControlRigBlueprint();

	ControlRigEditorClosedDelegate.Broadcast(this, RigBlueprint);

	if(PropertyChangedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChangedHandle);
	}

	if (RigBlueprint)
	{
		// clear editor related data from the debugged control rig instance 
		RigBlueprint->SetObjectBeingDebugged(nullptr);

		UControlRigBlueprint::sCurrentlyOpenedRigBlueprints.Remove(RigBlueprint);

		RigBlueprint->OnHierarchyModified().RemoveAll(this);
		if (FControlRigEditMode* EditMode = GetEditMode())
		{
			RigBlueprint->OnHierarchyModified().RemoveAll(EditMode);
		}
		RigBlueprint->OnRefreshEditor().RemoveAll(this);
		RigBlueprint->OnVariableDropped().RemoveAll(this);
		RigBlueprint->OnBreakpointAdded().RemoveAll(this);
		RigBlueprint->OnNodeDoubleClicked().RemoveAll(this);
		RigBlueprint->OnGraphImported().RemoveAll(this);
		RigBlueprint->OnRequestLocalizeFunctionDialog().RemoveAll(this);
		RigBlueprint->OnRequestBulkEditDialog().Unbind();
		RigBlueprint->OnRequestBreakLinksDialog().Unbind();
		RigBlueprint->OnRequestJumpToHyperlink().Unbind();
		RigBlueprint->OnReportCompilerMessage().RemoveAll(this);

#if WITH_EDITOR
		RigBlueprint->SetDebugMode(false);
		RigBlueprint->ClearBreakpoints();
		SetHaltedNode(nullptr);
#endif
	}

	if (UWorld* PreviewWorld = GetPersonaToolkit()->GetPreviewScene()->GetWorld())
	{
		PreviewWorld->MarkObjectsPendingKill();
		PreviewWorld->MarkAsGarbage();
	}

	if (PersonaToolkit.IsValid())
	{
		constexpr bool bSetPreviewMeshInAsset = false;
		PersonaToolkit->SetPreviewMesh(nullptr, bSetPreviewMeshInAsset);
	}
}

UControlRigBlueprint* FControlRigEditor::GetControlRigBlueprint() const
{
	return Cast<UControlRigBlueprint>(GetBlueprintObj());
}

URigHierarchy* FControlRigEditor::GetHierarchyBeingDebugged() const
{
	if(UControlRigBlueprint* RigBlueprint = GetControlRigBlueprint())
	{
		if(UControlRig* RigBeingDebugged = Cast<UControlRig>(RigBlueprint->GetObjectBeingDebugged()))
		{
			return RigBeingDebugged->GetHierarchy();
		}
		return RigBlueprint->Hierarchy;
	}
	return nullptr;
}

void FControlRigEditor::ExtendMenu()
{
	if(MenuExtender.IsValid())
	{
		RemoveMenuExtender(MenuExtender);
		MenuExtender.Reset();
	}

	MenuExtender = MakeShareable(new FExtender);

	AddMenuExtender(MenuExtender);

	// add extensible menu if exists
	FControlRigEditorModule& ControlRigEditorModule = FModuleManager::LoadModuleChecked<FControlRigEditorModule>("ControlRigEditor");
	AddMenuExtender(ControlRigEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

void FControlRigEditor::InitControlRigEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UControlRigBlueprint* InControlRigBlueprint)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	check(InControlRigBlueprint);

	FBlueprintCompilationManager::FlushCompilationQueue(nullptr);

	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");

	FPersonaToolkitArgs PersonaToolkitArgs;
	PersonaToolkitArgs.OnPreviewSceneCreated = FOnPreviewSceneCreated::FDelegate::CreateSP(this, &FControlRigEditor::HandlePreviewSceneCreated);
	PersonaToolkit = PersonaModule.CreatePersonaToolkit(InControlRigBlueprint, PersonaToolkitArgs);

	// set delegate prior to setting mesh
	// otherwise, you don't get delegate
	PersonaToolkit->GetPreviewScene()->RegisterOnPreviewMeshChanged(FOnPreviewMeshChanged::CreateSP(this, &FControlRigEditor::HandlePreviewMeshChanged));

	// Set a default preview mesh, if any
	PersonaToolkit->SetPreviewMesh(InControlRigBlueprint->GetPreviewMesh(), false);

	Toolbox = SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(0.f);

	if (!Toolbar.IsValid())
	{
		Toolbar = MakeShareable(new FBlueprintEditorToolbar(SharedThis(this)));
	}

	// Build up a list of objects being edited in this asset editor
	TArray<UObject*> ObjectsBeingEdited;
	ObjectsBeingEdited.Add(InControlRigBlueprint);

	// Initialize the asset editor and spawn tabs
	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	InitAssetEditor(Mode, InitToolkitHost, ControlRigEditorAppName, FTabManager::FLayout::NullLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsBeingEdited);

	CreateDefaultCommands();

	UControlRigBlueprint::sCurrentlyOpenedRigBlueprints.AddUnique(InControlRigBlueprint);

	TArray<UBlueprint*> ControlRigBlueprints;
	ControlRigBlueprints.Add(InControlRigBlueprint);

	InControlRigBlueprint->InitializeModelIfRequired();

	CommonInitialization(ControlRigBlueprints, false);

	// update function references once
	InitFunctionReferences();
	
	// user-defined-struct can change even after load
	// refresh the models such that pins are updated to match
	// the latest struct member layout
	InControlRigBlueprint->RefreshAllModels(EControlRigBlueprintLoadType::CheckUserDefinedStructs);

	{
		TArray<UEdGraph*> EdGraphs;
		InControlRigBlueprint->GetAllGraphs(EdGraphs);

		for (UEdGraph* Graph : EdGraphs)
		{
			UControlRigGraph* RigGraph = Cast<UControlRigGraph>(Graph);
			if (RigGraph == nullptr)
			{
				continue;
			}

			RigGraph->Initialize(InControlRigBlueprint);
		}

	}

	InControlRigBlueprint->OnModified().AddSP(this, &FControlRigEditor::HandleModifiedEvent);
	InControlRigBlueprint->OnVMCompiled().AddSP(this, &FControlRigEditor::HandleVMCompiledEvent);
	InControlRigBlueprint->OnRequestInspectObject().AddSP(this, &FControlRigEditor::SetDetailObjects);

	BindCommands();

	AddApplicationMode(
		FControlRigEditorModes::ControlRigEditorMode,
		MakeShareable(new FControlRigEditorMode(SharedThis(this))));

	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	// Activate the initial mode (which will populate with a real layout)
	SetCurrentMode(FControlRigEditorModes::ControlRigEditorMode);

	// Activate our edit mode
	GetEditorModeManager().SetDefaultMode(FControlRigEditorEditMode::ModeName);
	GetEditorModeManager().ActivateMode(FControlRigEditorEditMode::ModeName);

	if (FControlRigEditMode* EditMode = GetEditMode())
	{
		EditMode->OnGetRigElementTransform() = FOnGetRigElementTransform::CreateSP(this, &FControlRigEditor::GetRigElementTransform);
		EditMode->OnSetRigElementTransform() = FOnSetRigElementTransform::CreateSP(this, &FControlRigEditor::SetRigElementTransform);
		EditMode->OnGetContextMenu() = FOnGetContextMenu::CreateSP(this, &FControlRigEditor::HandleOnGetViewportContextMenuDelegate);
		EditMode->OnContextMenuCommands() = FNewMenuCommandsDelegate::CreateSP(this, &FControlRigEditor::HandleOnViewportContextMenuCommandsDelegate);
		EditMode->OnAnimSystemInitialized().Add(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FControlRigEditor::OnAnimInitialized));
		
		PersonaToolkit->GetPreviewScene()->SetRemoveAttachedComponentFilter(FOnRemoveAttachedComponentFilter::CreateSP(EditMode, &FControlRigEditMode::CanRemoveFromPreviewScene));
	}

	UpdateControlRig();

	// Post-layout initialization
	PostLayoutBlueprintEditorInitialization();

	// tabs opened before reload
	FString ActiveTabNodePath;
	TArray<FString> OpenedTabNodePaths;

	if (ControlRigBlueprints.Num() > 0)
	{
		bool bBroughtGraphToFront = false;
		for(UEdGraph* Graph : ControlRigBlueprints[0]->UbergraphPages)
		{
			if (UControlRigGraph* RigGraph = Cast<UControlRigGraph>(Graph))
			{
				if (!bBroughtGraphToFront)
				{
					OpenGraphAndBringToFront(Graph, false);
					bBroughtGraphToFront = true;
				}

				RigGraph->OnGraphNodeClicked.AddSP(this, &FControlRigEditor::OnGraphNodeClicked);
				ActiveTabNodePath = RigGraph->ModelNodePath;
			}
		}
	}

	{
		if (URigVMGraph* Model = InControlRigBlueprint->GetDefaultModel())
		{
			if (Model->GetNodes().Num() == 0)
			{
				URigVMNode* Node = InControlRigBlueprint->GetController()->AddUnitNode(FRigUnit_BeginExecution::StaticStruct(), FRigUnit::GetMethodName(), FVector2D::ZeroVector, FString(), false);
				if (Node)
				{
					TArray<FName> NodeNames;
					NodeNames.Add(Node->GetFName());
					InControlRigBlueprint->GetController()->SetNodeSelection(NodeNames, false);
				}
			}
			else
			{
				// remember all ed graphs which were visible as tabs
				TArray<UEdGraph*> EdGraphs;
				InControlRigBlueprint->GetAllGraphs(EdGraphs);

				for (UEdGraph* EdGraph : EdGraphs)
				{
					if (UControlRigGraph* RigGraph = Cast<UControlRigGraph>(EdGraph))
					{
						TArray<TSharedPtr<SDockTab>> TabsForEdGraph;
						FindOpenTabsContainingDocument(EdGraph, TabsForEdGraph);

						if (TabsForEdGraph.Num() > 0)
						{
							OpenedTabNodePaths.Add(RigGraph->ModelNodePath);

							if(RigGraph->bIsFunctionDefinition)
							{
								CloseDocumentTab(RigGraph);
							}
						}
					}
				}

				InControlRigBlueprint->RebuildGraphFromModel();

				// selection state does not need to be persistent, even though it is saved in the RigVM.
				InControlRigBlueprint->GetController()->ClearNodeSelection(false);

				if (UPackage* Package = InControlRigBlueprint->GetOutermost())
				{
					Package->SetDirtyFlag(InControlRigBlueprint->bDirtyDuringLoad);
				}
			}
		}

		// listening to the BP's event instead of BP's Hierarchy's Event ensure a propagation order of
		// 1. Hierarchy change in BP
		// 2. BP propagate to instances
		// 3. Editor forces propagation again, and reflects hierarchy change in either instances or BP
		// 
		// if directly listening to BP's Hierarchy's Event, this ordering is not guaranteed due to multicast,
		// a problematic order we have encountered looks like:
		// 1. Hierarchy change in BP
		// 2. FControlRigEditor::OnHierarchyModified performs propagation from BP to instances, refresh UI
		// 3. BP performs propagation again in UControlRigBlueprint::HandleHierarchyModified, invalidates the rig element
		//    that the UI is observing
		// 4. Editor UI shows an invalid rig element
		InControlRigBlueprint->OnHierarchyModified().AddSP(this, &FControlRigEditor::OnHierarchyModified);
		
		InControlRigBlueprint->OnRefreshEditor().AddSP(this, &FControlRigEditor::HandleRefreshEditorFromBlueprint);
		InControlRigBlueprint->OnVariableDropped().AddSP(this, &FControlRigEditor::HandleVariableDroppedFromBlueprint);
		InControlRigBlueprint->OnBreakpointAdded().AddSP(this, &FControlRigEditor::HandleBreakpointAdded);

		if (FControlRigEditMode* EditMode = GetEditMode())
		{
			InControlRigBlueprint->OnHierarchyModified().AddSP(EditMode, &FControlRigEditMode::OnHierarchyModified_AnyThread);
		}

		InControlRigBlueprint->OnNodeDoubleClicked().AddSP(this, &FControlRigEditor::OnNodeDoubleClicked);
		InControlRigBlueprint->OnGraphImported().AddSP(this, &FControlRigEditor::OnGraphImported);
		InControlRigBlueprint->OnRequestLocalizeFunctionDialog().AddSP(this, &FControlRigEditor::OnRequestLocalizeFunctionDialog);
		InControlRigBlueprint->OnRequestBulkEditDialog().BindSP(this, &FControlRigEditor::OnRequestBulkEditDialog);
		InControlRigBlueprint->OnRequestBreakLinksDialog().BindSP(this, &FControlRigEditor::OnRequestBreakLinksDialog);
		InControlRigBlueprint->OnRequestJumpToHyperlink().BindSP(this, &FControlRigEditor::HandleJumpToHyperlink);
	}

	UpdateStaleWatchedPins();

	for (const FString& OpenedTabNodePath : OpenedTabNodePaths)
	{
		if (UEdGraph* EdGraph = InControlRigBlueprint->GetEdGraph(OpenedTabNodePath))
		{
			OpenDocument(EdGraph, FDocumentTracker::RestorePreviousDocument);
		}
	}

	if (UEdGraph* ActiveGraph = InControlRigBlueprint->GetEdGraph(ActiveTabNodePath))
	{
		OpenGraphAndBringToFront(ActiveGraph, true);
	}

	FControlRigBlueprintUtils::HandleRefreshAllNodes(InControlRigBlueprint);

	bControlRigEditorInitialized = true;

	if (ControlRigBlueprints.Num() > 0)
	{
		if(ControlRigBlueprints[0]->Status == BS_Error)
		{
			Compile();
		}
	}

	FFunctionGraphTask::CreateAndDispatchWhenReady([this]()
	{
		// Always show the myblueprint tab
		FTabId MyBlueprintTabId(FBlueprintEditorTabs::MyBlueprintID);
		if(!GetTabManager()->FindExistingLiveTab(MyBlueprintTabId).IsValid())
		{
			GetTabManager()->TryInvokeTab(MyBlueprintTabId);
		}
		
	}, TStatId(), NULL, ENamedThreads::GameThread);
	
	CreateRigHierarchyToGraphDragAndDropMenu();

#if WITH_EDITOR
	FString BlueprintName = InControlRigBlueprint->GetPathName();
	RigVMPythonUtils::PrintPythonContext(BlueprintName);
#endif

	TArray<UScriptStruct*> StructsToCustomize = {
		TBaseStructure<FVector>::Get(),
		TBaseStructure<FVector2D>::Get(),
		TBaseStructure<FVector4>::Get(),
		TBaseStructure<FRotator>::Get(),
		TBaseStructure<FQuat>::Get(),
		TBaseStructure<FTransform>::Get(),
		TBaseStructure<FEulerTransform>::Get(),
	};

	for(UScriptStruct* StructToCustomize : StructsToCustomize)
	{
		Inspector->GetPropertyView()->RegisterInstancedCustomPropertyTypeLayout(StructToCustomize->GetFName(),
			FOnGetPropertyTypeCustomizationInstance::CreateLambda([=]()
			{
				return FControlRigGraphMathTypeDetails::MakeInstance();
			}));
	}

	PropertyChangedHandle = FCoreUObjectDelegates::OnObjectPropertyChanged.AddSP(this, &FControlRigEditor::OnPropertyChanged);
}

void FControlRigEditor::InitFunctionReferences()
{
	if (bAreFunctionReferencesInitialized)
	{
		return;
	}
	
	FFunctionGraphTask::CreateAndDispatchWhenReady([]()
	{
		if(URigVMBuildData* BuildData = URigVMController::GetBuildData())
		{
			const FArrayProperty* ReferenceNodeDataProperty =
				CastField<FArrayProperty>(UControlRigBlueprint::StaticClass()->FindPropertyByName(TEXT("FunctionReferenceNodeData")));
			if(ReferenceNodeDataProperty)
			{
				const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

				// find all control rigs in the project
				TArray<FAssetData> ControlRigAssetDatas;
				FARFilter ControlRigAssetFilter;
				ControlRigAssetFilter.ClassPaths.Add(UControlRigBlueprint::StaticClass()->GetClassPathName());
				AssetRegistryModule.Get().GetAssets(ControlRigAssetFilter, ControlRigAssetDatas);

				// loop over all control rigs in the project
				for(const FAssetData& ControlRigAssetData : ControlRigAssetDatas)
				{
					const FString ReferenceNodeDataString =
						ControlRigAssetData.GetTagValueRef<FString>(ReferenceNodeDataProperty->GetFName());
					if(ReferenceNodeDataString.IsEmpty())
					{
						continue;
					}

					TArray<FRigVMReferenceNodeData> ReferenceNodeDatas;
					ReferenceNodeDataProperty->ImportText_Direct(*ReferenceNodeDataString, &ReferenceNodeDatas, nullptr, EPropertyPortFlags::PPF_None);

					for(const FRigVMReferenceNodeData& ReferenceNodeData : ReferenceNodeDatas)
					{
						BuildData->RegisterFunctionReference(ReferenceNodeData);
					}
				}
			}
		}
	}, TStatId(), NULL, ENamedThreads::GameThread);
	
	bAreFunctionReferencesInitialized = true;
}

void FControlRigEditor::BindCommands()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	/*
	GetToolkitCommands()->MapAction(
		FControlRigBlueprintCommands::Get().ExecuteGraph,
		FExecuteAction::CreateSP(this, &FControlRigEditor::ToggleExecuteGraph), 
		FCanExecuteAction(), 
		FIsActionChecked::CreateSP(this, &FControlRigEditor::IsExecuteGraphOn));
	*/

	GetToolkitCommands()->MapAction(
		FControlRigBlueprintCommands::Get().AutoCompileGraph,
		FExecuteAction::CreateSP(this, &FControlRigEditor::ToggleAutoCompileGraph), 
		FIsActionChecked::CreateSP(this, &FControlRigEditor::CanAutoCompileGraph),
		FIsActionChecked::CreateSP(this, &FControlRigEditor::IsAutoCompileGraphOn));

	GetToolkitCommands()->MapAction(
		FControlRigBlueprintCommands::Get().ToggleEventQueue,
		FExecuteAction::CreateSP(this, &FControlRigEditor::ToggleEventQueue),
		FCanExecuteAction());

	GetToolkitCommands()->MapAction(
		FControlRigBlueprintCommands::Get().ConstructionEvent,
		FExecuteAction::CreateSP(this, &FControlRigEditor::SetEventQueue, TArray<FName>(ConstructionEventQueue)),
		FCanExecuteAction());

	GetToolkitCommands()->MapAction(
		FControlRigBlueprintCommands::Get().ForwardsSolveEvent,
		FExecuteAction::CreateSP(this, &FControlRigEditor::SetEventQueue, TArray<FName>(ForwardsSolveEventQueue)),
		FCanExecuteAction());

	GetToolkitCommands()->MapAction(
		FControlRigBlueprintCommands::Get().BackwardsSolveEvent,
		FExecuteAction::CreateSP(this, &FControlRigEditor::SetEventQueue, TArray<FName>(BackwardsSolveEventQueue)),
		FCanExecuteAction());

	GetToolkitCommands()->MapAction(
		FControlRigBlueprintCommands::Get().BackwardsAndForwardsSolveEvent,
		FExecuteAction::CreateSP(this, &FControlRigEditor::SetEventQueue, TArray<FName>(BackwardsAndForwardsSolveEventQueue)),
		FCanExecuteAction());

	GetToolkitCommands()->MapAction(
		FControlRigBlueprintCommands::Get().ToggleExecutionMode,
		FExecuteAction::CreateSP(this, &FControlRigEditor::ToggleExecutionMode),
		FCanExecuteAction());

	GetToolkitCommands()->MapAction(
		FControlRigBlueprintCommands::Get().ReleaseMode,
		FExecuteAction::CreateSP(this, &FControlRigEditor::SetExecutionMode, EControlRigExecutionModeType_Release),
		FCanExecuteAction());

	GetToolkitCommands()->MapAction(
		FControlRigBlueprintCommands::Get().DebugMode,
		FExecuteAction::CreateSP(this, &FControlRigEditor::SetExecutionMode, EControlRigExecutionModeType_Debug),
		FCanExecuteAction());

	GetToolkitCommands()->MapAction(
		FControlRigBlueprintCommands::Get().ResumeExecution,
		FExecuteAction::CreateSP(this, &FControlRigEditor::HandleBreakpointActionRequested, ERigVMBreakpointAction::Resume),			
		FIsActionChecked::CreateSP(this, &FControlRigEditor::IsHaltedAtBreakpoint));

	GetToolkitCommands()->MapAction(
		FControlRigBlueprintCommands::Get().ShowCurrentStatement,
		FExecuteAction::CreateSP(this, &FControlRigEditor::HandleShowCurrentStatement),
		FIsActionChecked::CreateSP(this, &FControlRigEditor::IsHaltedAtBreakpoint));

	GetToolkitCommands()->MapAction(
		FControlRigBlueprintCommands::Get().StepOver,
		FExecuteAction::CreateSP(this, &FControlRigEditor::HandleBreakpointActionRequested, ERigVMBreakpointAction::StepOver),
		FIsActionChecked::CreateSP(this, &FControlRigEditor::IsHaltedAtBreakpoint));

	GetToolkitCommands()->MapAction(
		FControlRigBlueprintCommands::Get().StepInto,
		FExecuteAction::CreateSP(this, &FControlRigEditor::HandleBreakpointActionRequested, ERigVMBreakpointAction::StepInto),
		FIsActionChecked::CreateSP(this, &FControlRigEditor::IsHaltedAtBreakpoint));

	GetToolkitCommands()->MapAction(
		FControlRigBlueprintCommands::Get().StepOut,
		FExecuteAction::CreateSP(this, &FControlRigEditor::HandleBreakpointActionRequested, ERigVMBreakpointAction::StepOut),
		FIsActionChecked::CreateSP(this, &FControlRigEditor::IsHaltedAtBreakpoint));

	GetToolkitCommands()->MapAction(
		FControlRigBlueprintCommands::Get().FrameSelection,
		FExecuteAction::CreateSP(this, &FControlRigEditor::FrameSelection),
		FCanExecuteAction());
}

void FControlRigEditor::ToggleExecuteGraph()
{
	if (ControlRig)
	{
		bExecutionControlRig = !bExecutionControlRig;

		// This is required now since we update execution/input flag on update controlrig
		// @fixme: change this to just change flag instead of updating whole control rig
		// I'll do this before first check-in
		UpdateControlRig();
	}
}

bool FControlRigEditor::IsExecuteGraphOn() const
{
	return bExecutionControlRig;
}

void FControlRigEditor::ToggleAutoCompileGraph()
{
	if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
	{
		RigBlueprint->bAutoRecompileVM = !RigBlueprint->bAutoRecompileVM;
		if (RigBlueprint->bAutoRecompileVM)
		{
			RigBlueprint->RequestAutoVMRecompilation();
		}
	}
}

bool FControlRigEditor::IsAutoCompileGraphOn() const
{
	if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
	{
		return RigBlueprint->bAutoRecompileVM;
	}
	return false;
}

void FControlRigEditor::ToggleEventQueue()
{
	SetEventQueue(LastEventQueue);
}

void FControlRigEditor::ToggleExecutionMode()
{
	SetExecutionMode((ExecutionMode == EControlRigExecutionModeType_Debug) ?
		EControlRigExecutionModeType_Release
		: EControlRigExecutionModeType_Debug);
}

TSharedRef<SWidget> FControlRigEditor::GenerateEventQueueMenuContent()
{
	FMenuBuilder MenuBuilder(true, GetToolkitCommands());

	MenuBuilder.BeginSection(TEXT("Events"));
	MenuBuilder.AddMenuEntry(FControlRigBlueprintCommands::Get().ConstructionEvent, TEXT("Setup"), TAttribute<FText>(), TAttribute<FText>(), GetEventQueueIcon(ConstructionEventQueue));
	MenuBuilder.AddMenuEntry(FControlRigBlueprintCommands::Get().ForwardsSolveEvent, TEXT("Forwards Solve"), TAttribute<FText>(), TAttribute<FText>(), GetEventQueueIcon(ForwardsSolveEventQueue));
	MenuBuilder.AddMenuEntry(FControlRigBlueprintCommands::Get().BackwardsSolveEvent, TEXT("Backwards Solve"), TAttribute<FText>(), TAttribute<FText>(), GetEventQueueIcon(BackwardsSolveEventQueue));
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(TEXT("Validation"));
	MenuBuilder.AddMenuEntry(FControlRigBlueprintCommands::Get().BackwardsAndForwardsSolveEvent, TEXT("BackwardsAndForwards"), TAttribute<FText>(), TAttribute<FText>(), GetEventQueueIcon(BackwardsAndForwardsSolveEventQueue));
	MenuBuilder.EndSection();

	if (const UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
	{
		bool bFoundUserDefinedEvent = false;
		const TArray<FName> EntryNames = RigBlueprint->GetRigVMClient()->GetEntryNames();
		for(const FName& EntryName : EntryNames)
		{
			if(UControlRigGraphSchema::IsControlRigDefaultEvent(EntryName))
			{
				continue;
			}

			if(!bFoundUserDefinedEvent)
			{
				MenuBuilder.AddSeparator();
				bFoundUserDefinedEvent = true;
			}

			FString EventNameStr = EntryName.ToString();
			if(!EventNameStr.EndsWith(TEXT("Event")))
			{
				EventNameStr += TEXT(" Event");
			}

			MenuBuilder.AddMenuEntry(
				FText::FromString(EventNameStr),
				FText::FromString(FString::Printf(TEXT("Runs the user defined %s"), *EventNameStr)),
				GetEventQueueIcon({EntryName}),
				FUIAction(
					FExecuteAction::CreateSP(this, &FControlRigEditor::SetEventQueue, TArray<FName>({EntryName})),
					FCanExecuteAction()
				)
			);
		}
	}

	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> FControlRigEditor::GenerateExecutionModeMenuContent()
{
	FMenuBuilder MenuBuilder(true, GetToolkitCommands());

	MenuBuilder.BeginSection(TEXT("Events"));
	MenuBuilder.AddMenuEntry(FControlRigBlueprintCommands::Get().ReleaseMode, TEXT("Release"), TAttribute<FText>(), TAttribute<FText>(), GetExecutionModeIcon(EControlRigExecutionModeType_Release));
	MenuBuilder.AddMenuEntry(FControlRigBlueprintCommands::Get().DebugMode, TEXT("Debug"), TAttribute<FText>(), TAttribute<FText>(), GetExecutionModeIcon(EControlRigExecutionModeType_Debug));
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void FControlRigEditor::ExtendToolbar()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	// If the ToolbarExtender is valid, remove it before rebuilding it
	if(ToolbarExtender.IsValid())
	{
		RemoveToolbarExtender(ToolbarExtender);
		ToolbarExtender.Reset();
	}

	ToolbarExtender = MakeShareable(new FExtender);

	AddToolbarExtender(ToolbarExtender);

	FControlRigEditorModule& ControlRigEditorModule = FModuleManager::LoadModuleChecked<FControlRigEditorModule>("ControlRigEditor");
	AddToolbarExtender(ControlRigEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	TArray<IControlRigEditorModule::FControlRigEditorToolbarExtender> ToolbarExtenderDelegates = ControlRigEditorModule.GetAllControlRigEditorToolbarExtenders();

	for (auto& ToolbarExtenderDelegate : ToolbarExtenderDelegates)
	{
		if(ToolbarExtenderDelegate.IsBound())
		{
			AddToolbarExtender(ToolbarExtenderDelegate.Execute(GetToolkitCommands(), SharedThis(this)));
		}
	}

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateSP(this, &FControlRigEditor::FillToolbar)
	);
}

void FControlRigEditor::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("Toolbar");
	{
		/*
		ToolbarBuilder.AddToolBarButton(FControlRigBlueprintCommands::Get().ExecuteGraph,
			NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ControlRig.ExecuteGraph"));
		*/

		ToolbarBuilder.AddToolBarButton(
			FControlRigBlueprintCommands::Get().ToggleEventQueue,
			NAME_None, 
			TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FControlRigEditor::GetEventQueueLabel)),
			TAttribute<FText>(), 
			TAttribute<FSlateIcon>::Create(TAttribute<FSlateIcon>::FGetter::CreateSP(this, &FControlRigEditor::GetEventQueueIcon))
		);

		FUIAction DefaultAction;
		ToolbarBuilder.AddComboButton(
			DefaultAction,
			FOnGetContent::CreateSP(this, &FControlRigEditor::GenerateEventQueueMenuContent),
			LOCTEXT("EventQueue_Label", "Available Events"),
			LOCTEXT("EventQueue_ToolTip", "Pick between different events / modes for testing the Control Rig"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Recompile"),
			true);

		ToolbarBuilder.AddToolBarButton(FControlRigBlueprintCommands::Get().AutoCompileGraph,
			NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ControlRig.AutoCompileGraph"));

		ToolbarBuilder.AddWidget(SNew(SBlueprintEditorSelectedDebugObjectWidget, SharedThis(this)));

		ToolbarBuilder.AddSeparator();
		ToolbarBuilder.AddToolBarButton(
			FControlRigBlueprintCommands::Get().ToggleExecutionMode,
			NAME_None, 
			TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(this, &FControlRigEditor::GetExecutionModeLabel)),
			TAttribute<FText>(), 
			TAttribute<FSlateIcon>::Create(TAttribute<FSlateIcon>::FGetter::CreateSP(this, &FControlRigEditor::GetExecutionModeIcon))
		);
		
		FUIAction DefaultExecutionMode;
		ToolbarBuilder.AddComboButton(
			DefaultExecutionMode,
			FOnGetContent::CreateSP(this, &FControlRigEditor::GenerateExecutionModeMenuContent),
			LOCTEXT("ExecutionMode_Label", "Execution Modes"),
			LOCTEXT("ExecutionMode_ToolTip", "Pick between different execution modes for testing the Control Rig"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Recompile"),
			true);

		ToolbarBuilder.BeginStyleOverride(FName("Toolbar.BackplateLeftPlay"));
		ToolbarBuilder.AddToolBarButton(FControlRigBlueprintCommands::Get().ResumeExecution,
			NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlayWorld.ResumePlaySession"));

		ToolbarBuilder.BeginStyleOverride(FName("Toolbar.BackplateLeft"));
		ToolbarBuilder.AddToolBarButton(FControlRigBlueprintCommands::Get().ShowCurrentStatement,
			NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlayWorld.ShowCurrentStatement"));

		ToolbarBuilder.BeginStyleOverride(FName("Toolbar.BackplateCenter"));
		ToolbarBuilder.AddToolBarButton(FControlRigBlueprintCommands::Get().StepOver,
			NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlayWorld.StepOver"));
		ToolbarBuilder.AddToolBarButton(FControlRigBlueprintCommands::Get().StepInto,
			NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlayWorld.StepInto"));

		ToolbarBuilder.BeginStyleOverride(FName("Toolbar.BackplateRight"));
		ToolbarBuilder.AddToolBarButton(FControlRigBlueprintCommands::Get().StepOut,
			NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlayWorld.StepOut"));

		ToolbarBuilder.EndStyleOverride();

	}
	ToolbarBuilder.EndSection();
}

TArray<FName> FControlRigEditor::GetEventQueue() const
{
	if (ControlRig)
	{
		return ControlRig->GetEventQueue();
	}

	return ForwardsSolveEventQueue;
}

void FControlRigEditor::SetEventQueue(TArray<FName> InEventQueue)
{
	return SetEventQueue(InEventQueue, false);
}

void FControlRigEditor::SetEventQueue(TArray<FName> InEventQueue, bool bCompile)
{
	if (GetEventQueue() == InEventQueue)
	{
		return;
	}

	LastEventQueue = GetEventQueue();

	SetHaltedNode(nullptr);

	TArray<FRigElementKey> PreviousSelection;
	if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
	{
		if(bCompile)
		{
			if (RigBlueprint->bAutoRecompileVM)
			{
				RigBlueprint->RequestAutoVMRecompilation();
			}
			RigBlueprint->Validator->SetControlRig(ControlRig);
		}
		
		// need to clear selection before remove transient control
		// because active selection will trigger transient control recreation after removal	
		PreviousSelection = GetHierarchyBeingDebugged()->GetSelectedKeys();
		RigBlueprint->GetHierarchyController()->ClearSelection();
		
		// need to copy here since the removal changes the iterator
		if (ControlRig)
		{
			RigBlueprint->ClearTransientControls();
		}
	}

	if (ControlRig)
	{
		if (InEventQueue.Num() > 0)
		{
			ControlRig->SetEventQueue(InEventQueue);

			if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
			{
				RigBlueprint->Validator->SetControlRig(ControlRig);
			}
		}

		// Reset transforms only for construction and forward solve to not inturrupt any animation that might be playing
		if (InEventQueue.Contains(FRigUnit_PrepareForExecution::EventName) ||
			InEventQueue.Contains(FRigUnit_BeginExecution::EventName))
		{
			ControlRig->GetHierarchy()->ResetPoseToInitial(ERigElementType::All);
		}
	}

	if (FControlRigEditMode* EditMode = GetEditMode())
	{
		EditMode->RecreateControlShapeActors(GetHierarchyBeingDebugged()->GetSelectedKeys());

		UControlRigEditModeSettings* Settings = GetMutableDefault<UControlRigEditModeSettings>();
		Settings->bDisplayNulls = IsConstructionModeEnabled();
	}

	if (PreviousSelection.Num() > 0)
	{
		GetHierarchyBeingDebugged()->GetController(true)->SetSelection(PreviousSelection);
		SetDetailViewForRigElements();
	}
}

int32 FControlRigEditor::GetEventQueueComboValue() const
{
	const TArray<FName> EventQueue = GetEventQueue();
	if(EventQueue == ForwardsSolveEventQueue)
	{
		return 0;
	}
	if(EventQueue == ConstructionEventQueue)
	{
		return 1;
	}
	if(EventQueue == BackwardsSolveEventQueue)
	{
		return 2;
	}
	if(EventQueue == BackwardsAndForwardsSolveEventQueue)
	{
		return 3;
	}
	return INDEX_NONE;
}

FText FControlRigEditor::GetEventQueueLabel() const
{
	TArray<FName> EventQueue = GetEventQueue();

	if(EventQueue == ConstructionEventQueue)
	{
		return FRigUnit_PrepareForExecution::StaticStruct()->GetDisplayNameText();
	}
	if(EventQueue == ForwardsSolveEventQueue)
	{
		return FRigUnit_BeginExecution::StaticStruct()->GetDisplayNameText();
	}
	if(EventQueue == BackwardsSolveEventQueue)
	{
		return FRigUnit_InverseExecution::StaticStruct()->GetDisplayNameText();
	}
	if(EventQueue == BackwardsAndForwardsSolveEventQueue)
	{
		return FText::FromString(FString::Printf(TEXT("%s and %s"),
			*FRigUnit_InverseExecution::StaticStruct()->GetDisplayNameText().ToString(),
			*FRigUnit_BeginExecution::StaticStruct()->GetDisplayNameText().ToString()));
	}

	if(EventQueue.Num() == 1)
	{
		FString EventName = EventQueue[0].ToString();
		if(!EventName.EndsWith(TEXT("Event")))
		{
			EventName += TEXT(" Event");
		}
		return FText::FromString(EventName);
	}
	return LOCTEXT("CustomEventQueue", "Custom Event Queue");
}

FSlateIcon FControlRigEditor::GetEventQueueIcon(const TArray<FName>& InEventQueue)
{
	if(InEventQueue == ConstructionEventQueue)
	{
		return FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ControlRig.ConstructionMode");
	}
	if(InEventQueue == ForwardsSolveEventQueue)
	{
		return FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ControlRig.ForwardsSolveEvent");
	}
	if(InEventQueue == BackwardsSolveEventQueue)
	{
		return FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ControlRig.BackwardsSolveEvent");
	}
	if(InEventQueue == BackwardsAndForwardsSolveEventQueue)
	{
		return FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ControlRig.BackwardsAndForwardsSolveEvent");
	}

	return FSlateIcon(FAppStyle::GetAppStyleSetName(), "GraphEditor.Event_16x");
}

FSlateIcon FControlRigEditor::GetEventQueueIcon() const
{
	return GetEventQueueIcon(GetEventQueue());
}

void FControlRigEditor::SetExecutionMode(const EControlRigExecutionModeType InExecutionMode)
{
	if (ExecutionMode == InExecutionMode)
	{
		return;
	}

	ExecutionMode = InExecutionMode;
	GetControlRigBlueprint()->SetDebugMode(InExecutionMode == EControlRigExecutionModeType_Debug);
	Compile();
	
	if (ControlRig)
	{
		ControlRig->bIsInDebugMode = InExecutionMode == EControlRigExecutionModeType_Debug;
	}

	SetHaltedNode(nullptr);
	
	RefreshDetailView();
}

int32 FControlRigEditor::GetExecutionModeComboValue() const
{
	return (int32) ExecutionMode;
}

FText FControlRigEditor::GetExecutionModeLabel() const
{
	if (ExecutionMode == EControlRigExecutionModeType_Debug)
	{
		return FText::FromString(TEXT("DebugMode"));
	}
	return FText::FromString(TEXT("ReleaseMode"));
}

FSlateIcon FControlRigEditor::GetExecutionModeIcon(EControlRigExecutionModeType InExecutionMode)
{
	if (InExecutionMode == EControlRigExecutionModeType_Debug)
	{
		return FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Debug");
	}
	return FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ControlRig.ReleaseMode");
}

FSlateIcon FControlRigEditor::GetExecutionModeIcon() const
{
	return GetExecutionModeIcon(ExecutionMode);
}

void FControlRigEditor::GetCustomDebugObjects(TArray<FCustomDebugObject>& DebugList) const
{
	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	if (RigBlueprint == nullptr)
	{
		return;
	}

	if (ControlRig)
	{
		FCustomDebugObject DebugObject;
		DebugObject.Object = ControlRig;
		DebugObject.NameOverride = GetCustomDebugObjectLabel(ControlRig);
		DebugList.Add(DebugObject);
	}

	UControlRigBlueprintGeneratedClass* GeneratedClass = RigBlueprint->GetControlRigBlueprintGeneratedClass();
	if (GeneratedClass)
	{
		struct Local
		{
			static bool IsPendingKillOrUnreachableRecursive(UObject* InObject)
			{
				if (InObject != nullptr)
				{
					if (!IsValidChecked(InObject) || InObject->IsUnreachable())
					{
						return true;
					}
					return IsPendingKillOrUnreachableRecursive(InObject->GetOuter());
				}
				return false;
			}

			static bool OuterNameContainsRecursive(UObject* InObject, const FString& InStringToSearch)
			{
				if (InObject == nullptr)
				{
					return false;
				}

				UObject* InObjectOuter = InObject->GetOuter();
				if (InObjectOuter == nullptr)
				{
					return false;
				}

				if (InObjectOuter->GetName().Contains(InStringToSearch))
				{
					return true;
				}

				return OuterNameContainsRecursive(InObjectOuter, InStringToSearch);
			}
		};

		if (UObject* DefaultObject = GeneratedClass->GetDefaultObject(false))
		{
			TArray<UObject*> ArchetypeInstances;
			DefaultObject->GetArchetypeInstances(ArchetypeInstances);

			for (UObject* Instance : ArchetypeInstances)
			{
				UControlRig* InstanceControlRig = Cast<UControlRig>(Instance);
				if (InstanceControlRig && InstanceControlRig != ControlRig)
				{
					if (InstanceControlRig->GetOuter() == nullptr)
					{
						continue;
					}

					UWorld* World = InstanceControlRig->GetWorld();
					if (World == nullptr)
					{
						continue;
					}

					// ensure to only allow preview actors in preview worlds
					if (World->IsPreviewWorld())
					{
						if (!Local::OuterNameContainsRecursive(InstanceControlRig, TEXT("Preview")))
						{
							continue;
						}
					}

					if (Local::IsPendingKillOrUnreachableRecursive(InstanceControlRig))
					{
						continue;
					}

					FCustomDebugObject DebugObject;
					DebugObject.Object = InstanceControlRig;
					DebugObject.NameOverride = GetCustomDebugObjectLabel(InstanceControlRig);
					DebugList.Add(DebugObject);
				}
			}
		}
	}
}

void FControlRigEditor::HandleSetObjectBeingDebugged(UObject* InObject)
{
	UControlRig* DebuggedControlRig = Cast<UControlRig>(InObject);

	if (DebuggedControlRig == nullptr)
	{
		// fall back to our default control rig (which still can be nullptr)
		if (ControlRig != nullptr && GetBlueprintObj() && !bIsSettingObjectBeingDebugged)
		{
			TGuardValue<bool> GuardSettingObjectBeingDebugged(bIsSettingObjectBeingDebugged, true);
			GetBlueprintObj()->SetObjectBeingDebugged(ControlRig);
			return;
		}
	}

	if(UControlRig* PreviouslyDebuggedControlRig = Cast<UControlRig>(GetBlueprintObj()->GetObjectBeingDebugged()))
	{
		if(!PreviouslyDebuggedControlRig->HasAnyFlags(RF_BeginDestroyed))
		{
			PreviouslyDebuggedControlRig->GetHierarchy()->OnModified().RemoveAll(this);
			PreviouslyDebuggedControlRig->OnPreConstructionForUI_AnyThread().RemoveAll(this);
			PreviouslyDebuggedControlRig->OnPostConstruction_AnyThread().RemoveAll(this);
		}
	}

	if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
	{
		if (UControlRigBlueprintGeneratedClass* GeneratedClass = RigBlueprint->GetControlRigBlueprintGeneratedClass())
		{
			UControlRig* CDO = Cast<UControlRig>(GeneratedClass->GetDefaultObject(true /* create if needed */));
			if (CDO->VM->GetInstructions().Num() <= 1 /* only exit */)
			{
				RigBlueprint->RecompileVM();
				RigBlueprint->RequestControlRigInit();
			}
		}

		RigBlueprint->Validator->SetControlRig(DebuggedControlRig);
	}

	if (DebuggedControlRig)
	{
		bool bIsExternalControlRig = DebuggedControlRig != ControlRig;
		bool bShouldExecute = (!bIsExternalControlRig) && bExecutionControlRig;
		DebuggedControlRig->ControlRigLog = &ControlRigLog;
		GetControlRigBlueprint()->Hierarchy->HierarchyForSelectionPtr = DebuggedControlRig->DynamicHierarchy;

		UControlRigSkeletalMeshComponent* EditorSkelComp = Cast<UControlRigSkeletalMeshComponent>(GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent());
		if (EditorSkelComp)
		{
			UControlRigLayerInstance* AnimInstance = Cast<UControlRigLayerInstance>(EditorSkelComp->GetAnimInstance());
			if (AnimInstance)
			{
				FControlRigIOSettings IOSettings = FControlRigIOSettings::MakeEnabled();
				IOSettings.bUpdatePose = bShouldExecute;
				IOSettings.bUpdateCurves = bShouldExecute;

				// we might want to move this into another method
				FInputBlendPose Filter;
				AnimInstance->ResetControlRigTracks();
				AnimInstance->AddControlRigTrack(0, DebuggedControlRig);
				AnimInstance->UpdateControlRigTrack(0, 1.0f, IOSettings, bShouldExecute);
				AnimInstance->RecalcRequiredBones();

				// since rig has changed, rebuild draw skeleton
				EditorSkelComp->SetControlRigBeingDebugged(DebuggedControlRig);

				if (FControlRigEditMode* EditMode = GetEditMode())
				{
					EditMode->SetObjects(DebuggedControlRig, EditorSkelComp,nullptr);
				}
			}
			
			// get the bone intial transforms from the preview skeletal mesh
			DebuggedControlRig->SetBoneInitialTransformsFromSkeletalMeshComponent(EditorSkelComp);
			if(UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
			{
				// copy the initial transforms back to the blueprint
				// no need to call modify here since this code only modifies the bp if the preview mesh changed
				RigBlueprint->Hierarchy->CopyPose(DebuggedControlRig->GetHierarchy(), false, true, false);
			}
		}

		DebuggedControlRig->GetHierarchy()->OnModified().AddSP(this, &FControlRigEditor::OnHierarchyModified_AnyThread);
		DebuggedControlRig->OnPreConstructionForUI_AnyThread().AddSP(this, &FControlRigEditor::OnPreConstruction_AnyThread);
		DebuggedControlRig->OnPostConstruction_AnyThread().AddSP(this, &FControlRigEditor::OnPostConstruction_AnyThread);
		LastHierarchyHash = INDEX_NONE;

		if(EditorSkelComp)
		{
			EditorSkelComp->SetComponentToWorld(FTransform::Identity);
		}
	}
	else
	{
		if (FControlRigEditMode* EditMode = GetEditMode())
		{
			EditMode->SetObjects(nullptr,  nullptr,nullptr);
		}
	}

	RefreshDetailView();
	LastDebuggedRig = GetCustomDebugObjectLabel(DebuggedControlRig);
}

FString FControlRigEditor::GetCustomDebugObjectLabel(UObject* ObjectBeingDebugged) const
{
	if (ObjectBeingDebugged == nullptr)
	{
		return FString();
	}

	if (ObjectBeingDebugged == ControlRig)
	{
		return TEXT("Control Rig Editor Preview");
	}

	if (AActor* ParentActor = ObjectBeingDebugged->GetTypedOuter<AActor>())
	{
		return FString::Printf(TEXT("%s in %s"), *GetBlueprintObj()->GetName(), *ParentActor->GetName());
	}

	return GetBlueprintObj()->GetName();
}

UBlueprint* FControlRigEditor::GetBlueprintObj() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	for (UObject* Obj : EditingObjs)
	{
		if (Obj->IsA<UControlRigBlueprint>()) 
		{
			return (UBlueprint*)Obj;
		}
	}
	return nullptr;
}

TSubclassOf<UEdGraphSchema> FControlRigEditor::GetDefaultSchemaClass() const
{
	return UControlRigGraphSchema::StaticClass();
}

class FMemoryTypeMetaData : public ISlateMetaData
{
public:
	SLATE_METADATA_TYPE(FMemoryTypeMetaData, ISlateMetaData)

		FMemoryTypeMetaData(ERigVMMemoryType InMemoryType)
		: MemoryType(InMemoryType)
	{
	}
	ERigVMMemoryType MemoryType;
};

void FControlRigEditor::SetDetailObjects(const TArray<UObject*>& InObjects)
{
	SetDetailObjects(InObjects, true);
}

void FControlRigEditor::SetDetailObjects(const TArray<UObject*>& InObjects, bool bChangeUISelectionState)
{
	if(bSuspendDetailsPanelRefresh)
	{
		return;
	}

	if(InObjects.Num() == 1)
	{
		if(URigVMMemoryStorage* Memory = Cast<URigVMMemoryStorage>(InObjects[0]))
		{
			FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

			FDetailsViewArgs DetailsViewArgs;
			DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
			DetailsViewArgs.bHideSelectionTip = true;

			TSharedRef<IDetailsView> DetailsView = EditModule.CreateDetailView( DetailsViewArgs );
			TSharedRef<SDockTab> DockTab = SNew(SDockTab)
			.Label( LOCTEXT("ControlRigMemoryDetails", "Control Rig Memory Details") )
			.AddMetaData<FMemoryTypeMetaData>(FMemoryTypeMetaData(Memory->GetMemoryType()))
			.TabRole(ETabRole::NomadTab)
			[
				DetailsView
			];

			FName TabId = *FString::Printf(TEXT("ControlRigMemoryDetails_%d"), (int32)Memory->GetMemoryType());
			if(TSharedPtr<SDockTab> ActiveTab = GetTabManager()->FindExistingLiveTab(TabId))
			{
				ActiveTab->RequestCloseTab();
			}

			GetTabManager()->InsertNewDocumentTab(
				FBlueprintEditorTabs::DetailsID,
				TabId,
				FTabManager::FLastMajorOrNomadTab(TEXT("ControlRigMemoryDetails")),
				DockTab
			);

			FFunctionGraphTask::CreateAndDispatchWhenReady([DetailsView, InObjects]()
			{
				
				DetailsView->SetObject(InObjects[0]);
				
			}, TStatId(), NULL, ENamedThreads::GameThread);

			return;
		}
	}

	ClearDetailObject(bChangeUISelectionState);

	if (InObjects.Num() == 1)
	{
		if (InObjects[0]->GetClass()->GetDefaultObject() == InObjects[0])
		{
			EditClassDefaults_Clicked();
			return;
		}
		else if (InObjects[0] == GetBlueprintObj())
		{
			EditGlobalOptions_Clicked();
			return;
		}
	}

	TArray<UObject*> FilteredObjects;

	TArray<URigVMNode*> ModelNodes;
	for(UObject* InObject : InObjects)
	{
		if (URigVMNode* ModelNode = Cast<URigVMNode>(InObject))
		{
			ModelNodes.Add(ModelNode);
		}
	}
	
	for(UObject* InObject : InObjects)
	{
		if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(InObject))
		{
			if(!LibraryNode->IsA<URigVMFunctionReferenceNode>())
			{
				if (UEdGraph* EdGraph = GetControlRigBlueprint()->GetEdGraph(LibraryNode->GetContainedGraph()))
				{
					FilteredObjects.AddUnique(EdGraph);
					continue;
				}
			}
		}
		else if (Cast<URigVMFunctionEntryNode>(InObject) || Cast<URigVMFunctionReturnNode>(InObject))
		{
			if (UEdGraph* EdGraph = GetControlRigBlueprint()->GetEdGraph(CastChecked<URigVMNode>(InObject)->GetGraph()))
			{
				FilteredObjects.AddUnique(EdGraph);
				continue;
			}
		}
		else if (URigVMCommentNode* CommentNode = Cast<URigVMCommentNode>(InObject))
		{
			if (UControlRigGraph* EdGraph = Cast<UControlRigGraph>(GetControlRigBlueprint()->GetEdGraph(CastChecked<URigVMNode>(InObject)->GetGraph())))
			{
				if(UEdGraphNode* EdGraphNode = EdGraph->FindNodeForModelNodeName(CommentNode->GetFName()))
				{
					FilteredObjects.AddUnique(EdGraphNode);
					continue;
				}
			}
		}

		if (URigVMNode* ModelNode = Cast<URigVMNode>(InObject))
		{
			// check if we know the dynamic class already
			bool bClassCreated = UDetailsViewWrapperObject::GetClassForNodes(ModelNodes, false) == nullptr;

			// create the wrapper object
			if(UDetailsViewWrapperObject* WrapperObject = UDetailsViewWrapperObject::MakeInstance(ModelNodes, ModelNode, ModelNode))
			{
				WrapperObject->GetWrappedPropertyChangedChainEvent().AddSP(this, &FControlRigEditor::OnWrappedPropertyChangedChainEvent);
				WrapperObject->AddToRoot();

				if(bClassCreated)
				{
					UClass* WrapperClass = UDetailsViewWrapperObject::GetClassForNodes(ModelNodes, false);
					check(WrapperClass);

					FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
					EditModule.RegisterCustomClassLayout(WrapperClass->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FControlRigWrappedNodeDetails::MakeInstance));
				}

				// todo: use transform widget for transforms
				// todo: use rotation widget for rotations
				
				FilteredObjects.Add(WrapperObject);
				continue;
			}
		}


		FilteredObjects.Add(InObject);
	}

	for(UObject* FilteredObject : FilteredObjects)
	{
		if(UDetailsViewWrapperObject* WrapperObject = Cast<UDetailsViewWrapperObject>(FilteredObject))
		{
			WrapperObjects.Add(TStrongObjectPtr<UDetailsViewWrapperObject>(WrapperObject));
		}
	}

	SKismetInspector::FShowDetailsOptions Options;
	Options.bForceRefresh = true;
	Inspector->ShowDetailsForObjects(FilteredObjects, Options);
}

void FControlRigEditor::SetDetailViewForRigElements()
{
	if(bSuspendDetailsPanelRefresh)
	{
		return;
	}

	ClearDetailObject();

	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	URigHierarchy* HierarchyBeingDebugged = GetHierarchyBeingDebugged();
	TArray<UObject*> Objects;

	TArray<FRigElementKey> CurrentSelection = HierarchyBeingDebugged->GetSelectedKeys();
	for(const FRigElementKey& SelectedKey : CurrentSelection)
	{
		FRigBaseElement* Element = HierarchyBeingDebugged->Find(SelectedKey);
		if (Element == nullptr)
		{
			continue;
		}

		UDetailsViewWrapperObject* WrapperObject = UDetailsViewWrapperObject::MakeInstance(Element->GetElementStruct(), (uint8*)Element, HierarchyBeingDebugged);
		WrapperObject->GetWrappedPropertyChangedChainEvent().AddSP(this, &FControlRigEditor::OnWrappedPropertyChangedChainEvent);
		WrapperObject->AddToRoot();

		Objects.Add(WrapperObject);
	}
	
	SetDetailObjects(Objects);
}

void FControlRigEditor::SetDetailViewForGraph(URigVMGraph* InGraph)
{
	check(InGraph);
	
	if(bSuspendDetailsPanelRefresh)
	{
		return;
	}

	ClearDetailObject();

	TArray<UObject*> SelectedNodes;
	TArray<FName> SelectedNodeNames = InGraph->GetSelectNodes();
	for(FName SelectedNodeName : SelectedNodeNames)
	{
		if(URigVMNode* Node = InGraph->FindNodeByName(SelectedNodeName))
		{
			SelectedNodes.Add(Node);
		}
	}

	SetDetailObjects(SelectedNodes);
}

void FControlRigEditor::SetDetailViewForFocusedGraph()
{
	if(bSuspendDetailsPanelRefresh)
	{
		return;
	}

	URigVMGraph* Model = GetFocusedModel();
	if(Model == nullptr)
	{
		return;
	}

	SetDetailViewForGraph(Model);
}

void FControlRigEditor::SetDetailViewForLocalVariable()
{
	FName VariableName;
	TArray< TWeakObjectPtr<UObject> > SelectedObjects = Inspector->GetSelectedObjects();
	for (TWeakObjectPtr<UObject> SelectedObject : SelectedObjects)
	{
		if (SelectedObject.IsValid())
		{
			if(UDetailsViewWrapperObject* WrapperObject = Cast<UDetailsViewWrapperObject>(SelectedObject.Get()))
			{
				VariableName = WrapperObject->GetContent<FRigVMGraphVariableDescription>().Name;
				break;
			}
		}
	}
		
	SelectLocalVariable(GetFocusedGraph(), VariableName);
}

void FControlRigEditor::RefreshDetailView()
{
	if(DetailViewShowsAnyRigElement())
	{
		SetDetailViewForRigElements();
	}
	else if(DetailViewShowsAnyRigUnit())
	{
		SetDetailViewForFocusedGraph();
	}
	else if(DetailViewShowsLocalVariable())
	{
		SetDetailViewForLocalVariable();	
	}
}

bool FControlRigEditor::DetailViewShowsAnyRigElement() const
{
	return DetailViewShowsStruct(FRigBaseElement::StaticStruct());
}

bool FControlRigEditor::DetailViewShowsAnyRigUnit() const
{
	if (DetailViewShowsStruct(FRigUnit::StaticStruct()))
	{
		return true;
	}

	const TArray< TWeakObjectPtr<UObject> >& SelectedObjects = Inspector->GetSelectedObjects();
	for (TWeakObjectPtr<UObject> SelectedObject : SelectedObjects)
	{
		if (SelectedObject.IsValid())
		{
			if(UDetailsViewWrapperObject* WrapperObject = Cast<UDetailsViewWrapperObject>(SelectedObject.Get()))
			{
				const FString Notation = WrapperObject->GetWrappedNodeNotation();
				if(!Notation.IsEmpty())
				{
					return true;
				}
			}
		}
	}
	
	return false;
}

bool FControlRigEditor::DetailViewShowsLocalVariable() const
{
	return DetailViewShowsStruct(FRigVMGraphVariableDescription::StaticStruct());
}

bool FControlRigEditor::DetailViewShowsStruct(UScriptStruct* InStruct) const
{
	TArray< TWeakObjectPtr<UObject> > SelectedObjects = Inspector->GetSelectedObjects();
	for (TWeakObjectPtr<UObject> SelectedObject : SelectedObjects)
	{
		if (SelectedObject.IsValid())
		{
			if(UDetailsViewWrapperObject* WrapperObject = Cast<UDetailsViewWrapperObject>(SelectedObject.Get()))
			{
				if(const UScriptStruct* WrappedStruct = WrapperObject->GetWrappedStruct())
				{
					if (WrappedStruct->IsChildOf(InStruct))
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool FControlRigEditor::DetailViewShowsRigElement(FRigElementKey InKey) const
{
	TArray< TWeakObjectPtr<UObject> > SelectedObjects = Inspector->GetSelectedObjects();
	for (TWeakObjectPtr<UObject> SelectedObject : SelectedObjects)
	{
		if (SelectedObject.IsValid())
		{
			if(UDetailsViewWrapperObject* WrapperObject = Cast<UDetailsViewWrapperObject>(SelectedObject.Get()))
			{
				if (const UScriptStruct* WrappedStruct = WrapperObject->GetWrappedStruct())
				{
					if (WrappedStruct->IsChildOf(FRigBaseElement::StaticStruct()))
					{
						if(WrapperObject->GetContent<FRigBaseElement>().GetKey() == InKey)
						{
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

void FControlRigEditor::ClearDetailObject(bool bChangeUISelectionState)
{
	if(bSuspendDetailsPanelRefresh)
	{
		return;
	}

	for(const TStrongObjectPtr<UDetailsViewWrapperObject>& WrapperObjectPtr : WrapperObjects)
	{
		if(WrapperObjectPtr.IsValid())
		{
			UDetailsViewWrapperObject* WrapperObject = WrapperObjectPtr.Get();
			WrapperObject->RemoveFromRoot();
			WrapperObject->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional);
			WrapperObject->MarkAsGarbage();
		}
	}
	WrapperObjects.Reset();
	
	Inspector->GetPropertyView()->SetObjects(TArray<UObject*>(), true); // clear property view synchronously
	Inspector->ShowDetailsForObjects(TArray<UObject*>());
	Inspector->ShowSingleStruct(TSharedPtr<FStructOnScope>());

	if (bChangeUISelectionState)
	{
		SetUISelectionState(FBlueprintEditor::SelectionState_Graph);
	}
}


void FControlRigEditor::CreateDefaultCommands() 
{
	if (GetBlueprintObj())
	{
		FBlueprintEditor::CreateDefaultCommands();
	}
	else
	{
		ToolkitCommands->MapAction( FGenericCommands::Get().Undo, 
			FExecuteAction::CreateSP( this, &FControlRigEditor::UndoAction ));
		ToolkitCommands->MapAction( FGenericCommands::Get().Redo, 
			FExecuteAction::CreateSP( this, &FControlRigEditor::RedoAction ));
	}
}

void FControlRigEditor::OnCreateGraphEditorCommands(TSharedPtr<FUICommandList> GraphEditorCommandsList)
{
	GraphEditorCommandsList->MapAction(
		FControlRigBlueprintCommands::Get().StoreNodeSnippet1,
		FExecuteAction::CreateSP(this, &FControlRigEditor::StoreNodeSnippet, 1),
		FCanExecuteAction());

	GraphEditorCommandsList->MapAction(
		FControlRigBlueprintCommands::Get().StoreNodeSnippet2,
		FExecuteAction::CreateSP(this, &FControlRigEditor::StoreNodeSnippet, 2),
		FCanExecuteAction());

	GraphEditorCommandsList->MapAction(
		FControlRigBlueprintCommands::Get().StoreNodeSnippet3,
		FExecuteAction::CreateSP(this, &FControlRigEditor::StoreNodeSnippet, 3),
		FCanExecuteAction());

	GraphEditorCommandsList->MapAction(
		FControlRigBlueprintCommands::Get().StoreNodeSnippet4,
		FExecuteAction::CreateSP(this, &FControlRigEditor::StoreNodeSnippet, 4),
		FCanExecuteAction());

	GraphEditorCommandsList->MapAction(
		FControlRigBlueprintCommands::Get().StoreNodeSnippet5,
		FExecuteAction::CreateSP(this, &FControlRigEditor::StoreNodeSnippet, 5),
		FCanExecuteAction());

	GraphEditorCommandsList->MapAction(
		FControlRigBlueprintCommands::Get().StoreNodeSnippet6,
		FExecuteAction::CreateSP(this, &FControlRigEditor::StoreNodeSnippet, 6),
		FCanExecuteAction());

	GraphEditorCommandsList->MapAction(
		FControlRigBlueprintCommands::Get().StoreNodeSnippet7,
		FExecuteAction::CreateSP(this, &FControlRigEditor::StoreNodeSnippet, 7),
		FCanExecuteAction());

	GraphEditorCommandsList->MapAction(
		FControlRigBlueprintCommands::Get().StoreNodeSnippet8,
		FExecuteAction::CreateSP(this, &FControlRigEditor::StoreNodeSnippet, 8),
		FCanExecuteAction());

	GraphEditorCommandsList->MapAction(
		FControlRigBlueprintCommands::Get().StoreNodeSnippet9,
		FExecuteAction::CreateSP(this, &FControlRigEditor::StoreNodeSnippet, 9),
		FCanExecuteAction());

	GraphEditorCommandsList->MapAction(
		FControlRigBlueprintCommands::Get().StoreNodeSnippet0,
		FExecuteAction::CreateSP(this, &FControlRigEditor::StoreNodeSnippet, 0),
		FCanExecuteAction());

}

TSharedRef<SGraphEditor> FControlRigEditor::CreateGraphEditorWidget(TSharedRef<FTabInfo> InTabInfo, UEdGraph* InGraph)
{
	TSharedRef<SGraphEditor> GraphEditor = FBlueprintEditor::CreateGraphEditorWidget(InTabInfo, InGraph);
	GraphEditor->GetGraphPanel()->SetZoomLevelsContainer<FControlRigZoomLevelsContainer>();
	return GraphEditor;
}

void FControlRigEditor::Compile()
{
	{
		DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

		TUniquePtr<UControlRigBlueprint::FControlValueScope> ValueScope;
		if (!UControlRigEditorSettings::Get()->bResetControlsOnCompile) // if we need to retain the controls
		{
			ValueScope = MakeUnique<UControlRigBlueprint::FControlValueScope>(GetControlRigBlueprint());
		}

		// force to disable the supended notif brackets
		if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
		{
			RigBlueprint->bSuspendModelNotificationsForOthers = false;
			RigBlueprint->bSuspendModelNotificationsForSelf = false;
		}

		UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
		if (RigBlueprint == nullptr)
		{
			return;
		}

		RigBlueprint->CompileLog.Messages.Reset();

		FString LastDebuggedObjectName = GetCustomDebugObjectLabel(RigBlueprint->GetObjectBeingDebugged());
		RigBlueprint->SetObjectBeingDebugged(nullptr);

		TArray< TWeakObjectPtr<UObject> > SelectedObjects = Inspector->GetSelectedObjects();

		if (ControlRig)
		{
			ControlRig->OnInitialized_AnyThread().Clear();
			ControlRig->OnExecuted_AnyThread().Clear();
			if (ControlRig->GetVM())
			{
				ControlRig->GetVM()->ExecutionHalted().RemoveAll(this);
			}
		}

		if(IsConstructionModeEnabled())
		{
			SetEventQueue(ForwardsSolveEventQueue, false);
		}

		// clear transient controls such that we don't leave
		// a phantom shape in the viewport
		// have to do this before compile() because during compile
		// a new control rig instance is created without the transient controls
		// so clear is never called for old transient controls
		RigBlueprint->ClearTransientControls();

		// default to always reset all bone modifications 
		ResetAllBoneModification(); 
		
		{
			TGuardValue<bool> GuardCompileReEntry(bIsCompilingThroughUI, true);
			FBlueprintEditor::Compile();
		}

		// ensure the skeletal mesh is still bound
		UControlRigSkeletalMeshComponent* SkelMeshComponent = Cast<UControlRigSkeletalMeshComponent>(GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent());
		if (SkelMeshComponent)
		{
			bool bWasCreated = false;
			FAnimCustomInstanceHelper::BindToSkeletalMeshComponent<UControlRigLayerInstance>(SkelMeshComponent, bWasCreated);
			if (bWasCreated)
			{
				OnAnimInitialized();
			}
		}
		
		if (ControlRig)
		{
			ControlRig->ControlRigLog = &ControlRigLog;

			UControlRigBlueprintGeneratedClass* GeneratedClass = Cast<UControlRigBlueprintGeneratedClass>(ControlRig->GetClass());
			if (GeneratedClass)
			{
				UControlRig* CDO = Cast<UControlRig>(GeneratedClass->GetDefaultObject(true /* create if needed */));
				FRigVMInstructionArray Instructions = CDO->VM->GetInstructions();

				if (Instructions.Num() <= 1) // just the "done" operator
				{
					FNotificationInfo Info(LOCTEXT("ControlRigBlueprintCompilerEmptyRigMessage", "The Control Rig you compiled doesn't do anything. Did you forget to add a Begin_Execution node?"));
					Info.bFireAndForget = true;
					Info.FadeOutDuration = 5.0f;
					Info.ExpireDuration = 5.0f;
					TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
					NotificationPtr->SetCompletionState(SNotificationItem::CS_Success);
				}
			}
		}

		TArray<FCustomDebugObject> DebugList;
		GetCustomDebugObjects(DebugList);

		for (const FCustomDebugObject& DebugObject : DebugList)
		{
			if (DebugObject.NameOverride == LastDebuggedObjectName)
			{
				RigBlueprint->SetObjectBeingDebugged(DebugObject.Object);
			}
		}

		// invalidate all node titles
		TArray<UEdGraph*> EdGraphs;
		RigBlueprint->GetAllGraphs(EdGraphs);
		for (UEdGraph* EdGraph : EdGraphs)
		{
			UControlRigGraph* RigGraph = Cast<UControlRigGraph>(EdGraph);
			if (RigGraph == nullptr)
			{
				continue;
			}

			for (UEdGraphNode* EdNode : RigGraph->Nodes)
			{
				if (UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(EdNode))
				{
					RigNode->InvalidateNodeTitle();
				}
			}
		}

		// store the defaults from the CDO back on the new variables list
		bool bAnyVariableValueChanged = false;
		for(FBPVariableDescription& NewVariable : RigBlueprint->NewVariables)
		{
			bAnyVariableValueChanged |= UpdateDefaultValueForVariable(NewVariable, true);
		}
		if (bAnyVariableValueChanged)
		{
			// Go over all the instances to update the default values from CDO
			for (const FCustomDebugObject& DebugObject : DebugList)
			{
				if (UControlRig* DebuggedRig = Cast<UControlRig>(DebugObject.Object))
				{
					DebuggedRig->CopyExternalVariableDefaultValuesFromCDO();
				}
			}
		}

		if (SelectedObjects.Num() > 0)
		{
			RefreshDetailView();
		}

		if (UControlRigEditorSettings::Get()->bResetControlTransformsOnCompile)
		{
			RigBlueprint->Hierarchy->ForEach<FRigControlElement>([RigBlueprint](FRigControlElement* ControlElement) -> bool
            {
				const FTransform Transform = RigBlueprint->Hierarchy->GetInitialLocalTransform(ControlElement->GetIndex());

				/*/
				if (ControlRig)
				{
					ControlRig->Modify();
					ControlRig->GetControlHierarchy().SetLocalTransform(Control.Index, Transform);
					ControlRig->ControlModified().Broadcast(ControlRig, Control, EControlRigSetKey::DoNotCare);
				}
				*/

				RigBlueprint->Hierarchy->SetLocalTransform(ControlElement->GetIndex(), Transform);
				return true;
			});
		}

		RigBlueprint->PropagatePoseFromBPToInstances();

		if (FControlRigEditMode* EditMode = GetEditMode())
		{
			EditMode->RecreateControlShapeActors(GetHierarchyBeingDebugged()->GetSelectedKeys());
		}
	}

	// enable this for creating a new unit test
	// DumpUnitTestCode();

	// FStatsHierarchical::EndMeasurements();
	// FMessageLog LogForMeasurements("ControlRigLog");
	// FStatsHierarchical::DumpMeasurements(LogForMeasurements);
}

void FControlRigEditor::SaveAsset_Execute()
{
	LastDebuggedRig = GetCustomDebugObjectLabel(GetBlueprintObj()->GetObjectBeingDebugged());
	FBlueprintEditor::SaveAsset_Execute();

	// Save the new state of the hierarchy in the default object, so that it has the correct values on load
	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	if(ControlRig)
	{
		UControlRig* CDO = ControlRig->GetClass()->GetDefaultObject<UControlRig>();
		CDO->DynamicHierarchy->CopyHierarchy(RigBlueprint->Hierarchy);
	}

	FBlueprintActionDatabase& ActionDatabase = FBlueprintActionDatabase::Get();
	ActionDatabase.ClearAssetActions(UControlRigBlueprint::StaticClass());
	ActionDatabase.RefreshClassActions(UControlRigBlueprint::StaticClass());
}

void FControlRigEditor::SaveAssetAs_Execute()
{
	LastDebuggedRig = GetCustomDebugObjectLabel(GetBlueprintObj()->GetObjectBeingDebugged());
	FBlueprintEditor::SaveAssetAs_Execute();

	// Save the new state of the hierarchy in the default object, so that it has the correct values on load
	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	if(ControlRig)
	{
		UControlRig* CDO = ControlRig->GetClass()->GetDefaultObject<UControlRig>();
		CDO->DynamicHierarchy->CopyHierarchy(RigBlueprint->Hierarchy);
	}

	FBlueprintActionDatabase& ActionDatabase = FBlueprintActionDatabase::Get();
	ActionDatabase.ClearAssetActions(UControlRigBlueprint::StaticClass());
	ActionDatabase.RefreshClassActions(UControlRigBlueprint::StaticClass());
}

FName FControlRigEditor::GetToolkitFName() const
{
	return FName("ControlRigEditor");
}

FText FControlRigEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Control Rig Editor");
}

FText FControlRigEditor::GetToolkitToolTipText() const
{
	return FAssetEditorToolkit::GetToolTipTextForObject(GetBlueprintObj());
}

FString FControlRigEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Control Rig Editor ").ToString();
}

FLinearColor FControlRigEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.5f, 0.25f, 0.35f, 0.5f );
}

void FControlRigEditor::InitToolMenuContext(FToolMenuContext& MenuContext)
{
	FBlueprintEditor::InitToolMenuContext(MenuContext);

	if (UControlRigBlueprint* RigBlueprint = GetControlRigBlueprint())
	{
		URigVMGraph* Model = nullptr;
		URigVMNode* Node = nullptr;
		URigVMPin* Pin = nullptr;
		
		if (UGraphNodeContextMenuContext* GraphNodeContext = MenuContext.FindContext<UGraphNodeContextMenuContext>())
		{
		
			if (GraphNodeContext->Node)
			{
				Model = RigBlueprint->GetModel(GraphNodeContext->Graph);
				if(Model)
				{
					Node = Model->FindNodeByName(GraphNodeContext->Node->GetFName());
				}
			}
		
			if (GraphNodeContext->Pin && Node)
			{
				Pin = Model->FindPin(GraphNodeContext->Pin->GetName());
			}
		}
		
		UControlRigContextMenuContext* ControlRigMenuContext = NewObject<UControlRigContextMenuContext>();
		FControlRigMenuSpecificContext MenuSpecificContext;	
		MenuSpecificContext.GraphNodeContextMenuContext = FControlRigGraphNodeContextMenuContext(Model, Node, Pin);
		ControlRigMenuContext->Init(SharedThis(this), MenuSpecificContext);

		MenuContext.AddObject(ControlRigMenuContext);
	}
}

bool FControlRigEditor::TransactionObjectAffectsBlueprint(UObject* InTransactedObject)
{
	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	if (RigBlueprint == nullptr)
	{
		return false;
	}

	if (InTransactedObject->GetOuter() == GetFocusedController())
	{
		return false;
	}
	return FBlueprintEditor::TransactionObjectAffectsBlueprint(InTransactedObject);
}

bool FControlRigEditor::CanAddNewLocalVariable() const
{
	const URigVMGraph* Graph = GetFocusedModel();
	const URigVMGraph* ParentGraph = Graph->GetParentGraph();				
	if (ParentGraph && ParentGraph->IsA<URigVMFunctionLibrary>())
	{
		return true;
	}
	return false;
}

void FControlRigEditor::OnAddNewLocalVariable()
{
	if (!CanAddNewLocalVariable())
	{
		return;
	}

	FRigVMGraphVariableDescription LastTypeVar;
	LastTypeVar.ChangeType(MyBlueprintWidget->GetLastPinTypeUsed());
	FRigVMGraphVariableDescription NewVar = GetFocusedController()->AddLocalVariable(TEXT("NewLocalVar"), LastTypeVar.CPPType, LastTypeVar.CPPTypeObject, LastTypeVar.DefaultValue, true, true);
	if(NewVar.Name.IsNone())
	{
		LogSimpleMessage( LOCTEXT("AddLocalVariable_Error", "Adding new local variable failed.") );
	}
	else
	{
		RenameNewlyAddedAction(NewVar.Name);
	}
}

void FControlRigEditor::OnPasteNewLocalVariable(const FBPVariableDescription& VariableDescription)
{
	if (!CanAddNewLocalVariable())
	{
		return;
	}

	FRigVMGraphVariableDescription TypeVar;
	TypeVar.ChangeType(VariableDescription.VarType);
	FRigVMGraphVariableDescription NewVar = GetFocusedController()->AddLocalVariable(VariableDescription.VarName, TypeVar.CPPType, TypeVar.CPPTypeObject, VariableDescription.DefaultValue, true, true);
	if(NewVar.Name.IsNone())
	{
		LogSimpleMessage( LOCTEXT("PasteLocalVariable_Error", "Pasting new local variable failed.") );
	}
}

void FControlRigEditor::DeleteSelectedNodes()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	if (RigBlueprint == nullptr)
	{
		return;
	}

	const FGraphPanelSelectionSet SelectedNodes = GetSelectedNodes();
	SetUISelectionState(NAME_None);

	bool DeletedAnything = false;
	GetFocusedController()->OpenUndoBracket(TEXT("Delete selected nodes"));

	for (FGraphPanelSelectionSet::TConstIterator NodeIt(SelectedNodes); NodeIt; ++NodeIt)
	{
		if (UEdGraphNode* Node = Cast<UEdGraphNode>(*NodeIt))
		{
			if (Node->CanUserDeleteNode())
			{
				AnalyticsTrackNodeEvent(GetBlueprintObj(), Node, true);
				if (UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(Node))
				{
					const bool bReconnectPins = (FSlateApplication::Get().GetModifierKeys().IsShiftDown());
					if(GetFocusedController()->RemoveNodeByName(*RigNode->ModelNodePath, true, false, true, bReconnectPins))
					{
						DeletedAnything = true;
					}
				}
				else if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Node))
				{
					if(GetFocusedController()->RemoveNodeByName(CommentNode->GetFName(), true, false, true))
					{
						DeletedAnything = true;
					}
				}
				else
				{
					Node->GetGraph()->RemoveNode(Node);
				}
			}
		}
	}

	if(DeletedAnything)
	{
		GetFocusedController()->CloseUndoBracket();
	}
	else
	{
		GetFocusedController()->CancelUndoBracket();
	}
}

bool FControlRigEditor::CanDeleteNodes() const
{
	return true;
}

void FControlRigEditor::CopySelectedNodes()
{
	FString ExportedText = GetFocusedController()->ExportSelectedNodesToText();
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
}

bool FControlRigEditor::CanCopyNodes() const
{
	return GetFocusedModel()->GetSelectNodes().Num() > 0;
}

bool FControlRigEditor::CanPasteNodes() const
{
	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);
	return GetFocusedController()->CanImportNodesFromText(TextToImport);
}

FReply FControlRigEditor::OnSpawnGraphNodeByShortcut(FInputChord InChord, const FVector2D& InPosition,
                                                     UEdGraph* InGraph)
{
	if(!InChord.HasAnyModifierKeys())
	{
		if(UControlRigGraph* RigGraph = Cast<UControlRigGraph>(InGraph))
		{
			if(URigVMController* Controller = RigGraph->GetController())
			{
				if(InChord.Key == EKeys::B)
				{
					Controller->AddBranchNode(InPosition, "", true, true);
				}
				else if(InChord.Key == EKeys::S)
				{
					Controller->AddUnitNode(FRigUnit_SequenceAggregate::StaticStruct(), FRigUnit::GetMethodName(), InPosition, FString(), true, true);
				}
				else if(InChord.Key == EKeys::One)
				{
					Controller->AddUnitNode(FRigUnit_GetTransform::StaticStruct(), FRigUnit::GetMethodName(), InPosition, FString(), true, true);
				}
				else if(InChord.Key == EKeys::Two)
				{
					Controller->AddUnitNode(FRigUnit_SetTransform::StaticStruct(), FRigUnit::GetMethodName(), InPosition, FString(), true, true);
				}
				else if(InChord.Key == EKeys::Three)
				{
					Controller->AddUnitNode(FRigUnit_ParentConstraint::StaticStruct(), FRigUnit::GetMethodName(), InPosition, FString(), true, true);
				}
				else if(InChord.Key == EKeys::Four)
				{
					Controller->AddUnitNode(FRigUnit_GetControlFloat::StaticStruct(), FRigUnit::GetMethodName(), InPosition, FString(), true, true);
				}
				else if(InChord.Key == EKeys::Five)
				{
					Controller->AddUnitNode(FRigUnit_SetCurveValue::StaticStruct(), FRigUnit::GetMethodName(), InPosition, FString(), true, true);
				}
			}
		}
	}
	else if(InChord.NeedsAlt() && !InChord.NeedsControl() && !InChord.NeedsShift())
	{
		if(InChord.Key == EKeys::One)
		{
			RestoreNodeSnippet(1);
			return FReply::Handled();
		}
		else if(InChord.Key == EKeys::Two)
		{
			RestoreNodeSnippet(2);
			return FReply::Handled();
		}
		else if(InChord.Key == EKeys::Three)
		{
			RestoreNodeSnippet(3);
			return FReply::Handled();
		}
		else if(InChord.Key == EKeys::Four)
		{
			RestoreNodeSnippet(4);
			return FReply::Handled();
		}
		else if(InChord.Key == EKeys::Five)
		{
			RestoreNodeSnippet(5);
			return FReply::Handled();
		}
		else if(InChord.Key == EKeys::Six)
		{
			RestoreNodeSnippet(6);
			return FReply::Handled();
		}
		else if(InChord.Key == EKeys::Seven)
		{
			RestoreNodeSnippet(7);
			return FReply::Handled();
		}
		else if(InChord.Key == EKeys::Eight)
		{
			RestoreNodeSnippet(8);
			return FReply::Handled();
		}
		else if(InChord.Key == EKeys::Nine)
		{
			RestoreNodeSnippet(9);
			return FReply::Handled();
		}
		else if(InChord.Key == EKeys::Zero)
		{
			RestoreNodeSnippet(0);
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

void FControlRigEditor::PasteNodes()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	GetFocusedController()->OpenUndoBracket(TEXT("Pasted Nodes."));

	FVector2D PasteLocation = FSlateApplication::Get().GetCursorPos();

	TSharedPtr<SDockTab> ActiveTab = DocumentManager->GetActiveTab();
	if (ActiveTab.IsValid())
	{
		TSharedPtr<SGraphEditor> GraphEditor = StaticCastSharedRef<SGraphEditor>(ActiveTab->GetContent());
		if (GraphEditor.IsValid())
		{
			PasteLocation = GraphEditor->GetPasteLocation();

		}
	}

	FString TextToImport;
	FPlatformApplicationMisc::ClipboardPaste(TextToImport);

	TGuardValue<FRigVMController_RequestLocalizeFunctionDelegate> RequestLocalizeDelegateGuard(
		GetFocusedController()->RequestLocalizeFunctionDelegate,
		FRigVMController_RequestLocalizeFunctionDelegate::CreateLambda([this](URigVMLibraryNode* InFunctionToLocalize)
		{
			OnRequestLocalizeFunctionDialog(InFunctionToLocalize, GetControlRigBlueprint(), true);

			const URigVMLibraryNode* LocalizedFunctionNode = GetControlRigBlueprint()->GetLocalFunctionLibrary()->FindPreviouslyLocalizedFunction(InFunctionToLocalize);
			return LocalizedFunctionNode != nullptr;
		})
	);
	
	TArray<FName> NodeNames = GetFocusedController()->ImportNodesFromText(TextToImport, true, true);

	if (NodeNames.Num() > 0)
	{
		FBox2D Bounds;
		Bounds.bIsValid = false;

		TArray<FName> NodesToSelect;
		for (const FName& NodeName : NodeNames)
		{
			const URigVMNode* Node = GetFocusedModel()->FindNodeByName(NodeName);
			check(Node);

			if (Node->IsInjected())
			{
				continue;
			}
			NodesToSelect.Add(NodeName);

			FVector2D Position = Node->GetPosition();
			FVector2D Size = Node->GetSize();

			if (!Bounds.bIsValid)
			{
				Bounds.Min = Bounds.Max = Position;
				Bounds.bIsValid = true;
			}
			Bounds += Position;
			Bounds += Position + Size;
		}

		for (const FName& NodeName : NodesToSelect)
		{
			const URigVMNode* Node = GetFocusedModel()->FindNodeByName(NodeName);
			check(Node);

			FVector2D Position = Node->GetPosition();
			GetFocusedController()->SetNodePositionByName(NodeName, PasteLocation + Position - Bounds.GetCenter(), true, false, true);
		}

		GetFocusedController()->SetNodeSelection(NodesToSelect);
		GetFocusedController()->CloseUndoBracket();
	}
	else
	{
		GetFocusedController()->CancelUndoBracket();
	}
}

void FControlRigEditor::PostUndo(bool bSuccess)
{
	const FTransaction* Transaction = GEditor->Trans->GetTransaction(GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount());
	IControlRigEditor::PostUndo(bSuccess);
	PostTransaction(bSuccess, Transaction, false);
}

void FControlRigEditor::PostRedo(bool bSuccess)
{
	const FTransaction* Transaction = GEditor->Trans->GetTransaction(GEditor->Trans->GetQueueLength() - GEditor->Trans->GetUndoCount() - 1);
	IControlRigEditor::PostRedo(bSuccess);
	PostTransaction(bSuccess, Transaction, true);
}

void FControlRigEditor::PostTransaction(bool bSuccess, const FTransaction* Transaction, bool bIsRedo)
{
	EnsureValidRigElementsInDetailPanel();

	if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
	{
		// Do not compile here. ControlRigBlueprint::PostTransacted decides when it is necessary to compile depending
		// on the properties that are affected.
		//Compile();
		
		USkeletalMesh* PreviewMesh = GetPersonaToolkit()->GetPreviewScene()->GetPreviewMesh();
		if (PreviewMesh != RigBlueprint->GetPreviewMesh())
		{
			RigBlueprint->SetPreviewMesh(PreviewMesh);
			GetPersonaToolkit()->SetPreviewMesh(PreviewMesh, true);
		}

		if (UControlRig* DebuggedControlRig = Cast<UControlRig>(RigBlueprint->GetObjectBeingDebugged()))
		{
			if(URigHierarchy* Hierarchy = DebuggedControlRig->GetHierarchy())
			{
				if(Hierarchy->Num() == 0)
				{
					OnHierarchyChanged();
				}
			}
		}

		if (FControlRigEditMode* EditMode = GetEditMode())
		{
			EditMode->RequestToRecreateControlShapeActors();
		}
	}
}

void FControlRigEditor::JumpToHyperlink(const UObject* ObjectReference, bool bRequestRename)
{
	if(const UControlRigGraph* Graph = Cast<UControlRigGraph>(ObjectReference))
	{
		OpenGraphAndBringToFront((UEdGraph*)Graph, true);
		return;
	}
	
	IControlRigEditor::JumpToHyperlink(ObjectReference, bRequestRename);
}

void FControlRigEditor::EnsureValidRigElementsInDetailPanel()
{
	UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint();
	URigHierarchy* Hierarchy = ControlRigBP->Hierarchy; 

	TArray< TWeakObjectPtr<UObject> > SelectedObjects = Inspector->GetSelectedObjects();
	for (TWeakObjectPtr<UObject> SelectedObject : SelectedObjects)
	{
		if (SelectedObject.IsValid())
		{
			if(UDetailsViewWrapperObject* WrapperObject = Cast<UDetailsViewWrapperObject>(SelectedObject.Get()))
			{
				if(const UScriptStruct* WrappedStruct = WrapperObject->GetWrappedStruct())
				{
					if (WrappedStruct->IsChildOf(FRigBaseElement::StaticStruct()))
					{
						FRigElementKey Key = WrapperObject->GetContent<FRigBaseElement>().GetKey();
						if(!Hierarchy->Contains(Key))
						{
							ClearDetailObject();
						}
					}
				}
			}
		}
	}
}

void FControlRigEditor::OnStartWatchingPin()
{
	if (UEdGraphPin* Pin = GetCurrentlySelectedPin())
	{
		GetFocusedController()->SetPinIsWatched(Pin->GetName(), true);
	}
}

bool FControlRigEditor::CanStartWatchingPin() const
{
	if (UEdGraphPin* Pin = GetCurrentlySelectedPin())
	{
		if (URigVMPin* ModelPin = GetFocusedModel()->FindPin(Pin->GetName()))
		{
			return ModelPin->GetParentPin() == nullptr &&
					!ModelPin->RequiresWatch();
		}
	}
	return false;
}

void FControlRigEditor::OnStopWatchingPin()
{
	if (UEdGraphPin* Pin = GetCurrentlySelectedPin())
	{
		GetFocusedController()->SetPinIsWatched(Pin->GetName(), false);
	}
}

bool FControlRigEditor::CanStopWatchingPin() const
{
	if (UEdGraphPin* Pin = GetCurrentlySelectedPin())
	{
		if (URigVMPin* ModelPin = GetFocusedModel()->FindPin(Pin->GetName()))
		{
			return ModelPin->RequiresWatch();
		}
	}
	return false;
}

void FControlRigEditor::OnToolkitHostingStarted(const TSharedRef<class IToolkit>& Toolkit)
{
	TSharedPtr<SWidget> InlineContent = Toolkit->GetInlineContent();
	if (InlineContent.IsValid())
	{
		Toolbox->SetContent(InlineContent.ToSharedRef());
	}
}

void FControlRigEditor::OnToolkitHostingFinished(const TSharedRef<class IToolkit>& Toolkit)
{
	Toolbox->SetContent(SNullWidget::NullWidget);
}

void FControlRigEditor::OnActiveTabChanged( TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated )
{
	if (!NewlyActivated.IsValid())
	{
		TArray<UObject*> ObjArray;
		Inspector->ShowDetailsForObjects(ObjArray);
	}
	else 
	{
		FBlueprintEditor::OnActiveTabChanged(PreviouslyActive, NewlyActivated);
	}
}

void FControlRigEditor::OnAnimInitialized()
{
	UControlRigSkeletalMeshComponent* EditorSkelComp = Cast<UControlRigSkeletalMeshComponent>(GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent());
	if (EditorSkelComp)
	{
		EditorSkelComp->bRequiredBonesUpToDateDuringTick = 0;

		UControlRigLayerInstance* AnimInstance = Cast<UControlRigLayerInstance>(EditorSkelComp->GetAnimInstance());
		if (AnimInstance && ControlRig)
		{
			// update control rig data to anim instance since animation system has been reinitialized
			FInputBlendPose Filter;
			AnimInstance->ResetControlRigTracks();
			AnimInstance->AddControlRigTrack(0, ControlRig);
			AnimInstance->UpdateControlRigTrack(0, 1.0f, FControlRigIOSettings::MakeEnabled(), bExecutionControlRig);
		}
	}
}

void FControlRigEditor::UndoAction()
{
	GEditor->UndoTransaction();
}

void FControlRigEditor::RedoAction()
{
	GEditor->RedoTransaction();
}

void FControlRigEditor::CreateDefaultTabContents(const TArray<UBlueprint*>& InBlueprints)
{
	FBlueprintEditor::CreateDefaultTabContents(InBlueprints);
}

void FControlRigEditor::NewDocument_OnClicked(ECreatedDocumentType GraphType)
{
	if (GraphType == FBlueprintEditor::CGT_NewFunctionGraph)
	{
		if (UControlRigBlueprint* Blueprint = GetControlRigBlueprint())
		{
			if (URigVMController* Controller = Blueprint->GetOrCreateController(Blueprint->GetLocalFunctionLibrary()))
			{
				if (const URigVMLibraryNode* FunctionNode = Controller->AddFunctionToLibrary(TEXT("New Function"), true, FVector2D::ZeroVector, true, true))
				{
					if (const UEdGraph* NewGraph = Blueprint->GetEdGraph(FunctionNode->GetContainedGraph()))
					{
						OpenDocument(NewGraph, FDocumentTracker::OpenNewDocument);
						RenameNewlyAddedAction(FunctionNode->GetFName());
					}

				}
			}
		}
	}
	else if(GraphType == FBlueprintEditor::CGT_NewEventGraph)
	{
		if (UControlRigBlueprint* Blueprint = GetControlRigBlueprint())
		{
			if(URigVMGraph* Model = Blueprint->AddModel(UControlRigGraphSchema::GraphName_ControlRig.ToString()))
			{
				if (const UEdGraph* NewGraph = Blueprint->GetEdGraph(Model))
				{
					OpenDocument(NewGraph, FDocumentTracker::OpenNewDocument);
					RenameNewlyAddedAction(NewGraph->GetFName());
				}
			}
		}
	}
}

bool FControlRigEditor::IsSectionVisible(NodeSectionID::Type InSectionID) const
{
	switch (InSectionID)
	{
		case NodeSectionID::GRAPH:
		case NodeSectionID::VARIABLE:
		case NodeSectionID::FUNCTION:
		{
			return true;
		}
		case NodeSectionID::LOCAL_VARIABLE:
		{
			if(const URigVMGraph* Graph = GetFocusedModel())
			{
				const URigVMGraph* ParentGraph = Graph->GetParentGraph();				
				if (ParentGraph && ParentGraph->IsA<URigVMFunctionLibrary>())
				{
					return true;
				}
			}
		}
		default:
		{
			break;
		}
	}
	return false;
}

FGraphAppearanceInfo FControlRigEditor::GetGraphAppearance(UEdGraph* InGraph) const
{
	FGraphAppearanceInfo AppearanceInfo = FBlueprintEditor::GetGraphAppearance(InGraph);

	if (GetBlueprintObj()->IsA(UControlRigBlueprint::StaticClass()))
	{
		AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_ControlRig", "RIG");
	}

	return AppearanceInfo;
}

void FControlRigEditor::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	UControlRigBlueprint* ControlRigBlueprint = CastChecked<UControlRigBlueprint>(GetBlueprintObj());
	if (ControlRigBlueprint == nullptr)
	{
		return;
	}

	switch (InNotifType)
	{
		case ERigVMGraphNotifType::NodeSelected:
		case ERigVMGraphNotifType::NodeDeselected:
		{
			if (UControlRigGraph* RigGraph = Cast<UControlRigGraph>(ControlRigBlueprint->GetEdGraph(InGraph)))
			{
				TSharedPtr<SGraphEditor> GraphEd = GetGraphEditor(RigGraph);
				URigVMNode* Node = Cast<URigVMNode>(InSubject);

				if (GraphEd.IsValid() && Node != nullptr)
				{
					SetDetailViewForGraph(Node->GetGraph());

					if (!RigGraph->bIsSelecting)
					{
						TGuardValue<bool> SelectingGuard(RigGraph->bIsSelecting, true);
						if (URigVMNode* ModelNode = Cast<URigVMNode>(InSubject))
						{
							if (UEdGraphNode* EdNode = RigGraph->FindNodeForModelNodeName(ModelNode->GetFName()))
							{
								GraphEd->SetNodeSelection(EdNode, InNotifType == ERigVMGraphNotifType::NodeSelected);
							}
						}
					}
				}
			}
			break;
		}
		case ERigVMGraphNotifType::PinDefaultValueChanged:
		{
			URigVMPin* Pin = Cast<URigVMPin>(InSubject);

			if(URigVMPin* RootPin = Pin->GetRootPin())
			{
				const FString DefaultValue = RootPin->GetDefaultValue();
				if(!DefaultValue.IsEmpty())
				{
					// sync the value change with the unit(s) displayed 
					TArray< TWeakObjectPtr<UObject> > SelectedObjects = Inspector->GetSelectedObjects();
					for (TWeakObjectPtr<UObject> SelectedObject : SelectedObjects)
					{
						if (SelectedObject.IsValid())
						{
							if(UDetailsViewWrapperObject* WrapperObject = Cast<UDetailsViewWrapperObject>(SelectedObject.Get()))
							{
								if(WrapperObject->GetOuter() == Pin->GetNode())
								{
									const FProperty* TargetProperty = WrapperObject->GetClass()->FindPropertyByName(RootPin->GetFName());
									if(TargetProperty)
									{
										uint8* PropertyStorage = TargetProperty->ContainerPtrToValuePtr<uint8>(WrapperObject);

										// we are ok with not reacting to errors here
										FRigVMPinDefaultValueImportErrorContext ErrorPipe;										
										TargetProperty->ImportText_Direct(*DefaultValue, PropertyStorage, nullptr, PPF_None, &ErrorPipe);
									}
								}
							}
						}
					}
				}

				if(const URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(RootPin->GetNode()))
				{
					if(UnitNode->IsEvent())
					{
						FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprintObj());
						CacheNameLists();
					}
				}
			}
	
			break;
		}
		case ERigVMGraphNotifType::PinArraySizeChanged:
		case ERigVMGraphNotifType::PinBoundVariableChanged:
		case ERigVMGraphNotifType::PinTypeChanged:
		{
			URigVMPin* Pin = Cast<URigVMPin>(InSubject);

			if(Pin->GetNode()->IsSelected())
			{
				TArray<UObject*> Objects;
				Objects.Add(Pin->GetNode());
				SetDetailObjects(Objects);
			}
			break;
		}
		case ERigVMGraphNotifType::NodeRemoved:
		{
			if (URigVMCollapseNode* CollapseNode = Cast<URigVMCollapseNode>(InSubject))
			{
				if (UEdGraph* EdGraph = ControlRigBlueprint->GetEdGraph(CollapseNode->GetContainedGraph()))
				{
					CloseDocumentTab(EdGraph);
					ClearDetailObject();
				}
			}
			else if(URigVMFunctionReferenceNode* FunctionRefNode = Cast<URigVMFunctionReferenceNode>(InSubject))
			{
				ClearDetailObject();
			}
				
			// fall through next case since we want to refresh the name lists
			// both for removing or adding an event
		}
		case ERigVMGraphNotifType::NodeAdded:
		{
			if(URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(InSubject))
			{
				if(UnitNode->IsEvent())
				{
					FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(GetBlueprintObj());
					CacheNameLists();
				}
			}
			break;
		}
		case ERigVMGraphNotifType::NodeSelectionChanged:
		default:
		{
			break;
		}
	}
}

void FControlRigEditor::HandleVMCompiledEvent(UObject* InCompiledObject, URigVM* InVM)
{
	if(UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(InCompiledObject))
	{
		CompilerResultsListing->ClearMessages();
		CompilerResultsListing->AddMessages(RigBlueprint->CompileLog.Messages);
		RigBlueprint->CompileLog.Messages.Reset();
		RigBlueprint->CompileLog.NumErrors = RigBlueprint->CompileLog.NumWarnings = 0;
	}

	RefreshDetailView();
	
	TArray<FName> TabIds;
	TabIds.Add(*FString::Printf(TEXT("ControlRigMemoryDetails_%d"), (int32)ERigVMMemoryType::Literal));
	TabIds.Add(*FString::Printf(TEXT("ControlRigMemoryDetails_%d"), (int32)ERigVMMemoryType::Work));
	TabIds.Add(*FString::Printf(TEXT("ControlRigMemoryDetails_%d"), (int32)ERigVMMemoryType::Debug));

	for (const FName& TabId : TabIds)
	{
		TSharedPtr<SDockTab> ActiveTab = GetTabManager()->FindExistingLiveTab(TabId);
		if(ActiveTab)
		{
			if(ActiveTab->GetMetaData<FMemoryTypeMetaData>().IsValid())
			{
				ERigVMMemoryType MemoryType = ActiveTab->GetMetaData<FMemoryTypeMetaData>()->MemoryType;			
				TSharedRef<IDetailsView> DetailsView = StaticCastSharedRef<IDetailsView>(ActiveTab->GetContent());
				DetailsView->SetObject(InVM->GetMemoryByType(MemoryType));
			}
		}
	}

	UpdateGraphCompilerErrors();
}

void FControlRigEditor::HandleControlRigExecutedEvent(UControlRig* InControlRig, const EControlRigState InState, const FName& InEventName)
{
	if (UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint())
	{
		UControlRig* DebuggedControlRig = Cast<UControlRig>(ControlRigBP->GetObjectBeingDebugged());
		if (DebuggedControlRig == nullptr)
		{
			DebuggedControlRig = ControlRig;
		}

		URigHierarchy* Hierarchy = GetHierarchyBeingDebugged(); 

		TArray< TWeakObjectPtr<UObject> > SelectedObjects = Inspector->GetSelectedObjects();
		for (TWeakObjectPtr<UObject> SelectedObject : SelectedObjects)
		{
			if (SelectedObject.IsValid())
			{
				if(UDetailsViewWrapperObject* WrapperObject = Cast<UDetailsViewWrapperObject>(SelectedObject.Get()))
				{
					if (const UScriptStruct* Struct = WrapperObject->GetWrappedStruct())
					{
						if(Struct->IsChildOf(FRigBaseElement::StaticStruct()))
						{
							const FRigElementKey Key = WrapperObject->GetContent<FRigBaseElement>().GetKey();

							FRigBaseElement* Element = Hierarchy->Find(Key);
							if(Element == nullptr)
							{
								ClearDetailObject();
								break;
							}

							if(FRigControlElement* ControlElement = Cast<FRigControlElement>(Element))
							{
								// compute all transforms
								Hierarchy->GetTransform(ControlElement, ERigTransformType::CurrentGlobal);
								Hierarchy->GetTransform(ControlElement, ERigTransformType::CurrentLocal);
								Hierarchy->GetTransform(ControlElement, ERigTransformType::InitialGlobal);
								Hierarchy->GetTransform(ControlElement, ERigTransformType::InitialLocal);
								Hierarchy->GetControlOffsetTransform(ControlElement, ERigTransformType::CurrentGlobal);
								Hierarchy->GetControlOffsetTransform(ControlElement, ERigTransformType::CurrentLocal);
								Hierarchy->GetControlOffsetTransform(ControlElement, ERigTransformType::InitialGlobal);
								Hierarchy->GetControlOffsetTransform(ControlElement, ERigTransformType::InitialLocal);
								Hierarchy->GetControlShapeTransform(ControlElement, ERigTransformType::CurrentGlobal);
								Hierarchy->GetControlShapeTransform(ControlElement, ERigTransformType::CurrentLocal);
								Hierarchy->GetControlShapeTransform(ControlElement, ERigTransformType::InitialGlobal);
								Hierarchy->GetControlShapeTransform(ControlElement, ERigTransformType::InitialLocal);

								WrapperObject->SetContent<FRigControlElement>(*ControlElement);
							}
							else if(FRigTransformElement* TransformElement = Cast<FRigTransformElement>(Element))
							{
								// compute all transforms
								Hierarchy->GetTransform(TransformElement, ERigTransformType::CurrentGlobal);
								Hierarchy->GetTransform(TransformElement, ERigTransformType::CurrentLocal);
								Hierarchy->GetTransform(TransformElement, ERigTransformType::InitialGlobal);
								Hierarchy->GetTransform(TransformElement, ERigTransformType::InitialLocal);

								WrapperObject->SetContent<FRigTransformElement>(*TransformElement);
							}
							else
							{
								WrapperObject->SetContent<FRigBaseElement>(*Element);
							}
						}
					}
				}
			}
		}

		if(ControlRigBP->RigGraphDisplaySettings.NodeRunLimit > 1)
		{
			if(DebuggedControlRig)
			{
				if(URigVM* VM = DebuggedControlRig->GetVM())
				{
					bool bFoundLimitWarnings = false;
					
					const FRigVMByteCode& ByteCode = VM->GetByteCode();
					for(int32 InstructionIndex = 0; InstructionIndex < ByteCode.GetNumInstructions(); InstructionIndex++)
					{
						const int32 Count = VM->GetInstructionVisitedCount(InstructionIndex);
						if(Count > ControlRigBP->RigGraphDisplaySettings.NodeRunLimit)
						{
							bFoundLimitWarnings = true;

							const FString CallPath = VM->GetByteCode().GetCallPathForInstruction(InstructionIndex); 
							if(!KnownInstructionLimitWarnings.Contains(CallPath))
							{
								const FString Message = FString::Printf(
                                    TEXT("Instruction has hit the NodeRunLimit\n(ran %d times, limit is %d)\n\nYou can increase the limit in the class settings."),
                                    Count,
                                    ControlRigBP->RigGraphDisplaySettings.NodeRunLimit
                                );

								if(DebuggedControlRig->ControlRigLog)
								{
									DebuggedControlRig->ControlRigLog->Entries.Add(
										FControlRigLog::FLogEntry(EMessageSeverity::Warning, InEventName, InstructionIndex, Message
									));
								}

								if(URigVMNode* Subject = Cast<URigVMNode>(VM->GetByteCode().GetSubjectForInstruction(InstructionIndex)))
								{
									FNotificationInfo Info(FText::FromString(Message));
									Info.bFireAndForget = true;
									Info.FadeOutDuration = 1.0f;
									Info.ExpireDuration = 5.0f;

									if(UControlRigGraph* EdGraph = Cast<UControlRigGraph>(GetControlRigBlueprint()->GetEdGraph(Subject->GetGraph())))
									{
										if(UEdGraphNode* Node = EdGraph->FindNodeForModelNodeName(Subject->GetFName()))
										{
											Info.Hyperlink = FSimpleDelegate::CreateLambda([this, Node] ()
	                                        {
	                                            JumpToHyperlink(Node, false);
	                                        });
									
											Info.HyperlinkText = FText::FromString(Subject->GetName());
										}
									}

									TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
									NotificationPtr->SetCompletionState(SNotificationItem::CS_Fail);
								}

								KnownInstructionLimitWarnings.Add(CallPath, Message);
							}
						}
					}

					if(!bFoundLimitWarnings)
					{
						KnownInstructionLimitWarnings.Reset();
					}
				}
			}
		}

		if(ControlRigBP->VMRuntimeSettings.bEnableProfiling)
		{
			if(DebuggedControlRig)
			{
				ControlRigBP->RigGraphDisplaySettings.TotalMicroSeconds = DebuggedControlRig->GetVM()->GetContext().LastExecutionMicroSeconds;
			}

			if(ControlRigBP->RigGraphDisplaySettings.bAutoDetermineRange)
			{
				if(ControlRigBP->RigGraphDisplaySettings.LastMaxMicroSeconds < 0.0)
				{
					ControlRigBP->RigGraphDisplaySettings.LastMinMicroSeconds = ControlRigBP->RigGraphDisplaySettings.MinMicroSeconds; 
					ControlRigBP->RigGraphDisplaySettings.LastMaxMicroSeconds = ControlRigBP->RigGraphDisplaySettings.MaxMicroSeconds;
				}
				else if(ControlRigBP->RigGraphDisplaySettings.MaxMicroSeconds >= 0.0)
				{
					const double T = 0.05;
					ControlRigBP->RigGraphDisplaySettings.LastMinMicroSeconds = FMath::Lerp<double>(ControlRigBP->RigGraphDisplaySettings.LastMinMicroSeconds, ControlRigBP->RigGraphDisplaySettings.MinMicroSeconds, T); 
					ControlRigBP->RigGraphDisplaySettings.LastMaxMicroSeconds = FMath::Lerp<double>(ControlRigBP->RigGraphDisplaySettings.LastMaxMicroSeconds, ControlRigBP->RigGraphDisplaySettings.MaxMicroSeconds, T); 
				}

				ControlRigBP->RigGraphDisplaySettings.MinMicroSeconds = DBL_MAX; 
				ControlRigBP->RigGraphDisplaySettings.MaxMicroSeconds = (double)INDEX_NONE;
			}
			else
			{
				ControlRigBP->RigGraphDisplaySettings.LastMinMicroSeconds = ControlRigBP->RigGraphDisplaySettings.MinMicroSeconds; 
				ControlRigBP->RigGraphDisplaySettings.LastMaxMicroSeconds = ControlRigBP->RigGraphDisplaySettings.MaxMicroSeconds;
			}
		}
	}

	UpdateGraphCompilerErrors();
}

void FControlRigEditor::HandleControlRigExecutionHalted(const int32 InstructionIndex, UObject* InNodeObject, const FName& InEntryName)
{
	if (HaltedAtNode == InNodeObject)
	{
		return;
	}
		
	if (URigVMNode* InNode = Cast<URigVMNode>(InNodeObject))
	{
		SetHaltedNode(InNode);
		
		if (UControlRigBlueprint* Blueprint = GetControlRigBlueprint())
		{
			if (Blueprint->GetAllModels().Contains(InNode->GetGraph()))
			{
				if(UControlRigGraph* EdGraph = Cast<UControlRigGraph>(Blueprint->GetEdGraph(InNode->GetGraph())))
				{
					if(UEdGraphNode* EdNode = EdGraph->FindNodeForModelNodeName(InNode->GetFName()))
					{
						JumpToHyperlink(EdNode, false);
					}
				}
			}
		}
	}
	else 
	{
		if (InEntryName == ControlRig->GetEventQueue().Last())
		{
			SetHaltedNode(nullptr);
		}
	}
}

void FControlRigEditor::SetHaltedNode(URigVMNode* Node)
{
	if (HaltedAtNode)
	{
		HaltedAtNode->SetExecutionIsHaltedAtThisNode(false);
	}
	HaltedAtNode = Node;
	if (HaltedAtNode)
	{
		HaltedAtNode->SetExecutionIsHaltedAtThisNode(true);
	}
}

void FControlRigEditor::CreateEditorModeManager()
{
	EditorModeManager = MakeShareable(FModuleManager::LoadModuleChecked<FPersonaModule>("Persona").CreatePersonaEditorModeManager());
}

void FControlRigEditor::Tick(float DeltaTime)
{
	FBlueprintEditor::Tick(DeltaTime);

	bool bDrawHierarchyBones = false;

	// tick the control rig in case we don't have skeletal mesh
	if (UControlRigBlueprint* Blueprint = GetControlRigBlueprint())
	{
		if (Blueprint->GetPreviewMesh() == nullptr && 
			ControlRig != nullptr && 
			bExecutionControlRig)
		{
			{
				// prevent transient controls from getting reset
				UControlRig::FTransientControlPoseScope	PoseScope(ControlRig);
				// reset transforms here to prevent additive transforms from accumulating to INF
				ControlRig->GetHierarchy()->ResetPoseToInitial(ERigElementType::Bone);
			}

			if (PreviewInstance)
			{
				// since we don't have a preview mesh the anim instance cannot deal with the modify bone
				// functionality. we need to perform this manually to ensure the pose is kept.
				const TArray<FAnimNode_ModifyBone>& BoneControllers = PreviewInstance->GetBoneControllers();
				for(const FAnimNode_ModifyBone& ModifyBone : BoneControllers)
				{
					const FRigElementKey BoneKey(ModifyBone.BoneToModify.BoneName, ERigElementType::Bone);
					const FTransform BoneTransform(ModifyBone.Rotation, ModifyBone.Translation, ModifyBone.Scale);
					ControlRig->GetHierarchy()->SetLocalTransform(BoneKey, BoneTransform);
				}
			}
			
			ControlRig->SetDeltaTime(DeltaTime);
			ControlRig->Evaluate_AnyThread();
			bDrawHierarchyBones = true;
		}

		if (LastDebuggedRig != GetCustomDebugObjectLabel(Blueprint->GetObjectBeingDebugged()))
		{
			TArray<FCustomDebugObject> DebugList;
			GetCustomDebugObjects(DebugList);

			for (const FCustomDebugObject& DebugObject : DebugList)
			{
				if (DebugObject.NameOverride == LastDebuggedRig)
				{
					GetBlueprintObj()->SetObjectBeingDebugged(DebugObject.Object);
					break;
				}
			}
		}
	}

	if (FControlRigEditorEditMode* EditMode = GetEditMode())
	{
		if (bDrawHierarchyBones)
		{
			EditMode->bDrawHierarchyBones = bDrawHierarchyBones;
		}
	}

	if(WeakGroundActorPtr.IsValid())
	{
		const TSharedRef<IPersonaPreviewScene> CurrentPreviewScene = GetPersonaToolkit()->GetPreviewScene();
		const float FloorOffset = CurrentPreviewScene->GetFloorOffset();
		const FTransform FloorTransform(FRotator(0, 0, 0), FVector(0, 0, -(FloorOffset)), FVector(4.0f, 4.0f, 1.0f));
		WeakGroundActorPtr->GetStaticMeshComponent()->SetRelativeTransform(FloorTransform);
	}
}

bool FControlRigEditor::IsEditable(UEdGraph* InGraph) const
{
	return IsGraphInCurrentBlueprint(InGraph);
}

bool FControlRigEditor::IsCompilingEnabled() const
{
	return true;
}

FText FControlRigEditor::GetGraphDecorationString(UEdGraph* InGraph) const
{
	return FText::GetEmpty();
}

TStatId FControlRigEditor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FControlRigEditor, STATGROUP_Tickables);
}

void FControlRigEditor::OnSelectedNodesChangedImpl(const TSet<class UObject*>& NewSelection)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	UControlRigGraph* RigGraph = Cast<UControlRigGraph>(GetFocusedGraph());
	if (RigGraph == nullptr)
	{
		return;
	}

	if (RigGraph->bIsSelecting || GIsTransacting)
	{
		return;
	}

	TGuardValue<bool> SelectGuard(RigGraph->bIsSelecting, true);

	UControlRigBlueprint* ControlRigBlueprint = CastChecked<UControlRigBlueprint>(GetBlueprintObj());
	if (ControlRigBlueprint)
	{
		TArray<FName> NodeNamesToSelect;
		for (UObject* Object : NewSelection)
		{
			if (UControlRigGraphNode* ControlRigGraphNode = Cast<UControlRigGraphNode>(Object))
			{
				NodeNamesToSelect.Add(ControlRigGraphNode->GetModelNodeName());
			}
			else if(UEdGraphNode* Node = Cast<UEdGraphNode>(Object))
			{
				if (UEdGraphNode_Comment* CommentNode = Cast<UEdGraphNode_Comment>(Node))
				{
					URigVMNode* ModelNode = GetFocusedModel()->FindNodeByName(Node->GetFName());
					if (ModelNode == nullptr)
					{
						TGuardValue<bool> BlueprintNotifGuard(ControlRigBlueprint->bSuspendModelNotificationsForOthers, true);
						FVector2D NodePos(CommentNode->NodePosX, CommentNode->NodePosY);
						FVector2D NodeSize(CommentNode->NodeWidth, CommentNode->NodeHeight);
						FLinearColor NodeColor = CommentNode->CommentColor;
						GetFocusedController()->AddCommentNode(CommentNode->NodeComment, NodePos, NodeSize, NodeColor, CommentNode->GetName(), true, true);
					}
				}
				NodeNamesToSelect.Add(Node->GetFName());
			}
		}
		GetFocusedController()->SetNodeSelection(NodeNamesToSelect, true, true);
	}
}

void FControlRigEditor::HandleHideItem()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	UControlRigBlueprint* ControlRigBlueprint = CastChecked<UControlRigBlueprint>(GetBlueprintObj());

	TSet<UObject*> SelectedNodes = GetSelectedNodes();
	if(SelectedNodes.Num() > 0)
	{
		FScopedTransaction Transaction(LOCTEXT("HideRigItem", "Hide rig item"));

		ControlRigBlueprint->Modify();

		for(UObject* SelectedNodeObject : SelectedNodes)
		{
			if(UControlRigGraphNode* SelectedNode = Cast<UControlRigGraphNode>(SelectedNodeObject))
			{
				FBlueprintEditorUtils::RemoveNode(ControlRigBlueprint, SelectedNode, true);
			}
		}
	}
}

bool FControlRigEditor::CanHideItem() const
{
	return GetNumberOfSelectedNodes() > 0;
}

void FControlRigEditor::OnBlueprintChangedImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (!bControlRigEditorInitialized)
	{
		return;
	}

	FBlueprintEditor::OnBlueprintChangedImpl(InBlueprint, bIsJustBeingCompiled);

	if(InBlueprint == GetBlueprintObj())
	{
		if(bIsJustBeingCompiled)
		{
			UpdateControlRig();

			if (!LastDebuggedRig.IsEmpty())
			{
				TArray<FCustomDebugObject> DebugList;
				GetCustomDebugObjects(DebugList);

				for (const FCustomDebugObject& DebugObject : DebugList)
				{
					if (DebugObject.NameOverride == LastDebuggedRig)
					{
						GetBlueprintObj()->SetObjectBeingDebugged(DebugObject.Object);
						LastDebuggedRig.Empty();
						break;
					}
				}
			}
		}
	}
}

void FControlRigEditor::RefreshEditors(ERefreshBlueprintEditorReason::Type Reason)
{
	if(Reason == ERefreshBlueprintEditorReason::UnknownReason)
	{
		// we mark the reason as just compiled since we don't want to
		// update the graph(s) all the time during compilation
		Reason = ERefreshBlueprintEditorReason::BlueprintCompiled;
	}
	IControlRigEditor::RefreshEditors(Reason);
}

void FControlRigEditor::HandleViewportCreated(const TSharedRef<class IPersonaViewport>& InViewport)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	// TODO: this is duplicated code from FAnimBlueprintEditor, would be nice to consolidate. 
	auto GetCompilationStateText = [this]()
	{
		if (UBlueprint* Blueprint = GetBlueprintObj())
		{
			switch (Blueprint->Status)
			{
			case BS_UpToDate:
			case BS_UpToDateWithWarnings:
				// Fall thru and return empty string
				break;
			case BS_Dirty:
				return LOCTEXT("ControlRigBP_Dirty", "Preview out of date");
			case BS_Error:
				return LOCTEXT("ControlRigBP_CompileError", "Compile Error");
			default:
				return LOCTEXT("ControlRigBP_UnknownStatus", "Unknown Status");
			}
		}

		return FText::GetEmpty();
	};

	auto GetCompilationStateVisibility = [this]()
	{
		if (UBlueprint* Blueprint = GetBlueprintObj())
		{
			const bool bUpToDate = (Blueprint->Status == BS_UpToDate) || (Blueprint->Status == BS_UpToDateWithWarnings);
			return bUpToDate ? EVisibility::Collapsed : EVisibility::Visible;
		}

		return EVisibility::Collapsed;
	};

	auto GetCompileButtonVisibility = [this]()
	{
		if (UBlueprint* Blueprint = GetBlueprintObj())
		{
			return (Blueprint->Status == BS_Dirty) ? EVisibility::Visible : EVisibility::Collapsed;
		}

		return EVisibility::Collapsed;
	};

	auto CompileBlueprint = [this]()
	{
		if (UBlueprint* Blueprint = GetBlueprintObj())
		{
			if (!Blueprint->IsUpToDate())
			{
				Compile();
			}
		}

		return FReply::Handled();
	};

	auto GetErrorSeverity = [this]()
	{
		if (UBlueprint* Blueprint = GetBlueprintObj())
		{
			return (Blueprint->Status == BS_Error) ? EMessageSeverity::Error : EMessageSeverity::Warning;
		}

		return EMessageSeverity::Warning;
	};

	auto GetIcon = [this]()
	{
		if (UBlueprint* Blueprint = GetBlueprintObj())
		{
			return (Blueprint->Status == BS_Error) ? FEditorFontGlyphs::Exclamation_Triangle : FEditorFontGlyphs::Eye;
		}

		return FEditorFontGlyphs::Eye;
	};

	auto GetChangingShapeTransformText = [this]()
	{
		if (FControlRigEditMode* EditMode = GetEditMode())
		{
			FText HotKeyText = EditMode->GetToggleControlShapeTransformEditHotKey();

			if (!HotKeyText.IsEmpty())
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("HotKey"), HotKeyText);
				return FText::Format(LOCTEXT("ControlRigBPViewportShapeTransformEditNotificationPress", "Currently Manipulating Shape Transform - Press {HotKey} to Exit"), Args);
			}
		}
		
		return LOCTEXT("ControlRigBPViewportShapeTransformEditNotificationAssign", "Currently Manipulating Shape Transform - Assign a Hotkey and Use It to Exit");
	};

	auto GetChangingShapeTransformTextVisibility = [this]()
	{
		if (FControlRigEditMode* EditMode = GetEditMode())
		{
			return EditMode->bIsChangingControlShapeTransform ? EVisibility::Visible : EVisibility::Collapsed;
		}
		return EVisibility::Collapsed;
	};

	InViewport->AddNotification(MakeAttributeLambda(GetErrorSeverity),
		false,
		SNew(SHorizontalBox)
		.Visibility_Lambda(GetCompilationStateVisibility)
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(4.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			.ToolTipText_Lambda(GetCompilationStateText)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(STextBlock)
				.TextStyle(FAppStyle::Get(), "AnimViewport.MessageText")
				.Font(FAppStyle::Get().GetFontStyle("FontAwesome.9"))
				.Text_Lambda(GetIcon)
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text_Lambda(GetCompilationStateText)
				.TextStyle(FAppStyle::Get(), "AnimViewport.MessageText")
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(SButton)
			.ForegroundColor(FSlateColor::UseForeground())
			.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
			.Visibility_Lambda(GetCompileButtonVisibility)
			.ToolTipText(LOCTEXT("ControlRigBPViewportCompileButtonToolTip", "Compile this Animation Blueprint to update the preview to reflect any recent changes."))
			.OnClicked_Lambda(CompileBlueprint)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "AnimViewport.MessageText")
					.Font(FAppStyle::Get().GetFontStyle("FontAwesome.9"))
					.Text(FEditorFontGlyphs::Cog)
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "AnimViewport.MessageText")
					.Text(LOCTEXT("ControlRigBPViewportCompileButtonLabel", "Compile"))
				]
			]
		],
		FPersonaViewportNotificationOptions(TAttribute<EVisibility>::Create(GetCompilationStateVisibility))
	);
	
	FPersonaViewportNotificationOptions ChangeShapeTransformNotificationOptions;
	ChangeShapeTransformNotificationOptions.OnGetVisibility = TAttribute<EVisibility>::Create(GetChangingShapeTransformTextVisibility);
	ChangeShapeTransformNotificationOptions.OnGetBrushOverride = TAttribute<const FSlateBrush*>(FControlRigEditorStyle::Get().GetBrush("ControlRig.Viewport.Notification.ChangeShapeTransform"));

	// notification that shows when users enter the mode that allows them to change shape transform
	InViewport->AddNotification(EMessageSeverity::Type::Info,
		false,
		SNew(SHorizontalBox)
		.Visibility_Lambda(GetChangingShapeTransformTextVisibility)
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(4.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			.ToolTipText_Lambda(GetChangingShapeTransformText)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text_Lambda(GetChangingShapeTransformText)
				.TextStyle(FAppStyle::Get(), "AnimViewport.MessageText")
			]
		],
		ChangeShapeTransformNotificationOptions
	);

	InViewport->AddToolbarExtender(TEXT("AnimViewportDefaultCamera"), FMenuExtensionDelegate::CreateLambda(
		[&](FMenuBuilder& InMenuBuilder)
		{
			InMenuBuilder.AddMenuSeparator(TEXT("Control Rig"));
			InMenuBuilder.BeginSection("ControlRig", LOCTEXT("ControlRig_Label", "Control Rig"));
			{
				InMenuBuilder.AddWidget(
					SNew(SBox)
					.HAlign(HAlign_Right)
					[
						SNew(SBox)
						.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
						.WidthOverride(100.0f)
						.IsEnabled(this, &FControlRigEditor::IsToolbarDrawNullsEnabled)
						[
							SNew(SCheckBox)
							.IsChecked(this, &FControlRigEditor::GetToolbarDrawNulls)
							.OnCheckStateChanged(this, &FControlRigEditor::OnToolbarDrawNullsChanged)
							.ToolTipText(LOCTEXT("ControlRigDrawNullsToolTip", "If checked all nulls are drawn as axes."))
						]
					],
					LOCTEXT("ControlRigDisplayNulls", "Display Nulls")
				);

				InMenuBuilder.AddWidget(
					SNew(SBox)
					.HAlign(HAlign_Right)
					[
						SNew(SBox)
						.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
						.WidthOverride(100.0f)
						[
							SNew(SCheckBox)
							.IsChecked(this, &FControlRigEditor::GetToolbarDrawAxesOnSelection)
							.OnCheckStateChanged(this, &FControlRigEditor::OnToolbarDrawAxesOnSelectionChanged)
							.ToolTipText(LOCTEXT("ControlRigDisplayAxesOnSelectionToolTip", "If checked axes will be drawn for all selected rig elements."))
						]
					],
					LOCTEXT("ControlRigDisplayAxesOnSelection", "Display Axes On Selection")
				);

				InMenuBuilder.AddWidget(
					SNew(SBox)
					.HAlign(HAlign_Right)
					[
						SNew(SBox)
						.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
						.WidthOverride(100.0f)
						[
							SNew(SNumericEntryBox<float>)
							.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
							.AllowSpin(true)
							.MinSliderValue(0.0f)
							.MaxSliderValue(100.0f)
							.Value(this, &FControlRigEditor::GetToolbarAxesScale)
							.OnValueChanged(this, &FControlRigEditor::OnToolbarAxesScaleChanged)
							.ToolTipText(LOCTEXT("ControlRigAxesScaleToolTip", "Scale of axes drawn for selected rig elements"))
						]
					], 
					LOCTEXT("ControlRigAxesScale", "Axes Scale")
				);

				if (UControlRigBlueprint* ControlRigBlueprint = CastChecked<UControlRigBlueprint>(GetBlueprintObj()))
				{
					for (UEdGraph* Graph : ControlRigBlueprint->UbergraphPages)
					{
						if (UControlRigGraph* RigGraph = Cast<UControlRigGraph>(Graph))
						{
							const TArray<TSharedPtr<FString>>* BoneNameList = RigGraph->GetBoneNameList();

							InMenuBuilder.AddWidget(
								SNew(SBox)
								.HAlign(HAlign_Right)
								[
									SNew(SBox)
									.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
									.WidthOverride(100.0f)
									.IsEnabled(this, &FControlRigEditor::IsPinControlNameListEnabled)
									[
										SAssignNew(PinControlNameList, SControlRigGraphPinNameListValueWidget)
										.OptionsSource(BoneNameList)
										.OnGenerateWidget(this, &FControlRigEditor::MakePinControlNameListItemWidget)
										.OnSelectionChanged(this, &FControlRigEditor::OnPinControlNameListChanged)
										.OnComboBoxOpening(this, &FControlRigEditor::OnPinControlNameListComboBox, BoneNameList)
										.InitiallySelectedItem(GetPinControlCurrentlySelectedItem(BoneNameList))
										.Content()
										[
											SNew(STextBlock)
											.Text(this, &FControlRigEditor::GetPinControlNameListText)
										]
									]
								],
								LOCTEXT("ControlRigAuthoringSpace", "Pin Control Space")
							);
							break;
						}
					}
				}
			}
			InMenuBuilder.EndSection();
		}
	));

	auto GetBorderColorAndOpacity = [this]()
	{
		FLinearColor Color = FLinearColor::Transparent;
		const TArray<FName> EventQueue = GetEventQueue();
		if(EventQueue == ConstructionEventQueue)
		{
			Color = UControlRigEditorSettings::Get()->ConstructionEventBorderColor;
		}
		if(EventQueue == BackwardsSolveEventQueue)
		{
			Color = UControlRigEditorSettings::Get()->BackwardsSolveBorderColor;
		}
		if(EventQueue == BackwardsAndForwardsSolveEventQueue)
		{
			Color = UControlRigEditorSettings::Get()->BackwardsAndForwardsBorderColor;
		}
		return Color;
	};

	auto GetBorderVisibility = [this]()
	{
		EVisibility Visibility = EVisibility::Collapsed;
		if (GetEventQueueComboValue() != 0)
		{
			Visibility = EVisibility::HitTestInvisible;
		}
		return Visibility;
	};
	
	InViewport->AddOverlayWidget(
		SNew(SBorder)
        .BorderImage(FControlRigEditorStyle::Get().GetBrush( "ControlRig.Viewport.Border"))
        .BorderBackgroundColor_Lambda(GetBorderColorAndOpacity)
        .Visibility_Lambda(GetBorderVisibility)
        .Padding(0.0f)
        .ShowEffectWhenDisabled(false)
	);
	
	InViewport->GetKeyDownDelegate().BindLambda([&](const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) -> FReply {
		if (OnKeyDownDelegate.IsBound())
		{
			return OnKeyDownDelegate.Execute(MyGeometry, InKeyEvent);
		}
		return FReply::Unhandled();
	});

	// register callbacks to allow control rig asset to store the Bone Size viewport setting
	FEditorViewportClient& ViewportClient = InViewport->GetViewportClient();
	if (FAnimationViewportClient* AnimViewportClient = static_cast<FAnimationViewportClient*>(&ViewportClient))
	{
		AnimViewportClient->OnSetBoneSize.BindLambda([this](float InBoneSize)
		{
			if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
			{
				RigBlueprint->Modify();
				RigBlueprint->DebugBoneRadius = InBoneSize;
			}
		});
		
		AnimViewportClient->OnGetBoneSize.BindLambda([this]() -> float
		{
			if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
			{
				return RigBlueprint->DebugBoneRadius;
			}

			return 1.0f;
		});
	}
}

TOptional<float> FControlRigEditor::GetToolbarAxesScale() const
{
	if (const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>())
	{
		return Settings->AxisScale;
	}
	return 0.f;
}

void FControlRigEditor::OnToolbarAxesScaleChanged(float InValue)
{
	if (UControlRigEditModeSettings* Settings = GetMutableDefault<UControlRigEditModeSettings>())
	{
		Settings->AxisScale = InValue;
	}
}

ECheckBoxState FControlRigEditor::GetToolbarDrawAxesOnSelection() const
{
	if (const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>())
	{
		return Settings->bDisplayAxesOnSelection ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void FControlRigEditor::OnToolbarDrawAxesOnSelectionChanged(ECheckBoxState InNewValue)
{
	if (UControlRigEditModeSettings* Settings = GetMutableDefault<UControlRigEditModeSettings>())
	{
		Settings->bDisplayAxesOnSelection = InNewValue == ECheckBoxState::Checked;
	}
}

bool FControlRigEditor::IsToolbarDrawNullsEnabled() const
{
	if (ControlRig)
	{
		if (!ControlRig->IsConstructionModeEnabled())
		{
			return true;
		}
	}
	return false;
}

ECheckBoxState FControlRigEditor::GetToolbarDrawNulls() const
{
	if (const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>())
	{
		return Settings->bDisplayNulls ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void FControlRigEditor::OnToolbarDrawNullsChanged(ECheckBoxState InNewValue)
{
	if (UControlRigEditModeSettings* Settings = GetMutableDefault<UControlRigEditModeSettings>())
	{
		Settings->bDisplayNulls = InNewValue == ECheckBoxState::Checked;
	}
}

bool FControlRigEditor::IsPinControlNameListEnabled() const
{
	if (ControlRig)
	{
		TArray<FRigControlElement*>	TransientControls = ControlRig->GetHierarchy()->GetTransientControls();
		if (TransientControls.Num() > 0)
		{
			// if the transient control is not for a rig element, it is for a pin
			if (UControlRig::GetElementKeyFromTransientControl(TransientControls[0]->GetKey()) == FRigElementKey())
			{
				return true;
			}
		}
	}
	return false;
}

TSharedRef<SWidget> FControlRigEditor::MakePinControlNameListItemWidget(TSharedPtr<FString> InItem)
{
	return 	SNew(STextBlock).Text(FText::FromString(*InItem));
}

FText FControlRigEditor::GetPinControlNameListText() const
{
	if (ControlRig)
	{
		FText Result;
		ControlRig->GetHierarchy()->ForEach<FRigControlElement>([this, &Result](FRigControlElement* ControlElement) -> bool
        {
			if (ControlElement->Settings.bIsTransientControl)
			{
				FRigElementKey Parent = ControlRig->GetHierarchy()->GetFirstParent(ControlElement->GetKey());
				Result = FText::FromName(Parent.Name);
				
				return false;
			}
			return true;
		});
		
		if(!Result.IsEmpty())
		{
			return Result;
		}
	}
	return FText::FromName(NAME_None);
}

TSharedPtr<FString> FControlRigEditor::GetPinControlCurrentlySelectedItem(const TArray<TSharedPtr<FString>>* InNameList) const
{
	FString CurrentItem = GetPinControlNameListText().ToString();
	for (const TSharedPtr<FString>& Item : *InNameList)
	{
		if (Item->Equals(CurrentItem))
		{
			return Item;
		}
	}
	return TSharedPtr<FString>();
}

void FControlRigEditor::SetPinControlNameListText(const FText& NewTypeInValue, ETextCommit::Type /*CommitInfo*/)
{
	if (ControlRig)
	{
		ControlRig->GetHierarchy()->ForEach<FRigControlElement>([this, NewTypeInValue](FRigControlElement* ControlElement) -> bool
        {
            if (ControlElement->Settings.bIsTransientControl)
			{
				FName NewParentName = *NewTypeInValue.ToString();
				const int32 NewParentIndex = ControlRig->GetHierarchy()->GetIndex(FRigElementKey(NewParentName, ERigElementType::Bone));
				if (NewParentIndex == INDEX_NONE)
				{
					NewParentName = NAME_None;
					ControlRig->GetHierarchy()->GetController()->RemoveAllParents(ControlElement->GetKey(), true, false);
				}
				else
				{
					ControlRig->GetHierarchy()->GetController()->SetParent(ControlElement->GetKey(), FRigElementKey(NewParentName, ERigElementType::Bone), true, false);
				}

				// find out if the controlled pin is part of a visual debug node
				if (UControlRigBlueprint* ControlRigBlueprint = CastChecked<UControlRigBlueprint>(GetBlueprintObj()))
				{
					FString PinName = UControlRig::GetPinNameFromTransientControl(ControlElement->GetKey());
					if (URigVMPin* ControlledPin = GetFocusedModel()->FindPin(PinName))
					{
						URigVMNode* ControlledNode = ControlledPin->GetPinForLink()->GetNode();
						if (URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(ControlledNode))
						{
							if (const FString* Value = UnitNode->GetScriptStruct()->FindMetaData(FRigVMStruct::TemplateNameMetaName))
							{
								if (Value->Equals("VisualDebug"))
								{
									if (URigVMPin* SpacePin = ControlledNode->FindPin(TEXT("Space")))
									{
										FString DefaultValue;
										const FRigElementKey NewSpaceKey(NewParentName, ERigElementType::Bone); 
										FRigElementKey::StaticStruct()->ExportText(DefaultValue, &NewSpaceKey, nullptr, nullptr, PPF_None, nullptr);
										ensure(GetFocusedController()->SetPinDefaultValue(SpacePin->GetPinPath(), DefaultValue, false, false, false));
									}
									else if (URigVMPin* BoneSpacePin = ControlledNode->FindPin(TEXT("BoneSpace")))
									{
										if (BoneSpacePin->GetCPPType() == TEXT("FName") && BoneSpacePin->GetCustomWidgetName() == TEXT("BoneName"))
										{
											GetFocusedController()->SetPinDefaultValue(BoneSpacePin->GetPinPath(), NewParentName.ToString(), false, false, false);
										}
									}
								}
							}
						}
					}
				}
			}
			return true;
		});
	}
}

void FControlRigEditor::OnPinControlNameListChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::Direct)
	{
		FString NewValue = *NewSelection.Get();
		SetPinControlNameListText(FText::FromString(NewValue), ETextCommit::OnEnter);
	}
}

void FControlRigEditor::OnPinControlNameListComboBox(const TArray<TSharedPtr<FString>>* InNameList)
{
	TSharedPtr<FString> CurrentlySelected = GetPinControlCurrentlySelectedItem(InNameList);
	PinControlNameList->SetSelectedItem(CurrentlySelected);
}

bool FControlRigEditor::IsConstructionModeEnabled() const
{
	if(ControlRig)
	{
		return ControlRig->IsConstructionModeEnabled();
	}
	return false;
}

void FControlRigEditor::HandlePreviewSceneCreated(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	// load a ground mesh
	static const TCHAR* GroundAssetPath = TEXT("/Engine/MapTemplates/SM_Template_Map_Floor.SM_Template_Map_Floor");
	UStaticMesh* FloorMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), NULL, GroundAssetPath, NULL, LOAD_None, NULL));
	UMaterial* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
	check(FloorMesh);
	check(DefaultMaterial);

	// create ground mesh actor
	AStaticMeshActor* GroundActor = InPersonaPreviewScene->GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform::Identity);
	GroundActor->SetFlags(RF_Transient);
	GroundActor->GetStaticMeshComponent()->SetStaticMesh(FloorMesh);
	GroundActor->GetStaticMeshComponent()->SetMaterial(0, DefaultMaterial);
	GroundActor->SetMobility(EComponentMobility::Static);
	GroundActor->GetStaticMeshComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GroundActor->GetStaticMeshComponent()->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	GroundActor->GetStaticMeshComponent()->bSelectable = false;
	// this will be an invisible collision box that users can use to test traces
	GroundActor->GetStaticMeshComponent()->SetVisibility(false);

	WeakGroundActorPtr = GroundActor;

	AAnimationEditorPreviewActor* Actor = InPersonaPreviewScene->GetWorld()->SpawnActor<AAnimationEditorPreviewActor>(AAnimationEditorPreviewActor::StaticClass(), FTransform::Identity);
	Actor->SetFlags(RF_Transient);
	InPersonaPreviewScene->SetActor(Actor);

	// Create the preview component
	UControlRigSkeletalMeshComponent* EditorSkelComp = NewObject<UControlRigSkeletalMeshComponent>(Actor);
	EditorSkelComp->SetSkeletalMesh(InPersonaPreviewScene->GetPersonaToolkit()->GetPreviewMesh());
	InPersonaPreviewScene->SetPreviewMeshComponent(EditorSkelComp);
	bool bWasCreated = false;
	FAnimCustomInstanceHelper::BindToSkeletalMeshComponent<UControlRigLayerInstance>(EditorSkelComp, bWasCreated);
	InPersonaPreviewScene->AddComponent(EditorSkelComp, FTransform::Identity);

	// set root component, so we can attach to it. 
	Actor->SetRootComponent(EditorSkelComp);
	EditorSkelComp->bSelectable = false;
	EditorSkelComp->MarkRenderStateDirty();
	
	InPersonaPreviewScene->SetAllowMeshHitProxies(false);
	InPersonaPreviewScene->SetAdditionalMeshesSelectable(false);

	PreviewInstance = nullptr;
	if (UControlRigLayerInstance* ControlRigLayerInstance = Cast<UControlRigLayerInstance>(EditorSkelComp->GetAnimInstance()))
	{
		PreviewInstance = Cast<UAnimPreviewInstance>(ControlRigLayerInstance->GetSourceAnimInstance());
	}
	else
	{
		PreviewInstance = Cast<UAnimPreviewInstance>(EditorSkelComp->GetAnimInstance());
	}

	if (GEditor)
	{
		// remove the preview scene undo handling - it has unwanted side effects
		FAnimationEditorPreviewScene* AnimationEditorPreviewScene = static_cast<FAnimationEditorPreviewScene*>(&InPersonaPreviewScene.Get());
		if (AnimationEditorPreviewScene)
		{
			GEditor->UnregisterForUndo(AnimationEditorPreviewScene);
		}
	}
}

void FControlRigEditor::UpdateControlRig()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	if(UClass* Class = Blueprint->GeneratedClass)
	{
		UControlRigSkeletalMeshComponent* EditorSkelComp = Cast<UControlRigSkeletalMeshComponent>(GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent());
		UControlRigLayerInstance* AnimInstance = Cast<UControlRigLayerInstance>(EditorSkelComp->GetAnimInstance());

		if (AnimInstance)
		{
			if (ControlRig)
			{
				// if this control rig is from a temporary step,
				// for example the reinstancing class, clear it 
				// and create a new one!
				if (ControlRig->GetClass() != Class)
				{
					ControlRig = nullptr;
				}
			}

			if (ControlRig == nullptr)
			{
				ControlRig = NewObject<UControlRig>(EditorSkelComp, Class);
				// this is editing time rig
				ControlRig->ExecutionType = ERigExecutionType::Editing;
				ControlRig->ControlRigLog = &ControlRigLog;

				ControlRig->Initialize(true);
 			}

			ControlRig->PreviewInstance = PreviewInstance;

#if WITH_EDITOR
			ControlRig->bIsInDebugMode = ExecutionMode == EControlRigExecutionModeType_Debug;
#endif

			if (UControlRig* CDO = Cast<UControlRig>(Class->GetDefaultObject()))
			{
				CDO->ShapeLibraries = GetControlRigBlueprint()->ShapeLibraries;
			}

			CacheNameLists();

			// When the control rig is re-instanced on compile, it loses its binding, so we refresh it here if needed
			if (!ControlRig->GetObjectBinding().IsValid())
			{
				ControlRig->SetObjectBinding(MakeShared<FControlRigObjectBinding>());
			}
			
			// Make sure the object being debugged is the preview instance
			GetBlueprintObj()->SetObjectBeingDebugged(ControlRig);

			// initialize is moved post reinstance
			AnimInstance->ResetControlRigTracks();
			AnimInstance->AddControlRigTrack(0, ControlRig);
			AnimInstance->UpdateControlRigTrack(0, 1.0f, FControlRigIOSettings::MakeEnabled(), bExecutionControlRig);
			AnimInstance->RecalcRequiredBones();

			// since rig has changed, rebuild draw skeleton
			EditorSkelComp->RebuildDebugDrawSkeleton();
			if (FControlRigEditMode* EditMode = GetEditMode())
			{
				EditMode->SetObjects(ControlRig, EditorSkelComp,nullptr);
			}

			if(!bIsCompilingThroughUI)
			{
				Blueprint->SetFlags(RF_Transient);
				Blueprint->RecompileVM();
				Blueprint->ClearFlags(RF_Transient);
			}

			ControlRig->OnInitialized_AnyThread().AddSP(this, &FControlRigEditor::HandleControlRigExecutedEvent);
			ControlRig->OnExecuted_AnyThread().AddSP(this, &FControlRigEditor::HandleControlRigExecutedEvent);
			ControlRig->RequestInit();
			ControlRig->ControlModified().AddSP(this, &FControlRigEditor::HandleOnControlModified);

			if (ControlRig->GetVM())
			{
				ControlRig->GetVM()->ExecutionHalted().AddSP(this, &FControlRigEditor::HandleControlRigExecutionHalted);
			}
		}
	}
}

void FControlRigEditor::CacheNameLists()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint())
	{
		TArray<UEdGraph*> EdGraphs;
		ControlRigBP->GetAllGraphs(EdGraphs);

		URigHierarchy* Hierarchy = GetHierarchyBeingDebugged();
		for (UEdGraph* Graph : EdGraphs)
		{
			UControlRigGraph* RigGraph = Cast<UControlRigGraph>(Graph);
			if (RigGraph == nullptr)
			{
				continue;
			}
			RigGraph->CacheNameLists(Hierarchy, &ControlRigBP->DrawContainer, ControlRigBP->ShapeLibraries);
		}
	}
}

void FControlRigEditor::AddReferencedObjects( FReferenceCollector& Collector )
{
	FBlueprintEditor::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(ControlRig);
}

void FControlRigEditor::HandlePreviewMeshChanged(USkeletalMesh* InOldSkeletalMesh, USkeletalMesh* InNewSkeletalMesh)
{
	RebindToSkeletalMeshComponent();

	if (GetObjectsCurrentlyBeingEdited()->Num() > 0)
	{
		if (UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint())
		{
			ControlRigBP->SetPreviewMesh(InNewSkeletalMesh);
			UpdateControlRig();
			
			if(UControlRig* DebuggedControlRig = Cast<UControlRig>(GetBlueprintObj()->GetObjectBeingDebugged()))
			{
				DebuggedControlRig->GetHierarchy()->Notify(ERigHierarchyNotification::HierarchyReset, nullptr);
				DebuggedControlRig->Initialize(true);
			}
		}

		Compile();
	}
}

void FControlRigEditor::RebindToSkeletalMeshComponent()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	UDebugSkelMeshComponent* MeshComponent = GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent();
	if (MeshComponent)
	{
		bool bWasCreated = false;
		FAnimCustomInstanceHelper::BindToSkeletalMeshComponent<UControlRigLayerInstance>(MeshComponent , bWasCreated);
	}
}

void FControlRigEditor::UpdateMeshInAnimInstance(USkeletalMesh* InNewSkeletalMesh)
{
	UControlRigSkeletalMeshComponent* EditorSkelComp = Cast<UControlRigSkeletalMeshComponent>(GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent());
	if (EditorSkelComp)
	{
		EditorSkelComp->GetAnimInstance()->CurrentSkeleton = InNewSkeletalMesh->GetSkeleton();
	}
}

void FControlRigEditor::UpdateStaleWatchedPins()
{
	UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint();
	if (ControlRigBP == nullptr)
	{
		return;
	}

	TSet<UEdGraphPin*> AllPins;
	uint16 WatchCount;

	// Find all unique pins being watched
	FKismetDebugUtilities::ForeachPinWatch(
		ControlRigBP,
		[&AllPins, &WatchCount](UEdGraphPin* Pin)
		{
			++WatchCount;
			if (Pin == nullptr)
			{
				return; // ~continue
			}

			UEdGraphNode* OwningNode = Pin->GetOwningNode();
			// during node reconstruction, dead pins get moved to the transient 
			// package (so just in case this blueprint got saved with dead pin watches)
			if (OwningNode == nullptr)
			{
				return; // ~continue
			}

			if (!OwningNode->Pins.Contains(Pin))
			{
				return; // ~continue
			}

			AllPins.Add(Pin);
		}
	);

	// Refresh watched pins with unique pins (throw away null or duplicate watches)
	if (WatchCount != AllPins.Num())
	{
		ControlRigBP->Status = BS_Dirty;
	}

	FKismetDebugUtilities::ClearPinWatches(ControlRigBP);

	TArray<URigVMGraph*> Models = ControlRigBP->GetAllModels();
	for (URigVMGraph* Model : Models)
	{
		for (URigVMNode* ModelNode : Model->GetNodes())
		{
			TArray<URigVMPin*> ModelPins = ModelNode->GetAllPinsRecursively();
			for (URigVMPin* ModelPin : ModelPins)
			{
				if (ModelPin->RequiresWatch())
				{
					ControlRigBP->GetController(Model)->SetPinIsWatched(ModelPin->GetPinPath(), false, false);
				}
			}
		}
	}
	for (UEdGraphPin* Pin : AllPins)
	{
		FKismetDebugUtilities::AddPinWatch(ControlRigBP, FBlueprintWatchedPin(Pin));
		UEdGraph* EdGraph = Pin->GetOwningNode()->GetGraph();
		ControlRigBP->GetController(EdGraph)->SetPinIsWatched(Pin->GetName(), true, false);
	}
}

void FControlRigEditor::SetupGraphEditorEvents(UEdGraph* InGraph, SGraphEditor::FGraphEditorEvents& InEvents)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FBlueprintEditor::SetupGraphEditorEvents(InGraph, InEvents);

	InEvents.OnCreateActionMenu = SGraphEditor::FOnCreateActionMenu::CreateSP(this, &FControlRigEditor::HandleCreateGraphActionMenu);
	InEvents.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &FControlRigEditor::OnNodeTitleCommitted);
}

void FControlRigEditor::FocusInspectorOnGraphSelection(const TSet<UObject*>& NewSelection, bool bForceRefresh)
{
	// nothing to do here for control rig
}

FActionMenuContent FControlRigEditor::HandleCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed)
{
	return FBlueprintEditor::OnCreateGraphActionMenu(InGraph, InNodePosition, InDraggedPins, bAutoExpand, InOnMenuClosed);
}

void FControlRigEditor::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (UEdGraphNode_Comment* CommentBeingChanged = Cast<UEdGraphNode_Comment>(NodeBeingChanged))
	{
		if (UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint())
		{
			GetFocusedController()->SetCommentTextByName(CommentBeingChanged->GetFName(), NewText.ToString(), CommentBeingChanged->FontSize, CommentBeingChanged->bCommentBubbleVisible, CommentBeingChanged->bColorCommentBubble, true, true);
		}
	}
}

FTransform FControlRigEditor::GetRigElementTransform(const FRigElementKey& InElement, bool bLocal, bool bOnDebugInstance) const
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if(bOnDebugInstance)
	{
		UControlRig* DebuggedControlRig = Cast<UControlRig>(GetBlueprintObj()->GetObjectBeingDebugged());
		if (DebuggedControlRig == nullptr)
		{
			DebuggedControlRig = ControlRig;
		}

		if (DebuggedControlRig)
		{
			if (bLocal)
			{
				return DebuggedControlRig->GetHierarchy()->GetLocalTransform(InElement);
			}
			return DebuggedControlRig->GetHierarchy()->GetGlobalTransform(InElement);
		}
	}

	if (bLocal)
	{
		return GetHierarchyBeingDebugged()->GetLocalTransform(InElement);
	}
	return GetHierarchyBeingDebugged()->GetGlobalTransform(InElement);
}

void FControlRigEditor::SetRigElementTransform(const FRigElementKey& InElement, const FTransform& InTransform, bool bLocal)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FScopedTransaction Transaction(LOCTEXT("Move Bone", "Move Bone transform"));
	UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint();
	ControlRigBP->Modify();

	switch (InElement.Type)
	{
		case ERigElementType::Bone:
		{
			FTransform Transform = InTransform;
			if (bLocal)
			{
				FTransform ParentTransform = FTransform::Identity;
				FRigElementKey ParentKey = ControlRigBP->Hierarchy->GetFirstParent(InElement);
				if (ParentKey.IsValid())
				{
					ParentTransform = GetRigElementTransform(ParentKey, false, false);
				}
				Transform = Transform * ParentTransform;
				Transform.NormalizeRotation();
			}

			ControlRigBP->Hierarchy->SetInitialGlobalTransform(InElement, Transform);
			ControlRigBP->Hierarchy->SetGlobalTransform(InElement, Transform);
			OnHierarchyChanged();
			break;
		}
		case ERigElementType::Control:
		{
			FTransform LocalTransform = InTransform;
			FTransform GlobalTransform = InTransform;
			if (!bLocal)
			{
				ControlRigBP->Hierarchy->SetGlobalTransform(InElement, InTransform);
				LocalTransform = ControlRigBP->Hierarchy->GetLocalTransform(InElement);
			}
			else
			{
				ControlRigBP->Hierarchy->SetLocalTransform(InElement, InTransform);
				GlobalTransform = ControlRigBP->Hierarchy->GetGlobalTransform(InElement);
			}
			ControlRigBP->Hierarchy->SetInitialLocalTransform(InElement, LocalTransform);
			ControlRigBP->Hierarchy->SetGlobalTransform(InElement, GlobalTransform);
			OnHierarchyChanged();
			break;
		}
		case ERigElementType::Null:
		{
			FTransform LocalTransform = InTransform;
			FTransform GlobalTransform = InTransform;
			if (!bLocal)
			{
				ControlRigBP->Hierarchy->SetGlobalTransform(InElement, InTransform);
				LocalTransform = ControlRigBP->Hierarchy->GetLocalTransform(InElement);
			}
			else
			{
				ControlRigBP->Hierarchy->SetLocalTransform(InElement, InTransform);
				GlobalTransform = ControlRigBP->Hierarchy->GetGlobalTransform(InElement);
			}

			ControlRigBP->Hierarchy->SetInitialLocalTransform(InElement, LocalTransform);
			ControlRigBP->Hierarchy->SetGlobalTransform(InElement, GlobalTransform);
			OnHierarchyChanged();
			break;
		}
		default:
		{
			ensureMsgf(false, TEXT("Unsupported RigElement Type : %d"), InElement.Type);
			break;
		}
	}
	
	UControlRigSkeletalMeshComponent* EditorSkelComp = Cast<UControlRigSkeletalMeshComponent>(GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent());
	if (EditorSkelComp)
	{
		EditorSkelComp->RebuildDebugDrawSkeleton();
	}
}

void FControlRigEditor::NotifyPreChange(FProperty* PropertyAboutToChange)
{
	FBlueprintEditor::NotifyPreChange(PropertyAboutToChange);

	if (UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint())
	{
		ControlRigBP->Modify();
	}
}

void FControlRigEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged)
{
	FBlueprintEditor::NotifyPostChange(PropertyChangedEvent, PropertyThatChanged);

	// we need to listen to changes for variables on the blueprint here since
	// OnFinishedChangingProperties is called only for top level property changes.
	// changes on a lower level property like transform under a user defined struct
	// only go through this.
	UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint();
	if(ControlRig && ControlRigBP)
	{
		bool bUseCDO = false; 
		if(PropertyChangedEvent.GetNumObjectsBeingEdited() == 1)
		{
			bUseCDO = PropertyChangedEvent.GetObjectBeingEdited(0)->HasAnyFlags(RF_ClassDefaultObject);
		}
		
		const FName VarName = PropertyChangedEvent.MemberProperty->GetFName();
		for(FBPVariableDescription& NewVariable : ControlRigBP->NewVariables)
		{
			if(NewVariable.VarName == VarName)
			{
				UpdateDefaultValueForVariable(NewVariable, bUseCDO);
				break;
			}
		}
	}
}

void FControlRigEditor::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint();

	if (ControlRigBP)
	{
		if (PropertyChangedEvent.MemberProperty->GetNameCPP() == GET_MEMBER_NAME_STRING_CHECKED(UControlRigBlueprint, VMCompileSettings))
		{
			ControlRigBP->RecompileVM();
			return;
		}

		if (PropertyChangedEvent.MemberProperty->GetNameCPP() == GET_MEMBER_NAME_STRING_CHECKED(UControlRigBlueprint, VMRuntimeSettings))
		{
			ControlRigBP->VMRuntimeSettings.Validate();
			ControlRigBP->PropagateRuntimeSettingsFromBPToInstances();
			return;
		}

		if (PropertyChangedEvent.MemberProperty->GetNameCPP() == GET_MEMBER_NAME_STRING_CHECKED(UControlRigBlueprint, HierarchySettings))
		{
			ControlRigBP->PropagateHierarchyFromBPToInstances();
			return;
		}

		if (PropertyChangedEvent.MemberProperty->GetNameCPP() == GET_MEMBER_NAME_STRING_CHECKED(UControlRigBlueprint, DrawContainer))
		{
			ControlRigBP->PropagateDrawInstructionsFromBPToInstances();
			return;
		}
	}
}

void FControlRigEditor::OnPropertyChanged(UObject* InObject, FPropertyChangedEvent& InEvent)
{
	UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint();

	if (ControlRigBP && InObject == ControlRigBP)
	{
		// if the models have changed - we may need to close a document
		if(InEvent.MemberProperty == ControlRigBP->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UControlRigBlueprint, RigVMClient)) ||
			InEvent.MemberProperty == ControlRigBP->GetClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UControlRigBlueprint, UbergraphPages)))
		{
			DocumentManager->CleanInvalidTabs();
		}
	}
}

void FControlRigEditor::OnWrappedPropertyChangedChainEvent(UDetailsViewWrapperObject* InWrapperObject, const FString& InPropertyPath, FPropertyChangedChainEvent& InPropertyChangedChainEvent)
{
	check(InWrapperObject);
	check(!WrapperObjects.IsEmpty());

	TGuardValue<bool> SuspendDetailsPanelRefresh(bSuspendDetailsPanelRefresh, true);

	UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint();

	FString PropertyPath = InPropertyPath;
	if(UScriptStruct* WrappedStruct = InWrapperObject->GetWrappedStruct())
	{
		if(WrappedStruct->IsChildOf(FRigBaseElement::StaticStruct()))
		{
			check(WrappedStruct == WrapperObjects[0]->GetWrappedStruct());

			URigHierarchy* Hierarchy = CastChecked<URigHierarchy>(InWrapperObject->GetOuter());
			const FRigBaseElement WrappedElement = InWrapperObject->GetContent<FRigBaseElement>();
			const FRigBaseElement FirstWrappedElement = WrapperObjects[0]->GetContent<FRigBaseElement>();
			const FRigElementKey& Key = WrappedElement.GetKey();
			if(!Hierarchy->Contains(Key))
			{
				return;
			}

			static constexpr TCHAR PropertyChainElementFormat[] = TEXT("%s->");
			static const FString PoseString = FString::Printf(PropertyChainElementFormat, GET_MEMBER_NAME_STRING_CHECKED(FRigTransformElement, Pose));
			static const FString OffsetString = FString::Printf(PropertyChainElementFormat, GET_MEMBER_NAME_STRING_CHECKED(FRigControlElement, Offset));
			static const FString ShapeString = FString::Printf(PropertyChainElementFormat, GET_MEMBER_NAME_STRING_CHECKED(FRigControlElement, Shape));
			static const FString SettingsString = FString::Printf(PropertyChainElementFormat, GET_MEMBER_NAME_STRING_CHECKED(FRigControlElement, Settings));

			struct Local
			{
				static ERigTransformType::Type GetTransformTypeFromPath(FString& PropertyPath)
				{
					static const FString InitialString = FString::Printf(PropertyChainElementFormat, GET_MEMBER_NAME_STRING_CHECKED(FRigCurrentAndInitialTransform, Initial));
					static const FString CurrentString = FString::Printf(PropertyChainElementFormat, GET_MEMBER_NAME_STRING_CHECKED(FRigCurrentAndInitialTransform, Current));
					static const FString GlobalString = FString::Printf(PropertyChainElementFormat, GET_MEMBER_NAME_STRING_CHECKED(FRigLocalAndGlobalTransform, Global));
					static const FString LocalString = FString::Printf(PropertyChainElementFormat, GET_MEMBER_NAME_STRING_CHECKED(FRigLocalAndGlobalTransform, Local));

					ERigTransformType::Type TransformType = ERigTransformType::CurrentLocal;

					if(PropertyPath.RemoveFromStart(InitialString))
					{
						TransformType = ERigTransformType::MakeInitial(TransformType);
					}
					else
					{
						verify(PropertyPath.RemoveFromStart(CurrentString));
						TransformType = ERigTransformType::MakeCurrent(TransformType);
					}

					if(PropertyPath.RemoveFromStart(GlobalString))
					{
						TransformType = ERigTransformType::MakeGlobal(TransformType);
					}
					else
					{
						verify(PropertyPath.RemoveFromStart(LocalString));
						TransformType = ERigTransformType::MakeLocal(TransformType);
					}

					return TransformType;
				}
			};

			bool bIsInitial = false;
			if(PropertyPath.RemoveFromStart(PoseString))
			{
				const ERigTransformType::Type TransformType = Local::GetTransformTypeFromPath(PropertyPath);
				bIsInitial = bIsInitial || ERigTransformType::IsInitial(TransformType);
				
				if(ERigTransformType::IsInitial(TransformType) || IsConstructionModeEnabled())
				{
					Hierarchy = ControlRigBP->Hierarchy;
				}

				FRigTransformElement* TransformElement = Hierarchy->Find<FRigTransformElement>(WrappedElement.GetKey());
				if(TransformElement == nullptr)
				{
					return;
				}

				const FTransform Transform = InWrapperObject->GetContent<FRigTransformElement>().Pose.Get(TransformType);

				if(ERigTransformType::IsLocal(TransformType) && TransformElement->IsA<FRigControlElement>())
				{
					FRigControlElement* ControlElement = Cast<FRigControlElement>(TransformElement);
							
					FRigControlValue Value;
					Value.SetFromTransform(Transform, ControlElement->Settings.ControlType, ControlElement->Settings.PrimaryAxis);
							
					if(ERigTransformType::IsInitial(TransformType) || IsConstructionModeEnabled())
					{
						Hierarchy->SetControlValue(ControlElement, Value, ERigControlValueType::Initial, true, true, true);
					}
					Hierarchy->SetControlValue(ControlElement, Value, ERigControlValueType::Current, true, true, true);
				}
				else
				{
					Hierarchy->SetTransform(TransformElement, Transform, TransformType, true, true, false, true);
				}
			}
			else if(PropertyPath.RemoveFromStart(OffsetString))
			{
				FRigControlElement* ControlElement = ControlRigBP->Hierarchy->Find<FRigControlElement>(WrappedElement.GetKey());
				if(ControlElement == nullptr)
				{
					return;
				}
				
				ERigTransformType::Type TransformType = Local::GetTransformTypeFromPath(PropertyPath);
				bIsInitial = bIsInitial || ERigTransformType::IsInitial(TransformType);

				const FTransform Transform = WrapperObjects[0]->GetContent<FRigControlElement>().Offset.Get(TransformType);
				
				ControlRigBP->Hierarchy->SetControlOffsetTransform(ControlElement, Transform, ERigTransformType::MakeInitial(TransformType), true, true, false, true);
			}
			else if(PropertyPath.RemoveFromStart(ShapeString))
			{
				FRigControlElement* ControlElement = ControlRigBP->Hierarchy->Find<FRigControlElement>(WrappedElement.GetKey());
				if(ControlElement == nullptr)
				{
					return;
				}

				ERigTransformType::Type TransformType = Local::GetTransformTypeFromPath(PropertyPath);
				bIsInitial = bIsInitial || ERigTransformType::IsInitial(TransformType);

				const FTransform Transform = WrapperObjects[0]->GetContent<FRigControlElement>().Shape.Get(TransformType);
				
				ControlRigBP->Hierarchy->SetControlShapeTransform(ControlElement, Transform, ERigTransformType::MakeInitial(TransformType), true, false, true);
			}
			else if(PropertyPath.RemoveFromStart(SettingsString))
			{
				const FRigControlSettings Settings  = InWrapperObject->GetContent<FRigControlElement>().Settings;

				FRigControlElement* ControlElement = ControlRigBP->Hierarchy->Find<FRigControlElement>(WrappedElement.GetKey());
				if(ControlElement == nullptr)
				{
					return;
				}

				ControlRigBP->Hierarchy->SetControlSettings(ControlElement, Settings, true, false, true);
			}

			if(IsConstructionModeEnabled() || bIsInitial)
			{
				ControlRigBP->PropagatePoseFromBPToInstances();
				ControlRigBP->Modify();
				ControlRigBP->MarkPackageDirty();
			}
		}
		else if(WrappedStruct->IsChildOf(FRigVMGraphVariableDescription::StaticStruct()))
		{
			check(WrappedStruct == WrapperObjects[0]->GetWrappedStruct());
			
			const FRigVMGraphVariableDescription VariableDescription = InWrapperObject->GetContent<FRigVMGraphVariableDescription>();
			URigVMGraph* Graph = CastChecked<URigVMGraph>(InWrapperObject->GetOuter());
			URigVMController* Controller = ControlRigBP->GetController(Graph);
			if (PropertyPath == TEXT("Name") && MyBlueprintWidget.IsValid())
			{
				if (FEdGraphSchemaAction_BlueprintVariableBase* VariableAcion = MyBlueprintWidget->SelectionAsBlueprintVariable())
				{
					const FName OldVariableName = VariableAcion->GetVariableName();
					if (!OldVariableName.IsNone())
					{
						for (FRigVMGraphVariableDescription& Variable : Graph->GetLocalVariables())
						{
							if (Variable.Name == OldVariableName)
							{
								Controller->RenameLocalVariable(OldVariableName, VariableDescription.Name);
								break;
							}
						}
					}
				}
				RefreshMyBlueprint();
				GetControlRigBlueprint()->RequestAutoVMRecompilation();
			}
			else if (PropertyPath == TEXT("CPPType") || PropertyPath == TEXT("CPPTypeObject"))
			{			
				for (FRigVMGraphVariableDescription& Variable : Graph->GetLocalVariables())
				{
					if (Variable.Name == VariableDescription.Name)
					{
						Controller->SetLocalVariableType(Variable.Name, VariableDescription.CPPType, VariableDescription.CPPTypeObject);
						break;
					}
				}
				GetControlRigBlueprint()->RequestAutoVMRecompilation();
			}
			else if (PropertyPath == TEXT("DefaultValue"))
			{			
				for (FRigVMGraphVariableDescription& Variable : Graph->GetLocalVariables())
				{
					if (Variable.Name == VariableDescription.Name)
					{
						Controller->SetLocalVariableDefaultValue(Variable.Name, VariableDescription.DefaultValue, true, true, false);
						break;
					}
				}

				// Do not recompile now! That destroys the object that is currently being displayed (the literal memory storage), and can cause a crash.
				// The user has to manually trigger the recompilation.
			}		
		}
	}
	else if(!InWrapperObject->GetWrappedNodeNotation().IsEmpty())
	{
		FName RootPinName = InPropertyChangedChainEvent.PropertyChain.GetHead()->GetValue()->GetFName();
		FProperty* TargetProperty = WrapperObjects[0]->GetClass()->FindPropertyByName(RootPinName);
		uint8* FirstPropertyStorage = TargetProperty->ContainerPtrToValuePtr<uint8>(WrapperObjects[0].Get());

		URigVMNode* Node = CastChecked<URigVMNode>(InWrapperObject->GetOuter());

		FString DefaultValue = FRigVMStruct::ExportToFullyQualifiedText(TargetProperty, FirstPropertyStorage);

		if(TargetProperty->IsA<FStrProperty>() || TargetProperty->IsA<FNameProperty>())
		{
			DefaultValue.TrimCharInline(TEXT('\"'), nullptr);
		}
		
		URigVMController* Controller = GetControlRigBlueprint()->GetController(Node->GetGraph());

		if (!DefaultValue.IsEmpty())
		{
			FString PinPath = FString::Printf(TEXT("%s.%s"), *Node->GetName(), *RootPinName.ToString());
			const bool bInteractive = InPropertyChangedChainEvent.ChangeType == EPropertyChangeType::Interactive;
			Controller->SetPinDefaultValue(PinPath, DefaultValue, true, !bInteractive, true, !bInteractive);
		}
	}
}

void FControlRigEditor::OnRequestLocalizeFunctionDialog(URigVMLibraryNode* InFunction, UControlRigBlueprint* InTargetBlueprint, bool bForce)
{
	check(InFunction);
	check(InTargetBlueprint);

	if(InTargetBlueprint != GetControlRigBlueprint())
	{
		return;
	}
	
	if(URigVMController* TargetController = InTargetBlueprint->GetController(InTargetBlueprint->GetDefaultModel()))
	{
		if(URigVMFunctionLibrary* FunctionLibrary = Cast<URigVMFunctionLibrary>(InFunction->GetOuter()))
		{
			if(UControlRigBlueprint* FunctionRigBlueprint = Cast<UControlRigBlueprint>(FunctionLibrary->GetOuter()))
			{
				if(FunctionRigBlueprint != InTargetBlueprint)
				{
					if(bForce || !FunctionRigBlueprint->IsFunctionPublic(InFunction->GetFName()))
					{
                        TSharedRef<SControlRigFunctionLocalizationDialog> LocalizationDialog = SNew(SControlRigFunctionLocalizationDialog)
                        .Function(InFunction)
                        .TargetBlueprint(InTargetBlueprint);

						if (LocalizationDialog->ShowModal() != EAppReturnType::Cancel)
						{
							TargetController->LocalizeFunctions(LocalizationDialog->GetFunctionsToLocalize(), true, true, true);
						}
					}
				}
			}
		}
	}
}

FRigVMController_BulkEditResult FControlRigEditor::OnRequestBulkEditDialog(UControlRigBlueprint* InBlueprint, URigVMController* InController,
	URigVMLibraryNode* InFunction, ERigVMControllerBulkEditType InEditType)
{
	const TArray<FAssetData> FirstLevelReferenceAssets = InController->GetAffectedAssets(InEditType, false, true);
	if(FirstLevelReferenceAssets.Num() == 0)
	{
		return FRigVMController_BulkEditResult();
	}
	
	TSharedRef<SControlRigFunctionBulkEditDialog> BulkEditDialog = SNew(SControlRigFunctionBulkEditDialog)
	.Blueprint(InBlueprint)
	.Controller(InController)
	.Function(InFunction)
	.EditType(InEditType);

	FRigVMController_BulkEditResult Result;
	Result.bCanceled = BulkEditDialog->ShowModal() == EAppReturnType::Cancel; 
	Result.bSetupUndoRedo = false;
	return Result;
}

bool FControlRigEditor::OnRequestBreakLinksDialog(TArray<URigVMLink*> InLinks)
{
	if(InLinks.Num() == 0)
	{
		return true;
	}

	TSharedRef<SControlRigBreakLinksDialog> BreakLinksDialog = SNew(SControlRigBreakLinksDialog)
	.Links(InLinks)
	.OnFocusOnLink(FControlRigOnFocusOnLinkRequestedDelegate::CreateLambda([&](URigVMLink* InLink)
	{
		HandleJumpToHyperlink(InLink);
	}));

	return BreakLinksDialog->ShowModal() == EAppReturnType::Ok; 
}

void FControlRigEditor::HandleJumpToHyperlink(const UObject* InSubject)
{
	UControlRigBlueprint* RigBlueprint = GetControlRigBlueprint();
	if(RigBlueprint == nullptr)
	{
		return;
	}

	const URigVMGraph* GraphToJumpTo = nullptr;
	const URigVMNode* NodeToJumpTo = nullptr;
	const URigVMPin* PinToJumpTo = nullptr;
	if(const URigVMNode* Node = Cast<URigVMNode>(InSubject))
	{
		GraphToJumpTo = Node->GetGraph();
		NodeToJumpTo = Node;
	}
	else if(const URigVMPin* Pin = Cast<URigVMPin>(InSubject))
	{
		GraphToJumpTo = Pin->GetGraph();
		NodeToJumpTo = Pin->GetNode();
		PinToJumpTo = Pin;
	}
	else if(const URigVMLink* Link = Cast<URigVMLink>(InSubject))
	{
		GraphToJumpTo = Link->GetGraph();
		if(const URigVMPin* TargetPin = ((URigVMLink*)Link)->GetTargetPin())
		{
			NodeToJumpTo = TargetPin->GetNode();
			PinToJumpTo = TargetPin;
		}
	}

	if (GraphToJumpTo && NodeToJumpTo)
	{
		if(UControlRigGraph* EdGraph = Cast<UControlRigGraph>(RigBlueprint->GetEdGraph(NodeToJumpTo->GetGraph())))
		{
			if(const UControlRigGraphNode* EdGraphNode = Cast<UControlRigGraphNode>(EdGraph->FindNodeForModelNodeName(NodeToJumpTo->GetFName())))
			{
				if(PinToJumpTo)
				{
					if(const UEdGraphPin* EdGraphPin = EdGraphNode->FindPin(PinToJumpTo->GetSegmentPath(true)))
					{
						JumpToPin(EdGraphPin);
						return;
					}
				}
				
				JumpToNode(EdGraphNode);
				return;
			}
			
			JumpToHyperlink(EdGraph);
		}
	}
}

bool FControlRigEditor::UpdateDefaultValueForVariable(FBPVariableDescription& InVariable, bool bUseCDO)
{
	bool bAnyValueChanged = false;
	if (UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint())
	{
		UClass* GeneratedClass = ControlRigBP->GeneratedClass;
		UObject* ObjectContainer = bUseCDO ? GeneratedClass->GetDefaultObject() : ControlRigBP->GetObjectBeingDebugged();
		if(ObjectContainer)
		{
			FProperty* TargetProperty = FindFProperty<FProperty>(GeneratedClass, InVariable.VarName);

			if (TargetProperty)
			{
				FString NewDefaultValue;
				const uint8* Container = (const uint8*)ObjectContainer;
				FBlueprintEditorUtils::PropertyValueToString(TargetProperty, Container, NewDefaultValue, nullptr);
				if (InVariable.DefaultValue != NewDefaultValue)
				{
					InVariable.DefaultValue = NewDefaultValue;
					bAnyValueChanged = true;
				}
			}
		}
	}
	return bAnyValueChanged;
}

bool FControlRigEditor::SelectLocalVariable(const UEdGraph* Graph, const FName& VariableName)
{
	if (const UControlRigGraph* ControlRigGraph = Cast<UControlRigGraph>(Graph))
	{
		if (URigVMGraph* RigVMGraph = ControlRigGraph->GetModel())
		{
			for (FRigVMGraphVariableDescription& Variable : RigVMGraph->GetLocalVariables())
			{
				if (Variable.Name == VariableName)
				{
					UDetailsViewWrapperObject* WrapperObject = UDetailsViewWrapperObject::MakeInstance(
						Variable.StaticStruct(), (uint8*)&Variable, RigVMGraph);
					WrapperObject->GetWrappedPropertyChangedChainEvent().AddSP(this, &FControlRigEditor::OnWrappedPropertyChangedChainEvent);
					WrapperObject->AddToRoot();

					TArray<UObject*> Objects = {WrapperObject};
					SetDetailObjects(Objects, false);
					return true;
				}
			}
		}
	}
	return false;
}

void FControlRigEditor::OnCreateComment()
{
	TSharedPtr<SGraphEditor> GraphEditor = FocusedGraphEdPtr.Pin();
	if (GraphEditor.IsValid())
	{
		if (UEdGraph* Graph = GraphEditor->GetCurrentGraph())
		{
			FEdGraphSchemaAction_K2AddComment CommentAction;
			CommentAction.PerformAction(Graph, NULL, GraphEditor->GetPasteLocation());
		}
	}
}

void FControlRigEditor::OnHierarchyChanged()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint())
	{
		{
			TGuardValue<bool> GuardNotifs(ControlRigBP->bSuspendAllNotifications, true);
			ControlRigBP->PropagateHierarchyFromBPToInstances();
		}

		FBlueprintEditorUtils::MarkBlueprintAsModified(GetControlRigBlueprint());
		
		TArray<const FRigBaseElement*> SelectedElements = GetHierarchyBeingDebugged()->GetSelectedElements();
		for(const FRigBaseElement* SelectedElement : SelectedElements)
		{
			ControlRigBP->Hierarchy->OnModified().Broadcast(ERigHierarchyNotification::ElementSelected, ControlRigBP->Hierarchy, SelectedElement);
		}
		GetControlRigBlueprint()->RequestAutoVMRecompilation();

		SynchronizeViewportBoneSelection();

		UControlRigSkeletalMeshComponent* EditorSkelComp = Cast<UControlRigSkeletalMeshComponent>(GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent());
		// since rig has changed, rebuild draw skeleton
		if (EditorSkelComp)
		{ 
			EditorSkelComp->RebuildDebugDrawSkeleton(); 
		}

		RefreshDetailView();
	}
	else
	{
		ClearDetailObject();
	}
	
	CacheNameLists();
}


void FControlRigEditor::OnHierarchyModified(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement)
{
	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	if(RigBlueprint == nullptr)
	{
		return;
	}

	if (RigBlueprint->bSuspendAllNotifications)
	{
		return;
	}

	if(InHierarchy != RigBlueprint->Hierarchy)
	{
		return;
	}

	switch(InNotif)
	{
		case ERigHierarchyNotification::ElementAdded:
		case ERigHierarchyNotification::ParentChanged:
		case ERigHierarchyNotification::HierarchyReset:
		{
			OnHierarchyChanged();
			break;
		}
		case ERigHierarchyNotification::ElementRemoved:
		{
			UEnum* RigElementTypeEnum = StaticEnum<ERigElementType>();
			if (RigElementTypeEnum == nullptr)
			{
				return;
			}

			CacheNameLists();

			const FString RemovedElementName = InElement->GetName().ToString();
			const ERigElementType RemovedElementType = InElement->GetType();

			TArray<UEdGraph*> EdGraphs;
			RigBlueprint->GetAllGraphs(EdGraphs);

			for (UEdGraph* Graph : EdGraphs)
			{
				UControlRigGraph* RigGraph = Cast<UControlRigGraph>(Graph);
				if (RigGraph == nullptr)
				{
					continue;
				}

				for (UEdGraphNode* Node : RigGraph->Nodes)
				{
					if (UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(Node))
					{
						if (URigVMNode* ModelNode = RigNode->GetModelNode())
						{
							TArray<URigVMPin*> ModelPins = ModelNode->GetAllPinsRecursively();
							for (URigVMPin* ModelPin : ModelPins)
							{
								if ((ModelPin->GetCPPType() == TEXT("FName") && ModelPin->GetCustomWidgetName() == TEXT("BoneName") && RemovedElementType == ERigElementType::Bone) ||
									(ModelPin->GetCPPType() == TEXT("FName") && ModelPin->GetCustomWidgetName() == TEXT("ControlName") && RemovedElementType == ERigElementType::Control) ||
									(ModelPin->GetCPPType() == TEXT("FName") && ModelPin->GetCustomWidgetName() == TEXT("SpaceName") && RemovedElementType == ERigElementType::Null) ||
									(ModelPin->GetCPPType() == TEXT("FName") && ModelPin->GetCustomWidgetName() == TEXT("CurveName") && RemovedElementType == ERigElementType::Curve))
								{
									if (ModelPin->GetDefaultValue() == RemovedElementName)
									{
										RigNode->ReconstructNode();
										break;
									}
								}
								else if (ModelPin->GetCPPTypeObject() == FRigElementKey::StaticStruct())
								{
									if (URigVMPin* TypePin = ModelPin->FindSubPin(TEXT("Type")))
									{
										FString TypeStr = TypePin->GetDefaultValue();
										int64 TypeValue = RigElementTypeEnum->GetValueByNameString(TypeStr);
										if (TypeValue == (int64)RemovedElementType)
										{
											if (URigVMPin* NamePin = ModelPin->FindSubPin(TEXT("Name")))
											{
												FString NameStr = NamePin->GetDefaultValue();
												if (NameStr == RemovedElementName)
												{
													RigNode->ReconstructNode();
													break;
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}

			OnHierarchyChanged();
			break;
		}
		case ERigHierarchyNotification::ElementRenamed:
		{
			UEnum* RigElementTypeEnum = StaticEnum<ERigElementType>();
			if (RigElementTypeEnum == nullptr)
			{
				return;
			}

			const FString OldNameStr = InHierarchy->GetPreviousName(InElement->GetKey()).ToString();
			const FString NewNameStr = InElement->GetName().ToString();
			const ERigElementType ElementType = InElement->GetType(); 

			TArray<UEdGraph*> EdGraphs;
			RigBlueprint->GetAllGraphs(EdGraphs);

			for (UEdGraph* Graph : EdGraphs)
			{
				UControlRigGraph* RigGraph = Cast<UControlRigGraph>(Graph);
				if (RigGraph == nullptr)
				{
					continue;
				}

				URigVMController* Controller = RigGraph->GetController();
				if(Controller == nullptr)
				{
					continue;
				}

				{
					FControlRigBlueprintVMCompileScope CompileScope(RigBlueprint);
					for (UEdGraphNode* Node : RigGraph->Nodes)
					{
						if (UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(Node))
						{
							if (URigVMNode* ModelNode = RigNode->GetModelNode())
							{
								TArray<URigVMPin*> ModelPins = ModelNode->GetAllPinsRecursively();
								for (URigVMPin * ModelPin : ModelPins)
								{
									if ((ModelPin->GetCPPType() == TEXT("FName") && ModelPin->GetCustomWidgetName() == TEXT("BoneName") && ElementType == ERigElementType::Bone) ||
										(ModelPin->GetCPPType() == TEXT("FName") && ModelPin->GetCustomWidgetName() == TEXT("ControlName") && ElementType == ERigElementType::Control) ||
										(ModelPin->GetCPPType() == TEXT("FName") && ModelPin->GetCustomWidgetName() == TEXT("SpaceName") && ElementType == ERigElementType::Null) ||
										(ModelPin->GetCPPType() == TEXT("FName") && ModelPin->GetCustomWidgetName() == TEXT("CurveName") && ElementType == ERigElementType::Curve))
									{
										if (ModelPin->GetDefaultValue() == OldNameStr)
										{
											Controller->SetPinDefaultValue(ModelPin->GetPinPath(), NewNameStr, false);
										}
									}
									else if (ModelPin->GetCPPTypeObject() == FRigElementKey::StaticStruct())
									{
										if (URigVMPin* TypePin = ModelPin->FindSubPin(TEXT("Type")))
										{
											const FString TypeStr = TypePin->GetDefaultValue();
											const int64 TypeValue = RigElementTypeEnum->GetValueByNameString(TypeStr);
											if (TypeValue == (int64)ElementType)
											{
												if (URigVMPin* NamePin = ModelPin->FindSubPin(TEXT("Name")))
												{
													FString NameStr = NamePin->GetDefaultValue();
													if (NameStr == OldNameStr)
													{
														Controller->SetPinDefaultValue(NamePin->GetPinPath(), NewNameStr);
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
				
			OnHierarchyChanged();

			break;
		}
		default:
		{
			break;
		}
	}
}

void FControlRigEditor::OnHierarchyModified_AnyThread(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement)
{
	if(bIsConstructionEventRunning)
	{
		return;
	}
	
	FRigElementKey Key;
	if(InElement)
	{
		Key = InElement->GetKey();
	}

	if(IsInGameThread())
	{
		UControlRigBlueprint* RigBlueprint = GetControlRigBlueprint();
		check(RigBlueprint);

		if(RigBlueprint->bSuspendAllNotifications)
		{
			return;
		}
	}

	TWeakObjectPtr<URigHierarchy> WeakHierarchy = InHierarchy;
	auto Task = [this, InNotif, WeakHierarchy, Key]()
    {
		if(!WeakHierarchy.IsValid())
    	{
    		return;
    	}

        FRigBaseElement* Element = WeakHierarchy.Get()->Find(Key);

		switch(InNotif)
		{
			case ERigHierarchyNotification::ElementSelected:
			case ERigHierarchyNotification::ElementDeselected:
			{
				if(Element)
				{
					const bool bSelected = InNotif == ERigHierarchyNotification::ElementSelected;

					if (Element->GetType() == ERigElementType::Bone)
					{
						SynchronizeViewportBoneSelection();
					}

					if (bSelected)
					{
						SetDetailViewForRigElements();
					}
					else
					{
						TArray<FRigElementKey> CurrentSelection = GetHierarchyBeingDebugged()->GetSelectedKeys();
						if (CurrentSelection.Num() > 0)
						{
							if(FRigBaseElement* LastSelectedElement = WeakHierarchy.Get()->Find(CurrentSelection.Last()))
							{
								OnHierarchyModified(ERigHierarchyNotification::ElementSelected,  WeakHierarchy.Get(), LastSelectedElement);
							}
						}
						else
						{
							ClearDetailObject();
						}
					}
				}						
				break;
			}
			case ERigHierarchyNotification::ElementAdded:
			case ERigHierarchyNotification::ElementRemoved:
			case ERigHierarchyNotification::ElementRenamed:
			case ERigHierarchyNotification::ParentChanged:
            case ERigHierarchyNotification::HierarchyReset:
			{
				CacheNameLists();
				break;
			}
			case ERigHierarchyNotification::ControlSettingChanged:
			{
				if(DetailViewShowsRigElement(Key))
				{
					UControlRigBlueprint* RigBlueprint = GetControlRigBlueprint();
					check(RigBlueprint);

					const FRigControlElement* SourceControlElement = Cast<FRigControlElement>(Element);
					FRigControlElement* TargetControlElement = RigBlueprint->Hierarchy->Find<FRigControlElement>(Key);

					if(SourceControlElement && TargetControlElement)
					{
						TargetControlElement->Settings = SourceControlElement->Settings;
					}
				}
				break;
			}
			case ERigHierarchyNotification::ControlShapeTransformChanged:
			{
				if(DetailViewShowsRigElement(Key))
				{
					UControlRigBlueprint* RigBlueprint = GetControlRigBlueprint();
					check(RigBlueprint);

					FRigControlElement* SourceControlElement = Cast<FRigControlElement>(Element);
					if(SourceControlElement)
					{
						FTransform InitialShapeTransform = WeakHierarchy.Get()->GetControlShapeTransform(SourceControlElement, ERigTransformType::InitialLocal);

						// set current shape transform = initial shape transform so that the viewport reflects this change
						WeakHierarchy.Get()->SetControlShapeTransform(SourceControlElement, InitialShapeTransform, ERigTransformType::CurrentLocal, false); 

						RigBlueprint->Hierarchy->SetControlShapeTransform(Key, WeakHierarchy.Get()->GetControlShapeTransform(SourceControlElement, ERigTransformType::InitialLocal), true);
						RigBlueprint->Hierarchy->SetControlShapeTransform(Key, WeakHierarchy.Get()->GetControlShapeTransform(SourceControlElement, ERigTransformType::CurrentLocal), false);

						RigBlueprint->Modify();
						RigBlueprint->MarkPackageDirty();
					}
				}
				break;
			}
			default:
			{
				break;
			}
		}
		
    };

	if(IsInGameThread())
	{
		Task();
	}
	else
	{
		FFunctionGraphTask::CreateAndDispatchWhenReady([Task]()
		{
			Task();
		}, TStatId(), NULL, ENamedThreads::GameThread);
	}
}

void FControlRigEditor::SynchronizeViewportBoneSelection()
{
	UControlRigBlueprint* RigBlueprint = GetControlRigBlueprint();
	if (RigBlueprint == nullptr)
	{
		return;
	}

	UControlRigSkeletalMeshComponent* EditorSkelComp = Cast<UControlRigSkeletalMeshComponent>(GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent());
	if (EditorSkelComp)
	{
		EditorSkelComp->BonesOfInterest.Reset();

		TArray<const FRigBaseElement*> SelectedBones = GetHierarchyBeingDebugged()->GetSelectedElements(ERigElementType::Bone);
		for (const FRigBaseElement* SelectedBone : SelectedBones)
		{
 			const int32 BoneIndex = EditorSkelComp->GetReferenceSkeleton().FindBoneIndex(SelectedBone->GetName());
			if(BoneIndex != INDEX_NONE)
			{
				EditorSkelComp->BonesOfInterest.AddUnique(BoneIndex);
			}
		}
	}
}

void FControlRigEditor::UpdateBoneModification(FName BoneName, const FTransform& LocalTransform)
{
	if (ControlRig)
	{ 
		if (PreviewInstance)
		{ 
			if (FAnimNode_ModifyBone* Modify = PreviewInstance->FindModifiedBone(BoneName))
			{
				Modify->Translation = LocalTransform.GetTranslation();
				Modify->Rotation = LocalTransform.GetRotation().Rotator();
				Modify->TranslationSpace = EBoneControlSpace::BCS_ParentBoneSpace;
				Modify->RotationSpace = EBoneControlSpace::BCS_ParentBoneSpace; 
			}
		}
		
		TMap<FName, FTransform>* TransformOverrideMap = &ControlRig->TransformOverrideForUserCreatedBones;
		if (UControlRig* DebuggedControlRig = Cast<UControlRig>(GetBlueprintObj()->GetObjectBeingDebugged()))
		{
			TransformOverrideMap = &DebuggedControlRig->TransformOverrideForUserCreatedBones;
		}

		if (FTransform* Transform = TransformOverrideMap->Find(BoneName))
		{
			*Transform = LocalTransform;
		}
	}
}

void FControlRigEditor::RemoveBoneModification(FName BoneName)
{
	if (ControlRig)
	{
		if (PreviewInstance)
		{
			PreviewInstance->RemoveBoneModification(BoneName);
		}

		TMap<FName, FTransform>* TransformOverrideMap = &ControlRig->TransformOverrideForUserCreatedBones;
		if (UControlRig* DebuggedControlRig = Cast<UControlRig>(GetBlueprintObj()->GetObjectBeingDebugged()))
		{
			TransformOverrideMap = &DebuggedControlRig->TransformOverrideForUserCreatedBones;
		}

		TransformOverrideMap->Remove(BoneName);
	}
}

void FControlRigEditor::ResetAllBoneModification()
{
	if (ControlRig)
	{
		if (PreviewInstance)
		{
			PreviewInstance->ResetModifiedBone();
		}

		TMap<FName, FTransform>* TransformOverrideMap = &ControlRig->TransformOverrideForUserCreatedBones;
		if (UControlRig* DebuggedControlRig = Cast<UControlRig>(GetBlueprintObj()->GetObjectBeingDebugged()))
		{
			TransformOverrideMap = &DebuggedControlRig->TransformOverrideForUserCreatedBones;
		}

		TransformOverrideMap->Reset();
	}
}

FControlRigEditorEditMode* FControlRigEditor::GetEditMode() const
{
	return static_cast<FControlRigEditorEditMode*>(GetEditorModeManager().GetActiveMode(FControlRigEditorEditMode::ModeName));
}


void FControlRigEditor::OnCurveContainerChanged()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	ClearDetailObject();

	FBlueprintEditorUtils::MarkBlueprintAsModified(GetControlRigBlueprint());

	UControlRigSkeletalMeshComponent* EditorSkelComp = Cast<UControlRigSkeletalMeshComponent>(GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent());
	if (EditorSkelComp)
	{
		// restart animation 
		EditorSkelComp->InitAnim(true);
		UpdateControlRig();
	}
	CacheNameLists();

	// notification
	FNotificationInfo Info(LOCTEXT("CurveContainerChangeHelpMessage", "CurveContainer has been successfully modified."));
	Info.bFireAndForget = true;
	Info.FadeOutDuration = 5.0f;
	Info.ExpireDuration = 5.0f;

	TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	NotificationPtr->SetCompletionState(SNotificationItem::CS_Success);
}

void FControlRigEditor::CreateRigHierarchyToGraphDragAndDropMenu() const
{
	const FName MenuName = RigHierarchyToGraphDragAndDropMenuName;
	UToolMenus* ToolMenus = UToolMenus::Get();
	
	if(!ensure(ToolMenus))
	{
		return;
	}

	if (!ToolMenus->IsMenuRegistered(MenuName))
	{
		UToolMenu* Menu = ToolMenus->RegisterMenu(MenuName);

		Menu->AddDynamicSection(NAME_None, FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
			{
				UControlRigContextMenuContext* MainContext = InMenu->FindContext<UControlRigContextMenuContext>();
				
				if (FControlRigEditor* ControlRigEditor = MainContext->GetControlRigEditor())
				{
					const FControlRigRigHierarchyToGraphDragAndDropContext& DragDropContext = MainContext->GetRigHierarchyToGraphDragAndDropContext();

					URigHierarchy* Hierarchy = ControlRigEditor->GetHierarchyBeingDebugged();
					const TArray<FRigElementKey>& DraggedKeys = DragDropContext.DraggedElementKeys;
					UEdGraph* Graph = DragDropContext.Graph.Get();
					const FVector2D& NodePosition = DragDropContext.NodePosition;
					
					// if multiple types are selected, we show Get Elements/Set Elements
					bool bMultipleTypeSelected = false;

					ERigElementType LastType = ERigElementType::None;
			
					if (DraggedKeys.Num() > 0)
					{
						LastType = DraggedKeys[0].Type;
					}
			
					uint8 DraggedTypes = 0;
					uint8 DraggedAnimationTypes = 2;
					for (const FRigElementKey& DraggedKey : DragDropContext.DraggedElementKeys)
					{
						if (DraggedKey.Type != LastType)
						{
							bMultipleTypeSelected = true;
						}
						else if(DraggedKey.Type == ERigElementType::Control)
						{
							if(const FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(DraggedKey))
							{
								const uint8 DraggedAnimationType = ControlElement->IsAnimationChannel() ? 1 : 0; 
								if(DraggedAnimationTypes == 2)
								{
									DraggedAnimationTypes = DraggedAnimationType;
								}
								else
								{
									if(DraggedAnimationTypes != DraggedAnimationType)
									{
										bMultipleTypeSelected = true;
									}
								}
							}
						}
				
						DraggedTypes = DraggedTypes | (uint8)DraggedKey.Type;
					}
					
					const FText SectionText = FText::FromString(DragDropContext.GetSectionTitle());
					FToolMenuSection& Section = InMenu->AddSection(NAME_None, SectionText);

					FText GetterLabel = LOCTEXT("GetElement","Get Element");
					FText GetterTooltip = LOCTEXT("GetElement_ToolTip", "Getter For Element");
					FText SetterLabel = LOCTEXT("SetElement","Set Element");
					FText SetterTooltip = LOCTEXT("SetElement_ToolTip", "Setter For Element");
					// if multiple types are selected, we show Get Elements/Set Elements
					if (bMultipleTypeSelected)
					{
						GetterLabel = LOCTEXT("GetElements","Get Elements");
						GetterTooltip = LOCTEXT("GetElements_ToolTip", "Getter For Elements");
						SetterLabel = LOCTEXT("SetElements","Set Elements");
						SetterTooltip = LOCTEXT("SetElements_ToolTip", "Setter For Elements");
					}
					else
					{
						// otherwise, we show "Get Bone/NUll/Control"
						if ((DraggedTypes & (uint8)ERigElementType::Bone) != 0)
						{
							GetterLabel = LOCTEXT("GetBone","Get Bone");
							GetterTooltip = LOCTEXT("GetBone_ToolTip", "Getter For Bone");
							SetterLabel = LOCTEXT("SetBone","Set Bone");
							SetterTooltip = LOCTEXT("SetBone_ToolTip", "Setter For Bone");
						}
						else if ((DraggedTypes & (uint8)ERigElementType::Null) != 0)
						{
							GetterLabel = LOCTEXT("GetNull","Get Null");
							GetterTooltip = LOCTEXT("GetNull_ToolTip", "Getter For Null");
							SetterLabel = LOCTEXT("SetNull","Set Null");
							SetterTooltip = LOCTEXT("SetNull_eoolTip", "Setter For Null");
						}
						else if ((DraggedTypes & (uint8)ERigElementType::Control) != 0)
						{
							if(DraggedAnimationTypes == 0)
							{
								GetterLabel = LOCTEXT("GetControl","Get Control");
								GetterTooltip = LOCTEXT("GetControl_ToolTip", "Getter For Control");
								SetterLabel = LOCTEXT("SetControl","Set Control");
								SetterTooltip = LOCTEXT("SetControl_ToolTip", "Setter For Control");
							}
							else
							{
								GetterLabel = LOCTEXT("GetAnimationChannel","Get Animation Channel");
								GetterTooltip = LOCTEXT("GetAnimationChannel_ToolTip", "Getter For Animation Channel");
								SetterLabel = LOCTEXT("SetAnimationChannel","Set Animation Channel");
								SetterTooltip = LOCTEXT("SetAnimationChannel_ToolTip", "Setter For Animation Channel");
							}
						}
					}

					FToolMenuEntry GetElementsEntry = FToolMenuEntry::InitMenuEntry(
						TEXT("GetElements"),
						GetterLabel,
						GetterTooltip,
						FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateSP(ControlRigEditor, &FControlRigEditor::HandleMakeElementGetterSetter, ERigElementGetterSetterType_Transform, true, DraggedKeys, Graph, NodePosition),
							FCanExecuteAction()
						)
					);
					GetElementsEntry.InsertPosition.Name = NAME_None;
					GetElementsEntry.InsertPosition.Position = EToolMenuInsertType::First;
					

					Section.AddEntry(GetElementsEntry);

					FToolMenuEntry SetElementsEntry = FToolMenuEntry::InitMenuEntry(
						TEXT("SetElements"),
						SetterLabel,
						SetterTooltip,
						FSlateIcon(),
						FUIAction(
							FExecuteAction::CreateSP(ControlRigEditor, &FControlRigEditor::HandleMakeElementGetterSetter, ERigElementGetterSetterType_Transform, false, DraggedKeys, Graph, NodePosition),
							FCanExecuteAction()
						)
					);
					SetElementsEntry.InsertPosition.Name = GetElementsEntry.Name;
					SetElementsEntry.InsertPosition.Position = EToolMenuInsertType::After;	

					Section.AddEntry(SetElementsEntry);

					if (((DraggedTypes & (uint8)ERigElementType::Bone) != 0) ||
						((DraggedTypes & (uint8)ERigElementType::Control) != 0) ||
						((DraggedTypes & (uint8)ERigElementType::Null) != 0))
					{
						FToolMenuEntry& RotationTranslationSeparator = Section.AddSeparator(TEXT("RotationTranslationSeparator"));
						RotationTranslationSeparator.InsertPosition.Name = SetElementsEntry.Name;
						RotationTranslationSeparator.InsertPosition.Position = EToolMenuInsertType::After;

						FToolMenuEntry SetRotationEntry = FToolMenuEntry::InitMenuEntry(
							TEXT("SetRotation"),
							LOCTEXT("SetRotation","Set Rotation"),
							LOCTEXT("SetRotation_ToolTip","Setter for Rotation"),
							FSlateIcon(),
							FUIAction(
								FExecuteAction::CreateSP(ControlRigEditor, &FControlRigEditor::HandleMakeElementGetterSetter, ERigElementGetterSetterType_Rotation, false, DraggedKeys, Graph, NodePosition),
								FCanExecuteAction()
							)
						);
						
						SetRotationEntry.InsertPosition.Name = RotationTranslationSeparator.Name;
						SetRotationEntry.InsertPosition.Position = EToolMenuInsertType::After;		
						Section.AddEntry(SetRotationEntry);

						FToolMenuEntry SetTranslationEntry = FToolMenuEntry::InitMenuEntry(
							TEXT("SetTranslation"),
							LOCTEXT("SetTranslation","Set Translation"),
							LOCTEXT("SetTranslation_ToolTip","Setter for Translation"),
							FSlateIcon(),
							FUIAction(
								FExecuteAction::CreateSP(ControlRigEditor, &FControlRigEditor::HandleMakeElementGetterSetter, ERigElementGetterSetterType_Translation, false, DraggedKeys, Graph, NodePosition),
								FCanExecuteAction()
							)
						);

						SetTranslationEntry.InsertPosition.Name = SetRotationEntry.Name;
						SetTranslationEntry.InsertPosition.Position = EToolMenuInsertType::After;		
						Section.AddEntry(SetTranslationEntry);

						FToolMenuEntry AddOffsetEntry = FToolMenuEntry::InitMenuEntry(
							TEXT("AddOffset"),
							LOCTEXT("AddOffset","Add Offset"),
							LOCTEXT("AddOffset_ToolTip","Setter for Offset"),
							FSlateIcon(),
							FUIAction(
								FExecuteAction::CreateSP(ControlRigEditor, &FControlRigEditor::HandleMakeElementGetterSetter, ERigElementGetterSetterType_Offset, false, DraggedKeys, Graph, NodePosition),
								FCanExecuteAction()
							)
						);
						
						AddOffsetEntry.InsertPosition.Name = SetTranslationEntry.Name;
						AddOffsetEntry.InsertPosition.Position = EToolMenuInsertType::After;						
						Section.AddEntry(AddOffsetEntry);

						FToolMenuEntry& RelativeTransformSeparator = Section.AddSeparator(TEXT("RelativeTransformSeparator"));
						RelativeTransformSeparator.InsertPosition.Name = AddOffsetEntry.Name;
						RelativeTransformSeparator.InsertPosition.Position = EToolMenuInsertType::After;
						
						FToolMenuEntry GetRelativeTransformEntry = FToolMenuEntry::InitMenuEntry(
							TEXT("GetRelativeTransformEntry"),
							LOCTEXT("GetRelativeTransform", "Get Relative Transform"),
							LOCTEXT("GetRelativeTransform_ToolTip", "Getter for Relative Transform"),
							FSlateIcon(),
							FUIAction(
								FExecuteAction::CreateSP(ControlRigEditor, &FControlRigEditor::HandleMakeElementGetterSetter, ERigElementGetterSetterType_Relative, true, DraggedKeys, Graph, NodePosition),
								FCanExecuteAction()
							)
						);
						
						GetRelativeTransformEntry.InsertPosition.Name = RelativeTransformSeparator.Name;
						GetRelativeTransformEntry.InsertPosition.Position = EToolMenuInsertType::After;	
						Section.AddEntry(GetRelativeTransformEntry);
						
						FToolMenuEntry SetRelativeTransformEntry = FToolMenuEntry::InitMenuEntry(
							TEXT("SetRelativeTransformEntry"),
							LOCTEXT("SetRelativeTransform", "Set Relative Transform"),
							LOCTEXT("SetRelativeTransform_ToolTip", "Setter for Relative Transform"),
							FSlateIcon(),
							FUIAction(
								FExecuteAction::CreateSP(ControlRigEditor, &FControlRigEditor::HandleMakeElementGetterSetter, ERigElementGetterSetterType_Relative, false, DraggedKeys, Graph, NodePosition),
								FCanExecuteAction()
							)
						);
						SetRelativeTransformEntry.InsertPosition.Name = GetRelativeTransformEntry.Name;
						SetRelativeTransformEntry.InsertPosition.Position = EToolMenuInsertType::After;		
						Section.AddEntry(SetRelativeTransformEntry);
					}

					if (DraggedKeys.Num() > 0 && Hierarchy != nullptr)
					{
						FToolMenuEntry& ItemArraySeparator = Section.AddSeparator(TEXT("ItemArraySeparator"));
						ItemArraySeparator.InsertPosition.Name = TEXT("SetRelativeTransformEntry"),
						ItemArraySeparator.InsertPosition.Position = EToolMenuInsertType::After;
						
						FToolMenuEntry CreateItemArrayEntry = FToolMenuEntry::InitMenuEntry(
							TEXT("CreateItemArray"),
							LOCTEXT("CreateItemArray", "Create Item Array"),
							LOCTEXT("CreateItemArray_ToolTip", "Creates an item array from the selected elements in the hierarchy"),
							FSlateIcon(),
							FUIAction(
								FExecuteAction::CreateLambda([ControlRigEditor, DraggedKeys, NodePosition]()
									{
										if (URigVMController* Controller = ControlRigEditor->GetFocusedController())
										{
											Controller->OpenUndoBracket(TEXT("Create Item Array From Selection"));

											static const FString ItemArrayCPPType = FString::Printf(TEXT("TArray<%s>"), *RigVMTypeUtils::GetUniqueStructTypeName(FRigElementKey::StaticStruct()));
											static const FString ItemArrayObjectPath = FRigElementKey::StaticStruct()->GetPathName();
							
											if (URigVMNode* ItemsNode = Controller->AddFreeRerouteNode(true, ItemArrayCPPType, *ItemArrayObjectPath, false, NAME_None, FString(), NodePosition))
											{
												if (URigVMPin* ItemsPin = ItemsNode->FindPin(TEXT("Value")))
												{
													Controller->SetArrayPinSize(ItemsPin->GetPinPath(), DraggedKeys.Num());

													TArray<URigVMPin*> ItemPins = ItemsPin->GetSubPins();
													ensure(ItemPins.Num() == DraggedKeys.Num());

													for (int32 ItemIndex = 0; ItemIndex < DraggedKeys.Num(); ItemIndex++)
													{
														FString DefaultValue;
														FRigElementKey::StaticStruct()->ExportText(DefaultValue, &DraggedKeys[ItemIndex], nullptr, nullptr, PPF_None, nullptr);
														Controller->SetPinDefaultValue(ItemPins[ItemIndex]->GetPinPath(), DefaultValue, true, true, false, true);
														Controller->SetPinExpansion(ItemPins[ItemIndex]->GetPinPath(), true, true, true);
													}

													Controller->SetPinExpansion(ItemsPin->GetPinPath(), true, true, true);
												}
											}

											Controller->CloseUndoBracket();
										}
									}
								)
							)
						);
						
						CreateItemArrayEntry.InsertPosition.Name = ItemArraySeparator.Name,
						CreateItemArrayEntry.InsertPosition.Position = EToolMenuInsertType::After;
						Section.AddEntry(CreateItemArrayEntry);
					}
				}
			})
		);
	}
}

void FControlRigEditor::OnGraphNodeDropToPerform(TSharedPtr<FGraphNodeDragDropOp> InDragDropOp, UEdGraph* InGraph, const FVector2D& InNodePosition, const FVector2D& InScreenPosition)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (InDragDropOp->IsOfType<FRigElementHierarchyDragDropOp>())
	{
		TSharedPtr<FRigElementHierarchyDragDropOp> RigHierarchyOp = StaticCastSharedPtr<FRigElementHierarchyDragDropOp>(InDragDropOp);

		if (RigHierarchyOp->GetElements().Num() > 0 && FocusedGraphEdPtr.IsValid())
		{
			const FName MenuName = RigHierarchyToGraphDragAndDropMenuName;
			
			UControlRigContextMenuContext* MenuContext = NewObject<UControlRigContextMenuContext>();
			FControlRigMenuSpecificContext MenuSpecificContext;
			MenuSpecificContext.RigHierarchyToGraphDragAndDropContext =
				FControlRigRigHierarchyToGraphDragAndDropContext(
					RigHierarchyOp->GetElements(),
					InGraph,
					InNodePosition
				);
			MenuContext->Init(SharedThis(this), MenuSpecificContext);
			
			UToolMenus* ToolMenus = UToolMenus::Get();
			TSharedRef<SWidget>	MenuWidget = ToolMenus->GenerateWidget(MenuName, FToolMenuContext(MenuContext));
			
			TSharedRef<SWidget> GraphEditorPanel = FocusedGraphEdPtr.Pin().ToSharedRef();

			// Show menu to choose getter vs setter
			FSlateApplication::Get().PushMenu(
				GraphEditorPanel,
				FWidgetPath(),
				MenuWidget,
				InScreenPosition,
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
			);

		}
	}
}

void FControlRigEditor::HandleMakeElementGetterSetter(ERigElementGetterSetterType Type, bool bIsGetter, TArray<FRigElementKey> Keys, UEdGraph* Graph, FVector2D NodePosition)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	if (Keys.Num() == 0)
	{
		return;
	}

	URigHierarchy* Hierarchy = GetHierarchyBeingDebugged();
	if (Hierarchy == nullptr)
	{
		return;
	}
	if (GetFocusedController() == nullptr)
	{
		return;
	}

	GetFocusedController()->OpenUndoBracket(TEXT("Adding Nodes from Hierarchy"));

	struct FNewNodeData
	{
		FName Name;
		FName ValuePinName;
		ERigControlType ValueType;
		FRigControlValue Value;
	};
	TArray<FNewNodeData> NewNodes;

	for (const FRigElementKey& Key : Keys)
	{
		UScriptStruct* StructTemplate = nullptr;

		FNewNodeData NewNode;
		NewNode.Name = NAME_None;
		NewNode.ValuePinName = NAME_None;

		TArray<FName> ItemPins;
		ItemPins.Add(TEXT("Item"));

		FName NameValue = Key.Name;
		FName ChannelValue = Key.Name;
		TArray<FName> NamePins;
		TArray<FName> ChannelPins;
		TMap<FName, int32> PinsToResolve; 

		if(FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(Key))
		{
			if(ControlElement->IsAnimationChannel())
			{
				if(const FRigControlElement* ParentControlElement = Cast<FRigControlElement>(Hierarchy->GetFirstParent(ControlElement)))
				{
					NameValue = ParentControlElement->GetName();
				}
				else
				{
					NameValue = NAME_None;
				}

				ItemPins.Reset();
				NamePins.Add(TEXT("Control"));
				ChannelPins.Add(TEXT("Channel"));
				static const FName ValueName = GET_MEMBER_NAME_CHECKED(FRigUnit_GetBoolAnimationChannel, Value);

				switch (ControlElement->Settings.ControlType)
				{
					case ERigControlType::Bool:
					{
						if(bIsGetter)
						{
							StructTemplate = FRigUnit_GetBoolAnimationChannel::StaticStruct();
						}
						else
						{
							StructTemplate = FRigUnit_SetBoolAnimationChannel::StaticStruct();
						}
						PinsToResolve.Add(ValueName, RigVMTypeUtils::TypeIndex::Bool);
						break;
					}
					case ERigControlType::Float:
					{
						if(bIsGetter)
						{
							StructTemplate = FRigUnit_GetFloatAnimationChannel::StaticStruct();
						}
						else
						{
							StructTemplate = FRigUnit_SetFloatAnimationChannel::StaticStruct();
						}
						PinsToResolve.Add(ValueName, RigVMTypeUtils::TypeIndex::Float);
						break;
					}
					case ERigControlType::Integer:
					{
						if(bIsGetter)
						{
							StructTemplate = FRigUnit_GetIntAnimationChannel::StaticStruct();
						}
						else
						{
							StructTemplate = FRigUnit_SetIntAnimationChannel::StaticStruct();
						}
						PinsToResolve.Add(ValueName, RigVMTypeUtils::TypeIndex::Int32);
						break;
					}
					case ERigControlType::Vector2D:
					{
						if(bIsGetter)
						{
							StructTemplate = FRigUnit_GetVector2DAnimationChannel::StaticStruct();
						}
						else
						{
							StructTemplate = FRigUnit_SetVector2DAnimationChannel::StaticStruct();
						}

						UScriptStruct* ValueStruct = TBaseStructure<FVector2D>::Get();
						const FRigVMTemplateArgumentType TypeForStruct(*RigVMTypeUtils::GetUniqueStructTypeName(ValueStruct), ValueStruct);
						const int32 TypeIndex = FRigVMRegistry::Get().GetTypeIndex(TypeForStruct);
						PinsToResolve.Add(ValueName, TypeIndex);
						break;
					}
					case ERigControlType::Position:
					case ERigControlType::Scale:
					{
						if(bIsGetter)
						{
							StructTemplate = FRigUnit_GetVectorAnimationChannel::StaticStruct();
						}
						else
						{
							StructTemplate = FRigUnit_SetVectorAnimationChannel::StaticStruct();
						}
						UScriptStruct* ValueStruct = TBaseStructure<FVector>::Get();
						const FRigVMTemplateArgumentType TypeForStruct(*RigVMTypeUtils::GetUniqueStructTypeName(ValueStruct), ValueStruct);
						const int32 TypeIndex = FRigVMRegistry::Get().GetTypeIndex(TypeForStruct);
						PinsToResolve.Add(ValueName, TypeIndex);
						break;
					}
					case ERigControlType::Rotator:
					{
						if(bIsGetter)
						{
							StructTemplate = FRigUnit_GetRotatorAnimationChannel::StaticStruct();
						}
						else
						{
							StructTemplate = FRigUnit_SetRotatorAnimationChannel::StaticStruct();
						}
						UScriptStruct* ValueStruct = TBaseStructure<FRotator>::Get();
						const FRigVMTemplateArgumentType TypeForStruct(*ValueStruct->GetStructCPPName(), ValueStruct);
						const int32 TypeIndex = FRigVMRegistry::Get().GetTypeIndex(TypeForStruct);
						PinsToResolve.Add(ValueName, TypeIndex);
						break;
					}
					case ERigControlType::Transform:
					case ERigControlType::TransformNoScale:
					case ERigControlType::EulerTransform:
					{
						if(bIsGetter)
						{
							StructTemplate = FRigUnit_GetTransformAnimationChannel::StaticStruct();
						}
						else
						{
							StructTemplate = FRigUnit_SetTransformAnimationChannel::StaticStruct();
						}
						UScriptStruct* ValueStruct = TBaseStructure<FTransform>::Get();
						const FRigVMTemplateArgumentType TypeForStruct(*RigVMTypeUtils::GetUniqueStructTypeName(ValueStruct), ValueStruct);
						const int32 TypeIndex = FRigVMRegistry::Get().GetTypeIndex(TypeForStruct);
						PinsToResolve.Add(ValueName, TypeIndex);
						break;
					}
					default:
					{
						break;
					}
				}
			}
		}

		if (bIsGetter && StructTemplate == nullptr)
		{
			switch (Type)
			{
				case ERigElementGetterSetterType_Transform:
				{
					if (Key.Type == ERigElementType::Control)
					{
						FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(Key);
						check(ControlElement);
						
						switch (ControlElement->Settings.ControlType)
						{
							case ERigControlType::Bool:
							{
								NamePins.Add(TEXT("Control"));
								StructTemplate = FRigUnit_GetControlBool::StaticStruct();
								break;
							}
							case ERigControlType::Float:
							{
								NamePins.Add(TEXT("Control"));
								StructTemplate = FRigUnit_GetControlFloat::StaticStruct();
								break;
							}
							case ERigControlType::Integer:
							{
								NamePins.Add(TEXT("Control"));
								StructTemplate = FRigUnit_GetControlInteger::StaticStruct();
								break;
							}
							case ERigControlType::Vector2D:
							{
								NamePins.Add(TEXT("Control"));
								StructTemplate = FRigUnit_GetControlVector2D::StaticStruct();
								break;
							}
							case ERigControlType::Position:
							case ERigControlType::Scale:
							{
								NamePins.Add(TEXT("Control"));
								StructTemplate = FRigUnit_GetControlVector::StaticStruct();
								break;
							}
							case ERigControlType::Rotator:
							{
								NamePins.Add(TEXT("Control"));
								StructTemplate = FRigUnit_GetControlRotator::StaticStruct();
								break;
							}
							case ERigControlType::Transform:
							case ERigControlType::TransformNoScale:
							case ERigControlType::EulerTransform:
							{
								StructTemplate = FRigUnit_GetTransform::StaticStruct();
								break;
							}
							default:
							{
								break;
							}
						}
					}
					else
					{
						StructTemplate = FRigUnit_GetTransform::StaticStruct();
					}
					break;
				}
				case ERigElementGetterSetterType_Initial:
				{
					StructTemplate = FRigUnit_GetTransform::StaticStruct();
					break;
				}
				case ERigElementGetterSetterType_Relative:
				{
					StructTemplate = FRigUnit_GetRelativeTransformForItem::StaticStruct();
					ItemPins.Reset();
					ItemPins.Add(TEXT("Child"));
					ItemPins.Add(TEXT("Parent"));
					break;
				}
				default:
				{
					break;
				}
			}
		}
		else if(StructTemplate == nullptr)
		{
			switch (Type)
			{
				case ERigElementGetterSetterType_Transform:
				{
					if (Key.Type == ERigElementType::Control)
					{
						FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(Key);
						check(ControlElement);

						switch (ControlElement->Settings.ControlType)
						{
							case ERigControlType::Bool:
							{
								NamePins.Add(TEXT("Control"));
								StructTemplate = FRigUnit_SetControlBool::StaticStruct();
								break;
							}
							case ERigControlType::Float:
							{
								NamePins.Add(TEXT("Control"));
								StructTemplate = FRigUnit_SetControlFloat::StaticStruct();
								break;
							}
							case ERigControlType::Integer:
							{
								NamePins.Add(TEXT("Control"));
								StructTemplate = FRigUnit_SetControlInteger::StaticStruct();
								break;
							}
							case ERigControlType::Vector2D:
							{
								NamePins.Add(TEXT("Control"));
								StructTemplate = FRigUnit_SetControlVector2D::StaticStruct();
								break;
							}
							case ERigControlType::Position:
							{
								NamePins.Add(TEXT("Control"));
								StructTemplate = FRigUnit_SetControlVector::StaticStruct();
								NewNode.ValuePinName = TEXT("Vector");
								NewNode.ValueType = ERigControlType::Position;
								NewNode.Value = FRigControlValue::Make<FVector>(Hierarchy->GetGlobalTransform(Key).GetLocation());
								break;
							}
							case ERigControlType::Scale:
							{
								NamePins.Add(TEXT("Control"));
								StructTemplate = FRigUnit_SetControlVector::StaticStruct();
								NewNode.ValuePinName = TEXT("Vector");
								NewNode.ValueType = ERigControlType::Scale;
								NewNode.Value = FRigControlValue::Make<FVector>(Hierarchy->GetGlobalTransform(Key).GetScale3D());
								break;
							}
							case ERigControlType::Rotator:
							{
								NamePins.Add(TEXT("Control"));
								StructTemplate = FRigUnit_SetControlRotator::StaticStruct();
								NewNode.ValuePinName = TEXT("Rotator");
								NewNode.ValueType = ERigControlType::Rotator;
								NewNode.Value = FRigControlValue::Make<FRotator>(Hierarchy->GetGlobalTransform(Key).Rotator());
								break;
							}
							case ERigControlType::Transform:
							case ERigControlType::TransformNoScale:
							case ERigControlType::EulerTransform:
							{
								StructTemplate = FRigUnit_SetTransform::StaticStruct();
								NewNode.ValuePinName = TEXT("Transform");
								NewNode.ValueType = ERigControlType::Transform;
								NewNode.Value = FRigControlValue::Make<FTransform>(Hierarchy->GetGlobalTransform(Key));
								break;
							}
							default:
							{
								break;
							}
						}
					}
					else
					{
						StructTemplate = FRigUnit_SetTransform::StaticStruct();
						NewNode.ValuePinName = TEXT("Transform");
						NewNode.ValueType = ERigControlType::Transform;
						NewNode.Value = FRigControlValue::Make<FTransform>(Hierarchy->GetGlobalTransform(Key));
					}
					break;
				}
				case ERigElementGetterSetterType_Relative:
				{
					StructTemplate = FRigUnit_SetRelativeTransformForItem::StaticStruct();
					ItemPins.Reset();
					ItemPins.Add(TEXT("Child"));
					ItemPins.Add(TEXT("Parent"));
					break;
				}
				case ERigElementGetterSetterType_Rotation:
				{
					StructTemplate = FRigUnit_SetRotation::StaticStruct();
					NewNode.ValuePinName = TEXT("Rotation");
					NewNode.ValueType = ERigControlType::Rotator;
					NewNode.Value = FRigControlValue::Make<FRotator>(Hierarchy->GetGlobalTransform(Key).Rotator());
					break;
				}
				case ERigElementGetterSetterType_Translation:
				{
					StructTemplate = FRigUnit_SetTranslation::StaticStruct();
					NewNode.ValuePinName = TEXT("Translation");
					NewNode.ValueType = ERigControlType::Position;
					NewNode.Value = FRigControlValue::Make<FVector>(Hierarchy->GetGlobalTransform(Key).GetLocation());
					break;
				}
				case ERigElementGetterSetterType_Offset:
				{
					StructTemplate = FRigUnit_OffsetTransformForItem::StaticStruct();
					break;
				}
				default:
				{
					break;
				}
			}
		}

		if (StructTemplate == nullptr)
		{
			return;
		}

		FVector2D NodePositionIncrement(0.f, 120.f);
		if (!bIsGetter)
		{
			NodePositionIncrement = FVector2D(380.f, 0.f);
		}

		FName Name = FControlRigBlueprintUtils::ValidateName(GetControlRigBlueprint(), StructTemplate->GetName());
		if (URigVMUnitNode* ModelNode = GetFocusedController()->AddUnitNode(StructTemplate, FRigUnit::GetMethodName(), NodePosition, FString(), true, true))
		{
			FString ItemTypeStr = StaticEnum<ERigElementType>()->GetDisplayNameTextByValue((int64)Key.Type).ToString();
			NewNode.Name = ModelNode->GetFName();
			NewNodes.Add(NewNode);

			for (const TPair<FName, int32>& PinToResolve : PinsToResolve)
			{
				if(URigVMPin* Pin = ModelNode->FindPin(PinToResolve.Key.ToString()))
				{
					GetFocusedController()->ResolveWildCardPin(Pin, PinToResolve.Value, true, true);
				}
			}

			for (const FName& ItemPin : ItemPins)
			{
				GetFocusedController()->SetPinDefaultValue(FString::Printf(TEXT("%s.%s.Name"), *ModelNode->GetName(), *ItemPin.ToString()), Key.Name.ToString(), true, true, false, true);
				GetFocusedController()->SetPinDefaultValue(FString::Printf(TEXT("%s.%s.Type"), *ModelNode->GetName(), *ItemPin.ToString()), ItemTypeStr, true, true, false, true);
			}

			for (const FName& NamePin : NamePins)
			{
				const FString PinPath = FString::Printf(TEXT("%s.%s"), *ModelNode->GetName(), *NamePin.ToString());
				GetFocusedController()->SetPinDefaultValue(PinPath, NameValue.ToString(), true, true, false, true);
			}

			for (const FName& ChannelPin : ChannelPins)
			{
				const FString PinPath = FString::Printf(TEXT("%s.%s"), *ModelNode->GetName(), *ChannelPin.ToString());
				GetFocusedController()->SetPinDefaultValue(PinPath, ChannelValue.ToString(), true, true, false, true);
			}

			if (!NewNode.ValuePinName.IsNone())
			{
				FString DefaultValue;

				switch (NewNode.ValueType)
				{
					case ERigControlType::Position:
					case ERigControlType::Scale:
					{
						DefaultValue = NewNode.Value.ToString<FVector>();
						break;
					}
					case ERigControlType::Rotator:
					{
						DefaultValue = NewNode.Value.ToString<FRotator>();
						break;
					}
					case ERigControlType::Transform:
					{
						DefaultValue = NewNode.Value.ToString<FTransform>();
						break;
					}
					default:
					{
						break;
					}
				}
				if (!DefaultValue.IsEmpty())
				{
					GetFocusedController()->SetPinDefaultValue(FString::Printf(TEXT("%s.%s"), *ModelNode->GetName(), *NewNode.ValuePinName.ToString()), DefaultValue, true, true, false, true);
				}
			}

			UControlRigUnitNodeSpawner::HookupMutableNode(ModelNode, GetControlRigBlueprint());
		}

		NodePosition += NodePositionIncrement;
	}

	if (NewNodes.Num() > 0)
	{
		TArray<FName> NewNodeNames;
		for (const FNewNodeData& NewNode : NewNodes)
		{
			NewNodeNames.Add(NewNode.Name);
		}
		GetFocusedController()->SetNodeSelection(NewNodeNames);
		GetFocusedController()->CloseUndoBracket();
	}
	else
	{
		GetFocusedController()->CancelUndoBracket();
	}
}

void FControlRigEditor::HandleOnControlModified(UControlRig* Subject, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context)
{
	if (Subject != ControlRig)
	{
		return;
	}

	UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	if (Blueprint == nullptr)
	{
		return;
	}

	URigHierarchy* Hierarchy = Subject->GetHierarchy();

	if (ControlElement->Settings.bIsTransientControl)
	{
		FRigControlValue ControlValue = Hierarchy->GetControlValue(ControlElement, ERigControlValueType::Current);

		const FString PinPath = UControlRig::GetPinNameFromTransientControl(ControlElement->GetKey());
		if (URigVMPin* Pin = Blueprint->GetRigVMClient()->FindPin(PinPath))
		{
			FString NewDefaultValue;
			switch (ControlElement->Settings.ControlType)
			{
				case ERigControlType::Position:
				case ERigControlType::Scale:
				{
					NewDefaultValue = ControlValue.ToString<FVector>();
					break;
				}
				case ERigControlType::Rotator:
				{
					FVector3f RotatorAngles = ControlValue.Get<FVector3f>();
					FRotator Rotator = FRotator::MakeFromEuler((FVector)RotatorAngles);
					FRigControlValue RotatorValue = FRigControlValue::Make<FRotator>(Rotator);
					NewDefaultValue = RotatorValue.ToString<FRotator>();
					break;
				}
				case ERigControlType::Transform:
				{
					NewDefaultValue = ControlValue.ToString<FTransform>();
					break;
				}
				case ERigControlType::TransformNoScale:
				{
					NewDefaultValue = ControlValue.ToString<FTransformNoScale>();
					break;
				}
				case ERigControlType::EulerTransform:
				{
					NewDefaultValue = ControlValue.ToString<FEulerTransform>();
					break;
				}
				default:
				{
					break;
				}
			}

			if (!NewDefaultValue.IsEmpty())
			{
				bool bRequiresRecompile = true;

				if(ControlRig)
				{
					if(TSharedPtr<FRigVMParserAST> AST = Pin->GetGraph()->GetDiagnosticsAST())
					{
						FRigVMASTProxy PinProxy = FRigVMASTProxy::MakeFromUObject(Pin);
						if(const FRigVMExprAST* PinExpr = AST->GetExprForSubject(PinProxy))
						{
							if(PinExpr->IsA(FRigVMExprAST::Var))
							{
								const FString PinHash = URigVMCompiler::GetPinHash(Pin, PinExpr->To<FRigVMVarExprAST>(), false);
								if(const FRigVMOperand* OperandForPin = Blueprint->PinToOperandMap.Find(PinHash))
								{
									if(URigVM* VM = ControlRig->GetVM())
									{
										// only operands which are shared across multiple instructions require recompile
										if(!VM->GetByteCode().IsOperandShared(*OperandForPin))
										{
											VM->SetPropertyValueFromString(*OperandForPin, NewDefaultValue);
											bRequiresRecompile = false;
										}
									}
								}
							}
						}
					}
				}
				
				TGuardValue<bool> DisableBlueprintNotifs(Blueprint->bSuspendModelNotificationsForSelf, !bRequiresRecompile);
				GetFocusedController()->SetPinDefaultValue(Pin->GetPinPath(), NewDefaultValue, true, true, true);
			}
		}
		else
		{
			const FRigElementKey ElementKey = UControlRig::GetElementKeyFromTransientControl(ControlElement->GetKey());

			if (ElementKey.Type == ERigElementType::Bone)
			{
				const FTransform CurrentValue = ControlValue.Get<FRigControlValue::FTransform_Float>().ToTransform();
				const FTransform Transform = CurrentValue * Hierarchy->GetControlOffsetTransform(ControlElement, ERigTransformType::CurrentLocal);
				Blueprint->Hierarchy->SetLocalTransform(ElementKey, Transform);
				Hierarchy->SetLocalTransform(ElementKey, Transform);

				if (IsConstructionModeEnabled())
				{
					Blueprint->Hierarchy->SetInitialLocalTransform(ElementKey, Transform);
					Hierarchy->SetInitialLocalTransform(ElementKey, Transform);
				}
				else
				{ 
					UpdateBoneModification(ElementKey.Name, Transform);
				}
			}
			else if (ElementKey.Type == ERigElementType::Null)
			{
				const FTransform GlobalTransform = ControlRig->GetControlGlobalTransform(ControlElement->GetName());
				Blueprint->Hierarchy->SetGlobalTransform(ElementKey, GlobalTransform);
				Hierarchy->SetGlobalTransform(ElementKey, GlobalTransform);
				if (IsConstructionModeEnabled())
				{
					Blueprint->Hierarchy->SetInitialGlobalTransform(ElementKey, GlobalTransform);
					Hierarchy->SetInitialGlobalTransform(ElementKey, GlobalTransform);
				}
			}
		}
	}
	else if (IsConstructionModeEnabled())
	{
		FRigControlElement* SourceControlElement = Hierarchy->Find<FRigControlElement>(ControlElement->GetKey());
		FRigControlElement* TargetControlElement = Blueprint->Hierarchy->Find<FRigControlElement>(ControlElement->GetKey());
		if(SourceControlElement && TargetControlElement)
		{
			TargetControlElement->Settings = SourceControlElement->Settings;

			// only fire the setting change if the interaction is not currently ongoing
			if(!Subject->ElementsBeingInteracted.Contains(ControlElement->GetKey()))
			{
				Blueprint->Hierarchy->OnModified().Broadcast(ERigHierarchyNotification::ControlSettingChanged, Blueprint->Hierarchy, TargetControlElement);
			}

			// we copy the pose including the weights since we want the topology to align during construction mode.
			// i.e. dynamic reparenting should be reset here.
			TargetControlElement->CopyPose(SourceControlElement, true, true, true);
		}
	}
}

void FControlRigEditor::HandleRefreshEditorFromBlueprint(UControlRigBlueprint* InBlueprint)
{
	OnHierarchyChanged();
	Compile();
}

void FControlRigEditor::HandleVariableDroppedFromBlueprint(UObject* InSubject, FProperty* InVariableToDrop, const FVector2D& InDropPosition, const FVector2D& InScreenPosition)
{
	UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	if (Blueprint == nullptr)
	{
		return;
	}

	URigVMController* Controller = GetFocusedController();
	check(Controller);

	FRigVMExternalVariable ExternalVariable = FRigVMExternalVariable::Make(InVariableToDrop, nullptr);
	if (!ExternalVariable.IsValid(true /* allow null ptr */))
	{
		return;
	}

	FMenuBuilder MenuBuilder(true, NULL);
	const FText SectionText = FText::FromString(FString::Printf(TEXT("Variable %s"), *ExternalVariable.Name.ToString()));

	MenuBuilder.BeginSection("VariableDropped", SectionText);

	MenuBuilder.AddMenuEntry(
		FText::FromString(FString::Printf(TEXT("Get %s"), *ExternalVariable.Name.ToString())),
		FText::FromString(FString::Printf(TEXT("Adds a getter node for variable %s"), *ExternalVariable.Name.ToString())),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([ExternalVariable, Controller, InDropPosition] {

				Controller->AddVariableNode(ExternalVariable.Name, ExternalVariable.TypeName.ToString(), ExternalVariable.TypeObject, true, FString(), InDropPosition, FString(), true, true);

			}),
			FCanExecuteAction()
		)
	);

	MenuBuilder.AddMenuEntry(
		FText::FromString(FString::Printf(TEXT("Set %s"), *ExternalVariable.Name.ToString())),
		FText::FromString(FString::Printf(TEXT("Adds a setter node for variable %s"), *ExternalVariable.Name.ToString())),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([ExternalVariable, Controller, InDropPosition] {

				Controller->AddVariableNode(ExternalVariable.Name, ExternalVariable.TypeName.ToString(), ExternalVariable.TypeObject, false, FString(), InDropPosition, FString(), true, true);

			}),
			FCanExecuteAction()
		)
	);

	MenuBuilder.EndSection();

	TSharedRef<SWidget> GraphEditorPanel = FocusedGraphEdPtr.Pin().ToSharedRef();

	// Show dialog to choose getter vs setter
	FSlateApplication::Get().PushMenu(
		GraphEditorPanel,
		FWidgetPath(),
		MenuBuilder.MakeWidget(),
		InScreenPosition,
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
	);
}

void FControlRigEditor::HandleBreakpointAdded()
{
	SetExecutionMode(EControlRigExecutionModeType_Debug);
}

void FControlRigEditor::OnGraphNodeClicked(UControlRigGraphNode* InNode)
{
	if (InNode)
	{
		if (InNode->IsSelectedInEditor())
		{
			SetDetailViewForGraph(InNode->GetModel());
		}
	}
}

void FControlRigEditor::OnNodeDoubleClicked(UControlRigBlueprint* InBlueprint, URigVMNode* InNode)
{
	ensure(GetControlRigBlueprint() == InBlueprint);

	if (URigVMLibraryNode* LibraryNode = Cast<URigVMLibraryNode>(InNode))
	{
		if(URigVMGraph* ContainedGraph = LibraryNode->GetContainedGraph())
		{
			if (UEdGraph* EdGraph = InBlueprint->GetEdGraph(ContainedGraph))
			{
				OpenGraphAndBringToFront(EdGraph, true);
			}
			else
			{
				if(URigVMCollapseNode* FunctionLibraryNode = Cast<URigVMCollapseNode>(ContainedGraph->GetOuter()))
				{
					if(URigVMFunctionLibrary* FunctionLibrary = FunctionLibraryNode->GetLibrary())
					{
						if(UControlRigBlueprint* FunctionBlueprint = Cast<UControlRigBlueprint>(FunctionLibrary->GetOuter()))
						{
							if (UEdGraph* FunctionEdGraph = FunctionBlueprint->GetEdGraph(ContainedGraph))
							{
								FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(FunctionEdGraph);
							}
						}
					}
				}
			}
		}
	}
}

void FControlRigEditor::OnGraphImported(UEdGraph* InEdGraph)
{
	check(InEdGraph);

	OpenDocument(InEdGraph, FDocumentTracker::OpenNewDocument);
	RenameNewlyAddedAction(InEdGraph->GetFName());
}

bool FControlRigEditor::OnActionMatchesName(FEdGraphSchemaAction* InAction, const FName& InName) const
{
	if (InAction->GetMenuDescription().ToString() == InName.ToString())
	{
		return true;
	}
	return false;
}

void FControlRigEditor::HandleShowCurrentStatement()
{
	if(HaltedAtNode)
	{
		if(UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
		{
			if(UControlRigGraph* EdGraph = Cast<UControlRigGraph>(Blueprint->GetEdGraph(HaltedAtNode->GetGraph())))
			{
				if(UEdGraphNode* EdNode = EdGraph->FindNodeForModelNodeName(HaltedAtNode->GetFName()))
				{
					JumpToHyperlink(EdNode, false);
				}
			}
		}
	}
}

void FControlRigEditor::HandleBreakpointActionRequested(const ERigVMBreakpointAction BreakpointAction)
{
	if (ControlRig)
	{
		ControlRig->ExecuteBreakpointAction(BreakpointAction);
	}
}

bool FControlRigEditor::IsHaltedAtBreakpoint() const
{
	return HaltedAtNode != nullptr;
}

void FControlRigEditor::FrameSelection()
{
	if (SGraphEditor* GraphEd = FocusedGraphEdPtr.Pin().Get())
	{
		if(URigVMGraph* Model = GetFocusedModel())
		{
			const bool bFrameAll = Model->GetSelectNodes().Num() == 0;
			GraphEd->ZoomToFit(!bFrameAll);
		}
	}
}

void FControlRigEditor::UpdateGraphCompilerErrors()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	if (Blueprint && ControlRig && ControlRig->VM)
	{
		if (ControlRigLog.Entries.Num() == 0 && !bAnyErrorsLeft)
		{
			return;
		}

		const FRigVMByteCode& ByteCode = ControlRig->VM->GetByteCode();

		TArray<UEdGraph*> EdGraphs;
		Blueprint->GetAllGraphs(EdGraphs);

		for (UEdGraph* Graph : EdGraphs)
		{
			UControlRigGraph* RigGraph = Cast<UControlRigGraph>(Graph);
			if (RigGraph == nullptr)
			{
				continue;
			}

			// reset all nodes and store them in the map
			bool bFoundWarning = false;
			bool bFoundError = false;
			
			for (UEdGraphNode* GraphNode : Graph->Nodes)
			{
				if (UControlRigGraphNode* ControlRigGraphNode = Cast<UControlRigGraphNode>(GraphNode))
				{
					bFoundError = bFoundError || ControlRigGraphNode->ErrorType <= (int32)EMessageSeverity::Error;
					bFoundWarning = bFoundWarning || ControlRigGraphNode->ErrorType <= (int32)EMessageSeverity::Warning;

					if(ControlRigGraphNode->ErrorType <= (int32)EMessageSeverity::Warning)
					{
						if(!ControlRig->VM->WasInstructionVisitedDuringLastRun(ControlRigGraphNode->GetInstructionIndex(true)) &&
							!ControlRig->VM->WasInstructionVisitedDuringLastRun(ControlRigGraphNode->GetInstructionIndex(false)))
						{
							continue;
						}
					}
				}

				GraphNode->ErrorType = int32(EMessageSeverity::Info) + 1;
			}

			// update the nodes' error messages
			for (const FControlRigLog::FLogEntry& Entry : ControlRigLog.Entries)
			{
				URigVMNode* ModelNode = Cast<URigVMNode>(ByteCode.GetSubjectForInstruction(Entry.InstructionIndex));
				if (ModelNode == nullptr)
				{
					continue;
				}

				UEdGraphNode* GraphNode = RigGraph->FindNodeForModelNodeName(ModelNode->GetFName());
				if (GraphNode == nullptr)
				{
					continue;
				}

				bFoundError = bFoundError || Entry.Severity <= EMessageSeverity::Error;
				bFoundWarning = bFoundWarning || Entry.Severity <= EMessageSeverity::Warning;

				int32 ErrorType = (int32)Entry.Severity;
				if (GraphNode->ErrorType < ErrorType)
				{
					continue;
				}
				else if (GraphNode->ErrorType == ErrorType)
				{
					GraphNode->ErrorMsg = FString::Printf(TEXT("%s\n%s"), *GraphNode->ErrorMsg, *Entry.Message);
				}
				else
				{
					GraphNode->ErrorMsg = Entry.Message;
					GraphNode->ErrorType = ErrorType;
				}
			}

			bAnyErrorsLeft = false;
			for (UEdGraphNode* GraphNode : Graph->Nodes)
			{
				GraphNode->bHasCompilerMessage = GraphNode->ErrorType <= int32(EMessageSeverity::Info);
				bAnyErrorsLeft = bAnyErrorsLeft || GraphNode->bHasCompilerMessage; 
			}

			if (bFoundError)
			{
				Blueprint->Status = BS_Error;
				Blueprint->MarkPackageDirty();
			}
		}

		//Stack
	}

}

UToolMenu* FControlRigEditor::HandleOnGetViewportContextMenuDelegate()
{
	if (OnGetViewportContextMenuDelegate.IsBound())
	{
		return OnGetViewportContextMenuDelegate.Execute();
	}
	return nullptr;
}

TSharedPtr<FUICommandList> FControlRigEditor::HandleOnViewportContextMenuCommandsDelegate()
{
	if (OnViewportContextMenuCommandsDelegate.IsBound())
	{
		return OnViewportContextMenuCommandsDelegate.Execute();
	}
	return TSharedPtr<FUICommandList>();
}

URigVMGraph* FControlRigEditor::GetFocusedModel() const
{
	UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	if (Blueprint == nullptr)
	{
		return nullptr;
	}

	UControlRigGraph* EdGraph = Cast<UControlRigGraph>(GetFocusedGraph());
	return Blueprint->GetModel(EdGraph);
}

URigVMController* FControlRigEditor::GetFocusedController() const
{
	UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	if (Blueprint == nullptr)
	{
		return nullptr;
	}
	return Blueprint->GetOrCreateController(GetFocusedModel());
}

TSharedPtr<SGraphEditor> FControlRigEditor::GetGraphEditor(UEdGraph* InEdGraph) const
{
	TArray< TSharedPtr<SDockTab> > GraphEditorTabs;
	DocumentManager->FindAllTabsForFactory(GraphEditorTabFactoryPtr, /*out*/ GraphEditorTabs);

	for (TSharedPtr<SDockTab>& GraphEditorTab : GraphEditorTabs)
	{
		TSharedRef<SGraphEditor> Editor = StaticCastSharedRef<SGraphEditor>((GraphEditorTab)->GetContent());
		if (Editor->GetCurrentGraph() == InEdGraph)
		{
			return Editor;
		}
	}

	return TSharedPtr<SGraphEditor>();
}

void FControlRigEditor::StoreNodeSnippet(int32 InSnippetIndex)
{
	check((InSnippetIndex >= 0) && (InSnippetIndex < 10));

	URigVMController* Controller = GetFocusedController();
	if(Controller == nullptr)
	{
		return;
	}

	const TArray<FName> SelectedNodeNames = Controller->GetGraph()->GetSelectNodes();
	if(SelectedNodeNames.Num() == 0)
	{
		return;
	}
	
	FString Snippet = Controller->ExportNodesToText(SelectedNodeNames);
	if(Snippet.IsEmpty())
	{
		return;
	}

	TArray<FString> NodeNameStrings;
	for(const FName& SelectedNodeName : SelectedNodeNames)
	{
		NodeNameStrings.Add(SelectedNodeName.ToString());
	}
	const FString NodeNamesJoined = FString::Join(NodeNameStrings, TEXT(", "));

	FString* Setting = GetSnippetStorage(InSnippetIndex);
	check(Setting != nullptr);

	UControlRigEditorSettings* Settings = UControlRigEditorSettings::Get();
	Settings->Modify();
	(*Setting) = Snippet;

	const FString PropertyName = FString::Printf(TEXT("NodeSnippet_%d"), InSnippetIndex);
	FProperty* Property = UControlRigEditorSettings::StaticClass()->FindPropertyByName(*PropertyName);
	check(Property);
	
	Settings->UpdateSinglePropertyInConfigFile(Property, Settings->GetDefaultConfigFilename());

	FNotificationInfo Info(FText::FromString(FString::Printf(
		TEXT("A snippet has been stored to the Project Settings.\n")
		TEXT("Nodes %s are now stored as snippet %d.\n")
		TEXT("You can restore the snippet by pressing Alt-%d and left clicking into the graph."),
		*NodeNamesJoined, InSnippetIndex, InSnippetIndex)));
	Info.bFireAndForget = true;
	Info.FadeOutDuration = 2.0f;
	Info.ExpireDuration = 8.0f;

	TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	NotificationPtr->SetCompletionState(SNotificationItem::CS_Success);
}

void FControlRigEditor::RestoreNodeSnippet(int32 InSnippetIndex)
{
	FString* Setting = GetSnippetStorage(InSnippetIndex);
	check(Setting != nullptr);

	if(Setting->IsEmpty())
	{
		return;
	}

	FPlatformApplicationMisc::ClipboardCopy(*(*Setting));
	PasteNodes();
	
	FNotificationInfo Info(FText::FromString(FString::Printf(
		TEXT("Snippet %d has been restored."),
		InSnippetIndex)));
	Info.bFireAndForget = true;
	Info.FadeOutDuration = 0.5f;
	Info.ExpireDuration = 3.0f;

	TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	NotificationPtr->SetCompletionState(SNotificationItem::CS_Success);
}

FString* FControlRigEditor::GetSnippetStorage(int32 InSnippetIndex)
{
	UControlRigEditorSettings* Settings = UControlRigEditorSettings::Get();
	switch(InSnippetIndex)
	{
		case 1:
		{
			return &Settings->NodeSnippet_1;
			break;
		}
		case 2:
		{
			return &Settings->NodeSnippet_2;
			break;
		}
		case 3:
		{
			return &Settings->NodeSnippet_3;
			break;
		}
		case 4:
		{
			return &Settings->NodeSnippet_4;
			break;
		}
		case 5:
		{
			return &Settings->NodeSnippet_5;
			break;
		}
		case 6:
		{
			return &Settings->NodeSnippet_6;
			break;
		}
		case 7:
		{
			return &Settings->NodeSnippet_7;
			break;
		}
		case 8:
		{
			return &Settings->NodeSnippet_8;
			break;
		}
		case 9:
		{
			return &Settings->NodeSnippet_9;
			break;
		}
		case 0:
		{
			return &Settings->NodeSnippet_0;
			break;
		}
		default:
		{
			checkNoEntry();
			break;
		}
	}

	return nullptr;
}

void FControlRigEditor::OnPreConstruction_AnyThread(UControlRig* InRig, const EControlRigState InState,
	const FName& InEventName)
{
	bIsConstructionEventRunning = true;
}

void FControlRigEditor::OnPostConstruction_AnyThread(UControlRig* InRig, const EControlRigState InState,
	const FName& InEventName)
{
	bIsConstructionEventRunning = false;
	
	const int32 HierarchyHash = InRig->GetHierarchy()->GetTopologyHash(false);
	if(LastHierarchyHash != HierarchyHash)
	{
		LastHierarchyHash = HierarchyHash;
		
		auto Task = [this, InRig]()
		{
			CacheNameLists();
			SynchronizeViewportBoneSelection();
			RebindToSkeletalMeshComponent();
			if(DetailViewShowsAnyRigElement())
			{
				SetDetailViewForRigElements();
			}
		};
				
		if(IsInGameThread())
		{
			Task();
		}
		else
		{
			FFunctionGraphTask::CreateAndDispatchWhenReady([Task]()
			{
				Task();
			}, TStatId(), NULL, ENamedThreads::GameThread);
		}
	}
}

#undef LOCTEXT_NAMESPACE
