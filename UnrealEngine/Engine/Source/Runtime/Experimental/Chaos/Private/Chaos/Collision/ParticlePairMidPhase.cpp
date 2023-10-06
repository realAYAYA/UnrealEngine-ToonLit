// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Collision/ParticlePairMidPhase.h"
#include "Chaos/ChaosDebugDraw.h"
#include "Chaos/Collision/CollisionConstraintAllocator.h"
#include "Chaos/Collision/CollisionContext.h"
#include "Chaos/Collision/CollisionFilter.h"
#include "Chaos/Collision/ContactTriangles.h"
#include "Chaos/Collision/PBDCollisionConstraint.h"
#include "Chaos/CollisionResolution.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/ImplicitObjectBVH.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/Particle/ParticleUtilities.h"
#include "Chaos/PBDCollisionConstraints.h"
#include "ChaosStats.h"
#include "Misc/MemStack.h"


extern bool Chaos_Collision_NarrowPhase_AABBBoundsCheck;

namespace Chaos
{
#if CHAOS_DEBUG_DRAW
	namespace CVars
	{
		extern int32 ChaosSolverDrawCCDInteractions;
		extern DebugDraw::FChaosDebugDrawSettings ChaosSolverDebugDebugDrawSettings;
	}
	namespace DebugDraw
	{
		extern float ChaosDebugDrawCCDDuration;
	}
#endif

	namespace CVars
	{
		bool bChaos_Collision_MidPhase_EnableBoundsChecks = true;
		FAutoConsoleVariableRef CVarChaos_Collision_EnableBoundsChecks(TEXT("p.Chaos.Collision.EnableBoundsChecks"), bChaos_Collision_MidPhase_EnableBoundsChecks, TEXT(""));

		Chaos::FRealSingle Chaos_Collision_CullDistanceScaleInverseSize = 0.01f;	// 100cm
		Chaos::FRealSingle Chaos_Collision_MinCullDistanceScale = 1.0f;
		FAutoConsoleVariableRef CVarChaos_Collision_CullDistanceReferenceSize(TEXT("p.Chaos.Collision.CullDistanceReferenceSize"), Chaos_Collision_CullDistanceScaleInverseSize, TEXT(""));
		FAutoConsoleVariableRef CVarChaos_Collision_MinCullDistanecScale(TEXT("p.Chaos.Collision.MinCullDistanceScale"), Chaos_Collision_MinCullDistanceScale, TEXT(""));

		// Whether we support the pre-flattened shape pair list which optimizes the common midphase case of collision between particles 
		// with only a small number implicit objects each. If so, how many shape pairs do we allow before switching to the generic version?
		bool bChaos_Collision_MidPhase_EnableShapePairs = true;
		int32 Chaos_Collision_MidPhase_MaxShapePairs = 100;
		FAutoConsoleVariableRef CVarChaos_Collision_EnableShapePairs(TEXT("p.Chaos.Collision.EnableShapePairs"), bChaos_Collision_MidPhase_EnableShapePairs, TEXT(""));
		FAutoConsoleVariableRef CVarChaos_Collision_MaxShapePairs(TEXT("p.Chaos.Collision.MaxShapePairs"), Chaos_Collision_MidPhase_MaxShapePairs, TEXT(""));
	}

	using namespace CVars;


	////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////

	inline bool ImplicitOverlapOBBToAABB(
		const FImplicitObject* Implicit0,
		const FImplicitObject* Implicit1,
		const FRigidTransform3& ShapeTransform1To0,
		const FReal CullDistance)
	{
		if (Implicit0->HasBoundingBox() && Implicit1->HasBoundingBox())
		{
			const FAABB3 Box1In0 = Implicit1->CalculateTransformedBounds(ShapeTransform1To0).Thicken(CullDistance);
			const FAABB3 Box0 = Implicit0->BoundingBox();
			return Box0.Intersects(Box1In0);
		}
		return true;
	}

	inline bool ImplicitOverlapOBBToAABB(
		const FImplicitObject* Implicit0,
		const FImplicitObject* Implicit1,
		const FRigidTransform3& ShapeWorldTransform0,
		const FRigidTransform3& ShapeWorldTransform1,
		const FReal CullDistance)
	{
		const FRigidTransform3 ShapeTransform1To0 = ShapeWorldTransform1.GetRelativeTransform(ShapeWorldTransform0);
		return ImplicitOverlapOBBToAABB(Implicit0, Implicit1, ShapeTransform1To0, CullDistance);
	}

	// Get the number of leaf objects in the implicit hierarchy, and set a flag if this hierarchy is a tree
	// (i.e., contains a Union of Unions ro a BVH, rather than a single flat union at the root)
	inline int32 GetNumLeafImplicits(const FImplicitObject* Implicit)
	{
		int32 NumImplicits = 0;

		if (Implicit != nullptr)
		{
			// All implicit excepts unions have 1 leaf
			NumImplicits = 1;

			if (const FImplicitObjectUnion* Union = Implicit->template AsA<FImplicitObjectUnion>())
			{
				NumImplicits = Union->GetNumLeafObjects();
			}
		}

		return NumImplicits;
	}

	// Get the ShapeInstance data from the particle for the implicit with the specified root object index (its index
	// in the root union implicit if there is one). Usually every implicit in the root union is represented in the
	// ShapesArray, but not always. GCS and ClusterUnions sometimes have a Union at the root but only a single ShapeInstance
	const FShapeInstance* GetShapeInstance(const FShapeInstanceArray& ShapeInstances, const int32 RootObjectIndex)
	{
		const int32 ShapeIndex = (ShapeInstances.IsValidIndex(RootObjectIndex)) ? RootObjectIndex : 0;
		return ShapeInstances[ShapeIndex].Get();
	}


	// Find the LevelSet particles for an ImplicitObject. This used to just be the BVHParticles on the owning Particle,
	// but we can now merge particles with BVHs into a ClusterUnion sp we can end up with LevelSet implicits referencing
	// BVHParticles from other sources. Therefore We need to search the hierarchy for the implicit and if there is an 
	// ImplicitObjectUnionClustered in the hierarchy, it will contain a mapping to the original particle so we can get 
	// its BVHParticles.
	// @todo(chaos): we should rework how BVHParticles are created and stored and fix this
	const FBVHParticles* FindLevelSetParticles(const FGeometryParticleHandle* Particle, const FImplicitObject* Implicit)
	{
		const FBVHParticles* BVHParticles = nullptr;

		if (const FPBDRigidParticleHandle* Rigid = Particle->CastToRigidParticle())
		{
			// Visit the hierarchy in pre-order depth-first traversal. If we hit a UnionClustered, see if it has our particles
			Implicit->VisitObjects(
				[Implicit, &BVHParticles](const FImplicitObject* HierarchyImplicit, const FRigidTransform3& Transform, const int32 RootObjectIndex, const int32 ObjectIndex, const int32 LeafObjectIndex) -> bool
				{
					if (const FImplicitObjectUnionClustered* UnionClustered = HierarchyImplicit->template GetObject<FImplicitObjectUnionClustered>())
					{
						// If this union has our implicit in its map, extract the collision particles and we're done
						if (const FPBDRigidParticleHandle* SourceRigid = UnionClustered->FindParticleForImplicitObject(Implicit))
						{
							BVHParticles = SourceRigid->CollisionParticles().Get();
							return false;
						}
					}

					// This is not the implicit we're looking for. Move along.
					return true;
				});

			// If we didn't find a source particle, it must be ours (if we have any)
			if (BVHParticles == nullptr)
			{
				BVHParticles = Rigid->CollisionParticles().Get();
			}
		}

		return BVHParticles;
	}

	FPBDCollisionConstraintPtr CreateShapePairConstraint(
		FGeometryParticleHandle* Particle0,
		const FPerShapeData* InShape0,
		FGeometryParticleHandle* Particle1,
		const FPerShapeData* InShape1,
		const FReal CullDistance,
		const EContactShapesType ShapePairType,
		const bool bUseManifold,
		const FCollisionContext& Context)
	{
		const FImplicitObject* Implicit0 = InShape0->GetLeafGeometry();
		const FBVHParticles* BVHParticles0 = FConstGenericParticleHandle(Particle0)->CollisionParticles().Get();
		const FRigidTransform3& ShapeRelativeTransform0 = (FRigidTransform3)InShape0->GetLeafRelativeTransform();
		const FImplicitObject* Implicit1 = InShape1->GetLeafGeometry();
		const FBVHParticles* BVHParticles1 = FConstGenericParticleHandle(Particle1)->CollisionParticles().Get();
		const FRigidTransform3& ShapeRelativeTransform1 = (FRigidTransform3)InShape1->GetLeafRelativeTransform();

		return Context.GetAllocator()->CreateConstraint(Particle0, Implicit0, InShape0, BVHParticles0, ShapeRelativeTransform0, Particle1, Implicit1, InShape1, BVHParticles1, ShapeRelativeTransform1, CullDistance, bUseManifold, ShapePairType);
	}

	FPBDCollisionConstraintPtr CreateImplicitPairConstraint(
		FGeometryParticleHandle* Particle0,
		const FImplicitObject* Implicit0,
		const FPerShapeData* Shape0,
		const FBVHParticles* BVHParticles0,
		const FRigidTransform3& ShapeRelativeTransform0,
		FGeometryParticleHandle* Particle1,
		const FImplicitObject* Implicit1,
		const FPerShapeData* Shape1,
		const FBVHParticles* BVHParticles1,
		const FRigidTransform3& ShapeRelativeTransform1,
		const FReal CullDistance,
		const EContactShapesType ShapePairType,
		const bool bUseManifold,
		const FCollisionContext& Context)
	{
		return Context.GetAllocator()->CreateConstraint(Particle0, Implicit0, Shape0, BVHParticles0, ShapeRelativeTransform0, Particle1, Implicit1, Shape1, BVHParticles1, ShapeRelativeTransform1, CullDistance, bUseManifold, ShapePairType);
	}


	////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////


