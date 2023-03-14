// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollectionProxyData.h"

#include "Chaos/Framework/PhysicsProxy.h"
#include "GeometryCollection/ManagedArray.h"
#include "GeometryCollection/GeometryCollectionCollisionStructureManager.h"
#include "Chaos/CollisionFilterData.h"
#include "Chaos/Framework/BufferedData.h"
#include "Chaos/GeometryParticlesfwd.h"
#include "Chaos/ParticleHandle.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Containers/Array.h"
#include "PBDRigidsSolver.h"
#include "Chaos/Defines.h"
#include "Chaos/GeometryParticlesfwd.h"

namespace Chaos
{
	template <typename T> class TSerializablePtr;
	class FErrorReporter;
	struct FClusterCreationParameters;

	struct FDirtyGeometryCollectionData;

	class FPBDRigidsEvolutionBase;
}

/**
 * index abstraction for HitResults
 * this allows regular bones and internal clusters to be represented by a unique int32 index that can be passed to HitResults
 * and in return understood by the geometry collection physics proxy 
 */ 
struct FGeometryCollectionItemIndex
{
public:
	static FGeometryCollectionItemIndex CreateInternalClusterItemIndex(int32 ClusterUniqueIdx)
	{
		return FGeometryCollectionItemIndex(ClusterUniqueIdx, true);
	}

	static FGeometryCollectionItemIndex CreateTransformItemIndex(int32 TransformIndex)
	{
		return FGeometryCollectionItemIndex(TransformIndex, false);
	}

	static FGeometryCollectionItemIndex CreateFromExistingItemIndex(int32 ItemIndex)
	{
		FGeometryCollectionItemIndex Result(INDEX_NONE, false);
		Result.ItemIndex = ItemIndex;
		return Result;
	}
	
	static FGeometryCollectionItemIndex CreateInvalidItemIndex()
	{
		return FGeometryCollectionItemIndex(INDEX_NONE, false);
	}
	
public:
	FGeometryCollectionItemIndex(const FGeometryCollectionItemIndex& Other)
		: ItemIndex(Other.ItemIndex)
	{}
	
	bool IsInternalCluster() const
	{
		return ItemIndex < INDEX_NONE;
	}

	int32 GetInternalClusterIndex() const
	{
		check(IsInternalCluster());
		return (ItemIndex - InternalClusterBaseIndex);
	}

	int32 GetTransformIndex() const
	{
		check(!IsInternalCluster());
		return ItemIndex;
	}
	
	bool IsValid() const
	{
		return ItemIndex != INDEX_NONE;
	}

	int32 GetItemIndex() const
	{
		return ItemIndex;
	}

	bool operator==(const FGeometryCollectionItemIndex& Other) const
	{
		return ItemIndex == Other.ItemIndex;
	}
	
private:
	static const int32 InternalClusterBaseIndex = TNumericLimits<int32>::Lowest();   

	FGeometryCollectionItemIndex(int32 Index, bool bInternalCluster)
		: ItemIndex(INDEX_NONE)
	{
		if (Index > INDEX_NONE)
		{
			ItemIndex = Index + (bInternalCluster? InternalClusterBaseIndex: 0); 	
		}
	}
	
	int32 ItemIndex;
};

class FStubGeometryCollectionData : public Chaos::FParticleData 
{
public:
	typedef Chaos::FParticleData Base;

	FStubGeometryCollectionData(const FGeometryCollectionResults* DataIn=nullptr)
		: Base(Chaos::EParticleType::GeometryCollection)
		, Data(DataIn)
	{}

	void Reset() 
	{
		Base::Reset(); // Sets Type to EParticleType::Static
	}

	const FGeometryCollectionResults* GetStateData() const { return Data; }

private:
	const FGeometryCollectionResults* Data;
};

/**
 * Class to manage sharing data between the game thread and the simulation thread 
 * (which may not be different than the game thread) for a \c FGeometryDynamicCollection.
 */
class CHAOS_API FGeometryCollectionPhysicsProxy : public TPhysicsProxy<FGeometryCollectionPhysicsProxy, FStubGeometryCollectionData, FGeometryCollectionProxyTimestamp>
{
public:
	typedef TPhysicsProxy<FGeometryCollectionPhysicsProxy, FStubGeometryCollectionData, FGeometryCollectionProxyTimestamp> Base;
	typedef FCollisionStructureManager::FSimplicial FSimplicial;
	typedef Chaos::TPBDRigidParticleHandle<Chaos::FReal, 3> FParticleHandle;
	typedef Chaos::TPBDRigidClusteredParticleHandle<Chaos::FReal, 3> FClusterHandle;

