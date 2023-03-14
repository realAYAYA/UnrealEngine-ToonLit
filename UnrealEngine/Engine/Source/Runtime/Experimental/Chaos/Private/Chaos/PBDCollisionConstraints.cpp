// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/PBDCollisionConstraints.h"

#include "Chaos/ChaosPerfTest.h"
#include "Chaos/ContactModification.h"
#include "Chaos/PBDCollisionConstraintsContact.h"
#include "Chaos/CollisionResolution.h"
#include "Chaos/Collision/CollisionPruning.h"
#include "Chaos/Collision/SolverCollisionContainer.h"
#include "Chaos/Defines.h"
#include "Chaos/Evolution/SolverBodyContainer.h"
#include "Chaos/GeometryQueries.h"
#include "Chaos/Island/IslandManager.h"
#include "Chaos/SpatialAccelerationCollection.h"
#include "Chaos/PBDRigidsSOAs.h"
#include "Chaos/CastingUtilities.h"
#include "ChaosLog.h"
#include "ChaosStats.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Algo/Sort.h"
#include "Algo/StableSort.h"

// Private includes
#include "Collision/PBDCollisionSolver.h"

//PRAGMA_DISABLE_OPTIMIZATION

namespace Chaos
{
	int32 CollisionParticlesBVHDepth = 4;
	FAutoConsoleVariableRef CVarCollisionParticlesBVHDepth(TEXT("p.CollisionParticlesBVHDepth"), CollisionParticlesBVHDepth, TEXT("The maximum depth for collision particles bvh"));

	int32 ConstraintBPBVHDepth = 2;
	FAutoConsoleVariableRef CVarConstraintBPBVHDepth(TEXT("p.ConstraintBPBVHDepth"), ConstraintBPBVHDepth, TEXT("The maximum depth for constraint bvh"));

	int32 BPTreeOfGrids = 1;
	FAutoConsoleVariableRef CVarBPTreeOfGrids(TEXT("p.BPTreeOfGrids"), BPTreeOfGrids, TEXT("Whether to use a seperate tree of grids for bp"));

	FRealSingle CollisionFrictionOverride = -1.0f;
	FAutoConsoleVariableRef CVarCollisionFrictionOverride(TEXT("p.CollisionFriction"), CollisionFrictionOverride, TEXT("Collision friction for all contacts if >= 0"));

	FRealSingle CollisionRestitutionOverride = -1.0f;
	FAutoConsoleVariableRef CVarCollisionRestitutionOverride(TEXT("p.CollisionRestitution"), CollisionRestitutionOverride, TEXT("Collision restitution for all contacts if >= 0"));
	
	FRealSingle CollisionAngularFrictionOverride = -1.0f;
	FAutoConsoleVariableRef CVarCollisionAngularFrictionOverride(TEXT("p.CollisionAngularFriction"), CollisionAngularFrictionOverride, TEXT("Collision angular friction for all contacts if >= 0"));

	CHAOS_API int32 EnableCollisions = 1;
	FAutoConsoleVariableRef CVarEnableCollisions(TEXT("p.EnableCollisions"), EnableCollisions, TEXT("Enable/Disable collisions on the Chaos solver."));
	
	FRealSingle DefaultCollisionFriction = 0;
	FAutoConsoleVariableRef CVarDefaultCollisionFriction(TEXT("p.DefaultCollisionFriction"), DefaultCollisionFriction, TEXT("Collision friction default value if no materials are found."));

	FRealSingle DefaultCollisionRestitution = 0;
	FAutoConsoleVariableRef CVarDefaultCollisionRestitution(TEXT("p.DefaultCollisionRestitution"), DefaultCollisionRestitution, TEXT("Collision restitution default value if no materials are found."));

	FRealSingle CollisionRestitutionThresholdOverride = -1.0f;
	FAutoConsoleVariableRef CVarDefaultCollisionRestitutionThreshold(TEXT("p.CollisionRestitutionThreshold"), CollisionRestitutionThresholdOverride, TEXT("Collision restitution threshold override if >= 0 (units of acceleration)"));

