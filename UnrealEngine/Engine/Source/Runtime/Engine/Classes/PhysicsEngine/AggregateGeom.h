// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/LevelSetElem.h"
#include "PhysicsEngine/BoxElem.h"
#include "PhysicsEngine/SphereElem.h"
#include "PhysicsEngine/SphylElem.h"
#include "PhysicsEngine/TaperedCapsuleElem.h"
#include "AggregateGeom.generated.h"

class FMaterialRenderProxy;

/** Container for an aggregate of collision shapes */
USTRUCT()
struct ENGINE_API FKAggregateGeom
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

	class FKConvexGeomRenderInfo* RenderInfo;

	FKAggregateGeom()
		: RenderInfo(NULL)
	{
	}

	FKAggregateGeom(const FKAggregateGeom& Other)
		: RenderInfo(nullptr)
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
		return SphereElems.Num() + SphylElems.Num() + BoxElems.Num() + ConvexElems.Num() + TaperedCapsuleElems.Num() + LevelSetElems.Num();
	}

	int32 GetElementCount(EAggCollisionShape::Type Type) const;

	SIZE_T GetAllocatedSize() const { return SphereElems.GetAllocatedSize() + SphylElems.GetAllocatedSize() + BoxElems.GetAllocatedSize() + ConvexElems.GetAllocatedSize() + TaperedCapsuleElems.GetAllocatedSize(); }

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
		ensure(false);
		return nullptr;
	}

	void EmptyElements()
	{
		BoxElems.Empty();
		ConvexElems.Empty();
		SphylElems.Empty();
		SphereElems.Empty();
		TaperedCapsuleElems.Empty();
		LevelSetElems.Empty();

		FreeRenderInfo();
	}

#if WITH_EDITORONLY_DATA
	void FixupDeprecated(FArchive& Ar);
#endif

	void GetAggGeom(const FTransform& Transform, const FColor Color, const FMaterialRenderProxy* MatInst, bool bPerHullColor, bool bDrawSolid, bool bOutputVelocity, int32 ViewIndex, class FMeshElementCollector& Collector) const;

	/** Release the RenderInfo (if its there) and safely clean up any resources. Call on the game thread. */
	void FreeRenderInfo();

	FBox CalcAABB(const FTransform& Transform) const;

	/**
	* Calculates a tight box-sphere bounds for the aggregate geometry; this is more expensive than CalcAABB
	* (tight meaning the sphere may be smaller than would be required to encompass the AABB, but all individual components lie within both the box and the sphere)
	*
	* @param Output The output box-sphere bounds calculated for this set of aggregate geometry
	*	@param LocalToWorld Transform
	*/
	void CalcBoxSphereBounds(FBoxSphereBounds& Output, const FTransform& LocalToWorld) const;

	/** Returns the volume of this element */
	UE_DEPRECATED(5.1, "Changed to GetScaledVolume. Note that Volume calculation now includes non-uniform scale so values may have changed")
	FVector::FReal GetVolume(const FVector& Scale3D) const;

	/** Returns the volume of this element */
	FVector::FReal GetScaledVolume(const FVector& Scale3D) const;

	FGuid MakeDDCKey() const;

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
	}
};
