// Copyright Epic Games, Inc. All Rights Reserved.


#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "PhysicsProxy/FieldSystemProxyHelper.h"

#include "PhysicsSolver.h"
#include "ChaosStats.h"
#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/BVHParticles.h"
#include "Chaos/Transform.h"
#include "Chaos/ParallelFor.h"
#include "Chaos/GeometryParticles.h"
#include "Chaos/Particles.h"
#include "Chaos/TriangleMesh.h"
#include "Chaos/MassProperties.h"
#include "ChaosSolversModule.h"
#include "Chaos/PBDCollisionConstraintsUtil.h"
#include "Chaos/PerParticleGravity.h"
#include "Chaos/ImplicitObject.h"
#include "Chaos/ImplicitObjectScaled.h"
#include "Chaos/Convex.h"
#include "Chaos/Serializable.h"
#include "Chaos/ErrorReporter.h"
#include "Chaos/PBDRigidClustering.h"
#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "GeometryCollection/GeometryCollectionUtility.h"
#include "GeometryCollection/GeometryCollectionSizeSpecificUtility.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "Modules/ModuleManager.h"
#include "Chaos/PullPhysicsDataImp.h"
#include "Chaos/PBDRigidsEvolution.h"
#include "GeometryCollection/Facades/CollectionAnchoringFacade.h"

#ifndef TODO_REIMPLEMENT_INIT_COMMANDS
#define TODO_REIMPLEMENT_INIT_COMMANDS 0
#endif

#ifndef TODO_REIMPLEMENT_FRACTURE
#define TODO_REIMPLEMENT_FRACTURE 0
#endif

#ifndef TODO_REIMPLEMENT_RIGID_CACHING
#define TODO_REIMPLEMENT_RIGID_CACHING 0
#endif

float CollisionParticlesPerObjectFractionDefault = 1.0f;
FAutoConsoleVariableRef CVarCollisionParticlesPerObjectFractionDefault(
	TEXT("p.CollisionParticlesPerObjectFractionDefault"), 
	CollisionParticlesPerObjectFractionDefault, 
	TEXT("Fraction of verts"));

bool DisableGeometryCollectionGravity = false;
FAutoConsoleVariableRef CVarGeometryCollectionDisableGravity(
	TEXT("p.GeometryCollectionDisableGravity"),
	DisableGeometryCollectionGravity,
	TEXT("Disable gravity for geometry collections"));

bool GeometryCollectionCollideAll = false;
FAutoConsoleVariableRef CVarGeometryCollectionCollideAll(
	TEXT("p.GeometryCollectionCollideAll"),
	GeometryCollectionCollideAll,
	TEXT("Bypass the collision matrix and make geometry collections collide against everything"));


bool bGeometryCollectionEnabledNestedChildTransformUpdates = true;
FAutoConsoleVariableRef CVarEnabledNestedChildTransformUpdates(
	TEXT("p.GeometryCollection.EnabledNestedChildTransformUpdates"),
	bGeometryCollectionEnabledNestedChildTransformUpdates,
	TEXT("Enable updates for driven, disabled, child bodies. Used for line trace results against geometry collections.[def: true]"));

bool bGeometryCollectionAlwaysGenerateGTCollisionForClusters = true;
FAutoConsoleVariableRef CVarGeometryCollectionAlwaysGenerateGTCollisionForClusters(
	TEXT("p.GeometryCollection.AlwaysGenerateGTCollisionForClusters"),
	bGeometryCollectionAlwaysGenerateGTCollisionForClusters,
	TEXT("When enabled, always generate a game thread side collision for clusters.[def: true]"));

bool bGeometryCollectionAlwaysGenerateConnectionGraph = false;
FAutoConsoleVariableRef CVarGeometryCollectionAlwaysGenerateConnectionGraph(
	TEXT("p.GeometryCollection.AlwaysGenerateConnectionGraph"),
	bGeometryCollectionAlwaysGenerateConnectionGraph,
	TEXT("When enabled, always  generate the cluster's connection graph instead of using the rest collection stored one - Note: this should only be used for troubleshooting.[def: false]"));

DEFINE_LOG_CATEGORY_STATIC(UGCC_LOG, Error, All);

static const FSharedSimulationSizeSpecificData& GetSizeSpecificData(const TArray<FSharedSimulationSizeSpecificData>& SizeSpecificData, const FGeometryCollection& RestCollection, const int32 TransformIndex, const FBox& BoundingBox);

//==============================================================================
// FGeometryCollectionResults
//==============================================================================

FGeometryCollectionResults::FGeometryCollectionResults()
	: IsObjectDynamic(false)
	, IsObjectLoading(false)
{}

void FGeometryCollectionResults::Reset()
{
	SolverDt = 0.0f;
	States.SetNum(0);
	GlobalTransforms.SetNum(0);
	ParticleXs.SetNum(0);
	ParticleRs.SetNum(0);
	ParticleVs.SetNum(0);
	ParticleWs.SetNum(0);
	IsObjectDynamic = false;
	IsObjectLoading = false;
}

//==============================================================================
// FGeometryCollectionPhysicsProxy helper functions
//==============================================================================


TUniquePtr<Chaos::FTriangleMesh> CreateTriangleMesh(
	const int32 FaceStart,
	const int32 FaceCount, 
	const TManagedArray<bool>& Visible, 
	const TManagedArray<FIntVector>& Indices,
	bool bRotateWinding)
{
	TArray<Chaos::TVector<int32, 3>> Faces;
	Faces.Reserve(FaceCount);
	
	const int32 FaceEnd = FaceStart + FaceCount;
	for (int Idx = FaceStart; Idx < FaceEnd; ++Idx)
	{
		// Note: This function used to cull small triangles.  As one of the purposes 
		// of the tri mesh this function creates is for level set rasterization, we 
		// don't want to do that.  Keep the mesh intact, which hopefully is water tight.
		if (Visible[Idx])
		{
			const FIntVector& Tri = Indices[Idx];

			if(bRotateWinding)
			{
				Faces.Add(Chaos::TVector<int32, 3>(Tri.Z, Tri.Y, Tri.X));
			}
			else
			{
				Faces.Add(Chaos::TVector<int32, 3>(Tri.X, Tri.Y, Tri.Z));
			}
		}
	}
	return MakeUnique<Chaos::FTriangleMesh>(MoveTemp(Faces)); // Culls geometrically degenerate faces
}

TArray<int32> ComputeTransformToGeometryMap(const FGeometryCollection& Collection)
{
	const int32 NumTransforms = Collection.NumElements(FGeometryCollection::TransformGroup);
	const int32 NumGeometries = Collection.NumElements(FGeometryCollection::GeometryGroup);
	const TManagedArray<int32>& TransformIndex = Collection.TransformIndex;

	TArray<int32> TransformToGeometryMap;
	TransformToGeometryMap.AddUninitialized(NumTransforms);
	for(int32 GeometryIndex = 0; GeometryIndex < NumGeometries; ++GeometryIndex)
	{
		const int32 TransformGroupIndex = TransformIndex[GeometryIndex];
		TransformToGeometryMap[TransformGroupIndex] = GeometryIndex;
	}

	return TransformToGeometryMap;
}


DECLARE_CYCLE_STAT(TEXT("FGeometryCollectionPhysicsProxy::PopulateSimulatedParticle"), STAT_PopulateSimulatedParticle, STATGROUP_Chaos);
void PopulateSimulatedParticle(
	Chaos::TPBDRigidParticleHandle<Chaos::FReal,3>* Handle,
	const FSharedSimulationParameters& SharedParams,
	const FCollisionStructureManager::FSimplicial* Simplicial,
	FGeometryDynamicCollection::FSharedImplicit Implicit,
	const FCollisionFilterData SimFilterIn,
	const FCollisionFilterData QueryFilterIn,
	Chaos::FReal MassIn,
	Chaos::TVec3<Chaos::FRealSingle> InertiaTensorVec,
	const FTransform& WorldTransform, 
	const uint8 DynamicState, 
	const int16 CollisionGroup,
	float CollisionParticlesPerObjectFraction)
{
	SCOPE_CYCLE_COUNTER(STAT_PopulateSimulatedParticle);
	Handle->SetDisabledLowLevel(false);
	Handle->SetX(WorldTransform.GetTranslation());
	Handle->SetV(Chaos::FVec3(0.f));
	Handle->SetR(WorldTransform.GetRotation().GetNormalized());
	Handle->SetW(Chaos::FVec3(0.f));
	Handle->SetP(Handle->X());
	Handle->SetQ(Handle->R());
	Handle->SetCenterOfMass(FVector3f::ZeroVector);
	Handle->SetRotationOfMass(FQuat::Identity);

	//
	// Setup Mass
	//
	{
		Handle->SetObjectStateLowLevel(Chaos::EObjectStateType::Uninitialized);

		if (!CHAOS_ENSURE_MSG(FMath::IsWithinInclusive<Chaos::FReal>(MassIn, SharedParams.MinimumMassClamp, SharedParams.MaximumMassClamp),
			TEXT("Clamped mass[%3.5f] to range [%3.5f,%3.5f]"), MassIn, SharedParams.MinimumMassClamp, SharedParams.MaximumMassClamp))
		{
			MassIn = FMath::Clamp<Chaos::FReal>(MassIn, SharedParams.MinimumMassClamp, SharedParams.MaximumMassClamp);
		}

		if (!CHAOS_ENSURE_MSG(!FMath::IsNaN(InertiaTensorVec[0]) && !FMath::IsNaN(InertiaTensorVec[1]) && !FMath::IsNaN(InertiaTensorVec[2]),
			TEXT("Nan Tensor, reset to unit tesor")))
		{
			InertiaTensorVec = FVector3f(1);
		}
		else if (!CHAOS_ENSURE_MSG(FMath::IsWithinInclusive(InertiaTensorVec[0], SharedParams.MinimumInertiaTensorDiagonalClamp, SharedParams.MaximumInertiaTensorDiagonalClamp)
			&& FMath::IsWithinInclusive(InertiaTensorVec[1], SharedParams.MinimumInertiaTensorDiagonalClamp, SharedParams.MaximumInertiaTensorDiagonalClamp)
			&& FMath::IsWithinInclusive(InertiaTensorVec[2], SharedParams.MinimumInertiaTensorDiagonalClamp, SharedParams.MaximumInertiaTensorDiagonalClamp),
			TEXT("Clamped Inertia tensor[%3.5f,%3.5f,%3.5f]. Clamped each element to [%3.5f, %3.5f,]"), InertiaTensorVec[0], InertiaTensorVec[1], InertiaTensorVec[2],
			SharedParams.MinimumInertiaTensorDiagonalClamp, SharedParams.MaximumInertiaTensorDiagonalClamp))
		{
			InertiaTensorVec[0] = FMath::Clamp(InertiaTensorVec[0], SharedParams.MinimumInertiaTensorDiagonalClamp, SharedParams.MaximumInertiaTensorDiagonalClamp);
			InertiaTensorVec[1] = FMath::Clamp(InertiaTensorVec[1], SharedParams.MinimumInertiaTensorDiagonalClamp, SharedParams.MaximumInertiaTensorDiagonalClamp);
			InertiaTensorVec[2] = FMath::Clamp(InertiaTensorVec[2], SharedParams.MinimumInertiaTensorDiagonalClamp, SharedParams.MaximumInertiaTensorDiagonalClamp);
		}

		Handle->SetM(MassIn);
		Handle->SetI(InertiaTensorVec);
		const Chaos::FReal MassInv = (MassIn > 0.0f) ? 1.0f / MassIn : 0.0f;
		const Chaos::FVec3 InertiaInv = (MassIn > 0.0f) ? Chaos::FVec3(InertiaTensorVec).Reciprocal() : Chaos::FVec3::ZeroVector;
		Handle->SetInvM(MassInv);
		Handle->SetInvI(InertiaInv);
		Handle->SetObjectStateLowLevel(Chaos::EObjectStateType::Dynamic); // this step sets InvM, InvInertia, P, Q
	}

	Handle->SetCollisionGroup(CollisionGroup);

	// @todo(GCCollisionShapes) : add support for multiple shapes, currently just one. 
	FCollectionCollisionTypeData SingleSupportedCollisionTypeData = FCollectionCollisionTypeData();
	if (SharedParams.SizeSpecificData.Num() && SharedParams.SizeSpecificData[0].CollisionShapesData.Num())
	{
		SingleSupportedCollisionTypeData = SharedParams.SizeSpecificData[0].CollisionShapesData[0];
	}
	const FVector Scale = WorldTransform.GetScale3D();
	if (Implicit)	//todo(ocohen): this is only needed for cases where clusters have no proxy. Kind of gross though, should refactor
	{
		auto DeepCopyImplicit = [&Scale](FGeometryDynamicCollection::FSharedImplicit ImplicitToCopy) -> TUniquePtr<Chaos::FImplicitObject>
		{
			if (Scale.Equals(FVector::OneVector))
			{
				return ImplicitToCopy->DeepCopy();
			}
			else
			{
				return ImplicitToCopy->DeepCopyWithScale(Scale);
			}
		};

		TSharedPtr<Chaos::FImplicitObject, ESPMode::ThreadSafe> SharedImplicitTS(DeepCopyImplicit(Implicit).Release());
		FCollisionStructureManager::UpdateImplicitFlags(SharedImplicitTS.Get(), SingleSupportedCollisionTypeData.CollisionType);
		Handle->SetSharedGeometry(SharedImplicitTS);
		Handle->SetHasBounds(true);
		Handle->SetLocalBounds(SharedImplicitTS->BoundingBox());
		const Chaos::FRigidTransform3 Xf(Handle->X(), Handle->R());
		Handle->UpdateWorldSpaceState(Xf, Chaos::FVec3(0));
	}

	if (Simplicial && SingleSupportedCollisionTypeData.CollisionType == ECollisionTypeEnum::Chaos_Surface_Volumetric)
	{
		Handle->CollisionParticlesInitIfNeeded();

		TUniquePtr<Chaos::FBVHParticles>& CollisionParticles = Handle->CollisionParticles();
		CollisionParticles.Reset(Simplicial->NewCopy()); // @chaos(optimize) : maybe just move this memory instead. 

		const int32 NumCollisionParticles = CollisionParticles->Size();
		const int32 AdjustedNumCollisionParticles = FMath::TruncToInt(CollisionParticlesPerObjectFraction * (float)NumCollisionParticles);
		int32 CollisionParticlesSize = FMath::Max<int32>(0, FMath::Min<int32>(AdjustedNumCollisionParticles, NumCollisionParticles));
		CollisionParticles->Resize(CollisionParticlesSize); // Truncates! ( particles are already sorted by importance )

		Chaos::FAABB3 ImplicitShapeDomain = Chaos::FAABB3::FullAABB();
		if (Implicit && Implicit->GetType() == Chaos::ImplicitObjectType::LevelSet && Implicit->HasBoundingBox())
		{
			ImplicitShapeDomain = Implicit->BoundingBox();
			ImplicitShapeDomain.Scale(Scale);
		}

		// we need to account for scale and check if the particle is still within its domain
		for (int32 ParticleIndex = 0; ParticleIndex < (int32)CollisionParticles->Size(); ++ParticleIndex)
		{
			CollisionParticles->X(ParticleIndex) *= Scale;
			
			// Make sure the collision particles are at least in the domain 
			// of the implicit shape.
			ensure(ImplicitShapeDomain.Contains(CollisionParticles->X(ParticleIndex)));
		}

		// @todo(remove): IF there is no simplicial we should not be forcing one. 
		if (!CollisionParticles->Size())
		{
			CollisionParticles->AddParticles(1);
			CollisionParticles->X(0) = Chaos::FVec3(0);
		}
		CollisionParticles->UpdateAccelerationStructures();
	}

	if (GeometryCollectionCollideAll) // cvar
	{
		// Override collision filters and make this body collide with everything.
		int32 CurrShape = 0;
		FCollisionFilterData FilterData;
		FilterData.Word1 = 0xFFFF; // this body channel
		FilterData.Word3 = 0xFFFF; // collision candidate channels
		for (const TUniquePtr<Chaos::FPerShapeData>& Shape : Handle->ShapesArray())
		{
			Shape->SetSimEnabled(true);
			Shape->SetCollisionTraceType(Chaos::EChaosCollisionTraceFlag::Chaos_CTF_UseDefault);
			//Shape->CollisionTraceType = Chaos::EChaosCollisionTraceFlag::Chaos_CTF_UseSimpleAndComplex;
			Shape->SetSimData(FilterData);
			Shape->SetQueryData(FCollisionFilterData());
		}
	}
	else
	{
		for (const TUniquePtr<Chaos::FPerShapeData>& Shape : Handle->ShapesArray())
		{
			Shape->SetSimData(SimFilterIn);
			Shape->SetQueryData(QueryFilterIn);
		}
	}

	//
	//  Manage Object State
	//

	// Only sleep if we're not replaying a simulation
	// #BG TODO If this becomes an issue, recorded tracks should track awake state as well as transforms
	if (DynamicState == (uint8)EObjectStateTypeEnum::Chaos_Object_Sleeping)
	{
		Handle->SetObjectStateLowLevel(Chaos::EObjectStateType::Sleeping);
	}
	else if (DynamicState == (uint8)EObjectStateTypeEnum::Chaos_Object_Kinematic)
	{
		Handle->SetObjectStateLowLevel(Chaos::EObjectStateType::Kinematic);
	}
	else if (DynamicState == (uint8)EObjectStateTypeEnum::Chaos_Object_Static)
	{
		Handle->SetObjectStateLowLevel(Chaos::EObjectStateType::Static);
	}
	else
	{
		Handle->SetObjectStateLowLevel(Chaos::EObjectStateType::Dynamic);
	}
}

//==============================================================================
// FGeometryCollectionPhysicsProxy
//==============================================================================


FGeometryCollectionPhysicsProxy::FGeometryCollectionPhysicsProxy(
	UObject* InOwner,
	FGeometryDynamicCollection& GameThreadCollectionIn,
	const FSimulationParameters& SimulationParameters,
	FCollisionFilterData InSimFilter,
	FCollisionFilterData InQueryFilter,
	FGuid InCollectorGuid,
	const Chaos::EMultiBufferMode BufferMode)
	: Base(InOwner)
	, Parameters(SimulationParameters)
	, NumParticles(INDEX_NONE)
	, BaseParticleIndex(INDEX_NONE)
	, IsObjectDynamic(false)
	, IsObjectLoading(true)
	, IsObjectDeleting(false)
	, SimFilter(InSimFilter)
	, QueryFilter(InQueryFilter)
#if TODO_REIMPLEMENT_RIGID_CACHING
	, ProxySimDuration(0.0f)
	, LastSyncCountGT(MAX_uint32)
#endif
	, CollisionParticlesPerObjectFraction(CollisionParticlesPerObjectFractionDefault)

	, GameThreadCollection(GameThreadCollectionIn)
	, bIsPhysicsThreadWorldTransformDirty(false)
	, bIsCollisionFilterDataDirty(false)
	, CollectorGuid(InCollectorGuid)
{
	// We rely on a guarded buffer.
	check(BufferMode == Chaos::EMultiBufferMode::TripleGuarded);
}


FGeometryCollectionPhysicsProxy::~FGeometryCollectionPhysicsProxy()
{}

float ReportHighParticleFraction = -1.f;
FAutoConsoleVariableRef CVarReportHighParticleFraction(TEXT("p.gc.ReportHighParticleFraction"), ReportHighParticleFraction, TEXT("Report any objects with particle fraction above this threshold"));

