// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/StringFormatter.h"
#include "Misc/AutomationTest.h"
#include "Misc/ExpressionParser.h"

#define LOCTEXT_NAMESPACE "StringFormatter"

FStringFormatArg::FStringFormatArg(const int32 Value) : Type(Int), IntValue(Value) {}
FStringFormatArg::FStringFormatArg(const uint32 Value) : Type(UInt), UIntValue(Value) {}
FStringFormatArg::FStringFormatArg(const int64 Value) : Type(Int), IntValue(Value) {}
FStringFormatArg::FStringFormatArg(const uint64 Value) : Type(UInt), UIntValue(Value) {}
FStringFormatArg::FStringFormatArg(const float Value) : Type(Double), DoubleValue(Value) {}
FStringFormatArg::FStringFormatArg(const double Value) : Type(Double), DoubleValue(Value) {}
FStringFormatArg::FStringFormatArg(FString Value) : Type(String), StringValue(MoveTemp(Value)) {}
FStringFormatArg::FStringFormatArg(FStringView Value) : Type(String), StringValue(Value) {}
FStringFormatArg::FStringFormatArg(const ANSICHAR* Value) : Type(StringLiteralANSI), StringLiteralANSIValue(Value) {}
FStringFormatArg::FStringFormatArg(const WIDECHAR* Value) : Type(StringLiteralWIDE), StringLiteralWIDEValue(Value) {}
FStringFormatArg::FStringFormatArg(const UCS2CHAR* Value) : Type(StringLiteralUCS2), StringLiteralUCS2Value(Value) {}
FStringFormatArg::FStringFormatArg(const UTF8CHAR* Value) : Type(StringLiteralUTF8), StringLiteralUTF8Value(Value) {}
FStringFormatArg& FStringFormatArg::operator=(const FStringFormatArg& Other)
{
	if (this != &Other)
	{
		Type = Other.Type;
		switch (Type)
		{
			case Int: 				IntValue = Other.IntValue; break;
			case UInt: 				UIntValue = Other.UIntValue; break;
			case Double: 			IntValue = Other.IntValue; break;
			case String: 			StringValue = Other.StringValue; break;
			case StringLiteralANSI: StringLiteralANSIValue = Other.StringLiteralANSIValue; break;
			case StringLiteralWIDE: StringLiteralWIDEValue = Other.StringLiteralWIDEValue; break;
			case StringLiteralUCS2: StringLiteralUCS2Value = Other.StringLiteralUCS2Value; break;
			case StringLiteralUTF8: StringLiteralUTF8Value = Other.StringLiteralUTF8Value; break;
		}
	}
	return *this;
}
FStringFormatArg& FStringFormatArg::operator=(FStringFormatArg&& Other)
{
	if (this != &Other)
	{
		Type = Other.Type;
		switch (Type)
		{
			case Int: 				IntValue = Other.IntValue; break;
			case UInt: 				UIntValue = Other.UIntValue; break;
			case Double: 			IntValue = Other.IntValue; break;
			case String: 			StringValue = MoveTemp(Other.StringValue); break;
			case StringLiteralANSI: StringLiteralANSIValue = Other.StringLiteralANSIValue; break;
			case StringLiteralWIDE: StringLiteralWIDEValue = Other.StringLiteralWIDEValue; break;
			case StringLiteralUCS2: StringLiteralUCS2Value = Other.StringLiteralUCS2Value; break;
			case StringLiteralUTF8: StringLiteralUTF8Value = Other.StringLiteralUTF8Value; break;
		}
	}
	return *this;
}

void AppendToString(const FStringFormatArg& Arg, FString& StringToAppendTo)
{
	switch(Arg.Type)
	{
		case FStringFormatArg::Int: 				StringToAppendTo.Append(LexToString(Arg.IntValue)); break;
		case FStringFormatArg::UInt: 				StringToAppendTo.Append(LexToString(Arg.UIntValue)); break;
		case FStringFormatArg::Double: 				StringToAppendTo.Append(LexToString(Arg.DoubleValue)); break;
		case FStringFormatArg::String: 				StringToAppendTo.AppendChars(*Arg.StringValue, Arg.StringValue.Len()); break;
		case FStringFormatArg::StringLiteralANSI: 	StringToAppendTo.Append(Arg.StringLiteralANSIValue); break;
		case FStringFormatArg::StringLiteralWIDE: 	StringToAppendTo.Append(Arg.StringLiteralWIDEValue); break;
		case FStringFormatArg::StringLiteralUCS2: 	StringToAppendTo.Append(Arg.StringLiteralUCS2Value); break;
		case FStringFormatArg::StringLiteralUTF8: 	StringToAppendTo.Append(Arg.StringLiteralUTF8Value); break;
	}
}

