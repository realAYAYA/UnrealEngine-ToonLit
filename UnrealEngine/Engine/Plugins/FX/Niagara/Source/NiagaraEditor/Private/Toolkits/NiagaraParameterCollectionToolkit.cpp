// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraParameterCollectionToolkit.h"
#include "NiagaraEditorModule.h"
#include "SNiagaraParameterCollection.h"

#include "NiagaraObjectSelection.h"
#include "Widgets/SNiagaraSelectedObjectsDetails.h"
#include "NiagaraEditorCommands.h"
#include "NiagaraEditorStyle.h"
#include "ScopedTransaction.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#include "Styling/AppStyle.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Framework/Docking/WorkspaceItem.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Docking/SDockTab.h"
#include "NiagaraCollectionParameterViewModel.h"
#include "NiagaraParameterCollection.h"
#include "NiagaraParameterCollectionAssetViewModel.h"

#include "PropertyEditorModule.h"
#include "IDetailsView.h"

#define LOCTEXT_NAMESPACE "NiagaraParameterCollectionEditor"

const FName FNiagaraParameterCollectionToolkit::MainTabID(TEXT("NiagaraParameterCollectionEditor_Main"));

void FNiagaraParameterCollectionToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_NiagaraParameterCollectionEditor", "Niagara Parameter Collection"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(MainTabID, FOnSpawnTab::CreateSP(this, &FNiagaraParameterCollectionToolkit::SpawnTab_Main))
		.SetDisplayName(LOCTEXT("Parameters", "Parameters"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));
}

void FNiagaraParameterCollectionToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(MainTabID);
}


FNiagaraParameterCollectionToolkit::~FNiagaraParameterCollectionToolkit()
{
}

void FNiagaraParameterCollectionToolkit::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Collection);
	Collector.AddReferencedObject(Instance);
}

void FNiagaraParameterCollectionToolkit::Initialize(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraParameterCollection* InCollection)
{
	Collection = InCollection;
	checkf(Collection != nullptr, TEXT("Can not create toolkit with null parameter collection."));
	Instance = Collection->GetDefaultInstance();

	ParameterCollectionViewModel = MakeShareable(new FNiagaraParameterCollectionAssetViewModel(Collection, FText::FromString(Collection->GetName()), ENiagaraParameterEditMode::EditAll));

	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_Niagara_ParameterCollection_Layout_V1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.1f)
				->AddTab(MainTabID, ETabState::OpenedTab)
			)
		);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, FNiagaraEditorModule::NiagaraEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Collection);

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
	AddMenuExtender(NiagaraEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	SetupCommands();
	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

void FNiagaraParameterCollectionToolkit::Initialize(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraParameterCollectionInstance* InInstance)
{
	Instance = InInstance;
	checkf(Instance != nullptr, TEXT("Can not create toolkit with null parameter collection instance."));
	Collection = Instance->GetParent();

	ParameterCollectionViewModel = MakeShareable(new FNiagaraParameterCollectionAssetViewModel(InInstance, FText::FromString(InInstance->GetName()), ENiagaraParameterEditMode::EditAll));

	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_Niagara_ParameterCollection_Layout_V1")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.1f)
				->AddTab(MainTabID, ETabState::OpenedTab)
			)
		);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, FNiagaraEditorModule::NiagaraEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, InInstance);

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
	AddMenuExtender(NiagaraEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	SetupCommands();
	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

FName FNiagaraParameterCollectionToolkit::GetToolkitFName() const
{
	return FName("Niagara");
}

FText FNiagaraParameterCollectionToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Niagara");
}

FString FNiagaraParameterCollectionToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Niagara ").ToString();
}


FLinearColor FNiagaraParameterCollectionToolkit::GetWorldCentricTabColorScale() const
{
	return FNiagaraEditorModule::WorldCentricTabColorScale;
}


TSharedRef<SDockTab> FNiagaraParameterCollectionToolkit::SpawnTab_Main(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == MainTabID);

	ParameterCollection = SNew(SNiagaraParameterCollection, ParameterCollectionViewModel.ToSharedRef());

	TSharedRef<SVerticalBox> Contents = SNew(SVerticalBox);


	FDetailsViewArgs DetailArgs;
	DetailArgs.bAllowSearch = false;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.NotifyHook = ParameterCollectionViewModel.Get();
	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	if (Instance->IsDefaultInstance())
	{
		DetailsView->SetObject(Collection);
	}
	else
	{
		DetailsView->SetObject(Instance);
	}

	Contents->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 2.0f))
		[
			DetailsView
		];

	Contents->AddSlot()
		.AutoHeight()
		.Padding(FMargin(0.0f, 2.0f))
		[
			ParameterCollection.ToSharedRef()
		];
	
	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			Contents
		];

	return SpawnedTab;
}

void FNiagaraParameterCollectionToolkit::SetupCommands()
{
	// 	GetToolkitCommands()->MapAction(
	// 		FNiagaraEditorCommands::Get().ToggleUnlockToChanges,
	// 		FExecuteAction::CreateSP(this, &FNiagaraSystemToolkit::ToggleUnlockToChanges),
	// 		FCanExecuteAction(),
	// 		FIsActionChecked::CreateSP(this, &FNiagaraSystemToolkit::IsToggleUnlockToChangesChecked));
}

void FNiagaraParameterCollectionToolkit::ExtendToolbar()
{
	// 	struct Local
	// 	{
	// 		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, FOnGetContent GetEmitterMenuContent, FNiagaraSystemToolkit* Toolkit)
	// 		{
	// 			ToolbarBuilder.BeginSection("LockEmitters");
	// 			{
	// 				ToolbarBuilder.AddToolBarButton(FNiagaraEditorCommands::Get().ToggleUnlockToChanges, NAME_None,
	// 					TAttribute<FText>(Toolkit, &FNiagaraSystemToolkit::GetEmitterLockToChangesLabel),
	// 					TAttribute<FText>(Toolkit, &FNiagaraSystemToolkit::GetEmitterLockToChangesLabelTooltip),
	// 					TAttribute<FSlateIcon>(Toolkit, &FNiagaraSystemToolkit::GetEmitterLockToChangesIcon));
	// 			}
	// 			ToolbarBuilder.EndSection();
	// 			ToolbarBuilder.BeginSection("AddEmitter");
	// 			{
	// 				ToolbarBuilder.AddComboButton(
	// 					FUIAction(),
	// 					GetEmitterMenuContent,
	// 					LOCTEXT("AddEmitterButtonText", "Add Emitter"),
	// 					LOCTEXT("AddEmitterButtonTextToolTip", "Adds an emitter to the system from an existing emitter asset."),
	// 					FSlateIcon(FNiagaraEditorStyle::Get().GetStyleSetName(), "NiagaraEditor.AddEmitter"));
	// 			}
	// 			ToolbarBuilder.EndSection();
	// 		}
	// 	};

	//TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	//	FOnGetContent GetEmitterMenuContent = FOnGetContent::CreateSP(this, &FNiagaraSystemToolkit::CreateAddEmitterMenuContent);

	// 	ToolbarExtender->AddToolBarExtension(
	// 		"Asset",
	// 		EExtensionHook::After,
	// 		GetToolkitCommands(),
	// 		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar, GetEmitterMenuContent, this)
	// 	);

	//AddToolbarExtender(ToolbarExtender);

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
	AddToolbarExtender(NiagaraEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

#undef LOCTEXT_NAMESPACE
