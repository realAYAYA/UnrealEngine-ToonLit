// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFaderGroupControllerView.h"

#include "Algo/Find.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "DMXControlConsoleRawFader.h"
#include "Framework/Application/SlateApplication.h"
#include "Layouts/Controllers/DMXControlConsoleElementController.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/Controllers/DMXControlConsoleMatrixCellController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/DMXControlConsoleElementControllerModel.h"
#include "Models/DMXControlConsoleFaderGroupControllerModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "Views/SDMXControlConsoleEditorElementControllerView.h"
#include "Views/SDMXControlConsoleEditorMatrixCellControllerView.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorAddButton.h"
#include "Widgets/SDMXControlConsoleEditorExpandArrowButton.h"
#include "Widgets/SDMXControlConsoleEditorFaderGroupControllerPanel.h"
#include "Widgets/SDMXControlConsoleEditorFaderGroupControllerToolbar.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFaderGroupControllerView"

namespace UE::DMX::Private
{
	namespace DMXControlConsoleEditorFaderGroupControllerView
	{
		namespace Private
		{
			constexpr float CollapsedViewModeHeight = 280.f;
			constexpr float ExpandedViewModeHeight = 360.f;
		}
	}

	SDMXControlConsoleEditorFaderGroupControllerView::SDMXControlConsoleEditorFaderGroupControllerView()
		: ViewMode(EDMXControlConsoleEditorViewMode::Expanded)
	{}

