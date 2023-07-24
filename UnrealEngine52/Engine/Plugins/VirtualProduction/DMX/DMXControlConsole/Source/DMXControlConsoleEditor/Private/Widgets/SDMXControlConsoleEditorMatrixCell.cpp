// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorMatrixCell.h"

#include "DMXControlConsoleEditorManager.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFixturePatchCellAttributeFader.h"
#include "DMXControlConsoleFixturePatchMatrixCell.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Widgets/SDMXControlConsoleEditorFader.h"
#include "Widgets/SDMXControlConsoleEditorSpinBoxVertical.h"
#include "Widgets/SDMXControlConsoleEditorExpandArrowButton.h"

#include "ScopedTransaction.h"
#include "Algo/Find.h"
#include "Misc/Optional.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorMatrixCell"

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
				.HeightOverride(270.f)
				[
					SNew(SBorder)
					.BorderBackgroundColor(FLinearColor::White)
					[
						SNew(SBorder)
						.BorderImage(this, &SDMXControlConsoleEditorMatrixCell::GetBorderImage)
						[
							SNew(SVerticalBox)

							// Matrix Cell Label
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SBox)
								.HeightOverride(5.f)
								[
									SNew(SImage)
									.Image(FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.WhiteBrush"))
									.ColorAndOpacity(this, &SDMXControlConsoleEditorMatrixCell::GetLabelBorderColor)
								]
							]

							// Matrix Cell Text Label
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SNew(SBorder)
								.BorderImage(FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.BlackBrush"))
								[
									SNew(STextBlock)
									.ColorAndOpacity(FLinearColor::White)
									.Text(this, &SDMXControlConsoleEditorMatrixCell::GetMatrixCellLabelText)
									.Font(FAppStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
									.Justification(ETextJustify::Center)
								]
							]

							// Matrix Cell Expand button
							+ SVerticalBox::Slot()
							.AutoHeight()
							[
								SAssignNew(ExpandArrowButton, SDMXControlConsoleEditorExpandArrowButton)
							]
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

void SDMXControlConsoleEditorMatrixCell::ApplyGlobalFilter(const FString& InSearchString)
{
	bool bHasVisibleChildren = false;

	for (TWeakPtr<SDMXControlConsoleEditorFader> WeakCellAttributeFaderWidget : CellAttributeFaderWidgets)
	{
		if (const TSharedPtr<SDMXControlConsoleEditorFader> CellAttributeFaderWidget = WeakCellAttributeFaderWidget.Pin())
		{
			CellAttributeFaderWidget->ApplyGlobalFilter(InSearchString);
			if (CellAttributeFaderWidget->GetVisibility() == EVisibility::Visible)
			{
				bHasVisibleChildren = true;
			}
		}
	}

	const EVisibility NewVisibility = bHasVisibleChildren ? EVisibility::Visible : EVisibility::Collapsed;
	SetVisibility(NewVisibility);
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
		.Padding(FMargin(2.f, 0.f));

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

	const TArray<UDMXControlConsoleFaderBase*>& Faders = MatrixCell->GetFaders();
	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();

	auto IsCellAttributeFaderSelectedLambda = [SelectionHandler](UDMXControlConsoleFaderBase* Fader)
		{
			return SelectionHandler->IsSelected(Fader);
		};

	return Algo::FindByPredicate(Faders, IsCellAttributeFaderSelectedLambda) ? true : false;
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
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.FaderGroup_Highlighted");;
		}
		else
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.FaderGroup_Hovered");;
		}
	}
	else
	{
		if (IsSelected())
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.FaderGroup_Selected");;
		}
		else
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.BlackBrush");
		}
	}
}

#undef LOCTEXT_NAMESPACE
