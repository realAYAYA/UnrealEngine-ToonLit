// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Providers/IAdvancedRenamerProvider.h"
#include "Styling/SlateTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"

class FRegexPattern;
class FUICommandList;
class IAdvancedRenamerProvider;
class SAdvancedRenamerPanel;
class SBox;
class SButton;
class SCanvas;
class SCheckBox;
class SEditableTextBox;
class SHeaderRow;
class SMultiLineEditableTextBox; 
class UObject;
template<typename NumericType> class SSpinBox;

struct FAdvancedRenamerPreviewListItem
{
	static FName OriginalNameColumnName;
	static FName NewNameColumnName;

	FAdvancedRenamerPreviewListItem(int32 InHash, const FString InOriginalName)
		: Hash(InHash)
		, OriginalName(InOriginalName)
		, NewName(FString(""))
	{
	}

	int32 Hash;
	const FString OriginalName;
	mutable FString NewName;
};

using FObjectRenamePreviewListItemPtr = TSharedPtr<FAdvancedRenamerPreviewListItem, ESPMode::ThreadSafe>;
using FObjectRenamePreviewListItemWeakPtr = TWeakPtr<FAdvancedRenamerPreviewListItem, ESPMode::ThreadSafe>;

class SAdvancedRenamerPreviewListRow : public SMultiColumnTableRow<FObjectRenamePreviewListItemPtr>
{
public:
	SLATE_BEGIN_ARGS(SAdvancedRenamerPreviewListRow) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<SAdvancedRenamerPanel> InRenamePanel, 
		const TSharedRef<STableViewBase>& InOwnerTableView, FObjectRenamePreviewListItemPtr InRowItem);

	virtual TSharedRef<SWidget> GenerateWidgetForColumn(const FName& ColumnName) override;

protected:

	TWeakPtr<SAdvancedRenamerPanel> RenamePanel;
	FObjectRenamePreviewListItemWeakPtr RowItem;

};

/**
 * Implements its own provider interface so it can avoid long Execute_ lines and handle
 * the 2 different types of provider (SharedPtr and UObject.)
 */
class SAdvancedRenamerPanel : public SCompoundWidget, private IAdvancedRenamerProvider
{
	friend class SAdvancedRenamerPreviewListRow;

public:
	SLATE_BEGIN_ARGS(SAdvancedRenamerPanel) {}
		SLATE_ARGUMENT(TSharedPtr<IAdvancedRenamerProvider>, SharedProvider)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

protected:
	static const inline double MinUpdateFrequency = 0.1;

	TSharedPtr<IAdvancedRenamerProvider> SharedProvider;
	TArray<TSharedPtr<FAdvancedRenamerPreviewListItem>> ListData;
	TSharedPtr<FUICommandList> CommandList;

	bool bValidNames = false;
	bool bListNeedsUpdate = false;
	bool bBaseName = false;
	bool bPrefix = false;
	bool bPrefixRemove = false;
	bool bPrefixRemoveCharacters = false;
	bool bSuffix = false;
	bool bSuffixRemove = false;
	bool bSuffixRemoveCharacters = false;
	bool bSuffixRemoveNumber = false;
	bool bSuffixNumber = false;
	bool bSearchReplacePlainText = false;
	bool bSearchReplaceRegex = false;
	bool bSearchReplaceIgnoreCase = true;

	double ListLastUpdateTime = 0;
	float MinDesiredOriginalNameWidth = 0.f;
	float MinDesiredNewNameWidth = 0.f;

	uint8 PrefixRemoveCharacterCount = 0;
	uint8 SuffixRemoveCharacterCount = 0;

	TSharedPtr<SCheckBox> BaseNameCheckBox;
	TSharedPtr<SEditableTextBox> BaseNameTextBox;
	TSharedPtr<SCheckBox> PrefixCheckBox;
	TSharedPtr<SEditableTextBox> PrefixTextBox;
	TSharedPtr<SCheckBox> PrefixRemoveCheckBox;
	TSharedPtr<SEditableTextBox> PrefixSeparatorTextBox;
	TSharedPtr<SCheckBox> PrefixRemoveCharactersCheckBox;
	TSharedPtr<SSpinBox<uint8>> PrefixRemoveCharactersSpinBox;
	TSharedPtr<SCheckBox> SuffixCheckBox;
	TSharedPtr<SEditableTextBox> SuffixTextBox;
	TSharedPtr<SCheckBox> SuffixRemoveCheckBox;
	TSharedPtr<SEditableTextBox> SuffixSeparatorTextBox;
	TSharedPtr<SCheckBox> SuffixRemoveCharactersCheckBox;
	TSharedPtr<SSpinBox<uint8>> SuffixRemoveCharactersSpinBox;
	TSharedPtr<SCheckBox> SuffixRemoveNumberCheckBox;
	TSharedPtr<SCheckBox> SuffixNumberCheckBox;
	TSharedPtr<SSpinBox<int32>> SuffixNumberStartSpinBox;
	TSharedPtr<SSpinBox<int32>> SuffixNumberStepSpinBox;
	TSharedPtr<SCheckBox> SearchReplacePlainTextCheckbox;
	TSharedPtr<SCheckBox> SearchReplaceRegexCheckbox;
	TSharedPtr<SCheckBox> SearchReplaceIgnoreCaseCheckBox;
	TSharedPtr<SMultiLineEditableTextBox> SearchReplaceSearchTextBox;
	TSharedPtr<SMultiLineEditableTextBox> SearchReplaceReplaceTextBox;
	TSharedPtr<SBox> RenamePreviewListBox;
	TSharedPtr<SHeaderRow> RenamePreviewListHeaderRow;
	TSharedPtr<SListView<FObjectRenamePreviewListItemPtr>> RenamePreviewList;
	TSharedPtr<SButton> ApplyButton;

