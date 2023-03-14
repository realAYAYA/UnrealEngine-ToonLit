// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigEditor/IKRigToolkit.h"

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
#include "Animation/DebugSkelMeshComponent.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#include "IKRigDefinition.h"
#include "IPersonaViewport.h"
#include "PersonaPreviewSceneDescription.h"
#include "RigEditor/IKRigAnimInstance.h"
#include "RigEditor/IKRigCommands.h"
#include "RigEditor/IKRigEditMode.h"
#include "RigEditor/IKRigEditorController.h"
#include "RigEditor/IKRigMode.h"

#define LOCTEXT_NAMESPACE "IKRigEditorToolkit"

const FName IKRigEditorModes::IKRigEditorMode("IKRigEditorMode");
const FName IKRigEditorAppName = FName(TEXT("IKRigEditorApp"));

FIKRigEditorToolkit::FIKRigEditorToolkit()
	: EditorController(MakeShared<FIKRigEditorController>())
{
}

FIKRigEditorToolkit::~FIKRigEditorToolkit()
{
	if (PersonaToolkit.IsValid())
	{
		static constexpr bool bSetPreviewMeshInAsset = false;
		PersonaToolkit->SetPreviewMesh(nullptr, bSetPreviewMeshInAsset);
	}
}

void FIKRigEditorToolkit::InitAssetEditor(
	const EToolkitMode::Type Mode,
    const TSharedPtr<IToolkitHost>& InitToolkitHost, 
    UIKRigDefinition* IKRigAsset)
{
	EditorController->Initialize(SharedThis(this), IKRigAsset);

	BindCommands();
	
	FPersonaToolkitArgs PersonaToolkitArgs;
	PersonaToolkitArgs.OnPreviewSceneCreated = FOnPreviewSceneCreated::FDelegate::CreateSP(this, &FIKRigEditorToolkit::HandlePreviewSceneCreated);
	PersonaToolkitArgs.OnPreviewSceneSettingsCustomized = FOnPreviewSceneSettingsCustomized::FDelegate::CreateSP(this, &FIKRigEditorToolkit::HandleOnPreviewSceneSettingsCustomized);
	
	const FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	PersonaToolkit = PersonaModule.CreatePersonaToolkit(IKRigAsset, PersonaToolkitArgs);

	const TSharedRef<IAssetFamily> AssetFamily = PersonaModule.CreatePersonaAssetFamily(IKRigAsset);
	AssetFamily->RecordAssetOpened(FAssetData(IKRigAsset));

	static constexpr bool bCreateDefaultStandaloneMenu = true;
	static constexpr bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(
		Mode, 
		InitToolkitHost, 
		IKRigEditorAppName, 
		FTabManager::FLayout::NullLayout, 
		bCreateDefaultStandaloneMenu, 
		bCreateDefaultToolbar, 
		IKRigAsset);

	AddApplicationMode(
		IKRigEditorModes::IKRigEditorMode,
		MakeShareable(new FIKRigMode(SharedThis(this), PersonaToolkit->GetPreviewScene())));

	SetCurrentMode(IKRigEditorModes::IKRigEditorMode);

	GetEditorModeManager().SetDefaultMode(FIKRigEditMode::ModeName);
	GetEditorModeManager().ActivateMode(FIKRigEditMode::ModeName);
	static_cast<FIKRigEditMode*>(GetEditorModeManager().GetActiveMode(FIKRigEditMode::ModeName))->SetEditorController(EditorController);

	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

void FIKRigEditorToolkit::OnClose()
{
	FPersonaAssetEditorToolkit::OnClose();
	EditorController->Close();
}

void FIKRigEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_IKRigEditor", "IK Rig Editor"));
	TSharedRef<FWorkspaceItem> WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}

void FIKRigEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

void FIKRigEditorToolkit::BindCommands()
{
	const FIKRigCommands& Commands = FIKRigCommands::Get();

	ToolkitCommands->MapAction(
        Commands.Reset,
        FExecuteAction::CreateSP(EditorController, &FIKRigEditorController::Reset),
		EUIActionRepeatMode::RepeatDisabled);
}

void FIKRigEditorToolkit::ExtendToolbar()
{
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	AddToolbarExtender(ToolbarExtender);

	ToolbarExtender->AddToolBarExtension(
        "Asset",
        EExtensionHook::After,
        GetToolkitCommands(),
        FToolBarExtensionDelegate::CreateSP(this, &FIKRigEditorToolkit::FillToolbar)
    );
}

void FIKRigEditorToolkit::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("Reset");
	{
		ToolbarBuilder.AddToolBarButton(
			FIKRigCommands::Get().Reset,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::Get().GetStyleSetName(),"Icons.Refresh"));
	}
	ToolbarBuilder.EndSection();
}

FName FIKRigEditorToolkit::GetToolkitFName() const
{
	return FName("IKRigEditor");
}

FText FIKRigEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("IKRigEditorAppLabel", "IK Rig Editor");
}

