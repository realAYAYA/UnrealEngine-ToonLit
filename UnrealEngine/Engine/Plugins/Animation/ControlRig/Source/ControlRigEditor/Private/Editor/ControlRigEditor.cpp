// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/ControlRigEditor.h"
#include "Modules/ModuleManager.h"
#include "ControlRigEditorModule.h"
#include "ControlRigBlueprint.h"
#include "SBlueprintEditorToolbar.h"
#include "Editor/ControlRigEditorMode.h"
#include "SKismetInspector.h"
#include "SEnumCombo.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Framework/Commands/GenericCommands.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Graph/ControlRigGraph.h"
#include "BlueprintActionDatabase.h"
#include "ControlRigEditorCommands.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "IPersonaToolkit.h"
#include "PersonaModule.h"
#include "Editor/ControlRigEditorEditMode.h"
#include "EditMode/ControlRigEditModeSettings.h"
#include "EditorModeManager.h"
#include "RigVMBlueprintGeneratedClass.h"
#include "AnimCustomInstanceHelper.h"
#include "Sequencer/ControlRigLayerInstance.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "IPersonaPreviewScene.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "ControlRig.h"
#include "ModularRig.h"
#include "Editor/ControlRigSkeletalMeshComponent.h"
#include "ControlRigObjectBinding.h"
#include "RigVMBlueprintUtils.h"
#include "EditorViewportClient.h"
#include "AnimationEditorPreviewActor.h"
#include "Misc/MessageDialog.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ControlRigEditorStyle.h"
#include "Editor/RigVMEditorStyle.h"
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
#include "EdGraph/NodeSpawners/RigVMEdGraphUnitNodeSpawner.h"
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
#include "PropertyCustomizationHelpers.h"
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
#include "Widgets/SRigVMGraphFunctionLocalizationWidget.h"
#include "Widgets/SRigVMGraphFunctionBulkEditWidget.h"
#include "Widgets/SRigVMGraphBreakLinksWidget.h"
#include "Widgets/SRigVMGraphChangePinType.h"
#include "SGraphPanel.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "RigVMFunctions/Execution/RigVMFunction_Sequence.h"
#include "Editor/ControlRigContextMenuContext.h"
#include "Types/ISlateMetaData.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/WatchedPin.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "MaterialDomain.h"
#include "RigVMFunctions/RigVMFunction_ControlFlow.h"
#include "RigVMModel/Nodes/RigVMAggregateNode.h"
#include "AnimationEditorViewportClient.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "SchematicGraphPanel/SSchematicGraphPanel.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Editor/RigVMGraphDetailCustomization.h"

#define LOCTEXT_NAMESPACE "ControlRigEditor"

TAutoConsoleVariable<bool> CVarControlRigShowTestingToolbar(TEXT("ControlRig.Test.EnableTestingToolbar"), false, TEXT("When true we'll show the testing toolbar in Control Rig Editor."));
TAutoConsoleVariable<bool> CVarShowSchematicPanelOverlay(TEXT("ControlRig.Preview.ShowSchematicPanelOverlay"), true, TEXT("When true we'll add an overlay to the persona viewport to show modular rig information."));

const FName FControlRigEditorModes::ControlRigEditorMode = TEXT("Rigging");
const TArray<FName> FControlRigEditor::ForwardsSolveEventQueue = {FRigUnit_BeginExecution::EventName};
const TArray<FName> FControlRigEditor::BackwardsSolveEventQueue = {FRigUnit_InverseExecution::EventName};
const TArray<FName> FControlRigEditor::ConstructionEventQueue = {FRigUnit_PrepareForExecution::EventName};
const TArray<FName> FControlRigEditor::BackwardsAndForwardsSolveEventQueue = {FRigUnit_InverseExecution::EventName, FRigUnit_BeginExecution::EventName};

FControlRigEditor::FControlRigEditor()
	: IControlRigEditor()
	, PreviewInstance(nullptr)
	, ActiveController(nullptr)
	, bExecutionControlRig(true)
	, RigHierarchyTabCount(0)
	, ModularRigHierarchyTabCount(0)
	, bIsConstructionEventRunning(false)
	, LastHierarchyHash(INDEX_NONE)
	, bRefreshDirectionManipulationTargetsRequired(false)
{
	LastEventQueue = ConstructionEventQueue;
}

FControlRigEditor::~FControlRigEditor()
{
	UControlRigBlueprint* RigBlueprint = GetControlRigBlueprint();

	if (RigBlueprint)
	{
		UControlRigBlueprint::sCurrentlyOpenedRigBlueprints.Remove(RigBlueprint);

		RigBlueprint->OnHierarchyModified().RemoveAll(this);
		if (FControlRigEditMode* EditMode = GetEditMode())
		{
			RigBlueprint->OnHierarchyModified().RemoveAll(EditMode);
			EditMode->OnEditorClosed();
		}

		RigBlueprint->OnRigTypeChanged().RemoveAll(this);
		if (RigBlueprint->IsModularRig())
		{
			RigBlueprint->GetModularRigController()->OnModified().RemoveAll(this);
			RigBlueprint->OnModularRigCompiled().RemoveAll(this);

			RigBlueprint->OnSetObjectBeingDebugged().RemoveAll(&SchematicModel);
			RigBlueprint->OnHierarchyModified().RemoveAll(&SchematicModel);
			RigBlueprint->GetModularRigController()->OnModified().RemoveAll(&SchematicModel);
		}
	}

	if (PersonaToolkit.IsValid())
	{
		constexpr bool bSetPreviewMeshInAsset = false;
		PersonaToolkit->SetPreviewMesh(nullptr, bSetPreviewMeshInAsset);
	}
}

UObject* FControlRigEditor::GetOuterForHost() const
{
	UControlRigSkeletalMeshComponent* EditorSkelComp = Cast<UControlRigSkeletalMeshComponent>(GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent());
	if(EditorSkelComp)
	{
		return EditorSkelComp;
	}
	return FRigVMEditor::GetOuterForHost();
}

UClass* FControlRigEditor::GetDetailWrapperClass() const
{
	return UControlRigWrapperObject::StaticClass();
}

bool FControlRigEditor::IsSectionVisible(NodeSectionID::Type InSectionID) const
{
	if(!IControlRigEditor::IsSectionVisible(InSectionID))
	{
		return false;
	}

	if(const UControlRigBlueprint* RigBlueprint = GetControlRigBlueprint())
	{
		if(IsModularRig())
		{
			switch (InSectionID)
			{
				case NodeSectionID::GRAPH:
				{
					return RigBlueprint->SupportsEventGraphs();
				}
				case NodeSectionID::FUNCTION:
				{
					return RigBlueprint->SupportsFunctions();
				}
				default:
				{
					break;
				}
			}
		}
	}
	return true;
}

bool FControlRigEditor::NewDocument_IsVisibleForType(ECreatedDocumentType GraphType) const
{
	if(!IControlRigEditor::NewDocument_IsVisibleForType(GraphType))
	{
		return false;
	}

	if(const UControlRigBlueprint* RigBlueprint = GetControlRigBlueprint())
	{
		if(IsModularRig())
		{
			switch(GraphType)
			{
				case CGT_NewEventGraph:
				{
					return RigBlueprint->SupportsEventGraphs();
				}
				case CGT_NewFunctionGraph:
				{
					return RigBlueprint->SupportsFunctions();
				}
				default:
				{
					break;
				}
			}
		}
	}

	return true;
}

FReply FControlRigEditor::OnViewportDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const FReply SuperReply = IControlRigEditor::OnViewportDrop(MyGeometry, DragDropEvent);
	if(SuperReply.IsEventHandled())
	{
		return SuperReply;
	}

	if(IsModularRig())
	{
		const TSharedPtr<FAssetDragDropOp> AssetDragDropOperation = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
		if (AssetDragDropOperation)
		{
			for (const FAssetData& AssetData : AssetDragDropOperation->GetAssets())
			{
				const UClass* AssetClass = AssetData.GetClass();
				if (!AssetClass->IsChildOf(UControlRigBlueprint::StaticClass()))
				{
					continue;
				}

				if(const UControlRigBlueprint* AssetBlueprint = Cast<UControlRigBlueprint>(AssetData.GetAsset()))
				{
					UClass* ControlRigClass = AssetBlueprint->GetControlRigClass();
					if(AssetBlueprint->IsControlRigModule() && ControlRigClass)
					{
						FSlateApplication::Get().DismissAllMenus();

						UModularRigController* Controller = GetControlRigBlueprint()->GetModularRigController();
						FString ClassName = ControlRigClass->GetName();
						ClassName.RemoveFromEnd(TEXT("_C"));
						const FRigName Name = Controller->GetSafeNewName(FString(), FRigName(ClassName));
						const FString ModulePath = Controller->AddModule(Name, ControlRigClass, FString());
						if (!ModulePath.IsEmpty())
						{
							return FReply::Handled();
						}
					}
				}
			}
		}
	}

	return FReply::Unhandled();
}

void FControlRigEditor::CreateEmptyGraphContent(URigVMController* InController)
{
	URigVMNode* Node = InController->AddUnitNode(FRigUnit_BeginExecution::StaticStruct(), FRigUnit::GetMethodName(), FVector2D::ZeroVector, FString(), false);
	if (Node)
	{
		TArray<FName> NodeNames;
		NodeNames.Add(Node->GetFName());
		InController->SetNodeSelection(NodeNames, false);
	}
}

UControlRigBlueprint* FControlRigEditor::GetControlRigBlueprint() const
{
	return Cast<UControlRigBlueprint>(GetRigVMBlueprint());
}

UControlRig* FControlRigEditor::GetControlRig() const
{
	return Cast<UControlRig>(GetRigVMHost());
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

void FControlRigEditor::InitRigVMEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, URigVMBlueprint* InRigVMBlueprint)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FRigVMEditor::InitRigVMEditor(Mode, InitToolkitHost, InRigVMBlueprint);

	UControlRigBlueprint* ControlRigBlueprint = CastChecked<UControlRigBlueprint>(InRigVMBlueprint);

	CreatePersonaToolKitIfRequired();
	UControlRigBlueprint::sCurrentlyOpenedRigBlueprints.AddUnique(ControlRigBlueprint);

	if (FControlRigEditMode* EditMode = GetEditMode())
	{
		EditMode->OnGetRigElementTransform() = FOnGetRigElementTransform::CreateSP(this, &FControlRigEditor::GetRigElementTransform);
		EditMode->OnSetRigElementTransform() = FOnSetRigElementTransform::CreateSP(this, &FControlRigEditor::SetRigElementTransform);
		EditMode->OnGetContextMenu() = FOnGetContextMenu::CreateSP(this, &FControlRigEditor::HandleOnGetViewportContextMenuDelegate);
		EditMode->OnContextMenuCommands() = FNewMenuCommandsDelegate::CreateSP(this, &FControlRigEditor::HandleOnViewportContextMenuCommandsDelegate);
		EditMode->OnAnimSystemInitialized().Add(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FControlRigEditor::OnAnimInitialized));
	
		PersonaToolkit->GetPreviewScene()->SetRemoveAttachedComponentFilter(FOnRemoveAttachedComponentFilter::CreateSP(EditMode, &FControlRigEditMode::CanRemoveFromPreviewScene));
	}

	{
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
		ControlRigBlueprint->OnHierarchyModified().AddSP(this, &FControlRigEditor::OnHierarchyModified);

		if (FControlRigEditMode* EditMode = GetEditMode())
		{
			ControlRigBlueprint->OnHierarchyModified().AddSP(EditMode, &FControlRigEditMode::OnHierarchyModified_AnyThread);
		}

		if (ControlRigBlueprint->IsModularRig())
		{
			SchematicModel.SetEditor(SharedThis(this));
			ControlRigBlueprint->OnSetObjectBeingDebugged().AddRaw(&SchematicModel, &FControlRigSchematicModel::OnSetObjectBeingDebugged);
			ControlRigBlueprint->GetModularRigController()->OnModified().AddRaw(&SchematicModel, &FControlRigSchematicModel::HandleModularRigModified);
		}

		ControlRigBlueprint->OnRigTypeChanged().AddSP(this, &FControlRigEditor::HandleRigTypeChanged);
		if (ControlRigBlueprint->IsModularRig())
		{
			ControlRigBlueprint->GetModularRigController()->OnModified().AddSP(this, &FControlRigEditor::HandleModularRigModified);
			ControlRigBlueprint->OnModularRigCompiled().AddRaw(this, &FControlRigEditor::HandlePostCompileModularRigs);
		}
	}

	CreateRigHierarchyToGraphDragAndDropMenu();

	if(SchematicViewport.IsValid())
	{
		SchematicModel.UpdateControlRigContent();
	}
}

void FControlRigEditor::CreatePersonaToolKitIfRequired()
{
	if(PersonaToolkit.IsValid())
	{
		return;
	}
	
	UControlRigBlueprint* ControlRigBlueprint = GetControlRigBlueprint();

	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");

	FPersonaToolkitArgs PersonaToolkitArgs;
	PersonaToolkitArgs.OnPreviewSceneCreated = FOnPreviewSceneCreated::FDelegate::CreateSP(this, &FControlRigEditor::HandlePreviewSceneCreated);
	PersonaToolkitArgs.bPreviewMeshCanUseDifferentSkeleton = true;
	USkeleton* Skeleton = nullptr;
	if(USkeletalMesh* PreviewMesh = ControlRigBlueprint->GetPreviewMesh())
	{
		Skeleton = PreviewMesh->GetSkeleton();
	}
	PersonaToolkit = PersonaModule.CreatePersonaToolkit(ControlRigBlueprint, PersonaToolkitArgs, Skeleton);

	// Set a default preview mesh, if any
	PersonaToolkit->SetPreviewMesh(ControlRigBlueprint->GetPreviewMesh(), false);

	// set delegate prior to setting mesh
	// otherwise, you don't get delegate
	PersonaToolkit->GetPreviewScene()->RegisterOnPreviewMeshChanged(FOnPreviewMeshChanged::CreateSP(this, &FControlRigEditor::HandlePreviewMeshChanged));
}

