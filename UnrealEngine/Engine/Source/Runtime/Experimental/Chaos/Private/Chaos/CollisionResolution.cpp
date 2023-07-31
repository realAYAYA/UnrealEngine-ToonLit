// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/CollisionResolution.h"

#include "Chaos/ChaosPerfTest.h"
#include "Chaos/Capsule.h"
#include "Chaos/CollisionResolutionTypes.h"
#include "Chaos/CollisionResolutionUtil.h"
#include "Chaos/Collision/CapsuleConvexContactPoint.h"
#include "Chaos/Collision/CollisionContext.h"
#include "Chaos/Collision/CollisionConstraintAllocator.h"
#include "Chaos/Collision/CollisionFilter.h"
#include "Chaos/Collision/PBDCollisionConstraint.h"
#include "Chaos/Collision/SphereConvexContactPoint.h"
#include "Chaos/Collision/ContactPointsMiscShapes.h"
#include "Chaos/Collision/GJKContactPointSwept.h"
#include "Chaos/Collision/GJKContactPoint.h"
#include "Chaos/Convex.h"
#include "Chaos/Defines.h"
#include "Chaos/HeightField.h"
#include "Chaos/ImplicitFwd.h"
#include "Chaos/ImplicitObjectScaled.h"
#include "Chaos/ImplicitObjectTransformed.h"
#include "Chaos/Levelset.h"
#include "Chaos/Pair.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/Particle/ParticleUtilities.h"
#include "Chaos/PBDCollisionConstraints.h"
#include "Chaos/Sphere.h"
#include "Chaos/Transform.h"
#include "Chaos/TriangleMeshImplicitObject.h"
#include "Chaos/GeometryQueries.h"
#include "Chaos/CollisionOneShotManifolds.h"
#include "Chaos/CollisionOneShotManifoldsMiscShapes.h"
#include "Chaos/CCDUtilities.h"

//PRAGMA_DISABLE_OPTIMIZATION


// @todo(chaos): clean up the contact creation time rejection to avoid extra transforms
#define CHAOS_COLLISION_CREATE_BOUNDSCHECK 1

DECLARE_CYCLE_STAT(TEXT("Collisions::ConstructConstraints"), STAT_Collisions_ConstructConstraints, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::ConstructConstraintsInternal"), STAT_Collisions_ConstructConstraintsInternal, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::FindAllIntersectingClusteredObjects"), STAT_Collisions_FindAllIntersectingClusteredObjects, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::ConstructGenericConvexConvexConstraints"), STAT_Collisions_ConstructGenericConvexConvexConstraints, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::ConstructGenericConvexConvexConstraintsSwept"), STAT_Collisions_ConstructGenericConvexConvexConstraintsSwept, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::UpdateConstraintFromGeometryInternal"), STAT_Collisions_UpdateConstraintFromGeometryInternal, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::UpdateGenericConvexConvexConstraint"), STAT_Collisions_UpdateGenericConvexConvexConstraint, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::UpdateConvexTriangleMeshConstraint"), STAT_Collisions_UpdateConvexTriangleMeshConstraint, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::UpdateConvexTriangleMeshConstraintSwept"), STAT_Collisions_UpdateConvexTriangleMeshConstraintSwept, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::UpdateConvexHeightFieldConstraint"), STAT_Collisions_UpdateConvexHeightFieldConstraint, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::ConstructConvexHeightFieldConstraints"), STAT_Collisions_ConstructConvexHeightFieldConstraints, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::ConstructConvexHeightFieldConstraintsSwept"), STAT_Collisions_ConstructConvexHeightFieldConstraintsSwept, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::ConstructConvexTriangleMeshConstraints"), STAT_Collisions_ConstructConvexTriangleMeshConstraints, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::ConstructConvexTriangleMeshConstraintsSwept"), STAT_Collisions_ConstructConvexTriangleMeshConstraintsSwept, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::ConstructCapsuleTriangleMeshConstraintsSwept"), STAT_Collisions_ConstructCapsuleTriangleMeshConstraintsSwept, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::ConstructCapsuleTriangleMeshConstraints"), STAT_Collisions_ConstructCapsuleTriangleMeshConstraints, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::ConstructCapsuleHeightFieldConstraints"), STAT_Collisions_ConstructCapsuleHeightFieldConstraints, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::ConstructCapsuleHeightFieldConstraintsSwept"), STAT_Collisions_ConstructCapsuleHeightFieldConstraintsSwept, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::UpdateGenericConvexConvexConstraintSwept"), STAT_Collisions_UpdateGenericConvexConvexConstraintSwept, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::UpdateCapsuleHeightFieldConstraintSwept"), STAT_Collisions_UpdateCapsuleHeightFieldConstraintSwept, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::UpdateCapsuleHeightFieldConstraint"), STAT_Collisions_UpdateCapsuleHeightFieldConstraint, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::UpdateCapsuleTriangleMeshConstraint"), STAT_Collisions_UpdateCapsuleTriangleMeshConstraint, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::UpdateCapsuleTriangleMeshConstraintSwept"), STAT_Collisions_UpdateCapsuleTriangleMeshConstraintSwept, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::UpdateConvexHeightFieldConstraintSwept"), STAT_Collisions_UpdateConvexHeightFieldConstraintSwept, STATGROUP_ChaosCollision);
DECLARE_CYCLE_STAT(TEXT("Collisions::UpdateLevelsetLevelsetConstraint"), STAT_UpdateLevelsetLevelsetConstraint, STATGROUP_ChaosCollision);

// Stat Collision counters (need to be reset every advance)
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NumParticlePairs"), STAT_ChaosCollisionCounter_NumParticlePairs, STATGROUP_ChaosCollisionCounters);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NumShapePairs"), STAT_ChaosCollisionCounter_NumShapePairs, STATGROUP_ChaosCollisionCounters);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NumContactsCreated"), STAT_ChaosCollisionCounter_NumContactsCreated, STATGROUP_ChaosCollisionCounters);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("NumContactUpdates"), STAT_ChaosCollisionCounter_NumContactUpdates, STATGROUP_ChaosCollisionCounters);

int32 CCDUseGenericSweptConvexConstraints = 1;
FAutoConsoleVariableRef CVarUseGenericSweptConvexConstraints(TEXT("p.Chaos.CCD.UseGenericSweptConvexConstraints"), CCDUseGenericSweptConvexConstraints, TEXT("Use generic convex convex swept constraint generation for convex shape pairs which don't have specialized implementations."));

int32 CCDOnlyConsiderDynamicStatic = 0;
FAutoConsoleVariableRef CVarCCDOnlyConsiderDynamicStatic(TEXT("p.Chaos.CCD.OnlyConsiderDynamicStatic"), CCDOnlyConsiderDynamicStatic, TEXT("Only enable CCD for dynamic-static pairs."));

int32 ConstraintsDetailedStats = 0;
FAutoConsoleVariableRef CVarConstraintsDetailedStats(TEXT("p.Chaos.Constraints.DetailedStats"), ConstraintsDetailedStats, TEXT("When set to 1, will enable more detailed stats."));

bool bChaos_Collision_AllowLevelsetManifolds = true;
FAutoConsoleVariableRef CVarChaosCollisionAllowLevelsetManifolds(TEXT("p.Chaos.Collision.AllowLevelsetManifolds"), bChaos_Collision_AllowLevelsetManifolds, TEXT("Use incremental manifolds for levelset-levelset collision. This does not work well atm - too much rotation in the small pieces"));

bool CCDNoCullAllShapePairs = true;
FAutoConsoleVariableRef CVarCCDNoCullAllShapePairs(TEXT("p.Chaos.CCD.NoCullAllShapePairs"), CCDNoCullAllShapePairs, TEXT("Whether to cull contacts early based on phi for sweeps for all shape pairs (not just convex convex)."));

bool Chaos_Collision_NarrowPhase_SphereBoundsCheck = true;
bool Chaos_Collision_NarrowPhase_AABBBoundsCheck = true;
FAutoConsoleVariableRef CVarChaosCollisionSphereBoundsCheck(TEXT("p.Chaos.Collision.SphereBoundsCheck"), Chaos_Collision_NarrowPhase_SphereBoundsCheck, TEXT(""));
FAutoConsoleVariableRef CVarChaosCollisionAABBBoundsCheck(TEXT("p.Chaos.Collision.AABBBoundsCheck"), Chaos_Collision_NarrowPhase_AABBBoundsCheck, TEXT(""));

bool bChaos_Collision_ShapesArrayMode = true;
FAutoConsoleVariableRef CVarChaos_Collision_ShapesArrayMode(TEXT("p.Chaos.Collision.ShapesArrayMode"), bChaos_Collision_ShapesArrayMode, TEXT(""));

bool bChaos_Collision_SortParticlesOnConstraintConstruct = true;
FAutoConsoleVariableRef CVarChaos_Collision_SortParticlesOnConstraintConstruct(TEXT("p.Chaos.Collision.SortParticlesOnConstraintConstruct"), bChaos_Collision_SortParticlesOnConstraintConstruct, TEXT(""));

namespace Chaos
{
	namespace Collisions
	{
		void ResetChaosCollisionCounters()
		{
			SET_DWORD_STAT(STAT_ChaosCollisionCounter_NumParticlePairs, 0);
			SET_DWORD_STAT(STAT_ChaosCollisionCounter_NumShapePairs, 0);
			SET_DWORD_STAT(STAT_ChaosCollisionCounter_NumContactsCreated, 0);
			SET_DWORD_STAT(STAT_ChaosCollisionCounter_NumContactUpdates, 0);
		}

		// Traits to control how contacts are generated
		template<bool B_IMMEDIATEUPDATE>
		struct TConstructCollisionTraits
		{
			// If true, ConstructConstraints also initializes the constraint Phi and Normal based on current state, and
			// contacts beyond CullDistance are culled. If false, a constraint is created for every shape pair.
			// NOTE: Contact Phi and Normal are also calculated at the beginning of each iteration as well. The reason you may
			// want bImmediateUpdate=true is to reduce the number of contacts that make it to the constraint graph (because
			// we have culled those with a separation greater than CullDistance).
			// However, early culling can cause collisions to be missed, e.g., if a joint moves a body into a space where
			// a culled collision should now be active. This can (does) lead to jitter in some cases.
			static const bool bImmediateUpdate = B_IMMEDIATEUPDATE;
		};

		/** Determines if body should use CCD. If using CCD, computes Dir and Length of sweep.
		* If either of the object is CCD-enabled and fast moving, we enable CCD for the constraint.
		* We compare P-X and BoundingBox().Extents().Min() to determine if an object is fast-moving or not. Here is how this works:
		* Think about a box colliding with a static plane. If P-X <= 0.5 * BoundingBox().Extents().Min(), it is impossible for the box to tunnel through the plane. Otherwise, it is possible but not necessary to have tunneling.
		* Now think about two boxes. If P-X <= 0.5 * BoundingBox().Extents().Min() both boxes, it is impossible for one box to tunnel through the other (it is still possible for a box's corner to move through another box's corner).
		* So far this is sufficient for us to determine CCD constraints for our primitives. But the caveat is that for arbitrary objects (think about a rod that is not axis aligned), min of a bounding box extent can be too large and tunneling could still happen if P-X falls below this threshold.
		*/
		bool ShouldUseCCD(const FGeometryParticleHandle* Particle0, const FVec3& DeltaX0, const FGeometryParticleHandle* Particle1, const FVec3& DeltaX1, FVec3& Dir, FReal& Length)
		{
			if (CCDOnlyConsiderDynamicStatic > 0 && Particle1->ObjectState() != EObjectStateType::Static)
			{
				return false;
			}

			const TPBDRigidParticleHandle<FReal, 3>* Rigid0 = Particle0->CastToRigidParticle();
			const bool bIsCCDEnabled0 = Rigid0 && Rigid0->CCDEnabled();
			const TPBDRigidParticleHandle<FReal, 3>* Rigid1 = Particle1->CastToRigidParticle();
			const bool bIsCCDEnabled1 = Rigid1 && Rigid1->CCDEnabled();
			if (!bIsCCDEnabled0 && !bIsCCDEnabled1)
			{
				return false;
			}

			bool bUseCCD = CCDHelpers::DeltaExceedsThreshold(
				Particle0->CCDAxisThreshold(), DeltaX0, FConstGenericParticleHandle(Particle0)->Q(),
				Particle1->CCDAxisThreshold(), DeltaX1, FConstGenericParticleHandle(Particle1)->Q());

			if (bUseCCD)
			{
				Dir = DeltaX0 - DeltaX1;
				Length = Dir.SafeNormalize();
				if (Length < UE_SMALL_NUMBER) // This is the case where both particles' velocities are high but the relative velocity is low. 
				{
					bUseCCD = false;
				}

				// Do not perform CCD if the vector is not close to unit length to prevent getting caught in a large or infinite loop when raycasting.
				// @todo(chaos): What's this? Dir is normalized above so this seems unnecessary unless it is checking for 0 length, not unit length - remove it or fix
				if (!FMath::IsNearlyEqual((float)Dir.SizeSquared(), 1.f, UE_KINDA_SMALL_NUMBER))
				{
					bUseCCD = false;
				}
			}

			return bUseCCD;
		}

		// Add the contact to the manifold (or update the existing point). Disable the contact if the contact distance is greater than the Cull Distance
		void UpdateContactPoint(FPBDCollisionConstraint& Constraint, const FContactPoint& ContactPoint, const FReal Dt)
		{
			// Permanently disable contacts beyond the CullDistance
			// NOTE: We cannot do this for incremental manifolds because of a hack in the heightfield collision detection.
			// See FHeightField::GJKContactPointImp where it uses a sweep.
#if CHAOS_COLLISION_CREATE_BOUNDSCHECK
			const bool bIsIncrementalManifold = Constraint.GetUseManifold() && (Constraint.GetManifoldPoints().Num() > 0);
			if ((ContactPoint.Phi > Constraint.GetCullDistance()) && !bIsIncrementalManifold)
			{
				Constraint.SetDisabled(true);
				return;
			}
#endif

			// Ignore points that have not been initialized - i.e., if there is no detectable contact 
			// point within reasonable range despite passing the AABB tests
			if (ContactPoint.IsSet())
			{
				Constraint.AddIncrementalManifoldContact(ContactPoint);
			}
			else
			{
				// If we did not add a new contact point, select the best existing one
				// We only hit this because of the FHeightField::GJKContactPointImp hack mentioned above
				// @todo(chaos): fix heightfield collision and get rid of this
				if (Constraint.GetUseManifold())
				{
					Constraint.UpdateManifoldContacts();
				}
			}
		}

		// Same as UpdateContact Point but without checking CullDistance. Used by CCD because sweeps do not set the separation unless the sweep actually hits
		void UpdateContactPointNoCull(FPBDCollisionConstraint& Constraint, const FContactPoint& ContactPoint, const FReal Dt, const bool bNoCull = true)
		{
			if (!bNoCull)
			{
				if (ContactPoint.Phi > Constraint.GetCullDistance())
				{
					Constraint.SetDisabled(true);
					return;
				}
			}

			if (ContactPoint.IsSet())
			{
				Constraint.AddIncrementalManifoldContact(ContactPoint);
			}
		}

		// This is pretty unnecessary - all instanced shapes have the same implementation so we should be able to
		// collapse this switch into a generic call. Maybe add a base class to TImplicitObjectInstanced.
		inline const FImplicitObject* GetInstancedImplicit(const FImplicitObject* Implicit0)
		{
			EImplicitObjectType Implicit0OuterType = Implicit0->GetType();

			if (Implicit0OuterType == TImplicitObjectInstanced<FConvex>::StaticType())
			{
				return Implicit0->template GetObject<const TImplicitObjectInstanced<FConvex>>()->GetInstancedObject();
			}
			else if (Implicit0OuterType == TImplicitObjectInstanced<TBox<FReal, 3>>::StaticType())
			{
				return Implicit0->template GetObject<const TImplicitObjectInstanced<TBox<FReal, 3>>>()->GetInstancedObject();
			}
			else if (Implicit0OuterType == TImplicitObjectInstanced<FCapsule>::StaticType())
			{
				return Implicit0->template GetObject<const TImplicitObjectInstanced<FCapsule>>()->GetInstancedObject();
			}
			else if (Implicit0OuterType == TImplicitObjectInstanced<TSphere<FReal, 3>>::StaticType())
			{
				return Implicit0->template GetObject<const TImplicitObjectInstanced<TSphere<FReal, 3>>>()->GetInstancedObject();
			}
			else if (Implicit0OuterType == TImplicitObjectInstanced<FConvex>::StaticType())
			{
				return Implicit0->template GetObject<const TImplicitObjectInstanced<FConvex>>()->GetInstancedObject();
			}
			else if (Implicit0OuterType == TImplicitObjectInstanced<FTriangleMeshImplicitObject>::StaticType())
			{
				return Implicit0->template GetObject<const TImplicitObjectInstanced<FTriangleMeshImplicitObject>>()->GetInstancedObject();
			}

			return nullptr;
		}


