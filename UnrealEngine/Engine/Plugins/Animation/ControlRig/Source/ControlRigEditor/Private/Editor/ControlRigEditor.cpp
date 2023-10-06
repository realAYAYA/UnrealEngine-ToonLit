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
#include "RigVMCore/RigVMExecuteContext.h"
#include "Editor/RigVMGraphDetailCustomization.h"

#define LOCTEXT_NAMESPACE "ControlRigEditor"

TAutoConsoleVariable<bool> CVarControlRigShowTestingToolbar(TEXT("ControlRig.Test.EnableTestingToolbar"), false, TEXT("When true we'll show the testing toolbar in Control Rig Editor."));

namespace ControlRigEditorTabs
{
	const FName DetailsTab(TEXT("DetailsTab"));
// 	const FName ViewportTab(TEXT("Viewport"));
// 	const FName AdvancedPreviewTab(TEXT("AdvancedPreviewTab"));
};

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
	, bIsConstructionEventRunning(false)
	, LastHierarchyHash(INDEX_NONE)
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

	// Post-layout initialization
	PostLayoutBlueprintEditorInitialization();

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
	}

	CreateRigHierarchyToGraphDragAndDropMenu();
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
	PersonaToolkit = PersonaModule.CreatePersonaToolkit(ControlRigBlueprint, PersonaToolkitArgs);

	// set delegate prior to setting mesh
	// otherwise, you don't get delegate
	PersonaToolkit->GetPreviewScene()->RegisterOnPreviewMeshChanged(FOnPreviewMeshChanged::CreateSP(this, &FControlRigEditor::HandlePreviewMeshChanged));

	// Set a default preview mesh, if any
	PersonaToolkit->SetPreviewMesh(ControlRigBlueprint->GetPreviewMesh(), false);
}

const FName FControlRigEditor::GetEditorAppName() const
{
	static const FName ControlRigEditorAppName(TEXT("ControlRigEditorApp"));
	return ControlRigEditorAppName;
}

const FName FControlRigEditor::GetEditorModeName() const
{
	return FControlRigEditorEditMode::ModeName;
}

TSharedPtr<FApplicationMode> FControlRigEditor::CreateEditorMode()
{
	CreatePersonaToolKitIfRequired();
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
		if(!PreviouslyDebuggedControlRig->HasAnyFlags(RF_BeginDestroyed))
		{
			PreviouslyDebuggedControlRig->GetHierarchy()->OnModified().RemoveAll(this);
			PreviouslyDebuggedControlRig->OnPreConstructionForUI_AnyThread().RemoveAll(this);
			PreviouslyDebuggedControlRig->OnPostConstruction_AnyThread().RemoveAll(this);
		}
	}

	if (UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj()))
	{
		RigBlueprint->Validator->SetControlRig(DebuggedControlRig);
	}

	if (DebuggedControlRig)
	{
		bool bIsExternalControlRig = DebuggedControlRig != GetControlRig();
		bool bShouldExecute = (!bIsExternalControlRig) && bExecutionControlRig;
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
}

void FControlRigEditor::SetDetailViewForRigElements()
{
	if(IsDetailsPanelRefreshSuspended())
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

		URigVMDetailsViewWrapperObject* WrapperObject = URigVMDetailsViewWrapperObject::MakeInstance(GetDetailWrapperClass(), GetBlueprintObj(), Element->GetElementStruct(), (uint8*)Element, HierarchyBeingDebugged);
		WrapperObject->GetWrappedPropertyChangedChainEvent().AddSP(this, &FControlRigEditor::OnWrappedPropertyChangedChainEvent);
		WrapperObject->AddToRoot();

		Objects.Add(WrapperObject);
	}
	
	SetDetailObjects(Objects);
}

void FControlRigEditor::RefreshDetailView()
{
	if(DetailViewShowsAnyRigElement())
	{
		SetDetailViewForRigElements();
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

void FControlRigEditor::SaveAsset_Execute()
{
	FRigVMEditor::SaveAsset_Execute();

	// Save the new state of the hierarchy in the default object, so that it has the correct values on load
	UControlRigBlueprint* RigBlueprint = Cast<UControlRigBlueprint>(GetBlueprintObj());
	if(const UControlRig* ControlRig = GetControlRig())
	{
		const UControlRig* CDO = ControlRig->GetClass()->GetDefaultObject<UControlRig>();
		CDO->DynamicHierarchy->CopyHierarchy(RigBlueprint->Hierarchy);
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
		const UControlRig* CDO = ControlRig->GetClass()->GetDefaultObject<UControlRig>();
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
										SAssignNew(PinControlNameList, SRigVMGraphPinNameListValueWidget)
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

bool FControlRigEditor::IsPinControlNameListEnabled() const
{
	if (UControlRig* ControlRig = GetControlRig())
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
	if (UControlRig* ControlRig = GetControlRig())
	{
		FText Result;
		ControlRig->GetHierarchy()->ForEach<FRigControlElement>([this, &Result, ControlRig](FRigControlElement* ControlElement) -> bool
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
	if (UControlRig* ControlRig = GetControlRig())
	{
		ControlRig->GetHierarchy()->ForEach<FRigControlElement>([this, NewTypeInValue, ControlRig](FRigControlElement* ControlElement) -> bool
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
	return GetEventQueue() == ConstructionEventQueue;
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

			ControlRig->ControlModified().AddSP(this, &FControlRigEditor::HandleOnControlModified);
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

void FControlRigEditor::HandlePreviewMeshChanged(USkeletalMesh* InOldSkeletalMesh, USkeletalMesh* InNewSkeletalMesh)
{
	RebindToSkeletalMeshComponent();

	if (GetObjectsCurrentlyBeingEdited()->Num() > 0)
	{
		if (UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint())
		{
			ControlRigBP->SetPreviewMesh(InNewSkeletalMesh);
			UpdateRigVMHost();
			
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
	}
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
	if (Subject != GetControlRig())
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

				if(UControlRig* ControlRig = GetControlRig())
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
				const FTransform GlobalTransform = GetControlRig()->GetControlGlobalTransform(ControlElement->GetName());
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

void FControlRigEditor::OnPreConstruction_AnyThread(UControlRig* InRig, const FName& InEventName)
{
	bIsConstructionEventRunning = true;
}

void FControlRigEditor::OnPostConstruction_AnyThread(UControlRig* InRig, const FName& InEventName)
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
}

#undef LOCTEXT_NAMESPACE
