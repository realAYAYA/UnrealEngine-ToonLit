// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Framework/Text/SyntaxTokenizer.h"
#include "Text/HLSLSyntaxHighlighterMarshaller.h"

class FTextLayout;

class NIAGARAEDITOR_API FNiagaraHLSLSyntaxHighlighter : public FHLSLSyntaxHighlighterMarshaller
{
public:

	static TSharedRef<FNiagaraHLSLSyntaxHighlighter> Create(const FSyntaxTextStyle& InSyntaxTextStyle = GetSyntaxTextStyle());
	static FSyntaxTextStyle GetSyntaxTextStyle();

protected:
	FNiagaraHLSLSyntaxHighlighter(TSharedPtr< ISyntaxTokenizer > InTokenizer, const FSyntaxTextStyle& InSyntaxTextStyle);
};
