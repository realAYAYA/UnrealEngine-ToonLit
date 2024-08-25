// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorGridLayout.h"

#include "Algo/AnyOf.h"
#include "Algo/Find.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Editor.h"
#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "TimerManager.h"
#include "Views/SDMXControlConsoleEditorFaderGroupControllerView.h"
#include "Views/SDMXControlConsoleEditorLayoutRowView.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorGridLayout"

namespace UE::DMX::Private
{
	void SDMXControlConsoleEditorGridLayout::Construct(const FArguments& InArgs, UDMXControlConsoleEditorGlobalLayoutBase* InLayout, UDMXControlConsoleEditorModel* InEditorModel)
	{
		if (!ensureMsgf(InEditorModel && InLayout, TEXT("Invalid control console editor model, can't create layout view correctly.")))
		{
			return;
		}

		EditorModel = InEditorModel;
		EditorLayout = InLayout;

		EditorModel->GetOnEditorModelUpdated().AddSP(this, &SDMXControlConsoleEditorGridLayout::Refresh);
		EditorModel->GetOnScrollFaderGroupControllerIntoView().AddSP(this, &SDMXControlConsoleEditorGridLayout::OnScrollIntoView);

		const TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
			.Orientation(Orient_Vertical);

		const TSharedRef<SScrollBar> HorizontalScrollBar = SNew(SScrollBar)
			.Orientation(Orient_Horizontal);

		ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					[
						SAssignNew(HorizontalScrollBox, SScrollBox)
						.ExternalScrollbar(HorizontalScrollBar)
						.Orientation(Orient_Horizontal)

						+ SScrollBox::Slot()
						[
							SAssignNew(VerticalScrollBox, SScrollBox)
							.ExternalScrollbar(VerticalScrollBar)
							.Orientation(Orient_Vertical)

							+ SScrollBox::Slot()
							[
								SAssignNew(LayoutRowsVerticalBox, SVerticalBox)
							]
						]
					]

					// Horizontal ScrollBar slot
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						VerticalScrollBar
					]
				]

				// Vertical Scrollbar slot
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					HorizontalScrollBar
				]
			];
	}

	bool SDMXControlConsoleEditorGridLayout::CanRefresh() const
	{
		if (!EditorModel.IsValid() || !EditorLayout.IsValid())
		{
			return false;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ActiveLayout)
		{
			return false;
		}

		if (ActiveLayout->GetLayoutRows().Num() == LayoutRowViews.Num())
		{
			return false;
		}

		return true;
	}

	void SDMXControlConsoleEditorGridLayout::OnLayoutElementAdded()
	{
		if (!ensureMsgf(EditorModel.IsValid(), TEXT("Invalid control console editor model, can't add new element to layout correctly.")))
		{
			return;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel->GetControlConsoleLayouts();
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ensureMsgf(ActiveLayout, TEXT("Invalid active layout, can't add new element to layout correctly.")))
		{
			return;
		}

		const TArray<UDMXControlConsoleEditorGlobalLayoutRow*>& LayoutRows = ActiveLayout->GetLayoutRows();
		for (UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow : LayoutRows)
		{
			if (!LayoutRow)
			{
				continue;
			}

			if (IsLayoutRowContained(LayoutRow))
			{
				continue;
			}

			const TSharedRef<SDMXControlConsoleEditorLayoutRowView> LayoutRowWidget =
				SNew(SDMXControlConsoleEditorLayoutRowView, LayoutRow, EditorModel.Get())
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorGridLayout::GetLayoutRowViewVisibility, LayoutRow));

			const int32 RowIndex = LayoutRow->GetRowIndex();
			if (ensureMsgf(RowIndex == 0 || LayoutRowViews.IsValidIndex(RowIndex - 1), TEXT("Unexpected, invalid layout row index when trying to add layout row view.")))
			{
				LayoutRowViews.Insert(LayoutRowWidget, RowIndex);

				LayoutRowsVerticalBox->InsertSlot(RowIndex)
					.AutoHeight()
					.VAlign(VAlign_Top)
					.Padding(0.f, 8.f)
					[
						LayoutRowWidget
					];
			}
			else
			{
				LayoutRowViews.Add(LayoutRowWidget);

				LayoutRowsVerticalBox->AddSlot()
					.AutoHeight()
					.VAlign(VAlign_Top)
					.Padding(0.f, 8.f)
					[
						LayoutRowWidget
					];
			}
		}
	}

	void SDMXControlConsoleEditorGridLayout::OnLayoutElementRemoved()
	{
		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ensureMsgf(ActiveLayout, TEXT("Invalid active layout, can't remove element from the layout correctly.")))
		{
			return;
		}

		const TArray<UDMXControlConsoleEditorGlobalLayoutRow*>& LayoutRows = ActiveLayout->GetLayoutRows();

		TArray<TWeakPtr<SDMXControlConsoleEditorLayoutRowView>> LayoutRowViewsToRemove;
		for (TWeakPtr<SDMXControlConsoleEditorLayoutRowView>& LayoutRowView : LayoutRowViews)
		{
			if (!LayoutRowView.IsValid())
			{
				continue;
			}

			const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = LayoutRowView.Pin()->GetLayoutRow();
			if (!LayoutRow || !LayoutRows.Contains(LayoutRow))
			{
				LayoutRowsVerticalBox->RemoveSlot(LayoutRowView.Pin().ToSharedRef());
				LayoutRowViewsToRemove.Add(LayoutRowView);
			}
		}

		LayoutRowViews.RemoveAll([&LayoutRowViewsToRemove](const TWeakPtr<SDMXControlConsoleEditorLayoutRowView>& LayoutRowView)
			{
				return !LayoutRowView.IsValid() || LayoutRowViewsToRemove.Contains(LayoutRowView);
			});
	}

	bool SDMXControlConsoleEditorGridLayout::IsLayoutRowContained(UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow)
	{
		if (!LayoutRow)
		{
			return false;
		}

		const bool bContainsLayoutRow = Algo::FindByPredicate(LayoutRowViews,
			[LayoutRow](const TWeakPtr<SDMXControlConsoleEditorLayoutRowView>& LayoutRowViewWeakPtr)
			{
				if (const TSharedPtr<SDMXControlConsoleEditorLayoutRowView> LayoutRowView = LayoutRowViewWeakPtr.Pin())
				{
					const UDMXControlConsoleEditorGlobalLayoutRow* OtherLayoutRow = LayoutRowView->GetLayoutRow();
					return LayoutRow == OtherLayoutRow;
				}

				return false;
			}) != nullptr;

		return bContainsLayoutRow;
	}

	TSharedPtr<SDMXControlConsoleEditorLayoutRowView> SDMXControlConsoleEditorGridLayout::FindLayoutRowView(const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow)
	{
		if (LayoutRow)
		{
			const TWeakPtr<SDMXControlConsoleEditorLayoutRowView>* LayoutRowViewPtr = Algo::FindByPredicate(LayoutRowViews, 
				[LayoutRow](const TWeakPtr<SDMXControlConsoleEditorLayoutRowView>& LayoutRowViewWeakPtr)
				{
					const TSharedPtr<SDMXControlConsoleEditorLayoutRowView> LayoutRowView = LayoutRowViewWeakPtr.Pin();
					return LayoutRowView.IsValid() && LayoutRowView->GetLayoutRow() == LayoutRow;
				});

			if (LayoutRowViewPtr && LayoutRowViewPtr->IsValid())
			{
				return LayoutRowViewPtr->Pin();
			}
		}

		return nullptr;
	}

	void SDMXControlConsoleEditorGridLayout::OnScrollIntoView(const UDMXControlConsoleFaderGroupController* FaderGroupController)
	{
		if (!HorizontalScrollBox.IsValid() || !VerticalScrollBox.IsValid())
		{
			return;
		}

		const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorModel.IsValid() ? EditorModel->GetControlConsoleLayouts() : nullptr;
		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = ControlConsoleLayouts ? ControlConsoleLayouts->GetActiveLayout() : nullptr;
		if (!ActiveLayout || !FaderGroupController)
		{
			return;
		}

		const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = ActiveLayout->GetLayoutRow(FaderGroupController);
		const TSharedPtr<SDMXControlConsoleEditorLayoutRowView> LayoutRowView = FindLayoutRowView(LayoutRow);
		if (LayoutRowView.IsValid())
		{
			const TSharedPtr<SDMXControlConsoleEditorFaderGroupControllerView> FaderGroupControllerView = LayoutRowView->FindFaderGroupControllerView(FaderGroupController);
			if (FaderGroupControllerView.IsValid())
			{
				VerticalScrollBox->ScrollDescendantIntoView(LayoutRowView, true, EDescendantScrollDestination::Center);
				HorizontalScrollBox->ScrollDescendantIntoView(FaderGroupControllerView, true, EDescendantScrollDestination::TopOrLeft);
			}
		}
	}

	EVisibility SDMXControlConsoleEditorGridLayout::GetLayoutRowViewVisibility(TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutRow> LayoutRow) const
	{
		if (!LayoutRow.IsValid())
		{
			return EVisibility::Collapsed;
		}

		const auto IsAnyFaderGroupControllerVisibleLambda = [](const UDMXControlConsoleFaderGroupController* FaderGroupController)
		{
			if (!FaderGroupController)
			{
				return false;
			}

			return FaderGroupController->IsActive() && FaderGroupController->IsMatchingFilter();
		};

		const TArray<UDMXControlConsoleFaderGroupController*>& FaderGroupControllers = LayoutRow->GetFaderGroupControllers();
		const bool bIsVisible = Algo::AnyOf(FaderGroupControllers, IsAnyFaderGroupControllerVisibleLambda);
		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}
}

#undef LOCTEXT_NAMESPACE