// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class ISequencer;

class IAvaSequencerController : public TSharedFromThis<IAvaSequencerController>
{
public:
	virtual ~IAvaSequencerController() = default;

	virtual void SetSequencer(const TSharedPtr<ISequencer>& InSequencer) = 0;
};
