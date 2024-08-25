// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerModule.h"
#include "Modules/ModuleManager.h"
#include "Sequencer/PropertyAnimatorEditorCurveChannelInterface.h"
#include "Sequencer/PropertyAnimatorEditorCurveSectionMenuExtension.h"

class ISequencerModule;

class FPropertyAnimatorEditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface
	virtual void StartupModule() override;
	//~ End IModuleInterface

	template<typename InCurveChannelType>
	void RegisterCurveChannelInterface(ISequencerModule& InSequencerModule)
	{
		using FMenuExtensionType = TPropertyAnimatorEditorCurveSectionMenuExtension<InCurveChannelType>;
		using FChannelInterfaceType = TPropertyAnimatorEditorCurveChannelInterface<InCurveChannelType, FMenuExtensionType>;
		InSequencerModule.RegisterChannelInterface<InCurveChannelType>(MakeUnique<FChannelInterfaceType>());
	}
};
