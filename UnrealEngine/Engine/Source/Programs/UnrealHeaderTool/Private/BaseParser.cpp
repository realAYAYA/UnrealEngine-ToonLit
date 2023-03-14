// Copyright Epic Games, Inc. All Rights Reserved.

#include "BaseParser.h"
#include "UnrealHeaderTool.h"
#include "UnrealSourceFile.h"
#include "UObject/NameTypes.h"

#include "ParserHelper.h"


namespace
{
	namespace EMetadataValueArgument
	{
		enum Type
		{
			None,
			Required,
			Optional
		};
	}

	namespace EMetadataValueAction
	{
		enum Type
		{
			Remove,
			Add
		};
	}

	struct FMetadataValueAction
	{
		FMetadataValueAction(const TCHAR* InMapping, const TCHAR* InDefaultValue, EMetadataValueAction::Type InValueAction)
		: Mapping         (InMapping)
		, DefaultValue    (InDefaultValue)
		, ValueAction     (InValueAction)
		{
		}

		FName                      Mapping;
		FString                    DefaultValue;
		EMetadataValueAction::Type ValueAction;
	};

	struct FMetadataKeyword
	{
		FMetadataKeyword(EMetadataValueArgument::Type InValueArgument)
		: ValueArgument(InValueArgument)
		{
		}

		void InsertAddAction(const TCHAR* InMapping, const TCHAR* InDefaultValue)
		{
			ValueActions.Add(FMetadataValueAction(InMapping, InDefaultValue, EMetadataValueAction::Add));
		}

		void InsertRemoveAction(const TCHAR* InMapping)
		{
			ValueActions.Add(FMetadataValueAction(InMapping, TEXT(""), EMetadataValueAction::Remove));
		}

		void ApplyToMetadata(FBaseParser& Parser, TMap<FName, FString>& Metadata, const FString* Value = nullptr)
		{
			for (const FMetadataValueAction& ValueAction : ValueActions)
			{
				if (ValueAction.ValueAction == EMetadataValueAction::Add)
				{
					Parser.InsertMetaDataPair(Metadata, ValueAction.Mapping, Value ? *Value : ValueAction.DefaultValue);
				}
				else
				{
					Metadata.Remove(ValueAction.Mapping);
				}
			}
		}

		TArray<FMetadataValueAction> ValueActions;
		EMetadataValueArgument::Type ValueArgument;
	};

	TMap<FString, FMetadataKeyword> MetadataDictionary;

	void InitMetadataKeyswordsInternal()
	{
		check(MetadataDictionary.Num() == 0);
		if (!MetadataDictionary.Num())
		{
			FMetadataKeyword& DisplayName = MetadataDictionary.Add(TEXT("DisplayName"), EMetadataValueArgument::Required);
			DisplayName.InsertAddAction(TEXT("DisplayName"), TEXT(""));

			FMetadataKeyword& FriendlyName = MetadataDictionary.Add(TEXT("FriendlyName"), EMetadataValueArgument::Required);
			FriendlyName.InsertAddAction(TEXT("FriendlyName"), TEXT(""));

			FMetadataKeyword& BlueprintInternalUseOnly = MetadataDictionary.Add(TEXT("BlueprintInternalUseOnly"), EMetadataValueArgument::None);
			BlueprintInternalUseOnly.InsertAddAction(TEXT("BlueprintInternalUseOnly"), TEXT("true"));
			BlueprintInternalUseOnly.InsertAddAction(TEXT("BlueprintType"), TEXT("true"));

			FMetadataKeyword& BlueprintInternalUseOnlyHierarchical = MetadataDictionary.Add(TEXT("BlueprintInternalUseOnlyHierarchical"), EMetadataValueArgument::None);
			BlueprintInternalUseOnlyHierarchical.InsertAddAction(TEXT("BlueprintInternalUseOnlyHierarchical"), TEXT("true"));
			BlueprintInternalUseOnlyHierarchical.InsertAddAction(TEXT("BlueprintInternalUseOnly"), TEXT("true"));
			BlueprintInternalUseOnlyHierarchical.InsertAddAction(TEXT("BlueprintType"), TEXT("true"));
			
			FMetadataKeyword& BlueprintType = MetadataDictionary.Add(TEXT("BlueprintType"), EMetadataValueArgument::None);
			BlueprintType.InsertAddAction(TEXT("BlueprintType"), TEXT("true"));

			FMetadataKeyword& NotBlueprintType = MetadataDictionary.Add(TEXT("NotBlueprintType"), EMetadataValueArgument::None);
			NotBlueprintType.InsertAddAction(TEXT("NotBlueprintType"), TEXT("true"));
			NotBlueprintType.InsertRemoveAction(TEXT("BlueprintType"));

			FMetadataKeyword& Blueprintable = MetadataDictionary.Add(TEXT("Blueprintable"), EMetadataValueArgument::None);
			Blueprintable.InsertAddAction(TEXT("IsBlueprintBase"), TEXT("true"));
			Blueprintable.InsertAddAction(TEXT("BlueprintType"), TEXT("true"));

			FMetadataKeyword& CallInEditor = MetadataDictionary.Add(TEXT("CallInEditor"), EMetadataValueArgument::None);
			CallInEditor.InsertAddAction(TEXT("CallInEditor"), TEXT("true"));

			FMetadataKeyword& NotBlueprintable = MetadataDictionary.Add(TEXT("NotBlueprintable"), EMetadataValueArgument::None);
			NotBlueprintable.InsertAddAction(TEXT("IsBlueprintBase"), TEXT("false"));
			NotBlueprintable.InsertRemoveAction(TEXT("BlueprintType"));

			FMetadataKeyword& Category = MetadataDictionary.Add(TEXT("Category"), EMetadataValueArgument::Required);
			Category.InsertAddAction(TEXT("Category"), TEXT(""));

			FMetadataKeyword& ExperimentalFeature = MetadataDictionary.Add(TEXT("Experimental"), EMetadataValueArgument::None);
			ExperimentalFeature.InsertAddAction(TEXT("DevelopmentStatus"), TEXT("Experimental"));

			FMetadataKeyword& EarlyAccessFeature = MetadataDictionary.Add(TEXT("EarlyAccessPreview"), EMetadataValueArgument::None);
			EarlyAccessFeature.InsertAddAction(TEXT("DevelopmentStatus"), TEXT("EarlyAccess"));

			FMetadataKeyword& DocumentationPolicy = MetadataDictionary.Add(TEXT("DocumentationPolicy"), EMetadataValueArgument::None);
			DocumentationPolicy.InsertAddAction(TEXT("DocumentationPolicy"), TEXT("Strict"));

			FMetadataKeyword& SparseClassDataType = MetadataDictionary.Add(TEXT("SparseClassDataType"), EMetadataValueArgument::Required);
			SparseClassDataType.InsertAddAction(TEXT("SparseClassDataType"), TEXT(""));
		}
	}

