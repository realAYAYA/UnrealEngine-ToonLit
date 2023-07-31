// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/StyleColors.h"
#include "Widgets/SCompoundWidget.h"

#define LOCTEXT_NAMESPACE "ObjectMixerEditor"

class SInlineEditableTextBlock;
class SMenuAnchor;
class SObjectMixerEditorMainPanel;
class FObjectMixerEditorListFilter_Collection;

class SCollectionSelectionButton final : public SCompoundWidget
{
public:
	
	SLATE_BEGIN_ARGS(SCollectionSelectionButton)
	{}

	SLATE_END_ARGS()
	
	void Construct(
		const FArguments& InArgs,
		const TSharedRef<SObjectMixerEditorMainPanel> MainPanelWidget,
		const TSharedRef<FObjectMixerEditorListFilter_Collection> InCollectionListFilter);

	TSharedRef<SWidget> GetContextMenu() const;

	void OnEditableTextCommitted(const FText& Text, ETextCommit::Type Type);

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;

	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	const FSlateBrush* GetBorderBrush() const;
	FSlateColor GetBorderForeground() const;

	TWeakPtr<FObjectMixerEditorListFilter_Collection> GetCollectionListFilter()
	{
		return  CollectionListFilter;
	}

	/** Whether the associated list filter is active */
	bool GetIsChecked() const;

	TWeakPtr<SObjectMixerEditorMainPanel> MainPanelPtr;
	TWeakPtr<FObjectMixerEditorListFilter_Collection> CollectionListFilter;

private:

	bool bIsPressed = false;
	bool bDropIsValid = false;

	FSlateRoundedBoxBrush UncheckedImage = FSlateRoundedBoxBrush(FStyleColors::Header, 4.0f, FStyleColors::Input, 1.0f);
	FSlateRoundedBoxBrush UncheckedHoveredImage = FSlateRoundedBoxBrush(FStyleColors::Hover, 4.0f, FStyleColors::Input, 1.0f);
	FSlateRoundedBoxBrush UncheckedPressedImage = FSlateRoundedBoxBrush(FStyleColors::Hover, 4.0f, FStyleColors::Input, 1.0f);
	FSlateRoundedBoxBrush UncheckedValidDropImage = FSlateRoundedBoxBrush(FStyleColors::Secondary, 4.0f, FStyleColors::Input, 1.0f);
	FSlateRoundedBoxBrush CheckedImage = FSlateRoundedBoxBrush(FStyleColors::Primary, 4.0f, FStyleColors::Input, 1.0f);
	FSlateRoundedBoxBrush CheckedHoveredImage = FSlateRoundedBoxBrush(FStyleColors::PrimaryHover, 4.0f, FStyleColors::Input, 1.0f);
	FSlateRoundedBoxBrush CheckedPressedImage = FSlateRoundedBoxBrush(FStyleColors::PrimaryHover, 4.0f, FStyleColors::Input, 1.0f);
	FSlateRoundedBoxBrush CheckedValidDropImage = FSlateRoundedBoxBrush(FStyleColors::Secondary, 4.0f, FStyleColors::Input, 1.0f);
	
	TSharedPtr<SInlineEditableTextBlock> EditableTextBlock;
	TSharedPtr<SMenuAnchor> MenuAnchor;
};

#undef LOCTEXT_NAMESPACE
