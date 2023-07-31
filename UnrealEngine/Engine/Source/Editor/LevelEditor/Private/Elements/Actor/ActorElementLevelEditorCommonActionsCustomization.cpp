// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Actor/ActorElementLevelEditorCommonActionsCustomization.h"
#include "Elements/Actor/ActorElementData.h"
#include "GameFramework/Actor.h"

#include "Elements/Framework/TypedElementSelectionSet.h"

#include "EditorModeManager.h"
#include "Toolkits/IToolkitHost.h"

bool FActorElementLevelEditorCommonActionsCustomization::DeleteElements(ITypedElementWorldInterface* InWorldInterface, TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, UTypedElementSelectionSet* InSelectionSet, const FTypedElementDeletionOptions& InDeletionOptions)
{
	if (const IToolkitHost* ToolkitHostPtr = GetToolkitHost())
	{
		// TODO: Needs to pass in the actors to delete
		if (ToolkitHostPtr->GetEditorModeManager().ProcessEditDelete())
		{
			return true;
		}
	}

	return FTypedElementCommonActionsCustomization::DeleteElements(InWorldInterface, InElementHandles, InWorld, InSelectionSet, InDeletionOptions);
}

void FActorElementLevelEditorCommonActionsCustomization::DuplicateElements(ITypedElementWorldInterface* InWorldInterface, TArrayView<const FTypedElementHandle> InElementHandles, UWorld* InWorld, const FVector& InLocationOffset, TArray<FTypedElementHandle>& OutNewElements)
{
	if (const IToolkitHost* ToolkitHostPtr = GetToolkitHost())
	{
		// TODO: Needs to pass in the actors to duplicate
		if (ToolkitHostPtr->GetEditorModeManager().ProcessEditDuplicate())
		{
			return;
		}
	}

	FTypedElementCommonActionsCustomization::DuplicateElements(InWorldInterface, InElementHandles, InWorld, InLocationOffset, OutNewElements);
}
