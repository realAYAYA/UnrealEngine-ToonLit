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
#include "MaterialTypes.h"
#include "MovieSceneTrackEditor.h"

class UMovieSceneCustomPrimitiveDataTrack;

using FGetStartIndexDelegate = TFunction<uint8()>;

/**
 * Track editor for custom primitive data tracks
 */
class MOVIESCENETOOLS_API FCustomPrimitiveDataTrackEditor
	: public FMovieSceneTrackEditor
{
public:

	/** Constructor. */
	FCustomPrimitiveDataTrackEditor(TSharedRef<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FCustomPrimitiveDataTrackEditor() { }

	static TSharedRef<ISequencerTrackEditor> CreateTrackEditor(TSharedRef<ISequencer> OwningSequencer);

public:

	// ISequencerTrackEditor interface

	virtual TSharedPtr<SWidget> BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params) override;
	virtual TSharedRef<ISequencerSection> MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding) override;
	virtual bool SupportsType(TSubclassOf<UMovieSceneTrack> Type) const override;
	virtual bool GetDefaultExpansionState(UMovieSceneTrack* InTrack) const override; 
	virtual void ExtendObjectBindingTrackMenu(TSharedRef<FExtender> Extender, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass) override;

protected:

private:


	/** Provides the contents of the outliner edit widget */
	TSharedRef<SWidget> OnGetAddMenuContent(FGuid ObjectBinding, UMovieSceneCustomPrimitiveDataTrack* CPDTrack, int32 TrackInsertRowIndex);

	/** Provides the contents of the add parameter menu. */
	TSharedRef<SWidget> OnGetAddParameterMenuContent(FGuid ObjectBinding, UMovieSceneCustomPrimitiveDataTrack* CPDTrack);

	/** Provides the contents of the add parameter menu. */
	void OnBuildAddParameterMenu(FMenuBuilder& MenuBuilder, FGuid ObjectBinding, UMovieSceneCustomPrimitiveDataTrack* CPDTrack);

	/** Adds a scalar parameter and initial key to a custom primitive data track.
	 * @param ObjectBinding The object binding which owns the custom primitive data track.
	 * @param CPDTrack The track in which to look for sections to add the parameter to.
	 * @param PrimitiveDataIndex The index for the primitive data to animate
	 */
	void AddScalarParameter(UPrimitiveComponent* Component, UMovieSceneCustomPrimitiveDataTrack* CPDTrack, FGetStartIndexDelegate GetStartIndexDelegate);

	/** Adds a Vector2D parameter and initial key to a custom primitive data track.
	 * @param ObjectBinding The object binding which owns the custom primitive data track.
	 * @param CPDTrack The track in which to look for sections to add the parameter to.
	 * @param PrimitiveDataStartIndex The start index in primitive data where we are animating a Vector2D
	 */
	void AddVector2DParameter(UPrimitiveComponent* Component, UMovieSceneCustomPrimitiveDataTrack* CPDTrack, FGetStartIndexDelegate GetStartIndexDelegate);

	/** Adds a Vector parameter and initial key to a custom primitive data track.
	 * @param ObjectBinding The object binding which owns the custom primitive data track.
	 * @param CPDTrack The track in which to look for sections to add the parameter to.
	 * @param PrimitiveDataStartIndex The start index in primitive data where we are animating a Vector
	 */
	void AddVectorParameter(UPrimitiveComponent* Component, UMovieSceneCustomPrimitiveDataTrack* CPDTrack, FGetStartIndexDelegate GetStartIndexDelegate);

	/** Adds a color parameter and initial key to a custom primitive data track.
	 * @param ObjectBinding The object binding which owns the custom primitive data track.
	 * @param CPDTrack The track in which to look for sections to add the parameter to.
	 * @param PrimitiveDataStartIndex The start index in primitive data where we are animating a Color
	 */
	void AddColorParameter(UPrimitiveComponent* Component, UMovieSceneCustomPrimitiveDataTrack* CPDTrack, FGetStartIndexDelegate GetStartIndexDelegate);

	/** Returns whether we can add a parameter starting at this index of size ParameterSize. 
	 * Will return false if another parameter added overlaps that index.
	 */
	bool CanAddParameter(UMovieSceneCustomPrimitiveDataTrack* CPDTrack, FGetStartIndexDelegate GetStartIndexDelegate, int ParameterSize);

	void ConstructObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, TArray<FGuid> ObjectBindings);

	void HandleAddCustomPrimitiveDataTrackExecute(UPrimitiveComponent* Component);

	uint8 StartIndex = 0;
};