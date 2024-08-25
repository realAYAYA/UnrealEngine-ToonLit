// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "CoreMinimal.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "Components/ActorComponent.h"
#include "MovementMode.h"
#include "MoverTypes.h"
#include "LayeredMove.h"
#include "MoveLibrary/BasedMovementUtils.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "Engine/HitResult.h"
#endif // UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_4
#include "Backends/MoverBackendLiaison.h"
#include "UObject/WeakInterfacePtr.h"
#include "MoverComponent.generated.h"

struct FMoverTimeStep;
class UMovementModeStateMachine;
class UMovementMixer;

namespace MoverComponentConstants
{
	extern const FVector DefaultGravityAccel;		// Fallback gravity if not determined by the component or world (cm/s^2)
	extern const FVector DefaultUpDir;				// Fallback up direction if not determined by the component or world (normalized)
}


// Fired just before a simulation tick, regardless of being a re-simulated frame or not.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMover_OnPreSimTick, const FMoverTimeStep&, TimeStep, const FMoverInputCmdContext&, InputCmd);

// Fired during a simulation tick, after movement has occurred but before the state is finalized, allowing changes to the output state.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FMover_OnPostMovement, const FMoverTimeStep&, TimeStep, FMoverSyncState&, SyncState, FMoverAuxStateContext&, AuxState);

// Fired after a simulation tick, regardless of being a re-simulated frame or not.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMover_OnPostSimTick, const FMoverTimeStep&, TimeStep);

// Fired after a rollback. First param is the time step we've rolled back to. Second param is when we rolled back from, and represents a later frame that is no longer valid.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMover_OnPostSimRollback, const FMoverTimeStep&, CurrentTimeStep, const FMoverTimeStep&, ExpungedTimeStep);

// Fired after changing movement modes. First param is the name of the previous movement mode. Second is the name of the new movement mode. 
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMover_OnMovementModeChanged, const FName&, PreviousMovementModeName, const FName&, NewMovementModeName);

/**
 * 
 */
UCLASS(BlueprintType, meta = (BlueprintSpawnableComponent))
class MOVER_API UMoverComponent : public UActorComponent
{
	GENERATED_BODY()


public:	
	UMoverComponent();

	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual void OnRegister() override;
	virtual void RegisterComponentTickFunctions(bool bRegister) override;
	virtual void PostLoad() override;
	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	
	// Broadcast before each simulation tick
	UPROPERTY(BlueprintAssignable, Category = Mover)
	FMover_OnPreSimTick OnPreSimulationTick;

	// Broadcast at the end of a simulation tick after movement has occurred, but allowing additions/modifications to the state. Not assignable as a BP event due to it having mutable parameters.
	UPROPERTY()
	FMover_OnPostMovement OnPostMovement;

	// Broadcast after each simulation tick and the state is finalized
	UPROPERTY(BlueprintAssignable, Category = Mover)
	FMover_OnPostSimTick OnPostSimulationTick;

	// Broadcast when a rollback has occurred, just before the next simulation tick occurs
	UPROPERTY(BlueprintAssignable, Category = Mover)
	FMover_OnPostSimRollback OnPostSimulationRollback;

	// Broadcast when a MovementMode has changed. Happens during a simulation tick if the mode changed that tick or when SetModeImmediately is used to change modes.
	UPROPERTY(BlueprintAssignable, Category = Mover)
	FMover_OnMovementModeChanged OnMovementModeChanged;