	FSingleShapePairCollisionDetector::FSingleShapePairCollisionDetector(
		FGeometryParticleHandle* InParticle0,
		const FPerShapeData* InShape0,
		FGeometryParticleHandle* InParticle1,
		const FPerShapeData* InShape1,
		const EContactShapesType InShapePairType,
		FParticlePairMidPhase& InMidPhase)
		: MidPhase(InMidPhase)
		, Constraint(nullptr)
		, Particle0(InParticle0)
		, Particle1(InParticle1)
		, Shape0(InShape0)
		, Shape1(InShape1)
		, SphereBoundsCheckSize(0)
		, LastUsedEpoch(-1)
		, ShapePairType(InShapePairType)
		, Flags()
	{
		const FImplicitObject* Implicit0 = Shape0->GetLeafGeometry();
		const FImplicitObject* Implicit1 = Shape1->GetLeafGeometry();
		const bool bHasBounds0 = (Implicit0 != nullptr) && Implicit0->HasBoundingBox();
		const bool bHasBounds1 = (Implicit1 != nullptr) && Implicit1->HasBoundingBox();
		const EImplicitObjectType ImplicitType0 = (Implicit0 != nullptr) ? GetInnerType(Implicit0->GetCollisionType()) : ImplicitObjectType::Unknown;
		const EImplicitObjectType ImplicitType1 = (Implicit1 != nullptr) ? GetInnerType(Implicit1->GetCollisionType()) : ImplicitObjectType::Unknown;
		const bool bIsSphere0 = (ImplicitType0 == ImplicitObjectType::Sphere);
		const bool bIsSphere1 = (ImplicitType1 == ImplicitObjectType::Sphere);
		const bool bIsCapsule0 = (ImplicitType0 == ImplicitObjectType::Capsule);
		const bool bIsCapsule1 = (ImplicitType1 == ImplicitObjectType::Capsule);
		const bool bIsTriangle0 = (ImplicitType0 == ImplicitObjectType::TriangleMesh) || (ImplicitType0 == ImplicitObjectType::HeightField);
		const bool bIsTriangle1 = (ImplicitType1 == ImplicitObjectType::TriangleMesh) || (ImplicitType1 == ImplicitObjectType::HeightField);
		const bool bIsLevelSet = ((ShapePairType == EContactShapesType::LevelSetLevelSet) || (ShapePairType == EContactShapesType::Unknown));

		const bool bAllowBoundsChecked = bChaos_Collision_MidPhase_EnableBoundsChecks && bHasBounds0 && bHasBounds1;
		Flags.bEnableAABBCheck = bAllowBoundsChecked && !(bIsSphere0 && bIsSphere1);	// No AABB test if both are spheres
		Flags.bEnableOBBCheck0 = bAllowBoundsChecked && !bIsSphere0;					// No OBB test for spheres
		Flags.bEnableOBBCheck1 = bAllowBoundsChecked && !bIsSphere1;					// No OBB test for spheres

		if (bAllowBoundsChecked && bIsSphere0 && bIsSphere1)
		{
			SphereBoundsCheckSize = FRealSingle(Implicit0->GetMargin() + Implicit1->GetMargin());	// Sphere-Sphere bounds test
		}

		// Do not try to reuse manifold points for capsules or spheres (against anything)
		// NOTE: This can also be disabled for all shape types by the solver (see GenerateCollisionImpl and the Context)
		Flags.bEnableManifoldUpdate = !bIsSphere0 && !bIsSphere1 && !bIsCapsule0 && !bIsCapsule1 && !bIsTriangle0 && !bIsTriangle1 && !bIsLevelSet;

		// Mark probe flag now so we know which GenerateCollisions to use
		// @todo(chaos): it looks like this can be changed by a collision modifier so we should not be caching it
		Flags.bIsProbe = Shape0->GetIsProbe() || Shape1->GetIsProbe();
	}

	FSingleShapePairCollisionDetector::~FSingleShapePairCollisionDetector()
	{
	}

	FSingleShapePairCollisionDetector::FSingleShapePairCollisionDetector(FSingleShapePairCollisionDetector&& R)
		: MidPhase(R.MidPhase)
		, Constraint(MoveTemp(R.Constraint))
		, Particle0(R.Particle0)
		, Particle1(R.Particle1)
		, Shape0(R.Shape0)
		, Shape1(R.Shape1)
		, SphereBoundsCheckSize(R.SphereBoundsCheckSize)
		, ShapePairType(R.ShapePairType)
		, Flags(R.Flags)
	{
	}

	bool FSingleShapePairCollisionDetector::DoBoundsOverlap(const FReal CullDistance, const int32 CurrentEpoch)
	{
		PHYSICS_CSV_SCOPED_EXPENSIVE(PhysicsVerbose, NarrowPhase_ShapeBounds);

		const FAABB3& ShapeWorldBounds0 = Shape0->GetWorldSpaceInflatedShapeBounds();
		const FAABB3& ShapeWorldBounds1 = Shape1->GetWorldSpaceInflatedShapeBounds();

		// World-space expanded bounds check
		if (Flags.bEnableAABBCheck)
		{
			if (!ShapeWorldBounds0.Intersects(ShapeWorldBounds1))
			{
				return false;
			}
		}

		// World-space sphere bounds check
		if (SphereBoundsCheckSize > FRealSingle(0))
		{
			const FVec3 Separation = ShapeWorldBounds0.GetCenter() - ShapeWorldBounds1.GetCenter();
			const FReal SeparationSq = Separation.SizeSquared();
			const FReal CullDistanceSq = FMath::Square(CullDistance + FReal(SphereBoundsCheckSize));
			if (SeparationSq > CullDistanceSq)
			{
				return false;
			}
		}

		// OBB-AABB test on both directions. This is beneficial for shapes which do not fit their AABBs very well,
		// which includes boxes and other shapes that are not roughly spherical. It is especially beneficial when
		// one shape is long and thin (i.e., it does not fit an AABB well when the shape is rotated).
		// However, it is quite expensive to do this all the time so we only do this test when we did not 
		// collide last frame. This is ok if we assume not much changes from frame to frame, but it means
		// we might call the narrow phase one time too many when shapes become separated.
		const int32 LastEpoch = CurrentEpoch - 1;
		const bool bCollidedLastTick = IsUsedSince(LastEpoch);
		if ((Flags.bEnableOBBCheck0 || Flags.bEnableOBBCheck1) && !bCollidedLastTick)
		{
			const FRigidTransform3& ShapeWorldTransform0 = Shape0->GetLeafWorldTransform(GetParticle0());
			const FRigidTransform3& ShapeWorldTransform1 = Shape1->GetLeafWorldTransform(GetParticle1());
			const FImplicitObject* Implicit0 = Shape0->GetLeafGeometry();
			const FImplicitObject* Implicit1 = Shape1->GetLeafGeometry();

			if (Flags.bEnableOBBCheck0)
			{
				if (!ImplicitOverlapOBBToAABB(Implicit0, Implicit1, ShapeWorldTransform0, ShapeWorldTransform1, CullDistance))
				{
					return false;
				}
			}

			if (Flags.bEnableOBBCheck1)
			{
				if (!ImplicitOverlapOBBToAABB(Implicit1, Implicit0, ShapeWorldTransform1, ShapeWorldTransform0, CullDistance))
				{
					return false;
				}
			}
		}

		return true;
	}

	int32 FSingleShapePairCollisionDetector::GenerateCollision(
		const FReal CullDistance,
		const FReal Dt,
		const FCollisionContext& Context)
	{
		const int32 CurrentEpoch = Context.GetAllocator()->GetCurrentEpoch();
		if (DoBoundsOverlap(CullDistance, CurrentEpoch))
		{
			return GenerateCollisionImpl(CullDistance, Dt, Context);
		}
		return 0;
	}

	int32 FSingleShapePairCollisionDetector::GenerateCollisionCCD(
		const bool bEnableCCDSweep,
		const FReal CullDistance,
		const FReal Dt,
		const FCollisionContext& Context)
	{
		return GenerateCollisionCCDImpl(bEnableCCDSweep, CullDistance, Dt, Context);
	}

	void FSingleShapePairCollisionDetector::CreateConstraint(const FReal CullDistance, const FCollisionContext& Context)
	{
		PHYSICS_CSV_SCOPED_EXPENSIVE(PhysicsVerbose, NarrowPhase_CreateConstraint);
		check(!Constraint.IsValid());

		Constraint = CreateShapePairConstraint(GetParticle0(), Shape0, GetParticle1(), Shape1, CullDistance, ShapePairType, Context.GetSettings().bAllowManifolds, Context);

		const int32 CurrentEpoch = Context.GetAllocator()->GetCurrentEpoch();
		Constraint->GetContainerCookie().MidPhase = &MidPhase;
		Constraint->GetContainerCookie().bIsMultiShapePair = false;
		Constraint->GetContainerCookie().CreationEpoch = CurrentEpoch;
		LastUsedEpoch = -1;
	}

	int32 FSingleShapePairCollisionDetector::GenerateCollisionImpl(
		const FReal CullDistance, 
		const FReal Dt,
		const FCollisionContext& Context)
	{
		if (Flags.bIsProbe)
		{
			return GenerateCollisionProbeImpl(CullDistance, Dt, Context);
		}

		if (!Constraint.IsValid())
		{
			// Lazy creation of the constraint. If a shape pair never gets within CullDistance of each
			// other, we never allocate a constraint for them. Once they overlap, we reuse the constraint
			// until the owing particles are not overlapping. i.e., we keep the constraint even if
			// the shape pairs stop overlapping, reusing it if they start overlapping again.
			CreateConstraint(CullDistance, Context);
		}

		if (Constraint.IsValid())
		{
			PHYSICS_CSV_SCOPED_EXPENSIVE(PhysicsVerbose, NarrowPhase_UpdateConstraint);

			const FRigidTransform3 ShapeWorldTransform0 = Shape0->GetLeafWorldTransform(GetParticle0());
			const FRigidTransform3 ShapeWorldTransform1 = Shape1->GetLeafWorldTransform(GetParticle1());
			const int32 CurrentEpoch = Context.GetAllocator()->GetCurrentEpoch();
			const int32 LastEpoch = CurrentEpoch - 1;
			const bool bWasUpdatedLastTick = IsUsedSince(LastEpoch);

			// Update the world shape transforms on the constraint (we cannot just give it the PerShapeData 
			// pointer because of Unions - see FMultiShapePairCollisionDetector)
			Constraint->SetShapeWorldTransforms(ShapeWorldTransform0, ShapeWorldTransform1);

			Constraint->SetCullDistance(CullDistance);

			// If the constraint was not used last frame, it needs to be reset, otherwise we will try to reuse
			if (!bWasUpdatedLastTick || (Constraint->GetManifoldPoints().Num() == 0))
			{
				// Clear all manifold data including saved contact data
				Constraint->ResetManifold();
			}
			
			bool bWasManifoldRestored = false;
			const bool bAllowManifoldRestore = Context.GetSettings().bAllowManifoldReuse && Flags.bEnableManifoldUpdate;
			if (bAllowManifoldRestore && bWasUpdatedLastTick && Constraint->GetCanRestoreManifold())
			{
				// Update the existing manifold. We can re-use as-is if none of the points have moved much and the bodies have not moved much
				// NOTE: this can succeed in "restoring" even if we have no manifold points
				// NOTE: this uses the transforms from SetLastShapeWorldTransforms, so we can only do this if we were updated last tick
				bWasManifoldRestored = Constraint->TryRestoreManifold();
			}

			if (!bWasManifoldRestored)
			{
				// We are not trying to (or chose not to) reuse manifold points, so reset them but leave stored data intact (for friction)
				Constraint->ResetActiveManifoldContacts();

				if (!Context.GetSettings().bDeferNarrowPhase)
				{
					// Run the narrow phase
					Collisions::UpdateConstraint(*Constraint.Get(), ShapeWorldTransform0, ShapeWorldTransform1, Dt);
				}

				// We will be updating the manifold so update transforms used to check for movement in UpdateAndTryRestoreManifold on future ticks
				// NOTE: We call this after Collisions::UpdateConstraint because it may reset the manifold and reset the bCanRestoreManifold flag.
				// @todo(chaos): Collisions::UpdateConstraint does not need to reset the manifold - fix that
				Constraint->SetLastShapeWorldTransforms(ShapeWorldTransform0, ShapeWorldTransform1);
			}

			// If we have a valid contact, add it to the active list
			// We also add it to the active list if collision detection is deferred because the data will be filled in later and we
			// don't know in advance whether we will pass the Phi check (deferred narrow phase is used with RBAN)
			if (Constraint->GetPhi() <= CullDistance || Context.GetSettings().bDeferNarrowPhase)
			{
				if (Context.GetAllocator()->ActivateConstraint(Constraint.Get()))
				{
					LastUsedEpoch = CurrentEpoch;
					return 1;
				}
			}

			// If we get here, we did not activate the constraint and it should be disabled for this tick
			Constraint->SetDisabled(true);
		}

		return 0;
	}

