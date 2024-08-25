// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/LevelSetElem.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SkinnedLevelSetElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/TaperedCapsuleElem.h"
#include "Async/Mutex.h"
#include "AggregateGeom.generated.h"

class FMaterialRenderProxy;

/** Container for an aggregate of collision shapes */
USTRUCT()
struct FKAggregateGeom
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, editfixedsize, Category = "Aggregate Geometry", meta = (DisplayName = "Spheres"))
	TArray<FKSphereElem> SphereElems;

	UPROPERTY(EditAnywhere, editfixedsize, Category = "Aggregate Geometry", meta = (DisplayName = "Boxes"))
	TArray<FKBoxElem> BoxElems;

	UPROPERTY(EditAnywhere, editfixedsize, Category = "Aggregate Geometry", meta = (DisplayName = "Capsules"))
	TArray<FKSphylElem> SphylElems;

	UPROPERTY(EditAnywhere, editfixedsize, Category = "Aggregate Geometry", meta = (DisplayName = "Convex Elements"))
	TArray<FKConvexElem> ConvexElems;

	UPROPERTY(EditAnywhere, editfixedsize, Category = "Aggregate Geometry", meta = (DisplayName = "Tapered Capsules"))
	TArray<FKTaperedCapsuleElem> TaperedCapsuleElems;

	UPROPERTY(EditAnywhere, editfixedsize, Category = "Aggregate Geometry", meta = (DisplayName = "Level Sets"))
	TArray<FKLevelSetElem> LevelSetElems;

	UPROPERTY(EditAnywhere, editfixedsize, Category = "Aggregate Geometry", meta = (DisplayName = "(Experimental) Skinned Level Sets"), Experimental)
	TArray<FKSkinnedLevelSetElem> SkinnedLevelSetElems;

	FKAggregateGeom()
		: RenderInfoPtr(nullptr)
	{
	}

	FKAggregateGeom(const FKAggregateGeom& Other)
		: RenderInfoPtr(nullptr)
	{
		CloneAgg(Other);
	}

	const FKAggregateGeom& operator=(const FKAggregateGeom& Other)
	{
		FreeRenderInfo();
		CloneAgg(Other);
		return *this;
	}

	int32 GetElementCount() const
	{
		return SphereElems.Num() + SphylElems.Num() + BoxElems.Num() + ConvexElems.Num() + TaperedCapsuleElems.Num() + LevelSetElems.Num() + SkinnedLevelSetElems.Num();
	}

	ENGINE_API int32 GetElementCount(EAggCollisionShape::Type Type) const;

	SIZE_T GetAllocatedSize() const { return SphereElems.GetAllocatedSize() + SphylElems.GetAllocatedSize() + BoxElems.GetAllocatedSize() + ConvexElems.GetAllocatedSize() + TaperedCapsuleElems.GetAllocatedSize() + LevelSetElems.GetAllocatedSize() + SkinnedLevelSetElems.GetAllocatedSize(); }

	FKShapeElem* GetElement(const EAggCollisionShape::Type Type, const int32 Index)
	{
		switch (Type)
		{
		case EAggCollisionShape::Sphere:
			if (ensure(SphereElems.IsValidIndex(Index))) { return &SphereElems[Index]; }
		case EAggCollisionShape::Box:
			if (ensure(BoxElems.IsValidIndex(Index))) { return &BoxElems[Index]; }
		case EAggCollisionShape::Sphyl:
			if (ensure(SphylElems.IsValidIndex(Index))) { return &SphylElems[Index]; }
		case EAggCollisionShape::Convex:
			if (ensure(ConvexElems.IsValidIndex(Index))) { return &ConvexElems[Index]; }
		case EAggCollisionShape::TaperedCapsule:
			if (ensure(TaperedCapsuleElems.IsValidIndex(Index))) { return &TaperedCapsuleElems[Index]; }
		case EAggCollisionShape::LevelSet:
			if (ensure(LevelSetElems.IsValidIndex(Index))) { return &LevelSetElems[Index]; }
		case EAggCollisionShape::SkinnedLevelSet:
			if (ensure(SkinnedLevelSetElems.IsValidIndex(Index))) { return &SkinnedLevelSetElems[Index]; }
		default:
			ensure(false);
			return nullptr;
		}
	}

	FKShapeElem* GetElement(const int32 InIndex)
	{
		int Index = InIndex;
		if (Index < SphereElems.Num()) { return &SphereElems[Index]; }
		Index -= SphereElems.Num();
		if (Index < BoxElems.Num()) { return &BoxElems[Index]; }
		Index -= BoxElems.Num();
		if (Index < SphylElems.Num()) { return &SphylElems[Index]; }
		Index -= SphylElems.Num();
		if (Index < ConvexElems.Num()) { return &ConvexElems[Index]; }
		Index -= ConvexElems.Num();
		if (Index < TaperedCapsuleElems.Num()) { return &TaperedCapsuleElems[Index]; }
		Index -= TaperedCapsuleElems.Num();
		if (Index < LevelSetElems.Num()) { return &LevelSetElems[Index]; }
		Index -= LevelSetElems.Num();
		if (Index < SkinnedLevelSetElems.Num()) { return &SkinnedLevelSetElems[Index]; }
		ensure(false);
		return nullptr;
	}

	const FKShapeElem* GetElement(const int32 InIndex) const
	{
		int Index = InIndex;
		if (Index < SphereElems.Num()) { return &SphereElems[Index]; }
		Index -= SphereElems.Num();
		if (Index < BoxElems.Num()) { return &BoxElems[Index]; }
		Index -= BoxElems.Num();
		if (Index < SphylElems.Num()) { return &SphylElems[Index]; }
		Index -= SphylElems.Num();
		if (Index < ConvexElems.Num()) { return &ConvexElems[Index]; }
		Index -= ConvexElems.Num();
		if (Index < TaperedCapsuleElems.Num()) { return &TaperedCapsuleElems[Index]; }
		Index -= TaperedCapsuleElems.Num();
		if (Index < LevelSetElems.Num()) { return &LevelSetElems[Index]; }
		Index -= LevelSetElems.Num();
		if (Index < SkinnedLevelSetElems.Num()) { return &SkinnedLevelSetElems[Index]; }
		ensure(false);
		return nullptr;
	}

	const FKShapeElem* GetElementByName(const FName InName) const
	{
		if (const FKShapeElem* FoundSphereElem = GetElementByName<FKSphereElem>(MakeArrayView(SphereElems), InName))
		{
			return FoundSphereElem;
		}
		else if (const FKShapeElem* FoundBoxElem = GetElementByName<FKBoxElem>(MakeArrayView(BoxElems), InName))
		{
			return FoundBoxElem;
		}
		else if (const FKShapeElem* FoundSphylElem = GetElementByName<FKSphylElem>(MakeArrayView(SphylElems), InName))
		{
			return FoundSphylElem;
		}
		else if (const FKShapeElem* FoundConvexElem = GetElementByName<FKConvexElem>(MakeArrayView(ConvexElems), InName))
		{
			return FoundConvexElem;
		}
		else if (const FKShapeElem* FoundTaperedCapsuleElem = GetElementByName<FKTaperedCapsuleElem>(MakeArrayView(TaperedCapsuleElems), InName))
		{
			return FoundTaperedCapsuleElem;
		}
		else if (const FKShapeElem* FoundLevelSetElem = GetElementByName<FKLevelSetElem>(MakeArrayView(LevelSetElems), InName))
		{
			return FoundLevelSetElem;
		}
		else if (const FKShapeElem* FoundSkinnedLevelSetElem = GetElementByName<FKSkinnedLevelSetElem>(MakeArrayView(SkinnedLevelSetElems), InName))
		{
			return FoundSkinnedLevelSetElem;
		}

		return nullptr;
	}

	int32 GetElementIndexByName(const FName InName) const
	{
		int32 FoundIndex = GetElementIndexByName<FKSphereElem>(MakeArrayView(SphereElems), InName);
		int32 StartIndex = 0;
		if (FoundIndex != INDEX_NONE)
		{
			return FoundIndex + StartIndex;
		}
		StartIndex += SphereElems.Num();

		FoundIndex = GetElementIndexByName<FKBoxElem>(MakeArrayView(BoxElems), InName);
		if (FoundIndex != INDEX_NONE)
		{
			return FoundIndex + StartIndex;
		}
		StartIndex += BoxElems.Num();

		FoundIndex = GetElementIndexByName<FKSphylElem>(MakeArrayView(SphylElems), InName);
		if (FoundIndex != INDEX_NONE)
		{
			return FoundIndex + StartIndex;
		}
		StartIndex += SphylElems.Num();

		FoundIndex = GetElementIndexByName<FKConvexElem>(MakeArrayView(ConvexElems), InName);
		if (FoundIndex != INDEX_NONE)
		{
			return FoundIndex + StartIndex;
		}
		StartIndex += ConvexElems.Num();

		FoundIndex = GetElementIndexByName<FKTaperedCapsuleElem>(MakeArrayView(TaperedCapsuleElems), InName);
		if (FoundIndex != INDEX_NONE)
		{
			return FoundIndex + StartIndex;
		}
		StartIndex += TaperedCapsuleElems.Num();

		FoundIndex = GetElementIndexByName<FKLevelSetElem>(MakeArrayView(LevelSetElems), InName);
		if (FoundIndex != INDEX_NONE)
		{
			return FoundIndex + StartIndex;
		}
		StartIndex += LevelSetElems.Num();

		FoundIndex = GetElementIndexByName<FKSkinnedLevelSetElem>(MakeArrayView(SkinnedLevelSetElems), InName);
		if (FoundIndex != INDEX_NONE)
		{
			return FoundIndex + StartIndex;
		}

		return INDEX_NONE;
	}

	void EmptyElements()
	{
		BoxElems.Empty();
		ConvexElems.Empty();
		SphylElems.Empty();
		SphereElems.Empty();
		TaperedCapsuleElems.Empty();
		LevelSetElems.Empty();
		SkinnedLevelSetElems.Empty();

		FreeRenderInfo();
	}

