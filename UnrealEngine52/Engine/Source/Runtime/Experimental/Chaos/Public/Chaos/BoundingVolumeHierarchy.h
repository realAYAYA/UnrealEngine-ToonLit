// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Array.h"
#include "Chaos/AABB.h"
#include "Chaos/Defines.h"
#include "Chaos/GeometryParticles.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Chaos/BoundingVolume.h"
#include "Chaos/Transform.h"
#include "ChaosLog.h"
#include "Chaos/ISpatialAcceleration.h"
#include "Templates/Models.h"

template <typename T, bool>
struct TBVHLeafTraits
{
};

template <typename T>
struct TBVHLeafTraits<T, true>
{
	using TPayloadType = typename T::TPayloadType;
};

template <typename T>
struct TBVHLeafTraits<T, false>
{
	using TPayloadType = typename T::ElementType;
};

struct CComplexBVHLeaf
{
	template <typename T>
	auto Requires(typename T::TPayloadType) ->void;
};

namespace Chaos
{

template <typename T, int d>
struct TBVHNode
{
	TBVHNode() {}
	~TBVHNode() {}
	TVector<T, d> MMin, MMax;
	int32 MAxis;
	TArray<int32> MChildren;
	int32 LeafIndex;
};

template <typename T, int d>
FArchive& operator<<(FArchive& Ar, TBVHNode<T,d>& LeafNode)
{
	return Ar << LeafNode.LeafIndex << LeafNode.MAxis << LeafNode.MChildren << LeafNode.MMax << LeafNode.MMin;
}

template<class OBJECT_ARRAY, class LEAF_TYPE, class T = FReal, int d = 3>
class TBoundingVolumeHierarchy final : public ISpatialAcceleration<int32, T,d>
{
  public:
	static constexpr int32 DefaultMaxLevels = 12;
	static constexpr bool DefaultAllowMultipleSplitting = false;
	static constexpr bool DefaultUseVelocity = false;
	static constexpr T DefaultDt = 0;
	using TPayloadType = typename TBVHLeafTraits<LEAF_TYPE, TModels<CComplexBVHLeaf, LEAF_TYPE>::Value>::TPayloadType;

	CHAOS_API TBoundingVolumeHierarchy()
		: MObjects(nullptr)
	{
	}

	CHAOS_API TBoundingVolumeHierarchy(const OBJECT_ARRAY& Objects, const int32 MaxLevels = DefaultMaxLevels, const bool bUseVelocity = DefaultUseVelocity, const T Dt = DefaultDt);
	CHAOS_API TBoundingVolumeHierarchy(const OBJECT_ARRAY& Objects, const TArray<uint32>& ActiveIndices, const int32 MaxLevels = DefaultMaxLevels, const bool bUseVelocity = DefaultUseVelocity, const T Dt = DefaultDt);
	
	TBoundingVolumeHierarchy(const TBoundingVolumeHierarchy<OBJECT_ARRAY, LEAF_TYPE, T, d>& Other) = delete;
	CHAOS_API TBoundingVolumeHierarchy(TBoundingVolumeHierarchy<OBJECT_ARRAY, LEAF_TYPE, T, d>&& Other)
	    : MObjects(Other.MObjects), MGlobalObjects(MoveTemp(Other.MGlobalObjects)), MWorldSpaceBoxes(MoveTemp(Other.MWorldSpaceBoxes)), MMaxLevels(Other.MMaxLevels), Elements(MoveTemp(Other.Elements)), Leafs(MoveTemp(Other.Leafs))
	{
	}

	CHAOS_API virtual ISpatialAcceleration<int32, T, d>& operator=(const ISpatialAcceleration<int32, T, d>& Other) override
	{

		check(Other.GetType() == ESpatialAcceleration::AABBTreeBV);
		return operator=(static_cast<const TBoundingVolumeHierarchy<OBJECT_ARRAY, LEAF_TYPE, T, d>&>(Other));
	}

	CHAOS_API TBoundingVolumeHierarchy& operator=(const TBoundingVolumeHierarchy<OBJECT_ARRAY, LEAF_TYPE, T, d>& Other)
	{
		ISpatialAcceleration<int32, T, d>::operator=(Other);
		MObjects = Other.MObjects;
		MGlobalObjects = Other.MGlobalObjects;
		MWorldSpaceBoxes = Other.MWorldSpaceBoxes;
		MMaxLevels = Other.MMaxLevels;
		Elements = Other.Elements;
		Leafs = Other.Leafs;
		return *this;
	}

	CHAOS_API TBoundingVolumeHierarchy& operator=(TBoundingVolumeHierarchy<OBJECT_ARRAY, LEAF_TYPE, T, d>&& Other)
	{
		MObjects = Other.MObjects;
		MGlobalObjects = MoveTemp(Other.MGlobalObjects);
		MWorldSpaceBoxes = MoveTemp(Other.MWorldSpaceBoxes);
		MMaxLevels = Other.MMaxLevels;
		Elements = MoveTemp(Other.Elements);
		Leafs = MoveTemp(Other.Leafs);
		return *this;
	}

