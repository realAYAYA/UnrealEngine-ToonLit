// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Chaos/Core.h"

#include "Chaos/BVHParticles.h"
#include "Chaos/CollisionResolutionTypes.h"
#include "Chaos/Collision/CollisionKeys.h"
#include "Chaos/Collision/ContactPoint.h"
#include "Chaos/Collision/PBDCollisionConstraintHandle.h"
#include "Chaos/Framework/UncheckedArray.h"
#include "Chaos/GJK.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/Vector.h"

class FChaosVDDataWrapperUtils;

namespace Chaos
{
	namespace Private
	{
		class FCollisionConstraintAllocator;
		class FCollisionContextAllocator;
	}
	class FConstGenericParticleHandle;
	class FImplicitObject;
	class FParticlePairMidPhase;
	class FPBDCollisionConstraint;
	class FPBDCollisionConstraints;
	class FPerShapeData;
	class FShapeInstance;
	class FSingleShapePairCollisionDetector;
	class FSolverBody;
	class FSolverBodyContainer;

	UE_DEPRECATED(4.27, "Use FPBDCollisionConstraint instead")
	typedef FPBDCollisionConstraint FRigidBodyPointContactConstraint;

	CHAOS_API bool ContactConstraintSortPredicate(const FPBDCollisionConstraint& L, const FPBDCollisionConstraint& R);

	/*
	 * @brief Material properties for a collision constraint
	*/
	class FPBDCollisionConstraintMaterial
	{
	public:
		FPBDCollisionConstraintMaterial()
			: FaceIndex(INDEX_NONE)
			, MaterialDynamicFriction(0)
			, MaterialStaticFriction(0)
			, MaterialRestitution(0)
			, DynamicFriction(0)
			, StaticFriction(0)
			, Restitution(0)
			, RestitutionThreshold(0)
			, InvMassScale0(1)
			, InvMassScale1(1)
			, InvInertiaScale0(1)
			, InvInertiaScale1(1)
		{
		}

		// The face index that the material was extracted from
		int32 FaceIndex;

		// Material properties pulled from the materials of the two shapes involved in the contact
		// @todo(chaos): we can remove these now and just recollect the material data when a modifier was applied
		FRealSingle MaterialDynamicFriction;
		FRealSingle MaterialStaticFriction;
		FRealSingle MaterialRestitution;

		// Final material properties (post modifier) used by the solver. These get reset every frame to the material values above
		FRealSingle DynamicFriction;
		FRealSingle StaticFriction;
		FRealSingle Restitution;

		FRealSingle RestitutionThreshold;
		FRealSingle InvMassScale0;
		FRealSingle InvMassScale1;
		FRealSingle InvInertiaScale0;
		FRealSingle InvInertiaScale1;

		// Reset the material properties to those pulled from the shape's materials (i.e., back to the state before any contact modification)
		void ResetMaterialModifications()
		{
			DynamicFriction = MaterialDynamicFriction;
			StaticFriction = MaterialStaticFriction;
			Restitution = MaterialRestitution;
			InvMassScale0 = FRealSingle(1);
			InvMassScale1 = FRealSingle(1);
			InvInertiaScale0 = FRealSingle(1);
			InvInertiaScale1 = FRealSingle(1);
		}
	};

	// Renamed to FPBDCollisionConstraintMaterial
	using FCollisionContact UE_DEPRECATED(5.1, "FCollisionContact was renamed to FPBDCollisionConstraintMaterial") = FPBDCollisionConstraintMaterial;


	/**
	 * @brief Information used by the constraint allocator
	 * This includes any information used for optimizations like array indexes etc
	 * as well as data for managing lifetime and pruning.
	*/
	class FPBDCollisionConstraintContainerCookie
	{
	public:
		FPBDCollisionConstraintContainerCookie()
			: MidPhase(nullptr)
			, bIsMultiShapePair(false)
			, CreationEpoch(INDEX_NONE)
			, LastUsedEpoch(INDEX_NONE)
			, ConstraintIndex(INDEX_NONE)
			, CCDConstraintIndex(INDEX_NONE)
		{
		}

		/**
		 * @brief Used to clear the container data when copying constraints out of the container (see resim cache)
		 * The constraint index will not be valid when the constraint is copied out of the container, but everything
		 * else is ok and should be restorable.
		*/
		void ClearContainerData()
		{
			MidPhase = nullptr;
			ConstraintIndex = INDEX_NONE;
			CCDConstraintIndex = INDEX_NONE;
		}

		// The constraint owner - set when the constraint is created
		FParticlePairMidPhase* MidPhase;

		// Used by the MidPhase when a constraint is reactivated from a Resim cache
		// If true, indicates that the constraint was created from the recursive collision detection
		// path rather than the prefiltered shape-pair loop
		bool bIsMultiShapePair;

		// The Epoch when then constraint was initially created
		int32 CreationEpoch;

		// The Epoch when the constraint was last used
		int32 LastUsedEpoch;

		// The index in the container - this changes every tick (is valid for all constraints, including CCD)
		int32 ConstraintIndex;

