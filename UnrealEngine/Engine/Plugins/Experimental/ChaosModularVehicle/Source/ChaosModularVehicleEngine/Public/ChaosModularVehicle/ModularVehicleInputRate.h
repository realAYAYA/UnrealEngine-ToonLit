// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "SimModule/SimModulesInclude.h"
#include "Curves/CurveFloat.h"

#include "ModularVehicleInputRate.generated.h"

struct CHAOSMODULARVEHICLEENGINE_API FModularVehicleDebugParams
{
	bool ShowDebug = false;
	bool SuspensionRaycastsEnabled = true;
	bool ShowSuspensionRaycasts = false;
	bool ShowWheelData = false;
	bool ShowRaycastMaterial = false;
	bool ShowWheelCollisionNormal = false;

	bool DisableAnim = false;
	float FrictionOverride = 1.0f;
};

enum EModularVehicleInputType : uint8
{
	Throttle = 0,
	Brake,
	Clutch,
	Steering,
	Handbrake,
	Pitch,
	Roll,
	Yaw,
	Gear,
	DebugIndex,

	Max
};

UENUM()
enum class EModularVehicleInputFunctionType : uint8
{
	LinearFunction = 0,
	SquaredFunction,
	CustomCurve
};


struct CHAOSMODULARVEHICLEENGINE_API FModularVehicleHistory
{
	FModularVehicleHistory()
		: ControlInputs()
	{
	}

	// Control Inputs
	Chaos::FControlInputs ControlInputs;
};


USTRUCT()
struct CHAOSMODULARVEHICLEENGINE_API FModularVehicleInputRate
{
	GENERATED_USTRUCT_BODY()

	FModularVehicleInputRate() : Name(TEXT("Empty")), RiseRate(5.0f), FallRate(5.0f), InputCurveFunction(EModularVehicleInputFunctionType::LinearFunction) {}

	FModularVehicleInputRate(const FString& InName) : Name(InName), RiseRate(5.0f), FallRate(5.0f), InputCurveFunction(EModularVehicleInputFunctionType::LinearFunction) {}

	~FModularVehicleInputRate() {}

	UPROPERTY(VisibleAnywhere, Category = VehicleInputRate)
	FString Name;

	UPROPERTY(EditAnywhere, Category = VehicleInputRate)
	float RiseRate;

	UPROPERTY(EditAnywhere, Category = VehicleInputRate)
	float FallRate;

	UPROPERTY(EditAnywhere, Category = VehicleInputRate)
	EModularVehicleInputFunctionType InputCurveFunction;

	UPROPERTY(EditAnywhere, Category = VehicleInputRate)
	FRuntimeFloatCurve UserCurve;

	float InterpInputValue(float DeltaTime, float CurrentValue, float NewValue) const;

	float CalcControlFunction(float InputValue);

};
