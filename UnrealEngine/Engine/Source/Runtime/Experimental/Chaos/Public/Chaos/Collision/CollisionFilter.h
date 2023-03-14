// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Chaos/Core.h"
#include "Chaos/CollisionFilterData.h"
#include "Chaos/GeometryParticles.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/Particles.h"

namespace Chaos
{
	typedef uint8 FMaskFilter;
	enum { NumExtraFilterBits = 6 };
	enum { NumCollisionChannelBits = 5 };

	inline bool IsFilterValid(const FCollisionFilterData& Filter)
	{
		return Filter.Word0 || Filter.Word1 || Filter.Word2 || Filter.Word3;
	}

	//inline uint32 GetChaosCollisionChannel(uint32 Word3)
	//{
	//	uint32 ChannelMask = (Word3 << NumExtraFilterBits) >> (32 - NumCollisionChannelBits);
	//	return (uint32)ChannelMask;
	//}

	//inline uint32 GetChaosCollisionChannelAndExtraFilter(uint32 Word3, FMaskFilter& OutMaskFilter)
	//{
	//	uint32 ChannelMask = GetChaosCollisionChannel(Word3);
	//	OutMaskFilter = Word3 >> (32 - NumExtraFilterBits);
	//	return (uint32)ChannelMask;
	//}

	inline bool FilterHasSimEnabled(const FPerShapeData* Shape)
	{
		return (!Shape || (Shape->GetSimEnabled() && IsFilterValid(Shape->GetSimData())));
	}

	inline bool DoCollide(EImplicitObjectType Implicit0Type, const FPerShapeData* Shape0, EImplicitObjectType Implicit1Type, const FPerShapeData* Shape1)
	{
		//
		// Disabled shapes do not collide
		//
		if (!FilterHasSimEnabled(Shape0)) return false;
		if (!FilterHasSimEnabled(Shape1)) return false;

		//
		// Triangle Mesh geometry is only used if the shape specifies UseComplexAsSimple
		//
		if (Shape0)
		{
			if (Implicit0Type == ImplicitObjectType::TriangleMesh && Shape0->GetCollisionTraceType() != Chaos_CTF_UseComplexAsSimple)
			{
				return false;
			}
			else if (Shape0->GetCollisionTraceType() == Chaos_CTF_UseComplexAsSimple && Implicit0Type != ImplicitObjectType::TriangleMesh)
			{
				return false;
			}
		}
		else if (Implicit0Type == ImplicitObjectType::TriangleMesh)
		{
			return false;
		}

		if (Shape1)
		{
			if (Implicit1Type == ImplicitObjectType::TriangleMesh && Shape1->GetCollisionTraceType() != Chaos_CTF_UseComplexAsSimple)
			{
				return false;
			}
			else if (Shape1->GetCollisionTraceType() == Chaos_CTF_UseComplexAsSimple && Implicit1Type != ImplicitObjectType::TriangleMesh)
			{
				return false;
			}
		}
		else if (Implicit1Type == ImplicitObjectType::TriangleMesh)
		{
			return false;
		}

		//
		// Shape Filtering
		//
		if (Shape0 && Shape1)
		{

			if (IsFilterValid(Shape0->GetSimData()) && IsFilterValid(Shape1->GetSimData()))
			{
				FMaskFilter Filter0Mask, Filter1Mask;
				const uint32 Filter0Channel = GetChaosCollisionChannelAndExtraFilter(Shape0->GetSimData().Word3, Filter0Mask);
				const uint32 Filter1Channel = GetChaosCollisionChannelAndExtraFilter(Shape1->GetSimData().Word3, Filter1Mask);

				const uint32 Filter1Bit = 1 << (Filter1Channel); // SIMDATA_TO_BITFIELD
				uint32 const Filter0Bit = 1 << (Filter0Channel); // SIMDATA_TO_BITFIELD
				return (Filter0Bit & Shape1->GetSimData().Word1) && (Filter1Bit & Shape0->GetSimData().Word1);
			}
		}


		return true;
	}
}