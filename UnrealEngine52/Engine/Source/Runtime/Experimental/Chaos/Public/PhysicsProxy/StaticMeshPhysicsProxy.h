// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/Framework/PhysicsProxy.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"
#include "Chaos/Framework/BufferedData.h"

struct FPhysicsProxyKinematicUpdate
{
	FTransform NewTransform;
	FVector NewVelocity;
};

struct FStubSkeletalMeshData : public Chaos::FParticleData {
	void Reset() { };
};

class CHAOS_API FStaticMeshPhysicsProxy : public TPhysicsProxy<FStaticMeshPhysicsProxy, FStubSkeletalMeshData, FProxyTimestampBase>
{
	typedef  TPhysicsProxy<FStaticMeshPhysicsProxy, FStubSkeletalMeshData, FProxyTimestampBase> Base;
public:

	struct FShapeParams
	{
		FVector BoxExtents;
		FVector2D CapsuleHalfHeightAndRadius;
		float SphereRadius;

		FShapeParams() 
		{ 
			FMemory::Memset(this, 0, sizeof(FShapeParams));
		}
	};

	struct Params
	{
		Params()
			: Name("")
			, InitialTransform(FTransform::Identity)
			, InitialLinearVelocity(FVector::ZeroVector)
			, InitialAngularVelocity(FVector::ZeroVector)
			, ObjectType(EObjectStateTypeEnum::Chaos_Object_Dynamic)
			, ShapeType(EImplicitTypeEnum::Chaos_Max)
			, bSimulating(false)
			, TargetTransform(nullptr)
			, Mass(0.0f)
			, MinRes(5)
			, MaxRes(10)
		{}

		FString Name;
		Chaos::FParticles MeshVertexPositions;
		TArray<Chaos::TVector<int32, 3> > TriIndices;
		FShapeParams ShapeParams;
		FTransform InitialTransform;
		FVector InitialLinearVelocity;
		FVector InitialAngularVelocity;
		EObjectStateTypeEnum ObjectType;
		EImplicitTypeEnum ShapeType;
		bool bSimulating;
		FTransform* TargetTransform;
		Chaos::TSerializablePtr<Chaos::FChaosPhysicsMaterial> PhysicalMaterial;
		float Mass;
		int32 MinRes;
		int32 MaxRes;
	};

	// Engine interface functions
	using FCallbackInitFunc = TFunction<void(FStaticMeshPhysicsProxy::Params&)>;
	using FSyncDynamicFunc = TFunction<void(const FTransform&)>;

	FStaticMeshPhysicsProxy() = delete;
	FStaticMeshPhysicsProxy(UObject* InOwner, FCallbackInitFunc InInitFunc, FSyncDynamicFunc InSyncFunc);

	void Initialize();
	void Reset();

	/** Stores latest update, to be applied at next opportunity (via UpdateKinematicBodiesCallback). */
	void BufferKinematicUpdate(const FPhysicsProxyKinematicUpdate& InParamUpdate);

	/** Solver Object interface */
	bool IsSimulating() const;
	void UpdateKinematicBodiesCallback(const FParticlesType& Particles, const float Dt, const float Time, FKinematicProxy& Proxy);
	void StartFrameCallback(const float InDt, const float InTime);
	void EndFrameCallback(const float InDt);
	void BindParticleCallbackMapping(Chaos::TArrayCollectionArray<PhysicsProxyWrapper> & PhysicsProxyReverseMap, Chaos::TArrayCollectionArray<int32> & ParticleIDReverseMap);
	void CreateRigidBodyCallback(FParticlesType& InOutParticles);
	void ParameterUpdateCallback(FParticlesType& InParticles, const float InTime);
	void DisableCollisionsCallback(TSet<TTuple<int32, int32>>& InPairs);
	void AddForceCallback(FParticlesType& InParticles, const float InDt, const int32 InIndex);
	void BufferCommand(Chaos::FPhysicsSolver* InSolver, const FFieldSystemCommand& InCommand) {};

	void SyncBeforeDestroy() {};
	void OnRemoveFromScene();
	void PushToPhysicsState(const Chaos::FParticleData*) {};
	void ClearAccumulatedData() {}
	void BufferPhysicsResults();
	void FlipBuffer();
	bool PullFromPhysicsState(const int32 SolverSyncTimestamp);
	bool IsDirty() { return false; }
	FStubSkeletalMeshData* NewData() { return nullptr; }
	EPhysicsProxyType ConcreteType() { return EPhysicsProxyType::StaticMeshType; }
	/** ----------------------- */

private:

	Params Parameters;

	bool bInitializedState;
	int32 RigidBodyId;
	FVector CenterOfMass;
	FVector Scale;

	// Transform that the callback object will write into during simulation.
	// During sync this will be pushed back to the component
	FTransform SimTransform;

	// Double buffered result data
	Chaos::TBufferedData<FTransform> Results;

	/**
	 *	External functions for setup and sync, called on the game thread during callback creation and syncing
	 */
	FCallbackInitFunc InitialiseCallbackParamsFunc;
	FSyncDynamicFunc SyncDynamicTransformFunc;
	//////////////////////////////////////////////////////////////////////////

	bool bPendingKinematicUpdate;
	FPhysicsProxyKinematicUpdate BufferedKinematicUpdate;
};