	/** Proxy publics */
	using IPhysicsProxyBase::GetSolver;

	FGeometryCollectionPhysicsProxy() = delete;
	/**
	 * \p InOwner
	 * \p InDynamicCollection game thread owned geometry collection.
	 * \p InInitFunc callback invoked from \c Initialize().
	 * \p InCacheSyncFunc callback invoked from \c PullFromPhysicsState().
	 * \p InFinalSyncFunc callback invoked from \c SyncBeforeDestory().
	 */
	FGeometryCollectionPhysicsProxy(
		UObject* InOwner, 
		FGeometryDynamicCollection& GameThreadCollection,
		const FSimulationParameters& SimulationParameters,
		FCollisionFilterData InSimFilter,
		FCollisionFilterData InQueryFilter,
		FGuid InCollectorGuid = FGuid::NewGuid(),
		const Chaos::EMultiBufferMode BufferMode=Chaos::EMultiBufferMode::TripleGuarded);
	virtual ~FGeometryCollectionPhysicsProxy();

	/**
	 * Construct \c PTDynamicCollection, copying attributes from the game thread, 
	 * and prepare for simulation.
	 */
	void Initialize(Chaos::FPBDRigidsEvolutionBase* Evolution);
	void Reset() { }

	/** 
	 * Finish initialization on the physics thread. 
	 *
	 * Called by solver command registered by \c FPBDRigidsSolver::RegisterObject().
	 */
	void InitializeBodiesPT(
		Chaos::FPBDRigidsSolver* RigidsSolver,
		typename Chaos::FPBDRigidsSolver::FParticlesType& Particles);

	/** */
	static void InitializeDynamicCollection(FGeometryDynamicCollection& DynamicCollection, const FGeometryCollection& RestCollection, const FSimulationParameters& Params);

	/** */
	bool IsSimulating() const { return Parameters.Simulating; }

	/**
	 * Pushes current game thread particle state into the \c GameToPhysInterchange.
	 *
	 * Redirects to \c BufferGameState(), and returns nullptr as this class manages 
	 * data transport to the physics thread itself, without allocating memory.
	 */
	Chaos::FParticleData* NewData() { BufferGameState(); return nullptr; }
	void BufferGameState();

	/** Called at the end of \c FPBDRigidsSolver::PushPhysicsStateExec(). */
	void ClearAccumulatedData() {}

	/** Push PT state into the \c PhysToGameInterchange. */
	void BufferPhysicsResults_Internal(Chaos::FPBDRigidsSolver* CurrentSolver, Chaos::FDirtyGeometryCollectionData& BufferData);

	/** Push GT state into the \c PhysToGameInterchange for async physics */
	void BufferPhysicsResults_External(Chaos::FDirtyGeometryCollectionData& BufferData);

	/** Push data from the game thread to the physics thread */
	void PushStateOnGameThread(Chaos::FPBDRigidsSolver* InSolver);

	/** apply the state changes on the physics thread */
	void PushToPhysicsState();
	
	/** Does nothing as \c BufferPhysicsResults() already did this. */
	void FlipBuffer();
	
	/** 
	 * Pulls data out of the PhysToGameInterchange and updates \c GTDynamicCollection. 
	 * Called from FPhysScene_ChaosInterface::SyncBodies(), NOT the solver.
	 */
	bool PullFromPhysicsState(const Chaos::FDirtyGeometryCollectionData& BufferData, const int32 SolverSyncTimestamp, const Chaos::FDirtyGeometryCollectionData* NextPullData = nullptr, const Chaos::FRealSingle* Alpha= nullptr);

	bool IsDirty() { return false; }

	EPhysicsProxyType ConcreteType() { return EPhysicsProxyType::GeometryCollectionType; }

	void SyncBeforeDestroy();
	void OnRemoveFromSolver(Chaos::FPBDRigidsSolver *RBDSolver);
	void OnRemoveFromScene();

	void SetCollisionParticlesPerObjectFraction(float CollisionParticlesPerObjectFractionIn) 
	{CollisionParticlesPerObjectFraction = CollisionParticlesPerObjectFractionIn;}

	TArray<FClusterHandle*>& GetSolverClusterHandles() { return SolverClusterHandles; }

	TArray<FClusterHandle*>& GetSolverParticleHandles() { return SolverParticleHandles; }

