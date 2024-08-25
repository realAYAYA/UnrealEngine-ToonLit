// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleFixturePatchList.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "Algo/Copy.h"
#include "Algo/Find.h"
#include "Commands/DMXControlConsoleEditorCommands.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layouts/Controllers/DMXControlConsoleElementController.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "ToolMenus.h"
#include "Widgets/DMXReadOnlyFixturePatchListItem.h"
#include "Widgets/SDMXControlConsoleAddFixturePatchMenu.h"
#include "Widgets/SDMXControlConsoleFixturePatchListRow.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleFixturePatchList"

namespace UE::DMX::Private
{
	namespace Internal
	{
		/** Internal helper to find patches excluded from the list when in default layout mode */
		bool IsFixturePatchExcludedInDefaultLayout(const UDMXEntityFixturePatch* FixturePatch, EDMXReadOnlyFixturePatchListShowMode ShowMode, const UDMXControlConsoleEditorModel* InEditorModel)
		{
			const UDMXControlConsoleData* ControlConsoleData = InEditorModel ? InEditorModel->GetControlConsoleData() : nullptr;
			if (!ControlConsoleData)
			{
				return false;
			}

			const UDMXControlConsoleFaderGroup* FaderGroup = ControlConsoleData->FindFaderGroupByFixturePatch(FixturePatch);
			if (!FaderGroup)
			{
				return false;
			}

			const bool bEnabled = FaderGroup->IsEnabled();
			switch (ShowMode)
			{
			case EDMXReadOnlyFixturePatchListShowMode::Active:
				return bEnabled;
				break;
			case EDMXReadOnlyFixturePatchListShowMode::Inactive:
				return !bEnabled;
				break;
			default:
				return false;
			}
		}

		/** Internal helper to find patches excluded from the list when in user layout mode */
		bool IsFixturePatchExcludedInUserLayout(const UDMXEntityFixturePatch* FixturePatch, EDMXReadOnlyFixturePatchListShowMode ShowMode, const UDMXControlConsoleEditorModel* InEditorModel)
		{
			const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = InEditorModel ? InEditorModel->GetControlConsoleLayouts() : nullptr;
			if (!ControlConsoleLayouts)
			{
				return false;
			}

			const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
			if (!ActiveLayout)
			{
				return false;
			}

			const UDMXControlConsoleFaderGroupController* FaderGroupController = ActiveLayout->FindFaderGroupControllerByFixturePatch(FixturePatch);
			const bool bIsAddedToUserLayout = IsValid(FaderGroupController);

			switch (ShowMode)
			{
			case EDMXReadOnlyFixturePatchListShowMode::Active:
				return !bIsAddedToUserLayout;
				break;
			case EDMXReadOnlyFixturePatchListShowMode::Inactive:
				return bIsAddedToUserLayout;
				break;
			default:
				return false;
			}
		}

		/** Helper that returns true if the default layout is the active layout */
		bool IsDefaultLayoutActive(const UDMXControlConsoleEditorModel* InEditorModel)
		{
			const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = InEditorModel ? InEditorModel->GetControlConsoleLayouts() : nullptr;
			if (ControlConsoleLayouts)
			{
				const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
				return ActiveLayout && ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked();
			}

			return false;
		}

		/** Helper that returns fixture patches that should be excluded from the list, given the active layout class and the current show mode. */
		TArray<UDMXEntityFixturePatch*> FindFixturePatchesToExclude(const TArray<UDMXEntityFixturePatch*> AllFixturePatches, EDMXReadOnlyFixturePatchListShowMode ShowMode, const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel)
		{
			TArray<UDMXEntityFixturePatch*> Result;

			const UDMXControlConsoleEditorModel* EditorModel = InWeakEditorModel.Get();
			if (EditorModel)
			{
				Algo::CopyIf(AllFixturePatches, Result, [ShowMode, EditorModel](const UDMXEntityFixturePatch* FixturePatch)
					{
						if (!FixturePatch)
						{
							return false;
						}

						if (IsDefaultLayoutActive(EditorModel))
						{
							return Internal::IsFixturePatchExcludedInDefaultLayout(FixturePatch, ShowMode, EditorModel);
						}
						else
						{
							return Internal::IsFixturePatchExcludedInUserLayout(FixturePatch, ShowMode, EditorModel);
						}
					});
			}

			return Result;
		}
	}

	const FName FDMXControlConsoleReadOnlyFixturePatchListCollumnIDs::FaderGroupEnabled = "FaderGroupEnabled";

