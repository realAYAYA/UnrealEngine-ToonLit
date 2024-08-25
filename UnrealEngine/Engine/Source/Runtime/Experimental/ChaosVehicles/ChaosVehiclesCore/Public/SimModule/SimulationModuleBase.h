// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Chaos/GeometryParticlesfwd.h"


DECLARE_LOG_CATEGORY_EXTERN(LogSimulationModule, Warning, All);

struct CHAOSVEHICLESCORE_API FCoreModularVehicleDebugParams
{
	bool ShowMass = false;
	bool ShowForces = false;
	float DrawForceScaling = 0.0004f;
	float LevelSlopeThreshold = 0.86f;
	bool DisableForces = false;
};

namespace Chaos
{
	class FSimModuleTree;
	struct FModuleNetData;
	struct FSimOutputData;
	class FClusterUnionPhysicsProxy;

	struct CHAOSVEHICLESCORE_API FControlInputs
	{

		FControlInputs()
			: IsValid(false)
			, IsReversing(false)
			, Throttle(0)
			, Brake(0)
			, Steering(0)
			, Clutch(0)
			, Handbrake(0)
			, Roll(0)
			, Pitch(0)
			, Yaw(0)
			, Boost(0)
			, Drift(0)
			, ChangeUp(false)
			, ChangeDown(false)
			, GearNumber(0)
			, InputDebugIndex(0)
		{
		}

		bool InputNonZero()
		{
			return FMath::Abs(Throttle) > SMALL_NUMBER
				|| FMath::Abs(Brake) > SMALL_NUMBER
				|| FMath::Abs(Steering) > SMALL_NUMBER
				|| FMath::Abs(Roll) > SMALL_NUMBER
				|| FMath::Abs(Pitch) > SMALL_NUMBER
				|| FMath::Abs(Yaw) > SMALL_NUMBER
				|| FMath::Abs(Boost) > SMALL_NUMBER
				|| FMath::Abs(Drift) > SMALL_NUMBER;
		}

		bool IsValid;
		bool IsReversing;
		float Throttle;
		float Brake;
		float Steering;
		float Clutch;
		float Handbrake;
		float Roll;
		float Pitch;
		float Yaw;
		float Boost;
		float Drift;
		bool ChangeUp;
		bool ChangeDown;
		int GearNumber;
		int InputDebugIndex;
	};

	struct CHAOSVEHICLESCORE_API FModuleHitResults
	{
		int SimIndex;
		FVector ImpactPoint;
		float Distance;
		bool bBlockingHit;
	};

	struct CHAOSVEHICLESCORE_API FAllInputs
	{
		FTransform VehicleWorldTransform;
		TMap<int32, FModuleHitResults> HitResults;
		FControlInputs ControlInputs;
		bool bKeepVehicleAwake;
	};

	/**
	 * Code common between all simulation building blocks settings
	 */
	template <typename T>
	class CHAOSVEHICLESCORE_API TSimModuleSettings
	{
	public:

		TSimModuleSettings(const T& SetupIn) : SetupData(SetupIn)
		{
			SetupData = SetupIn; // deliberate copy for now
		}

		FORCEINLINE T& AccessSetup()
		{
			return (T&)(SetupData);
		}

		FORCEINLINE const T& Setup() const
		{
			return (SetupData);
		}

	private:
		T SetupData;
	};


	enum eSimModuleState
	{
		Disabled,
		Enabled
	};

	enum eSimModuleTypeFlags
	{
		NonFunctional = (1 << 0),	// bitmask 1,2,4,8
		Raycast = (1 << 1),	// requires raycast data
		TorqueBased = (1 << 2),	// performs torque calculations
		Velocity = (1 << 3),	// requires velocity data
	};

	enum eSimType
	{
		Undefined = 0,
		Chassis,		// linear/angular damping can be applied here
		Thruster,		// applies force (can be steerable)
		Aerofoil,		// applied drag and lift forces
		Wheel,			// a wheel will simply roll if it has no power source
		Suspension,		// associated with a wheel
		Axle,			// connects more than one wheel
		Transmission,	// gears - torque multiplier
		Engine,			// (torque curve required) power source generates torque for wheel, axle, transmission, clutch
		Motor,			// NOT USED YET (electric?, no torque curve required?) power source generates torque for wheel, axle, transmission, clutch
		Clutch,			// limits the amount of torque transferred between source and destination allowing for different rotation speeds of connected axles
		Wing,			// lift and controls aircraft roll
		Rudder,			// controls aircraft yaw
		Elevator,		// controls aircraft pitch
		Propeller,		// generates thrust when connected to a motor/engine
		TorqueSim
	};

	/**
	 * Interface base class for all simulation module building blocks
	 */
	class CHAOSVEHICLESCORE_API ISimulationModuleBase
	{
	public:
		const static int INVALID_IDX = -1;