void FGeometryCollectionPhysicsProxy::Initialize(Chaos::FPBDRigidsEvolutionBase *Evolution)
{
	check(IsInGameThread());
	//
	// Game thread initilization. 
	//
	//  1) Create a input buffer to store all game thread side data. 
	//  2) Populate the buffer with the necessary data.
	//  3) Deep copy the data to the other buffers. 
	//
	FGeometryDynamicCollection& DynamicCollection = GameThreadCollection;

	InitializeDynamicCollection(DynamicCollection, *Parameters.RestCollection, Parameters);

	// Attach the external particles to the gamethread collection
	if (DynamicCollection.HasAttribute(FGeometryCollection::ParticlesAttribute, FTransformCollection::TransformGroup))
		DynamicCollection.RemoveAttribute(FGeometryCollection::ParticlesAttribute, FTransformCollection::TransformGroup);
	DynamicCollection.AddExternalAttribute<TUniquePtr<Chaos::FGeometryParticle>>(FGeometryCollection::ParticlesAttribute, FTransformCollection::TransformGroup, GTParticles);


	NumParticles = DynamicCollection.NumElements(FGeometryCollection::TransformGroup);
	BaseParticleIndex = 0; // Are we always zero indexed now?
	SolverClusterID.Init(nullptr, NumParticles);
	SolverClusterHandles.Init(nullptr, NumParticles);
	SolverParticleHandles.Init(nullptr, NumParticles);

	// compatibility requirement to make sure we at least initialize GameThreadPerFrameData properly
	GameThreadPerFrameData.SetWorldTransform(Parameters.WorldTransform);

	//
	// Collision vertices down sampling validation.  
	//
	CollisionParticlesPerObjectFraction = Parameters.CollisionSampleFraction * CollisionParticlesPerObjectFractionDefault;
	if (ReportHighParticleFraction > 0)
	{
		for (const FSharedSimulationSizeSpecificData& Data : Parameters.Shared.SizeSpecificData)
		{
			if (ensure(Data.CollisionShapesData.Num()))
			{
				if (Data.CollisionShapesData[0].CollisionParticleData.CollisionParticlesFraction >= ReportHighParticleFraction)
				{
					ensureMsgf(false, TEXT("Collection with small particle fraction"));
					UE_LOG(LogChaos, Warning, TEXT("Collection with small particle fraction(%f):%s"), Data.CollisionShapesData[0].CollisionParticleData.CollisionParticlesFraction, *Parameters.Name);
				}
			}
		}
	}

	// Initialise GT/External particles
	const int32 NumTransforms = DynamicCollection.Transform.Num();

	// Attach the external particles to the gamethread collection
	if (GameThreadCollection.HasAttribute(FGeometryCollection::ParticlesAttribute, FTransformCollection::TransformGroup))
	{ 
		GameThreadCollection.RemoveAttribute(FGeometryCollection::ParticlesAttribute, FTransformCollection::TransformGroup);
	}
		
	GameThreadCollection.AddExternalAttribute<TUniquePtr<Chaos::FGeometryParticle>>(FGeometryCollection::ParticlesAttribute, FTransformCollection::TransformGroup, GTParticles);
	

	const FVector Scale = Parameters.WorldTransform.GetScale3D();

	TArray<int32> ChildrenToCheckForParentFix;
	if(ensure(NumTransforms == GameThreadCollection.Implicits.Num() && NumTransforms == GTParticles.Num())) // Implicits are in the transform group so this invariant should always hold
	{
		for(int32 Index = 0; Index < NumTransforms; ++Index)
		{
			GTParticles[Index] = Chaos::FGeometryParticle::CreateParticle();
			Chaos::FGeometryParticle* P = GTParticles[Index].Get();
			GTParticlesToTransformGroupIndex.Add(P, Index);

			GTParticles[Index]->SetUniqueIdx(Evolution->GenerateUniqueIdx());

			const FTransform& T = Parameters.WorldTransform * GameThreadCollection.Transform[Index];
			P->SetX(T.GetTranslation(), false);
			P->SetR(T.GetRotation(), false);
			P->SetUserData(Parameters.UserData);
			P->SetProxy(this);

			FGeometryDynamicCollection::FSharedImplicit ImplicitGeometry = GameThreadCollection.Implicits[Index];
			if (ImplicitGeometry && !Scale.Equals(FVector::OneVector))
			{
				TUniquePtr<Chaos::FImplicitObject> ScaledImplicit = ImplicitGeometry->CopyWithScale(Scale);
				ImplicitGeometry = FGeometryDynamicCollection::FSharedImplicit(ScaledImplicit.Release());
			}
			P->SetGeometry(ImplicitGeometry);

			// this step is necessary for Phase 2 where we need to walk back the hierarchy from children to parent 
			if (bGeometryCollectionAlwaysGenerateGTCollisionForClusters && GameThreadCollection.Children[Index].Num() == 0)
			{
				ChildrenToCheckForParentFix.Add(Index);
			}
			
			// IMPORTANT: we need to set the right spatial index because GT particle is static and PT particle is rigid
			// this is causing a mismatch when using the separate acceleration structures optimization which can cause crashes when destroying the particle while async tracing 
			// todo(chaos) we should eventually refactor this code to use rigid particles on the GT side for geometry collection  
			P->SetSpatialIdx(Chaos::FSpatialAccelerationIdx{ 0,1 });

			if (Chaos::AccelerationStructureSplitStaticAndDynamic == 1)
			{
				P->SetSpatialIdx(Chaos::FSpatialAccelerationIdx{ 0,1 });
			}
			else
			{
				P->SetSpatialIdx(Chaos::FSpatialAccelerationIdx{ 0,0 });
			}
		}

		if (bGeometryCollectionAlwaysGenerateGTCollisionForClusters)
		{
			// second phase: fixing parent geometries
			// @todo(chaos) this could certainly be done ahead at generation time rather than runtime
			TSet<int32> ParentToPotentiallyFix;
			while (ChildrenToCheckForParentFix.Num())
			{
				// step 1 : find parents
				for(const int32 ChildIndex: ChildrenToCheckForParentFix)
				{
					const int32 ParentIndex = GameThreadCollection.Parent[ChildIndex];
					if (ParentIndex != INDEX_NONE)
					{
						ParentToPotentiallyFix.Add(ParentIndex);
					}
				}

				// step 2: fix the parent if necessary
				for (const int32 ParentToFixIndex: ParentToPotentiallyFix)
				{
					if (GameThreadCollection.Implicits[ParentToFixIndex] == nullptr)
					{
						const Chaos::FRigidTransform3 ParentShapeTransform =  GameThreadCollection.MassToLocal[ParentToFixIndex];
				
						// let's make sure all our children have an implicit defined, other wise, postpone to next iteration 
						bool bAllChildrenHaveCollision = true;
						for (const int32& ChildIndex : GameThreadCollection.Children[ParentToFixIndex])
						{
							// defer if any of the children is a cluster with no collision yet generated 
							if (GameThreadCollection.Implicits[ChildIndex] == nullptr && GameThreadCollection.Children[ChildIndex].Num() > 0)
							{
								bAllChildrenHaveCollision = false;
								break;
							}
						}

						if (bAllChildrenHaveCollision)
						{
							// Make a union of the children geometry
							TArray<TUniquePtr<Chaos::FImplicitObject>> ChildImplicits;
							for (const int32& ChildIndex : GameThreadCollection.Children[ParentToFixIndex])
							{
								using FImplicitObjectTransformed = Chaos::TImplicitObjectTransformed<Chaos::FReal, 3>;

								Chaos::FGeometryParticle* ChildParticle = GTParticles[ChildIndex].Get();
								const FGeometryDynamicCollection::FSharedImplicit& ChildImplicit = GameThreadCollection.Implicits[ChildIndex];
								if (ChildImplicit)
								{
									const Chaos::FRigidTransform3 ChildShapeTransform = GameThreadCollection.MassToLocal[ChildIndex] * GameThreadCollection.Transform[ChildIndex];
									const Chaos::FRigidTransform3 RelativeShapeTransform = ChildShapeTransform.GetRelativeTransform(ParentShapeTransform);

									// assumption that we only have can only have one level of union for any child
									if (ChildImplicit->GetType() == Chaos::ImplicitObjectType::Union)
									{
										if (Chaos::FImplicitObjectUnion* Union = ChildImplicit->GetObject<Chaos::FImplicitObjectUnion>())
										{
											for (const TUniquePtr<Chaos::FImplicitObject>& ImplicitObject : Union->GetObjects())
											{
												TUniquePtr<Chaos::FImplicitObject> CopiedChildImplicit = ImplicitObject->DeepCopy();
												FImplicitObjectTransformed* TransformedChildImplicit = new FImplicitObjectTransformed(MoveTemp(CopiedChildImplicit), RelativeShapeTransform);
												ChildImplicits.Add(TUniquePtr<FImplicitObjectTransformed>(TransformedChildImplicit));
											}
										}
									}
									else
									{
										TUniquePtr<Chaos::FImplicitObject> CopiedChildImplicit = GameThreadCollection.Implicits[ChildIndex]->DeepCopy();
										if (CopiedChildImplicit->GetType() == Chaos::ImplicitObjectType::Transformed)
										{
											if (FImplicitObjectTransformed* Transformed = CopiedChildImplicit->GetObject<FImplicitObjectTransformed>())
											{
												Transformed->SetTransform(Transformed->GetTransform() * RelativeShapeTransform);
												ChildImplicits.Add(TUniquePtr<FImplicitObjectTransformed>(Transformed));
											}
										}
										else
										{
											FImplicitObjectTransformed* TransformedChildImplicit = new FImplicitObjectTransformed(MoveTemp(CopiedChildImplicit), RelativeShapeTransform);
											ChildImplicits.Add(TUniquePtr<FImplicitObjectTransformed>(TransformedChildImplicit));
										}
									}
								}
							}
							if (ChildImplicits.Num() > 0)
							{
								Chaos::FImplicitObject* UnionImplicit = new Chaos::FImplicitObjectUnion(MoveTemp(ChildImplicits));
								GameThreadCollection.Implicits[ParentToFixIndex] = FGeometryDynamicCollection::FSharedImplicit(UnionImplicit);
							}
							GTParticles[ParentToFixIndex]->SetGeometry(GameThreadCollection.Implicits[ParentToFixIndex]);
						}
					}
				}

				// step 3 : make the parent the new child to go up the hierarchy and continue the fixing
				ChildrenToCheckForParentFix = ParentToPotentiallyFix.Array(); 
				ParentToPotentiallyFix.Reset();
			}
		}
		
		// Phase 3 : finalization of shapes
		for(int32 Index = 0; Index < NumTransforms; ++Index)
		{
			Chaos::FGeometryParticle* P = GTParticles[Index].Get();
			const Chaos::FShapesArray& Shapes = P->ShapesArray();
			const int32 NumShapes = Shapes.Num();
			for(int32 ShapeIndex = 0; ShapeIndex < NumShapes; ++ShapeIndex)
			{
				Chaos::FPerShapeData* Shape = Shapes[ShapeIndex].Get();
				Shape->SetSimData(SimFilter);
				Shape->SetQueryData(QueryFilter);
				Shape->SetProxy(this);
				Shape->SetMaterial(Parameters.PhysicalMaterialHandle);
			}
		}
	}

	// Skip simplicials, as they're owned by unique pointers.
	TMap<FName, TSet<FName>> SkipList;
	TSet<FName>& TransformGroupSkipList = SkipList.Emplace(FTransformCollection::TransformGroup);
	TransformGroupSkipList.Add(DynamicCollection.SimplicialsAttribute);

	PhysicsThreadCollection.CopyMatchingAttributesFrom(DynamicCollection, &SkipList);

	// make sure we copy the anchored information over to the physics thread collection
	const Chaos::Facades::FCollectionAnchoringFacade DynamicCollectionAnchoringFacade(DynamicCollection);
	Chaos::Facades::FCollectionAnchoringFacade PhysicsThreadCollectionAnchoringFacade(PhysicsThreadCollection);
	PhysicsThreadCollectionAnchoringFacade.CopyAnchoredAttribute(DynamicCollectionAnchoringFacade);
	
	// Copy simplicials.
	// TODO: Ryan - Should we just transfer ownership of the SimplicialsAttribute from the DynamicCollection to
	// the PhysicsThreadCollection?
	{
		if (DynamicCollection.HasAttribute(DynamicCollection.SimplicialsAttribute, FTransformCollection::TransformGroup))
		{
			const auto& SourceSimplicials = DynamicCollection.GetAttribute<TUniquePtr<FSimplicial>>(
				DynamicCollection.SimplicialsAttribute, FTransformCollection::TransformGroup);
			for (int32 Index = PhysicsThreadCollection.NumElements(FTransformCollection::TransformGroup) - 1; 0 <= Index; Index--)
			{
				PhysicsThreadCollection.Simplicials[Index].Reset(
					SourceSimplicials[Index] ? SourceSimplicials[Index]->NewCopy() : nullptr);
			}
		}
		else
		{
			for (int32 Index = PhysicsThreadCollection.NumElements(FTransformCollection::TransformGroup) - 1; 0 <= Index; Index--)
			{
				PhysicsThreadCollection.Simplicials[Index].Reset();
			}
		}
	}

	// Add levels to physics thread collection, rename to make it clear these are initial levels from rest collection and not updated during sim.
	PhysicsThreadCollection.CopyAttribute(*Parameters.RestCollection, /*SrcName=*/"Level", /*DestName=*/"InitialLevel", FTransformCollection::TransformGroup);
	GameThreadCollection.CopyAttribute(*Parameters.RestCollection, /*SrcName=*/"Level", /*DestName=*/"InitialLevel", FTransformCollection::TransformGroup);


}


void FGeometryCollectionPhysicsProxy::InitializeDynamicCollection(FGeometryDynamicCollection& DynamicCollection, const FGeometryCollection& RestCollection, const FSimulationParameters& Params)
{
	// @todo(GCCollisionShapes) : add support for multiple shapes, currently just one. 

	// 
	// This function will use the rest collection to populate the dynamic collection. 
	//

	TMap<FName, TSet<FName>> SkipList;
	TSet<FName>& KeepFromDynamicCollection = SkipList.Emplace(FTransformCollection::TransformGroup);
	KeepFromDynamicCollection.Add(FTransformCollection::TransformAttribute);
	KeepFromDynamicCollection.Add(FTransformCollection::ParentAttribute);
	KeepFromDynamicCollection.Add(FTransformCollection::ChildrenAttribute);
	KeepFromDynamicCollection.Add(FGeometryCollection::SimulationTypeAttribute);
	KeepFromDynamicCollection.Add(DynamicCollection.SimplicialsAttribute);
	KeepFromDynamicCollection.Add(DynamicCollection.ActiveAttribute);
	KeepFromDynamicCollection.Add(DynamicCollection.CollisionGroupAttribute);
	DynamicCollection.CopyMatchingAttributesFrom(RestCollection, &SkipList);


	//
	// User defined initial velocities need to be populated. 
	//
	{
		if (Params.InitialVelocityType == EInitialVelocityTypeEnum::Chaos_Initial_Velocity_User_Defined)
		{
			DynamicCollection.InitialLinearVelocity.Fill(FVector3f(Params.InitialLinearVelocity));
			DynamicCollection.InitialAngularVelocity.Fill(FVector3f(Params.InitialAngularVelocity));
		}
	}

	// process simplicials
	{
		// CVar defined in BodyInstance but pertinent here as we will need to copy simplicials in the case that this is set.
		// Original CVar is read-only so taking a static ptr here is fine as the value cannot be changed
		static IConsoleVariable* AnalyticDisableCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("p.IgnoreAnalyticCollisionsOverride"));
		static const bool bAnalyticsDisabled = (AnalyticDisableCVar && AnalyticDisableCVar->GetBool());

		if (RestCollection.HasAttribute(DynamicCollection.SimplicialsAttribute, FTransformCollection::TransformGroup)
			&& Params.Shared.SizeSpecificData[0].CollisionShapesData.Num()
			&& (Params.Shared.SizeSpecificData[0].CollisionShapesData[0].CollisionType == ECollisionTypeEnum::Chaos_Surface_Volumetric || bAnalyticsDisabled))
		{
			const auto& RestSimplicials = RestCollection.GetAttribute<TUniquePtr<FSimplicial>>(
				DynamicCollection.SimplicialsAttribute, FTransformCollection::TransformGroup);
			for (int32 Index = DynamicCollection.NumElements(FTransformCollection::TransformGroup) - 1; 0 <= Index; Index--)
			{
				DynamicCollection.Simplicials[Index].Reset(
					RestSimplicials[Index] ? RestSimplicials[Index]->NewCopy() : nullptr);
			}
		}
		else
		{
			for (int32 Index = DynamicCollection.NumElements(FTransformCollection::TransformGroup) - 1; 0 <= Index; Index--)
			{
				DynamicCollection.Simplicials[Index].Reset();
			}
		}
	}

	// Process Activity
	{
		const int32 NumTransforms = DynamicCollection.SimulatableParticles.Num();
		if (!RestCollection.HasAttribute(FGeometryCollection::SimulatableParticlesAttribute, FTransformCollection::TransformGroup))
		{
			// If no simulation data is available then default to the simulation of just the rigid geometry.
			for (int32 TransformIdx = 0; TransformIdx < NumTransforms; TransformIdx++)
			{
				if (DynamicCollection.Children[TransformIdx].Num())
				{
					DynamicCollection.SimulatableParticles[TransformIdx] = false;
				}
				else
				{
					DynamicCollection.SimulatableParticles[TransformIdx] = DynamicCollection.Active[TransformIdx];
				}
			}
		}
	}
}

int32 ReportTooManyChildrenNum = -1;
FAutoConsoleVariableRef CVarReportTooManyChildrenNum(TEXT("p.ReportTooManyChildrenNum"), ReportTooManyChildrenNum, TEXT("Issue warning if more than this many children exist in a single cluster"));

void FGeometryCollectionPhysicsProxy::CreateNonClusteredParticles(Chaos::FPBDRigidsSolver* RigidsSolver, const FGeometryCollection& RestCollection, const FGeometryDynamicCollection& DynamicCollection)
{
	const TManagedArray<bool>& SimulatableParticles = DynamicCollection.SimulatableParticles;
	
	//const int NumRigids = 0; // ryan - Since we're doing SOA, we start at zero?
	int NumRigids = 0;
	BaseParticleIndex = NumRigids;

	// Gather unique indices from GT to pass into PT handle creation
	TArray<Chaos::FUniqueIdx> UniqueIndices;
	UniqueIndices.Reserve(SimulatableParticles.Num());

	// Count geometry collection leaf node particles to add
	int NumSimulatedParticles = 0;
	for (int32 Idx = 0; Idx < SimulatableParticles.Num(); ++Idx)
	{
		NumSimulatedParticles += SimulatableParticles[Idx];
		if (SimulatableParticles[Idx] && !RestCollection.IsClustered(Idx) && RestCollection.IsGeometry(Idx))
		{
			NumRigids++;
			UniqueIndices.Add(GTParticles[Idx]->UniqueIdx());
		}
	}

	// Add entries into simulation array
	RigidsSolver->GetEvolution()->ReserveParticles(NumSimulatedParticles);
	TArray<Chaos::TPBDGeometryCollectionParticleHandle<Chaos::FReal, 3>*> Handles = RigidsSolver->GetEvolution()->CreateGeometryCollectionParticles(NumRigids, UniqueIndices.GetData());

	int32 NextIdx = 0;
	for (int32 Idx = 0; Idx < SimulatableParticles.Num(); ++Idx)
	{
		SolverParticleHandles[Idx] = nullptr;
		if (SimulatableParticles[Idx] && !RestCollection.IsClustered(Idx))
		{
			// todo: Unblocked read access of game thread data on the physics thread.

			Chaos::TPBDGeometryCollectionParticleHandle<Chaos::FReal, 3>* Handle = Handles[NextIdx++];

			Handle->SetPhysicsProxy(this);

			SolverParticleHandles[Idx] = Handle;
			HandleToTransformGroupIndex.Add(Handle, Idx);

			// We're on the physics thread here but we've already set up the GT particles and we're just linking here
			Handle->GTGeometryParticle() = GTParticles[Idx].Get();

			check(SolverParticleHandles[Idx]->GetParticleType() == Handle->GetParticleType());
			RigidsSolver->GetEvolution()->RegisterParticle(Handle);
		}
	}

	// anchors
	
}

Chaos::FPBDRigidClusteredParticleHandle* FGeometryCollectionPhysicsProxy::FindClusteredParticleHandleByItemIndex_Internal(FGeometryCollectionItemIndex ItemIndex) const
{
	Chaos::FPBDRigidClusteredParticleHandle* ResultHandle = nullptr;;
	if (ItemIndex.IsInternalCluster())
	{
		const int32 InternalClusterUniqueIdx = ItemIndex.GetInternalClusterIndex();
		if (Chaos::FPBDRigidClusteredParticleHandle* const* InternalClusterHandle = UniqueIdxToInternalClusterHandle.Find(InternalClusterUniqueIdx))
		{
			ResultHandle = *InternalClusterHandle;
		}
	}
	else
	{
		const int32 TransformIndex = ItemIndex.GetTransformIndex();
		if (SolverParticleHandles.IsValidIndex(TransformIndex))
		{
			ResultHandle = SolverParticleHandles[TransformIndex];
		}
	}
	return ResultHandle;
}

void FGeometryCollectionPhysicsProxy::SetSleepingState(const Chaos::FPBDRigidsSolver& RigidsSolver)
{
	for (FClusterHandle* Handle: SolverParticleHandles)
	{
		if (Handle && !Handle->Disabled())
		{
			// Sleeping Geometry Collections:
			//   A sleeping geometry collection is dynamic internally, and then the top level
			//   active clusters are set to sleeping. Sleeping is not propagated up from the 
			//   leaf nodes like kinematic or dynamic clusters.
			RigidsSolver.GetEvolution()->SetParticleObjectState(Handle, Chaos::EObjectStateType::Sleeping);
		}
	}
}

void FGeometryCollectionPhysicsProxy::DirtyAllParticles(const Chaos::FPBDRigidsSolver& RigidsSolver)
{
	for (FClusterHandle* Handle: SolverParticleHandles)
	{
		if (Handle)
		{
			RigidsSolver.GetEvolution()->DirtyParticle(*Handle);
		}
	}
}