	int32 FSingleShapePairCollisionDetector::GenerateCollisionCCDImpl(
		const bool bEnableCCDSweep,
		const FReal CullDistance, 
		const FReal Dt,
		const FCollisionContext& Context)
	{
		if (Flags.bIsProbe)
		{
			return GenerateCollisionProbeImpl(CullDistance, Dt, Context);
		}

		if (!Constraint.IsValid())
		{
			// Lazy creation of the constraint. 
			CreateConstraint(CullDistance, Context);

			// Flag this contact as requiring CCD
			Constraint->SetCCDEnabled(true);
		}

		// Do we want to enable the CCD sweep? If not, we fall back to the standard collision detection for this tick
		Constraint->SetCCDSweepEnabled(bEnableCCDSweep);
		if (!bEnableCCDSweep)
		{
			return GenerateCollision(CullDistance, Dt, Context);
		}

		// Swept collision detection
		if (Constraint.IsValid())
		{
			PHYSICS_CSV_SCOPED_EXPENSIVE(PhysicsVerbose, NarrowPhase_UpdateConstraintCCD);

			const FRigidTransform3& ShapeWorldTransform0 = Shape0->GetLeafWorldTransform(GetParticle0());
			const FRigidTransform3& ShapeWorldTransform1 = Shape1->GetLeafWorldTransform(GetParticle1());

			// Update the world shape transforms on the constraint (we cannot just give it the PerShapeData 
			// pointer because of Unions - see FMultiShapePairCollisionDetector)
			// NOTE: these are not used by CCD which continuously moves the particles
			Constraint->SetShapeWorldTransforms(ShapeWorldTransform0, ShapeWorldTransform1);

			Constraint->SetCullDistance(CullDistance);
			Constraint->ResetManifold();
			Constraint->ResetActiveManifoldContacts();

			FConstGenericParticleHandle P0 = GetParticle0();
			FConstGenericParticleHandle P1 = GetParticle1();


			// We need the previous transform for the swept collision detector. It assumes that the current
			// transform has been set on the constraint. 
			// We assume that the particle's center of mass moved in a straight line and that it's rotation has 
			// not changed so we calculate sthe previous transform from the current one and the velocity.
			// NOTE: These are actor transforms, not CoM transforms
			// @todo(chaos): Pass both start and end transforms to the collision detector
			const FRigidTransform3 CCDParticleWorldTransform0 = FRigidTransform3(P0->P() - P0->V() * Dt, P0->Q());
			const FRigidTransform3 CCDParticleWorldTransform1 = FRigidTransform3(P1->P() - P1->V() * Dt, P1->Q());
			const FRigidTransform3 CCDShapeWorldTransform0 = Constraint->GetShapeRelativeTransform0() * CCDParticleWorldTransform0;
			const FRigidTransform3 CCDShapeWorldTransform1 = Constraint->GetShapeRelativeTransform1() * CCDParticleWorldTransform1;
			const bool bDidSweep = Collisions::UpdateConstraintSwept(*Constraint.Get(), CCDShapeWorldTransform0, CCDShapeWorldTransform1, Dt);

#if CHAOS_DEBUG_DRAW
			if (CVars::ChaosSolverDrawCCDInteractions)
			{
				if (FConstGenericParticleHandle(Constraint->GetParticle0())->CCDEnabled())
				{
					DebugDraw::DrawShape(CCDShapeWorldTransform0, Constraint->GetImplicit0(), Shape0, FColor::Black, DebugDraw::ChaosDebugDrawCCDDuration, &CVars::ChaosSolverDebugDebugDrawSettings);
					DebugDraw::DrawShape(ShapeWorldTransform0, Constraint->GetImplicit0(), Shape0, FColor::White, DebugDraw::ChaosDebugDrawCCDDuration, &CVars::ChaosSolverDebugDebugDrawSettings);
				}
				if (FConstGenericParticleHandle(Constraint->GetParticle1())->CCDEnabled())
				{
					DebugDraw::DrawShape(CCDShapeWorldTransform1, Constraint->GetImplicit1(), Shape1, FColor::Black, DebugDraw::ChaosDebugDrawCCDDuration, &CVars::ChaosSolverDebugDebugDrawSettings);
					DebugDraw::DrawShape(ShapeWorldTransform1, Constraint->GetImplicit1(), Shape1, FColor::White, DebugDraw::ChaosDebugDrawCCDDuration, &CVars::ChaosSolverDebugDebugDrawSettings);
				}
			}
#endif

			// If we did not get a sweep hit (TOI > 1) or did not sweep (bDidSweep = false), we need to run standard collision detection at T=1.
			// Likewise, if we did get a sweep hit but it's at TOI = 1, treat this constraint as a regular non-swept constraint and skip the rewind.
			// NOTE: The sweep will report TOI==1 for "shallow" sweep hits below the CCD thresholds in the constraint.
			if ((!bDidSweep) || (Constraint->GetCCDTimeOfImpact() >= FReal(1)))
			{
				// @todo(chaos): should we use a reduced cull distance if we get here? The cull distance will have been set based on movement speed...
				Collisions::UpdateConstraint(*Constraint.Get(), Constraint->GetShapeWorldTransform0(), Constraint->GetShapeWorldTransform1(), Dt);
				Constraint->SetCCDSweepEnabled(false);
			}

			Context.GetAllocator()->ActivateConstraint(Constraint.Get());
			LastUsedEpoch = Context.GetAllocator()->GetCurrentEpoch();

			return 1;
		}

		return 0;
	}

	int32 FSingleShapePairCollisionDetector::GenerateCollisionProbeImpl(
		const FReal CullDistance, 
		const FReal Dt,
		const FCollisionContext& Context)
	{
		// Same as regular constraint generation, but always defer narrow phase.
		// Don't do any initial constraint computations.

		if (!Constraint.IsValid())
		{
			CreateConstraint(CullDistance, Context);
		}

		if (Constraint.IsValid())
		{
			PHYSICS_CSV_SCOPED_EXPENSIVE(PhysicsVerbose, NarrowPhase_UpdateConstraintProbe);

			Context.GetAllocator()->ActivateConstraint(Constraint.Get());
			LastUsedEpoch = Context.GetAllocator()->GetCurrentEpoch();
			return 1;
		}

		return 0;
	}

	void FSingleShapePairCollisionDetector::WakeCollision(const int32 SleepEpoch, const int32 CurrentEpoch)
	{
		if (Constraint.IsValid() && IsUsedSince(SleepEpoch))
		{
			// We need to refresh the epoch so that the constraint state will be used as the previous
			// state if the pair is still colliding in the next tick. However, we are not in the active 
			// collisions array so reset the indices.
			Constraint->GetContainerCookie().LastUsedEpoch = CurrentEpoch;
			Constraint->GetContainerCookie().ConstraintIndex = INDEX_NONE;
			Constraint->GetContainerCookie().CCDConstraintIndex = INDEX_NONE;
			LastUsedEpoch = CurrentEpoch;

			// We have skipped collision detection for this particle because it was asleep, so we need to update the transforms...
			// NOTE: this relies on the shape world transforms being up-to-date. They are usually updated in Integarte which
			// is also skipped for sleeping particles, so they must be updated manually when waking partciles (see IslandManager)
			Constraint->SetShapeWorldTransforms(Shape0->GetLeafWorldTransform(GetParticle0()), Shape1->GetLeafWorldTransform(GetParticle1()));
		}
	}

	// This function is called by the resim system to add/replace a collision with one from the history buffer
	void FSingleShapePairCollisionDetector::SetCollision(const FPBDCollisionConstraint& SourceConstraint, const FCollisionContext& Context)
	{
		const int32 CurrentEpoch = Context.GetAllocator()->GetCurrentEpoch();

		if (!Constraint.IsValid())
		{
			Constraint = Context.GetAllocator()->CreateConstraint();
			Constraint->GetContainerCookie().MidPhase = &MidPhase;
			Constraint->GetContainerCookie().bIsMultiShapePair = false;
			Constraint->GetContainerCookie().CreationEpoch = CurrentEpoch;
		}

		// Copy the constraint data over the existing one (ensure we do not replace data required by the graph and the allocator/container)
		Constraint->RestoreFrom(SourceConstraint);

		// Add the constraint to the active list
		// If the constraint already existed and was already active, this will do nothing
		Context.GetAllocator()->ActivateConstraint(Constraint.Get());
		LastUsedEpoch = CurrentEpoch;
	}


	////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////


