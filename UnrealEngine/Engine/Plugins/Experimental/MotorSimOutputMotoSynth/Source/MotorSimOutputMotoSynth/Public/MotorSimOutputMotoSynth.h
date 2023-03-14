// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioMotorSimTypes.h"
#include "IAudioMotorSimOutput.h"
#include "SynthComponents/SynthComponentMoto.h"
#include "MotorSimOutputMotoSynth.generated.h"

UCLASS(ClassGroup = Synth, meta = (BlueprintSpawnableComponent))
class MOTORSIMOUTPUTMOTOSYNTH_API	UMotorSimOutputMotoSynth : public USynthComponentMoto, public IAudioMotorSimOutput
{
	GENERATED_BODY()

	virtual ~UMotorSimOutputMotoSynth() override {};
	
public:
	virtual void Update(FAudioMotorSimInputContext& Input, FAudioMotorSimRuntimeContext& RuntimeInfo) override;

	virtual void StartOutput() override;
	virtual void StopOutput() override;
};