void FGeometryCollectionPhysicsProxy::InitializeBodiesPT(Chaos::FPBDRigidsSolver* RigidsSolver, typename Chaos::FPBDRigidsSolver::FParticlesType& Particles)
{
	const FGeometryCollection* RestCollection = Parameters.RestCollection;
	const FGeometryDynamicCollection& DynamicCollection = PhysicsThreadCollection;

	if (Parameters.Simulating && RestCollection)
	{
		const TManagedArray<int32>& TransformIndex = RestCollection->TransformIndex;
		const TManagedArray<int32>& BoneMap = RestCollection->BoneMap;
		const TManagedArray<int32>& SimulationType = RestCollection->SimulationType;
		const TManagedArray<FVector3f>& Vertex = RestCollection->Vertex;
		const TManagedArray<float>& Mass = RestCollection->GetAttribute<float>("Mass", FTransformCollection::TransformGroup);
		const TManagedArray<FVector3f>& InertiaTensor = RestCollection->GetAttribute<FVector3f>("InertiaTensor", FTransformCollection::TransformGroup);

		const int32 NumTransforms = DynamicCollection.NumElements(FTransformCollection::TransformGroup);
		const TManagedArray<int32>& DynamicState = DynamicCollection.DynamicState;
		const TManagedArray<int32>& CollisionGroup = DynamicCollection.CollisionGroup;
		const TManagedArray<bool>& SimulatableParticles = DynamicCollection.SimulatableParticles;
		const TManagedArray<FTransform>& MassToLocal = DynamicCollection.MassToLocal;
		const TManagedArray<FVector3f>& InitialAngularVelocity = DynamicCollection.InitialAngularVelocity;
		const TManagedArray<FVector3f>& InitialLinearVelocity = DynamicCollection.InitialLinearVelocity;
		const TManagedArray<FGeometryDynamicCollection::FSharedImplicit>& Implicits = DynamicCollection.Implicits;
		const TManagedArray<TUniquePtr<FCollisionStructureManager::FSimplicial>>& Simplicials = DynamicCollection.Simplicials;
		const TManagedArray<TSet<int32>>& Children = DynamicCollection.Children;
		const TManagedArray<int32>& Parent = DynamicCollection.Parent;

		TArray<FTransform> Transform;
		GeometryCollectionAlgo::GlobalMatrices(DynamicCollection.Transform, Parent, Transform);

		CreateNonClusteredParticles(RigidsSolver, *RestCollection, DynamicCollection);

		const float StrainDefault = Parameters.DamageThreshold.Num() ? Parameters.DamageThreshold[0] : 0;
		// Add the rigid bodies

		const FVector WorldScale = Parameters.WorldTransform.GetScale3D();
		const FVector::FReal MassScale = WorldScale.X * WorldScale.Y * WorldScale.Z;

		const Chaos::Facades::FCollectionAnchoringFacade AnchoringFacade(PhysicsThreadCollection);
		
		// Iterating over the geometry group is a fast way of skipping everything that's
		// not a leaf node, as each geometry has a transform index, which is a shortcut
		// for the case when there's a 1-to-1 mapping between transforms and geometries.
		// At the point that we start supporting instancing, this assumption will no longer
		// hold, and those reverse mappints will be INDEX_NONE.
		ParallelFor(NumTransforms, [&](int32 TransformGroupIndex)
		{
			if (FClusterHandle* Handle = SolverParticleHandles[TransformGroupIndex])
			{
				// Mass space -> Composed parent space -> world
				const FTransform WorldTransform = 
					MassToLocal[TransformGroupIndex] * Transform[TransformGroupIndex] * Parameters.WorldTransform;

				const Chaos::FVec3f ScaledInertia = Chaos::Utilities::ScaleInertia<float>((Chaos::FVec3f)InertiaTensor[TransformGroupIndex], (Chaos::FVec3f)(WorldScale), true);
				
				PopulateSimulatedParticle(
					Handle,
					Parameters.Shared,
					Simplicials[TransformGroupIndex].Get(),
					Implicits[TransformGroupIndex],
					SimFilter,
					QueryFilter,
					Mass[TransformGroupIndex] * MassScale,
					ScaledInertia,
					WorldTransform,
					static_cast<uint8>(DynamicState[TransformGroupIndex]),
					static_cast<int16>(CollisionGroup[TransformGroupIndex]),
					CollisionParticlesPerObjectFraction);

				// initialize anchoring information if available 
				Handle->SetIsAnchored(AnchoringFacade.IsAnchored(TransformGroupIndex));
				
				if (Parameters.EnableClustering)
				{
					Handle->SetClusterGroupIndex(Parameters.ClusterGroupIndex);

					float DamageThreshold = StrainDefault; 
					if (!Parameters.bUsePerClusterOnlyDamageThreshold)
					{
						DamageThreshold = ComputeDamageThreshold(DynamicCollection, TransformGroupIndex);
					}
					Handle->SetStrain(DamageThreshold);
				}

				// #BGTODO - non-updating parameters - remove lin/ang drag arrays and always query material if this stays a material parameter
				Chaos::FChaosPhysicsMaterial* SolverMaterial = RigidsSolver->GetSimMaterials().Get(Parameters.PhysicalMaterialHandle.InnerHandle);
				if(SolverMaterial)
				{
					Handle->SetLinearEtherDrag(SolverMaterial->LinearEtherDrag);
					Handle->SetAngularEtherDrag(SolverMaterial->AngularEtherDrag);
				}

				const Chaos::FShapesArray& Shapes = Handle->ShapesArray();
				for(const TUniquePtr<Chaos::FPerShapeData>& Shape : Shapes)
				{
					Shape->SetMaterial(Parameters.PhysicalMaterialHandle);
				}
			}
		},true);

		// Enable the particle (register with graph, add to broadphase, etc)
		// NOTE: Cannot be called in parallel (do not move this to the above loop)
		// @todo(chaos): can this be done at the end of the function once we know 
		// whether we're asleep etc.? (but see calls to UpdateGeometryCollectionViews)
		for (FClusterHandle* ClusterHandle : SolverParticleHandles)
		{
			if ((ClusterHandle != nullptr) && !ClusterHandle->Disabled())
			{
				RigidsSolver->GetEvolution()->EnableParticle(ClusterHandle);
			}
		}

		// After population, the states of each particle could have changed
		// @todo(Chaos) Do we really need to maintain the Views while running this function? We do it twice, but there's also a Dirty flag built-in
		Particles.UpdateGeometryCollectionViews();

		for (FFieldSystemCommand& Cmd : Parameters.InitializationCommands)
		{
			if(Cmd.MetaData.Contains(FFieldSystemMetaData::EMetaType::ECommandData_ProcessingResolution))
			{
				Cmd.MetaData.Remove(FFieldSystemMetaData::EMetaType::ECommandData_ProcessingResolution);
			}

			FFieldSystemMetaDataProcessingResolution* ResolutionData = new FFieldSystemMetaDataProcessingResolution(EFieldResolutionType::Field_Resolution_Maximum);

			Cmd.MetaData.Add( FFieldSystemMetaData::EMetaType::ECommandData_ProcessingResolution, TUniquePtr<FFieldSystemMetaDataProcessingResolution>(ResolutionData));
			Commands.Add(Cmd);
		}
		Parameters.InitializationCommands.Empty();
		FieldParameterUpdateCallback(RigidsSolver, false);

		if (Parameters.InitialVelocityType == EInitialVelocityTypeEnum::Chaos_Initial_Velocity_User_Defined)
		{
			// A previous implementation of this went wide on this loop.  The general 
			// rule of thumb for parallelization is that each thread needs at least
			// 1000 operations in order to overcome the expense of threading.  I don't
			// think that's generally going to be the case here...
			for (int32 TransformGroupIndex = 0; TransformGroupIndex < NumTransforms; ++TransformGroupIndex)
			{
				if (Chaos::TPBDRigidParticleHandle<Chaos::FReal, 3>* Handle = SolverParticleHandles[TransformGroupIndex])
				{
					if (DynamicState[TransformGroupIndex] == (int32)EObjectStateTypeEnum::Chaos_Object_Dynamic)
					{
						Handle->SetV(InitialLinearVelocity[TransformGroupIndex]);
						Handle->SetW(InitialAngularVelocity[TransformGroupIndex]);
					}
				}
			}
		}

		// #BG Temporary - don't cluster when playing back. Needs to be changed when kinematics are per-proxy to support
		// kinematic to dynamic transition for clusters.
		if (Parameters.EnableClustering)// && Parameters.CacheType != EGeometryCollectionCacheType::Play)
		{
			// "RecursiveOrder" means bottom up - children come before their parents.
			const TArray<int32> RecursiveOrder = GeometryCollectionAlgo::ComputeRecursiveOrder(*RestCollection);

			// Propagate simulated particle flags up the hierarchy from children 
			// to their parents, grandparents, etc...
			TArray<bool> SubTreeContainsSimulatableParticle;
			SubTreeContainsSimulatableParticle.SetNumZeroed(RecursiveOrder.Num());
			for (const int32 TransformGroupIndex : RecursiveOrder)
			{
				if (SimulatableParticles[TransformGroupIndex] && !RestCollection->IsClustered(TransformGroupIndex))

				{
					// Rigid node
					SubTreeContainsSimulatableParticle[TransformGroupIndex] =
						SolverParticleHandles[TransformGroupIndex] != nullptr;
				}
				else
				{
					// Cluster parent
					const TSet<int32>& ChildIndices = Children[TransformGroupIndex];
					for (const int32 ChildIndex : ChildIndices)
					{
						if(SubTreeContainsSimulatableParticle[ChildIndex])
						{
							SubTreeContainsSimulatableParticle[TransformGroupIndex] = true;
							break;
						}
					}
				}
			}

			TArray<Chaos::TPBDRigidClusteredParticleHandle<Chaos::FReal, 3>*> ClusterHandles;
			// Ryan - It'd be better to batch allocate cluster particles ahead of time,
			// but if ClusterHandles is empty, then new particles will be allocated
			// on the fly by TPBDRigidClustering::CreateClusterParticle(), which 
			// needs to work before this does...
			//ClusterHandles = GetSolver()->GetEvolution()->CreateClusteredParticles(NumClusters);

			int32 ClusterHandlesIndex = 0;
			TArray<Chaos::TPBDRigidParticleHandle<Chaos::FReal, 3>*> RigidChildren;
			TArray<int32> RigidChildrenTransformGroupIndex;
			for (const int32 TransformGroupIndex : RecursiveOrder)
			{
				// Don't construct particles for branches of the hierarchy that  
				// don't contain any simulated particles.
				if (!SubTreeContainsSimulatableParticle[TransformGroupIndex])
				{
					continue;
				}

				RigidChildren.Reset(Children.Num());
				RigidChildrenTransformGroupIndex.Reset(Children.Num());
				for (const int32 ChildIndex : Children[TransformGroupIndex])
				{
					if (Chaos::TPBDRigidClusteredParticleHandle<Chaos::FReal, 3>* Handle = SolverParticleHandles[ChildIndex])
					{
						RigidChildren.Add(Handle);
						RigidChildrenTransformGroupIndex.Add(ChildIndex);
					}
				}

				if (RigidChildren.Num())
				{
					if (ReportTooManyChildrenNum >= 0 && RigidChildren.Num() > ReportTooManyChildrenNum)
					{
						UE_LOG(LogChaos, Warning, TEXT("Too many children (%d) in a single cluster:%s"), 
							RigidChildren.Num(), *Parameters.Name);
					}

					Chaos::FClusterCreationParameters CreationParameters;
					CreationParameters.ClusterParticleHandle = ClusterHandles.Num() ? ClusterHandles[ClusterHandlesIndex++] : nullptr;
					CreationParameters.Scale = Parameters.WorldTransform.GetScale3D();

					// Hook the handle up with the GT particle
					Chaos::FGeometryParticle* GTParticle = GTParticles[TransformGroupIndex].Get();

					Chaos::FUniqueIdx ExistingIndex = GTParticle->UniqueIdx();
					Chaos::FPBDRigidClusteredParticleHandle* Handle = BuildClusters_Internal(TransformGroupIndex, RigidChildren, RigidChildrenTransformGroupIndex, CreationParameters, &ExistingIndex);
					Handle->GTGeometryParticle() = GTParticle;

					int32 RigidChildrenIdx = 0;
					for(const int32 ChildTransformIndex : RigidChildrenTransformGroupIndex)
					{
						SolverClusterID[ChildTransformIndex] = RigidChildren[RigidChildrenIdx++]->CastToClustered()->ClusterIds().Id;;
					}
					SolverClusterID[TransformGroupIndex] = Handle->ClusterIds().Id;					

					SolverClusterHandles[TransformGroupIndex] = Handle;
					SolverParticleHandles[TransformGroupIndex] = Handle;
					HandleToTransformGroupIndex.Add(Handle, TransformGroupIndex);
					Handle->SetPhysicsProxy(this);

					// Dirty for SQ
					RigidsSolver->GetEvolution()->DirtyParticle(*Handle);

					// If we're not simulating we would normally not write any results back to the game thread.
					// This will force a single write in this case because we've updated the transform on the cluster
					// and it should be updated on the game thread also
					// #TODO Consider building this information at edit-time / offline
					if(!Parameters.Simulating)
					{
						RigidsSolver->GetEvolution()->GetParticles().MarkTransientDirtyParticle(Handle);
					}
				}
			}

			// We've likely changed the state of leaf nodes, which are geometry
			// collection particles.  Update which particle views they belong in,
			// as well as views of clustered particles.
			Particles.UpdateGeometryCollectionViews(true); 

			const TManagedArray<TSet<int32>>* Connections = RestCollection->FindAttribute<TSet<int32>>("Connections", FGeometryCollection::TransformGroup);
			const bool bGenerateConnectionGraph = (!Connections) || (Connections->Num() != RestCollection->Transform.Num()) || bGeometryCollectionAlwaysGenerateConnectionGraph;
			if (bGenerateConnectionGraph)
			{
				// Set cluster connectivity.  TPBDRigidClustering::CreateClusterParticle() 
				// will optionally do this, but we switch that functionality off in BuildClusters_Internal().
				for (int32 TransformGroupIndex = 0; TransformGroupIndex < NumTransforms; ++TransformGroupIndex)
				{
					if (RestCollection->IsClustered(TransformGroupIndex))
					{
						if (FClusterHandle* ClusteredParticle = SolverParticleHandles[TransformGroupIndex])
						{
							Chaos::FClusterCreationParameters ClusterParams;
							// #todo: should other parameters be set here?  Previously, there was no parameters being sent, and it is unclear
							// where some of these parameters are defined (ie: CollisionThicknessPercent)
							ClusterParams.ConnectionMethod = Parameters.ClusterConnectionMethod;
							ClusterParams.ConnectionGraphBoundsFilteringMargin = Parameters.ConnectionGraphBoundsFilteringMargin;
						
							RigidsSolver->GetEvolution()->GetRigidClustering().GenerateConnectionGraph(ClusteredParticle, ClusterParams);
						}
					}
				}
			}
			else
			{
				if (Connections)
				{
					for (int32 TransformGroupIndex = 0; TransformGroupIndex < NumTransforms; ++TransformGroupIndex)
					{
						if (FClusterHandle* ClusteredParticle = SolverParticleHandles[TransformGroupIndex]) 
						{
							const TSet<int32>& Siblings = (*Connections)[TransformGroupIndex];
							for (const int32 SiblingTransformIndex: Siblings)
							{
								if (FClusterHandle* OtherClusteredParticle = SolverParticleHandles[SiblingTransformIndex])
								{
									// we add both connection for integrity so let use the index order as a way to filter out the symmetrical part 
									if (SiblingTransformIndex > TransformGroupIndex)
									{
										Chaos::TConnectivityEdge<Chaos::FReal> Edge;
										Edge.Strain = (ClusteredParticle->Strains() + OtherClusteredParticle->Strains()) * 0.5f;
									
										Edge.Sibling = OtherClusteredParticle;
										ClusteredParticle->ConnectivityEdges().Add(Edge);
									
										Edge.Sibling = ClusteredParticle;
										OtherClusteredParticle->ConnectivityEdges().Add(Edge);
									}
								}
							}
						}
					}
				}
				// load connection graph from RestCollection
			}
		} // end if EnableClustering
 

#if TODO_REIMPLEMENT_RIGID_CACHING
		// If we're recording and want to start immediately caching then we should cache the rest state
		if (Parameters.IsCacheRecording() && Parameters.CacheBeginTime == 0.0f)
		{
			if (UpdateRecordedStateCallback)
			{
				UpdateRecordedStateCallback(0.0f, RigidBodyID, Particles, RigidSolver->GetCollisionConstraints());
			}
		}
#endif // TODO_REIMPLEMENT_RIGID_CACHING

		const bool bStartSleeping =
				(Parameters.ObjectType == EObjectStateTypeEnum::Chaos_Object_Sleeping
			 || (Parameters.ObjectType == EObjectStateTypeEnum::Chaos_Object_Dynamic && !Parameters.StartAwake));
		if (bStartSleeping)
		{
			SetSleepingState(*RigidsSolver);
		}

		// apply various features on the handles 
		const bool bEnableGravity = Parameters.EnableGravity && !DisableGeometryCollectionGravity;
		for (Chaos::FPBDRigidParticleHandle* Handle: SolverParticleHandles)
		{
			if (Handle)
			{
				Handle->SetGravityEnabled(bEnableGravity);
				Handle->SetCCDEnabled(Parameters.UseCCD);
				Handle->SetInertiaConditioningEnabled(Parameters.UseInertiaConditioning);
				Handle->SetLinearEtherDrag(Parameters.LinearDamping);
				Handle->SetAngularEtherDrag(Parameters.AngularDamping);
			}
		}
		
		// call DirtyParticle to make sure the acceleration structure is up to date with all the changes happening here
		DirtyAllParticles(*RigidsSolver);

	} // end if simulating...

}

int32 ReportNoLevelsetCluster = 0;
FAutoConsoleVariableRef CVarReportNoLevelsetCluster(TEXT("p.gc.ReportNoLevelsetCluster"), ReportNoLevelsetCluster, TEXT("Report any cluster objects without levelsets"));

DECLARE_CYCLE_STAT(TEXT("FGeometryCollectionPhysicsProxy::BuildClusters"), STAT_BuildClusters, STATGROUP_Chaos);
DECLARE_CYCLE_STAT(TEXT("FGeometryCollectionPhysicsProxy::BuildClusters:GlobalMatrices"), STAT_BuildClustersGlobalMatrices, STATGROUP_Chaos);

float FGeometryCollectionPhysicsProxy::ComputeDamageThreshold(const FGeometryDynamicCollection& DynamicCollection, int32 TransformIndex) const 
{
	float DamageThreshold = TNumericLimits<float>::Max();

	const int32 LevelOffset = Parameters.bUsePerClusterOnlyDamageThreshold ? 0 : -1;
	const int32 Level = FMath::Clamp(CalculateHierarchyLevel(DynamicCollection, TransformIndex) + LevelOffset, 0, INT_MAX);
	if (Level >= Parameters.MaxClusterLevel)
	{
		DamageThreshold = TNumericLimits<float>::Max();
	}
	else
	{
		if (Parameters.bUseSizeSpecificDamageThresholds)
		{
			// bounding box volume is used as a fallback to find specific size if the relative size if not available
			// ( May happen with older GC )
			FBox LocalBoundingBox;
			const FGeometryDynamicCollection::FSharedImplicit& Implicit = DynamicCollection.Implicits[TransformIndex];
			if (Implicit && Implicit->HasBoundingBox())
			{
				const Chaos::FAABB3& ImplicitBoundingBox = Implicit->BoundingBox();
				LocalBoundingBox = FBox(ImplicitBoundingBox.Min(), ImplicitBoundingBox.Max());
			}

			const FSharedSimulationSizeSpecificData& SizeSpecificData = GetSizeSpecificData(Parameters.Shared.SizeSpecificData, *Parameters.RestCollection, TransformIndex, LocalBoundingBox);
			DamageThreshold = SizeSpecificData.DamageThreshold;
		}
		else
		{
			const int32 NumThresholds = Parameters.DamageThreshold.Num();
			const float DefaultDamage = NumThresholds > 0 ? Parameters.DamageThreshold[NumThresholds - 1] : 0.f;
			DamageThreshold = Level < NumThresholds ? Parameters.DamageThreshold[Level] : DefaultDamage;
		}
	}

	return DamageThreshold;
}

Chaos::FPBDRigidClusteredParticleHandle*
FGeometryCollectionPhysicsProxy::BuildClusters_Internal(
	const uint32 CollectionClusterIndex, // TransformGroupIndex
	TArray<Chaos::FPBDRigidParticleHandle*>& ChildHandles,
	const TArray<int32>& ChildTransformGroupIndices,
	const Chaos::FClusterCreationParameters & ClusterParameters,
	const Chaos::FUniqueIdx* ExistingIndex)
{
	SCOPE_CYCLE_COUNTER(STAT_BuildClusters);

	check(CollectionClusterIndex != INDEX_NONE);
	check(ChildHandles.Num() != 0);

	FGeometryDynamicCollection& DynamicCollection = PhysicsThreadCollection;
	TManagedArray<int32>& DynamicState = DynamicCollection.DynamicState;
	TManagedArray<int32>& ParentIndex = DynamicCollection.Parent;
	TManagedArray<TSet<int32>>& Children = DynamicCollection.Children;
	TManagedArray<FTransform>& Transform = DynamicCollection.Transform;
	TManagedArray<FTransform>& MassToLocal = DynamicCollection.MassToLocal;
	//TManagedArray<TSharedPtr<FCollisionStructureManager::FSimplicial> >& Simplicials = DynamicCollection.Simplicials;
	TManagedArray<FGeometryDynamicCollection::FSharedImplicit>& Implicits = DynamicCollection.Implicits;

	//If we are a root particle use the world transform, otherwise set the relative transform
	const FTransform CollectionSpaceTransform = GeometryCollectionAlgo::GlobalMatrix(Transform, ParentIndex, CollectionClusterIndex);
	const Chaos::TRigidTransform<Chaos::FReal, 3> ParticleTM = MassToLocal[CollectionClusterIndex] * CollectionSpaceTransform * Parameters.WorldTransform;

	//create new cluster particle
	//The reason we need to pass in a mass orientation override is as follows:
	//Consider a pillar made up of many boxes along the Y-axis. In this configuration we could generate a proxy pillar along the Y with identity rotation.
	//Now if we instantiate the pillar and rotate it so that it is along the X-axis, we would still like to use the same pillar proxy.
	//Since the mass orientation is computed in world space in both cases we'd end up with a diagonal inertia matrix and identity rotation that looks like this: [big, small, big] or [small, big, big].
	//Because of this we need to know how to rotate collision particles and geometry to match with original computation. If it was just geometry we could transform it before passing, but we need collision particles as well
	Chaos::FClusterCreationParameters ClusterCreationParameters = ClusterParameters;
	// connectivity is taken care outside, no need to generate it here
	ClusterCreationParameters.bGenerateConnectionGraph = false;
	// fix... ClusterCreationParameters.CollisionParticles = Simplicials[CollectionClusterIndex];
	ClusterCreationParameters.ConnectionMethod = Parameters.ClusterConnectionMethod;
	if (ClusterCreationParameters.CollisionParticles)
	{
		const Chaos::FReal NumCollisionParticles = static_cast<Chaos::FReal>(ClusterCreationParameters.CollisionParticles->Size());
		const int32 ClampedCollisionParticlesSize = 
			FMath::TruncToInt32(Chaos::FReal(FMath::Max(0, FMath::Min(NumCollisionParticles * CollisionParticlesPerObjectFraction, NumCollisionParticles))));
		ClusterCreationParameters.CollisionParticles->Resize(ClampedCollisionParticlesSize);
	}
	Chaos::Facades::FCollectionAnchoringFacade AnchoringFacade(DynamicCollection);
	ClusterCreationParameters.bIsAnchored = AnchoringFacade.IsAnchored(CollectionClusterIndex);
	
	TArray<Chaos::TPBDRigidParticleHandle<Chaos::FReal, 3>*> ChildHandlesCopy(ChildHandles);

	// Construct an active cluster particle, disable children, derive M and I from children:
	Chaos::TPBDRigidClusteredParticleHandle<Chaos::FReal, 3>* Parent =
		static_cast<Chaos::FPBDRigidsSolver*>(Solver)->GetEvolution()->GetRigidClustering().CreateClusterParticle(
			Parameters.ClusterGroupIndex, 
			MoveTemp(ChildHandlesCopy),
			ClusterCreationParameters,
			Implicits[CollectionClusterIndex], // union from children if null
			&ParticleTM,
			ExistingIndex
			);

	if (ReportNoLevelsetCluster && 
		Parent->DynamicGeometry())
	{
		//ensureMsgf(false, TEXT("Union object generated for cluster"));
		UE_LOG(LogChaos, Warning, TEXT("Union object generated for cluster:%s"), *Parameters.Name);
	}

	if (Parent->InvM() == 0.0)
	{
		if (Parent->ObjectState() == Chaos::EObjectStateType::Static)
		{
			DynamicState[CollectionClusterIndex] = (uint8)EObjectStateTypeEnum::Chaos_Object_Static;
		}
		else //if (Particles.ObjectState(NewSolverClusterID) == Chaos::EObjectStateType::Kinematic)
		{
			DynamicState[CollectionClusterIndex] = (uint8)EObjectStateTypeEnum::Chaos_Object_Kinematic;
		}
	}

	check(Parameters.RestCollection);
	const TManagedArray<Chaos::FReal>& Mass =
		Parameters.RestCollection->GetAttribute<Chaos::FReal>("Mass", FTransformCollection::TransformGroup);
	const TManagedArray<FVector3f>& InertiaTensor = 
		Parameters.RestCollection->GetAttribute<FVector3f>("InertiaTensor", FTransformCollection::TransformGroup);

	const FVector WorldScale = Parameters.WorldTransform.GetScale3D();
	const FVector::FReal MassScale = WorldScale.X * WorldScale.Y * WorldScale.Z;
	const Chaos::FVec3f ScaledInertia = Chaos::Utilities::ScaleInertia<float>((Chaos::FVec3f)InertiaTensor[CollectionClusterIndex], FVector3f(WorldScale), true);  
	
	// NOTE: The particle creation call above (CreateClusterParticle) activates the particle, which does various things including adding
	// it to the constraint graph. It seems a bit dodgy to be completely changing what the particle is after it has been enabled. Maybe we should fix this.
	PopulateSimulatedParticle(
		Parent,
		Parameters.Shared, 
		nullptr, // CollisionParticles is optionally set from CreateClusterParticle()
		nullptr, // Parent->Geometry() ? Parent->Geometry() : Implicits[CollectionClusterIndex], 
		SimFilter,
		QueryFilter,
		Parent->M() > 0.0 ? Parent->M() : Mass[CollectionClusterIndex] * MassScale, 
		Parent->I() != Chaos::FVec3f(0.0) ? Parent->I() : ScaledInertia,
		ParticleTM, 
		(uint8)DynamicState[CollectionClusterIndex], 
		0,
		CollisionParticlesPerObjectFraction); // CollisionGroup

	// two-way mapping
	SolverClusterHandles[CollectionClusterIndex] = Parent;

	const float DamageThreshold = ComputeDamageThreshold(DynamicCollection, CollectionClusterIndex);
	Parent->SetStrains(DamageThreshold);
	
	// #BGTODO This will not automatically update - material properties should only ever exist in the material, not in other arrays
	const Chaos::FChaosPhysicsMaterial* CurMaterial = static_cast<Chaos::FPBDRigidsSolver*>(Solver)->GetSimMaterials().Get(Parameters.PhysicalMaterialHandle.InnerHandle);
	if(CurMaterial)
	{
		Parent->SetLinearEtherDrag(CurMaterial->LinearEtherDrag);
		Parent->SetAngularEtherDrag(CurMaterial->AngularEtherDrag);
	}

	const Chaos::FShapesArray& Shapes = Parent->ShapesArray();
	for(const TUniquePtr<Chaos::FPerShapeData>& Shape : Shapes)
	{
		Shape->SetMaterial(Parameters.PhysicalMaterialHandle);
	}

	const FTransform ParentTransform = GeometryCollectionAlgo::GlobalMatrix(DynamicCollection.Transform, DynamicCollection.Parent, CollectionClusterIndex);

	int32 MinCollisionGroup = INT_MAX;
	for(int32 Idx=0; Idx < ChildHandles.Num(); Idx++)
	{
		// set the damage threshold on children as they are the one where the strain is tested when breaking 
		Chaos::FPBDRigidParticleHandle* Child = ChildHandles[Idx];
		if (Parameters.bUsePerClusterOnlyDamageThreshold)
		{
			if (Chaos::FPBDRigidClusteredParticleHandle* ClusteredChild = Child->CastToClustered())
			{
				ClusteredChild->SetStrains(DamageThreshold);
			}
		}

		const int32 ChildTransformGroupIndex = ChildTransformGroupIndices[Idx];
		SolverClusterHandles[ChildTransformGroupIndex] = Parent;

		MinCollisionGroup = FMath::Min(Child->CollisionGroup(), MinCollisionGroup);
	}
	Parent->SetCollisionGroup(MinCollisionGroup);

	// Populate bounds as we didn't pass a shared implicit to PopulateSimulatedParticle this will have been skipped, now that we have the full cluster we can build it
	if(Parent->Geometry() && Parent->Geometry()->HasBoundingBox())
	{
		Parent->SetHasBounds(true);
		Parent->SetLocalBounds(Parent->Geometry()->BoundingBox());
		const Chaos::FRigidTransform3 Xf(Parent->X(), Parent->R());
		Parent->UpdateWorldSpaceState(Xf, Chaos::FVec3(0));

		static_cast<Chaos::FPBDRigidsSolver*>(Solver)->GetEvolution()->DirtyParticle(*Parent);
	}

	return Parent;
}

void FGeometryCollectionPhysicsProxy::GetFilteredParticleHandles(
	TArray<Chaos::TGeometryParticleHandle<Chaos::FReal, 3>*>& Handles,
	const Chaos::FPBDRigidsSolver* RigidSolver,
	const EFieldFilterType FilterType,
	const EFieldObjectType ObjectType)
{
	Handles.SetNum(0, false);
	if ((ObjectType == EFieldObjectType::Field_Object_All) || (ObjectType == EFieldObjectType::Field_Object_Destruction) || (ObjectType == EFieldObjectType::Field_Object_Max))
	{
		// only the local handles
		TArray<FClusterHandle*>& ParticleHandles = GetSolverParticleHandles();
		Handles.Reserve(ParticleHandles.Num());

		if (FilterType == EFieldFilterType::Field_Filter_Dynamic)
		{
			for (FClusterHandle* ClusterHandle : ParticleHandles)
			{
				if (ClusterHandle && (ClusterHandle->ObjectState() == Chaos::EObjectStateType::Dynamic))
				{
					Handles.Add(ClusterHandle);
				}
			}
		}
		else if (FilterType == EFieldFilterType::Field_Filter_Kinematic)
		{
			for (FClusterHandle* ClusterHandle : ParticleHandles)
			{
				if (ClusterHandle && (ClusterHandle->ObjectState() == Chaos::EObjectStateType::Kinematic))
				{
					Handles.Add(ClusterHandle);
				}
			}
		}
		else if (FilterType == EFieldFilterType::Field_Filter_Static)
		{
			for (FClusterHandle* ClusterHandle : ParticleHandles)
			{
				if (ClusterHandle && (ClusterHandle->ObjectState() == Chaos::EObjectStateType::Static))
				{
					Handles.Add(ClusterHandle);
				}
			}
		}
		else if (FilterType == EFieldFilterType::Field_Filter_Sleeping)
		{
			for (FClusterHandle* ClusterHandle : ParticleHandles)
			{
				if (ClusterHandle && (ClusterHandle->ObjectState() == Chaos::EObjectStateType::Sleeping))
				{
					Handles.Add(ClusterHandle);
				}
			}
		}
		else if (FilterType == EFieldFilterType::Field_Filter_Disabled)
		{
			for (FClusterHandle* ClusterHandle : ParticleHandles)
			{
				if (ClusterHandle && ClusterHandle->Disabled())
				{
					Handles.Add(ClusterHandle);
				}
			}
		}
		else if (FilterType == EFieldFilterType::Field_Filter_All)
		{
			for (FClusterHandle* ClusterHandle : ParticleHandles)
			{
				if (ClusterHandle && (ClusterHandle->ObjectState() != Chaos::EObjectStateType::Uninitialized))
				{
					Handles.Add(ClusterHandle);
				}
			}
		}
	}
}

