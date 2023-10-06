// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Chaos/Real.h"
#include "Chaos/Array.h"
namespace GeometryCollectionTest
{
	class EllipsoidGeometry2
	{

	public:
		EllipsoidGeometry2() {}
		~EllipsoidGeometry2() {}

		static const TArray<float>	RawVertexArray;
		static const TArray<int32>	RawIndicesArray;
	};
}