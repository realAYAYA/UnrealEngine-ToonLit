// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"

class IAvaViewportPostProcessVisualizer : public IAvaTypeCastable
{
public:
	UE_AVA_INHERITS(IAvaViewportPostProcessVisualizer, IAvaTypeCastable)

	virtual bool CanActivate(bool bInSilent) const = 0;
};
