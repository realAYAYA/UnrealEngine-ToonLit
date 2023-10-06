// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/Core.h"
#include "Containers/UnrealString.h"
#include "DataWrappers/ChaosVDCollisionDataWrappers.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "UObject/ObjectMacros.h"
#include "Containers/UnrealString.h"

namespace Chaos
{
	class FImplicitObject;
}

DECLARE_MULTICAST_DELEGATE(FChaosVDRecordingUpdated)
DECLARE_MULTICAST_DELEGATE_TwoParams(FChaosVDGeometryDataLoaded, const TSharedPtr<const Chaos::FImplicitObject>&, const uint32 GeometryID)

struct FChaosVDStepData
{
	FString StepName;
	TArray<FChaosVDParticleDataWrapper> RecordedParticlesData;
	TArray<TSharedPtr<FChaosVDParticlePairMidPhase>> RecordedMidPhases;
	TMap<int32, TArray<FChaosVDConstraint>> RecordedConstraintsByParticleID;
	TMap<int32, TArray<TSharedPtr<FChaosVDParticlePairMidPhase>>> RecordedMidPhasesByParticleID;
	TSet<int32> ParticlesDestroyedIDs;
};

typedef TArray<FChaosVDStepData, TInlineAllocator<16>> FChaosVDStepsContainer;

struct CHAOSVDDATA_API FChaosVDSolverFrameData
{
	FString DebugName;
	int32 SolverID = INDEX_NONE;
	uint64 FrameCycle = 0;
	Chaos::FRigidTransform3 SimulationTransform;
	bool bIsKeyFrame = false;
	FChaosVDStepsContainer SolverSteps;
	TSet<int32> ParticlesDestroyedIDs;
};

struct FChaosVDGameFrameData
{
	uint64 FirstCycle;
	uint64 LastCycle;
};

/**
 * Struct that represents a recorded Physics simulation.
 * It is currently populated while analyzing a Trace session
 */
struct CHAOSVDDATA_API FChaosVDRecording
{	
	/** Returns the current available recorded solvers number */
	int32 GetAvailableSolversNumber() const { return RecordedFramesDataPerSolver.Num(); }
	
	/** Returns the current available Game Frames */
	int32 GetAvailableGameFramesNumber() const { return GameFrames.Num(); }

	/** Returns a reference to the array holding all the available game frames */
	const TArray<FChaosVDGameFrameData>& GetAvailableGameFrames() const { return GameFrames; }

	/** Returns a reference to the map containing the available solver data */
	const TMap<int32, TArray<FChaosVDSolverFrameData>>& GetAvailableSolvers() const { return RecordedFramesDataPerSolver; }

	/**
	 * Returns the number of available frame data for the specified solver ID
	 * @param SolverID ID of the solver 
	 */
	int32 GetAvailableSolverFramesNumber(int32 SolverID) const;
	
	/**
	 * Returns the name of the specified solver id
	 * @param SolverID ID of the solver 
	 */
	FString GetSolverName(int32 SolverID);

	/**
	 * Return a ptr to the existing solver frame data from the specified ID and Frame number
	 * @param SolverID ID of the solver
	 * @param FrameNumber Frame number
	 * @return Ptr to the existing solver frame data from the specified ID and Frame number - It is a ptr to the array element, Do not store
	 */
	FChaosVDSolverFrameData* GetSolverFrameData(int32 SolverID, int32 FrameNumber);
	
	/**
	 * Return a ptr to the existing solver frame data from the specified ID and Frame number
	 * @param SolverID ID if the solver
	 * @param Cycle Platform cycle at which the solver frame was recorded
	 * @return Ptr to the existing solver frame data from the specified ID and Frame number - It is a ptr to the array element, Do not store
	 */
	FChaosVDSolverFrameData* GetSolverFrameDataAtCycle(int32 SolverID, uint64 Cycle);

	/**
	 * Searches and returns the lowest frame number of a solver at the specified cycle
	 * @param SolverID ID if the solver
	 * @param Cycle Platform cycle to use as lower bound
	 * @return Found frame number. INDEX_NONE if no frame is found for the specified cycle
	 */
	int32 GetLowestSolverFrameNumberAtCycle(int32 SolverID, uint64 Cycle);

	int32 FindFirstSolverKeyFrameNumberFromFrame(int32 SolverID, int32 StartFrameNumber);
	
	/**
	 * Searches and returns the lowest frame number of a solver at the specified cycle
	 * @param SolverID ID if the solver
	 * @param GameFrame Platform cycle to use as lower bound
	 * @return Found frame number. INDEX_NONE if no frame is found for the specified cycle
	 */
	int32 GetLowestSolverFrameNumberGameFrame(int32 SolverID, int32 GameFrame);
	