	FMetadataKeyword* GetMetadataKeyword(const FToken& Token)
	{
		check(MetadataDictionary.Num() > 0);
		return MetadataDictionary.Find(FString(Token.Value));
	}
}

//////////////////////////////////////////////////////////////////////////
// FPropertySpecifier

FString FPropertySpecifier::ConvertToString() const
{
	FString Result;

	// Emit the specifier key
	Result += Key;

	// Emit the values if there are any
	if (Values.Num())
	{
		Result += TEXT("=");

		if (Values.Num() == 1)
		{
			// One value goes on it's own
			Result += Values[0];
		}
		else
		{
			// More than one value goes in parens, separated by commas
			Result += TEXT("(");
			for (int32 ValueIndex = 0; ValueIndex < Values.Num(); ++ValueIndex)
			{
				if (ValueIndex > 0)
				{
					Result += TEXT(", ");
				}
				Result += Values[ValueIndex];
			}
			Result += TEXT(")");
		}
	}

	return Result;
}

/////////////////////////////////////////////////////
// FBaseParser

FBaseParser::FBaseParser(FUnrealSourceFile& InSourceFile)
	: UHTConfig(FUHTConfig::Get())
	, SourceFile(InSourceFile)
	, StatementsParsed(0)
	, LinesParsed(0)
{
}

FString FBaseParser::GetFilename() const
{
	return SourceFile.GetFilename();
};

void FBaseParser::ResetParser(const TCHAR* SourceBuffer, int32 StartingLineNumber)
{
	Input = SourceBuffer;
	InputLen = FCString::Strlen(Input);
	InputPos = 0;
	PrevPos = 0;
	PrevLine = 1;
	InputLine = StartingLineNumber;
}

/*-----------------------------------------------------------------------------
	Single-character processing.
-----------------------------------------------------------------------------*/

//
// Get a single character from the input stream and return it, or 0=end.
//
TCHAR FBaseParser::GetChar(bool bLiteral)
{
	PrevPos = InputPos;
	PrevLine = InputLine;

Loop:
	TCHAR Char = Input[InputPos++];
	if (Char == 0)
	{ 
		--InputPos;
	}
	else if (Char == TEXT('\n'))
	{
		++InputLine;
	}
	else if (Char == TEXT('/'))
	{
		if (!bLiteral && Input[InputPos] == TEXT('*'))
		{
			ClearComment();
			const TCHAR* CommentStart = &Input[InputPos - 1];
			const TCHAR* CommentEnd = CommentStart + 2;
			for (;;)
			{
				TCHAR CommentChar = *CommentEnd++;
				if (CommentChar == 0)
				{
					ClearComment();
					Throwf(TEXT("End of class header encountered inside comment"));
				}
				else if (CommentChar == TEXT('\n'))
				{
					++InputLine;
				}
				else if (CommentChar == TEXT('*') && *CommentEnd == TEXT('/'))
				{
					int32 Length = int32(CommentEnd - CommentStart);
					PrevComment.AppendChars(CommentStart, Length + 1);
					InputPos += Length;
					goto Loop;
				}
			}
		}
	}
	return Char;
}

