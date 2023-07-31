// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectAssetToolkit.h"

#include "AssetEditorModeManager.h"
#include "BaseGizmos/TransformGizmoUtil.h"
#include "EdModeInteractiveToolsContext.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "SmartObjectAssetEditorViewportClient.h"
#include "SmartObjectComponent.h"
#include "Tools/UAssetEditor.h"
#include "Widgets/Docking/SDockTab.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SmartObjectAssetToolkit)

#define LOCTEXT_NAMESPACE "SmartObjectAssetToolkit"

const FName FSmartObjectAssetToolkit::PreviewSettingsTabID(TEXT("SmartObjectAssetToolkit_Preview"));

//----------------------------------------------------------------------//
// USmartObjectAssetEditorTool
//----------------------------------------------------------------------//
void USmartObjectAssetEditorTool::Initialize(UInteractiveToolsContext* InteractiveToolsContext, USmartObjectDefinition* SmartObjectDefinition, USmartObjectComponent* SmartObjectComponent)
{
	ToolsContext = InteractiveToolsContext;
	Definition = SmartObjectDefinition;
	PreviewComponent = SmartObjectComponent;

	// Register gizmo context
	UE::TransformGizmoUtil::RegisterTransformGizmoContextObject(InteractiveToolsContext);

	// Create gizmos
	CreateGizmos();
}

void USmartObjectAssetEditorTool::CreateGizmos()
{
	check(ToolsContext != nullptr);
	check(PreviewComponent != nullptr);

	UInteractiveGizmoManager* GizmoManager = ToolsContext->GizmoManager;

	for (int32 Index = 0; Index < Definition->GetSlots().Num(); ++Index)
	{
		FSmartObjectSlotEditorTarget Transformable;
		Transformable.TransformProxy = NewObject<UTransformProxy>(this);
		Transformable.TransformProxy->AddComponentCustom(PreviewComponent,
			[this, Index]()
			{
				return Definition->GetSlotTransform(FTransform::Identity, FSmartObjectSlotIndex(Index)).GetValue();
			},
			[this, Index](const FTransform NewTransform)
			{
				const TArrayView<FSmartObjectSlotDefinition> SlotDefinitions = Definition->GetMutableSlots();
				FSmartObjectSlotDefinition& SlotDefinition = SlotDefinitions[Index];
				SlotDefinition.Offset = NewTransform.GetTranslation();
				SlotDefinition.Rotation = NewTransform.Rotator();
			},
			Index,
			/*bModifyComponentOnTransform*/ false
		);

		constexpr ETransformGizmoSubElements GizmoElements = ETransformGizmoSubElements::StandardTranslateRotate;
		Transformable.TransformGizmo = UE::TransformGizmoUtil::CreateCustomRepositionableTransformGizmo(GizmoManager, GizmoElements, this);
		Transformable.TransformGizmo->SetActiveTarget(Transformable.TransformProxy, GizmoManager);
		ActiveGizmos.Add(Transformable);
	}
}

void USmartObjectAssetEditorTool::DestroyGizmos()
{
	ToolsContext->GizmoManager->DestroyAllGizmosByOwner(this);
	ActiveGizmos.Reset();
}

void USmartObjectAssetEditorTool::Cleanup()
{
	DestroyGizmos();
	UE::TransformGizmoUtil::DeregisterTransformGizmoContextObject(ToolsContext);
}

void USmartObjectAssetEditorTool::RebuildGizmos()
{
	DestroyGizmos();
	CreateGizmos();
}

void USmartObjectAssetEditorTool::RefreshGizmos()
{
	check(Definition != nullptr);
	check(ActiveGizmos.Num() == Definition->GetSlots().Num());

	for (int32 Index = 0; Index < Definition->GetSlots().Num(); ++Index)
	{
		const FTransform NewTransform = Definition->GetSlotTransform(FTransform::Identity, FSmartObjectSlotIndex(Index)).GetValue();
		ActiveGizmos[Index].TransformGizmo->ReinitializeGizmoTransform(NewTransform);
	}
}

