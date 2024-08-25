// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosModularVehicle/ChaosSimModuleManagerAsyncCallback.h"
#include "SimModule/SimModulesInclude.h"
#include "ChaosModularVehicle/ModularVehicleInputRate.h"
#include "ChaosModularVehicle/ModularVehicleSimulationCU.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Components/PrimitiveComponent.h"
#include "Components/ActorComponent.h"
#include "Containers/Map.h"
#include "UObject/ObjectKey.h"

#include "ModularVehicleBaseComponent.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogModularBase, Log, All);

struct FModularVehicleAsyncInput;
struct FChaosSimModuleManagerAsyncOutput;
class UClusterUnionComponent;
namespace Chaos
{
	class FSimTreeUpdates;
}

USTRUCT()
struct FVehicleComponentData
{
	GENERATED_BODY()

	int Guid = -1;
};

/** Additional replicated state */
USTRUCT()
struct CHAOSMODULARVEHICLEENGINE_API FModularReplicatedState : public FModularVehicleInputs
{
	GENERATED_USTRUCT_BODY()

	FModularReplicatedState() : FModularVehicleInputs()
	{
	}

};

/** Input Options */
UENUM()
enum class EFunctionType : uint8
{
	LinearFunction = 0,
	SquaredFunction,
	CustomCurve
};

USTRUCT()
struct CHAOSMODULARVEHICLEENGINE_API FModularVehicleInputRateConfig
{
	GENERATED_USTRUCT_BODY()

	/**
		* Rate at which the input value rises
		*/
	UPROPERTY(EditAnywhere, Category = VehicleInputRate)
	float RiseRate;

	/**
	 * Rate at which the input value falls
	 */
	UPROPERTY(EditAnywhere, Category = VehicleInputRate)
	float FallRate;

	/**
	 * Controller input curve, various predefined options, linear, squared, or user can specify a custom curve function
	 */
	UPROPERTY(EditAnywhere, Category = VehicleInputRate)
	EFunctionType InputCurveFunction;

	/**
	 * Controller input curve - should be a normalized float curve, i.e. time from 0 to 1 and values between 0 and 1
	 * This curve is only sued if the InputCurveFunction above is set to CustomCurve
	 */
	UPROPERTY(EditAnywhere, Category = VehicleInputRate)
	FRuntimeFloatCurve UserCurve;

	FModularVehicleInputRateConfig() : RiseRate(5.0f), FallRate(5.0f), InputCurveFunction(EFunctionType::LinearFunction) { }

	/** Change an output value using max rise and fall rates */
	float InterpInputValue(float DeltaTime, float CurrentValue, float NewValue) const
	{
		const float DeltaValue = NewValue - CurrentValue;

		// We are "rising" when DeltaValue has the same sign as CurrentValue (i.e. delta causes an absolute magnitude gain)
		// OR we were at 0 before, and our delta is no longer 0.
		const bool bRising = ((DeltaValue > 0.0f) == (CurrentValue > 0.0f)) ||
			((DeltaValue != 0.f) && (CurrentValue == 0.f));

		const float MaxDeltaValue = DeltaTime * (bRising ? RiseRate : FallRate);
		const float ClampedDeltaValue = FMath::Clamp(DeltaValue, -MaxDeltaValue, MaxDeltaValue);
		return CurrentValue + ClampedDeltaValue;
	}

	float CalcControlFunction(float InputValue)
	{
		// user defined curve

		// else use option from drop down list
		switch (InputCurveFunction)
		{
		case EFunctionType::CustomCurve:
		{
			if (UserCurve.GetRichCurveConst() && !UserCurve.GetRichCurveConst()->IsEmpty())
			{
				float Output = FMath::Clamp(UserCurve.GetRichCurveConst()->Eval(FMath::Abs(InputValue)), 0.0f, 1.0f);
				return (InputValue < 0.f) ? -Output : Output;
			}
			else
			{
				return InputValue;
			}
		}
		break;
		case EFunctionType::SquaredFunction:
		{
			return (InputValue < 0.f) ? -InputValue * InputValue : InputValue * InputValue;
		}
		break;

		case EFunctionType::LinearFunction:
		default:
		{
			return InputValue;
		}
		break;

		}

	}
};

