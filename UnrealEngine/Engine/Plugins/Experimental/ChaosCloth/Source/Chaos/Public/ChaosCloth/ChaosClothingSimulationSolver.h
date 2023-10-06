// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ChaosCloth/ChaosClothConstraints.h"
#include "Chaos/PBDSoftsEvolutionFwd.h"
#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/Transform.h"
#include "Chaos/Framework/PhysicsSolverBase.h"
#include "Chaos/ImplicitObject.h"
#include "PhysicsProxy/PerSolverFieldSystem.h"

class USkeletalMeshComponent;
class UClothingAssetCommon;
struct FClothingSimulationCacheData;

namespace Chaos
{
	class FTriangleMesh;

	class FClothingSimulationCloth;
	class FClothingSimulationCollider;
	class FClothingSimulationMesh;
	class FClothingSimulationConfig;

	namespace Softs
	{
		class FCollectionPropertyConstFacade;
	}

	// Solver simulation node
	class FClothingSimulationSolver final : public FPhysicsSolverEvents
	{
	public:
		CHAOSCLOTH_API FClothingSimulationSolver();
		CHAOSCLOTH_API ~FClothingSimulationSolver();
		
		FClothingSimulationSolver(const FClothingSimulationSolver&) = delete;
		FClothingSimulationSolver(FClothingSimulationSolver&&) = delete;
		FClothingSimulationSolver& operator=(const FClothingSimulationSolver&) = delete;
		FClothingSimulationSolver& operator=(FClothingSimulationSolver&&) = delete;

		// ---- Animatable property setters ----
		CHAOSCLOTH_API void SetLocalSpaceLocation(const FVec3& InLocalSpaceLocation, bool bReset = false);
		const FVec3& GetLocalSpaceLocation() const { return LocalSpaceLocation; }
		void SetLocalSpaceRotation(const FQuat& InLocalSpaceRotation) { LocalSpaceRotation = InLocalSpaceRotation; }
		const FRotation3& GetLocalSpaceRotation() const { return LocalSpaceRotation; }
		void SetVelocityScale(FReal InVelocityScale) { VelocityScale = InVelocityScale; }
		FReal GetVelocityScale() const { return VelocityScale; }

		// Disables all Cloths gravity override mechanism
		void EnableClothGravityOverride(bool bInIsClothGravityOverrideEnabled) { bIsClothGravityOverrideEnabled = bInIsClothGravityOverrideEnabled; }
		bool IsClothGravityOverrideEnabled() const { return bIsClothGravityOverrideEnabled; }
		void SetGravity(const TVec3<FRealSingle>& InGravity) { Gravity = InGravity; }
		const TVec3<FRealSingle>& GetGravity() const { return Gravity; }

		CHAOSCLOTH_API void SetWindVelocity(const TVec3<FRealSingle>& InWindVelocity, FRealSingle InLegacyWindAdaption = (FRealSingle)0.);
		const TVec3<FRealSingle>& GetWindVelocity() const { return WindVelocity; }

		UE_DEPRECATED(5.3, "Set properties directly through FClothingSimulationConfig")
		CHAOSCLOTH_API void SetNumIterations(int32 InNumIterations);
		CHAOSCLOTH_API int32 GetNumIterations() const;
		UE_DEPRECATED(5.3, "Set properties directly through FClothingSimulationConfig")
		CHAOSCLOTH_API void SetMaxNumIterations(int32 InMaxNumIterations);
		CHAOSCLOTH_API int32 GetMaxNumIterations() const;
		UE_DEPRECATED(5.3, "Set properties directly through FClothingSimulationConfig")
		CHAOSCLOTH_API void SetNumSubsteps(int32 InNumSubsteps);
		CHAOSCLOTH_API int32 GetNumSubsteps() const;

		void SetEnableSolver(bool InbEnableSolver) { bEnableSolver = InbEnableSolver; }
		bool GetEnableSolver() const { return bEnableSolver; }
		// ---- End of the animatable property setters ----

		// ---- Object management functions ----
		CHAOSCLOTH_API void SetCloths(TArray<FClothingSimulationCloth*>&& InCloths); 
		CHAOSCLOTH_API void AddCloth(FClothingSimulationCloth* InCloth);
		CHAOSCLOTH_API void RemoveCloth(FClothingSimulationCloth* InCloth);
		CHAOSCLOTH_API void RemoveCloths();

		CHAOSCLOTH_API void RefreshCloth(FClothingSimulationCloth* InCloth);
		CHAOSCLOTH_API void RefreshCloths();

