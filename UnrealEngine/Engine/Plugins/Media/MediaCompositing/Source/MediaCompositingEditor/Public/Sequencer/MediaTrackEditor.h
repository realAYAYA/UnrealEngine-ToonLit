// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimatedPropertyKey.h"
#include "AssetRegistry/AssetData.h"
#include "MovieSceneTrackEditor.h"
#include "Templates/SharedPointer.h"

class ISequencer;
class FTrackEditorThumbnailPool;
class UMediaSource;
class UMovieSceneMediaTrack;

DECLARE_EVENT_OneParam(FMediaTrackEditor, FOnBuildOutlinerEditWidget, FMenuBuilder&);

/**
 * Track editor that understands how to animate MediaPlayer properties on objects
 */
class MEDIACOMPOSITINGEDITOR_API FMediaTrackEditor
	: public FMovieSceneTrackEditor
{
public:

	/**
	 * Create a new media track editor instance.
	 *
	 * @param OwningSequencer The sequencer object that will own the track editor.
	 * @return The new track editor.
	 */
	static TSharedRef<ISequencerTrackEditor>  CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer)
	{
		return MakeShared<FMediaTrackEditor>(OwningSequencer);
	}

	/**
	 * Get the list of all property types that this track editor animates.
	 *
	 * @return List of animated properties.
	 */
	static TArray<FAnimatedPropertyKey, TInlineAllocator<1>> GetAnimatedPropertyTypes();

	/**
	 * Event for when we build the widget for adding to the track.
	 * Hook into this if you want to add custom options.
	 */
	static FOnBuildOutlinerEditWidget OnBuildOutlinerEditWidget;

public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InSequencer The sequencer object that owns this track editor.
	 */
	FMediaTrackEditor(TSharedRef<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FMediaTrackEditor();

public:

	//~ FMovieSceneTrackEditor interface

	virtual UMovieSceneTrack* AddTrack(UMovieScene* FocusedMovieScene, const FGuid& ObjectHandle, TSubclassOf<class UMovieSceneTrack> TrackClass, FName UniqueTypeName) override;
	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const override;
	virtual void Tick(float DeltaTime) override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual void OnRelease() override;

protected:

	/** Callback for AnimatablePropertyChanged in HandleAssetAdded for attached media sources. */
	FKeyPropertyResult AddAttachedMediaSource(FFrameNumber KeyTime, class UMediaSource* MediaSource, TArray<TWeakObjectPtr<UObject>> ObjectsToAttachTo, int32 RowIndex);

	/** Callback for AnimatablePropertyChanged in HandleAssetAdded for master media sources. */
	FKeyPropertyResult AddMasterMediaSource(FFrameNumber KeyTime, class UMediaSource* MediaSource, int32 RowIndex);

	void AddNewSection(const FAssetData& Asset, UMovieSceneMediaTrack* Track);

	void AddNewSectionEnterPressed(const TArray<FAssetData>& Asset, UMovieSceneMediaTrack* Track);

	TSharedPtr<FTrackEditorThumbnailPool> GetThumbnailPool() const { return ThumbnailPool; }
private:

	/** Callback for executing the "Add Media Track" menu entry. */
	void HandleAddMediaTrackMenuEntryExecute();
	/** Callback for when some sequencer data like bindings change. */
	void OnSequencerDataChanged(EMovieSceneDataChangeType DataChangeType);

	/**
	 * Updates a media track with the current binding information.
	 * 
	 * @param MediaTrack			Track to update.
	 * @param Binding				Binding to get object from.
	 */
	void UpdateMediaTrackBinding(UMovieSceneMediaTrack* MediaTrack, const FMovieSceneBinding& Binding);

private:

	TSharedPtr<FTrackEditorThumbnailPool> ThumbnailPool;
};