	FParticlePairMidPhase::FParticlePairMidPhase(const EParticlePairMidPhaseType InMidPhaseType)
		: MidPhaseType(InMidPhaseType)
		, Flags()
		, Particle0(nullptr)
		, Particle1(nullptr)
		, CullDistanceScale(1)
		, Key()
		, LastUsedEpoch(INDEX_NONE)
		, NumActiveConstraints(0)
		, ParticleCollisionsIndex0(INDEX_NONE)
		, ParticleCollisionsIndex1(INDEX_NONE)
	{
	}

	FParticlePairMidPhase::~FParticlePairMidPhase()
	{
	}

	EParticlePairMidPhaseType FParticlePairMidPhase::CalculateMidPhaseType(FGeometryParticleHandle* InParticle0, FGeometryParticleHandle* InParticle1)
	{
		// For particles with a small number of shapes, we prebuild the set of potentially colliding
		// shape pairs and cache various data with them. However we don't do this if there are a
		// large number of shapes, or if the geometry hierarchy contains Unions of Unions.
		//
		// @todo(chaos): we should be able to support Unions of Unions in the accelerated path
		// when there aren't many objects (with some changes to FSingleShapePairCollisionDetector)

		// We can force use of the Generic midphase (mainly for testing)
		if (!CVars::bChaos_Collision_MidPhase_EnableShapePairs)
		{
			return EParticlePairMidPhaseType::Generic;
		}

		// How many implicits does each particle have?
		const int32 NumImplicits0 = GetNumLeafImplicits(InParticle0->Geometry().Get());
		const int32 NumImplicits1 = GetNumLeafImplicits(InParticle1->Geometry().Get());

		// Do we have a ShapeInstance for every implicit object?
		// Only the implicits in the root union are represented in the shapes array
		const bool bIsTree0 = (NumImplicits0 > InParticle0->ShapeInstances().Num());
		const bool bIsTree1 = (NumImplicits1 > InParticle1->ShapeInstances().Num());

		// Are there too many implicits for the flattened shape pair path to be optimal?
		const int32 MaxImplicitPairs = CVars::Chaos_Collision_MidPhase_MaxShapePairs;
		const bool bTooManyImplicitPairs = (MaxImplicitPairs > 0) && (NumImplicits0 * NumImplicits1 > MaxImplicitPairs);

		// Is either of the implicits a Union of Unions?
		const bool bIsTree = bIsTree0 || bIsTree1;

		const bool bPrebuildShapePairs = !bTooManyImplicitPairs && !bIsTree;
		return bPrebuildShapePairs ? EParticlePairMidPhaseType::ShapePair : EParticlePairMidPhaseType::Generic;
	}

	FParticlePairMidPhase* FParticlePairMidPhase::Make(FGeometryParticleHandle* InParticle0, FGeometryParticleHandle* InParticle1)
	{
		const EParticlePairMidPhaseType MidPhaseType = CalculateMidPhaseType(InParticle0, InParticle1);
		if (MidPhaseType == EParticlePairMidPhaseType::ShapePair)
		{
			return new FShapePairParticlePairMidPhase();
		}
		else
		{
			return new FGenericParticlePairMidPhase();
		}
	}

	void FParticlePairMidPhase::DetachParticle(FGeometryParticleHandle* Particle)
	{
		Reset();

		if (Particle == Particle0)
		{
			Particle0 = nullptr;
		}
		else if (Particle == Particle1)
		{
			Particle1 = nullptr;
		}
	}

	void FParticlePairMidPhase::Reset()
	{
		Flags.bIsActive = true;
		Flags.bIsCCD = false;
		Flags.bIsCCDActive = false;
		Flags.bIsSleeping = false;
		Flags.bIsModified = false;

		ResetImpl();
	}

	void FParticlePairMidPhase::ResetModifications()
	{
		if (Flags.bIsModified)
		{
			Flags.bIsActive = true;
			Flags.bIsCCDActive = Flags.bIsCCD;
			Flags.bIsModified = false;
		}
	}

	void FParticlePairMidPhase::Init(
		FGeometryParticleHandle* InParticle0,
		FGeometryParticleHandle* InParticle1,
		const FCollisionParticlePairKey& InKey,
		const FCollisionContext& Context)
	{
		PHYSICS_CSV_SCOPED_EXPENSIVE(PhysicsVerbose, NarrowPhase_Filter);

		Particle0 = InParticle0;
		Particle1 = InParticle1;
		Key = InKey;

		Flags.bIsActive = true;

		// If CCD is allowed in the current context and for at least one of
		// the particles involved, enable it for this midphase.
		//
		// bIsCCDActive is reset to bIsCCD each frame, but can be overridden
		// by modifiers.
		const bool bIsCCD = Context.GetSettings().bAllowCCD && (
			FConstGenericParticleHandle(Particle0)->CCDEnabled() ||
			FConstGenericParticleHandle(Particle1)->CCDEnabled());
		Flags.bIsCCD = bIsCCD;
		Flags.bIsCCDActive = bIsCCD;

		BuildDetectorsImpl();

		InitThresholds();
	}

	bool FParticlePairMidPhase::ShouldEnableCCD(const FReal Dt)
	{
		// bIsCCDActive is set to bIsCCD at the beginning of every frame, but may be
		// overridden in midphase modification or potentially other systems which run
		// in between mid and narrow phase. bIsCCDActive indicates the final
		// overridden value so we use that here instead of bIsCCD.
		if (Flags.bIsCCDActive)
		{
			FConstGenericParticleHandle ConstParticle0 = FConstGenericParticleHandle(Particle0);
			FConstGenericParticleHandle ConstParticle1 = FConstGenericParticleHandle(Particle1);

			// See comments in FSingleShapePairCollisionDetector::GenerateCollisionCCDImpl for why we use V() here
			FReal LengthCCD = 0;
			FVec3 DirCCD = FVec3(0);
			const FVec3 DeltaX0 = ConstParticle0->V() * Dt;
			const FVec3 DeltaX1 = ConstParticle1->V() * Dt;
			const bool bUseCCD = Collisions::ShouldUseCCD(Particle0, DeltaX0, Particle1, DeltaX1, DirCCD, LengthCCD);

			return bUseCCD;
		}
		return false;
	}

	void FParticlePairMidPhase::InitThresholds()
	{
		// @todo(chaos): improve this threshold calculation for thin objects? Dynamic thin objects have bigger problems so maybe we don't care
		// @todo(chaos): Spheres and capsules need smaller position tolerance - the restore test doesn't work well with rolling
		const bool bIsDynamic0 = FConstGenericParticleHandle(Particle0)->IsDynamic();
		const bool bIsDynamic1 = FConstGenericParticleHandle(Particle1)->IsDynamic();
		const bool bIsBounded0 = Particle0->HasBounds();
		const bool bIsBounded1 = Particle1->HasBounds();

		// NOTE: If CullDistance ends up smaller than the thresholds used to restore collisions, we can end up missing
		// collisions as the objects move if we restore a "zero contact" manifold after movement greater than the cull distance. 
		// Currently this should not happen, but it is not explicitly ensured by the way the thresholds and CullDistanceScale are calculated.
		// @todo(chaos): Add a way to enforce a CullDistance big enough to support the reuse thresholds
		const FReal CullDistanceReferenceSizeInv = FReal(Chaos_Collision_CullDistanceScaleInverseSize);
		const FReal MinCullDistanceScale = FReal(Chaos_Collision_MinCullDistanceScale);
		const FReal MaxBoundsSize0 = (bIsDynamic0 && bIsBounded0) ? Particle0->LocalBounds().Extents().GetMax() : FReal(0);
		const FReal MaxBoundsSize1 = (bIsDynamic1 && bIsBounded1) ? Particle1->LocalBounds().Extents().GetMax() : FReal(0);
		const FReal CullDistanceScale0 = MaxBoundsSize0 * CullDistanceReferenceSizeInv;
		const FReal CullDistanceScale1 = MaxBoundsSize1 * CullDistanceReferenceSizeInv;
		CullDistanceScale = (FRealSingle)FMath::Max3(CullDistanceScale0, CullDistanceScale1, MinCullDistanceScale);
	}

	void FParticlePairMidPhase::GenerateCollisions(
		const FReal InCullDistance,
		const FReal Dt,
		const FCollisionContext& Context)
	{
		//SCOPE_CYCLE_COUNTER(STAT_Collisions_GenerateCollisions);
		PHYSICS_CSV_SCOPED_EXPENSIVE(PhysicsVerbose, DetectCollisions_NarrowPhase);

		if (!IsValid())
		{
			return;
		}

		if (Flags.bIsActive)
		{
			// CullDistance is scaled by the size of the dynamic objects.
			FReal CullDistance = InCullDistance * CullDistanceScale;

			// If CCD is enabled, did we move far enough to require a sweep?
			Flags.bUseSweep = Flags.bIsCCD && ShouldEnableCCD(Dt);

			// Extend cull distance based on velocity
			// NOTE: We use PreV here which is the velocity after collisions from the previous tick because we want the 
			// velocity without gravity from this tick applied. This is mainly so that we get the same CullDistance from 
			// one tick to the next, even if one of the particles goes to sleep, and therefore its velocity is now zero 
			// because gravity is no longer applied. Also see FPBDIslandManager::PropagateIslandSleep for other issues 
			// related to velocity and sleeping...
			const FReal VMax0 = FConstGenericParticleHandle(GetParticle0())->PreV().GetAbsMax();
			const FReal VMax1 = FConstGenericParticleHandle(GetParticle1())->PreV().GetAbsMax();
			const FReal VMaxDt = FMath::Max(VMax0, VMax1) * Dt;
			if (!Flags.bUseSweep)
			{
				// Normal (non sweep) mode: we increase CullDistance based on velocity up to a limit
				// NOTE: This somewhat matches the bounds expansion in FPBDRigidsEvolutionGBF::Integrate
				const FReal VelocityBoundsMultiplier = Context.GetSettings().BoundsVelocityInflation;
				const FReal MaxVelocityBoundsExpansion = Context.GetSettings().MaxVelocityBoundsExpansion;
				if ((VelocityBoundsMultiplier > 0) && (MaxVelocityBoundsExpansion > 0))
				{
					CullDistance += FMath::Min(VelocityBoundsMultiplier * VMaxDt, MaxVelocityBoundsExpansion);
				}
			}
			else
			{
				// CCD (sweept) mode: we increase CullDistance based on velocity, with no limits
				CullDistance += VMaxDt;
			}

			// Run collision detection on all potentially colliding shape pairs
			NumActiveConstraints = GenerateCollisionsImpl(CullDistance, Dt, Context);
		}

		// Reset any modifications applied by the MidPhaseModifier.
		// In principle this belongs at the start of the frame but it is hard to avoid the cache miss there.
		ResetModifications();

		LastUsedEpoch = Context.GetAllocator()->GetCurrentEpoch();
	}

