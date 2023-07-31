// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayAccessor.h"

struct FManagedArrayCollection;

/**
 * this class helps reading and writing the packed data used in the managed array
 */
struct FRemoveOnBreakData
{
public:
	inline static const FVector4f DisabledPackedData{ -1, 0, 0, 0 };   
	
	FRemoveOnBreakData()
		: PackedData(FRemoveOnBreakData::DisabledPackedData)
	{}

	FRemoveOnBreakData(const FVector4f& InPackedData)
		: PackedData(InPackedData)
	{}

	FRemoveOnBreakData(bool bEnable, const FVector2f& BreakTimer, bool bClusterCrumbling, const FVector2f& RemovalTimer)
	{
		PackedData.X = FMath::Min(FMath::Abs(BreakTimer.X), FMath::Abs(BreakTimer.Y)); // Min break timer
		PackedData.Y = FMath::Max(FMath::Abs(BreakTimer.X), FMath::Abs(BreakTimer.Y)); // Max break timer
		PackedData.Z = FMath::Min(FMath::Abs(RemovalTimer.X), FMath::Abs(RemovalTimer.Y)); // Min removal timer
		PackedData.W = FMath::Max(FMath::Abs(RemovalTimer.X), FMath::Abs(RemovalTimer.Y)); // Max removal timer

		PackedData.X = bEnable? PackedData.X: -1.0;
		PackedData.Z = bClusterCrumbling? -1.0: PackedData.Z;
	}
	
	bool IsEnabled() const { return PackedData.X >= 0; }
	bool GetClusterCrumbling() const { return IsEnabled() && (PackedData.Z < 0); } 
	FVector2f GetBreakTimer() const	{ return FVector2f(FMath::Abs(PackedData.X), FMath::Abs(PackedData.Y)); }
	FVector2f GetRemovalTimer() const { return FVector2f(FMath::Abs(PackedData.Z), FMath::Abs(PackedData.W)); }

	const FVector4f& GetPackedData() const { return PackedData; }

private:
	FVector4f PackedData;
};

/**
 * Provides an API for the run time aspect of the remove on break feature
 * this is to be used with the dynamic collection
 */
class FGeometryCollectionRemoveOnBreakDynamicFacade
{
public:
	static constexpr float DisabledBreakTimer = -1;
	static constexpr float DisabledPostBreakDuration = -1;
	static constexpr float CrumblingRemovalTimer = -1;
	
	FGeometryCollectionRemoveOnBreakDynamicFacade(FManagedArrayCollection& InCollection);

	/** 
	 * returns true if all the necessary attributes are present
	 * if not then the API can be used to create  
	 */
	bool IsValid() const;	

	/**
	 * Add the necessary attributes if they are missing and initialize them if necessary
	 * @param RemoveOnBreakAttribute remove on break attribute from the rest collection
	 * @param ChildrenAttribute remove on break attribute from the rest collection
	*/
	void AddAttributes(const TManagedArray<FVector4f>& RemoveOnBreakAttribute, const TManagedArray<TSet<int32>>& ChildrenAttribute);

	/** true if the removal is active for a specific piece */
	bool IsRemovalActive(int32 TransformIndex) const;

	/** true if a specific transfom uses cluster crumbling */
	bool UseClusterCrumbling(int32 TransformIndex) const;
	
	/**
	 * Update break timer and return the matching decay  
	 * @param TransformIndex index of the transform to update
	 * @param DeltaTime elapsed time since the last update in second
	 * @return decay value computed from the timer and duration ( [0,1] range )
	 */
	float UpdateBreakTimerAndComputeDecay(int32 TransformIndex, float DeltaTime);
	
private:
	/** Time elapsed since the break in seconds */
	TManagedArrayAccessor<float> BreakTimerAttribute;

	/** duration after the break before the removal process starts */
	TManagedArrayAccessor<float> PostBreakDurationAttribute;
	
	/** removal duration */
	TManagedArrayAccessor<float> BreakRemovalDurationAttribute;
};

/**
 * Provides an API for the run time aspect of the remove on sleep feature
 * this is to be used with the dynamic collection
 */
class FGeometryCollectionRemoveOnSleepDynamicFacade
{
public:
	FGeometryCollectionRemoveOnSleepDynamicFacade(FManagedArrayCollection& InCollection);

	/** 
	 * returns true if all the necessary attributes are present
	 * if not then the API can be used to create  
	 */
	bool IsValid() const;	

	/**
	 * Add the necessary attributes if they are missing and initialize them if necessary
	 * @param MaximumSleepTime range of time to initialize the sleep duration
	 * @param RemovalDuration range of time to initialize the removal duration
	*/
	void AddAttributes(const FVector2D& MaximumSleepTime, const FVector2D& RemovalDuration);

	/** true if the removal is active for a specific piece */
	bool IsRemovalActive(int32 TransformIndex) const;

	/**
	 * Compute the slow moving state and update from the last position
	 * After calling this method, LastPosition will be updated with Position
	 * @param Position Current world position
	 * @param DeltaTime elapsed time since last update
	 * @return true if the piece if the piece is considered slow moving 
	 **/
	bool ComputeSlowMovingState(int32 TransformIndex, const FVector& Position, float DeltaTime, FVector::FReal VelocityThreshold);
	
	/**
	 * Update the sleep timer
	 * @param TransformIndex index of the transform to update
	 * @param DeltaTime elapsed time since last update
	 */
	void UpdateSleepTimer(int32 TransformIndex, float DeltaTime);

	/** Compute decay from elapsed timer and durantion attributes */
	float ComputeDecay(int32 TransformIndex) const;

	
private:
	/** Time elapsed since the sleep detection */
	TManagedArrayAccessor<float> SleepTimerAttribute;
	
	/** duration after the sleep detection before the removal process starts (read only from outside) */
	TManagedArrayAccessor<float> MaxSleepTimeAttribute;
	
	/** removal duration (read only from outside) */
	TManagedArrayAccessor<float> SleepRemovalDurationAttribute;

	/** Last position used to detect slow moving pieces */
	TManagedArrayAccessor<FVector> LastPositionAttribute;
};

/**
 * Provides an API for decay related attributes ( use for remove on break and remove on sleep )
 */
class FGeometryCollectionDecayDynamicFacade
{
public:
	FGeometryCollectionDecayDynamicFacade(FManagedArrayCollection& InCollection);

	/** 
	 * returns true if all the necessary attributes are present
	 * if not then the API can be used to create  
	 */
	bool IsValid() const;	

	/** Add the necessary attributes if they are missing and initialize them if necessary */
	void AddAttributes();

	/** state of decay ([0-1] range) */
	// @todo(chaos) this should eventually move to a common removal facade when break and sleep removal are consolidated
	TManagedArrayAccessor<float> DecayAttribute;

	/** scale transform used to shrink the geometry collection piece */
	TManagedArrayAccessor<FTransform> UniformScaleAttribute;
};