	// Callbacks
	UFUNCTION()
	virtual void OnBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* Other, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) { }

	// --------------------------------------------------------------------------------
	// NP Driver
	// --------------------------------------------------------------------------------

	// Get latest local input prior to simulation step. Called by Network Prediction system on owner's instance (autonomous or authority).
	void ProduceInput(const int32 DeltaTimeMS, FMoverInputCmdContext* Cmd);

	// Restore a previous frame prior to resimulating. Called by Network Prediction system.
	void RestoreFrame(const FMoverSyncState* SyncState, const FMoverAuxStateContext* AuxState);

	// Take output for simulation. Called by Network Prediction system.
	void FinalizeFrame(const FMoverSyncState* SyncState, const FMoverAuxStateContext* AuxState);

	// Seed initial values based on component's state. Called by Network Prediction system.
	void InitializeSimulationState(FMoverSyncState* OutSync, FMoverAuxStateContext* OutAux);

	// Primary movement simulation update. Given an starting state and timestep, produce a new state. Called by Network Prediction system.
	void SimulationTick(const FMoverTimeStep& InTimeStep, const FMoverTickStartData& SimInput, OUT FMoverTickEndData& SimOutput);

	// Specifies which supporting back end class should drive this Mover actor
	UPROPERTY(EditDefaultsOnly, Category = Mover, meta = (MustImplement = "/Script/Mover.MoverBackendLiaisonInterface"))
	TSubclassOf<UActorComponent> BackendClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = Mover, meta=(FullyExpand=true))
	TMap<FName, TObjectPtr<UBaseMovementMode>> MovementModes;

	// Name of the first mode to start in when simulation begins. Must have a mapping in MovementModes. Only used during initialization.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Mover, meta=(GetOptions=GetStartingMovementModeNames))
	FName StartingMovementMode = NAME_None;

	// Transition checks that are always evaluated regardless of mode. Evaluated in order, stopping at the first successful transition check. Mode-owned transitions take precedence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced, Category=Mover)
	TArray<TObjectPtr<UBaseMovementModeTransition>> Transitions;

	/** List of types that should always be present in this actor's sync state */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=Mover)
	TArray<FMoverDataPersistence> PersistentSyncStateDataTypes;

	/** Optional object for producing input cmds. Typically set at BeginPlay time. If not specified, defaulted input will be used. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Mover, meta = (MustImplement = "/Script/Mover.MoverInputProducerInterface"))
	TObjectPtr<UObject> InputProducer;

	/** Optional object for mixing proposed moves.Typically set at BeginPlay time. If not specified, UDefaultMovementMixer will be used. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Mover)
	TObjectPtr<UMovementMixer> MovementMixer;
	
	/**
	 * Queue a layered move to start during the next simulation frame. This will clone whatever move you pass in, so you'll need to fully set it up before queuing.
	 * @param LayeredMove			The move to queue, which must be a LayeredMoveBase sub-type. 
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = Mover, meta = (CustomStructureParam = "MoveAsRawData", AllowAbstract = "false", DisplayName = "Queue Layered Move"))
	void K2_QueueLayeredMove(UPARAM(DisplayName="Layered Move") const int32& MoveAsRawData);
	DECLARE_FUNCTION(execK2_QueueLayeredMove);

	// Queue a layered move to start during the next simulation frame
	void QueueLayeredMove(TSharedPtr<FLayeredMoveBase> Move);
	
	// Queue a movement mode change to occur during the next simulation frame. If bShouldReenter is true, then a mode change will occur even if already in that mode.
	UFUNCTION(BlueprintCallable, Category = Mover)
	void QueueNextMode(FName DesiredModeName, bool bShouldReenter=false);

	// Add a movement mode to available movement modes. Returns true if the movement mode was added successfully. Returns the mode that was made.
	UFUNCTION(BlueprintCallable, Category = Mover, meta=(DeterminesOutputType="MovementMode"))
	UBaseMovementMode* AddMovementModeFromClass(FName ModeName, UPARAM(meta = (AllowAbstract = "false"))TSubclassOf<UBaseMovementMode> MovementMode);

	// Add a movement mode to available movement modes. Returns true if the movement mode was added successfully
	UFUNCTION(BlueprintCallable, Category = Mover)
	bool AddMovementModeFromObject(FName ModeName, UBaseMovementMode* MovementMode);
	
	// Removes a movement mode from available movement modes. Returns number of modes removed from the available movement modes.
	UFUNCTION(BlueprintCallable, Category = Mover)
	bool RemoveMovementMode(FName ModeName);
	
public:
	// Set gravity override, as a directional acceleration in worldspace.  Gravity on Earth would be {x=0,y=0,z=-980}
	UFUNCTION(BlueprintCallable, Category = Mover)
	void SetGravityOverride(bool bOverrideGravity, FVector GravityAcceleration=FVector::ZeroVector);
	
	// Get the current acceleration due to gravity (cm/s^2) in worldspace
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = Mover)
	FVector GetGravityAcceleration() const;

	// Get the normalized direction considered "up" in worldspace. Typically aligned with gravity, and typically determines the plane an actor tries to move along.
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = Mover)
	FVector GetUpDirection() const;

public:	// Queries

	// Get the transform of the root component that our Mover simulation is moving
	FTransform GetUpdatedComponentTransform() const;

	// Access the primary visual component of the actor
	USceneComponent* GetPrimaryVisualComponent() const;

	// Typed accessor to primary visual component
	template<class T>
	T* GetPrimaryVisualComponent() const
	{
		return Cast<T>(GetPrimaryVisualComponent());
	}

	// Get the current velocity (units per second, worldspace)
	UFUNCTION(BlueprintPure, Category = Mover)
	FVector GetVelocity() const;

	// Get the intended movement direction in worldspace with magnitude (range 0-1)
	UFUNCTION(BlueprintPure, Category = Mover)
	FVector GetMovementIntent() const;

	// Get the orientation that the actor is moving towards
	UFUNCTION(BlueprintPure, Category = Mover)
	FRotator GetTargetOrientation() const;

	/** Get a sampling of where the actor is projected to be in the future, based on a current state. Note that this is projecting ideal movement without doing full simulation and collision. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = Mover)
	TArray<FTrajectorySampleInfo> GetFutureTrajectory(float FutureSeconds, float SamplesPerSecond) const;

	// Get the current movement mode name
	UFUNCTION(BlueprintPure, Category = Mover)
	FName GetMovementModeName() const;

	// Get the current movement base. Null if there isn't one.
	UFUNCTION(BlueprintPure, Category = Mover)
	UPrimitiveComponent* GetMovementBase() const;

	// Get the current movement base bone, NAME_None if there isn't one.
	UFUNCTION(BlueprintPure, Category = Mover)
	FName GetMovementBaseBoneName() const;

	// Signals whether we have a sync state saved yet. If not, most queries will not be meaningful.
	UFUNCTION(BlueprintPure, Category = Mover)
	bool HasValidCachedState() const;

	// Access the most recent captured sync state. Check @HasValidCachedState first.
	UFUNCTION(BlueprintPure, Category = Mover)
	const FMoverSyncState& GetSyncState() const;

	// Signals whether we have input data saved yet. If not, input queries will not be meaningful.
	UFUNCTION(BlueprintPure, Category = Mover)
	bool HasValidCachedInputCmd() const;

	// Access the most recently-used inputs. Check @HasValidCachedInputCmd first.
	UFUNCTION(BlueprintPure, Category = Mover)
	const FMoverInputCmdContext& GetLastInputCmd() const;

	// Access the most recent floor check hit result.
	UFUNCTION(BlueprintPure, Category = Mover)
	bool TryGetFloorCheckHitResult(FHitResult& OutHitResult) const;

	// Access the read-only version of the Mover's Blackboard
	UFUNCTION(BlueprintPure, Category=Mover)
	const UMoverBlackboard* GetSimBlackboard() const;

	UMoverBlackboard* GetSimBlackboard_Mutable() const;

	/** Find settings object by type. Returns null if there is none of that type */
	const IMovementSettingsInterface* FindSharedSettings(const UClass* ByType) const { return FindSharedSettings_Mutable(ByType); }
	template<class T>
	const T* FindSharedSettings() const { return Cast<const T>(FindSharedSettings(T::StaticClass())); }

	/** Find mutable settings object by type. Returns null if there is none of that type */
	IMovementSettingsInterface* FindSharedSettings_Mutable(const UClass* ByType) const;
	template<class T>
	T* FindSharedSettings_Mutable() const { return Cast<T>(FindSharedSettings_Mutable(T::StaticClass())); }

	/** Find mutable settings object by type. Returns null if there is none of that type */
	UFUNCTION(BlueprintPure, Category = Mover,  meta=(DeterminesOutputType="SharedSetting", DisplayName="Find Shared Settings Mutable"))
	UObject* FindSharedSettings_Mutable_BP(UPARAM(meta = (MustImplement = "MovementSettingsInterface")) TSubclassOf<UObject> SharedSetting) const;

	/** Find settings object by type. Returns null if there is none of that type */
	UFUNCTION(BlueprintPure, Category = Mover,  meta=(DeterminesOutputType="SharedSetting", DisplayName="Find Shared Settings"))
	const UObject* FindSharedSettings_BP(UPARAM(meta = (MustImplement = "MovementSettingsInterface")) TSubclassOf<UObject> SharedSetting) const;

	/** Find movement mode by type. Returns null if there is none of that type */
	UBaseMovementMode* FindMode_Mutable(const UClass* ByType, bool bRequireExactClass=false) const;
	template<class T>
	T* FindMode_Mutable(bool bRequireExactClass=false) const { return Cast<T>(FindMode_Mutable(T::StaticClass(), bRequireExactClass)); }

	UFUNCTION(BlueprintPure, Category = Mover,  meta=(DeterminesOutputType="MovementMode"))
	UBaseMovementMode* FindMovementMode(TSubclassOf<UBaseMovementMode> MovementMode) const;

	/**
	 * Retrieves an active layered move, by writing to a target instance if it is the matching type. Note: Writing to the struct returned will not modify the active struct.
	 * @param DidSucceed			Flag indicating whether data was actually written to target struct instance
	 * @param TargetAsRawBytes		The data struct instance to write to, which must be a FLayeredMoveBase sub-type
	 */
	UFUNCTION(BlueprintCallable, CustomThunk, Category = Mover, meta = (CustomStructureParam = "TargetAsRawBytes", AllowAbstract = "false", DisplayName = "Find Active Layered Move"))
	void K2_FindActiveLayeredMove(bool& DidSucceed, UPARAM(DisplayName = "Out Layered Move") int32& TargetAsRawBytes) const;
	DECLARE_FUNCTION(execK2_FindActiveLayeredMove);

	// Find an active layered move by type. Returns null if one wasn't found 
	const FLayeredMoveBase* FindActiveLayeredMoveByType(const UScriptStruct* DataStructType) const;

	/** Find a layered move of a specific type in this components active layered moves. If not found, null will be returned. */
	template <typename T>
	const T* FindActiveLayeredMoveByType() const
	{
		if (const FLayeredMoveBase* FoundData = FindActiveLayeredMoveByType(T::StaticStruct()))
		{
			return static_cast<const T*>(FoundData);
		}

		return nullptr;
	}
	
