// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFaderGroupRowView.h"

#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "Views/SDMXControlConsoleEditorFaderGroupView.h"

#include "Widgets/SBoxPanel.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFaderGroupRowView"

void SDMXControlConsoleEditorFaderGroupRowView::Construct(const FArguments& InArgs, const TObjectPtr<UDMXControlConsoleFaderGroupRow>& InFaderGroupRow)
{
	FaderGroupRow = InFaderGroupRow;

	if (!ensureMsgf(FaderGroupRow.IsValid(), TEXT("Invalid fader group row, cannot create fader group row view correctly.")))
	{
		return;
	}

	ChildSlot
		[
			SAssignNew(FaderGroupsHorizontalBox, SHorizontalBox)
		];
}

void SDMXControlConsoleEditorFaderGroupRowView::ApplyGlobalFilter(const FString& InSearchString)
{
	bool bHasVisibleChildren = false;
	for (TWeakPtr<SDMXControlConsoleEditorFaderGroupView>& WeakFaderGroupView : FaderGroupViews)
	{
		if (const TSharedPtr<SDMXControlConsoleEditorFaderGroupView> FaderGroupView = WeakFaderGroupView.Pin())
		{
			FaderGroupView->ApplyGlobalFilter(InSearchString);
			if (FaderGroupView->GetVisibility() == EVisibility::Visible)
			{
				bHasVisibleChildren = true;
			}
		}
	}

	const EVisibility NewVisibility = bHasVisibleChildren ? EVisibility::Visible : EVisibility::Collapsed;
	SetVisibility(NewVisibility);
}

void SDMXControlConsoleEditorFaderGroupRowView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (!ensureMsgf(FaderGroupRow.IsValid(), TEXT("Invalid fader group row, cannot update fader group row view state correctly.")))
	{
		return;
	}

	const TArray<UDMXControlConsoleFaderGroup*> FaderGroups = FaderGroupRow->GetFaderGroups();
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

void SDMXControlConsoleEditorFaderGroupRowView::OnFaderGroupAdded()
{
	const TArray<UDMXControlConsoleFaderGroup*> FaderGroups = FaderGroupRow->GetFaderGroups();

	for (UDMXControlConsoleFaderGroup* FaderGroup : FaderGroups)
	{
		if (!FaderGroup)
		{
			continue;
		}

		if (ContainsFaderGroup(FaderGroup))
		{
			continue;
		}

		AddFaderGroup(FaderGroup);
	}
}

void SDMXControlConsoleEditorFaderGroupRowView::AddFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup)
{
	if (!ensureMsgf(FaderGroup, TEXT("Invalid fader group, cannot add new fader group view correctly.")))
	{
		return;
	}

	if (!FaderGroupsHorizontalBox.IsValid())
	{
		return;
	}

	const int32 Index = FaderGroup->GetIndex();

	TSharedRef<SDMXControlConsoleEditorFaderGroupView> FaderGroupWidget = SNew(SDMXControlConsoleEditorFaderGroupView, FaderGroup);

	FaderGroupViews.Insert(FaderGroupWidget, Index);

	FaderGroupsHorizontalBox->InsertSlot(Index)
		.AutoWidth()
		.HAlign(HAlign_Left)
		.Padding(8.f, 0.f)
		[
			FaderGroupWidget
		];
}

void SDMXControlConsoleEditorFaderGroupRowView::OnFaderGroupRemoved()
{
	const TArray<UDMXControlConsoleFaderGroup*> FaderGroups = FaderGroupRow->GetFaderGroups();

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

bool SDMXControlConsoleEditorFaderGroupRowView::ContainsFaderGroup(UDMXControlConsoleFaderGroup* FaderGroup)
{
	const TWeakObjectPtr<UDMXControlConsoleFaderGroup> FaderGroupWeakPtr = FaderGroup;

	auto IsFaderGroupInUseLambda = [FaderGroupWeakPtr](const TWeakPtr<SDMXControlConsoleEditorFaderGroupView> FaderGroupView)
	{
		if (!FaderGroupView.IsValid())
		{
			return false;
		}

		const TWeakObjectPtr<UDMXControlConsoleFaderGroup> FaderGroup = FaderGroupView.Pin()->GetFaderGroup();
		if (!FaderGroup.IsValid())
		{
			return false;
		}

		return FaderGroup == FaderGroupWeakPtr;
	};

	return FaderGroupViews.ContainsByPredicate(IsFaderGroupInUseLambda);
}

#undef LOCTEXT_NAMESPACE
