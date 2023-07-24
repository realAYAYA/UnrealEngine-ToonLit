// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Framework/Text/SyntaxHighlighterTextLayoutMarshaller.h"
#include "Framework/Text/SyntaxTokenizer.h"
#include "Framework/Text/TextLayout.h"
#include "HAL/Platform.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"



/**
 * Syntax highlighting for hlsl text
 */
class UNREALED_API FHLSLSyntaxHighlighterMarshaller : public FSyntaxHighlighterTextLayoutMarshaller
{
public:

	struct FSyntaxTextStyle
	{
		FSyntaxTextStyle(
			const FTextBlockStyle& InNormalTextStyle,
			const FTextBlockStyle& InOperatorTextStyle,
			const FTextBlockStyle& InKeywordTextStyle,
			const FTextBlockStyle& InStringTextStyle,
			const FTextBlockStyle& InNumberTextStyle,
			const FTextBlockStyle& InCommentTextStyle,
			const FTextBlockStyle& InPreProcessorKeywordTextStyle,
			const FTextBlockStyle& InErrorTextStyle
			) :
			NormalTextStyle(InNormalTextStyle),
			OperatorTextStyle(InOperatorTextStyle),
			KeywordTextStyle(InKeywordTextStyle),
			StringTextStyle(InStringTextStyle),
			NumberTextStyle(InNumberTextStyle),
			CommentTextStyle(InCommentTextStyle),
			PreProcessorKeywordTextStyle(InPreProcessorKeywordTextStyle),
			ErrorTextStyle(InErrorTextStyle)
		{
		}

		FTextBlockStyle NormalTextStyle;
		FTextBlockStyle OperatorTextStyle;
		FTextBlockStyle KeywordTextStyle;
		FTextBlockStyle StringTextStyle;
		FTextBlockStyle NumberTextStyle;
		FTextBlockStyle CommentTextStyle;
		FTextBlockStyle PreProcessorKeywordTextStyle;
		FTextBlockStyle ErrorTextStyle;
	};

	static TSharedRef<FHLSLSyntaxHighlighterMarshaller> Create(const FSyntaxTextStyle& InSyntaxTextStyle);

protected:

	static TSharedPtr<ISyntaxTokenizer> CreateTokenizer();
	
	virtual void ParseTokens(const FString& SourceString, FTextLayout& TargetTextLayout, TArray<ISyntaxTokenizer::FTokenizedLine> TokenizedLines) override;

	virtual FTextLayout::FNewLineData ProcessTokenizedLine(const ISyntaxTokenizer::FTokenizedLine& TokenizedLine, const int32& LineNumber, const FString& SourceString);

	FHLSLSyntaxHighlighterMarshaller(TSharedPtr<ISyntaxTokenizer> InTokenizer, const FSyntaxTextStyle& InSyntaxTextStyle);

	/** Styles used to display the text */
	FSyntaxTextStyle SyntaxTextStyle;
};