// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ChaosArchive.h"
#include "UObject/ExternalPhysicsCustomObjectVersion.h"

// Supported flags for Chaos filter, shares some flags with EPhysXFilterDataFlags
// Shared flags should be kept in sync until unified. @see EPhysXFilterDataFlags
// #TODO unify filter builder with this flag list

namespace Chaos
{
	enum class EFilterFlags : uint8
	{
		SimpleCollision			= 0b00000001,	// The shape is used for simple collision
		ComplexCollision		= 0b00000010,	// The shape is used for complex (trimesh) collision
		CCD						= 0b00000100,	// Unused - present for compatibility. CCD handled per-particle in Chaos
		ContactNotify			= 0b00001000,	// Whether collisions with this shape should be reported back to the game thread
		StaticShape				= 0b00010000,	// Unused - present for compatibility
		ModifyContacts			= 0b00100000,	// Unused - present for compatibility, whether to allow contact modification, handled in Chaos callbacks now
		KinematicKinematicPairs	= 0b01000000,	// Unused - present for compatibility, whether to generate KK pairs, Chaos never generates KK pairs
	};
}

struct CHAOS_API FCollisionFilterData
{
	uint32 Word0;
	uint32 Word1;
	uint32 Word2;
	uint32 Word3;

	FORCEINLINE FCollisionFilterData()
	{
		Word0 = Word1 = Word2 = Word3 = 0;
	}

	bool HasFlag(Chaos::EFilterFlags InFlag) const
	{
		const uint32 FilterFlags = (Word3 & 0xFFFFFF);
		return FilterFlags & static_cast<uint32>(InFlag);
	}
};

inline Chaos::FChaosArchive& operator<<(Chaos::FChaosArchive& Ar, FCollisionFilterData& Filter)
{
	Ar << Filter.Word0 << Filter.Word1 << Filter.Word2 << Filter.Word3;
	return Ar;
}

inline bool operator!=(const FCollisionFilterData& A, const FCollisionFilterData& B)
{
	return A.Word0!=B.Word0 || A.Word1!=B.Word1 || A.Word2!=B.Word2 || A.Word3!=B.Word3;
}