		// The CCD index in the container - this changes every tick (is INDEX_NONE for non-CCD constraints)
		int32 CCDConstraintIndex;
	};


	/**
	 * @brief A contact constraint
	 * 
	 * A contact constraint represents the non-penetration, friction, and restitution constraints for a single
	 * shape pair on a particle pair. I.e., a specific particle-pair may have multiple contact constraints
	 * between them if one or boht has multuple collision shapes that overlap the shape(s) of the other body.
	 * 
	 * Each contact constraint contains a Manifold, which is a set of contact points that approximate the
	 * contact patch between the two shapes.
	 * 
	 * Contact constraints are allocated on the heap (Asee FCollisionConstraintAllocator) and have permanent addresses. 
	 * They use intrusive handles to reduce unnecessary indirection.
	 * 
	*/
	class FPBDCollisionConstraint final : public FPBDCollisionConstraintHandle
	{
		friend class Private::FCollisionConstraintAllocator;
		friend class Private::FCollisionContextAllocator;
		friend class FGenericParticlePairMidPhase;
		friend class FParticlePairMidPhase;
		friend class FPBDCollisionConstraints;
		friend class FShapePairParticlePairMidPhase;
		friend class FSingleShapePairCollisionDetector;

		friend CHAOS_API bool ContactConstraintSortPredicate(const FPBDCollisionConstraint& L, const FPBDCollisionConstraint& R);

	public:
		using FConstraintContainerHandle = TIntrusiveConstraintHandle<FPBDCollisionConstraint>;

		static const int32 MaxManifoldPoints = 4;

		static constexpr FRealSingle MaxTOI = std::numeric_limits<FRealSingle>::max();

		/**
		 * @brief Create a contact constraint
		 * Initializes a constraint stored inline in an object. Only intended to be called once right after construction.
		 * Does not reinitialize all data so not intended to reset a constraint for reuse with different particles etc.
		*/
		static CHAOS_API void Make(
			FGeometryParticleHandle* Particle0,
			const FImplicitObject* Implicit0,
			const FPerShapeData* Shape0,
			const FBVHParticles* Simplicial0,
			const FRigidTransform3& ImplicitLocalTransform0,
			FGeometryParticleHandle* Particle1,
			const FImplicitObject* Implicit1,
			const FPerShapeData* Shape1,
			const FBVHParticles* Simplicial1,
			const FRigidTransform3& ImplicitLocalTransform1,
			const FReal InCullDistance,
			const bool bInUseManifold,
			const EContactShapesType ShapesType,
			FPBDCollisionConstraint& OutConstraint);

		/**
		 * @brief For use by the tri mesh and heighfield collision detection as a temporary measure
		 * @see FHeightField::ContactManifoldImp, FTriangleMeshImplicitObject::ContactManifoldImp
		*/
		static CHAOS_API FPBDCollisionConstraint MakeTriangle(const FImplicitObject* Implicit0);

		/**
		 * @brief Return a constraint copied from the Source constraint, for use in the Resim Cache or other system
		 * @note Unlike the other factory method, this version returns a constraint by value for emplacing into an array (ther others are by pointer)
		*/
		static CHAOS_API FPBDCollisionConstraint MakeCopy(const FPBDCollisionConstraint& Source);

		/**
		 * Restore the properties of a collision from the properties of another. Used by the rewind/resim system when restoring
		 * a collision constraint that was saved (and called MakeCopy).
		 * This takes care not to overwrite any data maintained for the use of other systems (the ContainerCookie, Graph data, etc)
		 */
		CHAOS_API void RestoreFrom(const FPBDCollisionConstraint& Source);

		FPBDCollisionConstraint();
		CHAOS_API virtual ~FPBDCollisionConstraint();

		/**
		 * Whether CCD is enabled for this collision.
		 * This value depends only on the CCD requirements of the two particles. It does not change from tick to tick.
		 */
		bool GetCCDEnabled() const { return Flags.bCCDEnabled; }

		/**
		 * Enable or disable CCD for this constraint. Called once right after the constraint is created.
		 */
		void SetCCDEnabled(const bool bCCDEnabled)
		{ 
			Flags.bCCDEnabled = bCCDEnabled;
			Flags.bCCDSweepEnabled = bCCDEnabled;

			// Initialize the CCD thresholds (one time only)
			if (bCCDEnabled && (CCDEnablePenetration == FReal(0)))
			{
				InitCCDThreshold();
			}
		}

		/**
		 * For CCD constraints, do we need to run the initial sweep/rewind step.
		 * This may change from tick to tick as an object's velocity changes.
		 */
		bool GetCCDSweepEnabled() const { return Flags.bCCDSweepEnabled; }

		/**
		 * For CCD constraints, enable or disable the pre-solve sweep/rewind for this tick. Called
		 * every tick for CCD constraints based on current velocity versus size etc.
		 */
		void SetCCDSweepEnabled(const bool bCCDSweepEnabled)
		{
			Flags.bCCDSweepEnabled = bCCDSweepEnabled;
		}

