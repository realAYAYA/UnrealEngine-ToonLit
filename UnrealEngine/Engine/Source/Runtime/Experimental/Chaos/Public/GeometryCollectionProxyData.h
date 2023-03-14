// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Crc.h"

#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/GeometryCollectionCollisionStructureManager.h"

/*
* Managed arrays for simulation data used by the GeometryCollectionProxy
*/

/**
* FTransformDynamicCollection (FManagedArrayCollection)
*
* Stores per instance data for transforms and hierarchy information
*/
class CHAOS_API FTransformDynamicCollection : public FManagedArrayCollection
{
public:
	typedef FManagedArrayCollection Super;

	FTransformDynamicCollection();
	FTransformDynamicCollection(FTransformDynamicCollection&) = delete;
	FTransformDynamicCollection& operator=(const FTransformDynamicCollection&) = delete;
	FTransformDynamicCollection(FTransformDynamicCollection&&) = delete;
	FTransformDynamicCollection& operator=(FTransformDynamicCollection&&) = delete;

	// Transform Group
	TManagedArray<FTransform>   Transform;
	TManagedArray<int32>        Parent;
	TManagedArray<TSet<int32>>  Children;
	TManagedArray<int32>        SimulationType;
	TManagedArray<int32>        StatusFlags;

protected:

	/** Construct */
	void Construct();
};


/**
* FGeometryDynamicCollection (FTransformDynamicCollection)
*
* Stores per instance data for simulation level information
*/

class CHAOS_API FGeometryDynamicCollection : public FTransformDynamicCollection
{

public:
	FGeometryDynamicCollection();
	FGeometryDynamicCollection(FGeometryDynamicCollection&) = delete;
	FGeometryDynamicCollection& operator=(const FGeometryDynamicCollection&) = delete;
	FGeometryDynamicCollection(FGeometryDynamicCollection&&) = delete;
	FGeometryDynamicCollection& operator=(FGeometryDynamicCollection&&) = delete;

	typedef FTransformDynamicCollection Super;
	typedef TSharedPtr<Chaos::FImplicitObject, ESPMode::ThreadSafe> FSharedImplicit;

	static const FName ActiveAttribute;
	static const FName CollisionGroupAttribute;
	static const FName CollisionMaskAttribute;
	static const FName DynamicStateAttribute;
	static const FName ImplicitsAttribute;
	static const FName ShapesQueryDataAttribute;
	static const FName ShapesSimDataAttribute;
	static const FName SharedImplicitsAttribute;
	static const FName SimplicialsAttribute;
	static const FName SimulatableParticlesAttribute;

	// Transform Group
	TManagedArray<bool> Active;
	TManagedArray<int32> CollisionGroup;
	TManagedArray<int32> CollisionMask;
	TManagedArray<int32> CollisionStructureID;
	TManagedArray<int32> DynamicState;
	TManagedArray<FSharedImplicit> Implicits;
	TManagedArray<FVector3f> InitialAngularVelocity;
	TManagedArray<FVector3f> InitialLinearVelocity;
	TManagedArray<FTransform> MassToLocal;
	//TManagedArray<TArray<FCollisionFilterData>> ShapeQueryData;
	//TManagedArray<TArray<FCollisionFilterData>> ShapeSimData;
	TManagedArray<TUniquePtr<FCollisionStructureManager::FSimplicial>> Simplicials;
	TManagedArray<bool> SimulatableParticles;
};

/**
 * Provides an API for dynamic state related attributes
 * physics state , broken state, current parent (normal or internal clusters )
 * To be used with the dynamic collection
 */
class CHAOS_API FGeometryCollectionDynamicStateFacade
{
public:
	FGeometryCollectionDynamicStateFacade(FManagedArrayCollection& InCollection);

	/** returns true if all the necessary attributes are present */
	bool IsValid() const;

	/** return true if the transform is in a dynamic or sleeping state */
	bool IsDynamicOrSleeping(int32 TransformIndex) const;

	/** return true if the transform is in a sleeping state */
	bool IsSleeping(int32 TransformIndex) const;

	/** whether there's children attached to this transfom (Cluster) */
	bool HasChildren(int32 TransformIndex) const;
	
	/** return true if the transform has broken off its parent */
	bool HasBrokenOff(int32 TransformIndex) const;

	/** return true if the transform has an internal cluster parent */
	bool HasInternalClusterParent(int32 TransformIndex) const;

