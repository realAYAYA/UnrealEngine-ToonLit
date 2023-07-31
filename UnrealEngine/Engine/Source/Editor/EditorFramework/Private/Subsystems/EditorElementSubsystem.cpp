// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/EditorElementSubsystem.h"

#include "Elements/Framework/TypedElementHandle.h"
#include "Elements/Framework/TypedElementList.h"
#include "Elements/Framework/TypedElementRegistry.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Elements/Interfaces/TypedElementWorldInterface.h"
#include "Engine/World.h"

bool UEditorElementSubsystem::SetElementTransform(FTypedElementHandle InElementHandle, const FTransform& InWorldTransform)
{
	if (TTypedElement<ITypedElementWorldInterface> WorldInterfaceElement = UTypedElementRegistry::GetInstance()->GetElement<ITypedElementWorldInterface>(InElementHandle))
	{
		if (UWorld* ElementWorld = WorldInterfaceElement.GetOwnerWorld())
		{
			ETypedElementWorldType WorldType = ElementWorld->IsGameWorld() ? ETypedElementWorldType::Game : ETypedElementWorldType::Editor;
			if (WorldInterfaceElement.CanMoveElement(WorldType))
			{
				WorldInterfaceElement.NotifyMovementStarted();
				WorldInterfaceElement.SetWorldTransform(InWorldTransform);
				WorldInterfaceElement.NotifyMovementEnded();

				return true;
			}
		}
	}

	return false;
}

FTypedElementListRef UEditorElementSubsystem::GetEditorNormalizedSelectionSet(const UTypedElementSelectionSet& SelectionSet)
{
	const FTypedElementSelectionNormalizationOptions NormalizationOptions = FTypedElementSelectionNormalizationOptions()
		.SetExpandGroups(true)
		.SetFollowAttachment(true);

	return SelectionSet.GetNormalizedSelection(NormalizationOptions);
}

FTypedElementListRef UEditorElementSubsystem::GetEditorManipulableElements(const FTypedElementListRef& NormalizedSelection)
{
	 NormalizedSelection->RemoveAll<ITypedElementWorldInterface>([](const TTypedElement<ITypedElementWorldInterface>& InWorldElement)
		{
			return !UEditorElementSubsystem::IsElementEditorManipulable(InWorldElement);
		});

	 return NormalizedSelection;
}

TTypedElement<ITypedElementWorldInterface> UEditorElementSubsystem::GetLastSelectedEditorManipulableElement(const FTypedElementListRef& NormalizedSelection)
{
	return NormalizedSelection->GetBottomElement<ITypedElementWorldInterface>(&UEditorElementSubsystem::IsElementEditorManipulable);
}

bool UEditorElementSubsystem::IsElementEditorManipulable(const TTypedElement<ITypedElementWorldInterface>& WorldElement)
{
	UWorld* OwnerWorld = WorldElement.GetOwnerWorld();
	if (!OwnerWorld)
	{
		return false;
	}

	bool bIsPlayingWorld = OwnerWorld->IsPlayInEditor();

	if (!WorldElement.CanMoveElement(bIsPlayingWorld ? ETypedElementWorldType::Game : ETypedElementWorldType::Editor))
	{
		return false;
	}

	return true;
}