		/**
		 * @brief If CCD is enabled, contacts deeper than this will be handled by CCD
		*/
		FReal GetCCDEnablePenetration() const
		{
			return CCDEnablePenetration;
		}

		/**
		 * @brief If CCD is enabled and processed the contact, CCD resolution leaves up to this much penetration
		*/
		FReal GetCCDTargetPenetration() const
		{
			return CCDTargetPenetration;
		}

		//
		// API
		//

		FGeometryParticleHandle* GetParticle0() const { return Particle[0]; }
		FGeometryParticleHandle* GetParticle1() const { return Particle[1]; }
		FGeometryParticleHandle* GetParticle(const int32 ParticleIndex) const { check((ParticleIndex >= 0) && (ParticleIndex < 2)); return Particle[ParticleIndex]; }

		const FImplicitObject* GetImplicit0() const { return Implicit[0]; }
		const FImplicitObject* GetImplicit1() const { return Implicit[1]; }
		const FImplicitObject* GetImplicit(const int32 ParticleIndex) const { check((ParticleIndex >= 0) && (ParticleIndex < 2)); return Implicit[ParticleIndex]; }

		const FShapeInstance* GetShape0() const { return Shape[0]; }
		const FShapeInstance* GetShape1() const { return Shape[1]; }
		const FShapeInstance* GetShape(const int32 ParticleIndex) const { check((ParticleIndex >= 0) && (ParticleIndex < 2)); return Shape[ParticleIndex]; }

		const FBVHParticles* GetCollisionParticles0() const { return Simplicial[0]; }
		const FBVHParticles* GetCollisionParticles1() const { return Simplicial[1]; }
		const FBVHParticles* GetCollisionParticles(const int32 ParticleIndex) const { check((ParticleIndex >= 0) && (ParticleIndex < 2)); return Simplicial[ParticleIndex]; }

		const FReal GetCollisionMargin0() const { return CollisionMargins[0]; }
		const FReal GetCollisionMargin1() const { return CollisionMargins[1]; }

		const bool IsQuadratic0() const { return Flags.bIsQuadratic0; }
		const bool IsQuadratic1() const { return Flags.bIsQuadratic1; }
		const bool HasQuadraticShape() const { return (Flags.bIsQuadratic0 || Flags.bIsQuadratic1); }
		const FReal GetCollisionRadius0() const { return (Flags.bIsQuadratic0) ? CollisionMargins[0] : FReal(0); }
		const FReal GetCollisionRadius1() const { return (Flags.bIsQuadratic1) ? CollisionMargins[1] : FReal(0); }

		/** 
		 * Called each frame when the constraint is active after primary collision detection (but not per incremental collision detection call if enabled) 
		 */
		CHAOS_API void Activate();

		UE_DEPRECATED(5.2, "Removed parameter")
		void Activate(const FReal Dt) { Activate(); }

		// When a particle is moved under user control, we need to update some cached state to prevent friction from undoing the move
		CHAOS_API void UpdateParticleTransform(FGeometryParticleHandle* InParticle);

		// @todo(chaos): half of this API is wrong for the new multi-point manifold constraints. Remove it

		void ResetPhi(FReal InPhi) { ClosestManifoldPointIndex = INDEX_NONE; }
		FReal GetPhi() const { return (ClosestManifoldPointIndex != INDEX_NONE) ? ManifoldPoints[ClosestManifoldPointIndex].ContactPoint.Phi : TNumericLimits<FReal>::Max(); }

		void SetDisabled(bool bInDisabled) { Flags.bDisabled = bInDisabled; }
		bool GetDisabled() const { return Flags.bDisabled; }

		void SetIsProbe(bool bInProbe) { Flags.bIsProbe = bInProbe; }
		bool GetIsProbe() const { return Flags.bIsProbe; }

		virtual bool SupportsSleeping() const override final { return true; }
		CHAOS_API virtual bool IsSleeping() const override final;
		CHAOS_API virtual void SetIsSleeping(const bool bInIsSleeping) override final;

		// Get the world-space normal of the closest manifold point
		// @todo(chaos): remove (used by legacy RBAN collision solver)
		CHAOS_API FVec3 CalculateWorldContactNormal() const;

		// Get the world-space contact location of the closest manifold point
		// @todo(chaos): remove (used by legacy RBAN collision solver)
		CHAOS_API FVec3 CalculateWorldContactLocation() const;

		// Called to force the contact to recollect its material properties (e.g., when materials are modified)
		void ClearMaterialProperties()
		{
			Flags.bMaterialSet = false;
		}

		// Find the material for this contact and collect the friction, restitution, etc. This data is cached in the 
		// contact, so ClearMaterialProperties must be called if the material changes to force a re-gather of the data.
		void UpdateMaterialProperties()
		{
			if (!Flags.bMaterialSet)
			{
				UpdateMaterialPropertiesImpl();
				Flags.bMaterialSet = true;
			}
		}

		void SetModifierApplied() { Flags.bModifierApplied = true; }

		const FPBDCollisionConstraintMaterial& GetCollisionMaterial() const { return Material; }

