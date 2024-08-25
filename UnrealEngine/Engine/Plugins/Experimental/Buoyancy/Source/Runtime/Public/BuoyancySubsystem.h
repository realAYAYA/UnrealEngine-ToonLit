// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/WorldSubsystem.h"
#include "Chaos/SimCallbackObject.h"
#include "Chaos/Framework/PhysicsProxyBase.h"
#include "Chaos/PBDRigidsEvolutionFwd.h"
#include "PBDRigidsSolver.h"
#include "Engine/EngineBaseTypes.h"
#include "WaterBodyComponent.h"
#include "BuoyancyWaterSplineData.h"
#include "BuoyancyWaterSplineKeyCacheGrid.h"
#include "BuoyancyEventFlags.h"
#include "ChaosUserDataPT.h"
#include "PhysicsProxy/SingleParticlePhysicsProxyFwd.h"
#include "BuoyancySubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBuoyancySubsystem, Log, All);

//
// Callback object for keeping water splines up to date on the physics thread
//
// NOTE: We use shared ptr here because a single water spline might have many
// particles associated with it, but we'd like to only store a single copy
// of the spline.
//

class FBuoyancyWaterSplineDataManager : public Chaos::TUserDataManagerPT< TSharedPtr<FBuoyancyWaterSplineData> > { };

namespace Chaos
{
	class FMidPhaseModifierAccessor;
	class FMidPhaseModifier;
}


//
// Buoyancy Settings
//

struct FBuoyancySettings
{
	// Force buoyant particles which are in water to stay awake
	bool bKeepAwake = false;

	// Density of water is about 1g/cm^3
	// Source: https://en.wikipedia.org/wiki/Properties_of_water
	float WaterDensity = 0.0001f; // kg/cm^3

	float MaxDeltaV = 200.f; // cm/s

	float MaxDeltaW = 2.f; // rad/s

	float WaterDrag = 1.f; // unitless

	int32 MaxNumBoundsSubdivisions = 2;

	float MinBoundsSubdivisionVol = FMath::Pow(125.f, 3.f); // 1m^3

	ECollisionChannel WaterCollisionChannel = ECollisionChannel::ECC_MAX;

	uint8 SurfaceTouchCallbackFlags = EBuoyancyEventFlags::None;

	float MinVelocityForSurfaceTouchCallback = 10.f;

	bool bSplineKeyCacheGrid = true;

	float SplineKeyCacheGridSize = 300.f;

	uint32 SplineKeyCacheLimit = 256;
};


//
// Buoyancy Subsystem
//

UCLASS()
class BUOYANCY_API UBuoyancySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

		UBuoyancySubsystem()
		: UTickableWorldSubsystem()
		, bWaterObjectsChanged(false)
		, bBuoyancySettingsChanged(false)
		, BuoyancySettings(FBuoyancySettings())
		, SplineData(nullptr)
		, SimCallback(nullptr)
		, NetMode(ENetMode::NM_MAX)
	{ }

public:

	// Return true if enable/disable was successful, or if
	// we were already in the target state.
	bool SetEnabled(const bool bEnabled);

	// Return true if subsystem is enabled and running
	UFUNCTION()
	bool IsEnabled() const;

	bool SetEnabledWithUpdatedNetModeCallback(const bool bEnabled);

protected:

	// UTickableWorldSubsystem begin interface
	virtual void PostInitialize() override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	// UTickableWorldSubsystem end interface

	// FWaterBodyManager delegate begin callbacks
	void OnWaterBodyAdded(UWaterBodyComponent* WaterBodyComponent);
	void OnWaterBodyRemoved(UWaterBodyComponent* WaterBodyComponent);
	// FWaterBodyManager delegate end callbacks

private:

	void CreateSimCallback();
	void DestroySimCallback();

	// Self explanatory function name :D
	void UpdateAllAsyncInputs();

	// Get netmode from world and send it to PT
	void UpdateNetMode();

	// Update PT spline data structs for each waterbody in the map
	void UpdateSplineData();

	// Put updated settings struct onto async input to be sent to sim callback
	void UpdateBuoyancySettings();

	// Process async outputs which hold data for triggering callbacks
	void ProcessSurfaceTouchCallbacks();

	Chaos::FPhysicsSolver* GetSolver() const;

	// When water plugin settings change, this callback will apply changes
	void ApplyRuntimeSettings(const class UBuoyancyRuntimeSettings* InSettings, EPropertyChangeType::Type ChangeType);

	bool bWaterObjectsChanged;

	bool bBuoyancySettingsChanged;

	FBuoyancySettings BuoyancySettings;

	FBuoyancyWaterSplineDataManager* SplineData;

	class FBuoyancySubsystemSimCallback* SimCallback;

	ENetMode NetMode;
};


//
// Buoyancy Sim Callback
//

// Each particle will have a list of potential midphases to process,
// which must be sorted in descending Z order. This struct is used
// to store them
struct FBuoyancyInteraction
{
	Chaos::FPBDRigidParticleHandle* RigidParticle;
	Chaos::FGeometryParticleHandle* WaterParticle;
	const FBuoyancyWaterSplineData& WaterSpline;
	float ClosestSplineKey;
	FVector ClosestPoint;
};

// Metadata for submersions, used for event callbacks
struct FBuoyancySubmersionMetaData
{
	struct FWaterContact
	{
		Chaos::FGeometryParticleHandle* Water;
		float Vol;
		FVector CoM;
		FVector Vel;
	};

