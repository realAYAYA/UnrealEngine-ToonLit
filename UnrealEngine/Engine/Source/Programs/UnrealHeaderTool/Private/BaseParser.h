// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Exceptions.h"
#include "GeneratedCodeVersion.h"
#include "ParserHelper.h"
#include "Containers/UnrealString.h"

enum class EPointerMemberBehavior
{
	Disallow,
	AllowSilently,
	AllowAndLog,
};

enum class EUnderlyingEnumType
{
	Unspecified,
	uint8,
	uint16,
	uint32,
	uint64,
	int8,
	int16,
	int32,
	int64
};

using FMetaData = TMap<FName, FString>;

/////////////////////////////////////////////////////
// UHTConfig

struct FUHTConfig
{
	static const FUHTConfig& Get();

	// Types that have been renamed, treat the old deprecated name as the new name for code generation
	TMap<FString, FString> TypeRedirectMap;

	// Special parsed struct names that do not require a prefix
	TArray<FString> StructsWithNoPrefix;

	// Special parsed struct names that have a 'T' prefix
	TArray<FString> StructsWithTPrefix;

	// Mapping from 'human-readable' macro substring to # of parameters for delegate declarations
	// Index 0 is 1 parameter, Index 1 is 2, etc...
	TArray<FString> DelegateParameterCountStrings;

	// Default version of generated code. Defaults to oldest possible, unless specified otherwise in config.
	EGeneratedCodeVersion DefaultGeneratedCodeVersion = EGeneratedCodeVersion::V1;

	EPointerMemberBehavior EngineNativePointerMemberBehavior = EPointerMemberBehavior::AllowSilently;
	EPointerMemberBehavior EngineObjectPtrMemberBehavior = EPointerMemberBehavior::AllowSilently;

	EPointerMemberBehavior EnginePluginNativePointerMemberBehavior = EPointerMemberBehavior::AllowSilently;
	EPointerMemberBehavior EnginePluginObjectPtrMemberBehavior = EPointerMemberBehavior::AllowSilently;

	EPointerMemberBehavior NonEngineNativePointerMemberBehavior = EPointerMemberBehavior::AllowSilently;
	EPointerMemberBehavior NonEngineObjectPtrMemberBehavior = EPointerMemberBehavior::AllowSilently;

private:
	FUHTConfig();
};

/////////////////////////////////////////////////////
// Token types.

enum class ETokenType
{
	None,			// No token.
	Identifier,		// Alphanumeric identifier.
	Symbol,			// Symbol.
	TrueConst,		// True value via identifier
	FalseConst,		// False value via identifier
	FloatConst,		// Floating point constant
	DecimalConst,	// Decimal Integer constant
	HexConst,		// Hex integer constant
	CharConst,		// Single character constant
	StringConst,	// String constant
};

// Helper structure for strings.  Only valid when called on a StringConst or CharConst.
// The surrounding quotes are removed and \n escape characters are processed.
struct FTokenString
{
	TCHAR String[MAX_STRING_CONST_SIZE];

	FTokenString()
	{
		String[0] = TEXT('\0');
	}

	const TCHAR* operator *() const
	{
		return String;
	}
};

// Helper structure for null terminated value.  This will be the raw text from the token.
struct FTokenValue
{
	TCHAR Value[NAME_SIZE];

	FTokenValue()
	{
		Value[0] = TEXT('\0');
	}

	const TCHAR* operator *() const
	{
		return Value;
	}
};

/////////////////////////////////////////////////////
// FToken

class FToken
{
public:
	/** Type of token. */
	ETokenType TokenType = ETokenType::None;

	/** Starting position in script where this token came from. */
	int32 StartPos = 0;

	/** Starting line in script. */
	int32 StartLine = 0;

	/** Input line of the token */
	int32 InputLine = 0;

	/** The text of the token */
	FStringView Value;

	// Return true if the token is an identifier
	bool IsIdentifier() const
	{
		return TokenType == ETokenType::Identifier;
	}

	// Test to see if the token value matches the given string
	bool IsValue(const TCHAR* Str, ESearchCase::Type SearchCase) const
	{
		return Value.Equals(Str, SearchCase);
	}

	// Test to see if the token value begins with the given string
	bool ValueStartsWith(const TCHAR* Str, ESearchCase::Type SearchCase) const
	{
		return Value.StartsWith(Str, SearchCase);
	}

	// Return true if the token is a specific identifier
	bool IsIdentifier(const TCHAR* Str, ESearchCase::Type SearchCase) const
	{
		return IsIdentifier() && IsValue(Str, SearchCase);
	}