	void FParticlePairMidPhase::InjectCollision(const FPBDCollisionConstraint& Constraint, const FCollisionContext& Context)
	{
		InjectCollisionImpl(Constraint, Context);

		LastUsedEpoch = Context.GetAllocator()->GetCurrentEpoch();
	}

	void FParticlePairMidPhase::SetIsSleeping(const bool bInIsSleeping, const int32 CurrentEpoch)
	{
		// This can be called from two locations:
		// 1)	At the start of the tick as a results of some state change from the game thread such as an explicit wake event,
		//		applying an impulse, or moving a particle.
		// 2)	After the constraint solver phase when we put non-moving islands to sleep.
		// 
		// Note that in both cases there is a collision detection phase before the next constraint solving phase.
		//
		// When awakening we re-activate collisions so that we have a "previous" collision to use for static friction etc.
		// We don't need to do anything when going to sleep because sleeping particles pairs are ignored in collision detection 
		// so the next set of active collisions generated will not contain these collisions.

		if (Flags.bIsSleeping != bInIsSleeping)
		{
			// If we are waking particles, reactivate all collisions that were
			// active when we were put to sleep, i.e., all collisions whose LastUsedEpoch
			// is equal to our LastUsedEpoch.
			const bool bWakingUp = !bInIsSleeping;
			if (bWakingUp)
			{
				if (LastUsedEpoch < CurrentEpoch)
				{
					// Restore all constraints that were active when we were put to sleep
					WakeCollisionsImpl(CurrentEpoch);

					LastUsedEpoch = CurrentEpoch;
				}
			}
			// If we are going to sleep, there is nothing to do (see comments above)

			Flags.bIsSleeping = bInIsSleeping;
		}
	}

	bool FParticlePairMidPhase::IsInConstraintGraph() const
	{
		// @todo(chaos): optimize
		bool bInGraph = false;
		VisitConstCollisions([&bInGraph](const FPBDCollisionConstraint& Constraint)
		{
			if (Constraint.GetConstraintGraphEdge() != nullptr)
			{
				bInGraph = true;
				return ECollisionVisitorResult::Stop;
			}
			return ECollisionVisitorResult::Continue;
		}, false);
		return bInGraph;
	}


	////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////


	FShapePairParticlePairMidPhase::FShapePairParticlePairMidPhase()
		: FParticlePairMidPhase(EParticlePairMidPhaseType::ShapePair)
	{

	}

	void FShapePairParticlePairMidPhase::ResetImpl()
	{
		ShapePairDetectors.Reset();
	}

	void FShapePairParticlePairMidPhase::BuildDetectorsImpl()
	{
		if (IsValid() && (Particle0 != Particle1))
		{
			const FShapesArray& Shapes0 = Particle0->ShapesArray();
			const FShapesArray& Shapes1 = Particle1->ShapesArray();
			for (int32 ShapeIndex0 = 0; ShapeIndex0 < Shapes0.Num(); ++ShapeIndex0)
			{
				const FPerShapeData* Shape0 = Shapes0[ShapeIndex0].Get();
				for (int32 ShapeIndex1 = 0; ShapeIndex1 < Shapes1.Num(); ++ShapeIndex1)
				{
					const FPerShapeData* Shape1 = Shapes1[ShapeIndex1].Get();
					TryAddShapePair(Shape0, Shape1);
				}
			}
		}
	}

	void FShapePairParticlePairMidPhase::TryAddShapePair(const FPerShapeData* Shape0, const FPerShapeData* Shape1)
	{
		const FImplicitObject* Implicit0 = Shape0->GetLeafGeometry();
		const EImplicitObjectType ImplicitType0 = Collisions::GetImplicitCollisionType(Particle0, Implicit0);

		const FImplicitObject* Implicit1 = Shape1->GetLeafGeometry();
		const EImplicitObjectType ImplicitType1 = Collisions::GetImplicitCollisionType(Particle1, Implicit1);

		const bool bDoPassFilter = ShapePairNarrowPhaseFilter(ImplicitType0, Shape0, ImplicitType1, Shape1);
		if (bDoPassFilter)
		{
			bool bSwap = false;
			const EContactShapesType ShapePairType = Collisions::CalculateShapePairType(Particle0, Implicit0, Particle1, Implicit1, bSwap);

			if (ShapePairType != EContactShapesType::Unknown)
			{
				if (!bSwap)
				{
					ShapePairDetectors.Emplace(FSingleShapePairCollisionDetector(Particle0, Shape0, Particle1, Shape1, ShapePairType, *this));
				}
				else
				{
					ShapePairDetectors.Emplace(FSingleShapePairCollisionDetector(Particle1, Shape1, Particle0, Shape0, ShapePairType, *this));
				}
			}
			else
			{
				// If we get here we have a Union of Unions, but that means we should have created 
				// a FGenericParticlePairMidPhase. See CalculateMidPhaseType
				ensure(false);
			}
		}
	}

	int32 FShapePairParticlePairMidPhase::GenerateCollisionsImpl(
		const FReal CullDistance,
		const FReal Dt,
		const FCollisionContext& Context)
	{
		int32 NumActive = 0;
		if (Flags.bIsCCD)
		{
			for (FSingleShapePairCollisionDetector& ShapePair : ShapePairDetectors)
			{
				NumActive += ShapePair.GenerateCollisionCCD(Flags.bUseSweep, CullDistance, Dt, Context);
			}
		}
		else
		{
			for (FSingleShapePairCollisionDetector& ShapePair : ShapePairDetectors)
			{
				NumActive += ShapePair.GenerateCollision(CullDistance, Dt, Context);
			}
		}
		return NumActive;
	}

	void FShapePairParticlePairMidPhase::WakeCollisionsImpl(const int32 CurrentEpoch)
	{
		for (FSingleShapePairCollisionDetector& ShapePair : ShapePairDetectors)
		{
			ShapePair.WakeCollision(LastUsedEpoch, CurrentEpoch);
		}
	}

