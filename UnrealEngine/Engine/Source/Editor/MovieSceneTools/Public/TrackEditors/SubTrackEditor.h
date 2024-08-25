// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Widgets/SWidget.h"
#include "ISequencer.h"
#include "MovieSceneTrack.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "MovieSceneTrackEditor.h"

class AActor;
struct FAssetData;
class FMenuBuilder;
class UMovieSceneSubTrack;

/**
 * Tools for subsequences
 */
class MOVIESCENETOOLS_API FSubTrackEditor
	: public FMovieSceneTrackEditor
{
public:

	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	FSubTrackEditor(TSharedRef<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FSubTrackEditor() { }

	/**
	 * Creates an instance of this class.  Called by a sequencer 
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

public:

	// ISequencerTrackEditor interface

	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override;
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual bool OnAllowDrop(const FDragDropEvent& DragDropEvent, FSequencerDragDropParams& DragDropParams) override;
	virtual FReply OnDrop(const FDragDropEvent& DragDropEvent, const FSequencerDragDropParams& DragDropParams) override;
	virtual bool IsResizable(UMovieSceneTrack* InTrack) const override;
	virtual void Resize(float NewSize, UMovieSceneTrack* InTrack) override;

public:
	
	/** Insert sequence into this track */
	virtual void InsertSection(UMovieSceneTrack* Track);

	/** Duplicate the section into this track */
	virtual void DuplicateSection(UMovieSceneSubSection* Section);

	/** Create a new take of the given section */
	virtual void CreateNewTake(UMovieSceneSubSection* Section);

	/** Switch the selected section's take sequence */
	virtual void ChangeTake(UMovieSceneSequence* Sequence);

	UE_DEPRECATED(5.3, "SwitchTake has been deprecated, please use ChangeTake(UMovieSceneSequence*)")
	virtual void SwitchTake(UObject* TakeObject);

	/** Generate a menu for takes for this section */
	virtual void AddTakesMenu(UMovieSceneSubSection* Section, FMenuBuilder& MenuBuilder);

	/** Edit the section's metadata */
	virtual void EditMetaData(UMovieSceneSubSection* Section);

	/**
	 * Check whether the given sequence can be added as a sub-sequence.
	 *
	 * The purpose of this method is to disallow circular references
	 * between sub-sequences in the focused movie scene.
	 *
	 * @param Sequence The sequence to check.
	 * @return true if the sequence can be added as a sub-sequence, false otherwise.
	 */
	bool CanAddSubSequence(const UMovieSceneSequence& Sequence) const;

public:

	/** Get the name of the sub track */
	virtual FText GetSubTrackName() const;

	/** Get the tooltip for this sub track editor */
	virtual FText GetSubTrackToolTip() const;

	/** Get the brush used for the sub track editor */
	virtual FName GetSubTrackBrushName() const;

	/** Get the display name for the sub section */
	virtual FString GetSubSectionDisplayName(const UMovieSceneSubSection* Section) const;

	/** Get the default sub sequence name */
	virtual FString GetDefaultSubsequenceName() const;

	/** Get the sub sequence directory */
	virtual FString GetDefaultSubsequenceDirectory() const;

	/** Get the UMovieSceneSubTrack class */
	virtual TSubclassOf<UMovieSceneSubTrack> GetSubTrackClass() const;

protected:

	/** Get the list of supported sequence class paths */
	virtual void GetSupportedSequenceClassPaths(TArray<FTopLevelAssetPath>& OutClassPaths) const;

	/** Callback for executing the "Add Subsequence" menu entry. */
	virtual void HandleAddSubTrackMenuEntryExecute();

	/** Callback for determining whether the "Add Subsequence" menu entry can execute. */
	virtual bool HandleAddSubTrackMenuEntryCanExecute() const { return true; }

	/** Whether to handle this asset being dropped onto the sequence as opposed to a specific track. */
	virtual bool CanHandleAssetAdded(UMovieSceneSequence* Sequence) const;

	UE_DEPRECATED(5.3, "CreateNewTrack has been deprecated, please implement GetSubTrackClass")
	virtual UMovieSceneSubTrack* CreateNewTrack(UMovieScene* MovieScene) const;

	/** Find or create a sub track. If the given track is a subtrack, it will be returned. */
	UMovieSceneSubTrack* FindOrCreateSubTrack(UMovieScene* MovieScene, UMovieSceneTrack* Track) const;

	/** Callback for generating the menu of the "Add Sequence" combo button. */
	TSharedRef<SWidget> HandleAddSubSequenceComboButtonGetMenuContent(UMovieSceneTrack* InTrack);

private:

	/** Callback for executing a menu entry in the "Add Sequence" combo button. */
	void HandleAddSubSequenceComboButtonMenuEntryExecute(const FAssetData& AssetData, UMovieSceneTrack* InTrack);

	/** Callback for executing a menu entry in the "Add Sequence" combo button when enter pressed. */
	void HandleAddSubSequenceComboButtonMenuEntryEnterPressed(const TArray<FAssetData>& AssetData, UMovieSceneTrack* InTrack);

	/** Delegate for AnimatablePropertyChanged in AddKey */
	FKeyPropertyResult AddKeyInternal(FFrameNumber KeyTime, UMovieSceneSequence* InMovieSceneSequence, UMovieSceneTrack* InTrack, int32 RowIndex);

	/** Callback for AnimatablePropertyChanged in HandleAssetAdded. */
	FKeyPropertyResult HandleSequenceAdded(FFrameNumber KeyTime, UMovieSceneSequence* Sequence, UMovieSceneTrack* Track, int32 RowIndex);
};