	// Return true if the token is a symbol
	bool IsSymbol() const
	{
		return TokenType == ETokenType::Symbol;
	}

	// Return true if the token is a specific single character symbol
	bool IsSymbol(const TCHAR Ch) const
	{
		return IsSymbol() && Value.Len() == 1 && Value[0] == Ch;
	}

	// Return true if the token is a specific symbol
	bool IsSymbol(const TCHAR* Str, ESearchCase::Type SearchCase) const
	{
		return IsSymbol() && IsValue(Str, SearchCase);
	}

	// Return true if the token is a decimal or hexidecimal constant
	bool IsConstInt() const
	{
		return TokenType == ETokenType::DecimalConst || TokenType == ETokenType::HexConst;
	}

	// Return true if the token is a specific constant integer
	bool IsConstInt(const TCHAR* Str) const
	{
		return IsConstInt() && IsValue(Str, ESearchCase::CaseSensitive);
	}

	// Return true if the token is a floating point constant
	bool IsConstFloat() const
	{
		return TokenType == ETokenType::FloatConst;
	}

	// Return true if the token is a string or character constant
	bool IsConstString() const
	{
		return TokenType == ETokenType::StringConst || TokenType == ETokenType::CharConst;
	}

	// Get the 32 bit integer value of the token.  Only supported for decimal, hexidecimal, and floating point values
	bool GetConstInt(int32& I) const
	{
		switch (TokenType)
		{
		case ETokenType::DecimalConst:
			I = (int32)GetDecimalValue();
			return true;
		case ETokenType::HexConst:
			I = (int32)GetHexValue();
			return true;
		case ETokenType::FloatConst:
		{
			float Float = GetFloatValue();
			if (Float == (float)FMath::TruncToInt(Float))
			{
				I = (int32)Float;
				return true;
			}
			return false;
		}
		default:
			return false;
		}
	}

	// Get the 64 bit integer value of the token.  Only supported for decimal, hexidecimal, and floating point values
	bool GetConstInt64(int64& I) const
	{
		switch (TokenType)
		{
		case ETokenType::DecimalConst:
			I = GetDecimalValue();
			return true;
		case ETokenType::HexConst:
			I = GetHexValue();
			return true;
		case ETokenType::FloatConst:
		{
			float Float = GetFloatValue();
			if (Float == (float)FMath::TruncToInt(Float))
			{
				I = (int32)Float;
				return true;
			}
			return false;
		}
		default:
			return false;
		}
	}

	// Return a string representation of a constant value.  In the case of strings, the surrounding quotes and \n escapres are removed/converted
	FString GetConstantValue() const
	{
		switch (TokenType)
		{
		case ETokenType::DecimalConst:
			return FString::Printf(TEXT("%" INT64_FMT), GetDecimalValue());
		case ETokenType::HexConst:
			return FString::Printf(TEXT("%" INT64_FMT), GetHexValue());
		case ETokenType::FloatConst:
			return FString::Printf(TEXT("%f"), GetFloatValue());
		case ETokenType::TrueConst:
			return FName::GetEntry(NAME_TRUE)->GetPlainNameString();
		case ETokenType::FalseConst:
			return FName::GetEntry(NAME_FALSE)->GetPlainNameString();
		case ETokenType::CharConst:
		case ETokenType::StringConst:
			return FString(*GetTokenString());
		default:
			return TEXT("NotConstant");
		}
	}

	// Return the token value as a null terminated string.
	void GetTokenValue(FTokenValue& TokenValue) const
	{
		Value.CopyString(TokenValue.Value, Value.Len(), 0);
		TokenValue.Value[Value.Len()] = TEXT('\0');
	}

	// Return the token value as a null terminated string.
	FTokenValue GetTokenValue() const
	{
		FTokenValue TokenValue;
		GetTokenValue(TokenValue);
		return TokenValue;
	}