//----------------------------------------------------------------------//
// FSmartObjectAssetToolkit
//----------------------------------------------------------------------//
FSmartObjectAssetToolkit::FSmartObjectAssetToolkit(UAssetEditor* InOwningAssetEditor)
	: FBaseAssetToolkit(InOwningAssetEditor)
	, Tool(nullptr)
{
	FPreviewScene::ConstructionValues PreviewSceneArgs;
	AdvancedPreviewScene = MakeUnique<FAdvancedPreviewScene>(PreviewSceneArgs);

	// Apply small Z offset to not hide the grid
	constexpr float DefaultFloorOffset = 1.0f;
	AdvancedPreviewScene->SetFloorOffset(DefaultFloorOffset);

	// Setup our default layout
	StandaloneDefaultLayout = FTabManager::NewLayout(FName("SmartObjectAssetEditorLayout1"))
		->AddArea
		(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.7f)
					->AddTab(ViewportTabID, ETabState::OpenedTab)
					->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.3f)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.3f)
						->AddTab(PreviewSettingsTabID, ETabState::OpenedTab)
						->SetHideTabWell(true)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.7f)
						->AddTab(DetailsTabID, ETabState::OpenedTab)
						->SetHideTabWell(true)
					)
				)
			)
		);
}

TSharedPtr<FEditorViewportClient> FSmartObjectAssetToolkit::CreateEditorViewportClient() const
{
	// Set our advanced preview scene in the EditorModeManager
	StaticCastSharedPtr<FAssetEditorModeManager>(EditorModeManager)->SetPreviewScene(AdvancedPreviewScene.Get());

	// Create and setup our custom viewport client
	SmartObjectViewportClient = MakeShared<FSmartObjectAssetEditorViewportClient>(SharedThis(this), AdvancedPreviewScene.Get());

	SmartObjectViewportClient->ViewportType = LVT_Perspective;
	SmartObjectViewportClient->SetViewLocation(EditorViewportDefs::DefaultPerspectiveViewLocation);
	SmartObjectViewportClient->SetViewRotation(EditorViewportDefs::DefaultPerspectiveViewRotation);

	return SmartObjectViewportClient;
}

void FSmartObjectAssetToolkit::PostInitAssetEditor()
{
	USmartObjectComponent* PreviewComponent = NewObject<USmartObjectComponent>(GetTransientPackage(), NAME_None, RF_Transient );
	USmartObjectDefinition* Definition = Cast<USmartObjectDefinition>(GetEditingObject());
	PreviewComponent->SetDefinition(Definition);

	// Add component to the preview scene
	if (FPreviewScene* PreviewScene = ViewportClient.Get()->GetPreviewScene())
	{
		check(AdvancedPreviewScene.Get() == PreviewScene);
		PreviewScene->AddComponent(PreviewComponent, FTransform::Identity);
	}

	// Allow the viewport client to interact with the preview component
	checkf(SmartObjectViewportClient.IsValid(), TEXT("ViewportClient is created in CreateEditorViewportClient before calling PostInitAssetEditor"));
	SmartObjectViewportClient->SetPreviewComponent(PreviewComponent);

	// Use preview information from asset
	if (const UClass* ActorClass = Definition->PreviewClass.Get())
	{
		PreviewActorClass = ActorClass;
		SmartObjectViewportClient->SetPreviewActorClass(ActorClass);
	}

	PreviewMeshObjectPath = Definition->PreviewMeshPath.GetAssetPathString();
	if (!PreviewMeshObjectPath.IsEmpty())
	{
		UStaticMesh* PreviewMesh = FindObject<UStaticMesh>(nullptr, *PreviewMeshObjectPath);
		if (PreviewMesh == nullptr)
		{
			PreviewMesh = LoadObject<UStaticMesh>(nullptr, *PreviewMeshObjectPath);
			if (PreviewMesh == nullptr)
			{
				// Path is no longer valid so clear references to it (without dirtying on load)
				PreviewMeshObjectPath.Reset();
				Definition->PreviewMeshPath.Reset();
			}
		}
		SmartObjectViewportClient->SetPreviewMesh(PreviewMesh);
	}

	// Instantiate the tool to author slot definitions through the preview component
	Tool = NewObject<USmartObjectAssetEditorTool>(GetTransientPackage(), NAME_None, RF_Transient );
	Tool->Initialize(GetEditorModeManager().GetInteractiveToolsContext(), Definition, PreviewComponent);

	// Register to be notified when properties are edited
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FSmartObjectAssetToolkit::OnPropertyChanged);
}

void FSmartObjectAssetToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FBaseAssetToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(PreviewSettingsTabID, FOnSpawnTab::CreateSP(this, &FSmartObjectAssetToolkit::SpawnTab_PreviewSettings))
		.SetDisplayName(LOCTEXT("PreviewSettings", "Preview Settings"))
		.SetGroup(AssetEditorTabsCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Visibility"));
}