	const FGeometryCollectionResults* GetConsumerResultsGT() const 
	{ return PhysToGameInterchange.PeekConsumerBuffer(); }

	/** Enqueue a field \p Command to be processed by \c ProcessCommands() or 
	 * \c FieldForcesUpdateCallback(). 
	 */
	void BufferCommand(Chaos::FPBDRigidsSolver* , const FFieldSystemCommand& Command)
	{ Commands.Add(Command); }

	static void InitializeSharedCollisionStructures(Chaos::FErrorReporter& ErrorReporter, FGeometryCollection& RestCollection, const FSharedSimulationParameters& SharedParams);

	void FieldForcesUpdateCallback(Chaos::FPBDRigidsSolver* RigidSolver);

	void FieldParameterUpdateCallback(Chaos::FPBDRigidsSolver* RigidSolver, const bool bUpdateViews = true);

	void UpdateKinematicBodiesCallback(const FParticlesType& InParticles, const float InDt, const float InTime, FKinematicProxy& InKinematicProxy) {}
	void StartFrameCallback(const float InDt, const float InTime) {}
	void EndFrameCallback(const float InDt) {}
	void BindParticleCallbackMapping(Chaos::TArrayCollectionArray<PhysicsProxyWrapper>& PhysicsProxyReverseMap, Chaos::TArrayCollectionArray<int32>& ParticleIDReverseMap) {}
	void CreateRigidBodyCallback(FParticlesType& InOutParticles) {}
	void DisableCollisionsCallback(TSet<TTuple<int32, int32>>& InPairs) {}
	void AddForceCallback(FParticlesType& InParticles, const float InDt, const int32 InIndex) {}

	bool IsGTCollectionDirty() const { return GameThreadCollection.IsDirty(); }

	// set the world transform ( this needs to be called on the game thread ) 
	void SetWorldTransform_External(const FTransform& WorldTransform);

	const TArray<FClusterHandle*> GetParticles() const
	{
		return SolverParticleHandles;
	}

	const FSimulationParameters& GetSimParameters() const
	{
		return Parameters;
	}

	FSimulationParameters& GetSimParameters()
	{
		return Parameters;
	}

	FGeometryDynamicCollection& GetPhysicsCollection()
	{
		return PhysicsThreadCollection;
	}

	TManagedArray<TUniquePtr<Chaos::FGeometryParticle>>& GetExternalParticles()
	{
		return GTParticles;
	}

	/**
	*  * Get all the geometry collection particle handles based on the processing resolution
	 */
	void GetRelevantParticleHandles(
		TArray<Chaos::TGeometryParticleHandle<Chaos::FReal, 3>*>& Handles,
		const Chaos::FPBDRigidsSolver* RigidSolver,
		EFieldResolutionType ResolutionType);

	/**
	 * Get all the geometry collection particle handles filtered by object state
	 */
	void GetFilteredParticleHandles(
		TArray<Chaos::TGeometryParticleHandle<Chaos::FReal, 3>*>& Handles,
		const Chaos::FPBDRigidsSolver* RigidSolver,
		const EFieldFilterType FilterType,
		const EFieldObjectType ObjectType);
		
	/* Implemented so we can construct TAccelerationStructureHandle. */
	virtual void* GetHandleUnsafe() const override { return nullptr; }

	int32 GetTransformGroupIndexFromHandle(const FParticleHandle* Handle) const
	{
		if (const int32* TransformGroupIndex = HandleToTransformGroupIndex.Find(Handle))
		{
			return *TransformGroupIndex;
		}
		return INDEX_NONE;
	}

	void GetTransformGroupIndicesFromHandles(const TArray<FParticleHandle*> Handles, TArray<int32>& IndicesOut) const
	{
		IndicesOut.SetNumUninitialized(Handles.Num());
		for (int32 HandleIndex = 0; HandleIndex < Handles.Num(); ++HandleIndex)
		{
			IndicesOut[HandleIndex] = INDEX_NONE;
			if (const int32* TransformGroupIndex = HandleToTransformGroupIndex.Find(Handles[HandleIndex]))
			{
				IndicesOut[HandleIndex] = (*TransformGroupIndex);
			}
		}
	}

