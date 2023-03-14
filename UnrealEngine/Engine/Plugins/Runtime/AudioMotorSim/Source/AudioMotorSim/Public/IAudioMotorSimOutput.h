// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AudioMotorSimTypes.h"
#include "UObject/Interface.h"
#include "IAudioMotorSimOutput.generated.h"

UINTERFACE()
class UAudioMotorSimOutput : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class AUDIOMOTORSIM_API IAudioMotorSimOutput
{
	GENERATED_IINTERFACE_BODY()
	
public:
	virtual void Update(FAudioMotorSimInputContext& Input, FAudioMotorSimRuntimeContext& RuntimeInfo) = 0;

	virtual void StartOutput() = 0;
	virtual void StopOutput() = 0;
};