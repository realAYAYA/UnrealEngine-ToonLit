// Copyright Epic Games, Inc. All Rights Reserved.

#include "StyleSheet/CommonStyleSheet.h"
#include "StyleSheet/CommonStyleSheetTypes.h"

#include "Blueprint/UserWidget.h"
#include "CommonTextBlock.h"
#include "Components/Widget.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(CommonStyleSheet)

//////////////////////////////
// PROTOTYPE: DO NOT USE!!!
//////////////////////////////

void UCommonStyleSheet::Apply(UWidget* Widget)
{
	// FIXME: Current usage of the SetX functions can results in Apply being called
	// resulting in an infinite loop. To prevent this, we use this flag.
	if (bIsApplying)
	{
		return;
	}

	bIsApplying = true;
	if (UCommonTextBlock* TextBlock = Cast<UCommonTextBlock>(Widget))
	{
		TryApplyColorAndOpacity(TextBlock);
		TryApplyLineHeightPercentage(TextBlock);
		TryApplyFont(TextBlock);
		TryApplyMargin(TextBlock);
	}
	bIsApplying = false;
}

void UCommonStyleSheet::TryApplyColorAndOpacity(UCommonTextBlock* TextBlock) const
{
	const UCommonStyleSheetTypeColor* TypeColor = FindStyleSheetProperty<UCommonStyleSheetTypeColor>(this);
	FLinearColor Color = TypeColor ? TypeColor->Color : TextBlock->GetColorAndOpacity().GetSpecifiedColor();

	if (const UCommonStyleSheetTypeOpacity* TypeOpacity = FindStyleSheetProperty<UCommonStyleSheetTypeOpacity>(this))
	{
		Color.A = TypeOpacity->Opacity;
	}

	TextBlock->SetColorAndOpacity(Color);
}

void UCommonStyleSheet::TryApplyLineHeightPercentage(UCommonTextBlock* TextBlock) const
{
	if (const UCommonStyleSheetTypeLineHeightPercentage* TypeLineHeightPercentage = FindStyleSheetProperty<UCommonStyleSheetTypeLineHeightPercentage>(this))
	{
		TextBlock->SetLineHeightPercentage(TypeLineHeightPercentage->LineHeightPercentage);
	}
}

void UCommonStyleSheet::TryApplyFont(UCommonTextBlock* TextBlock) const
{
	FSlateFontInfo FontInfo = TextBlock->GetFont();

	if (const UCommonStyleSheetTypeFontTypeface* TypeFontTypeface = FindStyleSheetProperty<UCommonStyleSheetTypeFontTypeface>(this))
	{
		FontInfo.FontObject = TypeFontTypeface->Typeface.FontObject;
		FontInfo.TypefaceFontName = TypeFontTypeface->Typeface.TypefaceFontName;
	}

	if (const UCommonStyleSheetTypeFontSize* TypeFontSize = FindStyleSheetProperty<UCommonStyleSheetTypeFontSize>(this))
	{
		FontInfo.Size = TypeFontSize->Size;
	}

	if (const UCommonStyleSheetTypeFontLetterSpacing* TypeFontLetterSpacing = FindStyleSheetProperty<UCommonStyleSheetTypeFontLetterSpacing>(this))
	{
		FontInfo.LetterSpacing = TypeFontLetterSpacing->LetterSpacing;
	}

	TextBlock->SetFont(FontInfo);
}

void UCommonStyleSheet::TryApplyMargin(UCommonTextBlock* TextBlock) const
{
	FMargin Margin = TextBlock->GetMargin();

	if (const UCommonStyleSheetTypeMarginLeft* TypeMarginLeft = FindStyleSheetProperty<UCommonStyleSheetTypeMarginLeft>(this))
	{
		Margin.Left = TypeMarginLeft->Left;
	}

	if (const UCommonStyleSheetTypeMarginRight* TypeMarginRight = FindStyleSheetProperty<UCommonStyleSheetTypeMarginRight>(this))
	{
		Margin.Right = TypeMarginRight->Right;
	}

	if (const UCommonStyleSheetTypeMarginTop* TypeMarginTop = FindStyleSheetProperty<UCommonStyleSheetTypeMarginTop>(this))
	{
		Margin.Top = TypeMarginTop->Top;
	}

	if (const UCommonStyleSheetTypeMarginBottom* TypeMarginBottom = FindStyleSheetProperty<UCommonStyleSheetTypeMarginBottom>(this))
	{
		Margin.Bottom = TypeMarginBottom->Bottom;
	}

	TextBlock->SetMargin(Margin);
}

template <typename T>
const T* UCommonStyleSheet::FindStyleSheetProperty(const UCommonStyleSheet* StyleSheet) const
{
	for (const TObjectPtr<UCommonStyleSheetTypeBase>& Property : StyleSheet->Properties)
	{
		const UCommonStyleSheetTypeBase* PropertyPtr = Property.Get();
		if (PropertyPtr && PropertyPtr->bIsEnabled && PropertyPtr->IsA(T::StaticClass()))
		{
			return Cast<T>(PropertyPtr);
		}
	}

	for (const UCommonStyleSheet* ImportedStyleSheet : StyleSheet->ImportedStyleSheets)
	{
		if (ImportedStyleSheet)
		{
			if (const T* Property = FindStyleSheetProperty<T>(ImportedStyleSheet))
			{
				return Property;
			}
		}
	}

	return nullptr;
}

