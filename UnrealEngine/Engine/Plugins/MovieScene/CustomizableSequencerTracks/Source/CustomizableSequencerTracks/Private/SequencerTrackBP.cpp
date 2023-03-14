// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerTrackBP.h"
#include "SequencerSectionBP.h"

#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneInstanceRegistry.h"


UMovieSceneSection* USequencerTrackBP::CreateNewSection()
{
	if (UClass* Class = DefaultSectionType.Get())
	{
		return NewObject<USequencerSectionBP>(this, Class, NAME_None, RF_Transactional);
	}

	for (TSubclassOf<USequencerSectionBP> SupportedSection : SupportedSections)
	{
		if (UClass* Class = SupportedSection.Get())
		{
			return NewObject<USequencerSectionBP>(this, Class, NAME_None, RF_Transactional);
		}
	}

	ensureMsgf(false, TEXT("Track does not have any supported section types. Returning a base class to avoid crashing."));
	return NewObject<UMovieSceneSection>(this, NAME_None, RF_Transactional);
}

void USequencerTrackBP::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
}