		void SetInvMassScale0(const FReal InInvMassScale) { Material.InvMassScale0 = FRealSingle(InInvMassScale); }
		FReal GetInvMassScale0() const { return Material.InvMassScale0; }

		void SetInvMassScale1(const FReal InInvMassScale) { Material.InvMassScale1 = FRealSingle(InInvMassScale); }
		FReal GetInvMassScale1() const { return Material.InvMassScale1; }

		void SetInvInertiaScale0(const FReal InInvInertiaScale) { Material.InvInertiaScale0 = FRealSingle(InInvInertiaScale); }
		FReal GetInvInertiaScale0() const { return Material.InvInertiaScale0; }

		void SetInvInertiaScale1(const FReal InInvInertiaScale) { Material.InvInertiaScale1 = FRealSingle(InInvInertiaScale); }
		FReal GetInvInertiaScale1() const { return Material.InvInertiaScale1; }

		void SetStiffness(FReal InStiffness) { Stiffness = FRealSingle(InStiffness); }
		FReal GetStiffness() const { return Stiffness; }

		void SetRestitution(const FReal InRestitution) { Material.Restitution = FRealSingle(InRestitution); }
		FReal GetRestitution() const { return Material.Restitution; }

		void SetRestitutionThreshold(const FReal InRestitutionThreshold) { Material.RestitutionThreshold = FRealSingle(InRestitutionThreshold); }
		FReal GetRestitutionThreshold() const { return Material.RestitutionThreshold; }

		void SetStaticFriction(const FReal InStaticFriction) { Material.StaticFriction = FRealSingle(InStaticFriction); }
		FReal GetStaticFriction() const { return FMath::Max(Material.StaticFriction, Material.DynamicFriction); }

		void SetDynamicFriction(const FReal InDynamicFriction) { Material.DynamicFriction = FRealSingle(InDynamicFriction); }
		FReal GetDynamicFriction() const { return Material.DynamicFriction; }

		EContactShapesType GetShapesType() const { return ShapesType; }

		CHAOS_API FString ToString() const;

		FReal GetCullDistance() const { return CullDistance; }
		void SetCullDistance(FReal InCullDistance) { CullDistance = FRealSingle(InCullDistance); }

		// Whether we are using manifolds (either one-shot or incremental)
		bool GetUseManifold() const { return Flags.bUseManifold; }

		// Whether we can use incremental manifolds (updated each iteration)
		bool GetUseIncrementalManifold() const { return Flags.bUseIncrementalManifold; }

		// Whether we run collision detection every iteration (true if we are not using one shot manifolds)
		// NOTE: This is initially set based on whether are allowing incremental manifolds
		bool GetUseIncrementalCollisionDetection() const { return !Flags.bUseManifold || Flags.bUseIncrementalManifold; }

		/**
		* Reset the material properties to those from the shape materials. Called each frame to reset contact modifications to the material.
		*/
		inline void ResetModifications()
		{
			if (Flags.bModifierApplied)
			{
				Material.ResetMaterialModifications();

				// Reset other properties which may have changed in contact modification last frame
				Flags.bIsProbe = Flags.bIsProbeUnmodified;

				Flags.bModifierApplied = false;
			}
		}

		/**
		 * @brief Clear the current and previous manifolds
		*/
		CHAOS_API void ResetManifold();

		// @todo(chaos): remove array view and provide per-point accessor
		TArrayView<FManifoldPoint> GetManifoldPoints() { return MakeArrayView(ManifoldPoints.begin(), ManifoldPoints.Num()); }
		TArrayView<const FManifoldPoint> GetManifoldPoints() const { return MakeArrayView(ManifoldPoints.begin(), ManifoldPoints.Num()); }

		int32 NumManifoldPoints() const { return ManifoldPoints.Num(); }
		FManifoldPoint& GetManifoldPoint(const int32 PointIndex) { return ManifoldPoints[PointIndex]; }
		const FManifoldPoint& GetManifoldPoint(const int32 PointIndex) const { return ManifoldPoints[PointIndex]; }
		const FManifoldPoint* GetClosestManifoldPoint() const { return (ClosestManifoldPointIndex != INDEX_NONE) ? &ManifoldPoints[ClosestManifoldPointIndex] : nullptr; }
		bool IsManifoldPointActive(const int32 PointIndex) const { return IsEnabled() && (PointIndex < NumManifoldPoints()) && !ManifoldPoints[PointIndex].Flags.bDisabled; }

		const FManifoldPointResult& GetManifoldPointResult(const int32 PointIndex) const
		{
			// We may request manifold point results for points that are never simulated (e.g., they start asleep).
			// We could handle this by adding zeroed results when we add manifold points, but that's unnecessary work for
			// active constraints, and this is function currently only used for debug draw and stats. If that changes we 
			// may want to change this.
			check(PointIndex < NumManifoldPoints());
			if (PointIndex >= ManifoldPointResults.Num())
			{
				static FManifoldPointResult ZeroResult;
				return ZeroResult;
			}
			return ManifoldPointResults[PointIndex];
		}

