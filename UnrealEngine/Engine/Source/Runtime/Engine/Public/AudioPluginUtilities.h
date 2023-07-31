// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "IAudioExtensionPlugin.h"

/************************************************************************/
/* Plugin Utilities                                                     */
/************************************************************************/
struct ENGINE_API AudioPluginUtilities
{
	static const FName& GetDefaultModulationPluginName();

	/*
	 * These functions return a pointer to the plugin factory
	 * that matches the plugin name specified in the target
	 * platform's settings.
	 * 
	 * if no matching plugin is found, nullptr is returned.
	 */
	static FName GetDesiredSpatializationPluginName();
	static TArray<IAudioSpatializationFactory*> GetSpatialPluginArray();
	static IAudioSpatializationFactory* GetDesiredSpatializationPlugin();
	static IAudioSourceDataOverrideFactory* GetDesiredSourceDataOverridePlugin();
	static IAudioReverbFactory* GetDesiredReverbPlugin();
	static IAudioOcclusionFactory* GetDesiredOcclusionPlugin();
	static IAudioModulationFactory* GetDesiredModulationPlugin();

	/** This function returns the name of the plugin specified in the platform settings. */
	static FString GetDesiredPluginName(EAudioPlugin PluginType);
};