		TConstArrayView<const FClothingSimulationCloth*> GetCloths() const { return Cloths; }

		/** Get the solver configuration. */
		FClothingSimulationConfig* GetConfig() const { return Config; }

		/**
		 * Set the solver configuration.
		 * Can use a cloth config if a single cloth is being simulated.
		 */
		CHAOSCLOTH_API void SetConfig(FClothingSimulationConfig* InConfig);

		void SetSolverLOD(int32 LODIndex) { SolverLOD = LODIndex; }
		int32 GetSolverLOD() const { return SolverLOD; }

		/** Advance the simulation. */
		CHAOSCLOTH_API void Update(FSolverReal InDeltaTime);

		/** Return the last delta time used for advancing the simulation. */
		FSolverReal GetDeltaTime() const { return DeltaTime; }

		/** Set the cached positions onto the particles */
		CHAOSCLOTH_API void UpdateFromCache(const FClothingSimulationCacheData& CacheData);
		UE_DEPRECATED(5.3, "Use UpdateFromCache(CacheData) instead")
		CHAOSCLOTH_API void UpdateFromCache(const TArray<FVector>& CachedPositions, const TArray<FVector>& CachedVelocities);

		// Return the actual of number of iterations used by the Evolution solver after the update (different from the number of iterations, depends on frame rate)
		CHAOSCLOTH_API int32 GetNumUsedIterations() const;

		CHAOSCLOTH_API FBoxSphereBounds CalculateBounds() const;
		// ---- End of the object management functions ----

		// ---- Cloth interface ----
		CHAOSCLOTH_API int32 AddParticles(int32 NumParticles, uint32 GroupId);
		CHAOSCLOTH_API void EnableParticles(int32 Offset, bool bEnable);

		CHAOSCLOTH_API void ResetStartPose(int32 Offset, int32 NumParticles);

		// Get the current solver time
		FSolverReal GetTime() const { return Time; }
		CHAOSCLOTH_API void SetParticleMassUniform(int32 Offset, FRealSingle UniformMass, FRealSingle MinPerParticleMass, const FTriangleMesh& Mesh, const TFunctionRef<bool(int32)>& KinematicPredicate);
		CHAOSCLOTH_API void SetParticleMassFromTotalMass(int32 Offset, FRealSingle TotalMass, FRealSingle MinPerParticleMass, const FTriangleMesh& Mesh, const TFunctionRef<bool(int32)>& KinematicPredicate);
		CHAOSCLOTH_API void SetParticleMassFromDensity(int32 Offset, FRealSingle Density, FRealSingle MinPerParticleMass, const FTriangleMesh& Mesh, const TFunctionRef<bool(int32)>& KinematicPredicate);

		// Set the amount of velocity allowed to filter from the given change in reference space transform, including local simulation space.
		CHAOSCLOTH_API void SetReferenceVelocityScale(uint32 GroupId,
			const FRigidTransform3& OldReferenceSpaceTransform,
			const FRigidTransform3& ReferenceSpaceTransform,
			const TVec3<FRealSingle>& LinearVelocityScale,
			FRealSingle AngularVelocityScale,
			FRealSingle FictitiousAngularScale);

		// Set general cloth simulation properties.
		CHAOSCLOTH_API void SetProperties(
			uint32 GroupId,
			FRealSingle DampingCoefficient,
			FRealSingle MotionDampingCoefficient,
			FRealSingle CollisionThickness,
			FRealSingle FrictionCoefficient);

		// Set whether to use continuous collision detection.
		CHAOSCLOTH_API void SetUseCCD(uint32 GroupId, bool bUseCCD);

		// Set per group gravity, used to override solver's gravity. Must be called during cloth update.
		CHAOSCLOTH_API void SetGravity(uint32 GroupId, const TVec3<FRealSingle>& Gravity);

		// Set per group wind velocity, used to override solver's wind velocity. Must be called during cloth update.
		CHAOSCLOTH_API void SetWindVelocity(uint32 GroupId, const TVec3<FRealSingle>& InWindVelocity);

		// Set the geometry affected by the wind and pressure
		CHAOSCLOTH_API void SetWindAndPressureGeometry(
			uint32 GroupId,
			const FTriangleMesh& TriangleMesh,
			const Softs::FCollectionPropertyConstFacade& PropertyCollection,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps);

		CHAOSCLOTH_API void SetWindAndPressureGeometry(
			uint32 GroupId,
			const FTriangleMesh& TriangleMesh,
			const TConstArrayView<FRealSingle>& DragMultipliers,
			const TConstArrayView<FRealSingle>& LiftMultipliers,
			const TConstArrayView<FRealSingle>& PressureMultipliers);

