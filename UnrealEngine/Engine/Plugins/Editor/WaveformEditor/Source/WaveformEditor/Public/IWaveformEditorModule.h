// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"

class IWaveformEditorInstantiator;

class IWaveformEditorModule : public IModuleInterface
{
	/** Registers waveform editor asset actions. */
	virtual void RegisterContentBrowserExtensions(IWaveformEditorInstantiator* Instantiator) = 0;
};
