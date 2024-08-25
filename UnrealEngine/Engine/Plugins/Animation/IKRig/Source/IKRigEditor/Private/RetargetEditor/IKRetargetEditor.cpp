// Copyright Epic Games, Inc. All Rights Reserved.

#include "RetargetEditor/IKRetargetEditor.h"

#include "AnimationEditorViewportClient.h"
#include "AnimationEditorPreviewActor.h"
#include "EditorModeManager.h"
#include "GameFramework/WorldSettings.h"
#include "Modules/ModuleManager.h"
#include "PersonaModule.h"
#include "IPersonaToolkit.h"
#include "IAssetFamily.h"
#include "ISkeletonEditorModule.h"
#include "Preferences/PersonaOptions.h"
#include "AnimCustomInstanceHelper.h"
#include "IPersonaViewport.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "Retargeter/IKRetargeter.h"
#include "Retargeter/IKRetargetOps.h"
#include "RetargetEditor/IKRetargetAnimInstance.h"
#include "RetargetEditor/IKRetargetCommands.h"
#include "RetargetEditor/IKRetargetEditPoseMode.h"
#include "RetargetEditor/IKRetargetApplicationMode.h"
#include "RetargetEditor/IKRetargetDefaultMode.h"
#include "RetargetEditor/IKRetargetEditorController.h"

#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "IKRetargeterEditor"

const FName IKRetargetApplicationModes::IKRetargetApplicationMode("IKRetargetApplicationMode");
const FName IKRetargetEditorAppName = FName(TEXT("IKRetargetEditorApp"));

FIKRetargetEditor::FIKRetargetEditor()
	: EditorController(MakeShared<FIKRetargetEditorController>())
	, PreviousTime(-1.0f)
{
}

void FIKRetargetEditor::InitAssetEditor(
	const EToolkitMode::Type Mode,
    const TSharedPtr<IToolkitHost>& InitToolkitHost, 
    UIKRetargeter* InAsset)
{
	EditorController->Initialize(SharedThis(this), InAsset);

	BindCommands();
	
	FPersonaToolkitArgs PersonaToolkitArgs;
	PersonaToolkitArgs.OnPreviewSceneCreated = FOnPreviewSceneCreated::FDelegate::CreateSP(this, &FIKRetargetEditor::HandlePreviewSceneCreated);
	PersonaToolkitArgs.OnPreviewSceneSettingsCustomized = FOnPreviewSceneSettingsCustomized::FDelegate::CreateSP(this, &FIKRetargetEditor::HandleOnPreviewSceneSettingsCustomized);
	
	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	PersonaToolkit = PersonaModule.CreatePersonaToolkit(InAsset, PersonaToolkitArgs);

	constexpr bool bCreateDefaultStandaloneMenu = true;
	constexpr bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(
		Mode, 
		InitToolkitHost, 
		IKRetargetEditorAppName, 
		FTabManager::FLayout::NullLayout, 
		bCreateDefaultStandaloneMenu, 
		bCreateDefaultToolbar, 
		InAsset);

	// this sets the application mode which defines the tab factory that builds the editor layout
	AddApplicationMode(
		IKRetargetApplicationModes::IKRetargetApplicationMode,
		MakeShareable(new FIKRetargetApplicationMode(SharedThis(this), PersonaToolkit->GetPreviewScene())));
	SetCurrentMode(IKRetargetApplicationModes::IKRetargetApplicationMode);

	// set the default editing mode to use in the editor
	GetEditorModeManager().SetDefaultMode(FIKRetargetDefaultMode::ModeName);
	
	// give default editing mode a pointer to the editor controller
	GetEditorModeManager().ActivateMode(FIKRetargetDefaultMode::ModeName);
	FIKRetargetDefaultMode* DefaultMode = GetEditorModeManager().GetActiveModeTyped<FIKRetargetDefaultMode>(FIKRetargetDefaultMode::ModeName);
	DefaultMode->SetEditorController(EditorController);
	GetEditorModeManager().DeactivateMode(FIKRetargetDefaultMode::ModeName);

	// give edit pose mode a pointer to the editor controller
	GetEditorModeManager().ActivateMode(FIKRetargetEditPoseMode::ModeName);
	FIKRetargetEditPoseMode* EditPoseMode = GetEditorModeManager().GetActiveModeTyped<FIKRetargetEditPoseMode>(FIKRetargetEditPoseMode::ModeName);
	EditPoseMode->SetEditorController(EditorController);
	GetEditorModeManager().DeactivateMode(FIKRetargetEditPoseMode::ModeName);

	ExtendToolbar();
	RegenerateMenusAndToolbars();

	// run retarget by default
	EditorController->SetRetargeterMode(ERetargeterOutputMode::RunRetarget);
}

