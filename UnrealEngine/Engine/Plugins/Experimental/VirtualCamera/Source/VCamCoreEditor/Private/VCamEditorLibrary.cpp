// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamEditorLibrary.h"
#include "VCamComponent.h"

void UVCamEditorLibrary::GetAllVCamComponentsInLevel(TArray<UVCamComponent*>& VCamComponents)
{
	VCamComponents.Empty();

	// Early out if we're not running in the editor
	if (!IsInGameThread() || !GIsEditor || GEditor->PlayWorld || GIsPlayInEditorWorld)
	{
		return;
	}

	// Loop all VCamComponents which are not pending kill or CDOs
	const EObjectFlags ExcludeFlags = RF_ClassDefaultObject;
	for (TObjectIterator<UVCamComponent> It(ExcludeFlags, true, EInternalObjectFlags::Garbage); It; ++It)
	{
		UVCamComponent* VCamComponent = *It;
		if (IsValid(VCamComponent))
		{
			// Ensure the object lives in the editor world
			UWorld* World = VCamComponent->GetWorld();
			if (World && World->WorldType == EWorldType::Editor)
			{
				VCamComponents.Add(VCamComponent);
			}
		}
	}
}
