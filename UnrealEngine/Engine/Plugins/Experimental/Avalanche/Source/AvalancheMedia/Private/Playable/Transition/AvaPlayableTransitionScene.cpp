// Copyright Epic Games, Inc. All Rights Reserved.

#include "Playable/Transition/AvaPlayableTransitionScene.h"

#include "IAvaSceneInterface.h"
#include "Playable/AvaPlayable.h"
#include "Playable/Transition/AvaPlayableTransition.h"
#include "Transition/Extensions/IAvaTransitionRCExtension.h"

namespace UE::AvaMedia::Private
{
	/** Controllers aren't applied to the Preset, instead, this compares the latest remote control values for a given playable */
	class FAvaRCTransitionPlayableExtension : public IAvaRCTransitionExtension
	{
		virtual EAvaTransitionComparisonResult CompareControllers(const FGuid& InControllerId, const FAvaTransitionScene& InMyScene, const FAvaTransitionScene& InOtherScene) const override
		{
			const UAvaPlayable* MyPlayable = InMyScene.GetDataView().GetPtr<UAvaPlayable>();
			const UAvaPlayable* OtherPlayable = InOtherScene.GetDataView().GetPtr<UAvaPlayable>();
			if (!MyPlayable || !OtherPlayable)
			{
				return EAvaTransitionComparisonResult::None;
			}

			const FAvaPlayableRemoteControlValue* MyValue = MyPlayable->GetLatestRemoteControlValues().ControllerValues.Find(InControllerId);
			const FAvaPlayableRemoteControlValue* OtherValue = OtherPlayable->GetLatestRemoteControlValues().ControllerValues.Find(InControllerId);
			if (!MyValue || !OtherValue)
			{
				return EAvaTransitionComparisonResult::None;
			}

			return MyValue->IsSameValueAs(*OtherValue)
				? EAvaTransitionComparisonResult::Same
				: EAvaTransitionComparisonResult::Different;
		}
	};
}

FAvaPlayableTransitionScene::FAvaPlayableTransitionScene(UAvaPlayable* InPlayable, UAvaPlayableTransition* InPlayableTransition)
	: FAvaTransitionScene(InPlayable)
	, PlayableTransitionWeak(InPlayableTransition)
{
	AddExtension<UE::AvaMedia::Private::FAvaRCTransitionPlayableExtension>();
}

FAvaPlayableTransitionScene::FAvaPlayableTransitionScene(const FAvaTagHandle& InTransitionLayer, UAvaPlayableTransition* InPlayableTransition)
	: FAvaPlayableTransitionScene(nullptr, InPlayableTransition)
{
	OverrideTransitionLayer = InTransitionLayer;
}

EAvaTransitionComparisonResult FAvaPlayableTransitionScene::Compare(const FAvaTransitionScene& InOther) const
{
	const UAvaPlayable* MyPlayable    = GetDataView().GetPtr<UAvaPlayable>();
	const UAvaPlayable* OtherPlayable = InOther.GetDataView().GetPtr<UAvaPlayable>();

	if (!MyPlayable || !OtherPlayable)
	{
		return EAvaTransitionComparisonResult::None;
	}

	// Determine if Template is the same via the Package Name To Load (i.e. Source Level)
	if (MyPlayable->GetSourceAssetPath() == OtherPlayable->GetSourceAssetPath())
	{
		return EAvaTransitionComparisonResult::Same;
	}

	return EAvaTransitionComparisonResult::Different;
}

ULevel* FAvaPlayableTransitionScene::GetLevel() const
{
	const UAvaPlayable* Playable = GetDataView().GetPtr<UAvaPlayable>();
	const IAvaSceneInterface* SceneInterface = Playable ? Playable->GetSceneInterface() : nullptr;
	return SceneInterface ? SceneInterface->GetSceneLevel() : nullptr;
}

void FAvaPlayableTransitionScene::GetOverrideTransitionLayer(FAvaTagHandle& OutTransitionLayer) const
{
	if (OverrideTransitionLayer.IsSet())
	{
		OutTransitionLayer = *OverrideTransitionLayer;
	}
}

void FAvaPlayableTransitionScene::OnFlagsChanged()
{
	UAvaPlayable* Playable = GetDataView().GetMutablePtr<UAvaPlayable>();
	UAvaPlayableTransition* PlayableTransition = PlayableTransitionWeak.Get();
	if (!Playable || !PlayableTransition)
	{
		return;
	}

	// Event received when the playable can be discarded/recycled.
	if (HasAnyFlags(EAvaTransitionSceneFlags::NeedsDiscard))
	{
		// Do some error checking.
		if (PlayableTransition->IsEnterPlayable(Playable))
		{
			UE_LOG(LogAvaPlayable, Error, TEXT("Playable Transition \"%s\" Error: An \"enter\" playable is being discarded."), *PlayableTransition->GetFullName());
		}
		PlayableTransition->MarkPlayableAsDiscard(Playable);
	}
}