		inline int32 NumEnabledManifoldPoints() const
		{
			int32 NumEnabled = 0;
			if (IsEnabled())
			{
				for (int32 ManifoldPointIndex = 0; ManifoldPointIndex < NumManifoldPoints(); ++ManifoldPointIndex)
				{
					if (!ManifoldPoints[ManifoldPointIndex].Flags.bDisabled)
					{
						++NumEnabled;
					}
				}
			}
			return NumEnabled;
		}

		inline void DisableManifoldPoint(const int32 DisabledManifoldPointIndex)
		{
			ManifoldPoints[DisabledManifoldPointIndex].Flags.bDisabled = true;
			if (NumEnabledManifoldPoints() == 0)
			{
				SetDisabled(true);
			}
		}

		CHAOS_API void AddIncrementalManifoldContact(const FContactPoint& ContactPoint);

		// @todo(chaos): remove this and use SetOneShotManifoldContacts
		inline void AddOneshotManifoldContact(const FContactPoint& ContactPoint)
		{
			if (ContactPoint.IsSet() && !ManifoldPoints.IsFull())
			{
				int32 ManifoldPointIndex = AddManifoldPoint(ContactPoint);
				if (ManifoldPoints[ManifoldPointIndex].ContactPoint.Phi < GetPhi())
				{
					ClosestManifoldPointIndex = ManifoldPointIndex;
				}
			}
		}

		/**
		 * @brief Replace the current manifold points with the input.
		 * The input array should contain no more than MaxManifoldPoints contacts (any extra will be ignored).
		 * We assume that all input contacts have been initialized and will not return false from IsSet().
		 * Ignores contacts deeper than the CullDistance for this constraint.
		*/
		inline void SetOneShotManifoldContacts(const TArrayView<const FContactPoint>& ContactPoints)
		{
			ResetActiveManifoldContacts();

			FReal MinPhi = TNumericLimits<FReal>::Max();
			const int32 NumContacts = FMath::Min(ContactPoints.Num(), MaxManifoldPoints);
			for (int32 ContactIndex = 0; ContactIndex < NumContacts; ++ContactIndex)
			{
				const FContactPoint& ContactPoint = ContactPoints[ContactIndex];
				if (ContactPoint.Phi < CullDistance)
				{
					int32 ManifoldPointIndex = AddManifoldPoint(ContactPoint);
					if (ContactPoint.Phi < MinPhi)
					{
						ClosestManifoldPointIndex = ManifoldPointIndex;
						MinPhi = ContactPoint.Phi;
					}
				}
			}

			// If we have a new face we may need to change the material
			// @todo(chaos): support per-manifold point materials?
			if (NumManifoldPoints() > 0)
			{
				if (ManifoldPoints[0].ContactPoint.FaceIndex != Material.FaceIndex)
				{
					Material.FaceIndex = ManifoldPoints[0].ContactPoint.FaceIndex;
					Flags.bMaterialSet = false;
				}
			}

		}

		CHAOS_API void UpdateManifoldContacts();

		// Particle-relative transform of each collision shape in the constraint
		const FRigidTransform3& GetShapeRelativeTransform0() const { return ImplicitTransform[0]; }
		const FRigidTransform3& GetShapeRelativeTransform1() const { return ImplicitTransform[1]; }
		const FRigidTransform3& GetShapeRelativeTransform(const int32 ParticleIndex) const { check((ParticleIndex >= 0) && (ParticleIndex < 2)); return ImplicitTransform[ParticleIndex]; }

		const FRigidTransform3& GetShapeWorldTransform0() const { return ShapeWorldTransforms[0]; }
		const FRigidTransform3& GetShapeWorldTransform1() const { return ShapeWorldTransforms[1]; }

		void SetShapeWorldTransforms(const FRigidTransform3& InShapeWorldTransform0, const FRigidTransform3& InShapeWorldTransform1)
		{
			ShapeWorldTransforms[0] = InShapeWorldTransform0;
			ShapeWorldTransforms[1] = InShapeWorldTransform1;
		}

		// Set the transforms when we last ran collision detection. This also sets the bCanRestoreManifold flag which
		// allows the use of UpdateAndTryRestoreManifold on the next tick.
		void SetLastShapeWorldTransforms(const FRigidTransform3& InShapeWorldTransform0, const FRigidTransform3& InShapeWorldTransform1)
		{
			LastShapeWorldPositionDelta = InShapeWorldTransform0.GetTranslation() - InShapeWorldTransform1.GetTranslation();
			LastShapeWorldRotationDelta = InShapeWorldTransform0.GetRotation().Inverse() * InShapeWorldTransform1.GetRotation();
			Flags.bCanRestoreManifold = true;
		}

		bool GetCanRestoreManifold() const { return Flags.bCanRestoreManifold; }
		CHAOS_API bool TryRestoreManifold();
		CHAOS_API void ResetActiveManifoldContacts();
		CHAOS_API bool TryAddManifoldContact(const FContactPoint& ContactPoint);
		CHAOS_API bool TryInsertManifoldContact(const FContactPoint& ContactPoint);