/** Token representing a literal string inside the string */
struct FStringLiteral
{
	FStringLiteral(const FStringToken& InString) : String(InString), Len(UE_PTRDIFF_TO_INT32(InString.GetTokenEndPos() - InString.GetTokenStartPos())) {}
	/** The string literal token */
	FStringToken String;
	/** Cached length of the string */
	int32 Len;
};

/** Token representing a user-defined token, such as {Argument} */
struct FFormatSpecifier
{
	FFormatSpecifier(const FStringToken& InIdentifier, const FStringToken& InEntireToken) : Identifier(InIdentifier), EntireToken(InEntireToken), Len(UE_PTRDIFF_TO_INT32(Identifier.GetTokenEndPos() - Identifier.GetTokenStartPos())) {}

	/** The identifier part of the token */
	FStringToken Identifier;
	/** The entire token */
	FStringToken EntireToken;
	/** Cached length of the identifier */
	int32 Len;
};

/** Token representing a user-defined index token, such as {0} */
struct FIndexSpecifier
{
	FIndexSpecifier(int32 InIndex, const FStringToken& InEntireToken) : Index(InIndex), EntireToken(InEntireToken) {}

	/** The index of the parsed token */
	int32 Index;
	/** The entire token */
	FStringToken EntireToken;
};

/** Token representing an escaped character */
struct FEscapedCharacter
{
	FEscapedCharacter(TCHAR InChar) : Character(InChar) {}

	/** The character that was escaped */
	TCHAR Character;
};

DEFINE_EXPRESSION_NODE_TYPE(FStringLiteral, 0x03ED3A25, 0x85D94664, 0x8A8001A1, 0xDCC637F7)
DEFINE_EXPRESSION_NODE_TYPE(FFormatSpecifier, 0xAAB48E5B, 0xEDA94853, 0xA951ED2D, 0x0A8E795D)
DEFINE_EXPRESSION_NODE_TYPE(FIndexSpecifier, 0xE11F9937, 0xAF714AC5, 0x88A4E04E, 0x723A753C)
DEFINE_EXPRESSION_NODE_TYPE(FEscapedCharacter, 0x48FF0754, 0x508941BB, 0x9D5447FF, 0xCAC61362)

FExpressionError GenerateErrorMsg(const FStringToken& Token)
{
	FFormatOrderedArguments Args;
	Args.Add(FText::FromString(FString(Token.GetTokenEndPos()).Left(10) + TEXT("...")));
	return FExpressionError(FText::Format(LOCTEXT("InvalidTokenDefinition", "Invalid token definition at '{0}'"), Args));
}

TOptional<FExpressionError> ParseIndex(FExpressionTokenConsumer& Consumer, bool bEmitErrors)
{
	auto& Stream = Consumer.GetStream();

	TOptional<FStringToken> OpeningChar = Stream.ParseSymbol(TEXT('{'));
	if (!OpeningChar.IsSet())
	{
		return TOptional<FExpressionError>();
	}

	FStringToken& EntireToken = OpeningChar.GetValue();

	// Optional whitespace
	Stream.ParseToken([](TCHAR InC) { return FChar::IsWhitespace(InC) ? EParseState::Continue : EParseState::StopBefore; }, &EntireToken);

	// The identifier itself
	TOptional<int32> Index;
	Stream.ParseToken([&](TCHAR InC) {
		if (FChar::IsDigit(InC))
		{
			if (!Index.IsSet())
			{
				Index = 0;
			}
			Index.GetValue() *= 10;
			Index.GetValue() += InC - '0';
			return EParseState::Continue;
		}
		return EParseState::StopBefore;
	}, &EntireToken);

	if (!Index.IsSet())
	{
		// Not a valid token
		if (bEmitErrors)
		{
			return GenerateErrorMsg(EntireToken);
		}
		else
		{
			return TOptional<FExpressionError>();
		}
	}

	// Optional whitespace
	Stream.ParseToken([](TCHAR InC) { return FChar::IsWhitespace(InC) ? EParseState::Continue : EParseState::StopBefore; }, &EntireToken);
	
	if (!Stream.ParseSymbol(TEXT('}'), &EntireToken).IsSet())
	{
		// Not a valid token
		if (bEmitErrors)
		{
			return GenerateErrorMsg(EntireToken);
		}
		else
		{
			return TOptional<FExpressionError>();
		}
	}

	// Add the token to the consumer. This moves the read position in the stream to the end of the token.
	Consumer.Add(EntireToken, FIndexSpecifier(Index.GetValue(), EntireToken));
	return TOptional<FExpressionError>();
}