USTRUCT()
struct CHAOSMODULARVEHICLEENGINE_API FConstructionData
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	UPrimitiveComponent* Component = nullptr;

	UPROPERTY()
	int32 ConstructionIndex = INDEX_NONE;
};


UCLASS(ClassGroup = (Physics), meta = (BlueprintSpawnableComponent), hidecategories = (PlanarMovement, "Components|Movement|Planar", Activation, "Components|Activation"))
class CHAOSMODULARVEHICLEENGINE_API UModularVehicleBaseComponent : public UPawnMovementComponent
{
	GENERATED_UCLASS_BODY()

	~UModularVehicleBaseComponent();

	friend struct FModularVehicleAsyncInput;
	friend struct FModularVehicleAsyncOutput;

	friend struct FNetworkModularVehicleInputs;
	friend struct FNetworkModularVehicleStates;

	friend class FModularVehicleManager;
	friend class FChaosSimModuleManagerAsyncCallback;

	friend class FModularVehicleBuilder;
public:
	APlayerController* GetPlayerController() const;
	bool IsLocallyControlled() const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual bool ShouldCreatePhysicsState() const override { return true; }
	virtual void OnCreatePhysicsState() override;
	virtual void OnDestroyPhysicsState() override;
	virtual void SetClusterComponent(UClusterUnionComponent* InPhysicalComponent);

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void CreateAssociatedSimComponents(const UPrimitiveComponent* AttachedComponent, int ParentIndex, int TransformIndex, Chaos::FSimTreeUpdates& TreeUpdatesOut);

	void CreateConstraint(Chaos::ISimulationModuleBase* NewModule);
	void DestroyConstraint(int ConstraintIndex);
	void DestroyAllConstraints();
	void EnableConstraint(int ConstraintIndex, bool bEnabled);

	void PreTickGT(float DeltaTime);
	void UpdateState(float DeltaTime);
	TUniquePtr<FModularVehicleAsyncInput> SetCurrentAsyncData(int32 InputIdx, FChaosSimModuleManagerAsyncOutput* CurOutput, FChaosSimModuleManagerAsyncOutput* NextOutput, float Alpha, int32 VehicleManagerTimestamp);

	void ParallelUpdate(float DeltaTime);
	void Update(float DeltaTime);
	void FinalizeSimCallbackData(FChaosSimModuleManagerAsyncInput& Input);

	/** handle stand-alone and networked mode control inputs */
	void ProcessControls(float DeltaTime);