	// How many metadata's allowed per submerged particle
	static constexpr int32 MaxNumWaterContacts = 3;
	TArray<FWaterContact, TInlineAllocator<MaxNumWaterContacts>> WaterContacts;
};


// A minimal struct of data tracking all the submersions in a frame.
struct FBuoyancySubmersion
{
	Chaos::FPBDRigidParticleHandle* Particle;
	float Vol;
	FVector CoM;
	FVector Vel;
	FVector Norm;
};

struct FBuoyancySubsystemSimCallbackInput : public Chaos::FSimCallbackInput
{
	// If this ptr is set, then we have a new spline data manager...
	// That should only probably happen one time
	TOptional<FBuoyancyWaterSplineDataManager*> SplineData;

	// Here we use a unique ptr so that it is possible to provide an async
	// input _without_ buoyancy settings (which may be eventually desirable
	// when we eventually are passing lists of water bodies or water wave
	// data).
	mutable TUniquePtr<FBuoyancySettings> BuoyancySettings;

	// Set when net mode changes - should be one time on initialization.
	TOptional<ENetMode> NetMode;

	void Reset();
};

struct FBuoyancySubsystemSimCallbackOutput : public Chaos::FSimCallbackOutput
{
	struct FSurfaceTouch
	{
		uint8 Flag;
		IPhysicsProxyBase* RigidProxy;
		IPhysicsProxyBase* WaterProxy;
		float Vol;
		FVector CoM;
		FVector Vel;
	};

	TArray<FSurfaceTouch> SurfaceTouches;

	void Reset();
};

// NOTE: The Presimulate option is only needed for proper registry with the solver.
//       We don't actually need (or want!) a presimulate tick.
class FBuoyancySubsystemSimCallback : public Chaos::TSimCallbackObject<
	FBuoyancySubsystemSimCallbackInput,
	FBuoyancySubsystemSimCallbackOutput,
	Chaos::ESimCallbackOptions::Presimulate | Chaos::ESimCallbackOptions::MidPhaseModification>
{
private:

	virtual void OnPreSimulate_Internal() override;
	virtual void OnMidPhaseModification_Internal(Chaos::FMidPhaseModifierAccessor& Modifier) override;

	void TrackInteractions(Chaos::FPBDRigidsEvolution& Evolution, Chaos::FMidPhaseModifierAccessor& MidPhaseAccessor);
	void TrackInteraction(Chaos::FPBDRigidsEvolution& Evolution, Chaos::FGeometryParticleHandle* WaterParticle, Chaos::FPBDRigidParticleHandle* RigidParticle, const FBuoyancyWaterSplineData& WaterSpline, Chaos::FMidPhaseModifier& MidPhase);
	void ProcessInteractions(Chaos::FPBDRigidsEvolution& Evolution);
	void ProcessInteraction(Chaos::FPBDRigidsEvolution& Evolution, FBuoyancyInteraction& Interaction);
	void ApplyBuoyantForces(Chaos::FPBDRigidsEvolution& Evolution);
	void GenerateCallbackData();

	// Reference to UserDataPT sim callback which manages synchronization of
	// water spline data
	FBuoyancyWaterSplineDataManager* SplineData = nullptr;

	// Initially we won't have any settings - they have to get passed down
	// via async input. I used TUniquePtr to control access to the same
	// memory that was allocated by GT to minimize copies.
	TUniquePtr<FBuoyancySettings> BuoyancySettings;

	// Sparse array of arrays of buoyancy interactions - the outer array has one
	// entry per particle, the inner array has an entry per water body that it interacts
	// with. Each will be a very small array, sorted by Z.
	//
	// We use inline allocator to avoid more heap allocations, and to express the
	// assumption that a single particle is unlikely to exceed interactions with a
	// certain number of waterbodies at a time.
	static constexpr int32 MaxNumBuoyancyInteractions = 2;
	TSparseArray<TArray<FBuoyancyInteraction, TInlineAllocator<MaxNumBuoyancyInteractions>>> Interactions;

	// This sparse array of submersion events is indexed on particle unique indices.
	// All buoyant forces due to submersions are applied at once. It's stored as
	// a member variable and reset every frame, to avoid reallocation of similarly
	// sized data.
	TSparseArray<FBuoyancySubmersion> Submersions;
	TSparseArray<FBuoyancySubmersion> PrevSubmersions;

	// Another sparse array to be kept in sync with Submersions, which will contain
	// metadata useful for event callbacks
	TSparseArray<FBuoyancySubmersionMetaData> SubmersionMetaData;
	TSparseArray<FBuoyancySubmersionMetaData> PrevSubmersionMetaData;

	// This is a sparse array of bit arrays representing which shapes in an object
	// have already been accounted for when submerging an object. For example, if
	// a massive BVH object has two leaf node shapes submerged in different pools
	// of water and we've already detected that leaf A is submerged, we don't
	// need to test A again. This helps to avoid double counting submerged shapes.
	//
	// Just like Submersions, we have this as a member variable only to keep the
	// memory hot - the array is reset, repopulated and traversed, every frame,
	// so we want to minimize allocations.
	TSparseArray<TBitArray<>> SubmergedShapes;

	// Used to track the net mode of the world that owns the phys scene that this
	// sim tick is taking place in.
	ENetMode NetMode = ENetMode::NM_MAX;

	// Local cache of spline keys to reduce spline evaluations.
	FSplineKeyCacheGrid SplineKeyCache;
};
