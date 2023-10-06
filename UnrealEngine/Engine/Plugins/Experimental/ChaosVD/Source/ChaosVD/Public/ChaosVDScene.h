// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDRecording.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "UObject/ObjectMacros.h"
#include "UObject/GCObject.h"

class FChaosVDGeometryBuilder;
class AChaosVDParticleActor;
class FReferenceCollector;
class UObject;
class UTypedElementSelectionSet;
class UWorld;

struct FTypedElementHandle;

typedef TMap<int32, AChaosVDParticleActor*> FChaosVDParticlesByIDMap;

DECLARE_MULTICAST_DELEGATE(FChaosVDSceneUpdatedDelegate)
DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDOnObjectSelectedDelegate, UObject*)

/** Recreates a UWorld from a recorded Chaos VD Frame */
class FChaosVDScene : public FGCObject , public TSharedFromThis<FChaosVDScene>
{
public:
	FChaosVDScene();
	virtual ~FChaosVDScene() override;

	void Initialize();
	void DeInitialize();

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FChaosVDScene");
	}

	/** Called each time this Scene is modified */
	FChaosVDSceneUpdatedDelegate& OnSceneUpdated() { return SceneUpdatedDelegate; }

	/** Updates, Adds and Remove actors to match the provided Step Data */
	void UpdateFromRecordedStepData(const int32 SolverID, const FString& SolverName, const FChaosVDStepData& InRecordedStepData, const FChaosVDSolverFrameData& InFrameData);

	void UpdateParticlesCollisionData(const FChaosVDStepData& InRecordedStepData, int32 SolverID);

	void HandleNewGeometryData(const TSharedPtr<const Chaos::FImplicitObject>&, const uint32 GeometryID) const;

	void HandleEnterNewGameFrame(int32 FrameNumber, const TArray<int32>& AvailableSolversIds);

	/** Deletes all actors of the Scene and underlying UWorld */
	void CleanUpScene();

	/** Returns the a ptr to the UWorld used to represent the current recorded frame data */
	UWorld* GetUnderlyingWorld() const { return PhysicsVDWorld; };

	bool IsInitialized() const { return  bIsInitialized; }

	const TSharedPtr<FChaosVDGeometryBuilder>& GetGeometryGenerator() { return  GeometryGenerator; }

	FChaosVDGeometryDataLoaded& OnNewGeometryAvailable(){ return NewGeometryAvailableDelegate; }

	const TSharedPtr<const Chaos::FImplicitObject>* GetUpdatedGeometry(int32 GeometryID) const;

	/** Adds an object to the selection set if it was not selected already, making it selected in practice */
	void SetSelectedObject(UObject* SelectedObject);

	/** Evaluates an object and returns true if it is selected */
	bool IsObjectSelected(const UObject* Object);

	/** Returns a ptr to the current selection set object */
	UTypedElementSelectionSet* GetElementSelectionSet() const { return SelectionSet; }
	
	/** Event triggered when an object is focused in the scene (double click in the scene outliner)*/
	FChaosVDOnObjectSelectedDelegate& OnObjectFocused() { return ObjectFocusedDelegate; }

	/** Returns a ptr to the particle actor representing the provided Particle ID
	 * @param SolverID ID of the solver owning the Particle
	 * @param ParticleID ID of the particle
	 */
	AChaosVDParticleActor* GetParticleActor(int32 SolverID, int32 ParticleID);

	TSharedPtr<FChaosVDRecording> LoadedRecording;

private:

	/** Creates an ChaosVDParticle actor for the Provided recorded Particle Data */
	AChaosVDParticleActor* SpawnParticleFromRecordedData(const FChaosVDParticleDataWrapper& InParticleData, const FChaosVDSolverFrameData& InFrameData);

	/** Returns the ID used to track this recorded particle data */
	int32 GetIDForRecordedParticleData(const FChaosVDParticleDataWrapper& InParticleData) const;

	/** Creates the instance of the World which will be used the recorded data*/
	UWorld* CreatePhysicsVDWorld() const;

	/** Map of ID-ChaosVDParticle Actor. Used to keep track of actor instances and be able to modify them as needed*/
	TMap<int32, FChaosVDParticlesByIDMap> ParticlesBySolverID;

	/** Returns the correct TypedElementHandle based on an object type so it can be used with the selection set object */
	FTypedElementHandle GetSelectionHandleForObject(const UObject* Object) const;

	/** Updates the render state of the hit proxies of an array of actors. This used to update the selection outline state */
	void UpdateSelectionProxiesForActors(const TArray<AActor*>& SelectedActors);

	void HandlePreSelectionChange(const UTypedElementSelectionSet* PreChangeSelectionSet);
	void HandlePostSelectionChange(const UTypedElementSelectionSet* PreChangeSelectionSet);

	void ClearSelectionAndNotify();

	/** UWorld instance used to represent the recorded debug data */
	TObjectPtr<UWorld> PhysicsVDWorld = nullptr;

	FChaosVDSceneUpdatedDelegate SceneUpdatedDelegate;

	TSharedPtr<FChaosVDGeometryBuilder> GeometryGenerator;

	FChaosVDGeometryDataLoaded NewGeometryAvailableDelegate;

	FChaosVDOnObjectSelectedDelegate ObjectFocusedDelegate;

	/** Selection set object holding the current selection state */
	TObjectPtr<UTypedElementSelectionSet> SelectionSet;

	/** Array of actors with hit proxies that need to be updated */
	TArray<AActor*> PendingActorsToUpdateSelectionProxy;

	bool bIsInitialized = false;
};
