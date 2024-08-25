// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/PropertyAnimatorCoreEditorEditPanelOptions.h"

#include "GameFramework/Actor.h"
#include "Widgets/SPropertyAnimatorCoreEditorEditPanel.h"

void FPropertyAnimatorCoreEditorEditPanelOptions::SetContextActor(AActor* InActor)
{
	ContextActorWeak = InActor;

	if (const TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> EditPanel = EditPanelWeak.Pin())
	{
		EditPanel->Update();
	}
}