	int32 CollisionCanAlwaysDisableContacts = 0;
	FAutoConsoleVariableRef CVarCollisionCanAlwaysDisableContacts(TEXT("p.CollisionCanAlwaysDisableContacts"), CollisionCanAlwaysDisableContacts, TEXT("Collision culling will always be able to permanently disable contacts"));

	int32 CollisionCanNeverDisableContacts = 0;
	FAutoConsoleVariableRef CVarCollisionCanNeverDisableContacts(TEXT("p.CollisionCanNeverDisableContacts"), CollisionCanNeverDisableContacts, TEXT("Collision culling will never be able to permanently disable contacts"));

	bool CollisionsAllowParticleTracking = true;
	FAutoConsoleVariableRef CVarCollisionsAllowParticleTracking(TEXT("p.Chaos.Collision.AllowParticleTracking"), CollisionsAllowParticleTracking, TEXT("Allow particles to track their collisions constraints when their DoBufferCollisions flag is enable [def:true]"));
	
	bool bCollisionsEnableSubSurfaceCollisionPruning = false;
	FAutoConsoleVariableRef CVarCollisionsEnableSubSurfaceCollisionPruning(TEXT("p.Chaos.Collision.EnableSubSurfaceCollisionPruning"), bCollisionsEnableSubSurfaceCollisionPruning, TEXT(""));

	bool DebugDrawProbeDetection = false;
	FAutoConsoleVariableRef CVarDebugDrawProbeDetection(TEXT("p.Chaos.Collision.DebugDrawProbeDetection"), DebugDrawProbeDetection, TEXT("Draw probe constraint detection."));
	
	extern bool bChaosSolverPersistentGraph;

#if CHAOS_DEBUG_DRAW
	namespace CVars
	{
		extern DebugDraw::FChaosDebugDrawSettings ChaosSolverDebugDebugDrawSettings;
	}
#endif
	
	DECLARE_CYCLE_STAT(TEXT("Collisions::Reset"), STAT_Collisions_Reset, STATGROUP_ChaosCollision);
	DECLARE_CYCLE_STAT(TEXT("Collisions::UpdatePointConstraints"), STAT_Collisions_UpdatePointConstraints, STATGROUP_ChaosCollision);
	DECLARE_CYCLE_STAT(TEXT("Collisions::BeginDetect"), STAT_Collisions_BeginDetect, STATGROUP_ChaosCollision);
	DECLARE_CYCLE_STAT(TEXT("Collisions::EndDetect"), STAT_Collisions_EndDetect, STATGROUP_ChaosCollision);
	DECLARE_CYCLE_STAT(TEXT("Collisions::DetectProbeCollisions"), STAT_Collisions_DetectProbeCollisions, STATGROUP_ChaosCollision);

	//
	// Collision Constraint Container
	//

	FPBDCollisionConstraints::FPBDCollisionConstraints(
		const FPBDRigidsSOAs& InParticles,
		TArrayCollectionArray<bool>& Collided,
		const TArrayCollectionArray<TSerializablePtr<FChaosPhysicsMaterial>>& InPhysicsMaterials,
		const TArrayCollectionArray<TUniquePtr<FChaosPhysicsMaterial>>& InPerParticlePhysicsMaterials,
		const THandleArray<FChaosPhysicsMaterial>* const InSimMaterials,
		const int32 NumCollisionsPerBlock,
		const FReal InRestitutionThreshold)
		: FPBDConstraintContainer(FConstraintContainerHandle::StaticType())
		, Particles(InParticles)
		, ConstraintAllocator(NumCollisionsPerBlock)
		, NumActivePointConstraints(0)
		, MCollided(Collided)
		, MPhysicsMaterials(InPhysicsMaterials)
		, MPerParticlePhysicsMaterials(InPerParticlePhysicsMaterials)
		, SimMaterials(InSimMaterials)
		, RestitutionThreshold(InRestitutionThreshold)	// @todo(chaos): expose as property
		, bEnableCollisions(true)
		, bEnableRestitution(true)
		, bHandlesEnabled(true)
		, bEnableEdgePruning(true)
		, bIsDeterministic(false)
		, bCanDisableContacts(true)
		, GravityDirection(FVec3(0,0,-1))
		, GravitySize(980)
		, SolverSettings()
	{
	}