void FGeometryCollectionPhysicsProxy::GetRelevantParticleHandles(
	TArray<Chaos::TGeometryParticleHandle<Chaos::FReal, 3>*>& Handles,
	const Chaos::FPBDRigidsSolver* RigidSolver, 
	EFieldResolutionType ResolutionType)
{
	Handles.SetNum(0, false);

	// only the local handles
	TArray<FClusterHandle*>& ParticleHandles = GetSolverParticleHandles();
	Handles.Reserve(ParticleHandles.Num());

	if (ResolutionType == EFieldResolutionType::Field_Resolution_Maximum)
	{
		for (FClusterHandle* ClusterHandle : ParticleHandles)
		{
			if (ClusterHandle )
			{
				Handles.Add(ClusterHandle);
			}
		}
	}
	else if (ResolutionType == EFieldResolutionType::Field_Resolution_DisabledParents)
	{
		for (FClusterHandle* ClusterHandle : ParticleHandles)
		{
			if (ClusterHandle && ClusterHandle->ClusterIds().Id == nullptr)
			{
				Handles.Add(ClusterHandle);
			}
		}
	}
	else if (ResolutionType == EFieldResolutionType::Field_Resolution_Minimal)
	{
		const auto& Clustering = RigidSolver->GetEvolution()->GetRigidClustering();
		const auto& ClusterMap = Clustering.GetChildrenMap();

		for (FClusterHandle* ClusterHandle : ParticleHandles)
		{
			if (ClusterHandle && !ClusterHandle->Disabled())
			{
				Handles.Add(ClusterHandle);
				if (ClusterHandle->ClusterIds().NumChildren)
				{
					if (ClusterMap.Contains(ClusterHandle))
					{
						for (Chaos::TPBDRigidParticleHandle<Chaos::FReal, 3> * Child : ClusterMap[ClusterHandle])
						{
							Handles.Add(Child);
						}
					}
				}
			}
		}
	}

#if TODO_REIMPLEMENT_GET_RIGID_PARTICLES
		const Chaos::FPhysicsSolver::FParticlesType & Particles = RigidSolver->GetRigidParticles();
		if (ResolutionType == EFieldResolutionType::Field_Resolution_Minimal)
		{
			const Chaos::TArrayCollectionArray<Chaos::ClusterId>& ClusterIdArray = RigidSolver->GetRigidClustering().GetClusterIdsArray();


			//  Generate a Index mapping between the rigid body indices and 
			//  the particle indices. This allows the geometry collection to
			//  evaluate only its own ACTIVE particles + ClusterChildren
			int32 NumIndices = 0;
			Array.SetNumUninitialized(RigidBodyID.Num());
			for (int32 i = 0; i < RigidBodyID.Num(); i++)
			{
				const int32 RigidBodyIndex = RigidBodyID[i];
				if (RigidBodyIndex != INDEX_NONE && !Particles.Disabled(RigidBodyIndex)) // active bodies
				{
					Array[NumIndices] = { RigidBodyID[i],i };
					NumIndices++;
				}
				if (ClusterIdArray[RigidBodyIndex].Id != INDEX_NONE && !Particles.Disabled(ClusterIdArray[RigidBodyIndex].Id)) // children
				{
					Array[NumIndices] = { RigidBodyID[i],i };
					NumIndices++;
				}
			}
			Array.SetNum(NumIndices);
		}
		else if (ResolutionType == EFieldResolutionType::Field_Resolution_Maximum)
		{
			//  Generate a Index mapping between the rigid body indices and 
			//  the particle indices. This allows the geometry collection to
			//  evaluate only its own particles. 
			int32 NumIndices = 0;
			Array.SetNumUninitialized(RigidBodyID.Num());
			for (int32 i = 0; i < RigidBodyID.Num(); i++)
			{
				const int32 RigidBodyIndex = RigidBodyID[i];
				if (RigidBodyIndex != INDEX_NONE)
				{
					Array[NumIndices] = { RigidBodyIndex, i };
					NumIndices++;
				}
			}
			Array.SetNum(NumIndices);
		}
#endif
}

void FGeometryCollectionPhysicsProxy::DisableParticles_External(TArray<int32>&& TransformGroupIndices)
{
	check(IsInGameThread());

	if (Chaos::FPhysicsSolver* RBDSolver = GetSolver<Chaos::FPhysicsSolver>())
	{
		RBDSolver->EnqueueCommandImmediate([this, RBDSolver, IndicesToDisable = MoveTemp(TransformGroupIndices)]()
			{
				for (int32 TransformIdx : IndicesToDisable)
				{
					RBDSolver->GetEvolution()->DisableParticleWithRemovalEvent(SolverParticleHandles[TransformIdx]);
				}
			});
	}
}

void FGeometryCollectionPhysicsProxy::ApplyForceAt_External(FVector Force, FVector WorldLocation)
{
	check(IsInGameThread());

	if (Chaos::FPhysicsSolver* RBDSolver = GetSolver<Chaos::FPhysicsSolver>())
	{
		RBDSolver->EnqueueCommandImmediate([this, Force, WorldLocation, RBDSolver]()
			{
				Chaos::FReal ClosestDistanceSquared = TNumericLimits<Chaos::FReal>::Max();
				Chaos::FPBDRigidClusteredParticleHandle* ClosestHandle = nullptr;

				Chaos::FRigidClustering& Clustering = RBDSolver->GetEvolution()->GetRigidClustering();
				for (Chaos::FPBDRigidClusteredParticleHandle* ClusteredHandle : Clustering.GetTopLevelClusterParents())
				{
					if (ClusteredHandle && ClusteredHandle->PhysicsProxy() == this)
					{
						if (ClusteredHandle->IsDynamic())
						{
							const Chaos::FReal DistanceSquared = (WorldLocation - ClusteredHandle->X()).SquaredLength();
							if (DistanceSquared < ClosestDistanceSquared)
							{
								ClosestDistanceSquared = DistanceSquared;
								ClosestHandle = ClusteredHandle;
							}
						}
					}
				}
				if (ClosestHandle)
				{
					const Chaos::FVec3 WorldCOM = Chaos::FParticleUtilitiesXR::GetCoMWorldPosition(ClosestHandle);
					const Chaos::FVec3 WorldTorque = Chaos::FVec3::CrossProduct(WorldLocation - WorldCOM, Force);
					ClosestHandle->AddForce(Force);
					ClosestHandle->AddTorque(WorldTorque);
				}
			});
	}
}

void FGeometryCollectionPhysicsProxy::ApplyImpulseAt_External(FVector Impulse, FVector WorldLocation)
{
	check(IsInGameThread());

	if (Chaos::FPhysicsSolver* RBDSolver = GetSolver<Chaos::FPhysicsSolver>())
	{
		RBDSolver->EnqueueCommandImmediate([this, Impulse, WorldLocation, RBDSolver]()
			{
				Chaos::FReal ClosestDistanceSquared = TNumericLimits<Chaos::FReal>::Max();
				Chaos::FPBDRigidClusteredParticleHandle* ClosestHandle = nullptr;

				Chaos::FRigidClustering& Clustering = RBDSolver->GetEvolution()->GetRigidClustering();
				for (Chaos::FPBDRigidClusteredParticleHandle* ClusteredHandle : Clustering.GetTopLevelClusterParents())
				{
					if (ClusteredHandle && ClusteredHandle->PhysicsProxy() == this)
					{
						if (ClusteredHandle->IsDynamic())
						{
							const Chaos::FReal DistanceSquared = (WorldLocation - ClusteredHandle->X()).SquaredLength();
							if (DistanceSquared < ClosestDistanceSquared)
							{
								ClosestDistanceSquared = DistanceSquared;
								ClosestHandle = ClusteredHandle;
							}
						}
					}
				}
				if (ClosestHandle)
				{
					const Chaos::FVec3 WorldCOM = Chaos::FParticleUtilitiesXR::GetCoMWorldPosition(ClosestHandle);
					ClosestHandle->SetLinearImpulseVelocity(ClosestHandle->LinearImpulseVelocity() + (Impulse * ClosestHandle->InvM()), false);

					const Chaos::FMatrix33 WorldInvI = Chaos::Utilities::ComputeWorldSpaceInertia(ClosestHandle->R() * ClosestHandle->RotationOfMass(), ClosestHandle->InvI());
					const Chaos::FVec3 AngularImpulse = Chaos::FVec3::CrossProduct(WorldLocation - WorldCOM, Impulse);
					ClosestHandle->SetAngularImpulseVelocity(ClosestHandle->AngularImpulseVelocity() + WorldInvI * AngularImpulse, false);
				}
			});
	}
}

void FGeometryCollectionPhysicsProxy::BreakClusters_External(TArray<FGeometryCollectionItemIndex>&& ItemIndices)
{
	check(IsInGameThread());

	if (Chaos::FPhysicsSolver* RBDSolver = GetSolver<Chaos::FPhysicsSolver>())
	{
		RBDSolver->EnqueueCommandImmediate([this, RBDSolver, IndicesToBreakParent = MoveTemp(ItemIndices)]()
		{
			Chaos::FRigidClustering& Clustering = RBDSolver->GetEvolution()->GetRigidClustering();
			for (const FGeometryCollectionItemIndex& ItemIndex : IndicesToBreakParent)
			{
				if (Chaos::FPBDRigidClusteredParticleHandle* ClusteredHandle = FindClusteredParticleHandleByItemIndex_Internal(ItemIndex))
				{
					Clustering.BreakCluster(ClusteredHandle);
				}
			}
		});
	}
}


void FGeometryCollectionPhysicsProxy::BreakActiveClusters_External()
{
	check(IsInGameThread());

	if (Chaos::FPhysicsSolver* RBDSolver = GetSolver<Chaos::FPhysicsSolver>())
	{
		RBDSolver->EnqueueCommandImmediate([this, RBDSolver]()
		{
			Chaos::FRigidClustering& Clustering = RBDSolver->GetEvolution()->GetRigidClustering();
			Clustering.BreakClustersByProxy(this);
		});
	}
}

void FGeometryCollectionPhysicsProxy::RemoveAllAnchors_External()
{
	check(IsInGameThread());

	if (Chaos::FPhysicsSolver* RBDSolver = GetSolver<Chaos::FPhysicsSolver>())
	{
		RBDSolver->EnqueueCommandImmediate([this, RBDSolver]()
			{
				Chaos::FPBDRigidsEvolution* Evolution = RBDSolver->GetEvolution();

				// disable anchoring where relevant and collect the nodes to update
				Chaos::FKinematicTarget NoKinematicTarget;
				for (FClusterHandle* ParticleHandle : GetSolverParticleHandles())
				{
					if (ParticleHandle)
					{
						ParticleHandle->SetIsAnchored(false);
						if (!ParticleHandle->IsDynamic())
						{
							Evolution->SetParticleObjectState(ParticleHandle, Chaos::EObjectStateType::Dynamic);
							Evolution->SetParticleKinematicTarget(ParticleHandle, NoKinematicTarget);
						}
					}
				}
			});
	}
}

template <typename TAction>
static void ApplyToChildrenAtPointWithRadiusAndPropagation_Internal(
	Chaos::FRigidClustering& Clustering,
	Chaos::FPBDRigidClusteredParticleHandle& ClusteredHandle,
	const FVector& WorldLocation,
	float Radius,
	int32 PropagationDepth,
	float PropagationFactor,
	TAction Action)
{
	const bool bIsCluster = (ClusteredHandle.ClusterIds().NumChildren > 0);
	if (bIsCluster)
	{
		TArray<Chaos::FPBDRigidParticleHandle*> ChildrenHandles = Clustering.FindChildrenWithinRadius(&ClusteredHandle, WorldLocation, Radius, true /* always return closest */ );
		if (ChildrenHandles.Num())
		{
			TArray<Chaos::FPBDRigidParticleHandle*> ProcessedHandles;
			Chaos::FReal PropagationMultiplier = 1.0f;

			int32 CurrentPropagationDepth = PropagationDepth;
			while (CurrentPropagationDepth >= 0)
			{
				// apply to current
				for (Chaos::FPBDRigidParticleHandle* ChildHandle: ChildrenHandles)
				{
					if (Chaos::FPBDRigidClusteredParticleHandle* ClusteredChildHandle = ChildHandle->CastToClustered())
					{
						Action(ClusteredChildHandle, PropagationMultiplier);
//						ClusteredChildHandle->SetExternalStrain(FMath::Max(ClusteredChildHandle->GetExternalStrain(), StrainValue * PropagationMultiplier));
					}
				}

				// find handles to propagate to
				if (CurrentPropagationDepth > 0)
				{
					// only propagate to newly added ( avoids allocating a new temporary array for next level handles )
					// move the entire ChildrenHandle array to the processed ones so that it also resets it for us
					const int32 StartIndex = ProcessedHandles.Num(); 
					ProcessedHandles.Append(MoveTemp(ChildrenHandles));
					
					for (int32 ChildIndex = StartIndex; ChildIndex < ProcessedHandles.Num(); ++ChildIndex)
					{
						if (Chaos::FPBDRigidClusteredParticleHandle* ClusteredChildHandle = ProcessedHandles[ChildIndex]->CastToClustered())
						{
							for (const Chaos::TConnectivityEdge<Chaos::FReal>& Edge:ClusteredChildHandle->ConnectivityEdges())
							{
								if (Edge.Sibling && !ChildrenHandles.Contains(Edge.Sibling))
								{
									ChildrenHandles.Add(Edge.Sibling);
								}
							}
						}
					}
				}
				
				PropagationMultiplier *= static_cast<Chaos::FReal>(PropagationFactor);
				--CurrentPropagationDepth;
			}
		}
	}
}


void FGeometryCollectionPhysicsProxy::ApplyExternalStrain_External(FGeometryCollectionItemIndex ItemIndex, const FVector& WorldLocation, float Radius, int32 PropagationDepth, float PropagationFactor, float StrainValue)
{
	check(IsInGameThread());

	if (Chaos::FPhysicsSolver* RBDSolver = GetSolver<Chaos::FPhysicsSolver>())
	{
		RBDSolver->EnqueueCommandImmediate([this, Radius, StrainValue, PropagationDepth, PropagationFactor, RBDSolver, WorldLocation, ItemIndex]()
		{
			if (Chaos::FPBDRigidClusteredParticleHandle* ClusteredHandle = FindClusteredParticleHandleByItemIndex_Internal(ItemIndex))
			{
				ApplyToChildrenAtPointWithRadiusAndPropagation_Internal(
					RBDSolver->GetEvolution()->GetRigidClustering(), *ClusteredHandle,
					WorldLocation, Radius, PropagationDepth, PropagationFactor,
					[StrainValue](Chaos::FPBDRigidClusteredParticleHandle* ClusteredChildHandle, Chaos::FReal PropagationMultiplier)
					{
						ClusteredChildHandle->SetExternalStrain(FMath::Max(ClusteredChildHandle->GetExternalStrain(), StrainValue * PropagationMultiplier));
					});
			}
		});
	}
}

void FGeometryCollectionPhysicsProxy::ApplyInternalStrain_External(FGeometryCollectionItemIndex ItemIndex, const FVector& WorldLocation, float Radius, int32 PropagationDepth, float PropagationFactor, float StrainValue)
{
	check(IsInGameThread());

	if (Chaos::FPhysicsSolver* RBDSolver = GetSolver<Chaos::FPhysicsSolver>())
	{
		RBDSolver->EnqueueCommandImmediate([this, Radius, StrainValue, PropagationDepth, PropagationFactor, RBDSolver, WorldLocation, ItemIndex]()
		{
			if (Chaos::FPBDRigidClusteredParticleHandle* ClusteredHandle = FindClusteredParticleHandleByItemIndex_Internal(ItemIndex))
			{
				ApplyToChildrenAtPointWithRadiusAndPropagation_Internal(
					RBDSolver->GetEvolution()->GetRigidClustering(), *ClusteredHandle,
					WorldLocation, Radius, PropagationDepth, PropagationFactor,
					[StrainValue](Chaos::FPBDRigidClusteredParticleHandle* ClusteredChildHandle, Chaos::FReal PropagationMultiplier)
					{
						const Chaos::FReal NewInternalStrain = ClusteredChildHandle->Strain() - ((Chaos::FReal)StrainValue * PropagationMultiplier);
						ClusteredChildHandle->SetStrain(FMath::Max(0,NewInternalStrain));
					});
			}
		});
	}
}

template <typename TAction>
static void ApplyToBreakingChildren_Internal(Chaos::FRigidClustering& Clustering, Chaos::FPBDRigidClusteredParticleHandle& ClusteredHandle, TAction Action)
{
	const bool bIsCluster = (ClusteredHandle.ClusterIds().NumChildren > 0);
	if (bIsCluster && !ClusteredHandle.Disabled())
	{
		Chaos::FRigidClustering::FClusterMap& ChildrenMap = Clustering.GetChildrenMap();
		if (const TArray<Chaos::FPBDRigidParticleHandle*>* ChildrenHandles = ChildrenMap.Find(&ClusteredHandle))
		{
			for (Chaos::FPBDRigidParticleHandle* ChildHandle: *ChildrenHandles)
			{
				if (Chaos::FPBDRigidClusteredParticleHandle* ClusteredChildHandle = ChildHandle->CastToClustered())
				{
					if (ClusteredChildHandle->GetExternalStrain() > ClusteredChildHandle->Strain())
					{
						Action(ClusteredChildHandle);
					}
				}
			}
		}
	}
}

void FGeometryCollectionPhysicsProxy::ApplyBreakingLinearVelocity_External(FGeometryCollectionItemIndex ItemIndex, const FVector& LinearVelocity)
{
	check(IsInGameThread());

	if (Chaos::FPhysicsSolver* RBDSolver = GetSolver<Chaos::FPhysicsSolver>())
	{
		RBDSolver->EnqueueCommandImmediate([this, RBDSolver, ItemIndex, LinearVelocity]()
		{
			if (Chaos::FPBDRigidClusteredParticleHandle* ClusteredHandle = FindClusteredParticleHandleByItemIndex_Internal(ItemIndex))
			{
				ApplyToBreakingChildren_Internal(RBDSolver->GetEvolution()->GetRigidClustering(), *ClusteredHandle,
					[LinearVelocity](Chaos::FPBDRigidClusteredParticleHandle* ClusteredChildHandle)
					{
						ClusteredChildHandle->V() += LinearVelocity;
					});
			}
		});
	}
}

void FGeometryCollectionPhysicsProxy::ApplyBreakingAngularVelocity_External(FGeometryCollectionItemIndex ItemIndex, const FVector& AngularVelocity)
{
	check(IsInGameThread());

	if (Chaos::FPhysicsSolver* RBDSolver = GetSolver<Chaos::FPhysicsSolver>())
	{
		RBDSolver->EnqueueCommandImmediate([this, RBDSolver, ItemIndex, AngularVelocity]()
		{
			if (Chaos::FPBDRigidClusteredParticleHandle* ClusteredHandle = FindClusteredParticleHandleByItemIndex_Internal(ItemIndex))
			{
				ApplyToBreakingChildren_Internal(RBDSolver->GetEvolution()->GetRigidClustering(), *ClusteredHandle,
					[AngularVelocity](Chaos::FPBDRigidClusteredParticleHandle* ClusteredChildHandle)
					{
						ClusteredChildHandle->W() += AngularVelocity;
					});
			}
		});
	}
}

void FGeometryCollectionPhysicsProxy::ApplyLinearVelocity_External(FGeometryCollectionItemIndex ItemIndex, const FVector& LinearVelocity)
{
	check(IsInGameThread());

	if (Chaos::FPhysicsSolver* RBDSolver = GetSolver<Chaos::FPhysicsSolver>())
	{
		RBDSolver->EnqueueCommandImmediate([this, ItemIndex, LinearVelocity]()
		{
			if (Chaos::FPBDRigidClusteredParticleHandle* ClusteredHandle = FindClusteredParticleHandleByItemIndex_Internal(ItemIndex))
			{
				ClusteredHandle->V() += LinearVelocity;
			}
		});
	}
}

void FGeometryCollectionPhysicsProxy::ApplyAngularVelocity_External(FGeometryCollectionItemIndex ItemIndex, const FVector& AngularVelocity)
{
	check(IsInGameThread());

	if (Chaos::FPhysicsSolver* RBDSolver = GetSolver<Chaos::FPhysicsSolver>())
	{
		RBDSolver->EnqueueCommandImmediate([this, ItemIndex, AngularVelocity]()
		{
			if (Chaos::FPBDRigidClusteredParticleHandle* ClusteredHandle = FindClusteredParticleHandleByItemIndex_Internal(ItemIndex))
			{
				ClusteredHandle->W() += AngularVelocity;
			}
		});
	}
}

int32 FGeometryCollectionPhysicsProxy::CalculateHierarchyLevel(const FGeometryDynamicCollection& DynamicCollection, int32 TransformIndex)
{
	int32 Level = 0;
	while (DynamicCollection.Parent[TransformIndex] != INDEX_NONE)
	{
		TransformIndex = DynamicCollection.Parent[TransformIndex];
		Level++;
	}
	return Level;
}



int32 FGeometryCollectionPhysicsProxy::CalculateAndSetLevel(int32 TransformGroupIndex, const TManagedArray<int32>& Parent, TManagedArray<int32>& Levels)
{
	// Count levels up to root or first parent that has level initialized
	int32 LevelsFromRoot = 0;

	// Parents up tree from current index that need level initialization, ordered depth first
	TArray<int32, TInlineAllocator<8>> ParentStack;

	int32 ParentTransformGroupIndex = Parent[TransformGroupIndex];
	while (ParentTransformGroupIndex != INDEX_NONE)
	{
		LevelsFromRoot++;

		int32 ParentLevel = Levels[ParentTransformGroupIndex];
		if (ParentLevel > 0)
		{
			// If parent level is not 0 it has already been computed, 
			// can early out and not search for true root.
			Levels[TransformGroupIndex] = ParentLevel + LevelsFromRoot;
			break;
		}

		// save this parent so we can set it's level once we determine depth
		ParentStack.Push(ParentTransformGroupIndex);

		ParentTransformGroupIndex = Parent[ParentTransformGroupIndex];
	}

	if (LevelsFromRoot > 0) // Not at root, update levels of current particle and uninitialized parents
	{
		if (Levels[TransformGroupIndex] == 0)
		{
			// If we are uninitialized, we did not early out after finding an initialized parent up the tree, and LevelsFromRoot is new level.
			Levels[TransformGroupIndex] = LevelsFromRoot;
		}

		// Initialize newly discovered parent levels
		for (int32 Idx = 0; Idx < ParentStack.Num(); ++Idx)
		{
			int32 ParentTransformGroupIdx = ParentStack[Idx];

			// Level of parent is leaf level minus parent's distance from leaf minus 1.
			Levels[ParentTransformGroupIdx] = Levels[TransformGroupIndex] - Idx - 1;
		}
	}

	return Levels[TransformGroupIndex];
}

void FGeometryCollectionPhysicsProxy::OnRemoveFromSolver(Chaos::FPBDRigidsSolver *RBDSolver)
{
	Chaos::FPBDRigidsEvolutionGBF* Evolution = RBDSolver->GetEvolution();

	TSet< FClusterHandle* > ClustersToRebuild;
	for (int i = 0; i < SolverParticleHandles.Num(); i++)
	{
		if (FClusterHandle* Handle = SolverParticleHandles[i])
		{
			if (FClusterHandle* ParentCluster = Evolution->GetRigidClustering().DestroyClusterParticle(Handle))
			{
				if (ParentCluster->InternalCluster())
				{
					ClustersToRebuild.Add(ParentCluster);
				}
			}
		}
	}

	for (int i = 0; i < SolverParticleHandles.Num(); i++)
	{
		if (FClusterHandle* Handle = SolverParticleHandles[i])
		{
			Chaos::FUniqueIdx UniqueIdx = Handle->UniqueIdx();
			Evolution->DestroyParticle(Handle);
			Evolution->ReleaseUniqueIdx(UniqueIdx);
		}
	}

	for (FClusterHandle* Cluster : ClustersToRebuild)
	{
		ensure(Cluster->InternalCluster());
		if (ensure(Evolution->GetRigidClustering().GetChildrenMap().Contains(Cluster)))
		{
			// copy cluster state for recreation
			int32 ClusterGroupIndex = Cluster->ClusterGroupIndex();
			TArray<FParticleHandle*> Children = Evolution->GetRigidClustering().GetChildrenMap()[Cluster];

			// destroy the invalid cluster
			FClusterHandle* NullHandle = Evolution->GetRigidClustering().DestroyClusterParticle(Cluster);
			ensure(NullHandle == nullptr);

			// create a new cluster if needed
			if (Children.Num())
			{
				if (FClusterHandle* NewParticle = Evolution->GetRigidClustering().CreateClusterParticle(ClusterGroupIndex, MoveTemp(Children)))
				{
					NewParticle->SetInternalCluster(true);
				}
			}
		}		
	}

	IsObjectDeleting = true;
}

