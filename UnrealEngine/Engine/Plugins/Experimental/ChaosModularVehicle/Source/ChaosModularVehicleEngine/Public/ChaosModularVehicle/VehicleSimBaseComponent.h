// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"

#include "VehicleSimBaseComponent.generated.h"

namespace Chaos
{
	class ISimulationModuleBase;
}

UENUM()
enum class ESimModuleType : uint8
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
	Balloon			// TODO: rename anti gravity??
};


UCLASS(BlueprintType, Blueprintable)
class CHAOSMODULARVEHICLEENGINE_API UVehicleSimBaseComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()
public:

	virtual ESimModuleType GetModuleType() const { return ESimModuleType::Undefined; }

	/**
	 * Caller takes ownership of pointer to new Sim Module
	 */
	virtual Chaos::ISimulationModuleBase* CreateNewCoreModule() const { return nullptr; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ModularVehicle)
	int TransformIndex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ModularVehicle)
	bool bRemoveFromClusterCollisionModel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = ModularVehicle)
	bool bAnimationEnabled;

	int TreeIndex; // helper - since Component->GetAttachChildren doesn't contain any data
};

