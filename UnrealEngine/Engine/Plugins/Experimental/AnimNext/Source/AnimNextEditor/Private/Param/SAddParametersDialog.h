// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/ParamType.h"
#include "Widgets/SWindow.h"
#include "AssetRegistry/AssetData.h"
#include "Widgets/Views/SListView.h"
#include "Param/ParameterPickerArgs.h"

class SWrapBox;
class UAnimNextParameterLibrary;
class UAnimNextParameterBlock_EditorData;

namespace UE::AnimNext::Editor
{
	struct FParameterToAdd;


class SAddParametersDialog : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SAddParametersDialog)
		: _Block(nullptr),
		_AllowMultiple(true)
	{}

	SLATE_ARGUMENT(UAnimNextParameterBlock_EditorData*, Block)

	/** Whether we allow multiple parameters to be added or just one at a time */
	SLATE_ARGUMENT(bool, AllowMultiple)

	/** Delegate called to filter parameters by type for display to the user */
	SLATE_EVENT(FOnFilterParameterType, OnFilterParameterType)

	/** Initial parameter type to use */
	SLATE_ARGUMENT(FAnimNextParamType, InitialParamType)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	bool ShowModal(TArray<FParameterToAdd>& OutParameters);

private:
	void AddEntry(const FAnimNextParamType& InParamType = FAnimNextParamType());

	void RefreshEntries();

	TSharedRef<ITableRow> HandleGenerateRow(TSharedRef<FParameterToAdd> InEntry, const TSharedRef<STableViewBase>& InOwnerTable);

	TSharedRef<SWidget> HandleGetAddParameterMenuContent(TSharedPtr<FParameterToAdd> InEntry);
	
private:
	friend class SParameterToAdd;

	TSharedPtr<SWrapBox> QueuedParametersBox;

	TSharedPtr<SListView<TSharedRef<FParameterToAdd>>> EntriesList;

	TArray<TSharedRef<FParameterToAdd>> Entries;

	UAnimNextParameterBlock_EditorData* TargetBlock = nullptr;
	
	FOnFilterParameterType OnFilterParameterType;

	bool bCancelPressed = false;
};

}