void FGeometryCollectionPhysicsProxy::OnRemoveFromScene()
{
#if TODO_REIMPLEMENT_GET_RIGID_PARTICLES
	// #BG TODO This isn't great - we currently cannot handle things being removed from the solver.
	// need to refactor how we handle this and actually remove the particles instead of just constantly
	// growing the array. Currently everything is just tracked by index though so the solver will have
	// to notify all the proxies that a chunk of data was removed - or use a sparse array (undesireable)
	Chaos::FPhysicsSolver::FParticlesType& Particles = GetSolver<FSolver>()->GetRigidParticles();

	// #BG TODO Special case here because right now we reset/realloc the evolution per geom component
	// in endplay which clears this out. That needs to not happen and be based on world shutdown
	if(Particles.Size() == 0)
	{
		return;
	}

	const int32 Begin = BaseParticleIndex;
	const int32 Count = NumParticles;

	if (ensure((int32)Particles.Size() > 0 && (Begin + Count) <= (int32)Particles.Size()))
	{
		for (int32 ParticleIndex = 0; ParticleIndex < Count; ++ParticleIndex)
		{
			GetSolver<FSolver>()->GetEvolution()->DisableParticle(Begin + ParticleIndex);
			GetSolver<FSolver>()->GetRigidClustering().GetTopLevelClusterParents().Remove(Begin + ParticleIndex);
		}
	}
#endif
}

void FGeometryCollectionPhysicsProxy::SyncBeforeDestroy()
{

}

void FGeometryCollectionPhysicsProxy::BufferGameState() 
{
	//
	// There is currently no per advance updates to the GeometryCollection
	//
}


void FGeometryCollectionPhysicsProxy::SetWorldTransform_External(const FTransform& WorldTransform)
{
	check(IsInGameThread());
	GameThreadPerFrameData.SetWorldTransform(WorldTransform);

	if (Chaos::FPhysicsSolver* RBDSolver = GetSolver<Chaos::FPhysicsSolver>())
	{
		RBDSolver->AddDirtyProxy(this);
	}
}

void FGeometryCollectionPhysicsProxy::PushStateOnGameThread(Chaos::FPBDRigidsSolver* InSolver)
{
	// CONTEXT: GAMETHREAD
	// this is running on GAMETHREAD before the PhysicsThread code runs for this frame
	bIsPhysicsThreadWorldTransformDirty = GameThreadPerFrameData.GetIsWorldTransformDirty();
	if (bIsPhysicsThreadWorldTransformDirty)
	{
		Parameters.WorldTransform = GameThreadPerFrameData.GetWorldTransform();
		GameThreadPerFrameData.ResetIsWorldTransformDirty();
	}

	bIsCollisionFilterDataDirty = GameThreadPerFrameData.GetIsCollisionFilterDataDirty();
	if (bIsCollisionFilterDataDirty)
	{
		Parameters.QueryFilterData = GameThreadPerFrameData.GetQueryFilter();
		Parameters.SimulationFilterData = GameThreadPerFrameData.GetSimFilter();
		GameThreadPerFrameData.ResetIsCollisionFilterDataDirty();
	}
}

void FGeometryCollectionPhysicsProxy::PushToPhysicsState()
{
	using namespace Chaos;
	// CONTEXT: PHYSICSTHREAD
	// because the attached actor can be dynamic, we need to update the kinematic particles properly
	if (bIsPhysicsThreadWorldTransformDirty || bIsCollisionFilterDataDirty)
	{
		const FTransform& ActorToWorld = Parameters.WorldTransform;

		// used to avoid doing the work twice if we have a internalCluster parent 
		bool InternalClusterParentUpdated = false;

		int32 NumTransformGroupElements = PhysicsThreadCollection.NumElements(FGeometryCollection::TransformGroup);
		for (int32 TransformGroupIndex = 0; TransformGroupIndex < NumTransformGroupElements; ++TransformGroupIndex)
		{
			Chaos::FPBDRigidClusteredParticleHandle* Handle = SolverParticleHandles[TransformGroupIndex];
			if (Handle)
			{
				// Must update our filters before updating an internal cluster parent
				if (bIsCollisionFilterDataDirty)
				{
					const Chaos::FShapesArray& ShapesArray = Handle->ShapesArray();
					for (const TUniquePtr<Chaos::FPerShapeData>& Shape : ShapesArray)
					{
						Shape->SetQueryData(Parameters.QueryFilterData);
						Shape->SetSimData(Parameters.SimulationFilterData);
					}
				}

				// in the case of cluster union we need to find our Internal Cluster parent and update it
				if (!InternalClusterParentUpdated)
				{
					FClusterHandle* ParentHandle = Handle->Parent();
					if (ParentHandle && ParentHandle->InternalCluster())
					{
						if (bIsPhysicsThreadWorldTransformDirty && Handle->ObjectState() == Chaos::EObjectStateType::Kinematic && ParentHandle->ObjectState() == Chaos::EObjectStateType::Kinematic && !ParentHandle->Disabled())
						{
							FTransform NewChildWorldTransform = PhysicsThreadCollection.MassToLocal[TransformGroupIndex] * PhysicsThreadCollection.Transform[TransformGroupIndex] * ActorToWorld;
							Chaos::FRigidTransform3 ParentToChildTransform = Handle->ChildToParent().Inverse();
							FTransform NewParentWorldTRansform = ParentToChildTransform * NewChildWorldTransform;
							SetClusteredParticleKinematicTarget_Internal(ParentHandle, NewParentWorldTRansform);
						}

						if (bIsCollisionFilterDataDirty)
						{
							if(Chaos::FPhysicsSolver* RigidSolver = GetSolver<Chaos::FPhysicsSolver>())
							{
								FRigidClustering& RigidClustering = RigidSolver->GetEvolution()->GetRigidClustering();
								FRigidClustering::FRigidHandleArray* ChildArray = RigidClustering.GetChildrenMap().Find(ParentHandle);
								if (ensure(ChildArray))
								{
									// If our filter changed, internal cluster parent's filter may be stale.
									UpdateClusterFilterDataFromChildren(ParentHandle, *ChildArray);
								}

							}
						}


						InternalClusterParentUpdated = true;
					}
				}

				if (bIsPhysicsThreadWorldTransformDirty && !Handle->Disabled() && Handle->ObjectState() == Chaos::EObjectStateType::Kinematic)
				{
					FTransform WorldTransform = PhysicsThreadCollection.MassToLocal[TransformGroupIndex] * PhysicsThreadCollection.Transform[TransformGroupIndex] * ActorToWorld;
					SetClusteredParticleKinematicTarget_Internal(Handle, WorldTransform);
				}
			}
		}
	}
}

void FGeometryCollectionPhysicsProxy::SetClusteredParticleKinematicTarget_Internal(Chaos::FPBDRigidClusteredParticleHandle* Handle, const FTransform& NewWorldTransform)
{
	// CONTEXT: PHYSICSTHREAD
	// this should be called only on the physics thread
	const Chaos::EObjectStateType ObjectState = Handle->ObjectState();
	if (ensure(ObjectState == Chaos::EObjectStateType::Kinematic))
	{
		Chaos::TKinematicTarget<Chaos::FReal, 3> NewKinematicTarget;
		NewKinematicTarget.SetTargetMode(NewWorldTransform);

		if (Chaos::FPhysicsSolver* RBDSolver = GetSolver<Chaos::FPhysicsSolver>())
		{
			RBDSolver->GetEvolution()->SetParticleKinematicTarget(Handle, NewKinematicTarget);
			RBDSolver->GetEvolution()->DirtyParticle(*Handle);
		}
	}
}

static EObjectStateTypeEnum GetObjectStateFromHandle(const Chaos::TPBDRigidClusteredParticleHandle<Chaos::FReal, 3>* Handle)
{
	if (!Handle->Sleeping())
	{
		switch (Handle->ObjectState())
		{
		case Chaos::EObjectStateType::Kinematic:
			return EObjectStateTypeEnum::Chaos_Object_Kinematic;
		case Chaos::EObjectStateType::Static:
			return EObjectStateTypeEnum::Chaos_Object_Static;
		case Chaos::EObjectStateType::Sleeping:
			return EObjectStateTypeEnum::Chaos_Object_Sleeping;
		case Chaos::EObjectStateType::Dynamic:
		case Chaos::EObjectStateType::Uninitialized:
		default:
			return EObjectStateTypeEnum::Chaos_Object_Dynamic;
		}
	}
	return EObjectStateTypeEnum::Chaos_Object_Sleeping;
}

void FGeometryCollectionPhysicsProxy::PrepareBufferData(Chaos::FDirtyGeometryCollectionData& BufferData, const FGeometryDynamicCollection& ThreadCollection,  Chaos::FReal SolverLastDt)
{
	/**
	* CONTEXT: GAMETHREAD or PHYSICS THREAD
	* this method needs to be careful of data access , ultimately only the this pointer can be passed to the BufferData 
	*/
	BufferData.SetProxy(*this);

	IsObjectDynamic = false;
	FGeometryCollectionResults& TargetResults = BufferData.Results;
	TargetResults.SolverDt = SolverLastDt;

	const int32 NumTransformGroupElements = ThreadCollection.NumElements(FGeometryCollection::TransformGroup);
	if (TargetResults.NumTransformGroup() != NumTransformGroupElements)
	{
		TargetResults.InitArrays(ThreadCollection);
	}
}

void FGeometryCollectionPhysicsProxy::BufferPhysicsResults_External(Chaos::FDirtyGeometryCollectionData& BufferData)
{
	/**
	* CONTEXT: GAMETHREAD
	* Called per-tick when async is on after the simulation has completed.
	* goal is collect current game thread data of the proxy so it can be used if no previous physics thread data is available for interpolating  
	*/
	//SCOPE_CYCLE_COUNTER(STAT_GeometryCollection_BufferPhysicsResults_External);
	if (IsObjectDeleting) return;
	PrepareBufferData(BufferData, GameThreadCollection);
	
	FGeometryCollectionResults& TargetResults = BufferData.Results;
	//TargetResults.SolverDt = CurrentSolver->GetLastDt();	//todo: should this use timestamp for async mode?

	// not interpolatable, so we don't fill it 
	//	TargetResults.DisabledStates;
	//	TargetResults.Parent;
	//
	//TargetResults.DynamicState; // ObjectState
	const TManagedArray<FVector3f>* LinearVelocities = GameThreadCollection.FindAttributeTyped<FVector3f>("LinearVelocity", FTransformCollection::TransformGroup);
	const TManagedArray<FVector3f>* AngularVelocities = GameThreadCollection.FindAttributeTyped<FVector3f>("AngularVelocity", FTransformCollection::TransformGroup);
	
	for (int32 TransformGroupIndex = 0; TransformGroupIndex < TargetResults.Transforms.Num(); ++TransformGroupIndex)
	{
		const Chaos::FGeometryParticle& GTParticle = *GTParticles[TransformGroupIndex];
		TargetResults.ParticleXs[TransformGroupIndex] = GTParticle.X();
		TargetResults.ParticleRs[TransformGroupIndex] = GTParticle.R();

		if (LinearVelocities && AngularVelocities)
		{
			TargetResults.ParticleVs[TransformGroupIndex] = Chaos::FVec3((*LinearVelocities)[TransformGroupIndex]);
			TargetResults.ParticleWs[TransformGroupIndex] = Chaos::FVec3((*AngularVelocities)[TransformGroupIndex]);
		}

		TargetResults.Parent[TransformGroupIndex] = GameThreadCollection.Parent[TransformGroupIndex];
		TargetResults.Transforms[TransformGroupIndex] = GameThreadCollection.Transform[TransformGroupIndex];

		TargetResults.InternalClusterUniqueIdx[TransformGroupIndex] = INDEX_NONE;
		
#if WITH_EDITORONLY_DATA
		TargetResults.DamageInfo[TransformGroupIndex].Damage = 0.0f;
		TargetResults.DamageInfo[TransformGroupIndex].DamageThreshold = 0.0f;
#endif
	}
	
	// TargetResults.GlobalTransforms; // no need, can be recomputed ?  
	//
	// TargetResults.IsObjectDynamic;
	// TargetResults.IsObjectLoading;
}

static void UpdateParticleHandleTransform(Chaos::FPBDRigidsSolver& CurrentSolver, Chaos::FPBDRigidClusteredParticleHandle& Handle, const Chaos::FRigidTransform3& NewTransform)
{
	Handle.X() = NewTransform.GetTranslation();
	Handle.R() = NewTransform.GetRotation();
	Handle.UpdateWorldSpaceState(NewTransform, Chaos::FVec3{0});
	CurrentSolver.GetEvolution()->DirtyParticle(Handle);
}

static void UpdateParticleHandleTransformIfNeeded(Chaos::FPBDRigidsSolver& CurrentSolver, Chaos::FPBDRigidClusteredParticleHandle& Handle, const Chaos::FRigidTransform3& NewTransform)
{
	const Chaos::FVec3 NewX = NewTransform.GetTranslation();
	const Chaos::FVec3 OldX = Handle.X();
	
	const Chaos::FRotation3 NewR  = NewTransform.GetRotation();
	const Chaos::FRotation3 OldR = Handle.R();
	
	if (OldX != NewX || OldR != NewR)
	{
		UpdateParticleHandleTransform(CurrentSolver, Handle, NewTransform);
	}
}



void FGeometryCollectionPhysicsProxy::BufferPhysicsResults_Internal(Chaos::FPBDRigidsSolver* CurrentSolver, Chaos::FDirtyGeometryCollectionData& BufferData)
{
	/**
	 * CONTEXT: PHYSICSTHREAD
	 * Called per-tick after the simulation has completed. The proxy should cache the results of their
	 * simulation into the local buffer. 
	 */
	using namespace Chaos;
	SCOPE_CYCLE_COUNTER(STAT_CacheResultGeomCollection);
	if (IsObjectDeleting) return;

	//todo: should this use timestamp instead of GetLastDt for async mode?
	PrepareBufferData(BufferData, PhysicsThreadCollection, CurrentSolver->GetLastDt());

	IsObjectDynamic = false;
	FGeometryCollectionResults& TargetResults = BufferData.Results;

	const FTransform& ActorToWorld = Parameters.WorldTransform;
	const TManagedArray<int32>& Parent = PhysicsThreadCollection.Parent;
	const TManagedArray<TSet<int32>>& Children = PhysicsThreadCollection.Children;
	const bool IsActorScaled = !ActorToWorld.GetScale3D().Equals(FVector::OneVector);
	const FTransform ActorScaleTransform(FQuat::Identity,  FVector::ZeroVector, ActorToWorld.GetScale3D());

	UniqueIdxToInternalClusterHandle.Reset();
	
	const int32 NumTransformGroupElements = TargetResults.Transforms.Num(); 
	if(NumTransformGroupElements > 0)
	{ 
		SCOPE_CYCLE_COUNTER(STAT_CalcParticleToWorld);

		for (int32 TransformGroupIndex = 0; TransformGroupIndex < NumTransformGroupElements; ++TransformGroupIndex)
		{
			TargetResults.Transforms[TransformGroupIndex] = PhysicsThreadCollection.Transform[TransformGroupIndex];
			TargetResults.Parent[TransformGroupIndex] = PhysicsThreadCollection.Parent[TransformGroupIndex];
			TargetResults.InternalClusterUniqueIdx[TransformGroupIndex] = INDEX_NONE;

			TargetResults.States[TransformGroupIndex].DisabledState = true;
			TargetResults.States[TransformGroupIndex].HasInternalClusterParent = false;
			TargetResults.States[TransformGroupIndex].DynamicInternalClusterParent = false;
			Chaos::FPBDRigidClusteredParticleHandle* Handle = SolverParticleHandles[TransformGroupIndex];
			if (!Handle)
			{
				PhysicsThreadCollection.Active[TransformGroupIndex] = !TargetResults.States[TransformGroupIndex].DisabledState;
				continue;
			}

			// Dynamic state is also updated by the solver during field interaction.
			TargetResults.States[TransformGroupIndex].DynamicState = static_cast<int8>(GetObjectStateFromHandle(Handle));

			// Update the transform and parent hierarchy of the active rigid bodies. Active bodies can be either
			// rigid geometry defined from the leaf nodes of the collection, or cluster bodies that drive an entire
			// branch of the hierarchy within the GeometryCollection.
			// - Active bodies are directly driven from the global position of the corresponding
			//   rigid bodies within the solver ( cases where RigidBodyID[TransformGroupIndex] is not disabled ). 
			// - Deactivated bodies are driven from the transforms of their active parents. However the solver can
			//   take ownership of the parents during the simulation, so it might be necessary to force deactivated
			//   bodies out of the collections hierarchy during the simulation.  
			if (!Handle->Disabled())
			{
				// Update the transform of the active body. The active body can be either a single rigid
				// or a collection of rigidly attached geometries (Clustering). The cluster is represented as a
				// single transform in the GeometryCollection, and all children are stored in the local space
				// of the parent cluster.

				TargetResults.ParticleXs[TransformGroupIndex] = Handle->X();
				TargetResults.ParticleRs[TransformGroupIndex] = Handle->R();
				TargetResults.ParticleVs[TransformGroupIndex] = Handle->V();
				TargetResults.ParticleWs[TransformGroupIndex] = Handle->W();
				FRigidTransform3 ParticleToWorld(Handle->X(), Handle->R());
				const FTransform MassToLocal = PhysicsThreadCollection.MassToLocal[TransformGroupIndex];

				TargetResults.Transforms[TransformGroupIndex] = MassToLocal.GetRelativeTransformReverse(ParticleToWorld).GetRelativeTransform(ActorToWorld);
				TargetResults.Transforms[TransformGroupIndex].NormalizeRotation();
				if (IsActorScaled)
				{
					TargetResults.Transforms[TransformGroupIndex] = MassToLocal.Inverse() * ActorScaleTransform * MassToLocal * TargetResults.Transforms[TransformGroupIndex];
				}

				PhysicsThreadCollection.Transform[TransformGroupIndex] = TargetResults.Transforms[TransformGroupIndex];

				// Indicate that this object needs to be updated and the proxy is active.
				TargetResults.States[TransformGroupIndex].DisabledState = false;
				TargetResults.States[TransformGroupIndex].HasInternalClusterParent = false;
				TargetResults.States[TransformGroupIndex].DynamicInternalClusterParent = false;
				IsObjectDynamic = true;

				// If the parent of this NON DISABLED body is set to anything other than INDEX_NONE,
				// then it was just unparented, likely either by rigid clustering or by fields.  We
				// need to force all such enabled rigid bodies out of the transform hierarchy.
				TargetResults.Parent[TransformGroupIndex] = INDEX_NONE;
				if (PhysicsThreadCollection.Parent[TransformGroupIndex] != INDEX_NONE)
				{
					//GeometryCollectionAlgo::UnparentTransform(&PhysicsThreadCollection,TransformGroupIndex);
					PhysicsThreadCollection.Children[PhysicsThreadCollection.Parent[TransformGroupIndex]].Remove(TransformGroupIndex);
					PhysicsThreadCollection.Parent[TransformGroupIndex] = INDEX_NONE;
				}

				// When a leaf node rigid body is removed from a cluster, the rigid
				// body will become active and needs its clusterID updated.  This just
				// syncs the clusterID all the time.
				TPBDRigidParticleHandle<Chaos::FReal, 3>* ClusterParentId = Handle->ClusterIds().Id;
				SolverClusterID[TransformGroupIndex] = ClusterParentId;
			}
			else    // Handle->Disabled()
			{
				// The rigid body parent cluster has changed within the solver, and its
				// parent body is not tracked within the geometry collection. So we need to
				// pull the rigid bodies out of the transform hierarchy, and just drive
				// the positions directly from the solvers cluster particle.
				if(TPBDRigidParticleHandle<Chaos::FReal, 3>* ClusterParentBase = Handle->ClusterIds().Id)
				{
					if(Chaos::FPBDRigidClusteredParticleHandle* ClusterParent = ClusterParentBase->CastToClustered())
					{
						// syncronize parents if it has changed.
						if(SolverClusterID[TransformGroupIndex] != ClusterParent)
						{
							// Force all driven rigid bodies out of the transform hierarchy
							if(Parent[TransformGroupIndex] != INDEX_NONE)
							{
								// If the parent of this NON DISABLED body is set to anything other than INDEX_NONE,
								// then it was just unparented, likely either by rigid clustering or by fields.  We
								// need to force all such enabled rigid bodies out of the transform hierarchy.
								TargetResults.Parent[TransformGroupIndex] = INDEX_NONE;

								// GeometryCollectionAlgo::UnparentTransform(&PhysicsThreadCollection, ChildIndex);
								PhysicsThreadCollection.Children[PhysicsThreadCollection.Parent[TransformGroupIndex]].Remove(TransformGroupIndex);
								PhysicsThreadCollection.Parent[TransformGroupIndex] = INDEX_NONE;

								// Indicate that this object needs to be updated and the proxy is active.
								TargetResults.States[TransformGroupIndex].DisabledState = false;
								IsObjectDynamic = true;
							}
							SolverClusterID[TransformGroupIndex] = Handle->ClusterIds().Id;
						}

						if(ClusterParent->InternalCluster())
						{
							const int32 InternalClusterParentUniqueIdx = ClusterParent->UniqueIdx().Idx;
							if (!UniqueIdxToInternalClusterHandle.Contains(InternalClusterParentUniqueIdx))
							{
								UniqueIdxToInternalClusterHandle.Add(InternalClusterParentUniqueIdx, ClusterParent);
							}
							TargetResults.InternalClusterUniqueIdx[TransformGroupIndex] = ClusterParent->UniqueIdx().Idx;
							
							const FTransform ParticleToWorld = Handle->ChildToParent() * FRigidTransform3(ClusterParent->X(), ClusterParent->R());    // aka ClusterChildToWorld
							TargetResults.ParticleXs[TransformGroupIndex] = ParticleToWorld.GetTranslation();
							TargetResults.ParticleRs[TransformGroupIndex] = ParticleToWorld.GetRotation();
							TargetResults.ParticleVs[TransformGroupIndex] = ClusterParent->V();
							TargetResults.ParticleWs[TransformGroupIndex] = ClusterParent->W();

							// GeomToActor = ActorToWorld.Inv() * ClusterChildToWorld * MassToLocal.Inv();
							const FTransform MassToLocal                  = PhysicsThreadCollection.MassToLocal[TransformGroupIndex];
							TargetResults.Transforms[TransformGroupIndex] = MassToLocal.GetRelativeTransformReverse(ParticleToWorld).GetRelativeTransform(ActorToWorld);
							TargetResults.Transforms[TransformGroupIndex].NormalizeRotation();
							if (IsActorScaled)
							{
								TargetResults.Transforms[TransformGroupIndex] = MassToLocal.Inverse() * ActorScaleTransform * MassToLocal * TargetResults.Transforms[TransformGroupIndex];
							}

							PhysicsThreadCollection.Transform[TransformGroupIndex] = TargetResults.Transforms[TransformGroupIndex];

							// Indicate that this object needs to be updated and the proxy is active.
							TargetResults.States[TransformGroupIndex].DisabledState = false;
							TargetResults.States[TransformGroupIndex].HasInternalClusterParent = true;
							TargetResults.States[TransformGroupIndex].DynamicInternalClusterParent = (ClusterParent->IsDynamic());
							IsObjectDynamic = true;

							// as we just transitioned from disabled to non disabled the update is unconditional 
							UpdateParticleHandleTransform(*CurrentSolver, *Handle, ParticleToWorld);
						}

						if (bGeometryCollectionEnabledNestedChildTransformUpdates)
						{
							if (!ClusterParent->Disabled())
							{
								const FRigidTransform3 ChildToWorld = Handle->ChildToParent() * FRigidTransform3(ClusterParent->X(), ClusterParent->R());
								UpdateParticleHandleTransformIfNeeded(*CurrentSolver, *Handle, ChildToWorld);
								// fields may have applied velocities, we need to make sure to clear that up, so that we don't accumulate
								Handle->SetV(FVec3::Zero());
								Handle->SetW(FVec3::Zero());
							}
						}
					}
				}
			}    // end if

			PhysicsThreadCollection.Active[TransformGroupIndex] = !TargetResults.States[TransformGroupIndex].DisabledState;

#if WITH_EDITORONLY_DATA
			FGeometryCollectionResults::FDamageInfo& DamageInfo = TargetResults.DamageInfo[TransformGroupIndex];
			DamageInfo.Damage = static_cast<float>(Handle->CollisionImpulse());
			DamageInfo.DamageThreshold = static_cast<float>(Handle->Strain());
#endif			
		}    // end for
	}        // STAT_CalcParticleToWorld scope
	
	// If object is dynamic, compute global matrices	
	if (IsObjectDynamic || TargetResults.GlobalTransforms.Num() == 0)
	{
		SCOPE_CYCLE_COUNTER(STAT_CalcGlobalGCMatrices);
		check(TargetResults.Transforms.Num() == TargetResults.Parent.Num());
		GeometryCollectionAlgo::GlobalMatrices(TargetResults.Transforms, TargetResults.Parent, TargetResults.GlobalTransforms);
	}
	

	// Advertise to game thread
	TargetResults.IsObjectDynamic = IsObjectDynamic;
	TargetResults.IsObjectLoading = IsObjectLoading;
}

