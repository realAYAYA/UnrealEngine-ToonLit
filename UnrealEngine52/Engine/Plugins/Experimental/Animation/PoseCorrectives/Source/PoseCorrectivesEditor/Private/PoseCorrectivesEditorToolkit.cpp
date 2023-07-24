// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseCorrectivesEditorToolkit.h"

#include "PoseCorrectivesAnimInstance.h"
#include "PoseCorrectivesAsset.h"
#include "PoseCorrectivesCommands.h"
#include "PoseCorrectivesEditorController.h"

#include "AnimPreviewInstance.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimationEditorPreviewActor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailsView.h"
#include "IPersonaToolkit.h"
#include "PersonaModule.h"


#define LOCTEXT_NAMESPACE "PoseCorrectivesEditor"

const FName PoseCorrectivesEditorModes::PoseCorrectivesEditorMode("PoseCorrectivesEditorMode");
const FName PoseCorrectivesEditorAppName = FName(TEXT("PoseCorrectivesEditorApp"));

FPoseCorrectivesEditorToolkit::FPoseCorrectivesEditorToolkit()
	: EditorController(MakeShared<FPoseCorrectivesEditorController>())
{
}

FPoseCorrectivesEditorToolkit::~FPoseCorrectivesEditorToolkit()
{
}

void FPoseCorrectivesEditorToolkit::InitAssetEditor(
	const EToolkitMode::Type Mode,
    const TSharedPtr<IToolkitHost>& InitToolkitHost, 
    UPoseCorrectivesAsset* InAsset)
{
	EditorController->Initialize(SharedThis(this), InAsset);

	BindCommands();

	FPersonaToolkitArgs PersonaToolkitArgs;
	PersonaToolkitArgs.OnPreviewSceneCreated = FOnPreviewSceneCreated::FDelegate::CreateSP(this, &FPoseCorrectivesEditorToolkit::HandlePreviewSceneCreated);
	
	FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
	EditorController->PersonaToolkit = PersonaModule.CreatePersonaToolkit(InAsset, PersonaToolkitArgs);
	
	PersonaModule.RecordAssetOpened(FAssetData(InAsset));

	constexpr bool bCreateDefaultStandaloneMenu = true;
	constexpr bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(
		Mode, 
		InitToolkitHost, 
		PoseCorrectivesEditorAppName, 
		FTabManager::FLayout::NullLayout, 
		bCreateDefaultStandaloneMenu, 
		bCreateDefaultToolbar, 
		InAsset);


	AddApplicationMode(
		PoseCorrectivesEditorModes::PoseCorrectivesEditorMode,
		MakeShareable(new FPoseCorrectivesMode(SharedThis(this), EditorController->PersonaToolkit->GetPreviewScene())));


	SetCurrentMode(PoseCorrectivesEditorModes::PoseCorrectivesEditorMode);

	EditorController->InitializeTargetControlRigBP();

	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

void FPoseCorrectivesEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}

void FPoseCorrectivesEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

FName FPoseCorrectivesEditorToolkit::GetToolkitFName() const
{
	return FName("PoseCorrectivesEditor");
}

FText FPoseCorrectivesEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("PoseCorrectivesAppLabel", "Pose Correctives Editor");
}

FLinearColor FPoseCorrectivesEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

FString FPoseCorrectivesEditorToolkit::GetWorldCentricTabPrefix() const
{
	return TEXT("PoseCorrectivesEditor");
}


TSharedRef<IPersonaToolkit> FPoseCorrectivesEditorToolkit::GetPersonaToolkit() const
{
	return EditorController->PersonaToolkit.ToSharedRef();
}


void FPoseCorrectivesEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(EditorController->Asset);
}

FString FPoseCorrectivesEditorToolkit::GetReferencerName() const
{
	return TEXT("FPoseCorrectivesEditorToolkit");
}

void FPoseCorrectivesEditorToolkit::Tick(float DeltaTime)
{
	float Scale = EditorController->Asset->SourceMeshScale;
	FVector SourceScale = FVector(Scale,Scale,Scale);

	EditorController->SourceSkelMeshComponent->SetWorldLocation(EditorController->Asset->SourceMeshOffset, false, nullptr,  ETeleportType::ResetPhysics);
	EditorController->SourceSkelMeshComponent->SetWorldScale3D(SourceScale);	
}

TStatId FPoseCorrectivesEditorToolkit::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FPoseCorrectivesEditorToolkit, STATGROUP_Tickables);
}


