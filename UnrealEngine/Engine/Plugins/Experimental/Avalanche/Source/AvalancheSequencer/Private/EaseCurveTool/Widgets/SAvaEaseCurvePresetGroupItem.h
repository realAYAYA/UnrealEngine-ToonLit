// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameRate.h"
#include "Templates/SharedPointer.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class FText;
class SEditableTextBox;
class STableViewBase;
struct FAvaEaseCurvePreset;

DECLARE_DELEGATE_RetVal_OneParam(bool, FAvaEaseCurvePresetDelegate, const TSharedPtr<FAvaEaseCurvePreset>& /*InPreset*/)
DECLARE_DELEGATE_RetVal_TwoParams(bool, FAvaEaseCurvePresetClickDelegate, const TSharedPtr<FAvaEaseCurvePreset>& /*InPreset*/, const FModifierKeysState& /*InModifierKeys*/)
DECLARE_DELEGATE_RetVal_TwoParams(bool, FAvaEaseCurvePresetRenameDelegate, const TSharedPtr<FAvaEaseCurvePreset>& /*InPreset*/, const FString& /*InNewName*/)
DECLARE_DELEGATE_RetVal_TwoParams(bool, FAvaEaseCurvePresetMoveDelegate, const TSharedPtr<FAvaEaseCurvePreset>& /*InPreset*/, const FString& /*InNewCategoryName*/)

class SAvaEaseCurvePresetGroupItem : public STableRow<TSharedPtr<FAvaEaseCurvePreset>>
{
public:
	SLATE_BEGIN_ARGS(SAvaEaseCurvePresetGroupItem)
		: _DisplayRate(FFrameRate(30, 1))
	{}
		SLATE_ARGUMENT(TSharedPtr<FAvaEaseCurvePreset>, Preset)
		SLATE_ATTRIBUTE(bool, IsEditMode)
		SLATE_ARGUMENT(FFrameRate, DisplayRate)
		SLATE_ATTRIBUTE(bool, IsSelected)
		SLATE_EVENT(FAvaEaseCurvePresetClickDelegate, OnClick)
		SLATE_EVENT(FAvaEaseCurvePresetDelegate, OnDelete)
		SLATE_EVENT(FAvaEaseCurvePresetRenameDelegate, OnRename)
		SLATE_EVENT(FAvaEaseCurvePresetMoveDelegate, OnBeginMove)
		SLATE_EVENT(FAvaEaseCurvePresetMoveDelegate, OnEndMove)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedPtr<STableViewBase>& InOwnerTableView);

	void SetPreset(const TSharedPtr<FAvaEaseCurvePreset>& InPreset);

	bool IsEditMode() const;

	void TriggerBeginMove();
	void TriggerEndMove();

protected:
	void HandlePresetClick() const;

	void HandleRenameTextCommitted(const FText& InNewText, ETextCommit::Type InCommitType) const;

	FReply HandleDeleteClick() const;

	EVisibility GetEditModeVisibility() const;

	EVisibility GetBorderVisibility() const;
	const FSlateBrush* GetBackgroundImage() const;

	EVisibility GetQuickPresetIconVisibility() const;
	FText GetQuickPresetIconToolTip() const;

	//~ Begin SWidget
	virtual FReply OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnDragDetected(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent) override;
	//~ End SWidget

	TSharedPtr<FAvaEaseCurvePreset> Preset;
	TAttribute<bool> bIsEditMode;
	TAttribute<bool> IsSelected;
	FAvaEaseCurvePresetClickDelegate OnClick;
	FAvaEaseCurvePresetDelegate OnDelete;
	FAvaEaseCurvePresetRenameDelegate OnRename;
	FAvaEaseCurvePresetMoveDelegate OnBeginMove;
	FAvaEaseCurvePresetMoveDelegate OnEndMove;

	TSharedPtr<SEditableTextBox> RenameTextBox;

	bool bIsDragging = false;
};
