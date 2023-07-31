// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

struct FMovieSceneCompiledDataID
{
	FMovieSceneCompiledDataID()
		: Value(INDEX_NONE)
	{}

	explicit FMovieSceneCompiledDataID(int32 SuppliedIndex)
		: Value(SuppliedIndex)
	{}

	bool IsValid() const
	{
		return Value != INDEX_NONE;
	}

	int32 Value;
};