		template<typename T_IMPLICITA>
		struct TConvexImplicitTraits
		{
			static const bool bSupportsOneShotManifold = false;
		};

		template<>
		struct TConvexImplicitTraits<FImplicitBox3>
		{
			static const bool bSupportsOneShotManifold = true;
		};
		template<>
		struct TConvexImplicitTraits<FImplicitConvex3>
		{
			static const bool bSupportsOneShotManifold = true;
		};
		template<>
		struct TConvexImplicitTraits<TImplicitObjectInstanced<FImplicitConvex3>>
		{
			static const bool bSupportsOneShotManifold = true;
		};
		template<>
		struct TConvexImplicitTraits<TImplicitObjectScaled<FImplicitConvex3>>
		{
			static const bool bSupportsOneShotManifold = true;
		};

		// Convex pair type traits
		template<typename T_IMPLICITA, typename T_IMPLICITB>
		struct TConvexImplicitPairTraits
		{
			// Whether the pair types should use a manifold
			static const bool bSupportsOneShotManifold = TConvexImplicitTraits<T_IMPLICITA>::bSupportsOneShotManifold && TConvexImplicitTraits<T_IMPLICITB>::bSupportsOneShotManifold;
		};

		// Convex pair type traits
		template<typename T_IMPLICITB>
		struct TCapsuleImplicitPairTraits
		{
			// Whether the Capsules can use the generic path. If false, a specialized path exists
			static const bool bSupportsGenericOneShotManifoldPath = false;
		};

		// Convex pair type traits
		template<>
		struct TCapsuleImplicitPairTraits<FConvex>
		{
			static const bool bSupportsGenericOneShotManifoldPath = true;
		};

		// Convex pair type traits
		template<>
		struct TCapsuleImplicitPairTraits<TImplicitObjectScaled<FConvex, 1>>
		{
			static const bool bSupportsGenericOneShotManifoldPath = true;
		};

		// Convex pair type traits
		template<>
		struct TCapsuleImplicitPairTraits<class TImplicitObjectInstanced<FConvex>>
		{
			static const bool bSupportsGenericOneShotManifoldPath = true;
		};

		// Convex pair type traits		

		// Use the traits to call the appropriate convex-convex update method. 
		// Either incremental manifold (default) or one-shot manifold
		template<bool T_SUPPORTSONESHOTMANIFOLD = false>
		struct TConvexConvexUpdater
		{
			template<typename T_ImplicitA, typename T_ImplicitB>
			static void UpdateConvexConvexConstraint(const T_ImplicitA& A, const FRigidTransform3& ATM, const T_ImplicitB& B, const FRigidTransform3& BTM, const FReal Dt, FPBDCollisionConstraint& Constraint)
			{
				UpdateContactPoint(Constraint, GJKContactPoint(A, ATM, B, BTM, FVec3(1, 0, 0)), Dt);
			}
		};

		// One-shot manifold convex-convex update
		template<>
		struct TConvexConvexUpdater<true>
		{
			template<typename T_ImplicitA, typename T_ImplicitB>
			static void UpdateConvexConvexConstraint(const T_ImplicitA& A, const FRigidTransform3& ATM, const T_ImplicitB& B, const FRigidTransform3& BTM, const FReal Dt, FPBDCollisionConstraint& Constraint)
			{
				if (Constraint.GetUseManifold())
				{
					ConstructConvexConvexOneShotManifold(A, ATM, B, BTM, Dt, Constraint);
				}
				else
				{
					UpdateContactPoint(Constraint, GJKContactPoint(A, ATM, B, BTM, FVec3(1, 0, 0)), Dt);
				}
			}
		};

		// Another helper required by UpdateGenericConvexConvexConstraintHelper which uses CastHelper and does not have any typedefs 
		// for the concrete implicit types, so we need to rely on type deduction from the compiler.
		struct FConvexConvexUpdaterCaller
		{
			template<typename T_ImplicitA, typename T_ImplicitB>
			static void Update(const T_ImplicitA& A, const FRigidTransform3& ATM, const T_ImplicitB& B, const FRigidTransform3& BTM, const FReal Dt, FPBDCollisionConstraint& Constraint)
			{
				using FConvexConvexUpdater = TConvexConvexUpdater<TConvexImplicitPairTraits<T_ImplicitA, T_ImplicitB>::bSupportsOneShotManifold>;
				FConvexConvexUpdater::UpdateConvexConvexConstraint(A, ATM, B, BTM, Dt, Constraint);
			}
		};


		// Unwrap the many convex types, including scaled, and call the appropriate update which depends on the concrete types
		void UpdateGenericConvexConvexConstraintHelper(const FImplicitObject& A, const FRigidTransform3& ATM, const FImplicitObject& B, const FRigidTransform3& BTM, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			// This expands to a switch of switches that calls the inner function with the appropriate concrete implicit types
			Utilities::CastHelperNoUnwrap(A, ATM, [&](const auto& ADowncast, const FRigidTransform3& AFullTM)
			{
				Utilities::CastHelperNoUnwrap(B, BTM, [&](const auto& BDowncast, const FRigidTransform3& BFullTM)
				{
					FConvexConvexUpdaterCaller::Update(ADowncast, AFullTM, BDowncast, BFullTM, Dt, Constraint);
				});
			});
		}


		template <typename TPGeometryClass>
		const TPGeometryClass* GetInnerObject(const FImplicitObject& Geometry)
		{
			if (const TImplicitObjectScaled<TPGeometryClass>* ScaledConvexImplicit = Geometry.template GetObject<const TImplicitObjectScaled<TPGeometryClass> >())
				return (Geometry.template GetObject<const TImplicitObjectScaled<TPGeometryClass> >())->GetUnscaledObject();
			else if (const TImplicitObjectInstanced<TPGeometryClass>* InstancedImplicit = Geometry.template GetObject<const TImplicitObjectInstanced<TPGeometryClass> >())
				return (Geometry.template GetObject<const TImplicitObjectInstanced<TPGeometryClass> >())->GetInstancedObject();
			else if (const TPGeometryClass* ConvexImplicit = Geometry.template GetObject<const TPGeometryClass>())
				return Geometry.template GetObject<const TPGeometryClass>();
			return nullptr;
		}


		//
		// Box - Box
		//

		FContactPoint BoxBoxContactPoint(const FImplicitBox3& Box1, const FImplicitBox3& Box2, const FRigidTransform3& Box1TM, const FRigidTransform3& Box2TM)
		{
			return GJKContactPoint(Box1, Box1TM, Box2, Box2TM, FVec3(1, 0, 0));
		}

		void UpdateBoxBoxConstraint(const FImplicitBox3& Box1, const FRigidTransform3& Box1Transform, const FImplicitBox3& Box2, const FRigidTransform3& Box2Transform, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			if (Constraint.GetUseManifold())
			{
				ConstructBoxBoxOneShotManifold(Box1, Box1Transform, Box2, Box2Transform, Dt, Constraint);
			}
			else
			{
				UpdateContactPoint(Constraint, BoxBoxContactPoint(Box1, Box2, Box1Transform, Box2Transform), Dt);
			}
		}

		template<typename T_TRAITS>
		void ConstructBoxBoxConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{
			const TBox<FReal, 3>* Object0 = Implicit0->template GetObject<const TBox<FReal, 3> >();
			const TBox<FReal, 3>* Object1 = Implicit1->template GetObject<const TBox<FReal, 3> >();
			if (ensure(Object0 && Object1))
			{
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::BoxBox, Context.GetSettings().bAllowManifolds, Context);
				if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
				{
					const FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
					const FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateBoxBoxConstraint(*Object0, WorldTransform0, *Object1, WorldTransform1, Dt, *Constraint);
				}
			}
		}


		//
		// Box - HeightField
		//


		void UpdateBoxHeightFieldConstraint(const FImplicitBox3& A, const FRigidTransform3& ATransform, const FHeightField& B, const FRigidTransform3& BTransform, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			if(Constraint.GetUseManifold())
			{
				ConstructPlanarConvexHeightFieldOneShotManifold(A, ATransform, B, BTransform, Constraint);
			}
			else
			{
				UpdateContactPoint(Constraint, BoxHeightFieldContactPoint(A, ATransform, B, BTransform, Constraint.GetCullDistance()), Dt);
			}
		}

