// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorGridLayout.h"

#include "Algo/AnyOf.h"
#include "Algo/Find.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "Editor.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutDefault.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutUser.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "TimerManager.h"
#include "Views/SDMXControlConsoleEditorLayoutRowView.h"
#include "Views/SDMXControlConsoleEditorFaderGroupView.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorAddButton.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorGridLayout"

namespace UE::DMXControlConsoleEditor::Layout::Private
{
	void SDMXControlConsoleEditorGridLayout::Construct(const FArguments& InArgs, UDMXControlConsoleEditorGlobalLayoutBase* InLayout)
	{
		if (!ensureMsgf(InLayout, TEXT("Invalid layout, cannot create layout view correctly.")))
		{
			return;
		}

		EditorLayout = InLayout;

		UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
		EditorConsoleModel->GetOnConsoleLoaded().AddSP(this, &SDMXControlConsoleEditorGridLayout::Refresh);
		EditorConsoleModel->GetOnControlConsoleForceRefresh().AddSP(this, &SDMXControlConsoleEditorGridLayout::Refresh);
		EditorConsoleModel->GetOnScrollFaderGroupIntoView().AddSP(this, &SDMXControlConsoleEditorGridLayout::OnScrollIntoView);

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
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Center)
							[
								SNew(SBox)
								.WidthOverride(50.f)
								.HeightOverride(50.f)
								.HAlign(HAlign_Center)
								.VAlign(VAlign_Center)
								[
									SNew(SDMXControlConsoleEditorAddButton)
									.OnClicked(this, &SDMXControlConsoleEditorGridLayout::OnAddFirstFaderGroup)
									.Visibility(TAttribute<EVisibility>(this, &SDMXControlConsoleEditorGridLayout::GetAddButtonVisibility))
								]
							]

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
		if (!EditorLayout.IsValid())
		{
			return false;
		}

		const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
		const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
		if (!EditorConsoleLayouts)
		{
			return false;
		}

		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
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
		const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
		const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
		if (!ensureMsgf(EditorConsoleLayouts, TEXT("Invalid Control Console Layouts, can't add new element to layout correctly.")))
		{
			return;
		}

		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
		const TArray<UDMXControlConsoleEditorGlobalLayoutRow*> LayoutRows = ActiveLayout->GetLayoutRows();
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
				SNew(SDMXControlConsoleEditorLayoutRowView, LayoutRow)
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
		const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
		const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
		if (!ensureMsgf(EditorConsoleLayouts, TEXT("Invalid Control Console Layouts, can't delete layout row correctly.")))
		{
			return;
		}

		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
		const TArray<UDMXControlConsoleEditorGlobalLayoutRow*> LayoutRows = ActiveLayout->GetLayoutRows();

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

