// Copyright Epic Games, Inc. All Rights Reserved.

#include "TakesCoreLog.h"
#include "TakeMetaData.h"
#include "TakeData.h"

#include "LevelSequence.h"
#include "MovieSceneToolsModule.h"

#include "Modules/ModuleManager.h"

LLM_DEFINE_TAG(Takes_TakesCore);

class FTakesCoreModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		LLM_SCOPE_BYTAG(Takes_TakesCore);

		// Ensure the level sequence module is loaded
		FModuleManager::Get().LoadModuleChecked("LevelSequence");

		// Add empty take meta data to the ULevelSequence CDO to ensure that
		// asset registry tooltips show up in the editor
		UTakeMetaData* MetaData = GetMutableDefault<ULevelSequence>()->FindOrAddMetaData<UTakeMetaData>();
		MetaData->SetFlags(RF_Transient);

		LevelSequenceCDO = GetMutableDefault<ULevelSequence>();

		// Register take data with movie scene tools so sequencer knows how to switch takes
		TakeData = MakeShareable(new FTakesCoreTakeData);
		FMovieSceneToolsModule::Get().RegisterTakeData(TakeData.Get());
	}

	virtual void ShutdownModule() override
	{
		LLM_SCOPE_BYTAG(Takes_TakesCore);

		if (ULevelSequence* CDO = LevelSequenceCDO.Get())
		{
			CDO->RemoveMetaData<UTakeMetaData>();
		}

		FMovieSceneToolsModule::Get().UnregisterTakeData(TakeData.Get());
	}

	// Weak ptr to the level sequence CDO so we can gracefully remove the take meta-data on shutdown module
	// without crashing when ShutdownModule is called after the CDO has been destroyed.
	TWeakObjectPtr<ULevelSequence> LevelSequenceCDO;

	TSharedPtr<IMovieSceneToolsTakeData> TakeData;
};

IMPLEMENT_MODULE(FTakesCoreModule, TakesCore);
DEFINE_LOG_CATEGORY(LogTakesCore);
