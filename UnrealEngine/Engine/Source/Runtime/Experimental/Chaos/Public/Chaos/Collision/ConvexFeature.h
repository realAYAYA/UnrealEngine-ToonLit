// Copyright Epic Games, Inc.All Rights Reserved.
#pragma once

#include "Chaos/Core.h"

namespace Chaos::Private
{
	// The type of a convex feature
	enum class EConvexFeatureType : int8
	{
		Unknown,
		Vertex,
		Edge,
		Plane
	};

	// Information used to identify a feasture on a convex shape
	class FConvexFeature
	{
	public:
		FConvexFeature& Init()
		{
			ObjectIndex = INDEX_NONE;
			PlaneIndex = INDEX_NONE;
			PlaneFeatureIndex = INDEX_NONE;
			FeatureType = EConvexFeatureType::Unknown;
			return *this;
		}

		void Set(const int32 InObjectIndex, const EConvexFeatureType InFeatureType, const int32 InPlaneIndex, const int32 InPlaneFeatureIndex)
		{
			ObjectIndex = InObjectIndex;
			PlaneIndex = InPlaneIndex;
			PlaneFeatureIndex = InPlaneFeatureIndex;
			FeatureType = InFeatureType;
		}

		// An index used to index into some collection (e.g., triangle index in a mesh)
		int32 ObjectIndex;

		// The plane index on the convex. E.g., See FConvex::GetPlane()
		int32 PlaneIndex;

		// For Vertex or Edge features, the vertex the feature on the plane. E.g., See FConvex::GetPlaneVertex()
		int32 PlaneFeatureIndex;

		// The feature type
		EConvexFeatureType FeatureType;
	};

	// A contact point between two convex features
	template<typename T>
	class TConvexContactPoint
	{
	public:
		using FRealType = T;

		TConvexContactPoint<FRealType>& Init()
		{
			Features[0].Init();
			Features[1].Init();
			ShapeContactPoints[0] = TVec3<FRealType>(0);
			ShapeContactPoints[1] = TVec3<FRealType>(0);
			ShapeContactNormal = TVec3<FRealType>(0);
			Phi = TNumericLimits<FRealType>::Max();
			return *this;
		}

		FConvexFeature Features[2];
		TVec3<FRealType> ShapeContactPoints[2];
		TVec3<FRealType> ShapeContactNormal;
		FRealType Phi;
	};

	using FConvexContactPoint = TConvexContactPoint<FReal>;
	using FConvexContactPointf = TConvexContactPoint<FRealSingle>;
}