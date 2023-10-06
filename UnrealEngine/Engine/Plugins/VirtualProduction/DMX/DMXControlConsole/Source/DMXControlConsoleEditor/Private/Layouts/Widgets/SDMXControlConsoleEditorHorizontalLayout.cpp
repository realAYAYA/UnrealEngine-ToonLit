// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorHorizontalLayout.h"

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
#include "Views/SDMXControlConsoleEditorFaderGroupView.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SDMXControlConsoleEditorAddButton.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorHorizontalLayout"

namespace UE::DMXControlConsoleEditor::Layout::Private
{
	void SDMXControlConsoleEditorHorizontalLayout::Construct(const FArguments& InArgs, UDMXControlConsoleEditorGlobalLayoutBase* InLayout)
	{
		if (!ensureMsgf(InLayout, TEXT("Invalid layout, cannot create layout view correctly.")))
		{
			return;
		}

		EditorLayout = InLayout;

		UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
		EditorConsoleModel->GetOnConsoleLoaded().AddSP(this, &SDMXControlConsoleEditorHorizontalLayout::Refresh);
		EditorConsoleModel->GetOnControlConsoleForceRefresh().AddSP(this, &SDMXControlConsoleEditorHorizontalLayout::Refresh);
		EditorConsoleModel->GetOnScrollFaderGroupIntoView().AddSP(this, &SDMXControlConsoleEditorHorizontalLayout::OnScrollIntoView);

		ChildSlot
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					[
						SAssignNew(HorizontalScrollBox, SScrollBox)
						.Orientation(Orient_Horizontal)

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
								.OnClicked(this, &SDMXControlConsoleEditorHorizontalLayout::OnAddFirstFaderGroup)
								.Visibility(TAttribute<EVisibility>(this, &SDMXControlConsoleEditorHorizontalLayout::GetAddButtonVisibility))
							]
						]

