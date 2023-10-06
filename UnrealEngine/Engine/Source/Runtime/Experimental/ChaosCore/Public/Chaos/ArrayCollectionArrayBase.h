// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"

namespace Chaos
{
class TArrayCollectionArrayBase
{
  public:
	virtual void ApplyShrinkPolicy(const float MaxSlackFraction, const int32 MinSlack) = 0;
	virtual void Resize(const int Num) = 0;
	virtual void RemoveAt(const int Idx, const int Count) = 0;
	virtual void RemoveAtSwap(const int Idx) = 0;
	virtual uint64 SizeOfElem() const = 0;
	virtual void MoveToOtherArray(const int Idx, TArrayCollectionArrayBase& Other) = 0;
};
}