	/** return true if the transform has an internal cluster parent in a dynamic state */
	bool HasDynamicInternalClusterParent(int32 TransformIndex) const;
	
private:
	/** Active state, true means that the transform is active or broken off from its parent */
	TManagedArrayAccessor<bool> ActiveAttribute;

	/** physics state of the transform (Dynamic, kinematic, static, sleeping) */
	TManagedArrayAccessor<int32> DynamicStateAttribute;

	/** currently attached children (potentially different from the initial children setup) */
	TManagedArrayAccessor<TSet<int32>> ChildrenAttribute;
	
	/** Current parent (potentially different from the initial parent) */
	TManagedArrayAccessor<int32> ParentAttribute;

	/** type of internal state parent */
	TManagedArrayAccessor<uint8> InternalClusterParentTypeAttribute;
};

class FGeometryCollectioPerFrameData
{
public:
	FGeometryCollectioPerFrameData()
		: IsWorldTransformDirty(false)
		, bIsCollisionFilterDataDirty(false) {}

	const FTransform& GetWorldTransform() const { return WorldTransform; }

	void SetWorldTransform(const FTransform& InWorldTransform)
	{
		if (!WorldTransform.Equals(InWorldTransform))
		{
			WorldTransform = InWorldTransform;
			IsWorldTransformDirty = true;
		}
	}

	bool GetIsWorldTransformDirty() const { return IsWorldTransformDirty; }
	void ResetIsWorldTransformDirty() { IsWorldTransformDirty = false; }

	const FCollisionFilterData& GetSimFilter() const { return SimFilter; }
	void SetSimFilter(const FCollisionFilterData& NewSimFilter)
	{
		SimFilter = NewSimFilter;
		bIsCollisionFilterDataDirty = true;
	}

	const FCollisionFilterData& GetQueryFilter() const { return QueryFilter; }
	void SetQueryFilter(const FCollisionFilterData& NewQueryFilter)
	{
		QueryFilter = NewQueryFilter;
		bIsCollisionFilterDataDirty = true;
	}

	bool GetIsCollisionFilterDataDirty() const { return bIsCollisionFilterDataDirty; }
	void ResetIsCollisionFilterDataDirty() { bIsCollisionFilterDataDirty = false; }

private:
	FTransform WorldTransform;
	bool IsWorldTransformDirty;

	FCollisionFilterData SimFilter;
	FCollisionFilterData QueryFilter;
	bool bIsCollisionFilterDataDirty;
};

/**
 * Buffer structure for communicating simulation state between game and physics
 * threads.
 */
class FGeometryCollectionResults
{
public:
	FGeometryCollectionResults();

	void Reset();

	int32 NumTransformGroup() const { return Transforms.Num(); }

	void InitArrays(const FGeometryDynamicCollection& Other)
	{
		const int32 NumTransforms = Other.NumElements(FGeometryCollection::TransformGroup);
		States.SetNumUninitialized(NumTransforms);
		GlobalTransforms.SetNumUninitialized(NumTransforms);
		ParticleXs.SetNumUninitialized(NumTransforms);
		ParticleRs.SetNumUninitialized(NumTransforms);
		ParticleVs.SetNumUninitialized(NumTransforms);
		ParticleWs.SetNumUninitialized(NumTransforms);
		
		Transforms.SetNumUninitialized(NumTransforms);
		Parent.SetNumUninitialized(NumTransforms);
		InternalClusterUniqueIdx.SetNumUninitialized(NumTransforms);
#if WITH_EDITORONLY_DATA
		DamageInfo.SetNumUninitialized(NumTransforms);
#endif		
	}

	struct FState
	{
		int16 DynamicState: 8; // need to fit EObjectStateTypeEnum
		int16 DisabledState: 1;
		int16 HasInternalClusterParent: 1;
		int16 DynamicInternalClusterParent: 1;
		// 5 bits left
	};

	Chaos::FReal SolverDt;
	TArray<FState> States;
	TArray<FMatrix> GlobalTransforms;
	TArray<Chaos::FVec3> ParticleXs;
	TArray<Chaos::FRotation3> ParticleRs;
	TArray<Chaos::FVec3> ParticleVs;
	TArray<Chaos::FVec3> ParticleWs;

	TArray<FTransform> Transforms;
	TArray<int32> Parent;
	TArray<int32> InternalClusterUniqueIdx;

#if WITH_EDITORONLY_DATA
	struct FDamageInfo
	{
		float Damage = 0;
		float DamageThreshold = 0;
	};
	
	// use to display impulse statistics in editor
	TArray<FDamageInfo> DamageInfo;
#endif
	
	bool IsObjectDynamic;
	bool IsObjectLoading;
};