void FPoseCorrectivesEditorToolkit::HandleAssetDoubleClicked(UObject* InNewAsset)
{
	UAnimationAsset* NewAnimationAsset = Cast<UAnimationAsset>(InNewAsset);
	if (NewAnimationAsset)
	{
		EditorController->PlayAnimationAsset(NewAnimationAsset);
	}
}

void FPoseCorrectivesEditorToolkit::HandleAnimationSequenceBrowserCreated(const TSharedRef<IAnimationSequenceBrowser>& InSequenceBrowser)
{

}

void FPoseCorrectivesEditorToolkit::HandlePreviewSceneCreated(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene)
{
	AAnimationEditorPreviewActor* Actor = InPersonaPreviewScene->GetWorld()->SpawnActor<AAnimationEditorPreviewActor>(AAnimationEditorPreviewActor::StaticClass(), FTransform::Identity);
	Actor->SetFlags(RF_Transient);
	InPersonaPreviewScene->SetActor(Actor);

	USkeletalMesh* SourceMesh = EditorController->GetSourceSkeletalMesh();

	UDebugSkelMeshComponent* EditorSkelComp = NewObject<UDebugSkelMeshComponent>(Actor);
	EditorSkelComp->SetSkeletalMesh(SourceMesh);
	EditorSkelComp->bSelectable = false;
	EditorSkelComp->MarkRenderStateDirty();

	Actor->SetRootComponent(EditorSkelComp);
	EditorController->SourceSkelMeshComponent = EditorSkelComp;
	EditorController->SourceSkelMeshComponent->SkeletonDrawMode = ESkeletonDrawMode::GreyedOut;
	
	InPersonaPreviewScene->SetPreviewMeshComponent(EditorSkelComp);
	InPersonaPreviewScene->SetPreviewMesh(SourceMesh);
	InPersonaPreviewScene->AddComponent(EditorSkelComp, FTransform::Identity);
	
	EditorController->TargetSkelMeshComponent = NewObject<UDebugSkelMeshComponent>(Actor);	
	EditorController->TargetSkelMeshComponent->SetSkeletalMesh(EditorController->GetTargetSkeletalMesh());
	EditorController->TargetSkelMeshComponent->bSelectable = false;
	EditorController->TargetSkelMeshComponent->SkeletonDrawMode = ESkeletonDrawMode::GreyedOut;
	EditorController->TargetSkelMeshComponent->MarkRenderStateDirty();
	
	EditorController->SourceAnimInstance = NewObject<UPoseCorrectivesAnimSourceInstance>(EditorController->SourceSkelMeshComponent, TEXT("PoseCorrectivesSourceAnimInstance"));
	EditorController->TargetAnimInstance = NewObject<UPoseCorrectivesAnimInstance>(EditorController->TargetSkelMeshComponent, TEXT("PoseCorrectivesTargetAnimInstance"));
	SetupAnimInstance();

	InPersonaPreviewScene->AddComponent(EditorController->SourceSkelMeshComponent, FTransform::Identity);
	InPersonaPreviewScene->AddComponent(EditorController->TargetSkelMeshComponent, FTransform::Identity);
}

void FPoseCorrectivesEditorToolkit::HandleDetailsCreated(const TSharedRef<class IDetailsView>& InDetailsView)
{
	EditorController->DetailsView = InDetailsView;
	EditorController->DetailsView->OnFinishedChangingProperties().AddSP(this, &FPoseCorrectivesEditorToolkit::OnFinishedChangingDetails);
	EditorController->DetailsView->SetObject(EditorController->Asset);
}

