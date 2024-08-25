// Copyright Epic Games, Inc. All Rights Reserved.

#include "FunctionLibraries/LevelSequenceVCamLibrary.h"

#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "MovieSceneSpawnableAnnotation.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "Compilation/MovieSceneCompiledDataManager.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Evaluation/MovieSceneSequenceHierarchy.h"

#if WITH_EDITOR
#include "ILevelSequenceEditorToolkit.h"
#include "ISequencer.h"
#include "LevelSequenceEditorBlueprintLibrary.h"
#endif

namespace UE::VirtualCamera::Private
{
#if WITH_EDITOR
	static TSharedPtr<ISequencer> GetGlobalSequencer()
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

	static void IterateActorsAffectedBySequence(ISequencer& Sequencer, UMovieSceneSequence* Sequence, FMovieSceneSequenceIDRef SequenceID, const FMovieSceneSequenceHierarchy* Hierarchy, TFunctionRef<void(AActor&)> OnActorFound)
	{
		if (UMovieScene* MovieScene = Sequence->GetMovieScene())
		{
			// Search all possessables
			for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index)
			{
				FGuid ThisGuid = MovieScene->GetPossessable(Index).GetGuid();

				for (TWeakObjectPtr<> WeakObject : Sequencer.FindBoundObjects(ThisGuid, SequenceID))
				{
					AActor* Actor = Cast<AActor>(WeakObject.Get());
					if (Actor != nullptr)
					{
						OnActorFound(*Actor);
					}
				}
			}

			// Search all spawnables
			for (int32 Index = 0; Index < MovieScene->GetSpawnableCount(); ++Index)
			{
				FGuid ThisGuid = MovieScene->GetSpawnable(Index).GetGuid();

				for (TWeakObjectPtr<> WeakObject : Sequencer.FindBoundObjects(ThisGuid, SequenceID))
				{
					AActor* Actor = Cast<AActor>(WeakObject.Get());
					if (Actor != nullptr)
					{
						OnActorFound(*Actor);
					}
				}
			}
		}

		if (Hierarchy)
		{
			// Recurse into child nodes
			if (const FMovieSceneSequenceHierarchyNode* Node = Hierarchy->FindNode(SequenceID))
			{
				for (FMovieSceneSequenceIDRef ChildID : Node->Children)
				{
					const FMovieSceneSubSequenceData* SubData     = Hierarchy->FindSubData(ChildID);
					UMovieSceneSequence*              SubSequence = SubData ? SubData->GetSequence() : nullptr;

					if (SubSequence)
					{
						IterateActorsAffectedBySequence(Sequencer, SubSequence, ChildID, Hierarchy, OnActorFound);
					}
				}
			}
		}
	}
#endif
}

bool ULevelSequenceVCamLibrary::HasAnyCameraCutsInLevelSequence(ULevelSequence* Sequence)
{
	if (!Sequence)
	{
		return false;
	}
	
	UMovieScene* FocusedScene = Sequence->GetMovieScene();
	UMovieSceneTrack* CameraCutTrack = FocusedScene->GetCameraCutTrack();
	return CameraCutTrack && !CameraCutTrack->IsEmpty();
}

TArray<FPilotableSequenceCameraInfo> ULevelSequenceVCamLibrary::FindPilotableCamerasInActiveLevelSequence()
{
	TArray<FPilotableSequenceCameraInfo> Result;
#if WITH_EDITOR
	if (const TSharedPtr<ISequencer> Sequencer = UE::VirtualCamera::Private::GetGlobalSequencer())
	{
		FMovieSceneRootEvaluationTemplateInstance& RootInstance = Sequencer->GetEvaluationTemplate();
		const FMovieSceneSequenceHierarchy*        Hierarchy    = RootInstance.GetCompiledDataManager()->FindHierarchy(RootInstance.GetCompiledDataID());

		UMovieSceneSequence* RootSequence = Sequencer->GetRootMovieSceneSequence();
		UE::VirtualCamera::Private::IterateActorsAffectedBySequence(*Sequencer,
			RootSequence,
			MovieSceneSequenceID::Root,
			Hierarchy,
			[&Result](AActor& Actor)
			{
				if (UCameraComponent* Camera = Actor.FindComponentByClass<UCameraComponent>())
				{
					Result.Add({ Camera });
				}
			});
	}
#endif
	return Result;
}
