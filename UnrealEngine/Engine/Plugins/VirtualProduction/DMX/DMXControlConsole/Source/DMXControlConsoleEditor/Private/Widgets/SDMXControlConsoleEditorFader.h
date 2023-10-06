// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

enum class ECheckBoxState : uint8;
struct FOptionalSize;
struct FSlateColor;
class SButton;
template<typename NumericType> class SDMXControlConsoleEditorSpinBoxVertical;
class SInlineEditableTextBlock;
class UDMXControlConsoleFaderBase;


/** Individual fader UI class */
class SDMXControlConsoleEditorFader
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFader)
	{}

	SLATE_ARGUMENT(FMargin, Padding)

	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs, const TObjectPtr<UDMXControlConsoleFaderBase>& InFader);

	/** Gets the Fader this Fader widget is based on */
	UDMXControlConsoleFaderBase* GetFader() const { return Fader.Get(); }

	/** Sets the value of the fader by a percentage value */
	void SetValueByPercentage(float InNewPercentage);

protected:
	//~ Begin SWidget interface
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	//~ End of SWidget interface

private:
	/** Generates Lock button widget  */
	TSharedRef<SWidget> GenerateLockButtonWidget();

	/** Generates a menu widget for Fader options  */
	TSharedRef<SWidget> GenerateFaderOptionsMenuWidget();

	/** Gets wheter this Fader is selected or not */
	bool IsSelected() const;

	/** Gets wheter this widget should have readonly properties or not */
	bool IsReadOnly() const;

	/** True if this Fader is a Raw Fader */
	bool IsRawFader() const;

	/** Gets the Fader Name */
	FString GetFaderName() const;

	/**  Gets current FaderName */
	FText GetFaderNameText() const;

	/** Gets Fader's value */
	uint32 GetValue() const;

	/** Returns the value as text */
	FText GetValueAsText() const;

	/** Called when a new text on value editable text box is committed */
	void OnValueTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

	/** Gets the Fader's minimum value */
	TOptional<uint32> GetMinValue() const;

	/** Returns the min value as text */
	FText GetMinValueAsText() const;

	/** Called when a new text on min value editable text box is committed */
	void OnMinValueTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

	/** Gets Fader's maximum value */
	TOptional<uint32> GetMaxValue() const;

	/** Returns the max value as text */
	FText GetMaxValueAsText() const;

	/** Called when a new text on max value editable text box is committed */
	void OnMaxValueTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

	/** Handles when the user changes the Fader value */
	void HandleValueChanged(uint32 NewValue);

	/** Called before Fader Value starts to change */
	void OnBeginValueChange();

	/** Called when new Fader Value is committed */
	void OnValueCommitted(uint32 NewValue, ETextCommit::Type CommitType);

	/** Called when mute option is selected */
	void OnMuteFader(bool bMute) const;

	/** Called when remove option is selected */
	void OnRemoveFader() const;

	/** Called when reset option is selected */
	void OnResetFader() const;

	/** Called when lock option is selected */
	void OnLockFader(bool bLock) const;

	/** Called when the delete button was clicked */
	FReply OnDeleteClicked();

	/** Called to lock/unlock this Fader */
	FReply OnLockClicked();

	/** Checks the current mute state of the Fader */
	ECheckBoxState IsMuteChecked() const;

	/** Called to toggle the mute state of this Fader */
	void OnMuteToggleChanged(ECheckBoxState CheckState);

	/** Gets wheter the FaderSpinBox widget should be active or not */
	bool IsFaderSpinBoxActive() const;

	/** Gets the height of the Fader according to the current View Mode  */
	FOptionalSize GetFaderHeightByViewMode() const;

	/** Returns Fader's parameters as tooltip text */
	FText GetToolTipText() const;

	/** Gets correct text for lock button */
	FSlateColor GetLockButtonColor() const;

	/** Gets visibility for expanded view only toolbar sections  */
	EVisibility GetExpandedViewModeVisibility() const;

	/** Gets visibility for lock button  */
	EVisibility GetLockButtonVisibility() const;

	/** Change fader background color on hover */
	const FSlateBrush* GetBorderImage() const;

	/** Change spin box background color on hover */
	const FSlateBrush* GetSpinBoxBorderImage() const;

	/** Reference to Lock button widget */
	TSharedPtr<SButton> LockButton;

	/** The actual editable fader */
	TSharedPtr<SDMXControlConsoleEditorSpinBoxVertical<uint32>> FaderSpinBox;

	/** Reference to the Fader being displayed */
	TWeakObjectPtr<UDMXControlConsoleFaderBase> Fader;

	/** Fader Value before committing */
	uint32 PreCommittedValue;
};
