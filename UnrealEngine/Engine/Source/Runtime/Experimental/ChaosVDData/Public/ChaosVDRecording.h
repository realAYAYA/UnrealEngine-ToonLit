// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/Core.h"
#include "Containers/UnrealString.h"
#include "DataWrappers/ChaosVDCollisionDataWrappers.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "UObject/ObjectMacros.h"
#include "Containers/UnrealString.h"
#include "Chaos/ImplicitFwd.h"
#include "Chaos/ImplicitObject.h"
#include "ChaosVisualDebugger/ChaosVDMemWriterReader.h"
#include "DataWrappers/ChaosVDJointDataWrappers.h"
#include "DataWrappers/ChaosVDQueryDataWrappers.h"
#include <atomic>

namespace Chaos::VisualDebugger
{
	class FChaosVDSerializableNameTable;
}

DECLARE_MULTICAST_DELEGATE_TwoParams(FChaosVDGeometryDataLoaded, const Chaos::FConstImplicitObjectPtr&, const uint32 GeometryID)

struct FChaosVDStepData
{
	FString StepName;
	TArray<TSharedPtr<FChaosVDParticleDataWrapper>> RecordedParticlesData;
	TArray<TSharedPtr<FChaosVDParticlePairMidPhase>> RecordedMidPhases;
	TArray<TSharedPtr<FChaosVDJointConstraint>> RecordedJointConstraints;
	TArray<FChaosVDConstraint> RecordedConstraints;
	TMap<int32, TArray<FChaosVDConstraint>> RecordedConstraintsByParticleID;
	TMap<int32, TArray<TSharedPtr<FChaosVDParticlePairMidPhase>>> RecordedMidPhasesByParticleID;
	TSet<int32> ParticlesDestroyedIDs;
};

struct FChaosVDTrackedLocation
{
	FString DebugName;
	FVector Location;
};

struct FChaosVDTrackedTransform
{
	FString DebugName;
	FTransform Transform;
};

typedef TArray<FChaosVDStepData, TInlineAllocator<16>> FChaosVDStepsContainer;

struct CHAOSVDDATA_API FChaosVDSolverFrameData
{
	FString DebugName;
	int32 SolverID = INDEX_NONE;
	uint64 FrameCycle = 0;
	Chaos::FRigidTransform3 SimulationTransform;
	bool bIsKeyFrame = false;
	bool bIsResimulated = false;
	FChaosVDStepsContainer SolverSteps;
	TSet<int32> ParticlesDestroyedIDs;
	double StartTime = -1.0;
	double EndTime = -1.0;

	/** Calculates and returns the frame time for this recorded frame.
	 * @return Calculated frame time. -1 if it was not recorded
	 */
	double GetFrameTime() const
	{
		if (StartTime < 0 || EndTime < 0)
		{
			return -1.0;
		}

		return EndTime - StartTime;
	}
};

struct FChaosVDGameFrameData
{
	uint64 FirstCycle;
	uint64 LastCycle;
	double StartTime = -1.0;
	double EndTime = -1.0;

	/** Calculates and returns the frame time for this recorded frame.
	 * @return Calculated frame time. -1 if it was not recorded
	 */
	double GetFrameTime() const
	{
		if (StartTime < 0 || EndTime < 0)
		{
			return -1.0;
		}

		return EndTime - StartTime;
	}

	TMap<FName, FChaosVDTrackedLocation> RecordedNonSolverLocationsByID;
	TMap<FName, FChaosVDTrackedTransform> RecordedNonSolverTransformsByID;
	TMap<int32, TSharedPtr<FChaosVDQueryDataWrapper>> RecordedSceneQueries;
};

/**
 * Struct that represents a recorded Physics simulation.
 * It is currently populated while analyzing a Trace session
 */
struct CHAOSVDDATA_API FChaosVDRecording
{
	FChaosVDRecording();

	/** Returns the current available recorded solvers number */
	int32 GetAvailableSolversNumber_AssumesLocked() const { return RecordedFramesDataPerSolver.Num(); }
	
	/** Returns the current available Game Frames */
	int32 GetAvailableGameFramesNumber() const;
	int32 GetAvailableGameFramesNumber_AssumesLocked() const;

	/** Returns a reference to the array holding all the available game frames */
	const TArray<FChaosVDGameFrameData>& GetAvailableGameFrames_AssumesLocked() const { return GameFrames; }

