// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE::MovieScene
{

struct FEntityRange;
struct FEntityAllocationWriteContext;

struct IMutualComponentInitializer
{
	virtual ~IMutualComponentInitializer(){}
	virtual void Run(const FEntityRange& Range, const FEntityAllocationWriteContext& WriteContext) = 0;
};

} // namespace UE::MovieScene