// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFaderGroupControllerToolbar.h"

#include "Algo/AllOf.h"
#include "Algo/AnyOf.h"
#include "DMXControlConsoleEditorData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXEditorStyle.h"
#include "Filters/SFilterSearchBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layouts/Controllers/DMXControlConsoleElementController.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/DMXControlConsoleFaderGroupControllerModel.h"
#include "Models/Filter/DMXControlConsoleGlobalFilterModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Styling/SlateColor.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorFaderGroupControllerComboBox.h"
#include "Widgets/SDMXControlConsoleEditorFaderGroupControllerPanel.h"
#include "Widgets/SDMXControlConsoleEditorFaderGroupControllerTag.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFaderGroupControllerToolbar"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorFaderGroupControllerToolbar::Construct(const FArguments& InArgs, const TWeakPtr<FDMXControlConsoleFaderGroupControllerModel>& InFaderGroupControllerModel, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, cannot create fader group controller toolbar correctly.")))
		{
			return;
		}

		if (!ensureMsgf(InFaderGroupControllerModel.IsValid(), TEXT("Invalid fader group controller model, cannot create fader group controller toolbar correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
		WeakFaderGroupControllerModel = InFaderGroupControllerModel;

		IsExpandedViewModeEnabledDelegate = InArgs._IsExpandedViewModeEnabled;

		ChildSlot
			[
				SNew(SHorizontalBox)

				// Fader Group tag section
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(6.f, 0.f, 4.f, 0.f)
				.AutoWidth()
				[
					SNew(SDMXControlConsoleEditorFaderGroupControllerTag, WeakFaderGroupControllerModel.Pin())
				]

				// Fixture Patch ComboBox section
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.MaxWidth(120.f)
				.Padding(4.f, 8.f)
				.AutoWidth()
				[
					SAssignNew(ControllerComboBox, SDMXControlConsoleEditorFaderGroupControllerComboBox, WeakFaderGroupControllerModel.Pin(), EditorModel.Get())
				]

				// Expand Arrow button section
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.MaxWidth(20.f)
				.AutoWidth()
				[
					SAssignNew(ExpandArrowButton, SDMXControlConsoleEditorExpandArrowButton)
					.OnExpandClicked(InArgs._OnExpanded)
					.ToolTipText(LOCTEXT("FaderGroupExpandArrowButton_Tooltip", "Switch between Collapsed/Expanded view mode"))
				]

				// Info Combo button section
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(4.f, 8.f, 0.f, 8.f)
				[
					SNew(SComboButton)
					.ButtonStyle(&FAppStyle::Get().GetWidgetStyle<FButtonStyle>("SimpleButton"))
					.ContentPadding(2.f)
					.ForegroundColor(FSlateColor::UseStyle())
					.HasDownArrow(false)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.OnGetMenuContent(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::GenerateFaderGroupControllerInfoMenuWidget)
					.ToolTipText(LOCTEXT("InfoButtonToolTipText", "Info"))
					.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::GetExpandedViewModeVisibility))
					.ButtonContent()
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FAppStyle::Get().GetBrush("Icons.Info"))
					]
				]

				//Searchbox section
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.MaxWidth(250.f)
				.Padding(4.f, 8.f)
				[
					SNew(SBox)
					.VAlign(VAlign_Center)
					.WidthOverride(150.f)
					.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::GetSearchBoxVisibility))
					[
						SAssignNew(ToolbarSearchBox, SFilterSearchBox)
						.DelayChangeNotificationsWhileTyping(true)
						.ShowSearchHistory(true)
						.OnTextChanged(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::OnSearchTextChanged)
						.HintText(LOCTEXT("SearchBarHintText", "Search"))
						.ToolTipText(LOCTEXT("SearchBarTooltip", "Searches for Fader Name, Attributes, Fixture ID, Universe or Patch. Examples:\n\n* FaderName\n* Dimmer\n* Pan, Tilt\n* 1\n* 1.\n* 1.1\n* Universe 1\n* Uni 1-3\n* Uni 1, 3\n* Uni 1, 4-5'."))
					]
				]

				//Settings section
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.Padding(4.f, 8.f)
				[
					SNew(SComboButton)
					.ContentPadding(0.f)
					.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
					.OnGetMenuContent(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::GenerateSettingsMenuWidget)
					.HasDownArrow(true)
					.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::GetExpandedViewModeVisibility))
					.ButtonContent()
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FAppStyle::Get().GetBrush("Icons.Settings"))
					]
				]
			];

		RestoreFaderGroupControllerFilter();
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorFaderGroupControllerToolbar::GenerateSettingsMenuWidget()
	{
		constexpr bool bShouldCloseWindowAfterClosing = true;
		FMenuBuilder MenuBuilder(bShouldCloseWindowAfterClosing, nullptr);

		MenuBuilder.BeginSection("Options", LOCTEXT("FaderGroupViewOptionsCategory", "Options"));
		{
			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("InfoLabel", "Info"),
				FText::GetEmpty(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Info"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::OnGetInfoPanel)
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("SelectAllLabel", "Select All"),
				FText::GetEmpty(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::OnSelectAllElementControllers)
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("DuplicateLabel", "Duplicate"),
				FText::GetEmpty(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Duplicate"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::OnDuplicateFaderGroupController), 
					FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::CanDuplicateFaderGroupController), 
					FIsActionChecked(), 
					FIsActionButtonVisible::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::CanDuplicateFaderGroupController)
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("RemoveLabel", "Remove"),
				FText::GetEmpty(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::OnRemoveFaderGroupController),
					FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::CanRemoveFaderGroupController)
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);
		}
		MenuBuilder.EndSection();

		MenuBuilder.BeginSection("Controls", LOCTEXT("FaderGroupViewControlsCategory", "Controls"));
		{
			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("GroupLabel", "Group"),
				FText::GetEmpty(),
				FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Group"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::OnGroupFaderGroupControllers),
					FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::CanGroupFaderGroupControllers),
					FIsActionChecked(),
					FIsActionButtonVisible::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::CanGroupFaderGroupControllers)
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("UngroupLabel", "Ungroup"),
				FText::GetEmpty(),
				FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.Ungroup"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::OnUngroupFaderGroupControllers),
					FCanExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::CanUngroupFaderGroupControllers),
					FIsActionChecked(),
					FIsActionButtonVisible::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::CanUngroupFaderGroupControllers)
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("ResetToDefaultLabel", "Reset To Default"),
				FText::GetEmpty(),
				FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.ResetToDefault"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::OnResetFaderGroupController)
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("LockLabel", "Lock"),
				FText::GetEmpty(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Lock"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::OnLockFaderGroupController, true),
					FCanExecuteAction::CreateLambda([this]() { return GetFaderGroupController() && !GetFaderGroupController()->IsLocked(); }),
					FIsActionChecked(),
					FIsActionButtonVisible::CreateLambda([this]() { return GetFaderGroupController() && !GetFaderGroupController()->IsLocked(); })
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);

			MenuBuilder.AddMenuEntry
			(
				LOCTEXT("UnlockLabel", "Unlock"),
				FText::GetEmpty(),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Unlock"),
				FUIAction
				(
					FExecuteAction::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerToolbar::OnLockFaderGroupController, false),
					FCanExecuteAction::CreateLambda([this]() { return GetFaderGroupController() && GetFaderGroupController()->IsLocked(); }),
					FIsActionChecked(),
					FIsActionButtonVisible::CreateLambda([this]() { return GetFaderGroupController() && GetFaderGroupController()->IsLocked(); })
				),
				NAME_None,
				EUserInterfaceActionType::Button
			);
		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	UDMXControlConsoleFaderGroupController* SDMXControlConsoleEditorFaderGroupControllerToolbar::GetFaderGroupController() const
	{
		const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = WeakFaderGroupControllerModel.Pin();
		return FaderGroupControllerModel.IsValid() ? FaderGroupControllerModel->GetFaderGroupController() : nullptr;
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorFaderGroupControllerToolbar::GenerateFaderGroupControllerInfoMenuWidget()
	{
		if (WeakFaderGroupControllerModel.IsValid())
		{
			constexpr bool bShouldCloseWindowAfterClosing = false;
			FMenuBuilder MenuBuilder(bShouldCloseWindowAfterClosing, nullptr);

			MenuBuilder.BeginSection("Info", LOCTEXT("FaderGroupInfoMenuCategory", "Info"));
			{
				const TSharedRef<SWidget> FaderGroupInfoPanel =
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(8.f)
					.MaxWidth(116.f)
					[
						SNew(SDMXControlConsoleEditorFaderGroupControllerPanel, WeakFaderGroupControllerModel.Pin())
					];

				MenuBuilder.AddWidget(FaderGroupInfoPanel, FText::GetEmpty());
			}
			MenuBuilder.EndSection();

			return MenuBuilder.MakeWidget();
		}

		return SNullWidget::NullWidget;
	}

	void SDMXControlConsoleEditorFaderGroupControllerToolbar::RestoreFaderGroupControllerFilter()
	{
		const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = WeakFaderGroupControllerModel.Pin();
		if (!FaderGroupControllerModel.IsValid())
		{
			return;
		}

		if (const UDMXControlConsoleFaderGroup* FaderGroup = FaderGroupControllerModel->GetFirstAvailableFaderGroup())
		{
			const FString& FilterString = FaderGroup->FilterString;
			const FText FilterText = FText::FromString(FilterString);
			ToolbarSearchBox->SetText(FilterText);
		}
	}

	void SDMXControlConsoleEditorFaderGroupControllerToolbar::OnSearchTextChanged(const FText& SearchText)
	{
		const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = WeakFaderGroupControllerModel.Pin();
		const UDMXControlConsoleEditorData* EditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (!FaderGroupControllerModel.IsValid() || !EditorData)
		{
			return;
		}

		const UDMXControlConsoleFaderGroupController* FaderGroupController = FaderGroupControllerModel->GetFaderGroupController();
		if (!FaderGroupController)
		{
			return;
		}

		const TSharedRef<FDMXControlConsoleGlobalFilterModel> GlobalFilterModel = EditorModel->GetGlobalFilterModel();
		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& FaderGroups = FaderGroupController->GetFaderGroups();
		for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
		{
			if (FaderGroup.IsValid())
			{
				GlobalFilterModel->SetFaderGroupFilter(FaderGroup.Get(), SearchText.ToString());
			}
		}

		if (!SearchText.IsEmpty() && EditorData->GetAutoSelectFilteredElements())
		{
			TArray<UObject*> ElementControllersToSelect;
			TArray<UObject*> ElementControllersToUnselect;
			const TArray<UDMXControlConsoleElementController*> AllElementControllers = FaderGroupController->GetAllElementControllers();
			for (UDMXControlConsoleElementController* ElementController : AllElementControllers)
			{
				if (!ElementController)
				{
					continue;
				}

				if (ElementController->IsMatchingFilter())
				{
					ElementControllersToSelect.Add(ElementController);
				}
				else
				{
					ElementControllersToUnselect.Add(ElementController);
				}
			}

			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
			constexpr bool bNotifySelection = false;
			SelectionHandler->AddToSelection(ElementControllersToSelect, bNotifySelection);
			SelectionHandler->RemoveFromSelection(ElementControllersToUnselect);
		}
	}

	void SDMXControlConsoleEditorFaderGroupControllerToolbar::OnGetInfoPanel()
	{
		FWidgetPath WidgetPath;
		FSlateApplication::Get().GeneratePathToWidgetChecked(AsShared(), WidgetPath);

		FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, GenerateFaderGroupControllerInfoMenuWidget(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
	}

	void SDMXControlConsoleEditorFaderGroupControllerToolbar::OnSelectAllElementControllers() const
	{
		UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
		if (EditorModel.IsValid() && FaderGroupController)
		{
			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
			constexpr bool bNotifySelectionChange = true;
			SelectionHandler->AddAllElementControllersFromFaderGroupControllerToSelection(FaderGroupController, bNotifySelectionChange);
		}
	}

	void SDMXControlConsoleEditorFaderGroupControllerToolbar::OnDuplicateFaderGroupController()  const
	{
		if (!EditorModel.IsValid())
		{
			return;
		}

		UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ActiveLayout || !FaderGroupController || !ActiveLayout->ContainsFaderGroupController(FaderGroupController))
		{
			return;
		}

		const FScopedTransaction DuplicateFaderGroupControllerOptionTransaction(LOCTEXT("DuplicateFaderGroupControllerOptionTransaction", "Fader Group duplicated"));
		TArray<UDMXControlConsoleFaderGroup*> DuplicatedFaderGroups;
		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& FaderGroups = FaderGroupController->GetFaderGroups();
		for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
		{
			if (FaderGroup.IsValid())
			{
				FaderGroup->PreEditChange(nullptr);
				UDMXControlConsoleFaderGroup* DuplicatedFaderGroup = FaderGroup->Duplicate();
				FaderGroup->PostEditChange();

				DuplicatedFaderGroups.Add(DuplicatedFaderGroup);
			}
		}

		if (DuplicatedFaderGroups.IsEmpty())
		{
			return;
		}

		UDMXControlConsoleEditorGlobalLayoutRow& OwnerLayoutRow = FaderGroupController->GetOwnerLayoutRowChecked();
		const int32 Index = OwnerLayoutRow.GetIndex(FaderGroupController);
		const FString DuplicatedUserName = FaderGroupController->GetUserName() + TEXT("_Duplicated");
		
		OwnerLayoutRow.PreEditChange(nullptr);
		UDMXControlConsoleFaderGroupController* DuplicatedController = OwnerLayoutRow.CreateFaderGroupController(DuplicatedFaderGroups, DuplicatedUserName, Index + 1);
		OwnerLayoutRow.PostEditChange();
		if (DuplicatedController)
		{
			DuplicatedController->Modify();
			DuplicatedController->SetIsActive(true);

			ActiveLayout->PreEditChange(nullptr);
			ActiveLayout->AddToActiveFaderGroupControllers(FaderGroupController);
			ActiveLayout->PostEditChange();
		}
	}

	bool SDMXControlConsoleEditorFaderGroupControllerToolbar::CanDuplicateFaderGroupController() const
	{
		const UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
		return FaderGroupController && !FaderGroupController->HasFixturePatch();
	}

	void SDMXControlConsoleEditorFaderGroupControllerToolbar::OnRemoveFaderGroupController() const
	{
		UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
		if (!EditorModel.IsValid() || !FaderGroupController)
		{
			return;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ControlConsoleLayouts || !ActiveLayout || ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			return;
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		SelectionHandler->RemoveFromSelection(FaderGroupController);

		const FScopedTransaction RemoveFaderGroupControllerOptionTransaction(LOCTEXT("RemoveFaderGroupControllerOptionTransaction", "Remove Fader Group"));

		// Destroy all unpatched fader groups in the controller
		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& FaderGroups = FaderGroupController->GetFaderGroups();
		for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
		{
			if (FaderGroup.IsValid() && !FaderGroup->HasFixturePatch())
			{
				FaderGroup->Modify();
				FaderGroup->Destroy();
			}
		}

		FaderGroupController->PreEditChange(nullptr);
		FaderGroupController->Destroy();
		FaderGroupController->PostEditChange();

		ActiveLayout->PreEditChange(nullptr);
		ActiveLayout->RemoveFromActiveFaderGroupControllers(FaderGroupController);
		ActiveLayout->ClearEmptyLayoutRows();
		ActiveLayout->PostEditChange();
	}

	bool SDMXControlConsoleEditorFaderGroupControllerToolbar::CanRemoveFaderGroupController() const
	{
		const UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
		if (!EditorModel.IsValid() || !FaderGroupController)
		{
			return false;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		if (!ControlConsoleLayouts)
		{
			return false;
		}

		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts->GetActiveLayout();
		return IsValid(ActiveLayout) && ActiveLayout != &ControlConsoleLayouts->GetDefaultLayoutChecked();
	}

	void SDMXControlConsoleEditorFaderGroupControllerToolbar::OnGroupFaderGroupControllers() const
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

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		UDMXControlConsoleFaderGroupController* FirstSelectedFaderGroupController = SelectionHandler->GetFirstSelectedFaderGroupController();
		if (!FirstSelectedFaderGroupController)
		{
			return;
		}

		const FScopedTransaction GroupFaderGroupControllersOptionTransaction(LOCTEXT("GroupFaderGroupControllersOptionTransaction", "Group Fader Groups"));

		TArray<UObject*> FaderGroupControllersToUnselect;
		TArray<UDMXControlConsoleFaderGroup*> FaderGroupsToGroup;
		const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupControllers = SelectionHandler->GetSelectedFaderGroupControllers();
		for (const TWeakObjectPtr<UObject> SelectFaderGroupControllerObject : SelectedFaderGroupControllers)
		{
			UDMXControlConsoleFaderGroupController* SelectedFaderGroupController = Cast<UDMXControlConsoleFaderGroupController>(SelectFaderGroupControllerObject);
			if (!SelectedFaderGroupController || SelectedFaderGroupController == FirstSelectedFaderGroupController)
			{
				continue;
			}

			// Destroy each selected fader group controller except the first one
			if(SelectedFaderGroupController->HasFixturePatch())
			{ 
				const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>>& FaderGroups = SelectedFaderGroupController->GetFaderGroups();
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
				FaderGroupControllersToUnselect.Add(SelectedFaderGroupController);

				ActiveLayout->PreEditChange(nullptr);
				ActiveLayout->RemoveFromActiveFaderGroupControllers(SelectedFaderGroupController);
				ActiveLayout->PostEditChange();

				SelectedFaderGroupController->PreEditChange(nullptr);
				SelectedFaderGroupController->Destroy();
				SelectedFaderGroupController->PostEditChange();
			}
		}

		// Group all the selected fader groups in the first selected controller
		FirstSelectedFaderGroupController->PreEditChange(nullptr);

		FirstSelectedFaderGroupController->Possess(FaderGroupsToGroup);
		const FString UserName = FirstSelectedFaderGroupController->GenerateUserNameByFaderGroupsNames();
		FirstSelectedFaderGroupController->SetUserName(UserName);
		FirstSelectedFaderGroupController->Group();

		FirstSelectedFaderGroupController->PostEditChange();

		SelectionHandler->RemoveFromSelection(FaderGroupControllersToUnselect);
		EditorModel->RequestUpdateEditorModel();
	}

	void SDMXControlConsoleEditorFaderGroupControllerToolbar::OnUngroupFaderGroupControllers() const
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

		const FScopedTransaction UngroupFaderGroupControllersOptionTransaction(LOCTEXT("UngroupFaderGroupControllersOptionTransaction", "Ungroup Fader Groups"));

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		TArray<UObject*> FaderGroupControllersToSelect;
		TArray<UObject*> FaderGroupControllersToUnselect;
		const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroupControllers = SelectionHandler->GetSelectedFaderGroupControllers();
		for (const TWeakObjectPtr<UObject> SelectFaderGroupControllerObject : SelectedFaderGroupControllers)
		{
			UDMXControlConsoleFaderGroupController* SelectedFaderGroupController = Cast<UDMXControlConsoleFaderGroupController>(SelectFaderGroupControllerObject);
			if (!SelectedFaderGroupController || SelectedFaderGroupController->GetFaderGroups().Num() <= 1)
			{
				continue;
			}

			// Create a new controller for each fader group in each selected controller
			UDMXControlConsoleEditorGlobalLayoutRow& OwnerLayoutRow = SelectedFaderGroupController->GetOwnerLayoutRowChecked();
			const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> FaderGroups = SelectedFaderGroupController->GetFaderGroups();
			for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
			{
				if (!FaderGroup.IsValid())
				{
					continue;
				}
				
				SelectedFaderGroupController->PreEditChange(nullptr);
				SelectedFaderGroupController->UnPossess(FaderGroup.Get());

				int32 ColumIndex = ActiveLayout->GetFaderGroupControllerColumnIndex(SelectedFaderGroupController);
				OwnerLayoutRow.PreEditChange(nullptr);
				UDMXControlConsoleFaderGroupController* NewController = OwnerLayoutRow.CreateFaderGroupController(FaderGroup.Get(), FaderGroup->GetFaderGroupName(), ColumIndex);
				OwnerLayoutRow.PostEditChange();
				if (NewController)
				{
					ColumIndex++;
					NewController->Modify();
					NewController->SetIsExpanded(SelectedFaderGroupController->IsExpanded());
					NewController->SetIsActive(true);

					ActiveLayout->PreEditChange(nullptr);
					ActiveLayout->AddToActiveFaderGroupControllers(NewController);
					ActiveLayout->PostEditChange();

					FaderGroupControllersToSelect.Add(NewController);
				}
			}

			SelectedFaderGroupController->Destroy();
			SelectedFaderGroupController->PostEditChange();

			FaderGroupControllersToUnselect.Add(SelectedFaderGroupController);
		}

		constexpr bool bNotifySelectionChange = false;
		SelectionHandler->RemoveFromSelection(FaderGroupControllersToUnselect, bNotifySelectionChange);
		SelectionHandler->AddToSelection(FaderGroupControllersToSelect);
	}

	bool SDMXControlConsoleEditorFaderGroupControllerToolbar::CanGroupFaderGroupControllers() const
	{
		if (!EditorModel.IsValid())
		{
			return false;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ActiveLayout || ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			return false;
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		const TArray<TWeakObjectPtr<UObject>>& SelectedFaderGroupControllers = SelectionHandler->GetSelectedFaderGroupControllers();
		if (SelectedFaderGroupControllers.Num() <= 1)
		{
			return false;
		}

		// Group selected elements only if they all belong to the same fader group
		const bool bAreAllControllersPatched = Algo::AllOf(SelectedFaderGroupControllers,
			[](const TWeakObjectPtr<UObject>& FaderGroupControllerObject)
			{
				const UDMXControlConsoleFaderGroupController* FaderGroupController = Cast<UDMXControlConsoleFaderGroupController>(FaderGroupControllerObject);
				return FaderGroupController && FaderGroupController->HasFixturePatch();
			});

		return bAreAllControllersPatched;
	}

	bool SDMXControlConsoleEditorFaderGroupControllerToolbar::CanUngroupFaderGroupControllers() const
	{
		if (!EditorModel.IsValid())
		{
			return false;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ActiveLayout || ActiveLayout == &ControlConsoleLayouts->GetDefaultLayoutChecked())
		{
			return false;
		}

		// Ungroup only if one of the selected controllers has multiple elements
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		const TArray<TWeakObjectPtr<UObject>>& SelectedFaderGroupControllers = SelectionHandler->GetSelectedFaderGroupControllers();
		const bool bHasAnySelectedControllerMultipleFaderGroups = Algo::AnyOf(SelectedFaderGroupControllers, 
			[](const TWeakObjectPtr<UObject>& FaderGroupControllerObject)
			{
				const UDMXControlConsoleFaderGroupController* FaderGroupController = Cast<UDMXControlConsoleFaderGroupController>(FaderGroupControllerObject);
				return FaderGroupController && FaderGroupController->GetFaderGroups().Num() > 1;
			});

		return bHasAnySelectedControllerMultipleFaderGroups;
	}

	void SDMXControlConsoleEditorFaderGroupControllerToolbar::OnResetFaderGroupController() const
	{
		UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
		if (!FaderGroupController)
		{
			return;
		}

		const FScopedTransaction ResetFaderGroupControllerOptionTransaction(LOCTEXT("ResetFaderGroupControllerOptionTransaction", "Fader Group reset to default"));
		const TArray<UDMXControlConsoleElementController*>& ElementControllers = FaderGroupController->GetElementControllers();
		for (UDMXControlConsoleElementController* ElementController : ElementControllers)
		{
			if (!ElementController)
			{
				continue;
			}

			// Ensure that each fader in the controller is registered to the transaction
			for (UDMXControlConsoleFaderBase* Fader : ElementController->GetFaders())
			{
				if (Fader)
				{
					Fader->Modify();
				}
			}
			
			ElementController->PreEditChange(nullptr);
			ElementController->ResetToDefault();
			ElementController->PostEditChange();
		}
	}

	void SDMXControlConsoleEditorFaderGroupControllerToolbar::OnLockFaderGroupController(bool bLock) const
	{
		const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = WeakFaderGroupControllerModel.Pin();
		UDMXControlConsoleFaderGroupController* FaderGroupController = FaderGroupControllerModel.IsValid() ? FaderGroupControllerModel->GetFaderGroupController() : nullptr;
		if (!FaderGroupController)
		{
			return;
		}

		const FScopedTransaction LockFaderGroupControllerOptionTransaction(LOCTEXT("LockFaderGroupControllerOptionTransaction", "Edit Fader Group lock state"));
		FaderGroupController->PreEditChange(UDMXControlConsoleFaderGroupController::StaticClass()->FindPropertyByName(UDMXControlConsoleFaderGroupController::GetIsLockedPropertyName()));
		FaderGroupController->SetLocked(bLock);
		FaderGroupController->PostEditChange();
	}

	EVisibility SDMXControlConsoleEditorFaderGroupControllerToolbar::GetExpandedViewModeVisibility() const
	{
		const bool bIsVisible = 
			IsExpandedViewModeEnabledDelegate.IsBound() && 
			IsExpandedViewModeEnabledDelegate.Execute();

		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorFaderGroupControllerToolbar::GetSearchBoxVisibility() const
	{
		// Visible if the toolbar is expanded
		if (!IsExpandedViewModeEnabledDelegate.IsBound() || !IsExpandedViewModeEnabledDelegate.Execute())
		{
			return EVisibility::Collapsed;
		}
		
		// Visible if the filter string is not empty
		const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel> FaderGroupControllerModel = WeakFaderGroupControllerModel.Pin();
		const UDMXControlConsoleFaderGroup* FaderGroup = FaderGroupControllerModel.IsValid() ? FaderGroupControllerModel->GetFirstAvailableFaderGroup() : nullptr;
		if (FaderGroup && !FaderGroup->FilterString.IsEmpty())
		{
			return EVisibility::Visible;
		}
		
		// Visible if the controller has more than the specified number of element controllers
		constexpr int32 ElementControllersNumLimit = 3;
		const bool bIsVisible =
			FaderGroupControllerModel.IsValid() &&
			FaderGroupControllerModel->GetMatchingFilterElementControllersOnly().Num() > ElementControllersNumLimit;

		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}
}

#undef LOCTEXT_NAMESPACE
