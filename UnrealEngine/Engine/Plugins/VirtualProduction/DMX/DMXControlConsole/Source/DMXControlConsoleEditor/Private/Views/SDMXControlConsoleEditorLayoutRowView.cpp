// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorLayoutRowView.h"

#include "Algo/Find.h"
#include "DMXControlConsoleFaderGroup.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Views/SDMXControlConsoleEditorFaderGroupView.h"
#include "Widgets/SBoxPanel.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorLayoutRowView"

void SDMXControlConsoleEditorLayoutRowView::Construct(const FArguments& InArgs, const TObjectPtr<UDMXControlConsoleEditorGlobalLayoutRow>& InLayoutRow)
{
	LayoutRow = InLayoutRow;

	if (!ensureMsgf(LayoutRow.IsValid(), TEXT("Invalid layout row, cannot create fader group row view correctly.")))
	{
		return;
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	EditorConsoleModel->GetOnControlConsoleForceRefresh().AddSP(this, &SDMXControlConsoleEditorLayoutRowView::Refresh);

	ChildSlot
		[
			SAssignNew(FaderGroupsHorizontalBox, SHorizontalBox)
		];
}

TSharedPtr<SDMXControlConsoleEditorFaderGroupView> SDMXControlConsoleEditorLayoutRowView::FindFaderGroupView(const UDMXControlConsoleFaderGroup* FaderGroup) const
{
	if (FaderGroup)
	{
		const TWeakPtr<SDMXControlConsoleEditorFaderGroupView>* FaderGroupViewPtr = Algo::FindByPredicate(FaderGroupViews, [FaderGroup](const TWeakPtr<SDMXControlConsoleEditorFaderGroupView>& WeakFaderGroupView)
			{
				return WeakFaderGroupView.IsValid() && WeakFaderGroupView.Pin()->GetFaderGroup() == FaderGroup;
			});

		if (FaderGroupViewPtr)
		{
			return FaderGroupViewPtr->Pin();
		}
	}

	return nullptr;
}

void SDMXControlConsoleEditorLayoutRowView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (!ensureMsgf(LayoutRow.IsValid(), TEXT("Invalid layout row, cannot update fader group row view state correctly.")))
	{
		return;
	}

	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> FaderGroups = LayoutRow->GetFaderGroups();
	if (FaderGroups.Num() == FaderGroupViews.Num())
	{
		return;
	}

	if (FaderGroups.Num() > FaderGroupViews.Num())
	{
		OnFaderGroupAdded();
	}
	else
	{
		OnFaderGroupRemoved();
	}
}

void SDMXControlConsoleEditorLayoutRowView::Refresh()
{
	if (FaderGroupsHorizontalBox.IsValid())
	{
		FaderGroupsHorizontalBox->ClearChildren();
		FaderGroupViews.Reset(FaderGroupViews.Num());
		OnFaderGroupAdded();
		OnFaderGroupRemoved();
	}
}

void SDMXControlConsoleEditorLayoutRowView::OnFaderGroupAdded()
{
	if (!LayoutRow.IsValid())
	{
		return;
	}

	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> FaderGroups = LayoutRow->GetFaderGroups();
	for (const TWeakObjectPtr<UDMXControlConsoleFaderGroup>& FaderGroup : FaderGroups)
	{
		if (FaderGroup.IsValid() &&
			!ContainsFaderGroup(FaderGroup.Get()))
		{
			AddFaderGroup(FaderGroup.Get());
		}
	}
}

void SDMXControlConsoleEditorLayoutRowView::AddFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup)
{
	if (!ensureMsgf(FaderGroup, TEXT("Invalid fader group, cannot add new fader group view correctly.")))
	{
		return;
	}

	if (!LayoutRow.IsValid() || !FaderGroupsHorizontalBox.IsValid())
	{
		return;
	}

	const int32 Index = LayoutRow->GetIndex(FaderGroup);

	const TSharedRef<SDMXControlConsoleEditorFaderGroupView> FaderGroupWidget =
		SNew(SDMXControlConsoleEditorFaderGroupView, FaderGroup)
		.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorLayoutRowView::GetFaderGroupViewVisibility, FaderGroup));

	FaderGroupViews.Insert(FaderGroupWidget, Index);

	FaderGroupsHorizontalBox->InsertSlot(Index)
		.AutoWidth()
		.HAlign(HAlign_Left)
		.Padding(8.f, 0.f)
		[
			FaderGroupWidget
		];
}

void SDMXControlConsoleEditorLayoutRowView::OnFaderGroupRemoved()
{
	if (!LayoutRow.IsValid())
	{
		return;
	}

	const TArray<TWeakObjectPtr<UDMXControlConsoleFaderGroup>> FaderGroups = LayoutRow->GetFaderGroups();

	TArray<TWeakPtr<SDMXControlConsoleEditorFaderGroupView>> FaderGroupViewsToRemove;
	for (const TWeakPtr<SDMXControlConsoleEditorFaderGroupView>& FaderGroupView : FaderGroupViews)
	{
		if (!FaderGroupView.IsValid())
		{
			continue;
		}

		const UDMXControlConsoleFaderGroup* FaderGroup = FaderGroupView.Pin()->GetFaderGroup();
		if (!FaderGroup || !FaderGroups.Contains(FaderGroup))
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

bool SDMXControlConsoleEditorLayoutRowView::ContainsFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup)
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
				return OtherFaderGroup == FaderGroup;
			}

			return false;
		}) != nullptr;

	return bContainsFaderGroup;
}

EVisibility SDMXControlConsoleEditorLayoutRowView::GetFaderGroupViewVisibility(UDMXControlConsoleFaderGroup* FaderGroup) const
{
	if (!FaderGroup)
	{
		return EVisibility::Collapsed;
	}

	const bool bIsVisible = FaderGroup->IsActive() && FaderGroup->IsMatchingFilter();
	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