	/** Returns a reference to the map containing the available solver data */
	const TMap<int32, TArray<FChaosVDSolverFrameData>>& GetAvailableSolvers_AssumesLocked() const { return RecordedFramesDataPerSolver; }

	/**
	 * Returns the number of available frame data for the specified solver ID
	 * @param SolverID ID of the solver 
	 */
	int32 GetAvailableSolverFramesNumber(int32 SolverID) const;
	int32 GetAvailableSolverFramesNumber_AssumesLocked(int32 SolverID) const;
	
	/**
	 * Returns the name of the specified solver id
	 * @param SolverID ID of the solver 
	 */
	FString GetSolverName(int32 SolverID);

	/**
	 * Returns the name of the specified solver id. Must be called from within a ReadLock
	 * @param SolverID ID of the solver
	 */
	FString GetSolverName_AssumedLocked(int32 SolverID);

	/**
	 * Return a ptr to the existing solver frame data from the specified ID and Frame number
	 * @param SolverID ID of the solver
	 * @param FrameNumber Frame number
	 * @param bKeyFrameOnly True if we should return a keyframe (real or generated) for the provided frame number if available or nothing
	 * @return Ptr to the existing solver frame data from the specified ID and Frame number - It is a ptr to the array element, Do not store
	 */
	FChaosVDSolverFrameData* GetSolverFrameData_AssumesLocked(int32 SolverID, int32 FrameNumber, bool bKeyFrameOnly = false);
	
	/**
	 * Return a ptr to the existing solver frame data from the specified ID and Frame number
	 * @param SolverID ID if the solver
	 * @param Cycle Platform cycle at which the solver frame was recorded
	 * @return Ptr to the existing solver frame data from the specified ID and Frame number - It is a ptr to the array element, Do not store
	 */
	FChaosVDSolverFrameData* GetSolverFrameDataAtCycle_AssumesLocked(int32 SolverID, uint64 Cycle);

	/**
	 * Searches and returns the lowest frame number of a solver at the specified cycle
	 * @param SolverID ID if the solver
	 * @param Cycle Platform cycle to use as lower bound
	 * @return Found frame number. INDEX_NONE if no frame is found for the specified cycle
	 */
	int32 GetLowestSolverFrameNumberAtCycle(int32 SolverID, uint64 Cycle);
	int32 GetLowestSolverFrameNumberAtCycle_AssumesLocked(int32 SolverID, uint64 Cycle);

	int32 FindFirstSolverKeyFrameNumberFromFrame_AssumesLocked(int32 SolverID, int32 StartFrameNumber);
	
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
	void AddGameFrameData(const FChaosVDGameFrameData& InFrameData);

	/** Called each time new geometry data becomes available in the recording - Mainly when a new frame is added from the Trace analysis */
	FChaosVDGeometryDataLoaded& OnGeometryDataLoaded() { return GeometryDataLoaded; };

	/**
	 * Searches for a recorded Game frame at the specified cycle 
	 * @param Cycle Platform Cycle to be used in the search
	 * @return A ptr to the recorded game frame data - This is a ptr to the array element. Do not store
	 */
	FChaosVDGameFrameData* GetGameFrameDataAtCycle_AssumesLocked(uint64 Cycle);

	/**
	 * Searches for a recorded Game frame at the specified cycle 
	 * @param FrameNumber Frame Number
	 * @return A ptr to the recorded game frame data - This is a ptr to the array element. Do not store
	 */
	FChaosVDGameFrameData* GetGameFrameData_AssumesLocked(int32 FrameNumber);

	/** Returns a ptr to the last recorded game frame - This is a ptr to the array element. Do not store */
	FChaosVDGameFrameData* GetLastGameFrameData_AssumesLocked();

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
	void GetAvailableSolverIDsAtGameFrameNumber_AssumesLocked(int32 FrameNumber, TArray<int32>& OutSolversID);
	void GetAvailableSolverIDsAtGameFrame(const FChaosVDGameFrameData& GameFrameData, TArray<int32>& OutSolversID);
	void GetAvailableSolverIDsAtGameFrame_AssumesLocked(const FChaosVDGameFrameData& GameFrameData, TArray<int32>& OutSolversID);

	/** Collapses the most important frame data from a range of solver frames into a single solver frame data */
	void CollapseSolverFramesRange_AssumesLocked(int32 SolverID, int32 StartFrame, int32 EndFrame, FChaosVDSolverFrameData& OutCollapsedFrameData);