void FIKRetargetEditor::OnClose()
{
	FPersonaAssetEditorToolkit::OnClose();
	EditorController->Close();
}

void FIKRetargetEditor::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_IKRigEditor", "IK Rig Editor"));
	TSharedRef<FWorkspaceItem> WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}

void FIKRetargetEditor::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

void FIKRetargetEditor::BindCommands()
{
	const FIKRetargetCommands& Commands = FIKRetargetCommands::Get();

	//
	// Retarget output modes
	//
	ToolkitCommands->MapAction(
		Commands.RunRetargeter,
		FExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::SetRetargeterMode, ERetargeterOutputMode::RunRetarget),
		FCanExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::IsReadyToRetarget));

	ToolkitCommands->MapAction(
		Commands.EditRetargetPose,
		FExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::SetRetargeterMode, ERetargeterOutputMode::EditRetargetPose),
		FCanExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::IsCurrentMeshLoaded),
		FIsActionChecked());


	//
	// Show various settings in details panel
	//
	ToolkitCommands->MapAction(
		Commands.ShowAssetSettings,
		FExecuteAction::CreateLambda([this]()
		{
			UIKRetargeter* Asset = EditorController->AssetController->GetAsset();
			return EditorController->SetDetailsObject(Asset);
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() ->bool
		{
			const UIKRetargeter* Asset = EditorController->AssetController->GetAsset();
			return EditorController->IsObjectInDetailsView(Asset);	
		}));
	ToolkitCommands->MapAction(
		Commands.ShowGlobalSettings,
		FExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::ShowGlobalSettings),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() ->bool
		{
			const UIKRetargetGlobalSettings* GlobalSettings = EditorController->AssetController->GetAsset()->GetGlobalSettingsUObject();
			return EditorController->IsObjectInDetailsView(GlobalSettings);	
		}));
	ToolkitCommands->MapAction(
		Commands.ShowRootSettings,
		FExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::ShowRootSettings),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() ->bool
		{
			const URetargetRootSettings* RootSettings = EditorController->AssetController->GetAsset()->GetRootSettingsUObject();
			return EditorController->IsObjectInDetailsView(RootSettings);	
		}));
	ToolkitCommands->MapAction(
		Commands.ShowPostSettings,
		FExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::ShowPostPhaseSettings),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() ->bool
		{
			const URetargetOpStack* PostSettings = EditorController->AssetController->GetAsset()->GetPostSettingsUObject();
			return EditorController->IsObjectInDetailsView(PostSettings);	
		}));

	//
	// Edit pose commands
	//
	
	ToolkitCommands->MapAction(
		Commands.ResetAllBones,
		FExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::HandleResetAllBones),
		FCanExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::IsEditingPose),
		EUIActionRepeatMode::RepeatDisabled);

	ToolkitCommands->MapAction(
		Commands.ResetSelectedBones,
		FExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::HandleResetSelectedBones),
		FCanExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::IsEditingPoseWithAnyBoneSelected),
		EUIActionRepeatMode::RepeatDisabled);

	ToolkitCommands->MapAction(
		Commands.ResetSelectedAndChildrenBones,
		FExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::HandleResetSelectedAndChildrenBones),
		FCanExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::IsEditingPoseWithAnyBoneSelected),
		EUIActionRepeatMode::RepeatDisabled);

	ToolkitCommands->MapAction(
		Commands.NewRetargetPose,
		FExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::HandleNewPose),
		FCanExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::IsEditingPose),
		EUIActionRepeatMode::RepeatDisabled);

	ToolkitCommands->MapAction(
		Commands.DuplicateRetargetPose,
		FExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::HandleDuplicatePose),
		FCanExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::IsEditingPose),
		EUIActionRepeatMode::RepeatDisabled);

	ToolkitCommands->MapAction(
		Commands.DeleteRetargetPose,
		FExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::HandleDeletePose),
		FCanExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::CanDeletePose),
		EUIActionRepeatMode::RepeatDisabled);

	ToolkitCommands->MapAction(
		Commands.RenameRetargetPose,
		FExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::HandleRenamePose),
		FCanExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::CanRenamePose),
		EUIActionRepeatMode::RepeatDisabled);

	//
	// Auto-gen retarget pose
	//
	
	ToolkitCommands->MapAction(
		Commands.AutoAlignAllBones,
		FExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::HandleAlignAllBones),
		FCanExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::IsEditingPose),
		EUIActionRepeatMode::RepeatDisabled);
	
	ToolkitCommands->MapAction(
		Commands.AlignSelected,
		FExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::HandleAlignSelectedBones, ERetargetAutoAlignMethod::ChainToChain, false /* no children*/),
		FCanExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::IsEditingPoseWithAnyBoneSelected),
		EUIActionRepeatMode::RepeatDisabled);

	ToolkitCommands->MapAction(
		Commands.AlignSelectedAndChildren,
		FExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::HandleAlignSelectedBones, ERetargetAutoAlignMethod::ChainToChain, true /* include children*/),
		FCanExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::IsEditingPoseWithAnyBoneSelected),
		EUIActionRepeatMode::RepeatDisabled);

	ToolkitCommands->MapAction(
		Commands.AlignSelectedUsingMesh,
		FExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::HandleAlignSelectedBones, ERetargetAutoAlignMethod::MeshToMesh, false /* no children*/),
		FCanExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::IsEditingPoseWithAnyBoneSelected),
		EUIActionRepeatMode::RepeatDisabled);

	ToolkitCommands->MapAction(
		Commands.SnapCharacterToGround,
		FExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::HandleSnapToGround),
		FCanExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::IsEditingPose),
		EUIActionRepeatMode::RepeatDisabled);
	

	//
	// Pose exporter
	//
	const TSharedRef<FIKRetargetPoseExporter> PoseExporterRef = EditorController->PoseExporter.ToSharedRef();
	
	ToolkitCommands->MapAction(
		Commands.ImportRetargetPose,
		FExecuteAction::CreateSP(PoseExporterRef, &FIKRetargetPoseExporter::HandleImportFromPoseAsset),
		FCanExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::IsEditingPose),
		EUIActionRepeatMode::RepeatDisabled);

	ToolkitCommands->MapAction(
		Commands.ImportRetargetPoseFromAnim,
		FExecuteAction::CreateSP(PoseExporterRef, &FIKRetargetPoseExporter::HandleImportFromSequenceAsset),
		FCanExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::IsEditingPose),
		EUIActionRepeatMode::RepeatDisabled);

	ToolkitCommands->MapAction(
		Commands.ExportRetargetPose,
		FExecuteAction::CreateSP(PoseExporterRef, &FIKRetargetPoseExporter::HandleExportPoseAsset),
		FCanExecuteAction::CreateSP(EditorController, &FIKRetargetEditorController::IsEditingPose),
		EUIActionRepeatMode::RepeatDisabled);
}

