// Copyright Epic Games, Inc. All Rights Reserved.


#include "OptimusExpressionEvaluator.h"

#include <limits>

namespace Optimus::Expression
{

FEngine::FEngine(
	const TMap<FName, int32>& InConstants, 
	EParseFlags InParseFlags
	) :
	Constants(InConstants),
	ParseFlags(InParseFlags)
{
}


FEngine::FEngine(
	TMap<FName, int32>&& InConstants,
	EParseFlags InParseFlags
	) :
	Constants(MoveTemp(InConstants)),
	ParseFlags(InParseFlags)
{
}


void FEngine::UpdateConstantValues(
	const TMap<FName, int32>& InConstants
	)
{
	for (const TTuple<FName, int32>& NewConstantItem: InConstants)
	{
		if (int32* OldConstant = Constants.Find(NewConstantItem.Key))
		{
			*OldConstant = NewConstantItem.Value; 
		}
	}
}

// We can't use FChar::IsDigit, since it obeys locale, which would allow all sorts of non-ASCII
// numeric digits from Unicode.
static bool IsASCIIDigit(TCHAR InCh)
{
	return InCh >= '0' && InCh <= '9';
}

// Naive pow function for ints. Does not check for overflow, returns zero for negative exponents.
static int32 IntPow(int32 InBase, int32 InExponent)
{
	if (InExponent < 0)
	{
		return 0;
	}
	int32 Result = 1;
	while (InExponent)
	{
		if ((InExponent % 2) != 0)
			Result *= InBase;
		InExponent /= 2;
		InBase *= InBase;
	}
	return Result;
}

	
TVariant<FEngine::FToken, FParseError> FEngine::ParseTokenError(
	FString&& InError,
	const TCHAR* InCurrentPos,
	FStringView InExpression)
{
	return TVariant<FToken, FParseError>(TInPlaceType<FParseError>(),
		static_cast<int32>(InCurrentPos - InExpression.GetData()), MoveTemp(InError));
}


FString FEngine::TokenToString(
	const FEngine::FToken& InToken
	)
{
	const TCHAR* TypeStr = TEXT("<unknown>");
	FString ValueStr;
	
	if (const EOperatorToken* TokenKind = InToken.TryGet<EOperatorToken>())
	{
		TypeStr = TEXT("Token");
		switch(*TokenKind)
		{
		case EOperatorToken::Negate:		ValueStr = TEXT("<->"); break;
		case EOperatorToken::Add:			ValueStr = TEXT("+"); break;
		case EOperatorToken::Subtract:		ValueStr = TEXT("-"); break;
		case EOperatorToken::Multiply:		ValueStr = TEXT("*"); break;
		case EOperatorToken::Divide:		ValueStr = TEXT("/"); break;
		case EOperatorToken::Modulo:		ValueStr = TEXT("%"); break;
		case EOperatorToken::Power:			ValueStr = TEXT("**"); break;
		// case EOperatorToken::FloorDivide:	ValueStr = TEXT("//"); break;
		case EOperatorToken::ParenOpen:		ValueStr = TEXT("("); break;
		case EOperatorToken::ParenClose:	ValueStr = TEXT(")"); break;
		}
	}
	else if (const FName* Identifier = InToken.TryGet<FName>())
	{
		TypeStr = TEXT("Identifier");
		ValueStr = Identifier->ToString();
	}
	else if (const int32* Value = InToken.TryGet<int32>())
	{
		TypeStr = TEXT("Value");
		ValueStr = FString::Printf(TEXT("%d"), *Value);
	}

	return FString::Printf(TEXT("%s[%s]"), TypeStr, *ValueStr);
}


TVariant<FEngine::FToken, FParseError> FEngine::ParseInt32(
	FStringView& InOutParseRange,
	FStringView InExpression
	)
{
	int64 Value = 0;
	constexpr int32 MaxDigits = std::numeric_limits<int32>::digits10;

	// Skip over leading zeroes.
	int32 DigitStart = 0;
	for (; DigitStart < InOutParseRange.Len() && InOutParseRange[DigitStart] == '0'; DigitStart++)
	{
		/**/
	}

	int32 DigitIndex = DigitStart; 
	for(; DigitIndex < InOutParseRange.Len(); DigitIndex++)
	{
		if ((DigitIndex - DigitStart) > MaxDigits)
		{
			return ParseTokenError(TEXT("Integer value too large"), InOutParseRange.GetData(), InExpression);
		}

		TCHAR Ch = InOutParseRange[DigitIndex];
		if (!IsASCIIDigit(Ch))
		{
			break;
		}

		Value *= 10;
		Value += Ch - '0';
	}

	if (Value > std::numeric_limits<int32>::max())
	{
		return ParseTokenError(TEXT("Integer value too large"), InOutParseRange.GetData(), InExpression);
	}

	InOutParseRange = InOutParseRange.Mid(DigitIndex);
	
	return TVariant<FToken, FParseError>(
		TInPlaceType<FToken>(), FToken(TInPlaceType<int32>(), Value));
}


TVariant<FEngine::FToken, FParseError> FEngine::ParseIdentifier(
	FStringView& InOutParseRange,
	FStringView InExpression
	) const
{
	// FIXME: We need quote escaping.
	bool bStopAtSingleQuote = false;
	int32 ParseIndex = 0;
	if (InOutParseRange[0] == '\'')
	{
		bStopAtSingleQuote = true;
		ParseIndex++;
		if (ParseIndex >= InOutParseRange.Len())
		{
			return ParseTokenError(TEXT("Missing end quote for constant"), InOutParseRange.GetData(), InExpression);
		}
	}
	// Unquoted constants have to start with an alphabetic character.
	else if (!FChar::IsAlpha(InOutParseRange[0]) && InOutParseRange[0] != '_')
	{
		return ParseTokenError(TEXT("Unexpected character"), InOutParseRange.GetData(), InExpression);
	}

	const int32 ConstantStart = ParseIndex;
	int32 ConstantEnd;

	ParseIndex++;

	if (bStopAtSingleQuote)
	{
		while(ParseIndex < InOutParseRange.Len() && InOutParseRange[ParseIndex] != '\'')
		{
			ParseIndex++;
		}

		if (ParseIndex == InOutParseRange.Len())
		{
			return ParseTokenError(TEXT("Missing end quote for constant"), InOutParseRange.GetData(), InExpression);
		}

		ConstantEnd = ParseIndex;
		ParseIndex++;
	}
	else
	{
		while(ParseIndex < InOutParseRange.Len() &&
			  (FChar::IsAlnum(InOutParseRange[ParseIndex]) || InOutParseRange[ParseIndex] == '_'))
		{
			ParseIndex++;
		}
		
		ConstantEnd = ParseIndex;
	}

	// Look up this identifier. We don't want to create a new FName, to avoid polluting the
	// FNameEntry cache.
	const FStringView Identifier(InOutParseRange.SubStr(ConstantStart, ConstantEnd - ConstantStart));
	const FName Name(Identifier.Len(), Identifier.GetData(), FNAME_Find);
	
	InOutParseRange = InOutParseRange.Mid(ParseIndex); 

	if (Constants.Contains(Name))
	{
		return TVariant<FToken, FParseError>(
			TInPlaceType<FToken>(), FToken(TInPlaceType<FName>(), Name));
	}
	else if (EnumHasAllFlags(ParseFlags, EParseFlags::ValidateConstants))
	{
		return ParseTokenError(FString::Printf(TEXT("Unknown identifier '%s'"), *FString(Identifier)),
			InOutParseRange.GetData(), InExpression); 
	}
	else
	{
		// If the constant value is not found, it gets a default value of 0.
		return TVariant<FToken, FParseError>(
			TInPlaceType<FToken>(), FToken(TInPlaceType<int32>(), 0));
	}
}


TVariant<FEngine::FToken, FParseError> FEngine::ParseToken(
	FStringView& InOutParseRange,
	FStringView InExpression
	) const
{
	auto TokenResult = [](EOperatorToken InTokenKind)
	{
		return TVariant<FToken, FParseError>(
			TInPlaceType<FToken>(), FToken(TInPlaceType<EOperatorToken>(), InTokenKind));
	};

	switch(InOutParseRange[0])
	{
	case '+':
		InOutParseRange = InOutParseRange.Mid(1);
		return TokenResult(EOperatorToken::Add);
		
	case '-':
		InOutParseRange = InOutParseRange.Mid(1);
		return TokenResult(EOperatorToken::Subtract);
		
	case '*':
		if (InOutParseRange.Len() >= 2 && InOutParseRange[1] == '*')
		{
			InOutParseRange = InOutParseRange.Mid(2);
			return TokenResult(EOperatorToken::Power);
		}
		InOutParseRange = InOutParseRange.Mid(1);
		return TokenResult(EOperatorToken::Multiply);
		
	case '/':
		/*
		if (InOutParseRange.Len() >= 2 && InOutParseRange[1] == '/')
		{
			InOutParseRange = InOutParseRange.Mid(2);
			return TokenResult(EOperatorToken::FloorDivide);
		}
		*/
		InOutParseRange = InOutParseRange.Mid(1);
		return TokenResult(EOperatorToken::Divide);
		
	case '%':
		InOutParseRange = InOutParseRange.Mid(1);
		return TokenResult(EOperatorToken::Modulo);

	case '(':
		InOutParseRange = InOutParseRange.Mid(1);
		return TokenResult(EOperatorToken::ParenOpen);
		
	case ')':
		InOutParseRange = InOutParseRange.Mid(1);
		return TokenResult(EOperatorToken::ParenClose);
		
	default:
		if (IsASCIIDigit(InOutParseRange[0]))
		{
			return ParseInt32(InOutParseRange, InExpression);
		}
		return ParseIdentifier(InOutParseRange, InExpression);
	}
}


TVariant<FExpressionObject, FParseError> FEngine::Parse(
	FStringView InExpression
	) const
{
	enum class EAssociativity
	{
		None,
		Left,
		Right
	};
	struct FOperatorTokenInfo
	{
		EOperatorToken OperatorToken;
		int32 Precedence;
		EAssociativity Associativity;
		TOptional<FExpressionObject::EOperator> Operator;
	};
	// Keep in same order as EOperatorToken. One day, if we're lucky, we'll get reflections.
	auto GetOperatorTokenInfo = [](EOperatorToken InOpToken)
	{
		static const FOperatorTokenInfo OperatorTokenInfo[] = {
			{ EOperatorToken::Negate,		5, EAssociativity::Right, FExpressionObject::EOperator::Negate},
			{ EOperatorToken::Add,			1, EAssociativity::Left, FExpressionObject::EOperator::Add},
			{ EOperatorToken::Subtract,		1, EAssociativity::Left, FExpressionObject::EOperator::Subtract},
			{ EOperatorToken::Multiply,		2, EAssociativity::Left, FExpressionObject::EOperator::Multiply},
			{ EOperatorToken::Divide,		2, EAssociativity::Left, FExpressionObject::EOperator::Divide},
			{ EOperatorToken::Modulo,		3, EAssociativity::Left, FExpressionObject::EOperator::Modulo},
			{ EOperatorToken::Power,		4, EAssociativity::Right, FExpressionObject::EOperator::Power},
			// { EOperatorToken::FloorDivide,	3, EAssociativity::Left, FExpressionObject::EOperator::FloorDivide},
			{ EOperatorToken::ParenOpen,	0, EAssociativity::None},
			{ EOperatorToken::ParenClose,	0, EAssociativity::None},
		};
		checkSlow(OperatorTokenInfo[static_cast<int32>(InOpToken)].OperatorToken == InOpToken);
		return OperatorTokenInfo[static_cast<int32>(InOpToken)];
	};
	
	static TVariant<FExpressionObject, FParseError> EmptyResult{TInPlaceType<FExpressionObject>()};

	TStringView ParseRange = InExpression.TrimStartAndEnd();
	if (ParseRange.IsEmpty())
	{
		return EmptyResult;
	}

	// Run Dijkstra's classic Shunting Yard algorithm to convert infix expressions to RPN.
	// TODO: Add support for calling functions.
	// TODO: Better error pinpointing by storing the token parse result with the token.
	TArray<FExpressionObject::OpElement, TInlineAllocator<64>> Expression;
	TArray<EOperatorToken, TInlineAllocator<32>> OperatorStack;

	auto ParseError = [InExpression](FString&& InError, const TCHAR* InCurrentPos)
	{
		return TVariant<FExpressionObject, FParseError>(TInPlaceType<FParseError>(),
			static_cast<int32>(InCurrentPos - InExpression.GetData()), MoveTemp(InError));
	};
	auto PushOperatorTokenToExpression = [GetOperatorTokenInfo, &Expression](EOperatorToken InOpToken)
	{
		Expression.Push(FExpressionObject::OpElement(TInPlaceType<FExpressionObject::EOperator>(), 
			*GetOperatorTokenInfo(InOpToken).Operator));
	};
	// Used to check if we encounter a '-' to see if we need to turn it into a negate operator
	// and also to see if we have two operators in a row (error).
	TOptional<FToken> LastToken;

	do
	{
		const TCHAR* TokenStart = ParseRange.GetData();
		TVariant<FToken, FParseError> ParseResult = ParseToken(ParseRange, InExpression);
		
		if (FParseError* Error = ParseResult.TryGet<FParseError>())
		{
			return TVariant<FExpressionObject, FParseError>(TInPlaceType<FParseError>(), *Error);
		}

		const FStringView TokenParseRange(TokenStart, ParseRange.GetData() - TokenStart);
		const FToken& Token = ParseResult.Get<FToken>();

		// Values / identifiers go onto the expression stack immediately.
		if (const FName* Identifier = Token.TryGet<FName>())
		{
			if (LastToken.IsSet() &&
				(LastToken->IsType<FName>() || LastToken->IsType<int32>()))
			{
				return ParseError(TEXT("Expected an operator"), TokenParseRange.GetData());
			}
			Expression.Push(FExpressionObject::OpElement(TInPlaceType<FName>(), *Identifier));
		}
		else if (const int32* Value = Token.TryGet<int32>())
		{
			if (LastToken.IsSet() &&
				(LastToken->IsType<FName>() || LastToken->IsType<int32>()))
			{
				return ParseError(TEXT("Expected an operator"), TokenParseRange.GetData());
			}
			Expression.Push(FExpressionObject::OpElement(TInPlaceType<int32>(), *Value));
		}
		else
		{
			EOperatorToken Op = Token.Get<EOperatorToken>();
			const FOperatorTokenInfo& TokenInfo = GetOperatorTokenInfo(Op);

			// Was the last token also an operator token?
			if (LastToken.IsSet())
			{
				if (const EOperatorToken* LastOp = LastToken->TryGet<EOperatorToken>())
				{
					const FOperatorTokenInfo& LastTokenInfo = GetOperatorTokenInfo(*LastOp);
					if (TokenInfo.Operator.IsSet() && LastTokenInfo.Operator.IsSet())
					{
						return ParseError(TEXT("Expected an expression"), TokenParseRange.GetData());
					}
					if (*LastOp == EOperatorToken::ParenClose && Op == EOperatorToken::ParenOpen)
					{
						return ParseError(TEXT("Expected an operator"), TokenParseRange.GetData());
					}
					if (*LastOp == EOperatorToken::ParenOpen && Op == EOperatorToken::ParenClose)
					{
						return ParseError(TEXT("Empty Parentheses"), TokenParseRange.GetData());
					}
				}
			}

			// Special-case for unary prefix.
			if (Op == EOperatorToken::Subtract &&
				(!LastToken.IsSet() || (LastToken->IsType<EOperatorToken>() && LastToken->Get<EOperatorToken>() != EOperatorToken::ParenClose)))
			{
				Op = EOperatorToken::Negate;
				OperatorStack.Push(Op);
			}
			else if (Op == EOperatorToken::ParenOpen)
			{
				OperatorStack.Push(Op);
			}
			else if (Op == EOperatorToken::ParenClose)
			{
				while(OperatorStack.IsEmpty() || OperatorStack.Last() != EOperatorToken::ParenOpen)
				{
					if (OperatorStack.IsEmpty())
					{
						return ParseError(TEXT("Mismatched parentheses"), TokenParseRange.GetData()); 
					}
					PushOperatorTokenToExpression(OperatorStack.Pop(false));
				}
				// Remove the now unneeded open parentheses.
				OperatorStack.Pop(false);
			}
			else if (GetOperatorTokenInfo(Op).Associativity == EAssociativity::Left)
			{
				// Peel off all operators that have the same precedence or higher.
				while (!OperatorStack.IsEmpty() && GetOperatorTokenInfo(OperatorStack.Last()).Precedence >= TokenInfo.Precedence)
				{
					PushOperatorTokenToExpression(OperatorStack.Pop(false));
				}
				OperatorStack.Push(Op);
			}
			else if (GetOperatorTokenInfo(Op).Associativity == EAssociativity::Right)
			{
				// Peel off all operators that have higher precedence.
				while (!OperatorStack.IsEmpty() && GetOperatorTokenInfo(OperatorStack.Last()).Precedence > TokenInfo.Precedence)
				{
					PushOperatorTokenToExpression(OperatorStack.Pop(false));
				}
				OperatorStack.Push(Op);
			}
		}

		LastToken = Token;
		ParseRange = ParseRange.TrimStart();
	} while(!ParseRange.IsEmpty());

	while (!OperatorStack.IsEmpty())
	{
		if (!GetOperatorTokenInfo(OperatorStack.Last()).Operator.IsSet())
		{
			return ParseError(TEXT("Mismatched parentheses"), InExpression.GetData()); 
		}
		PushOperatorTokenToExpression(OperatorStack.Pop(false));
	}

	FExpressionObject S;
	S.Expression = Expression;
	return TVariant<FExpressionObject, FParseError>(TInPlaceType<FExpressionObject>(), S);
}


int32 FEngine::Execute(
	const FExpressionObject& InExpressionObject
	) const
{
	// An empty expression object is valid.
	if (InExpressionObject.Expression.IsEmpty())
	{
		return 0;
	}

	// We assume the codons in the expression object are correct and will always result
	// in a single value remaining on the stack.
	TArray<int32, TInlineAllocator<32>> ValueStack;
	
	for (const FExpressionObject::OpElement& Token: InExpressionObject.Expression)
	{
		if (const FExpressionObject::EOperator* Operator = Token.TryGet<FExpressionObject::EOperator>())
		{
			switch(*Operator)
			{
			case FExpressionObject::EOperator::Negate:
				{
					ValueStack.Last() = -ValueStack.Last();
				}
				break;	
				
			case FExpressionObject::EOperator::Add:
				{
					const int32 V = ValueStack.Pop(false);
					ValueStack.Last() += V;
				}
				break;	
			case FExpressionObject::EOperator::Subtract:
				{
					const int32 V = ValueStack.Pop(false);
					ValueStack.Last() -= V;
				}
				break;	
			case FExpressionObject::EOperator::Multiply:
				{
					const int32 V = ValueStack.Pop(false);
					ValueStack.Last() *= V;
				}
				break;
			case FExpressionObject::EOperator::Divide:
				{
					const int32 V = ValueStack.Pop(false);
					if (V == 0)
					{
						ValueStack.Last() = 0;
					}
					else
					{
						ValueStack.Last() /= V;
					}
				}
				break;	
			case FExpressionObject::EOperator::Modulo:
				{
					const int32 V = ValueStack.Pop(false);
					if (V == 0)
					{
						ValueStack.Last() = 0;
					}
					else
					{
						ValueStack.Last() = ValueStack.Last() % V;
					}
				}
				break;	
			case FExpressionObject::EOperator::Power:
				{
					const int32 V = ValueStack.Pop(false);
					ValueStack.Last() = IntPow(ValueStack.Last(), V);
				}
				break;	
			}
		}
		else if (const FName* Constant = Token.TryGet<FName>())
		{
			const int32 *Value = Constants.Find(*Constant);
			ValueStack.Push(Value ? *Value : 0);
		}
		else if (const int32* Value = Token.TryGet<int32>())
		{
			ValueStack.Push(*Value);
		}
	}

	return ValueStack.Last();
}


TOptional<int32> FEngine::Evaluate(
	FStringView InExpression
	) const
{
	TVariant<FExpressionObject, FParseError> Result = Parse(InExpression);
	if (const FExpressionObject* ExpressionObject = Result.TryGet<FExpressionObject>())
	{
		return Execute(*ExpressionObject);
	}

	return {};
}


TOptional<FParseError> FEngine::Verify(
	FStringView InExpression
	) const
{
	TVariant<FExpressionObject, FParseError> Result = Parse(InExpression);
	if (const FParseError* Error = Result.TryGet<FParseError>())
	{
		return *Error;
	}
	return {};
}

}