		// Set the wind and pressure properties.
		CHAOSCLOTH_API void SetWindAndPressureProperties(
			uint32 GroupId,
			const Softs::FCollectionPropertyConstFacade& PropertyCollection,
			const TMap<FString, TConstArrayView<FRealSingle>>& WeightMaps,
			bool bEnableAerodynamics);

		CHAOSCLOTH_API void SetWindAndPressureProperties(
			uint32 GroupId,
			const TVec2<FRealSingle>& Drag,
			const TVec2<FRealSingle>& Lift,
			FRealSingle FluidDensity = 1.225f,
			const TVec2<FRealSingle>& Pressure = TVec2<FRealSingle>::ZeroVector);

		// Return the wind velocity and pressure field associated with a given group id.
		CHAOSCLOTH_API const Softs::FVelocityAndPressureField& GetWindVelocityAndPressureField(uint32 GroupId) const;

		// Add external forces to the particles
		CHAOSCLOTH_API void AddExternalForces(uint32 GroupId, bool bUseLegacyWind);

		const Softs::FSolverVec3* GetOldAnimationPositions(int32 Offset) const { return OldAnimationPositions.GetData() + Offset; }
		Softs::FSolverVec3* GetOldAnimationPositions(int32 Offset) { return OldAnimationPositions.GetData() + Offset; }
		const Softs::FSolverVec3* GetAnimationPositions(int32 Offset) const { return AnimationPositions.GetData() + Offset; }
		Softs::FSolverVec3* GetAnimationPositions(int32 Offset) { return AnimationPositions.GetData() + Offset; }
		const Softs::FSolverVec3* GetInterpolatedAnimationPositions(int32 Offset) const { return InterpolatedAnimationPositions.GetData() + Offset; }
		Softs::FSolverVec3* GetInterpolatedAnimationPositions(int32 Offset) { return InterpolatedAnimationPositions.GetData() + Offset; }
		const Softs::FSolverVec3* GetOldAnimationNormals(int32 Offset) const { return OldAnimationNormals.GetData() + Offset; }
		Softs::FSolverVec3* GetOldAnimationNormals(int32 Offset) { return OldAnimationNormals.GetData() + Offset; }
		const Softs::FSolverVec3* GetAnimationNormals(int32 Offset) const { return AnimationNormals.GetData() + Offset; }
		Softs::FSolverVec3* GetAnimationNormals(int32 Offset) { return AnimationNormals.GetData() + Offset; }
		const Softs::FSolverVec3* GetInterpolatedAnimationNormals(int32 Offset) const { return InterpolatedAnimationNormals.GetData() + Offset; }
		Softs::FSolverVec3* GetInterpolatedAnimationNormals(int32 Offset) { return InterpolatedAnimationNormals.GetData() + Offset; }
		const Softs::FSolverVec3* GetNormals(int32 Offset) const { return Normals.GetData() + Offset; }
		const Softs::FSolverVec3* GetAnimationVelocities(int32 Offset) const { return AnimationVelocities.GetData() + Offset; }
		Softs::FSolverVec3* GetAnimationVelocities(int32 Offset) { return AnimationVelocities.GetData() + Offset; }
		Softs::FSolverVec3* GetNormals(int32 Offset) { return Normals.GetData() + Offset; }
		CHAOSCLOTH_API const Softs::FPAndInvM* GetParticlePandInvMs(int32 Offset) const;
		CHAOSCLOTH_API Softs::FPAndInvM* GetParticlePandInvMs(int32 Offset);
		CHAOSCLOTH_API const Softs::FSolverVec3* GetParticleXs(int32 Offset) const;
		CHAOSCLOTH_API Softs::FSolverVec3* GetParticleXs(int32 Offset);
		CHAOSCLOTH_API const Softs::FSolverVec3* GetParticleVs(int32 Offset) const;
		CHAOSCLOTH_API Softs::FSolverVec3* GetParticleVs(int32 Offset);
		CHAOSCLOTH_API const Softs::FSolverReal* GetParticleInvMasses(int32 Offset) const;
		const FClothConstraints& GetClothConstraints(int32 Offset) const { return *ClothsConstraints.FindChecked(Offset); }
		FClothConstraints& GetClothConstraints(int32 Offset) { return *ClothsConstraints.FindChecked(Offset); }
		CHAOSCLOTH_API uint32 GetNumParticles() const;
		// ---- End of the Cloth interface ----