void FGeometryCollectionPhysicsProxy::FlipBuffer()
{
	/**
	 * CONTEXT: PHYSICSTHREAD (Write Locked)
	 * Called by the physics thread to signal that it is safe to perform any double-buffer flips here.
	 * The physics thread has pre-locked an RW lock for this operation so the game thread won't be reading
	 * the data
	 */

	PhysToGameInterchange.FlipProducer();
}

// Called from FPhysScene_ChaosInterface::SyncBodies(), NOT the solver.
bool FGeometryCollectionPhysicsProxy::PullFromPhysicsState(const Chaos::FDirtyGeometryCollectionData& PullData, const int32 SolverSyncTimestamp, const Chaos::FDirtyGeometryCollectionData* NextPullData, const Chaos::FRealSingle* Alpha)
{
	if(IsObjectDeleting) return false;

	/**
	 * CONTEXT: GAMETHREAD (Read Locked)
	 * Perform a similar operation to Sync, but take the data from a gamethread-safe buffer. This will be called
	 * from the game thread when it cannot sync to the physics thread. The simulation is very likely to be running
	 * when this happens so never read any physics thread data here!
	 *
	 * Note: A read lock will have been acquired for this - so the physics thread won't force a buffer flip while this
	 * sync is ongoing
	 *
	 */

	const FGeometryCollectionResults& TargetResults = PullData.Results;

	TManagedArray<FVector3f>* LinearVelocities = GameThreadCollection.FindAttributeTyped<FVector3f>("LinearVelocity", FTransformCollection::TransformGroup);
	TManagedArray<FVector3f>* AngularVelocities = GameThreadCollection.FindAttributeTyped<FVector3f>("AngularVelocity", FTransformCollection::TransformGroup);
	TManagedArray<uint8>* InternalClusterParentTypeArray = GameThreadCollection.FindAttributeTyped<uint8>("InternalClusterParentTypeArray", FTransformCollection::TransformGroup);

	bool bIsCollectionDirty = false;
	
	// We should never be changing the number of entries, this would break other 
	// attributes in the transform group.
	const int32 NumTransforms = GameThreadCollection.Transform.Num();
	if (ensure(NumTransforms == TargetResults.Transforms.Num()))
	{
		// first : copy the non interpolate-able values
		for (int32 TransformGroupIndex = 0; TransformGroupIndex < NumTransforms; ++TransformGroupIndex)
		{
			const bool bIsActive = !TargetResults.States[TransformGroupIndex].DisabledState;
			if (GameThreadCollection.Active[TransformGroupIndex] != bIsActive)
			{
				GameThreadCollection.Active[TransformGroupIndex] = bIsActive;
				bIsCollectionDirty = true;
			}
			
			const int32 NewState = TargetResults.States[TransformGroupIndex].DynamicState;
			if (GameThreadCollection.DynamicState[TransformGroupIndex] != NewState)
			{
				GameThreadCollection.DynamicState[TransformGroupIndex] = NewState;
				bIsCollectionDirty = true;
			}

			if (bIsActive)
			{
				const int32 NewParent = TargetResults.Parent[TransformGroupIndex];
				if (GameThreadCollection.Parent[TransformGroupIndex] != NewParent)
				{
					GameThreadCollection.Parent[TransformGroupIndex] = NewParent;
					bIsCollectionDirty = true;
				}
				// if (NewState != (int32)EObjectStateTypeEnum::Chaos_Object_Static && NewState != (int32)EObjectStateTypeEnum::Chaos_Object_Sleeping)
				// {
				// 	bIsCollectionDirty = true;
				// }
			}

			if (InternalClusterParentTypeArray)
			{
				Chaos::EInternalClusterType ParentType = Chaos::EInternalClusterType::None;
				if (TargetResults.States[TransformGroupIndex].HasInternalClusterParent != 0)
				{
					ParentType = (TargetResults.States[TransformGroupIndex].DynamicInternalClusterParent != 0)? Chaos::EInternalClusterType::Dynamic: Chaos::EInternalClusterType::KinematicOrStatic; 
				}
				const uint8 ParentTypeUInt8 = static_cast<uint8>(ParentType);
				if ((*InternalClusterParentTypeArray)[TransformGroupIndex] != ParentTypeUInt8)
				{
					(*InternalClusterParentTypeArray)[TransformGroupIndex] = ParentTypeUInt8;
					bIsCollectionDirty = true;
				}
			}
		}

#if WITH_EDITORONLY_DATA
		if (FDamageCollector* Collector = FRuntimeDataCollector::GetInstance().Find(CollectorGuid))
		{
			for (int32 TransformGroupIndex = 0; TransformGroupIndex < NumTransforms; ++TransformGroupIndex)
			{
				const FGeometryCollectionResults::FDamageInfo& DamageInfo = TargetResults.DamageInfo[TransformGroupIndex]; 
				Collector->SampleDamage(TransformGroupIndex, DamageInfo.Damage, DamageInfo.DamageThreshold);
			}
		}
#endif
		
		// second : interpolate-able ones
		const bool bNeedInterpolation = (NextPullData!= nullptr);
		if (bNeedInterpolation)
		{
			const FGeometryCollectionResults& Prev = PullData.Results;
			const FGeometryCollectionResults& Next = NextPullData->Results;
			for (int32 TransformGroupIndex = 0; TransformGroupIndex < NumTransforms; ++TransformGroupIndex)
			{
				if (!TargetResults.States[TransformGroupIndex].DisabledState)
				{
					Chaos::FGeometryParticle& GTParticle = *GTParticles[TransformGroupIndex];

					const Chaos::FVec3 OldX = GTParticle.X();
					const Chaos::FVec3 NewX = FMath::Lerp(Prev.ParticleXs[TransformGroupIndex], Next.ParticleXs[TransformGroupIndex], *Alpha);
					const bool bNeedUpdateX = (NewX != OldX);
					if (bNeedUpdateX)
					{
						GTParticle.SetX(NewX, false);
					}
					
					const Chaos::FRotation3 OldR = GTParticle.R();
					const Chaos::FRotation3 NewR = FQuat::Slerp(Prev.ParticleRs[TransformGroupIndex], Next.ParticleRs[TransformGroupIndex], *Alpha);
					const bool bNeedUpdateR = (NewR != OldR); 
					if (bNeedUpdateR)
					{
						GTParticle.SetR(NewR, false);
					}

					if (bNeedUpdateX || bNeedUpdateR)
					{
						GTParticle.UpdateShapeBounds();
						bIsCollectionDirty = true;
					}

					
					// need to interpolate using MassToLocal
					const FTransform& MassToLocal = GameThreadCollection.MassToLocal[TransformGroupIndex];
					FTransform InterpolatedTransform;
					InterpolatedTransform.Blend(MassToLocal * Prev.Transforms[TransformGroupIndex], MassToLocal * Next.Transforms[TransformGroupIndex], *Alpha);
					GameThreadCollection.Transform[TransformGroupIndex] = MassToLocal.Inverse() * InterpolatedTransform;

					if(LinearVelocities && AngularVelocities)
					{
						// LWC : potential loss of precision if the velocity is insanely big ( unlikely ) 
						(*LinearVelocities)[TransformGroupIndex] = FVector3f(FMath::Lerp(Prev.ParticleVs[TransformGroupIndex], Next.ParticleVs[TransformGroupIndex], *Alpha)); 
						(*AngularVelocities)[TransformGroupIndex] = FVector3f(FMath::Lerp(Prev.ParticleWs[TransformGroupIndex], Next.ParticleWs[TransformGroupIndex], *Alpha));
					}
				}
			}
		}
		else
		{
			for (int32 TransformGroupIndex = 0; TransformGroupIndex < NumTransforms; ++TransformGroupIndex)
			{
				if (!TargetResults.States[TransformGroupIndex].DisabledState)
				{
					Chaos::FGeometryParticle& GTParticle = *GTParticles[TransformGroupIndex];

					const Chaos::FVec3 OldX = GTParticle.X();
					const Chaos::FVec3& NewX = TargetResults.ParticleXs[TransformGroupIndex];
					const bool bNeedUpdateX = (NewX != OldX);
					if (bNeedUpdateX)
					{
						GTParticle.SetX(NewX, false);
					}
					
					const Chaos::FRotation3 OldR = GTParticle.R();
					const Chaos::FRotation3& NewR = TargetResults.ParticleRs[TransformGroupIndex];
					const bool bNeedUpdateR = (NewR != OldR);
					if (bNeedUpdateR)
					{
						GTParticle.SetR(NewR, false);
					}

					if (bNeedUpdateX || bNeedUpdateR)
					{
						GTParticle.UpdateShapeBounds();
						bIsCollectionDirty = true;
					}

					GameThreadCollection.Transform[TransformGroupIndex] = TargetResults.Transforms[TransformGroupIndex];

					if(LinearVelocities && AngularVelocities)
					{
						// LWC : potential loss of precision if the velocity is insanely big ( unlikely ) 
						(*LinearVelocities)[TransformGroupIndex] = FVector3f(TargetResults.ParticleVs[TransformGroupIndex]); 
						(*AngularVelocities)[TransformGroupIndex] = FVector3f(TargetResults.ParticleWs[TransformGroupIndex]);
					}
				}
			}
		}

		// internal cluster index map update
		GTParticlesToInternalClusterUniqueIdx.Reset();
		InternalClusterUniqueIdxToChildrenTransformIndices.Reset();
		for (int32 TransformGroupIndex = 0; TransformGroupIndex < NumTransforms; ++TransformGroupIndex)
		{
			const int32 InternalClusterUniqueIdx = TargetResults.InternalClusterUniqueIdx[TransformGroupIndex]; 
			if (InternalClusterUniqueIdx > INDEX_NONE)
			{
				GTParticlesToInternalClusterUniqueIdx.Add(GTParticles[TransformGroupIndex].Get(), InternalClusterUniqueIdx);
				InternalClusterUniqueIdxToChildrenTransformIndices.FindOrAdd(InternalClusterUniqueIdx).Add(TransformGroupIndex); 
			}
		}

		//question: why do we need this? Sleeping objects will always have to update GPU
		if (bIsCollectionDirty)
		{
			GameThreadCollection.MakeDirty();
		}
	}

	if (PostPhysicsSyncCallback)
	{
		PostPhysicsSyncCallback();
	}
	
	return true;
}


void FGeometryCollectionPhysicsProxy::UpdateFilterData_External(const FCollisionFilterData& NewSimFilter, const FCollisionFilterData& NewQueryFilter)
{
	SCOPE_CYCLE_COUNTER(STAT_GCUpdateFilterData);
	check(IsInGameThread());

	// SimFilter/QueryFilter members are read on both threads, these are const after initialization and are not updated here.

	int32 NumTransforms = GameThreadCollection.Transform.Num();
	for (int32 Index = 0; Index < NumTransforms; ++Index)
	{
		Chaos::FGeometryParticle* P = GTParticles[Index].Get();
		const Chaos::FShapesArray& Shapes = P->ShapesArray();
		const int32 NumShapes = Shapes.Num();
		for (int32 ShapeIndex = 0; ShapeIndex < NumShapes; ++ShapeIndex)
		{
			Chaos::FPerShapeData* Shape = Shapes[ShapeIndex].Get();
			Shape->SetSimData(NewSimFilter);
			Shape->SetQueryData(NewQueryFilter);
		}
	}

	GameThreadPerFrameData.SetSimFilter(NewSimFilter);
	GameThreadPerFrameData.SetQueryFilter(NewQueryFilter);

	if (Chaos::FPhysicsSolver* RBDSolver = GetSolver<Chaos::FPhysicsSolver>())
	{
		RBDSolver->AddDirtyProxy(this);
	}
}

//==============================================================================
// STATIC SETUP FUNCTIONS
//==============================================================================
static const FSharedSimulationSizeSpecificData& GetSizeSpecificData(const TArray<FSharedSimulationSizeSpecificData>& SizeSpecificData, const FGeometryCollection& RestCollection, const int32 TransformIndex, const FBox& BoundingBox)
{
	// If we have a normalized Size available, use that to determine SizeSpecific index, otherwise fall back on Bounds volume.
	int32 SizeSpecificIdx;
	const TManagedArray<float>* RelativeSizes = RestCollection.FindAttribute<float>("Size", FTransformCollection::TransformGroup);
	if (RelativeSizes)
	{
		SizeSpecificIdx = GeometryCollection::SizeSpecific::FindIndexForVolume(SizeSpecificData, (*RelativeSizes)[TransformIndex]);
	}
	else
	{
		SizeSpecificIdx = GeometryCollection::SizeSpecific::FindIndexForVolume(SizeSpecificData, BoundingBox);
	}
	return SizeSpecificData[SizeSpecificIdx];
}

// compute inner and outer radii from geometry in parallel if the attribute value as not been set
static void GenerateInnerAndOuterRadiiIfNeeded(FGeometryCollection& RestCollection)
{
	const int32 NumGeometries = RestCollection.NumElements(FGeometryCollection::GeometryGroup);
	const TManagedArray<FVector3f>& Vertices = RestCollection.Vertex;

	ParallelFor(NumGeometries, [&](int32 GeometryIndex)
	{
		if (RestCollection.InnerRadius[GeometryIndex] == 0.0f || RestCollection.OuterRadius[GeometryIndex] == 0.0f)
		{
			GeometryCollection::ComputeInnerAndOuterRadiiFromGeometryVertices(
				Vertices,
				RestCollection.VertexStart[GeometryIndex],
				RestCollection.VertexCount[GeometryIndex],
				RestCollection.InnerRadius[GeometryIndex],
				RestCollection.OuterRadius[GeometryIndex]);
		}
	});
}

static FGeometryDynamicCollection::FSharedImplicit CreateImplicitGeometry(
	const FSharedSimulationSizeSpecificData& SizeSpecificData,
	const int32 TransformGroupIndex,
	const FGeometryCollection& RestCollection,
	const Chaos::FParticles& MassSpaceParticles,
	const Chaos::FTriangleMesh& TriMesh,
	const FBox& InstanceBoundingBox,
	const Chaos::FReal InnerRadius,
	const Chaos::FErrorReporter& ErrorReporter,
	const Chaos::FVec3* ClusterMaxChildBounds = nullptr
	)
{
	FGeometryDynamicCollection::FSharedImplicit NewImplicit;
	if (SizeSpecificData.CollisionShapesData.Num())
	{
		const TManagedArray<FTransform>& CollectionMassToLocal = RestCollection.GetAttribute<FTransform>("MassToLocal", FTransformCollection::TransformGroup);
		const TManagedArray<TSet<int32>>* TransformToConvexIndices = RestCollection.FindAttribute<TSet<int32>>("TransformToConvexIndices", FTransformCollection::TransformGroup);
		const TManagedArray<TUniquePtr<Chaos::FConvex>>* ConvexGeometry = RestCollection.FindAttribute<TUniquePtr<Chaos::FConvex>>("ConvexHull", "Convex");

		const FCollectionCollisionTypeData& CollisionTypeData = SizeSpecificData.CollisionShapesData[0]; 
		switch(CollisionTypeData.ImplicitType)
		{
		case EImplicitTypeEnum::Chaos_Implicit_LevelSet:
			{
				const FCollectionLevelSetData& LevelSetData = CollisionTypeData.LevelSetData; 
				int32 MinResolution = LevelSetData.MinLevelSetResolution;
				int32 MaxResolution = LevelSetData.MaxLevelSetResolution;
				if (ClusterMaxChildBounds)
				{
					const Chaos::FVec3 Scale = 2. * InstanceBoundingBox.GetExtent() / (*ClusterMaxChildBounds); // FBox's extents are 1/2 (Max - Min)
					const Chaos::FReal ScaleMax = Scale.GetAbsMax();
					const Chaos::FReal ScaleMin = Scale.GetAbsMin();

					Chaos::FReal MinResolutionReal = ScaleMin * (Chaos::FReal)LevelSetData.MinLevelSetResolution;
					MinResolutionReal = FMath::Clamp(MinResolutionReal,
						(Chaos::FReal)LevelSetData.MinLevelSetResolution,
						(Chaos::FReal)LevelSetData.MinClusterLevelSetResolution);

					Chaos::FReal MaxResolutionReal = ScaleMax * (Chaos::FReal)LevelSetData.MaxLevelSetResolution;
					MaxResolutionReal = FMath::Clamp(MaxResolutionReal,
						(Chaos::FReal)LevelSetData.MaxLevelSetResolution,
						(Chaos::FReal)LevelSetData.MaxClusterLevelSetResolution);

					MinResolution = FMath::FloorToInt32(MinResolutionReal);
					MaxResolution = FMath::FloorToInt32(MaxResolutionReal);
				}
				
				NewImplicit = FGeometryDynamicCollection::FSharedImplicit(
					FCollisionStructureManager::NewImplicitLevelset(
						ErrorReporter,
						MassSpaceParticles,
						TriMesh,
						InstanceBoundingBox,
						MinResolution,
						MaxResolution,
						CollisionTypeData.CollisionObjectReductionPercentage,
						CollisionTypeData.CollisionType));
				
				// Fall back on sphere if level set rasterization failed.
				if (!NewImplicit)
				{
					NewImplicit = FGeometryDynamicCollection::FSharedImplicit(
						FCollisionStructureManager::NewImplicitSphere(
						InnerRadius,
						CollisionTypeData.CollisionObjectReductionPercentage,
						CollisionTypeData.CollisionType));
				}
			}
			break;
				
		case EImplicitTypeEnum::Chaos_Implicit_Box:
			{
				NewImplicit = FGeometryDynamicCollection::FSharedImplicit(
					FCollisionStructureManager::NewImplicitBox(
						InstanceBoundingBox,
						CollisionTypeData.CollisionObjectReductionPercentage,
						CollisionTypeData.CollisionType));
			}
			break;
			
		case EImplicitTypeEnum::Chaos_Implicit_Sphere:
			{
				NewImplicit = FGeometryDynamicCollection::FSharedImplicit(
					FCollisionStructureManager::NewImplicitSphere(
						InnerRadius,
						CollisionTypeData.CollisionObjectReductionPercentage,
						CollisionTypeData.CollisionType));
			}
			break;
			
		case EImplicitTypeEnum::Chaos_Implicit_Convex:
			{
				if (ConvexGeometry && TransformToConvexIndices)
				{
					NewImplicit = FGeometryDynamicCollection::FSharedImplicit(
						FCollisionStructureManager::NewImplicitConvex(
							(*TransformToConvexIndices)[TransformGroupIndex].Array(),
							ConvexGeometry,
							CollisionTypeData.CollisionType,
							CollectionMassToLocal[TransformGroupIndex],
							CollisionTypeData.CollisionMarginFraction,
							CollisionTypeData.CollisionObjectReductionPercentage
						)
					);
				}
			}
			break;
			
		case EImplicitTypeEnum::Chaos_Implicit_Capsule:
			{
				NewImplicit = FGeometryDynamicCollection::FSharedImplicit(
					FCollisionStructureManager::NewImplicitCapsule(
						InstanceBoundingBox,
						CollisionTypeData.CollisionObjectReductionPercentage,
						CollisionTypeData.CollisionType));
			}
			break;
		
		case EImplicitTypeEnum::Chaos_Implicit_None:
			// nothing to do here 
			break;
			
		default:
			ensure(false); // unsupported implicit type!
			break;
		}
	}
	return NewImplicit;
}

static void ComputeGeometryVolumeAndCenterOfMass(
	const TArray<FVector3f>& Vertices,
	const Chaos::FTriangleMesh& TriMesh,
	const FBox& BoundingBox,
	const FSharedSimulationParameters& SharedParams, 
	Chaos::FReal& OutUncorrectedVolume,
	Chaos::FReal& OutVolume,
	Chaos::FVec3& OutCenterOfMass,
	bool& OutIsTooSmallGeometry
	)
{
	const float MinVolume = FMath::Max(UE_SMALL_NUMBER, SharedParams.MinimumVolumeClamp());
	const float MaxVolume = FMath::Max(MinVolume, SharedParams.MaximumVolumeClamp());

	OutUncorrectedVolume = 0.f;
	OutVolume = 0.f;
    OutCenterOfMass = FVector::ZeroVector;
	OutIsTooSmallGeometry = (BoundingBox.GetExtent().GetAbsMin() < MinVolume);
	
	if (!OutIsTooSmallGeometry)
	{
		float Volume;
		FVector3f CenterOfMass;
		Chaos::CalculateVolumeAndCenterOfMass(Vertices, TriMesh.GetElements(), Volume, CenterOfMass);
		OutVolume = Volume;
		OutUncorrectedVolume = Volume;
		OutCenterOfMass = CenterOfMass;
		
		if (OutUncorrectedVolume == 0)
		{
			Chaos::CalculateVolumeAndCenterOfMass(BoundingBox, OutVolume, OutCenterOfMass);
		}
		if (OutUncorrectedVolume < MinVolume)
		{
			// For rigid bodies outside of range just default to a clamped bounding box, and warn the user.
			OutVolume = MinVolume;
			OutCenterOfMass = BoundingBox.GetCenter();
		}
		else if (MaxVolume < OutUncorrectedVolume)
		{
			// For rigid bodies outside of range just default to a clamped bounding box, and warn the user
			OutVolume = MaxVolume;
			OutCenterOfMass = BoundingBox.GetCenter();
		}
	}
}

static void ComputeMassPropertyFromMesh(
	const int32 GeometryIndex,
	const FGeometryCollection& RestCollection,
	const FSharedSimulationParameters& SharedParams,
	TUniquePtr<Chaos::FTriangleMesh>& TriMesh,
	Chaos::FMassProperties& OutMassProperties,
	bool& bOutIsTooSmallGeometry
)
{
	// VerticesGroup
	const TManagedArray<FVector3f>& Vertex = RestCollection.Vertex;

	// FacesGroup
	const TManagedArray<bool>& Visible = RestCollection.Visible;
	const TManagedArray<FIntVector>& Indices = RestCollection.Indices;

	// GeometryGroup
	const TManagedArray<FBox>& BoundingBox = RestCollection.BoundingBox;
	const TManagedArray<int32>& FaceStart = RestCollection.FaceStart;
	const TManagedArray<int32>& FaceCount = RestCollection.FaceCount;

	
	TriMesh = CreateTriangleMesh(
		FaceStart[GeometryIndex],
		FaceCount[GeometryIndex],
		Visible,
		Indices);

	Chaos::FReal UncorrectedVolume = 0;
	
	bOutIsTooSmallGeometry = false;
	ComputeGeometryVolumeAndCenterOfMass(
		Vertex.GetConstArray(),
		*TriMesh,
		BoundingBox[GeometryIndex],
		SharedParams,
		UncorrectedVolume,
		OutMassProperties.Volume,
		OutMassProperties.CenterOfMass,
		bOutIsTooSmallGeometry
		);

	// now compute unit inertia ( assuming density of 1 )
	constexpr Chaos::FReal UnitDensityKGPerCM = 1;
	const bool bUseBoundingBoxInertia = (UncorrectedVolume == 0);  
	if (bUseBoundingBoxInertia)
	{
		OutMassProperties.CenterOfMass = BoundingBox[GeometryIndex].GetCenter();
		Chaos::CalculateInertiaAndRotationOfMass(BoundingBox[GeometryIndex], UnitDensityKGPerCM, OutMassProperties.InertiaTensor, OutMassProperties.RotationOfMass);
	}
	else
	{
		const FVector3f CenterOfMass{ OutMassProperties.CenterOfMass };
		Chaos::PMatrix<float,3,3> InertiaTensor;
		Chaos::TRotation<float,3> RotationOfMass;
		CalculateInertiaAndRotationOfMass(Vertex.GetConstArray(), TriMesh->GetSurfaceElements(), (float)UnitDensityKGPerCM, CenterOfMass, InertiaTensor, RotationOfMass);
		OutMassProperties.CenterOfMass = CenterOfMass;
		OutMassProperties.InertiaTensor = InertiaTensor;
		OutMassProperties.RotationOfMass = RotationOfMass;

		// scale the inertia tensor accordingly to compensate for the volume adjustment
		if (OutMassProperties.Volume != UncorrectedVolume)
		{
			const FVector::FReal InertiaScale = OutMassProperties.Volume / UncorrectedVolume;
			OutMassProperties.InertiaTensor = OutMassProperties.InertiaTensor.ApplyScale(InertiaScale);
		}
	}
}