//
// Unget the previous character retrieved with GetChar().
//
void FBaseParser::UngetChar()
{
	InputPos = PrevPos;
	InputLine = PrevLine;
}

//
// Look at a single character from the input stream and return it, or 0=end.
// Has no effect on the input stream.
//
TCHAR FBaseParser::PeekChar()
{
	return (InputPos < InputLen) ? Input[InputPos] : TEXT('\0');
}

void FBaseParser::SkipWhitespaceAndComments()
{
	const TCHAR* Pos = &Input[InputPos];
	bool bGotNewlineBetweenComments = false;
	bool bGotInlineComment = false;

	for (;;)
	{
		TCHAR Char = *Pos++;
		if (Char == 0)
		{
			--Pos;
			break;
		}
		else if (Char == TEXT('\n'))
		{
			bGotNewlineBetweenComments |= bGotInlineComment;
			++InputLine;
		}
		else if (Char == TEXT('\r') || Char == TEXT('\t') || Char == TEXT(' '))
		{
		}
		else if (Char == TEXT('/'))
		{
			TCHAR NextChar = *Pos;
			if (NextChar == TEXT('*'))
			{
				ClearComment();
				const TCHAR* CommentStart = Pos - 1;
				++Pos;
				for (;;)
				{
					TCHAR CommentChar = *Pos++;
					if (CommentChar == 0)
					{
						ClearComment();
						Throwf(TEXT("End of class header encountered inside comment"));
					}
					else if (CommentChar == TEXT('\n'))
					{
						++InputLine;
					}
					else if (CommentChar == TEXT('*') && *Pos == TEXT('/'))
					{
						++Pos;
						break;
					}
				}
				PrevComment.AppendChars(CommentStart, int32(Pos - CommentStart));
			}
			else if (NextChar == TEXT('/'))
			{
				if (bGotNewlineBetweenComments)
				{
					bGotNewlineBetweenComments = false;
					ClearComment();
				}
				bGotInlineComment = true;
				const TCHAR* CommentStart = Pos - 1;
				Pos++;

				// Scan to the end of the line
				for (;;)
				{
					TCHAR CommentChar = *Pos++;
					if (CommentChar == 0)
					{
						--Pos;
						break;
					}
					if (CommentChar == TEXT('\r'))
					{
					} 
					else if (CommentChar == TEXT('\n'))
					{
						++InputLine;
						break;
					}
				}
				PrevComment.AppendChars(CommentStart, int32(Pos - CommentStart));
			}
			else
			{
				--Pos;
				break;
			}
		}
		else
		{
			--Pos;
			break;
		}
	}
	InputPos = int32(Pos - Input);
	return;
}

//
// Skip past all spaces and tabs in the input stream.
//
TCHAR FBaseParser::GetLeadingChar()
{
	SkipWhitespaceAndComments();
	return GetChar();
}

bool FBaseParser::IsEOL( TCHAR c )
{
	return c==TEXT('\n') || c==TEXT('\r') || c==0;
}

bool FBaseParser::IsWhitespace( TCHAR c )
{
	return c==TEXT(' ') || c==TEXT('\t') || c==TEXT('\r') || c==TEXT('\n');
}

/*-----------------------------------------------------------------------------
	Tokenizing.
-----------------------------------------------------------------------------*/