	void SDMXControlConsoleEditorFaderGroupControllerView::Construct(const FArguments& InArgs, const TSharedPtr<FDMXControlConsoleFaderGroupControllerModel>& InFaderGroupControllerModel, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, cannot create fader group controller view correctly.")))
		{
			return;
		}

		if (!ensureMsgf(InFaderGroupControllerModel.IsValid(), TEXT("Invalid model, cannot create fader group controller view correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
		FaderGroupControllerModel = InFaderGroupControllerModel;

		EditorModel->GetOnEditorModelUpdated().AddSP(this, &SDMXControlConsoleEditorFaderGroupControllerView::OnElementControllerAdded);
		EditorModel->GetOnEditorModelUpdated().AddSP(this, &SDMXControlConsoleEditorFaderGroupControllerView::OnElementControllerRemoved);
		
		UDMXControlConsoleFaderGroupController* FaderGroupController = FaderGroupControllerModel->GetFaderGroupController();
		if (FaderGroupController)
		{
			FaderGroupController->GetOnFaderGroupControllerExpanded().AddSP(this, &SDMXControlConsoleEditorFaderGroupControllerView::UpdateExpansionState);
			FaderGroupController->GetOnControllerGrouped().AddSP(this, &SDMXControlConsoleEditorFaderGroupControllerView::OnFaderGroupControllerGrouped);
			FaderGroupController->GetOnFixturePatchChanged().AddSP(this, &SDMXControlConsoleEditorFaderGroupControllerView::OnFaderGroupControllerFixturePatchChanged);
		}

		UDMXControlConsoleEditorData* EditorData = EditorModel->GetControlConsoleEditorData();
		if (EditorData)
		{
			EditorData->GetOnFaderGroupsViewModeChanged().AddSP(this, &SDMXControlConsoleEditorFaderGroupControllerView::OnViewModeChanged);
		}

		ChildSlot
			[
				SNew(SBorder)
				.BorderBackgroundColor(this, &SDMXControlConsoleEditorFaderGroupControllerView::GetFaderGroupControllerViewBorderColor)
				.BorderImage(this, &SDMXControlConsoleEditorFaderGroupControllerView::GetFaderGroupControllerViewBorderImage)
				[
					SNew(SBorder)
					.BorderBackgroundColor(FLinearColor(0.01f, 0.01f, 0.01f, 1.f))
					.BorderImage(this, &SDMXControlConsoleEditorFaderGroupControllerView::GetFaderGroupControllerViewBorderImage)
					[
						SNew(SBorder)
						.BorderImage(this, &SDMXControlConsoleEditorFaderGroupControllerView::GetFaderGroupControllerViewBackgroundBorderImage)
						.Padding(6.f)
						[
							SNew(SBox)
							.MinDesiredHeight(TAttribute<FOptionalSize>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerView::GetFaderGroupControllerViewHeightByFadersViewMode))
							[
								SNew(SHorizontalBox)

								// Group Color Tag section
								+SHorizontalBox::Slot()
								.AutoWidth()
								.Padding(-2.f, -4.f, 2.f, -4.f)
								[
									SNew(SImage)
									.DesiredSizeOverride(FVector2D(2.f))
									.Image(FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.FaderGroupTag"))
									.ColorAndOpacity(this, &SDMXControlConsoleEditorFaderGroupControllerView::GetFaderGroupControllerViewBorderColor)
									.Visibility(this, &SDMXControlConsoleEditorFaderGroupControllerView::GetGroupColorTagImageVisibility)
								]

								+ SHorizontalBox::Slot()
								.HAlign(HAlign_Center)
								.AutoWidth()
								[
									SNew(SVerticalBox)

									// Toolbar section
									+ SVerticalBox::Slot()
									.HAlign(HAlign_Left)
									.VAlign(VAlign_Top)
									.AutoHeight()
									[
										SAssignNew(FaderGroupControllerToolbar, SDMXControlConsoleEditorFaderGroupControllerToolbar, FaderGroupControllerModel, EditorModel.Get())
										.OnExpanded(this, &SDMXControlConsoleEditorFaderGroupControllerView::OnExpandArrowClicked)
										.IsExpandedViewModeEnabled(this, &SDMXControlConsoleEditorFaderGroupControllerView::IsCurrentViewMode, EDMXControlConsoleEditorViewMode::Expanded)
									]

									// Core section
									+ SVerticalBox::Slot()
									[
										SNew(SHorizontalBox)

										// Fader Group Core section
										+ SHorizontalBox::Slot()
										.Padding(20.f, 20.f, 8.f, 8.f)
										.MaxWidth(116.f)
										[
											SNew(SDMXControlConsoleEditorFaderGroupControllerPanel, FaderGroupControllerModel)
											.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerView::GetViewModeVisibility, EDMXControlConsoleEditorViewMode::Collapsed))
										]

										// Faders widget section
										+ SHorizontalBox::Slot()
										.HAlign(HAlign_Left)
										.VAlign(VAlign_Center)
										.Padding(4.f, 2.f)
										.AutoWidth()
										[
											GenerateElementControllersWidget()
										]
									]
								]
							]
						]
					]
				]
			];

		UpdateExpansionState();
	}

	UDMXControlConsoleFaderGroupController* SDMXControlConsoleEditorFaderGroupControllerView::GetFaderGroupController() const
	{
		return FaderGroupControllerModel.IsValid() ? FaderGroupControllerModel->GetFaderGroupController() : nullptr;
	}

	FReply SDMXControlConsoleEditorFaderGroupControllerView::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
			if (!FaderGroupController || !EditorModel.IsValid())
			{
				return FReply::Handled();
			}
			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();

			if (MouseEvent.IsLeftShiftDown())
			{
				SelectionHandler->Multiselect(FaderGroupController);
			}
			else if (MouseEvent.IsControlDown())
			{
				if (IsSelected())
				{
					SelectionHandler->RemoveFromSelection(FaderGroupController);
				}
				else
				{
					SelectionHandler->AddToSelection(FaderGroupController);
				}
			}
			else
			{
				constexpr bool bNotifySelectionChange = false;
				SelectionHandler->ClearSelection(bNotifySelectionChange);
				SelectionHandler->AddToSelection(FaderGroupController);
			}
		}

