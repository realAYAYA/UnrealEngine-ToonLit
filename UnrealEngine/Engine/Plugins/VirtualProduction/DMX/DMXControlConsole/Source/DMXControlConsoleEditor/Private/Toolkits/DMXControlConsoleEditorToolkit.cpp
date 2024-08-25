// Copyright Epic Games, Inc. All Rights Reserved.

#include "Toolkits/DMXControlConsoleEditorToolkit.h"

#include "Commands/DMXControlConsoleEditorCommands.h"
#include "DMXControlConsole.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorData.h"
#include "DMXControlConsoleEditorModule.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleEditorToolbar.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXEditorSettings.h"
#include "DMXEditorUtils.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/GenericCommands.h"
#include "Layouts/Controllers/DMXControlConsoleElementController.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/Controllers/DMXControlConsoleMatrixCellController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Views/SDMXControlConsoleEditorDetailsView.h"
#include "Views/SDMXControlConsoleEditorDMXLibraryView.h"
#include "Views/SDMXControlConsoleEditorFiltersView.h"
#include "Views/SDMXControlConsoleEditorLayoutView.h"
#include "Widgets/Docking/SDockTab.h"


#define LOCTEXT_NAMESPACE "DMXControlConsoleEditorToolkit"

namespace UE::DMX::Private
{
	const FName FDMXControlConsoleEditorToolkit::DMXLibraryViewTabID(TEXT("DMXControlConsoleEditorToolkit_DMXLibraryViewTabID"));
	const FName FDMXControlConsoleEditorToolkit::LayoutViewTabID(TEXT("DMXControlConsoleEditorToolkit_LayoutViewTabID"));
	const FName FDMXControlConsoleEditorToolkit::DetailsViewTabID(TEXT("DMXControlConsoleEditorToolkit_DetailsViewTabID"));
	const FName FDMXControlConsoleEditorToolkit::FiltersViewTabID(TEXT("DMXControlConsoleEditorToolkit_FiltersViewTabID"));

	FDMXControlConsoleEditorToolkit::FDMXControlConsoleEditorToolkit()
		: ControlConsole(nullptr)
		, AnalyticsProvider("ControlConsoleEditor")
	{}

	FDMXControlConsoleEditorToolkit::~FDMXControlConsoleEditorToolkit()
	{
		StopPlayingDMX();
	}