const FName FControlRigEditor::GetEditorAppName() const
{
	static const FName ControlRigEditorAppName(TEXT("ControlRigEditorApp"));
	return ControlRigEditorAppName;
}

const FName FControlRigEditor::GetEditorModeName() const
{
	if(IsModularRig())
	{
		return FModularRigEditorEditMode::ModeName;
	}
	return FControlRigEditorEditMode::ModeName;
}

TSharedPtr<FApplicationMode> FControlRigEditor::CreateEditorMode()
{
	CreatePersonaToolKitIfRequired();

	if(IsModularRig())
	{
		return MakeShareable(new FModularRigEditorMode(SharedThis(this)));
	}
	return MakeShareable(new FControlRigEditorMode(SharedThis(this)));
}

FText FControlRigEditor::GetTestAssetName() const
{
	if(TestDataStrongPtr.IsValid())
	{
		return FText::FromString(TestDataStrongPtr->GetName());
	}

	static const FText NoTestAsset = LOCTEXT("NoTestAsset", "No Test Asset");
	return NoTestAsset;
}

FText FControlRigEditor::GetTestAssetTooltip() const
{
	if(TestDataStrongPtr.IsValid())
	{
		return FText::FromString(TestDataStrongPtr->GetPathName());
	}
	static const FText NoTestAssetTooltip = LOCTEXT("NoTestAssetTooltip", "Click the record button to the left to record a new frame");
	return NoTestAssetTooltip;
}

bool FControlRigEditor::SetTestAssetPath(const FString& InAssetPath)
{
	if(TestDataStrongPtr.IsValid())
	{
		if(TestDataStrongPtr->GetPathName().Equals(InAssetPath, ESearchCase::CaseSensitive))
		{
			return false;
		}
	}

	if(TestDataStrongPtr.IsValid())
	{
		TestDataStrongPtr->ReleaseReplay();
		TestDataStrongPtr.Reset();
	}

	if(!InAssetPath.IsEmpty())
	{
		if(UControlRigTestData* TestData = LoadObject<UControlRigTestData>(GetControlRigBlueprint(), *InAssetPath))
		{
			TestDataStrongPtr = TStrongObjectPtr<UControlRigTestData>(TestData);
		}
	}
	return true;
}

TSharedRef<SWidget> FControlRigEditor::GenerateTestAssetModeMenuContent()
{
	FMenuBuilder MenuBuilder(true, GetToolkitCommands());

	MenuBuilder.BeginSection(TEXT("Default"));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ClearTestAssset", "Clear"),
		LOCTEXT("ClearTestAsset_ToolTip", "Clears the test asset"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]()
			{
				SetTestAssetPath(FString());
			}
		)	
	));
	MenuBuilder.EndSection();

	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> TestDataAssets;
	FARFilter AssetFilter;
	AssetFilter.ClassPaths.Add(UControlRigTestData::StaticClass()->GetClassPathName());
	AssetRegistryModule.Get().GetAssets(AssetFilter, TestDataAssets);

	const FString CurrentObjectPath = GetControlRigBlueprint()->GetPathName();
	TestDataAssets.RemoveAll([CurrentObjectPath](const FAssetData& InAssetData)
	{
		const FString ControlRigObjectPath = InAssetData.GetTagValueRef<FString>(GET_MEMBER_NAME_CHECKED(UControlRigTestData, ControlRigObjectPath));
		return ControlRigObjectPath != CurrentObjectPath;
	});

	if(!TestDataAssets.IsEmpty())
	{
		MenuBuilder.BeginSection(TEXT("Assets"));
		for(const FAssetData& TestDataAsset : TestDataAssets)
		{
			const FString TestDataObjectPath = TestDataAsset.GetObjectPathString();
			FString Right;
			if(TestDataObjectPath.Split(TEXT("."), nullptr, &Right))
			{
				MenuBuilder.AddMenuEntry(FText::FromString(Right), FText::FromString(TestDataObjectPath), FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateLambda([this, TestDataObjectPath]()
						{
							SetTestAssetPath(TestDataObjectPath);
						}
					)	
				));
			}
		}
		MenuBuilder.EndSection();
	}
	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> FControlRigEditor::GenerateTestAssetRecordMenuContent()
{
	FMenuBuilder MenuBuilder(true, GetToolkitCommands());

	MenuBuilder.AddMenuEntry(
		LOCTEXT("TestAssetRecordSingleFrame", "Single Frame"),
		LOCTEXT("TestAssetRecordSingleFrame_ToolTip", "Records a single frame into the test data asset"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]()
			{
				RecordTestData(0);
			}
		)	
	));
	MenuBuilder.AddMenuEntry(
		LOCTEXT("TestAssetRecordOneSecond", "1 Second"),
		LOCTEXT("TestAssetRecordOneSecond_ToolTip", "Records 1 second of animation into the test data asset"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([this]()
			{
				RecordTestData(1);
			}
		)	
	));

	MenuBuilder.AddMenuEntry(
	LOCTEXT("TestAssetRecordFiveSeconds", "5 Seconds"),
	LOCTEXT("TestAssetRecordFiveSeconds_ToolTip", "Records 5 seconds of animation into the test data asset"),
	FSlateIcon(),
	FUIAction(
			FExecuteAction::CreateLambda([this]()
			{
				RecordTestData(5);
			}
		)	
	));

	MenuBuilder.AddMenuEntry(
	LOCTEXT("TestAssetRecordTenSeconds", "10 Seconds"),
	LOCTEXT("TestAssetRecordTenSeconds_ToolTip", "Records 10 seconds of animation into the test data asset"),
	FSlateIcon(),
	FUIAction(
			FExecuteAction::CreateLambda([this]()
			{
				RecordTestData(10);
			}
		)	
	));

	return MenuBuilder.MakeWidget();
}

bool FControlRigEditor::RecordTestData(double InRecordingDuration)
{
	if(GetControlRig() == nullptr)
	{
		return false;
	}
	
	if(!TestDataStrongPtr.IsValid())
	{
		// create a new test asset
		static const FString Folder = TEXT("/Game/Animation/ControlRig/NoCook/");
		const FString DesiredPackagePath =  FString::Printf(TEXT("%s/%s_TestData"), *Folder, *GetControlRigBlueprint()->GetName());

		if(UControlRigTestData* TestData = UControlRigTestData::CreateNewAsset(DesiredPackagePath, GetControlRigBlueprint()->GetPathName()))
		{
			SetTestAssetPath(TestData->GetPathName());
		}
	}
	
	if(UControlRigTestData* TestData = TestDataStrongPtr.Get())
	{
		TestData->Record(GetControlRig(), InRecordingDuration);
	}
	return true;
}

void FControlRigEditor::ToggleTestData()
{
	if(TestDataStrongPtr.IsValid())
	{
		switch(TestDataStrongPtr->GetPlaybackMode())
		{
			case EControlRigTestDataPlaybackMode::ReplayInputs:
			{
				TestDataStrongPtr->ReleaseReplay();
				break;
			}
			case EControlRigTestDataPlaybackMode::GroundTruth:
			{
				TestDataStrongPtr->SetupReplay(GetControlRig(), false);
				break;
			}
			default:
			{
				TestDataStrongPtr->SetupReplay(GetControlRig(), true);
				break;
			}
		}
	}
}

void FControlRigEditor::FillToolbar(FToolBarBuilder& ToolbarBuilder, bool bEndSection)
{
	FRigVMEditor::FillToolbar(ToolbarBuilder, false);
	
	{
		if(CVarControlRigHierarchyEnableModules.GetValueOnAnyThread())
		{
			TWeakObjectPtr<UControlRigBlueprint> WeakBlueprint = GetControlRigBlueprint();
			ToolbarBuilder.AddToolBarButton(
				FUIAction(
					FExecuteAction::CreateLambda([WeakBlueprint]()
					{
						if(WeakBlueprint.IsValid())
						{
							if(WeakBlueprint->IsControlRigModule())
							{
								WeakBlueprint->TurnIntoStandaloneRig();
							}
							else
							{
								if(!WeakBlueprint->CanTurnIntoControlRigModule(false))
								{
									static const FText Message(LOCTEXT("TurnIntoControlRigModuleMessage", "This rig requires some changes to the hierarchy to turn it into a module.\n\nWe'll try to recreate the hierarchy by relying on nodes in the construction event instead.\n\nDo you want to continue?"));
									EAppReturnType::Type Ret = FMessageDialog::Open(EAppMsgType::YesNo, Message);
									if(Ret == EAppReturnType::No)
									{
										return;
									}
								}
								WeakBlueprint->TurnIntoControlRigModule(true);
							}
						}
					}),
					FCanExecuteAction::CreateLambda([WeakBlueprint]
					{
						if(WeakBlueprint.IsValid())
						{
							if(WeakBlueprint->IsControlRigModule())
							{
								return WeakBlueprint->CanTurnIntoStandaloneRig();
							}
							return WeakBlueprint->CanTurnIntoControlRigModule(true);
						}
						return false;
					})
				),
				NAME_None,
				TAttribute<FText>::CreateLambda([WeakBlueprint]()
				{
					static const FText StandaloneRig = LOCTEXT("SwitchToRigModule", "Switch to Rig Module"); 
					static const FText RigModule = LOCTEXT("SwitchToStandaloneRig", "Switch to Standalone Rig");
					if(WeakBlueprint.IsValid())
					{
						if(WeakBlueprint->IsControlRigModule())
						{
							return RigModule;
						}
					}
					return StandaloneRig;
				}),
				TAttribute<FText>::CreateLambda([WeakBlueprint]()
				{
					static const FText StandaloneRigTooltip = LOCTEXT("StandaloneRigTooltip", "A standalone control rig."); 
					static const FText RigModuleTooltip = LOCTEXT("RigModuleTooltip", "A rig module used to build rigs.");
					if(WeakBlueprint.IsValid())
					{
						if(!WeakBlueprint->IsControlRigModule())
						{
							FString FailureReason;
							if(!WeakBlueprint->CanTurnIntoControlRigModule(true, &FailureReason))
							{
								return FText::Format(
									LOCTEXT("StandaloneRigTooltipFormat", "{0}\n\nThis rig cannot be turned into a module:\n\n{1}"),
									StandaloneRigTooltip,
									FText::FromString(FailureReason)
								);
							}
							return StandaloneRigTooltip;
						}
					}
					return RigModuleTooltip;
				}),
				TAttribute<FSlateIcon>::CreateLambda([WeakBlueprint]()
				{
					static const FSlateIcon ModuleIcon = FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ControlRig.Tree.Connector");
					static const FSlateIcon RigIcon = FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(),"ClassIcon.ControlRigBlueprint"); 
					if(WeakBlueprint.IsValid())
					{
						if(WeakBlueprint->IsControlRigModule())
						{
							return ModuleIcon;
						}
					}
					return RigIcon;
				}),
				EUserInterfaceActionType::Button
			);
		}

		if(CVarControlRigShowTestingToolbar.GetValueOnAnyThread())
		{
			ToolbarBuilder.AddSeparator();

			FUIAction TestAssetAction;
			ToolbarBuilder.AddComboButton(
				FUIAction(FExecuteAction(), FCanExecuteAction::CreateLambda([this]()
				{
					if(TestDataStrongPtr.IsValid())
					{
						return !TestDataStrongPtr->IsReplaying() && !TestDataStrongPtr->IsRecording();
					}
					return true;
				})),
				FOnGetContent::CreateSP(this, &FControlRigEditor::GenerateTestAssetModeMenuContent),
				TAttribute<FText>::CreateSP(this, &FControlRigEditor::GetTestAssetName),
				TAttribute<FText>::CreateSP(this, &FControlRigEditor::GetTestAssetTooltip),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "AutomationTools.TestAutomation"),
				false);

			ToolbarBuilder.AddToolBarButton(FUIAction(
					FExecuteAction::CreateLambda([this]()
					{
						RecordTestData(0.0);
					}),
					FCanExecuteAction::CreateLambda([this]()
					{
						if(TestDataStrongPtr.IsValid())
						{
							return !TestDataStrongPtr->IsReplaying() && !TestDataStrongPtr->IsRecording();
						}
						return true;
					})
				),
				NAME_None,
				LOCTEXT("TestDataRecordButton", "Record"),
				LOCTEXT("TestDataRecordButton_Tooltip", "Records a new frame into the test data asset.\nA test data asset will be created if necessary."),
				FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(),"ControlRig.TestData.Record")
				);
			ToolbarBuilder.AddComboButton(
				FUIAction(),
				FOnGetContent::CreateSP(this, &FControlRigEditor::GenerateTestAssetRecordMenuContent),
				LOCTEXT("TestDataRecordMenu_Label", "Recording Modes"),
				LOCTEXT("TestDataRecordMenu_ToolTip", "Pick between different modes for recording"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Recompile"),
				true);

			ToolbarBuilder.AddToolBarButton(FUIAction(
					FExecuteAction::CreateLambda([this]()
					{
						ToggleTestData();
					}),
					FCanExecuteAction::CreateLambda([this]
					{
						if(TestDataStrongPtr.IsValid())
						{
							return !TestDataStrongPtr->IsRecording();
						}
						return false;
					})
				),
				NAME_None,
				TAttribute<FText>::CreateLambda([this]()
				{
					static const FText LiveStatus = LOCTEXT("LiveStatus", "Live"); 
					static const FText ReplayInputsStatus = LOCTEXT("ReplayInputsStatus", "Replay Inputs");
					static const FText GroundTruthStatus = LOCTEXT("GroundTruthStatus", "Ground Truth");
					if(TestDataStrongPtr.IsValid())
					{
						switch(TestDataStrongPtr->GetPlaybackMode())
						{
							case EControlRigTestDataPlaybackMode::ReplayInputs:
							{
								return ReplayInputsStatus;
							}
							case EControlRigTestDataPlaybackMode::GroundTruth:
							{
								return GroundTruthStatus;
							}
							default:
							{
								break;
							}
						}
					}
					return LiveStatus;
				}),
				TAttribute<FText>::CreateLambda([this]()
				{
					static const FText LiveStatusTooltip = LOCTEXT("LiveStatusTooltip", "The test data is not affecting the rig."); 
					static const FText ReplayInputsStatusTooltip = LOCTEXT("ReplayInputsStatusTooltip", "The test data inputs are being replayed onto the rig.");
					static const FText GroundTruthStatusTooltip = LOCTEXT("GroundTruthStatusTooltip", "The complete result of the rig is being overwritten.");
					if(TestDataStrongPtr.IsValid())
					{
						switch(TestDataStrongPtr->GetPlaybackMode())
						{
							case EControlRigTestDataPlaybackMode::ReplayInputs:
							{
								return ReplayInputsStatusTooltip;
							}
							case EControlRigTestDataPlaybackMode::GroundTruth:
							{
								return GroundTruthStatusTooltip;
							}
							default:
							{
								break;
							}
						}
					}
					return LiveStatusTooltip;
				}),
				TAttribute<FSlateIcon>::CreateLambda([this]()
				{
					static const FSlateIcon LiveIcon = FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(),"ClassIcon.ControlRigBlueprint"); 
					static const FSlateIcon ReplayIcon = FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(),"ClassIcon.ControlRigSequence"); 
					if(TestDataStrongPtr.IsValid() && TestDataStrongPtr->IsReplaying())
					{
						return ReplayIcon;
					}
					return LiveIcon;
				}),
				EUserInterfaceActionType::Button
			);
		}
	}
	
	if(bEndSection)
	{
		ToolbarBuilder.EndSection();
	}
}