	FPBDCollisionConstraints::~FPBDCollisionConstraints()
	{
	}

	void FPBDCollisionConstraints::DisableHandles()
	{
		check(NumConstraints() == 0);
		bHandlesEnabled = false;
	}

	FPBDCollisionConstraints::FHandles FPBDCollisionConstraints::GetConstraintHandles() const
	{
		return ConstraintAllocator.GetConstraints();
	}

	FPBDCollisionConstraints::FConstHandles FPBDCollisionConstraints::GetConstConstraintHandles() const
	{
		return ConstraintAllocator.GetConstConstraints();
	}

	const FChaosPhysicsMaterial* GetPhysicsMaterial(const TGeometryParticleHandle<FReal, 3>* Particle, const FImplicitObject* Geom, const TArrayCollectionArray<TSerializablePtr<FChaosPhysicsMaterial>>& PhysicsMaterials, const TArrayCollectionArray<TUniquePtr<FChaosPhysicsMaterial>>& PerParticlePhysicsMaterials, const THandleArray<FChaosPhysicsMaterial>* const SimMaterials)
	{
		// Use the per-particle material if it exists
		const FChaosPhysicsMaterial* UniquePhysicsMaterial = Particle->AuxilaryValue(PerParticlePhysicsMaterials).Get();
		if (UniquePhysicsMaterial != nullptr)
		{
			return UniquePhysicsMaterial;
		}
		const FChaosPhysicsMaterial* PhysicsMaterial = Particle->AuxilaryValue(PhysicsMaterials).Get();
		if (PhysicsMaterial != nullptr)
		{
			return PhysicsMaterial;
		}

		// If no particle material, see if the shape has one
		// @todo(chaos): handle materials for meshes etc
		for (const TUniquePtr<FPerShapeData>& ShapeData : Particle->ShapesArray())
		{
			const FImplicitObject* OuterShapeGeom = ShapeData->GetGeometry().Get();
			const FImplicitObject* InnerShapeGeom = Utilities::ImplicitChildHelper(OuterShapeGeom);
			if (Geom == OuterShapeGeom || Geom == InnerShapeGeom)
			{
				if (ShapeData->GetMaterials().Num() > 0)
				{
					if(SimMaterials)
					{
						return SimMaterials->Get(ShapeData->GetMaterials()[0].InnerHandle);
					}
					else
					{
						UE_LOG(LogChaos, Warning, TEXT("Attempted to resolve a material for a constraint but we do not have a sim material container."));
					}
				}
				else
				{
					// This shape doesn't have a material assigned
					return nullptr;
				}
			}
		}

		// The geometry used for this particle does not belong to the particle.
		// This can happen in the case of fracture.
		return nullptr;
	}