TSharedRef<SDockTab> FSmartObjectAssetToolkit::SpawnTab_PreviewSettings(const FSpawnTabArgs& Args)
{
	const TSharedPtr<SDockTab> PreviewSettingsTab = SNew(SDockTab)
		.Label(LOCTEXT("PreviewSettingsTitle", "Preview Settings"))
		.ShouldAutosize(true)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(3.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PreviewActor_Title", "Select Preview Actor"))
				.ToolTipText(LOCTEXT("PreviewActor_Tooltip", "Select Actor to instantiate for previewing the definition."))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(AActor::StaticClass())
				.ObjectPath_Lambda([this]()
				{
					return PreviewActorObjectPath;
				})
				.OnObjectChanged_Lambda([this](const FAssetData& AssetData)
				{
					AActor* PreviewActor = Cast<AActor>(AssetData.GetAsset());
					PreviewActorObjectPath.Reset();
					if (PreviewActor != nullptr)
					{
						PreviewActorObjectPath = AssetData.GetObjectPathString();
					}

					SmartObjectViewportClient->SetPreviewActor(PreviewActor);
				})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(3.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PreviewMesh_Title", "Select Preview Mesh"))
				.ToolTipText(LOCTEXT("PreviewMesh_Tooltip", "Select Mesh to instantiate for previewing the definition."))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UStaticMesh::StaticClass())
				.ObjectPath_Lambda([this]()
				{
					return PreviewMeshObjectPath;
				})
				.OnObjectChanged_Lambda([this](const FAssetData& AssetData)
				{
					UStaticMesh* PreviewMesh = Cast<UStaticMesh>(AssetData.GetAsset());
					PreviewMeshObjectPath.Reset();
					if (PreviewMesh != nullptr)
					{
						PreviewMeshObjectPath = AssetData.GetObjectPathString();
					}

					FScopedTransaction Transaction(LOCTEXT("SetPreviewMesh", "Set Preview Mesh"));
					USmartObjectDefinition* Definition = CastChecked<USmartObjectDefinition>(GetEditingObject());
					Definition->Modify();
					Definition->PreviewMeshPath = PreviewMeshObjectPath;

					SmartObjectViewportClient->SetPreviewMesh(PreviewMesh);
				})
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(3.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PreviewActorClass_Title", "Select Preview Actor Class"))
				.ToolTipText(LOCTEXT("PreviewActorClass_Tooltip", "Select class of Actor to instantiate for previewing the definition."))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6.0f)
			.VAlign(VAlign_Center)
			[
				SNew(SClassPropertyEntryBox)
				.MetaClass(AActor::StaticClass())
				.SelectedClass_Lambda([this]()
				{
					return PreviewActorClass.Get();
				})
				.OnSetClass_Lambda([this](const UClass* SelectedClass)
				{
					PreviewActorClass = MakeWeakObjectPtr(SelectedClass);
					SmartObjectViewportClient->SetPreviewActorClass(SelectedClass);

					FScopedTransaction Transaction(LOCTEXT("SetPreviewClass", "Set Preview Class"));
					USmartObjectDefinition* Definition = CastChecked<USmartObjectDefinition>(GetEditingObject());
					Definition->Modify();
					Definition->PreviewClass = SelectedClass;
				})
			]
		];

	return PreviewSettingsTab.ToSharedRef();
}

void FSmartObjectAssetToolkit::OnClose()
{
	Tool->Cleanup();

	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);

	FBaseAssetToolkit::OnClose();
}

void FSmartObjectAssetToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Tool);
}

void FSmartObjectAssetToolkit::OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent) const
{
	if (ObjectBeingModified == nullptr || ObjectBeingModified != GetEditingObject())
	{
		return;
	}

	// Only monitor changes to Slots since we need to recreate the proper amount of Gizmos
	// Note that we can't use GET_MEMBER_NAME_CHECKED(USmartObjectDefinition, Slots)) since
	// the property is not public
	const FName SlotsMemberName(TEXT("Slots"));
	if (PropertyChangedEvent.GetPropertyName() == SlotsMemberName)
	{
		Tool->RebuildGizmos();
	}
	else if (PropertyChangedEvent.MemberProperty == nullptr)
	{
		// Provided event is invalid for undo, refresh isn't enough when it is undoing a delete, 
		// A rebuild is needed in that case as the gizmos are being destroyed upon deletion.
		Tool->RebuildGizmos();
	}
	else if (PropertyChangedEvent.MemberProperty->GetFName() == SlotsMemberName)
	{
		Tool->RefreshGizmos();
	}
}

#undef LOCTEXT_NAMESPACE
