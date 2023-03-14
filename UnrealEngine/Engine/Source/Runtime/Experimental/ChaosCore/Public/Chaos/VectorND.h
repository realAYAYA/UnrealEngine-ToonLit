// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"

namespace Chaos
{
template<class T>
class VectorND : public TArray<T>
{
  public:
	using TArray<T>::SetNum;

	VectorND(const int32 Size)
	{
		SetNum(Size);
	}
};
}