	void ShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos);

	TUniquePtr<FPhysicsVehicleOutput>& PhysicsVehicleOutput()
	{
		return PVehicleOutput;
	}

	FORCEINLINE const FTransform& GetComponentTransform() const;

	//UPROPERTY(EditAnywhere, Category = VehicleInput, AdvancedDisplay)
	TArray<FModularVehicleInputRate> InputInterpolationRates;

	/** Use to naturally decelerate linear velocity of objects */
	UPROPERTY(EditAnywhere, Category = "Game|Components|ModularVehicle")
	float LinearDamping;

	/** Use to naturally decelerate angular velocity of objects */
	UPROPERTY(EditAnywhere, Category = "Game|Components|ModularVehicle")
	float AngularDamping;

	UPROPERTY(EditAnywhere, Category = "Game|Components|ModularVehicle")
	struct FCollisionResponseContainer SuspensionTraceCollisionResponses;

	UPROPERTY(EditAnywhere, Category = "Game|Components|ModularVehicle")
	bool bSuspensionTraceComplex;

	UPROPERTY(EditAnywhere, Category = "Game|Components|ModularVehicle")
	bool bKeepVehicleAwake;

	/** Adds any associated simulation components to the ModularVehicleSimulation */
	UFUNCTION()
	void AddComponentToSimulation(UPrimitiveComponent* Component, const TArray<FClusterUnionBoneData>& BonesData, const TArray<FClusterUnionBoneData>& RemovedBoneIDs, bool bIsNew);

	/** Removes any associated simulation components from the ModularVehicleSimulation */
	UFUNCTION()
	void RemoveComponentFromSimulation(UPrimitiveComponent* Component, const TArray<FClusterUnionBoneData>& RemovedBonesData);

	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	void SetLocallyControlled(bool bLocallyControlledIn);

	// CONTROLS
	// 
	/** Set the user input for the vehicle throttle [range 0 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	void SetThrottleInput(float Throttle);

	/** Set the user input for the vehicle boost [range 0 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	void SetBoostInput(float Boost);

	/** Set the user input for the vehicle drift [range 0 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	void SetDriftInput(float Drift);

	/** Increase the vehicle throttle position [throttle range normalized 0 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	void IncreaseThrottleInput(float ThrottleDelta);

	/** Decrease the vehicle throttle position  [throttle range normalized 0 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	void DecreaseThrottleInput(float ThrottleDelta);

	/** Set the user input for the vehicle Brake [range 0 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	void SetBrakeInput(float Brake);

	/** Set the user input for the vehicle steering [range -1 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	void SetSteeringInput(float Steering);

	/** Set the user input for the vehicle pitch [range -1 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	void SetPitchInput(float Pitch);

	/** Set the user input for the vehicle roll [range -1 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	void SetRollInput(float Roll);

	/** Set the user input for the vehicle yaw [range -1 to 1] */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	void SetYawInput(float Yaw);

	/** Set the user input for handbrake */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	void SetHandbrakeInput(float Handbrake);

	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	void SetReverseInput(bool Reverse);

	/** Set the gear directly */
	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	void SetGearInput(int32 Gear);

	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	int32 GetCurrentGear();

	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	bool IsReversing();

	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	void AddActorsToIgnore(TArray<AActor*>& ActorsIn);

	UFUNCTION(BlueprintCallable, Category = "Game|Components|ModularVehicle")
	void RemoveActorsToIgnore(TArray<AActor*>& ActorsIn);

	// Bypass the need for a controller in order for the controls to be processed.
	UPROPERTY(EditAnywhere, Category = "Game|Components|ModularVehicle")
	bool bRequiresControllerForInputs;

	/** Grab nearby components and add them to the cluster union representing the vehicle */
	UPROPERTY(EditAnywhere, Category = "Game|Components|ModularVehicle")
	bool bAutoAddComponentsFromWorld;

	/** The size of the overlap box testing for nearby components in the world  */
	UPROPERTY(EditAnywhere, Category = "Game|Components|ModularVehicle", meta = (EditCondition = "bAutoAddComponentsToCluster"))
	FVector AutoAddOverlappingBoxSize;

	UPROPERTY(EditAnywhere, Category = "Game|Components|ModularVehicle", meta = (EditCondition = "bAutoAddComponentsToCluster"))
	int32 DelayClusteringCount;

	/*** Map simulation component to our vehicle setup data */
	TMap<TObjectKey<UPrimitiveComponent>, FVehicleComponentData> ComponentToPhysicsObjects;

	UClusterUnionComponent* ClusterUnionComponent;

	/** Set all channels to the specified response - for wheel raycasts */
	void SetWheelTraceAllChannels(ECollisionResponse NewResponse)
	{
		SuspensionTraceCollisionResponses.SetAllChannels(NewResponse);
	}

	/** Set the response of this body to the supplied settings - for wheel raycasts */
	void SetWheelTraceResponseToChannel(ECollisionChannel Channel, ECollisionResponse NewResponse)
	{
		SuspensionTraceCollisionResponses.SetResponse(Channel, NewResponse);
	}

