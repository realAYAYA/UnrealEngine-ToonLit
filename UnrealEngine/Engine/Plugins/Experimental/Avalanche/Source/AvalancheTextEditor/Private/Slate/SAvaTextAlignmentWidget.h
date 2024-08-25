// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

enum class EText3DVerticalTextAlignment : uint8;
enum class EText3DHorizontalTextAlignment : uint8;
class IPropertyHandle;
class SButton;

DECLARE_DELEGATE_OneParam(FOnAvaTextHorizontalAlignmentChanged, EText3DHorizontalTextAlignment)
DECLARE_DELEGATE_OneParam(FOnAvaTextVerticalAlignmentChanged, EText3DVerticalTextAlignment)

class SAvaTextAlignmentWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAvaTextAlignmentWidget)
		:_TextAlignmentPropertyHandle(),
		_OnHorizontalAlignmentChanged(),
		_OnVerticalAlignmentChanged()
		{
		}
	SLATE_ATTRIBUTE(TSharedPtr<IPropertyHandle>, TextAlignmentPropertyHandle)
	SLATE_EVENT(FOnAvaTextHorizontalAlignmentChanged, OnHorizontalAlignmentChanged)
	SLATE_EVENT(FOnAvaTextVerticalAlignmentChanged, OnVerticalAlignmentChanged)

	SLATE_END_ARGS()

	TAttribute<TSharedPtr<IPropertyHandle>> TextAlignmentPropertyHandle;

	void OnAlignmentPropertyChanged() const;
	
	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

protected:
	FOnAvaTextHorizontalAlignmentChanged OnHorizontalAlignmentChanged;
	FOnAvaTextVerticalAlignmentChanged OnVerticalAlignmentChanged;

	TSharedRef<SButton> GetHorizontalAlignmentButton(TSharedPtr<SButton>& OutButton, EText3DHorizontalTextAlignment InHorizontalAlignment, FName Image, FText Tooltip);
	TSharedRef<SButton> GetVerticalAlignmentButton(TSharedPtr<SButton>& OutButton, EText3DVerticalTextAlignment InVerticalAlignment, FName Image, FText Tooltip);
	
	FReply ApplyHorizontalAlignment(EText3DHorizontalTextAlignment InHorizontalAlignment) const;
	FReply ApplyVerticalAlignment(EText3DVerticalTextAlignment InVerticalAlignment) const;

	FSlateColor ApplyHorizontalAlignmentButtonColorAndOpacity(EText3DHorizontalTextAlignment InHorizontalTextAlignment) const;
	FSlateColor ApplyVerticalAlignmentButtonColorAndOpacity(EText3DVerticalTextAlignment InVerticalAlignment) const;

	EText3DHorizontalTextAlignment GetCurrentHorizontalAlignment() const;
	EText3DVerticalTextAlignment GetCurrentVerticalAlignment() const;

	TSharedPtr<SButton> HorizontalLeftButton;
	TSharedPtr<SButton> HorizontalCenterButton;
	TSharedPtr<SButton> HorizontalRightButton;

	TSharedPtr<SButton> VerticalFirstLineButton;
	TSharedPtr<SButton> VerticalTopButton;
	TSharedPtr<SButton> VerticalCenterButton;
	TSharedPtr<SButton> VerticalBottomButton;

	TSharedPtr<IPropertyHandle> HorizontalAlignmentPropertyHandle;
	TSharedPtr<IPropertyHandle> VerticalAlignmentPropertyHandle;
};

