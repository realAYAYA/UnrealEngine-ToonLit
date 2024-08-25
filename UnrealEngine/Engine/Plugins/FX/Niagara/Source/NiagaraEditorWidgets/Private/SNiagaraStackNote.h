// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ViewModels/Stack/NiagaraStackNote.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

/** A possibly interactable widget showing a note. On hover, summons the note as a tooltip. On clicked, toggles between inline & full display */
class NIAGARAEDITORWIDGETS_API SNiagaraStackInlineNote : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraStackInlineNote)
		: _bInteractable(true)
		{}
	
		/** If true, a button will be added. If false, it will be a simple image.  */
		SLATE_ARGUMENT(bool, bInteractable)	
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UNiagaraStackEntry* InStackEntry);

	void UpdateTooltip();
private:
	FReply OnClicked() const;

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
private:
	TWeakObjectPtr<UNiagaraStackEntry> StackEntry;
	bool bInteractable = true;
};

/** The full stack note widget displayed in the stack. Features editable title, message & toggle inline display button. */
class NIAGARAEDITORWIDGETS_API SNiagaraStackNote : public SCompoundWidget
{
public:	
	SLATE_BEGIN_ARGS(SNiagaraStackNote)
		: _bShowEditTextButtons(true)
		{}
		SLATE_ATTRIBUTE(bool, bShowEditTextButtons)
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs, UNiagaraStackNote& StackNote);
	virtual ~SNiagaraStackNote() override;
	
	void Rebuild();

	void FillRowContextMenu(FMenuBuilder& MenuBuilder);
private:
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	
	TOptional<FNiagaraStackNoteData> GetStackNoteData() const;
	
	void EditHeaderText() const;
	void EditBodyText() const;
	
	void CommitStackNoteHeaderUpdate(const FText& Text, ETextCommit::Type Arg);
	void CommitStackNoteBodyUpdate(const FText& Text, ETextCommit::Type Arg);
	
	void ToggleInlineDisplay() const;
	void DeleteStackNote() const;
	
	FText GetStackNoteHeader() const;
	FText GetStackNoteBody() const;

private:
	FReply OnToggleInlineDisplayClicked() const;

	FReply OnEditHeaderButtonClicked();
	FReply OnEditBodyButtonClicked();
	
	EVisibility GetEditNoteHeaderButtonVisibility() const;
	EVisibility GetEditNoteBodyButtonVisibility() const;

private:
	TWeakObjectPtr<UNiagaraStackNote> StackNote;
private:
	TSharedPtr<SExpandableArea> ExpandableArea;
	TSharedPtr<SInlineEditableTextBlock> HeaderText;
	TSharedPtr<SInlineEditableTextBlock> BodyText;

	TAttribute<bool> ShowEditTextButtons;
};
