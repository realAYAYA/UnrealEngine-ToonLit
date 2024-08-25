// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDRecording.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"

class AChaosVDSceneQueryDataContainer;
class AChaosVDSolverInfoActor;
class AChaosVDSceneCollisionContainer;
class UChaosVDEditorSettings;
class FChaosVDGeometryBuilder;
class AChaosVDParticleActor;
class FReferenceCollector;
class UChaosVDSceneQueryDataComponent;
class UObject;
class UMaterial;
class USelection;
class UTypedElementSelectionSet;
class UWorld;

struct FTypedElementHandle;

typedef TMap<int32, AChaosVDSolverInfoActor*> FChaosVDSolverInfoByIDMap;

DECLARE_MULTICAST_DELEGATE(FChaosVDSceneUpdatedDelegate)
DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDActorActiveStateUpdateDelegate, AChaosVDParticleActor*)
DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDOnObjectSelectedDelegate, UObject*)
DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDSolverInfoActorCreatedDelegate, AChaosVDSolverInfoActor*)

DECLARE_MULTICAST_DELEGATE_TwoParams(FChaosVDSolverVisibilityChangedDelegate, int32 SolverID, bool bNewVisibility)

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
	void UpdateFromRecordedStepData(const int32 SolverID, const FChaosVDStepData& InRecordedStepData, const FChaosVDSolverFrameData& InFrameData);

	void UpdateParticlesCollisionData(const FChaosVDStepData& InRecordedStepData, int32 SolverID);
	void UpdateJointConstraintsData(const FChaosVDStepData& InRecordedStepData, int32 SolverID);

	// No need to deprecate the old version since it is not a public API nor inline 
	void HandleNewGeometryData(const Chaos::FConstImplicitObjectPtr& Geometry, const uint32 GeometryID) const;

	void HandleEnterNewGameFrame(int32 FrameNumber, const TArray<int32>& AvailableSolversIds, const FChaosVDGameFrameData& InNewGameFrameData);

	/** Deletes all actors of the Scene and underlying UWorld */
	void CleanUpScene();

	/** Returns the a ptr to the UWorld used to represent the current recorded frame data */
	UWorld* GetUnderlyingWorld() const { return PhysicsVDWorld; };

	bool IsInitialized() const { return  bIsInitialized; }

	const TSharedPtr<FChaosVDGeometryBuilder>& GetGeometryGenerator() { return  GeometryGenerator; }

	FChaosVDGeometryDataLoaded& OnNewGeometryAvailable(){ return NewGeometryAvailableDelegate; }

	// No need to deprecate the old version since it is not a public API nor inline 
	Chaos::FConstImplicitObjectPtr GetUpdatedGeometry(int32 GeometryID) const;
	
	/** Adds an object to the selection set if it was not selected already, making it selected in practice */
	void SetSelectedObject(UObject* SelectedObject);

	/** Evaluates an object and returns true if it is selected */
	bool IsObjectSelected(const UObject* Object);

	/** Returns a ptr to the current selection set object */
	UTypedElementSelectionSet* GetElementSelectionSet() const { return SelectionSet; }
	
	USelection* GetActorSelectionObject() const { return ActorSelection; }
	USelection* GetComponentsSelectionObject() const { return ComponentSelection; }
	USelection* GetObjectsSelectionObject() const { return ObjectSelection; }
	
	/** Event triggered when an object is focused in the scene (double click in the scene outliner)*/
	FChaosVDOnObjectSelectedDelegate& OnObjectFocused() { return ObjectFocusedDelegate; }

	/** Returns a ptr to the particle actor representing the provided Particle ID
	 * @param SolverID ID of the solver owning the Particle
	 * @param ParticleID ID of the particle
	 */
	AChaosVDParticleActor* GetParticleActor(int32 SolverID, int32 ParticleID);

	const FChaosVDSolverInfoByIDMap& GetSolverInfoActorsMap() { return SolverDataContainerBySolverID; }
	AChaosVDSolverInfoActor* GetSolverInfoActor(int32 SolverID);

	/** Is the specified solver from a Server or a Client? (note: currently inferred from the solver name) */
	bool IsSolverForServer(int32 Solver) const;

	AActor* GetSkySphereActor() const { return SkySphere; }

	AActor* GetMeshComponentsContainerActor() const { return MeshComponentContainerActor; }

	UChaosVDSceneQueryDataComponent* GetSceneQueryDataContainerComponent() const;

	FChaosVDActorActiveStateUpdateDelegate& OnActorActiveStateChanged() { return ParticleActorUpdateDelegate; }

	FChaosVDSolverInfoActorCreatedDelegate& OnSolverInfoActorCreated() { return SolverInfoActorCreatedDelegate; }

	FChaosVDSolverVisibilityChangedDelegate& OnSolverVisibilityUpdated() { return SolverVisibilityChangedDelegate; }

	TSharedPtr<FChaosVDRecording> LoadedRecording;

