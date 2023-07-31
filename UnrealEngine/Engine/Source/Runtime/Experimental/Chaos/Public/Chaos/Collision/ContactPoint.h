// Copyright Epic Games, Inc.All Rights Reserved.
#pragma once

#include "Chaos/Core.h"

namespace Chaos
{
	/**
	 * @brief Used in FContactPoint to indicate whether the contact is vertex-plane, edge-edge, etc
	 * 
	 * @note the order here is the order of preference in the solver. I.e., we like to solve Plane contacts before edge contacts before vertex contacts.
	 * This is most impotant for collisions against triangle meshes (or any concave shape) where the second shape is always the triangle, and so a PlaneVertex collision 
	 * counts as a vertex collision.
	*/
	enum class EContactPointType : int8
	{
		Unknown,
		VertexPlane,
		EdgeEdge,
		PlaneVertex,
		VertexVertex,
	};


	/**
	 * The value used for Phi (contact separation) to indicate that it is unset/invalid.
	 */
	template<typename T>
	constexpr T InvalidPhi() 
	{ 
		return TNumericLimits<T>::Max();
	}

	/**
	 * Data returned by the low-level collision functions.
	 * 
	 * @note All data is invalid/uninitialized when IsSet() is false.
	 * @see FContactPoint, FContactPointf
	*/
	template<typename T>
	class CHAOS_API TContactPoint
	{
	public:
		using FRealType = T;

		// Shape-space contact points on the two bodies
		TVec3<FRealType> ShapeContactPoints[2];

		// Shape-space contact normal on the second shape with direction that points away from shape 1
		TVec3<FRealType> ShapeContactNormal;

		// Contact separation (negative for overlap)
		FRealType Phi;

		// Face index of the shape we hit. Only valid for Heightfield and Trimesh contact points, otherwise INDEX_NONE
		int32 FaceIndex;

		// Whether this is a vertex-plane contact, edge-edge contact etc.
		EContactPointType ContactType;

		TContactPoint()
			: Phi(InvalidPhi<FRealType>())
			, FaceIndex(INDEX_NONE)
			, ContactType(EContactPointType::Unknown)
		{
		}

		template<typename U>
		TContactPoint(const TContactPoint<U>& Other)
			: ShapeContactPoints{ TVec3<FRealType>(Other.ShapeContactPoints[0]), TVec3<FRealType>(Other.ShapeContactPoints[1]) }
			, ShapeContactNormal{ TVec3<FRealType>(Other.ShapeContactNormal) }
			, Phi(FRealType(Other.Phi))
			, FaceIndex(Other.FaceIndex)
			, ContactType(Other.ContactType)
		{
		}

		// Whether the contact point has been set up with contact data
		bool IsSet() const 
		{ 
			return (Phi != InvalidPhi<FRealType>());
		}

		// Switch the shape indices. For use when calling a collision detection method which takes shape types in the opposite order to what you want.
		// WARNING: this function is not "general purporse" and only works when ShapeContactPoints are both in the smae space, because
		// the normal is in the space of the second body and negating it does not otherwise transform it to be relative to the other body. 
		// However, it is useful in the cases where we perform collision detection in the space of one of the bodies before transforming into 
		// shape space, whcih is actually most of the time.
		//TContactPoint<FRealType>& SwapShapes()
		//{
		//	if (IsSet())
		//	{
		//		Swap(ShapeContactPoints[0], ShapeContactPoints[1]);
		//		ShapeContactNormal = -ShapeContactNormal;
		//	}
		//	return *this;
		//}
	};
}