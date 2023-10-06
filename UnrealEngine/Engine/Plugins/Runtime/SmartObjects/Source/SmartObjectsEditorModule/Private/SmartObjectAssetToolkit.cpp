// Copyright Epic Games, Inc. All Rights Reserved.

#include "SmartObjectAssetToolkit.h"
#include "AssetEditorModeManager.h"
#include "Engine/StaticMesh.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "SmartObjectAssetEditorViewportClient.h"
#include "SmartObjectDefinition.h"
#include "SmartObjectTypes.h"
#include "Viewports.h"
#include "Widgets/Docking/SDockTab.h"
#include "SSmartObjectViewport.h"
#include "IStructureDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"

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

	if (PreviewDetailsView.IsValid())
	{
		CachedPreviewData = MakeShared<TStructOnScope<FSmartObjectDefinitionPreviewData>>(Definition->PreviewData);
		PreviewDetailsView->SetStructureData(CachedPreviewData);
	}

	UpdatePreviewActor();

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
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	FStructureDetailsViewArgs StructureViewArgs;
	StructureViewArgs.bShowObjects = true;
	StructureViewArgs.bShowAssets = true;
	StructureViewArgs.bShowClasses = true;
	StructureViewArgs.bShowInterfaces = true;

	PreviewDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureViewArgs, nullptr);
	PreviewDetailsView->GetOnFinishedChangingPropertiesDelegate().AddLambda([&](const FPropertyChangedEvent& PropertyChangedEvent)
	{
		USmartObjectDefinition* Definition = Cast<USmartObjectDefinition>(GetEditingObject());
		// Ignore temporary interaction (dragging sliders, etc.)
		if (PropertyChangedEvent.ChangeType == EPropertyChangeType::ValueSet
			&& CachedPreviewData.IsValid()
			&& Definition)
		{
			{
				const FText PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetDisplayNameText() : FText::GetEmpty(); 
				FScopedTransaction Transaction(FText::Format(LOCTEXT("OnPreviewValueChanged", "Set {0}"), PropertyName));
				Definition->Modify();

				if (const FSmartObjectDefinitionPreviewData* PreviewData = CachedPreviewData->Get())
				{
					Definition->PreviewData = *PreviewData;
				}
			}
			
			UpdatePreviewActor();
		}
	});

	if (USmartObjectDefinition* Definition = Cast<USmartObjectDefinition>(GetEditingObject()))
	{
		CachedPreviewData = MakeShared<TStructOnScope<FSmartObjectDefinitionPreviewData>>(Definition->PreviewData);
		PreviewDetailsView->SetStructureData(CachedPreviewData);
	}
	
	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("PreviewSettingsTitle", "Preview Settings"))
		[
			PreviewDetailsView->GetWidget().ToSharedRef()
		];

	return SpawnedTab;
}

void FSmartObjectAssetToolkit::PostUndo(bool bSuccess)
{
	UpdateCachedPreviewDataFromDefinition();
}

void FSmartObjectAssetToolkit::PostRedo(bool bSuccess)
{
	UpdateCachedPreviewDataFromDefinition();
}

void FSmartObjectAssetToolkit::UpdateCachedPreviewDataFromDefinition()
{
	const USmartObjectDefinition* Definition = Cast<USmartObjectDefinition>(GetEditingObject());
	if (Definition
		&& CachedPreviewData.IsValid())
	{
		if (FSmartObjectDefinitionPreviewData* PreviewData = CachedPreviewData->Get())
		{
			*PreviewData = Definition->PreviewData;
		}
		
		PreviewDetailsView->GetDetailsView()->ForceRefresh();
		UpdatePreviewActor();
	}
}

void FSmartObjectAssetToolkit::UpdatePreviewActor()
{
	SmartObjectViewportClient->ResetPreviewActor();
	
	const USmartObjectDefinition* Definition = CastChecked<USmartObjectDefinition>(GetEditingObject());
	if (!Definition)
	{
		return;
	}

	if (!Definition->PreviewData.ObjectActorClass.IsNull())
	{
		SmartObjectViewportClient->SetPreviewActorClass(Definition->PreviewData.ObjectActorClass.LoadSynchronous());
	}
	else if (Definition->PreviewData.ObjectMeshPath.IsValid())
	{
		UStaticMesh* PreviewMesh = Cast<UStaticMesh>(Definition->PreviewData.ObjectMeshPath.TryLoad());
		if (PreviewMesh)
		{
			SmartObjectViewportClient->SetPreviewMesh(PreviewMesh);
		}
	}
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
