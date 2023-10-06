// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dialog/SCustomDialog.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"

SCustomDialog::FArguments& SCustomDialog::FArguments::IconBrush(FName InIconBrush)
{
	const FSlateBrush* ImageBrush = FAppStyle::Get().GetBrush(InIconBrush);
	if (ensureMsgf(ImageBrush != nullptr, TEXT("Brush %s is unknown"), *InIconBrush.ToString()))
	{
		_Icon = ImageBrush;
	}
	return *this;
}

void SCustomDialog::Construct(const FArguments& InArgs)
{
	check(InArgs._Buttons.Num() > 0);
	
	OnClosed = InArgs._OnClosed;
	bAutoCloseOnButtonPress = InArgs._AutoCloseOnButtonPress;
	
	SWindow::Construct( SWindow::FArguments(InArgs._WindowArguments)
		.Title(InArgs._Title)
		.SizingRule(ESizingRule::Autosized)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SBorder)
			.Padding(InArgs._RootPadding)
			.BorderImage(FAppStyle::Get().GetBrush( "ToolPanel.GroupBorder" ))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					CreateContentBox(InArgs)
				]
				
				+ SVerticalBox::Slot()
				.VAlign(InArgs._VAlignButtonBox)
				.HAlign(InArgs._HAlignButtonBox)
				.AutoHeight()
				.Padding(InArgs._ButtonAreaPadding)
				[
					CreateButtonBox(InArgs)
				]
			]
		] );
}

int32 SCustomDialog::ShowModal()
{
	FSlateApplication::Get().AddModalWindow(StaticCastSharedRef<SWindow>(this->AsShared()), FGlobalTabmanager::Get()->GetRootWindow());
	return LastPressedButton;
}

void SCustomDialog::Show()
{
	const TSharedRef<SWindow> Window = FSlateApplication::Get().AddWindow(StaticCastSharedRef<SWindow>(this->AsShared()), true);
	if (OnClosed.IsBound())
	{
		Window->GetOnWindowClosedEvent().AddLambda([this](const TSharedRef<SWindow>&) { OnClosed.Execute(); });
	}
}

TSharedRef<SWidget> SCustomDialog::CreateContentBox(const FArguments& InArgs)
{
	TSharedRef<SHorizontalBox> ContentBox = SNew(SHorizontalBox);

	ContentBox->AddSlot()
		.AutoWidth()
		.VAlign(InArgs._VAlignIcon)
		.HAlign(InArgs._HAlignIcon)
		[
			SNew(SBox)
			.Padding(0, 0, 8, 0)
			.Visibility_Lambda([IconAttr = InArgs._Icon]() { return IconAttr.Get(nullptr) ? EVisibility::Visible : EVisibility::Collapsed; })
			[
				SNew(SImage)
				.DesiredSizeOverride(InArgs._IconDesiredSizeOverride)
				.Image(InArgs._Icon)
			]
		];

	if (InArgs._UseScrollBox)
	{
		ContentBox->AddSlot()
		.VAlign(InArgs._VAlignContent)
		.HAlign(InArgs._HAlignContent)
		.Padding(InArgs._ContentAreaPadding)
		[
			SNew(SBox)
			.MaxDesiredHeight(InArgs._ScrollBoxMaxHeight)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					InArgs._Content.Widget
				]
			]
		];
	}
	else
	{
		ContentBox->AddSlot()
			.FillWidth(1.0f)
			.VAlign(InArgs._VAlignContent)
			.HAlign(InArgs._HAlignContent)
			.Padding(InArgs._ContentAreaPadding)
			[
				InArgs._Content.Widget
			];
	}
	
	return ContentBox;
}

TSharedRef<SWidget> SCustomDialog::CreateButtonBox(const FArguments& InArgs)
{
	TSharedPtr<SHorizontalBox> ButtonPanel;
	TSharedRef<SHorizontalBox> ButtonBox =
		SNew(SHorizontalBox)
	
		// Before buttons
		+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				InArgs._BeforeButtons.Widget
			]

		// Buttons
		+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			[
				SAssignNew(ButtonPanel, SHorizontalBox)
			];

	bool bCanFocusLastPrimary = true;
	bool bFocusedAnyButtonYet = false;
	for (int32 ButtonIndex = 0; ButtonIndex < InArgs._Buttons.Num(); ++ButtonIndex)
	{
		const FButton& Button = InArgs._Buttons[ButtonIndex];

		const FButtonStyle* ButtonStyle = Button.bIsPrimary ?
				&FAppStyle::Get().GetWidgetStyle< FButtonStyle >( "PrimaryButton" ) :
				&FAppStyle::Get().GetWidgetStyle< FButtonStyle >("Button");
		
		TSharedRef<SButton> ButtonWidget = SNew(SButton)
			.OnClicked(FOnClicked::CreateSP(this, &SCustomDialog::OnButtonClicked, Button.OnClicked, ButtonIndex))
			.ButtonStyle(ButtonStyle)
			[
				SNew(STextBlock)
				.Text(Button.ButtonText)
			];

		ButtonPanel->AddSlot()
			.Padding(FAppStyle::Get().GetMargin("StandardDialog.SlotPadding"))
			.AutoWidth()
		[
			ButtonWidget
		];

		if (Button.bShouldFocus)
		{
			bCanFocusLastPrimary = false;
		}
		
		const bool bIsLastButton = ButtonIndex == InArgs._Buttons.Num() - 1;
		if (Button.bShouldFocus
			|| (bCanFocusLastPrimary && Button.bIsPrimary)
			|| (bIsLastButton && !bFocusedAnyButtonYet))
		{
			bFocusedAnyButtonYet = true;
			SetWidgetToFocusOnActivate(ButtonWidget);
		}
	}
	return ButtonBox;
}

/** Handle the button being clicked */
FReply SCustomDialog::OnButtonClicked(FSimpleDelegate OnClicked, int32 ButtonIndex)
{
	LastPressedButton = ButtonIndex;

	if (bAutoCloseOnButtonPress)
	{
		RequestDestroyWindow();
	}

	OnClicked.ExecuteIfBound();
	return FReply::Handled();
}