void FIKRetargetEditor::ExtendToolbar()
{
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	AddToolbarExtender(ToolbarExtender);

	ToolbarExtender->AddToolBarExtension(
        "Asset",
        EExtensionHook::After,
        GetToolkitCommands(),
        FToolBarExtensionDelegate::CreateSP(this, &FIKRetargetEditor::FillToolbar)
    );
}

void FIKRetargetEditor::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("Retarget Modes");
	{
		ToolbarBuilder.AddToolBarButton(
			FExecuteAction::CreateLambda([this]{ EditorController->SetRetargetModeToPreviousMode(); }),
			NAME_None, 
			TAttribute<FText>::Create(TAttribute<FText>::FGetter::CreateSP(EditorController, &FIKRetargetEditorController::GetRetargeterModeLabel)),
			TAttribute<FText>(), 
			TAttribute<FSlateIcon>::Create(TAttribute<FSlateIcon>::FGetter::CreateSP(EditorController, &FIKRetargetEditorController::GetCurrentRetargetModeIcon))
		);
		
		ToolbarBuilder.AddComboButton(
			FUIAction(),
			FOnGetContent::CreateSP(this, &FIKRetargetEditor::GenerateRetargetModesMenu),
			LOCTEXT("RetargetMode_Label", "UI Modes"),
			LOCTEXT("RetargetMode_ToolTip", "Choose which mode to display in the viewport."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Recompile"),
			true);

		ToolbarBuilder.AddSeparator();
		const TSharedPtr<SHorizontalBox> RetargetPhaseButtons = GenerateRetargetPhaseButtons();
		ToolbarBuilder.AddWidget(RetargetPhaseButtons.ToSharedRef());
	}
	ToolbarBuilder.EndSection();

	ToolbarBuilder.AddSeparator();
	ToolbarBuilder.AddWidget(SNew(SSpacer), NAME_None, true, HAlign_Right);

	ToolbarBuilder.BeginSection("Show Settings");
	{
		ToolbarBuilder.AddToolBarButton(
		FIKRetargetCommands::Get().ShowAssetSettings,
		NAME_None,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FIKRetargetEditorStyle::Get().GetStyleSetName(),"IKRetarget.AssetSettings"));
		
		ToolbarBuilder.AddToolBarButton(
		FIKRetargetCommands::Get().ShowGlobalSettings,
		NAME_None,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FIKRetargetEditorStyle::Get().GetStyleSetName(),"IKRetarget.GlobalSettings"));

		ToolbarBuilder.AddToolBarButton(
		FIKRetargetCommands::Get().ShowRootSettings,
		NAME_None,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FIKRetargetEditorStyle::Get().GetStyleSetName(),"IKRetarget.RootSettings"));

		ToolbarBuilder.AddToolBarButton(
		FIKRetargetCommands::Get().ShowPostSettings,
		NAME_None,
		TAttribute<FText>(),
		TAttribute<FText>(),
		FSlateIcon(FIKRetargetEditorStyle::Get().GetStyleSetName(),"IKRetarget.PostSettings"));
	}
	ToolbarBuilder.EndSection();
}

