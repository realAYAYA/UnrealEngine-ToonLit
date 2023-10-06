// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/NumericLimits.h"

namespace TypedElementDataStorage
{
	using TableHandle = uint64;
	static constexpr TableHandle InvalidTableHandle = TNumericLimits<TableHandle>::Max();

	using RowHandle = uint64;
	static constexpr RowHandle InvalidRowHandle = TNumericLimits<RowHandle>::Max();

	using QueryHandle = uint64;
	static constexpr QueryHandle InvalidQueryHandle = TNumericLimits<QueryHandle>::Max();
} // namespace TypedElementDataStorage
