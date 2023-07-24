// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Misc/Optional.h"
#include "Misc/TVariant.h"
#include "UObject/NameTypes.h"

namespace Optimus::Expression
{

struct FParseError
{
	FParseError() = default;
	FParseError(const int32 InColumn, FString&& InMessage) : Column(InColumn), Message(MoveTemp(InMessage)) {}

	/** The column at which the error occurs. Zero-based. */
	const int32 Column = 0;

	/** The human-readable parsing error */
	const FString Message = FString();
};

struct FExpressionObject
{
private:
	enum class EOperator
	{
		Negate,				// Negation operator.
		Add,				// Add last two values on stack and add the result to the stack.
		Subtract,			// Same but subtract 
		Multiply,			// Same but multiply
		Divide,				// Same but divide (div-by-zero returns zero)
		Modulo,				// Same but modulo (mod-by-zero returns zero)
		Power,				// Raise to power
		// FloorDivide,    	// Divide and round the result down
	};
	
	friend class FEngine;
	using OpElement = TVariant<EOperator, /* Constant Ref */ FName, /* Value */ int32>;
	TArray<OpElement, TInlineAllocator<8>> Expression;
};

/** Simple, fast expression evaluation engine. Can evaluate simple arithmetic expressions,
 *  including user-settable constants. 
 *		1 + 1
 *		foo + (3 * bar)
 *
 *  Allowable operands are taken from Python and are currently as follow:
 *      -x				Unary negation
 *      x + y			Addition
 *      x - y			Subtraction
 *      x * y			Multiplication
 *      x / y			Division (div-by-zero returns zero)
 *      x ** y			Raise x to y's power
 *
 *  Terms can also be grouped by parentheses '(' and ')'.
 *  
 *	Constants that begin with a number, include spaces or any of the above operands/parentheses 
 *	are allowed, but must be then enclosed in single quotes when referenced.
 *	E.g:
 *	   'foo(1)' * 2
 *	rather than:
 *	   foo (1) * 2
 */
enum class EParseFlags
{
	Default				= 0,		/** Default parsing behavior */
	ValidateConstants	= 1 << 0,	/** Validate that the referred-to constant exists */
};
ENUM_CLASS_FLAGS(EParseFlags)


class OPTIMUSCORE_API FEngine
{
public:
	explicit FEngine(EParseFlags InParseFlags = EParseFlags::Default) :
		ParseFlags(InParseFlags)
	{ }

	/** Initialize the engine with constants, making a copy of the value constant map. */
	explicit FEngine(
		const TMap<FName, int32>& InConstants,
		EParseFlags InParseFlags = EParseFlags::Default
		);
	
	/** Initialize the engine with constants, taking ownership of the value constant map. */
	explicit FEngine(
		TMap<FName, int32>&& InConstants,
		EParseFlags InParseFlags = EParseFlags::Default
		);

	/** Update existing constant values. No new constants are added. */
	void UpdateConstantValues(
		const TMap<FName, int32>& InConstants
		);

	/** Return existing constants and their values. */
	const TMap<FName, int32>& GetConstantValues() const
	{
		return Constants;
	}

	/** Parse an expression and either receive an error or a expression execution object that
	 *  matches this evaluator. */
	TVariant<FExpressionObject, FParseError> Parse(
		FStringView InExpression
		) const;

	/** Executes the given expression object */
	int32 Execute(
		const FExpressionObject& InExpressionObject
		) const;
	
	/** Evaluate an expression directly and returns the result. If the expression is incorrectly
	 *  formed, then the optional return value holds nothing.
	 */ 
	TOptional<int32> Evaluate(
		FStringView InExpression
		) const;

	/** Verify an expression. Does not check for variable validity, only that the expression
	 *  can be evaluated */
	TOptional<FParseError> Verify(
		FStringView InExpression
		) const;
	
private:
	enum class EOperatorToken
	{
		Negate,				// Placeholder for when constructing the op stack. Gets converted to (0 - V)
		Add,				// '+'
		Subtract,			// '-'
		Multiply,			// '*'
		Divide,				// '/'
		Modulo,				// '%'
		Power,				// '**'
		// FloorDivide,    	// '//'
		ParenOpen,			// '('
		ParenClose,			// ')'
	};
	using FToken = TVariant<EOperatorToken, /* Identifier */ FName, /* Value */ int32>;
	
	/** Parse a integer value and return the result (or error). Same semantics
	 *  apply as with ParseToken.
	 */
	static TVariant<FToken, FParseError> ParseInt32(
		FStringView& InOutParseRange,
		FStringView InExpression
		);

	/** Parse a constant reference and return the result (or error). Same semantics
	 *  apply as with ParseToken.
	 */
	TVariant<FToken, FParseError> ParseIdentifier(
		FStringView& InOutParseRange,
		FStringView InExpression
		) const;
	
	/** Parse a single token from the beginning of the range, returning either the token
	 *  or an error, if one was encountered.
	 *  If the token was successfully parsed, then the given string view range gets updated
	 *  with the new starting point at the end of the token just parsed
	 */
	TVariant<FToken, FParseError> ParseToken(
		FStringView& InOutParseRange,
		FStringView InExpression
		) const;

	/** Helper function to construct a token parse error result */
	static TVariant<FToken, FParseError> ParseTokenError(
		FString&& InError,
		const TCHAR* InCurrentPos,
		FStringView InExpression
		);
	
	/** Helper function to get a text version of a token. */
	static FString TokenToString(
		const FToken& InToken
		);
	
	/** The list of constants, stored using the FName key since the expression object refers
	 *  to it directly */
	// FUTURE: Add support for variables by allowing TFunction{Ref} objects along with direct
	// constants.
	TMap<FName, int32> Constants;

	/** The parse flags for this engine. Only set once */
	EParseFlags ParseFlags = EParseFlags::Default;
};

}