TSharedRef<SWidget> FIKRetargetEditor::GenerateRetargetModesMenu()
{
	FMenuBuilder MenuBuilder(true, GetToolkitCommands());
	
	MenuBuilder.BeginSection(TEXT("Retarget Modes"));
	MenuBuilder.AddMenuEntry(FIKRetargetCommands::Get().RunRetargeter, TEXT("Run Retargeter"), TAttribute<FText>(), TAttribute<FText>(),  EditorController->GetRetargeterModeIcon(ERetargeterOutputMode::RunRetarget));
	MenuBuilder.AddMenuEntry(FIKRetargetCommands::Get().EditRetargetPose, TEXT("Edit Retarget Pose"), TAttribute<FText>(), TAttribute<FText>(), EditorController->GetRetargeterModeIcon(ERetargeterOutputMode::EditRetargetPose));
	MenuBuilder.EndSection();
	
	return MenuBuilder.MakeWidget();
}

TSharedPtr<SHorizontalBox> FIKRetargetEditor::GenerateRetargetPhaseButtons() const
{
	constexpr float PhaseButtonMargin = 2.f;
	
	TSharedPtr<SHorizontalBox> Box = SNew(SHorizontalBox)
	+ SHorizontalBox::Slot()
	.VAlign(VAlign_Center)
	.HAlign(HAlign_Center)
	.AutoWidth()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		.HAlign(HAlign_Center)
		[
			SNew(STextBlock).Text(FText::FromString("Retarget Phases: "))
		]
	]
	+ SHorizontalBox::Slot()
	.AutoWidth()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(PhaseButtonMargin))
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
			.IsChecked_Lambda([this]()
			{
				const bool bIsOn =  EditorController->GetGlobalSettings().bEnableRoot;
				return bIsOn ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this](ECheckBoxState InCheckBoxState)
			{
				FRetargetGlobalSettings& GlobalSettings = EditorController->GetGlobalSettings();
				GlobalSettings.bEnableRoot = !GlobalSettings.bEnableRoot;
			})
			[
				SNew(STextBlock).Text(FText::FromString("Root"))
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(PhaseButtonMargin))
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
			.IsChecked_Lambda([this]()
			{
				const bool bIsOn =  EditorController->GetGlobalSettings().bEnableFK;
				return bIsOn ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this](ECheckBoxState InCheckBoxState)
			{
				FRetargetGlobalSettings& GlobalSettings = EditorController->GetGlobalSettings();
				GlobalSettings.bEnableFK = !GlobalSettings.bEnableFK;
			})
			[
				SNew(STextBlock).Text(FText::FromString("FK"))
			]
		]
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(PhaseButtonMargin))
		[

			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
			.IsChecked_Lambda([this]()
			{
				const bool bIsOn =  EditorController->GetGlobalSettings().bEnableIK;
				return bIsOn ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this](ECheckBoxState InCheckBoxState)
			{
				FRetargetGlobalSettings& GlobalSettings = EditorController->GetGlobalSettings();
				GlobalSettings.bEnableIK = !GlobalSettings.bEnableIK;
			})
			[
				SNew(STextBlock).Text(FText::FromString("IK"))
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(FMargin(PhaseButtonMargin))
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
			.IsChecked_Lambda([this]()
			{
				const bool bIsOn =  EditorController->GetGlobalSettings().bEnablePost;
				return bIsOn ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this](ECheckBoxState InCheckBoxState)
			{
				FRetargetGlobalSettings& GlobalSettings = EditorController->GetGlobalSettings();
				GlobalSettings.bEnablePost = !GlobalSettings.bEnablePost;
			})
			[
				SNew(STextBlock).Text(FText::FromString("Post"))
			]
		]
	];
	
	return Box;
}

