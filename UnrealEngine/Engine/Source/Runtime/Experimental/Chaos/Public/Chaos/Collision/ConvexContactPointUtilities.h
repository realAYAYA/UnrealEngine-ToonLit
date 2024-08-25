// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"
#include "Chaos/Capsule.h"
#include "Chaos/Collision/ContactPoint.h"
#include "Chaos/Triangle.h"

namespace Chaos:: Private
{
	// Project a convex onto an axis and return the projected range as well as the vertex indices that bound the range
	template <typename ConvexType>
	inline void ProjectOntoAxis(const ConvexType& Convex, const FVec3& AxisN, const FVec3& AxisX, FReal& PMin, FReal& PMax, int32& MinVertexIndex, int32& MaxVertexIndex, TArrayView<FReal>* VertexDs)
	{
		PMin = std::numeric_limits<FReal>::max();
		PMax = std::numeric_limits<FReal>::lowest();
		for (int32 VertexIndex = 0; VertexIndex < Convex.NumVertices(); ++VertexIndex)
		{
			const FVec3 V = Convex.GetVertex(VertexIndex);
			const FReal D = FVec3::DotProduct(V - AxisX, AxisN);
			if (D < PMin)
			{
				PMin = D;
				MinVertexIndex = VertexIndex;
			}
			if (D > PMax)
			{
				PMax = D;
				MaxVertexIndex = VertexIndex;
			}
			if (VertexDs != nullptr)
			{
				(*VertexDs)[VertexIndex] = D;
			}
		}
	}

	// Project a triangle onto an axis and return the projected range as well as the vertex indices that bound the range
	inline void ProjectOntoAxis(const FTriangle& Triangle, const FVec3& AxisN, const FVec3& AxisX, FReal& PMin, FReal& PMax, int32& MinVertexIndex, int32& MaxVertexIndex)
	{
		const FVec3& V0 = Triangle.GetVertex(0);
		const FVec3& V1 = Triangle.GetVertex(1);
		const FVec3& V2 = Triangle.GetVertex(2);
		const FReal Ds[3] =
		{
			FVec3::DotProduct(V0 - AxisX, AxisN),
			FVec3::DotProduct(V1 - AxisX, AxisN),
			FVec3::DotProduct(V2 - AxisX, AxisN)
		};
		MinVertexIndex = FMath::Min3Index(Ds[0], Ds[1], Ds[2]);
		MaxVertexIndex = FMath::Max3Index(Ds[0], Ds[1], Ds[2]);
		PMin = Ds[MinVertexIndex];
		PMax = Ds[MaxVertexIndex];
	}

	// Project a capsule segment onto an axis and return the projected range as well as the vertex indices that bound the range
	inline void ProjectOntoAxis(const FCapsule& Capsule, const FVec3& AxisN, const FVec3& AxisX, FReal& PMin, FReal& PMax, int32& MinVertexIndex, int32& MaxVertexIndex)
	{
		const FVec3 V0 = Capsule.GetX1();
		const FVec3 V1 = Capsule.GetX2();
		const FReal D0 = FVec3::DotProduct(V0 - AxisX, AxisN);
		const FReal D1 = FVec3::DotProduct(V1 - AxisX, AxisN);
		if (D0 < D1)
		{
			MinVertexIndex = 0;
			MaxVertexIndex = 1;
			PMin = D0;
			PMax = D1;
		}
		else
		{
			MinVertexIndex = 1;
			MaxVertexIndex = 0;
			PMin = D1;
			PMax = D0;
		}
	}
}

namespace Chaos
{
	template <typename ConvexType>
	UE_DEPRECATED(5.4, "Not part of public API")
	inline void ProjectOntoAxis(const ConvexType& Convex, const FVec3& AxisN, const FVec3& AxisX, FReal& PMin, FReal& PMax, int32& MinVertexIndex, int32& MaxVertexIndex, TArrayView<FReal>* VertexDs)
	{
		return Private::ProjectOntoAxis(Convex, AxisN, AxisX, PMin, PMax, MinVertexIndex, MaxVertexIndex, VertexDs);
	}

	UE_DEPRECATED(5.4, "Not part of public API")
	inline void ProjectOntoAxis(const FTriangle& Triangle, const FVec3& AxisN, const FVec3& AxisX, FReal& PMin, FReal& PMax, int32& MinVertexIndex, int32& MaxVertexIndex)
	{
		return Private::ProjectOntoAxis(Triangle, AxisN, AxisX, PMin, PMax, MinVertexIndex, MaxVertexIndex);
	}

	UE_DEPRECATED(5.4, "Not part of public API")
	inline void ProjectOntoAxis(const FCapsule& Capsule, const FVec3& AxisN, const FVec3& AxisX, FReal& PMin, FReal& PMax, int32& MinVertexIndex, int32& MaxVertexIndex)
	{
		return Private::ProjectOntoAxis(Capsule, AxisN, AxisX, PMin, PMax, MinVertexIndex, MaxVertexIndex);
	}
}