protected:

	void CreateVehicleSim();
	void DestroyVehicleSim();
	void UpdatePhysicalProperties();
	void AddOverlappingComponentsToCluster();
	void AddGeometryCollectionsFromOwnedActor();

	void ActionTreeUpdates(Chaos::FSimTreeUpdates* NextTreeUpdates);

	void SetCurrentAsyncDataInternal(FModularVehicleAsyncInput* CurInput, int32 InputIdx, FChaosSimModuleManagerAsyncOutput* CurOutput, FChaosSimModuleManagerAsyncOutput* NextOutput, float Alpha, int32 VehicleManagerTimestamp);

	IPhysicsProxyBase* GetPhysicsProxy() const;

	int32 FindComponentAddOrder(UPrimitiveComponent* InComponent);
	bool FindAndRemoveNextPendingUpdate(int32 NextIndex, Chaos::FSimTreeUpdates* OutData);

	// replicated state of vehicle 
	UPROPERTY(Transient, Replicated)
	FModularReplicatedState ReplicatedState;

	// What the player has the steering set to. Range -1...1
	UPROPERTY(Transient)
	float RawSteeringInput;

	// What the player has the accelerator set to. Range -1...1
	UPROPERTY(Transient)
	float RawThrottleInput;

	// What the player has the brake set to. Range -1...1
	UPROPERTY(Transient)
	float RawBrakeInput;

	// What the player has the brake set to. Range -1...1
	UPROPERTY(Transient)
	float RawHandbrakeInput;

	// What the player has the clutch set to. Range -1...1
	UPROPERTY(Transient)
	float RawClutchInput;

	// What the player has the pitch set to. Range -1...1
	UPROPERTY(Transient)
	float RawPitchInput;

	// What the player has the roll set to. Range -1...1
	UPROPERTY(Transient)
	float RawRollInput;

	// What the player has the yaw set to. Range -1...1
	UPROPERTY(Transient)
	float RawYawInput;

	// What the player has the yaw set to. Range -1...1
	UPROPERTY(Transient)
	float RawBoostInput;

	// What the player has the yaw set to. Range -1...1
	UPROPERTY(Transient)
	float RawDriftInput;

	// latest gear selected
	UPROPERTY(Transient)
	int32 RawGearInput;

	// reverse direction enbaled
	UPROPERTY(Transient)
	bool RawReverseInput;

	// Steering output to physics system. Range -1...1
	UPROPERTY(Transient)
	float SteeringInput;

	// Accelerator output to physics system. Range 0...1
	UPROPERTY(Transient)
	float ThrottleInput;

	// Brake output to physics system. Range 0...1
	UPROPERTY(Transient)
	float BrakeInput;

	// Handbrake output to physics system. Range 0...1
	UPROPERTY(Transient)
	float HandbrakeInput;

	// Clutch output to physics system. Range 0...1
	UPROPERTY(Transient)
	float ClutchInput;

	// Body Pitch output to physics system. Range -1...1
	UPROPERTY(Transient)
	float PitchInput;

	// Body Roll output to physics system. Range -1...1
	UPROPERTY(Transient)
	float RollInput;

	// Body Yaw output to physics system. Range -1...1
	UPROPERTY(Transient)
	float YawInput;

	// Boost output to physics system. Range 0...1
	UPROPERTY(Transient)
	float BoostInput;

	// Boost output to physics system. Range 0...1
	UPROPERTY(Transient)
	float DriftInput;

	// Reverse state
	UPROPERTY(Transient)
	bool ReverseInput;

	// The currently selected gear
	UPROPERTY(Transient)
	int32 CurrentGear;

	// The engine RPM
	UPROPERTY(Transient)
	float EngineRPM;

	// The engine Torque
	UPROPERTY(Transient)
	float EngineTorque;

	UPROPERTY()
	TObjectPtr<UNetworkPhysicsComponent> NetworkPhysicsComponent = nullptr;