TOptional<FExpressionError> ParseSpecifier(FExpressionTokenConsumer& Consumer, bool bEmitErrors)
{
	auto& Stream = Consumer.GetStream();

	TOptional<FStringToken> OpeningChar = Stream.ParseSymbol(TEXT('{'));
	if (!OpeningChar.IsSet())
	{
		return TOptional<FExpressionError>();
	}

	FStringToken& EntireToken = OpeningChar.GetValue();

	// Optional whitespace
	Stream.ParseToken([](TCHAR InC) { return FChar::IsWhitespace(InC) ? EParseState::Continue : EParseState::StopBefore; }, &EntireToken);

	// The identifier itself
	TOptional<FStringToken> Identifier = Stream.ParseToken([](TCHAR InC) {
		if (FChar::IsWhitespace(InC) || InC == '}')
		{
			return EParseState::StopBefore;
		}
		else if (FChar::IsIdentifier(InC))
		{
			return EParseState::Continue;
		}
		else
		{
			return EParseState::Cancel;
		}

	}, &EntireToken);

	if (!Identifier.IsSet())
	{
		// Not a valid token
		// Not a valid token
		if (bEmitErrors)
		{
			return GenerateErrorMsg(EntireToken);
		}
		else
		{
			return TOptional<FExpressionError>();
		}
	}

	// Optional whitespace
	Stream.ParseToken([](TCHAR InC) { return FChar::IsWhitespace(InC) ? EParseState::Continue : EParseState::StopBefore; }, &EntireToken);

	if (!Stream.ParseSymbol(TEXT('}'), &EntireToken).IsSet())
	{		
		// Not a valid token
		if (bEmitErrors)
		{
			return GenerateErrorMsg(EntireToken);
		}
		else
		{
			return TOptional<FExpressionError>();
		}
	}

	// Add the token to the consumer. This moves the read position in the stream to the end of the token.
	Consumer.Add(EntireToken, FFormatSpecifier(Identifier.GetValue(), EntireToken));
	return TOptional<FExpressionError>();
}

static const TCHAR EscapeChar = TEXT('`');

/** Parse an escaped character */
TOptional<FExpressionError> ParseEscapedChar(FExpressionTokenConsumer& Consumer, bool bEmitErrors)
{
	static const TCHAR* ValidEscapeChars = TEXT("{`");

	TOptional<FStringToken> Token = Consumer.GetStream().ParseSymbol(EscapeChar);
	if (!Token.IsSet())
	{
		return TOptional<FExpressionError>();
	}

	// Accumulate the next character into the token
	TOptional<FStringToken> EscapedChar = Consumer.GetStream().ParseSymbol(&Token.GetValue());
	if (!EscapedChar.IsSet())
	{
		return TOptional<FExpressionError>();
	}

	// Check for a valid escape character
	const TCHAR Character = *EscapedChar->GetTokenStartPos();
	if (FCString::Strchr(ValidEscapeChars, Character))
	{
		// Add the token to the consumer. This moves the read position in the stream to the end of the token.
		Consumer.Add(Token.GetValue(), FEscapedCharacter(Character));
		return TOptional<FExpressionError>();
	}
	else if (bEmitErrors)
	{
		FString CharStr;
		CharStr += Character;
		FFormatOrderedArguments Args;
		Args.Add(FText::FromString(CharStr));
		return FExpressionError(FText::Format(LOCTEXT("InvalidEscapeCharacter", "Invalid escape character '{0}'"), Args));
	}
	else
	{
		return TOptional<FExpressionError>();
	}
}

