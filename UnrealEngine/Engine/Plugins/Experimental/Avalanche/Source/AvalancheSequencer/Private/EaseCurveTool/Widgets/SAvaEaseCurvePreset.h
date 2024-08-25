// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EaseCurveTool/AvaEaseCurveTangents.h"
#include "EaseCurveTool/Widgets/SAvaEaseCurvePresetGroup.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"

class FName;
class FText;
class SUniformWrapPanel;
struct FAvaEaseCurvePreset;

DECLARE_DELEGATE_OneParam(FAvaOnPresetChanged, const TSharedPtr<FAvaEaseCurvePreset>&)
DECLARE_DELEGATE_RetVal_OneParam(bool, FAvaOnGetNewPresetTangents, FAvaEaseCurveTangents& /*InTangents*/)

class SAvaEaseCurvePreset : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaEaseCurvePreset)
		: _DisplayRate(FFrameRate(30, 1))
	{}
		SLATE_ATTRIBUTE(FFrameRate, DisplayRate)
		SLATE_EVENT(FAvaOnPresetChanged, OnPresetChanged)
		SLATE_EVENT(FAvaOnPresetChanged, OnQuickPresetChanged)
		SLATE_EVENT(FAvaOnGetNewPresetTangents, OnGetNewPresetTangents)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	void ClearSelection();
	bool GetSelectedItem(FAvaEaseCurvePreset& OutPreset) const;
	bool SetSelectedItem(const FString& InName);
	bool SetSelectedItem(const FAvaEaseCurvePreset& InPreset);
	bool SetSelectedItem(const FAvaEaseCurveTangents& InTangents);

protected:
	TSharedRef<SWidget> ConstructPresetComboBox();
	TSharedRef<SWidget> GeneratePresetDropdown();
	
	TSharedRef<SWidget> GenerateSelectedRowWidget() const;

	void UpdateGroupsContent();
	void RegenerateGroupWrapBox();

	FReply OnCreateNewPresetClick();
	FReply OnCancelNewPresetClick();
	FReply OnDeletePresetClick();

	FReply OnNewPresetKeyDownHandler(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent);
	void OnNewPresetTextCommitted(const FText& InNewText, ETextCommit::Type InCommitType);

	void OnSearchTextChanged(const FText& InSearchText);

	FReply ReloadJsonPresets();
	FReply ExploreJsonPresetsFolder();
	FReply CreateNewCategory();

	void ToggleEditMode(const ECheckBoxState bInNewState);

	bool HandleCategoryDelete(const FString& InCategoryName);
	bool HandleCategoryRename(const FString& InCategoryName, const FString& InNewName);
	bool HandlePresetDelete(const TSharedPtr<FAvaEaseCurvePreset>& InPreset);
	bool HandlePresetRename(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FString& InNewName);
	bool HandleBeginPresetMove(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FString& InNewCategoryName);
	bool HandleEndPresetMove(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FString& InNewCategoryName);
	bool HandlePresetClick(const TSharedPtr<FAvaEaseCurvePreset>& InPreset, const FModifierKeysState& InModifierKeys);

	TAttribute<FFrameRate> DisplayRate;

	FAvaOnPresetChanged OnPresetChanged;
	FAvaOnPresetChanged OnQuickPresetChanged;
	FAvaOnGetNewPresetTangents OnGetNewPresetTangents;

	TSharedPtr<SBox> GroupWidgetsParent;
	TSharedPtr<SUniformWrapPanel> GroupWrapBox;
	TArray<TSharedPtr<SAvaEaseCurvePresetGroup>> GroupWidgets;

	SHorizontalBox::FSlot* ComboBoxSlot = nullptr;
	TSharedPtr<SEditableTextBox> NewPresetNameTextBox;

	bool bIsCreatingNewPreset = false;
	TAttribute<bool> bIsInEditMode = false;

	TSharedPtr<FAvaEaseCurvePreset> SelectedItem;

	FText SearchText;
};