// compute mass properties and trimesh in parallel
// ( non-simulatable particles will not compute the it  ) 
// also return if the geometry mesh is too small 
static void ComputeMassPropertiesAndTriMeshes(
	const FGeometryCollection& RestCollection,
	const FSharedSimulationParameters& SharedParams,
	TArray<TUniquePtr<Chaos::FTriangleMesh>>& OutTriangleMeshesArray,
	TArray<Chaos::FMassProperties>& MassPropertiesArray,
	TArray<bool>& IsTooSmallGeometryArray
	)
{
	using FImplicitGeom = FGeometryDynamicCollection::FSharedImplicit;
	
	const int32 NumGeometries = RestCollection.NumElements(FGeometryCollection::GeometryGroup);
	const TManagedArray<int32>& TransformIndex = RestCollection.TransformIndex;
	const TManagedArray<bool>& CollectionSimulatableParticles = RestCollection.GetAttribute<bool>(FGeometryCollection::SimulatableParticlesAttribute, FTransformCollection::TransformGroup);
	const TManagedArray<FImplicitGeom>* ExternaCollisions = RestCollection.FindAttribute<FImplicitGeom>("ExternalCollisions", FGeometryCollection::TransformGroup);
	const TManagedArray<int32>& SimulationType = RestCollection.SimulationType;

	ParallelFor(NumGeometries, [&](int32 GeometryIndex)
	{
		const int32 TransformGroupIndex = TransformIndex[GeometryIndex];
		if (CollectionSimulatableParticles[TransformGroupIndex])
		{
			// todo compute mesh here and pass it on 
			ComputeMassPropertyFromMesh(GeometryIndex, RestCollection, SharedParams, OutTriangleMeshesArray[GeometryIndex], MassPropertiesArray[GeometryIndex], IsTooSmallGeometryArray[GeometryIndex]);
			// todo : needs to be a better decision as we are still computing the inertia above to replace it ?( see comment above )
			if (SharedParams.bUseImportedCollisionImplicits && ExternaCollisions && (*ExternaCollisions)[TransformGroupIndex])
			{
				constexpr Chaos::FReal UnitDensityKGPerCM = 1;
				Chaos::CalculateMassPropertiesOfImplicitType(MassPropertiesArray[GeometryIndex], Chaos::FRigidTransform3::Identity, (*ExternaCollisions)[TransformGroupIndex].Get(), UnitDensityKGPerCM);
				IsTooSmallGeometryArray[GeometryIndex] = false;
			}
		}

	});
}

static TUniquePtr<Chaos::FImplicitObject> MakeTransformImplicitObject(const Chaos::FImplicitObject& ImplicitObject, const Chaos::FRigidTransform3& Transform)
{
	TUniquePtr<Chaos::FImplicitObject> ResultObject;
	
	// we cannot really put a transform on top a union, so we need to transform each member
	if (ImplicitObject.IsUnderlyingUnion())
	{
		 TArray<TUniquePtr<Chaos::FImplicitObject>> TransformedObjects;
		const Chaos::FImplicitObjectUnion& Union = static_cast<const Chaos::FImplicitObjectUnion&>(ImplicitObject);
		for (const TUniquePtr<Chaos::FImplicitObject>& Object: Union.GetObjects())
		{
			TransformedObjects.Add(MakeTransformImplicitObject(*Object, Transform));
		}
		ResultObject = MakeUnique<Chaos::FImplicitObjectUnion>(MoveTemp(TransformedObjects));
	}
	else if (ImplicitObject.GetType() == Chaos::ImplicitObjectType::Transformed)
	{
		const Chaos::TImplicitObjectTransformed<Chaos::FReal,3>& TransformObject = static_cast<const Chaos::TImplicitObjectTransformed<Chaos::FReal,3>&>(ImplicitObject);
		// we deep copy at this point as the transform is going to be handled at this level
		TUniquePtr<Chaos::FImplicitObject> TransformedObjectCopy =  TransformObject.GetTransformedObject()->DeepCopy();
		ResultObject = MakeUnique<Chaos::TImplicitObjectTransformed<Chaos::FReal,3>>(MoveTemp(TransformedObjectCopy),  TransformObject.GetTransform() * Transform);
	}
	else
	{
		// we deep copy at this point as the transform is going to be handled at this level
		TUniquePtr<Chaos::FImplicitObject> TransformedObjectCopy =  ImplicitObject.DeepCopy();
		ResultObject = MakeUnique<Chaos::TImplicitObjectTransformed<Chaos::FReal,3>>(MoveTemp(TransformedObjectCopy), Transform);
	}
	return ResultObject;
}

/** 
	NOTE - Making any changes to data stored on the rest collection below MUST be accompanied
	by a rotation of the DDC key in FDerivedDataGeometryCollectionCooker::GetVersionString
*/
void FGeometryCollectionPhysicsProxy::InitializeSharedCollisionStructures(
	Chaos::FErrorReporter& ErrorReporter,
	FGeometryCollection& RestCollection,
	const FSharedSimulationParameters& SharedParams)
{
	check(SharedParams.SizeSpecificData.Num());

	FString BaseErrorPrefix = ErrorReporter.GetPrefix();

	// fracture tools can create an empty GC before appending new geometry
	if (RestCollection.NumElements(FGeometryCollection::GeometryGroup) == 0)
	{
		return;
	}

	using namespace Chaos;

	// TransformGroup
	const TManagedArray<int32>& BoneMap = RestCollection.BoneMap;
	const TManagedArray<int32>& Parent = RestCollection.Parent;
	const TManagedArray<TSet<int32>>& Children = RestCollection.Children;
	const TManagedArray<int32>& SimulationType = RestCollection.SimulationType;
	TManagedArray<bool>& CollectionSimulatableParticles = RestCollection.ModifyAttribute<bool>(FGeometryCollection::SimulatableParticlesAttribute, FTransformCollection::TransformGroup);
	TManagedArray<FVector3f>& CollectionInertiaTensor = RestCollection.AddAttribute<FVector3f>(TEXT("InertiaTensor"), FTransformCollection::TransformGroup);
	TManagedArray<FRealSingle>& CollectionMass = RestCollection.AddAttribute<FRealSingle>(TEXT("Mass"), FTransformCollection::TransformGroup);
	TManagedArray<TUniquePtr<FSimplicial>>& CollectionSimplicials =	RestCollection.AddAttribute<TUniquePtr<FSimplicial>>(FGeometryDynamicCollection::SimplicialsAttribute, FTransformCollection::TransformGroup);

	TManagedArray<int32>& Levels = RestCollection.AddAttribute<int32>(TEXT("Level"), FTransformCollection::TransformGroup);

	RestCollection.RemoveAttribute(FGeometryDynamicCollection::ImplicitsAttribute, FTransformCollection::TransformGroup);
	TManagedArray<FGeometryDynamicCollection::FSharedImplicit>& CollectionImplicits = RestCollection.AddAttribute<FGeometryDynamicCollection::FSharedImplicit>(FGeometryDynamicCollection::ImplicitsAttribute, FTransformCollection::TransformGroup);

	bool bUseRelativeSize = RestCollection.HasAttribute(TEXT("Size"), FTransformCollection::TransformGroup);
	if (!bUseRelativeSize)
	{
		UE_LOG(LogChaos, Display, TEXT("Relative Size not found on Rest Collection. Using bounds volume for SizeSpecificData indexing instead."));
	}


	// @todo(chaos_transforms) : do we still use this?
	TManagedArray<FTransform>& CollectionMassToLocal = RestCollection.AddAttribute<FTransform>(TEXT("MassToLocal"), FTransformCollection::TransformGroup);
	FTransform IdentityXf(FQuat::Identity, FVector(0));
	IdentityXf.NormalizeRotation();
	CollectionMassToLocal.Fill(IdentityXf);

	// VerticesGroup
	const TManagedArray<FVector3f>& Vertex = RestCollection.Vertex;

	// FacesGroup
	const TManagedArray<bool>& Visible = RestCollection.Visible;
	const TManagedArray<FIntVector>& Indices = RestCollection.Indices;

	// GeometryGroup
	const TManagedArray<int32>& TransformIndex = RestCollection.TransformIndex;
	const TManagedArray<FBox>& BoundingBox = RestCollection.BoundingBox;
	TManagedArray<Chaos::FRealSingle>& InnerRadius = RestCollection.InnerRadius;
	TManagedArray<Chaos::FRealSingle>& OuterRadius = RestCollection.OuterRadius;
	const TManagedArray<int32>& VertexStart = RestCollection.VertexStart;
	const TManagedArray<int32>& VertexCount = RestCollection.VertexCount;
	const TManagedArray<int32>& FaceStart = RestCollection.FaceStart;
	const TManagedArray<int32>& FaceCount = RestCollection.FaceCount;
	
	TArray<FTransform> CollectionSpaceTransforms;
	{ // tmp scope
		const TManagedArray<FTransform>& HierarchyTransform = RestCollection.Transform;
		GeometryCollectionAlgo::GlobalMatrices(HierarchyTransform, Parent, CollectionSpaceTransforms);
	} // tmp scope

	const int32 NumTransforms = CollectionSpaceTransforms.Num();
	const int32 NumGeometries = RestCollection.NumElements(FGeometryCollection::GeometryGroup);

	TArray<TUniquePtr<FTriangleMesh>> TriangleMeshesArray;	//use to union trimeshes in cluster case
	TriangleMeshesArray.AddDefaulted(NumTransforms);

	TArray<FMassProperties> MassPropertiesArray;
	MassPropertiesArray.AddUninitialized(NumGeometries);

	// The geometry group has a set of transform indices that maps a geometry index
	// to a transform index, but only in the case where there is a 1-to-1 mapping 
	// between the two.  In the event where a geometry is instanced for multiple
	// transforms, the transform index on the geometry group should be INDEX_NONE.
	// Otherwise, iterating over the geometry group is a convenient way to iterate
	// over all the leaves of the hierarchy.
	check(!TransformIndex.Contains(INDEX_NONE)); // TODO: implement support for instanced bodies

	TArray<bool> IsTooSmallGeometryArray;
	IsTooSmallGeometryArray.Init(false, NumGeometries);

	GenerateInnerAndOuterRadiiIfNeeded(RestCollection);

	for (int32 TransformGroupIndex = 0; TransformGroupIndex < NumTransforms; TransformGroupIndex++)
	{
		if (SimulationType[TransformGroupIndex] == FGeometryCollection::ESimulationTypes::FST_None)
		{
			CollectionSimulatableParticles[TransformGroupIndex] = false;
		}
	}
	
	// compute mass properties in parallel
	ComputeMassPropertiesAndTriMeshes(RestCollection, SharedParams, TriangleMeshesArray, MassPropertiesArray, IsTooSmallGeometryArray);
	
	// compute total volume and whether we need to skip small geometry ( can't use parallelfors )
	bool bSkippedSmallGeometry = false;
	FReal TotalVolume = 0.f;
	for (int32 GeometryIndex = 0; GeometryIndex < NumGeometries; GeometryIndex++)
	{
		const int32 TransformGroupIndex = TransformIndex[GeometryIndex];
		if (CollectionSimulatableParticles[TransformGroupIndex])
		{
			TotalVolume += MassPropertiesArray[GeometryIndex].Volume;
		}
		// We skip very small geometry and log as a warning. To avoid log spamming, we wait
		// until we complete the loop before reporting the skips.
		if (IsTooSmallGeometryArray[GeometryIndex])
		{
			CollectionSimulatableParticles[TransformGroupIndex] = false;
			bSkippedSmallGeometry = true;
		}
	}

	// log any warning necessary
	if (bSkippedSmallGeometry)
	{
		UE_LOG(LogChaos, Warning, TEXT("Some geometry is too small to be simulated and has been skipped."));
	}

	using FImplicitGeomSharePtr = FGeometryDynamicCollection::FSharedImplicit;
	const TManagedArray<FImplicitGeomSharePtr>* ExternaCollisions = RestCollection.FindAttribute<FImplicitGeomSharePtr>("ExternalCollisions", FGeometryCollection::TransformGroup);
	
	// User provides us with total mass or density.
	// Density must be the same for individual parts and the total. Density_i = Density = Mass_i / Volume_i
	// Total mass must equal sum of individual parts. Mass_i = TotalMass * Volume_i / TotalVolume => Density_i = TotalMass / TotalVolume
	TotalVolume = FMath::Max(TotalVolume, SharedParams.MinimumVolumeClamp());
	const FReal DesiredTotalMass = SharedParams.bMassAsDensity ? SharedParams.Mass * TotalVolume : SharedParams.Mass;
	const FReal ClampedTotalMass = FMath::Clamp(DesiredTotalMass, SharedParams.MinimumMassClamp, SharedParams.MaximumMassClamp);
	const FReal DesiredDensity = ClampedTotalMass / TotalVolume;

	ParallelFor(NumGeometries, [&](int32 GeometryIndex)
	{
		// Need a new error reporter for parallel for loop here as it wouldn't be thread-safe to write to the prefix
		Chaos::FErrorReporter LocalErrorReporter;
		const int32 TransformGroupIndex = TransformIndex[GeometryIndex];

		const FReal Volume_i = MassPropertiesArray[GeometryIndex].Volume;
		if (CollectionSimulatableParticles[TransformGroupIndex])
		{
			// Must clamp each individual mass regardless of desired density
			if (DesiredDensity * Volume_i > SharedParams.MaximumMassClamp)
			{
				// For rigid bodies outside of range just defaut to a clamped bounding box, and warn the user.
				LocalErrorReporter.ReportError(*FString::Printf(TEXT("Geometry has invalid mass (too large)")));
				LocalErrorReporter.HandleLatestError();

				CollectionSimulatableParticles[TransformGroupIndex] = false;
			}
		}

		// compute the rest of the mass property ( Mass, InertiaTensor and RotationOfMass )
		if (CollectionSimulatableParticles[TransformGroupIndex])
		{
			TUniquePtr<FTriangleMesh>& TriMesh = TriangleMeshesArray[TransformGroupIndex];
			FMassProperties& MassProperties = MassPropertiesArray[GeometryIndex];

			const FReal Mass_i = FMath::Max(DesiredDensity * Volume_i, SharedParams.MinimumMassClamp);
			const FReal Density_i = Mass_i / Volume_i;
			CollectionMass[TransformGroupIndex] = (FRealSingle)Mass_i;

			// scale the mass properties inertia tensor by the Density_i ( as we computedit earlier with a density of one )
			MassPropertiesArray[GeometryIndex].InertiaTensor *= Density_i;
			const Chaos::FMatrix33& InertiaTensor = MassPropertiesArray[GeometryIndex].InertiaTensor;
			CollectionInertiaTensor[TransformGroupIndex] = FVector3f((float)InertiaTensor.M[0][0], (float)InertiaTensor.M[1][1], (float)InertiaTensor.M[2][2]);

			CollectionMassToLocal[TransformGroupIndex] = FTransform(MassProperties.RotationOfMass, MassProperties.CenterOfMass);
		}
	});

	// we need the vertices in mass space to compute the collision particles
	// todo(chaos) we shoudl eventually get rid of that that's a waste of memory and performance
	FParticles MassSpaceParticles;
	MassSpaceParticles.AddParticles(Vertex.Num());
	for (int32 Idx = 0; Idx < Vertex.Num(); ++Idx)
	{
		MassSpaceParticles.X(Idx) = Vertex[Idx];	//mass space computation done later down
	}
	
	// compute the bounding box in mesh space
	FVec3 MaxChildBounds(1);
	ParallelFor(NumGeometries, [&](int32 GeometryIndex)
	{
		const int32 TransformGroupIndex = TransformIndex[GeometryIndex];
		if (CollectionSimulatableParticles[TransformGroupIndex])
		{
			const TUniquePtr<Chaos::FTriangleMesh>& TriMesh = TriangleMeshesArray[GeometryIndex];
			const FTransform& MassToLocalTransform = CollectionMassToLocal[TransformGroupIndex];

			// Orient particle in the local mass space
			const bool bIdentityMassTransform = MassToLocalTransform.Equals(FTransform::Identity);
			if (!bIdentityMassTransform)
			{
				const int32 IdxStart = VertexStart[GeometryIndex];
				const int32 IdxEnd = IdxStart + VertexCount[GeometryIndex];
				for (int32 Idx = IdxStart; Idx < IdxEnd; ++Idx)
				{
					MassSpaceParticles.X(Idx) = MassToLocalTransform.InverseTransformPositionNoScale(MassSpaceParticles.X(Idx));
				}
			}

			FBox InstanceBoundingBox(EForceInit::ForceInitToZero);
			if (TriMesh->GetElements().Num())
			{
				// really not optimized as this generate a full set of the vertices with all the allocation that comes with it
				// the cost of going through all the vertices multiple time  may be faster 
				const TSet<int32> MeshVertices = TriMesh->GetVertices();
				for (const int32 Idx : MeshVertices)
				{
					InstanceBoundingBox += MassSpaceParticles.X(Idx);
				}
			}
			else if(VertexCount[GeometryIndex])
			{
				const int32 IdxStart = VertexStart[GeometryIndex];
				const int32 IdxEnd = IdxStart + VertexCount[GeometryIndex];
				for (int32 Idx = IdxStart; Idx < IdxEnd; ++Idx)
				{
					InstanceBoundingBox += MassSpaceParticles.X(Idx);
				}
			}
			else
			{
				const Chaos::FVec3 CenterOfMass = MassPropertiesArray[GeometryIndex].CenterOfMass;
				InstanceBoundingBox = FBox(CenterOfMass, CenterOfMass);
			}

			const FSharedSimulationSizeSpecificData& SizeSpecificData = GetSizeSpecificData(SharedParams.SizeSpecificData, RestCollection, TransformGroupIndex, InstanceBoundingBox);
			if (SizeSpecificData.CollisionShapesData.Num())
			{
				// Need a new error reporter for parallel for loop here as it wouldn't be thread-safe to write to the prefix
				Chaos::FErrorReporter LocalErrorReporter;
				//
				//  Build the simplicial for the rest collection. This will be used later in the DynamicCollection to 
				//  populate the collision structures of the simulation. 
				//
				if (ensureMsgf(TriMesh, TEXT("No Triangle representation")))
				{
					Chaos::FBVHParticles* Simplicial =
						FCollisionStructureManager::NewSimplicial(
							MassSpaceParticles,
							BoneMap,
							SizeSpecificData.CollisionShapesData[0].CollisionType,
							*TriMesh,
							SizeSpecificData.CollisionShapesData[0].CollisionParticleData.CollisionParticlesFraction);
					CollectionSimplicials[TransformGroupIndex] = TUniquePtr<FSimplicial>(Simplicial); // CollectionSimplicials is in the TransformGroup
					//ensureMsgf(CollectionSimplicials[TransformGroupIndex], TEXT("No simplicial representation."));
					if (!CollectionSimplicials[TransformGroupIndex]->Size())
					{
						ensureMsgf(false, TEXT("Simplicial is empty."));
					}

					// tdo(chaos) : if imported is selected but no shape exists shoudl we simply ignore ? 
					if (SharedParams.bUseImportedCollisionImplicits && ExternaCollisions && (*ExternaCollisions)[TransformGroupIndex])
					{
						// for now  simply copy the shared pointer as the imported geometry should not change under the hood without being recreated
						const FImplicitGeomSharePtr ExternalCollisionImplicit = (*ExternaCollisions)[TransformGroupIndex];
						if (!bIdentityMassTransform)
						{
							// since we do not set the rotation of mass and center of mass properties on the particle and have a MasstoLocal managed property on the collection instead
							// we need to reverse transform the external shapes 
							TUniquePtr<FImplicitObject> TransformedCollisionImplicit = MakeTransformImplicitObject(*ExternalCollisionImplicit, MassToLocalTransform.Inverse());
							CollectionImplicits[TransformGroupIndex] = TSharedPtr<Chaos::FImplicitObject>(TransformedCollisionImplicit.Release());
						}
						else
						{
							CollectionImplicits[TransformGroupIndex] = ExternalCollisionImplicit;
						}
					}
					else
					{
						LocalErrorReporter.SetPrefix(BaseErrorPrefix + " | Transform Index: " + FString::FromInt(TransformGroupIndex) + " of " + FString::FromInt(TransformIndex.Num()));
						CollectionImplicits[TransformGroupIndex] = CreateImplicitGeometry(
							SizeSpecificData,
							TransformGroupIndex,
							RestCollection,
							MassSpaceParticles,
							*TriMesh,
							InstanceBoundingBox,
							InnerRadius[GeometryIndex],
							LocalErrorReporter);
					}
					
					if (CollectionImplicits[TransformGroupIndex] && CollectionImplicits[TransformGroupIndex]->HasBoundingBox())
					{
						const auto Implicit = CollectionImplicits[TransformGroupIndex];
						const auto BBox = Implicit->BoundingBox();
						const FVec3 Extents = BBox.Extents(); // Chaos::FAABB3::Extents() is Max - Min
						MaxChildBounds = MaxChildBounds.ComponentwiseMax(Extents);
					}
				}
			}
		}
	});

	// question: at the moment we always build cluster data in the asset. This 
	// allows for per instance toggling. Is this needed? It increases memory 
	// usage for all geometry collection assets.
	const bool bEnableClustering = true;	
	if (bEnableClustering)
	{
		//Put all children into collection space so we can compute mass properties.
		TUniquePtr<TPBDRigidClusteredParticles<FReal, 3>> CollectionSpaceParticles(new TPBDRigidClusteredParticles<FReal, 3>());
		CollectionSpaceParticles->AddParticles(NumTransforms);

		// Init to -FLT_MAX for debugging purposes
		for (int32 Idx = 0; Idx < NumTransforms; Idx++)
		{
			CollectionSpaceParticles->X(Idx) = Chaos::FVec3(-TNumericLimits<FReal>::Max());
		}

		//
		// TODO: We generate particles & handles for leaf nodes so that we can use some 
		// runtime clustering functions.  That's adding a lot of work and dependencies
		// just so we can make an API happy.  We should refactor the common routines
		// to have a handle agnostic implementation.
		//

		TMap<const TGeometryParticleHandle<FReal, 3>*, int32> HandleToTransformIdx;
		TArray<TUniquePtr<TPBDRigidClusteredParticleHandle<FReal, 3>>> Handles;
		Handles.Reserve(NumTransforms);
		for (int32 Idx = 0; Idx < NumTransforms; Idx++)
		{
			Handles.Add(TPBDRigidClusteredParticleHandle<FReal, 3>::CreateParticleHandle(
				MakeSerializable(CollectionSpaceParticles), Idx, Idx));
			HandleToTransformIdx.Add(Handles[Handles.Num() - 1].Get(), Idx);
		}

		// We use PopulateSimulatedParticle here just to give us some valid particles to operate on - with correct
		// position, mass and inertia so we can accumulate data for clusters just below.
 		for (int32 GeometryIdx = 0; GeometryIdx < NumGeometries; ++GeometryIdx)
 		{
 			const int32 TransformGroupIndex = TransformIndex[GeometryIdx];

 			if (CollectionSimulatableParticles[TransformGroupIndex])
 			{
				FTransform GeometryWorldTransform = CollectionMassToLocal[TransformGroupIndex] * CollectionSpaceTransforms[TransformGroupIndex];

 				PopulateSimulatedParticle(
 					Handles[TransformGroupIndex].Get(),
 					SharedParams, 
 					CollectionSimplicials[TransformGroupIndex].Get(),
 					CollectionImplicits[TransformGroupIndex],
 					FCollisionFilterData(),		// SimFilter
 					FCollisionFilterData(),		// QueryFilter
 					CollectionMass[TransformGroupIndex],
 					CollectionInertiaTensor[TransformGroupIndex], 
					GeometryWorldTransform,
 					(uint8)EObjectStateTypeEnum::Chaos_Object_Dynamic, 
 					INDEX_NONE,  // CollisionGroup
					1.0f // todo(chaos) CollisionParticlesPerObjectFraction is not accessible right there for now but we can pass 1.0 for the time being
				);
 			}
 		}

		const TArray<int32> RecursiveOrder = GeometryCollectionAlgo::ComputeRecursiveOrder(RestCollection);
		const TArray<int32> TransformToGeometry = ComputeTransformToGeometryMap(RestCollection);

		TArray<bool> IsClusterSimulated;
		IsClusterSimulated.Init(false, CollectionSpaceParticles->Size());
		//build collision structures depth first
		for (const int32 TransformGroupIndex : RecursiveOrder)
		{
			if (RestCollection.IsClustered(TransformGroupIndex))
			{
				const int32 ClusterTransformIdx = TransformGroupIndex;
				//update mass 
				TSet<TPBDRigidParticleHandle<FReal,3>*> ChildrenIndices;
				{ // tmp scope
					ChildrenIndices.Reserve(Children[ClusterTransformIdx].Num());
					for (int32 ChildIdx : Children[ClusterTransformIdx])
					{
						if (CollectionSimulatableParticles[ChildIdx] || IsClusterSimulated[ChildIdx])
						{
							ChildrenIndices.Add(Handles[ChildIdx].Get());
						}
					}
					if (!ChildrenIndices.Num())
					{
						continue;
					}
				} // tmp scope

				//CollectionSimulatableParticles[TransformGroupIndex] = true;
				IsClusterSimulated[TransformGroupIndex] = true;


				// TODO: This needs to be rotated to diagonal, used to update I()/InvI() from diagonal, and update transform with rotation.
				FMatrix33 ClusterInertia(0);
				UpdateClusterMassProperties(Handles[ClusterTransformIdx].Get(), ChildrenIndices, ClusterInertia);	//compute mass properties
				const FTransform ClusterMassToCollection = 
					FTransform(CollectionSpaceParticles->R(ClusterTransformIdx), 
							   CollectionSpaceParticles->X(ClusterTransformIdx));

				CollectionMassToLocal[ClusterTransformIdx] = 
					ClusterMassToCollection.GetRelativeTransform(
						CollectionSpaceTransforms[ClusterTransformIdx]);

				//update geometry
				//merge children meshes and move them into cluster's mass space
				TArray<TVector<int32, 3>> UnionMeshIndices;
				int32 BiggestNumElements = 0;
				{ // tmp scope
					int32 NumChildIndices = 0;
					for (TPBDRigidParticleHandle<FReal, 3>* Child : ChildrenIndices)
					{
						const int32 ChildTransformIdx = HandleToTransformIdx[Child];
						if (Chaos::FTriangleMesh* ChildMesh = TriangleMeshesArray[ChildTransformIdx].Get())
						{
							BiggestNumElements = FMath::Max(BiggestNumElements, ChildMesh->GetNumElements());
							NumChildIndices += ChildMesh->GetNumElements();
						}
					}
					UnionMeshIndices.Reserve(NumChildIndices);
				} // tmp scope

				FBox InstanceBoundingBox(EForceInit::ForceInitToZero);
				{ // tmp scope
					TSet<int32> VertsAdded;
					VertsAdded.Reserve(BiggestNumElements);
					for (TPBDRigidParticleHandle<FReal, 3>* Child : ChildrenIndices)
					{
						const int32 ChildTransformIdx = HandleToTransformIdx[Child];
						if (Chaos::FTriangleMesh* ChildMesh = TriangleMeshesArray[ChildTransformIdx].Get())
						{
							const TArray<TVector<int32, 3>>& ChildIndices = ChildMesh->GetSurfaceElements();
							UnionMeshIndices.Append(ChildIndices);

							// To move a particle from mass-space in the child to mass-space in the cluster parent, calculate
							// the relative transform between the mass-space origin for both the parent and child before
							// transforming the mass space particles into the parent mass-space.
							const FTransform ChildMassToClusterMass = (CollectionMassToLocal[ChildTransformIdx] * CollectionSpaceTransforms[ChildTransformIdx]).GetRelativeTransform(CollectionMassToLocal[ClusterTransformIdx] * CollectionSpaceTransforms[ClusterTransformIdx]);

							ChildMesh->GetVertexSet(VertsAdded);
							for (const int32 VertIdx : VertsAdded)
							{
								//Update particles so they are in the cluster's mass space
								MassSpaceParticles.X(VertIdx) =
									ChildMassToClusterMass.TransformPosition(MassSpaceParticles.X(VertIdx));
								InstanceBoundingBox += MassSpaceParticles.X(VertIdx);
							}
						}
					}
				} // tmp scope

				TUniquePtr<FTriangleMesh> UnionMesh(new FTriangleMesh(MoveTemp(UnionMeshIndices)));
				// TODO: Seems this should rotate full matrix and not discard off diagonals.
				const FVec3& InertiaDiagonal = CollectionSpaceParticles->I(ClusterTransformIdx);
				CollectionInertiaTensor[ClusterTransformIdx] = FVector3f(InertiaDiagonal);	// LWC_TODO: Precision loss
				CollectionMass[ClusterTransformIdx] = (FRealSingle)CollectionSpaceParticles->M(ClusterTransformIdx);

				const FSharedSimulationSizeSpecificData& SizeSpecificData = GetSizeSpecificData(SharedParams.SizeSpecificData, RestCollection, TransformGroupIndex, InstanceBoundingBox);

				int32 SizeSpecificIdx;
				if (bUseRelativeSize)
				{
					const TManagedArray<float>& RelativeSize = RestCollection.GetAttribute<float>(TEXT("Size"), FTransformCollection::TransformGroup);
					SizeSpecificIdx = GeometryCollection::SizeSpecific::FindIndexForVolume(SharedParams.SizeSpecificData, RelativeSize[TransformGroupIndex]);
				}
				else
				{
					SizeSpecificIdx = GeometryCollection::SizeSpecific::FindIndexForVolume(SharedParams.SizeSpecificData, InstanceBoundingBox);	
				}

				ErrorReporter.SetPrefix(BaseErrorPrefix + " | Cluster Transform Index: " + FString::FromInt(ClusterTransformIdx));

				if (SharedParams.bUseImportedCollisionImplicits && ExternaCollisions && (*ExternaCollisions)[TransformGroupIndex])
				{
					const FTransform& ClusterMassToLocal = CollectionMassToLocal[ClusterTransformIdx];
					const bool bIdentityMassTransform = ClusterMassToLocal.Equals(FTransform::Identity);

					// for now  simply copy the shared pointer as the imported geometry should not change under the hood without being recreated
					const FImplicitGeomSharePtr ExternalCollisionImplicit = (*ExternaCollisions)[TransformGroupIndex];
					if (!bIdentityMassTransform)
					{
						// since we do not set the rotation of mass and center of mass properties on the particle and have a MasstoLocal managed property on the collection instead
						// we need to reverse transform the external shapes 
						TUniquePtr<FImplicitObject> TransformedCollisionImplicit = MakeTransformImplicitObject(*ExternalCollisionImplicit, ClusterMassToLocal.Inverse());
						CollectionImplicits[TransformGroupIndex] = TSharedPtr<Chaos::FImplicitObject>(TransformedCollisionImplicit.Release());
					}
					else
					{
						CollectionImplicits[ClusterTransformIdx] = (*ExternaCollisions)[ClusterTransformIdx];
					}
				}
				else
				{
					CollectionImplicits[ClusterTransformIdx] = CreateImplicitGeometry(
						SizeSpecificData,
						ClusterTransformIdx,
						RestCollection,
						MassSpaceParticles,
						*UnionMesh,
						InstanceBoundingBox,
						InstanceBoundingBox.GetExtent().GetAbsMin(), // InnerRadius
						ErrorReporter,
						&MaxChildBounds
						);
				}
				// create simplicial from the implicit geometry
				CollectionSimplicials[ClusterTransformIdx] = TUniquePtr<FSimplicial>(
							FCollisionStructureManager::NewSimplicial(MassSpaceParticles, *UnionMesh, CollectionImplicits[ClusterTransformIdx].Get(),
							SharedParams.MaximumCollisionParticleCount));

				TriangleMeshesArray[ClusterTransformIdx] = MoveTemp(UnionMesh);
			}

			// Set level of TransformGroupIndex in Levels attribute.
			CalculateAndSetLevel(TransformGroupIndex, Parent, Levels);
		}
	}
}

