// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorMatrixCell.h"

#include "Algo/Find.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFixturePatchCellAttributeFader.h"
#include "DMXControlConsoleFixturePatchMatrixCell.h"
#include "Misc/Optional.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SDMXControlConsoleEditorFader.h"
#include "Widgets/SDMXControlConsoleEditorSpinBoxVertical.h"
#include "Widgets/SDMXControlConsoleEditorExpandArrowButton.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorMatrixCell"

namespace UE::DMXControlConsoleEditor::DMXControlConsoleEditorMatrixCell::Private
{
	static float CollapsedViewModeHeight = 200.f;
	static float ExpandedViewModeHeight = 280.f;
};

void SDMXControlConsoleEditorMatrixCell::Construct(const FArguments& InArgs, const TObjectPtr<UDMXControlConsoleFixturePatchMatrixCell>& InMatrixCell)
{
	MatrixCell = InMatrixCell;

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
				.HeightOverride(TAttribute<FOptionalSize>::CreateSP(this, &SDMXControlConsoleEditorMatrixCell::GetMatrixCellHeightByFadersViewMode))
				[
					SNew(SBorder)
					.BorderImage(this, &SDMXControlConsoleEditorMatrixCell::GetBorderImage)
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
								.ColorAndOpacity(this, &SDMXControlConsoleEditorMatrixCell::GetLabelBorderColor)
							]
						]

						// Matrix Cell Expand button
						+ SVerticalBox::Slot()
						.Padding(0.f, 4.f, 0.f, 0.f)
						.AutoHeight()
						[
							SNew(STextBlock)
							.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
							.Text(this, &SDMXControlConsoleEditorMatrixCell::GetMatrixCellLabelText)
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
				SAssignNew(CellAttributeFadersHorizontalBox, SHorizontalBox)
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorMatrixCell::GetCellAttributeFadersHorizontalBoxVisibility))
			]
		];
}

FReply SDMXControlConsoleEditorMatrixCell::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (!MatrixCell.IsValid())
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

void SDMXControlConsoleEditorMatrixCell::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (!ensureMsgf(MatrixCell.IsValid(), TEXT("Invalid matrix cell fader, cannot update matrix cell fader state correctly.")))
	{
		return;
	}

	const TArray<UDMXControlConsoleFaderBase*>& CellAttributeFaders = MatrixCell->GetFaders();
	if (CellAttributeFaders.Num() == CellAttributeFaderWidgets.Num())
	{
		return;
	}

	if (CellAttributeFaders.Num() > CellAttributeFaderWidgets.Num())
	{
		OnCellAttributeFaderAdded();
	}
	else
	{
		OnCellAttributeFaderRemoved();
	}
}

void SDMXControlConsoleEditorMatrixCell::OnCellAttributeFaderAdded()
{
	const TArray<UDMXControlConsoleFaderBase*>& CellAttributeFaders = MatrixCell->GetFaders();

	for (UDMXControlConsoleFaderBase* CellAttributeFader : CellAttributeFaders)
	{
		if (!CellAttributeFader)
		{
			continue;
		}

		if (ContainsCellAttributeFader(CellAttributeFader))
		{
			continue;
		}

		AddCellAttributeFader(CellAttributeFader);
	}
}

void SDMXControlConsoleEditorMatrixCell::AddCellAttributeFader(UDMXControlConsoleFaderBase* CellAttributeFader)
{
	if (!ensureMsgf(CellAttributeFader, TEXT("Invalid cell attribute faders, cannot add new matrix cell fader correctly.")))
	{
		return;
	}

	if (!CellAttributeFadersHorizontalBox.IsValid())
	{
		return;
	}

	const int32 Index = CellAttributeFader->GetIndex();

	TSharedRef<SDMXControlConsoleEditorFader> CellAttributeFaderWidget =
		SNew(SDMXControlConsoleEditorFader, CellAttributeFader)
		.Padding(FMargin(2.f, 0.f))
		.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorMatrixCell::GetFaderWidgetVisibility, CellAttributeFader));

	CellAttributeFaderWidgets.Insert(CellAttributeFaderWidget, Index);

	CellAttributeFadersHorizontalBox->InsertSlot(Index)
		.AutoWidth()
		.HAlign(HAlign_Left)
		[
			CellAttributeFaderWidget
		];
}

