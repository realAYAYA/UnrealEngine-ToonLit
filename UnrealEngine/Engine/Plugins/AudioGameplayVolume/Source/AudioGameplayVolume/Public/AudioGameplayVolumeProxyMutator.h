// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * DEPRECATED AudioGameplayVolumeProxyMutator.h.  Use the following files instead:
	 AudioGameplayVolumeSubsystem.h for FAudioProxyMutatorSearchResult
	 AudioGameplayVolumeSubsystem.h for FAudioProxyMutatorSearchObject
	 AudioGameplayVolumeMutator.h	for FAudioProxyMutatorPriorities
	 AudioGameplayVolumeMutator.h	for FAudioProxyActiveSoundParams
	 AudioGameplayVolumeMutator.h	for FProxyVolumeMutator
 */

#pragma once

#ifdef _MSC_VER
#pragma message(__FILE__"(15): warning: use AudioGameplayVolumeSubsystem.h and AudioGameplayVolumeMutator.h instead of AudioGameplayVolumeProxyMutator.h")
#else
#pragma message("#include AudioGameplayVolumeSubsystem.h and AudioGameplayVolumeMutator.h instead of AudioGameplayVolumeProxyMutator.h")
#endif

 // Include the new file so that the project still compiles, but has warnings
#include "AudioGameplayVolumeSubsystem.h"
#include "AudioGameplayVolumeMutator.h"