private:

	/** Creates an ChaosVDParticle actor for the Provided recorded Particle Data */
	AChaosVDParticleActor* SpawnParticleFromRecordedData(const TSharedPtr<FChaosVDParticleDataWrapper>& InParticleData, const FChaosVDSolverFrameData& InFrameData);

	/** Returns the ID used to track this recorded particle data */
	int32 GetIDForRecordedParticleData(const TSharedPtr<FChaosVDParticleDataWrapper>& InParticleData) const;

	void CreateBaseLights(UWorld* TargetWorld) const;

	void CreateSceneQueriesContainer(UWorld* TargetWorld);

	/** Creates an actor that will contain all solver data for the provided Solver ID*/
	void CreateSolverInfoActor(int32 SolverID);

	AActor* CreateMeshComponentsContainer(UWorld* TargetWorld);

	/** Creates the instance of the World which will be used the recorded data*/
	UWorld* CreatePhysicsVDWorld();

	/** Map of SolverID-ChaosVDSolverInfo Actor. Used to keep track of active solvers representations and be able to modify them as needed*/
	FChaosVDSolverInfoByIDMap SolverDataContainerBySolverID;

	/** Returns the correct TypedElementHandle based on an object type so it can be used with the selection set object */
	FTypedElementHandle GetSelectionHandleForObject(const UObject* Object) const;

	/** Updates the render state of the hit proxies of an array of actors. This used to update the selection outline state */
	void UpdateSelectionProxiesForActors(const TArray<AActor*>& SelectedActors);

	void HandlePreSelectionChange(const UTypedElementSelectionSet* PreChangeSelectionSet);
	void HandlePostSelectionChange(const UTypedElementSelectionSet* PreChangeSelectionSet);

	void ClearSelectionAndNotify();

	void HandleVisibilitySettingsChanged(UChaosVDEditorSettings* SettingsObject);
	void HandleColorSettingsChanged(UChaosVDEditorSettings* SettingsObject);

	void InitializeSelectionSets();
	void DeInitializeSelectionSets();

	void HandleActorDestroyed(AActor* ActorDestroyed);

	/** UWorld instance used to represent the recorded debug data */
	TObjectPtr<UWorld> PhysicsVDWorld = nullptr;

	FChaosVDSceneUpdatedDelegate SceneUpdatedDelegate;

	TSharedPtr<FChaosVDGeometryBuilder> GeometryGenerator;

	FChaosVDGeometryDataLoaded NewGeometryAvailableDelegate;

	FChaosVDOnObjectSelectedDelegate ObjectFocusedDelegate;

	/** Selection set object holding the current selection state */
	TObjectPtr<UTypedElementSelectionSet> SelectionSet;

	TObjectPtr<USelection> ActorSelection = nullptr;
	TObjectPtr<USelection> ComponentSelection = nullptr;
	TObjectPtr<USelection> ObjectSelection = nullptr;

	/** Array of actors with hit proxies that need to be updated */
	TArray<AActor*> PendingActorsToUpdateSelectionProxy;

	/** Scene Streamable manager that we'll use to async load any assets we depend on */
	TSharedPtr<struct FStreamableManager> StreamableManager;

	mutable AActor* SkySphere = nullptr;

	AActor* MeshComponentContainerActor = nullptr;
	AChaosVDSceneQueryDataContainer* SceneQueriesContainer = nullptr;

	bool bIsInitialized = false;

	FChaosVDActorActiveStateUpdateDelegate ParticleActorUpdateDelegate;

	FDelegateHandle ActorDestroyedHandle;

	FChaosVDSolverInfoActorCreatedDelegate SolverInfoActorCreatedDelegate;

	FChaosVDSolverVisibilityChangedDelegate SolverVisibilityChangedDelegate;
};