void SDMXControlConsoleEditorMatrixCell::OnCellAttributeFaderRemoved()
{
	const TArray<UDMXControlConsoleFaderBase*>& CellAttributeFaders = MatrixCell->GetFaders();

	TArray<TWeakPtr<SDMXControlConsoleEditorFader>> CellAttributeFaderWidgetsToRemove;
	for (TWeakPtr<SDMXControlConsoleEditorFader>& CellAttributeFaderWidget : CellAttributeFaderWidgets)
	{
		if (!CellAttributeFaderWidget.IsValid())
		{
			continue;
		}

		const UDMXControlConsoleFaderBase* CellAttributeFader = CellAttributeFaderWidget.Pin()->GetFader();
		if (!CellAttributeFader || !CellAttributeFaders.Contains(CellAttributeFader))
		{
			CellAttributeFadersHorizontalBox->RemoveSlot(CellAttributeFaderWidget.Pin().ToSharedRef());
			CellAttributeFaderWidgetsToRemove.Add(CellAttributeFaderWidget);
		}
	}

	CellAttributeFaderWidgets.RemoveAll([&CellAttributeFaderWidgetsToRemove](const TWeakPtr<SDMXControlConsoleEditorFader> CellAttributeFaderWidget)
		{
			return !CellAttributeFaderWidget.IsValid() || CellAttributeFaderWidgetsToRemove.Contains(CellAttributeFaderWidget);
		});
}

bool SDMXControlConsoleEditorMatrixCell::ContainsCellAttributeFader(UDMXControlConsoleFaderBase* CellAttributeFader)
{
	auto IsCellAttributeFaderInUseLambda = [CellAttributeFader](const TWeakPtr<SDMXControlConsoleEditorFader> CellAttributeFaderWidget)
		{
			if (!CellAttributeFaderWidget.IsValid())
			{
				return false;
			}

			const TWeakObjectPtr<UDMXControlConsoleFaderBase> Other = CellAttributeFaderWidget.Pin()->GetFader();
			if (!Other.IsValid())
			{
				return false;
			}

			return Other == CellAttributeFader;
		};

	return CellAttributeFaderWidgets.ContainsByPredicate(IsCellAttributeFaderInUseLambda);
}

bool SDMXControlConsoleEditorMatrixCell::IsSelected() const
{
	return IsAnyCellAttributeFaderSelected();
}

bool SDMXControlConsoleEditorMatrixCell::IsAnyCellAttributeFaderSelected() const
{
	if (!MatrixCell.IsValid())
	{
		return false;
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
	const TArray<UDMXControlConsoleFaderBase*>& Faders = MatrixCell->GetFaders();

	auto IsCellAttributeFaderSelectedLambda = [SelectionHandler](UDMXControlConsoleFaderBase* Fader)
		{
			return SelectionHandler->IsSelected(Fader);
		};

	return Algo::FindByPredicate(Faders, IsCellAttributeFaderSelectedLambda) ? true : false;
}

FOptionalSize SDMXControlConsoleEditorMatrixCell::GetMatrixCellHeightByFadersViewMode() const
{
	using namespace UE::DMXControlConsoleEditor::DMXControlConsoleEditorMatrixCell::Private;

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const EDMXControlConsoleEditorViewMode ViewMode = EditorConsoleModel->GetFadersViewMode();
	return ViewMode == EDMXControlConsoleEditorViewMode::Collapsed ? CollapsedViewModeHeight : ExpandedViewModeHeight;
}

FText SDMXControlConsoleEditorMatrixCell::GetMatrixCellLabelText() const
{
	if (MatrixCell.IsValid())
	{
		return FText::FromString(FString::FromInt(MatrixCell->GetCellID()));
	}

	return FText::GetEmpty();
}

FSlateColor SDMXControlConsoleEditorMatrixCell::GetLabelBorderColor() const
{
	if (MatrixCell.IsValid())
	{
		const UDMXControlConsoleFaderGroup& FaderGroup = MatrixCell->GetOwnerFaderGroupChecked();
		return FaderGroup.GetEditorColor();
	}

	return FSlateColor(FLinearColor::White);
}

EVisibility SDMXControlConsoleEditorMatrixCell::GetFaderWidgetVisibility(const UDMXControlConsoleFaderBase* Fader) const
{
	const bool bIsVisible = Fader && Fader->IsMatchingFilter();
	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SDMXControlConsoleEditorMatrixCell::GetCellAttributeFadersHorizontalBoxVisibility() const
{
	const bool bIsVisible = ExpandArrowButton.IsValid() && ExpandArrowButton->IsExpanded();
	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

const FSlateBrush* SDMXControlConsoleEditorMatrixCell::GetBorderImage() const
{
	if (!MatrixCell.IsValid())
	{
		return nullptr;
	}

	if (IsHovered())
	{
		if (IsSelected())
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
		if (IsSelected())
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader_Selected");;
		}
		else
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.Fader");
		}
	}
}

#undef LOCTEXT_NAMESPACE

