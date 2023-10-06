// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LongJump.h"

namespace AutoRTFM
{

class FContext;

class FCallNest final
{
public:
	FCallNest(FContext* Context)
		: Context(Context)
	{}

	FCallNest* Parent{ nullptr };
	FContext* Context;
	FLongJump AbortJump;

	// Whether this succeeded or not is reflected in Context::GetStatus().
	template<typename TTryFunctor>
	void Try(const TTryFunctor& TryFunctor);
};


} // namespace AutoRTFM