		//@ todo(chaos): These are for the collision forwarding system - this should use the collision modifier system (which should be extended to support adding collisions)
		void SetManifoldPoints(const TArray<FManifoldPoint>& InManifoldPoints)
		{ 
			ManifoldPoints.SetNum(FMath::Min(MaxManifoldPoints, InManifoldPoints.Num()));
			for (int32 ManifoldPointIndex = 0; ManifoldPoints.Num(); ++ManifoldPointIndex)
			{
				ManifoldPoints[ManifoldPointIndex] = InManifoldPoints[ManifoldPointIndex];
			}
		}

		// The GJK warm-start data. This is updated directly in the narrow phase
		FGJKSimplexData& GetGJKWarmStartData() { return GJKWarmStartData; }

		const FSolverBody* GetSolverBody0() const { return SolverBodies[0]; }
		const FSolverBody* GetSolverBody1() const { return SolverBodies[1]; }

		void SetSolverBodies(const FSolverBody* InSolverBody0, const FSolverBody* InSolverBody1)
		{
			SolverBodies[0] = InSolverBody0;
			SolverBodies[1] = InSolverBody1;
		}

		/**
		 * @brief Whether this constraint was fully restored from a previous tick, and the manifold should be reused as-is
		*/
		bool WasManifoldRestored() const { return Flags.bWasManifoldRestored; }

		/**
		 * Determine the constraint direction based on Normal and Phi.
		 * This function assumes that the constraint is update-to-date.
		 */
		CHAOS_API ECollisionConstraintDirection GetConstraintDirection(const FReal Dt) const;

		/**
		 * @brief Clear the saved manifold points. This effectively resets friction anchors.
		*/
		void ResetSavedManifoldPoints()
		{
			SavedManifoldPoints.Reset();
		}

		/**
		 * Called after the simulation to reset any state that need to be reset before the next tick
		 */
		void EndTick()
		{
		}

		/**
		 * @brief Time of impact from CCD sweep test if CCD is activate.Otherwise undefined.
		*/
		FReal GetCCDTimeOfImpact() const { return CCDTimeOfImpact; }

		/**
		 * @brief Set the CCD TOI from the collision detection sweep
		*/
		void SetCCDTimeOfImpact(const FReal TOI) { check(TOI <= FReal(MaxTOI)); CCDTimeOfImpact = FRealSingle(TOI); }

		/**
		 * Initialize the CCD TOI to the highest possible value
		 */
		void ResetCCDTimeOfImpact() { CCDTimeOfImpact = MaxTOI; }

		/**
		 * \brief Store the results of CCD contact resolution, if active
		 */
		void SetCCDResults(const FVec3& InNetImpulse)
		{
			AccumulatedImpulse += FVec3f(InNetImpulse);
		}

		FORCEINLINE void ResetSolverResults()
		{
			// NOTE: does not initalize any data. All properties will be written to in SetSolverResults
			SavedManifoldPoints.SetNum(ManifoldPoints.Num());
			ManifoldPointResults.SetNum(ManifoldPoints.Num());
		}

		/**
		 * @brief Store the data from the solver that is retained between ticks for the specified manifold point or used by dependent systems (plasticity, breaking, etc.)
		*/
		FORCEINLINE void SetSolverResults(
			const int32 ManifoldPointIndex, 
			const FVec3f& NetPushOut, 
			const FVec3f& NetImpulse, 
			const FRealSingle StaticFrictionRatio,
			const FRealSingle Dt)
		{
			FManifoldPoint& ManifoldPoint = ManifoldPoints[ManifoldPointIndex];
			FManifoldPointResult& ManifoldPointResult = ManifoldPointResults[ManifoldPointIndex];
			FSavedManifoldPoint& SavedManifoldPoint = SavedManifoldPoints[ManifoldPointIndex];

			ManifoldPointResult.NetPushOut = NetPushOut;
			ManifoldPointResult.NetImpulse = NetImpulse;
			ManifoldPointResult.bIsValid = true;
			ManifoldPointResult.bInsideStaticFrictionCone = false;

			AccumulatedImpulse += NetImpulse + (NetPushOut / Dt);

			// Save contact data for friction\
			// NOTE: We also write the anchors back to the manifold point for use if the point gets restored next tick
			// in which case it skips the saved point search etc
			if (StaticFrictionRatio >= FReal(1.0f - UE_KINDA_SMALL_NUMBER))
			{
				// StaticFrictionRatio ~= 1: Static friction held - we keep the same contacts points as-is for use next frame
				SavedManifoldPoint.ShapeContactPoints[0] = ManifoldPoint.ShapeAnchorPoints[0];
				SavedManifoldPoint.ShapeContactPoints[1] = ManifoldPoint.ShapeAnchorPoints[1];
				ManifoldPointResult.bInsideStaticFrictionCone = true;
			}
			else if (StaticFrictionRatio < FReal(UE_KINDA_SMALL_NUMBER))
			{
				// StaticFrictionRatio ~= 0: No friction (or no contact) - discard the friction anchors
				const FVec3 Anchor0 = ManifoldPoint.ContactPoint.ShapeContactPoints[0];
				const FVec3 Anchor1 = ManifoldPoint.ContactPoint.ShapeContactPoints[1];
				SavedManifoldPoint.ShapeContactPoints[0] = Anchor0;
				SavedManifoldPoint.ShapeContactPoints[1] = Anchor1;
			}
			else
			{
				// 0 < StaticFrictionRatio < 1: We exceeded the friction cone. Slide the friction anchor 
				// toward the last-detected contact position so that it sits at the edge of the friction cone.
				const FVec3 Anchor0 = FVec3::Lerp(ManifoldPoint.ContactPoint.ShapeContactPoints[0], ManifoldPoint.ShapeAnchorPoints[0], StaticFrictionRatio);
				const FVec3 Anchor1 = FVec3::Lerp(ManifoldPoint.ContactPoint.ShapeContactPoints[1], ManifoldPoint.ShapeAnchorPoints[1], StaticFrictionRatio);
				SavedManifoldPoint.ShapeContactPoints[0] = Anchor0;
				SavedManifoldPoint.ShapeContactPoints[1] = Anchor1;
			}
		}