		if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && FaderGroupControllerToolbar.IsValid())
		{
			const FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
			FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, FaderGroupControllerToolbar->GenerateSettingsMenuWidget(),
				FSlateApplication::Get().GetCursorPos(),
				FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
		}

		return FReply::Handled();
	}

	FReply SDMXControlConsoleEditorFaderGroupControllerView::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> ExpandArrowButton = GetExpandArrowButton();
			if (ExpandArrowButton.IsValid())
			{
				ExpandArrowButton->ToggleExpandArrow();

				return FReply::Handled();
			}
		}

		return FReply::Unhandled();
	}

	void SDMXControlConsoleEditorFaderGroupControllerView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
		if (!ensureMsgf(FaderGroupController, TEXT("Invalid fader group, cannot update fader group view state correctly.")))
		{
			return;
		}

		const TArray<UDMXControlConsoleElementController*>& ElementControllers = FaderGroupController->GetElementControllers();
		if (ElementControllers.Num() == ElementControllerWidgets.Num())
		{
			return;
		}

		if (ElementControllers.Num() > ElementControllerWidgets.Num())
		{
			OnElementControllerAdded();
		}
		else
		{
			OnElementControllerRemoved();
		}
	}

	TSharedRef<SWidget> SDMXControlConsoleEditorFaderGroupControllerView::GenerateElementControllersWidget()
	{
		const TSharedRef<SWidget> ElementsWidget =
			SNew(SHorizontalBox)
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerView::GetElementControllersHorizontalBoxVisibility))

			//Add Faders Horizontal Box
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SAssignNew(ElementControllersHorizontalBox, SHorizontalBox)
			]

			//Add Fader button
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.MaxWidth(20.f)
			.Padding(2.f, 4.f)
			.AutoWidth()
			[
				SNew(SDMXControlConsoleEditorAddButton)
				.OnClicked(this, &SDMXControlConsoleEditorFaderGroupControllerView::OnAddElementControllerClicked)
				.ToolTipText(LOCTEXT("AddFaderButton_ToolTip", "Add a new Raw Fader."))
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerView::GetAddElementControllerButtonVisibility))
			];

		return ElementsWidget;
	}

	bool SDMXControlConsoleEditorFaderGroupControllerView::IsSelected() const
	{
		UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
		if (!FaderGroupController || !EditorModel.IsValid())
		{
			return false;
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		return SelectionHandler->IsSelected(FaderGroupController);
	}

	TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> SDMXControlConsoleEditorFaderGroupControllerView::GetExpandArrowButton() const
	{
		return FaderGroupControllerToolbar.IsValid() ? FaderGroupControllerToolbar->GetExpandArrowButton() : nullptr;
	}

	void SDMXControlConsoleEditorFaderGroupControllerView::OnElementControllerAdded()
	{
		UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
		if (!FaderGroupController)
		{
			return;
		}

		const TArray<UDMXControlConsoleElementController*>& ElementControllers = FaderGroupController->GetElementControllers();
		for (UDMXControlConsoleElementController* ElementController : ElementControllers)
		{
			if (!ElementController)
			{
				continue;
			}

			if (ContainsElementController(ElementController))
			{
				continue;
			}

			AddElementController(ElementController);
		}
	}

	void SDMXControlConsoleEditorFaderGroupControllerView::AddElementController(UDMXControlConsoleElementController* ElementController)
	{
		if (!ensureMsgf(EditorModel.IsValid(), TEXT("Invalid control console editor model, cannot add new fader correctly.")))
		{
			return;
		}

		if (!ensureMsgf(ElementController, TEXT("Invalid element controller, cannot add new controller widget correctly.")))
		{
			return;
		}

		if (!ElementControllersHorizontalBox.IsValid())
		{
			return;
		}

		const TSharedRef<FDMXControlConsoleElementControllerModel> ElementControllerModel = MakeShared<FDMXControlConsoleElementControllerModel>(ElementController);
		TSharedPtr<SWidget> ElementControllerWidget = nullptr;

		if (Cast<UDMXControlConsoleMatrixCellController>(ElementController))
		{
			SAssignNew(ElementControllerWidget, SDMXControlConsoleEditorMatrixCellControllerView, ElementControllerModel, EditorModel.Get())
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerView::GetElementControllerWidgetVisibility, ElementControllerModel.ToSharedPtr()));
		}
		else
		{
			SAssignNew(ElementControllerWidget, SDMXControlConsoleEditorElementControllerView, ElementControllerModel, EditorModel.Get())
			.Padding(FMargin(4.f, 0.f))
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupControllerView::GetElementControllerWidgetVisibility, ElementControllerModel.ToSharedPtr()));
		}

		ElementControllerWidgets.Add(ElementControllerWidget);

		const int32 Index = ElementController->GetIndex();
		ElementControllersHorizontalBox->InsertSlot(Index)
			.HAlign(HAlign_Left)
			.AutoWidth()
			[
				ElementControllerWidget.ToSharedRef()
			];
	}

	void SDMXControlConsoleEditorFaderGroupControllerView::OnElementControllerRemoved()
	{
		UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
		if (!FaderGroupController)
		{
			return;
		}

		const TArray<UDMXControlConsoleElementController*>& ElementControllers = FaderGroupController->GetElementControllers();

		TArray<TWeakPtr<SWidget>> ElementControllerWidgetsToRemove;
		for (TWeakPtr<SWidget>& Widget : ElementControllerWidgets)
		{
			const UDMXControlConsoleElementController* ElementController = [Widget]() -> const UDMXControlConsoleElementController*
				{
					if (Widget.IsValid() && Widget.Pin()->GetTypeAsString() == TEXT("SDMXControlConsoleEditorElementControllerView"))
					{
						const TSharedPtr<SDMXControlConsoleEditorElementControllerView> ElementControllerView = StaticCastSharedPtr<SDMXControlConsoleEditorElementControllerView>(Widget.Pin());
						return ElementControllerView.IsValid() ? ElementControllerView->GetElementController() : nullptr;
					}
					else if (Widget.IsValid() && Widget.Pin()->GetTypeAsString() == TEXT("SDMXControlConsoleEditorMatrixCellControllerView"))
					{
						const TSharedPtr<SDMXControlConsoleEditorMatrixCellControllerView> MatrixCellWidget = StaticCastSharedPtr<SDMXControlConsoleEditorMatrixCellControllerView>(Widget.Pin());
						return MatrixCellWidget.IsValid() ? MatrixCellWidget->GetMatrixCellController() : nullptr;
					}

					return nullptr;
				}();


			if (ElementController && ElementControllers.Contains(ElementController))
			{
				continue;
			}

			ElementControllersHorizontalBox->RemoveSlot(Widget.Pin().ToSharedRef());
			ElementControllerWidgetsToRemove.Add(Widget);
		}

		ElementControllerWidgets.RemoveAll([&ElementControllerWidgetsToRemove](TWeakPtr<SWidget>& ElementControllerWidget)
			{
				return !ElementControllerWidget.IsValid() || ElementControllerWidgetsToRemove.Contains(ElementControllerWidget);
			});
	}

	bool SDMXControlConsoleEditorFaderGroupControllerView::ContainsElementController(const UDMXControlConsoleElementController* InElementController) const
	{
		if (!IsValid(InElementController))
		{
			return false;
		}

		const auto IsElementControllerInUseLambda = [InElementController](const TWeakPtr<SWidget>& Widget)
			{
				const UDMXControlConsoleElementController* ElementController = [Widget]() -> const UDMXControlConsoleElementController*
					{
						if (Widget.IsValid() && Widget.Pin()->GetTypeAsString() == TEXT("SDMXControlConsoleEditorElementControllerView"))
						{
							const TSharedPtr<SDMXControlConsoleEditorElementControllerView> ElementControllerView = StaticCastSharedPtr<SDMXControlConsoleEditorElementControllerView>(Widget.Pin());
							return ElementControllerView.IsValid() ? ElementControllerView->GetElementController() : nullptr;
						}
						else if (Widget.IsValid() && Widget.Pin()->GetTypeAsString() == TEXT("SDMXControlConsoleEditorMatrixCellControllerView"))
						{
							const TSharedPtr<SDMXControlConsoleEditorMatrixCellControllerView> MatrixCellWidget = StaticCastSharedPtr<SDMXControlConsoleEditorMatrixCellControllerView>(Widget.Pin());
							return MatrixCellWidget.IsValid() ? MatrixCellWidget->GetMatrixCellController() : nullptr;
						}

						return nullptr;
					}();

				return ElementController && ElementController == InElementController;
			};

		return Algo::FindByPredicate(ElementControllerWidgets, IsElementControllerInUseLambda) != nullptr;
	}

	void SDMXControlConsoleEditorFaderGroupControllerView::UpdateExpansionState()
	{
		UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
		if (FaderGroupController)
		{
			TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> ExpandArrowButton = GetExpandArrowButton();
			if (ExpandArrowButton.IsValid())
			{
				// Get expansion state from model
				const bool bIsExpanded = FaderGroupController->IsExpanded();
				ExpandArrowButton->SetExpandArrow(FaderGroupController->IsExpanded());
			}
		}
	}

	void SDMXControlConsoleEditorFaderGroupControllerView::OnExpandArrowClicked(bool bExpand)
	{
		UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
		if (FaderGroupController)
		{
			ViewMode = bExpand ? EDMXControlConsoleEditorViewMode::Expanded : EDMXControlConsoleEditorViewMode::Collapsed;

			constexpr bool bNotifyExpansionStateChange = false;
			FaderGroupController->Modify();
			FaderGroupController->SetIsExpanded(bExpand, bNotifyExpansionStateChange);
		}
	}

	void SDMXControlConsoleEditorFaderGroupControllerView::OnFaderGroupControllerGrouped()
	{
		OnElementControllerAdded();
		OnElementControllerRemoved();
	}

	void SDMXControlConsoleEditorFaderGroupControllerView::OnFaderGroupControllerFixturePatchChanged()
	{
		OnElementControllerAdded();
		OnElementControllerRemoved();
	}

	FReply SDMXControlConsoleEditorFaderGroupControllerView::OnAddElementControllerClicked()
	{
		if (!FaderGroupControllerModel.IsValid())
		{
			return FReply::Handled();
		}

		const FScopedTransaction FaderClickedTransaction(LOCTEXT("FaderClickedTransaction", "Add Fader"));

		UDMXControlConsoleFaderGroupController* FaderGroupController = FaderGroupControllerModel->GetFaderGroupController();
		UDMXControlConsoleFaderGroup* FaderGroup = FaderGroupControllerModel->GetFirstAvailableFaderGroup();
		if (FaderGroupController && FaderGroup && !FaderGroup->HasFixturePatch())
		{
			FaderGroup->PreEditChange(nullptr);
			UDMXControlConsoleRawFader* RawFader = FaderGroup->AddRawFader();
			FaderGroup->PostEditChange();

			FaderGroupController->PreEditChange(nullptr);
			FaderGroupController->CreateElementController(RawFader);
			FaderGroupController->PostEditChange();
		}

		return FReply::Handled();
	}

	void SDMXControlConsoleEditorFaderGroupControllerView::OnViewModeChanged()
	{
		const UDMXControlConsoleEditorData* EditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (!EditorData)
		{
			return;
		}

		ViewMode = EditorData->GetFaderGroupsViewMode();

		UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
		if (!FaderGroupController)
		{
			return;
		}

		switch (ViewMode)
		{
		case EDMXControlConsoleEditorViewMode::Collapsed:
		{
			FaderGroupController->SetIsExpanded(false);
			break;
		}
		case EDMXControlConsoleEditorViewMode::Expanded:
			FaderGroupController->SetIsExpanded(true);
			break;
		}
	}

	bool SDMXControlConsoleEditorFaderGroupControllerView::IsCurrentViewMode(EDMXControlConsoleEditorViewMode InViewMode) const
	{
		return ViewMode == InViewMode;
	}

	FOptionalSize SDMXControlConsoleEditorFaderGroupControllerView::GetFaderGroupControllerViewHeightByFadersViewMode() const
	{
		using namespace DMXControlConsoleEditorFaderGroupControllerView::Private;
		const UDMXControlConsoleEditorData* EditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (EditorData)
		{
			const EDMXControlConsoleEditorViewMode FadersViewMode = EditorData->GetFadersViewMode();
			return FadersViewMode == EDMXControlConsoleEditorViewMode::Collapsed ? CollapsedViewModeHeight : ExpandedViewModeHeight;
		}

		return CollapsedViewModeHeight;
	}

	FSlateColor SDMXControlConsoleEditorFaderGroupControllerView::GetFaderGroupControllerViewBorderColor() const
	{
		const UDMXControlConsoleFaderGroupController* FaderGroupController = GetFaderGroupController();
		if (!FaderGroupController)
		{
			return FLinearColor::White;
		}

		return FaderGroupController->GetEditorColor();
	}

	const FSlateBrush* SDMXControlConsoleEditorFaderGroupControllerView::GetFaderGroupControllerViewBorderImage() const
	{
		if (IsHovered())
		{
			if (IsSelected())
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.WhiteBrush");
			}
			else
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.WhiteBrush_Tansparent");
			}
		}
		else
		{
			if (IsSelected())
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.WhiteBrush");
			}
			else
			{
				return FAppStyle::GetBrush("NoBorder");
			}
		}
	}

	const FSlateBrush* SDMXControlConsoleEditorFaderGroupControllerView::GetFaderGroupControllerViewBackgroundBorderImage() const
	{
		if (IsHovered())
		{
			if (IsSelected())
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.FaderGroup_Highlighted");
			}
			else
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.DefaultBrush");
			}
		}
		else
		{
			if (IsSelected())
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.FaderGroup_Selected");
			}
			else
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.DefaultBrush");
			}
		}
	}

	EVisibility SDMXControlConsoleEditorFaderGroupControllerView::GetViewModeVisibility(EDMXControlConsoleEditorViewMode InViewMode) const
	{
		return  IsCurrentViewMode(InViewMode) ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorFaderGroupControllerView::GetElementControllerWidgetVisibility(TSharedPtr<FDMXControlConsoleElementControllerModel> ElementControllerModel) const
	{
		const UDMXControlConsoleElementController* ElementController = ElementControllerModel.IsValid() ? ElementControllerModel->GetElementController() : nullptr;
		const bool bIsVisible = ElementController && ElementController->IsMatchingFilter();
		return  bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorFaderGroupControllerView::GetAddButtonVisibility() const
	{
		const bool bIsVisible =
			FaderGroupControllerModel.IsValid() &&
			FaderGroupControllerModel->CanAddFaderGroupController() &&
			IsCurrentViewMode(EDMXControlConsoleEditorViewMode::Collapsed);

		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorFaderGroupControllerView::GetAddRowButtonVisibility() const
	{
		if (IsCurrentViewMode(EDMXControlConsoleEditorViewMode::Expanded))
		{
			return EVisibility::Collapsed;
		}

		const bool bIsVisble = FaderGroupControllerModel.IsValid() && FaderGroupControllerModel->CanAddFaderGroupControllerOnNewRow();
		return bIsVisble ? EVisibility::Visible : EVisibility::Hidden;
	}

	EVisibility SDMXControlConsoleEditorFaderGroupControllerView::GetElementControllersHorizontalBoxVisibility() const
	{
		const bool bIsVisible =
			GetExpandArrowButton().IsValid() &&
			GetExpandArrowButton()->IsExpanded() &&
			IsCurrentViewMode(EDMXControlConsoleEditorViewMode::Expanded);

		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorFaderGroupControllerView::GetAddElementControllerButtonVisibility() const
	{
		const bool bIsVisble = FaderGroupControllerModel.IsValid() && FaderGroupControllerModel->CanAddElementController();
		return bIsVisble ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorFaderGroupControllerView::GetGroupColorTagImageVisibility() const
	{
		const bool bIsVisble = FaderGroupControllerModel.IsValid() && !FaderGroupControllerModel->HasSingleFaderGroup();
		return bIsVisble ? EVisibility::Visible : EVisibility::Hidden;
	}
}

#undef LOCTEXT_NAMESPACE