// Gets the next token from the input stream, advancing the variables which keep track of the current input position and line.
// NOTE: There is a slight difference between StartPos/StartLine handling here.  In the old version, the starting location would 
// include any preceeding /**/ comments (but not // comments) while this version does not.  The only place in the code this
// caused an issue was in the processing of default arguments.
bool FBaseParser::GetToken(FToken& Token, bool bNoConsts/*=false*/, ESymbolParseOption ParseTemplateClosingBracket/*=ESymbolParseOption::Normal*/ )
{
	SkipWhitespaceAndComments();
	PrevPos = InputPos;
	PrevLine = InputLine;
	if (Input[InputPos] == 0)
	{
		return false;
	}
	Token.StartPos = InputPos;
	Token.StartLine = InputLine;

	const TCHAR* Start = &Input[InputPos];
	const TCHAR* Pos = Start;
	TCHAR c = *Pos++;

	// Identifier
	if ((c >= TEXT('A') && c <= TEXT('Z')) || (c >= TEXT('a') && c <= TEXT('z')) || c == TEXT('_'))
	{
		for (;; ++Pos)
		{
			c = *Pos;
			if (!((c >= TEXT('A') && c <= TEXT('Z')) || (c >= TEXT('a') && c <= TEXT('z')) || (c >= TEXT('0') && c <= TEXT('9')) || c == TEXT('_')))
			{
				break;
			}
		}

		// If const values are allowed, determine whether the identifier represents a constant
		Token.TokenType = ETokenType::Identifier;
		Token.Value = FStringView(Start, int32(Pos - Start));
		Token.InputLine = InputLine;
		if (!bNoConsts)
		{
			// See if the identifier is part of a vector, rotation or other struct constant.
			// boolean true/false
			int32 Length = int32(Pos - Start);
			if (Length == 4 && Token.IsValue(TEXT("true"), ESearchCase::IgnoreCase))
			{
				Token.TokenType = ETokenType::TrueConst;
			}
			else if (Length == 5 && Token.IsValue(TEXT("false"), ESearchCase::IgnoreCase))
			{
				Token.TokenType = ETokenType::FalseConst;
			}
		}

		if (Token.Value.Len() >= NAME_SIZE)
		{
			Throwf(TEXT("Identifer length exceeds maximum of %i"), (int32)NAME_SIZE);
		}

		InputPos = int32(Pos - Input);
		if (bRecordTokens)
		{
			RecordedTokens.Push(Token);
		}
		return true;
	}

	// if const values are allowed, determine whether the non-identifier token represents a const
	else if (!bNoConsts && ((c >= TEXT('0') && c <= TEXT('9')) || ((c == TEXT('+') || c == TEXT('-')) && (*Pos >= TEXT('0') && *Pos <= TEXT('9')))))
	{
		// Integer or floating point constant.

		bool  bIsFloat = 0;
		bool  bIsHex = 0;
		do
		{
			if (c == TEXT('.'))
			{
				bIsFloat = true;
			}
			if (c == TEXT('X') || c == TEXT('x'))
			{
				bIsHex = true;
			}
			c = FChar::ToUpper(*Pos++);
		} while ((c >= TEXT('0') && c <= TEXT('9')) || (!bIsFloat && c == TEXT('.')) || (!bIsHex && c == TEXT('X')) || (bIsHex && c >= TEXT('A') && c <= TEXT('F')));

		if (!bIsFloat || c != TEXT('F'))
		{
			--Pos;
		}

		Token.TokenType = bIsFloat ? ETokenType::FloatConst : (bIsHex ? ETokenType::HexConst : ETokenType::DecimalConst);
		Token.Value = FStringView(Start, int32(Pos - Start));
		Token.InputLine = InputLine;

		if (Token.Value.Len() >= NAME_SIZE)
		{
			Throwf(TEXT("Number length exceeds maximum of %i "), (int32)NAME_SIZE);
		}

		InputPos = int32(Pos - Input);
		if (bRecordTokens)
		{
			RecordedTokens.Push(Token);
		}
		return true;
	}

	// Escaped character constant
	else if (c == TEXT('\''))
	{

		// We try to skip the character constant value. But if it is backslash, we have to skip another character
		if (*Pos++ == TEXT('\\'))
		{
			++Pos;
		}

		if (*Pos++ != TEXT('\''))
		{
			Throwf(TEXT("Unterminated character constant"));
		}

		Token.TokenType = ETokenType::CharConst;
		Token.Value = FStringView(Start, int32(Pos - Start));
		Token.InputLine = InputLine;
		InputPos = int32(Pos - Input);
		if (bRecordTokens)
		{
			RecordedTokens.Push(Token);
		}
		return true;
	}

	// String contant
	else if (c == TEXT('"'))
	{
		c = *Pos++;
		int32 EscapeCount = 0;
		while( (c != TEXT('"')) && !IsEOL(c) )
		{
			if( c == TEXT('\\') )
			{
				c = *Pos++;
				if( IsEOL(c) )
				{
					break;
				}
				else if(c == TEXT('n'))
				{
					// Newline escape sequence.
					c = TEXT('\n');
				}
				++EscapeCount;
			}
			c = *Pos++;
		}

		if( c != TEXT('"') )
		{
			Throwf(TEXT("Unterminated string constant"));
		}

		Token.TokenType = ETokenType::StringConst;
		Token.Value = FStringView(Start, int32(Pos - Start));
		Token.InputLine = InputLine;

		if (!Token.Value.Len() - EscapeCount >= MAX_STRING_CONST_SIZE)
		{
			Throwf(TEXT("String constant exceeds maximum of %i characters"), (int32)MAX_STRING_CONST_SIZE);
		}

		InputPos = int32(Pos - Input);
		if (bRecordTokens)
		{
			RecordedTokens.Push(Token);
		}
		return true;
	}
	else
	{
		// Handle special 2-character symbols.
#define PAIR(cc,dd) ((c==TEXT(cc))&&(d==TEXT(dd))) /* Comparison macro for convenience */
		TCHAR d = *Pos++;
		if
		(	PAIR('<','<')
		||	(PAIR('>','>') && (ParseTemplateClosingBracket != ESymbolParseOption::CloseTemplateBracket))
		||	PAIR('!','=')
		||	PAIR('<','=')
		||	PAIR('>','=')
		||	PAIR('+','+')
		||	PAIR('-','-')
		||	PAIR('+','=')
		||	PAIR('-','=')
		||	PAIR('*','=')
		||	PAIR('/','=')
		||	PAIR('&','&')
		||	PAIR('|','|')
		||	PAIR('^','^')
		||	PAIR('=','=')
		||	PAIR('*','*')
		||	PAIR('~','=')
		||	PAIR(':',':')
		)
		{
			if (c == TEXT('>') && d == TEXT('>') &&  *Pos == TEXT('>'))
			{
				++Pos;
			}
		}
		else
		{
			--Pos;
		}
#undef PAIR

		Token.TokenType = ETokenType::Symbol;
		Token.Value = FStringView(Start, int32(Pos - Start));
		Token.InputLine = InputLine;
		InputPos = int32(Pos - Input);
		if (bRecordTokens)
		{
			RecordedTokens.Push(Token);
		}
		return true;
	}
}

