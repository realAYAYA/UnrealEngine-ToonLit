// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Fonts/ShapedTextFwd.h"

enum class ETextOverflowDirection : uint8
{
	// No overflow
	NoOverflow,
	// Left justification overflow
	LeftToRight,
	// Right justification overflow
	RightToLeft
};

struct FTextOverflowArgs
{
	FTextOverflowArgs(FShapedGlyphSequencePtr& InOverflowText, ETextOverflowDirection InOverflowDirection)
		: OverflowTextPtr(InOverflowText)
		, OverflowDirection(InOverflowDirection)
		, bForceEllipsisDueToClippedLine(false)
		
	{}

	FTextOverflowArgs()
		: OverflowDirection(ETextOverflowDirection::NoOverflow)
		, bForceEllipsisDueToClippedLine(false)
	{}

	/** Sequence that represents the ellipsis glyph */
	FShapedGlyphSequencePtr OverflowTextPtr;
	ETextOverflowDirection OverflowDirection;
	bool bForceEllipsisDueToClippedLine;
	
};
