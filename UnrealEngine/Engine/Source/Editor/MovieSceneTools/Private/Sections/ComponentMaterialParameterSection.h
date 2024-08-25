// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "ISequencerSection.h"
#include "Input/Reply.h"

class FName;
class FSequencerSectionPainter;
class UMovieSceneSection;
struct FKeyHandle;

/**
 * A movie scene section for material parameters.
 */
class FComponentMaterialParameterSection
	: public FSequencerSection
{
public:

	FComponentMaterialParameterSection(UMovieSceneSection& InSectionObject)
		: FSequencerSection(InSectionObject)
	{ }

public:

	//~ ISequencerSection interface
	virtual FReply OnKeyDoubleClicked(const TArray<FKeyHandle>& KeyHandles) override;
	virtual bool RequestDeleteCategory(const TArray<FName>& CategoryNamePath) override;
	virtual bool RequestDeleteKeyArea(const TArray<FName>& KeyAreaNamePath) override;

	virtual TSharedPtr<UE::Sequencer::FCategoryModel> ConstructCategoryModel(FName InCategoryName, const FText& InDisplayText, TArrayView<const FChannelData> Channels) const;
};
