// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "VehicleSimBaseComponent.h"
#include "VehicleSimWheelComponent.generated.h"

UENUM()
enum class EWheelAxisType : uint8
{
	X = 0,		// X forwards
	Y			// Y forwards
};


UCLASS(ClassGroup = (ModularVehicle), meta = (BlueprintSpawnableComponent), hidecategories = (Object, Replication, Cooking, Activation, LOD, Physics, Collision, AssetUserData, Event))
class CHAOSMODULARVEHICLEENGINE_API UVehicleSimWheelComponent : public UVehicleSimBaseComponent
{
	GENERATED_BODY()

public:

	UVehicleSimWheelComponent();
	virtual ~UVehicleSimWheelComponent() = default;

	// - Wheel --------------------------------------------------------
	//UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	//float WheelMass;			// Mass of wheel [Kg]

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float WheelRadius;			// [cm]

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float WheelWidth;			// [cm]

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float WheelInertia;

	// grip and turning related
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float FrictionMultiplier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float CorneringStiffness;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float SlipAngleLimit;

	// #TODO: 
	// LateralSlipGraphMultiplier, LateralSlipGraph

	// - Braking ------------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float MaxBrakeTorque;

	// Handbrake
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	bool bHandbrakeEnabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes, meta = (EditCondition = "bHandbrakeEnabled"))
	float HandbrakeTorque;

	// - Steering -----------------------------------------------------
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	bool bSteeringEnabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes, meta = (EditCondition = "bSteeringEnabled"))
	float MaxSteeringAngle;

	// - Other --------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	bool bABSEnabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	bool bTractionControlEnabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	EWheelAxisType AxisType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	bool ReverseDirection;

	virtual ESimModuleType GetModuleType() const override { return ESimModuleType::Wheel; }
	virtual Chaos::ISimulationModuleBase* CreateNewCoreModule() const override;
};