	FGeometryCollectionItemIndex GetInternalClusterParentItemIndex_External(int32 ChildTransformIndex) const
	{
		// first find the GTParticle matching the Transform index
		if (ChildTransformIndex >= 0 && ChildTransformIndex < GTParticles.Num())
		{
			const TUniquePtr<Chaos::FGeometryParticle>& ChildGTParticle = GTParticles[ChildTransformIndex];
			if (const int32* InternalClusterUniqueIdx = GTParticlesToInternalClusterUniqueIdx.Find(ChildGTParticle.Get()))
			{
				return FGeometryCollectionItemIndex::CreateInternalClusterItemIndex(*InternalClusterUniqueIdx);
			}
		}
		return FGeometryCollectionItemIndex::CreateInvalidItemIndex();
	}

	const TArray<int32>* FindInternalClusterChildrenTransformIndices_External(FGeometryCollectionItemIndex ItemIndex) const
	{
		if (ensure(ItemIndex.IsInternalCluster()))
		{
			return InternalClusterUniqueIdxToChildrenTransformIndices.Find(ItemIndex.GetInternalClusterIndex());
		}
		return nullptr;
	}
	
	FGeometryCollectionItemIndex GetItemIndexFromGTParticle_External(const Chaos::FGeometryParticle* GTPParticle) const
	{
		// internal cluster have  no representation on the GT, so we use the child GT particle to find the matching internal cluster unique index 
		if (const int32* InternalClusterUniqueIdx = GTParticlesToInternalClusterUniqueIdx.Find(GTPParticle))
		{
			return FGeometryCollectionItemIndex::CreateInternalClusterItemIndex(*InternalClusterUniqueIdx);
		}
		// regular particle that has a matchig transform index 
		if (const int32* TransformGroupIndex = GTParticlesToTransformGroupIndex.Find(GTPParticle))
		{
			return FGeometryCollectionItemIndex::CreateTransformItemIndex(*TransformGroupIndex);
		}
		return FGeometryCollectionItemIndex::CreateInvalidItemIndex();
	}

	bool GetIsObjectDynamic() const { return IsObjectDynamic; }

	void DisableParticles_External(TArray<int32>&& TransformGroupIndices);

	void ApplyForceAt_External(FVector Force, FVector WorldLocation);
	void ApplyImpulseAt_External(FVector Force, FVector WorldLocation);
	void BreakClusters_External(TArray<FGeometryCollectionItemIndex>&& ItemIndices);
	void BreakActiveClusters_External();
	void RemoveAllAnchors_External();
	void ApplyExternalStrain_External(FGeometryCollectionItemIndex ItemIndex, const FVector& WorldLocation, float Radius, int32 PropagationDepth, float PropagationFactor, float StrainValue);
	void ApplyInternalStrain_External(FGeometryCollectionItemIndex ItemIndex, const FVector& WorldLocation, float Radius, int32 PropagationDepth, float PropagationFactor, float StrainValue);
	void ApplyBreakingLinearVelocity_External(FGeometryCollectionItemIndex ItemIndex, const FVector& LinearVelocity);
	void ApplyBreakingAngularVelocity_External(FGeometryCollectionItemIndex ItemIndex, const FVector& AngularVelocity);
	void ApplyLinearVelocity_External(FGeometryCollectionItemIndex ItemIndex, const FVector& LinearVelocity);
	void ApplyAngularVelocity_External(FGeometryCollectionItemIndex ItemIndex, const FVector& AngularVelocity);

	FProxyInterpolationData& GetInterpolationData() { return InterpolationData; }
	const FProxyInterpolationData& GetInterpolationData() const { return InterpolationData; }

	enum class EReplicationMode: uint8
	{
		Unknown,
		Server,
		Client,
	};

	void SetReplicationMode(EReplicationMode Mode) { ReplicationMode = Mode; }
	EReplicationMode GetReplicationMode() const { return ReplicationMode; }

	void UpdateFilterData_External(const FCollisionFilterData& NewSimFilter, const FCollisionFilterData& NewQueryFilter);
	
	/** 
	 * Traverses the parents of TransformGroupIdx counting number of levels,
	 * and sets levels array value for TransformGroupIdx and its parents if not yet initialized.
	 * If level is already set, retrieve stored level.
	 * Uninitialized level array should be correct size and defaulted to zeros.
	 */
	static int32 CalculateAndSetLevel(int32 TransformGroupIdx, const TManagedArray<int32>& Parent, TManagedArray<int32>& Levels);

