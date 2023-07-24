// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/SlateDelegates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"

class TOOLWIDGETS_API SSimpleButton : public SButton
{
public:

	SLATE_BEGIN_ARGS(SSimpleButton)
	{}
		/** The text to display in the button. */
		SLATE_ATTRIBUTE(FText, Text)

		SLATE_ATTRIBUTE(const FSlateBrush*, Icon)

		/** The clicked handler. */
		SLATE_EVENT(FOnClicked, OnClicked)

	SLATE_END_ARGS()

	SSimpleButton() {}

	void Construct(const FArguments& InArgs);
};