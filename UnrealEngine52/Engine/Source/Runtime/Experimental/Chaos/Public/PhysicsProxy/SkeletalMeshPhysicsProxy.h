// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/Framework/PhysicsProxy.h"
#include "BoneHierarchy.h"
#include "GeometryCollection/GeometryCollectionSimulationTypes.h"

#include "Chaos/PBDJointConstraints.h"
#include "Framework/TripleBufferedData.h"

// @todo(chaos): remove this file

struct CHAOS_API FSkeletalMeshPhysicsProxyParams
{
	FSkeletalMeshPhysicsProxyParams()
		: Name("")
		, InitialTransform(FTransform::Identity)
		, InitialLinearVelocity(FVector::ZeroVector)
		, InitialAngularVelocity(FVector::ZeroVector)

		, ObjectType(EObjectStateTypeEnum::Chaos_Object_Kinematic)

		, CollisionType(ECollisionTypeEnum::Chaos_Volumetric)
		, ParticlesPerUnitArea(0.1f)
		, MinNumParticles(0)
		, MaxNumParticles(50)
		, MinRes(5)
		, MaxRes(10)
		, CollisionGroup(0)
#if 0
		, bEnableClustering(false)
		, ClusterGroupIndex(0)
		, MaxClusterLevel(100)
		, DamageThreshold(250.f)
#endif
		, Density(2.4f)
		, MinMass(0.001f)
		, MaxMass(1.e6f)

		, bSimulating(false)
	{}

	FString Name;

	//
	// Analytic implicit representation
	//

	FBoneHierarchy BoneHierarchy;

	//
	// Mesh
	//

	TArray<FVector> MeshVertexPositions;
	TArray<FIntVector> Triangles;

	//
	// Transform hierarchy
	//

	FTransform InitialTransform;
	FTransform LocalToWorld;
	FVector InitialLinearVelocity;
	FVector InitialAngularVelocity;

	Chaos::TSerializablePtr<Chaos::FChaosPhysicsMaterial> PhysicalMaterial;	// @todo(ccaulfield): should be per-shape
	EObjectStateTypeEnum ObjectType;												// @todo(ccaulfield): should be per-body

	ECollisionTypeEnum CollisionType;
	float ParticlesPerUnitArea;
	int32 MinNumParticles;
	int32 MaxNumParticles;
	int32 MinRes;
	int32 MaxRes;
	int32 CollisionGroup;
#if 0
	bool bEnableClustering;
	int32 ClusterGroupIndex;
	int32 MaxClusterLevel;
	float DamageThreshold;
#endif
	float Density;
	float MinMass;
	float MaxMass;

	bool bSimulating;
};

// @todo(ccaulfield): make the IO structures private again - only the hierarchy should be required outside the PhysicsProxy
struct FSkeletalMeshPhysicsProxyInputs
{
	TArray<FTransform> Transforms;
	TArray<FVector> LinearVelocities;
	TArray<FVector> AngularVelocities;
};

struct FSkeletalMeshPhysicsProxyOutputs : public Chaos::FParticleData
{
	TArray<FTransform> Transforms;
	TArray<FVector> LinearVelocities;
	TArray<FVector> AngularVelocities;
	void Reset() {
		Transforms = TArray<FTransform>();
		LinearVelocities = TArray<FVector>();
		AngularVelocities = TArray<FVector>();
	}
};


class CHAOS_API FSkeletalMeshPhysicsProxy : public TPhysicsProxy<FSkeletalMeshPhysicsProxy, FSkeletalMeshPhysicsProxyOutputs, FProxyTimestampBase>
{
	typedef TPhysicsProxy<FSkeletalMeshPhysicsProxy, FSkeletalMeshPhysicsProxyOutputs, FProxyTimestampBase> Base;
public:


	using FInitFunc = TFunction<void(FSkeletalMeshPhysicsProxyParams& OutParams)>;
	using FInputFunc = TFunction<bool(const float Dt, FSkeletalMeshPhysicsProxyParams& OutParams)>;


	FSkeletalMeshPhysicsProxy() = delete;
	FSkeletalMeshPhysicsProxy(UObject* InOwner, const FInitFunc& InitFunc);
	~FSkeletalMeshPhysicsProxy();

	/** Solver Object interface */
	void Initialize();
	bool IsSimulating() const;
	void UpdateKinematicBodiesCallback(const FParticlesType& Particles, const float Dt, const float Time, FKinematicProxy& Proxy);
	void StartFrameCallback(const float InDt, const float InTime);
	void EndFrameCallback(const float InDt);
	void CreateRigidBodyCallback(FParticlesType& InOutParticles);
	void ParameterUpdateCallback(FParticlesType& InParticles, const float InTime);
	void DisableCollisionsCallback(TSet<TTuple<int32, int32>>& InPairs);
	void AddForceCallback(FParticlesType& InParticles, const float InDt, const int32 InIndex);

	void BindParticleCallbackMapping(Chaos::TArrayCollectionArray<PhysicsProxyWrapper>& PhysicsProxyReverseMap, Chaos::TArrayCollectionArray<int32>& ParticleIDReverseMap);

	void BufferCommand(Chaos::FPhysicsSolver* InSolver, const FFieldSystemCommand& InCommmand) {}

	FSkeletalMeshPhysicsProxyOutputs* NewData() { return nullptr; }
	void SyncBeforeDestroy();
	void OnRemoveFromScene();
	void PushToPhysicsState(const Chaos::FParticleData*) {};
	void ClearAccumulatedData() {}
	void BufferPhysicsResults();
	void FlipBuffer();
	bool PullFromPhysicsState(const int32 SolverSyncTimestamp);
	bool IsDirty() { return false; }
	EPhysicsProxyType ConcreteType() { return EPhysicsProxyType::SkeletalMeshType; }
	/** ----------------------- */

	/**
	 *
	 */
	void Reset();

	/**
	 * Capture the current animation pose for use by the physics.
	 * Called by game thread via the owning component's tick.
	 */
	void CaptureInputs(const float Dt, const FInputFunc& InputFunc);

	/** 
	 */
	const FSkeletalMeshPhysicsProxyOutputs* GetOutputs() const { return CurrentOutputConsumerBuffer; }

	const FBoneHierarchy& GetBoneHierarchy() const { return Parameters.BoneHierarchy; }

private:
	using FJointConstraints = Chaos::FPBDJointConstraints;

	FSkeletalMeshPhysicsProxyParams Parameters;
	TArray<int32> RigidBodyIds;
	FJointConstraints JointConstraints;
	// @todo(ccaulfield): sort out the IO buffer stuff
	Chaos::TTripleBufferedData<FSkeletalMeshPhysicsProxyInputs> InputBuffers;
	Chaos::TBufferedData<FSkeletalMeshPhysicsProxyOutputs> OutputBuffers;
	FSkeletalMeshPhysicsProxyInputs* NextInputProducerBuffer;				// Buffer for the game to write to next
	const FSkeletalMeshPhysicsProxyOutputs* CurrentOutputConsumerBuffer;	// Buffer for the game to read from next
	bool bInitializedState;

	FInitFunc InitFunc;
};