#if WITH_EDITORONLY_DATA
	ENGINE_API void FixupDeprecated(FArchive& Ar);
#endif

	ENGINE_API void GetAggGeom(const FTransform& Transform, const FColor Color, const FMaterialRenderProxy* MatInst, bool bPerHullColor, bool bDrawSolid, bool bOutputVelocity, int32 ViewIndex, class FMeshElementCollector& Collector) const;

	/** Release the RenderInfo (if its there) and safely clean up any resources. Call on the game thread. */
	ENGINE_API void FreeRenderInfo();

	ENGINE_API FBox CalcAABB(const FTransform& Transform) const;

	/**
	* Calculates a tight box-sphere bounds for the aggregate geometry; this is more expensive than CalcAABB
	* (tight meaning the sphere may be smaller than would be required to encompass the AABB, but all individual components lie within both the box and the sphere)
	*
	* @param Output The output box-sphere bounds calculated for this set of aggregate geometry
	*	@param LocalToWorld Transform
	*/
	ENGINE_API void CalcBoxSphereBounds(FBoxSphereBounds& Output, const FTransform& LocalToWorld) const;

	/** Returns the volume of this element */
	UE_DEPRECATED(5.1, "Changed to GetScaledVolume. Note that Volume calculation now includes non-uniform scale so values may have changed")
	ENGINE_API FVector::FReal GetVolume(const FVector& Scale3D) const;

	/** Returns the volume of this element */
	ENGINE_API FVector::FReal GetScaledVolume(const FVector& Scale3D) const;

	ENGINE_API FGuid MakeDDCKey() const;

