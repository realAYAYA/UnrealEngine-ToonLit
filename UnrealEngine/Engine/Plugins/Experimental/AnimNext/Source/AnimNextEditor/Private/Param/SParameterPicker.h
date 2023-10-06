// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Param/ParameterPickerArgs.h"
#include "Widgets/Views/SListView.h"

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

	void RefreshFilter();

	TSharedRef<ITableRow> HandleGenerateRow(TSharedRef<FParameterPickerEntry> InEntry, const TSharedRef<STableViewBase>& InOwnerTable) const;

	void HandleSelectionChanged(TSharedPtr<FParameterPickerEntry> InEntry, ESelectInfo::Type InSelectInfo);
	
	void HandleGetParameterBindings(TArray<FParameterBindingReference>& OutParameterBindings) const;

private:
	FParameterPickerArgs Args;

	TSharedPtr<SListView<TSharedRef<FParameterPickerEntry>>> EntriesList;

	TArray<TSharedRef<FParameterPickerEntry>> Entries;

	TArray<TSharedRef<FParameterPickerEntry>> FilteredEntries;

	FText FilterText;
};

}