		// ---- Collider interface ----
		CHAOSCLOTH_API int32 AddCollisionParticles(int32 NumCollisionParticles, uint32 GroupId, int32 RecycledOffset = 0);
		CHAOSCLOTH_API void EnableCollisionParticles(int32 Offset, bool bEnable);

		CHAOSCLOTH_API void ResetCollisionStartPose(int32 Offset, int32 NumCollisionParticles);

		const int32* GetCollisionBoneIndices(int32 Offset) const { return CollisionBoneIndices.GetData() + Offset; }
		int32* GetCollisionBoneIndices(int32 Offset) { return CollisionBoneIndices.GetData() + Offset; }
		const Softs::FSolverRigidTransform3* GetCollisionBaseTransforms(int32 Offset) const { return CollisionBaseTransforms.GetData() + Offset; }
		Softs::FSolverRigidTransform3* GetCollisionBaseTransforms(int32 Offset) { return CollisionBaseTransforms.GetData() + Offset; }
		const Softs::FSolverRigidTransform3* GetOldCollisionTransforms(int32 Offset) const { return OldCollisionTransforms.GetData() + Offset; }
		Softs::FSolverRigidTransform3* GetOldCollisionTransforms(int32 Offset) { return OldCollisionTransforms.GetData() + Offset; }
		const Softs::FSolverRigidTransform3* GetCollisionTransforms(int32 Offset) const { return CollisionTransforms.GetData() + Offset; }
		Softs::FSolverRigidTransform3* GetCollisionTransforms(int32 Offset) { return CollisionTransforms.GetData() + Offset; }
		CHAOSCLOTH_API const Softs::FSolverVec3* GetCollisionParticleXs(int32 Offset) const;
		CHAOSCLOTH_API Softs::FSolverVec3* GetCollisionParticleXs(int32 Offset);
		CHAOSCLOTH_API const Softs::FSolverRotation3* GetCollisionParticleRs(int32 Offset) const;
		CHAOSCLOTH_API Softs::FSolverRotation3* GetCollisionParticleRs(int32 Offset);
		CHAOSCLOTH_API void SetCollisionGeometry(int32 Offset, int32 Index, TUniquePtr<FImplicitObject>&& Geometry);
		CHAOSCLOTH_API const TUniquePtr<FImplicitObject>* GetCollisionGeometries(int32 Offset) const;
		CHAOSCLOTH_API const bool* GetCollisionStatus(int32 Offset) const;
		CHAOSCLOTH_API const TArray<Softs::FSolverVec3>& GetCollisionContacts() const;
		CHAOSCLOTH_API const TArray<Softs::FSolverVec3>& GetCollisionNormals() const;
		CHAOSCLOTH_API const TArray<Softs::FSolverReal>& GetCollisionPhis() const;
		// ---- End of the Collider interface ----

		// ---- Field interface ----
		FPerSolverFieldSystem& GetPerSolverField() { return PerSolverField; }
		const FPerSolverFieldSystem& GetPerSolverField() const { return PerSolverField; }
		// ---- End of the Field interface ----

	private:
		CHAOSCLOTH_API void ResetParticles();
		CHAOSCLOTH_API void ResetCollisionParticles(int32 InCollisionParticlesOffset = 0);
		CHAOSCLOTH_API void ApplyPreSimulationTransforms();
		CHAOSCLOTH_API void PreSubstep(const Softs::FSolverReal InterpolationAlpha);
		CHAOSCLOTH_API Softs::FSolverReal SetParticleMassPerArea(int32 Offset, int32 Size, const FTriangleMesh& Mesh);
		CHAOSCLOTH_API void ParticleMassUpdateDensity(const FTriangleMesh& Mesh, Softs::FSolverReal Density);
		CHAOSCLOTH_API void ParticleMassClampAndKinematicStateUpdate(int32 Offset, int32 Size, Softs::FSolverReal MinPerParticleMass, const TFunctionRef<bool(int32)>& KinematicPredicate);

		// Update the solver field forces/velocities at the particles location
		CHAOSCLOTH_API void UpdateSolverField();

	private:
		TUniquePtr<Softs::FPBDEvolution> Evolution;

		// Object arrays
		TArray<FClothingSimulationCloth*> Cloths;
		
		// Solver config
		int32 SolverLOD = 0; // Config may contain multiple LODs.
		FClothingSimulationConfig* Config = nullptr;
		TSharedPtr<FManagedArrayCollection> PropertyCollection;  // Used for backward compatibility only, otherwise the properties are owned by the Config

