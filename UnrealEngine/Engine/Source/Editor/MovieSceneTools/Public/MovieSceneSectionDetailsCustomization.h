// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "Widgets/Input/SCheckBox.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class UMovieScene;

/**
 *  Customizes FMovieSceneSection to expose the section bounds to the UI and allow changing their bounded states.
 */
class MOVIESCENETOOLS_API FMovieSceneSectionDetailsCustomization : public IDetailCustomization
{
public:
	static TSharedRef<IDetailCustomization> MakeInstance(TSharedPtr<INumericTypeInterface<double>> InNumericTypeInterface, TWeakObjectPtr<UMovieScene> InParentMovieScene)
	{
		return MakeShared<FMovieSceneSectionDetailsCustomization>(InNumericTypeInterface, InParentMovieScene);
	}

	FMovieSceneSectionDetailsCustomization(TSharedPtr<INumericTypeInterface<double>> InNumericTypeInterface, TWeakObjectPtr<UMovieScene> InParentMovieScene)
	{
		NumericTypeInterface = InNumericTypeInterface;
		ParentMovieScene = InParentMovieScene;
	}

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder);

private:
	enum class ERangeBoundValueType
	{
		Finite,
		Infinite,
		MultipleValues
	};

private:
	/** Get the range start value */
	ERangeBoundValueType GetRangeStartValue(FFrameNumber& OutValue) const;

	/** Convert the range start into an FText for display */
	FText OnGetRangeStartText() const;
	/** Convert the range start into an FText for tooltip display */
	FText OnGetRangeStartToolTipText() const;
	/** Convert the text into a new range start */
	void OnRangeStartTextCommitted(const FText& InText, ETextCommit::Type CommitInfo);
	/** Should the textbox be editable? False if we have an infinite range.  */
	bool IsRangeStartTextboxEnabled() const;

	/** Determines if the range is Open, Closed, or Undetermined which can happen in the case of multi-select.  */
	ECheckBoxState GetRangeStartBoundedState() const;
	/** Sets the range to have a fixed bound or convert to an open bound. */
	void SetRangeStartBounded(bool InbIsBounded);


	/** Get the FText representing the appropriate Unicode icon for the toggle button. */
	FText GetRangeStartButtonIcon() const;
	/** Called by the UI when the button is pressed to toggle the current state. */
	FReply ToggleRangeStartBounded();

private:
	/** Get the range end value */
	ERangeBoundValueType GetRangeEndValue(FFrameNumber& OutValue) const;

	/** Convert the range end into an FText for display */
	FText OnGetRangeEndText() const;
	/** Convert the range end into an FText for tooltip display */
	FText OnGetRangeEndToolTipText() const;
	/** Convert the text into a new range start */
	void OnRangeEndTextCommitted(const FText& InText, ETextCommit::Type CommitInfo);
	/** Should the textbox be editable? False if we have an infinite range.  */
	bool IsRangeEndTextboxEnabled() const;

	/** Determines if the range is Open, Closed, or Undetermined which can happen in the case of multi-select.  */
	ECheckBoxState GetRangeEndBoundedState() const;
	/** Sets the range to have a fixed bound or convert to an open bound. */
	void SetRangeEndBounded(bool InbIsBounded);


	/** Get the FText representing the appropriate Unicode icon for the toggle button. */
	FText GetRangeEndButtonIcon() const;
	/** Called by the UI when the button is pressed to toggle the current state. */
	FReply ToggleRangeEndBounded();
private:

	/** The Numeric Type interface used to convert between display formats and internal tick resolution. */
	TSharedPtr<INumericTypeInterface<double>> NumericTypeInterface;

	/** Store the property handle to the FrameNumber field so we can get/set the value on the object via text box callbacks. */
	TSharedPtr<IPropertyHandle> MovieSceneSectionPropertyHandle;

	/** The movie scene that owns the section we're customizing. Used to find out the overall bounds for changing a section bounds from infinite -> closed. */
	TWeakObjectPtr<UMovieScene> ParentMovieScene;
};