	void CreateLeftPane(TSharedRef<SCanvas> Canvas);
	TSharedRef<SWidget> CreateBaseName();
	TSharedRef<SWidget> CreatePrefix();
	TSharedRef<SWidget> CreateSuffix();
	TSharedRef<SWidget> CreateSearchAndReplace();

	void CreateRightPane(TSharedRef<SCanvas> Canvas);
	TSharedRef<SWidget> CreateRenamePreview();

	bool RenameObjects();

	bool CloseWindow();

	FString CreateNewName(int32 Index) const;
	FString ApplyRename(const FString& OriginalName, int32 Index) const;
	FString ApplyBaseName(const FString& OriginalName) const;
	FString ApplyPrefix(const FString& OriginalName) const;
	FString ApplySuffix(const FString& OriginalName, int32 Index) const;
	FString ApplySearchPlainText(const FString& OriginalName) const;
	FString ApplySearchReplaceRegex(const FString& OriginalName) const;

	FString RegexReplace(const FString& OriginalString, const FRegexPattern Pattern, const FString& ReplaceString) const;

	void UpdateEnables();
	void RequestListViewRefresh();
	void RefreshListView(const double InCurrentTime);
	void UpdateRequiredListWidth();

	void RemoveSelectedObjects();

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	ECheckBoxState IsBaseNameChecked() const { return bBaseName ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
	void OnBaseNameCheckBoxChanged(ECheckBoxState NewState);

	void OnBaseNameChanged(const FText& NewText);

	ECheckBoxState IsPrefixChecked() const { return bPrefix ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
	void OnPrefixCheckBoxChanged(ECheckBoxState NewState);

	void OnPrefixChanged(const FText& NewText);

	ECheckBoxState IsPrefixRemoveChecked() const { return bPrefixRemove ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
	void OnPrefixRemoveCheckBoxChanged(ECheckBoxState NewState);

	bool OnPrefixSeparatorVerifyTextChanged(const FText& InText, FText& OutErrorText) const;
	void OnPrefixSeparatorChanged(const FText& NewText);

	ECheckBoxState IsPrefixRemoveCharactersChecked() const { return bPrefixRemoveCharacters ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
	void OnPrefixRemoveCharactersCheckBoxChanged(ECheckBoxState NewState);

	void OnPrefixRemoveCharactersChanged(uint8 NewValue);

	ECheckBoxState IsSuffixChecked() const { return bSuffix ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
	void OnSuffixCheckBoxChanged(ECheckBoxState NewState);

	void OnSuffixChanged(const FText& NewText);

	ECheckBoxState IsSuffixRemoveChecked() const { return bSuffixRemove ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
	void OnSuffixRemoveCheckBoxChanged(ECheckBoxState NewState);

	bool OnSuffixSeparatorVerifyTextChanged(const FText& InText, FText& OutErrorText) const;
	void OnSuffixSeparatorChanged(const FText& NewText);

	ECheckBoxState IsSuffixRemoveCharactersChecked() const { return bSuffixRemoveCharacters ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
	void OnSuffixRemoveCharactersCheckBoxChanged(ECheckBoxState NewState);

	void OnSuffixRemoveCharactersChanged(uint8 NewValue);

	ECheckBoxState IsSuffixRemoveNumberChecked() const { return bSuffixRemoveNumber ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
	void OnSuffixRemoveNumberCheckBoxChanged(ECheckBoxState NewState);

	ECheckBoxState IsSuffixNumberChecked() const { return bSuffixNumber ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
	void OnSuffixNumberCheckBoxChanged(ECheckBoxState NewState);

	void OnSuffixNumberStartChanged(int32 NewValue);

	void OnSuffixNumberStepChanged(int32 NewValue);

	ECheckBoxState IsSearchReplacePlainTextChecked() const { return bSearchReplacePlainText ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
	void OnSearchReplacePlainTextCheckBoxChanged(ECheckBoxState NewState);

	ECheckBoxState IsSearchReplaceRegexChecked() const { return bSearchReplaceRegex ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
	void OnSearchReplaceRegexCheckBoxChanged(ECheckBoxState NewState);

	ECheckBoxState IsSearchReplaceIgnoreCaseChecked() const { return bSearchReplaceIgnoreCase ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }
	void OnSearchReplaceIgnoreCaseCheckBoxChanged(ECheckBoxState NewState);

	void OnSearchReplaceSearchTextChanged(const FText& NewText);

	void OnSearchReplaceReplaceTextChanged(const FText& NewText);

	TSharedRef<ITableRow> OnGenerateRowForList(FObjectRenamePreviewListItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable);
	FReply OnListViewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent);
	TSharedPtr<SWidget> GenerateListViewContextMenu();

	FReply OnApplyButtonClicked();

	virtual int32 Num() const override;
	virtual bool IsValidIndex(int32 Index) const override;
	virtual uint32 GetHash(int32 Index) const override;
	virtual FString GetOriginalName(int32 Index) const override;
	virtual bool RemoveIndex(int32 Index) override;
	virtual bool CanRename(int32 Index) const override;
	virtual bool ExecuteRename(int32 Index, const FString& NewName) override;
};