	/**
	 * Searches and returns the lowest game frame number at the specified solver frame
	 * @param SolverID ID if the solver to evaluate
	 * @param SolverFrame Frame number of the solver to evaluate
	 * @return Found Game frame number. INDEX_NONE if no frame is found for the specified cycle
	 */
	int32 GetLowestGameFrameAtSolverFrameNumber(int32 SolverID, int32 SolverFrame);

	/**
	 * Adds a Solver Frame Data entry for a specific Solver ID. Creates a solver entry if it does not exist 
	 * @param SolverID ID of the solver to add
	 * @param InFrameData Reference to the frame data we want to add
	 */
	void AddFrameForSolver(const int32 SolverID, FChaosVDSolverFrameData&& InFrameData);

	/**
	 * Adds a Game Frame Data entry. Creates a solver entry if it does not exist 
	 * @param InFrameData Reference to the frame data we want to add
	 */
	void AddGameFrameData(FChaosVDGameFrameData&& InFrameData);

	/** Called each time the recording changes - Mainly when a new frame is added from the Trace analysis */
	FChaosVDRecordingUpdated& OnRecordingUpdated() { return RecordingUpdatedDelegate; };

	/** Called each time new geometry data becomes available in the recording - Mainly when a new frame is added from the Trace analysis */
	FChaosVDGeometryDataLoaded& OnGeometryDataLoaded() { return GeometryDataLoaded; };

	/**
	 * Searches for a recorded Game frame at the specified cycle 
	 * @param Cycle Platform Cycle to be used in the search
	 * @return A ptr to the recorded game frame data - This is a ptr to the array element. Do not store
	 */
	FChaosVDGameFrameData* GetGameFrameDataAtCycle(uint64 Cycle);

	/**
	 * Searches for a recorded Game frame at the specified cycle 
	 * @param FrameNumber Frame Number
	 * @return A ptr to the recorded game frame data - This is a ptr to the array element. Do not store
	 */
	FChaosVDGameFrameData* GetGameFrameData(int32 FrameNumber);

	/** Returns a ptr to the last recorded game frame - This is a ptr to the array element. Do not store */
	FChaosVDGameFrameData* GetLastGameFrameData();

	/**
	 * Searches and returns the lowest game frame number at the specified cycle
	 * @param Cycle Platform Cycle to be used in the search as lower bound
	 * @return Found Game frame number. INDEX_NONE if no frame is found for the specified cycle
	 */
	int32 GetLowestGameFrameNumberAtCycle(uint64 Cycle);

	/**
     * Gathers all available solvers IDs at the given Game frame number
     * @param FrameNumber Game Frame number to evaluate
     * @param OutSolversID Solver's ID array to be filled with any IDs found
     */
	void GetAvailableSolverIDsAtGameFrameNumber(int32 FrameNumber, TArray<int32>& OutSolversID);

	/** Returns a reference to the GeometryID-ImplicitObject map of this recording */
	const TMap<uint32, TSharedPtr<const Chaos::FImplicitObject>>& GetGeometryDataMap() const { return ImplicitObjects; };

	/** Adds a shared Implicit Object to the recording */
	void AddImplicitObject(const uint32 ID, const TSharedPtr<Chaos::FImplicitObject>& InImplicitObject);

	/** Session name of the trace session used to re-build this recording */
	FString SessionName;

protected:

	/** Adds an Implicit Object to the recording and takes ownership of it */
	void AddImplicitObject(const uint32 ID, const Chaos::FImplicitObject* InImplicitObject);
	
	void AddImplicitObject_Internal(const uint32 ID, const TSharedPtr<const Chaos::FImplicitObject>& InImplicitObject);

	/** Stores a frame number of a solver that is a Key Frame -
	 * These are used when scrubbing to make sure the visualization is in sync with what was recorded
	 */
	void AddKeyFrameNumberForSolver(int32 SolverID, int32 FrameNumber);
	
	TMap<int32, TArray<FChaosVDSolverFrameData>> RecordedFramesDataPerSolver;
	TMap<int32, TArray<int32>> RecordedKeyFramesNumberPerSolver;
	TArray<FChaosVDGameFrameData> GameFrames;
	FChaosVDRecordingUpdated RecordingUpdatedDelegate;
	FChaosVDGeometryDataLoaded GeometryDataLoaded;

	/** Id to Ptr map of all shared geometry data required to visualize */
	TMap<uint32, TSharedPtr<const Chaos::FImplicitObject>> ImplicitObjects;

	friend class FChaosVDTraceProvider;
	friend class FChaosVDTraceImplicitObjectProcessor;
};
