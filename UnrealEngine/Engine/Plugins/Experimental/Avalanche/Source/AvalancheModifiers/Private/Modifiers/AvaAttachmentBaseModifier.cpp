// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/AvaAttachmentBaseModifier.h"

#include "GameFramework/Actor.h"

void UAvaAttachmentBaseModifier::OnModifierCDOSetup(FActorModifierCoreMetadata& InMetadata)
{
	Super::OnModifierCDOSetup(InMetadata);
	
	InMetadata.SetCompatibilityRule([](const AActor* InActor)->bool
	{
		return InActor && InActor->FindComponentByClass<USceneComponent>();		
	});
}

void UAvaAttachmentBaseModifier::OnModifierAdded(EActorModifierCoreEnableReason InReason)
{
	Super::OnModifierAdded(InReason);

	AddExtension<FAvaSceneTreeUpdateModifierExtension>(this);
}