	void FPBDCollisionConstraints::UpdateConstraintMaterialProperties(FPBDCollisionConstraint& Constraint)
	{
		const FChaosPhysicsMaterial* PhysicsMaterial0 = GetPhysicsMaterial(Constraint.Particle[0], Constraint.Implicit[0], MPhysicsMaterials, MPerParticlePhysicsMaterials, SimMaterials);
		const FChaosPhysicsMaterial* PhysicsMaterial1 = GetPhysicsMaterial(Constraint.Particle[1], Constraint.Implicit[1], MPhysicsMaterials, MPerParticlePhysicsMaterials, SimMaterials);

		FReal MaterialRestitution = 0;
		FReal MaterialRestitutionThreshold = 0;
		FReal MaterialStaticFriction = 0;
		FReal MaterialDynamicFriction = 0;

		if (PhysicsMaterial0 && PhysicsMaterial1)
		{
			const FChaosPhysicsMaterial::ECombineMode RestitutionCombineMode = FChaosPhysicsMaterial::ChooseCombineMode(PhysicsMaterial0->RestitutionCombineMode,PhysicsMaterial1->RestitutionCombineMode);
			MaterialRestitution = FChaosPhysicsMaterial::CombineHelper(PhysicsMaterial0->Restitution, PhysicsMaterial1->Restitution, RestitutionCombineMode);

			const FChaosPhysicsMaterial::ECombineMode FrictionCombineMode = FChaosPhysicsMaterial::ChooseCombineMode(PhysicsMaterial0->FrictionCombineMode,PhysicsMaterial1->FrictionCombineMode);
			MaterialDynamicFriction = FChaosPhysicsMaterial::CombineHelper(PhysicsMaterial0->Friction,PhysicsMaterial1->Friction, FrictionCombineMode);
			const FReal StaticFriction0 = FMath::Max(PhysicsMaterial0->Friction, PhysicsMaterial0->StaticFriction);
			const FReal StaticFriction1 = FMath::Max(PhysicsMaterial1->Friction, PhysicsMaterial1->StaticFriction);
			MaterialStaticFriction = FChaosPhysicsMaterial::CombineHelper(StaticFriction0, StaticFriction1, FrictionCombineMode);
		}
		else if (PhysicsMaterial0)
		{
			const FReal StaticFriction0 = FMath::Max(PhysicsMaterial0->Friction, PhysicsMaterial0->StaticFriction);
			MaterialRestitution = PhysicsMaterial0->Restitution;
			MaterialDynamicFriction = PhysicsMaterial0->Friction;
			MaterialStaticFriction = StaticFriction0;
		}
		else if (PhysicsMaterial1)
		{
			const FReal StaticFriction1 = FMath::Max(PhysicsMaterial1->Friction, PhysicsMaterial1->StaticFriction);
			MaterialRestitution = PhysicsMaterial1->Restitution;
			MaterialDynamicFriction = PhysicsMaterial1->Friction;
			MaterialStaticFriction = StaticFriction1;
		}
		else
		{
			MaterialDynamicFriction = DefaultCollisionFriction;
			MaterialStaticFriction = DefaultCollisionFriction;
			MaterialRestitution = DefaultCollisionRestitution;
		}

		MaterialRestitutionThreshold = RestitutionThreshold;

		// Overrides for testing
		if (CollisionFrictionOverride >= 0)
		{
			MaterialDynamicFriction = CollisionFrictionOverride;
			MaterialStaticFriction = CollisionFrictionOverride;
		}
		if (CollisionRestitutionOverride >= 0)
		{
			MaterialRestitution = CollisionRestitutionOverride;
		}
		if (CollisionRestitutionThresholdOverride >= 0.0f)
		{
			MaterialRestitutionThreshold = CollisionRestitutionThresholdOverride;
		}
		if (CollisionAngularFrictionOverride >= 0)
		{
			MaterialStaticFriction = CollisionAngularFrictionOverride;
		}
		if (!bEnableRestitution)
		{
			MaterialRestitution = 0.0f;
		}
		
		Constraint.Material.MaterialRestitution = FRealSingle(MaterialRestitution);
		Constraint.Material.RestitutionThreshold = FRealSingle(MaterialRestitutionThreshold);
		Constraint.Material.MaterialStaticFriction = FRealSingle(MaterialStaticFriction);
		Constraint.Material.MaterialDynamicFriction = FRealSingle(MaterialDynamicFriction);

		Constraint.Material.ResetMaterialModifications();
	}

	void FPBDCollisionConstraints::BeginFrame()
	{
		ConstraintAllocator.BeginFrame();
	}

	void FPBDCollisionConstraints::Reset()
	{
		SCOPE_CYCLE_COUNTER(STAT_Collisions_Reset);

		ConstraintAllocator.Reset();
	}

