// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Templates/SubclassOf.h"
#include "Widgets/SWidget.h"
#include "ISequencerSection.h"
#include "MovieSceneTrack.h"
#include "ISequencer.h"
#include "ISequencerTrackEditor.h"
#include "MovieSceneTrackEditor.h"
#include "IContentBrowserSingleton.h"
#include "Containers/Map.h"

struct FAssetData;
class FAudioThumbnail;
class FDelegateHandle;
class FMenuBuilder;
class FSequencerSectionPainter;
class USoundWave;
class UMovieSceneAudioTrack;

/**
 * Tools for audio tracks
 */
class MOVIESCENETOOLS_API FAudioTrackEditor
	: public FMovieSceneTrackEditor
{
public:

	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	FAudioTrackEditor(TSharedRef<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FAudioTrackEditor();

	/**
	 * Creates an instance of this class.  Called by a sequencer 
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

public:

	// ISequencerTrackEditor interface

	virtual void OnInitialize() override;
	virtual void OnRelease() override;
	virtual void BuildAddTrackMenu(FMenuBuilder& MenuBuilder) override;
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override;
	virtual const FSlateBrush* GetIconBrush() const override;
	virtual bool IsResizable(UMovieSceneTrack* InTrack) const override;
	virtual void Resize(float NewSize, UMovieSceneTrack* InTrack) override;
	virtual bool OnAllowDrop(const FDragDropEvent& DragDropEvent, FSequencerDragDropParams& DragDropParams) override;
	virtual FReply OnDrop(const FDragDropEvent& DragDropEvent, const FSequencerDragDropParams& DragDropParams) override;
	
protected:

	/** Delegate for AnimatablePropertyChanged in HandleAssetAdded for sounds */
	FKeyPropertyResult AddNewSound(FFrameNumber KeyTime, class USoundBase* Sound, UMovieSceneAudioTrack* Track, int32 RowIndex);

	/** Delegate for AnimatablePropertyChanged in HandleAssetAdded for attached sounds */
	FKeyPropertyResult AddNewAttachedSound(FFrameNumber KeyTime, class USoundBase* Sound, UMovieSceneAudioTrack* Track, TArray<TWeakObjectPtr<UObject>> ObjectsToAttachTo);

private:

	/** Callback for executing the "Add Audio Track" menu entry. */
	void HandleAddAudioTrackMenuEntryExecute();

	/** Callback for executing the "Add Audio Track" menu entry on an actor */
	void HandleAddAttachedAudioTrackMenuEntryExecute(FMenuBuilder& MenuBuilder, TArray<FGuid> ObjectBindings);

	/** Audio sub menu */
	TSharedRef<SWidget> BuildAudioSubMenu(FOnAssetSelected OnAssetSelected, FOnAssetEnterPressed OnAssetEnterPressed);

	/** Audio asset selected */
	void OnAudioAssetSelected(const FAssetData& AssetData, UMovieSceneTrack* Track);

	/** Audio asset enter pressed */
	void OnAudioAssetEnterPressed(const TArray<FAssetData>& AssetData, UMovieSceneTrack* Track);

	/** Attached audio asset selected */
	void OnAttachedAudioAssetSelected(const FAssetData& AssetData, TArray<FGuid> ObjectBindings);

	/** Attached audio asset enter pressed */
	void OnAttachedAudioEnterPressed(const TArray<FAssetData>& AssetData, TArray<FGuid> ObjectBindings);

	/** Registers a delegate with the sequencer for monitoring edits */
	void RegisterMovieSceneChangedDelegate();

	/** Movie Scene Data Changed Delegate */
	void OnMovieSceneDataChanged(EMovieSceneDataChangeType InChangeType);

	/** Returns true if the given Sequence or any subsequence contains an audio track */
	bool SequenceContainsAudioTrack(const UMovieSceneSequence* InSequence);

	/** Will return true if a sequence contains an audio track and the user was notified about the potential clock source issue */
	bool CheckSequenceClockSource();

	/** Prompts user and potentially modifies settings pref USequencerSettings::bAutoSelectAudioClockSource */
	void PromptUserForClockSource();

	/** Sets the clock source for the given sequence to use the audio clock */
	void SetClockSoureToAudioClock();

private:

	FDelegateHandle MovieSceneChangedDelegate;
};


/**
 * Class for audio sections, handles drawing of all waveform previews.
 */
class MOVIESCENETOOLS_API FAudioSection
	: public ISequencerSection
	, public TSharedFromThis<FAudioSection>
{
public:

	/** Constructor. */
	FAudioSection(UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FAudioSection();

public:

	// ISequencerSection interface

	virtual UMovieSceneSection* GetSectionObject() override;
	virtual FText GetSectionTitle() const override;
	virtual FText GetSectionToolTip() const override;
	virtual float GetSectionHeight(const UE::Sequencer::FViewDensityInfo& ViewDensity) const override;
	virtual int32 OnPaintSection(FSequencerSectionPainter& Painter) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const FGeometry& ParentGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual void BeginResizeSection() override;
	virtual void ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeTime) override;
	virtual void BeginSlipSection() override;
	virtual void SlipSection(FFrameNumber SlipTime) override;
	
private:

	/* Re-creates the texture used to preview the waveform. */
	void RegenerateWaveforms(TRange<float> DrawRange, int32 XOffset, int32 XSize, const FColor& ColorTint, float DisplayScale);

private:

	/** The section we are visualizing. */
	UMovieSceneSection& Section;

	/** The waveform thumbnail render object. */
	TSharedPtr<class FAudioThumbnail> WaveformThumbnail;

	/** Stored data about the waveform to determine when it is invalidated. */
	TRange<float> StoredDrawRange;
	FFrameNumber StoredStartOffset;
	int32 StoredXOffset;
	int32 StoredXSize;
	FColor StoredColor;
	float StoredSectionHeight;
	bool bStoredLooping;

	/** Stored sound wave to determine when it is invalidated. */
	TWeakObjectPtr<USoundWave> StoredSoundWave;

	TWeakPtr<ISequencer> Sequencer;

	/** Cached start offset value valid only during resize */
	FFrameNumber InitialStartOffsetDuringResize;
	
	/** Cached start time valid only during resize */
	FFrameNumber InitialStartTimeDuringResize;
};
