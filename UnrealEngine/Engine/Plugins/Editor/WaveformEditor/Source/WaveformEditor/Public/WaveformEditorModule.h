// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IWaveformEditorModule.h"
#include "Templates/SharedPointer.h"

class IWaveformEditorInstantiator;

class FWaveformEditorModule : public IWaveformEditorModule
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	virtual void RegisterContentBrowserExtensions(IWaveformEditorInstantiator* Instantiator) override;

	TSharedPtr<IWaveformEditorInstantiator> WaveformEditorInstantiator = nullptr;
};

