// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimationEditMode.h"

class PERSONA_API IPersonaEditMode : public FAnimationEditMode
{
	// These implementations are duplicated in the Persona module so that code that links only to Persona and not to
	// the AnimationEditMode module can work properly
public:
	IPersonaEditMode();
	virtual ~IPersonaEditMode() override;
	virtual void Enter() override;
	virtual void Exit() override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
};
