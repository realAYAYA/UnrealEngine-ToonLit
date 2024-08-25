// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorMatrixCellControllerView.h"

#include "Algo/AnyOf.h"
#include "DMXControlConsoleEditorData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFixturePatchMatrixCell.h"
#include "Layouts/Controllers/DMXControlConsoleCellAttributeController.h"
#include "Layouts/Controllers/DMXControlConsoleElementController.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/Controllers/DMXControlConsoleMatrixCellController.h"
#include "Misc/Optional.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Models/DMXControlConsoleElementControllerModel.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Views/SDMXControlConsoleEditorElementControllerView.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorExpandArrowButton.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorMatrixCellControllerView"

namespace UE::DMX::Private
{
	namespace DMXControlConsoleEditorMatrixCellControllerView
	{
		namespace Private
		{
			constexpr float CollapsedViewModeHeight = 230.f;
			constexpr float ExpandedViewModeHeight = 310.f;
		}
	}

	void SDMXControlConsoleEditorMatrixCellControllerView::Construct(const FArguments& InArgs, const TSharedPtr<FDMXControlConsoleElementControllerModel>& InElementControllerModel, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel, TEXT("Invalid control console editor model, can't constuct matrix cell controller view correctly.")))
		{
			return;
		}

		if (!ensureMsgf(InElementControllerModel.IsValid(), TEXT("Invalid element controller model, cannot create matrix cell controller view correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
		MatrixCellControllerModel = InElementControllerModel;

		EditorModel->GetOnEditorModelUpdated().AddSP(this, &SDMXControlConsoleEditorMatrixCellControllerView::OnEditorModelUpdated);

		ChildSlot
			[
				SNew(SHorizontalBox)
				// Matrix Cell section
				+ SHorizontalBox::Slot()
				.Padding(2.f, 0.f)
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(20.f)
					.HeightOverride(TAttribute<FOptionalSize>::CreateSP(this, &SDMXControlConsoleEditorMatrixCellControllerView::GetMatrixCellControllerHeightByFadersViewMode))
					[
						SNew(SBorder)
						.BorderImage(this, &SDMXControlConsoleEditorMatrixCellControllerView::GetBorderImage)
						[
							SNew(SVerticalBox)
							// Matrix Cell Label
							+ SVerticalBox::Slot()
							.Padding(0.f, 1.f, 0.f, 0.f)
							.AutoHeight()
							[
								SNew(SBox)
								.HeightOverride(8.f)
								.Padding(1.f)
								[
									SNew(SImage)
									.Image(FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.FaderGroupTag"))
									.ColorAndOpacity(this, &SDMXControlConsoleEditorMatrixCellControllerView::GetLabelBorderColor)
								]
							]

							// Matrix Cell Expand button
							+ SVerticalBox::Slot()
							.Padding(0.f, 4.f, 0.f, 0.f)
							.AutoHeight()
							[
								SNew(STextBlock)
								.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
								.Text(this, &SDMXControlConsoleEditorMatrixCellControllerView::GetMatrixCellLabelText)
								.Justification(ETextJustify::Center)
							]

							// Matrix Cell Text Label
							+ SVerticalBox::Slot()
							.Padding(0.f, 2.f, 0.f, 0.f)
							.AutoHeight()
							[
								SAssignNew(ExpandArrowButton, SDMXControlConsoleEditorExpandArrowButton)
								.ToolTipText(LOCTEXT("MatrixCellExpandArrowButton_Tooltip", "Switch expansion state of the cell"))
							]
						]
					]
				]

				// Matrix Cell Faders section
				+ SHorizontalBox::Slot()
				.Padding(2.f, 0.f)
				.AutoWidth()
				[
					SAssignNew(CellAttributeControllersHorizontalBox, SHorizontalBox)
					.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorMatrixCellControllerView::GetCellAttributeControllersHorizontalBoxVisibility))
				]
			];
	}

	UDMXControlConsoleMatrixCellController* SDMXControlConsoleEditorMatrixCellControllerView::GetMatrixCellController() const
	{
		UDMXControlConsoleElementController* ElementController = MatrixCellControllerModel.IsValid() ? MatrixCellControllerModel->GetElementController() : nullptr;
		return Cast<UDMXControlConsoleMatrixCellController>(ElementController);
	}

	UDMXControlConsoleFixturePatchMatrixCell* SDMXControlConsoleEditorMatrixCellControllerView::GetMatrixCell() const
	{
		return MatrixCellControllerModel.IsValid() ? MatrixCellControllerModel->GetMatrixCellElement() : nullptr;
	}

	FReply SDMXControlConsoleEditorMatrixCellControllerView::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			const UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = GetMatrixCell();
			if (!MatrixCell)
			{
				return FReply::Unhandled();
			}

			if (ExpandArrowButton.IsValid())
			{
				ExpandArrowButton->ToggleExpandArrow();
			}

			return FReply::Handled();
		}

		return FReply::Unhandled();
	}

	void SDMXControlConsoleEditorMatrixCellControllerView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
	{
		const UDMXControlConsoleMatrixCellController* MatrixCellController = GetMatrixCellController();
		if (!ensureMsgf(MatrixCellController, TEXT("Invalid matrix cell controller, cannot update matrix cell controller view state correctly.")))
		{
			return;
		}

		const TArray<UDMXControlConsoleCellAttributeController*>& CellAttributeControllers = MatrixCellController->GetCellAttributeControllers();
		if (CellAttributeControllers.Num() == CellAttributeControllerViews.Num())
		{
			return;
		}

		if (CellAttributeControllers.Num() > CellAttributeControllerViews.Num())
		{
			OnCellAttributeControllerAdded();
		}
		else
		{
			OnCellAttributeControllerRemoved();
		}
	}

	void SDMXControlConsoleEditorMatrixCellControllerView::OnCellAttributeControllerAdded()
	{
		const UDMXControlConsoleMatrixCellController* MatrixCellController = GetMatrixCellController();
		if (!ensureMsgf(MatrixCellController, TEXT("Invalid matrix cell controller, cannot add element contorller view correctly.")))
		{
			return;
		}

		const TArray<UDMXControlConsoleCellAttributeController*>& CellAttributeControllers = MatrixCellController->GetCellAttributeControllers();
		for (UDMXControlConsoleCellAttributeController* CellAttributeController : CellAttributeControllers)
		{
			if (!CellAttributeController)
			{
				continue;
			}

			if (ContainsCellAttributeController(CellAttributeController))
			{
				continue;
			}

			AddCellAttributeController(CellAttributeController);
		}
	}

	void SDMXControlConsoleEditorMatrixCellControllerView::AddCellAttributeController(UDMXControlConsoleCellAttributeController* CellAttributeController)
	{
		if (!ensureMsgf(EditorModel.IsValid(), TEXT("Invalid control console editor model, cannot add element controller view correctly.")))
		{
			return;
		}

		if (!ensureMsgf(CellAttributeController, TEXT("Invalid cell attribute controller, cannot add element controller view correctly.")))
		{
			return;
		}

		if (!CellAttributeControllersHorizontalBox.IsValid())
		{
			return;
		}

		const TSharedRef<FDMXControlConsoleElementControllerModel> NewElementControllerModel = MakeShared<FDMXControlConsoleElementControllerModel>(CellAttributeController);
		const TSharedRef<SDMXControlConsoleEditorElementControllerView> ElementControllerView =
			SNew(SDMXControlConsoleEditorElementControllerView, NewElementControllerModel, EditorModel.Get())
			.Padding(FMargin(2.f, 0.f))
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorMatrixCellControllerView::GetCellAttributeControllerWidgetVisibility, NewElementControllerModel.ToSharedPtr()));

		CellAttributeControllerViews.Add(ElementControllerView);

		const int32 Index = CellAttributeController->GetIndex();
		CellAttributeControllersHorizontalBox->InsertSlot(Index)
			.AutoWidth()
			.HAlign(HAlign_Left)
			[
				ElementControllerView
			];
	}

	void SDMXControlConsoleEditorMatrixCellControllerView::OnCellAttributeControllerRemoved()
	{
		const UDMXControlConsoleMatrixCellController* MatrixCellController = GetMatrixCellController();
		if (!ensureMsgf(MatrixCellController, TEXT("Invalid matrix cell controller, cannot remove the controller view correctly.")))
		{
			return;
		}

		const TArray<UDMXControlConsoleCellAttributeController*>& CellAttributeControllers = MatrixCellController->GetCellAttributeControllers();

		TArray<TWeakPtr<SDMXControlConsoleEditorElementControllerView>> CellAttributeControllerViewsToRemove;
		for (TWeakPtr<SDMXControlConsoleEditorElementControllerView>& CellAttributeControllerView : CellAttributeControllerViews)
		{
			if (!CellAttributeControllerView.IsValid())
			{
				continue;
			}

			const UDMXControlConsoleElementController* CellAttributeController = CellAttributeControllerView.Pin()->GetElementController();
			if (!CellAttributeController || !CellAttributeControllers.Contains(CellAttributeController))
			{
				CellAttributeControllersHorizontalBox->RemoveSlot(CellAttributeControllerView.Pin().ToSharedRef());
				CellAttributeControllerViewsToRemove.Add(CellAttributeControllerView);
			}
		}

		CellAttributeControllerViews.RemoveAll([&CellAttributeControllerViewsToRemove](const TWeakPtr<SDMXControlConsoleEditorElementControllerView>& CellAttributeControllerView)
			{
				return !CellAttributeControllerView.IsValid() || CellAttributeControllerViewsToRemove.Contains(CellAttributeControllerView);
			});
	}

	bool SDMXControlConsoleEditorMatrixCellControllerView::ContainsCellAttributeController(UDMXControlConsoleCellAttributeController* InCellAttributeController)
	{
		auto IsCellAttributeControllerInUseLambda = [InCellAttributeController](const TWeakPtr<SDMXControlConsoleEditorElementControllerView>& CellAttributeControllerView)
			{
				if (!CellAttributeControllerView.IsValid())
				{
					return false;
				}

				const UDMXControlConsoleElementController* CellAttributeController = CellAttributeControllerView.Pin()->GetElementController();
				if (!CellAttributeController)
				{
					return false;
				}

				return CellAttributeController == InCellAttributeController;
			};

		return CellAttributeControllerViews.ContainsByPredicate(IsCellAttributeControllerInUseLambda);
	}

	bool SDMXControlConsoleEditorMatrixCellControllerView::IsAnyCellAttributeControllerSelected() const
	{
		const UDMXControlConsoleMatrixCellController* MatrixCellController = GetMatrixCellController();
		if (!EditorModel.IsValid() || !MatrixCellController)
		{
			return false;
		}

		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorModel->GetSelectionHandler();
		const TArray<TWeakObjectPtr<UObject>>& SelectedElementControllers = SelectionHandler->GetSelectedElementControllers();

		const TArray<UDMXControlConsoleCellAttributeController*>& CellAttributeControllers = MatrixCellController->GetCellAttributeControllers();
		const bool bIsAnyCellAttributeControllerSelected = Algo::AnyOf(CellAttributeControllers,
			[SelectedElementControllers](UDMXControlConsoleCellAttributeController* CellAttributeController)
			{
				return CellAttributeController && SelectedElementControllers.Contains(CellAttributeController);
			});

		return bIsAnyCellAttributeControllerSelected;
	}

	void SDMXControlConsoleEditorMatrixCellControllerView::OnEditorModelUpdated()
	{
		const UDMXControlConsoleMatrixCellController* MatrixCellController = GetMatrixCellController();
		if (MatrixCellController)
		{
			OnCellAttributeControllerAdded();
			OnCellAttributeControllerRemoved();
		}
	}

	FOptionalSize SDMXControlConsoleEditorMatrixCellControllerView::GetMatrixCellControllerHeightByFadersViewMode() const
	{
		using namespace DMXControlConsoleEditorMatrixCellControllerView::Private;
		const UDMXControlConsoleEditorData* EditorData = EditorModel.IsValid() ? EditorModel->GetControlConsoleEditorData() : nullptr;
		if (EditorData)
		{
			const EDMXControlConsoleEditorViewMode ViewMode = EditorData->GetFadersViewMode();
			return ViewMode == EDMXControlConsoleEditorViewMode::Collapsed ? CollapsedViewModeHeight : ExpandedViewModeHeight;
		}

		return CollapsedViewModeHeight;
	}

	FText SDMXControlConsoleEditorMatrixCellControllerView::GetMatrixCellLabelText() const
	{
		const UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = GetMatrixCell();
		if (MatrixCell)
		{
			return FText::FromString(FString::FromInt(MatrixCell->GetCellID()));
		}

		return FText::GetEmpty();
	}

	FSlateColor SDMXControlConsoleEditorMatrixCellControllerView::GetLabelBorderColor() const
	{
		const UDMXControlConsoleMatrixCellController* MatrixCellController = GetMatrixCellController();
		if (MatrixCellController)
		{
			const UDMXControlConsoleFaderGroupController& FaderGroupController = MatrixCellController->GetOwnerFaderGroupControllerChecked();
			return FaderGroupController.GetEditorColor();
		}

		return FSlateColor(FLinearColor::White);
	}

	EVisibility SDMXControlConsoleEditorMatrixCellControllerView::GetCellAttributeControllerWidgetVisibility(TSharedPtr<FDMXControlConsoleElementControllerModel> ControllerModel) const
	{
		const UDMXControlConsoleElementController* ElementController = ControllerModel.IsValid() ? ControllerModel->GetElementController() : nullptr;
		const bool bIsVisible = ElementController && ElementController->IsMatchingFilter();
		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorMatrixCellControllerView::GetCellAttributeControllersHorizontalBoxVisibility() const
	{
		const bool bIsVisible = ExpandArrowButton.IsValid() && ExpandArrowButton->IsExpanded();
		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	const FSlateBrush* SDMXControlConsoleEditorMatrixCellControllerView::GetBorderImage() const
	{
		const UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = GetMatrixCell();
		if (!MatrixCell)
		{
			return nullptr;
		}

		if (IsHovered())
		{
			if (IsAnyCellAttributeControllerSelected())
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader_Highlighted");;
			}
			else
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader_Hovered");;
			}
		}
		else
		{
			if (IsAnyCellAttributeControllerSelected())
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader_Selected");;
			}
			else
			{
				return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader");
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