protected:

	/** Makes this component and owner actor reflect the state of a particular frame snapshot. This occurs after simulation ticking, as well as during a rollback before we resimulate forward.
	  @param bRebaseBasedState	If true and the state was using based movement, it will use the current game world base pos/rot instead of the captured one. This is necessary during rollbacks.
	*/
	void SetFrameStateFromContext(const FMoverSyncState* SyncState, const FMoverAuxStateContext* AuxState, bool bRebaseBasedState);

public:
	bool InitMoverSimulation();

	/** Handle a blocking impact.*/
	UFUNCTION(BlueprintCallable, Category = Mover)
	void HandleImpact(FMoverOnImpactParams& ImpactParams);

protected:
	// Basic "Update Component/Ticking"
	void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);
	void UpdateTickRegistration();

	/** Called when a rollback occurs, after the simulation state has been restored */
	void OnSimulationRollback(const FMoverSyncState* SyncState, const FMoverAuxStateContext* AuxState);

	void ProcessFirstSimTickAfterRollback(const FMoverTimeStep& TimeStep);

	#if WITH_EDITOR
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
	virtual void PostCDOCompiled(const FPostCDOCompiledContext& Context) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
	bool ValidateSetup(class FDataValidationContext& ValidationErrors) const;
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;

	UFUNCTION()
	TArray<FString> GetStartingMovementModeNames();
	#endif // WITH_EDITOR

	UFUNCTION()
	virtual void PhysicsVolumeChanged(class APhysicsVolume* NewVolume);

	virtual void OnHandleImpact(const FMoverOnImpactParams& ImpactParams);

	/** internal function to perform post-sim scheduling to optionally support simple based movement */
	void UpdateBasedMovementScheduling(const FMoverTickEndData& SimOutput);

	TObjectPtr<UPrimitiveComponent> MovementBaseDependency;	// used internally for based movement scheduling management
	
	/** internal function to ensure SharedSettings array matches what's needed by the list of Movement Modes */
	void RefreshSharedSettings();

	/** This is the component that's actually being moved. Typically it is the Actor's root component and often a collidable primitive. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> UpdatedComponent = nullptr;

	/** UpdatedComponent, cast as a UPrimitiveComponent. May be invalid if UpdatedComponent was null or not a UPrimitiveComponent. */
	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> UpdatedCompAsPrimitive = nullptr;

	/** The main visual component associated with this Mover actor, typically a mesh and typically parented to the UpdatedComponent. */
	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> PrimaryVisualComponent;

	bool bHasValidLastProducedInput = false;
	FMoverInputCmdContext CachedLastProducedInputCmd;

	bool bHasValidCachedUsedInput = false;
	FMoverInputCmdContext CachedLastUsedInputCmd;
	
	bool bHasValidCachedState = false;
	FMoverSyncState CachedLastSyncState;
	FMoverAuxStateContext CachedLastAuxState;

	FMoverTimeStep CachedLastSimTickTimeStep;	// Saved timestep info from our last simulation tick, used during rollback handling. This will rewind during corrections.
	FMoverTimeStep CachedNewestSimTickTimeStep;	// Saved timestep info from the newest (farthest-advanced) simulation tick. This will not rewind during corrections.

	TWeakInterfacePtr<IMoverBackendLiaisonInterface> BackendLiaisonComp;

	/** Tick function that may be called anytime after this actor's movement step, useful as a way to support based movement on objects that are not */
	FMoverDynamicBasedMovementTickFunction BasedMovementTickFunction;