		/**
		 *	A key used to uniquely identify the constraint (it is based on the two particle IDs)
		 */
		FCollisionParticlePairKey GetParticlePairKey() const
		{
			return FCollisionParticlePairKey(GetParticle0(), GetParticle1());
		}

	public:
		const FPBDCollisionConstraintHandle* GetConstraintHandle() const { return this; }
		FPBDCollisionConstraintHandle* GetConstraintHandle() { return this; }


		UE_DEPRECATED(5.3, "Use TryRestoreManifold")
		bool UpdateAndTryRestoreManifold() { return TryRestoreManifold(); }

	protected:

		FPBDCollisionConstraint(
			FGeometryParticleHandle* Particle0,
			const FImplicitObject* Implicit0,
			const FPerShapeData* Shape0,
			const FBVHParticles* Simplicial0,
			FGeometryParticleHandle* Particle1,
			const FImplicitObject* Implicit1,
			const FPerShapeData* Shape1,
			const FBVHParticles* Simplicial1);

		// Set all the data not initialized in the constructor
		CHAOS_API void Setup(
			const ECollisionCCDType InCCDType,
			const EContactShapesType InShapesType,
			const FRigidTransform3& InImplicitTransform0,
			const FRigidTransform3& InImplicitTransform1,
			const FReal InCullDistance,
			const bool bInUseManifold);

		// Access to the data used by the container
		const FPBDCollisionConstraintContainerCookie& GetContainerCookie() const { return ContainerCookie; }
		FPBDCollisionConstraintContainerCookie& GetContainerCookie() { return ContainerCookie; }

		CHAOS_API bool AreMatchingContactPoints(const FContactPoint& A, const FContactPoint& B, FReal& OutScore) const;
		CHAOS_API int32 FindManifoldPoint(const FContactPoint& ContactPoint) const;

		CHAOS_API int32 FindSavedManifoldPoint(const int32 ManifoldPointIndex, TCArray<int32, MaxManifoldPoints>& InOutAllowedSavedPointIndices) const;
		CHAOS_API void AssignSavedManifoldPoints();

		inline void InitManifoldPoint(const int32 ManifoldPointIndex, const FContactPoint& ContactPoint)
		{
			FManifoldPoint& ManifoldPoint = ManifoldPoints[ManifoldPointIndex];
			ManifoldPoint.ContactPoint = ContactPoint;
			ManifoldPoint.ShapeAnchorPoints[0] = ContactPoint.ShapeContactPoints[0];
			ManifoldPoint.ShapeAnchorPoints[1] = ContactPoint.ShapeContactPoints[1];
			ManifoldPoint.InitialShapeContactPoints[0] = ContactPoint.ShapeContactPoints[0];
			ManifoldPoint.InitialShapeContactPoints[1] = ContactPoint.ShapeContactPoints[1];
			ManifoldPoint.TargetPhi = FReal(0);
			ManifoldPoint.Flags.Reset();
		}

		inline int32 AddManifoldPoint(const FContactPoint& ContactPoint)
		{
			int32 ManifoldPointIndex = ManifoldPoints.Add();	// Note: no initialization (see TCArray)
			InitManifoldPoint(ManifoldPointIndex, ContactPoint);
			return ManifoldPointIndex;
		}

		// Update the store Phi for the manifold point based on current world-space shape transforms
		// @todo(chaos): Only intended for use by the legacy solvers - remove it
		CHAOS_API void UpdateManifoldPointPhi(const int32 ManifoldPointIndex);

		CHAOS_API void InitMarginsAndTolerances(const EImplicitObjectType ImplicitType0, const EImplicitObjectType ImplicitType1, const FRealSingle Margin0, const FRealSingle Margin1);

		CHAOS_API void InitCCDThreshold();

		CHAOS_API void UpdateMaterialPropertiesImpl();

	private:
		CHAOS_API FReal CalculateSavedManifoldPointScore(const FSavedManifoldPoint& SavedManifoldPoint, const FManifoldPoint& ManifoldPoint, const FReal DistanceToleranceSq) const;

