// Copyright Epic Games, Inc. All Rights Reserved.

// Common implementation that can be shared by mnay ARSystemSupport implementations

#pragma once

#include "ARSystem.h"

class AUGMENTEDREALITY_API FARSystemSupportBase : public IARSystemSupport
{
public:
	virtual bool OnPinComponentToARPin(USceneComponent* ComponentToPin, UARPin* Pin) override;
};




