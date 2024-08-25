// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettingsBackedByCVars.h"
#include "NNEDenoiserModelData.h"

#include "NNEDenoiserSettings.generated.h"

/** An enum to represent denoiser NNE runtime type */
UENUM()
enum EDenoiserRuntimeType : uint8
{
	CPU,
	GPU,
	RDG
};

/** Settings used to create a NNE Denoise */
UCLASS(Config = Engine, meta = (DisplayName = "NNE Denoise"))
class NNEDENOISER_API UNNEDenoiserSettings : public UDeveloperSettingsBackedByCVars
{
	GENERATED_BODY()

public:
	UNNEDenoiserSettings();

	virtual void PostInitProperties() override;

	/** Denoiser model data used to create a NNE Denoise */
	UPROPERTY(Config, EditAnywhere, Category = "NNE Denoise", meta = (DisplayName = "Denoiser model data", ToolTip = "Select the denoiser model data"))
	TSoftObjectPtr<UNNEDenoiserModelData> DenoiserModelData;

private:
	/** Runtime type used to run the NNE Denoise model. Backed by the console variable 'NNEDenoiser.Runtime.Type'. */
	UPROPERTY(Config, EditAnywhere, Category = "NNE Denoise", meta = (DisplayName = "Runtime Type", ToolTip = "Select a Runtime type", ConsoleVariable = "NNEDenoiser.Runtime.Type"))
	TEnumAsByte<EDenoiserRuntimeType> RuntimeType;

	/** Runtime name used to run the NNE Denoise model. Backed by the console variable 'NNEDenoiser.Runtime.Name'. */
	UPROPERTY(Config, EditAnywhere, Category = "NNE Denoise", meta = (DisplayName = "Runtime Name Override", ToolTip = "(Optional) Specify the Runtime name", ConsoleVariable = "NNEDenoiser.Runtime.Name"))
	FString RuntimeName;
};
