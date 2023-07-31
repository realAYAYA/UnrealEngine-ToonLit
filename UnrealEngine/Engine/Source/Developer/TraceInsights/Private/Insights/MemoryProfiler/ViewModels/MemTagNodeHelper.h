// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"

// Insights
#include "Insights/MemoryProfiler/ViewModels/MemTagNode.h"
#include "Insights/MemoryProfiler/ViewModels/MemTagNodeGroupingAndSorting.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

/** Helper struct that contains helper functions and classes for EMemTagNodeType enum. */
struct MemTagNodeTypeHelper
{
	/**
	 * @param Type - The value to get the text for.
	 *
	 * @return text representation of the specified EMemTagNodeType value.
	 */
	static FText ToText(const EMemTagNodeType Type);

	/**
	 * @param Type - The value to get the text for.
	 *
	 * @return text representation with more detailed explanation of the specified EMemTagNodeType value.
	 */
	static FText ToDescription(const EMemTagNodeType Type);

	static const FSlateBrush* GetIconForGroup();
	static const FSlateBrush* GetIconForMemTagNodeType(const EMemTagNodeType Type);
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/** Helper struct that contains grouping static functions and classes. */
struct MemTagNodeGroupingHelper
{
	/**
	 * @param EMemTagNodeGroupingMode - The value to get the text for.
	 *
	 * @return text representation of the specified EMemTagNodeGroupingMode value.
	 */
	static FText ToText(const EMemTagNodeGroupingMode EMemTagNodeGroupingMode);

	/**
	 * @param EMemTagNodeGroupingMode - The value to get the text for.
	 *
	 * @return text representation with more detailed explanation of the specified EMemTagNodeGroupingMode value.
	 */
	static FText ToDescription(const EMemTagNodeGroupingMode EMemTagNodeGroupingMode);
};

////////////////////////////////////////////////////////////////////////////////////////////////////