bool FBaseParser::GetRawStringRespectingQuotes(FTokenString& String, TCHAR StopChar /* = TCHAR('\n') */ )
{
	// Get token after whitespace.
	int32 Length=0;
	TCHAR c = GetLeadingChar();

	bool bInQuote = false;

	while( !IsEOL(c) && ((c != StopChar) || bInQuote) )
	{
		if( !bInQuote && ( (c == TEXT('/') && PeekChar() == TEXT('/')) || (c == TEXT('/') && PeekChar() == TEXT('*')) ) )
		{
			break;
		}

		if (c == TEXT('"'))
		{
			bInQuote = !bInQuote;
		}

		String.String[Length++] = c;
		if( Length >= MAX_STRING_CONST_SIZE)
		{
			Throwf(TEXT("Identifier exceeds maximum of %i characters"), (int32)MAX_STRING_CONST_SIZE);
			c = GetChar(true);
			Length = ((int32)MAX_STRING_CONST_SIZE) - 1;
			break;
		}
		c = GetChar(true);
	}
	UngetChar();

	if (bInQuote)
	{
		Throwf(TEXT("Unterminated quoted string"));
	}

	// Get rid of trailing whitespace.
	while( Length>0 && (String.String[Length-1] == TEXT(' ') || String.String[Length-1]==9 ) )
	{
		Length--;
	}
	String.String[Length] = TEXT('\0');

	return Length>0;
}

/**
 * Put all text from the current position up to either EOL or the StopToken
 * into Token.  Advances the compiler's current position.
 *
 * @param	Token	[out] will contain the text that was parsed
 * @param	StopChar	stop processing when this character is reached
 *
 * @return	the number of character parsed
 */
bool FBaseParser::GetRawString( FTokenString& String, TCHAR StopChar /* = TCHAR('\n') */ )
{
	// Get token after whitespace.
	int32 Length=0;
	TCHAR c = GetLeadingChar();
	while( !IsEOL(c) && c != StopChar )
	{
		if( (c == TEXT('/') && PeekChar() == TEXT('/')) || (c == TEXT('/') && PeekChar() == TEXT('*')) )
		{
			break;
		}
		String.String[Length++] = c;
		if( Length >= MAX_STRING_CONST_SIZE )
		{
			Throwf(TEXT("Identifier exceeds maximum of %i characters"), (int32)MAX_STRING_CONST_SIZE );
		}
		c = GetChar(true);
	}
	UngetChar();

	// Get rid of trailing whitespace.
	while( Length>0 && (String.String[Length-1] == TEXT(' ') || String.String[Length-1]==9 ) )
	{
		Length--;
	}
	String.String[Length] = TEXT('\0');

	return Length>0;
}

//
// Get an identifier token, return 1 if gotten, 0 if not.
//
bool FBaseParser::GetIdentifier(FToken& Token, bool bNoConsts)
{
	if (!GetToken(Token, bNoConsts))
	{
		return false;
	}

	if (Token.IsIdentifier())
	{
		return true;
	}

	UngetToken(Token);
	return false;
}

// Modify token to fix redirected types if needed
void FBaseParser::RedirectTypeIdentifier(FToken& Token) const
{
	check(Token.IsIdentifier());

	const FString* FoundRedirect = UHTConfig.TypeRedirectMap.Find(FString(Token.Value));
	if (FoundRedirect)
	{
		Token.Value = *FoundRedirect;
	}
}

bool FBaseParser::GetConstInt(int32& Result, const TCHAR* Tag)
{
	FToken Token;
	if (GetToken(Token))
	{
		if (Token.GetConstInt(Result))
		{
			return true;
		}
		else
		{
			UngetToken(Token);
		}
	}

	if (Tag != NULL)
	{
		Throwf(TEXT("%s: Missing constant integer"), Tag );
	}

	return false;
}

bool FBaseParser::GetConstInt64(int64& Result, const TCHAR* Tag)
{
	FToken Token;
	if (GetToken(Token))
	{
		if (Token.GetConstInt64(Result))
		{
			return true;
		}
		else
		{
			UngetToken(Token);
		}
	}

	if (Tag != NULL)
	{
		Throwf(TEXT("%s: Missing constant integer"), Tag );
	}

	return false;
}