private:
	/** Collection of settings objects that are shared between movement modes. This list is automatically managed based on the @MovementModes contents. */
	UPROPERTY(EditDefaultsOnly, EditFixedSize, Instanced, Category = Mover, meta = (NoResetToDefault, MustImplement = "/Script/Mover.MovementSettingsInterface"))
	TArray<TObjectPtr<UObject>> SharedSettings;

	// Whether or not gravity is overridden on this actor. Otherwise, fall back on world settings. See @SetGravityOverride
	UPROPERTY(EditDefaultsOnly, Category="Mover|Gravity")
	bool bHasGravityOverride = false;
	
	// cm/s^2, only meaningful if @bHasGravityOverride is enabled. Set @SetGravityOverride
	UPROPERTY(EditDefaultsOnly, Category="Mover|Gravity", meta=(ForceUnits = "cm/s^2"))
	FVector GravityAccelOverride;

	/** If enabled, this actor will be moved to follow a base actor that it's standing on. Typically disabled for physics-based movement, which handles based movement internally. */
	UPROPERTY(EditDefaultsOnly, Category = "Mover")
	bool bSupportsKinematicBasedMovement = true;

	/** Transient flag indicating whether we are executing OnRegister(). */
	bool bInOnRegister = false;

	/** Transient flag indicating whether we are executing InitializeComponent(). */
	bool bInInitializeComponent = false;

	// Transient flag indicating we've had a rollback and haven't started simulating forward again yet
	bool bHasRolledBack = false;

	UPROPERTY(Transient)
	TObjectPtr<UMovementModeStateMachine> ModeFSM;	// JAH TODO: Also consider allowing a type property on the component to allow an alternative machine implementation to be allocated/used

	/** Used to store cached data & computations between decoupled systems, that can be referenced by name */
	UPROPERTY(Transient)
	TObjectPtr<UMoverBlackboard> SimBlackboard;


	friend class UBaseMovementMode;
	friend class UMoverNetworkPhysicsLiaisonComponent;
	friend class UMoverDebugComponent;
	friend class UBasedMovementUtils;
};
