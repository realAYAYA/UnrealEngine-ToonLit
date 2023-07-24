// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ClothingSystemRuntimeTypes.h"
#include "Containers/Map.h"
#include "CoreTypes.h"
#include "Math/MathFwd.h"

class UClothingAssetBase;
class USkeletalMeshComponent;
class USkinnedMeshComponent;
struct FClothCollisionData;
struct FClothSimulData;

/** Empty interface, derived simulation modules define the contents of the context. */
class CLOTHINGSYSTEMRUNTIMEINTERFACE_API IClothingSimulationContext
{
public:
	IClothingSimulationContext();
	virtual ~IClothingSimulationContext();
};

/** Base class for clothing simulators. */
class CLOTHINGSYSTEMRUNTIMEINTERFACE_API IClothingSimulation
{
public:
	IClothingSimulation();
	virtual ~IClothingSimulation();

	// The majority of the API for this class is protected. The required objects (skel meshes and the parallel task)
	// are friends so they can use the functionality. For the most part the simulation is not designed to be used
	// outside of the skeletal mesh component as its parallel simulation is tied to the skel mesh tick and
	// dependents.
	// Any method that is available in the public section below should consider that it may be called while
	// the simulation is running.
	// Currently only a const pointer is exposed externally to the skeletal mesh component, so barring that
	// being cast away external callers can't call non-const methods. The debug skel mesh component exposes a
	// mutable version for editor use
protected:

	friend class USkeletalMeshComponent;
	friend class FParallelClothTask;

	/**
	 * Create an actor for this simulation from the data in InAsset
	 * Simulation data for this actor should be written back to SimDataIndex in GetSimulationData
	 * @param InOwnerComponent - The component requesting this actor
	 * @param InAsset - The asset to create an actor from
	 * @param SimDataIndex - the sim data index to use when doing the writeback in GetSimulationData
	 */
	virtual void CreateActor(USkeletalMeshComponent* InOwnerComponent, UClothingAssetBase* InAsset, int32 SimDataIndex) = 0;

	/** Do any required initialization after all the actors have been loaded */
	UE_DEPRECATED(4.26, "No longer required.")
	virtual void PostActorCreationInitialize() {}

	/** Create a new context, will not be filled, call FillContext before simulating with this context */
	virtual IClothingSimulationContext* CreateContext() = 0;

	/**
	 * Fills an existing context for a single simulation step, called by the engine on the game thread prior to simulation 
	 * @param InComponent - The component to fill the context for
	 * @param InOutContext - The context to fill
	 */
	virtual void FillContext(USkeletalMeshComponent* InComponent, float InDeltaTime, IClothingSimulationContext* InOutContext) = 0;

	/**
	 * Fills an existing context for a single simulation step, called by the engine on the game thread prior to simulation 
	 * @param InComponent - The component to fill the context for
	 * @param InOutContext - The context to fill
	 * @param bIsInitialization - Whether this fill is occurring as part of the actor creation stage
	 */
	virtual void FillContext(USkeletalMeshComponent* InComponent, float InDeltaTime, IClothingSimulationContext* InOutContext, bool bIsInitialization) = 0;

	/** Initialize the simulation, will be called before any Simulate calls */
	virtual void Initialize() = 0;

	/**
	 * Shutdown the simulation, this should clear ALL resources as we no longer expect to
	 * call any other function on this simulation without first calling Initialize again.
	 */
	virtual void Shutdown() = 0;

	/**
	 * Called by the engine to detect whether or not the simulation should run, essentially
	 * are there any actors that need to simulate in this simulation
	 */
	virtual bool ShouldSimulate() const = 0;

	/**
	 * Run a single tick of the simulation.
	 * The pointer InContext is guaranteed (when called by the engine) to be the context allocated in
	 * CreateContext and can be assumed to be safely castable to any derived type allocated there.
	 * New callers should make care to make sure only the correct context is ever passed through
	 * @param InContext - The context to use during simulation, will have been filled in FillContext
	 */
	virtual void Simulate(IClothingSimulationContext* InContext) = 0;

	/** Simulation should remove all of it's actors when next possible and free them */
	virtual void DestroyActors() = 0;

	/** 
	 * Destroy a context object, engine will always pass a context created using CreateContext 
	 * @param InContext - The context to destroy
	 */
	virtual void DestroyContext(IClothingSimulationContext* InContext) = 0;