FName FIKRetargetEditor::GetToolkitFName() const
{
	return FName("IKRetargetEditor");
}

FText FIKRetargetEditor::GetBaseToolkitName() const
{
	return LOCTEXT("IKRetargetEditorAppLabel", "IK Retarget Editor");
}

FText FIKRetargetEditor::GetToolkitName() const
{
	return FText::FromString(EditorController->AssetController->GetAsset()->GetName());
}

FLinearColor FIKRetargetEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

FString FIKRetargetEditor::GetWorldCentricTabPrefix() const
{
	return TEXT("IKRetargetEditor");
}

void FIKRetargetEditor::Tick(float DeltaTime)
{
	// update with latest offsets
	EditorController->AddOffsetToMeshComponent(FVector::ZeroVector, EditorController->SourceSkelMeshComponent);
	EditorController->AddOffsetToMeshComponent(FVector::ZeroVector, EditorController->TargetSkelMeshComponent);

	// retargeter IK planting must be reset when time is reversed or playback jumps ahead 
	const float CurrentTime = EditorController->SourceAnimInstance->GetCurrentTime();
	constexpr float MaxSkipTimeBeforeReset = 0.25f;
	if (CurrentTime < PreviousTime || CurrentTime > PreviousTime + MaxSkipTimeBeforeReset)
	{
		EditorController->ResetIKPlantingState();
	}
	PreviousTime = CurrentTime;
	
	// forces viewport to always update, even when mouse pressed down in other tabs
	GetPersonaToolkit()->GetPreviewScene()->InvalidateViews();
}

TStatId FIKRetargetEditor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FIKRetargetEditor, STATGROUP_Tickables);
}

void FIKRetargetEditor::PostUndo(bool bSuccess)
{
	EditorController->AssetController->CleanAsset();
	EditorController->HandlePreviewMeshReplaced(ERetargetSourceOrTarget::Source);
	EditorController->HandleRetargeterNeedsInitialized();
}

void FIKRetargetEditor::PostRedo(bool bSuccess)
{
	EditorController->AssetController->CleanAsset();
	EditorController->HandlePreviewMeshReplaced(ERetargetSourceOrTarget::Source);
	EditorController->HandleRetargeterNeedsInitialized();
}

void FIKRetargetEditor::HandleViewportCreated(const TSharedRef<class IPersonaViewport>& InViewport)
{
	// register callbacks to allow the asset to store the Bone Size viewport setting
	FEditorViewportClient& ViewportClient = InViewport->GetViewportClient();
	if (FAnimationViewportClient* AnimViewportClient = static_cast<FAnimationViewportClient*>(&ViewportClient))
	{
		AnimViewportClient->OnSetBoneSize.BindLambda([this](float InBoneSize)
		{
			if (UIKRetargeter* Asset = EditorController->AssetController->GetAsset())
			{
				Asset->Modify();
				Asset->BoneDrawSize = InBoneSize;
			}
		});
		
		AnimViewportClient->OnGetBoneSize.BindLambda([this]() -> float
		{
			if (const UIKRetargeter* Asset = EditorController->AssetController->GetAsset())
			{
				return Asset->BoneDrawSize;
			}

			return 1.0f;
		});
	}

	auto GetBorderColorAndOpacity = [this]()
	{
		// no processor or processor not initialized
		const UIKRetargetProcessor* Processor = EditorController.Get().GetRetargetProcessor();
		if (!Processor || !Processor->IsInitialized() )
		{
			return FLinearColor::Red;
		}

		const ERetargeterOutputMode OutputMode = EditorController.Get().GetRetargeterMode();
		if (OutputMode == ERetargeterOutputMode::RunRetarget)
		{
			return FLinearColor::Transparent;
		}

		return FLinearColor::Blue;
	};
	
	InViewport->AddOverlayWidget(
		SNew(SBorder)
		.BorderImage(FIKRetargetEditorStyle::Get().GetBrush( "IKRetarget.Viewport.Border"))
		.BorderBackgroundColor_Lambda(GetBorderColorAndOpacity)
		.Visibility(EVisibility::HitTestInvisible)
		.Padding(0.0f)
		.ShowEffectWhenDisabled(false)
	);
}