	SDMXControlConsoleFixturePatchList::~SDMXControlConsoleFixturePatchList()
	{
		const FName MenuName = GetHeaderRowFilterMenuName();

		UToolMenus* ToolMenus = UToolMenus::Get();
		if (ToolMenus->IsMenuRegistered(MenuName))
		{
			ToolMenus->RemoveMenu(MenuName);
		}
	}

	void SDMXControlConsoleFixturePatchList::Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, can't constuct fixture patch list correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
		EditorModelUniqueID = EditorModel->GetUniqueID();

		// Register the header row filter menu extender
		const FName MenuName = GetHeaderRowFilterMenuName();
		if (UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(MenuName))
		{
			const FToolMenuInsert SectionInsertLocation("ShowColumnSection", EToolMenuInsertType::Before);

			Menu->AddDynamicSection
			(
				"FilterActiveAndInactivePatches",
				FNewToolMenuDelegate::CreateSP(this, &SDMXControlConsoleFixturePatchList::ExtendHeaderRowFilterMenu),
				SectionInsertLocation
			);
		}

		FDMXReadOnlyFixturePatchListDescriptor ListDescriptor;
		UDMXControlConsoleEditorData* EditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (EditorData)
		{
			ListDescriptor = EditorData->FixturePatchListDescriptor;
		}

		SDMXReadOnlyFixturePatchList::Construct(SDMXReadOnlyFixturePatchList::FArguments()
			.ListDescriptor(ListDescriptor)
			.DMXLibrary(InArgs._DMXLibrary)
			.OnContextMenuOpening(this, &SDMXControlConsoleFixturePatchList::OnContextMenuOpening)
			.OnRowClicked(this, &SDMXControlConsoleFixturePatchList::OnRowClicked)
			.OnRowDoubleClicked(this, &SDMXControlConsoleFixturePatchList::OnRowDoubleClicked)
			.OnRowSelectionChanged(this, &SDMXControlConsoleFixturePatchList::OnSelectionChanged));

		EditorModel->GetOnEditorModelUpdated().AddSP(this, &SDMXControlConsoleFixturePatchList::RequestRefresh);