/** Parse anything until we find an unescaped { */
TOptional<FExpressionError> ParseLiteral(FExpressionTokenConsumer& Consumer, bool bEmitErrors)
{
	// Include a leading { character - if it was a valid argument token it would have been picked up by a previous token definition
	bool bFirstChar = true;
	TOptional<FStringToken> Token = Consumer.GetStream().ParseToken([&](TCHAR C){
		if (C == '{' && !bFirstChar)
		{
			return EParseState::StopBefore;
		}
		else if (C == EscapeChar)
		{
			return EParseState::StopBefore;
		}
		else
		{
			bFirstChar = false;
			// Keep consuming
			return EParseState::Continue;
		}
	});

	if (Token.IsSet())
	{
		// Add the token to the consumer. This moves the read position in the stream to the end of the token.
		Consumer.Add(Token.GetValue(), FStringLiteral(Token.GetValue()));
	}
	return TOptional<FExpressionError>();
}


FStringFormatter::FStringFormatter()
{
	using namespace ExpressionParser;

	// Token definition logic for named tokens
	NamedDefinitions.DefineToken([](FExpressionTokenConsumer& Consumer)			{ return ParseSpecifier(Consumer, false); });
	NamedDefinitions.DefineToken([](FExpressionTokenConsumer& Consumer)			{ return ParseEscapedChar(Consumer, false); });
	NamedDefinitions.DefineToken([](FExpressionTokenConsumer& Consumer)			{ return ParseLiteral(Consumer, false); });

	// Token definition logic for strict named tokens - will emit errors for any syntax errors
	StrictNamedDefinitions.DefineToken([](FExpressionTokenConsumer& Consumer)	{ return ParseSpecifier(Consumer, true); });
	StrictNamedDefinitions.DefineToken([](FExpressionTokenConsumer& Consumer)	{ return ParseEscapedChar(Consumer, true); });
	StrictNamedDefinitions.DefineToken([](FExpressionTokenConsumer& Consumer)	{ return ParseLiteral(Consumer, true); });

	// Token definition logic for ordered tokens
	OrderedDefinitions.DefineToken([](FExpressionTokenConsumer& Consumer)		{ return ParseIndex(Consumer, false); });
	OrderedDefinitions.DefineToken([](FExpressionTokenConsumer& Consumer)		{ return ParseEscapedChar(Consumer, false); });
	OrderedDefinitions.DefineToken([](FExpressionTokenConsumer& Consumer)		{ return ParseLiteral(Consumer, false); });

	// Token definition logic for strict ordered tokens - will emit errors for any syntax errors
	StrictOrderedDefinitions.DefineToken([](FExpressionTokenConsumer& Consumer)	{ return ParseIndex(Consumer, true); });
	OrderedDefinitions.DefineToken([](FExpressionTokenConsumer& Consumer)		{ return ParseEscapedChar(Consumer, true); });
	StrictOrderedDefinitions.DefineToken([](FExpressionTokenConsumer& Consumer)	{ return ParseLiteral(Consumer, true); });
}