void FIKRetargetEditor::HandlePreviewSceneCreated(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene)
{	
	AAnimationEditorPreviewActor* Actor = InPersonaPreviewScene->GetWorld()->SpawnActor<AAnimationEditorPreviewActor>(AAnimationEditorPreviewActor::StaticClass(), FTransform::Identity);
	Actor->SetFlags(RF_Transient);
	InPersonaPreviewScene->SetActor(Actor);
	
	// create the skeletal mesh components
	EditorController->SourceSkelMeshComponent = NewObject<UDebugSkelMeshComponent>(Actor);
	EditorController->TargetSkelMeshComponent = NewObject<UDebugSkelMeshComponent>(Actor);

	// do not process root motion, we need all motion in world space for retargeting to work correctly
	EditorController->SourceSkelMeshComponent->SetProcessRootMotionMode(EProcessRootMotionMode::Ignore);
	EditorController->TargetSkelMeshComponent->SetProcessRootMotionMode(EProcessRootMotionMode::Ignore);

	// hide skeletons, we want to do custom rendering
	EditorController->SourceSkelMeshComponent->SkeletonDrawMode = ESkeletonDrawMode::Hidden;
	EditorController->TargetSkelMeshComponent->SkeletonDrawMode = ESkeletonDrawMode::Hidden;

	// don't want selectable meshes as it gets in the way of bone selection
	EditorController->SourceSkelMeshComponent->bSelectable = false;
	EditorController->TargetSkelMeshComponent->bSelectable = false;
	
	// setup and apply an anim instance to the skeletal mesh component
	EditorController->SourceAnimInstance = NewObject<UIKRetargetAnimInstance>(EditorController->SourceSkelMeshComponent, TEXT("IKRetargetSourceAnimScriptInstance"));
	EditorController->TargetAnimInstance = NewObject<UIKRetargetAnimInstance>(EditorController->TargetSkelMeshComponent, TEXT("IKRetargetTargetAnimScriptInstance"));
	SetupAnimInstance();
	
	// set components to use custom animation mode
	EditorController->SourceSkelMeshComponent->SetAnimationMode(EAnimationMode::AnimationCustomMode);
	EditorController->TargetSkelMeshComponent->SetAnimationMode(EAnimationMode::AnimationCustomMode);
	
	// must call AddComponent() BEFORE assigning the mesh to prevent auto-assignment of a default anim instance
	InPersonaPreviewScene->AddComponent(EditorController->SourceSkelMeshComponent, FTransform::Identity);
    InPersonaPreviewScene->AddComponent(EditorController->TargetSkelMeshComponent, FTransform::Identity);
    
    // apply component to the preview scene (must be done BEFORE setting mesh)
    InPersonaPreviewScene->SetPreviewMeshComponent(EditorController->SourceSkelMeshComponent);
    InPersonaPreviewScene->SetAdditionalMeshesSelectable(false);
    
    // assign source mesh to the preview scene (applies the mesh to the source component)
	// (must be done AFTER adding the component to prevent InitAnim() from overriding anim instance)
    USkeletalMesh* SourceMesh = EditorController->GetSkeletalMesh(ERetargetSourceOrTarget::Source);
    InPersonaPreviewScene->SetPreviewMesh(SourceMesh);
    
    // assign target mesh directly to the target component
    USkeletalMesh* TargetMesh = EditorController->GetSkeletalMesh(ERetargetSourceOrTarget::Target);
    EditorController->TargetSkelMeshComponent->SetSkeletalMesh(TargetMesh);
	
	EditorController->FixZeroHeightRetargetRoot(ERetargetSourceOrTarget::Source);
	EditorController->FixZeroHeightRetargetRoot(ERetargetSourceOrTarget::Target);

	// bind a callback to update the UI whenever the retarget processor in the target anim instance is initialized
	EditorController->RetargeterInitializedDelegateHandle = EditorController->GetRetargetProcessor()->OnRetargeterInitialized().AddLambda([this]()
	{
		EditorController->RefreshHierarchyView();
	});
}