		template<typename T_TRAITS>
		void ConstructBoxHeightFieldConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{

			const TBox<FReal, 3>* Object0 = Implicit0->template GetObject<const TBox<FReal, 3> >();
			const FHeightField* Object1 = Implicit1->template GetObject<const FHeightField >();
			if (ensure(Object0 && Object1))
			{
				// @todo(chaos): one-shot manifold
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::BoxHeightField, Context.GetSettings().bAllowManifolds, Context);
				if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
				{
					FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateBoxHeightFieldConstraint(*Object0, WorldTransform0, *Object1, WorldTransform1, Dt, *Constraint);
				}
			}
		}



		//
		// Box-Plane
		//

		FContactPoint BoxPlaneContactPoint(const FImplicitBox3& Box, const FImplicitPlane3& Plane, const FRigidTransform3& BoxTransform, const FRigidTransform3& PlaneTransform)
		{
			FContactPoint Contact;

#if USING_CODE_ANALYSIS
			MSVC_PRAGMA(warning(push))
			MSVC_PRAGMA(warning(disable : ALL_CODE_ANALYSIS_WARNINGS))
#endif	// USING_CODE_ANALYSIS

			const FRigidTransform3 BoxToPlaneTransform(BoxTransform.GetRelativeTransform(PlaneTransform));
			const FVec3 Extents = Box.Extents();
			constexpr int32 NumCorners = 2 + 2 * 3;
			constexpr FReal Epsilon = UE_KINDA_SMALL_NUMBER;

			FVec3 Corners[NumCorners];
			int32 CornerIdx = 0;
			Corners[CornerIdx++] = BoxToPlaneTransform.TransformPosition(Box.Max());
			Corners[CornerIdx++] = BoxToPlaneTransform.TransformPosition(Box.Min());
			for (int32 j = 0; j < 3; ++j)
			{
				Corners[CornerIdx++] = BoxToPlaneTransform.TransformPosition(Box.Min() + FVec3::AxisVector(j) * Extents);
				Corners[CornerIdx++] = BoxToPlaneTransform.TransformPosition(Box.Max() - FVec3::AxisVector(j) * Extents);
			}

#if USING_CODE_ANALYSIS
			MSVC_PRAGMA(warning(pop))
#endif	// USING_CODE_ANALYSIS

			FVec3 PotentialConstraints[NumCorners];
			int32 NumConstraints = 0;
			for (int32 i = 0; i < NumCorners; ++i)
			{
				FVec3 Normal;
				const FReal NewPhi = Plane.PhiWithNormal(Corners[i], Normal);
				if (NewPhi < Contact.Phi + Epsilon)
				{
					if (NewPhi <= Contact.Phi - Epsilon)
					{
						NumConstraints = 0;
					}
					Contact.Phi = NewPhi;
					Contact.ShapeContactPoints[1] = Corners[i];
					Contact.ShapeContactNormal = Normal;
					PotentialConstraints[NumConstraints++] = Corners[i];
				}
			}
			if (NumConstraints > 1)
			{
				FVec3 AverageLocation(0);
				for (int32 ConstraintIdx = 0; ConstraintIdx < NumConstraints; ++ConstraintIdx)
				{
					AverageLocation += PotentialConstraints[ConstraintIdx];
				}
				Contact.ShapeContactPoints[1] = AverageLocation / static_cast<FReal>(NumConstraints);
			}

			Contact.ShapeContactPoints[0] = BoxToPlaneTransform.InverseTransformPositionNoScale(Contact.ShapeContactPoints[1]);
			Contact.ShapeContactPoints[1] = Contact.ShapeContactPoints[1] - Contact.Phi * Contact.ShapeContactNormal;

			return Contact;
		}

		void UpdateBoxPlaneConstraint(const FImplicitBox3& A, const FRigidTransform3& ATransform, const TPlane<FReal, 3>& B, const FRigidTransform3& BTransform, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			// @todo(chaos): one-shot manifold
			UpdateContactPoint(Constraint, BoxPlaneContactPoint(A, B, ATransform, BTransform), Dt);
		}


		template<typename T_TRAITS>
		void ConstructBoxPlaneConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{
			const TBox<FReal, 3>* Object0 = Implicit0->template GetObject<const TBox<FReal, 3> >();
			const TPlane<FReal, 3>* Object1 = Implicit1->template GetObject<const TPlane<FReal, 3> >();
			if (ensure(Object0 && Object1))
			{
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::BoxPlane, Context.GetSettings().bAllowManifolds, Context);
				if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
				{
					FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateBoxPlaneConstraint(*Object0, WorldTransform0, *Object1, WorldTransform1, Dt, *Constraint);
				}
			}
		}

		//
		// Box-TriangleMesh
		//

		template <typename TriMeshType>
		void UpdateBoxTriangleMeshConstraint(const FImplicitBox3& Box0, const FRigidTransform3& WorldTransform0, const TriMeshType& TriangleMesh1, const FRigidTransform3& WorldTransform1, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			if(Constraint.GetUseManifold())
			{
				ConstructPlanarConvexTriMeshOneShotManifold(Box0, WorldTransform0, TriangleMesh1, WorldTransform1, Constraint);
			}
			else
			{
				// @toto(chaos): restitutionpadding
				UpdateContactPoint(Constraint, BoxTriangleMeshContactPoint(Box0, WorldTransform0, TriangleMesh1, WorldTransform1, Constraint.GetCullDistance()), Dt);
			}
		}

		template<typename T_TRAITS>
		void ConstructBoxTriangleMeshConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{
			const TBox<FReal, 3>* Object0 = Implicit0->template GetObject<const TBox<FReal, 3> >();
			if (ensure(Object0))
			{
				if (const TImplicitObjectScaled<FTriangleMeshImplicitObject>* ScaledTriangleMesh = Implicit1->template GetObject<const TImplicitObjectScaled<FTriangleMeshImplicitObject>>())
				{
					// @todo(chaos): one-shot manifold
					FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::BoxTriMesh, Context.GetSettings().bAllowManifolds, Context);
					if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
					{
						FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
						FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
						UpdateBoxTriangleMeshConstraint(*Object0, WorldTransform0, *ScaledTriangleMesh, WorldTransform1, Dt, *Constraint);
					}
				}
				else if (const FTriangleMeshImplicitObject* TriangleMesh = Implicit1->template GetObject<const FTriangleMeshImplicitObject>())
				{
					// @todo(chaos): one-shot manifold
					FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::BoxTriMesh, Context.GetSettings().bAllowManifolds, Context);
					if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
					{
						FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
						FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
						UpdateBoxTriangleMeshConstraint(*Object0, WorldTransform0, *TriangleMesh, WorldTransform1, Dt, *Constraint);
					}
				}
				else
				{
					ensure(false);
				}
			}
		}

		//
		// Sphere - Sphere
		//

		void UpdateSphereSphereConstraint(const TSphere<FReal, 3>& Sphere1, const FRigidTransform3& Sphere1Transform, const TSphere<FReal, 3>& Sphere2, const FRigidTransform3& Sphere2Transform, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			if (Constraint.GetUseManifold())
			{
				ConstructSphereSphereOneShotManifold(Sphere1, Sphere1Transform, Sphere2, Sphere2Transform, Dt, Constraint);
			}
			else
			{
				UpdateContactPoint(Constraint, SphereSphereContactPoint(Sphere1, Sphere1Transform, Sphere2, Sphere2Transform, Constraint.GetCullDistance()), Dt);
			}
		}

		template<typename T_TRAITS>
		void ConstructSphereSphereConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{
			const TSphere<FReal, 3>* Object0 = Implicit0->template GetObject<const TSphere<FReal, 3> >();
			const TSphere<FReal, 3>* Object1 = Implicit1->template GetObject<const TSphere<FReal, 3> >();
			if (ensure(Object0 && Object1))
			{
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::SphereSphere, Context.GetSettings().bAllowManifolds, Context);
				if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
				{
					FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateSphereSphereConstraint(*Object0, WorldTransform0, *Object1, WorldTransform1, Dt, *Constraint);
				}
			}
		}

		//
		// Sphere - HeightField
		//

		void UpdateSphereHeightFieldConstraint(const TSphere<FReal, 3>& A, const FRigidTransform3& ATransform, const FHeightField& B, const FRigidTransform3& BTransform, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			if (Constraint.GetUseManifold())
			{
				ConstructSphereHeightFieldOneShotManifold(A, ATransform, B, BTransform, Dt, Constraint);
			}
			else
			{
				UpdateContactPoint(Constraint, SphereHeightFieldContactPoint(A, ATransform, B, BTransform, Constraint.GetCullDistance()), Dt);
			}
		}

		void UpdateSphereHeightFieldConstraintSwept(TGeometryParticleHandle<FReal, 3>* Particle0, const TSphere<FReal, 3>& A, const FRigidTransform3& ATransform, const FHeightField& B, const FRigidTransform3& BTransform, const FVec3& Dir, const FReal Length, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			FReal TOI = 1.0f;
			UpdateContactPointNoCull(Constraint, GJKImplicitSweptContactPoint(A, ATransform, B, BTransform, Dir, Length, Constraint.GetCCDEnablePenetration(), Constraint.GetCCDTargetPenetration(), TOI), Dt, CCDNoCullAllShapePairs);
			Constraint.SetCCDTimeOfImpact(TOI);
		}

		template<typename T_TRAITS>
		void ConstructSphereHeightFieldConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{

			const TSphere<FReal, 3>* Object0 = Implicit0->template GetObject<const TSphere<FReal, 3> >();
			const FHeightField* Object1 = Implicit1->template GetObject<const FHeightField >();
			if (ensure(Object0 && Object1))
			{
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::SphereHeightField, Context.GetSettings().bAllowManifolds, Context);
				if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
				{
					FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateSphereHeightFieldConstraint(*Object0, WorldTransform0, *Object1, WorldTransform1, Dt, *Constraint);
				}
			}
		}

		void ConstructSphereHeightFieldConstraintsSwept(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FVec3& Dir, FReal Length, const FCollisionContext& Context)
		{
			const TSphere<FReal, 3>* Object0 = Implicit0->template GetObject<const TSphere<FReal, 3>>();
			const FHeightField* Object1 = Implicit1->template GetObject<const FHeightField>();
			if (ensure(Object0 && Object1))
			{
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateSweptConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::SphereHeightField, Context);
				FRigidTransform3 WorldTransformX0 = LocalTransform0 * FRigidTransform3(Particle0->X(), Particle0->R());
				FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
				UpdateSphereHeightFieldConstraintSwept(Particle0, *Object0, WorldTransformX0, *Object1, WorldTransform1, Dir, Length, Dt, *Constraint);
			}
		}

		//
		//  Sphere-Plane
		//

		void UpdateSpherePlaneConstraint(const TSphere<FReal, 3>& Sphere, const FRigidTransform3& SphereTransform, const TPlane<FReal, 3>& Plane, const FRigidTransform3& PlaneTransform, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			if (Constraint.GetUseManifold())
			{
				ConstructSpherePlaneOneShotManifold(Sphere, SphereTransform, Plane, PlaneTransform, Dt, Constraint);
			}
			else
			{
				UpdateContactPoint(Constraint, SpherePlaneContactPoint(Sphere, SphereTransform, Plane, PlaneTransform), Dt);
			}
		}

		template<typename T_TRAITS>
		void ConstructSpherePlaneConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{

			const TSphere<FReal, 3>* Object0 = Implicit0->template GetObject<const TSphere<FReal, 3> >();
			const TPlane<FReal, 3>* Object1 = Implicit1->template GetObject<const TPlane<FReal, 3> >();
			if (ensure(Object0 && Object1))
			{
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::SpherePlane, Context.GetSettings().bAllowManifolds, Context);
				if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
				{
					FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateSpherePlaneConstraint(*Object0, WorldTransform0, *Object1, WorldTransform1, Dt, *Constraint);
				}
			}
		}

		//
		// Sphere - Box
		//


		void UpdateSphereBoxConstraint(const TSphere<FReal, 3>& Sphere, const FRigidTransform3& SphereTransform, const FImplicitBox3& Box, const FRigidTransform3& BoxTransform, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			if (Constraint.GetUseManifold())
			{
				ConstructSphereBoxOneShotManifold(Sphere, SphereTransform, Box, BoxTransform, Dt, Constraint);
			}
			else
			{
				UpdateContactPoint(Constraint, SphereBoxContactPoint(Sphere, SphereTransform, Box, BoxTransform), Dt);
			}
		}

		template<typename T_TRAITS>
		void ConstructSphereBoxConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{

			const TSphere<FReal, 3>* Object0 = Implicit0->template GetObject<const TSphere<FReal, 3> >();
			const TBox<FReal, 3>* Object1 = Implicit1->template GetObject<const TBox<FReal, 3> >();
			if (ensure(Object0 && Object1))
			{
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::SphereBox, Context.GetSettings().bAllowManifolds, Context);
				if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
				{
					FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateSphereBoxConstraint(*Object0, WorldTransform0, *Object1, WorldTransform1, Dt, *Constraint);
				}
			}
		}


		//
		// Sphere - Capsule
		//


		void UpdateSphereCapsuleConstraint(const TSphere<FReal, 3>& A, const FRigidTransform3& ATransform, const FCapsule& B, const FRigidTransform3& BTransform, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			if (Constraint.GetUseManifold())
			{
				ConstructSphereCapsuleOneShotManifold(A, ATransform, B, BTransform, Dt, Constraint);
			}
			else
			{
				UpdateContactPoint(Constraint, SphereCapsuleContactPoint(A, ATransform, B, BTransform), Dt);
			}
		}

		template<typename T_TRAITS>
		void ConstructSphereCapsuleConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{
			const TSphere<FReal, 3>* Object0 = Implicit0->template GetObject<const TSphere<FReal, 3> >();
			const FCapsule* Object1 = Implicit1->template GetObject<const FCapsule >();
			if (ensure(Object0 && Object1))
			{
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::SphereCapsule, Context.GetSettings().bAllowManifolds, Context);
				if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
				{
					FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateSphereCapsuleConstraint(*Object0, WorldTransform0, *Object1, WorldTransform1, Dt, *Constraint);
				}
			}
		}

		//
		// Sphere-Convex
		//

		void UpdateSphereConvexConstraint(const TSphere<FReal, 3>& A, const FRigidTransform3& ATransform, const FImplicitObject3& B, const FRigidTransform3& BTransform, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			if (Constraint.GetUseManifold())
			{
				ConstructSphereConvexManifold(A, ATransform, B, BTransform, Dt, Constraint);
			}
			else
			{
				UpdateContactPoint(Constraint, SphereConvexContactPoint(A, ATransform, B, BTransform), Dt);
			}
		}

		template<typename T_TRAITS>
		void ConstructSphereConvexConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{
			const TSphere<FReal, 3>* Object0 = Implicit0->template GetObject<const TSphere<FReal, 3> >();
			const FImplicitObject* Object1 = Implicit1;
			if (ensure(Object0 && Object1))
			{
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::SphereConvex, Context.GetSettings().bAllowManifolds, Context);
				if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
				{
					FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateSphereConvexConstraint(*Object0, WorldTransform0, *Object1, WorldTransform1, Dt, *Constraint);
				}
			}
		}

		//
		// Sphere-TriangleMesh
		//

		template <typename TriMeshType>
		void UpdateSphereTriangleMeshConstraint(const TSphere<FReal, 3>& Sphere0, const FRigidTransform3& WorldTransform0, const TriMeshType& TriangleMesh1, const FRigidTransform3& WorldTransform1, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			if (Constraint.GetUseManifold())
			{
				ConstructSphereTriangleMeshOneShotManifold(Sphere0, WorldTransform0, TriangleMesh1, WorldTransform1, Dt, Constraint);
			}
			else
			{
				// @todo(chaos): restitutionpadding
				UpdateContactPoint(Constraint, SphereTriangleMeshContactPoint(Sphere0, WorldTransform0, TriangleMesh1, WorldTransform1, Constraint.GetCullDistance()), Dt);
			}
		}

		template<typename TriMeshType>
		void UpdateSphereTriangleMeshConstraintSwept(TGeometryParticleHandle<FReal, 3>* Particle0, const TSphere<FReal, 3>& Sphere0, const FRigidTransform3& WorldTransform0, const TriMeshType& TriangleMesh1, const FRigidTransform3& WorldTransform1, const FVec3& Dir, const FReal Length, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			FReal TOI = 1.0f;
			UpdateContactPointNoCull(Constraint, SphereTriangleMeshSweptContactPoint(Sphere0, WorldTransform0, TriangleMesh1, WorldTransform1, Dir, Length, Constraint.GetCCDEnablePenetration(), Constraint.GetCCDTargetPenetration(), TOI), Dt, CCDNoCullAllShapePairs);
			Constraint.SetCCDTimeOfImpact(TOI);
		}

		template<typename T_TRAITS>
		void ConstructSphereTriangleMeshConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{
			const TSphere<FReal, 3>* Object0 = Implicit0->template GetObject<const TSphere<FReal, 3> >();
			if (ensure(Object0))
			{
				if (const TImplicitObjectScaled<FTriangleMeshImplicitObject>* ScaledTriangleMesh = Implicit1->template GetObject<const TImplicitObjectScaled<FTriangleMeshImplicitObject>>())
				{
					FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::SphereTriMesh, Context.GetSettings().bAllowManifolds, Context);
					if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
					{
						FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
						FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
						UpdateSphereTriangleMeshConstraint(*Object0, WorldTransform0, *ScaledTriangleMesh, WorldTransform1, Dt, *Constraint);
					}
				}
				else if (const FTriangleMeshImplicitObject* TriangleMesh = Implicit1->template GetObject<const FTriangleMeshImplicitObject>())
				{
					FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::SphereTriMesh, Context.GetSettings().bAllowManifolds, Context);
					if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
					{
						FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
						FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
						UpdateSphereTriangleMeshConstraint(*Object0, WorldTransform0, *TriangleMesh, WorldTransform1, Dt, *Constraint);
					}
				}
				else
				{
					ensure(false);
				}
			}
		}


		void ConstructSphereTriangleMeshConstraintsSwept(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FVec3& Dir, FReal Length, const FCollisionContext& Context)
		{
			const TSphere<FReal, 3>* Object0 = Implicit0->template GetObject<const TSphere<FReal, 3>>();
			if (ensure(Object0))
			{
				if (const TImplicitObjectScaled<FTriangleMeshImplicitObject>* ScaledTriangleMesh = Implicit1->template GetObject<const TImplicitObjectScaled<FTriangleMeshImplicitObject>>())
				{
					FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateSweptConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::SphereTriMesh, Context);
					FRigidTransform3 WorldTransformX0 = LocalTransform0 * FRigidTransform3(Particle0->X(), Particle0->R());
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateSphereTriangleMeshConstraintSwept(Particle0, *Object0, WorldTransformX0, *ScaledTriangleMesh, WorldTransform1, Dir, Length, Dt, *Constraint);
				}
				else if (const FTriangleMeshImplicitObject* TriangleMesh = Implicit1->template GetObject<const FTriangleMeshImplicitObject>())
				{
					FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateSweptConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::SphereTriMesh, Context);
					FRigidTransform3 WorldTransformX0 = LocalTransform0 * FRigidTransform3(Particle0->X(), Particle0->R());
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateSphereTriangleMeshConstraintSwept(Particle0, *Object0, WorldTransformX0, *TriangleMesh, WorldTransform1, Dir, Length, Dt, *Constraint);
				}
				else
				{
					ensure(false);
				}
			}
		}



		//
		// Capsule-Capsule
		//


		void UpdateCapsuleCapsuleConstraint(const FCapsule& A, const FRigidTransform3& ATransform, const FCapsule& B, const FRigidTransform3& BTransform, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			if (Constraint.GetUseManifold())
			{
				ConstructCapsuleCapsuleOneShotManifold(A, ATransform, B, BTransform, Dt, Constraint);
			}
			else
			{
				UpdateContactPoint(Constraint, CapsuleCapsuleContactPoint(A, ATransform, B, BTransform), Dt);
			}
		}

		template<typename T_TRAITS>
		void ConstructCapsuleCapsuleConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{
			const FCapsule* Object0 = Implicit0->template GetObject<const FCapsule >();
			const FCapsule* Object1 = Implicit1->template GetObject<const FCapsule >();
			if (ensure(Object0 && Object1))
			{
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::CapsuleCapsule, Context.GetSettings().bAllowManifolds, Context);
				if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
				{
					FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateCapsuleCapsuleConstraint(*Object0, WorldTransform0, *Object1, WorldTransform1, Dt, *Constraint);
				}
			}
		}

		//
		// Capsule - Box
		//

		void UpdateCapsuleBoxConstraint(const FCapsule& A, const FRigidTransform3& ATransform, const FImplicitBox3& B, const FRigidTransform3& BTransform, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			if (Constraint.GetUseManifold())
			{
				TCArray<FContactPoint, 4> ContactPoints;
				ConstructCapsuleConvexOneShotManifold<FImplicitBox3>(A, ATransform, B, BTransform, Constraint.GetCullDistance(), ContactPoints);

				Constraint.ResetActiveManifoldContacts();
				for (const FContactPoint& ContactPoint : ContactPoints)
				{
					Constraint.AddOneshotManifoldContact(ContactPoint);
				}
			}
			else
			{
				const FVec3 InitialDir = FVec3(1,0,0);
				UpdateContactPoint(Constraint, CapsuleBoxContactPoint(A, ATransform, B, BTransform, InitialDir), Dt);
			}
		}

		template<typename T_TRAITS>
		void ConstructCapsuleBoxConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{
			const FCapsule* Object0 = Implicit0->template GetObject<const FCapsule>();
			const TBox<FReal, 3>* Object1 = Implicit1->template GetObject<const TBox<FReal, 3>>();
			if (ensure(Object0 && Object1))
			{
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::CapsuleBox, Context.GetSettings().bAllowManifolds, Context);
				if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
				{
					FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateCapsuleBoxConstraint(*Object0, WorldTransform0, *Object1, WorldTransform1, Dt, *Constraint);
				}
			}
		}


		//
		// Capsule-Convex
		//

		// Use the traits to call the appropriate convex-convex update method. 
		// Either incremental manifold (default) or one-shot manifold
		template<bool T_SUPPORTSONESHOTMANIFOLD = false>
		struct TCapsuleConvexUpdater
		{
			template<typename T_ImplicitB>
			static void UpdateCapsuleConvexConstraint(const FCapsule& A, const FRigidTransform3& ATM, const T_ImplicitB& B, const FRigidTransform3& BTM, const FReal Dt, FPBDCollisionConstraint& Constraint)
			{
				// We should have called the specialized update function and not gone through the generic path
				ensure(false);
			}
		};

		// One-shot manifold convex-convex update
		template<>
		struct TCapsuleConvexUpdater<true>
		{
			template<typename ConvexType>
			static void UpdateCapsuleConvexConstraint(const FCapsule& A, const FRigidTransform3& ATransform, const ConvexType& B, const FRigidTransform3& BTransform, const FReal Dt, FPBDCollisionConstraint& Constraint)
			{
				if (Constraint.GetUseManifold())
				{
					TCArray<FContactPoint, 4> ContactPoints;
					ConstructCapsuleConvexOneShotManifold<ConvexType>(A, ATransform, B, BTransform, Constraint.GetCullDistance(), ContactPoints);

					Constraint.ResetActiveManifoldContacts();
					for (const FContactPoint& ContactPoint : ContactPoints)
					{
						Constraint.AddOneshotManifoldContact(ContactPoint);
					}
				}
				else
				{
					UpdateContactPoint(Constraint, CapsuleConvexContactPoint(A, ATransform, B, BTransform), Dt);
				}
			}
		};

		// Another helper required by UpdateCapsuleGenericConvexConstraintHelper which uses CastHelper and does not have any typedefs 
		// for the concrete implicit types, so we need to rely on type deduction from the compiler.
		struct FCapsuleConvexUpdaterCaller
		{
			template<typename T_ImplicitA, typename T_ImplicitB>
			static void Update(const T_ImplicitA& A, const FRigidTransform3& ATM, const T_ImplicitB& B, const FRigidTransform3& BTM, const FReal Dt, FPBDCollisionConstraint& Constraint)
			{
				using FCapsuleConvexUpdater = TCapsuleConvexUpdater<TCapsuleImplicitPairTraits<T_ImplicitB>::bSupportsGenericOneShotManifoldPath>;
				FCapsuleConvexUpdater::UpdateCapsuleConvexConstraint(A, ATM, B, BTM, Dt, Constraint);
			}
		};

		// Unwrap the many convex types, including scaled, and call the appropriate update which depends on the concrete types
		void UpdateCapsuleGenericConvexConstraintHelper(const FCapsule& A, const FRigidTransform3& ATM, const FImplicitObject& B, const FRigidTransform3& BTM, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			// This expands to a switch that calls the inner function with the appropriate concrete implicit type
			Utilities::CastHelperNoUnwrap(B, BTM, [&](const auto& BDowncast, const FRigidTransform3& BFullTM)
				{
					FCapsuleConvexUpdaterCaller::Update(A, ATM, BDowncast, BFullTM, Dt, Constraint);
				});
		}

		template<typename T_TRAITS>
		void ConstructCapsuleConvexConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{
			const FCapsule* Object0 = Implicit0->template GetObject<const FCapsule>();
			const FImplicitObject* Object1 = Implicit1;
			if (ensure(Object0 && Object1))
			{
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::CapsuleConvex, Context.GetSettings().bAllowManifolds, Context);
				if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
				{
					FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateCapsuleGenericConvexConstraintHelper(*Object0, WorldTransform0, *Object1, WorldTransform1, Dt, *Constraint);
				}
			}
		}


		//
		// Capsule-HeightField
		//

		void UpdateCapsuleHeightFieldConstraint(const FCapsule& A, const FRigidTransform3& ATransform, const FHeightField& B, const FRigidTransform3& BTransform, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_UpdateCapsuleHeightFieldConstraint, ConstraintsDetailedStats);
			if (Constraint.GetUseManifold())
			{
				ConstructCapsuleHeightFieldOneShotManifold(A, ATransform, B, BTransform, Dt, Constraint);
			}
			else
			{
				UpdateContactPoint(Constraint, CapsuleHeightFieldContactPoint(A, ATransform, B, BTransform, Constraint.GetCullDistance()), Dt);
			}
		}


		void UpdateCapsuleHeightFieldConstraintSwept(TGeometryParticleHandle<FReal, 3>* Particle0, const FCapsule& A, const FRigidTransform3& ATransform, const FHeightField& B, const FRigidTransform3& BTransform, const FVec3& Dir, const FReal Length, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_UpdateCapsuleHeightFieldConstraintSwept, ConstraintsDetailedStats);
			FReal TOI = 1.0f;
			UpdateContactPointNoCull(Constraint, GJKImplicitSweptContactPoint(A, ATransform, B, BTransform, Dir, Length, Constraint.GetCCDEnablePenetration(), Constraint.GetCCDTargetPenetration(), TOI), Dt, CCDNoCullAllShapePairs);
			Constraint.SetCCDTimeOfImpact(TOI);
		}

		template<typename T_TRAITS>
		void ConstructCapsuleHeightFieldConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_ConstructCapsuleHeightFieldConstraints, ConstraintsDetailedStats);

			const FCapsule* Object0 = Implicit0->template GetObject<const FCapsule >();
			const FHeightField* Object1 = Implicit1->template GetObject<const FHeightField >();
			if (ensure(Object0 && Object1))
			{
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::CapsuleHeightField, Context.GetSettings().bAllowManifolds, Context);
				if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
				{
					FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateCapsuleHeightFieldConstraint(*Object0, WorldTransform0, *Object1, WorldTransform1, Dt, *Constraint);
				}
			}
		}

		void ConstructCapsuleHeightFieldConstraintsSwept(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FVec3& Dir, FReal Length, const FCollisionContext& Context)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_ConstructCapsuleHeightFieldConstraintsSwept, ConstraintsDetailedStats);
			const FCapsule* Object0 = Implicit0->template GetObject<const FCapsule >();
			const FHeightField* Object1 = Implicit1->template GetObject<const FHeightField >();
			if (ensure(Object0 && Object1))
			{
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateSweptConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::CapsuleHeightField, Context);
				FRigidTransform3 WorldTransformX0 = LocalTransform0 * FRigidTransform3(Particle0->X(), Particle0->R());
				FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
				UpdateCapsuleHeightFieldConstraintSwept(Particle0, *Object0, WorldTransformX0, *Object1, WorldTransform1, Dir, Length, Dt, *Constraint);
			}
		}


		//
		// Capsule-TriangleMesh
		//


		template <typename TriMeshType>
		void UpdateCapsuleTriangleMeshConstraint(const FCapsule& Capsule0, const FRigidTransform3& WorldTransform0, const TriMeshType& TriangleMesh1, const FRigidTransform3& WorldTransform1, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			// @todo(chaos): restitution padding
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_UpdateCapsuleTriangleMeshConstraint, ConstraintsDetailedStats);
			if (Constraint.GetUseManifold())
			{
				ConstructCapsuleTriMeshOneShotManifold(Capsule0, WorldTransform0, TriangleMesh1, WorldTransform1, Dt, Constraint);
			}
			else
			{
				UpdateContactPoint(Constraint, CapsuleTriangleMeshContactPoint(Capsule0, WorldTransform0, TriangleMesh1, WorldTransform1, Constraint.GetCullDistance()), Dt);
			}
		}

		template <typename TriMeshType>
		void UpdateCapsuleTriangleMeshConstraintSwept(TGeometryParticleHandle<FReal, 3>* Particle0, const FCapsule& Capsule0, const FRigidTransform3& WorldTransform0, const TriMeshType& TriangleMesh1, const FRigidTransform3& WorldTransform1, const FVec3& Dir, const FReal Length, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_UpdateCapsuleTriangleMeshConstraint, ConstraintsDetailedStats);
			FReal TOI = 1.0f;
			UpdateContactPointNoCull(Constraint, CapsuleTriangleMeshSweptContactPoint(Capsule0, WorldTransform0, TriangleMesh1, WorldTransform1, Dir, Length, Constraint.GetCCDEnablePenetration(), Constraint.GetCCDTargetPenetration(), TOI), Dt, CCDNoCullAllShapePairs);
			Constraint.SetCCDTimeOfImpact(TOI);
		}

		template<typename T_TRAITS>
		void ConstructCapsuleTriangleMeshConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_ConstructCapsuleTriangleMeshConstraints, ConstraintsDetailedStats);

			const FCapsule* Object0 = Implicit0->template GetObject<const FCapsule >();
			if (ensure(Object0))
			{
				if (const TImplicitObjectScaled<FTriangleMeshImplicitObject>* ScaledTriangleMesh = Implicit1->template GetObject<const TImplicitObjectScaled<FTriangleMeshImplicitObject>>())
				{
					FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1,  nullptr, LocalTransform1, CullDistance, EContactShapesType::CapsuleTriMesh, Context.GetSettings().bAllowManifolds, Context);
					if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
					{
						FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
						FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
						UpdateCapsuleTriangleMeshConstraint(*Object0, WorldTransform0, *ScaledTriangleMesh, WorldTransform1, Dt, *Constraint);
					}
				}
				else if (const FTriangleMeshImplicitObject* TriangleMesh = Implicit1->template GetObject<const FTriangleMeshImplicitObject>())
				{
					FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::CapsuleTriMesh, Context.GetSettings().bAllowManifolds, Context);
					if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
					{
						FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
						FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
						UpdateCapsuleTriangleMeshConstraint(*Object0, WorldTransform0, *TriangleMesh, WorldTransform1, Dt, *Constraint);
					}
				}
				else
				{
					ensure(false);
				}
			}
		}

		void ConstructCapsuleTriangleMeshConstraintsSwept(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FVec3& Dir, FReal Length, const FCollisionContext& Context)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_ConstructCapsuleTriangleMeshConstraintsSwept, ConstraintsDetailedStats);

			const FCapsule* Object0 = Implicit0->template GetObject<const FCapsule >();
			if (ensure(Object0))
			{
				if (const TImplicitObjectScaled<FTriangleMeshImplicitObject>* ScaledTriangleMesh = Implicit1->template GetObject<const TImplicitObjectScaled<FTriangleMeshImplicitObject>>())
				{
					FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateSweptConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::CapsuleTriMesh, Context);
					FRigidTransform3 WorldTransformX0 = LocalTransform0 * FRigidTransform3(Particle0->X(), Particle0->R());
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateCapsuleTriangleMeshConstraintSwept(Particle0, *Object0, WorldTransformX0, *ScaledTriangleMesh, WorldTransform1, Dir, Length, Dt, *Constraint);
				}
				else if (const FTriangleMeshImplicitObject* TriangleMesh = Implicit1->template GetObject<const FTriangleMeshImplicitObject>())
				{
					FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateSweptConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::CapsuleTriMesh, Context);
					FRigidTransform3 WorldTransformX0 = LocalTransform0 * FRigidTransform3(Particle0->X(), Particle0->R());
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateCapsuleTriangleMeshConstraintSwept(Particle0, *Object0, WorldTransformX0, *TriangleMesh, WorldTransform1, Dir, Length, Dt, *Constraint);
				}
				else
				{
					ensure(false);
				}
			}
		}

		//
		// Generic Convex - Convex (actual concrete type could be anything)
		//

		void UpdateGenericConvexConvexConstraint(const FImplicitObject& Implicit0, const FRigidTransform3& WorldTransform0, const FImplicitObject& Implicit1, const FRigidTransform3& WorldTransform1, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_UpdateGenericConvexConvexConstraint, ConstraintsDetailedStats);

			UpdateGenericConvexConvexConstraintHelper(Implicit0, WorldTransform0, Implicit1, WorldTransform1, Dt, Constraint);
		}

		void UpdateGenericConvexConvexConstraintSwept(TGeometryParticleHandle<FReal, 3>* Particle0, const FImplicitObject& Implicit0, const FRigidTransform3& StartWorldTransform0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject& Implicit1, const FRigidTransform3& StartWorldTransform1, const FVec3& Dir, const FReal Length, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_UpdateGenericConvexConvexConstraintSwept, ConstraintsDetailedStats);
			FReal TOI = FPBDCollisionConstraint::MaxTOI;
			const FRigidTransform3& EndWorldTransform0 = Constraint.GetShapeWorldTransform0();
			const FRigidTransform3& EndWorldTransform1 = Constraint.GetShapeWorldTransform1();
			UpdateContactPointNoCull(Constraint, GenericConvexConvexContactPointSwept(Implicit0, StartWorldTransform0, EndWorldTransform0, Implicit1, StartWorldTransform1, EndWorldTransform1, Dir, Length, Constraint.GetCCDEnablePenetration(), Constraint.GetCCDTargetPenetration(), TOI), Dt);
			Constraint.SetCCDTimeOfImpact(TOI);
		}


		template<typename T_TRAITS>
		void ConstructGenericConvexConvexConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_ConstructGenericConvexConvexConstraints, ConstraintsDetailedStats);
			EImplicitObjectType Implicit0Type = Particle0->Geometry()->GetType();
			EImplicitObjectType Implicit1Type = Particle1->Geometry()->GetType();

			FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::GenericConvexConvex, Context.GetSettings().bAllowManifolds, Context);
			if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
			{
				FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
				FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
				UpdateGenericConvexConvexConstraint(*Implicit0, WorldTransform0, *Implicit1, WorldTransform1, Dt, *Constraint);
			}
		}

		void ConstructGenericConvexConvexConstraintsSwept(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FVec3& StartX0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FVec3& StartX1, const FReal CullDistance, const FReal Dt, const FVec3& Dir, FReal Length, const FCollisionContext& Context)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_ConstructGenericConvexConvexConstraintsSwept, ConstraintsDetailedStats);
			FGenericParticleHandle RigidParticle0 = FGenericParticleHandle(Particle0);
			FGenericParticleHandle RigidParticle1 = FGenericParticleHandle(Particle1);
			FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateSweptConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::GenericConvexConvex, Context);
			// Rotations of WorldTransformXRx are not actually used in UpdateGenericConvexConvexConstraintSwept.
			FRigidTransform3 WorldTransformXR0 = LocalTransform0 * FRigidTransform3(StartX0, RigidParticle0->R());
			FRigidTransform3 WorldTransformXR1 = LocalTransform1 * FRigidTransform3(StartX1, RigidParticle1->R());
			UpdateGenericConvexConvexConstraintSwept(Particle0, *Implicit0, WorldTransformXR0, Particle1, *Implicit1, WorldTransformXR1, Dir, Length, Dt, *Constraint);

			// @todo(zhenglin): If resweep is not enabled, we don't need to process secondary collision constraints so constraints with TOI > 1.0f can be deleted. This could be a potential optimization but I have not made this work yet.
			// if (!bChaosCollisionCCDEnableResweep && Constraint->TimeOfImpact > 1.0f)
			// {
			// 	Context.MultiShapeCollisionDetector->DestroyConstraint(Constraint);
			// }
		}

		//
		// Convex - HeightField
		//


		void UpdateConvexHeightFieldConstraint(const FImplicitObject& A, const FRigidTransform3& ATransform, const FHeightField& B, const FRigidTransform3& BTransform, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_UpdateConvexHeightFieldConstraint, ConstraintsDetailedStats);
			if (Constraint.GetUseManifold())
			{
				ConstructPlanarConvexHeightFieldOneShotManifold(A, ATransform, B, BTransform, Constraint);
			}
			else
			{
				UpdateContactPoint(Constraint, ConvexHeightFieldContactPoint(A, ATransform, B, BTransform, Constraint.GetCullDistance()), Dt);
			}
		}


		void UpdateConvexHeightFieldConstraintSwept(TGeometryParticleHandle<FReal, 3>* Particle0, const FImplicitObject& A, const FRigidTransform3& ATransform, const FHeightField& B, const FRigidTransform3& BTransform, const FVec3& Dir, const FReal Length, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_UpdateConvexHeightFieldConstraintSwept, ConstraintsDetailedStats);
			FReal TOI = 1.0f;
			UpdateContactPointNoCull(Constraint, GJKImplicitSweptContactPoint(A, ATransform, B, BTransform, Dir, Length, Constraint.GetCCDEnablePenetration(), Constraint.GetCCDTargetPenetration(), TOI), Dt, CCDNoCullAllShapePairs);
			Constraint.SetCCDTimeOfImpact(TOI);
		}

		template<typename T_TRAITS>
		void ConstructConvexHeightFieldConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_ConstructConvexHeightFieldConstraints, ConstraintsDetailedStats);
			const FHeightField* Object1 = Implicit1->template GetObject<const FHeightField >();
			if (ensure(Implicit0->IsConvex() && Object1))
			{
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::ConvexHeightField, Context.GetSettings().bAllowManifolds, Context);
				if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
				{
					FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateConvexHeightFieldConstraint(*Implicit0, WorldTransform0, *Object1, WorldTransform1, Dt, *Constraint);
				}
			}
		}

		void ConstructConvexHeightFieldConstraintsSwept(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FVec3& Dir, FReal Length, const FCollisionContext& Context)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_ConstructConvexHeightFieldConstraintsSwept, ConstraintsDetailedStats);
			const FHeightField* Object1 = Implicit1->template GetObject<const FHeightField >();
			if (ensure(Implicit0->IsConvex() && Object1))
			{
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateSweptConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::ConvexHeightField, Context);
				FRigidTransform3 WorldTransformX0 = LocalTransform0 * FRigidTransform3(Particle0->X(), Particle0->R());
				FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
				UpdateConvexHeightFieldConstraintSwept(Particle0, *Implicit0, WorldTransformX0, *Object1, WorldTransform1, Dir, Length, Dt, *Constraint);
			}
		}

		//
		// Convex-TriangleMesh
		//

		void UpdateConvexTriangleMeshConstraint(const FImplicitObject& Convex0, const FRigidTransform3& WorldTransform0, const FImplicitObject& TriangleMesh1, const FRigidTransform3& WorldTransform1, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_UpdateConvexTriangleMeshConstraint, ConstraintsDetailedStats);
			if (Constraint.GetUseManifold())
			{
				ConstructPlanarConvexTriMeshOneShotManifold(Convex0, WorldTransform0, TriangleMesh1, WorldTransform1, Constraint);
			}
			else
			{
				UpdateContactPoint(Constraint, ConvexTriangleMeshContactPoint(Convex0, WorldTransform0, TriangleMesh1, WorldTransform1, Constraint.GetCullDistance()), Dt);
			}
		}

		// Sweeps convex against trimesh
		template <typename TriMeshType>
		void UpdateConvexTriangleMeshConstraintSwept(TGeometryParticleHandle<FReal, 3>* Particle0, const FImplicitObject& Convex0, const FRigidTransform3& WorldTransform0, const TriMeshType& TriangleMesh1, const FRigidTransform3& WorldTransform1, const FVec3& Dir, const FReal Length, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_UpdateConvexTriangleMeshConstraintSwept, ConstraintsDetailedStats);
			FReal TOI = 1.0f;
			UpdateContactPointNoCull(Constraint, ConvexTriangleMeshSweptContactPoint(Convex0, WorldTransform0, TriangleMesh1, WorldTransform1, Dir, Length, Constraint.GetCCDEnablePenetration(), Constraint.GetCCDTargetPenetration(), TOI), Dt, CCDNoCullAllShapePairs);
			Constraint.SetCCDTimeOfImpact(TOI);
		}

		template<typename T_TRAITS>
		void ConstructConvexTriangleMeshConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_ConstructConvexTriangleMeshConstraints, ConstraintsDetailedStats);
			if (ensure(Implicit0->IsConvex()))
			{
				FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::ConvexTriMesh, Context.GetSettings().bAllowManifolds, Context);
				if (T_TRAITS::bImmediateUpdate && (Constraint != nullptr))
				{
					FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateConvexTriangleMeshConstraint(*Implicit0, WorldTransform0, *Implicit1, WorldTransform1, Dt, *Constraint);
				}
			}
		}

		void ConstructConvexTriangleMeshConstraintsSwept(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FVec3& Dir, FReal Length, const FCollisionContext& Context)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_ConstructConvexTriangleMeshConstraintsSwept, ConstraintsDetailedStats);
			if (ensure(Implicit0->IsConvex()))
			{
				if (const TImplicitObjectScaled<FTriangleMeshImplicitObject>* ScaledTriangleMesh = Implicit1->template GetObject<const TImplicitObjectScaled<FTriangleMeshImplicitObject>>())
				{
					FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateSweptConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::ConvexTriMesh, Context);
					FRigidTransform3 WorldTransformX0 = LocalTransform0 * FRigidTransform3(Particle0->X(), Particle0->R());
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateConvexTriangleMeshConstraintSwept(Particle0, *Implicit0, WorldTransformX0, *ScaledTriangleMesh, WorldTransform1, Dir, Length, Dt, *Constraint);
				}
				else if (const FTriangleMeshImplicitObject* TriangleMesh = Implicit1->template GetObject<const FTriangleMeshImplicitObject>())
				{
					FPBDCollisionConstraint* Constraint = Context.MultiShapeCollisionDetector->FindOrCreateSweptConstraint(Particle0, Implicit0, Shape0, nullptr, LocalTransform0, Particle1, Implicit1, Shape1, nullptr, LocalTransform1, CullDistance, EContactShapesType::ConvexTriMesh, Context);
					FRigidTransform3 WorldTransformX0 = LocalTransform0 * FRigidTransform3(Particle0->X(), Particle0->R());
					FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
					UpdateConvexTriangleMeshConstraintSwept(Particle0, *Implicit0, WorldTransformX0, *TriangleMesh, WorldTransform1, Dir, Length, Dt, *Constraint);
				}
				else
				{
					ensure(false);
				}
			}
		}


		//
		// Levelset-Levelset
		//

		template<ECollisionUpdateType UpdateType>
		void UpdateLevelsetLevelsetConstraint(const FRigidTransform3& WorldTransform0, const FRigidTransform3& WorldTransform1, const FReal Dt, FPBDCollisionConstraint& Constraint)
		{
			SCOPE_CYCLE_COUNTER(STAT_UpdateLevelsetLevelsetConstraint);

			// @todo(chaos): get rid of this
			FRigidTransform3 ParticlesTM = WorldTransform0;
			if (!(ensure(!FMath::IsNaN(ParticlesTM.GetTranslation().X)) && ensure(!FMath::IsNaN(ParticlesTM.GetTranslation().Y)) && ensure(!FMath::IsNaN(ParticlesTM.GetTranslation().Z))))
			{
				return;
			}
			FRigidTransform3 LevelsetTM = WorldTransform1;
			if (!(ensure(!FMath::IsNaN(LevelsetTM.GetTranslation().X)) && ensure(!FMath::IsNaN(LevelsetTM.GetTranslation().Y)) && ensure(!FMath::IsNaN(LevelsetTM.GetTranslation().Z))))
			{
				return;
			}

			// NOTE: We are assuming that if only one body has collision particles, it is the first one. This implies
			// that the first body is dynamic (currently this is true)
			const FBVHParticles* SampleParticles = Constraint.GetCollisionParticles0();
			if (SampleParticles)
			{
				// Sample particles are in Particle(Actor) space, but SolverBody state in in CoM space
				// The transforms passed in are the Particle transforms but concatenatated with the shape transform(s) so we need
				// to regenerate the Particle transform without those shape transforms. We should be able to eliminate this...
				FGenericParticleHandle Particle0 = Constraint.GetParticle0();
				// @todo(chaos): we should not be accessing the particle here (or anywhere in collision detection)
				// The two paths here are required for unit testing. Units tests don't always set up the solver bodies, but this function is
				// usually called in the solver loop where solver bodies must be used.
				if (Constraint.GetSolverBody0() != nullptr)
				{
					const FRigidTransform3 CoMToParticleTransform = FParticleUtilities::ParticleLocalToCoMLocal(Particle0, FRigidTransform3::Identity);
					ParticlesTM = CoMToParticleTransform * FRigidTransform3(Constraint.GetSolverBody0()->CorrectedP(), Constraint.GetSolverBody0()->CorrectedQ());
				}
				else
				{
					ParticlesTM = FParticleUtilities::GetActorWorldTransform(Particle0);
				}
			}

			if (SampleParticles)
			{
				const FImplicitObject* Obj1 = Constraint.GetImplicit1();
				FContactPoint ContactPoint = SampleObject<UpdateType>(*Obj1, LevelsetTM, *SampleParticles, ParticlesTM, Constraint.GetCullDistance());
				UpdateContactPoint(Constraint, ContactPoint, Dt);
			}
		}

		void UpdateLevelsetLevelsetManifold(FPBDCollisionConstraint& Constraint, const FRigidTransform3& ATM, const FRigidTransform3& BTM)
		{
			// @todo(chaos) : Stub Update Manifold
			//   Stub function for updating the manifold prior to the Apply and ApplyPushOut
		}


		template<typename T_TRAITS>
		void ConstructLevelsetLevelsetConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FBVHParticles* Simplicial0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FBVHParticles* Simplicial1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{
			const bool bAllowManifolds = Context.GetSettings().bAllowManifolds && bChaos_Collision_AllowLevelsetManifolds;
			FPBDCollisionConstraint* Constraint = nullptr;

			// @todo(chaos): We do not really support a runtime decision for the particle order now that we have persistent collisions. Fix the logic below...?
			// Also clean this logic up so it is readable. When is Simplical0 not equal to CollisionParticles0?
			bool bIsParticleDynamic0 = Particle0->CastToRigidParticle() && ((Particle0->ObjectState() == EObjectStateType::Dynamic) || (Particle0->ObjectState() == EObjectStateType::Sleeping));
			int32 P0NumCollisionParticles = Simplicial0 ? Simplicial0->Size() : Particle0->CastToRigidParticle()->CollisionParticlesSize();
			if (!Particle1->Geometry() || (bIsParticleDynamic0 && !P0NumCollisionParticles && Particle0->Geometry() && !Particle0->Geometry()->IsUnderlyingUnion()))
			{
				Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle1, Implicit1, Shape1, Simplicial1, LocalTransform1, Particle0, Implicit0, Shape0, Simplicial0, LocalTransform0, CullDistance, EContactShapesType::LevelSetLevelSet, bAllowManifolds, Context);
			}
			else
			{
				Constraint = Context.MultiShapeCollisionDetector->FindOrCreateConstraint(Particle0, Implicit0, Shape0, Simplicial0, LocalTransform0, Particle1, Implicit1, Shape1, Simplicial1, LocalTransform1, CullDistance, EContactShapesType::LevelSetLevelSet, bAllowManifolds, Context);
			}

			if (Constraint != nullptr)
			{
				// @todo(chaos): support manifold restoration for Levelsets and remove this
				Constraint->ResetActiveManifoldContacts();

				if (T_TRAITS::bImmediateUpdate && !Constraint->WasManifoldRestored())
				{
					FRigidTransform3 WorldTransform0 = Constraint->GetShapeRelativeTransform0() * ParticleWorldTransform0;
					FRigidTransform3 WorldTransform1 = Constraint->GetShapeRelativeTransform1() * ParticleWorldTransform1;
					UpdateLevelsetLevelsetConstraint<ECollisionUpdateType::Deepest>(WorldTransform0, WorldTransform1, Dt, *Constraint);
				}
			}
		}


		//
		// Constraint API
		//


		// Run collision detection for the specified constraint to update the nearest contact point.
		// NOTE: Transforms are world space shape transforms
		// @todo(chaos): use a lookup table?
		// @todo(chaos): add the missing cases below
		// @todo(chaos): see use GetInnerObject below - we should try to use the leaf types for all (currently only TriMesh needs this)
		template<ECollisionUpdateType UpdateType>
		inline void UpdateConstraintFromGeometryImpl(FPBDCollisionConstraint& Constraint, const FRigidTransform3& WorldTransform0, const FRigidTransform3& WorldTransform1, const FReal Dt)
		{
			// These add an extra 2% overhead in the simple shape cases
			//CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_UpdateConstraintFromGeometryInternal, ConstraintsDetailedStats);
			//INC_DWORD_STAT(STAT_ChaosCollisionCounter_NumContactUpdates);

			// @todo(chaos): remove
			//const FVec3 OriginalContactPositionLocal0 = WorldTransform0.InverseTransformPosition(Constraint.Manifold.Location);
			//const FVec3 OriginalContactPositionLocal1 = WorldTransform1.InverseTransformPosition(Constraint.Manifold.Location);
			if (!Constraint.GetImplicit0() || !Constraint.GetImplicit1())
			{
				return;
			}

			const FImplicitObject& Implicit0 = *Constraint.GetImplicit0();
			const FImplicitObject& Implicit1 = *Constraint.GetImplicit1();


			switch (Constraint.GetShapesType())
			{
			case EContactShapesType::SphereSphere:
				UpdateSphereSphereConstraint(*Implicit0.template GetObject<TSphere<FReal, 3>>(), WorldTransform0, *Implicit1.template GetObject<TSphere<FReal, 3>>(), WorldTransform1, Dt, Constraint);
				break;
			case EContactShapesType::SphereCapsule:
				UpdateSphereCapsuleConstraint(*Implicit0.template GetObject<TSphere<FReal, 3>>(), WorldTransform0, *Implicit1.template GetObject<FCapsule>(), WorldTransform1, Dt, Constraint);
				break;
			case EContactShapesType::SphereBox:
				UpdateSphereBoxConstraint(*Implicit0.template GetObject<TSphere<FReal, 3>>(), WorldTransform0, *Implicit1.template GetObject<TBox<FReal, 3>>(), WorldTransform1, Dt, Constraint);
				break;
			case EContactShapesType::SphereConvex:
				UpdateSphereConvexConstraint(*Implicit0.template GetObject<TSphere<FReal, 3>>(), WorldTransform0, Implicit1, WorldTransform1, Dt, Constraint);
				break;
			case EContactShapesType::SphereTriMesh:
				if (const TImplicitObjectScaled<FTriangleMeshImplicitObject>* ScaledTriMesh = Implicit1.template GetObject<const TImplicitObjectScaled<FTriangleMeshImplicitObject> >())
				{
					UpdateSphereTriangleMeshConstraint(*Implicit0.template GetObject<TSphere<FReal, 3>>(), WorldTransform0, *ScaledTriMesh, WorldTransform1, Dt, Constraint);
					break;
				}
				else if (const FTriangleMeshImplicitObject* TriangleMeshImplicit = Implicit1.template GetObject<const FTriangleMeshImplicitObject>())
				{
					UpdateSphereTriangleMeshConstraint(*Implicit0.template GetObject<TSphere<FReal, 3>>(), WorldTransform0, *TriangleMeshImplicit, WorldTransform1, Dt, Constraint);
					break;
				}
				ensure(false);
				break;
			case EContactShapesType::SphereHeightField:
				UpdateSphereHeightFieldConstraint(*Implicit0.template GetObject<TSphere<FReal, 3>>(), WorldTransform0, *Implicit1.template GetObject< FHeightField >(), WorldTransform1, Dt, Constraint);
				break;
			case EContactShapesType::SpherePlane:
				UpdateSpherePlaneConstraint(*Implicit0.template GetObject<TSphere<FReal, 3>>(), WorldTransform0, *Implicit1.template GetObject<TPlane<FReal, 3>>(), WorldTransform1, Dt, Constraint);
				break;
			case EContactShapesType::CapsuleCapsule:
				UpdateCapsuleCapsuleConstraint(*Implicit0.template GetObject<FCapsule>(), WorldTransform0, *Implicit1.template GetObject<FCapsule>(), WorldTransform1, Dt, Constraint);
				break;
			case EContactShapesType::CapsuleBox:
				UpdateCapsuleBoxConstraint(*Implicit0.template GetObject<FCapsule>(), WorldTransform0, *Implicit1.template GetObject<TBox<FReal, 3>>(), WorldTransform1, Dt, Constraint);
				break;
			case EContactShapesType::CapsuleConvex:
				UpdateCapsuleGenericConvexConstraintHelper(*Implicit0.template GetObject<FCapsule>(), WorldTransform0, Implicit1, WorldTransform1, Dt, Constraint);
				break;
			case EContactShapesType::CapsuleTriMesh:
				if (const TImplicitObjectScaled<FTriangleMeshImplicitObject>* ScaledTriMesh = Implicit1.template GetObject<const TImplicitObjectScaled<FTriangleMeshImplicitObject> >())
				{
					UpdateCapsuleTriangleMeshConstraint(*Implicit0.template GetObject<FCapsule>(), WorldTransform0, *ScaledTriMesh, WorldTransform1, Dt, Constraint);
					break;
				}
				else if (const FTriangleMeshImplicitObject* TriangleMeshImplicit = Implicit1.template GetObject<const FTriangleMeshImplicitObject>())
				{
					UpdateCapsuleTriangleMeshConstraint(*Implicit0.template GetObject<FCapsule>(), WorldTransform0, *TriangleMeshImplicit, WorldTransform1, Dt, Constraint);
					break;
				}
				ensure(false);
				break;
			case EContactShapesType::CapsuleHeightField:
				UpdateCapsuleHeightFieldConstraint(*Implicit0.template GetObject<FCapsule>(), WorldTransform0, *Implicit1.template GetObject< FHeightField >(), WorldTransform1, Dt, Constraint);
				break;
			case EContactShapesType::BoxBox:
				UpdateBoxBoxConstraint(*Implicit0.template GetObject<TBox<FReal, 3>>(), WorldTransform0, *Implicit1.template GetObject<TBox<FReal, 3>>(), WorldTransform1, Dt, Constraint);
				break;
			case EContactShapesType::BoxConvex:
				UpdateGenericConvexConvexConstraint(Implicit0, WorldTransform0, Implicit1, WorldTransform1, Dt, Constraint);
				break;
			case EContactShapesType::BoxTriMesh:
				if (const TImplicitObjectScaled<FTriangleMeshImplicitObject>* ScaledTriMesh = Implicit1.template GetObject<const TImplicitObjectScaled<FTriangleMeshImplicitObject> >())
				{
					UpdateBoxTriangleMeshConstraint(*Implicit0.template GetObject<TBox<FReal, 3>>(), WorldTransform0, *ScaledTriMesh, WorldTransform1, Dt, Constraint);
					break;
				}
				else if (const FTriangleMeshImplicitObject* TriangleMeshImplicit = Implicit1.template GetObject<const FTriangleMeshImplicitObject>())
				{
					UpdateBoxTriangleMeshConstraint(*Implicit0.template GetObject<TBox<FReal, 3>>(), WorldTransform0, *TriangleMeshImplicit, WorldTransform1, Dt, Constraint);
					break;
				}
				ensure(false);
				break;
			case EContactShapesType::BoxHeightField:
				UpdateBoxHeightFieldConstraint(*Implicit0.template GetObject<TBox<FReal, 3>>(), WorldTransform0, *Implicit1.template GetObject< FHeightField >(), WorldTransform1, Dt, Constraint);
				break;
			case EContactShapesType::BoxPlane:
				UpdateBoxPlaneConstraint(*Implicit0.template GetObject<TBox<FReal, 3>>(), WorldTransform0, *Implicit1.template GetObject<TPlane<FReal, 3>>(), WorldTransform1, Dt, Constraint);
				break;
			case EContactShapesType::GenericConvexConvex:
				UpdateGenericConvexConvexConstraint(Implicit0, WorldTransform0, Implicit1, WorldTransform1, Dt, Constraint);
				break;
			case EContactShapesType::ConvexTriMesh:
				UpdateConvexTriangleMeshConstraint(Implicit0, WorldTransform0, Implicit1, WorldTransform1, Dt, Constraint);
				break;
			case EContactShapesType::ConvexHeightField:
				UpdateConvexHeightFieldConstraint(Implicit0, WorldTransform0, *Implicit1.template GetObject< FHeightField >(), WorldTransform1, Dt, Constraint);
				break;
			case EContactShapesType::LevelSetLevelSet:
				UpdateLevelsetLevelsetConstraint<UpdateType>(WorldTransform0, WorldTransform1, Dt, Constraint);
				break;
			default:
				// Switch needs updating....
				ensure(false);
				break;
			}
		}

		// Run collision detection for the swept constraints
		// NOTE: Transforms are world space shape transforms
		
		template<ECollisionUpdateType UpdateType>
		inline bool UpdateConstraintFromGeometrySweptImpl(FPBDCollisionConstraint& Constraint, const FRigidTransform3& WorldTransform0, const FRigidTransform3& WorldTransform1, const FReal Dt)
		{
			INC_DWORD_STAT(STAT_ChaosCollisionCounter_NumContactUpdates);

			TGeometryParticleHandle<FReal, 3>* Particle0 = Constraint.GetParticle0();
			TGeometryParticleHandle<FReal, 3>* Particle1 = Constraint.GetParticle1();
			const FImplicitObject& Implicit0 = *Constraint.GetImplicit0();
			const FImplicitObject& Implicit1 = *Constraint.GetImplicit1();

			FReal LengthCCD = 0.0f;
			FVec3 DirCCD(0.0f);
			const FVec3 DeltaX0 = Constraint.GetShapeWorldTransform0().GetTranslation() - WorldTransform0.GetLocation();
			const FVec3 DeltaX1 = Constraint.GetShapeWorldTransform1().GetTranslation() - WorldTransform1.GetLocation();
			bool bUseCCD = ShouldUseCCD(Particle0, DeltaX0, Particle1, DeltaX1, DirCCD, LengthCCD);

			Constraint.ResetCCDTimeOfImpact();

			if (bUseCCD)
			{
				const EContactShapesType ShapePairType = Constraint.GetShapesType();
				switch (ShapePairType)
				{
				case EContactShapesType::SphereSphere:
				case EContactShapesType::SphereCapsule:
				case EContactShapesType::SphereBox:
				case EContactShapesType::SphereConvex:
				case EContactShapesType::SpherePlane:
				case EContactShapesType::CapsuleCapsule:
				case EContactShapesType::CapsuleBox:
				case EContactShapesType::CapsuleConvex:
				case EContactShapesType::BoxBox:
				case EContactShapesType::BoxConvex:
				case EContactShapesType::BoxPlane:
				case EContactShapesType::GenericConvexConvex:
					UpdateGenericConvexConvexConstraintSwept(Particle0, Implicit0, WorldTransform0, Particle1, Implicit1, WorldTransform1, DirCCD, LengthCCD, Dt, Constraint);
					return true;
				case EContactShapesType::SphereHeightField:
				{
					const TSphere<FReal, 3>* Object0 = Implicit0.template GetObject<const TSphere<FReal, 3>>();
					const FHeightField* Object1 = Implicit1.template GetObject<const FHeightField>();
					UpdateSphereHeightFieldConstraintSwept(Particle0, *Object0, WorldTransform0, *Object1, WorldTransform1, DirCCD, LengthCCD, Dt, Constraint);
					return true;
				}
				case EContactShapesType::CapsuleHeightField:
				{
					const FCapsule* Object0 = Implicit0.template GetObject<const FCapsule >();
					const FHeightField* Object1 = Implicit1.template GetObject<const FHeightField >();
					UpdateCapsuleHeightFieldConstraintSwept(Particle0, *Object0, WorldTransform0, *Object1, WorldTransform1, DirCCD, LengthCCD, Dt, Constraint);
					return true;
				}
				case EContactShapesType::SphereTriMesh:
				{
					const TSphere<FReal, 3>* Object0 = Implicit0.template GetObject<const TSphere<FReal, 3>>();
					if (const TImplicitObjectScaled<FTriangleMeshImplicitObject>* ScaledTriangleMesh = Implicit1.template GetObject<const TImplicitObjectScaled<FTriangleMeshImplicitObject>>())
					{
						UpdateSphereTriangleMeshConstraintSwept(Particle0, *Object0, WorldTransform0, *ScaledTriangleMesh, WorldTransform1, DirCCD, LengthCCD, Dt, Constraint);
					}
					else if (const FTriangleMeshImplicitObject* TriangleMesh = Implicit1.template GetObject<const FTriangleMeshImplicitObject>())
					{
						UpdateSphereTriangleMeshConstraintSwept(Particle0, *Object0, WorldTransform0, *TriangleMesh, WorldTransform1, DirCCD, LengthCCD, Dt, Constraint);
					}
					else
					{
						ensure(false);
					}
					return true;
				}
				case EContactShapesType::CapsuleTriMesh:
				{
					const FCapsule* Object0 = Implicit0.template GetObject<const FCapsule >();
					if (const TImplicitObjectScaled<FTriangleMeshImplicitObject>* ScaledTriangleMesh = Implicit1.template GetObject<const TImplicitObjectScaled<FTriangleMeshImplicitObject>>())
					{
						UpdateCapsuleTriangleMeshConstraintSwept(Particle0, *Object0, WorldTransform0, *ScaledTriangleMesh, WorldTransform1, DirCCD, LengthCCD, Dt, Constraint);
					}
					else if (const FTriangleMeshImplicitObject* TriangleMesh = Implicit1.template GetObject<const FTriangleMeshImplicitObject>())
					{
						UpdateCapsuleTriangleMeshConstraintSwept(Particle0, *Object0, WorldTransform0, *TriangleMesh, WorldTransform1, DirCCD, LengthCCD, Dt, Constraint);
					}
					else
					{
						ensure(false);
					}
					return true;
				}
				case EContactShapesType::BoxHeightField:
				case EContactShapesType::ConvexHeightField:
				{
					const FHeightField* Object1 = Implicit1.template GetObject<const FHeightField >();
					UpdateConvexHeightFieldConstraintSwept(Particle0, Implicit0, WorldTransform0, *Object1, WorldTransform1, DirCCD, LengthCCD, Dt, Constraint);
					return true;
				}
				case EContactShapesType::BoxTriMesh:
				case EContactShapesType::ConvexTriMesh:
					if (const TImplicitObjectScaled<FTriangleMeshImplicitObject>* ScaledTriangleMesh = Implicit1.template GetObject<const TImplicitObjectScaled<FTriangleMeshImplicitObject>>())
					{
						UpdateConvexTriangleMeshConstraintSwept(Particle0, Implicit0, WorldTransform0, *ScaledTriangleMesh, WorldTransform1, DirCCD, LengthCCD, Dt, Constraint);
					}
					else if (const FTriangleMeshImplicitObject* TriangleMesh = Implicit1.template GetObject<const FTriangleMeshImplicitObject>())
					{
						UpdateConvexTriangleMeshConstraintSwept(Particle0, Implicit0, WorldTransform0, *TriangleMesh, WorldTransform1, DirCCD, LengthCCD, Dt, Constraint);
					}
					else
					{
						ensure(false);
					}
					return true;
				default:
					ensure(false);
					break;
				}
			}

			// CCD will not be used for this constraint this tick
			return false;
		}

		template<typename T_TRAITS>
		void ConstructConstraintsImpl(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FBVHParticles* Simplicial0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FBVHParticles* Simplicial1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal Dt, const FCollisionContext& Context)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_ConstructConstraintsInternal, ConstraintsDetailedStats);
			INC_DWORD_STAT(STAT_ChaosCollisionCounter_NumShapePairs);

			// @todo(chaos): We use GetInnerType here because TriMeshes are left with their "Instanced" wrapper, unlike all other instanced implicits. Should we strip the instance on Tri Mesh too?
			EImplicitObjectType Implicit0Type = Implicit0 ? GetInnerType(Implicit0->GetCollisionType()) : ImplicitObjectType::Unknown;
			EImplicitObjectType Implicit1Type = Implicit1 ? GetInnerType(Implicit1->GetCollisionType()) : ImplicitObjectType::Unknown;
			bool bIsConvex0 = Implicit0 && Implicit0->IsConvex() && Implicit0Type != ImplicitObjectType::LevelSet;
			bool bIsConvex1 = Implicit1 && Implicit1->IsConvex() && Implicit1Type != ImplicitObjectType::LevelSet;

			FReal LengthCCD = 0.0f;
			FVec3 DirCCD(0.0f);
			FConstGenericParticleHandle ConstParticle0 = FConstGenericParticleHandle(Particle0);
			FConstGenericParticleHandle ConstParticle1 = FConstGenericParticleHandle(Particle1);

			// X is end-frame position for kinematic particles. To get start-frame position, we need to use P - V * Dt.
			const FVec3 StartX0 = (ConstParticle0->ObjectState() == EObjectStateType::Kinematic) ? (ConstParticle0->P() - ConstParticle0->V() * Dt) : ConstParticle0->X();
			const FVec3 StartX1 = (ConstParticle1->ObjectState() == EObjectStateType::Kinematic) ? (ConstParticle1->P() - ConstParticle1->V() * Dt) : ConstParticle1->X();
			const FVec3 EndX0 = ConstParticle0->P();
			const FVec3 EndX1 = ConstParticle1->P();
			bool bUseCCD = Context.GetSettings().bAllowCCD && ShouldUseCCD(Particle0, EndX0 - StartX0, Particle1, EndX1 - StartX1, DirCCD, LengthCCD);
			const bool bUseGenericSweptConstraints = bUseCCD && (CCDUseGenericSweptConvexConstraints > 0) && bIsConvex0 && bIsConvex1;
