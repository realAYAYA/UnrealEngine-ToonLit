// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EaseCurveTool/Widgets/SAvaEaseCurvePresetGroupItem.h"
#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

class STableViewBase;
struct FAvaEaseCurvePreset;

DECLARE_DELEGATE_RetVal_OneParam(bool, FAvaEaseCurveCategoryDeleteDelegate, const FString& /*InCategoryName*/)
DECLARE_DELEGATE_RetVal_TwoParams(bool, FAvaEaseCurveCategoryRenameDelegate, const FString& /*InOldName*/, const FString& /*InNewName*/)

class SAvaEaseCurvePresetGroup : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaEaseCurvePresetGroup)
		: _DisplayRate(FFrameRate(30, 1))
	{}
		SLATE_ARGUMENT(FString, CategoryName)
		SLATE_ARGUMENT(TArray<TSharedPtr<FAvaEaseCurvePreset>>, Presets)
		SLATE_ATTRIBUTE(TSharedPtr<FAvaEaseCurvePreset>, SelectedPreset)
		SLATE_ARGUMENT(FText, SearchText)
		SLATE_ATTRIBUTE(bool, IsEditMode)
		SLATE_ARGUMENT(FFrameRate, DisplayRate)
		SLATE_EVENT(FAvaEaseCurveCategoryDeleteDelegate, OnCategoryDelete)
		SLATE_EVENT(FAvaEaseCurveCategoryRenameDelegate, OnCategoryRename)
		SLATE_EVENT(FAvaEaseCurvePresetDelegate, OnPresetDelete)
		SLATE_EVENT(FAvaEaseCurvePresetRenameDelegate, OnPresetRename)
		SLATE_EVENT(FAvaEaseCurvePresetMoveDelegate, OnBeginPresetMove)
		SLATE_EVENT(FAvaEaseCurvePresetMoveDelegate, OnEndPresetMove)
		SLATE_EVENT(FAvaEaseCurvePresetClickDelegate, OnPresetClick)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void SetSearchText(const FText& InText);

	int32 GetVisiblePresetCount() const;

	bool IsEditMode() const;

	void ResetDragBorder();

	void NotifyCanDrop(const bool bInCanDrop) { bCanBeDroppedOn = bInCanDrop; }

	FString GetCategoryName() const { return CategoryName; }

	bool IsSelected(const TSharedPtr<FAvaEaseCurvePreset> InPreset) const;

protected:
	TSharedRef<SWidget> ConstructHeader();

	TSharedRef<ITableRow> GeneratePresetWidget(const TSharedPtr<FAvaEaseCurvePreset> InPreset, const TSharedRef<STableViewBase>& InOwnerTable);
	
	EVisibility GetEditModeVisibility() const;

	const FSlateBrush* GetBorderImage() const;

	FText GetPresetNameTooltipText() const;

	void HandleCategoryRenameCommitted(const FText& InNewText, ETextCommit::Type InCommitType);
	FReply HandleCategoryDelete() const;

	bool HandlePresetClick(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FModifierKeysState& InModifierKeys) const;
	bool HandlePresetDelete(const TSharedPtr<FAvaEaseCurvePreset>& InPreset);
	bool HandlePresetRename(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FString& InNewName);
	bool HandlePresetBeginMove(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FString& InNewCategoryName) const;
	bool HandlePresetEndMove(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FString& InNewCategoryName) const;

	//~ Begin SWidget
	virtual void OnDragEnter(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& InDragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent) override;
	//~ End SWidget

	FString CategoryName;
	TArray<TSharedPtr<FAvaEaseCurvePreset>> Presets;
	FText SearchText;
	TAttribute<bool> bIsEditMode;
	FFrameRate DisplayRate;
	TAttribute<TSharedPtr<FAvaEaseCurvePreset>> SelectedPreset;
	FAvaEaseCurveCategoryDeleteDelegate OnCategoryDelete;
	FAvaEaseCurveCategoryRenameDelegate OnCategoryRename;
	FAvaEaseCurvePresetDelegate OnPresetDelete;
	FAvaEaseCurvePresetRenameDelegate OnPresetRename;
	FAvaEaseCurvePresetMoveDelegate OnBeginPresetMove;
	FAvaEaseCurvePresetMoveDelegate OnEndPresetMove;
	FAvaEaseCurvePresetClickDelegate OnPresetClick;

	TArray<TSharedPtr<FAvaEaseCurvePreset>> VisiblePresets;

	TSharedPtr<SEditableTextBox> RenameCategoryNameTextBox;
	TMap<TSharedPtr<FAvaEaseCurvePreset>, TSharedPtr<SAvaEaseCurvePresetGroupItem>> PresetWidgetsMap;

	bool bCanBeDroppedOn = false;
	bool bIsOverDifferentCategory = false;
};
