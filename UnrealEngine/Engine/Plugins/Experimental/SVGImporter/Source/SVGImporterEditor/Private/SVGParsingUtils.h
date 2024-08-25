// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"

class FSVGParser_Base;
struct FSVGPathCommand;
struct FSVGRawAttribute;

class FSVGParsingUtils
{
public:

	/** List of available SVG Parsers */
	enum class ESVGParserType : uint8
	{
		SVGFastXml,
		SVGPugiXml
	};

	/**
	 * Create SVGParser to parse svg strings
	 * @param InStringToParse the SVG String to parse
	 * @param InParserType the type of parser to use
	 * @return the SVG Parser
	 */
	static TSharedRef<FSVGParser_Base> CreateSVGParser(const FString& InStringToParse, ESVGParserType InParserType = ESVGParserType::SVGPugiXml);

	/** Does the specified string correspond to a valid SVG? */
	static bool IsValidSVGString(const FString& InStringToParse);
	
	/** Parse the provided SVG String as a float */
	static float FloatFromString(const FString& InFloatString);

	/** Parse the provided SVG String as a list of float values */
	static TArray<float> FloatsFromString(FString InFloatsString, bool bApplyScaling = true);
	
	/**
	 * Parse the provided SVG String as a float, removing the specified suffix.
	 * Useful for numbers with a suffix (e.g. percentage for some gradients)
	 */
	static bool FloatFromStringWithSuffix(const FString& InFloatString, const FString& InSuffix, float& OutValue);
	
	/** Parse the provided SVG String as a list of 2D Points */
	static void PointsFromString(FString InPointsString, TArray<FVector2D>& OutPoints);

	/** Parses the SVG String as a list of SVG Commands, keeping the inner delimiters for each Command */
	static int32 ParseIntoCommandsArrayKeepDelimiters(const FString& InSourceString, TArray<FString>& OutArray, const TCHAR* const * DelimArray, int32 NumDelims, bool InCullEmpty);

	/**
	 * Adds a Move To instruction to the provided Commands list.
	 * @param OutCommands required command will be added to this list
	 * @param InOutCursorPos cursor position to be used and updated while parsing the arguments
	 * @param InArguments instruction arguments
	 * @param bInIsRelative instruction can be relative or absolute
	 * @return the initial position of the sub path starting with this MoveTo operation. Should be used to later perform a ClosePath if needed.
	 */
	static FVector2D PathMoveTo(TArray<FSVGPathCommand>& OutCommands, FVector2D& InOutCursorPos, const TArray<FVector2D>& InArguments, bool bInIsRelative);

	/**
	 * Adds a Line To instruction to the provided Commands list.
	 * @param OutCommands required command will be added to this list
	 * @param InOutCursorPos cursor position to be used and updated while parsing the arguments
	 * @param InArguments instruction arguments
	 * @param bInIsRelative instruction can be relative or absolute
	 */
	static void PathLineTo(TArray<FSVGPathCommand>& OutCommands, FVector2D& InOutCursorPos, const TArray<FVector2D>& InArguments, bool bInIsRelative);

	/**
	 * Adds a Horizontal Line To instruction to the provided Commands list.
	 * @param OutCommands required command will be added to this list
	 * @param InOutCursorPos cursor position to be used and updated while parsing the arguments
	 * @param InArguments instruction arguments
	 * @param bInIsRelative instruction can be relative or absolute
	 */
	static void PathHorizontalLineTo(TArray<FSVGPathCommand>& OutCommands, FVector2D& InOutCursorPos, const TArray<float>& InArguments, bool bInIsRelative);

	/**
	 * Adds a Vertical Line To instruction to the provided Commands list.
	 * @param OutCommands required command will be added to this list
	 * @param InOutCursorPos cursor position to be used and updated while parsing the arguments
	 * @param InArguments instruction arguments
	 * @param bInIsRelative instruction can be relative or absolute
	 */
	static void PathVerticalLineTo(TArray<FSVGPathCommand>& OutCommands, FVector2D& InOutCursorPos, const TArray<float>& InArguments, bool bInIsRelative);

	/**
	 * Adds a Cubic Bezier To instruction to the provided Commands list.
	 * @param OutCommands required command will be added to this list
	 * @param InOutCursorPos first cursor position to be used and updated while parsing the arguments
	 * @param InOutCursorPos2 second cursor position to be used and updated while parsing the arguments
	 * @param InArguments instruction arguments
	 * @param bInIsRelative instruction can be relative or absolute
	 */
	static void PathCubicBezierTo(TArray<FSVGPathCommand>& OutCommands, FVector2D& InOutCursorPos, FVector2D& InOutCursorPos2, const TArray<FVector2D>& InArguments, bool bInIsRelative);

	/**
	 * Adds a Smooth Cubic Bezier To instruction to the provided Commands list.
	 * @param OutCommands required command will be added to this list
	 * @param InOutCursorPos first cursor position to be used and updated while parsing the arguments
	 * @param InOutCursorPos2 second cursor position to be used and updated while parsing the arguments
	 * @param InArguments instruction arguments
	 * @param bInIsRelative instruction can be relative or absolute
	 */
	static void PathCubicBezierSmoothTo(TArray<FSVGPathCommand>& OutCommands, FVector2D& InOutCursorPos, FVector2D& InOutCursorPos2, const TArray<FVector2D>& InArguments, bool bInIsRelative);

	/**
	 * Adds a Quadratic Bezier To instruction to the provided Commands list.
	 * @param OutCommands required command will be added to this list
	 * @param InOutCursorPos first cursor position to be used and updated while parsing the arguments
	 * @param InOutCursorPos2 second cursor position to be used and updated while parsing the arguments
	 * @param InArguments instruction arguments
	 * @param bInIsRelative instruction can be relative or absolute
	 */
	static void PathQuadraticBezierTo(TArray<FSVGPathCommand>& OutCommands, FVector2D& InOutCursorPos, FVector2D& InOutCursorPos2, const TArray<FVector2D>& InArguments, bool bInIsRelative);

	/**
	 * Adds a Smooth Quadratic Bezier To instruction to the provided Commands list.
	 * @param OutCommands required command will be added to this list
	 * @param InOutCursorPos first cursor position to be used and updated while parsing the arguments
	 * @param InOutCursorPos2 second cursor position to be used and updated while parsing the arguments
	 * @param InArguments instruction arguments
	 * @param bInIsRelative instruction can be relative or absolute
	 */
	static void PathQuadraticBezierSmoothTo(TArray<FSVGPathCommand>& OutCommands, FVector2D& InOutCursorPos, FVector2D& InOutCursorPos2, const TArray<FVector2D>& InArguments, bool bInIsRelative);

	/**
	 * Adds a Path Arc To instruction to the provided Commands list.
	 * @param OutCommands required command will be added to this list
	 * @param InOutCursorPos cursor position to be used and updated while parsing the arguments
	 * @param InArguments instruction arguments
	 * @param bInIsRelative instruction can be relative or absolute
	 */
	static void PathArcTo(TArray<FSVGPathCommand>& OutCommands, FVector2D& InOutCursorPos, const TArray<float>& InArguments, bool bInIsRelative);

	/** Parses the specified SVG string as a list of path commands */
	static TArray<TArray<FSVGPathCommand>> ParseStringAsPathCommands(const FString& InString);

	static bool IsURLString(const FString& InString);

	static FString ExtractURL(const FString& InString);

	static bool IsVisible(const TSharedRef<FSVGRawAttribute>& InAttribute);
};
