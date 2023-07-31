// Copyright Epic Games, Inc. All Rights Reserved.


#include "Tools/BakingHelper.h"

#include "ControlRig.h"
#include "ILevelSequenceEditorToolkit.h"
#include "ISequencer.h"
#include "LevelSequence.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#include "MovieSceneSequence.h"
#include "MovieSceneToolHelpers.h"
#include "Channels/MovieSceneChannelProxy.h"

TWeakPtr<ISequencer> FBakingHelper::GetSequencer()
{
	// if getting sequencer from level sequence need to use the current(leader), not the focused
	if (ULevelSequence* LevelSequence = ULevelSequenceEditorBlueprintLibrary::GetCurrentLevelSequence())
	{
		if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
		{
			static constexpr bool bFocusIfOpen = false;
			IAssetEditorInstance* AssetEditor = AssetEditorSubsystem->FindEditorForAsset(LevelSequence, bFocusIfOpen);
			const ILevelSequenceEditorToolkit* LevelSequenceEditor = static_cast<ILevelSequenceEditorToolkit*>(AssetEditor);
			return LevelSequenceEditor ? LevelSequenceEditor->GetSequencer() : nullptr;
		}
	}
	return nullptr;
}
