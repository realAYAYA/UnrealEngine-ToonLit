// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioDefines.h"
#include "AudioDeviceManager.h"
#include "AudioDevice.h"
#include "Containers/Array.h"
#include "IAudioModulation.h"
#include "SoundModulationGenerator.h"
#include "SoundModulationParameter.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "SoundControlBus.generated.h"


// Forward Declarations
class USoundModulatorBase;

struct FPropertyChangedEvent;


UCLASS(BlueprintType, hidecategories = Object, editinlinenew, MinimalAPI)
class USoundControlBus : public USoundModulatorBase
{
	GENERATED_UCLASS_BODY()

public:
	/** If true, bypasses control bus from being modulated by parameters, patches, or mixed (control bus remains active and computed). */
	UPROPERTY(EditAnywhere, Category = General, BlueprintReadWrite)
	bool bBypass;

#if WITH_EDITORONLY_DATA
	/** If true, Address field is used in place of object name for address used when applying mix changes using filtering. */
	UPROPERTY(EditAnywhere, Category = Mix, BlueprintReadWrite)
	bool bOverrideAddress;
#endif // WITH_EDITORONLY_DATA

	/** Address to use when applying mix changes. */
	UPROPERTY(EditAnywhere, Category = Mix, BlueprintReadWrite, meta = (EditCondition = "bOverrideAddress"))
	FString Address;

	UPROPERTY(EditAnywhere, Category = Generators, BlueprintReadWrite)
	TArray<TObjectPtr<USoundModulationGenerator>> Generators;

	UPROPERTY(EditAnywhere, Category = General, BlueprintReadOnly)
	TObjectPtr<USoundModulationParameter> Parameter;

	/* UObject Implementation */
#if WITH_EDITOR
	virtual void PostDuplicate(EDuplicateMode::Type DuplicateMode) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
	virtual void PostInitProperties() override;
	virtual void PostRename(UObject* OldOuter, const FName OldName) override;
#endif // WITH_EDITOR

	virtual void BeginDestroy() override;

	/* USoundModulatorBase Implementation */
	virtual TUniquePtr<Audio::IModulatorSettings> CreateProxySettings() const override;
	virtual TUniquePtr<Audio::IProxyData> CreateNewProxyData(const Audio::FProxyDataInitParams& InitParams) override;
	virtual const Audio::FModulationParameter& GetOutputParameter() const override;

	const Audio::FModulationMixFunction GetMixFunction() const;

	float GetDefaultNormalizedValue() const { return Parameter ? Parameter->Settings.ValueNormalized : 1.0f; }

};