#if CHAOS_COLLISION_CREATE_BOUNDSCHECK
			if ((Implicit0 != nullptr) && (Implicit1 != nullptr))
			{
				if (!bUseCCD)
				{
					if (Implicit0->HasBoundingBox() && Implicit1->HasBoundingBox())
					{
						const FRigidTransform3 WorldTransform0 = LocalTransform0 * ParticleWorldTransform0;
						const FRigidTransform3 WorldTransform1 = LocalTransform1 * ParticleWorldTransform1;
						if (Chaos_Collision_NarrowPhase_SphereBoundsCheck)
						{
							const FReal R1 = Implicit0->BoundingBox().OriginRadius();
							const FReal R2 = Implicit1->BoundingBox().OriginRadius();
							const FReal SeparationSq = (WorldTransform1.GetTranslation() - WorldTransform0.GetTranslation()).SizeSquared();
							if (SeparationSq > FMath::Square(R1 + R2 + CullDistance))
							{
								return;
							}
						}

						if (Chaos_Collision_NarrowPhase_AABBBoundsCheck)
						{
							const FAABB3 Box1 = Implicit0->BoundingBox();
							const FAABB3 Box2 = Implicit1->BoundingBox();
							//if (Box1.GetVolume() >= Box2.GetVolume())
							{
								const FRigidTransform3 Box2ToBox1TM = WorldTransform1.GetRelativeTransform(WorldTransform0);
								const FAABB3 Box2In1 = Box2.TransformedAABB(Box2ToBox1TM).Thicken(CullDistance);
								if (!Box1.Intersects(Box2In1))
								{
									return;
								}
							}
							//else
							{
								const FRigidTransform3 Box1ToBox2TM = WorldTransform0.GetRelativeTransform(WorldTransform1);
								const FAABB3 Box1In2 = Box1.TransformedAABB(Box1ToBox2TM).Thicken(CullDistance);
								if (!Box2.Intersects(Box1In2))
								{
									return;
								}
							}
						}
					}
				}
				else
				{
					const FAABB3 Box0 = Particle0->WorldSpaceInflatedBounds();
					const FAABB3 Box1 = Particle1->WorldSpaceInflatedBounds();
					if (!Box0.Intersects(Box1))
					{
						return;
					}
				}
			}