	/** 
	 * Fill FClothSimulData map for the clothing simulation. Should fill a map pair per-actor 
	 * @param OutData - The simulation data to write to
	 * @param InOwnerComponent - the component that owns the simulation
	 * @param InOverrideComponent - An override component if bound to a leader pose component
	 */
	virtual void GetSimulationData(TMap<int32, FClothSimulData>& OutData, USkeletalMeshComponent* InOwnerComponent, USkinnedMeshComponent* InOverrideComponent) const = 0;

	/** Get the bounds of the simulation mesh in local simulation space */
	virtual FBoxSphereBounds GetBounds(const USkeletalMeshComponent* InOwnerComponent) const = 0;

	/**
	 * Called by the engine when an external object wants to inject collision data into this simulation above
	 * and beyond what is specified in the asset for the internal actors
	 * Examples: Scene collision, collision for parents we are attached to
	 * @param InData - Collisions to add to this simulation
	 */
	virtual void AddExternalCollisions(const FClothCollisionData& InData) = 0;

	/**
	 * Called by the engine when external collisions are no longer necessary or when they need to be updated
	 * with some of the previous collisions removed. It is recommended in derived simulations to avoid freeing
	 * any allocations regarding external collisions as it is likely more will be added soon after this call
	 */
	virtual void ClearExternalCollisions() = 0;

	/**
	 * Called by the engine to request data on all active collisions in a simulation. if bIncludeExternal is
	 * true, derived implementations should add the asset collisions and any collisions added at runtime, if
	 * false only the collisions from the asset should be considered
	 * @param OutCollisions - Array to write collisions to
	 * @param bIncludeExternal - Whether or not external collisions should be retrieved, or just asset collisions
	 */
	virtual void GetCollisions(FClothCollisionData& OutCollisions, bool bIncludeExternal = true) const = 0;

public:
	/**
	 * Set the number of iterations used by the solver.
	 * Implementation is not considered thread safe, and therefore this function must be called at a synchronization point.
	 */
	virtual void SetNumIterations(int32 /*NumIterations*/) {}
	/**
	 * Set the maximum number of iterations used by the solver.
	 * Implementation is not considered thread safe, and therefore this function must be called at a synchronization point.
	 */
	virtual void SetMaxNumIterations(int32 /*MaxNumIterations*/) {}
	/**
	 * Set the number of substeps used by the solver.
	 * Implementation is not considered thread safe, and therefore this function must be called at a synchronization point.
	 */
	virtual void SetNumSubsteps(int32 /*NumSubsteps*/) {}

	/**
	 * Called by the engine once per physics tick to gather non-cycle stats (num sim verts etc.)
	 * Will not be called if STATS is not defined, not required to be implemented if no need for stats.
	 * STAT_NumCloths and STAT_NumClothVerts are already defined and can be incremented, more can be
	 * added per simulation.
	 * This can be called on any thread, so derived implementations should make sure the gather is
	 * safe
	 */
	virtual void GatherStats() const {}

	/** Return the number of simulated cloths. Implementation must be thread safe. */
	virtual int32 GetNumCloths() const { return 0;  }
	/** Return the number of kinematic (fixed) particles. Implementation must be thread safe. */
	virtual int32 GetNumKinematicParticles() const { return 0; }
	/** Return the number of dynamic (simulated) particles. Implementation must be thread safe. */
	virtual int32 GetNumDynamicParticles() const { return 0; }
	/**
	 * Return the number of iterations used by the solver.
	 * This is the maximum used as an indicative value only, as this could vary between cloths.
	 * Implementation must be thread safe.
	 */
	virtual int32 GetNumIterations() const { return 0; }
	/**
	 * Return the number of substeps used by the solver.
	 * This is the maximum used as an indicative value only, as this could vary between cloths.
	 * Implementation must be thread safe.
	 */
	virtual int32 GetNumSubsteps() const { return 0; }
	/** Return the simulation time in ms. Implementation must be thread safe. */
	virtual float GetSimulationTime() const { return 0.f; }
	/** Return whether the simulation is teleported. Implementation must be thread safe. */
	virtual bool IsTeleported() const { return false; }

	/** Update the forces coming from the world. */
	virtual void UpdateWorldForces(const USkeletalMeshComponent* OwnerComponent) {};
};