	/** Returns a reference to the GeometryID-ImplicitObject map of this recording */
	const TMap<uint32, Chaos::FConstImplicitObjectPtr>& GetGeometryMap() const { return ImplicitObjects; };

	UE_DEPRECATED(5.4, "Please use GetGeometryMap instead")
	const TMap<uint32, TSharedPtr<const Chaos::FImplicitObject>>& GetGeometryDataMap() const
	{
		check(false);
		static TMap<uint32, TSharedPtr<const Chaos::FImplicitObject>> DummyMap;
		return DummyMap;
	};

	/** Adds a shared Implicit Object to the recording */
	void AddImplicitObject(const uint32 ID, const Chaos::FImplicitObjectPtr& InImplicitObject);
	
	UE_DEPRECATED(5.4, "Please use AddImplicitObject with FImplicitObjectPtr instead")
	void AddImplicitObject(const uint32 ID, const TSharedPtr<Chaos::FImplicitObject>& InImplicitObject);

	/** Session name of the trace session used to re-build this recording */
	FString SessionName;

	FRWLock& GetRecordingDataLock() { return RecordingDataLock; }

	/** Returns true if this recording is being populated from a live session */
	bool IsLive() const { return bIsLive; }

	/** Sets if this recording is being populated from a live session */
	void SetIsLive(bool bNewIsLive) { bIsLive = bNewIsLive; }

	/** Returns the name table instances used to de-duplicate strings serialization */
	TSharedPtr<Chaos::VisualDebugger::FChaosVDSerializableNameTable> GetNameTableInstance() const { return NameTable; }

	/** Returns the FArchive header used to read the serialized binary data */
	const Chaos::VisualDebugger::FChaosVDArchiveHeader& GetHeaderData() const { return HeaderData; }
	
	/** Sets the FArchive header used to read the serialized binary data */
	void SetHeaderData(const Chaos::VisualDebugger::FChaosVDArchiveHeader& InNewHeader) { HeaderData = InNewHeader; }

	/** Returns true if this recording does not have any usable data */
	bool IsEmpty() const;

	/** Returns the last Platform Cycle on which this recording was updated (A new frame was added) */
	uint64 GetLastUpdatedTimeAsCycle() { return LastUpdatedTimeAsCycle; }

protected:

	/** Adds an Implicit Object to the recording and takes ownership of it */
	void AddImplicitObject(const uint32 ID, const Chaos::FImplicitObject* InImplicitObject);
	
	void AddImplicitObject_Internal(const uint32 ID, const Chaos::FConstImplicitObjectPtr& InImplicitObject);

	/** Stores a frame number of a solver that is a Key Frame -
	 * These are used when scrubbing to make sure the visualization is in sync with what was recorded
	 */
	void AddKeyFrameNumberForSolver(int32 SolverID, int32 FrameNumber);
	void AddKeyFrameNumberForSolver_AssumesLocked(int32 SolverID, int32 FrameNumber);
	void GenerateAndStoreKeyframeForSolver_AssumesLocked(int32 SolverID, int32 CurrentFrameNumber, int32 LastKeyFrameNumber);

	TMap<int32, TArray<FChaosVDSolverFrameData>> RecordedFramesDataPerSolver;
	TMap<int32, TMap<int32, FChaosVDSolverFrameData>> GeneratedKeyFrameDataPerSolver;
	TMap<int32, TArray<int32>> RecordedKeyFramesNumberPerSolver;
	TArray<FChaosVDGameFrameData> GameFrames;

	FChaosVDGeometryDataLoaded GeometryDataLoaded;

	/** Id to Ptr map of all shared geometry data required to visualize */
	TMap<uint32, Chaos::FConstImplicitObjectPtr> ImplicitObjects;

	TSharedPtr<Chaos::VisualDebugger::FChaosVDSerializableNameTable> NameTable;

	mutable FRWLock RecordingDataLock;

	/** True if this recording is being populated from a live session */
	bool bIsLive = false;

	/** Last Platform Cycle on which this recording was updated */
	std::atomic<uint64> LastUpdatedTimeAsCycle;

	/** Map that temporary holds generated particle data during the key frame generation process, keeping its memory allocation between generated frames*/
	TMap<int32, TSharedPtr<FChaosVDParticleDataWrapper>> ParticlesOnCurrentGeneratedKeyframe;

	Chaos::VisualDebugger::FChaosVDArchiveHeader HeaderData;

	friend class FChaosVDTraceProvider;
	friend class FChaosVDTraceImplicitObjectProcessor;
};
