// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencerModule.h"
#include "Modules/ModuleManager.h"
#include "Templates/SharedPointerFwd.h"
#include "Templates/Function.h"

class IAvaSequencerController;
enum class EMovieSceneTransformChannel : uint32;

struct FAvaSequencerUtils
{
	static const FName& GetSequencerModuleName()
	{
		static const FName SequencerModuleName("Sequencer");
		return SequencerModuleName;
	}

	static ISequencerModule& GetSequencerModule()
	{
		return FModuleManager::Get().LoadModuleChecked<ISequencerModule>(GetSequencerModuleName());
	}

	static bool IsSequencerModuleLoaded()
	{
		return FModuleManager::Get().IsModuleLoaded(GetSequencerModuleName());
	}

	AVALANCHESEQUENCER_API static TSharedRef<IAvaSequencerController> CreateSequencerController();
};