		RegisterCommands();
		ForceRefresh();
	}

	FName SDMXControlConsoleFixturePatchList::GetHeaderRowFilterMenuName() const
	{
		// Override the default menu, so it can be customized only for this list class here
		const FString DefaultFilterMenuNameAsString = TEXT("DMXEditor.ControlConsoleFixturePatchList.HeaderRowFilterMenu");
		const FString EditorModelUniqueIDAsString = FString::FromInt(EditorModelUniqueID);

		const FName FilterMenuName = *(DefaultFilterMenuNameAsString + EditorModelUniqueIDAsString);
		return FilterMenuName;
	}

	void SDMXControlConsoleFixturePatchList::ForceRefresh()
	{
		if (!EditorModel.IsValid())
		{
			return;
		}

		using namespace UE::DMX::Private::Internal;
		const TArray<UDMXEntityFixturePatch*> FixturePatchesToExclude = FindFixturePatchesToExclude(GetFixturePatchesInDMXLibrary(), ShowMode, EditorModel);

		SetExcludedFixturePatches(FixturePatchesToExclude);
		SDMXReadOnlyFixturePatchList::ForceRefresh();

		AdoptSelectionFromData();

		// Listen to data changes
		UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
		UDMXControlConsoleEditorData* ControlConsoleEditorData = EditorModel->GetControlConsoleEditorData();
		UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		if (!ControlConsoleData || !ControlConsoleEditorData || !ControlConsoleLayouts)
		{
			return;
		}

		if (!ControlConsoleData->GetOnFaderGroupAdded().IsBoundToObject(this))
		{
			ControlConsoleData->GetOnFaderGroupAdded().AddSP(this, &SDMXControlConsoleFixturePatchList::OnFaderGroupAddedOrRemoved);
		}

		if (!ControlConsoleData->GetOnFaderGroupRemoved().IsBoundToObject(this))
		{
			ControlConsoleData->GetOnFaderGroupRemoved().AddSP(this, &SDMXControlConsoleFixturePatchList::OnFaderGroupAddedOrRemoved);
		}

		if (!ControlConsoleEditorData->GetOnAutoGroupStateChanged().IsBoundToObject(this))
		{
			ControlConsoleEditorData->GetOnAutoGroupStateChanged().AddSP(this, &SDMXControlConsoleFixturePatchList::OnAutoGroupStateChanged);
		}

		if (!ControlConsoleLayouts->GetOnActiveLayoutChanged().IsBoundToObject(this))
		{
			ControlConsoleLayouts->GetOnActiveLayoutChanged().AddSP(this, &SDMXControlConsoleFixturePatchList::OnActiveLayoutChanged);
		}
	}

	TSharedRef<SHeaderRow> SDMXControlConsoleFixturePatchList::GenerateHeaderRow()
	{
		const TSharedRef<SHeaderRow> HeaderRow = SDMXReadOnlyFixturePatchList::GenerateHeaderRow();

		// Insert the fixture group enabled checkbox at column index 1
		constexpr int32 FixtureGroupEnabledColumnIndex = 1;
		HeaderRow->InsertColumn(SHeaderRow::FColumn::FArguments()
		.ColumnId(FDMXControlConsoleReadOnlyFixturePatchListCollumnIDs::FaderGroupEnabled)
		.DefaultLabel(LOCTEXT("CheckBoxColumnLabel", ""))
		.FixedWidth(32.f)
		.HeaderContent()
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("NoBorder"))
			[
				SNew(SBox)
				.WidthOverride(20.f)
				.HeightOverride(20.f)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(2.f)
				[
					SNew(SCheckBox)
					.IsChecked(this, &SDMXControlConsoleFixturePatchList::GetGlobalFaderGroupsEnabledCheckBoxState)
					.OnCheckStateChanged(this, &SDMXControlConsoleFixturePatchList::OnGlobalFaderGroupsEnabledCheckBoxStateChanged)
				]
			]
		],
		FixtureGroupEnabledColumnIndex);

		return HeaderRow;
	}

	TSharedRef<ITableRow> SDMXControlConsoleFixturePatchList::OnGenerateRow(TSharedPtr<FDMXReadOnlyFixturePatchListItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return
			SNew(SDMXControlConsoleFixturePatchListRow, OwnerTable, InItem.ToSharedRef(), EditorModel)
			.OnFaderGroupMutedChanged(this, &SDMXControlConsoleFixturePatchList::RequestRefresh);
	}

	void SDMXControlConsoleFixturePatchList::ToggleColumnShowState(const FName ColumnID)
	{
		SDMXReadOnlyFixturePatchList::ToggleColumnShowState(ColumnID);

		UDMXControlConsoleEditorData* EditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (EditorData)
		{
			const FDMXReadOnlyFixturePatchListDescriptor ListDescriptor = MakeListDescriptor();
			EditorData->Modify();
			EditorData->FixturePatchListDescriptor = ListDescriptor;
		}
	}

	void SDMXControlConsoleFixturePatchList::ExtendHeaderRowFilterMenu(UToolMenu* InMenu)
	{
		FToolMenuSection& Section = InMenu->AddSection(NAME_None, LOCTEXT("FixturePatchListShowPatchFilterSection", "Filter"));

		auto AddMenuEntryLambda = [this, &Section](const FName& Name, const FText& Label, const FText& ToolTip, const EDMXReadOnlyFixturePatchListShowMode InShowMode)
			{
				Section.AddMenuEntry(
					Name,
					Label,
					ToolTip,
					FSlateIcon(),
					FUIAction
					(
						FExecuteAction::CreateSP(this, &SDMXControlConsoleFixturePatchList::SetShowMode, InShowMode),
						FCanExecuteAction(),
						FIsActionChecked::CreateSP(this, &SDMXControlConsoleFixturePatchList::IsUsingShowMode, InShowMode)
					),
					EUserInterfaceActionType::RadioButton
				);
			};

		AddMenuEntryLambda(
			"ShowAllPatches",
			LOCTEXT("FixturePatchAllPatchesFilter_Label", "All Patches"),
			LOCTEXT("FixturePatchAllPatchesFilter", "Show all the Fixture Patches in the list"),
			EDMXReadOnlyFixturePatchListShowMode::All
		);

		AddMenuEntryLambda(
			"ShowActivePatches",
			LOCTEXT("FixturePatchActivePatchesFilter_Label", "Only Active"),
			LOCTEXT("FixturePatchActivePatchesFilter", "Show only active Fixture Patches in the list"),
			EDMXReadOnlyFixturePatchListShowMode::Active
		);

		AddMenuEntryLambda(
			"ShowInactivePatches",
			LOCTEXT("FixturePatchInactivePatchesFilter_Label", "Only Inactive"),
			LOCTEXT("FixturePatchInactivePatchesFilter", "Show only inactive Fixture Patches in the list"),
			EDMXReadOnlyFixturePatchListShowMode::Inactive
		);
	}

	void SDMXControlConsoleFixturePatchList::RegisterCommands()
	{
		CommandList = MakeShared<FUICommandList>();

		const auto MapEnableActionLambda = [this](TSharedPtr<FUICommandInfo> CommandInfo, bool bEnable, bool bOnlyActive)
			{
				CommandList->MapAction
				(
					CommandInfo,
					FExecuteAction::CreateSP(this, &SDMXControlConsoleFixturePatchList::OnEnableAllFaderGroups, bEnable, bOnlyActive),
					FCanExecuteAction::CreateSP(this, &SDMXControlConsoleFixturePatchList::IsAnyFaderGroupEnabled, !bEnable, bOnlyActive),
					FGetActionCheckState(),
					FIsActionButtonVisible::CreateSP(this, &SDMXControlConsoleFixturePatchList::IsAnyFaderGroupEnabled, !bEnable, bOnlyActive)
				);
			};

		MapEnableActionLambda(FDMXControlConsoleEditorCommands::Get().Enable, true, true);
		MapEnableActionLambda(FDMXControlConsoleEditorCommands::Get().EnableAll, true, false);
		MapEnableActionLambda(FDMXControlConsoleEditorCommands::Get().Disable, false, true);
		MapEnableActionLambda(FDMXControlConsoleEditorCommands::Get().DisableAll, false, false);
	}

	void SDMXControlConsoleFixturePatchList::AdoptSelectionFromData()
	{
		if (!EditorModel.IsValid())
		{
			return;
		}

		const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		if (!ControlConsoleData || !ControlConsoleLayouts)
		{
			return;
		}

		// Do only if the active layout is the default layout
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout || ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			return;
		}

		const TArray<UDMXControlConsoleFaderGroupController*> FaderGroupControllers = ActiveLayout->GetAllFaderGroupControllers();
		for (const UDMXControlConsoleFaderGroupController* FaderGroupController : FaderGroupControllers)
		{
			if (!FaderGroupController || !FaderGroupController->HasFixturePatch())
			{
				continue;
			}

			const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& FaderGroups = FaderGroupController->GetFaderGroups();
			for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
			{
				if (!FaderGroup.IsValid())
				{
					continue;
				}

				const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
				const TSharedPtr<FDMXReadOnlyFixturePatchListItem>* ItemPtr = Algo::FindByPredicate(GetListItems(), 
					[FixturePatch](const TSharedPtr<FDMXReadOnlyFixturePatchListItem>& Item)
					{
						return FixturePatch == Item->GetFixturePatch();
					});

				if (ItemPtr)
				{
					const bool bIsSelected = FaderGroupController->IsActive();
					SetItemSelection(*ItemPtr, bIsSelected, ESelectInfo::Direct);
				}
			}
		}
	}

	void SDMXControlConsoleFixturePatchList::OnFaderGroupAddedOrRemoved(const UDMXControlConsoleFaderGroup* FaderGroup)
	{
		RequestRefresh();
	}

	void SDMXControlConsoleFixturePatchList::OnAutoGroupStateChanged()
	{
		const TArray<TSharedPtr<FDMXReadOnlyFixturePatchListItem>> SelectedFixturePatches = GetSelectedItems();
		const TSharedPtr<FDMXReadOnlyFixturePatchListItem> NewSelection = !SelectedFixturePatches.IsEmpty() ? SelectedFixturePatches[0] : nullptr;
		OnSelectionChanged(NewSelection, ESelectInfo::OnMouseClick);
	}

	void SDMXControlConsoleFixturePatchList::OnActiveLayoutChanged(const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout)
	{
		RequestRefresh();
	}

	TSharedPtr<SWidget> SDMXControlConsoleFixturePatchList::OnContextMenuOpening()
	{
		ensureMsgf(CommandList.IsValid(), TEXT("Invalid command list in SDMXControlConsoleFixturePatchList. Commands should have been registered on Construction."));
		FMenuBuilder MenuBuilder(true, CommandList);

		MenuBuilder.BeginSection(NAME_None, LOCTEXT("EnableFaderGroupContextMenu", "Enable"));
		{
			MenuBuilder.AddMenuEntry
			(
				FDMXControlConsoleEditorCommands::Get().Enable,
				NAME_None,
				LOCTEXT("EnableContextMenu_Label", "Only Active"),
				FText::GetEmpty(),
				FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Fader.Unmute")
			);

			MenuBuilder.AddMenuEntry
			(
				FDMXControlConsoleEditorCommands::Get().EnableAll,
				NAME_None,
				LOCTEXT("EnableAllContextMenu_Label", "All"),
				FText::GetEmpty(),
				FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Fader.Unmute")
			);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection(NAME_None, LOCTEXT("DisableFaderGroupContextMenu", "Disable"));
		{
			MenuBuilder.AddMenuEntry
			(
				FDMXControlConsoleEditorCommands::Get().Disable,
				NAME_None,
				LOCTEXT("DisableContextMenu_Label", "Only Active"),
				FText::GetEmpty(),
				FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Fader.Mute")
			);

			MenuBuilder.AddMenuEntry
			(
				FDMXControlConsoleEditorCommands::Get().DisableAll,
				NAME_None,
				LOCTEXT("DisableAllContextMenu_Label", "All"),
				FText::GetEmpty(),
				FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Fader.Mute")
			);
		}
		MenuBuilder.EndSection();

		// Show Add Patch buttons only if the current layout is the user layout
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		if (EditorModel.IsValid() && ControlConsoleLayouts)
		{
			const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
			if (ActiveLayout && ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked())
			{
				TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> WeakFixturePatches;
				const TArray<UDMXEntityFixturePatch*> SelectedFixturePatches = GetSelectedFixturePatches();
				Algo::Transform(SelectedFixturePatches, WeakFixturePatches, [](UDMXEntityFixturePatch* FixturePatch)
					{
						return FixturePatch;
					});

				MenuBuilder.AddWidget(SNew(SDMXControlConsoleAddFixturePatchMenu, WeakFixturePatches, EditorModel.Get()), FText::GetEmpty());
			}
		}

		return MenuBuilder.MakeWidget();
	}

	void SDMXControlConsoleFixturePatchList::OnSelectionChanged(const TSharedPtr<FDMXReadOnlyFixturePatchListItem> NewSelection, ESelectInfo::Type SelectInfo)
	{
		if (!EditorModel.IsValid() || SelectInfo == ESelectInfo::Direct)
		{
			return;
		}

		// Continue only if the current layout is the default layout
		const UDMXControlConsoleEditorData* ControlConsoleEditorData = EditorModel->GetControlConsoleEditorData();
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ControlConsoleEditorData || !ActiveLayout || ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			return;
		}

		const TArray<TSharedPtr<FDMXReadOnlyFixturePatchListItem>> SelectedFixturePatches = GetSelectedItems();

		// Handle auto-grouping options
		const bool bAutoGroupActivePatches = ControlConsoleEditorData->GetAutoGroupActivePatches();
		if (bAutoGroupActivePatches)
		{
			if (SelectedFixturePatches.Num() == 1 && NewSelection.IsValid())
			{
				HandleAutoGroupSinglePatchSelection();
			}
			else if (SelectedFixturePatches.Num() > 1)
			{
				HandleAutoGroupMultiPatchSelection();
			}
		}
		else
		{
			HandleAutoGroupSinglePatchSelection();
		}

		TArray<UObject*> FaderGroupControllersToAddToSelection;
		TArray<UObject*> FaderGroupControllersToRemoveFromSelection;
		const UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
		const TArray<UDMXControlConsoleFaderGroupController*> FaderGroupControllers = ActiveLayout->GetAllFaderGroupControllers();
		for (UDMXControlConsoleFaderGroupController* FaderGroupController : FaderGroupControllers)
		{
			if (!FaderGroupController || !FaderGroupController->HasFixturePatch())
			{
				continue;
			}

			// Activate controller if one of the patches of its fader groups is selected
			const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& FaderGroups = FaderGroupController->GetFaderGroups();
			const bool bIsAnyFixturePatchSelected = Algo::AnyOf(FaderGroups,
				[SelectedFixturePatches](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup)
				{
					const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
					return SelectedFixturePatches.ContainsByPredicate(
						[FixturePatch](const TSharedPtr<FDMXReadOnlyFixturePatchListItem>& Item)
						{
							return Item.IsValid() && Item->GetFixturePatch() == FixturePatch;
						});
				});

			// Set fader group controller active if any of its fixture patches is selected
			FaderGroupController->SetIsActive(bIsAnyFixturePatchSelected);
			if (bIsAnyFixturePatchSelected)
			{
				ActiveLayout->AddToActiveFaderGroupControllers(FaderGroupController);

				const UDMXControlConsoleEditorData* EditorData = EditorModel->GetControlConsoleEditorData();
				const bool bAutoSelect = EditorData && EditorData->GetAutoSelectActivePatches();
				if (bAutoSelect)
				{
					const TArray<UDMXControlConsoleElementController*> AllElementControllers = FaderGroupController->GetAllElementControllers();
					FaderGroupControllersToAddToSelection.Append(AllElementControllers);
				}
			}
			else
			{
				ActiveLayout->RemoveFromActiveFaderGroupControllers(FaderGroupController);
				FaderGroupControllersToRemoveFromSelection.Add(FaderGroupController);
			}
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		constexpr bool bNotifySelectionChange = false;
		SelectionHandler->AddToSelection(FaderGroupControllersToAddToSelection, bNotifySelectionChange);
		SelectionHandler->RemoveFromSelection(FaderGroupControllersToRemoveFromSelection, bNotifySelectionChange);
		SelectionHandler->RemoveInvalidObjectsFromSelection();
	}

	void SDMXControlConsoleFixturePatchList::HandleAutoGroupSinglePatchSelection() const
	{
		if (!EditorModel.IsValid())
		{
			return;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ActiveLayout)
		{
			return;
		}

		TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		TArray<UObject*> FaderGroupControllersToSelect;

		// Ungroup all the grouped controllers
		const TArray<UDMXControlConsoleFaderGroupController*> FaderGroupControllers = ActiveLayout->GetAllFaderGroupControllers();
		for (UDMXControlConsoleFaderGroupController* FaderGroupController : FaderGroupControllers)
		{
			if (!FaderGroupController || !FaderGroupController->HasFixturePatch())
			{
				continue;
			}

			const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> FaderGroups = FaderGroupController->GetFaderGroups();
			if (FaderGroups.Num() <= 1)
			{
				continue;
			}

			const bool bIsControllerSelected = SelectionHandler->IsSelected(FaderGroupController);

			const int32 RowIndex = ActiveLayout->GetFaderGroupControllerRowIndex(FaderGroupController);
			int32 ColumIndex = ActiveLayout->GetFaderGroupControllerColumnIndex(FaderGroupController);
			for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
			{
				if (!FaderGroup.IsValid())
				{
					continue;
				}

				FaderGroupController->PreEditChange(nullptr);
				FaderGroupController->UnPossess(FaderGroup.Get());

				ActiveLayout->PreEditChange(nullptr);
				UDMXControlConsoleFaderGroupController* NewController = ActiveLayout->AddToLayout(FaderGroup.Get(), FaderGroup->GetFaderGroupName(), RowIndex, ColumIndex);
				ActiveLayout->PostEditChange();
				if (NewController)
				{
					NewController->Modify();
					NewController->SetIsActive(FaderGroupController->IsActive());
					NewController->SetIsExpanded(FaderGroupController->IsExpanded());
				}

				if (bIsControllerSelected)
				{
					FaderGroupControllersToSelect.Add(NewController);
				}

				ColumIndex++;
			}

			FaderGroupController->Destroy();
			FaderGroupController->PostEditChange();
		}

		SelectionHandler->AddToSelection(FaderGroupControllersToSelect);
	}

	void SDMXControlConsoleFixturePatchList::HandleAutoGroupMultiPatchSelection() const
	{
		if (!EditorModel.IsValid())
		{
			return;
		}

		UDMXControlConsoleData* ControlConsoleData = EditorModel->GetControlConsoleData();
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ControlConsoleData || !ActiveLayout)
		{
			return;
		}

		const TArray<TSharedPtr<FDMXReadOnlyFixturePatchListItem>> SelectedFixturePatches = GetSelectedItems();
		if (SelectedFixturePatches.IsEmpty())
		{
			return;
		}

		TArray<UDMXControlConsoleFaderGroup*> FaderGroupsToGroup;
		UDMXControlConsoleFaderGroupController* FirstSelectedFaderGroupController = nullptr;
		const TArray<UDMXControlConsoleFaderGroupController*> FaderGroupControllers = ActiveLayout->GetAllFaderGroupControllers();
		for (UDMXControlConsoleFaderGroupController* FaderGroupController : FaderGroupControllers)
		{
			if (!FaderGroupController || !FaderGroupController->HasFixturePatch())
			{
				continue;
			}

			// Group controller if one of the patches of its fader groups is selected
			const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> FaderGroups = FaderGroupController->GetFaderGroups();
			const bool bIsAnyFixturePatchSelected = Algo::AnyOf(FaderGroups,
				[SelectedFixturePatches](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup)
				{
					const UDMXEntityFixturePatch* FixturePatch = FaderGroup->GetFixturePatch();
					return SelectedFixturePatches.ContainsByPredicate([FixturePatch](const TSharedPtr<FDMXReadOnlyFixturePatchListItem>& Item)
						{
							return Item.IsValid() && Item->GetFixturePatch() == FixturePatch;
						});
				});

			if (bIsAnyFixturePatchSelected)
			{
				// Group all the fader groups with selected fixture patches into one controller
				if (!FirstSelectedFaderGroupController)
				{
					FirstSelectedFaderGroupController = FaderGroupController;
					continue;
				}

				// Destroy each selected fader group controller except the first one
				TArray<UDMXControlConsoleFaderGroup*> Result;
				Algo::TransformIf(FaderGroups, Result,
					[](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup)
					{
						return FaderGroup.IsValid();
					},
					[](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup)
					{
						return FaderGroup.Get();
					});

				FaderGroupsToGroup.Append(Result);
				FaderGroupController->Destroy();
			}
			else
			{
				// Ungroup all the grouped controllers without selected fixture patches
				if (FaderGroups.Num() <= 1)
				{
					continue;
				}

				const int32 RowIndex = ActiveLayout->GetFaderGroupControllerRowIndex(FaderGroupController);
				int32 ColumIndex = ActiveLayout->GetFaderGroupControllerColumnIndex(FaderGroupController);
				for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
				{
					if (!FaderGroup.IsValid())
					{
						continue;
					}

					FaderGroupController->UnPossess(FaderGroup.Get());
					ActiveLayout->AddToLayout(FaderGroup.Get(), FaderGroup->GetFaderGroupName(), RowIndex, ColumIndex);

					ColumIndex++;
				}

				FaderGroupController->Destroy();
			}
		}

		// Group all the selected fader groups in the first selected controller
		if (FirstSelectedFaderGroupController && !FaderGroupsToGroup.IsEmpty())
		{
			FirstSelectedFaderGroupController->Possess(FaderGroupsToGroup);
			const FString& UserName = FirstSelectedFaderGroupController->GenerateUserNameByFaderGroupsNames();
			FirstSelectedFaderGroupController->SetUserName(UserName);
			FirstSelectedFaderGroupController->Group();
		}
	}

	void SDMXControlConsoleFixturePatchList::OnRowClicked(const TSharedPtr<FDMXReadOnlyFixturePatchListItem> ClickedItem)
	{
		if (!EditorModel.IsValid() || !ClickedItem.IsValid())
		{
			return;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ActiveLayout)
		{
			return;
		}

		const UDMXEntityFixturePatch* FixturePatch = ClickedItem->GetFixturePatch();
		if (!FixturePatch)
		{
			return;
		}

		UDMXControlConsoleFaderGroupController* FaderGroupController = ActiveLayout->FindFaderGroupControllerByFixturePatch(FixturePatch);
		if (FaderGroupController && FaderGroupController->IsActive())
		{
			EditorModel->ScrollIntoView(FaderGroupController);
		}
	}

	void SDMXControlConsoleFixturePatchList::OnRowDoubleClicked(const TSharedPtr<FDMXReadOnlyFixturePatchListItem> ClickedItem)
	{
		if (!EditorModel.IsValid() || !ClickedItem.IsValid())
		{
			return;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ActiveLayout)
		{
			return;
		}

		const UDMXEntityFixturePatch* FixturePatch = ClickedItem->GetFixturePatch();
		if (!FixturePatch)
		{
			return;
		}

		UDMXControlConsoleFaderGroupController* FaderGroupController = ActiveLayout->FindFaderGroupControllerByFixturePatch(FixturePatch);
		if (FaderGroupController && FaderGroupController->IsActive())
		{
			FaderGroupController->SetIsExpanded(true);
		}
	}

	void SDMXControlConsoleFixturePatchList::OnEnableAllFaderGroups(bool bEnable, bool bOnlyActive) const
	{
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		if (!ControlConsoleData)
		{
			return;
		}

		const TArray<UDMXControlConsoleFaderGroup*> FaderGroups = ControlConsoleData->GetAllFaderGroups();
		for (UDMXControlConsoleFaderGroup* FaderGroup : FaderGroups)
		{
			if (!FaderGroup)
			{
				continue;
			}

			const UDMXControlConsoleFaderGroupController* Controller = Cast<UDMXControlConsoleFaderGroupController>(FaderGroup->GetFaderGroupController());
			if (bOnlyActive && Controller && !Controller->IsActive())
			{
				continue;
			}

			FaderGroup->SetEnabled(bEnable);
		}
	}

	bool SDMXControlConsoleFixturePatchList::IsAnyFaderGroupEnabled(bool bEnable, bool bOnlyActive) const
	{
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		if (!ControlConsoleData)
		{
			return false;
		}

		TArray<UDMXControlConsoleFaderGroup*> FaderGroups = ControlConsoleData->GetAllFaderGroups();
		if (bOnlyActive)
		{
			FaderGroups.RemoveAll([](const UDMXControlConsoleFaderGroup* FaderGroup)
				{
					const UDMXControlConsoleFaderGroupController* Controller = FaderGroup ? Cast<UDMXControlConsoleFaderGroupController>(FaderGroup->GetFaderGroupController()) : nullptr;
					return Controller && !Controller->IsActive();
				});
		}

		const bool bIsAnyFaderGroupEnabled = Algo::AnyOf(FaderGroups,
			[bEnable](UDMXControlConsoleFaderGroup* FaderGroup)
			{
				return FaderGroup && FaderGroup->IsEnabled() == bEnable;
			});

		return bIsAnyFaderGroupEnabled;
	}

	ECheckBoxState SDMXControlConsoleFixturePatchList::GetGlobalFaderGroupsEnabledCheckBoxState() const
	{
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		if (ControlConsoleData)
		{
			// Get all patched Fader Groups
			TArray<UDMXControlConsoleFaderGroup*> PatchedFaderGroups = ControlConsoleData->GetAllFaderGroups();
			PatchedFaderGroups.RemoveAll([](const UDMXControlConsoleFaderGroup* FaderGroup)
				{
					return FaderGroup && !FaderGroup->HasFixturePatch();
				});

			const bool bAreAllFaderGroupsEnabled = Algo::AllOf(PatchedFaderGroups,
				[](const UDMXControlConsoleFaderGroup* FaderGroup)
				{
					return FaderGroup && FaderGroup->IsEnabled();
				});

			if (bAreAllFaderGroupsEnabled)
			{
				return ECheckBoxState::Checked;
			}

			const bool bIsAnyFaderGroupEnabled = Algo::AnyOf(PatchedFaderGroups,
				[](const UDMXControlConsoleFaderGroup* FaderGroup)
				{
					return FaderGroup && FaderGroup->IsEnabled();
				});

			return bIsAnyFaderGroupEnabled ? ECheckBoxState::Undetermined : ECheckBoxState::Unchecked;
		}

		return ECheckBoxState::Undetermined;
	}

	void SDMXControlConsoleFixturePatchList::OnGlobalFaderGroupsEnabledCheckBoxStateChanged(ECheckBoxState CheckBoxState)
	{
		const UDMXControlConsoleData* ControlConsoleData = EditorModel.IsValid() ? EditorModel->GetControlConsoleData() : nullptr;
		if (!ControlConsoleData)
		{
			return;
		}

		const TArray<UDMXControlConsoleFaderGroup*> FaderGroups = ControlConsoleData->GetAllFaderGroups();
		for (UDMXControlConsoleFaderGroup* FaderGroup : FaderGroups)
		{
			if (FaderGroup && FaderGroup->HasFixturePatch())
			{
				const bool bIsEnabled = CheckBoxState == ECheckBoxState::Checked;
				FaderGroup->SetEnabled(bIsEnabled);
			}
		}
	}

	void SDMXControlConsoleFixturePatchList::SetShowMode(EDMXReadOnlyFixturePatchListShowMode NewShowMode)
	{
		ShowMode = NewShowMode;
		RequestRefresh();
	}

	bool SDMXControlConsoleFixturePatchList::IsUsingShowMode(EDMXReadOnlyFixturePatchListShowMode InShowMode) const
	{
		return ShowMode == InShowMode;
	}
}

#undef LOCTEXT_NAMESPACE