void FIKRetargetEditor::SetupAnimInstance()
{
	UIKRetargeter* Asset = EditorController->AssetController->GetAsset();
	
	// configure SOURCE anim instance (will only output retarget pose)
	EditorController->SourceAnimInstance->ConfigureAnimInstance(ERetargetSourceOrTarget::Source, Asset, nullptr);
	// configure TARGET anim instance (will output retarget pose AND retarget pose from source skel mesh component)
	EditorController->TargetAnimInstance->ConfigureAnimInstance(ERetargetSourceOrTarget::Target, Asset, EditorController->SourceSkelMeshComponent);

	EditorController->SourceSkelMeshComponent->PreviewInstance = EditorController->SourceAnimInstance.Get();
	EditorController->TargetSkelMeshComponent->PreviewInstance = EditorController->TargetAnimInstance.Get();
}

void FIKRetargetEditor::HandleOnPreviewSceneSettingsCustomized(IDetailLayoutBuilder& DetailBuilder) const
{
	DetailBuilder.HideCategory("Additional Meshes");
	DetailBuilder.HideCategory("Physics");
	DetailBuilder.HideCategory("Mesh");
	DetailBuilder.HideCategory("Animation Blueprint");
}

void FIKRetargetEditor::HandleDetailsCreated(const TSharedRef<class IDetailsView>& InDetailsView)
{
	InDetailsView->OnFinishedChangingProperties().AddSP(this, &FIKRetargetEditor::OnFinishedChangingDetails);
	InDetailsView->SetObject(EditorController->AssetController->GetAsset());
	EditorController->SetDetailsView(InDetailsView);
}

void FIKRetargetEditor::OnFinishedChangingDetails(const FPropertyChangedEvent& PropertyChangedEvent)
{
	UIKRetargeterController* AssetController = EditorController->AssetController;

	// determine which properties were modified
	const bool bSourceIKRigChanged = PropertyChangedEvent.GetPropertyName() == UIKRetargeter::GetSourceIKRigPropertyName();
	const bool bTargetIKRigChanged = PropertyChangedEvent.GetPropertyName() == UIKRetargeter::GetTargetIKRigPropertyName();
	const bool bSourcePreviewChanged = PropertyChangedEvent.GetPropertyName() == UIKRetargeter::GetSourcePreviewMeshPropertyName();
	const bool bTargetPreviewChanged = PropertyChangedEvent.GetPropertyName() == UIKRetargeter::GetTargetPreviewMeshPropertyName();

	// if no override target mesh has been specified, update the override to reflect the mesh in the ik rig asset
	if (bTargetIKRigChanged)
	{
		UIKRigDefinition* NewIKRig = AssetController->GetIKRigWriteable(ERetargetSourceOrTarget::Target);
		AssetController->SetIKRig(ERetargetSourceOrTarget::Target, NewIKRig);
	}

	// if no override source mesh has been specified, update the override to reflect the mesh in the ik rig asset
	if (bSourceIKRigChanged)
	{
		UIKRigDefinition* NewIKRig = AssetController->GetIKRigWriteable(ERetargetSourceOrTarget::Source);
		AssetController->SetIKRig(ERetargetSourceOrTarget::Source, NewIKRig);
	}

	// if either the source or target meshes are possibly modified, update scene components, anim instance and UI
	if (bSourcePreviewChanged)
	{
		USkeletalMesh* Mesh = AssetController->GetPreviewMesh(ERetargetSourceOrTarget::Source);
		AssetController->SetPreviewMesh(ERetargetSourceOrTarget::Source, Mesh);
	}
	
	if (bTargetPreviewChanged)
	{
		USkeletalMesh* Mesh = AssetController->GetPreviewMesh(ERetargetSourceOrTarget::Target);
		AssetController->SetPreviewMesh(ERetargetSourceOrTarget::Target, Mesh);
	}
}

#undef LOCTEXT_NAMESPACE