		ISimulationModuleBase()
			: SimModuleTree(nullptr)
			, SimTreeIndex(INVALID_IDX)
			, StateFlags(Enabled)
			, TransformIndex(INVALID_IDX)
			, ParticleIdx(INVALID_IDX)
			, LocalLinearVelocity(FVector::ZeroVector)
			, LocalAngularVelocity(FVector::ZeroVector)
			, bClustered(true)
			, bAnimationEnabled(true)
			, AppliedForce(FVector::ZeroVector)
			, Guid(INDEX_NONE)
		{}
		virtual ~ISimulationModuleBase() {}

		const int GetGuid() { return Guid; }
		void SetGuid(int GuidIn) { Guid = GuidIn; }

		/**
		* Get the friendly name for this module, primarily for logging & debugging module tree
		 */
		virtual const FString GetDebugName() const = 0;

		/**
		* Is Module of a specific behavioral data type
		 */
		virtual bool IsBehaviourType(eSimModuleTypeFlags InType) const = 0;

		/**
		 * The specific simulation type
		 */
		virtual eSimType GetSimType() const = 0;

		/**
		* Is Module active and simulating
		 */
		virtual bool IsEnabled() const { return (StateFlags == eSimModuleState::Enabled); }

		/*
		* Set Module state, if simulating or not
		*/
		void SetStateFlags(eSimModuleState StateFlagsIn) { StateFlags = StateFlagsIn; }

		/**
		 * The main Simulation function that is called from the physics async callback thread
		 */
		virtual void Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem) {}

		/**
		 * Animate/modify the childToParent transforms, to say rotate a wheel, or rudder, etc
		 */
		virtual void Animate(Chaos::FClusterUnionPhysicsProxy* Proxy) {}

		void SetAnimationEnabled(bool bInEnabled) { bAnimationEnabled = bInEnabled; }
		bool IsAnimationEnabled() { return bAnimationEnabled; }

		/**
		 * Option to draw debug for this module requires CVar p.Chaos.DebugDraw.Enabled 1
		 */
		virtual void DrawDebugInfo() {}

		/**
		 * Option to return debug text for drawing on the HUD in the Game Thread
		 */
		virtual bool GetDebugString(FString& StringOut) const;

		/**
		 * The transform index references the transform collection, mapping the simulation module to the geometry collection data
		 */
		void SetTransformIndex(int TransformIndexIn) { TransformIndex = TransformIndexIn; }
		const int GetTransformIndex() const { return TransformIndex; }

		/**
		 * The Particle unique index, should be valid on game and physics threads
		 */
		void SetParticleIndex(FUniqueIdx ParticleIndexIn) { ParticleIdx = ParticleIndexIn; }
		const FUniqueIdx GetParticleIndex() const { return ParticleIdx; }

		/**
		 * The modules own index in the simulation tree array
		 */
		void SetTreeIndex(int TreeIndexIn) { SimTreeIndex = TreeIndexIn; }
		int GetTreeIndex() const { return SimTreeIndex; }

		/**
		 * Very useful to store the simulation tree pointer in which we are stored, then we can access other modules that we reference through an index
		 */
		void SetSimModuleTree(FSimModuleTree* SimModuleTreeIn) { SimModuleTree = SimModuleTreeIn; }

		/**
		 * Force application function, handles deferred force application and applying the force at the collect location based on whether the GC cluster is intact or fractured
		 * Note: forces are applied in local coordinates of the module
		 */
		void AddLocalForceAtPosition(const FVector& Force, const FVector& Position, bool bAllowSubstepping = true, bool bIsLocalForce = false, bool bLevelSlope = false, const FColor& DebugColorIn = FColor::Blue);

		/**
		 * Force application function, handles deferred force application and applying the force at the collect location based on whether the GC cluster is intact or fractured
		 * Note: forces are applied in local coordinates of the module
		 */
		void AddLocalForce(const FVector& Force, bool bAllowSubstepping = true, bool bIsLocalForce = false, bool bLevelSlope = false, const FColor& DebugColorIn = FColor::Blue);

		/**
		 * Torque application function
		 * Note: forces are applied in local coordinates of the module
		 */
		void AddLocalTorque(const FVector& Torque, bool bAllowSubstepping = true, bool bAccelChangeIn = true, const FColor& DebugColorIn = FColor::Magenta);

		//---

		/**
		 * Let the module know if it is still clustered or not
		 */
		void SetClustered(bool IsClusteredIn) { bClustered = IsClusteredIn; }
		bool IsClustered() const { return bClustered; }

		/**
		 * Set the COM relative transform of module when it is clustered, so relative to parent COM
		 */
		void SetClusteredTransform(const FTransform& TransformIn) { ClusteredCOMRelativeTransform = TransformIn; }
		const FTransform& GetClusteredTransform() const { return ClusteredCOMRelativeTransform; }