bool FBaseParser::MatchSymbol( const TCHAR Match, ESymbolParseOption bParseTemplateClosingBracket/*=ESymbolParseOption::Normal*/ )
{
	FToken Token;

	if (GetToken(Token, /*bNoConsts=*/ true, bParseTemplateClosingBracket))
	{
		if (Token.IsSymbol(Match))
		{
			return true;
		}
		else
		{
			UngetToken(Token);
		}
	}

	return false;
}

bool FBaseParser::MatchSymbol(const TCHAR* Match, ESymbolParseOption bParseTemplateClosingBracket/*=ESymbolParseOption::Normal*/)
{
	FToken Token;

	if (GetToken(Token, /*bNoConsts=*/ true, bParseTemplateClosingBracket))
	{
		if (Token.IsSymbol(Match, ESearchCase::CaseSensitive))
		{
			return true;
		}
		else
		{
			UngetToken(Token);
		}
	}

	return false;
}

bool FBaseParser::MatchIdentifier( const TCHAR* Match, ESearchCase::Type SearchCase)
{
	FToken Token;
	if (GetToken(Token))
	{
		if (Token.IsIdentifier(Match, SearchCase))
		{
			return true;
		}
		else
		{
			UngetToken(Token);
		}
	}
	
	return false;
}

bool FBaseParser::MatchConstInt( const TCHAR* Match )
{
	FToken Token;
	if (GetToken(Token))
	{
		if (Token.IsConstInt(Match))
		{
			return true;
		}
		else
		{
			UngetToken(Token);
		}
	}
	
	return false;
}

bool FBaseParser::MatchAnyConstInt()
{
	FToken Token;
	if (GetToken(Token))
	{
		if (Token.IsConstInt())
		{
			return true;
		}
		else
		{
			UngetToken(Token);
		}
	}

	return false;
}

void FBaseParser::MatchSemi()
{
	if( !MatchSymbol(TEXT(';')) )
	{
		FToken Token;
		if( GetToken(Token) )
		{
			Throwf(TEXT("Missing ';' before '%s'"), *FString(Token.Value));
		}
		else
		{
			Throwf(TEXT("Missing ';'") );
		}
	}
}


//
// Peek ahead and see if a symbol follows in the stream.
//
bool FBaseParser::PeekSymbol( const TCHAR Match )
{
	FToken Token;
	if (!GetToken(Token, true))
	{
		return false;
	}
	UngetToken(Token);

	return Token.IsSymbol(Match);
}

//
// Peek ahead and see if an identifier follows in the stream.
//
bool FBaseParser::PeekIdentifier( const TCHAR* Match, ESearchCase::Type SearchCase)
{
	FToken Token;
	if (!GetToken(Token, true))
	{
		return false;
	}
	UngetToken(Token);
	return Token.IsIdentifier(Match, SearchCase);
}

//
// Unget the most recently gotten token.
//
void FBaseParser::UngetToken( const FToken& Token )
{
	InputPos = Token.StartPos;
	InputLine = Token.StartLine;
	if (bRecordTokens)
	{
		while (!RecordedTokens.IsEmpty() && RecordedTokens.Last().StartPos >= InputPos)
		{
			RecordedTokens.RemoveAt(RecordedTokens.Num() - 1);
		}
	}
}

void FBaseParser::UngetToken(int32 StartLine, int32 StartPos)
{
	InputLine = StartLine;
	InputPos = StartPos;
	if (bRecordTokens)
	{
		while (!RecordedTokens.IsEmpty() && RecordedTokens.Last().StartPos >= InputPos)
		{
			RecordedTokens.RemoveAt(RecordedTokens.Num() - 1);
		}
	}
}

//
// Require a symbol.
//
void FBaseParser::RequireSymbol( const TCHAR Match, const TCHAR* Tag, ESymbolParseOption bParseTemplateClosingBracket/*=ESymbolParseOption::Normal*/ )
{
	if (!MatchSymbol(Match, bParseTemplateClosingBracket))
	{
		Throwf(TEXT("Missing '%c' in %s"), Match, Tag );
	}
}

void FBaseParser::RequireSymbol(const TCHAR Match, const FStringView& Tag, ESymbolParseOption bParseTemplateClosingBracket/*=ESymbolParseOption::Normal*/)
{
	if (!MatchSymbol(Match, bParseTemplateClosingBracket))
	{
		Throwf(TEXT("Missing '%c' in %s"), Match, *FString(Tag));
	}
}

void FBaseParser::RequireSymbol(const TCHAR Match, TFunctionRef<FString()> ErrorGetter, ESymbolParseOption bParseTemplateClosingBracket/*=ESymbolParseOption::Normal*/)
{
	if (!MatchSymbol(Match, bParseTemplateClosingBracket))
	{
		Throwf(TEXT("Missing '%c' in %s"), Match, *ErrorGetter());
	}
}

