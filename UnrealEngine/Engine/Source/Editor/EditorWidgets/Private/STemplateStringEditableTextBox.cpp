// Copyright Epic Games, Inc. All Rights Reserved.

#include "STemplateStringEditableTextBox.h"

#include "EditorWidgetsStyle.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Text.h"
#include "Layout/Margin.h"
#include "SlateOptMacros.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateTypes.h"
#include "TemplateStringSyntaxHighlighterMarshaller.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void STemplateStringEditableTextBox::Construct(const FArguments& InArgs)
{
	SMultiLineEditableTextBox::Construct(SMultiLineEditableTextBox::FArguments()
		.Style(&FEditorWidgetsStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
		.Text(InArgs._Text)
		.Marshaller(FTemplateStringSyntaxHighlighterMarshaller::Create(FTemplateStringSyntaxHighlighterMarshaller::FSyntaxTextStyle()))
		.AllowMultiLine(false)
		.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
		.Margin(0.0f)
		.OnTextChanged(InArgs._OnTextChanged)
	);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
