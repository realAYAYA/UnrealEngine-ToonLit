// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dialog/SMessageDialog.h"

#include "HAL/PlatformApplicationMisc.h"

#include "Styling/AppStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"

void SMessageDialog::Construct(const FArguments& InArgs)
{
	Message = InArgs._Message;
	
	SCustomDialog::Construct(SCustomDialog::FArguments()
		.Title(InArgs._Title)
		.Content()
		[
			SNew(STextBlock)
			.Text(InArgs._Message)
			.Font(FAppStyle::Get().GetFontStyle("StandardDialog.LargeFont"))
			.WrapTextAt(InArgs._WrapMessageAt)
		]
		.WindowArguments(InArgs._WindowArguments)
		.RootPadding(16.f)
		.Buttons(InArgs._Buttons)
		.AutoCloseOnButtonPress(InArgs._AutoCloseOnButtonPress)
		.Icon(InArgs._Icon)
		.HAlignContent(HAlign_Fill)
		.VAlignContent(VAlign_Fill)
		.IconDesiredSizeOverride(FVector2D(24.f, 24.f))
		.HAlignIcon(HAlign_Left)
		.VAlignIcon(VAlign_Top)
		.ContentAreaPadding(FMargin(16.f, 0.f, 0.f, 0.f))
		.UseScrollBox(InArgs._UseScrollBox)
		.ScrollBoxMaxHeight(InArgs._UseScrollBox)
		.ButtonAreaPadding(0.f)
		.OnClosed(InArgs._OnClosed)
		.BeforeButtons()
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.OnClicked(this, &SMessageDialog::OnCopyMessage)
			.ToolTipText(NSLOCTEXT("SChoiceDialog", "CopyMessageTooltip", "Copy the text in this message to the clipboard (CTRL+C)"))
			.ContentPadding(2.f)
			.Content()
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Clipboard"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]
		);
}

FReply SMessageDialog::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::C && InKeyEvent.IsControlDown())
	{
		CopyMessageToClipboard();
		return FReply::Handled();
	}

	//if it was some other button, ignore it
	return FReply::Unhandled();
}

FReply SMessageDialog::OnCopyMessage()
{
	CopyMessageToClipboard();
	return FReply::Handled();
}

void SMessageDialog::CopyMessageToClipboard()
{
	FPlatformApplicationMisc::ClipboardCopy(*Message.ToString());
}