	CHAOS_API void UpdateHierarchy(const bool AllowMultipleSplitting = DefaultAllowMultipleSplitting, const bool bUseVelocity = DefaultUseVelocity, const T Dt = DefaultDt);
	CHAOS_API void UpdateHierarchy(const TArray<uint32>& ActiveIndices, const bool AllowMultipleSplitting = DefaultAllowMultipleSplitting, const bool bUseVelocity = DefaultUseVelocity, const T Dt = DefaultDt);

	CHAOS_API void Reinitialize(bool bUseVelocity, T Dt)
	{
		UpdateHierarchy(DefaultAllowMultipleSplitting, bUseVelocity, Dt);
	}

	CHAOS_API void Reinitialize(const TArray<uint32>& ActiveIndices, bool bUseVelocity, T Dt)
	{
		UpdateHierarchy(ActiveIndices, DefaultAllowMultipleSplitting, bUseVelocity, Dt);
	}

	template<class T_INTERSECTION>
	TArray<int32> FindAllIntersectionsImp(const T_INTERSECTION& Intersection) const
	{
		if (Elements.Num())
		{
			TArray<int32> IntersectionList = FindAllIntersectionsHelper(Elements[0], Intersection);
			IntersectionList.Append(MGlobalObjects);
			return IntersectionList;
		}
		else
		{
			return MGlobalObjects;
		}
	}

	// Begin ISpatialAcceleration interface
	CHAOS_API TArray<int32> FindAllIntersections(const FAABB3& Box) const { return FindAllIntersectionsImp(Box); }
	CHAOS_API TArray<int32> FindAllIntersections(const TSpatialRay<T,d>& Ray) const { return FindAllIntersectionsImp(Ray); }
	CHAOS_API TArray<int32> FindAllIntersections(const TVector<T, d>& Point) const { return FindAllIntersectionsImp(Point); }
	CHAOS_API TArray<int32> FindAllIntersections(const TGeometryParticles<T, d>& InParticles, const int32 i) const;
	// End ISpatialAcceleration interface

	CHAOS_API const TArray<int32>& GlobalObjects() const
	{
		return MGlobalObjects;
	}

	// TODO(mlentine): Need to move this elsewhere; probably on CollisionConstraint
	CHAOS_API const TAABB<T, d>& GetWorldSpaceBoundingBox(const TGeometryParticles<T, d>& InParticles, const int32 Index);

#if !UE_BUILD_SHIPPING
	CHAOS_API virtual void DebugDraw(ISpacialDebugDrawInterface<T>* InInterface) const override;
#endif

	CHAOS_API void Serialize(FArchive& Ar);

	CHAOS_API virtual void Serialize(FChaosArchive& Ar) override
	{
		check(false);
	}

  private:
	CHAOS_API void PrintTree(FString Prefix, const TBVHNode<T,d>* MyNode) const
	{
		UE_LOG(LogChaos, Verbose, TEXT("%sNode has Box: (%f, %f, %f) to (%f, %f, %f) with %d Children"), *Prefix, MyNode->MMin[0], MyNode->MMin[1], MyNode->MMin[2], MyNode->MMax[0], MyNode->MMax[1], MyNode->MMax[2], MyNode->MChildren.Num());
		for (auto& Child : MyNode->MChildren)
		{
			PrintTree(Prefix + " ", &Elements[Child]);
		}
	}

	CHAOS_API TArray<int32> FindAllIntersectionsHelper(const TBVHNode<T,d>& MyNode, const TVector<T, d>& Point) const;
	CHAOS_API TArray<int32> FindAllIntersectionsHelper(const TBVHNode<T,d>& MyNode, const TAABB<T, d>& ObjectBox) const;
	CHAOS_API TArray<int32> FindAllIntersectionsHelper(const TBVHNode<T, d>& MyNode, const TSpatialRay<T, d>& Ray) const;

	template <typename QUERY_OBJECT>
	CHAOS_API void FindAllIntersectionsHelperRecursive(const TBVHNode<T,d>& MyNode, const QUERY_OBJECT& ObjectBox, TArray<int32>& AccumulateElements) const;
	CHAOS_API void UpdateHierarchyImp(const TArray<int32>& AllObjects, const bool bAllowMultipleSplitting, const bool bUseVelocity, const T Dt);

	CHAOS_API int32 GenerateNextLevel(const TVector<T, d>& GlobalMin, const TVector<T, d>& GlobalMax, const TArray<int32>& Objects, const int32 Axis, const int32 Level, const bool AllowMultipleSplitting);
	CHAOS_API int32 GenerateNextLevel(const TVector<T, d>& GlobalMin, const TVector<T, d>& GlobalMax, const TArray<int32>& Objects, const int32 Level);

	OBJECT_ARRAY const* MObjects;
	TArray<int32> MGlobalObjects;
	TMap<int32, TAABB<T, d>> MWorldSpaceBoxes;
	TArray<int32> MScratchAllObjects;
	int32 MMaxLevels;
	TArray<TBVHNode<T,d>> Elements;
	TArray<LEAF_TYPE> Leafs;
	FCriticalSection CriticalSection;
};

template<class OBJECT_ARRAY, class LEAF_TYPE, class T, int d>
FArchive& operator<<(FArchive& Ar, TBoundingVolumeHierarchy<OBJECT_ARRAY, LEAF_TYPE, T, d>& BVH)
{
	BVH.Serialize(Ar);
	return Ar;
}

}