		union FFlags
		{
			FFlags() : Bits(0) {}
			struct
			{
				uint32 bDisabled : 1;					// Is this contact disabled (by the user or because cull distance is exceeded)
				uint32 bUseManifold : 1;				// Should we use contact manifolds or single points (faster but poor behaviour)
				uint32 bUseIncrementalManifold : 1;		// Do we need to run incremental collision detection (only LavelSets now)
				uint32 bCanRestoreManifold : 1;			// Can we try to restore the manifold this frame (set folowing narrowphase, cleared when reset for some reason)
				uint32 bWasManifoldRestored : 1;		// Did we restore the manifold this frame
				uint32 bIsQuadratic0 : 1;				// Is the first shape a sphere or capsule
				uint32 bIsQuadratic1 : 1;				// Is the second shape a sphere or capsule
				uint32 bIsProbeUnmodified : 1;			// Is this constraint a probe pre-contact-modification
				uint32 bIsProbe : 1;					// Is this constraint currently a probe
				uint32 bCCDEnabled : 1;					// Is CCD enabled for the current tick
				uint32 bCCDSweepEnabled: 1;				// If this is a CCD constraint, do we want to enable the sweep/rewind phase?
				uint32 bModifierApplied : 1;			// Was a constraint modifier applied this tick
				uint32 bMaterialSet : 1;				// Has the material been set (or does it need to be reset)
			};
			uint32 Bits;
		};
		static_assert(sizeof(FFlags) == 4, "Unexpected size for FPBDCollisionConstraint::FFLags");

		// Local-space transforms of the shape (relative to particle)
		FRigidTransform3 ImplicitTransform[2];
		
		FGeometryParticleHandle* Particle[2];
		const FImplicitObject* Implicit[2];
		const FShapeInstance* Shape[2];
		const FBVHParticles* Simplicial[2];

	public:
		FVec3f AccumulatedImpulse;					// @todo(chaos): we need to accumulate angular impulse separately

	private:
		FPBDCollisionConstraintContainerCookie ContainerCookie;
		EContactShapesType ShapesType;

		// The shape transforms at the current particle transforms
		FRigidTransform3 ShapeWorldTransforms[2];

		// The separation distance at which we don't track contacts
		FRealSingle CullDistance;

		// The margins to use during collision detection. We don't always use the margins on the shapes directly.
		// E.g., we use the smallest non-zero margin for 2 convex shapes. See InitMarginsAndTolerances
		FRealSingle CollisionMargins[2];

		// The collision tolerance is used to determine whether a new contact matches an old on. It is derived from the
		// margins of the two shapes, as well as their types
		FRealSingle CollisionTolerance;

		// The index into ManifoldPoints of the point with the lowest Phi
		int32 ClosestManifoldPointIndex;

		// Used by manifold point injection to see how many points were in the manifold before UpdateAndTryRestore
		int32 ExpectedNumManifoldPoints;

		// Relative transform the last time we ran the narrow phase
		// Used to detect when the bodies have moved too far to reues the manifold
		FVec3f LastShapeWorldPositionDelta;
		FRotation3f LastShapeWorldRotationDelta;

		// Simplex data from the last call to GJK, used to warm-start GJK
		FGJKSimplexData GJKWarmStartData;

		FPBDCollisionConstraintMaterial Material;
		FRealSingle Stiffness;

		FFlags Flags;

		TCArray<FSavedManifoldPoint, MaxManifoldPoints> SavedManifoldPoints;
		TCArray<FManifoldPoint, MaxManifoldPoints> ManifoldPoints;
		TCArray<FManifoldPointResult, MaxManifoldPoints> ManifoldPointResults;

		// @todo(chaos): These are only needed here to support incremental collision detection for LevelSet. Fix this.
		const FSolverBody* SolverBodies[2];

		// Value in range [0,1] used to interpolate P between [X,P] that we will rollback to when solving at time of impact.
		FRealSingle CCDTimeOfImpact;

		// The penetration at which CCD contacts get processed following a CCD sweep test.
		// NOTE: also see GeometryParticle CCDAxisThreshold, which is used to determine when to enable sweeping during collision detection.
		// Calculated from particle pair properties when constraint is created.
		FRealSingle CCDEnablePenetration;

		// The penetration we leave behind when rolling back to a CCD time of impact. Should be less than or equal to CCDEnablePenetration.
		// Calculated from particle pair properties when constraint is created.
		FRealSingle CCDTargetPenetration;

		friend class ::FChaosVDDataWrapperUtils;
	};


	UE_DEPRECATED(4.27, "FCollisionConstraintBase has been removed and folded into FPBDCollisionConstraint. Use FPBDCollisionConstraint")
	typedef FPBDCollisionConstraint FCollisionConstraintBase;

	UE_DEPRECATED(4.27, "FRigidBodySweptPointContactConstraint has been removed and folded into FPBDCollisionConstraint. Use FPBDCollisionConstraint")
	typedef FPBDCollisionConstraint FRigidBodySweptPointContactConstraint;
}
