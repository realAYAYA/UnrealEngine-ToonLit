// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayAccessor.h"

struct FManagedArrayCollection;
class FGeometryDynamicCollection;

namespace GeometryCollection::Facades
{
	class FCollectionRemoveOnBreakFacade;
}

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
	
	FGeometryCollectionRemoveOnBreakDynamicFacade(FGeometryDynamicCollection& InCollection);

	/** 
	 * returns true if all the necessary attributes are present
	 * if not then the API can be used to create  
	 */
	bool IsValid() const;	

	/** Is this facade const access */
	bool IsConst() const;

	/** Add the relevant attributes */
	void DefineSchema();

	/**
	 * Add the necessary attributes if they are missing and initialize them if necessary
	 * @param RemoveOnBreakFacade remove on break facade from the rest collection that contain the original user set attributes
	*/
	void SetAttributeValues(const GeometryCollection::Facades::FCollectionRemoveOnBreakFacade& RemoveOnBreakFacade);

	/** true if the removal is active for a specific piece */
	bool IsRemovalActive(int32 TransformIndex) const;

	/** true if a specific transform uses cluster crumbling */
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

	const FGeometryDynamicCollection& DynamicCollection;
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

	/** Is this facade const access */
	bool IsConst() const;

	/** Add the relevant attributes */
	void DefineSchema();

	/**
	 * Add the necessary attributes if they are missing and initialize them if necessary
	 * @param MaximumSleepTime range of time to initialize the sleep duration
	 * @param RemovalDuration range of time to initialize the removal duration
	*/
	void SetAttributeValues(const FVector2D& MaximumSleepTime, const FVector2D& RemovalDuration);

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

	/** Compute decay from elapsed timer and duration attributes */
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

	/** Get decay value for a specific transform index */
	float GetDecay(int32 TransformIndex) const;

	/** Set decay value for a specific transform index */
	void SetDecay(int32 TransformIndex, float DecayValue);

	/** Get the size of the decay attribute - this should match the number of transforms of the collection */
	int32 GetDecayAttributeSize() const;

private:
	/** state of decay ([0-1] range) */
	TManagedArrayAccessor<float> DecayAttribute;
};
