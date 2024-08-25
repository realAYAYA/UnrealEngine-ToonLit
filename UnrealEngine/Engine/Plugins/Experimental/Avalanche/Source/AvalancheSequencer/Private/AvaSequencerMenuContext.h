// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/Object.h"
#include "AvaSequencerMenuContext.generated.h"

class FAvaSequencer;

UCLASS()
class UAvaSequencerMenuContext : public UObject
{
	GENERATED_BODY()

public:
	TSharedPtr<FAvaSequencer> GetAvaSequencer() const
	{
		return AvaSequencerWeak.Pin();
	}

	void SetAvaSequencer(const TWeakPtr<FAvaSequencer>& InAvaSequencerWeak)
	{
		AvaSequencerWeak = InAvaSequencerWeak;
	}

private:
	TWeakPtr<FAvaSequencer> AvaSequencerWeak;
};
