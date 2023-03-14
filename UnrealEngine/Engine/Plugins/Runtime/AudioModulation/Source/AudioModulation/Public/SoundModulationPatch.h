// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IAudioModulation.h"
#include "SoundModulationParameter.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectSaveContext.h"
#include "WaveTableTransform.h"

#include "SoundModulationPatch.generated.h"

// Forward Declarations
class USoundControlBus;


USTRUCT(BlueprintType)
struct AUDIOMODULATION_API FSoundModulationTransform : public FWaveTableTransform
{
	GENERATED_USTRUCT_BODY()
};


USTRUCT(BlueprintType)
struct AUDIOMODULATION_API FSoundControlModulationInput
{
	GENERATED_USTRUCT_BODY()

	FSoundControlModulationInput();

	/** Get the modulated input value on parent patch initialization and hold that value for its lifetime */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Input, meta = (DisplayName = "Sample-And-Hold"))
	uint8 bSampleAndHold : 1;

	/** Transform to apply to the input prior to mix phase */
	UPROPERTY(EditAnywhere, Category = Input)
	FSoundModulationTransform Transform;

	/** The input bus */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Input)
	TObjectPtr<USoundControlBus> Bus = nullptr;

	const USoundControlBus* GetBus() const;
	const USoundControlBus& GetBusChecked() const;
};

USTRUCT(BlueprintType)
struct AUDIOMODULATION_API FSoundControlModulationPatch
{
	GENERATED_USTRUCT_BODY()

	/** Whether or not patch is bypassed (patch is still active, but always returns output parameter default value when modulated) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Inputs)
	bool bBypass = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Output, meta = (DisplayName = "Parameter"))
	TObjectPtr<USoundModulationParameter> OutputParameter = nullptr;

	/** Modulation inputs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Inputs)
	TArray<FSoundControlModulationInput> Inputs;
};

UCLASS(config = Engine, editinlinenew, BlueprintType)
class AUDIOMODULATION_API USoundModulationPatch : public USoundModulatorBase
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Modulation, meta = (ShowOnlyInnerProperties))
	FSoundControlModulationPatch PatchSettings;

	/* USoundModulatorBase Implementation */
	virtual TUniquePtr<Audio::IProxyData> CreateNewProxyData(const Audio::FProxyDataInitParams& InitParams) override;
	virtual const Audio::FModulationParameter& GetOutputParameter() const override;

	virtual TUniquePtr<Audio::IModulatorSettings> CreateProxySettings() const override;


#if WITH_EDITOR
	virtual void PreSave(FObjectPreSaveContext InSaveContext) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& InPropertyChangedEvent) override;
#endif // WITH_EDITOR
};
