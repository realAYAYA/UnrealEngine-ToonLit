// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/ParamType.h"
#include "Widgets/SWindow.h"
#include "AssetRegistry/AssetData.h"
#include "Widgets/Views/SListView.h"

class SWrapBox;
class UAnimNextParameterLibrary;

namespace UE::AnimNext::Editor
{

struct FParameterToAdd
{
	FParameterToAdd() = default;

	FParameterToAdd(const FAnimNextParamType& InType, FName InName, const FAssetData& InLibrary)
		: Type(InType)
		, Name(InName)
		, Library(InLibrary)
	{}

	bool IsValid() const
	{
		return Name != NAME_None && Type.IsValid() && Library.IsValid(); 
	}
	
	// Type
	FAnimNextParamType Type;

	// Name for parameter
	FName Name;

	// Parameter library
	FAssetData Library;
};

class SAddParametersDialog : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SAddParametersDialog)
		: _Library(nullptr)
	{}

	SLATE_ARGUMENT(UAnimNextParameterLibrary*, Library)
	
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	bool ShowModal(TArray<FParameterToAdd>& OutParameters);

private:
	void AddEntry();

	void RefreshEntries();

	TSharedRef<ITableRow> HandleGenerateRow(TSharedRef<FParameterToAdd> InEntry, const TSharedRef<STableViewBase>& InOwnerTable);

	TSharedRef<SWidget> HandleGetAddParameterMenuContent(TSharedPtr<FParameterToAdd> InEntry);
	
private:
	friend class SParameterToAdd;

	TSharedPtr<SWrapBox> QueuedParametersBox;

	TSharedPtr<SListView<TSharedRef<FParameterToAdd>>> EntriesList;

	TArray<TSharedRef<FParameterToAdd>> Entries;

	// The fixed library to use. If this is NULL, any library can be used.
	UAnimNextParameterLibrary* Library = nullptr;
	
	bool bCancelPressed = false;
};

}