		LayoutRowViews.RemoveAll([&LayoutRowViewsToRemove](const TWeakPtr<SDMXControlConsoleEditorLayoutRowView> LayoutRowView)
			{
				return !LayoutRowView.IsValid() || LayoutRowViewsToRemove.Contains(LayoutRowView);
			});
	}

	bool SDMXControlConsoleEditorGridLayout::IsLayoutRowContained(UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow)
	{
		const TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutRow> LayoutRowWeakPtr = LayoutRow;

		auto IsContainedLambda = [LayoutRowWeakPtr](const TWeakPtr<SDMXControlConsoleEditorLayoutRowView> LayoutRowView)
		{
			if (!LayoutRowView.IsValid())
			{
				return false;
			}

			const TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutRow> LayoutRow = LayoutRowView.Pin()->GetLayoutRow();
			if (!LayoutRow.IsValid())
			{
				return false;
			}

			return LayoutRow == LayoutRowWeakPtr;
		};

		return LayoutRowViews.ContainsByPredicate(IsContainedLambda);
	}

	TSharedPtr<SDMXControlConsoleEditorLayoutRowView> SDMXControlConsoleEditorGridLayout::FindLayoutRowView(const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow)
	{
		if (LayoutRow)
		{
			const TWeakPtr<SDMXControlConsoleEditorLayoutRowView>* LayoutRowViewPtr = Algo::FindByPredicate(LayoutRowViews, [LayoutRow](const TWeakPtr<SDMXControlConsoleEditorLayoutRowView>& WeakLayoutRowView)
				{
					return WeakLayoutRowView.IsValid() && WeakLayoutRowView.Pin()->GetLayoutRow() == LayoutRow;
				});

			if (LayoutRowViewPtr && LayoutRowViewPtr->IsValid())
			{
				return LayoutRowViewPtr->Pin();
			}
		}

		return nullptr;
	}

	FReply SDMXControlConsoleEditorGridLayout::OnAddFirstFaderGroup()
	{
		const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
		UDMXControlConsoleData* ControlConsoleData = EditorConsoleModel->GetEditorConsoleData();
		if (!ensureMsgf(ControlConsoleData, TEXT("Invalid Control Console Data, can't add fader group correctly.")))
		{
			return FReply::Unhandled();
		}

		const FScopedTransaction AddFaderGroupTransaction(LOCTEXT("AddFaderGroupTransaction", "Add Fader Group"));
		ControlConsoleData->PreEditChange(nullptr);
		const UDMXControlConsoleFaderGroupRow* NewRow = ControlConsoleData->AddFaderGroupRow(0);
		ControlConsoleData->PostEditChange();

		const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
		if (!ensureMsgf(EditorConsoleLayouts, TEXT("Invalid Control Console Layouts, can't add fader group correctly.")))
		{
			return FReply::Unhandled();
		}

		UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return FReply::Unhandled();
		}

		ActiveLayout->PreEditChange(nullptr);
		UDMXControlConsoleEditorGlobalLayoutRow* NewLayoutRow = ActiveLayout->AddNewRowToLayout(0);
		ActiveLayout->PostEditChange();
		if (!NewRow || !NewLayoutRow)
		{
			return FReply::Unhandled();
		}

		UDMXControlConsoleFaderGroup* NewFaderGroup = !NewRow->GetFaderGroups().IsEmpty() ? NewRow->GetFaderGroups()[0] : nullptr;
		NewLayoutRow->PreEditChange(nullptr);
		NewLayoutRow->AddToLayoutRow(NewFaderGroup);
		NewLayoutRow->PostEditChange();

		return FReply::Handled();
	}

	void SDMXControlConsoleEditorGridLayout::OnScrollIntoView(const UDMXControlConsoleFaderGroup* FaderGroup)
	{
		const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
		const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
		if (!EditorConsoleLayouts || !FaderGroup)
		{
			return;
		}

		if (!HorizontalScrollBox.IsValid() || !VerticalScrollBox.IsValid())
		{
			return;
		}

		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return;
		}

		const int32 RowIndex = ActiveLayout->GetFaderGroupRowIndex(FaderGroup);
		const UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = ActiveLayout->GetLayoutRow(RowIndex);
		const TSharedPtr<SDMXControlConsoleEditorLayoutRowView> LayoutRowView = FindLayoutRowView(LayoutRow);
		if (LayoutRowView.IsValid())
		{
			const TSharedPtr<SDMXControlConsoleEditorFaderGroupView> FaderGroupView = LayoutRowView->FindFaderGroupView(FaderGroup);
			if (FaderGroupView.IsValid())
			{
				VerticalScrollBox->ScrollDescendantIntoView(LayoutRowView, true, EDescendantScrollDestination::Center);
				HorizontalScrollBox->ScrollDescendantIntoView(FaderGroupView, true, EDescendantScrollDestination::TopOrLeft);
			}
		}
	}

	EVisibility SDMXControlConsoleEditorGridLayout::GetLayoutRowViewVisibility(TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutRow> LayoutRow) const
	{
		if (!LayoutRow.IsValid())
		{
			return EVisibility::Collapsed;
		}

		const auto IsAnyFaderGroupVisibleLambda = [](const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup)
		{
			if (!FaderGroup.IsValid())
			{
				return false;
			}

			return FaderGroup->IsActive() && FaderGroup->IsMatchingFilter();
		};

		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> FaderGroups = LayoutRow->GetFaderGroups();
		const bool bIsVisible = Algo::AnyOf(FaderGroups, IsAnyFaderGroupVisibleLambda);
		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorGridLayout::GetAddButtonVisibility() const
	{
		bool bIsVisible = false;

		// Visible if there are no layout rows and there's no global filter
		const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
		const UDMXControlConsoleData* EditorConsoleData = EditorConsoleModel->GetEditorConsoleData();
		const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
		if (EditorConsoleData && EditorConsoleLayouts)
		{
			const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
			bIsVisible =
				IsValid(ActiveLayout) &&
				ActiveLayout->GetClass() != UDMXControlConsoleEditorGlobalLayoutDefault::StaticClass() &&
				EditorConsoleData->FilterString.IsEmpty() &&
				(ActiveLayout->GetLayoutRows().IsEmpty() ||
				ActiveLayout->GetAllActiveFaderGroups().IsEmpty());
		}

		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}
}

#undef LOCTEXT_NAMESPACE