FText FIKRigEditorToolkit::GetToolkitName() const
{
	return FText::FromString(EditorController->AssetController->GetAsset()->GetName());
}

FLinearColor FIKRigEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

FString FIKRigEditorToolkit::GetWorldCentricTabPrefix() const
{
	return TEXT("IKRigEditor");
}

void FIKRigEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	// hold the asset we are working on
	const UIKRigDefinition* Asset = EditorController->AssetController->GetAsset();
	Collector.AddReferencedObject(Asset);
}

TStatId FIKRigEditorToolkit::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FIKRigEditorToolkit, STATGROUP_Tickables);
}

void FIKRigEditorToolkit::HandleOnPreviewSceneSettingsCustomized(IDetailLayoutBuilder& DetailBuilder) const
{
	DetailBuilder.HideCategory("Additional Meshes");
	DetailBuilder.HideCategory("Physics");
	DetailBuilder.HideCategory("Mesh");
	DetailBuilder.HideCategory("Animation Blueprint");
}

void FIKRigEditorToolkit::PostUndo(bool bSuccess)
{
	EditorController->ClearOutputLog();
	EditorController->AssetController->BroadcastNeedsReinitialized();
	EditorController->RefreshAllViews();
}

void FIKRigEditorToolkit::PostRedo(bool bSuccess)
{
	EditorController->ClearOutputLog();
	EditorController->AssetController->BroadcastNeedsReinitialized();
	EditorController->RefreshAllViews();
}

void FIKRigEditorToolkit::HandlePreviewSceneCreated(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene)
{
	AAnimationEditorPreviewActor* Actor = InPersonaPreviewScene->GetWorld()->SpawnActor<AAnimationEditorPreviewActor>(AAnimationEditorPreviewActor::StaticClass(), FTransform::Identity);
	Actor->SetFlags(RF_Transient);
	InPersonaPreviewScene->SetActor(Actor);
	
	// create the preview skeletal mesh component
	EditorController->SkelMeshComponent = NewObject<UDebugSkelMeshComponent>(Actor);
	// turn off default bone rendering (we do our own in the IK Rig editor)
	EditorController->SkelMeshComponent->SkeletonDrawMode = ESkeletonDrawMode::Hidden;

	// setup an apply an anim instance to the skeletal mesh component
	UIKRigAnimInstance* AnimInstance = NewObject<UIKRigAnimInstance>(EditorController->SkelMeshComponent, TEXT("IKRigAnimScriptInstance"));
	EditorController->AnimInstance = AnimInstance;
	AnimInstance->SetIKRigAsset(EditorController->AssetController->GetAsset());
	EditorController->SkelMeshComponent->PreviewInstance = AnimInstance;
	EditorController->OnIKRigNeedsInitialized(EditorController->AssetController->GetAsset());
	AnimInstance->InitializeAnimation();

	// set the skeletal mesh on the component
	// NOTE: this must be done AFTER setting the AnimInstance so that the correct root anim node is loaded
	USkeletalMesh* Mesh = EditorController->AssetController->GetAsset()->GetPreviewMesh();
	EditorController->SkelMeshComponent->SetSkeletalMesh(Mesh);

	// apply mesh to the preview scene
	InPersonaPreviewScene->SetPreviewMeshComponent(EditorController->SkelMeshComponent);
	InPersonaPreviewScene->SetAllowMeshHitProxies(false);
	InPersonaPreviewScene->SetAdditionalMeshesSelectable(false);
	EditorController->SkelMeshComponent->bSelectable = false;
	InPersonaPreviewScene->SetPreviewMesh(Mesh);
	InPersonaPreviewScene->AddComponent(EditorController->SkelMeshComponent, FTransform::Identity);
}

void FIKRigEditorToolkit::HandleDetailsCreated(const TSharedRef<class IDetailsView>& InDetailsView) const
{
	EditorController->SetDetailsView(InDetailsView);
}

void FIKRigEditorToolkit::HandleViewportCreated(const TSharedRef<IPersonaViewport>& InViewport)
{
	// register callbacks to allow the asset to store the Bone Size viewport setting
	FEditorViewportClient& ViewportClient = InViewport->GetViewportClient();
	if (FAnimationViewportClient* AnimViewportClient = static_cast<FAnimationViewportClient*>(&ViewportClient))
	{
		AnimViewportClient->OnSetBoneSize.BindLambda([this](float InBoneSize)
		{
			if (UIKRigDefinition* Asset = EditorController->AssetController->GetAsset())
			{
				Asset->Modify();
				Asset->BoneSize = InBoneSize;
			}
		});
		
		AnimViewportClient->OnGetBoneSize.BindLambda([this]() -> float
		{
			if (const UIKRigDefinition* Asset = EditorController->AssetController->GetAsset())
			{
				return Asset->BoneSize;
			}

			return 1.0f;
		});
	}
}

#undef LOCTEXT_NAMESPACE
