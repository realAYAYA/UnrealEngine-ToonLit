// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Templates/Function.h"

enum class EDelayedRegisterRunPhase : uint8
{
	StartOfEnginePreInit,
	FileSystemReady,
	TaskGraphSystemReady,
	StatSystemReady,
	IniSystemReady,
	EarliestPossiblePluginsLoaded,
	ShaderTypesReady,
	PreObjectSystemReady,
	ObjectSystemReady,
	DeviceProfileManagerReady,
	EndOfEngineInit,

	NumPhases,
};

struct CORE_API FDelayedAutoRegisterHelper
{

	FDelayedAutoRegisterHelper(EDelayedRegisterRunPhase RunPhase, TFunction<void()> RegistrationFunction);

	static void RunAndClearDelayedAutoRegisterDelegates(EDelayedRegisterRunPhase RunPhase);
};
