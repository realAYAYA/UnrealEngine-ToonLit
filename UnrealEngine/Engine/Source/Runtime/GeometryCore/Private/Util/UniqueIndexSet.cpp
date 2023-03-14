// Copyright Epic Games, Inc. All Rights Reserved.

#include "Util/UniqueIndexSet.h"

using namespace UE::Geometry;

FUniqueIndexSet::~FUniqueIndexSet()
{
	if (Bits != nullptr)
	{
		delete[] Bits;
	}
}