TArray<FName> FControlRigEditor::GetDefaultEventQueue() const
{
	return ForwardsSolveEventQueue;
}

void FControlRigEditor::SetEventQueue(TArray<FName> InEventQueue, bool bCompile)
{
	if (GetEventQueue() == InEventQueue)
	{
		return;
	}

	TArray<FRigElementKey> PreviousSelection;
	if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
	{
		if(bCompile)
		{
			if (RigBlueprint->GetAutoVMRecompile())
			{
				RigBlueprint->RequestAutoVMRecompilation();
			}
			RigBlueprint->Validator->SetControlRig(GetControlRig());
		}
		
		// need to clear selection before remove transient control
		// because active selection will trigger transient control recreation after removal	
		PreviousSelection = GetHierarchyBeingDebugged()->GetSelectedKeys();
		RigBlueprint->GetHierarchyController()->ClearSelection();
		
		// need to copy here since the removal changes the iterator
		if (GetControlRig())
		{
			RigBlueprint->ClearTransientControls();
		}
	}

	FRigVMEditor::SetEventQueue(InEventQueue, bCompile);

	if (UControlRig* ControlRig = GetControlRig())
	{
		if (InEventQueue.Num() > 0)
		{
			if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
			{
				RigBlueprint->Validator->SetControlRig(ControlRig);

				if (LastEventQueue == ConstructionEventQueue)
				{
					// This will propagate any user bone transformation done during construction to the preview instance
					ResetAllBoneModification();
				}
			}
		}

		// Reset transforms only for construction and forward solve to not interrupt any animation that might be playing
		if (InEventQueue.Contains(FRigUnit_PrepareForExecution::EventName) ||
			InEventQueue.Contains(FRigUnit_BeginExecution::EventName))
		{
			if(UControlRigEditorSettings::Get()->bResetPoseWhenTogglingEventQueue)
			{
				ControlRig->GetHierarchy()->ResetPoseToInitial(ERigElementType::All);
			}
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
	return FRigVMEditor::GetEventQueueComboValue();
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

FSlateIcon FControlRigEditor::GetEventQueueIcon(const TArray<FName>& InEventQueue) const
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

void FControlRigEditor::HandleSetObjectBeingDebugged(UObject* InObject)
{
	FRigVMEditor::HandleSetObjectBeingDebugged(InObject);
	
	UControlRig* DebuggedControlRig = Cast<UControlRig>(InObject);
	if(UControlRig* PreviouslyDebuggedControlRig = Cast<UControlRig>(GetBlueprintObj()->GetObjectBeingDebugged()))
	{
		if(!URigVMHost::IsGarbageOrDestroyed(PreviouslyDebuggedControlRig))
		{
			PreviouslyDebuggedControlRig->GetHierarchy()->OnModified().RemoveAll(this);
			PreviouslyDebuggedControlRig->OnPreForwardsSolve_AnyThread().RemoveAll(this);
			PreviouslyDebuggedControlRig->OnPreConstructionForUI_AnyThread().RemoveAll(this);
			PreviouslyDebuggedControlRig->OnPreConstruction_AnyThread().RemoveAll(this);
			PreviouslyDebuggedControlRig->OnPostConstruction_AnyThread().RemoveAll(this);
			PreviouslyDebuggedControlRig->ControlModified().RemoveAll(this);
		}
	}

	if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
	{
		RigBlueprint->Validator->SetControlRig(DebuggedControlRig);
	}

	if (DebuggedControlRig)
	{
		const bool bShouldExecute = ShouldExecuteControlRig(DebuggedControlRig);
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
			if(bShouldExecute)
			{
				DebuggedControlRig->SetBoneInitialTransformsFromSkeletalMeshComponent(EditorSkelComp);
				if(UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
				{
					// copy the initial transforms back to the blueprint
					// no need to call modify here since this code only modifies the bp if the preview mesh changed
					RigBlueprint->Hierarchy->CopyPose(DebuggedControlRig->GetHierarchy(), false, true, false);
				}
			}
		}

		DebuggedControlRig->GetHierarchy()->OnModified().RemoveAll(this);
		DebuggedControlRig->OnPreForwardsSolve_AnyThread().RemoveAll(this);
		DebuggedControlRig->OnPreConstructionForUI_AnyThread().RemoveAll(this);
		DebuggedControlRig->OnPreConstruction_AnyThread().RemoveAll(this);
		DebuggedControlRig->OnPostConstruction_AnyThread().RemoveAll(this);
		DebuggedControlRig->ControlModified().RemoveAll(this);

		DebuggedControlRig->GetHierarchy()->OnModified().AddSP(this, &FControlRigEditor::OnHierarchyModified_AnyThread);
		DebuggedControlRig->OnPreForwardsSolve_AnyThread().AddSP(this, &FControlRigEditor::OnPreForwardsSolve_AnyThread);
		DebuggedControlRig->OnPreConstructionForUI_AnyThread().AddSP(this, &FControlRigEditor::OnPreConstructionForUI_AnyThread);
		DebuggedControlRig->OnPreConstruction_AnyThread().AddSP(this, &FControlRigEditor::OnPreConstruction_AnyThread);
		DebuggedControlRig->OnPostConstruction_AnyThread().AddSP(this, &FControlRigEditor::OnPostConstruction_AnyThread);
		DebuggedControlRig->ControlModified().AddSP(this, &FControlRigEditor::HandleOnControlModified);
		
		LastHierarchyHash = INDEX_NONE;

		if(EditorSkelComp)
		{
			EditorSkelComp->SetComponentToWorld(FTransform::Identity);
		}

		if(!bShouldExecute)
		{
			if (FControlRigEditMode* EditMode = GetEditMode())
			{
				EditMode->RequestToRecreateControlShapeActors();
			}
		}
	}
	else
	{
		if (FControlRigEditMode* EditMode = GetEditMode())
		{
			EditMode->SetObjects(nullptr,  nullptr,nullptr);
		}
	}
}

void FControlRigEditor::SetDetailViewForRigElements()
{
	URigHierarchy* HierarchyBeingDebugged = GetHierarchyBeingDebugged();
	SetDetailViewForRigElements(HierarchyBeingDebugged->GetSelectedKeys());
}

void FControlRigEditor::SetDetailViewForRigElements(const TArray<FRigElementKey>& InKeys)
{
	if(IsDetailsPanelRefreshSuspended())
	{
		return;
	}

	ClearDetailObject();

	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	URigHierarchy* HierarchyBeingDebugged = GetHierarchyBeingDebugged();
	TArray<UObject*> Objects;

	for(const FRigElementKey& Key : InKeys)
	{
		FRigBaseElement* Element = HierarchyBeingDebugged->Find(Key);
		if (Element == nullptr)
		{
			continue;
		}

		URigVMDetailsViewWrapperObject* WrapperObject = URigVMDetailsViewWrapperObject::MakeInstance(GetDetailWrapperClass(), GetBlueprintObj(), Element->GetElementStruct(), (uint8*)Element, HierarchyBeingDebugged);
		WrapperObject->GetWrappedPropertyChangedChainEvent().AddSP(this, &FControlRigEditor::OnWrappedPropertyChangedChainEvent);
		WrapperObject->AddToRoot();

		Objects.Add(WrapperObject);
	}
	
	SetDetailObjects(Objects);
}

void FControlRigEditor::SetDetailObjects(const TArray<UObject*>& InObjects)
{
	// if no modules should be selected - we need to deselect all modules
	if (!InObjects.ContainsByPredicate([](const UObject* InObject) -> bool
	{
		return IsValid(InObject) && InObject->IsA<UControlRig>();
	}))
	{
		ModulesSelected.Reset();
	}
	
	IControlRigEditor::SetDetailObjects(InObjects);
}

void FControlRigEditor::RefreshDetailView()
{
	if(DetailViewShowsAnyRigElement())
	{
		SetDetailViewForRigElements();
		return;
	}
	else if(!ModulesSelected.IsEmpty())
	{
		SetDetailViewForRigModules();
		return;
	}

	FRigVMEditor::RefreshDetailView();
}

bool FControlRigEditor::DetailViewShowsAnyRigElement() const
{
	return DetailViewShowsStruct(FRigBaseElement::StaticStruct());
}

bool FControlRigEditor::DetailViewShowsRigElement(FRigElementKey InKey) const
{
	TArray< TWeakObjectPtr<UObject> > SelectedObjects = Inspector->GetSelectedObjects();
	for (TWeakObjectPtr<UObject> SelectedObject : SelectedObjects)
	{
		if (SelectedObject.IsValid())
		{
			if(URigVMDetailsViewWrapperObject* WrapperObject = Cast<URigVMDetailsViewWrapperObject>(SelectedObject.Get()))
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

TArray<FRigElementKey> FControlRigEditor::GetSelectedRigElementsFromDetailView() const
{
	TArray<FRigElementKey> Elements;
	
	TArray< TWeakObjectPtr<UObject> > SelectedObjects = Inspector->GetSelectedObjects();
	for (TWeakObjectPtr<UObject> SelectedObject : SelectedObjects)
	{
		if (SelectedObject.IsValid())
		{
			if(URigVMDetailsViewWrapperObject* WrapperObject = Cast<URigVMDetailsViewWrapperObject>(SelectedObject.Get()))
			{
				if (const UScriptStruct* WrappedStruct = WrapperObject->GetWrappedStruct())
				{
					if (WrappedStruct->IsChildOf(FRigBaseElement::StaticStruct()))
					{
						Elements.Add(WrapperObject->GetContent<FRigBaseElement>().GetKey());
					}
				}
			}
		}
	}

	return Elements;
}

void FControlRigEditor::SetDetailViewForRigModules()
{
	SetDetailViewForRigModules(ModulesSelected);
}

void FControlRigEditor::SetDetailViewForRigModules(const TArray<FString> InKeys)
{
	if(IsDetailsPanelRefreshSuspended())
	{
		return;
	}

	ClearDetailObject();

	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	UModularRig* RigBeingDebugged = Cast<UModularRig>(RigBlueprint->GetDebuggedControlRig());

	if (!RigBeingDebugged)
	{
		return;
	}

	ModulesSelected = InKeys;
	TArray<UObject*> Objects;

	for(const FString& Key : InKeys)
	{
		const FRigModuleInstance* Element = RigBeingDebugged->FindModule(Key);
		if (Element == nullptr)
		{
			continue;
		}

		if(UControlRig* ModuleInstance = Element->GetRig())
		{
			Objects.Add(ModuleInstance);
		}
	}
	
	SetDetailObjects(Objects);

	// In case the modules selected are still not available, lets set them again
	if (Objects.IsEmpty())
	{
		ModulesSelected = InKeys;
	}
}

bool FControlRigEditor::DetailViewShowsAnyRigModule() const
{
	return DetailViewShowsStruct(FRigModuleInstance::StaticStruct());
}

bool FControlRigEditor::DetailViewShowsRigModule(FString InKey) const
{
	TArray< TWeakObjectPtr<UObject> > SelectedObjects = Inspector->GetSelectedObjects();
	for (TWeakObjectPtr<UObject> SelectedObject : SelectedObjects)
	{
		if (SelectedObject.IsValid())
		{
			if(URigVMDetailsViewWrapperObject* WrapperObject = Cast<URigVMDetailsViewWrapperObject>(SelectedObject.Get()))
			{
				if (const UScriptStruct* WrappedStruct = WrapperObject->GetWrappedStruct())
				{
					if (WrappedStruct->IsChildOf(FRigModuleInstance::StaticStruct()))
					{
						if(WrapperObject->GetContent<FRigModuleInstance>().GetPath() == InKey)
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

void FControlRigEditor::Compile()
{
	{
		DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

		TUniquePtr<UControlRigBlueprint::FControlValueScope> ValueScope;
		if (!UControlRigEditorSettings::Get()->bResetControlsOnCompile) // if we need to retain the controls
		{
			ValueScope = MakeUnique<UControlRigBlueprint::FControlValueScope>(GetControlRigBlueprint());
		}

		UControlRigBlueprint* ControlRigBlueprint = GetControlRigBlueprint();
		if (ControlRigBlueprint == nullptr)
		{
			return;
		}

		TArray< TWeakObjectPtr<UObject> > SelectedObjects = Inspector->GetSelectedObjects();

		if(IsConstructionModeEnabled())
		{
			SetEventQueue(ForwardsSolveEventQueue, false);
		}

		// clear transient controls such that we don't leave
		// a phantom shape in the viewport
		// have to do this before compile() because during compile
		// a new control rig instance is created without the transient controls
		// so clear is never called for old transient controls
		ControlRigBlueprint->ClearTransientControls();

		// default to always reset all bone modifications 
		ResetAllBoneModification(); 

		{
			FRigVMEditor::Compile();
		}

		ControlRigBlueprint->RecompileModularRig();

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
		
		if (SelectedObjects.Num() > 0)
		{
			RefreshDetailView();
		}

		if (UControlRigEditorSettings::Get()->bResetControlTransformsOnCompile)
		{
			ControlRigBlueprint->Hierarchy->ForEach<FRigControlElement>([ControlRigBlueprint](FRigControlElement* ControlElement) -> bool
            {
				const FTransform Transform = ControlRigBlueprint->Hierarchy->GetInitialLocalTransform(ControlElement->GetIndex());

				/*/
				if (UControlRig* ControlRig = GetControlRig())
				{
					ControlRig->Modify();
					ControlRig->GetControlHierarchy().SetLocalTransform(Control.Index, Transform);
					ControlRig->ControlModified().Broadcast(ControlRig, Control, EControlRigSetKey::DoNotCare);
				}
				*/

				ControlRigBlueprint->Hierarchy->SetLocalTransform(ControlElement->GetIndex(), Transform);
				return true;
			});
		}

		ControlRigBlueprint->PropagatePoseFromBPToInstances();

		if (FControlRigEditMode* EditMode = GetEditMode())
		{
			EditMode->RecreateControlShapeActors(GetHierarchyBeingDebugged()->GetSelectedKeys());
		}
	}
}

void FControlRigEditor::HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject)
{
	IControlRigEditor::HandleModifiedEvent(InNotifType, InGraph, InSubject);

	if(InNotifType == ERigVMGraphNotifType::NodeSelected)
	{
		if(const URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(InSubject))
		{
			SetDirectionManipulationSubject(UnitNode);
		}
	}
	else if(InNotifType == ERigVMGraphNotifType::NodeSelectionChanged)
	{
		bool bNeedsToClearManipulationSubject = true;
		const TArray<FName> SelectedNodes = InGraph->GetSelectNodes();
		if(SelectedNodes.Num() == 1)
		{
			if(const URigVMUnitNode* UnitNode = Cast<URigVMUnitNode>(InGraph->FindNodeByName(SelectedNodes[0])))
			{
				SetDirectionManipulationSubject(UnitNode);
				bNeedsToClearManipulationSubject = false;
			}
		}

		if(bNeedsToClearManipulationSubject)
		{
			ClearDirectManipulationSubject();
		}
	}
	else if(InNotifType == ERigVMGraphNotifType::PinDefaultValueChanged)
	{
		if(const URigVMPin* Pin = Cast<URigVMPin>(InSubject))
		{
			if(Pin->GetNode() == DirectManipulationSubject.Get())
			{
				bRefreshDirectionManipulationTargetsRequired = true;
			}
		}
	}
	else if(InNotifType == ERigVMGraphNotifType::LinkAdded ||
		InNotifType == ERigVMGraphNotifType::LinkRemoved)
	{
		if(const URigVMLink* Link = Cast<URigVMLink>(InSubject))
		{
			if((Link->GetSourceNode() == DirectManipulationSubject.Get()) ||
				(Link->GetTargetNode() == DirectManipulationSubject.Get()))
			{
				bRefreshDirectionManipulationTargetsRequired = true;
			}
		}
	}
}

void FControlRigEditor::OnCreateGraphEditorCommands(TSharedPtr<FUICommandList> GraphEditorCommandsList)
{
	IControlRigEditor::OnCreateGraphEditorCommands(GraphEditorCommandsList);

	GraphEditorCommandsList->MapAction(
		FControlRigEditorCommands::Get().RequestDirectManipulationPosition,
		FExecuteAction::CreateSP(this, &FControlRigEditor::HandleRequestDirectManipulationPosition));
	GraphEditorCommandsList->MapAction(
		FControlRigEditorCommands::Get().RequestDirectManipulationRotation,
		FExecuteAction::CreateSP(this, &FControlRigEditor::HandleRequestDirectManipulationRotation));
	GraphEditorCommandsList->MapAction(
		FControlRigEditorCommands::Get().RequestDirectManipulationScale,
		FExecuteAction::CreateSP(this, &FControlRigEditor::HandleRequestDirectManipulationScale));
}

void FControlRigEditor::HandleVMCompiledEvent(UObject* InCompiledObject, URigVM* InVM, FRigVMExtendedExecuteContext& InContext)
{
	IControlRigEditor::HandleVMCompiledEvent(InCompiledObject, InVM, InContext);

	if(bRefreshDirectionManipulationTargetsRequired)
	{
		RefreshDirectManipulationTextList();
		bRefreshDirectionManipulationTargetsRequired = false;
	}

	if(UControlRigBlueprint* ControlRigBlueprint = GetControlRigBlueprint())
	{
		if(UControlRig* ControlRig = InVM->GetTypedOuter<UControlRig>())
		{
			ControlRigBlueprint->UpdateElementKeyRedirector(ControlRig);
		}
	}
}

void FControlRigEditor::SaveAsset_Execute()
{
	FRigVMEditor::SaveAsset_Execute();

	// Save the new state of the hierarchy in the default object, so that it has the correct values on load
	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	if(const UControlRig* ControlRig = GetControlRig())
	{
		UControlRig* CDO = ControlRig->GetClass()->GetDefaultObject<UControlRig>();
		CDO->DynamicHierarchy->CopyHierarchy(RigBlueprint->Hierarchy);
		RigBlueprint->UpdateElementKeyRedirector(CDO);
	}

	FBlueprintActionDatabase& ActionDatabase = FBlueprintActionDatabase::Get();
	ActionDatabase.ClearAssetActions(UControlRigBlueprint::StaticClass());
	ActionDatabase.RefreshClassActions(UControlRigBlueprint::StaticClass());
}

void FControlRigEditor::SaveAssetAs_Execute()
{
	FRigVMEditor::SaveAssetAs_Execute();

	// Save the new state of the hierarchy in the default object, so that it has the correct values on load
	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	if(const UControlRig* ControlRig = GetControlRig())
	{
		UControlRig* CDO = ControlRig->GetClass()->GetDefaultObject<UControlRig>();
		CDO->DynamicHierarchy->CopyHierarchy(RigBlueprint->Hierarchy);
		RigBlueprint->UpdateElementKeyRedirector(CDO);
	}

	FBlueprintActionDatabase& ActionDatabase = FBlueprintActionDatabase::Get();
	ActionDatabase.ClearAssetActions(UControlRigBlueprint::StaticClass());
	ActionDatabase.RefreshClassActions(UControlRigBlueprint::StaticClass());
}

bool FControlRigEditor::IsModularRig() const
{
	if(UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
	{
		return RigBlueprint->IsModularRig();
	}
	return false;
}

FName FControlRigEditor::GetToolkitFName() const
{
	return FName("ControlRigEditor");
}

FText FControlRigEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Control Rig Editor");
}

FString FControlRigEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Control Rig Editor ").ToString();
}

FReply FControlRigEditor::OnSpawnGraphNodeByShortcut(FInputChord InChord, const FVector2D& InPosition, UEdGraph* InGraph)
{
	const FReply SuperReply = FRigVMEditor::OnSpawnGraphNodeByShortcut(InChord, InPosition, InGraph);
	if(SuperReply.IsEventHandled())
	{
		return SuperReply;
	}

	if(!InChord.HasAnyModifierKeys())
	{
		if(UControlRigGraph* RigGraph = Cast<UControlRigGraph>(InGraph))
		{
			if(URigVMController* Controller = RigGraph->GetController())
			{
				if(InChord.Key == EKeys::S)
				{
					Controller->AddUnitNode(FRigVMFunction_Sequence::StaticStruct(), FRigUnit::GetMethodName(), InPosition, FString(), true, true);
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

	return FReply::Unhandled();
}

void FControlRigEditor::PostTransaction(bool bSuccess, const FTransaction* Transaction, bool bIsRedo)
{
	EnsureValidRigElementsInDetailPanel();

	if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
	{
		// Do not compile here. ControlRigBlueprint::PostTransacted decides when it is necessary to compile depending
		// on the properties that are affected.
		//Compile();

		UpdateRigVMHost();
		
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

void FControlRigEditor::EnsureValidRigElementsInDetailPanel()
{
	UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint();
	URigHierarchy* Hierarchy = ControlRigBP->Hierarchy; 

	TArray< TWeakObjectPtr<UObject> > SelectedObjects = Inspector->GetSelectedObjects();
	for (TWeakObjectPtr<UObject> SelectedObject : SelectedObjects)
	{
		if (SelectedObject.IsValid())
		{
			if(URigVMDetailsViewWrapperObject* WrapperObject = Cast<URigVMDetailsViewWrapperObject>(SelectedObject.Get()))
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

void FControlRigEditor::OnAnimInitialized()
{
	UControlRigSkeletalMeshComponent* EditorSkelComp = Cast<UControlRigSkeletalMeshComponent>(GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent());
	if (EditorSkelComp)
	{
		EditorSkelComp->bRequiredBonesUpToDateDuringTick = 0;

		UControlRigLayerInstance* AnimInstance = Cast<UControlRigLayerInstance>(EditorSkelComp->GetAnimInstance());
		if (AnimInstance && GetControlRig())
		{
			// update control rig data to anim instance since animation system has been reinitialized
			FInputBlendPose Filter;
			AnimInstance->ResetControlRigTracks();
			AnimInstance->AddControlRigTrack(0, GetControlRig());
			AnimInstance->UpdateControlRigTrack(0, 1.0f, FControlRigIOSettings::MakeEnabled(), bExecutionControlRig);
		}
	}
}

void FControlRigEditor::HandleVMExecutedEvent(URigVMHost* InHost, const FName& InEventName)
{
	FRigVMEditor::HandleVMExecutedEvent(InHost, InEventName);

	if (UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint())
	{
		URigHierarchy* Hierarchy = GetHierarchyBeingDebugged(); 

		TArray< TWeakObjectPtr<UObject> > SelectedObjects = Inspector->GetSelectedObjects();
		for (TWeakObjectPtr<UObject> SelectedObject : SelectedObjects)
		{
			if (SelectedObject.IsValid())
			{
				if(URigVMDetailsViewWrapperObject* WrapperObject = Cast<URigVMDetailsViewWrapperObject>(SelectedObject.Get()))
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

		// update transient controls on nodes / pins
		if(UControlRig* DebuggedControlRig = Cast<UControlRig>(ControlRigBP->GetObjectBeingDebugged()))
		{
			if(!DebuggedControlRig->RigUnitManipulationInfos.IsEmpty())
			{
				const FRigHierarchyRedirectorGuard RedirectorGuard(DebuggedControlRig);
				FControlRigExecuteContext& ExecuteContext = DebuggedControlRig->GetRigVMExtendedExecuteContext().GetPublicDataSafe<FControlRigExecuteContext>();
				
				for(const TSharedPtr<FRigDirectManipulationInfo>& ManipulationInfo : DebuggedControlRig->RigUnitManipulationInfos)
				{
					if(const URigVMUnitNode* Node = ManipulationInfo->Node.Get())
					{
						const UScriptStruct* ScriptStruct = Node->GetScriptStruct();
						if(ScriptStruct == nullptr)
						{
							continue;
						}

						TSharedPtr<FStructOnScope> NodeInstance = Node->ConstructLiveStructInstance(DebuggedControlRig);
						if(!NodeInstance.IsValid() || !NodeInstance->IsValid())
						{
							continue;
						}

						// if we are not manipulating right now - reset the info so that it can follow the hierarchy
						if (FControlRigEditorEditMode* EditMode = GetEditMode())
						{
							if(!EditMode->bIsTracking)
							{
								ManipulationInfo->Reset();
							}
						}
				
						FRigUnit* UnitInstance = UControlRig::GetRigUnitInstanceFromScope(NodeInstance);
						UnitInstance->UpdateHierarchyForDirectManipulation(Node, NodeInstance, ExecuteContext, ManipulationInfo);
						ManipulationInfo->bInitialized = true;
						UnitInstance->PerformDebugDrawingForDirectManipulation(Node, NodeInstance, ExecuteContext, ManipulationInfo);
					}
				}
			}
		}		
	}
}

void FControlRigEditor::CreateEditorModeManager()
{
	EditorModeManager = MakeShareable(FModuleManager::LoadModuleChecked<FPersonaModule>("Persona").CreatePersonaEditorModeManager());
}

void FControlRigEditor::Tick(float DeltaTime)
{
	FRigVMEditor::Tick(DeltaTime);

	bool bDrawHierarchyBones = false;

	// tick the control rig in case we don't have skeletal mesh
	if (UControlRigBlueprint* Blueprint = GetControlRigBlueprint())
	{
		UControlRig* ControlRig = GetControlRig();
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

void FControlRigEditor::HandleViewportCreated(const TSharedRef<class IPersonaViewport>& InViewport)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	PreviewViewport = InViewport;

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
		if (const UControlRigBlueprint* Blueprint = GetControlRigBlueprint())
		{
			if(Blueprint->IsModularRig())
			{
				if(Blueprint->GetPreviewMesh() == nullptr)
				{
					return EVisibility::Collapsed;
				}
			}
			const bool bUpToDate = (Blueprint->Status == BS_UpToDate) || (Blueprint->Status == BS_UpToDateWithWarnings);
			return bUpToDate ? EVisibility::Collapsed : EVisibility::Visible;
		}

		return EVisibility::Collapsed;
	};

	auto GetCompileButtonVisibility = [this]()
	{
		if (const UControlRigBlueprint* Blueprint = GetControlRigBlueprint())
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

	{
		FPersonaViewportNotificationOptions DirectManipulationNotificationOptions(TAttribute<EVisibility>::CreateSP(this, &FControlRigEditor::GetDirectManipulationVisibility));
		DirectManipulationNotificationOptions.OnGetBrushOverride = TAttribute<const FSlateBrush*>(FControlRigEditorStyle::Get().GetBrush("ControlRig.Viewport.Notification.DirectManipulation"));

		InViewport->AddNotification(
			EMessageSeverity::Info,
			false,
			SNew(SHorizontalBox)
			.Visibility(this, &FControlRigEditor::GetDirectManipulationVisibility)
			.ToolTipText(LOCTEXT("DirectManipulation", "Direct Manipulation"))
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(4.0f, 4.0f)
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
					.Text(FEditorFontGlyphs::Crosshairs)
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SAssignNew(DirectManipulationCombo, SComboBox<TSharedPtr<FString>>)
					.ContentPadding(FMargin(4.0f, 2.0f))
					.OptionsSource(&DirectManipulationTextList)
					.OnGenerateWidget_Lambda([this](TSharedPtr<FString> Item)
					{ 
						return SNew(SBox)
							.MaxDesiredWidth(600.0f)
							[
								SNew(STextBlock)
								.TextStyle(FAppStyle::Get(), "AnimViewport.MessageText")
								.Text(FText::FromString(*Item))
							];
					} )	
					.OnSelectionChanged(this, &FControlRigEditor::OnDirectManipulationChanged)
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "AnimViewport.MessageText")
						.Text(this, &FControlRigEditor::GetDirectionManipulationText)
					]
				]
			],
			DirectManipulationNotificationOptions
		);
	}

	{
		InViewport->AddNotification(
			EMessageSeverity::Warning,
			false,
			SNew(SHorizontalBox)
			.Visibility(this, &FControlRigEditor::GetConnectorWarningVisibility)
			.ToolTipText(LOCTEXT("ConnectorWarningTooltip", "This rig has unresolved connectors."))
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(4.0f, 4.0f)
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
					.Text(FEditorFontGlyphs::Exclamation_Triangle)
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "AnimViewport.MessageText")
					.Text(this, &FControlRigEditor::GetConnectorWarningText)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SButton)
					.ForegroundColor(FSlateColor::UseForeground())
					.ButtonStyle(FAppStyle::Get(), "FlatButton.Primary")
					.ToolTipText(LOCTEXT("ConnectorWarningNavigateTooltip", "Navigate to the first unresolved connector in the hierarchy"))
					.OnClicked(this, &FControlRigEditor::OnNavigateToConnectorWarning)
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
							.Text(LOCTEXT("ConnectorWarningNavigateButtonLabel", "Discover"))
						]
					]
				]
			],
			FPersonaViewportNotificationOptions(TAttribute<EVisibility>::CreateSP(this, &FControlRigEditor::GetConnectorWarningVisibility))
		);
	}	

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

	FPersonaViewportNotificationOptions ChangePreviewMeshNotificationOptions;
	ChangePreviewMeshNotificationOptions.OnGetVisibility = IsModularRig() ? EVisibility::Visible : EVisibility::Collapsed;
	//ChangePreviewMeshNotificationOptions.OnGetBrushOverride = TAttribute<const FSlateBrush*>(FControlRigEditorStyle::Get().GetBrush("ControlRig.Viewport.Notification.ChangeShapeTransform"));

	// notification to allow to change the preview mesh directly in the viewport
	InViewport->AddNotification(TAttribute<EMessageSeverity::Type>::CreateLambda([this]()
		{
			if(const UControlRigBlueprint* Blueprint = GetControlRigBlueprint())
			{
				if(Blueprint->GetPreviewMesh() == nullptr)
				{
					return EMessageSeverity::Warning;
				}
			}
			return EMessageSeverity::Info;
		}),
		false,
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(4.0f, 4.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MissingPreviewMesh", "Please choose a preview mesh!"))
			.TextStyle(FAppStyle::Get(), "AnimViewport.MessageText")
			.Visibility_Lambda([this]()
			{
				if(const UControlRigBlueprint* Blueprint = GetControlRigBlueprint())
				{
					if(Blueprint->GetPreviewMesh())
					{
						return EVisibility::Collapsed;
					}
				}
				return EVisibility::Visible;
			})
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(4.0f, 4.0f)
		[
			SNew(SObjectPropertyEntryBox)
			.ObjectPath_Lambda([this]()
			{
				if(const UControlRigBlueprint* Blueprint = GetControlRigBlueprint())
				{
					if(const USkeletalMesh* PreviewMesh = Blueprint->GetPreviewMesh())
					{
						return PreviewMesh->GetPathName();
					}
				}
				return FString();
			})
			.AllowedClass(USkeletalMesh::StaticClass())
			.OnObjectChanged_Lambda([this](const FAssetData& InAssetData)
			{
				if(const UControlRigBlueprint* Blueprint = GetControlRigBlueprint())
				{
					if(USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(InAssetData.GetAsset()))
					{
						const TSharedRef<IPersonaPreviewScene> CurrentPreviewScene = GetPersonaToolkit()->GetPreviewScene();
						CurrentPreviewScene->SetPreviewMesh(SkeletalMesh);
					}
				}
			})
			.AllowCreate(false)
			.AllowClear(false)
			.DisplayUseSelected(false)
			.DisplayBrowse(false)
			.NewAssetFactories(TArray<UFactory*>())
		],
		ChangePreviewMeshNotificationOptions
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
						.IsEnabled(this, &FControlRigEditor::IsToolbarDrawSocketsEnabled)
						[
							SNew(SCheckBox)
							.IsChecked(this, &FControlRigEditor::GetToolbarDrawSockets)
							.OnCheckStateChanged(this, &FControlRigEditor::OnToolbarDrawSocketsChanged)
							.ToolTipText(LOCTEXT("ControlRigDrawSocketsToolTip", "If checked all sockets are drawn."))
						]
					],
					LOCTEXT("ControlRigDisplaySockets", "Display Sockets")
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

	if (CVarShowSchematicPanelOverlay->GetBool())
	{
		if (UControlRigBlueprint* Blueprint = GetControlRigBlueprint())
		{
			if (Blueprint->IsModularRig())
			{
				SchematicViewport = SNew(SSchematicGraphPanel)
																.GraphData(&SchematicModel)
																.IsOverlay(true)
																.PaddingLeft(30)
																.PaddingRight(30)
																.PaddingTop(60)
																.PaddingBottom(60)
																.PaddingInterNode(5)
				;
				InViewport->AddOverlayWidget(SchematicViewport.ToSharedRef());

				
				InViewport->AddToolbarExtender(TEXT("ControlRig"), FMenuExtensionDelegate::CreateLambda([&](FMenuBuilder& InMenuBuilder)
					{
						InMenuBuilder.AddMenuSeparator(TEXT("Modular Rig"));
						InMenuBuilder.BeginSection("ModularRig", LOCTEXT("ModularRig_Label", "Modular Rig"));
						{
							InMenuBuilder.AddMenuEntry(FControlRigEditorCommands::Get().ToggleSchematicViewportVisibility);
						}
						InMenuBuilder.EndSection();
					}
				));
			}
		}
	}
	
	InViewport->GetKeyDownDelegate().BindLambda([&](const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) -> FReply {
		if (OnKeyDownDelegate.IsBound())
		{
			FReply Reply = OnKeyDownDelegate.Execute(MyGeometry, InKeyEvent);
			if(Reply.IsEventHandled())
			{
				return Reply;
			}
		}
		if(GetToolkitCommands()->ProcessCommandBindings(InKeyEvent.GetKey(), InKeyEvent.GetModifierKeys(), false))
		{
			return FReply::Handled();
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

void FControlRigEditor::HandleToggleSchematicViewport()
{
	if(SchematicViewport.IsValid())
	{
		SchematicModel.UpdateControlRigContent();
		SchematicViewport->ToggleVisibility();
	}
}

bool FControlRigEditor::IsSchematicViewportActive() const
{
	if (SchematicViewport.IsValid())
	{
		return SchematicViewport->GetVisibility() != EVisibility::Hidden;
	}
	return false;
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
	if (const UControlRig* ControlRig = GetControlRig())
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

bool FControlRigEditor::IsToolbarDrawSocketsEnabled() const
{
	if (const UControlRig* ControlRig = GetControlRig())
	{
		if (!ControlRig->IsConstructionModeEnabled())
		{
			return true;
		}
	}
	return false;
}

ECheckBoxState FControlRigEditor::GetToolbarDrawSockets() const
{
	if (const UControlRigEditModeSettings* Settings = GetDefault<UControlRigEditModeSettings>())
	{
		return Settings->bDisplaySockets ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void FControlRigEditor::OnToolbarDrawSocketsChanged(ECheckBoxState InNewValue)
{
	if (UControlRigEditModeSettings* Settings = GetMutableDefault<UControlRigEditModeSettings>())
	{
		Settings->bDisplaySockets = InNewValue == ECheckBoxState::Checked;
	}
}

bool FControlRigEditor::IsConstructionModeEnabled() const
{
	return GetEventQueue() == ConstructionEventQueue;
}

bool FControlRigEditor::IsDebuggingExternalControlRig(const UControlRig* InControlRig) const
{
	if(InControlRig == nullptr)
	{
		if(const UControlRigBlueprint* ControlRigBlueprint = GetControlRigBlueprint())
		{
			InControlRig = Cast<UControlRig>(ControlRigBlueprint->GetObjectBeingDebugged());
		}
	}
	return InControlRig != GetControlRig();
}

bool FControlRigEditor::ShouldExecuteControlRig(const UControlRig* InControlRig) const
{
	return (!IsDebuggingExternalControlRig(InControlRig)) && bExecutionControlRig;
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

	// leave some metadata on the world used for debug object labeling
	if(FWorldContext* WorldContext = GEngine->GetWorldContextFromWorld(InPersonaPreviewScene->GetWorld()))
	{
		static constexpr TCHAR Format[] = TEXT("ControlRigEditor (%s)");
		WorldContext->CustomDescription = FString::Printf(Format, *GetBlueprintObj()->GetName());
	}

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

	// remove the preview scene undo handling - it has unwanted side effects
	InPersonaPreviewScene->UnregisterForUndo();
}

void FControlRigEditor::UpdateRigVMHost()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FRigVMEditor::UpdateRigVMHost();

	UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	if(UClass* Class = Blueprint->GeneratedClass)
	{
		UControlRigSkeletalMeshComponent* EditorSkelComp = Cast<UControlRigSkeletalMeshComponent>(GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent());
		UControlRigLayerInstance* AnimInstance = Cast<UControlRigLayerInstance>(EditorSkelComp->GetAnimInstance());
		UControlRig* ControlRig = GetControlRig();

		if (AnimInstance && ControlRig)
		{
 			PreviewInstance = Cast<UAnimPreviewInstance>(AnimInstance->GetSourceAnimInstance());
			ControlRig->PreviewInstance = PreviewInstance;

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

			ControlRig->OnPreForwardsSolve_AnyThread().RemoveAll(this);
			ControlRig->ControlModified().RemoveAll(this);

			ControlRig->OnPreForwardsSolve_AnyThread().AddSP(this, &FControlRigEditor::OnPreForwardsSolve_AnyThread);
			ControlRig->ControlModified().AddSP(this, &FControlRigEditor::HandleOnControlModified);
		}

		if(IsModularRig() && ControlRig)
		{
			if(SchematicModel.ControlRigBlueprint.IsValid())
			{
				SchematicModel.OnSetObjectBeingDebugged(ControlRig);
			}
		}
	}
}

void FControlRigEditor::UpdateRigVMHost_PreClearOldHost(URigVMHost* InPreviousHost)
{
	if(TestDataStrongPtr.IsValid())
	{
		TestDataStrongPtr->ReleaseReplay();
	}
}

void FControlRigEditor::CacheNameLists()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	FRigVMEditor::CacheNameLists();

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

			const TArray<TSoftObjectPtr<UControlRigShapeLibrary>>* ShapeLibraries = &ControlRigBP->ShapeLibraries;
			if(const UControlRig* DebuggedControlRig = Hierarchy->GetTypedOuter<UControlRig>())
			{
				ShapeLibraries = &DebuggedControlRig->GetShapeLibraries();
			}
			RigGraph->CacheNameLists(Hierarchy, &ControlRigBP->DrawContainer, *ShapeLibraries);
		}
	}
}

FVector2D FControlRigEditor::ComputePersonaProjectedScreenPos(const FVector& InWorldPos, bool bClampToScreenRectangle)
{
	if (PreviewViewport.IsValid())
	{
		FEditorViewportClient& Client = PreviewViewport->GetViewportClient();
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
							Client.Viewport,
							Client.GetScene(),
							Client.EngineShowFlags));
		// SceneView is deleted with the ViewFamily
		FSceneView* SceneView = Client.CalcSceneView(&ViewFamily);
	
		// Compute the MinP/MaxP in pixel coord, relative to View.ViewRect.Min
		const FMatrix& WorldToView = SceneView->ViewMatrices.GetViewMatrix();
		const FMatrix& ViewToProj = SceneView->ViewMatrices.GetProjectionMatrix();
		const float NearClippingDistance = SceneView->NearClippingDistance + SMALL_NUMBER;
		const FIntRect ViewRect = SceneView->UnconstrainedViewRect;

		// Clamp position on the near plane to get valid rect even if bounds' points are behind the camera
		FPlane P_View = WorldToView.TransformFVector4(FVector4(InWorldPos, 1.f));
		if (P_View.Z <= NearClippingDistance)
		{
			P_View.Z = NearClippingDistance;
		}

		// Project from view to projective space
		FVector2D MinP(FLT_MAX, FLT_MAX);
		FVector2D MaxP(-FLT_MAX, -FLT_MAX);
		FVector2D ScreenPos;
		const bool bIsValid = FSceneView::ProjectWorldToScreen(P_View, ViewRect, ViewToProj, ScreenPos);

		// Clamp to pixel border
		ScreenPos = FIntPoint(FMath::FloorToInt(ScreenPos.X), FMath::FloorToInt(ScreenPos.Y));

		// Clamp to screen rect
		if(bClampToScreenRectangle)
		{
			ScreenPos.X = FMath::Clamp(ScreenPos.X, ViewRect.Min.X, ViewRect.Max.X);
			ScreenPos.Y = FMath::Clamp(ScreenPos.Y, ViewRect.Min.Y, ViewRect.Max.Y);
		}

		return FVector2D(ScreenPos.X, ScreenPos.Y);
	}
	return FVector2D::ZeroVector;
}

void FControlRigEditor::HandlePreviewMeshChanged(USkeletalMesh* InOldSkeletalMesh, USkeletalMesh* InNewSkeletalMesh)
{
	RebindToSkeletalMeshComponent();

	if (GetObjectsCurrentlyBeingEdited()->Num() > 0)
	{
		if (UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint())
		{
			ControlRigBP->SetPreviewMesh(InNewSkeletalMesh);

			FModularRigConnections PreviousConnections;
			if(IsModularRig())
			{
				PreviousConnections = ControlRigBP->ModularRigModel.Connections;
				{
					TGuardValue<bool> SuspendBlueprintNotifs(ControlRigBP->bSuspendAllNotifications, true);
					if(URigHierarchyController* Controller = ControlRigBP->GetHierarchyController())
					{
						// remove all connectors / sockets. keeping them around may mess up the order of the elements
						// in the hierarchy, such as [bone,bone,bone,connector,connector,bone,bone,bone].
						TArray<FRigElementKey> ConnectorsAndSockets = Controller->GetHierarchy()->GetConnectorKeys();
						ConnectorsAndSockets.Append(Controller->GetHierarchy()->GetSocketKeys());
						for(const FRigElementKey& Key : ConnectorsAndSockets)
						{
							(void)Controller->RemoveElement(Key, true, true);
						}
						
						USkeleton* Skeleton = InNewSkeletalMesh ? InNewSkeletalMesh->GetSkeleton() : nullptr;
						Controller->ImportBones(Skeleton, NAME_None, true, true, false, true, true);
						Controller->ImportCurves(Skeleton, NAME_None, false, true, true);
					}
				}
				ControlRigBP->PropagateHierarchyFromBPToInstances();
			}
			
			UpdateRigVMHost();
			
			if(UControlRig* DebuggedControlRig = Cast<UControlRig>(ControlRigBP->GetObjectBeingDebugged()))
			{
				DebuggedControlRig->GetHierarchy()->Notify(ERigHierarchyNotification::HierarchyReset, nullptr);
				DebuggedControlRig->Initialize(true);
			}

			Compile();

			if(IsModularRig())
			{
				if(UControlRig* DebuggedControlRig = Cast<UControlRig>(ControlRigBP->GetObjectBeingDebugged()))
				{
					DebuggedControlRig->RequestConstruction();
					DebuggedControlRig->Execute(FRigUnit_PrepareForExecution::EventName);
					
					if(URigHierarchy* Hierarchy = DebuggedControlRig->GetHierarchy())
					{
						FModularRigModel* Model = &ControlRigBP->ModularRigModel;
						
						// try to reestablish the connections.
						UModularRigController* ModularRigController = ControlRigBP->GetModularRigController();
						Model->ForEachModule(
							[Model, Hierarchy, ModularRigController, PreviousConnections]
							(const FRigModuleReference* Module) -> bool
							{
								bool bContinueResolval;
								do
								{
									bContinueResolval = false;
									
									const TArray<const FRigConnectorElement*> Connectors = Module->FindConnectors(Hierarchy);
									TArray<FRigElementKey> PrimaryConnectors, SecondaryConnectors, OptionalConnectors;
									for(const FRigConnectorElement* ExistingConnector : Connectors)
									{
										if(ExistingConnector->IsPrimary())
										{
											PrimaryConnectors.Add(ExistingConnector->GetKey());
										}
										else if(ExistingConnector->IsOptional())
										{
											OptionalConnectors.Add(ExistingConnector->GetKey());
										}
										else
										{
											SecondaryConnectors.Add(ExistingConnector->GetKey());
										}
									}
									TArray<FRigElementKey> ConnectorKeys;
									ConnectorKeys.Append(PrimaryConnectors);
									ConnectorKeys.Append(SecondaryConnectors);
									ConnectorKeys.Append(OptionalConnectors);
									
									for(const FRigElementKey& ConnectorKey : ConnectorKeys)
									{
										const bool bIsPrimary = ConnectorKey == ConnectorKeys[0];
										const bool bIsSecondary = !bIsPrimary;
										
										if(!Model->Connections.HasConnection(ConnectorKey, Hierarchy))
										{
											// try to reapply the connection
											if(PreviousConnections.HasConnection(ConnectorKey, Hierarchy))
											{
												const FRigElementKey Target = PreviousConnections.FindTargetFromConnector(ConnectorKey);
												if(ModularRigController->ConnectConnectorToElement(ConnectorKey, Target, true))
												{
													bContinueResolval = true;
												}
											}

											// try to auto resolve it
											if(!bContinueResolval && bIsSecondary)
											{
												if(ModularRigController->AutoConnectSecondaryConnectors({ConnectorKey}, true, true))
												{
													bContinueResolval = true;
												}
											}

											// only do one connector at a time
											break;
										}
									}
								}
								while (bContinueResolval);

								return true; // continue to the next module
							}
						);
					}
				}
			}
		}
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

void FControlRigEditor::GenerateEventQueueMenuContent(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.BeginSection(TEXT("Events"));
    MenuBuilder.AddMenuEntry(FControlRigEditorCommands::Get().ConstructionEvent, TEXT("Setup"), TAttribute<FText>(), TAttribute<FText>(), GetEventQueueIcon(ConstructionEventQueue));
    MenuBuilder.AddMenuEntry(FControlRigEditorCommands::Get().ForwardsSolveEvent, TEXT("Forwards Solve"), TAttribute<FText>(), TAttribute<FText>(), GetEventQueueIcon(ForwardsSolveEventQueue));
    MenuBuilder.AddMenuEntry(FControlRigEditorCommands::Get().BackwardsSolveEvent, TEXT("Backwards Solve"), TAttribute<FText>(), TAttribute<FText>(), GetEventQueueIcon(BackwardsSolveEventQueue));
    MenuBuilder.EndSection();

    MenuBuilder.BeginSection(TEXT("Validation"));
    MenuBuilder.AddMenuEntry(FControlRigEditorCommands::Get().BackwardsAndForwardsSolveEvent, TEXT("BackwardsAndForwards"), TAttribute<FText>(), TAttribute<FText>(), GetEventQueueIcon(BackwardsAndForwardsSolveEventQueue));
    MenuBuilder.EndSection();

    if (const UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
    {
    	URigVMEdGraphSchema* Schema = CastChecked<URigVMEdGraphSchema>(RigBlueprint->GetRigVMEdGraphSchemaClass()->GetDefaultObject());
    	
    	bool bFoundUserDefinedEvent = false;
    	const TArray<FName> EntryNames = RigBlueprint->GetRigVMClient()->GetEntryNames();
    	for(const FName& EntryName : EntryNames)
    	{
    		if(Schema->IsRigVMDefaultEvent(EntryName))
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
    				FExecuteAction::CreateSP(this, &FRigVMEditor::SetEventQueue, TArray<FName>({EntryName})),
    				FCanExecuteAction()
    			)
    		);
    	}
    }
}

void FControlRigEditor::FilterDraggedKeys(TArray<FRigElementKey>& Keys, bool bRemoveNameSpace)
{
	// if the keys being dragged contain something mapped to a connector - use that instead
	if(UControlRigBlueprint* ControlRigBlueprint = GetControlRigBlueprint())
	{
		TArray<FRigElementKey> FilteredKeys;
		FilteredKeys.Reserve(Keys.Num());
		for (FRigElementKey Key : Keys)
		{
			for(const FModularRigSingleConnection& Connection : ControlRigBlueprint->ModularRigModel.Connections)
			{
				if(Connection.Target == Key)
				{
					Key = Connection.Connector;
					break;
				}
			}

			if(bRemoveNameSpace)
			{
				const FString Name = Key.Name.ToString();
				int32 LastCharIndex = INDEX_NONE;
				if(Name.FindLastChar(TEXT(':'), LastCharIndex))
				{
					Key.Name = *Name.Mid(LastCharIndex+1);
				}
			}
			else
			{
				if(const UControlRig* DebuggedControlRig = Cast<UControlRig>(ControlRigBlueprint->GetObjectBeingDebugged()))
				{
					if(!DebuggedControlRig->GetHierarchy()->Contains(Key))
					{
						const FString NameSpace = DebuggedControlRig->GetRigModuleNameSpace();
						if(!NameSpace.IsEmpty())
						{
							static constexpr TCHAR Format[] = TEXT("%s%s");
							Key.Name = *FString::Printf(Format, *NameSpace, *Key.Name.ToString());
						}
					}
				}
			}
			FilteredKeys.Add(Key);
		}
		Keys = FilteredKeys;
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
			DebuggedControlRig = GetControlRig();
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
		case ERigElementType::Connector:
		case ERigElementType::Socket:
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

void FControlRigEditor::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
	FRigVMEditor::OnFinishedChangingProperties(PropertyChangedEvent);
	
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint();

	if (ControlRigBP)
	{
		if (PropertyChangedEvent.MemberProperty->GetNameCPP() == GET_MEMBER_NAME_STRING_CHECKED(UControlRigBlueprint, HierarchySettings))
		{
			ControlRigBP->PropagateHierarchyFromBPToInstances();
		}

		else if (PropertyChangedEvent.MemberProperty->GetNameCPP() == GET_MEMBER_NAME_STRING_CHECKED(UControlRigBlueprint, DrawContainer))
		{
			ControlRigBP->PropagateDrawInstructionsFromBPToInstances();
		}

		else if (PropertyChangedEvent.MemberProperty->GetNameCPP() == GET_MEMBER_NAME_STRING_CHECKED(UControlRigBlueprint, RigModuleSettings))
		{
			ControlRigBP->PropagateHierarchyFromBPToInstances();
		}
	}
}

void FControlRigEditor::OnWrappedPropertyChangedChainEvent(URigVMDetailsViewWrapperObject* InWrapperObject, const FString& InPropertyPath, FPropertyChangedChainEvent& InPropertyChangedChainEvent)
{
	FRigVMEditor::OnWrappedPropertyChangedChainEvent(InWrapperObject, InPropertyPath, InPropertyChangedChainEvent);
	
	check(InWrapperObject);
	check(!GetWrapperObjects().IsEmpty());

	TGuardValue<bool> SuspendDetailsPanelRefresh(GetSuspendDetailsPanelRefreshFlag(), true);

	UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint();

	FString PropertyPath = InPropertyPath;
	if(UScriptStruct* WrappedStruct = InWrapperObject->GetWrappedStruct())
	{
		if(WrappedStruct->IsChildOf(FRigBaseElement::StaticStruct()))
		{
			check(WrappedStruct == GetWrapperObjects()[0]->GetWrappedStruct());

			URigHierarchy* Hierarchy = CastChecked<URigHierarchy>(InWrapperObject->GetSubject());
			const FRigBaseElement WrappedElement = InWrapperObject->GetContent<FRigBaseElement>();
			const FRigBaseElement FirstWrappedElement = GetWrapperObjects()[0]->GetContent<FRigBaseElement>();
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

				const FTransform Transform = GetWrapperObjects()[0]->GetContent<FRigControlElement>().Offset.Get(TransformType);
				
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

				const FTransform Transform = GetWrapperObjects()[0]->GetContent<FRigControlElement>().Shape.Get(TransformType);
				
				ControlRigBP->Hierarchy->SetControlShapeTransform(ControlElement, Transform, ERigTransformType::MakeInitial(TransformType), true, false, true);
			}
			else if(PropertyPath.RemoveFromStart(SettingsString))
			{
				if(Key.Type == ERigElementType::Control)
				{
					const FRigControlSettings Settings  = InWrapperObject->GetContent<FRigControlElement>().Settings;

					FRigControlElement* ControlElement = ControlRigBP->Hierarchy->Find<FRigControlElement>(WrappedElement.GetKey());
					if(ControlElement == nullptr)
					{
						return;
					}

					ControlRigBP->Hierarchy->SetControlSettings(ControlElement, Settings, true, false, true);
				}
				else if(Key.Type == ERigElementType::Connector)
				{
					const FRigConnectorSettings Settings  = InWrapperObject->GetContent<FRigConnectorElement>().Settings;

					FRigConnectorElement* ConnectorElement = ControlRigBP->Hierarchy->Find<FRigConnectorElement>(WrappedElement.GetKey());
					if(ConnectorElement == nullptr)
					{
						return;
					}

					ControlRigBP->Hierarchy->SetConnectorSettings(ConnectorElement, Settings, true, false, true);
				}
			}

			if(IsConstructionModeEnabled() || bIsInitial)
			{
				ControlRigBP->PropagatePoseFromBPToInstances();
				ControlRigBP->Modify();
				ControlRigBP->MarkPackageDirty();
			}
		}
	}
}

bool FControlRigEditor::HandleRequestDirectManipulation(ERigControlType InControlType) const
{
	TArray<FRigDirectManipulationTarget> Targets = GetDirectManipulationTargets();
	for(const FRigDirectManipulationTarget& Target : Targets)
	{
		if(Target.ControlType == InControlType || Target.ControlType == ERigControlType::EulerTransform)
		{
			if (FControlRigEditorEditMode* EditMode = GetEditMode())
			{
				switch(InControlType)
				{
					case ERigControlType::Position:
					{
						EditMode->RequestTransformWidgetMode(UE::Widget::WM_Translate);
						break;
					}
					case ERigControlType::Rotator:
					{
						EditMode->RequestTransformWidgetMode(UE::Widget::WM_Rotate);
						break;
					}
					case ERigControlType::Scale:
					{
						EditMode->RequestTransformWidgetMode(UE::Widget::WM_Scale);
						break;
					}
					default:
					{
						break;
					}
				}
			}

			if(UControlRigBlueprint* Blueprint = GetControlRigBlueprint())
			{
				Blueprint->AddTransientControl(DirectManipulationSubject.Get(), Target);
			}
			return true;
		}
	}
	return false;
}

bool FControlRigEditor::SetDirectionManipulationSubject(const URigVMUnitNode* InNode)
{
	if(DirectManipulationSubject.Get() == InNode)
	{
		return false;
	}
	if(UControlRigBlueprint* Blueprint = GetControlRigBlueprint())
	{
		Blueprint->ClearTransientControls();
	}
	DirectManipulationSubject = InNode;

	// update the direct manipulation target list
	RefreshDirectManipulationTextList();
	return true;
}

bool FControlRigEditor::IsDirectManipulationEnabled() const
{
	return !GetDirectManipulationTargets().IsEmpty();
}

EVisibility FControlRigEditor::GetDirectManipulationVisibility() const
{
	return IsDirectManipulationEnabled() ? EVisibility::Visible : EVisibility::Hidden;
}

FText FControlRigEditor::GetDirectionManipulationText() const
{
	if (UControlRig* DebuggedControlRig = Cast<UControlRig>(GetBlueprintObj()->GetObjectBeingDebugged()))
	{
		if (URigHierarchy* Hierarchy = DebuggedControlRig->GetHierarchy())
		{
			TArray<FRigControlElement*> TransientControls = Hierarchy->GetTransientControls();
			for(const FRigControlElement* TransientControl : TransientControls)
			{
				const FString Target = UControlRig::GetTargetFromTransientControl(TransientControl->GetKey());
				if(!Target.IsEmpty())
				{
					return FText::FromString(Target);
				}
			}
		}
	}
	static const FText DefaultText = LOCTEXT("ControlRigDirectManipulation", "Direct Manipulation");
	return DefaultText;
}

void FControlRigEditor::OnDirectManipulationChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo)
{
	if(!NewValue.IsValid())
	{
		return;
	}
	
	const URigVMUnitNode* UnitNode = DirectManipulationSubject.Get();
	if(UnitNode == nullptr)
	{
		return;
	}
	
	UControlRigBlueprint* ControlRigBlueprint = CastChecked<UControlRigBlueprint>(GetBlueprintObj());
	if(ControlRigBlueprint == nullptr)
	{
		return;
	}

	// disable literal folding for the moment
	if(ControlRigBlueprint->VMCompileSettings.ASTSettings.bFoldLiterals)
	{
		ControlRigBlueprint->VMCompileSettings.ASTSettings.bFoldLiterals = false;
		ControlRigBlueprint->RecompileVM();
	}

	const FString& DesiredTarget = *NewValue.Get();
	const TArray<FRigDirectManipulationTarget> Targets = GetDirectManipulationTargets();
	for(const FRigDirectManipulationTarget& Target : Targets)
	{
		if(Target.Name.Equals(DesiredTarget, ESearchCase::CaseSensitive))
		{
			// run the task after a bit so that the rig has the opportunity to run first
			FFunctionGraphTask::CreateAndDispatchWhenReady([ControlRigBlueprint, UnitNode, Target]()
			{
				ControlRigBlueprint->AddTransientControl(UnitNode, Target);
			}, TStatId(), NULL, ENamedThreads::GameThread);
			break;
		}
	}
}

const TArray<FRigDirectManipulationTarget> FControlRigEditor::GetDirectManipulationTargets() const
{
	if(DirectManipulationSubject.IsValid())
	{
		if (UControlRig* DebuggedControlRig = Cast<UControlRig>(GetBlueprintObj()->GetObjectBeingDebugged()))
		{
			if(const URigVMUnitNode* Node = DirectManipulationSubject.Get())
			{
				if(Node->IsPartOfRuntime(DebuggedControlRig))
				{
					const TSharedPtr<FStructOnScope> NodeInstance = Node->ConstructLiveStructInstance(DebuggedControlRig);
					if(NodeInstance.IsValid() && NodeInstance->IsValid())
					{
						if(const FRigUnit* UnitInstance = UControlRig::GetRigUnitInstanceFromScope(NodeInstance))
						{
							TArray<FRigDirectManipulationTarget> Targets;
							if(UnitInstance->GetDirectManipulationTargets(Node, NodeInstance, DebuggedControlRig->GetHierarchy(), Targets, nullptr))
							{
								return Targets;
							}
						}
					}
				}
			}
		}
	}

	static const TArray<FRigDirectManipulationTarget> EmptyTargets;
	return EmptyTargets;
}

const TArray<TSharedPtr<FString>>& FControlRigEditor::GetDirectManipulationTargetTextList() const
{
	if(DirectManipulationTextList.IsEmpty())
	{
		const TArray<FRigDirectManipulationTarget> Targets = GetDirectManipulationTargets();
		for(const FRigDirectManipulationTarget& Target : Targets)
		{
			DirectManipulationTextList.Emplace(new FString(Target.Name));
		}
	}
	return DirectManipulationTextList;
}

void FControlRigEditor::RefreshDirectManipulationTextList()
{
	DirectManipulationTextList.Reset();
	(void)GetDirectManipulationTargetTextList();
	if(DirectManipulationCombo.IsValid())
	{
		DirectManipulationCombo->RefreshOptions();
	}
}

EVisibility FControlRigEditor::GetConnectorWarningVisibility() const
{
	if(GetConnectorWarningText().IsEmpty())
	{
		return EVisibility::Hidden;
	}
	return EVisibility::Visible;
}

FText FControlRigEditor::GetConnectorWarningText() const
{
	if (const UControlRigBlueprint* Blueprint = GetControlRigBlueprint())
	{
		if(Blueprint->IsControlRigModule())
		{
			if(UControlRig* ControlRig = GetControlRig())
			{
				FString FailureReason;
				if(!ControlRig->AllConnectorsAreResolved(&FailureReason))
				{
					if(FailureReason.IsEmpty())
					{
						static const FText ConnectorWarningDefault = LOCTEXT("ConnectorWarningDefault", "This rig has unresolved connectors.");
						return ConnectorWarningDefault;
					}
					return FText::FromString(FailureReason);
				}
			}
		}
	}
	return FText();
}

FReply FControlRigEditor::OnNavigateToConnectorWarning() const
{
	RequestNavigateToConnectorWarningDelegate.Broadcast();
	return FReply::Handled();
}

void FControlRigEditor::BindCommands()
{
	FRigVMEditor::BindCommands();

	GetToolkitCommands()->MapAction(
		FControlRigEditorCommands::Get().ConstructionEvent,
		FExecuteAction::CreateSP(this, &FRigVMEditor::SetEventQueue, TArray<FName>(ConstructionEventQueue)),
		FCanExecuteAction());

	GetToolkitCommands()->MapAction(
		FControlRigEditorCommands::Get().ForwardsSolveEvent,
		FExecuteAction::CreateSP(this, &FRigVMEditor::SetEventQueue, TArray<FName>(ForwardsSolveEventQueue)),
		FCanExecuteAction());

	GetToolkitCommands()->MapAction(
		FControlRigEditorCommands::Get().BackwardsSolveEvent,
		FExecuteAction::CreateSP(this, &FRigVMEditor::SetEventQueue, TArray<FName>(BackwardsSolveEventQueue)),
		FCanExecuteAction());

	GetToolkitCommands()->MapAction(
		FControlRigEditorCommands::Get().BackwardsAndForwardsSolveEvent,
		FExecuteAction::CreateSP(this, &FRigVMEditor::SetEventQueue, TArray<FName>(BackwardsAndForwardsSolveEventQueue)),
		FCanExecuteAction());

	GetToolkitCommands()->MapAction(
		FControlRigEditorCommands::Get().ToggleSchematicViewportVisibility,
		FExecuteAction::CreateSP(this, &FControlRigEditor::HandleToggleSchematicViewport),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FControlRigEditor::IsSchematicViewportActive));
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
		{
			if (!RigBlueprint->IsModularRig())
			{
				if(InElement->GetType() == ERigElementType::Connector)
				{
					if(InHierarchy->GetConnectors().Num() == 1)
					{
						FNotificationInfo Info(LOCTEXT("FirstConnectorEncountered", "Looks like you have added the first connector. This rig will now be configured as a module, settings can be found in the class settings Hierarchy -> Module Settings."));
						Info.bFireAndForget = true;
						Info.FadeOutDuration = 5.0f;
						Info.ExpireDuration = 5.0f;

						TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
						NotificationPtr->SetCompletionState(SNotificationItem::CS_Success);

						RigBlueprint->TurnIntoControlRigModule();
					}
				}
			}
			// no break - fall through
		}
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

			const FString RemovedElementName = InElement->GetName();
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
									(ModelPin->GetCPPType() == TEXT("FName") && ModelPin->GetCustomWidgetName() == TEXT("CurveName") && RemovedElementType == ERigElementType::Curve) ||
									(ModelPin->GetCPPType() == TEXT("FName") && ModelPin->GetCustomWidgetName() == TEXT("ConnectorName") && RemovedElementType == ERigElementType::Connector))
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
			const FString NewNameStr = InElement->GetName();
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
					FRigVMBlueprintCompileScope CompileScope(RigBlueprint);
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
										(ModelPin->GetCPPType() == TEXT("FName") && ModelPin->GetCustomWidgetName() == TEXT("CurveName") && ElementType == ERigElementType::Curve) ||
										(ModelPin->GetCPPType() == TEXT("FName") && ModelPin->GetCustomWidgetName() == TEXT("ConnectorName") && ElementType == ERigElementType::Connector))
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

	if(SchematicViewport)
	{
		SchematicModel.OnHierarchyModified(InNotif, InHierarchy, InElement);
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
							// only clear the details if we are not looking at a transient control
							if (UControlRig* DebuggedControlRig = Cast<UControlRig>(GetBlueprintObj()->GetObjectBeingDebugged()))
							{
								if(DebuggedControlRig->RigUnitManipulationInfos.IsEmpty())
								{
									ClearDetailObject();
								}
							}
						}
					}
				}
				break;
			}
			case ERigHierarchyNotification::ElementAdded:
			case ERigHierarchyNotification::ElementRemoved:
			case ERigHierarchyNotification::ElementRenamed:
			{
				if (Key.IsValid() && Key.Type == ERigElementType::Connector)
				{
					UControlRigBlueprint* RigBlueprint = GetControlRigBlueprint();
					check(RigBlueprint);
					RigBlueprint->UpdateExposedModuleConnectors();
				}
				// Fallthrough to next case
			}
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
			case ERigHierarchyNotification::ConnectorSettingChanged:
			{
				UControlRigBlueprint* RigBlueprint = GetControlRigBlueprint();
				check(RigBlueprint);
				RigBlueprint->UpdateExposedModuleConnectors();
				RigBlueprint->RecompileModularRig();
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

void FControlRigEditor::HandleRigTypeChanged(UControlRigBlueprint* InBlueprint)
{
	// todo: fire a notification.
	// todo: reapply the preview mesh and react to it accordingly.

	Compile();
}

void FControlRigEditor::HandleModularRigModified(EModularRigNotification InNotification, const FRigModuleReference* InModule)
{
	switch(InNotification)
	{
		case EModularRigNotification::ModuleAdded:
		{
			ModulesSelected = {InModule->GetPath()};
			break;
		}
		case EModularRigNotification::ModuleRemoved:
		{
			if (DetailViewShowsAnyRigModule())
			{
				ClearDetailObject();
			}

			// todo: update SchematicGraph
			break;
		}
		case EModularRigNotification::ModuleReparented:
		case EModularRigNotification::ModuleRenamed:
		{
			FString OldPath;
			if (InNotification == EModularRigNotification::ModuleRenamed)
			{
				OldPath = URigHierarchy::JoinNameSpace(InModule->ParentPath, InModule->PreviousName.ToString());
			}
			else
			{
				OldPath = URigHierarchy::JoinNameSpace(InModule->PreviousParentPath, InModule->Name.ToString());
			}
			ModulesSelected.Remove(OldPath);
			ModulesSelected.Add(InModule->GetPath());
			RefreshDetailView();

			// todo: update SchematicGraph
			break;
		}
		case EModularRigNotification::ConnectionChanged:
		{
			// todo: update SchematicGraph
			break;
		}
	}
}

void FControlRigEditor::HandlePostCompileModularRigs(URigVMBlueprint* InBlueprint)
{
	RefreshDetailView();
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
 			const int32 BoneIndex = EditorSkelComp->GetReferenceSkeleton().FindBoneIndex(SelectedBone->GetFName());
			if(BoneIndex != INDEX_NONE)
			{
				EditorSkelComp->BonesOfInterest.AddUnique(BoneIndex);
			}
		}
	}
}

void FControlRigEditor::UpdateBoneModification(FName BoneName, const FTransform& LocalTransform)
{
	if (UControlRig* ControlRig = GetControlRig())
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
	if (UControlRig* ControlRig = GetControlRig())
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
	if (UControlRig* ControlRig = GetControlRig())
	{
		if (IsValid(PreviewInstance))
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
	return static_cast<FControlRigEditorEditMode*>(GetEditorModeManager().GetActiveMode(GetEditorModeName()));
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
		UpdateRigVMHost();
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
					TArray<FRigElementKey> DraggedKeys = DragDropContext.DraggedElementKeys;
					ControlRigEditor->FilterDraggedKeys(DraggedKeys, true);
					
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
							SetterTooltip = LOCTEXT("SetNull_ToolTip", "Setter For Null");
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
						else if ((DraggedTypes & (uint8)ERigElementType::Connector) != 0)
						{
							GetterLabel = LOCTEXT("GetConnector","Get Connector");
							GetterTooltip = LOCTEXT("GetConnector_ToolTip", "Getter For Connector");
							SetterLabel = LOCTEXT("SetConnector","Set Connector");
							SetterTooltip = LOCTEXT("SetConnector_ToolTip", "Setter For Connector");
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
						((DraggedTypes & (uint8)ERigElementType::Null) != 0) ||
						((DraggedTypes & (uint8)ERigElementType::Connector) != 0))
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

											if (URigVMNode* ItemsNode = Controller->AddUnitNode(FRigUnit_ItemArray::StaticStruct(), TEXT("Execute"), NodePosition))
											{
												if (URigVMPin* ItemsPin = ItemsNode->FindPin(TEXT("Items")))
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

	TArray<FRigElementKey> KeysIncludingNameSpace = Keys;
	FilterDraggedKeys(KeysIncludingNameSpace, false);

	for (int32 Index = 0; Index < Keys.Num(); Index++)
	{
		const FRigElementKey& Key = Keys[Index];
		const FRigElementKey& KeyIncludingNameSpace = KeysIncludingNameSpace[Index];
		
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

		if(FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(KeyIncludingNameSpace))
		{
			if(ControlElement->IsAnimationChannel())
			{
				ChannelValue = ControlElement->GetDisplayName();
				
				if(const FRigControlElement* ParentControlElement = Cast<FRigControlElement>(Hierarchy->GetFirstParent(ControlElement)))
				{
					NameValue = ParentControlElement->GetFName();
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
					case ERigControlType::ScaleFloat:
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
						FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(KeyIncludingNameSpace);
						if(ControlElement == nullptr)
						{
							return;
						}
						
						switch (ControlElement->Settings.ControlType)
						{
							case ERigControlType::Bool:
							{
								NamePins.Add(TEXT("Control"));
								StructTemplate = FRigUnit_GetControlBool::StaticStruct();
								break;
							}
							case ERigControlType::Float:
							case ERigControlType::ScaleFloat:
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
						FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(KeyIncludingNameSpace);
						if(ControlElement == nullptr)
						{
							return;
						}

						switch (ControlElement->Settings.ControlType)
						{
							case ERigControlType::Bool:
							{
								NamePins.Add(TEXT("Control"));
								StructTemplate = FRigUnit_SetControlBool::StaticStruct();
								break;
							}
							case ERigControlType::Float:
							case ERigControlType::ScaleFloat:
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

		FName Name = FRigVMBlueprintUtils::ValidateName(GetControlRigBlueprint(), StructTemplate->GetName());
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

			URigVMEdGraphUnitNodeSpawner::HookupMutableNode(ModelNode, GetControlRigBlueprint());
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
	UControlRig* DebuggedControlRig = Cast<UControlRig>(GetBlueprintObj()->GetObjectBeingDebugged());
	if (Subject != DebuggedControlRig)
	{
		return;
	}

	UControlRigBlueprint* Blueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	if (Blueprint == nullptr)
	{
		return;
	}

	URigHierarchy* Hierarchy = Subject->GetHierarchy();

	if (ControlElement->Settings.bIsTransientControl && !GIsTransacting)
	{
		const URigVMUnitNode* UnitNode = nullptr;
		const FString NodeName = UControlRig::GetNodeNameFromTransientControl(ControlElement->GetKey());
		const FString PoseTarget = UControlRig::GetTargetFromTransientControl(ControlElement->GetKey());
		TSharedPtr<FStructOnScope> NodeInstance;
		TSharedPtr<FRigDirectManipulationInfo> ManipulationInfo;

		// try to find the direct manipulation info on the rig. if there's no matching information
		// the manipulation is likely happening on a bone instead.
		if(DebuggedControlRig && !NodeName.IsEmpty() && !PoseTarget.IsEmpty())
		{
			UnitNode = Cast<URigVMUnitNode>(GetFocusedModel()->FindNode(NodeName));
			if(UnitNode)
			{
				if(UnitNode->GetScriptStruct())
				{
					NodeInstance = UnitNode->ConstructStructInstance(false);
					ManipulationInfo = DebuggedControlRig->GetRigUnitManipulationInfoForTransientControl(ControlElement->GetKey());
				}
				else
				{
					UnitNode = nullptr;
				}
			}
		}
		
		if (UnitNode && NodeInstance.IsValid() && ManipulationInfo.IsValid())
		{
			FRigUnit* UnitInstance = DebuggedControlRig->GetRigUnitInstanceFromScope(NodeInstance);
			check(UnitInstance);

			const FRigPose Pose = DebuggedControlRig->GetHierarchy()->GetPose();

			// update the node based on the incoming pose. once that is done we'll need to compare the node instance
			// with the settings on the node in the graph and update them accordingly.
			FControlRigExecuteContext& ExecuteContext = DebuggedControlRig->GetRigVMExtendedExecuteContext().GetPublicDataSafe<FControlRigExecuteContext>();
			const FRigHierarchyRedirectorGuard RedirectorGuard(DebuggedControlRig);
			if(UnitInstance->UpdateDirectManipulationFromHierarchy(UnitNode, NodeInstance, ExecuteContext, ManipulationInfo))
			{
				UnitNode->UpdateHostFromStructInstance(DebuggedControlRig, NodeInstance);
				DebuggedControlRig->GetHierarchy()->SetPose(Pose);
				
				URigVMController* Controller = Blueprint->GetOrCreateController(UnitNode->GetGraph());
				TMap<FString, FString> PinPathToNewDefaultValue;
				UnitNode->ComputePinValueDifferences(NodeInstance, PinPathToNewDefaultValue);
				if(!PinPathToNewDefaultValue.IsEmpty())
				{
					// we'll disable compilation since the control rig editor module will have disabled folding of literals
					// so each register is free to be edited directly.
					TGuardValue<bool> DisableBlueprintNotifs(Blueprint->bSuspendModelNotificationsForSelf, true);

					if(PinPathToNewDefaultValue.Num() > 1)
					{
						Controller->OpenUndoBracket(TEXT("Set pin defaults during manipulation"));
					}
					bool bChangedSomething = false;

					for(const TPair<FString, FString>& Pair : PinPathToNewDefaultValue)
					{
						if(const URigVMPin* Pin = UnitNode->FindPin(Pair.Key))
						{
							if(Controller->SetPinDefaultValue(Pin->GetPinPath(), Pair.Value, true, true, true, false, false))
							{
								bChangedSomething = true;
							}
						}
					}

					if(PinPathToNewDefaultValue.Num() > 1)
					{
						if(bChangedSomething)
						{
							Controller->CloseUndoBracket();
						}
						else
						{
							Controller->CancelUndoBracket();
						}
					}
				}

			}
		}
		else
		{
			FRigControlValue ControlValue = Hierarchy->GetControlValue(ControlElement, ERigControlValueType::Current);
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
				const FTransform GlobalTransform = GetControlRig()->GetControlGlobalTransform(ControlElement->GetFName());
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

void FControlRigEditor::HandleRefreshEditorFromBlueprint(URigVMBlueprint* InBlueprint)
{
	OnHierarchyChanged();
	FRigVMEditor::HandleRefreshEditorFromBlueprint(InBlueprint);
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

void FControlRigEditor::OnPreForwardsSolve_AnyThread(UControlRig* InRig, const FName& InEventName)
{
	// if we are debugging a PIE instance, we need to remember the input pose on the
	// rig so we can perform multiple evaluations. this is to avoid double transforms / double forward solve results.
	if(InRig->GetWorld()->IsPlayInEditor())
	{
		if(!InRig->GetWorld()->IsPaused())
		{
			// store the pose while PIE is running
			InRig->InputPoseOnDebuggedRig = InRig->GetHierarchy()->GetPose(false, false);
		}
		else
		{
			// reapply the pose as PIE is paused. during pause the rig won't be updated with the input pose
			// from the animbp / client thus we need to reset the pose to avoid double transformation.
			InRig->GetHierarchy()->SetPose(InRig->InputPoseOnDebuggedRig);
		}
	}
}

void FControlRigEditor::OnPreConstructionForUI_AnyThread(UControlRig* InRig, const FName& InEventName)
{
	bIsConstructionEventRunning = true;

	if(ShouldExecuteControlRig(InRig))
	{
		const TArrayView<const FRigElementKey> Elements;
		PreConstructionPose = InRig->GetHierarchy()->GetPose(false, ERigElementType::ToResetAfterConstructionEvent, Elements);

		if(const UControlRigBlueprint* RigBlueprint = GetControlRigBlueprint())
		{
			if(RigBlueprint->IsControlRigModule())
			{
				SocketStates = InRig->GetHierarchy()->GetSocketStates();
				ConnectorStates = RigBlueprint->Hierarchy->GetConnectorStates();
			}
		}
	}
}

void FControlRigEditor::OnPreConstruction_AnyThread(UControlRig* InRig, const FName& InEventName)
{
	if(UControlRigBlueprint* RigBlueprint = GetControlRigBlueprint())
	{
		if(RigBlueprint->IsControlRigModule())
		{
			if(RigBlueprint->PreviewSkeletalMesh)
			{
				if(URigHierarchy* Hierarchy = InRig->GetHierarchy())
				{
					if(URigHierarchyController* Controller = Hierarchy->GetController(true))
					{
						// find the instruction index for the construction event
						int32 InstructionIndex = INDEX_NONE;
						if(URigVM* VM = InRig->GetVM())
						{
							const int32 EntryIndex = VM->GetByteCode().FindEntryIndex(FRigUnit_PrepareForExecution::EventName);
							if(EntryIndex != INDEX_NONE)
							{
								InstructionIndex = VM->GetByteCode().GetEntry(EntryIndex).InstructionIndex;
							}
						}

						// import the bones for the preview hierarchy
						// use the ref skeleton so we'll only see the bones that are actually part of the mesh
						const TArray<FRigElementKey> Bones = Controller->ImportBones(RigBlueprint->PreviewSkeletalMesh->GetRefSkeleton(), NAME_None, false, false, false, false);
						for(const FRigElementKey& Bone : Bones)
						{
							if(FRigBaseElement* Element = Hierarchy->Find(Bone))
							{
								Element->CreatedAtInstructionIndex = InstructionIndex;
							}
						}

						// create a null to store controls under
						static const FRigElementKey ControlParentKey(TEXT("Controls"), ERigElementType::Null);
						if(!Hierarchy->Contains(ControlParentKey))
						{
							const FRigElementKey Null = Controller->AddNull(ControlParentKey.Name, FRigElementKey(), FTransform::Identity, true, false, false);
							if(FRigBaseElement* Element = Hierarchy->Find(Null))
							{
								Element->CreatedAtInstructionIndex = InstructionIndex;
							}
						}
					}
				}

				if(ShouldExecuteControlRig(InRig))
				{
					RigBlueprint->Hierarchy->RestoreSocketsFromStates(SocketStates);
					InRig->GetHierarchy()->RestoreSocketsFromStates(SocketStates);
				}
			}
		}
	}
}

void FControlRigEditor::OnPostConstruction_AnyThread(UControlRig* InRig, const FName& InEventName)
{
	bIsConstructionEventRunning = false;
	const bool bShouldExecute = ShouldExecuteControlRig(InRig);

	if(UControlRigBlueprint* RigBlueprint = GetControlRigBlueprint())
	{
		if(bShouldExecute && RigBlueprint->IsControlRigModule())
		{
			RigBlueprint->Hierarchy->RestoreConnectorsFromStates(ConnectorStates);
		}

		if(bShouldExecute && RigBlueprint->IsModularRig())
		{
			// auto resolve the root module's primary connector
			if(RigBlueprint->ModularRigModel.Connections.IsEmpty() && RigBlueprint->ModularRigModel.Modules.Num() == 1 && RigBlueprint->Hierarchy->Num(ERigElementType::Bone) > 0)
			{
				const FRigModuleReference& RootModule = RigBlueprint->ModularRigModel.Modules[0];

				const FSoftObjectPath DefaultRootModulePath = UControlRigSettings::Get()->DefaultRootModule;
				if(const UControlRigBlueprint* DefaultRootModule = Cast<UControlRigBlueprint>(DefaultRootModulePath.TryLoad()))
				{
					if(DefaultRootModule->GetControlRigClass() == RootModule.Class)
					{
						if(const FRigConnectorElement* PrimaryConnector = RootModule.FindPrimaryConnector(RigBlueprint->Hierarchy))
						{
							if(const FRigBoneElement* RootBone = RigBlueprint->Hierarchy->GetBones()[0])
							{
								if(UModularRigController* ModularRigController = RigBlueprint->ModularRigModel.GetController())
								{
									(void)ModularRigController->ConnectConnectorToElement(PrimaryConnector->GetKey(), RootBone->GetKey(), false);
								}
							}
						}
					}
				}
			}
		}
	}
	
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
				const TArray<FRigElementKey> Keys = GetSelectedRigElementsFromDetailView();
				SetDetailViewForRigElements(Keys);
			}
			
			if (FControlRigEditorEditMode* EditMode = GetEditMode())
            {
				if (InRig)
            	{
            		EditMode->bDrawHierarchyBones = !InRig->GetHierarchy()->GetBones().IsEmpty();
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
	else if(bShouldExecute)
	{
		InRig->GetHierarchy()->SetPose(PreConstructionPose, ERigTransformType::CurrentGlobal);
	}
}

#undef LOCTEXT_NAMESPACE