private:

	/** Helper function for safely copying instances */
	void CloneAgg(const FKAggregateGeom& Other)
	{
		SphereElems = Other.SphereElems;
		BoxElems = Other.BoxElems;
		SphylElems = Other.SphylElems;
		ConvexElems = Other.ConvexElems;
		TaperedCapsuleElems = Other.TaperedCapsuleElems;
		LevelSetElems = Other.LevelSetElems;
		SkinnedLevelSetElems = Other.SkinnedLevelSetElems;
	}

	template <class T>
	const FKShapeElem* GetElementByName(TArrayView<const T> Elements, const FName InName) const
	{
		const FKShapeElem* FoundElem = Elements.FindByPredicate(
			[InName](const T& Elem)
			{
				return InName == Elem.GetName();
			});
		return FoundElem;
	}

	template <class T>
	int32 GetElementIndexByName(TArrayView<const T> Elements, const FName InName) const
	{
		int32 FoundIndex = Elements.IndexOfByPredicate(
			[InName](const T& Elem)
			{
				return InName == Elem.GetName();
			});
		return FoundIndex;
	}

	// NOTE: RenderInfo is generated concurrently and lazily (hence being mutable)
	mutable std::atomic<class FKConvexGeomRenderInfo*> RenderInfoPtr;
	mutable UE::FMutex RenderInfoLock;
};
