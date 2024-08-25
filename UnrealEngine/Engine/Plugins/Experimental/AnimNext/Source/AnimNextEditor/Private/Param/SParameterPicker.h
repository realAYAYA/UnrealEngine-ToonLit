// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Param/ParameterPickerArgs.h"
#include "Widgets/Views/STreeView.h"

class SSearchBox;

namespace UE::AnimNext::Editor
{

struct FParameterPickerEntry;

class SParameterPicker : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SParameterPicker) {}

	SLATE_ARGUMENT(FParameterPickerArgs, Args)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void RefreshEntries();

	void BuildHierarchy();

	void RefreshFilter();

	TSharedRef<ITableRow> HandleGenerateRow(TSharedRef<FParameterPickerEntry> InEntry, const TSharedRef<STableViewBase>& InOwnerTable) const;

	void HandleGetChildren(TSharedRef<FParameterPickerEntry> InEntry, TArray<TSharedRef<FParameterPickerEntry>>& OutChildren) const;

	void HandleSelectionChanged(TSharedPtr<FParameterPickerEntry> InEntry, ESelectInfo::Type InSelectInfo);

	void HandleGetParameterBindings(TArray<FParameterBindingReference>& OutParameterBindings) const;

	bool HandleIsSelectableOrNavigable(TSharedRef<FParameterPickerEntry> InEntry) const;

private:
	friend class SParameterPickerRow;

	FParameterPickerArgs Args;

	TSharedPtr<STreeView<TSharedRef<FParameterPickerEntry>>> EntriesList;

	TArray<TSharedRef<FParameterPickerEntry>> Entries;

	TArray<TSharedRef<FParameterPickerEntry>> FilteredEntries;

	TArray<TSharedRef<FParameterPickerEntry>> Hierarchy;

	TArray<TSharedRef<FParameterPickerEntry>> FilteredHierarchy;

	FText FilterText;

	TSharedPtr<SSearchBox> SearchBox;
};

}