		// Simulation group attributes
		TArrayCollectionArray<Softs::FSolverRigidTransform3> PreSimulationTransforms;  // Allow a different frame of reference for each cloth groups
		TArrayCollectionArray<Softs::FSolverVec3> FictitiousAngularDisplacements;  // Relative angular displacement of the reference bone that depends on the fictitious angular scale factor
		TArrayCollectionArray<Softs::FSolverVec3> ReferenceSpaceLocations;  // Center of rotations for fictitious forces in local coordinate to the simulation space location

		// Particle attributes
		TArrayCollectionArray<Softs::FSolverVec3> Normals;
		TArrayCollectionArray<Softs::FSolverVec3> OldAnimationPositions;
		TArrayCollectionArray<Softs::FSolverVec3> AnimationPositions;
		TArrayCollectionArray<Softs::FSolverVec3> InterpolatedAnimationPositions;
		TArrayCollectionArray<Softs::FSolverVec3> OldAnimationNormals;
		TArrayCollectionArray<Softs::FSolverVec3> AnimationNormals;
		TArrayCollectionArray<Softs::FSolverVec3> InterpolatedAnimationNormals;
		TArrayCollectionArray<Softs::FSolverVec3> AnimationVelocities;

		// Collision particle attributes
		TArrayCollectionArray<int32> CollisionBoneIndices;
		TArrayCollectionArray<Softs::FSolverRigidTransform3> CollisionBaseTransforms;
		TArrayCollectionArray<Softs::FSolverRigidTransform3> OldCollisionTransforms;
		TArrayCollectionArray<Softs::FSolverRigidTransform3> CollisionTransforms;

		// Cloth constraints
		TMap<int32, TUniquePtr<FClothConstraints>> ClothsConstraints;

		// Local space simulation
		FVec3 OldLocalSpaceLocation;  // This is used to translate between world space and simulation space,
		FVec3 LocalSpaceLocation;     // add this to simulation space coordinates to get world space coordinates, must keep FReal as underlying type for LWC
		FRotation3 LocalSpaceRotation;
		FReal VelocityScale;

		// Time stepping
		FSolverReal Time;
		FSolverReal DeltaTime;

		// Solver colliders offset
		int32 CollisionParticlesOffset;  // Collision particle offset on the first solver/non cloth collider
		int32 CollisionParticlesSize;  // Number of solver only colliders

		// Solver parameters
		TVec3<FRealSingle> Gravity;
		TVec3<FRealSingle> WindVelocity;
		FRealSingle LegacyWindAdaption;
		bool bIsClothGravityOverrideEnabled;

		// Field system unique to the cloth solver
		FPerSolverFieldSystem PerSolverField;

		// Boolean to enable/disable solver in case caching is used
		bool bEnableSolver;
	};

} // namespace Chaos

#if !defined(CHAOS_CALCULATE_BOUNDS_ISPC_ENABLED_DEFAULT)
#define CHAOS_CALCULATE_BOUNDS_ISPC_ENABLED_DEFAULT 1
#endif

#if !defined(CHAOS_PRE_SIMULATION_TRANSFORMS_ISPC_ENABLED_DEFAULT)
#define CHAOS_PRE_SIMULATION_TRANSFORMS_ISPC_ENABLED_DEFAULT 1
#endif

#if !defined(CHAOS_PRE_SUBSTEP_INTERPOLATION_ISPC_ENABLED_DEFAULT)
#define CHAOS_PRE_SUBSTEP_INTERPOLATION_ISPC_ENABLED_DEFAULT 1
#endif

// Support run-time toggling on supported platforms in non-shipping configurations
#if !INTEL_ISPC || UE_BUILD_SHIPPING
static constexpr bool bChaos_CalculateBounds_ISPC_Enabled = INTEL_ISPC && CHAOS_CALCULATE_BOUNDS_ISPC_ENABLED_DEFAULT;
static constexpr bool bChaos_PreSimulationTransforms_ISPC_Enabled = INTEL_ISPC && CHAOS_PRE_SIMULATION_TRANSFORMS_ISPC_ENABLED_DEFAULT;
static constexpr bool bChaos_PreSubstepInterpolation_ISPC_Enabled = INTEL_ISPC && CHAOS_PRE_SUBSTEP_INTERPOLATION_ISPC_ENABLED_DEFAULT;
#else
extern bool bChaos_PreSimulationTransforms_ISPC_Enabled;
extern bool bChaos_CalculateBounds_ISPC_Enabled;
extern bool bChaos_PreSubstepInterpolation_ISPC_Enabled;
#endif