	void FPBDCollisionConstraints::BeginDetectCollisions()
	{
		SCOPE_CYCLE_COUNTER(STAT_Collisions_BeginDetect);

		ConstraintAllocator.BeginDetectCollisions();
	}

	void FPBDCollisionConstraints::EndDetectCollisions()
	{
		SCOPE_CYCLE_COUNTER(STAT_Collisions_EndDetect);

		// Prune the unused contacts
		ConstraintAllocator.EndDetectCollisions();

		// Disable any edge collisions that are hidden by face collisions
		PruneEdgeCollisions();

		if (bIsDeterministic)
		{
			ConstraintAllocator.SortConstraintsHandles();
		}

		// Bind the constraints to this container and initialize other properties
		// @todo(chaos): this could be set on creation if the midphase knew about the container
		for (FPBDCollisionConstraint* Contact : GetConstraints())
		{
			if (Contact->GetContainer() == nullptr)
			{
				Contact->SetContainer(this);
				UpdateConstraintMaterialProperties(*Contact);
			}

			// Reset constraint modifications and accumulators
			Contact->Activate();
		}
	}

	void FPBDCollisionConstraints::DetectProbeCollisions(FReal Dt)
	{
		SCOPE_CYCLE_COUNTER(STAT_Collisions_DetectProbeCollisions);

		// Do a final narrow-phase update on probe constraints. This is done to avoid
		// false positive probe hit results, which may occur if the constraint was created
		// for a contact which no longer is occurring due to resolution of another constraint.
		for (FPBDCollisionConstraint* Contact : GetConstraints())
		{
			if (Contact->IsProbe())
			{
				const FGeometryParticleHandle* Particle0 = Contact->GetParticle0();
				const FGeometryParticleHandle* Particle1 = Contact->GetParticle1();
				const FRigidTransform3& ShapeWorldTransform0 = Contact->GetShapeRelativeTransform0() * FParticleUtilities::GetActorWorldTransform(FConstGenericParticleHandle(Particle0));
				const FRigidTransform3& ShapeWorldTransform1 = Contact->GetShapeRelativeTransform1() * FParticleUtilities::GetActorWorldTransform(FConstGenericParticleHandle(Particle1));
				Contact->SetCullDistance(0.f);
				Contact->SetShapeWorldTransforms(ShapeWorldTransform0, ShapeWorldTransform1);
				Collisions::UpdateConstraint(*Contact, ShapeWorldTransform0, ShapeWorldTransform1, Dt);

#if CHAOS_DEBUG_DRAW
				if (DebugDrawProbeDetection)
				{
					DebugDraw::FChaosDebugDrawSettings Settings = CVars::ChaosSolverDebugDebugDrawSettings;
					Settings.DrawDuration = 1.f;
					DebugDraw::DrawCollidingShapes(FRigidTransform3(), *Contact, 1.f, 1.f, &Settings);
				}
#endif
			}
		}
	}

	void FPBDCollisionConstraints::ApplyCollisionModifier(const TArray<ISimCallbackObject*>& CollisionModifiers, FReal Dt)
	{
		if (GetConstraints().Num() > 0)
		{
			TArrayView<FPBDCollisionConstraint* const> ConstraintHandles = GetConstraintHandles();
			FCollisionContactModifier Modifier(ConstraintHandles, Dt);

			for(ISimCallbackObject* ModifierCallback : CollisionModifiers)
			{
				ModifierCallback->ContactModification_Internal(Modifier);
			}

			Modifier.UpdateConstraintManifolds();
		}
	}

	void FPBDCollisionConstraints::DisconnectConstraints(const TSet<FGeometryParticleHandle*>& ParticleHandles)
	{
		RemoveConstraints(ParticleHandles);
	}

	void FPBDCollisionConstraints::RemoveConstraints(const TSet<FGeometryParticleHandle*>& ParticleHandles)
	{
		for (FGeometryParticleHandle* ParticleHandle : ParticleHandles)
		{
			ConstraintAllocator.RemoveParticle(ParticleHandle);
		}
	}

