// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/EngineSubsystem.h"
#include "GerstnerWaterWaveSubsystem.generated.h"

struct FResourceSizeEx;

class FGerstnerWaterWaveViewExtension;

/** UGerstnerWaterWaveSubsystem manages all UGerstnerWaterWaves objects, regardless of which world they belong to (it's a UEngineSubsystem) */
UCLASS()
class UGerstnerWaterWaveSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	UGerstnerWaterWaveSubsystem();

	// UEngineSubsystem implementation
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void Register(FGerstnerWaterWaveViewExtension* InViewExtension);
	void Unregister(FGerstnerWaterWaveViewExtension* InViewExtension);

	void RebuildGPUData() { bRebuildGPUData = true; }

	//~ Begin UObject Interface.	
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	//~ End UObject Interface

private:
	void BeginFrameCallback();

private:
	TArray<FGerstnerWaterWaveViewExtension*> GerstnerWaterWaveViewExtensions;
	bool bRebuildGPUData = true;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