	void SetPostPhysicsSyncCallback(TFunction<void()> Callback)
	{
		PostPhysicsSyncCallback = Callback;
	}
	
protected:
	/**
	* Compute damage threshold for a specific transform
	* this account for component level damage threshold as well as size specific ones
	* @param DynamicCollection dynamic collection to use
	* @param TransformIndex index of the transform to compute the threshold for
	* #return damage threshold value
	*/
	float ComputeDamageThreshold(const FGeometryDynamicCollection& DynamicCollection, int32 TransformIndex) const;
		
	/**
	 * Build a physics thread cluster parent particle.
	 *	\p CollectionClusterIndex - the source geometry collection transform index.
	 *	\p ChildHandles - physics particle handles of the cluster children.
	 *  \p ChildTransformGroupIndices - geometry collection indices of the children.
	 *  \P Parameters - uh, yeah...  Other parameters.
	 */

	Chaos::FPBDRigidClusteredParticleHandle* BuildClusters_Internal(
		const uint32 CollectionClusterIndex, 
		TArray<Chaos::FPBDRigidParticleHandle*>& ChildHandles,
		const TArray<int32>& ChildTransformGroupIndices,
		const Chaos::FClusterCreationParameters & Parameters,
		const Chaos::FUniqueIdx* ExistingIndex);

	void SetSleepingState(const Chaos::FPBDRigidsSolver& RigidsSolver);
	void DirtyAllParticles(const Chaos::FPBDRigidsSolver& RigidsSolver);

	/** 
	 * Traverses the parents of \p TransformIndex in \p GeometryCollection, counting
	 * the number of levels until the next parent is \c INDEX_NONE.
	 */
	static int32 CalculateHierarchyLevel(const FGeometryDynamicCollection& DynamicCollection, int32 TransformIndex);

	void SetClusteredParticleKinematicTarget_Internal(Chaos::FPBDRigidClusteredParticleHandle* Handle, const FTransform& WorldTransform);

	void PrepareBufferData(Chaos::FDirtyGeometryCollectionData& BufferData, const FGeometryDynamicCollection& ThreadCollection,  Chaos::FReal SolverLastDt = 0.0);

	void CreateNonClusteredParticles(Chaos::FPBDRigidsSolver* RigidsSolver,	const FGeometryCollection& RestCollection, const FGeometryDynamicCollection& DynamicCollection);

	Chaos::FPBDRigidClusteredParticleHandle* FindClusteredParticleHandleByItemIndex_Internal(FGeometryCollectionItemIndex ItemIndex) const;
	
private:

	FSimulationParameters Parameters;
	TArray<FFieldSystemCommand> Commands;

	/** Field Datas stored during evaluation */
	FFieldExecutionDatas ExecutionDatas;

	//
	//  Proxy State Information
	//
	int32 NumParticles;
	int32 BaseParticleIndex;
	TArray<FParticleHandle*> SolverClusterID;
	TArray<FClusterHandle*> SolverClusterHandles; // make a TArray of the base clase with type
	TArray<FClusterHandle*> SolverParticleHandles;// make a TArray of base class and join with above
	TSet<FClusterHandle*> SolverAnchors;
	TMap<FParticleHandle*, int32> HandleToTransformGroupIndex;
	TMap<int32, FClusterHandle*> UniqueIdxToInternalClusterHandle;

	//
	// Buffer Results State Information
	//
	bool IsObjectDynamic; // Records current dynamic state
	bool IsObjectLoading; // Indicate when loaded
	bool IsObjectDeleting; // Indicatge when pending deletion

	EReplicationMode ReplicationMode = EReplicationMode::Unknown;	

	TManagedArray<TUniquePtr<Chaos::FGeometryParticle>> GTParticles;
	TMap<Chaos::FGeometryParticle*, int32> GTParticlesToTransformGroupIndex;
	TMap<Chaos::FGeometryParticle*, int32> GTParticlesToInternalClusterUniqueIdx;
	TMap<int32, TArray<int32>> InternalClusterUniqueIdxToChildrenTransformIndices;

	TMap<int32, TUniquePtr<Chaos::FGeometryParticle>> GTInternalClustersByUniqueIdx;

	// These are read on both threads and should not be changed
	const FCollisionFilterData SimFilter;
	const FCollisionFilterData QueryFilter;