#endif

			INC_DWORD_STAT(STAT_ChaosCollisionCounter_NumContactsCreated);

			if (Implicit0Type == TBox<FReal, 3>::StaticType() && Implicit1Type == TBox<FReal, 3>::StaticType() && !bUseGenericSweptConstraints)
			{
				ConstructBoxBoxConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == TBox<FReal, 3>::StaticType() && Implicit1Type == FHeightField::StaticType())
			{
				ConstructBoxHeightFieldConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == FHeightField::StaticType() && Implicit1Type == TBox<FReal, 3>::StaticType())
			{
				ConstructBoxHeightFieldConstraints<T_TRAITS>(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == TBox<FReal, 3>::StaticType() && Implicit1Type == TPlane<FReal, 3>::StaticType() && !bUseGenericSweptConstraints)
			{
				ConstructBoxPlaneConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == TPlane<FReal, 3>::StaticType() && Implicit1Type == TBox<FReal, 3>::StaticType() && !bUseGenericSweptConstraints)
			{
				ConstructBoxPlaneConstraints<T_TRAITS>(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == TSphere<FReal, 3>::StaticType() && Implicit1Type == TSphere<FReal, 3>::StaticType() && !bUseGenericSweptConstraints)
			{
				ConstructSphereSphereConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == TSphere<FReal, 3>::StaticType() && Implicit1Type == FHeightField::StaticType())
			{
				if (bUseCCD)
				{
					ConstructSphereHeightFieldConstraintsSwept(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, DirCCD, LengthCCD, Context);
					return;
				}
				ConstructSphereHeightFieldConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == FHeightField::StaticType() && Implicit1Type == TSphere<FReal, 3>::StaticType())
			{
				if (bUseCCD)
				{
					ConstructSphereHeightFieldConstraintsSwept(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, DirCCD, LengthCCD, Context);
					return;
				}
				ConstructSphereHeightFieldConstraints<T_TRAITS>(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == TSphere<FReal, 3>::StaticType() && Implicit1Type == TPlane<FReal, 3>::StaticType() && !bUseGenericSweptConstraints)
			{
				ConstructSpherePlaneConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == TPlane<FReal, 3>::StaticType() && Implicit1Type == TSphere<FReal, 3>::StaticType() && !bUseGenericSweptConstraints)
			{
				ConstructSpherePlaneConstraints<T_TRAITS>(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == TSphere<FReal, 3>::StaticType() && Implicit1Type == TBox<FReal, 3>::StaticType() && !bUseGenericSweptConstraints)
			{
				ConstructSphereBoxConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == TBox<FReal, 3>::StaticType() && Implicit1Type == TSphere<FReal, 3>::StaticType() && !bUseGenericSweptConstraints)
			{
				ConstructSphereBoxConstraints<T_TRAITS>(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == TSphere<FReal, 3>::StaticType() && Implicit1Type == FCapsule::StaticType() && !bUseGenericSweptConstraints)
			{
				ConstructSphereCapsuleConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == FCapsule::StaticType() && Implicit1Type == TSphere<FReal, 3>::StaticType() && !bUseGenericSweptConstraints)
			{
				ConstructSphereCapsuleConstraints<T_TRAITS>(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == TSphere<FReal, 3>::StaticType() && Implicit1Type == FImplicitConvex3::StaticType() && !bUseGenericSweptConstraints)
			{
				ConstructSphereConvexConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == FImplicitConvex3::StaticType() && Implicit1Type == TSphere<FReal, 3>::StaticType() && !bUseGenericSweptConstraints)
			{
				ConstructSphereConvexConstraints<T_TRAITS>(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == FCapsule::StaticType() && Implicit1Type == FCapsule::StaticType() && !bUseGenericSweptConstraints)
			{
				ConstructCapsuleCapsuleConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == FCapsule::StaticType() && Implicit1Type == TBox<FReal, 3>::StaticType() && !bUseGenericSweptConstraints)
			{
				ConstructCapsuleBoxConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == TBox<FReal, 3>::StaticType() && Implicit1Type == FCapsule::StaticType() && !bUseGenericSweptConstraints)
			{
				ConstructCapsuleBoxConstraints<T_TRAITS>(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == FCapsule::StaticType() && Implicit1Type == FImplicitConvex3::StaticType() && !bUseGenericSweptConstraints)
			{
				ConstructCapsuleConvexConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == FImplicitConvex3::StaticType() && Implicit1Type == FCapsule::StaticType() && !bUseGenericSweptConstraints)
			{
				ConstructCapsuleConvexConstraints<T_TRAITS>(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == FCapsule::StaticType() && Implicit1Type == FHeightField::StaticType())
			{
				if (bUseCCD)
				{
					ConstructCapsuleHeightFieldConstraintsSwept(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, DirCCD, LengthCCD, Context);
					return;
				}

				ConstructCapsuleHeightFieldConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == FHeightField::StaticType() && Implicit1Type == FCapsule::StaticType())
			{
				if (bUseCCD)
				{
					ConstructCapsuleHeightFieldConstraintsSwept(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, DirCCD, LengthCCD, Context);
					return;
				}

				ConstructCapsuleHeightFieldConstraints<T_TRAITS>(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == TBox<FReal, 3>::StaticType() && Implicit1Type == FTriangleMeshImplicitObject::StaticType())
			{
				ConstructBoxTriangleMeshConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == FTriangleMeshImplicitObject::StaticType() && Implicit1Type == TBox<FReal, 3>::StaticType())
			{
				ConstructBoxTriangleMeshConstraints<T_TRAITS>(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == TSphere<FReal, 3>::StaticType() && Implicit1Type == FTriangleMeshImplicitObject::StaticType())
			{
				if (bUseCCD)
				{
					ConstructSphereTriangleMeshConstraintsSwept(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, DirCCD, LengthCCD, Context);
					return;
				}
				ConstructSphereTriangleMeshConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == FTriangleMeshImplicitObject::StaticType() && Implicit1Type == TSphere<FReal, 3>::StaticType())
			{
				if (bUseCCD)
				{
					ConstructSphereTriangleMeshConstraintsSwept(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, DirCCD, LengthCCD, Context);
					return;
				}
				ConstructSphereTriangleMeshConstraints<T_TRAITS>(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == FCapsule::StaticType() && Implicit1Type == FTriangleMeshImplicitObject::StaticType())
			{
				if (bUseCCD)
				{
					ConstructCapsuleTriangleMeshConstraintsSwept(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, DirCCD, LengthCCD, Context);
					return;
				}

				ConstructCapsuleTriangleMeshConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == FTriangleMeshImplicitObject::StaticType() && Implicit1Type == FCapsule::StaticType())
			{
				if (bUseCCD)
				{
					ConstructCapsuleTriangleMeshConstraintsSwept(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, DirCCD, LengthCCD, Context);
					return;
				}

				ConstructCapsuleTriangleMeshConstraints<T_TRAITS>(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, Context);
			}
			else if (bIsConvex0 && Implicit1Type == FHeightField::StaticType())
			{
				if (bUseCCD)
				{
					ConstructConvexHeightFieldConstraintsSwept(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, DirCCD, LengthCCD, Context);
					return;
				}

				ConstructConvexHeightFieldConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == FHeightField::StaticType() && bIsConvex1)
			{
				if (bUseCCD)
				{
					ConstructConvexHeightFieldConstraintsSwept(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, DirCCD, LengthCCD, Context);
					return;
				}

				ConstructConvexHeightFieldConstraints<T_TRAITS>(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, Context);
			}
			else if (bIsConvex0 && Implicit1Type == FTriangleMeshImplicitObject::StaticType())
			{
				if (bUseCCD)
				{
					ConstructConvexTriangleMeshConstraintsSwept(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, DirCCD, LengthCCD, Context);
					return;
				}

				ConstructConvexTriangleMeshConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else if (Implicit0Type == FTriangleMeshImplicitObject::StaticType() && bIsConvex1)
			{
				if (bUseCCD)
				{
					ConstructConvexTriangleMeshConstraintsSwept(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, DirCCD, LengthCCD, Context);
					return;
				}

				ConstructConvexTriangleMeshConstraints<T_TRAITS>(Particle1, Particle0, Implicit1, Shape1, Implicit0, Shape0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, Context);
			}
			else if (bIsConvex0 && bIsConvex1)
			{
				if (bUseCCD)
				{
					ConstructGenericConvexConvexConstraintsSwept(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, StartX0, ParticleWorldTransform1, LocalTransform1, StartX1, CullDistance, Dt, DirCCD, LengthCCD, Context);
					return;
				}

				ConstructGenericConvexConvexConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Implicit1, Shape1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
			}
			else
			{
				// LevelSets assume that the first or both particles are dynamic
				bool bSwap = (!FConstGenericParticleHandle(Particle0)->IsDynamic() && FConstGenericParticleHandle(Particle1)->IsDynamic());
				if (!bSwap)
				{
					ConstructLevelsetLevelsetConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Simplicial0, Implicit1, Shape1, Simplicial1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
				}
				else
				{
					ConstructLevelsetLevelsetConstraints<T_TRAITS>(Particle1, Particle0, Implicit1, Shape1, Simplicial1, Implicit0, Shape0, Simplicial0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, Context);
				}
			}
		}


		// Run collision detection for the specified constraint to update the nearest contact point.
		// NOTE: Transforms are world space shape transforms
		void UpdateConstraint(FPBDCollisionConstraint& Constraint, const FRigidTransform3& ShapeWorldTransform0, const FRigidTransform3& ShapeWorldTransform1, const FReal Dt)
		{
			UpdateConstraintFromGeometryImpl<ECollisionUpdateType::Deepest>(Constraint, ShapeWorldTransform0, ShapeWorldTransform1, Dt);
		}

		bool UpdateConstraintSwept(FPBDCollisionConstraint& Constraint, const FRigidTransform3& ShapeStartWorldTransform0, const FRigidTransform3& ShapeStartWorldTransform1, const FReal Dt)
		{
			return UpdateConstraintFromGeometrySweptImpl<ECollisionUpdateType::Deepest>(Constraint, ShapeStartWorldTransform0, ShapeStartWorldTransform1, Dt);
		}

		// Run collision detection for the specified constraint to update the nearest contact point.
		// NOTE: Transforms are world space particle transforms
		template<ECollisionUpdateType UpdateType>
		void UpdateConstraintFromGeometry(FPBDCollisionConstraint& Constraint, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& ParticleWorldTransform1, const FReal Dt)
		{
			const FRigidTransform3 WorldTransform0 = Constraint.GetShapeRelativeTransform0() * ParticleWorldTransform0;
			const FRigidTransform3 WorldTransform1 = Constraint.GetShapeRelativeTransform1() * ParticleWorldTransform1;
			Constraint.SetShapeWorldTransforms(WorldTransform0, WorldTransform1);

			UpdateConstraintFromGeometryImpl<UpdateType>(Constraint, WorldTransform0, WorldTransform1, Dt);
		}

		template<typename T_TRAITS>
		void LegacyConstructConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FShapeOrShapesArray& Shapes0, const FBVHParticles* Simplicial0, const FImplicitObject* Implicit1, const FShapeOrShapesArray& Shapes1, const FBVHParticles* Simplicial1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, FReal Dt, const FCollisionContext& Context)
		{
			CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_ConstructConstraints, ConstraintsDetailedStats);

			EImplicitObjectType Implicit0Type = Implicit0 ? GetInnerType(Implicit0->GetType()) : ImplicitObjectType::Unknown;
			EImplicitObjectType Implicit1Type = Implicit1 ? GetInnerType(Implicit1->GetType()) : ImplicitObjectType::Unknown;


			if (!Implicit0 || !Implicit1)
			{
				const FPerShapeData* Shape0	= nullptr;
				if (ensure(Shapes0.IsSingleShape()))
				{
					Shape0 = Shapes0.GetShape();
				}
				const FPerShapeData* Shape1 = nullptr;
				if (ensure(Shapes1.IsSingleShape()))
				{
					Shape1 = Shapes1.GetShape();
				}

				ConstructLevelsetLevelsetConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Simplicial0, Implicit1, Shape1, Simplicial1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
				return;
			}

			EImplicitObjectType Implicit0OuterType = Implicit0->GetType();
			EImplicitObjectType Implicit1OuterType = Implicit1->GetType();

			// Handle transform wrapper shape
			if ((Implicit0OuterType == TImplicitObjectTransformed<FReal, 3>::StaticType()) && (Implicit1OuterType == TImplicitObjectTransformed<FReal, 3>::StaticType()))
			{
				const TImplicitObjectTransformed<FReal, 3>* TransformedImplicit0 = Implicit0->template GetObject<const TImplicitObjectTransformed<FReal, 3>>();
				const TImplicitObjectTransformed<FReal, 3>* TransformedImplicit1 = Implicit1->template GetObject<const TImplicitObjectTransformed<FReal, 3>>();
				FRigidTransform3 TransformedTransform0 = TransformedImplicit0->GetTransform() * LocalTransform0;
				FRigidTransform3 TransformedTransform1 = TransformedImplicit1->GetTransform() * LocalTransform1;
				LegacyConstructConstraints<T_TRAITS>(Particle0, Particle1, TransformedImplicit0->GetTransformedObject(), Shapes0, Simplicial0, TransformedImplicit1->GetTransformedObject(), Shapes1, Simplicial1, ParticleWorldTransform0, TransformedTransform0, ParticleWorldTransform1, TransformedTransform1, CullDistance, Dt, Context);
				return;
			}
			else if (Implicit0OuterType == TImplicitObjectTransformed<FReal, 3>::StaticType())
			{
				const TImplicitObjectTransformed<FReal, 3>* TransformedImplicit0 = Implicit0->template GetObject<const TImplicitObjectTransformed<FReal, 3>>();
				FRigidTransform3 TransformedTransform0 = TransformedImplicit0->GetTransform() * LocalTransform0;
				LegacyConstructConstraints<T_TRAITS>(Particle0, Particle1, TransformedImplicit0->GetTransformedObject(), Shapes0, Simplicial0, Implicit1, Shapes1, Simplicial1, ParticleWorldTransform0, TransformedTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
				return;
			}
			else if (Implicit1OuterType == TImplicitObjectTransformed<FReal, 3>::StaticType())
			{
				const TImplicitObjectTransformed<FReal, 3>* TransformedImplicit1 = Implicit1->template GetObject<const TImplicitObjectTransformed<FReal, 3>>();
				FRigidTransform3 TransformedTransform1 = TransformedImplicit1->GetTransform() * LocalTransform1;
				LegacyConstructConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shapes0, Simplicial0, TransformedImplicit1->GetTransformedObject(), Shapes1, Simplicial1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, TransformedTransform1, CullDistance, Dt, Context);
				return;
			}

			// Strip the Instanced wrapper from most shapes, but not Convex or TriMesh.
			// Convex collision requires the wrapper because it holds the margin.
			// @todo(chaos): this collision logic is getting out of hand - can we make a better shape class hierarchy?
			if (((uint32)Implicit0OuterType & ImplicitObjectType::IsInstanced) || ((uint32)Implicit1OuterType & ImplicitObjectType::IsInstanced))
			{
				const FImplicitObject* InnerImplicit0 = nullptr;
				const FImplicitObject* InnerImplicit1 = nullptr;
				if (((uint32)Implicit0OuterType & ImplicitObjectType::IsInstanced) && (Implicit0Type != FImplicitConvex3::StaticType()))
				{
					InnerImplicit0 = GetInstancedImplicit(Implicit0);
				}
				if (((uint32)Implicit1OuterType & ImplicitObjectType::IsInstanced) && (Implicit1Type != FImplicitConvex3::StaticType()))
				{
					InnerImplicit1 = GetInstancedImplicit(Implicit1);
				}
				if (InnerImplicit0 && InnerImplicit1)
				{
					LegacyConstructConstraints<T_TRAITS>(Particle0, Particle1, InnerImplicit0, Shapes0, Simplicial0, InnerImplicit1, Shapes1, Simplicial1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
					return;
				}
				else if (InnerImplicit0 && !InnerImplicit1)
				{
					LegacyConstructConstraints<T_TRAITS>(Particle0, Particle1, InnerImplicit0, Shapes0, Simplicial0, Implicit1, Shapes1, Simplicial1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
					return;
				}
				else if (!InnerImplicit0 && InnerImplicit1)
				{
					LegacyConstructConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shapes0, Simplicial0, InnerImplicit1, Shapes1, Simplicial1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
					return;
				}

			}

			// Handle Unions
			if (Implicit0OuterType == FImplicitObjectUnion::StaticType())
			{
				const FImplicitObjectUnion* Union0 = Implicit0->template GetObject<FImplicitObjectUnion>();
				int32 ShapeIdx = 0;
				for (const auto& Child0 : Union0->GetObjects())
				{
					if (ensure(!Shapes0.IsSingleShape()))
					{
						const FShapesArray& ShapesArray = *Shapes0.GetShapesArray();
						const FPerShapeData* ChildShape0 = ShapesArray[ShapeIdx].Get();
					
						// If shape is not sim'd, we may end up iterating over a lot of shapes on particle1's union and wasting time filtering.
						if (Context.GetSettings().bFilteringEnabled == false || FilterHasSimEnabled(ChildShape0))
						{
							LegacyConstructConstraints<T_TRAITS>(Particle0, Particle1, Child0.Get(), FShapeOrShapesArray(ChildShape0), Simplicial0, Implicit1, Shapes1, Simplicial1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
						}
					}

					ShapeIdx++;
				}
				return;
			}

			if (Implicit0OuterType == FImplicitObjectUnionClustered::StaticType())
			{
				const FImplicitObjectUnionClustered* Union0 = Implicit0->template GetObject<FImplicitObjectUnionClustered>();
				if (Implicit1->HasBoundingBox())
				{
					TArray<FLargeUnionClusteredImplicitInfo> Children;

					// Need to get transformed bounds of 1 in the space of 0
					FRigidTransform3 TM0 = LocalTransform0 * ParticleWorldTransform0;
					FRigidTransform3 TM1 = LocalTransform1 * ParticleWorldTransform1;
					FRigidTransform3 TM1ToTM0 = TM1.GetRelativeTransform(TM0);
					FAABB3 QueryBounds = Implicit1->BoundingBox().TransformedAABB(TM1ToTM0);

					Union0->FindAllIntersectingClusteredObjects(Children, QueryBounds);

					for (const FLargeUnionClusteredImplicitInfo& Child0 : Children)
					{
						// Provide no shape for clustered child implicits, these use their parent's filter data, which was already tested when generating shape pair in midphase.
						LegacyConstructConstraints<T_TRAITS>(Particle0, Particle1, Child0.Implicit, FShapeOrShapesArray(), Child0.BVHParticles, Implicit1, Shapes1, Simplicial1, ParticleWorldTransform0, Child0.Transform * LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
					}
				}
				else
				{
					for (const TUniquePtr<FImplicitObject>& Child0 : Union0->GetObjects())
					{
						const TPBDRigidParticleHandle<FReal, 3>* OriginalParticle = Union0->FindParticleForImplicitObject(Child0.Get());

						// Provide no shape for clustered child implicits, these use their parent's filter data, which was already tested when generating shape pair in midphase.
						LegacyConstructConstraints<T_TRAITS>(Particle0, Particle1, Child0.Get(), FShapeOrShapesArray(), OriginalParticle ? OriginalParticle->CollisionParticles().Get() : Simplicial0, Implicit1, Shapes1, Simplicial1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
					}
				}
				return;
			}

			if (Implicit1OuterType == FImplicitObjectUnion::StaticType())
			{
				const FImplicitObjectUnion* Union1 = Implicit1->template GetObject<FImplicitObjectUnion>();
				int32 ShapeIdx = 0;
				for (const auto& Child1 : Union1->GetObjects())
				{
					if (ensure(!Shapes1.IsSingleShape()))
					{
						const FShapesArray& ShapesArray = *Shapes1.GetShapesArray();
						const FPerShapeData* ChildShape1 = ShapesArray[ShapeIdx].Get();

						// If shape is not sim'd, we may end up iterating over a lot of shapes on particle1's union and wasting time filtering.
						if (Context.GetSettings().bFilteringEnabled == false || FilterHasSimEnabled(ChildShape1))
						{
							LegacyConstructConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shapes0, Simplicial0, Child1.Get(), FShapeOrShapesArray(ChildShape1), Simplicial1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
						}
					}

					ShapeIdx++;
				}
				return;
			}

			if (Implicit1OuterType == FImplicitObjectUnionClustered::StaticType())
			{
				const FImplicitObjectUnionClustered* Union1 = Implicit1->template GetObject<FImplicitObjectUnionClustered>();
				if (Implicit0->HasBoundingBox())
				{
					TArray<FLargeUnionClusteredImplicitInfo> Children;

					// Need to get transformed bounds of 0 in the space of 1
					FRigidTransform3 TM0 = LocalTransform0 * ParticleWorldTransform0;
					FRigidTransform3 TM1 = LocalTransform1 * ParticleWorldTransform1;
					FRigidTransform3 TM0ToTM1 = TM0.GetRelativeTransform(TM1);
					FAABB3 QueryBounds = Implicit0->BoundingBox().TransformedAABB(TM0ToTM1);

					{
						CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_Collisions_FindAllIntersectingClusteredObjects, ConstraintsDetailedStats);
						Union1->FindAllIntersectingClusteredObjects(Children, QueryBounds);
					}

					for (const FLargeUnionClusteredImplicitInfo& Child1 : Children)
					{
						// Provide no shape for clustered child implicits, these use their parent's filter data, which was already tested when generating shape pair in midphase.
						LegacyConstructConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shapes0, Simplicial0, Child1.Implicit, FShapeOrShapesArray(), Child1.BVHParticles, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, Child1.Transform * LocalTransform1, CullDistance, Dt, Context);
					}
				}
				else
				{
					for (const TUniquePtr<FImplicitObject>& Child1 : Union1->GetObjects())
					{
						const TPBDRigidParticleHandle<FReal, 3>* OriginalParticle = Union1->FindParticleForImplicitObject(Child1.Get());

						// Provide no shape for clustered child implicits, these use their parent's filter data, which was already tested when generating shape pair in midphase.
						LegacyConstructConstraints<T_TRAITS>(Particle0, Particle1, Implicit0, Shapes0, Simplicial0, Child1.Get(), FShapeOrShapesArray(), OriginalParticle ? OriginalParticle->CollisionParticles().Get() : Simplicial1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
					}
				}
				return;
			}

			// Should not have unions at this point, shouldn't be shapes array
			const FPerShapeData* Shape0 = nullptr;
			if (ensure(Shapes0.IsSingleShape()))
			{
				Shape0 = Shapes0.GetShape();
			}
			const FPerShapeData* Shape1 = nullptr;
			if (ensure(Shapes1.IsSingleShape()))
			{
				Shape1 = Shapes1.GetShape();
			}

			// Check shape pair filtering if enable
			if (Context.GetSettings().bFilteringEnabled && !DoCollide(Implicit0Type, Shape0, Implicit1Type, Shape1))
			{
				return;
			}

			// If we get here, we have a pair of concrete shapes (i.e., no wrappers or containers)
			// Create a constraint for the shape pair
			{
				// let's make sure the order of particle is correct
				// this needs to be done at this level because we have the concrete type available here ( and no union or other wrappers )
				// @todo(chaos) : with this order enforced, ConstructConstraintsImpl can be optimized by remove reverse test cases ( keeping sphere|convex but removing convex|sphere for example )  
				bool bShouldSwapParticles = false;
				if (bChaos_Collision_SortParticlesOnConstraintConstruct)
				{
					CalculateShapePairType(Implicit0, Simplicial0, Implicit1, Simplicial1, bShouldSwapParticles);
				}
				if (bShouldSwapParticles)
				{
					ConstructConstraintsImpl<T_TRAITS>(Particle1, Particle0, Implicit1, Shape1, Simplicial1, Implicit0, Shape0, Simplicial0, ParticleWorldTransform1, LocalTransform1, ParticleWorldTransform0, LocalTransform0, CullDistance, Dt, Context);	
				}
				else
				{
					ConstructConstraintsImpl<T_TRAITS>(Particle0, Particle1, Implicit0, Shape0, Simplicial0, Implicit1, Shape1, Simplicial1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, Dt, Context);
				}
			}
		}

		EContactShapesType CalculateShapePairType(const EImplicitObjectType Implicit0Type, const EImplicitObjectType Implicit1Type, const bool bIsConvex0, const bool bIsConvex1, const bool bIsBVH0, const bool bIsBVH1,  bool &bOutSwap)
		{
			bOutSwap = false;
			if (Implicit0Type == TBox<FReal, 3>::StaticType() && Implicit1Type == TBox<FReal, 3>::StaticType())
			{
				return EContactShapesType::BoxBox;
			}
			else if (Implicit0Type == TBox<FReal, 3>::StaticType() && Implicit1Type == FHeightField::StaticType())
			{
				return EContactShapesType::BoxHeightField;
			}
			else if (Implicit0Type == FHeightField::StaticType() && Implicit1Type == TBox<FReal, 3>::StaticType())
			{
				bOutSwap = true;
				return EContactShapesType::BoxHeightField;
			}
			else if (Implicit0Type == TBox<FReal, 3>::StaticType() && Implicit1Type == TPlane<FReal, 3>::StaticType())
			{
				return EContactShapesType::BoxPlane;
			}
			else if (Implicit0Type == TPlane<FReal, 3>::StaticType() && Implicit1Type == TBox<FReal, 3>::StaticType())
			{
				bOutSwap = true;
				return EContactShapesType::BoxPlane;
			}
			else if (Implicit0Type == TSphere<FReal, 3>::StaticType() && Implicit1Type == TSphere<FReal, 3>::StaticType())
			{
				return EContactShapesType::SphereSphere;
			}
			else if (Implicit0Type == TSphere<FReal, 3>::StaticType() && Implicit1Type == FHeightField::StaticType())
			{
				return EContactShapesType::SphereHeightField;
			}
			else if (Implicit0Type == FHeightField::StaticType() && Implicit1Type == TSphere<FReal, 3>::StaticType())
			{
				bOutSwap = true;
				return EContactShapesType::SphereHeightField;
			}
			else if (Implicit0Type == TSphere<FReal, 3>::StaticType() && Implicit1Type == TPlane<FReal, 3>::StaticType())
			{
				return EContactShapesType::SpherePlane;
			}
			else if (Implicit0Type == TPlane<FReal, 3>::StaticType() && Implicit1Type == TSphere<FReal, 3>::StaticType())
			{
				bOutSwap = true;
				return EContactShapesType::SpherePlane;
			}
			else if (Implicit0Type == TSphere<FReal, 3>::StaticType() && Implicit1Type == TBox<FReal, 3>::StaticType())
			{
				return EContactShapesType::SphereBox;
			}
			else if (Implicit0Type == TBox<FReal, 3>::StaticType() && Implicit1Type == TSphere<FReal, 3>::StaticType())
			{
				bOutSwap = true;
				return EContactShapesType::SphereBox;
			}
			else if (Implicit0Type == TSphere<FReal, 3>::StaticType() && Implicit1Type == FCapsule::StaticType())
			{
				return EContactShapesType::SphereCapsule;
			}
			else if (Implicit0Type == FCapsule::StaticType() && Implicit1Type == TSphere<FReal, 3>::StaticType())
			{
				bOutSwap = true;
				return EContactShapesType::SphereCapsule;
			}
			else if (Implicit0Type == TSphere<FReal, 3>::StaticType() && Implicit1Type == FImplicitConvex3::StaticType())
			{
				return EContactShapesType::SphereConvex;
			}
			else if (Implicit0Type == FImplicitConvex3::StaticType() && Implicit1Type == TSphere<FReal, 3>::StaticType())
			{
				bOutSwap = true;
				return EContactShapesType::SphereConvex;
			}
			else if (Implicit0Type == FCapsule::StaticType() && Implicit1Type == FCapsule::StaticType())
			{
				return EContactShapesType::CapsuleCapsule;
			}
			else if (Implicit0Type == FCapsule::StaticType() && Implicit1Type == TBox<FReal, 3>::StaticType())
			{
				return EContactShapesType::CapsuleBox;
			}
			else if (Implicit0Type == TBox<FReal, 3>::StaticType() && Implicit1Type == FCapsule::StaticType())
			{
				bOutSwap = true;
				return EContactShapesType::CapsuleBox;
			}
			else if (Implicit0Type == FCapsule::StaticType() && Implicit1Type == FImplicitConvex3::StaticType())
			{
				return EContactShapesType::CapsuleConvex;
			}
			else if (Implicit0Type == FImplicitConvex3::StaticType() && Implicit1Type == FCapsule::StaticType())
			{
				bOutSwap = true;
				return EContactShapesType::CapsuleConvex;
			}
			else if (Implicit0Type == FCapsule::StaticType() && Implicit1Type == FHeightField::StaticType())
			{
				return EContactShapesType::CapsuleHeightField;
			}
			else if (Implicit0Type == FHeightField::StaticType() && Implicit1Type == FCapsule::StaticType())
			{
				bOutSwap = true;
				return EContactShapesType::CapsuleHeightField;
			}
			else if (Implicit0Type == TBox<FReal, 3>::StaticType() && Implicit1Type == FTriangleMeshImplicitObject::StaticType())
			{
				return EContactShapesType::BoxTriMesh;
			}
			else if (Implicit0Type == FTriangleMeshImplicitObject::StaticType() && Implicit1Type == TBox<FReal, 3>::StaticType())
			{
				bOutSwap = true;
				return EContactShapesType::BoxTriMesh;
			}
			else if (Implicit0Type == TSphere<FReal, 3>::StaticType() && Implicit1Type == FTriangleMeshImplicitObject::StaticType())
			{
				return EContactShapesType::SphereTriMesh;
			}
			else if (Implicit0Type == FTriangleMeshImplicitObject::StaticType() && Implicit1Type == TSphere<FReal, 3>::StaticType())
			{
				bOutSwap = true;
				return EContactShapesType::SphereTriMesh;
			}
			else if (Implicit0Type == FCapsule::StaticType() && Implicit1Type == FTriangleMeshImplicitObject::StaticType())
			{
				return EContactShapesType::CapsuleTriMesh;
			}
			else if (Implicit0Type == FTriangleMeshImplicitObject::StaticType() && Implicit1Type == FCapsule::StaticType())
			{
				bOutSwap = true;
				return EContactShapesType::CapsuleTriMesh;
			}
			else if (bIsConvex0 && Implicit1Type == FHeightField::StaticType())
			{
				return EContactShapesType::ConvexHeightField;
			}
			else if (Implicit0Type == FHeightField::StaticType() && bIsConvex1)
			{
				bOutSwap = true;
				return EContactShapesType::ConvexHeightField;
			}
			else if (bIsConvex0 && Implicit1Type == FTriangleMeshImplicitObject::StaticType())
			{
				return EContactShapesType::ConvexTriMesh;
			}
			else if (Implicit0Type == FTriangleMeshImplicitObject::StaticType() && bIsConvex1)
			{
				bOutSwap = true;
				return EContactShapesType::ConvexTriMesh;
			}
			else if (bIsConvex0 && bIsConvex1)
			{
				return EContactShapesType::GenericConvexConvex;
			}
			else if ((Implicit0Type == FLevelSet::StaticType()) || (Implicit1Type == FLevelSet::StaticType()))
			{
				bOutSwap = !bIsBVH0 && bIsBVH1;
				return EContactShapesType::LevelSetLevelSet;
			}

			// @todo(chaos): support FImplicitObjectUnion
			// @todo(chaos): support FImplicitObjectUnionClustered
			return EContactShapesType::Unknown;
		}

		EContactShapesType CalculateShapePairType(const FImplicitObject* Implicit0, const FBVHParticles* BVHParticles0, const FImplicitObject* Implicit1, const FBVHParticles* BVHParticles1, bool& bOutSwap)
		{
			const EImplicitObjectType Implicit0Type = (Implicit0 != nullptr) ? GetInnerType(Implicit0->GetCollisionType()) : ImplicitObjectType::Unknown;
			const EImplicitObjectType Implicit1Type = (Implicit1 != nullptr) ? GetInnerType(Implicit1->GetCollisionType()) : ImplicitObjectType::Unknown;
			const bool bIsConvex0 = (Implicit0 != nullptr) && Implicit0->IsConvex() && (Implicit0Type != ImplicitObjectType::LevelSet);
			const bool bIsConvex1 = (Implicit1 != nullptr) && Implicit1->IsConvex() && (Implicit1Type != ImplicitObjectType::LevelSet);
			const bool bIsBVH0 = (BVHParticles0 != nullptr);
			const bool bIsBVH1 = (BVHParticles1 != nullptr);
			return CalculateShapePairType(Implicit0Type, Implicit1Type, bIsConvex0, bIsConvex1, bIsBVH0, bIsBVH1, bOutSwap);
		}

		void ConstructConstraints(TGeometryParticleHandle<FReal, 3>* Particle0, TGeometryParticleHandle<FReal, 3>* Particle1, const FImplicitObject* Implicit0, const FPerShapeData* Shape0, const FBVHParticles* Simplicial0, const FImplicitObject* Implicit1, const FPerShapeData* Shape1, const FBVHParticles* Simplicial1, const FRigidTransform3& ParticleWorldTransform0, const FRigidTransform3& LocalTransform0, const FRigidTransform3& ParticleWorldTransform1, const FRigidTransform3& LocalTransform1, const FReal CullDistance, const FReal dT, const FCollisionContext& Context)
		{
			INC_DWORD_STAT(STAT_ChaosCollisionCounter_NumParticlePairs);

			// @todo(chaos): remove defer update - it is not a valid option with the new solver
			bool bDeferUpdate = Context.GetSettings().bDeferNarrowPhase;
			if (bDeferUpdate)
			{
				using TTraits = TConstructCollisionTraits<false>;
				LegacyConstructConstraints<TTraits>(Particle0, Particle1, Implicit0, FShapeOrShapesArray(Shape0), Simplicial0, Implicit1, FShapeOrShapesArray(Shape1), Simplicial1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, dT, Context);
			}
			else
			{
				using TTraits = TConstructCollisionTraits<true>;
				LegacyConstructConstraints<TTraits>(Particle0, Particle1, Implicit0, FShapeOrShapesArray(Shape0), Simplicial0, Implicit1, FShapeOrShapesArray(Shape1), Simplicial1, ParticleWorldTransform0, LocalTransform0, ParticleWorldTransform1, LocalTransform1, CullDistance, dT, Context);
			}
		}

		template void UpdateLevelsetLevelsetConstraint<ECollisionUpdateType::Any>(const FRigidTransform3& WorldTransform0, const FRigidTransform3& WorldTransform1, const FReal Dt, FPBDCollisionConstraint& Constraint);
		template void UpdateLevelsetLevelsetConstraint<ECollisionUpdateType::Deepest>(const FRigidTransform3& WorldTransform0, const FRigidTransform3& WorldTransform1, const FReal Dt, FPBDCollisionConstraint& Constraint);

		template void UpdateConstraintFromGeometry<ECollisionUpdateType::Any>(FPBDCollisionConstraint& ConstraintBase, const FRigidTransform3& Transform0, const FRigidTransform3& Transform1, const FReal Dt);
		template void UpdateConstraintFromGeometry<ECollisionUpdateType::Deepest>(FPBDCollisionConstraint& ConstraintBase, const FRigidTransform3& Transform0, const FRigidTransform3& Transform1, const FReal Dt);

	} // Collisions


} // Chaos