void FPoseCorrectivesEditorToolkit::OnFinishedChangingDetails(const FPropertyChangedEvent& PropertyChangedEvent)
{
	const bool bSourcePreviewChanged = PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_STRING_CHECKED(UPoseCorrectivesAsset, SourcePreviewMesh);
	const bool bTargetChanged = PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_STRING_CHECKED(UPoseCorrectivesAsset, TargetMesh);
	const bool bControlRigBPChanged = PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_STRING_CHECKED(UPoseCorrectivesAsset, ControlRigBlueprint);
	
	if (bSourcePreviewChanged)
	{
		USkeletalMesh* SourceMesh = EditorController->GetSourceSkeletalMesh();
		EditorController->SourceSkelMeshComponent->SetSkeletalMesh(SourceMesh);
			
		// apply mesh to the preview scene
		TSharedRef<IPersonaPreviewScene> PreviewScene = GetPersonaToolkit()->GetPreviewScene();
		if (PreviewScene->GetPreviewMesh() != SourceMesh)
		{
			PreviewScene->SetPreviewMeshComponent(EditorController->SourceSkelMeshComponent);
			PreviewScene->SetPreviewMesh(SourceMesh);
			PreviewScene->SetAdditionalMeshesSelectable(false);
		}

		SetupAnimInstance();
	}
	else if (bTargetChanged)
	{
		USkeletalMesh* TargetMesh = EditorController->GetTargetSkeletalMesh();
		EditorController->TargetSkelMeshComponent->SetSkeletalMesh(TargetMesh);
		EditorController->HandleTargetMeshChanged();
		SetupAnimInstance();
	}
	else if (bControlRigBPChanged)
	{
		EditorController->InitializeTargetControlRigBP();
		SetupAnimInstance();
	}
}

void FPoseCorrectivesEditorToolkit::SetupAnimInstance()
{
	EditorController->SourceAnimInstance->SetCorrectivesAsset(EditorController->Asset);
	EditorController->TargetAnimInstance->SetCorrectivesAssetAndSourceComponent(EditorController->Asset, EditorController->SourceSkelMeshComponent);

	EditorController->SourceSkelMeshComponent->PreviewInstance = EditorController->SourceAnimInstance.Get();
	EditorController->TargetSkelMeshComponent->PreviewInstance = EditorController->TargetAnimInstance.Get();

	EditorController->SourceSkelMeshComponent->EnablePreview(true, nullptr);
	EditorController->TargetSkelMeshComponent->EnablePreview(true, nullptr);

	EditorController->SourceAnimInstance->InitializeAnimation();
	EditorController->TargetAnimInstance->InitializeAnimation();
}

void FPoseCorrectivesEditorToolkit::HandleCreateCorrectiveClicked()
{
	EditorController->HandleNewCorrective();
}

void FPoseCorrectivesEditorToolkit::BindCommands()
{
	const FPoseCorrectivesCommands& Commands =  FPoseCorrectivesCommands::Get();

	ToolkitCommands->MapAction(
		Commands.AddCorrectivePose,
		FExecuteAction::CreateSP(EditorController, &FPoseCorrectivesEditorController::HandleNewCorrective),
		FIsActionButtonVisible::CreateSP(EditorController, &FPoseCorrectivesEditorController::CanAddCorrective),
		EUIActionRepeatMode::RepeatDisabled);

	ToolkitCommands->MapAction(
		Commands.SaveCorrective,
		FExecuteAction::CreateSP(EditorController, &FPoseCorrectivesEditorController::HandleStopEditPose),
		FIsActionButtonVisible::CreateSP(EditorController, &FPoseCorrectivesEditorController::IsEditingPose),
		EUIActionRepeatMode::RepeatDisabled);

	ToolkitCommands->MapAction(
		Commands.CancelCorrective,
		FExecuteAction::CreateSP(EditorController, &FPoseCorrectivesEditorController::HandleCancelEditPose),
		FIsActionButtonVisible::CreateSP(EditorController, &FPoseCorrectivesEditorController::IsEditingPose),
		EUIActionRepeatMode::RepeatDisabled);
}

void FPoseCorrectivesEditorToolkit::ExtendToolbar()
{
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	AddToolbarExtender(ToolbarExtender);

	ToolbarExtender->AddToolBarExtension(
        "Asset",
        EExtensionHook::After,
        GetToolkitCommands(),
        FToolBarExtensionDelegate::CreateSP(this, &FPoseCorrectivesEditorToolkit::FillToolbar)
    );
}

void FPoseCorrectivesEditorToolkit::FillToolbar(FToolBarBuilder& ToolbarBuilder)
{
	ToolbarBuilder.BeginSection("Edit Pose");
	{
		ToolbarBuilder.AddToolBarButton(
			FPoseCorrectivesCommands::Get().AddCorrectivePose,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(),"Icons.Plus"));

		ToolbarBuilder.AddToolBarButton(
			FPoseCorrectivesCommands::Get().SaveCorrective,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(),"GenericStop"));

		ToolbarBuilder.AddToolBarButton(
			FPoseCorrectivesCommands::Get().CancelCorrective,
			NAME_None,
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(),"GenericStop"));

	}
	ToolbarBuilder.EndSection();
}


#undef LOCTEXT_NAMESPACE