TValueOrError<FString, FExpressionError> FStringFormatter::FormatInternal(const TCHAR* InExpression, const TMap<FString, FStringFormatArg>& Args, bool bStrict) const
{
	TValueOrError<TArray<FExpressionToken>, FExpressionError> Result = ExpressionParser::Lex(InExpression, bStrict ? StrictNamedDefinitions : NamedDefinitions);
	if (!Result.IsValid())
	{
		return MakeError(Result.StealError());
	}

	TArray<FExpressionToken>& Tokens = Result.GetValue();
	if (Tokens.Num() == 0)
	{
		return MakeValue(InExpression);
	}
	
	// This code deliberately tries to reallocate as little as possible
	FString Formatted;
	Formatted.Reserve(UE_PTRDIFF_TO_INT32(Tokens.Last().Context.GetTokenEndPos() - InExpression));
	for (const FExpressionToken& Token : Tokens)
	{
		if (const FStringLiteral* Literal = Token.Node.Cast<FStringLiteral>())
		{
			Formatted.AppendChars(Literal->String.GetTokenStartPos(), Literal->Len);
		}
		else if (const FEscapedCharacter* Escaped = Token.Node.Cast<FEscapedCharacter>())
		{
			Formatted.AppendChar(Escaped->Character);
		}
		else if (const FFormatSpecifier* FormatToken = Token.Node.Cast<FFormatSpecifier>())
		{
			const FStringFormatArg* Arg = nullptr;
			for (const TPair<FString, FStringFormatArg>& Pair : Args)
			{
				if (Pair.Key.Len() == FormatToken->Len && FCString::Strnicmp(FormatToken->Identifier.GetTokenStartPos(), *Pair.Key, FormatToken->Len) == 0)
				{
					Arg = &Pair.Value;
					break;
				}
			}

			if (Arg)
			{
				AppendToString(*Arg, Formatted);
			}
			else if (bStrict)
			{
				return MakeError(FText::Format(LOCTEXT("UndefinedFormatSpecifier", "Undefined format token: {0}"), FText::FromString(FormatToken->Identifier.GetString())));
			}
			else
			{
				// No replacement found, so just add the original token string
				const int32 Length = UE_PTRDIFF_TO_INT32(FormatToken->EntireToken.GetTokenEndPos() - FormatToken->EntireToken.GetTokenStartPos());
				Formatted.AppendChars(FormatToken->EntireToken.GetTokenStartPos(), Length);
			}
		}
	}

	return MakeValue(MoveTemp(Formatted));
}

TValueOrError<FString, FExpressionError> FStringFormatter::FormatInternal(const TCHAR* InExpression, const TArray<FStringFormatArg>& Args, bool bStrict) const
{
	TValueOrError<TArray<FExpressionToken>, FExpressionError> Result = ExpressionParser::Lex(InExpression, bStrict ? StrictOrderedDefinitions : OrderedDefinitions);
	if (!Result.IsValid())
	{
		return MakeError(Result.StealError());
	}

	TArray<FExpressionToken>& Tokens = Result.GetValue();
	if (Tokens.Num() == 0)
	{
		return MakeValue(InExpression);
	}
	
	// This code deliberately tries to reallocate as little as possible
	FString Formatted;
	Formatted.Reserve(UE_PTRDIFF_TO_INT32(Tokens.Last().Context.GetTokenEndPos() - InExpression));
	for (const FExpressionToken& Token : Tokens)
	{
		if (const FStringLiteral* Literal = Token.Node.Cast<FStringLiteral>())
		{
			Formatted.AppendChars(Literal->String.GetTokenStartPos(), Literal->Len);
		}
		else if (const FEscapedCharacter* Escaped = Token.Node.Cast<FEscapedCharacter>())
		{
			Formatted.AppendChar(Escaped->Character);
		}
		else if (const FIndexSpecifier* IndexToken = Token.Node.Cast<FIndexSpecifier>())
		{
			if (Args.IsValidIndex(IndexToken->Index))
			{
				AppendToString(Args[IndexToken->Index], Formatted);
			}
			else if (bStrict)
			{
				return MakeError(FText::Format(LOCTEXT("InvalidArgumentIndex", "Invalid argument index: {0}"), FText::AsNumber(IndexToken->Index)));
			}
			else
			{
				// No replacement found, so just add the original token string
				const int32 Length = UE_PTRDIFF_TO_INT32(IndexToken->EntireToken.GetTokenEndPos() - IndexToken->EntireToken.GetTokenStartPos());
				Formatted.AppendChars(IndexToken->EntireToken.GetTokenStartPos(), Length);
			}
		}
	}

	return MakeValue(MoveTemp(Formatted));
}

/** Default formatter for string formatting - thread safe since all formatting is const */
FStringFormatter& GetDefaultFormatter()
{
	static FStringFormatter DefaultFormatter;
	return DefaultFormatter;
}

FString FString::Format(const TCHAR* InFormatString, const FStringFormatNamedArguments& InNamedArguments)
{
	FStringFormatter& DefaultFormatter = GetDefaultFormatter();
	return DefaultFormatter.Format(InFormatString, InNamedArguments);
}

FString FString::Format(const TCHAR* InFormatString, const FStringFormatOrderedArguments& InOrderedArguments)
{
	FStringFormatter& DefaultFormatter = GetDefaultFormatter();
	return DefaultFormatter.Format(InFormatString, InOrderedArguments);
}

#undef LOCTEXT_NAMESPACE