	void FPBDCollisionConstraints::AddConstraintsToGraph(FPBDIslandManager& IslandManager)
	{
		// Debugging/diagnosing: if we have collisions disabled, remove all collisions from the graph and don't add any more
		if (!GetCollisionsEnabled())
		{
			IslandManager.RemoveConstraints(GetContainerId());
			return;
		}

		// If we are running with a persistent graph, remove expired collisions	
		if (bChaosSolverPersistentGraph)
		{
			// Find all expired constraints in the graph
			TempCollisions.Reset();
			IslandManager.VisitConstraintsInAwakeIslands(GetContainerId(),
				[this](FConstraintHandle* ConstraintHandle)
				{
					FPBDCollisionConstraintHandle* CollisionHandle = ConstraintHandle->AsUnsafe<FPBDCollisionConstraintHandle>();
					if (!CollisionHandle->IsEnabled() || CollisionHandle->IsProbe() || ConstraintAllocator.IsConstraintExpired(CollisionHandle->GetContact()))
					{
						TempCollisions.Add(CollisionHandle);
					}
				});

			// Remove expired constraints
			for (FPBDCollisionConstraintHandle* CollisionHandle : TempCollisions)
			{
				IslandManager.RemoveConstraint(GetContainerId(), CollisionHandle);
			}
		}

		// Collect all the new constraints that need to be added to the graph
		TempCollisions.Reset();
		for (FPBDCollisionConstraintHandle* ConstraintHandle : ConstraintAllocator.GetConstraints())
		{
			FPBDCollisionConstraint& Constraint = ConstraintHandle->GetContact();
			if (!Constraint.IsInConstraintGraph() && Constraint.IsEnabled() && !Constraint.IsProbe())
			{
				TempCollisions.Add(ConstraintHandle);
			}
		}

		// Sort new constraints into a predictable order. This isn't strictly required, but without it we
		// can get fairly different behaviour from run to run because the collisions detection order is 
		// effectively random on multicore machines
		TempCollisions.Sort(
			[](const FPBDCollisionConstraintHandle& L, const FPBDCollisionConstraintHandle& R)
			{
				const uint64 LKey = L.GetContact().GetParticlePairKey().GetKey();
				const uint64 RKey = R.GetContact().GetParticlePairKey().GetKey();
				return LKey < RKey;
			});

		// Add the new constraints to the graph
		for (FPBDCollisionConstraintHandle* ConstraintHandle : TempCollisions)
		{
			IslandManager.AddConstraint(GetContainerId(), ConstraintHandle, ConstraintHandle->GetConstrainedParticles());
		}

		TempCollisions.Reset();
	}

	const FPBDCollisionConstraint& FPBDCollisionConstraints::GetConstraint(int32 Index) const
	{
		check(Index < NumConstraints());
		
		return *GetConstraints()[Index];
	}

	FPBDCollisionConstraint& FPBDCollisionConstraints::GetConstraint(int32 Index)
	{
		check(Index < NumConstraints());

		return *GetConstraints()[Index];
	}

	void FPBDCollisionConstraints::PruneEdgeCollisions()
	{
		if (bEnableEdgePruning)
		{
			for (auto& ParticleHandle : Particles.GetNonDisabledDynamicView())
			{
				if ((ParticleHandle.CollisionConstraintFlags() & (uint32)ECollisionConstraintFlags::CCF_SmoothEdgeCollisions) != 0)
				{
					FParticleEdgeCollisionPruner EdgePruner(ParticleHandle.Handle());
					EdgePruner.Prune();

					if (bCollisionsEnableSubSurfaceCollisionPruning)
					{
						const FVec3 UpVector = ParticleHandle.R().GetAxisZ();
						FParticleSubSurfaceCollisionPruner SubSurfacePruner(ParticleHandle.Handle());
						SubSurfacePruner.Prune(UpVector);
					}
				}
			}
		}
	}

}