//
// Require an integer.
//
void FBaseParser::RequireConstInt( const TCHAR* Match, const TCHAR* Tag )
{
	if (!MatchConstInt(Match))
	{
		Throwf(TEXT("Missing integer '%s' in %s"), Match, Tag );
	}
}

void FBaseParser::RequireAnyConstInt( const TCHAR* Tag )
{
	if (!MatchAnyConstInt())
	{
		Throwf(TEXT("Missing integer in %s"), Tag );
	}
}

//
// Require an identifier.
//
void FBaseParser::RequireIdentifier( const TCHAR* Match, const ESearchCase::Type SearchCase, const TCHAR* Tag )
{
	if (!MatchIdentifier(Match, SearchCase))
	{
		Throwf(TEXT("Missing '%s' in %s"), Match, Tag );
	}
}

// Clears out the stored comment.
void FBaseParser::ClearComment()
{
	PrevComment.Reset();
}

// Reads a new-style value
//@TODO: UCREMOVAL: Needs a better name
FString FBaseParser::ReadNewStyleValue(const TCHAR* TypeOfSpecifier)
{
	FToken ValueToken;
	if (!GetToken(ValueToken, false))
	{
		Throwf(TEXT("Expected a value when handling a %s"), TypeOfSpecifier);
	}

	FString Result;

	if (ValueToken.IsIdentifier() || ValueToken.IsSymbol())
	{
		Result = FString(ValueToken.Value);

		if (MatchSymbol(TEXT('=')))
		{
			Result += TEXT("=");
			Result += ReadNewStyleValue(TypeOfSpecifier);
		}
	}
	else
	{
		Result = ValueToken.GetConstantValue();
	}

	return Result;
}

// Reads [ Value | ['(' Value [',' Value]* ')'] ] and places each value into the Items array
bool FBaseParser::ReadOptionalCommaSeparatedListInParens(TArray<FString>& Items, const TCHAR* TypeOfSpecifier)
{
	if (MatchSymbol(TEXT('(')))
	{
		do 
		{
			FString Value = ReadNewStyleValue(TypeOfSpecifier);
			Items.Add(Value);
		} while ( MatchSymbol(TEXT(',')) );

		RequireSymbol(TEXT(')'), TypeOfSpecifier);

		return true;
	}

	return false;
}

void FBaseParser::ParseNameWithPotentialAPIMacroPrefix(FString& DeclaredName, FString& RequiredAPIMacroIfPresent, const TCHAR* FailureMessage)
{
	// Expecting Name | (MODULE_API Name)
	FToken NameToken;

	// Read an identifier
	if (!GetIdentifier(NameToken))
	{
		Throwf(TEXT("Missing %s name"), FailureMessage);
	}

	// Is the identifier the name or an DLL import/export API macro?
	FString NameTokenStr(NameToken.Value);
	if (NameTokenStr.EndsWith(TEXT("_API"), ESearchCase::CaseSensitive))
	{
		RequiredAPIMacroIfPresent = MoveTemp(NameTokenStr);

		// Read the real name
		if (!GetIdentifier(NameToken))
		{
			Throwf(TEXT("Missing %s name"), FailureMessage);
		}
		DeclaredName = FString(NameToken.Value);
	}
	else
	{
		DeclaredName = MoveTemp(NameTokenStr);
		RequiredAPIMacroIfPresent.Reset();
	}
}

