// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "SequencerTrackFilterBase.h"
#include "SequencerTrackFilterExtension.h"
#include "NiagaraSequencerFilters.generated.h"

UCLASS()
class UNiagaraSequencerTrackFilter : public USequencerTrackFilterExtension
{
public:
	GENERATED_BODY()

	// USequencerTrackFilterExtension interface
	virtual void AddTrackFilterExtensions(TArray< TSharedRef<class FSequencerTrackFilter> >& InOutFilterList) const override;
	// End of USequencerTrackFilterExtension interface
};