void IdentifySimulatableElements(Chaos::FErrorReporter& ErrorReporter, FGeometryCollection& GeometryCollection)
{
	// Determine which collection particles to simulate

	// Geometry group
	const TManagedArray<int32>& TransformIndex = GeometryCollection.TransformIndex;
	const TManagedArray<FBox>& BoundingBox = GeometryCollection.BoundingBox;
	const TManagedArray<int32>& VertexCount = GeometryCollection.VertexCount;

	const int32 NumTransforms = GeometryCollection.NumElements(FGeometryCollection::TransformGroup);
	const int32 NumTransformMappings = TransformIndex.Num();

	// Faces group
	const TManagedArray<FIntVector>& Indices = GeometryCollection.Indices;
	const TManagedArray<bool>& Visible = GeometryCollection.Visible;
	// Vertices group
	const TManagedArray<int32>& BoneMap = GeometryCollection.BoneMap;

	// Do not simulate hidden geometry
	TArray<bool> HiddenObject;
	HiddenObject.Init(true, NumTransforms);
	int32 PrevObject = INDEX_NONE;
	bool bContiguous = true;
	for(int32 i = 0; i < Indices.Num(); i++)
	{
		if(Visible[i]) // Face index i is visible
		{
			const int32 ObjIdx = BoneMap[Indices[i][0]]; // Look up associated bone to the faces X coord.
			HiddenObject[ObjIdx] = false;

			if (!ensure(ObjIdx >= PrevObject))
			{
				bContiguous = false;
			}

			PrevObject = ObjIdx;
		}
	}

	if (!bContiguous)
	{
		// What assumptions???  How are we ever going to know if this is still the case?
		ErrorReporter.ReportError(TEXT("Objects are not contiguous. This breaks assumptions later in the pipeline"));
		ErrorReporter.HandleLatestError();
	}

	//For now all simulation data is a non compiled attribute. Not clear what we want for simulated vs kinematic collections
	TManagedArray<bool>& SimulatableParticles = 
		GeometryCollection.AddAttribute<bool>(
			FGeometryCollection::SimulatableParticlesAttribute, FTransformCollection::TransformGroup);
	SimulatableParticles.Fill(false);

	for(int i = 0; i < NumTransformMappings; i++)
	{
		int32 Tdx = TransformIndex[i];
		checkSlow(0 <= Tdx && Tdx < NumTransforms);
		if (GeometryCollection.IsGeometry(Tdx) && // checks that TransformToGeometryIndex[Tdx] != INDEX_NONE
			VertexCount[i] &&					 // must have vertices to be simulated?
			0.f < BoundingBox[i].GetSize().SizeSquared() && // must have a non-zero bbox to be simulated?  No single point?
			!HiddenObject[Tdx])					 // must have 1 associated face
		{
			SimulatableParticles[Tdx] = true;
		}
	}
}

void BuildSimulationData(Chaos::FErrorReporter& ErrorReporter, FGeometryCollection& GeometryCollection, const FSharedSimulationParameters& SharedParams)
{
	IdentifySimulatableElements(ErrorReporter, GeometryCollection);
	FGeometryCollectionPhysicsProxy::InitializeSharedCollisionStructures(ErrorReporter, GeometryCollection, SharedParams);
}

//==============================================================================
// FIELDS
//==============================================================================

void FGeometryCollectionPhysicsProxy::FieldParameterUpdateCallback(Chaos::FPBDRigidsSolver* RigidSolver, const bool bUpdateViews)
{
	SCOPE_CYCLE_COUNTER(STAT_ParamUpdateField_Object);

	// We are updating the Collection from the InitializeBodiesPT, so we need the PT collection
	FGeometryDynamicCollection& Collection = PhysicsThreadCollection;
	Chaos::FPBDPositionConstraints PositionTarget;
	TMap<int32, int32> TargetedParticles;

	// Process Particle-Collection commands
	int32 NumCommands = Commands.Num();
	if (NumCommands && RigidSolver && !RigidSolver->IsShuttingDown() && Collection.Transform.Num())
	{
		TArray<int32> CommandsToRemove;
		CommandsToRemove.Reserve(NumCommands);

		EFieldResolutionType PrevResolutionType = EFieldResolutionType::Field_Resolution_Max;
		EFieldFilterType PrevFilterType = EFieldFilterType::Field_Filter_Max;
		EFieldObjectType PrevObjectType = EFieldObjectType::Field_Object_Max;
		EFieldPositionType PrevPositionType = EFieldPositionType::Field_Position_Max;

		for (int32 CommandIndex = 0; CommandIndex < NumCommands; CommandIndex++)
		{
			FFieldSystemCommand& FieldCommand = Commands[CommandIndex];
			if (IsParameterFieldValid(FieldCommand) || FieldCommand.PhysicsType == EFieldPhysicsType::Field_InitialLinearVelocity || FieldCommand.PhysicsType == EFieldPhysicsType::Field_InitialAngularVelocity)
			{
				if (Chaos::BuildFieldSamplePoints(this, RigidSolver, FieldCommand, ExecutionDatas, PrevResolutionType, PrevFilterType, PrevObjectType, PrevPositionType))
				{
					const Chaos::FReal TimeSeconds = RigidSolver->GetSolverTime() - FieldCommand.TimeCreation;

					FFieldContext FieldContext(
						ExecutionDatas,
						FieldCommand.MetaData,
						TimeSeconds);

					TArray<Chaos::FGeometryParticleHandle*>& ParticleHandles = ExecutionDatas.ParticleHandles[(uint8)EFieldCommandHandlesType::InsideHandles];

					if (FieldCommand.RootNode->Type() == FFieldNodeBase::EFieldType::EField_Int32)
					{
						TArray<int32>& FinalResults = ExecutionDatas.IntegerResults[(uint8)EFieldCommandResultType::FinalResult];
						ResetResultsArray < int32 >(ExecutionDatas.SamplePositions.Num(), FinalResults, 0);

						TFieldArrayView<int32> ResultsView(FinalResults, 0, FinalResults.Num());

						if (FieldCommand.PhysicsType == EFieldPhysicsType::Field_DynamicState)
						{
							SCOPE_CYCLE_COUNTER(STAT_ParamUpdateField_DynamicState);
							{
								InitDynamicStateResults(ParticleHandles, FieldContext, FinalResults);

								static_cast<const FFieldNode<int32>*>(FieldCommand.RootNode.Get())->Evaluate(FieldContext, ResultsView);

								bool bHasStateChanged = false;
								
								const TFieldArrayView<FFieldContextIndex>& EvaluatedSamples = FieldContext.GetEvaluatedSamples();
								for (const FFieldContextIndex& Index : EvaluatedSamples)
								{
									Chaos::TPBDRigidParticleHandle<Chaos::FReal, 3>* RigidHandle = ParticleHandles[Index.Sample]->CastToRigidParticle();
									if (RigidHandle)
									{
										const int32 CurrResult = ResultsView[Index.Result];
										check(CurrResult <= std::numeric_limits<int8>::max() &&
											CurrResult >= std::numeric_limits<int8>::min());

										const int8 ResultState = static_cast<int8>(CurrResult);
										const int32 TransformIndex = HandleToTransformGroupIndex[RigidHandle];

										// Update of the handles object state. No need to update 
										// the initial velocities since it is done after this function call in InitializeBodiesPT
										if (bUpdateViews && (Parameters.InitialVelocityType == EInitialVelocityTypeEnum::Chaos_Initial_Velocity_User_Defined))
										{
											bHasStateChanged |= ReportDynamicStateResult(RigidSolver, static_cast<Chaos::EObjectStateType>(ResultState), RigidHandle,
												true, Collection.InitialLinearVelocity[TransformIndex], true, Collection.InitialAngularVelocity[TransformIndex]);
										}
										else
										{
											bHasStateChanged |= ReportDynamicStateResult(RigidSolver, static_cast<Chaos::EObjectStateType>(ResultState), RigidHandle,
												false, Chaos::FVec3(0), false, Chaos::FVec3(0));
										}
										// Update of the Collection dynamic state. It will be used just after to set the initial velocity
										Collection.DynamicState[TransformIndex] = ResultState;
									}
								}
								if (bUpdateViews && bHasStateChanged)
								{
									UpdateSolverParticlesState(RigidSolver, EvaluatedSamples, ParticleHandles);
								}
							}
						}
						else
						{
							Chaos::FieldIntegerParameterUpdate(RigidSolver, FieldCommand, ExecutionDatas.ParticleHandles[(uint8)EFieldCommandHandlesType::InsideHandles],
								FieldContext, PositionTarget, TargetedParticles, FinalResults);
						}
					}
					else if (FieldCommand.RootNode->Type() == FFieldNodeBase::EFieldType::EField_FVector)
					{
						TArray<FVector>& FinalResults = ExecutionDatas.VectorResults[(uint8)EFieldCommandResultType::FinalResult];
						ResetResultsArray < FVector >(ExecutionDatas.SamplePositions.Num(), FinalResults, FVector::ZeroVector);

						TFieldArrayView<FVector> ResultsView(FinalResults, 0, FinalResults.Num());

						if (FieldCommand.PhysicsType == EFieldPhysicsType::Field_InitialLinearVelocity)
						{
							if (Parameters.InitialVelocityType == EInitialVelocityTypeEnum::Chaos_Initial_Velocity_User_Defined)
							{
								SCOPE_CYCLE_COUNTER(STAT_ParamUpdateField_LinearVelocity);
								{
									static_cast<const FFieldNode<FVector>*>(FieldCommand.RootNode.Get())->Evaluate(FieldContext, ResultsView);
									for (const FFieldContextIndex& Index : FieldContext.GetEvaluatedSamples())
									{
										Chaos::TPBDRigidParticleHandle<Chaos::FReal, 3>* RigidHandle = ParticleHandles[Index.Sample]->CastToRigidParticle();
										if (RigidHandle)
										{
											Collection.InitialLinearVelocity[HandleToTransformGroupIndex[RigidHandle]] = FVector3f(ResultsView[Index.Result]);
										}
									}
								}
							}
							else
							{
								UE_LOG(LogChaos, Error, TEXT("Field based evaluation of the simulations 'InitialLinearVelocity' requires the geometry collection be set to User Defined Initial Velocity"));
							}
						}
						else if (FieldCommand.PhysicsType == EFieldPhysicsType::Field_InitialAngularVelocity)
						{
							if (Parameters.InitialVelocityType == EInitialVelocityTypeEnum::Chaos_Initial_Velocity_User_Defined)
							{
								SCOPE_CYCLE_COUNTER(STAT_ParamUpdateField_AngularVelocity);
								{
									static_cast<const FFieldNode<FVector>*>(FieldCommand.RootNode.Get())->Evaluate(FieldContext, ResultsView);
									for (const FFieldContextIndex& Index : FieldContext.GetEvaluatedSamples())
									{
										Chaos::TPBDRigidParticleHandle<Chaos::FReal, 3>* RigidHandle = ParticleHandles[Index.Sample]->CastToRigidParticle();
										if (RigidHandle)
										{
											Collection.InitialAngularVelocity[HandleToTransformGroupIndex[RigidHandle]] = FVector3f(ResultsView[Index.Result]);
										}
									}
								}
							}
							else
							{
								UE_LOG(LogChaos, Error, TEXT("Field based evaluation of the simulations 'InitialAngularVelocity' requires the geometry collection be set to User Defined Initial Velocity"));
							}
						}
						else
						{
							Chaos::FieldVectorParameterUpdate(RigidSolver, FieldCommand, ParticleHandles,
								FieldContext, PositionTarget, TargetedParticles, FinalResults);
						}
					}
					else if (FieldCommand.RootNode->Type() == FFieldNodeBase::EFieldType::EField_Float)
					{
						TArray<float>& FinalResults = ExecutionDatas.ScalarResults[(uint8)EFieldCommandResultType::FinalResult];
						ResetResultsArray<float>(ExecutionDatas.SamplePositions.Num(), FinalResults, 0.0f);

						TFieldArrayView<float> ResultsView(FinalResults, 0, FinalResults.Num());

						Chaos::FieldScalarParameterUpdate(RigidSolver, FieldCommand, ParticleHandles,
							FieldContext, PositionTarget, TargetedParticles, FinalResults);
					}
				}
				CommandsToRemove.Add(CommandIndex);
			}
		}		
		
		for (int32 Index = CommandsToRemove.Num() - 1; Index >= 0; --Index)
		{
			Commands.RemoveAt(CommandsToRemove[Index]);
		}
	}
}

void FGeometryCollectionPhysicsProxy::FieldForcesUpdateCallback(Chaos::FPBDRigidsSolver* RigidSolver)
{
	SCOPE_CYCLE_COUNTER(STAT_ForceUpdateField_Object);

	const int32 NumCommands = Commands.Num();
	if (NumCommands && RigidSolver && !RigidSolver->IsShuttingDown())
	{
		TArray<int32> CommandsToRemove;
		CommandsToRemove.Reserve(NumCommands);

		EFieldResolutionType PrevResolutionType = EFieldResolutionType::Field_Resolution_Max;
		EFieldFilterType PrevFilterType = EFieldFilterType::Field_Filter_Max;
		EFieldObjectType PrevObjectType = EFieldObjectType::Field_Object_Max;
		EFieldPositionType PrevPositionType = EFieldPositionType::Field_Position_Max;

		for (int32 CommandIndex = 0; CommandIndex < NumCommands; CommandIndex++)
		{
			const FFieldSystemCommand& FieldCommand = Commands[CommandIndex];
			if (IsForceFieldValid(FieldCommand))
			{
				if (Chaos::BuildFieldSamplePoints(this, RigidSolver, FieldCommand, ExecutionDatas, PrevResolutionType, PrevFilterType, PrevObjectType, PrevPositionType))
				{
					const Chaos::FReal TimeSeconds = RigidSolver->GetSolverTime() - FieldCommand.TimeCreation;

					FFieldContext FieldContext(
						ExecutionDatas,
						FieldCommand.MetaData,
						TimeSeconds);

					TArray<Chaos::FGeometryParticleHandle*>& ParticleHandles = ExecutionDatas.ParticleHandles[(uint8)EFieldCommandHandlesType::InsideHandles];

					if (FieldCommand.RootNode->Type() == FFieldNode<FVector>::StaticType())
					{
						TArray<FVector>& FinalResults = ExecutionDatas.VectorResults[(uint8)EFieldCommandResultType::FinalResult];
						ResetResultsArray < FVector >(ExecutionDatas.SamplePositions.Num(), FinalResults, FVector::ZeroVector);

						Chaos::FieldVectorForceUpdate(RigidSolver, FieldCommand, ParticleHandles,
							FieldContext, FinalResults);
					}
				}
				CommandsToRemove.Add(CommandIndex);
			}
		}
		for (int32 Index = CommandsToRemove.Num() - 1; Index >= 0; --Index)
		{
			Commands.RemoveAt(CommandsToRemove[Index]);
		}
	}
}


void FDamageCollector::Reset(int32 NumTransforms)
{
	DamageData.Reset();
	DamageData.SetNum(NumTransforms);
}

void FDamageCollector::SampleDamage(int32 TransformIndex, float Damage, float DamageThreshold)
{
	if (DamageData.IsValidIndex(TransformIndex))
	{
		FDamageData& Data = DamageData[TransformIndex];
		Data.DamageThreshold = DamageThreshold;
		Data.MaxDamages = FMath::Max(Data.MaxDamages, Damage);
		Data.bIsBroken = Data.bIsBroken || (Damage > DamageThreshold);
	}
}

FRuntimeDataCollector& FRuntimeDataCollector::GetInstance()
{
	static FRuntimeDataCollector Instance;
	return Instance;
}

void FRuntimeDataCollector::Clear()
{
	Collectors.Reset();
}

FDamageCollector* FRuntimeDataCollector::Find(const FGuid& Guid)
{
	return Collectors.Find(Guid);
}

void FRuntimeDataCollector::AddCollector(const FGuid& Guid, int32 TransformNum)
{
	FDamageCollector& Collector = Collectors.FindOrAdd(Guid);
	Collector.Reset(TransformNum);
}

void FRuntimeDataCollector::RemoveCollector(const FGuid& Guid)
{
	Collectors.Remove(Guid);
	
}