	void FDMXControlConsoleEditorToolkit::InitControlConsoleEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UDMXControlConsole* InControlConsole)
	{
		checkf(InControlConsole, TEXT("Invalid control console, can't initialize toolkit correctly."));
		ControlConsole = InControlConsole;

		EditorModel = NewObject<UDMXControlConsoleEditorModel>(GetTransientPackage(), NAME_None, RF_Transient | RF_Transactional);
		EditorModel->Initialize(SharedThis(this));

		UDMXEditorSettings* DMXEditorSettings = GetMutableDefault<UDMXEditorSettings>();
		if (DMXEditorSettings)
		{
			DMXEditorSettings->LastOpenedControlConsolePath = ControlConsole->GetPathName();
			DMXEditorSettings->SaveConfig();
		}

		InitializeInternal(Mode, InitToolkitHost, FGuid::NewGuid());
	}

	UDMXControlConsoleData* FDMXControlConsoleEditorToolkit::GetControlConsoleData() const
	{
		return ControlConsole ? ControlConsole->GetControlConsoleData() : nullptr;
	}

	UDMXControlConsoleEditorData* FDMXControlConsoleEditorToolkit::GetControlConsoleEditorData() const
	{
		return ControlConsole ? Cast<UDMXControlConsoleEditorData>(ControlConsole->ControlConsoleEditorData) : nullptr;
	}

	UDMXControlConsoleEditorLayouts* FDMXControlConsoleEditorToolkit::GetControlConsoleLayouts() const
	{
		return ControlConsole ? Cast<UDMXControlConsoleEditorLayouts>(ControlConsole->ControlConsoleEditorLayouts) : nullptr;
	}

	void FDMXControlConsoleEditorToolkit::RemoveAllSelectedElements()
	{
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = GetControlConsoleLayouts();
		if (!ControlConsoleLayouts || !EditorModel)
		{
			return;
		}

		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout || ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			return;
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupControllersObjects = SelectionHandler->GetSelectedFaderGroupControllers();
		if (SelectedFaderGroupControllersObjects.IsEmpty())
		{
			return;
		}

		const FScopedTransaction RemoveAllSelectedElementsTransaction(LOCTEXT("RemoveAllSelectedElementsTransaction", "Selected Elements removed"));

		// Delete all selected fader group controllers
		for (const TWeakObjectPtr<UObject>& SelectedFaderGroupControllerObject : SelectedFaderGroupControllersObjects)
		{
			UDMXControlConsoleFaderGroupController* SelectedFaderGroupController = Cast<UDMXControlConsoleFaderGroupController>(SelectedFaderGroupControllerObject);
			if (!SelectedFaderGroupController)
			{
				continue;
			}

			// Remove the controller only if there's no selected element controller or if all its element controllers are selected
			TArray<UDMXControlConsoleElementController*> SelectedElementControllersFromController = SelectionHandler->GetSelectedElementControllersFromFaderGroupController(SelectedFaderGroupController);
			TArray<UDMXControlConsoleElementController*> AllElementControllers = SelectedFaderGroupController->GetAllElementControllers();
			const auto RemoveMatrixCellControllersLambda = 
				[](UDMXControlConsoleElementController* ElementController)
				{
					return IsValid(Cast<UDMXControlConsoleMatrixCellController>(ElementController));
				};

			SelectedElementControllersFromController.RemoveAll(RemoveMatrixCellControllersLambda);
			AllElementControllers.RemoveAll(RemoveMatrixCellControllersLambda);

			const bool bRemoveController =
				SelectedElementControllersFromController.IsEmpty() ||
				SelectedElementControllersFromController.Num() == AllElementControllers.Num();
			
			if (!bRemoveController)
			{
				continue;
			}

			// If there's only one fader group controller to delete, replace it in selection
			if (SelectedFaderGroupControllersObjects.Num() == 1)
			{
				SelectionHandler->ReplaceInSelection(SelectedFaderGroupController);
			}

			constexpr bool bNotifySelectedFaderGroupControllerChange = false;
			SelectionHandler->RemoveFromSelection(SelectedFaderGroupController, bNotifySelectedFaderGroupControllerChange);

			// Destroy all unpatched fader groups in the controller
			const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& FaderGroups = SelectedFaderGroupController->GetFaderGroups();
			for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
			{
				if (FaderGroup.IsValid() && !FaderGroup->HasFixturePatch())
				{
					FaderGroup->Destroy();
				}
			}
				
			SelectedFaderGroupController->PreEditChange(nullptr);
			SelectedFaderGroupController->Destroy();
			SelectedFaderGroupController->PostEditChange();

			ActiveLayout->PreEditChange(nullptr);
			ActiveLayout->RemoveFromActiveFaderGroupControllers(SelectedFaderGroupController);
			ActiveLayout->PostEditChange();
		}

		// Delete all selected element controllers
		const TArray<TWeakObjectPtr<UObject>> SelectedElementControllers = SelectionHandler->GetSelectedElementControllers();
		if (!SelectedElementControllers.IsEmpty())
		{
			for (const TWeakObjectPtr<UObject>& SelectedElementControllerObject : SelectedElementControllers)
			{
				UDMXControlConsoleElementController* SelectedElementController = Cast<UDMXControlConsoleElementController>(SelectedElementControllerObject);
				if (!SelectedElementController || SelectedElementController->GetOwnerFaderGroupControllerChecked().HasFixturePatch())
				{
					continue;
				}

				// If there's only one element controller to delete, replace it in selection
				if (SelectedElementControllers.Num() == 1)
				{
					SelectionHandler->ReplaceInSelection(SelectedElementController);
				}

				constexpr bool bNotifyFaderSelectionChange = false;
				SelectionHandler->RemoveFromSelection(SelectedElementController, bNotifyFaderSelectionChange);

				// Destroy all elements in the selected element controller
				const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>>& Elements = SelectedElementController->GetElements();
				for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
				{
					if (Element && !Element->GetOwnerFaderGroupChecked().HasFixturePatch())
					{
						Element->Destroy();
					}
				}

				SelectedElementController->PreEditChange(nullptr);
				SelectedElementController->Destroy();
				SelectedElementController->PostEditChange();
			}
		}

		SelectionHandler->RemoveInvalidObjectsFromSelection();
	}

	void FDMXControlConsoleEditorToolkit::ClearAll()
	{
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = GetControlConsoleLayouts();
		if (!ensureMsgf(ControlConsoleLayouts, TEXT("Invalid control console layouts, cannot clear the active layout correctly.")))
		{
			return;
		}

		if (!ensureMsgf(EditorModel, TEXT("Invalid control console editor model, cannot clear the active layout correctly.")))
		{
			return;
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		SelectionHandler->ClearSelection();

		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return;
		}

		const FScopedTransaction ClearAllTransaction(LOCTEXT("ClearAllTransaction", "Clear All"));
		ActiveLayout->PreEditChange(nullptr);
		ActiveLayout->ClearAll();
		if (ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			constexpr bool bClearOnlyPatchedControllers = true;
			const TArray<UDMXControlConsoleEditorGlobalLayoutBase*> UserLayouts = ControlConsoleLayouts->GetUserLayouts();
			for (UDMXControlConsoleEditorGlobalLayoutBase* UserLayout : UserLayouts)
			{
				UserLayout->PreEditChange(nullptr);
				UserLayout->ClearAll(bClearOnlyPatchedControllers);
				UserLayout->PostEditChange();
			}

			if (UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData())
			{
				ControlConsoleData->PreEditChange(nullptr);
				ControlConsoleData->Clear(bClearOnlyPatchedControllers);
				ControlConsoleData->PostEditChange();
			}
		}
		ActiveLayout->PostEditChange();
	}

	void FDMXControlConsoleEditorToolkit::ResetToDefault()
	{
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = GetControlConsoleLayouts();
		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ensureMsgf(ActiveLayout, TEXT("Invalid layout, cannot reset to zero correctly.")))
		{
			return;
		}

		const FScopedTransaction ResetToDefaultTransaction(LOCTEXT("ResetToDefaultTransaction", "Reset to default"));
		const TArray<UDMXControlConsoleFaderGroupController*> FaderGroupControllers = ActiveLayout->GetAllFaderGroupControllers();
		for (const UDMXControlConsoleFaderGroupController* FaderGroupController : FaderGroupControllers)
		{
			if (!FaderGroupController)
			{
				continue;
			}

			const TArray<UDMXControlConsoleElementController*> ElementControllers = FaderGroupController->GetAllElementControllers();
			for (UDMXControlConsoleElementController* ElementController : ElementControllers)
			{
				if (!ElementController)
				{
					continue;
				}

				const TArray<UDMXControlConsoleFaderBase*> Faders = ElementController->GetFaders();
				for (UDMXControlConsoleFaderBase* Fader : Faders)
				{
					if (!Fader)
					{
						continue;
					}

					Fader->Modify();
				}

				ElementController->Modify();
				ElementController->ResetToDefault();
			}
		}

		if (TabManager.IsValid())
		{
			FSlateApplication::Get().SetUserFocus(0, TabManager->GetOwnerTab());
		}
	}

	void FDMXControlConsoleEditorToolkit::ResetToZero()
	{
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = GetControlConsoleLayouts();
		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ensureMsgf(ActiveLayout, TEXT("Invalid layout, cannot reset to zero correctly.")))
		{
			return;
		}

		const FScopedTransaction ResetToZeroTransaction(LOCTEXT("ResetToZeroTransaction", "Reset to zero"));
		const TArray<UDMXControlConsoleFaderGroupController*> FaderGroupControllers = ActiveLayout->GetAllFaderGroupControllers();
		for (const UDMXControlConsoleFaderGroupController* FaderGroupController : FaderGroupControllers)
		{
			if (!FaderGroupController)
			{
				continue;
			}

			const TArray<UDMXControlConsoleElementController*> ElementControllers = FaderGroupController->GetAllElementControllers();
			for (UDMXControlConsoleElementController* ElementController : ElementControllers)
			{
				if (!ElementController)
				{
					continue;
				}

				const TArray<UDMXControlConsoleFaderBase*> Faders = ElementController->GetFaders();
				for (UDMXControlConsoleFaderBase* Fader : Faders)
				{
					if (!Fader)
					{
						continue;
					}

					Fader->Modify();
				}

				ElementController->PreEditChange(UDMXControlConsoleElementController::StaticClass()->FindPropertyByName(UDMXControlConsoleElementController::GetValuePropertyName()));
				ElementController->SetValue(0.f);
				ElementController->PostEditChange();
			}
		}

		if (TabManager.IsValid())
		{
			FSlateApplication::Get().SetUserFocus(0, TabManager->GetOwnerTab());
		}
	}

	void FDMXControlConsoleEditorToolkit::RegisterTabSpawners(const TSharedRef<FTabManager>& InTabManager)
	{
		WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_ControlConsoleEditor", "DMX Control Console Editor"));
		TSharedRef<FWorkspaceItem> WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

		FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

		InTabManager->RegisterTabSpawner(DMXLibraryViewTabID, FOnSpawnTab::CreateSP(this, &FDMXControlConsoleEditorToolkit::SpawnTab_DMXLibraryView))
			.SetDisplayName(LOCTEXT("Tab_DMXLibraryView", "DMX Library"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.DMXLibrary"));

		InTabManager->RegisterTabSpawner(LayoutViewTabID, FOnSpawnTab::CreateSP(this, &FDMXControlConsoleEditorToolkit::SpawnTab_LayoutView))
			.SetDisplayName(LOCTEXT("Tab_LayoutView", "Layout Editor"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.TabIcon"));

		InTabManager->RegisterTabSpawner(DetailsViewTabID, FOnSpawnTab::CreateSP(this, &FDMXControlConsoleEditorToolkit::SpawnTab_DetailsView))
			.SetDisplayName(LOCTEXT("Tab_EditorView", "Details"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Details"));

		InTabManager->RegisterTabSpawner(FiltersViewTabID, FOnSpawnTab::CreateSP(this, &FDMXControlConsoleEditorToolkit::SpawnTab_FiltersView))
			.SetDisplayName(LOCTEXT("Tab_FiltersView", "Filters"))
			.SetGroup(WorkspaceMenuCategoryRef)
			.SetIcon(FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Filter"));
	}

	void FDMXControlConsoleEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
	{
		FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

		InTabManager->UnregisterTabSpawner(DMXLibraryViewTabID);
		InTabManager->UnregisterTabSpawner(LayoutViewTabID);
		InTabManager->UnregisterTabSpawner(DetailsViewTabID);
		InTabManager->UnregisterTabSpawner(FiltersViewTabID);
	}

	const FSlateBrush* FDMXControlConsoleEditorToolkit::GetDefaultTabIcon() const
	{
		return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.TabIcon");
	}

	FText FDMXControlConsoleEditorToolkit::GetBaseToolkitName() const
	{
		return LOCTEXT("AppLabel", "DMX Control Console");
	}

	FName FDMXControlConsoleEditorToolkit::GetToolkitFName() const
	{
		return FName("DMXControlConsole");
	}

	FString FDMXControlConsoleEditorToolkit::GetWorldCentricTabPrefix() const
	{
		return LOCTEXT("WorldCentricTabPrefix", "DMX Control Console ").ToString();
	}

	void FDMXControlConsoleEditorToolkit::AddReferencedObjects(FReferenceCollector& Collector)
	{
		Collector.AddReferencedObject(EditorModel);
		Collector.AddReferencedObject(ControlConsole);
	}

	FString FDMXControlConsoleEditorToolkit::GetReferencerName() const
	{
		return TEXT("FDMXControlConsoleEditorToolkit");
	}

	void FDMXControlConsoleEditorToolkit::InitializeInternal(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, const FGuid& MessageLogGuid)
	{
		if (!ControlConsole)
		{
			return;
		}

		ExtendToolbar();
		GenerateInternalViews();

		TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_ControlConsole_Layout_1.5")
			->AddArea
			(
				FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Horizontal)
					->Split
					(
						FTabManager::NewStack()
						->AddTab(DMXLibraryViewTabID, ETabState::OpenedTab)
						->SetSizeCoefficient(.2f)
					)
					->Split
					(
						FTabManager::NewStack()
						->AddTab(LayoutViewTabID, ETabState::OpenedTab)
						->SetSizeCoefficient(.6f)
					)
					->Split
					(
						FTabManager::NewStack()
						->AddTab(DetailsViewTabID, ETabState::SidebarTab, ESidebarLocation::Right, .2f)
						->SetSizeCoefficient(.2f)
					)

					->Split
					(
						FTabManager::NewStack()
						->AddTab(FiltersViewTabID, ETabState::SidebarTab, ESidebarLocation::Right, .1f)
						->SetSizeCoefficient(.1f)
					)
				)
			);

		const bool bCreateDefaultStandaloneMenu = true;
		const bool bCreateDefaultToolbar = true;
		FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, FDMXControlConsoleEditorModule::ControlConsoleEditorAppIdentifier,
			StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ControlConsole);

		SetupCommands();
		RegenerateMenusAndToolbars();
	}

	void FDMXControlConsoleEditorToolkit::GenerateInternalViews()
	{
		GenerateDMXLibraryView();
		GenerateLayoutView();
		GenerateDetailsView();
		GenerateFiltersView();
	}

	TSharedRef<SDMXControlConsoleEditorDMXLibraryView> FDMXControlConsoleEditorToolkit::GenerateDMXLibraryView()
	{
		if (!DMXLibraryView.IsValid())
		{
			DMXLibraryView = SNew(SDMXControlConsoleEditorDMXLibraryView, EditorModel);
		}

		return DMXLibraryView.ToSharedRef();
	}

	TSharedRef<SDMXControlConsoleEditorLayoutView> FDMXControlConsoleEditorToolkit::GenerateLayoutView()
	{
		if (!LayoutView.IsValid())
		{
			LayoutView = SNew(SDMXControlConsoleEditorLayoutView, EditorModel);
		}

		return LayoutView.ToSharedRef();
	}

	TSharedRef<SDMXControlConsoleEditorDetailsView> FDMXControlConsoleEditorToolkit::GenerateDetailsView()
	{
		if (!DetailsView.IsValid())
		{
			DetailsView = SNew(SDMXControlConsoleEditorDetailsView, EditorModel);
		}

		return DetailsView.ToSharedRef();
	}

	TSharedRef<SDMXControlConsoleEditorFiltersView> FDMXControlConsoleEditorToolkit::GenerateFiltersView()
	{
		if (!FiltersView.IsValid())
		{
			FiltersView = SNew(SDMXControlConsoleEditorFiltersView, Toolbar, EditorModel);
		}

		return FiltersView.ToSharedRef();
	}

	TSharedRef<SDockTab> FDMXControlConsoleEditorToolkit::SpawnTab_DMXLibraryView(const FSpawnTabArgs& Args)
	{
		check(Args.GetTabId() == DMXLibraryViewTabID);

		const TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
			.Label(LOCTEXT("DMXLibraryViewTabID", "DMX Library"))
			[
				DMXLibraryView.ToSharedRef()
			];

		return SpawnedTab;
	}

	TSharedRef<SDockTab> FDMXControlConsoleEditorToolkit::SpawnTab_LayoutView(const FSpawnTabArgs& Args)
	{
		check(Args.GetTabId() == LayoutViewTabID);

		const TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
			.Label(LOCTEXT("LayoutViewTabID", "Layout Editor"))
			[
				LayoutView.ToSharedRef()
			];

		return SpawnedTab;
	}

	TSharedRef<SDockTab> FDMXControlConsoleEditorToolkit::SpawnTab_DetailsView(const FSpawnTabArgs& Args)
	{
		check(Args.GetTabId() == DetailsViewTabID);

		const TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
			.Label(LOCTEXT("DetailsViewTabID", "Details"))
			[
				DetailsView.ToSharedRef()
			];

		return SpawnedTab;
	}

	TSharedRef<SDockTab> FDMXControlConsoleEditorToolkit::SpawnTab_FiltersView(const FSpawnTabArgs& Args)
	{
		check(Args.GetTabId() == FiltersViewTabID);

		const TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
			.Label(LOCTEXT("FiltersViewTabID", "Filters"))
			[
				FiltersView.ToSharedRef()
			];

		return SpawnedTab;
	}

	void FDMXControlConsoleEditorToolkit::SetupCommands()
	{
		GetToolkitCommands()->MapAction(
			FDMXControlConsoleEditorCommands::Get().PlayDMX,
			FExecuteAction::CreateSP(this, &FDMXControlConsoleEditorToolkit::PlayDMX),
			FCanExecuteAction::CreateLambda([this]
				{
					return !IsPlayingDMX() && !bPaused;
				}),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateLambda([this]
				{
					return !IsPlayingDMX() && !bPaused;
				})
		);

		GetToolkitCommands()->MapAction(
			FDMXControlConsoleEditorCommands::Get().PauseDMX,
			FExecuteAction::CreateSP(this, &FDMXControlConsoleEditorToolkit::PauseDMX),
			FCanExecuteAction::CreateLambda([this]
				{
					return IsPlayingDMX();
				}),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateLambda([this]
				{
					return IsPlayingDMX();
				})
		);

		GetToolkitCommands()->MapAction(
			FDMXControlConsoleEditorCommands::Get().ResumeDMX,
			FExecuteAction::CreateSP(this, &FDMXControlConsoleEditorToolkit::PlayDMX),
			FCanExecuteAction::CreateLambda([this]
				{
					return !IsPlayingDMX() && bPaused;
				}),
			FIsActionChecked(),
			FIsActionButtonVisible::CreateLambda([this]
				{
					return !IsPlayingDMX() && bPaused;
				})
		);

		GetToolkitCommands()->MapAction(
			FDMXControlConsoleEditorCommands::Get().StopDMX,
			FExecuteAction::CreateSP(this, &FDMXControlConsoleEditorToolkit::StopPlayingDMX),
			FCanExecuteAction::CreateLambda([this]
				{
					return IsPlayingDMX() || bPaused;
				})
		);

		GetToolkitCommands()->MapAction(
			FDMXControlConsoleEditorCommands::Get().TogglePlayPauseDMX,
			FExecuteAction::CreateSP(this, &FDMXControlConsoleEditorToolkit::TogglePlayPauseDMX)
		);

		GetToolkitCommands()->MapAction(
			FDMXControlConsoleEditorCommands::Get().TogglePlayStopDMX,
			FExecuteAction::CreateSP(this, &FDMXControlConsoleEditorToolkit::TogglePlayStopDMX)
		);

		GetToolkitCommands()->MapAction(
			FDMXControlConsoleEditorCommands::Get().EditorStopKeepsLastValues,
			FExecuteAction::CreateSP(this, &FDMXControlConsoleEditorToolkit::SetStopDMXMode, EDMXControlConsoleStopDMXMode::DoNotSendValues),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &FDMXControlConsoleEditorToolkit::IsUsingStopDMXMode, EDMXControlConsoleStopDMXMode::DoNotSendValues)
		);

		GetToolkitCommands()->MapAction(
			FDMXControlConsoleEditorCommands::Get().EditorStopSendsDefaultValues,
			FExecuteAction::CreateSP(this, &FDMXControlConsoleEditorToolkit::SetStopDMXMode, EDMXControlConsoleStopDMXMode::SendDefaultValues),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &FDMXControlConsoleEditorToolkit::IsUsingStopDMXMode, EDMXControlConsoleStopDMXMode::SendDefaultValues)
		);

		GetToolkitCommands()->MapAction(
			FDMXControlConsoleEditorCommands::Get().EditorStopSendsZeroValues,
			FExecuteAction::CreateSP(this, &FDMXControlConsoleEditorToolkit::SetStopDMXMode, EDMXControlConsoleStopDMXMode::SendZeroValues),
			FCanExecuteAction(),
			FIsActionChecked::CreateSP(this, &FDMXControlConsoleEditorToolkit::IsUsingStopDMXMode, EDMXControlConsoleStopDMXMode::SendZeroValues)
		);		

		GetToolkitCommands()->MapAction
		(
			FDMXControlConsoleEditorCommands::Get().RemoveElements,
			FExecuteAction::CreateSP(this, &FDMXControlConsoleEditorToolkit::RemoveAllSelectedElements)
		);

		GetToolkitCommands()->MapAction
		(
			FDMXControlConsoleEditorCommands::Get().ClearAll,
			FExecuteAction::CreateSP(this, &FDMXControlConsoleEditorToolkit::ClearAll)
		);

		GetToolkitCommands()->MapAction
		(
			FDMXControlConsoleEditorCommands::Get().ResetToDefault,
			FExecuteAction::CreateSP(this, &FDMXControlConsoleEditorToolkit::ResetToDefault)
		);

		GetToolkitCommands()->MapAction
		(
			FDMXControlConsoleEditorCommands::Get().ResetToZero,
			FExecuteAction::CreateSP(this, &FDMXControlConsoleEditorToolkit::ResetToZero)
		);

		if (EditorModel)
		{
			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
			constexpr bool bSelectOnlyVisible = true;
			GetToolkitCommands()->MapAction
			(
				FDMXControlConsoleEditorCommands::Get().SelectAll,
				FExecuteAction::CreateSP(SelectionHandler, &FDMXControlConsoleEditorSelection::SelectAll, bSelectOnlyVisible)
			);
		}
	}

	void FDMXControlConsoleEditorToolkit::ExtendToolbar()
	{
		Toolbar = MakeShared<FDMXControlConsoleEditorToolbar>(SharedThis(this));

		const TSharedRef<FExtender> ToolbarExtender = MakeShareable(new FExtender);
		Toolbar->BuildToolbar(ToolbarExtender);
		AddToolbarExtender(ToolbarExtender);
	}

	void FDMXControlConsoleEditorToolkit::PlayDMX()
	{
		if (UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData())
		{
			ControlConsoleData->StartSendingDMX();
		}
	}

	bool FDMXControlConsoleEditorToolkit::IsPlayingDMX() const
	{
		UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData();
		return ControlConsoleData && ControlConsoleData->IsSendingDMX();
	}

	void FDMXControlConsoleEditorToolkit::PauseDMX()
	{
		if (UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData())
		{
			// When pausing, always use the stop mode that does not send DMX values
			const EDMXControlConsoleStopDMXMode RestoreStopDMXMode = ControlConsoleData->GetStopDMXMode();
			ControlConsoleData->SetStopDMXMode(EDMXControlConsoleStopDMXMode::DoNotSendValues);

			ControlConsoleData->StopSendingDMX();

			ControlConsoleData->SetStopDMXMode(RestoreStopDMXMode);
		}
	}

	void FDMXControlConsoleEditorToolkit::StopPlayingDMX()
	{
		if (UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData())
		{
			ControlConsoleData->StopSendingDMX();
		}
	}

	void FDMXControlConsoleEditorToolkit::TogglePlayPauseDMX()
	{
		if (IsPlayingDMX())
		{
			PauseDMX();
		}
		else
		{
			PlayDMX();
		}
	}

	void FDMXControlConsoleEditorToolkit::TogglePlayStopDMX()
	{
		if (IsPlayingDMX())
		{
			StopPlayingDMX();
		}
		else
		{
			PlayDMX();
		}
	}

	void FDMXControlConsoleEditorToolkit::SetStopDMXMode(EDMXControlConsoleStopDMXMode StopDMXMode)
	{
		// Intentionally without transaction, changes should not follow undo/redo
		if (UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData())
		{
			ControlConsoleData->MarkPackageDirty();
			ControlConsoleData->SetStopDMXMode(StopDMXMode);
		}
	}

	bool FDMXControlConsoleEditorToolkit::IsUsingStopDMXMode(EDMXControlConsoleStopDMXMode TestStopDMXMode) const
	{
		if (UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData())
		{
			return ControlConsoleData->GetStopDMXMode() == TestStopDMXMode;
		}
		return false;
	}
}

#undef LOCTEXT_NAMESPACE
