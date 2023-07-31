// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Guid.h"
#include "MovieSceneTrackEditor.h"

class FMenuBuilder;
class UMovieSceneCinePrestreamingSection;

/**
 * A sequencer track editor for data layer tracks.
 */
class FCinePrestreamingTrackEditor
	: public FMovieSceneTrackEditor
{
public:
	FCinePrestreamingTrackEditor(TSharedRef<ISequencer> InSequencer);

	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

protected:
	/** ISequencerTrackEditor interface */
	TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	bool SupportsSequence(UMovieSceneSequence* InSequence) const override;
	bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
	const FSlateBrush* GetIconBrush() const override;
	void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;

private:
	/** Adds a new section which spans the length of the owning movie scene with the specified desired state. */
	UMovieSceneCinePrestreamingSection* AddNewSection(UMovieScene* MovieScene, UMovieSceneTrack* DataLayerTrack /*, EDataLayerState DesiredState*/);

	/** Handles when the add track menu item is activated. */
	void HandleAddTrack();

	/** Builds the add data layer menu which is displayed on the track. */
	TSharedRef<SWidget> BuildAddDataLayerMenu(UMovieSceneTrack* DataLayerTrack);

	/** Called to add a new section with a desired state. */
	void HandleAddNewSection(UMovieSceneTrack* DataLayerTrack/*, EDataLayerState DesiredState*/);
};
