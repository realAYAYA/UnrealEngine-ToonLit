// Copyright Epic Games, Inc. All Rights Reserved.

#include "PhysicsControlProfileEditorToolkit.h"

#include "AnimationEditorPreviewActor.h"
#include "EditorModeManager.h"
#include "IDetailsView.h"
#include "IPersonaToolkit.h"
#include "Modules/ModuleManager.h"
#include "PersonaModule.h"
#include "PhysicsControlProfileApplicationMode.h"
#include "PhysicsControlProfileAsset.h"
#include "PhysicsControlProfileEditorMode.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimPreviewInstance.h"
#include "PhysicsControlProfileEditorData.h"

#define LOCTEXT_NAMESPACE "PhysicsControlProfileEditorToolkit"

const FName PhysicsControlProfileEditorModes::Editor("PhysicsControlProfileEditorMode");
const FName PhysicsControlProfileEditorAppName = FName(TEXT("PhysicsControlProfileEditorApp"));

//======================================================================================================================
void FPhysicsControlProfileEditorToolkit::InitAssetEditor(
	const EToolkitMode::Type        Mode,
	const TSharedPtr<IToolkitHost>& InitToolkitHost,
	UPhysicsControlProfileAsset*    InPhysicsControlProfileAsset)
{
	bIsInitialized = false;

	// Initialise EditorData
	{
		EditorData = MakeShared<FPhysicsControlProfileEditorData>();
		EditorData->PhysicsControlProfileAsset = InPhysicsControlProfileAsset;
		EditorData->CachePreviewMesh();
	}

	// Create Persona toolkit
	{
		FPersonaToolkitArgs PersonaToolkitArgs;
		PersonaToolkitArgs.OnPreviewSceneCreated = FOnPreviewSceneCreated::FDelegate::CreateSP(
			this, &FPhysicsControlProfileEditorToolkit::HandlePreviewSceneCreated);
		FPersonaModule& PersonaModule = FModuleManager::LoadModuleChecked<FPersonaModule>("Persona");
		PersonaToolkit = PersonaModule.CreatePersonaToolkit(InPhysicsControlProfileAsset, PersonaToolkitArgs);
		PersonaModule.RecordAssetOpened(FAssetData(InPhysicsControlProfileAsset));
	}

	// Note - we might want to make a custom skeleton tree view here, based on showing either the
	// animation or the physics bones/skeleton. See FPhysicsAssetEditor::InitPhysicsAssetEditor

	GEditor->RegisterForUndo(this);

	// Initialise the asset editor
	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(
		Mode,
		InitToolkitHost,
		PhysicsControlProfileEditorAppName,
		FTabManager::FLayout::NullLayout,
		bCreateDefaultStandaloneMenu,
		bCreateDefaultToolbar,
		InPhysicsControlProfileAsset);

	// Create and set the application mode.
	ApplicationMode = new FPhysicsControlProfileApplicationMode(SharedThis(this), PersonaToolkit->GetPreviewScene());
	AddApplicationMode(PhysicsControlProfileEditorModes::Editor, MakeShareable(ApplicationMode));
	SetCurrentMode(PhysicsControlProfileEditorModes::Editor);

	// Activate the editor mode.
	GetEditorModeManager().SetDefaultMode(FPhysicsControlProfileEditorMode::ModeName);
	GetEditorModeManager().ActivateMode(FPhysicsControlProfileEditorMode::ModeName);

	FPhysicsControlProfileEditorMode* EditorMode = GetEditorModeManager().
		GetActiveModeTyped<FPhysicsControlProfileEditorMode>(FPhysicsControlProfileEditorMode::ModeName);
	EditorMode->SetEditorToolkit(this);

	bIsInitialized = true;
}


//======================================================================================================================
void FPhysicsControlProfileEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_MLDeformerEditor", "ML Deformer Editor"));
	auto WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);
}

//======================================================================================================================
void FPhysicsControlProfileEditorToolkit::UnregisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);
}

//======================================================================================================================
FName FPhysicsControlProfileEditorToolkit::GetToolkitFName() const
{
	return FName("PhysicsControlProfileEditor");
}

//======================================================================================================================
FText FPhysicsControlProfileEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("PhysicsControlProfilekEditorAppLabel", "Physics Control Profile Editor");
}

//======================================================================================================================
FText FPhysicsControlProfileEditorToolkit::GetToolkitName() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("AssetName"), FText::FromString(EditorData->PhysicsControlProfileAsset->GetName()));
	return FText::Format(LOCTEXT("PhysicsControlProfileEditorToolkitName", "{AssetName}"), Args);
}

