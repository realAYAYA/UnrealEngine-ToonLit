// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "VehicleSimBaseComponent.h"
#include "VehicleSimTransmissionComponent.generated.h"


UENUM()
enum class EModuleTransType : uint8
{
	Manual = 0,
	Automatic
};


UCLASS(ClassGroup = (ModularVehicle), meta = (BlueprintSpawnableComponent), hidecategories = (Object, Tick, Replication, Cooking, Activation, LOD))
class CHAOSMODULARVEHICLEENGINE_API UVehicleSimTransmissionComponent : public UVehicleSimBaseComponent
{
	GENERATED_BODY()

public:
	UVehicleSimTransmissionComponent();
	virtual ~UVehicleSimTransmissionComponent() = default;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	TArray<float> ForwardRatios;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	TArray<float> ReverseRatios;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float FinalDriveRatio;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	int ChangeUpRPM;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	int ChangeDownRPM;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float GearChangeTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	float TransmissionEfficiency;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	EModuleTransType TransmissionType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Attributes)
	bool AutoReverse;

	virtual ESimModuleType GetModuleType() const override { return ESimModuleType::Transmission; }

	virtual Chaos::ISimulationModuleBase* CreateNewCoreModule() const override;

};

