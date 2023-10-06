// Copyright Epic Games, Inc. All Rights Reserved.

#include "FastUpdate/SlateInvalidationWidgetIndex.h"

#include <limits>

const FSlateInvalidationWidgetIndex FSlateInvalidationWidgetIndex::Invalid = { std::numeric_limits<FSlateInvalidationWidgetIndex::IndexType>::max(), std::numeric_limits<FSlateInvalidationWidgetIndex::IndexType>::max() };

