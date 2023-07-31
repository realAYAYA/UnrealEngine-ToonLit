// Copyright Epic Games, Inc. All Rights Reserved.

#include "IPersonaEditMode.h"

IPersonaEditMode::IPersonaEditMode()
{
}

IPersonaEditMode::~IPersonaEditMode()
{
}

void IPersonaEditMode::Enter()
{
	FAnimationEditMode::Enter();
}

void IPersonaEditMode::Exit()
{
	FAnimationEditMode::Exit();
}

void IPersonaEditMode::AddReferencedObjects(FReferenceCollector& Collector)
{
	FAnimationEditMode::AddReferencedObjects(Collector);
}
