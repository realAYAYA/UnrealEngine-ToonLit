// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModelTypeID.h"
#include "MVVM/ViewModels/SectionModel.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/ViewModelHierarchy.h"
#include "MVVM/ViewModels/ViewModelIterators.h"

class UMovieSceneSection;
class UMovieSceneTrack;
class ISequencerTrackEditor;

namespace UE::Sequencer
{

struct FViewModelChildren;

class SEQUENCER_API ITrackExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(ITrackExtension)

	virtual ~ITrackExtension(){}

	virtual UMovieSceneTrack* GetTrack() const = 0;
	virtual int32 GetRowIndex() const = 0;
	virtual FViewModelChildren GetSectionModels() = 0;

	TArray<UMovieSceneSection*> GetSections() 
	{ 
		TArray<UMovieSceneSection*> Sections; 
		for (const TViewModelPtr<FSectionModel>& Item : GetSectionModels().IterateSubList<FSectionModel>())
		{
			if (UMovieSceneSection* Section = Item->GetSection())
			{
				Sections.Add(Section);
			}
		}

		return Sections;
	}

	virtual TSharedPtr<ISequencerTrackEditor> GetTrackEditor() const = 0;
};

} // namespace UE::Sequencer

