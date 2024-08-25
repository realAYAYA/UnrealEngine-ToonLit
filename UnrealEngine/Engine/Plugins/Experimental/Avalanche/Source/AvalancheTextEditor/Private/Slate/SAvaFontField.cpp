// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAvaFontField.h"
#include "Engine/Font.h"
#include "Font/AvaFontManagerSubsystem.h"
#include "Font/AvaFontView.h"
#include "SlateOptMacros.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Text/STextBlock.h"

namespace UE::AvaTextEditor::Private
{
	static constexpr float DefaultFontSize = 12.0f;
}

#define LOCTEXT_NAMESPACE "AvaFontField"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAvaFontField::Construct(const FArguments& InArgs)
{
	FontView = InArgs._FontView;
	OnFontFieldModified = InArgs._OnFontFieldModified;

	const FVector2D Icon16x16(16.0f, 16.0f);

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(400.0f)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Clipping(EWidgetClipping::ClipToBoundsAlways) // we don't want list elements to render outside the box
		.Content()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle( FAppStyle::Get(), "SimpleButton" )
					.OnClicked(this, &SAvaFontField::OnToggleFavoriteClicked)
					.ToolTipText(LOCTEXT("ToggleFavorite", "Mark Font as Favorite"))
					.Visibility(this, &SAvaFontField::GetFavoriteVisibility)
					[
						SNew(SImage)
						.ColorAndOpacity(this, &SAvaFontField::GetToggleFavoriteColor)
						.Image(FAppStyle::GetBrush("Icons.Star"))
						.DesiredSizeOverride(Icon16x16)
					]
				]
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			[
				SAssignNew(LeftFontNameText, STextBlock)
				.Justification(ETextJustify::Left)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.AutoWidth()
			[
				SAssignNew(RightFontNameText, STextBlock)
				.Justification(ETextJustify::Right)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.AutoWidth()
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.ToolTipText(LOCTEXT( "FontLocallyAvailable", "This font is available on the local system"))
				.Visibility(this, &SAvaFontField::GetLocallyAvailableIconVisibility)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Symbols.Check"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		]
	];

	UpdateFont(FontView.Get());
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAvaFontField::UpdateFont(const TSharedPtr<FAvaFontView>& InFontView)
{
	if (!InFontView.IsValid())
	{
		return;
	}

	FontView.Set(InFontView);

	if (const TSharedPtr<FAvaFontView>& FontViewPtr = FontView.Get())
	{
		const UAvaFontObject* const CurrFontObj = FontViewPtr->GetFontObject();
		const UFont* const CurrFont = FontViewPtr->GetFont();
		if (!CurrFont || !IsValid(CurrFont))
		{
			return;
		}

		FontName = FontViewPtr->GetFontNameAsText();

		if (LeftFontNameText.IsValid())
		{
			FText TextDisplay = FontName;
			if (FontViewPtr->GetFontSource() == EAvaFontSource::System)
			{
				TextDisplay = FText::FromString(FontViewPtr->GetFontNameAsString() + TEXT("*"));
			}

			LeftFontNameText->SetText(TextDisplay);

			if (CurrFont)
			{
				if (CurrFont->GetCompositeFont())
				{
					LeftFontNameText->SetFont(FSlateFontInfo(CurrFont, UE::AvaTextEditor::Private::DefaultFontSize));
				}
			}
		}

		if (RightFontNameText.IsValid())
		{
			if (CurrFontObj)
			{
				const FString AlternateText = CurrFontObj->GetAlternateText();

				if (!AlternateText.IsEmpty())
				{
					RightFontNameText->SetVisibility(EVisibility::Visible);
					const FText TextDisplay = FText::FromString(CurrFontObj->GetAlternateText());
					RightFontNameText->SetText(TextDisplay);

					if (CurrFont->GetCompositeFont())
					{
						RightFontNameText->SetFont(FSlateFontInfo(CurrFont, UE::AvaTextEditor::Private::DefaultFontSize));
					}
				}
			}
			else
			{
				RightFontNameText->SetVisibility(EVisibility::Collapsed);
			}
		}
	}
}

ECheckBoxState SAvaFontField::GetFavoriteState() const
{
	if (const TSharedPtr<FAvaFontView>& FontViewPtr = FontView.Get())
	{
		if (FontViewPtr->IsFavorite())
		{
			return ECheckBoxState::Checked;
		}
	}

	return ECheckBoxState::Unchecked;
}

FSlateColor SAvaFontField::GetToggleFavoriteColor() const
{
	if (const TSharedPtr<FAvaFontView>& FontViewPtr = FontView.Get())
	{
		if (FontViewPtr->IsFavorite())
		{
			return FSlateColor(EStyleColor::AccentBlue);
		}
	}

	return FSlateColor(EStyleColor::Foreground);
}

FReply SAvaFontField::OnToggleFavoriteClicked()
{
	UAvaFontManagerSubsystem* FontManagerSubsystem = UAvaFontManagerSubsystem::Get();
	if (!FontManagerSubsystem)
	{
		FReply::Handled();
	}

	if (const TSharedPtr<FAvaFontView>& FontViewPtr = FontView.Get())
	{
		const bool bIsNowFavorite = !FontViewPtr->IsFavorite();

		FontViewPtr->SetFavorite(bIsNowFavorite);

		const FString FavFontName = FontViewPtr->GetFontNameAsString();

		if (bIsNowFavorite)
		{
			FontManagerSubsystem->AddFavorite(*FontViewPtr->GetFontNameAsString());
		}
		else
		{
			FontManagerSubsystem->RemoveFavorite(*FontViewPtr->GetFontNameAsString());
		}

		OnFontFieldModified.ExecuteIfBound(FontViewPtr);
	}

	return FReply::Handled();
}

EVisibility SAvaFontField::GetFavoriteVisibility() const
{
	return IsHovered() ? EVisibility::Visible : EVisibility::Hidden;
}

EVisibility SAvaFontField::GetLocallyAvailableIconVisibility() const
{
	bool bIsAvailable = false;
	if (const UAvaFontManagerSubsystem* FontManagerSubsystem = UAvaFontManagerSubsystem::Get())
	{
		if (const TSharedPtr<FAvaFontView>& FontViewPtr = FontView.Get())
		{
			bIsAvailable = FontManagerSubsystem->IsFontAvailableOnLocalOS(FontViewPtr->GetFont());
		}
	}

	return bIsAvailable && !IsSelected() ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
