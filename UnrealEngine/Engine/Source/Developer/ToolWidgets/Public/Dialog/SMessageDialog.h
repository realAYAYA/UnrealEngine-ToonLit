// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SCustomDialog.h"

/**
 * Special case of SCustomDialog dedicated to only displaying text messages.
 * This class enforces uniform style and also adds a button for copying the message.
 */
class TOOLWIDGETS_API SMessageDialog : public SCustomDialog
{
public:
	// Convenience for code using this class
	using FButton = SCustomDialog::FButton;

	SLATE_BEGIN_ARGS(SMessageDialog)
		: _AutoCloseOnButtonPress(true)
		, _DecoratorStyleSet(nullptr)
		, _Icon(nullptr)
		, _UseScrollBox(true)
		, _ScrollBoxMaxHeight(300)
		, _WrapMessageAt(512.f)
	{}

		/********** Functional **********/
	
		/** Title to display for the dialog. */
		SLATE_ARGUMENT(FText, Title)
	
		/** Message content */
		SLATE_ARGUMENT(FText, Message)
	
		/** The buttons that this dialog should have. One or more buttons must be added.*/
		SLATE_ARGUMENT(TArray<FButton>, Buttons)

		/** Event triggered when the dialog is closed, either because one of the buttons is pressed, or the windows is closed. */
		SLATE_EVENT(FSimpleDelegate, OnClosed)

		/** Provides default values for SWindow::FArguments not overriden by SCustomDialog. */
		SLATE_ARGUMENT(SWindow::FArguments, WindowArguments)
	
		/** Whether to automatically close this window when any button is pressed (default: true) */
		SLATE_ARGUMENT(bool, AutoCloseOnButtonPress)
	
		/** Text decorators used while parsing the rich text messages */
		SLATE_ARGUMENT(TArray<TSharedRef<class ITextDecorator>>, Decorators)

		/** Style set used to look up styles used by decorators for rich text messages */
		SLATE_ARGUMENT(const ISlateStyle*, DecoratorStyleSet)
		
		/********** Cosmetic **********/
	
		/** Optional icon to display in the dialog. (default: empty) */
		SLATE_ARGUMENT(const FSlateBrush*, Icon)

		/** Should this dialog use a scroll box for over-sized content? (default: true) */
		SLATE_ARGUMENT(bool, UseScrollBox)

		/** Max height for the scroll box (default: 300) */
		SLATE_ARGUMENT(int32, ScrollBoxMaxHeight)

		/** When to wrap the message text (default: 512) */
		SLATE_ATTRIBUTE(float, WrapMessageAt)
	
	SLATE_END_ARGS()
	
	void Construct(const FArguments& InArgs);

	virtual	FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

	virtual	FReply OnCopyMessage();

private:

	FText Message;
	
	void CopyMessageToClipboard();
};