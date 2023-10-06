// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Core.h"

namespace Chaos
{
	namespace ImplicitObjectType
	{
		enum
		{
			//Note: add entries in order to avoid serialization issues (but before IsInstanced)
			Sphere = 0, // warning: code assumes that this is an FSphere, but all TSpheres will think this is their type.
			Box,
			Plane,
			Capsule,
			Transformed,
			Union,
			LevelSet,
			Unknown,
			Convex,
			TaperedCylinder,
			Cylinder,
			TriangleMesh,
			HeightField,
			DEPRECATED_Scaled,	//needed for serialization of existing data
			Triangle,
			UnionClustered,
			TaperedCapsule,
			WeightedLatticeBone,

			//Add entries above this line for serialization
			ConcreteObjectCount, // Used to ensure bitflags do not overlap concrete type
			IsWeightedLattice = 1 << 5,
			IsInstanced = 1 << 6,
			IsScaled = 1 << 7
		};
		static_assert(ConcreteObjectCount <= IsWeightedLattice, "Too many Chaos::ImplicitObjectType concrete types.");
	}

	using EImplicitObjectType = uint8;	//see ImplicitObjectType

	FORCEINLINE bool IsInstanced(EImplicitObjectType Type)
	{
		return (Type & ImplicitObjectType::IsInstanced) != 0;
	}

	FORCEINLINE bool IsScaled(EImplicitObjectType Type)
	{
		return (Type & ImplicitObjectType::IsScaled) != 0;
	}

	FORCEINLINE bool IsWeightedLattice(EImplicitObjectType Type)
	{
		return (Type & ImplicitObjectType::IsWeightedLattice) != 0;
	}

	FORCEINLINE EImplicitObjectType GetInnerType(EImplicitObjectType Type)
	{
		return Type & (~(ImplicitObjectType::IsWeightedLattice | ImplicitObjectType::IsScaled | ImplicitObjectType::IsInstanced));
	}

	namespace EImplicitObject
	{
		enum Flags
		{
			IsConvex = 1,
			HasBoundingBox = 1 << 1,
			DisableCollisions = 1 << 2
		};

		inline const int32 FiniteConvex = IsConvex | HasBoundingBox;
	}
}