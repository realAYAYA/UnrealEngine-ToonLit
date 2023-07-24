// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectAssetToolkit.h"
#include "AssetEditorModeManager.h"
#include "Engine/StaticMesh.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "SmartObjectAssetEditorViewportClient.h"
#include "SmartObjectDefinition.h"
#include "Viewports.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Text/STextBlock.h"
#include "SSmartObjectViewport.h"

#define LOCTEXT_NAMESPACE "SmartObjectAssetToolkit"

const FName FSmartObjectAssetToolkit::PreviewSettingsTabID(TEXT("SmartObjectAssetToolkit_Preview"));
const FName FSmartObjectAssetToolkit::SceneViewportTabID(TEXT("SmartObjectAssetToolkit_Viewport"));

//----------------------------------------------------------------------//
// FSmartObjectAssetToolkit
//----------------------------------------------------------------------//
FSmartObjectAssetToolkit::FSmartObjectAssetToolkit(UAssetEditor* InOwningAssetEditor)
	: FBaseAssetToolkit(InOwningAssetEditor)
{
	FPreviewScene::ConstructionValues PreviewSceneArgs;
	AdvancedPreviewScene = MakeUnique<FAdvancedPreviewScene>(PreviewSceneArgs);

	// Apply small Z offset to not hide the grid
	constexpr float DefaultFloorOffset = 1.0f;
	AdvancedPreviewScene->SetFloorOffset(DefaultFloorOffset);

	// Setup our default layout
	StandaloneDefaultLayout = FTabManager::NewLayout(FName("SmartObjectAssetEditorLayout2"))
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
					->AddTab(SceneViewportTabID, ETabState::OpenedTab)
					->SetHideTabWell(true)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->AddTab(PreviewSettingsTabID, ETabState::OpenedTab)
					->AddTab(DetailsTabID, ETabState::OpenedTab)
					->SetForegroundTab(DetailsTabID)
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
	USmartObjectDefinition* Definition = Cast<USmartObjectDefinition>(GetEditingObject());
	check(Definition);

	// Allow the viewport client to interact with the preview component
	checkf(SmartObjectViewportClient.IsValid(), TEXT("ViewportClient is created in CreateEditorViewportClient before calling PostInitAssetEditor"));
	SmartObjectViewportClient->SetSmartObjectDefinition(*Definition);

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

	// Register to be notified when properties are edited
	FCoreUObjectDelegates::OnObjectPropertyChanged.AddRaw(this, &FSmartObjectAssetToolkit::OnPropertyChanged);
}

void FSmartObjectAssetToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
{
	FBaseAssetToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(SceneViewportTabID, FOnSpawnTab::CreateSP(this, &FSmartObjectAssetToolkit::SpawnTab_SceneViewport))
		.SetDisplayName(LOCTEXT("Viewport", "Viewport"))
		.SetGroup(AssetEditorTabsCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(PreviewSettingsTabID, FOnSpawnTab::CreateSP(this, &FSmartObjectAssetToolkit::SpawnTab_PreviewSettings))
		.SetDisplayName(LOCTEXT("PreviewSettings", "Preview Settings"))
		.SetGroup(AssetEditorTabsCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Visibility"));
}

TSharedRef<SDockTab> FSmartObjectAssetToolkit::SpawnTab_SceneViewport(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == FSmartObjectAssetToolkit::SceneViewportTabID);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab).Label(LOCTEXT("ViewportTab_Title", "Viewport"));

	TSharedRef<SSmartObjectViewport> ViewportWidget = SNew(SSmartObjectViewport, StaticCastSharedRef<FSmartObjectAssetToolkit>(AsShared()), AdvancedPreviewScene.Get());
	SpawnedTab->SetContent(ViewportWidget);

	return SpawnedTab;
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
	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);

	FBaseAssetToolkit::OnClose();
}

void FSmartObjectAssetToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
}

void FSmartObjectAssetToolkit::OnPropertyChanged(UObject* ObjectBeingModified, FPropertyChangedEvent& PropertyChangedEvent) const
{
	if (ObjectBeingModified == nullptr || ObjectBeingModified != GetEditingObject())
	{
		return;
	}
}

#undef LOCTEXT_NAMESPACE