						+ SScrollBox::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.AutoSize()
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.AutoHeight()
							.VAlign(VAlign_Center)
							.Padding(0.f, 8.f)
							[
								SAssignNew(FaderGroupsHorizontalBox, SHorizontalBox)
							]
						]
					]
				]
			];
	}

	bool SDMXControlConsoleEditorHorizontalLayout::CanRefresh() const
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

		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = ActiveLayout->GetAllFaderGroups();
		if (AllFaderGroups.Num() == FaderGroupViews.Num())
		{
			return false;
		}

		return true;
	}

	void SDMXControlConsoleEditorHorizontalLayout::OnLayoutElementAdded()
	{
		const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
		const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
		if (!ensureMsgf(EditorConsoleLayouts, TEXT("Invalid Control Console Layouts, can't add new element to layout correctly.")))
		{
			return;
		}

		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return;
		}

		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = ActiveLayout->GetAllFaderGroups();
		for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : AllFaderGroups)
		{
			if (!FaderGroup.IsValid())
			{
				continue;
			}

			if (IsFaderGroupContained(FaderGroup.Get()))
			{
				continue;
			}

			const int32 Index = AllFaderGroups.IndexOfByKey(FaderGroup);

			const TSharedRef<SDMXControlConsoleEditorFaderGroupView> FaderGroupWidget =
				SNew(SDMXControlConsoleEditorFaderGroupView, FaderGroup.Get())
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorHorizontalLayout::GetFaderGroupViewVisibility, FaderGroup.Get()));

			FaderGroupViews.Insert(FaderGroupWidget, Index);

			FaderGroupsHorizontalBox->InsertSlot(Index)
				.AutoWidth()
				.HAlign(HAlign_Left)
				.Padding(8.f, 0.f)
				[
					FaderGroupWidget
				];
		}
	}

	void SDMXControlConsoleEditorHorizontalLayout::OnLayoutElementRemoved()
	{
		const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
		const UDMXControlConsoleEditorLayouts* EditorConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
		if (!ensureMsgf(EditorConsoleLayouts, TEXT("Invalid DMX Control Console, can't delete fader group row correctly.")))
		{
			return;
		}

		const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout = EditorConsoleLayouts->GetActiveLayout();
		if (!ActiveLayout)
		{
			return;
		}

		const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> AllFaderGroups = ActiveLayout->GetAllFaderGroups();

		TArray<TWeakPtr<SDMXControlConsoleEditorFaderGroupView>>FaderGroupViewsToRemove;
		for (TWeakPtr<SDMXControlConsoleEditorFaderGroupView>& FaderGroupView : FaderGroupViews)
		{
			if (!FaderGroupView.IsValid())
			{
				continue;
			}

			const UDMXControlConsoleFaderGroup* FaderGroup = FaderGroupView.Pin()->GetFaderGroup();
			if (!FaderGroup || !AllFaderGroups.Contains(FaderGroup))
			{
				FaderGroupsHorizontalBox->RemoveSlot(FaderGroupView.Pin().ToSharedRef());
				FaderGroupViewsToRemove.Add(FaderGroupView);
			}
		}

		FaderGroupViews.RemoveAll([&FaderGroupViewsToRemove](const TWeakPtr<SDMXControlConsoleEditorFaderGroupView> FaderGroupView)
			{
				return !FaderGroupView.IsValid() || FaderGroupViewsToRemove.Contains(FaderGroupView);
			});
	}

	bool SDMXControlConsoleEditorHorizontalLayout::IsFaderGroupContained(UDMXControlConsoleFaderGroup* FaderGroup)
	{
		if (!FaderGroup)
		{
			return false;
		}

		const bool bContainsFaderGroup = Algo::FindByPredicate(FaderGroupViews,
			[FaderGroup](const TWeakPtr<SDMXControlConsoleEditorFaderGroupView> FaderGroupView)
			{
				if (FaderGroupView.IsValid())
				{
					const UDMXControlConsoleFaderGroup* OtherFaderGroup = FaderGroupView.Pin()->GetFaderGroup();
					return FaderGroup == OtherFaderGroup;
				}

				return false;
			}) != nullptr;

		return bContainsFaderGroup;
	}

	FReply SDMXControlConsoleEditorHorizontalLayout::OnAddFirstFaderGroup()
	{
		const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
		UDMXControlConsoleData* ControlConsoleData = EditorConsoleModel->GetEditorConsoleData();
		if (!ensureMsgf(ControlConsoleData, TEXT("Invalid DMX Control Console Data, can't add fader group correctly.")))
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

	void SDMXControlConsoleEditorHorizontalLayout::OnScrollIntoView(const UDMXControlConsoleFaderGroup* FaderGroup)
	{
		if (!FaderGroup ||
			!HorizontalScrollBox.IsValid())
		{
			return;
		}

		const TWeakPtr<SDMXControlConsoleEditorFaderGroupView>* FaderGroupViewPtr = 
			Algo::FindByPredicate(FaderGroupViews, [FaderGroup](TWeakPtr<SDMXControlConsoleEditorFaderGroupView>& FaderGroupView)
			{
					return FaderGroupView.IsValid() && FaderGroupView.Pin()->GetFaderGroup() == FaderGroup;
			});

		if (FaderGroupViewPtr && FaderGroupViewPtr->IsValid())
		{
			HorizontalScrollBox->ScrollDescendantIntoView(FaderGroupViewPtr->Pin(), true, EDescendantScrollDestination::TopOrLeft);
		}
	}

	EVisibility SDMXControlConsoleEditorHorizontalLayout::GetFaderGroupViewVisibility(TWeakObjectPtr<UDMXControlConsoleFaderGroup> FaderGroup) const
	{
		if (!FaderGroup.IsValid())
		{
			return EVisibility::Collapsed;
		}

		const bool bIsVisible = FaderGroup->IsActive() && FaderGroup->IsMatchingFilter();
		return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility SDMXControlConsoleEditorHorizontalLayout::GetAddButtonVisibility() const
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