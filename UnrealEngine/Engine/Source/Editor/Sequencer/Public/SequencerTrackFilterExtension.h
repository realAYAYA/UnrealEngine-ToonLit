// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "SequencerTrackFilterBase.h"
#include "SequencerTrackFilterExtension.generated.h"

// Override this class in order to make an additional track filter available in Sequencer
UCLASS(Abstract)
class SEQUENCER_API USequencerTrackFilterExtension : public UObject
{
	GENERATED_BODY()

public:
	// Override this method to add new track filters
	virtual void AddTrackFilterExtensions(TArray< TSharedRef<class FSequencerTrackFilter> >& InOutFilterList) const { }
};