	// Return the token value for string contants
	void GetTokenString(FTokenString& TokenString) const
	{
		switch (TokenType)
		{
		case ETokenType::StringConst:
		{
			TCHAR* Out = TokenString.String;;
			const TCHAR* Pos = &Value[1];
			TCHAR c = *Pos++;
			while (c != TEXT('"'))
			{
				if (c == TEXT('\\'))
				{
					c = *Pos++;
					if (c == TEXT('n'))
					{
						// Newline escape sequence.
						c = TEXT('\n');
					}
				}
				*Out++ = c;
				c = *Pos++;
			}
			*Out++ = TEXT('\0');
			// If this fails, GetToken has a bug/mismatch
			check(Out - TokenString.String < MAX_STRING_CONST_SIZE);
			break;
		}

		case ETokenType::CharConst:
		{
			TCHAR ActualCharLiteral = Value[1];

			if (ActualCharLiteral == TEXT('\\'))
			{
				ActualCharLiteral = Value[2];
				switch (ActualCharLiteral)
				{
				case TCHAR('t'):
					ActualCharLiteral = TEXT('\t');
					break;
				case TCHAR('n'):
					ActualCharLiteral = TEXT('\n');
					break;
				case TCHAR('r'):
					ActualCharLiteral = TEXT('\r');
					break;
				}
			}
			TokenString.String[0] = ActualCharLiteral;
			TokenString.String[1] = TEXT('\0');
			break;
		}

		default:
			checkf(false, TEXT("Call to GetTokenString on token that isn't a string or char constant"));
		}
	}

	// Return the token value for string contants
	FTokenString GetTokenString() const
	{
		FTokenString TokenString;
		GetTokenString(TokenString);
		return TokenString;
	}

private:
	float GetFloatValue() const
	{
		int32 Length = Value.Len();
		TCHAR* Temp = reinterpret_cast<TCHAR*>(alloca((Length + 1) * sizeof(TCHAR)));
		Value.CopyString(Temp, Length, 0);
		Temp[Length] = TEXT('\0');
		return FCString::Atof(Temp);
	}

	int64 GetDecimalValue() const
	{
		TCHAR* End = const_cast<TCHAR*>(Value.begin()) + Value.Len();
		return FCString::Strtoi64(Value.begin(), &End, 10);
	}

	int64 GetHexValue() const
	{
		TCHAR* End = const_cast<TCHAR*>(Value.begin()) + Value.Len();
		return FCString::Strtoi64(Value.begin(), &End, 16);
	}
};

/////////////////////////////////////////////////////
// FBaseParser

enum class ESymbolParseOption
{
	Normal,
	CloseTemplateBracket
};

// A specifier with optional value
struct FPropertySpecifier
{
public:
	explicit FPropertySpecifier(FString&& InKey)
		: Key(MoveTemp(InKey))
	{
	}

	explicit FPropertySpecifier(const FString& InKey)
		: Key(InKey)
	{
	}

	FString Key;
	TArray<FString> Values;

	FString ConvertToString() const;
};

//
// Base class of header parsers.
//

