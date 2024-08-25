// Copyright Epic Games, Inc. All Rights Reserved.

#include "Evaluation/PreAnimatedState/MovieSceneRestoreStateParams.h"

#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneInstanceRegistry.h"
#include "EntitySystem/MovieSceneSequenceInstance.h"

namespace UE
{
namespace MovieScene
{

IMovieScenePlayer* FRestoreStateParams::GetTerminalPlayer() const
{
	if (Linker && TerminalInstanceHandle.IsValid())
	{
		return Linker->GetInstanceRegistry()->GetInstance(TerminalInstanceHandle).GetPlayer();
	}

	ensureAlways(false);
	return nullptr;
}

TSharedPtr<const FSharedPlaybackState> FRestoreStateParams::GetTerminalPlaybackState() const
{
	if (Linker && TerminalInstanceHandle.IsValid())
	{
		const FSequenceInstance& TerminalInstance = Linker->GetInstanceRegistry()->GetInstance(TerminalInstanceHandle);
		return TerminalInstance.GetSharedPlaybackState().ToSharedPtr();
	}

	ensureAlways(false);
	return nullptr;
}

} // namespace MovieScene
} // namespace UE
