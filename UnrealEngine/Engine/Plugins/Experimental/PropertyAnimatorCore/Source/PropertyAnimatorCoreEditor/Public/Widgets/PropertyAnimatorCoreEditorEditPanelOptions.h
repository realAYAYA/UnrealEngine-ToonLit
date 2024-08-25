// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class AActor;
class SPropertyAnimatorCoreEditorEditPanel;

struct FPropertyAnimatorCoreEditorEditPanelOptions
{
	FPropertyAnimatorCoreEditorEditPanelOptions()
	{}

	explicit FPropertyAnimatorCoreEditorEditPanelOptions(TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> InPropertyPanel)
		: EditPanelWeak(InPropertyPanel)
	{}

	void SetContextActor(AActor* InActor);
	AActor* GetContextActor() const
	{
		return ContextActorWeak.Get();
	}

protected:
	TWeakPtr<SPropertyAnimatorCoreEditorEditPanel> EditPanelWeak;
	TWeakObjectPtr<AActor> ContextActorWeak;
};