// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "Framework/SlateDelegates.h"
#include "Misc/Attribute.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"

class FString;
class FText;
class ITextLayoutMarshaller;

class EDITORWIDGETS_API STemplateStringEditableTextBox : public SMultiLineEditableTextBox
{
public:
	SLATE_BEGIN_ARGS(STemplateStringEditableTextBox)
	{}
		/** The initial text that will appear in the widget. */
		SLATE_ATTRIBUTE(FText, Text)

		/** The list of available arguments to use in this template string. */
		SLATE_ATTRIBUTE(TArray<FString>, ValidArguments)

		/** The marshaller used to get/set the raw text to/from the text layout. */
		SLATE_ARGUMENT(TSharedPtr<ITextLayoutMarshaller>, Marshaller)

		/** Called whenever the text is changed interactively by the user. */
		SLATE_EVENT(FOnTextChanged, OnTextChanged)
	
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
};