	// This is a subset of the geometry group that are used in the transform hierarchy to represent geometry
	TArray<FBox> ValidGeometryBoundingBoxes;
	TArray<int32> ValidGeometryTransformIndices;

#ifdef TODO_REIMPLEMENT_RIGID_CACHING
	TFunction<void(void)> ResetAnimationCacheCallback;
	TFunction<void(const TArrayView<FTransform> &)> UpdateTransformsCallback;
	TFunction<void(const int32 & CurrentFrame, const TManagedArray<int32> & RigidBodyID, const TManagedArray<int32>& Level, const TManagedArray<int32>& Parent, const TManagedArray<TSet<int32>>& Children, const TManagedArray<uint32>& SimulationType, const TManagedArray<uint32>& StatusFlags, const FParticlesType& Particles)> UpdateRestStateCallback;
	TFunction<void(float SolverTime, const TManagedArray<int32> & RigidBodyID, const FParticlesType& Particles, const FCollisionConstraintsType& CollisionRule)> UpdateRecordedStateCallback;
	TFunction<void(FRecordedTransformTrack& InTrack)> CommitRecordedStateCallback;

	// Index of the first particles for this collection in the larger particle array
	// Time since this object started simulating
	float ProxySimDuration;

	// Sync frame numbers so we don't do many syncs when physics is running behind
	uint32 LastSyncCountGT;

	// Storage for the recorded frame information when we're caching the geometry component results.
	// Synced back to the component with SyncBeforeDestroy
	FRecordedTransformTrack RecordedTracks;
#endif

	// called after we sync the physics thread data ( called on the game thread )
	TFunction<void()> PostPhysicsSyncCallback;
	
	// Per object collision fraction.
	float CollisionParticlesPerObjectFraction;

	// The Simulation data is copied between the game and physics thread. It is 
	// expected that the two data sets will diverge, based on how the simulation
	// uses the data, but at the start of the simulation the PhysicsThreadCollection
	// is a deep copy from the GameThreadCollection. 
	FGeometryDynamicCollection PhysicsThreadCollection;
	FGeometryDynamicCollection& GameThreadCollection;

	// this data flows from Game thread to physics thread
	FGeometryCollectioPerFrameData GameThreadPerFrameData;
	bool bIsPhysicsThreadWorldTransformDirty;
	bool bIsCollisionFilterDataDirty;

	// Currently this is using triple buffers for game-physics and 
	// physics-game thread communication, but not for any reason other than this 
	// is the only implementation we currently have of a guarded buffer - a buffer 
	// that tracks it's own state, rather than having other mechanisms determine 
	// whether or not the contents of the buffer have been updated.  A double 
	// buffer would probably be fine, as that seems to be the assumption the logic
	// currently managing the exchange is built upon.  However, I believe that 
	// logic locks, and the triple buffer would enable a decoupled lock-free 
	// paradigm, at least for this component of the handshake.
	Chaos::FGuardedTripleBuffer<FGeometryCollectionResults> PhysToGameInterchange;

	FProxyInterpolationData InterpolationData;

	// this is used as a unique ID when collecting data from runtime
	FGuid CollectorGuid;
};

/**
 * the watcher collects runtime data about
 * damage on each piece of the geometry collection 
 */
struct CHAOS_API FDamageCollector
{
public:
	struct FDamageData
	{
		float DamageThreshold= 0;
		float MaxDamages = 0;
		bool  bIsBroken = false;
	};

	void Reset(int32 NumTransforms);
	int32 Num() const { return DamageData.Num(); }
	const FDamageData& operator[](int32 TransformIndex) const 
	{ 
		static FDamageData DefaultDamageData;
		if (DamageData.IsValidIndex(TransformIndex))
			return DamageData[TransformIndex]; 
		return DefaultDamageData;
	}
	void SampleDamage(int32 TransformIndex, float Damage, float DamageThreshold);
	
private:
	TArray<FDamageData> DamageData;
};

struct CHAOS_API FRuntimeDataCollector
{
public:
	void Clear();
	void AddCollector(const FGuid& Guid, int32 TransformNum);
	void RemoveCollector(const FGuid& Guid);

	FDamageCollector* Find(const FGuid& Guid);

	static FRuntimeDataCollector& GetInstance();
	
private:
	// collectors by geometry collection Guids
	TMap<FGuid,FDamageCollector> Collectors;
};

CHAOS_API TUniquePtr<Chaos::FTriangleMesh> CreateTriangleMesh(const int32 FaceStart,const int32 FaceCount,const TManagedArray<bool>& Visible,const TManagedArray<FIntVector>& Indices, bool bRotateWinding = true);
CHAOS_API void BuildSimulationData(Chaos::FErrorReporter& ErrorReporter, FGeometryCollection& GeometryCollection, const FSharedSimulationParameters& SharedParams);