		void SetInitialParticleTransform(const FTransform& TransformIn) { InitialParticleTransform = TransformIn; }
		const FTransform& GetInitialParticleTransform() const { return InitialParticleTransform; }

		void SetComponentTransform(const FTransform& TransformIn) { ComponentTransform = TransformIn; }
		const FTransform& GetComponentTransform() const { return ComponentTransform; }

		/**
		 * Set the COM relative transform of module when it is broken off, so relative to itself
		 */
		void SetIntactTransform(const FTransform& TransformIn) { IntactCOMRelativeTransform = TransformIn; IsInitialized = true; }
		const FTransform& GetIntactTransform() const { return IntactCOMRelativeTransform; }
		bool IsInitialized = false;
		/**
		 * The modules transform relative to the simulating body will depend on whether the GC is intact (get the transform relative to intact cluster)
		 * or fractured (transform relative to fractured part)
		 */
		const FTransform& GetParentRelativeTransform() const;

		/**
		 * Update the module with its current velocity
		 */
		void SetLocalLinearVelocity(const FVector& VelocityIn) { LocalLinearVelocity = VelocityIn; }
		const FVector& GetLocalLinearVelocity() const { return LocalLinearVelocity; }
		void SetLocalAngularVelocity(const FVector& VelocityIn) { LocalAngularVelocity = VelocityIn; }
		const FVector& GetLocalAngularVelocity() const { return LocalAngularVelocity; }

		ISimulationModuleBase* GetParent();
		ISimulationModuleBase* GetFirstChild();

		// for headless chaos testing
		const FVector& GetAppliedForce() { return AppliedForce; }

		// this is the replication datas
		virtual TSharedPtr<FModuleNetData> GenerateNetData(int NodeArrayIndex) const = 0;

		virtual FSimOutputData* GenerateOutputData() const { return nullptr; }

		//void SetClusterParticle(FPBDRigidClusteredParticleHandle* ParticleIn) { ClusterParticle = ParticleIn; }
		Chaos::FPBDRigidClusteredParticleHandle* GetClusterParticle(Chaos::FClusterUnionPhysicsProxy* Proxy);

		Chaos::FPBDRigidParticleHandle* GetParticleFromUniqueIndex(int32 ParticleUniqueIdx, TArray<Chaos::FPBDRigidParticleHandle*>& Particles);


	protected:

		FSimModuleTree* SimModuleTree;	// A pointer back to the simulation tree where we are stored
		int SimTreeIndex;	// Index of this SimModule in the FSimModuleTree
		eSimModuleState StateFlags;	// TODO: make this more like flags
		int TransformIndex; // Index of this Sim Module's node in Geometry Collection Transform array
		FUniqueIdx ParticleIdx; // Physics particle unique index

		FTransform InitialParticleTransform;
		FTransform RelativeOffsetTransform;
		FTransform ComponentTransform;

		FTransform ClusteredCOMRelativeTransform;
		FTransform IntactCOMRelativeTransform;
		FVector LocalLinearVelocity;
		FVector LocalAngularVelocity;
		bool bClustered;
		bool bAnimationEnabled;

		// for headless chaos testing
		FVector AppliedForce;
		int Guid; // needed a way of associating internal module with game thread.

		//FPBDRigidClusteredParticleHandle* ClusterParticle;
	};

	/**
	* Interface base class for all module network serialization
	*/
	struct CHAOSVEHICLESCORE_API FModuleNetData
	{
		FModuleNetData(int InSimArrayIndex, const FString& InDebugString = FString())
			: SimArrayIndex(InSimArrayIndex)
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
			, DebugString(InDebugString)
#endif
		{}

		virtual ~FModuleNetData() {}

		virtual eSimType GetType() = 0;
		virtual void Serialize(FArchive& Ar) = 0;
		virtual void FillNetState(const ISimulationModuleBase* SimModule) = 0;
		virtual void FillSimState(ISimulationModuleBase* SimModule) = 0;
		virtual void Lerp(const float LerpFactor, const FModuleNetData& Max, const FModuleNetData& MaxValue) = 0;

		int SimArrayIndex = -1;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		virtual FString ToString() const = 0;
		FString DebugString;
#endif
	};

	using FModuleNetDataArray = TArray<TSharedPtr<FModuleNetData>>;

	struct CHAOSVEHICLESCORE_API FSimOutputData
	{
		FSimOutputData() = default;
		virtual ~FSimOutputData() {}

		virtual eSimType GetType() = 0;
		virtual bool IsEnabled() { return bEnabled; }
		virtual FSimOutputData* MakeNewData() = 0;
		virtual void FillOutputState(const ISimulationModuleBase* SimModule);
		virtual void Lerp(const FSimOutputData& InCurrent, const FSimOutputData& InNext, float Alpha) = 0;

		bool bEnabled = true;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		virtual FString ToString() { return FString(); }
		FString DebugString;
#endif
	};


} // namespace Chaos


