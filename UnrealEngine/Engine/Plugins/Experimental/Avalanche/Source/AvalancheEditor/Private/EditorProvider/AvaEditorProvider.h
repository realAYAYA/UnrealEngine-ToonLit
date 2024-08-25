// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IAvaEditorProvider.h"

/** Base Implementation for the Core Editor Provider */
class FAvaEditorProvider : public IAvaEditorProvider
{
public:
	//~ Begin IAvaEditorProvider
	virtual UObject* GetSceneObject(UWorld* InWorld, EAvaEditorObjectQueryType InQueryType) override;
	virtual void GetActorsToEdit(TArray<AActor*>& InOutActorsToEdit) const override;
	//~ End IAvaEditorProvider
};