	void FShapePairParticlePairMidPhase::InjectCollisionImpl(const FPBDCollisionConstraint& Constraint, const FCollisionContext& Context)
	{
		const FPerShapeData* Shape0 = Constraint.GetShape0();
		const FPerShapeData* Shape1 = Constraint.GetShape1();

		// @todo(chaos): fix O(N) search for shape pair - store the index in the cookie (it will be the same
		// as long as the ShapesArray on each particle has not changed)
		for (FSingleShapePairCollisionDetector& ShapePair : ShapePairDetectors)
		{
			if (((Shape0 == ShapePair.GetShape0()) && (Shape1 == ShapePair.GetShape1())) || ((Shape0 == ShapePair.GetShape1()) && (Shape1 == ShapePair.GetShape0())))
			{
				ShapePair.SetCollision(Constraint, Context);
				break;
			}
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////////////


	FGenericParticlePairMidPhase::FGenericParticlePairMidPhase()
		: FParticlePairMidPhase(EParticlePairMidPhaseType::Generic)
	{
	}

	FGenericParticlePairMidPhase::~FGenericParticlePairMidPhase()
	{
	}

	void FGenericParticlePairMidPhase::ResetImpl()
	{
		Constraints.Empty();
		NewConstraints.Empty();
	}

	void FGenericParticlePairMidPhase::BuildDetectorsImpl()
	{
		check(Particle0->Geometry().Get() != nullptr);
		check(Particle1->Geometry().Get() != nullptr);
	}

	int32 FGenericParticlePairMidPhase::GenerateCollisionsImpl(
		const FReal CullDistance,
		const FReal Dt,
		const FCollisionContext& Context)
	{
		const FImplicitObject* Implicit0 = GetParticle0()->Geometry().Get();
		const FImplicitObject* Implicit1 = GetParticle1()->Geometry().Get();

		// See if we have a BVH for either/both of the particles
		const Private::FImplicitBVH* BVH0 = nullptr;
		const Private::FImplicitBVH* BVH1 = nullptr;
		if (CVars::bChaosUnionBVHEnabled)
		{
			if (const FImplicitObjectUnion* Union0 = Implicit0->template AsA<FImplicitObjectUnion>())
			{
				BVH0 = Union0->GetBVH();
			}
			if (const FImplicitObjectUnion* Union1 = Implicit1->template AsA<FImplicitObjectUnion>())
			{
				BVH1 = Union1->GetBVH();
			}
		}

		// Create constraints for all the shape pairs whose bounds overlap.
		// If we have a BVH, use it. Otherwise run a recursive hierarchy sweep.
		// @todo(chaos): if we have 2 BVHs select the deepest one as BVHA?
		if (BVH0 != nullptr)
		{
			GenerateCollisionsBVH(GetParticle0(), BVH0, GetParticle1(), Implicit1, CullDistance, Dt, Context);
		}
		else if (BVH1 != nullptr)
		{
			GenerateCollisionsBVH(GetParticle1(), BVH1, GetParticle0(), Implicit0, CullDistance, Dt, Context);
		}
		else
		{
			GenerateCollisionsImplicit(GetParticle0(), Implicit0, GetParticle1(), Implicit1, CullDistance, Dt, Context);
		}

		// Generate manifolds for each constraint we created/recovered and (re)activate if necessary
		int32 NumActive = ProcessNewConstraints(CullDistance, Dt, Context);

		// @todo(chaos): we could clean up unused collisions between this pair, but probably not worth it
		//PruneConstraints();

		return NumActive;
	}

	// Detect collisions betweena BVH and some other implicit object (which may be a hierarchy)
	void FGenericParticlePairMidPhase::GenerateCollisionsBVH(
		FGeometryParticleHandle* ParticleA, const Private::FImplicitBVH* BVHA,
		FGeometryParticleHandle* ParticleB, const FImplicitObject* RootImplicitB,
		const FReal CullDistance,
		const FReal Dt,
		const FCollisionContext& Context)
	{
		const FConstGenericParticleHandle PA = ParticleA;
		const FConstGenericParticleHandle PB = ParticleB;

		// Particle transforms
		const FRigidTransform3 ParticleWorldTransformA = PA->GetTransformPQ();
		const FRigidTransform3 ParticleWorldTransformB = PB->GetTransformPQ();
		const FRigidTransform3 ParticleTransformAToB = ParticleWorldTransformA.GetRelativeTransform(ParticleWorldTransformB);

		FMemMark Mark(FMemStack::Get());
		TArray<bool, TMemStackAllocator<alignof(bool)>> bIsVisitedA;
		bIsVisitedA.SetNumZeroed(BVHA->GetNumObjects());

		// Visitor for FImplicitBVH::VisitNodeObjects
		// Given an ImplicitObject from ParticleA (which we know overlaps the bounds of some parts of ParticleB),
		// run collision detection on ImplicitA against the implicit object hierarchy of ParticleB.
		// NOTE: may be called with the same ImplicitA multiple times because implicits may be in many BVH leaves
		const auto& ObjectVisitorA = 
			[this, ParticleA, &ParticleWorldTransformA, ParticleB, RootImplicitB, &ParticleWorldTransformB, &ParticleTransformAToB, &bIsVisitedA, CullDistance, Dt, &Context](const FImplicitObject* ImplicitA, const FRigidTransform3f& RelativeTransformfA, const FAABB3f& RelativeBoundsfA, const int32 RootObjectIndexA, const int32 LeafObjectIndexA) -> void
			{
				if (bIsVisitedA[LeafObjectIndexA])
				{
					return;
				}

				bIsVisitedA[LeafObjectIndexA] = true;

				// @todo(chaos): remove this float to double conversion and use floats where possible
				const FRigidTransform3& RelativeTransformA = FRigidTransform3(RelativeTransformfA);
				const FAABB3& RelativeBoundsA = FAABB3(RelativeBoundsfA);

				// If this is the first time we have seen ImplicitA, run the collision detection against ParticleB
				GenerateCollisionsShapeHierarchy(
					ParticleA, ImplicitA, ParticleWorldTransformA, RelativeTransformA, RelativeBoundsA, RootObjectIndexA, LeafObjectIndexA,
					ParticleB, RootImplicitB, ParticleWorldTransformB,
					ParticleTransformAToB,
					CullDistance, Dt, Context);
			};

		// Visitor for FImplicitBVH::VisitHierarchy
		// This will be passed the nodes of the BVH on ParticleA and its bounds and contents. If the bounds overlap something in ParticleB
		// we will keep recursing into the BVH. When we hit a leaf, run collision detection between the leaf contents and ParticleB.
		const auto& NodeVisitorA = 
			[BVHA, RootImplicitB, &ParticleTransformAToB, &ObjectVisitorA, CullDistance](const FAABB3f& NodeBoundsAf, const int32 NodeDepthA, const Private::FImplicitBVHNode& NodeA) -> bool
			{
				const FAABB3 NodeBoundsAInB = FAABB3(NodeBoundsAf).TransformedAABB(ParticleTransformAToB).ThickenSymmetrically(FVec3(CullDistance));

				// Does this Node in A overlap anything in B?
				// NOTE: IsOverlappingBounds performs a deep bounds check, including checking for node overlaps in BVH, Heightfield and TriMesh
				if (!RootImplicitB->IsOverlappingBounds(NodeBoundsAInB))
				{
					// No overlap - stop recursing down this branch
					return false;
				}

				// If we are at a leaf of A, we need to check all objects in the node against B
				if (NodeA.IsLeaf())
				{
					BVHA->VisitNodeObjects(NodeA, ObjectVisitorA);
				}

				// Keep recursing
				return true;
			};

		// Visit all the nodes in BVHA and detect collisions with ParticleB
		BVHA->VisitHierarchy(NodeVisitorA);
	}

	// Detect collisions between two implicits, where either or both may be a hierarchy
	void FGenericParticlePairMidPhase::GenerateCollisionsImplicit(
		FGeometryParticleHandle* ParticleA, const FImplicitObject* RootImplicitA,
		FGeometryParticleHandle* ParticleB, const FImplicitObject* RootImplicitB,
		const FReal CullDistance,
		const FReal Dt,
		const FCollisionContext& Context)
	{
		const FConstGenericParticleHandle PA = ParticleA;
		const FConstGenericParticleHandle PB = ParticleB;

		// Particle transforms
		const FRigidTransform3 ParticleWorldTransformA = PA->GetTransformPQ();
		const FRigidTransform3 ParticleWorldTransformB = PB->GetTransformPQ();

		// Calculate the overlapping volume in each particle's space
		const FRigidTransform3 ParticleTransformAToB = ParticleWorldTransformA.GetRelativeTransform(ParticleWorldTransformB);

		// Visitor for FImplicitBVH::VisitNodeObjects
		// Given an ImplicitObject from ParticleA (which we know overlaps the bounds of some parts of ParticleB),
		// run collision detection on ImplicitA against the implicit object hierarchy of ParticleB.
		// NOTE: may be called with the same ImplicitA multiple times because implicits may be in many BVH leaves
		const auto& ObjectVisitorA =
			[this, ParticleA, &ParticleWorldTransformA, ParticleB, RootImplicitB, &ParticleWorldTransformB, &ParticleTransformAToB, CullDistance, Dt, &Context]
			(const FImplicitObject* ImplicitA, const FRigidTransform3& RelativeTransformA, const int32 RootObjectIndexA, const int32 ObjectIndex, const int32 LeafObjectIndexA)
			{
				const FAABB3 RelativeBoundsA = ImplicitA->CalculateTransformedBounds(RelativeTransformA);

				// If this is the first time we have seen ImplicitA, run the collision detection against ParticleB
				GenerateCollisionsShapeHierarchy(
					ParticleA, ImplicitA, ParticleWorldTransformA, RelativeTransformA, RelativeBoundsA, RootObjectIndexA, LeafObjectIndexA,
					ParticleB, RootImplicitB, ParticleWorldTransformB,
					ParticleTransformAToB,
					CullDistance, Dt, Context);
			};

		// Detect collisons between Implicit Hierarchy of ParticleA and Implicit Hierarchy of ParticleB
		RootImplicitA->VisitLeafObjects(ObjectVisitorA);
	}

	// Generate collisions between lesf (non-hierarchy) implicit and some other (maybe hierarchy) implicit
	void FGenericParticlePairMidPhase::GenerateCollisionsShapeHierarchy(
		FGeometryParticleHandle* ParticleA, const FImplicitObject* ImplicitA, const FRigidTransform3 ParticleWorldTransformA, const FRigidTransform3& RelativeTransformA, const FAABB3& RelativeBoundsA, const int32 RootObjectIndexA, const int32 LeafObjectIndexA,
		FGeometryParticleHandle* ParticleB, const FImplicitObject* RootImplicitB, const FRigidTransform3 ParticleWorldTransformB,
		const FRigidTransform3 ParticleTransformAToB,
		const FReal CullDistance,
		const FReal Dt,
		const FCollisionContext& Context)
	{
		const FShapeInstanceArray& ShapeInstancesA = ParticleA->ShapeInstances();
		const FShapeInstanceArray& ShapeInstancesB = ParticleB->ShapeInstances();

		const FShapeInstance* ShapeInstanceA = GetShapeInstance(ShapeInstancesA, RootObjectIndexA);

		const FAABB3 ShapeBoundsAInB = FAABB3(RelativeBoundsA).TransformedAABB(ParticleTransformAToB).ThickenSymmetrically(FVec3(CullDistance));

		const auto& OverlappingLeafVisitor = 
			[this, ParticleA, ImplicitA, ShapeInstanceA, &ParticleWorldTransformA, &RelativeTransformA, LeafObjectIndexA, ParticleB, &ParticleWorldTransformB, &ShapeInstancesB, CullDistance, Dt, &Context]
			(const FImplicitObject* ImplicitB, const FRigidTransform3& RelativeTransformB, const int32 RootObjectIndexB, const int32 ObjectIndexB, const int32 LeafObjectIndexB)
			{
				const FShapeInstance* ShapeInstanceB = GetShapeInstance(ShapeInstancesB, RootObjectIndexB);

				GenerateCollisionsShapeShape(
					ParticleA, ImplicitA, ShapeInstanceA, ParticleWorldTransformA, RelativeTransformA, LeafObjectIndexA,
					ParticleB, ImplicitB, ShapeInstanceB, ParticleWorldTransformB, RelativeTransformB, LeafObjectIndexB,
					CullDistance, Dt, Context);
			};

		// Detect collisons between ImplicitA and Implicit Hierarchy of ParticleB
		RootImplicitB->VisitOverlappingLeafObjects(ShapeBoundsAInB, OverlappingLeafVisitor);
	}

	// Generate collisions between two leaf (not hierahcies) implicits
	void FGenericParticlePairMidPhase::GenerateCollisionsShapeShape(
		FGeometryParticleHandle* ParticleA, const FImplicitObject* ImplicitA, const FShapeInstance* ShapeInstanceA, const FRigidTransform3 ParticleWorldTransformA, const FRigidTransform3& RelativeTransformA, const int32 LeafObjectIndexA,
		FGeometryParticleHandle* ParticleB, const FImplicitObject* ImplicitB, const FShapeInstance* ShapeInstanceB, const FRigidTransform3 ParticleWorldTransformB, const FRigidTransform3& RelativeTransformB, const int32 LeafObjectIndexB,
		const FReal CullDistance,
		const FReal Dt,
		const FCollisionContext& Context)
	{
		// Check the sim filter to see if these shapes collide
		const EImplicitObjectType ImplicitTypeA = Collisions::GetImplicitCollisionType(ParticleA, ImplicitA);
		const EImplicitObjectType ImplicitTypeB = Collisions::GetImplicitCollisionType(ParticleB, ImplicitB);
		const bool bDoPassFilter = ShapePairNarrowPhaseFilter(ImplicitTypeA, ShapeInstanceA, ImplicitTypeB, ShapeInstanceB);
		if (!bDoPassFilter)
		{
			return;
		}

		// Do the objects bounds overlap?
		const bool bDoOverlap = DoBoundsOverlap(
			ImplicitA, ParticleWorldTransformA, RelativeTransformA,
			ImplicitB, ParticleWorldTransformB, RelativeTransformB,
			CullDistance);

		if (!bDoOverlap)
		{
			return;
		}

		// Find the BVH Particles if we have LevelSet Collision
		// @todo(chaos): This is not really right. The BVHParticles represent the whole
		// geometry hierarchy from the orginating particle (we may be a ClusterUnion)
		// so we shouldn't be doing a collision per LevelSet
		const FBVHParticles* LevelSetParticlesA = nullptr;
		const FBVHParticles* LevelSetParticlesB = nullptr;
		if (ImplicitA->GetCollisionType() == ImplicitObjectType::LevelSet)
		{
			LevelSetParticlesA = FindLevelSetParticles(ParticleA, ImplicitA);
		}
		if (ImplicitB->GetCollisionType() == ImplicitObjectType::LevelSet)
		{
			LevelSetParticlesB = FindLevelSetParticles(ParticleB, ImplicitB);
		}

		// What shape pair type is this (i.e., which contact update function do we call)
		bool bShouldSwapParticles = false;
		const EContactShapesType ShapePairType = Collisions::CalculateShapePairType(ParticleA, ImplicitA, ParticleB, ImplicitB, bShouldSwapParticles);

		// Create the constraint for this shape pair
		// NOTE: this just creates the object. The collision detection is done in ProcessNewConstraints
		if (!bShouldSwapParticles)
		{
			FindOrCreateConstraint(
				ParticleA, ImplicitA, LeafObjectIndexA, ShapeInstanceA, LevelSetParticlesA, RelativeTransformA,
				ParticleB, ImplicitB, LeafObjectIndexB, ShapeInstanceB, LevelSetParticlesB, RelativeTransformB,
				CullDistance, ShapePairType, Context.GetSettings().bAllowManifolds, Flags.bUseSweep, Context);
		}
		else
		{
			FindOrCreateConstraint(
				ParticleB, ImplicitB, LeafObjectIndexB, ShapeInstanceB, LevelSetParticlesB, RelativeTransformB,
				ParticleA, ImplicitA, LeafObjectIndexA, ShapeInstanceA, LevelSetParticlesA, RelativeTransformA,
				CullDistance, ShapePairType, Context.GetSettings().bAllowManifolds, Flags.bUseSweep, Context);
		}
	}

	bool FGenericParticlePairMidPhase::DoBoundsOverlap(
		const FImplicitObject* ImplicitA,
		const FRigidTransform3& ParticleWorldTransformA,
		const FRigidTransform3& ShapeRelativeTransformA,
		const FImplicitObject* ImplicitB,
		const FRigidTransform3& ParticleWorldTransformB,
		const FRigidTransform3& ShapeRelativeTransformB,
		const FReal CullDistance)
	{
		const bool bEnableAABBCheck = true;
		const bool bEnableOBBCheckA = true;
		const bool bEnableOBBCheckB = true;

		const FRigidTransform3 ShapeWorldTransformA = ShapeRelativeTransformA * ParticleWorldTransformA;
		const FRigidTransform3 ShapeWorldTransformB = ShapeRelativeTransformB * ParticleWorldTransformB;

		// NOTE: only expand one bounds by cull distance
		const FAABB3 ShapeWorldBoundsA = ImplicitA->CalculateTransformedBounds(ShapeWorldTransformA).ThickenSymmetrically(FVec3(CullDistance));
		const FAABB3 ShapeWorldBoundsB = ImplicitB->CalculateTransformedBounds(ShapeWorldTransformB);

		// World-space expanded bounds check
		if (bEnableAABBCheck)
		{
			if (!ShapeWorldBoundsA.Intersects(ShapeWorldBoundsB))
			{
				return false;
			}
		}

		// OBB-AABB test in both directions. This is beneficial for shapes which do not fit their AABBs very well,
		// which includes boxes and other shapes that are not roughly spherical. It is especially beneficial when
		// one shape is long and thin (i.e., it does not fit an AABB well when the shape is rotated).
		if (bEnableOBBCheckA)
		{
			if (!ImplicitOverlapOBBToAABB(ImplicitA, ImplicitB, ShapeWorldTransformA, ShapeWorldTransformB, CullDistance))
			{
				return false;
			}
		}

		if (bEnableOBBCheckB)
		{
			if (!ImplicitOverlapOBBToAABB(ImplicitB, ImplicitA, ShapeWorldTransformB, ShapeWorldTransformA, CullDistance))
			{
				return false;
			}
		}

		return true;
	}

	FPBDCollisionConstraint* FGenericParticlePairMidPhase::FindOrCreateConstraint(
		FGeometryParticleHandle* InParticle0,
		const FImplicitObject* InImplicit0,
		const int32 InImplicitId0,
		const FShapeInstance* InShape0,
		const FBVHParticles* InBVHParticles0,
		const FRigidTransform3& InShapeRelativeTransform0,
		FGeometryParticleHandle* InParticle1,
		const FImplicitObject* InImplicit1,
		const int32 InImplicitId1,
		const FShapeInstance* InShape1,
		const FBVHParticles* InBVHParticles1,
		const FRigidTransform3& InShapeRelativeTransform1,
		const FReal CullDistance,
		const EContactShapesType ShapePairType,
		const bool bUseManifold,
		const bool bEnableSweep,
		const FCollisionContext& Context)
	{
		// This is a callback from the low-level collision function. It should always be the same two particles, though the
		// shapes may be some children in the implicit hierarchy. The particles could be in the opposite order though, and
		// this will depend on the shape types involved. E.g., with two particles each with a sphere and a box in a union
		// would require up to two Sphere-Box contacts, with the particles in opposite orders.
		if (!ensure(((InParticle0 == Particle0) && (InParticle1 == Particle1)) || ((InParticle0 == Particle1) && (InParticle1 == Particle0))))
		{
			// We somehow received a callback for the wrong particle pair...this should not happen
			return nullptr;
		}

		const FCollisionParticlePairConstraintKey CollisionKey = FCollisionParticlePairConstraintKey(InShape0, InImplicit0, InImplicitId0, InBVHParticles0, InShape1, InImplicit1, InImplicitId1, InBVHParticles1);
		FPBDCollisionConstraint* Constraint = FindConstraint(CollisionKey);

		// @todo(chaos): fix key uniqueness guarantee.  We need a truly unique key gen function
		const bool bIsKeyCollision = (Constraint != nullptr) && ((Constraint->GetImplicit0() != InImplicit0) || (Constraint->GetImplicit1() != InImplicit1) || (Constraint->GetCollisionParticles0() != InBVHParticles0) || (Constraint->GetCollisionParticles1() != InBVHParticles1));
		if (bIsKeyCollision)
		{
			// If we get here, we have a key collision. The key uses a hash of pointers which is very likely to be unique for different implicit pairs, 
			// especially since it only needs to be unique for this particle pair, but it is not guaranteed.
			// Creating a new constraint with the same key could cause fatal problems (the original constraint will be deleted when we add the new one 
			// to the map, but if it is asleep it will be referenced in the contact graph) so we just abort and accept we will miss collisions. 
			// It is extremely unlikely to happen but we should fix it at some point.
			ensure(false);
			return nullptr;
		}

		if (Constraint == nullptr)
		{
			// NOTE: Using InParticle0 and InParticle1 here because the order may be different to what we have stored
			Constraint = CreateConstraint(InParticle0, InImplicit0, InShape0, InBVHParticles0, InShapeRelativeTransform0, InParticle1, InImplicit1, InShape1, InBVHParticles1, InShapeRelativeTransform1, CullDistance, ShapePairType, bUseManifold, CollisionKey, Context);

			// Is this a CCD constraint?
			Constraint->SetCCDEnabled(IsCCD());
		}

		// Do we want to sweep on this tick? 
		// CCD may be temporarily disabled by the user or because we are moving slowly.
		Constraint->SetCCDSweepEnabled(IsCCD() && bEnableSweep);

		NewConstraints.Add(Constraint);
		return Constraint;
	}

	FPBDCollisionConstraint* FGenericParticlePairMidPhase::FindConstraint(const FCollisionParticlePairConstraintKey& CollisionKey)
	{
		FPBDCollisionConstraintPtr* PConstraint = Constraints.Find(CollisionKey.GetKey());
		if (PConstraint != nullptr)
		{
			return (*PConstraint).Get();
		}
		return nullptr;
	}

	FPBDCollisionConstraint* FGenericParticlePairMidPhase::CreateConstraint(
		FGeometryParticleHandle* InParticle0,
		const FImplicitObject* Implicit0,
		const FPerShapeData* InShape0,
		const FBVHParticles* BVHParticles0,
		const FRigidTransform3& ShapeRelativeTransform0,
		FGeometryParticleHandle* InParticle1,
		const FImplicitObject* Implicit1,
		const FPerShapeData* InShape1,
		const FBVHParticles* BVHParticles1,
		const FRigidTransform3& ShapeRelativeTransform1,
		const FReal CullDistance,
		const EContactShapesType ShapePairType,
		const bool bUseManifold,
		const FCollisionParticlePairConstraintKey& CollisionKey,
		const FCollisionContext& Context)
	{
		PHYSICS_CSV_SCOPED_EXPENSIVE(PhysicsVerbose, NarrowPhase_CreateConstraint);

		FPBDCollisionConstraintPtr Constraint = Context.GetAllocator()->CreateConstraint(
			InParticle0, Implicit0, InShape0, BVHParticles0, ShapeRelativeTransform0,
			InParticle1, Implicit1, InShape1, BVHParticles1, ShapeRelativeTransform1,
			CullDistance, bUseManifold, ShapePairType);

		const int32 CurrentEpoch = Context.GetAllocator()->GetCurrentEpoch();
		Constraint->GetContainerCookie().MidPhase = this;
		Constraint->GetContainerCookie().bIsMultiShapePair = true;
		Constraint->GetContainerCookie().CreationEpoch = CurrentEpoch;

		return Constraints.Add(CollisionKey.GetKey(), MoveTemp(Constraint)).Get();
	}

	void FGenericParticlePairMidPhase::WakeCollisionsImpl(const int32 SleepEpoch, const int32 CurrentEpoch)
	{
		for (auto& KVP : Constraints)
		{
			FPBDCollisionConstraintPtr& Constraint = KVP.Value;
			if (Constraint->GetContainerCookie().LastUsedEpoch >= SleepEpoch)
			{
				Constraint->GetContainerCookie().LastUsedEpoch = CurrentEpoch;
				Constraint->GetContainerCookie().ConstraintIndex = INDEX_NONE;
				Constraint->GetContainerCookie().CCDConstraintIndex = INDEX_NONE;
			}
		}
	}

	int32 FGenericParticlePairMidPhase::ProcessNewConstraints(
		const FReal CullDistance,
		const FReal Dt,
		const FCollisionContext& Context)
	{
		int32 NumActive = 0;

		for (FPBDCollisionConstraint* Constraint : NewConstraints)
		{
			if (!Constraint->GetCCDSweepEnabled())
			{
				UpdateCollision(Constraint, CullDistance, Dt, Context);
			}
			else
			{
				UpdateCollisionCCD(Constraint, CullDistance, Dt, Context);
			}

			if (!Constraint->GetDisabled())
			{
				++NumActive;
			}
		}

		NewConstraints.Reset();

		return NumActive;
	}

	void FGenericParticlePairMidPhase::UpdateCollision(
		FPBDCollisionConstraint* Constraint,
		const FReal CullDistance,
		const FReal Dt,
		const FCollisionContext& Context)
	{
		// @todo(chaos): share this code with FSingleShapePairCollisionDetector

		const FRigidTransform3 ShapeWorldTransform0 = Constraint->GetShapeRelativeTransform0() * FConstGenericParticleHandle(Constraint->GetParticle0())->GetTransformPQ();
		const FRigidTransform3 ShapeWorldTransform1 = Constraint->GetShapeRelativeTransform1() * FConstGenericParticleHandle(Constraint->GetParticle1())->GetTransformPQ();
		const int32 CurrentEpoch = Context.GetAllocator()->GetCurrentEpoch();
		const int32 LastEpoch = CurrentEpoch - 1;
		const bool bWasUpdatedLastTick = (Constraint->GetContainerCookie().LastUsedEpoch >= LastEpoch);

		// Update the world shape transforms on the constraint (we cannot just give it the PerShapeData 
		// pointer because of Unions - see FMultiShapePairCollisionDetector)
		Constraint->SetShapeWorldTransforms(ShapeWorldTransform0, ShapeWorldTransform1);

		Constraint->SetCullDistance(CullDistance);

		// If the constraint was not used last frame, it needs to be reset, otherwise we will try to reuse
		if (!bWasUpdatedLastTick || (Constraint->GetManifoldPoints().Num() == 0))
		{
			// Clear all manifold data including saved contact data
			Constraint->ResetManifold();
		}

		bool bWasManifoldRestored = false;
		//if (Context.GetSettings().bAllowManifoldReuse && Flags.bEnableManifoldUpdate && bWasUpdatedLastTick)
		//{
		//	// Update the existing manifold. We can re-use as-is if none of the points have moved much and the bodies have not moved much
		//	// NOTE: this can succeed in "restoring" even if we have no manifold points
		//	// NOTE: this uses the transforms from SetLastShapeWorldTransforms, so we can only do this if we were updated last tick
		//	bWasManifoldRestored = Constraint->UpdateAndTryRestoreManifold();
		//}
		//else
		{
			// We are not trying to reuse manifold points, so reset them but leave stored data intact (for friction)
			Constraint->ResetActiveManifoldContacts();
		}

		if (!bWasManifoldRestored)
		{
			// We will be updating the manifold so update transforms used to check for movement in UpdateAndTryRestoreManifold on future ticks
			Constraint->SetLastShapeWorldTransforms(ShapeWorldTransform0, ShapeWorldTransform1);

			if (!Context.GetSettings().bDeferNarrowPhase)
			{
				Collisions::UpdateConstraint(*Constraint, Constraint->GetShapeWorldTransform0(), Constraint->GetShapeWorldTransform1(), Dt);
			}
		}

		// If we have a valid contact, add it to the active list
		// We also add it to the active list if collision detection is deferred because the data will be filled in later and we
		// don't know in advance whether we will pass the Phi check (deferred narrow phase is used with RBAN)
		if (Constraint->GetPhi() <= CullDistance || Context.GetSettings().bDeferNarrowPhase)
		{
			if (Context.GetAllocator()->ActivateConstraint(Constraint))
			{
				return;
			}
		}

		// If we get here, we did not activate the constraint and it should be disabled for this tick
		Constraint->SetDisabled(true);
	}


	void FGenericParticlePairMidPhase::UpdateCollisionCCD(
		FPBDCollisionConstraint* Constraint,
		const FReal CullDistance,
		const FReal Dt,
		const FCollisionContext& Context)
	{
		// @todo(chaos): share this code with FSingleShapePairCollisionDetector

		if (!Constraint->GetCCDSweepEnabled())
		{
			return UpdateCollision(Constraint, CullDistance, Dt, Context);
		}

		Constraint->ResetManifold();
		Constraint->ResetActiveManifoldContacts();

		const FRigidTransform3 ShapeWorldTransform0 = Constraint->GetShapeRelativeTransform0() * FConstGenericParticleHandle(Constraint->GetParticle0())->GetTransformPQ();
		const FRigidTransform3 ShapeWorldTransform1 = Constraint->GetShapeRelativeTransform1() * FConstGenericParticleHandle(Constraint->GetParticle1())->GetTransformPQ();

		// Update the world shape transforms on the constraint (we cannot just give it the PerShapeData 
		// pointer because of Unions - see FMultiShapePairCollisionDetector)
		// NOTE: these are not used by CCD which continuously moves the particles
		Constraint->SetShapeWorldTransforms(ShapeWorldTransform0, ShapeWorldTransform1);

		Constraint->SetCullDistance(CullDistance);
		Constraint->ResetManifold();
		Constraint->ResetActiveManifoldContacts();

		FConstGenericParticleHandle P0 = Constraint->GetParticle0();
		FConstGenericParticleHandle P1 = Constraint->GetParticle1();

		// We need the previous transform for the swept collision detector. It assumes that the current
		// transform has been set on the constraint. 
		// We assume that the particle's center of mass moved in a stright line and that it's rotation has 
		// not changed so we calculate the previous transform from the current one and the velocity.
		// NOTE: These are actor transforms, not CoM transforms
		// @todo(chaos): Pass both start and end transforms to the collision detector
		const FRigidTransform3 CCDParticleWorldTransform0 = FRigidTransform3(P0->P() - P0->V() * Dt, P0->Q());
		const FRigidTransform3 CCDParticleWorldTransform1 = FRigidTransform3(P1->P() - P1->V() * Dt, P1->Q());
		const FRigidTransform3 CCDShapeWorldTransform0 = Constraint->GetShapeRelativeTransform0() * CCDParticleWorldTransform0;
		const FRigidTransform3 CCDShapeWorldTransform1 = Constraint->GetShapeRelativeTransform1() * CCDParticleWorldTransform1;
		const bool bDidSweep = Collisions::UpdateConstraintSwept(*Constraint, CCDShapeWorldTransform0, CCDShapeWorldTransform1, Dt);

#if CHAOS_DEBUG_DRAW
		if (CVars::ChaosSolverDrawCCDInteractions)
		{
			if (FConstGenericParticleHandle(Constraint->GetParticle0())->CCDEnabled())
			{
				DebugDraw::DrawShape(CCDShapeWorldTransform0, Constraint->GetImplicit0(), Constraint->GetShape0(), FColor::Black, DebugDraw::ChaosDebugDrawCCDDuration, &CVars::ChaosSolverDebugDebugDrawSettings);
				DebugDraw::DrawShape(ShapeWorldTransform0, Constraint->GetImplicit0(), Constraint->GetShape0(), FColor::White, DebugDraw::ChaosDebugDrawCCDDuration, &CVars::ChaosSolverDebugDebugDrawSettings);
			}
			if (FConstGenericParticleHandle(Constraint->GetParticle1())->CCDEnabled())
			{
				DebugDraw::DrawShape(CCDShapeWorldTransform1, Constraint->GetImplicit1(), Constraint->GetShape1(), FColor::Black, DebugDraw::ChaosDebugDrawCCDDuration, &CVars::ChaosSolverDebugDebugDrawSettings);
				DebugDraw::DrawShape(ShapeWorldTransform1, Constraint->GetImplicit1(), Constraint->GetShape1(), FColor::White, DebugDraw::ChaosDebugDrawCCDDuration, &CVars::ChaosSolverDebugDebugDrawSettings);
			}
		}
#endif

		// If we did not get a sweep hit (TOI > 1) or did not sweep (bDidSweep = false), we need to run standard collision detection at T=1.
		// Likewise, if we did get a sweep hit but it's at TOI = 1, treat this constraint as a regular non-swept constraint and skip the rewind.
		// NOTE: The sweep will report TOI==1 for "shallow" sweep hits below the CCD thresholds in the constraint.
		if ((!bDidSweep) || (Constraint->GetCCDTimeOfImpact() >= FReal(1)))
		{
			Collisions::UpdateConstraint(*Constraint, Constraint->GetShapeWorldTransform0(), Constraint->GetShapeWorldTransform1(), Dt);
			Constraint->SetCCDSweepEnabled(false);
		}

		Context.GetAllocator()->ActivateConstraint(Constraint);
	}

	void FGenericParticlePairMidPhase::PruneConstraints(const int32 CurrentEpoch)
	{
		// Must call ProcessNewCollisions before Prune
		check(NewConstraints.Num() == 0);

		// Find all the expired collisions
		FMemMark Mark(FMemStack::Get());
		TArray<uint32, TMemStackAllocator<alignof(uint32)>> Pruned;
		Pruned.Reserve(Constraints.Num());

		for (auto& KVP : Constraints)
		{
			const uint32 CollisionKey = KVP.Key;
			FPBDCollisionConstraintPtr& Constraint = KVP.Value;

			// NOTE: Constraints in sleeping islands should be kept alive. They will still be in the graph
			// and will be restored when the island wakes.
			if ((Constraint->GetContainerCookie().LastUsedEpoch < CurrentEpoch) && !Constraint->IsInConstraintGraph())
			{
				Pruned.Add(CollisionKey);
			}
		}

		// Destroy expired collisions
		for (uint32 CollisionKey : Pruned)
		{
			Constraints.Remove(CollisionKey);
		}
	}

	void FGenericParticlePairMidPhase::WakeCollisionsImpl(const int32 CurrentEpoch)
	{
		for (auto& KVP : Constraints)
		{
			FPBDCollisionConstraintPtr& Constraint = KVP.Value;
			if (Constraint->GetContainerCookie().LastUsedEpoch >= LastUsedEpoch)
			{
				Constraint->GetContainerCookie().LastUsedEpoch = CurrentEpoch;
				Constraint->GetContainerCookie().ConstraintIndex = INDEX_NONE;
				Constraint->GetContainerCookie().CCDConstraintIndex = INDEX_NONE;
			}
		}
	}

	void FGenericParticlePairMidPhase::InjectCollisionImpl(const FPBDCollisionConstraint& Constraint, const FCollisionContext& Context)
	{
		// @todo(chaos): support rewind/resim here
		ensure(false);
	}
}