// Reads a set of specifiers (with optional values) inside the () of a new-style metadata macro like UPROPERTY or UFUNCTION
void FBaseParser::ReadSpecifierSetInsideMacro(TArray<FPropertySpecifier>& SpecifiersFound, const TCHAR* TypeOfSpecifier, TMap<FName, FString>& MetaData)
{
	int32 FoundSpecifierCount = 0;

	auto ErrorMessageGetter = [TypeOfSpecifier]() { return FString::Printf(TEXT("%s declaration specifier"), TypeOfSpecifier); };

	RequireSymbol(TEXT('('), ErrorMessageGetter);

	while (!MatchSymbol(TEXT(')')))
	{
		if (FoundSpecifierCount > 0)
		{
			RequireSymbol(TEXT(','), ErrorMessageGetter);
		}
		++FoundSpecifierCount;

		// Read the specifier key
		FToken Specifier;
		if (!GetToken(Specifier))
		{
			Throwf(TEXT("Expected %s"), *ErrorMessageGetter());
		}

		if (Specifier.IsIdentifier(TEXT("meta"), ESearchCase::IgnoreCase))
		{
			RequireSymbol(TEXT('='), ErrorMessageGetter);
			RequireSymbol(TEXT('('), ErrorMessageGetter);

			// Keep reading comma-separated metadata pairs
			do 
			{
				// Read a key
				FToken MetaKeyToken;
				if (!GetIdentifier(MetaKeyToken))
				{
					Throwf(TEXT("Expected a metadata key"));
				}

				FString Key(MetaKeyToken.Value);

				// Potentially read a value
				FString Value;
				if (MatchSymbol(TEXT('=')))
				{
					Value = ReadNewStyleValue(TypeOfSpecifier);
				}

				// Validate the value is a valid type for the key and insert it into the map
				InsertMetaDataPair(MetaData, MoveTemp(Key), MoveTemp(Value));
			} while ( MatchSymbol(TEXT(',')) );

			RequireSymbol(TEXT(')'), ErrorMessageGetter);
		}
		// Look up specifier in metadata dictionary
		else if (FMetadataKeyword* MetadataKeyword = GetMetadataKeyword(Specifier))
		{
			if (MatchSymbol(TEXT('=')))
			{
				if (MetadataKeyword->ValueArgument == EMetadataValueArgument::None)
				{
					Throwf(TEXT("Incorrect = after metadata specifier '%s'"), *Specifier.GetTokenValue());
				}

				FString Value = ReadNewStyleValue(TypeOfSpecifier);
				MetadataKeyword->ApplyToMetadata(*this, MetaData, &Value);
			}
			else
			{
				if (MetadataKeyword->ValueArgument == EMetadataValueArgument::Required)
				{
					Throwf(TEXT("Missing = after metadata specifier '%s'"), *Specifier.GetTokenValue());
				}

				MetadataKeyword->ApplyToMetadata(*this, MetaData);
			}
		}
		else
		{
			// Creating a new specifier
			SpecifiersFound.Emplace(FString(Specifier.Value));

			// Look for a value for this specifier
			if (MatchSymbol(TEXT('=')) || PeekSymbol(TEXT('(')))
			{
				TArray<FString>& NewPairValues = SpecifiersFound.Last().Values;
				if (!ReadOptionalCommaSeparatedListInParens(NewPairValues, TypeOfSpecifier))
				{
					NewPairValues.Add(ReadNewStyleValue(TypeOfSpecifier));
				}
			}
		}
	}
}

void FBaseParser::InsertMetaDataPair(TMap<FName, FString>& MetaData, FString Key, FString Value)
{
	// make sure the key is valid
	if (Key.Len() == 0)
	{
		Throwf(TEXT("Invalid metadata"));
	}

	// trim extra white space and quotes
	Key.TrimStartAndEndInline();

	InsertMetaDataPair(MetaData, FName(*Key), MoveTemp(Value));
}

void FBaseParser::InsertMetaDataPair(TMap<FName, FString>& MetaData, FName KeyName, FString Value)
{
	Value.TrimStartAndEndInline();
	Value.TrimQuotesInline();

	FString* ExistingValue = MetaData.Find(KeyName);
	if (ExistingValue && Value != *ExistingValue)
	{
		Throwf(TEXT("Metadata key '%s' first seen with value '%s' then '%s'"), *KeyName.ToString(), **ExistingValue, *Value);
	}

	// finally we have enough to put it into our metadata
	MetaData.Add(KeyName, MoveTemp(Value));
}

void FBaseParser::InitMetadataKeywords()
{
	InitMetadataKeyswordsInternal();
}

EUnderlyingEnumType FBaseParser::ParseUnderlyingEnumType()
{
	EUnderlyingEnumType UnderlyingType = EUnderlyingEnumType::uint8;
	if (MatchSymbol(TEXT(':')))
	{
		FToken BaseToken;
		if (!GetIdentifier(BaseToken))
		{
			Throwf(TEXT("Missing enum base"));
		}

		if (BaseToken.IsValue(TEXT("uint8"), ESearchCase::CaseSensitive))
		{
			UnderlyingType = EUnderlyingEnumType::uint8;
		}
		else if (BaseToken.IsValue(TEXT("uint16"), ESearchCase::CaseSensitive))
		{
			UnderlyingType = EUnderlyingEnumType::uint16;
		}
		else if (BaseToken.IsValue(TEXT("uint32"), ESearchCase::CaseSensitive))
		{
			UnderlyingType = EUnderlyingEnumType::uint32;
		}
		else if (BaseToken.IsValue(TEXT("uint64"), ESearchCase::CaseSensitive))
		{
			UnderlyingType = EUnderlyingEnumType::uint64;
		}
		else if (BaseToken.IsValue(TEXT("int8"), ESearchCase::CaseSensitive))
		{
			UnderlyingType = EUnderlyingEnumType::int8;
		}
		else if (BaseToken.IsValue(TEXT("int16"), ESearchCase::CaseSensitive))
		{
			UnderlyingType = EUnderlyingEnumType::int16;
		}
		else if (BaseToken.IsValue(TEXT("int32"), ESearchCase::CaseSensitive))
		{
			UnderlyingType = EUnderlyingEnumType::int32;
		}
		else if (BaseToken.IsValue(TEXT("int64"), ESearchCase::CaseSensitive))
		{
			UnderlyingType = EUnderlyingEnumType::int64;
		}
		else
		{
			Throwf(TEXT("Unsupported enum class base type: %s"), *BaseToken.GetTokenValue());
		}
	}
	else
	{
		UnderlyingType = EUnderlyingEnumType::Unspecified;
	}

	return UnderlyingType;
}

