// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Math/BasicMathExpressionEvaluator.h"
#include "Misc/ExpressionParserTypes.h"
#include "Misc/Optional.h"
#include "Templates/ValueOrError.h"

struct FFrameRate;
struct FFrameTime;

DEFINE_EXPRESSION_OPERATOR_NODE(TIMEMANAGEMENT_API, FHour, 0x702443E8, 0xEF9A48A0, 0x8DC56394, 0x48F2632A);
DEFINE_EXPRESSION_OPERATOR_NODE(TIMEMANAGEMENT_API, FMinute, 0x4FED0D41, 0x298D481C, 0xAC022899, 0xF196E817);
DEFINE_EXPRESSION_OPERATOR_NODE(TIMEMANAGEMENT_API, FSecond, 0x43365BB1, 0x9A6B457E, 0xA6C71BB3, 0xCE6D5ACD);
DEFINE_EXPRESSION_OPERATOR_NODE(TIMEMANAGEMENT_API, FMillisecond, 0xBFA9E4FB, 0x45799275, 0x0EFA76E3, 0x76E3533F);
DEFINE_EXPRESSION_OPERATOR_NODE(TIMEMANAGEMENT_API, FFrames, 0xA0D341C4, 0xA51D40D3, 0xA9DBDD45, 0x83E1A21A);
DEFINE_EXPRESSION_OPERATOR_NODE(TIMEMANAGEMENT_API, FTimecodeDelimiter, 0x6E947008, 0x523D4A17, 0xB5F7BCDA, 0xA43B62FC);
DEFINE_EXPRESSION_OPERATOR_NODE(TIMEMANAGEMENT_API, FDropcodeDelimiter, 0x80AF2C2C, 0x544A451F, 0x822E5B24, 0xAFC78EEF);
DEFINE_EXPRESSION_OPERATOR_NODE(TIMEMANAGEMENT_API, FBracketStart, 0xA7358BD1, 0xB4EF466D, 0xA336CD84, 0x14C93D2E);
DEFINE_EXPRESSION_OPERATOR_NODE(TIMEMANAGEMENT_API, FBracketEnd, 0xE49500E9, 0x03E64440, 0x87802630, 0xF1C1DDDF);

struct FFrameNumberTimeEvaluator
{
	/** Constructor that sets up the parser's lexer and compiler */
	TIMEMANAGEMENT_API FFrameNumberTimeEvaluator();

	/** Evaluate the given expression, resulting in either a double value, or an error */
	TIMEMANAGEMENT_API TValueOrError<FFrameTime, FExpressionError> EvaluateTimecode(const TCHAR* InExpression, const FFrameRate& InDisplayFrameRate, const FFrameRate& InTickResolution, bool& OutDirectlyParsed) const;
	TIMEMANAGEMENT_API TValueOrError<FFrameTime, FExpressionError> EvaluateFrame(const TCHAR* InExpression, const FFrameRate& InDisplayFrameRate, const FFrameRate& InTickResolution, bool& OutDirectlyParsed) const;
	TIMEMANAGEMENT_API TValueOrError<FFrameTime, FExpressionError> EvaluateTime(const TCHAR* InExpression, FFrameRate InFrameRate, bool& OutDirectlyParsed) const;

	FTokenDefinitions TimecodeTokenDefinitions;
	FTokenDefinitions FrameTokenDefinitions;
	FTokenDefinitions TimeTokenDefinitions;

	FExpressionGrammar FrameGrammar;
	FExpressionGrammar TimecodeGrammar;
	FExpressionGrammar TimeGrammar;

	FOperatorJumpTable TimeJumpTable;
};
