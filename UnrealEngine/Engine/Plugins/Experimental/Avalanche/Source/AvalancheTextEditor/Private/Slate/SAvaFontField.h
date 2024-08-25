// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

class FAvaFontView;
class SAvaFontSelector;
class STextBlock;
enum class ECheckBoxState : uint8;

/**
 * Represents a font selection field 
 */
DECLARE_DELEGATE_OneParam(FOnAvaFontFieldModified, const TSharedPtr<FAvaFontView>&)

class SAvaFontField : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaFontField) {}
		SLATE_ATTRIBUTE(TSharedPtr<FAvaFontView>, FontView)
		SLATE_EVENT(FOnAvaFontFieldModified, OnFontFieldModified)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void UpdateFont(const TSharedPtr<FAvaFontView>& InFontView);
	void Select() { bIsSelected = true; }
	void Deselect() { bIsSelected = false; }

protected:
	ECheckBoxState GetFavoriteState() const;
	FSlateColor GetToggleFavoriteColor() const;
	FReply OnToggleFavoriteClicked();
	EVisibility GetFavoriteVisibility() const;
	EVisibility GetLocallyAvailableIconVisibility() const;

	bool IsSelected() const { return bIsSelected; }

	FOnAvaFontFieldModified OnFontFieldModified;

	FText FontName;

	TSharedPtr<STextBlock> LeftFontNameText;
	TSharedPtr<STextBlock> RightFontNameText;

	TAttribute<TSharedPtr<FAvaFontView>> FontView;

	TSharedPtr<FSlateBrush> CheckBoxBg;

	bool bIsSelected = false;
};