//======================================================================================================================
FLinearColor FPhysicsControlProfileEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor::White;
}

//======================================================================================================================
FString FPhysicsControlProfileEditorToolkit::GetWorldCentricTabPrefix() const
{
	return TEXT("PhysicsControlProfileEditor");
}

//======================================================================================================================
void FPhysicsControlProfileEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(EditorData->PhysicsControlProfileAsset);
}

//======================================================================================================================
TStatId FPhysicsControlProfileEditorToolkit::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FPhysicsControlProfileEditorToolkit, STATGROUP_Tickables);
}

//======================================================================================================================
TSharedRef<IPersonaToolkit> FPhysicsControlProfileEditorToolkit::GetPersonaToolkit() const
{
	return PersonaToolkit.ToSharedRef();
}

//======================================================================================================================
// For inspiration here, see:
// - PhysicsAssetEditor
// - MLDeformerEditorToolkit
// - IKRigToolkit
// though note that they all do things differently!
void FPhysicsControlProfileEditorToolkit::HandlePreviewSceneCreated(
	const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene)
{
	EditorData->PreviewScene = InPersonaPreviewScene;

	AAnimationEditorPreviewActor* Actor = InPersonaPreviewScene->GetWorld()->SpawnActor<AAnimationEditorPreviewActor>(
		AAnimationEditorPreviewActor::StaticClass(), FTransform::Identity);
	Actor->SetFlags(RF_Transient);
	InPersonaPreviewScene->SetActor(Actor);

	// Create the preview skeletal mesh component
	ViewportSkeletalMeshComponent = NewObject<UDebugSkelMeshComponent>(Actor);
	// turn off default bone rendering
	//ViewportSkeletalMeshComponent->SkeletonDrawMode = ESkeletonDrawMode::Hidden;

	// Setup and apply an anim instance to the skeletal mesh component
	ViewportAnimInstance = NewObject<UAnimPreviewInstance>(
		ViewportSkeletalMeshComponent, TEXT("PhysicsControlProfileEditorAnimInstance"));
	ViewportSkeletalMeshComponent->PreviewInstance = ViewportAnimInstance;
	//ViewportAnimInstance->InitializeAnimation();

	// Set the skeletal mesh on the component, using the asset. Note that this will change if/when
	// the asset doesn't hold a mesh.
	USkeletalMesh* Mesh = EditorData->PhysicsControlProfileAsset->PreviewSkeletalMesh.Get();
	ViewportSkeletalMeshComponent->SetSkeletalMesh(Mesh);

	// apply mesh to the preview scene
	InPersonaPreviewScene->SetPreviewMeshComponent(ViewportSkeletalMeshComponent);
	InPersonaPreviewScene->SetAllowMeshHitProxies(false);
	InPersonaPreviewScene->SetAdditionalMeshesSelectable(false);
	ViewportSkeletalMeshComponent->bSelectable = false;
	InPersonaPreviewScene->SetPreviewMesh(Mesh);
	InPersonaPreviewScene->AddComponent(ViewportSkeletalMeshComponent, FTransform::Identity);
}

//======================================================================================================================
void FPhysicsControlProfileEditorToolkit::HandleViewportCreated(const TSharedRef<IPersonaViewport>& InPersonaViewport)
{
	PersonaViewport = InPersonaViewport;
}

//======================================================================================================================
void FPhysicsControlProfileEditorToolkit::ShowEmptyDetails() const
{
	DetailsView->SetObject(EditorData->PhysicsControlProfileAsset);
}

//======================================================================================================================
void FPhysicsControlProfileEditorToolkit::HandleDetailsCreated(const TSharedRef<class IDetailsView>& InDetailsView)
{
	DetailsView = InDetailsView;
	DetailsView->OnFinishedChangingProperties().AddSP(
		this, &FPhysicsControlProfileEditorToolkit::OnFinishedChangingDetails);
	ShowEmptyDetails();
}

//======================================================================================================================
void FPhysicsControlProfileEditorToolkit::OnFinishedChangingDetails(const FPropertyChangedEvent& PropertyChangedEvent)
{
	const bool bPreviewMeshChanged = PropertyChangedEvent.GetPropertyName() == UPhysicsControlProfileAsset::GetPreviewMeshPropertyName();
	if (bPreviewMeshChanged)
	{
		USkeletalMesh* Mesh = EditorData->PhysicsControlProfileAsset->PreviewSkeletalMesh.LoadSynchronous();
		ViewportSkeletalMeshComponent->SetSkeletalMesh(Mesh);
		EditorData->CachePreviewMesh();
	}
}

#undef LOCTEXT_NAMESPACE