public:

	// Rate at which input throttle can rise and fall
	UPROPERTY(EditAnywhere, Category = VehicleInput, AdvancedDisplay)
	FModularVehicleInputRateConfig ThrottleInputRate;

	// Rate at which input brake can rise and fall
	UPROPERTY(EditAnywhere, Category = VehicleInput, AdvancedDisplay)
	FModularVehicleInputRateConfig BrakeInputRate;

	// Rate at which input steering can rise and fall
	UPROPERTY(EditAnywhere, Category = VehicleInput, AdvancedDisplay)
	FModularVehicleInputRateConfig SteeringInputRate;

	// Rate at which input handbrake can rise and fall
	UPROPERTY(EditAnywhere, Category = VehicleInput, AdvancedDisplay)
	FModularVehicleInputRateConfig HandbrakeInputRate;

	// Rate at which input pitch can rise and fall
	UPROPERTY(EditAnywhere, Category = VehicleInput, AdvancedDisplay)
	FModularVehicleInputRateConfig PitchInputRate;

	// Rate at which input roll can rise and fall
	UPROPERTY(EditAnywhere, Category = VehicleInput, AdvancedDisplay)
	FModularVehicleInputRateConfig RollInputRate;

	// Rate at which input yaw can rise and fall
	UPROPERTY(EditAnywhere, Category = VehicleInput, AdvancedDisplay)
	FModularVehicleInputRateConfig YawInputRate;

	// Rate at which input can rise and fall
	UPROPERTY(EditAnywhere, Category = VehicleInput, AdvancedDisplay)
	FModularVehicleInputRateConfig BoostInputRate;

	// Rate at which input can rise and fall
	UPROPERTY(EditAnywhere, Category = VehicleInput, AdvancedDisplay)
	FModularVehicleInputRateConfig DriftInputRate;

	UPROPERTY(Transient, Replicated)
	TArray<FConstructionData> ConstructionDatas;

	/** Pass current state to server */
	UFUNCTION(reliable, server, WithValidation)
	void ServerUpdateState(float InSteeringInput, float InThrottleInput, float InBrakeInput
		, float InHandbrakeInput, int32 InCurrentGear, float InRollInput, float InPitchInput
		, float InYawInput, float InBoostInput, float InDriftInput, bool InReverseInput);

	TArray<AActor*> ActorsToIgnore;
	EChaosAsyncVehicleDataType CurAsyncType;
	FModularVehicleAsyncInput* CurAsyncInput;
	struct FModularVehicleAsyncOutput* CurAsyncOutput;
	struct FModularVehicleAsyncOutput* NextAsyncOutput;
	float OutputInterpAlpha;

	struct FAsyncOutputWrapper
	{
		int32 Idx;
		int32 Timestamp;

		FAsyncOutputWrapper()
			: Idx(INDEX_NONE)
			, Timestamp(INDEX_NONE)
		{
		}
	};

	TArray<FAsyncOutputWrapper> OutputsWaitingOn;
	TUniquePtr<FPhysicsVehicleOutput> PVehicleOutput;	/* physics simulation data output from the async physics thread */
	TUniquePtr<FModularVehicleSimulationCU> VehicleSimulationPT;	/* simulation code running on the physics thread async callback */

private:

	int NextTransformIndex = 0; // is there a better way, getting from size of map/array?
	UPrimitiveComponent* MyComponent = nullptr;

	bool bUsingNetworkPhysicsPrediction = false;
	float PrevSteeringInput = 0.0f;

	int32 LastComponentAddIndex = INDEX_NONE;
	TMap<TObjectKey<UPrimitiveComponent>, Chaos::FSimTreeUpdates> PendingTreeUpdates;

	int32 NextConstructionIndex = 0;

	TArray<FPhysicsConstraintHandle> ConstraintHandles;
	int32 ClusteringCount = 0;

	bool bIsLocallyControlled;
};