class FBaseParser 
	: public FUHTMessageProvider
{
protected:
	FBaseParser(FUnrealSourceFile& InSourceFile);
	virtual ~FBaseParser() = default;

	// UHTConfig data
	const FUHTConfig& UHTConfig;

public:
	// Source being parsed
	FUnrealSourceFile& SourceFile;

	// Input text.
	const TCHAR* Input;

	// Length of input text.
	int32 InputLen;

	// Current position in text.
	int32 InputPos;

	// Current line in text.
	int32 InputLine;

	// Position previous to last GetChar() call.
	int32 PrevPos;

	// Line previous to last GetChar() call.
	int32 PrevLine;

	// Previous comment parsed by GetChar() call.
	FString PrevComment;

	// Number of statements parsed.
	int32 StatementsParsed;

	// Total number of lines parsed.
	int32 LinesParsed;

	virtual FString GetFilename() const override;

	virtual int32 GetLineNumber() const override
	{
		return InputLine;
	};

	void ResetParser(const TCHAR* SourceBuffer, int32 StartingLineNumber = 1);

	// Low-level parsing functions.
	TCHAR GetChar( bool Literal = false );
	TCHAR PeekChar();
	TCHAR GetLeadingChar();
	void UngetChar();
	void SkipWhitespaceAndComments();

	/**
	 * Tests if a character is an end-of-line character.
	 *
	 * @param	c	The character to test.
	 *
	 * @return	true if c is an end-of-line character, false otherwise.
	 */
	static bool IsEOL( TCHAR c );

	/**
	 * Tests if a character is a whitespace character.
	 *
	 * @param	c	The character to test.
	 *
	 * @return	true if c is an whitespace character, false otherwise.
	 */
	static bool IsWhitespace( TCHAR c );

	/**
	 * Gets the next token from the input stream, advancing the variables which keep track of the current input position and line.
	 *
	 * @param	Token						receives the value of the parsed text; if Token is pre-initialized, special logic is performed
	 *										to attempt to evaluated Token in the context of that type.  Useful for distinguishing between ambigous symbols
	 *										like enum tags.
	 * @param	NoConsts					specify true to indicate that tokens representing literal const values are not allowed.
	 * @param	ParseTemplateClosingBracket	specify true to treat >> as two template closing brackets instead of shift operator.
	 *
	 * @return	true if a token was successfully processed, false otherwise.
	 */
	bool GetToken( FToken& Token, bool bNoConsts = false, ESymbolParseOption ParseTemplateClosingBracket = ESymbolParseOption::Normal );

	/**
	 * Put all text from the current position up to either EOL or the StopToken
	 * into string.  Advances the compiler's current position.
	 *
	 * @param	String		[out] will contain the text that was parsed
	 * @param	StopChar	stop processing when this character is reached
	 *
	 * @return	true if a token was parsed
	 */
	bool GetRawString( FTokenString& String, TCHAR StopChar = TCHAR('\n') );

	// Doesn't quit if StopChar is found inside a double-quoted string, but does not support quote escapes
	bool GetRawStringRespectingQuotes(FTokenString& String, TCHAR StopChar = TCHAR('\n') );

	void UngetToken( const FToken& Token );
	void UngetToken(int32 StartLine, int32 StartPos);
	bool GetIdentifier( FToken& Token, bool bNoConsts = false );

	// Modify token to fix redirected types if needed
	void RedirectTypeIdentifier(FToken& Token) const;

	/**
	 * Get an int constant
	 * @return true on success, otherwise false.
	 */
	bool GetConstInt(int32& Result, const TCHAR* Tag = NULL);
	bool GetConstInt64(int64& Result, const TCHAR* Tag = NULL);

	// Matching predefined text.
	bool MatchIdentifier( const TCHAR* Match, ESearchCase::Type SearchCase);
	bool MatchConstInt( const TCHAR* Match );
	bool MatchAnyConstInt();
	bool PeekIdentifier( const TCHAR* Match, ESearchCase::Type SearchCase);
	bool MatchSymbol( const TCHAR Match, ESymbolParseOption bParseTemplateClosingBracket = ESymbolParseOption::Normal );
	bool MatchSymbol(const TCHAR* Match, ESymbolParseOption bParseTemplateClosingBracket = ESymbolParseOption::Normal);
	void MatchSemi();
	bool PeekSymbol( const TCHAR Match );

	// Requiring predefined text.
	void RequireIdentifier( const TCHAR* Match, ESearchCase::Type SearchCase, const TCHAR* Tag );
	void RequireSymbol(const TCHAR Match, const TCHAR* Tag, ESymbolParseOption bParseTemplateClosingBracket = ESymbolParseOption::Normal);
	void RequireSymbol(const TCHAR Match, const FStringView& Tag, ESymbolParseOption bParseTemplateClosingBracket = ESymbolParseOption::Normal);
	void RequireSymbol(const TCHAR Match, TFunctionRef<FString()> TagGetter, ESymbolParseOption bParseTemplateClosingBracket = ESymbolParseOption::Normal);
	void RequireConstInt( const TCHAR* Match, const TCHAR* Tag );
	void RequireAnyConstInt( const TCHAR* Tag );

	/** Clears out the stored comment. */
	void ClearComment();

	// Reads a new-style value
	//@TODO: UCREMOVAL: Needs a better name
	FString ReadNewStyleValue(const TCHAR* TypeOfSpecifier);

	// Reads ['(' Value [',' Value]* ')'] and places each value into the Items array
	bool ReadOptionalCommaSeparatedListInParens(TArray<FString>& Items, const TCHAR* TypeOfSpecifier);

	//////////////
	// Complicated* parsing code that needs to be shared between the preparser and the parser
	// (* i.e., doesn't really belong in the base parser)

	// Expecting Name | (MODULE_API Name)
	//  Places Name into DeclaredName
	//  Places MODULE_API (where MODULE varies) into RequiredAPIMacroIfPresent
	// FailureMessage is printed out if the expectation is broken.
	void ParseNameWithPotentialAPIMacroPrefix(FString& DeclaredName, FString& RequiredAPIMacroIfPresent, const TCHAR* FailureMessage);

	// Reads a set of specifiers (with optional values) inside the () of a new-style metadata macro like UPROPERTY or UFUNCTION
	void ReadSpecifierSetInsideMacro(TArray<FPropertySpecifier>& SpecifiersFound, const TCHAR* TypeOfSpecifier, TMap<FName, FString>& MetaData);

	// Validates and inserts one key-value pair into the meta data map
	void InsertMetaDataPair(TMap<FName, FString>& MetaData, FString InKey, FString InValue);

	// Validates and inserts one key-value pair into the meta data map
	void InsertMetaDataPair(TMap<FName, FString>& MetaData, FName InKey, FString InValue);

	/**
	 * Parse class/struct inheritance.
	 *
	 * @param What				The name of the statement we are parsing.  (i.e. 'class')
	 * @param InLambda			Function to call for every parent.  Must be in the form of
	 *							Lambda(const TCHAR* Identifier, bool bSuperClass)
	 */
	template <typename Lambda>
	void ParseInheritance(const TCHAR* What, Lambda&& InLambda);

	/**
	 * Parse the underlying enum type
	 */
	EUnderlyingEnumType ParseUnderlyingEnumType();

	//////////////

	// Initialize the metadata keywords prior to parsing
	static void InitMetadataKeywords();

	TArray<FToken> RecordedTokens;
	bool bRecordTokens = false;
};

