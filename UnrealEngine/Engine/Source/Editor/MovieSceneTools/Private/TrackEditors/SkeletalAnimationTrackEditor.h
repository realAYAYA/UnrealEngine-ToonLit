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
#include "EditModes/SkeletalAnimationTrackEditMode.h"

struct FAssetData;
class FMenuBuilder;
class FSequencerSectionPainter;
class UMovieSceneSkeletalAnimationSection;
class USkeleton;
class USkeletalMeshComponent;
class UAnimSeqExportOption;

/**
 * Tools for animation tracks
 */
class FSkeletalAnimationTrackEditor : public FMovieSceneTrackEditor, public FGCObject
{
public:
	/**
	 * Constructor
	 *
	 * @param InSequencer The sequencer instance to be used by this tool
	 */
	FSkeletalAnimationTrackEditor( TSharedRef<ISequencer> InSequencer );

	/** Virtual destructor. */
	virtual ~FSkeletalAnimationTrackEditor() { }

	//~ FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FSkeletalAnimationTrackEditor");
	}

	/**
	 * Creates an instance of this class.  Called by a sequencer 
	 *
	 * @param OwningSequencer The sequencer instance to be used by this tool
	 * @return The new instance of this class
	 */
	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer );

	/**
	* Keeps track of how many skeletal animation track editors we have*
	*/
	static int32 NumberActive;
public:

	// ISequencerTrackEditor interface

	virtual void BuildObjectBindingContextMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass) override;
	virtual void BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass) override;
	virtual bool HandleAssetAdded(UObject* Asset, const FGuid& TargetObjectGuid) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding ) override;
	virtual bool SupportsSequence(UMovieSceneSequence* InSequence) const override;
	virtual bool SupportsType( TSubclassOf<UMovieSceneTrack> Type ) const override;
	virtual void BuildTrackContextMenu( FMenuBuilder& MenuBuilder, UMovieSceneTrack* Track ) override;
	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	virtual bool OnAllowDrop(const FDragDropEvent& DragDropEvent, FSequencerDragDropParams& DragDropParams) override;
	virtual FReply OnDrop(const FDragDropEvent& DragDropEvent, const FSequencerDragDropParams& DragDropParams) override;
	virtual void OnInitialize() override;
	virtual void OnRelease() override;

private:

	/** Animation sub menu */
	TSharedRef<SWidget> BuildAnimationSubMenu(FGuid ObjectBinding, USkeleton* Skeleton, UMovieSceneTrack* Track);
	void AddAnimationSubMenu(FMenuBuilder& MenuBuilder, TArray<FGuid> ObjectBindings, USkeleton* Skeleton, UMovieSceneTrack* Track);

	/** Filter only compatible skeletons */
	bool FilterAnimSequences(const FAssetData& AssetData, USkeleton* Skeleton);

	/** Animation sub menu filter function */
	bool ShouldFilterAsset(const FAssetData& AssetData);

	/** Animation asset selected */
	void OnAnimationAssetSelected(const FAssetData& AssetData, TArray<FGuid> ObjectBindings, UMovieSceneTrack* Track);

	/** Animation asset enter pressed */
	void OnAnimationAssetEnterPressed(const TArray<FAssetData>& AssetData, TArray<FGuid> ObjectBindings, UMovieSceneTrack* Track);

	/** Delegate for AnimatablePropertyChanged in AddKey */
	FKeyPropertyResult AddKeyInternal(FFrameNumber KeyTime, UObject* Object, class UAnimSequenceBase* AnimSequence, UMovieSceneTrack* Track, int32 RowIndex);
	
	/** Construct the binding menu*/
	void ConstructObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, TArray<FGuid> ObjectBindings);

	/** Callback to Create the Animation Asset, pop open the dialog */
	void HandleCreateAnimationSequence(USkeletalMeshComponent* SkelMeshComp, USkeleton* Skeleton, FGuid Binding, bool bCeateSoftLink);

	/** Callback to Creae the Animation Asset after getting the name*/
	bool CreateAnimationSequence(const TArray<UObject*> NewAssets,USkeletalMeshComponent* SkelMeshComp, FGuid Binding, bool bCreateSoftLink);

	/** Open the linked Anim Sequence*/
	void OpenLinkedAnimSequence(FGuid Binding);

	/** Can Open the linked Anim Sequence*/
	bool CanOpenLinkedAnimSequence(FGuid Binding);

	friend class FMovieSceneSkeletalAnimationParamsDetailCustomization;

private:
	/* Was part of the the section but should be at the track level since it takes the final blended result at the current time, not the section instance value*/
	bool CreatePoseAsset(const TArray<UObject*> NewAssets, FGuid InObjectBinding);
	void HandleCreatePoseAsset(FGuid InObjectBinding);

private:
	/* For Anim Sequence UI Option with be gc'd*/
	TObjectPtr<UAnimSeqExportOption> AnimSeqExportOption;


private:
	/* Delegate to handle sequencer changes for auto baking of anim sequences*/
	FDelegateHandle SequencerSavedHandle;
	void OnSequencerSaved(ISequencer& InSequence);
	FDelegateHandle SequencerChangedHandle;
	void OnSequencerDataChanged(EMovieSceneDataChangeType DataChangeType);
	void OnPostPropertyChanged(UObject* InObject, struct FPropertyChangedEvent& InPropertyChangedEvent);
};


/** Class for animation sections */
class FSkeletalAnimationSection
	: public ISequencerSection
	, public TSharedFromThis<FSkeletalAnimationSection>
{
public:

	/** Constructor. */
	FSkeletalAnimationSection( UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FSkeletalAnimationSection() { }

public:

	// ISequencerSection interface

	virtual UMovieSceneSection* GetSectionObject() override;
	virtual FText GetSectionTitle() const override;
	virtual FText GetSectionToolTip() const override;
	virtual TOptional<FFrameTime> GetSectionTime(FSequencerSectionPainter& InPainter) const override;
	virtual float GetSectionHeight(const UE::Sequencer::FViewDensityInfo& ViewDensity) const override;
	virtual FMargin GetContentPadding() const override;
	virtual int32 OnPaintSection( FSequencerSectionPainter& Painter ) const override;
	virtual void BeginResizeSection() override;
	virtual void ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeTime) override;
	virtual void BeginSlipSection() override;
	virtual void SlipSection(FFrameNumber SlipTime) override;
	virtual void CustomizePropertiesDetailsView(TSharedRef<IDetailsView> DetailsView, const FSequencerSectionPropertyDetailsViewCustomizationParams& InParams) const override;
	virtual void BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& InObjectBinding) override;
	virtual void BeginDilateSection() override;
	virtual void DilateSection(const TRange<FFrameNumber>& NewRange, float DilationFactor) override;


private:
	void FindBestBlendSection(FGuid InObjectBinding);
private:

	/** The section we are visualizing */
	UMovieSceneSkeletalAnimationSection& Section;

	/** Used to draw animation frame, need selection state and local time*/
	TWeakPtr<ISequencer> Sequencer;

	/** Cached first loop start offset value valid only during resize */
	FFrameNumber InitialFirstLoopStartOffsetDuringResize;

	/** Cached start time valid only during resize */
	FFrameNumber InitialStartTimeDuringResize;
};
