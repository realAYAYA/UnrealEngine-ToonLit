// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"


DECLARE_LOG_CATEGORY_EXTERN(LogSimulationModule, Warning, All);

struct CHAOSVEHICLESCORE_API FCoreModularVehicleDebugParams
{
	bool ShowMass = false;
	bool ShowForces = false;
	float DrawForceScaling = 0.0005f;
	float LevelSlopeThreshold = 0.96f; // ~16 degrees
};

namespace Chaos
{
	class FSimModuleTree;

	struct CHAOSVEHICLESCORE_API FControlInputs
	{

		FControlInputs()
			: Throttle(0)
			, Brake(0)
			, Steering(0)
			, Clutch(0)
			, Handbrake(0)
			, Roll(0)
			, Pitch(0)
			, Yaw(0)
			, ChangeUp(false)
			, ChangeDown(false)
			, SetGearNumber(0)
		{
		}

		float Throttle;
		float Brake;
		float Steering;
		float Clutch;
		float Handbrake;
		float Roll;
		float Pitch;
		float Yaw;
		bool ChangeUp;
		bool ChangeDown;
		int SetGearNumber;
	};

	struct CHAOSVEHICLESCORE_API FModuleHitResults
	{
		int SimIndex;
		FVector ImpactPoint;
		float Distance;
		bool bBlockingHit;
	};

	struct CHAOSVEHICLESCORE_API FAllInputs : public FControlInputs
	{
		FTransform VehicleWorldTransform;
		TMap<int32, FModuleHitResults> HitResults;
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
		NonFunctional	= (1<<0),	// bitmask 1,2,4,8
		Raycast			= (1<<1),	// requires raycast data
		TorqueBased		= (1 << 2),	// performs torque calculations
		Velocity		= (1 << 3),	// requires velocity data
	};

	enum eSimType
	{
		Undefined = 0,
		Chassis,		// no simulation effect
		Thruster,		// applies force
		Aerofoil,		// applied drag and lift forces
		Wheel,			// a wheel will simply roll if it has no power source
		Suspension,		// associated with a wheel
		Axle,			// connects more than one wheel
		Transmission,	// gears - torque multiplier
		Engine,			// (torque curve required) power source generates torque for wheel, axle, transmission, clutch
		Motor,			// (electric?, no torque curve required?) power source generates torque for wheel, axle, transmission, clutch
		Clutch,			// limits the amount of torque transferred between source and destination allowing for different rotation speeds of connected axles
		Wing,			// lift and controls aircraft roll
		Rudder,			// controls aircraft yaw
		Elevator,		// controls aircraft pitch
		Propeller,		// generates thrust when connected to a motor/engine

	};

	/**
	 * Interface base class for all simulation module building blocks
	 */
	class CHAOSVEHICLESCORE_API ISimulationModuleBase
	{
	public:
		const static int INVALID_INDEX = -1;

		ISimulationModuleBase()
			: SimModuleTree(nullptr)
			, SimTreeIndex(INVALID_INDEX)
			, StateFlags(Enabled)
			, TransformIndex(INVALID_INDEX)
			, ModuleLocalVelocity(FVector::ZeroVector)
			, bClustered(true)
			, AppliedForce(FVector::ZeroVector)
		{}
		virtual ~ISimulationModuleBase() {}

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
		virtual void Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem) {};

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

		//---

		/**
		 * Let the module know if it is still clustered or not
		 */
		void SetClustered(bool IsClusteredIn) { bClustered = IsClusteredIn; }
		bool IsClustered() const { return bClustered; }

		/**
		 * Set the COM relative transform of module when it is clustered, so relative to parent COM
		 */
		void SetClusteredTransform(const FTransform& TransformIn) { ClusteredCOMRelativeTransform  = TransformIn; }
		const FTransform& GetClusteredTransform() const { return ClusteredCOMRelativeTransform; }

		/**
		 * Set the COM relative transform of module when it is broken off, so relative to itself
		 */
		void SetIntactTransform(const FTransform& TransformIn) { IntactCOMRelativeTransform = TransformIn; }
		const FTransform& GetIntactTransform() const { return IntactCOMRelativeTransform; }

		/**
		 * The modules transform relative to the simulating body will depend on whether the GC is intact (get the transform relative to intact cluster)
		 * or fractured (transform relative to fractured part)
		 */
		const FTransform& GetParentRelativeTransform() const
		{
			if (bClustered)
			{
				return GetClusteredTransform();
			}
			else
			{
				return GetIntactTransform();
			}
		}

		/**
		 * Update the module with its current velocity
		 */
		void SetLocalVelocity(const FVector& VelocityIn) { ModuleLocalVelocity = VelocityIn; }
		const FVector& GetLocalVelocity() const { return ModuleLocalVelocity; }

		ISimulationModuleBase* GetParent();
		ISimulationModuleBase* GetFirstChild();

		// for headless chaos testing
		const FVector& GetAppliedForce() { return AppliedForce; }

	protected:

		FSimModuleTree* SimModuleTree;	// A pointer back to the simulation tree where we are stored
		int SimTreeIndex;	// Index of this SimModule in the FSimModuleTree
		eSimModuleState StateFlags;	// TODO: make this more like flags
		int TransformIndex; // Index of this Sim Module's node in Geometry Collection Transform array

		FTransform ClusteredCOMRelativeTransform;
		FTransform IntactCOMRelativeTransform;
		FVector ModuleLocalVelocity;
		bool bClustered;

		// for headless chaos testing
		FVector AppliedForce;

	};

} // namespace Chaos