template <typename Lambda>
void FBaseParser::ParseInheritance(const TCHAR* What, Lambda&& InLambda)
{

	if (!MatchSymbol(TEXT(':')))
	{
		return;
	}

	// Process the super class 
	{
		FToken Token;
		RequireIdentifier(TEXT("public"), ESearchCase::CaseSensitive, TEXT("inheritance"));
		if (!GetIdentifier(Token))
		{
			Throwf(TEXT("Missing %s name"), What);
		}
		RedirectTypeIdentifier(Token);
		InLambda(*FString(Token.Value), true);
	}

	// Handle additional inherited interface classes
	while (MatchSymbol(TEXT(',')))
	{
		RequireIdentifier(TEXT("public"), ESearchCase::CaseSensitive, TEXT("Interface inheritance must be public"));

		FString InterfaceName;

		for (;;)
		{
			FToken Token;
			if (!GetIdentifier(Token, true))
			{
				Throwf(TEXT("Failed to get interface class identifier"));
			}

			InterfaceName += Token.Value;

			// Handle templated native classes
			if (MatchSymbol(TEXT('<')))
			{
				InterfaceName += TEXT('<');

				int32 NestedScopes = 1;
				while (NestedScopes)
				{
					if (!GetToken(Token))
					{
						Throwf(TEXT("Unexpected end of file"));
					}

					if (Token.IsSymbol(TEXT('<')))
					{
						++NestedScopes;
					}
					else if (Token.IsSymbol(TEXT('>')))
					{
						--NestedScopes;
					}

					InterfaceName += Token.Value;
				}
			}

			// Handle scoped native classes
			if (MatchSymbol(TEXT("::")))
			{
				InterfaceName += TEXT("::");

				// Keep reading nested identifiers
				continue;
			}

			break;
		}
		InLambda(*InterfaceName, false);
	}
}

class FTokenReplay
{
public:
	explicit FTokenReplay(const TArray<FToken>& InTokens)
		: Tokens(InTokens)
	{
	}

	bool GetToken(FToken& Token)
	{
		if (CurrentIndex < Tokens.Num())
		{
			Token = Tokens[CurrentIndex++];
			return true;
		}
		else
		{
			Token = FToken();
			return false;
		}
	}

	void UngetToken(const FToken& Token)
	{
		while (CurrentIndex > 0)
		{
			--CurrentIndex;
			if (Tokens[CurrentIndex].StartPos == Token.StartPos)
			{
				break;
			}
		}
	}

	bool MatchIdentifier(const TCHAR* Match, ESearchCase::Type SearchCase)
	{
		if (CurrentIndex < Tokens.Num() && Tokens[CurrentIndex].IsIdentifier(Match, SearchCase))
		{
			++CurrentIndex;
			return true;
		}
		return false;
	}

	bool MatchSymbol(const TCHAR Match)
	{
		if (CurrentIndex < Tokens.Num() && Tokens[CurrentIndex].IsSymbol(Match))
		{
			++CurrentIndex;
			return true;
		}
		return false;
	}

	bool MatchSymbol(const TCHAR* Match)
	{
		if (CurrentIndex < Tokens.Num() && Tokens[CurrentIndex].IsSymbol(Match, ESearchCase::CaseSensitive))
		{
			++CurrentIndex;
			return true;
		}
		return false;
	}

private:
	const TArray<FToken>& Tokens;
	int CurrentIndex = 0;
};