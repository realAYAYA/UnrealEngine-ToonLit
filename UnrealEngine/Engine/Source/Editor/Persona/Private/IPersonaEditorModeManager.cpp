// Copyright Epic Games, Inc. All Rights Reserved.

#include "IPersonaEditorModeManager.h"

#include "ContextObjectStore.h"
#include "EdModeInteractiveToolsContext.h"

IPersonaEditorModeManager* UPersonaEditorModeManagerContext::GetPersonaEditorModeManager() const
{
	return ModeManager;
}

bool UPersonaEditorModeManagerContext::GetCameraTarget(FSphere& OutTarget) const
{
	check(ModeManager);
	return ModeManager->GetCameraTarget(OutTarget);
}

void UPersonaEditorModeManagerContext::GetOnScreenDebugInfo(TArray<FText>& OutDebugText) const
{
	check(ModeManager);
	return ModeManager->GetOnScreenDebugInfo(OutDebugText);
}

IPersonaEditorModeManager::IPersonaEditorModeManager() : FAssetEditorModeManager()
{
	UContextObjectStore* ContextObjectStore = GetInteractiveToolsContext()->ContextObjectStore;
	ContextObjectStore->AddContextObject(PersonaModeManagerContext.Get());
}

IPersonaEditorModeManager::~IPersonaEditorModeManager()
{
	UContextObjectStore* ContextObjectStore = GetInteractiveToolsContext()->ContextObjectStore;
	ContextObjectStore->RemoveContextObject(PersonaModeManagerContext.Get());
}

void IPersonaEditorModeManager::AddReferencedObjects(FReferenceCollector& Collector)
{
	FAssetEditorModeManager::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(PersonaModeManagerContext);
}