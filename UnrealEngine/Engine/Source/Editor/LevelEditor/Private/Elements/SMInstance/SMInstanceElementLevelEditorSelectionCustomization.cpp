// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/SMInstance/SMInstanceElementLevelEditorSelectionCustomization.h"
#include "Elements/SMInstance/SMInstanceElementData.h"
#include "Components/InstancedStaticMeshComponent.h"

#include "Elements/Framework/TypedElementList.h"
#include "Elements/Framework/EngineElementsLibrary.h"

#include "LevelUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogSMInstanceLevelEditorSelection, Log, All);

bool FSMInstanceElementLevelEditorSelectionCustomization::CanSelectElement(const TTypedElement<ITypedElementSelectionInterface>& InElementSelectionHandle, const FTypedElementSelectionOptions& InSelectionOptions)
{
	const FSMInstanceManager SMInstance = SMInstanceElementDataUtil::GetSMInstanceFromHandleChecked(InElementSelectionHandle);
	if (!SMInstance)
	{
		return false;
	}

	AActor* Owner = SMInstance.GetISMComponent()->GetOwner();
	AActor* SelectionRoot = Owner->GetRootSelectionParent();
	ULevel* SelectionLevel = (SelectionRoot != nullptr) ? SelectionRoot->GetLevel() : Owner->GetLevel();
	if (!Owner->IsTemplate() && FLevelUtils::IsLevelLocked(SelectionLevel))
	{
		return false;
	}
	
	// Bail if global selection is locked
	return !GEdSelectionLock;
}

bool FSMInstanceElementLevelEditorSelectionCustomization::CanDeselectElement(const TTypedElement<ITypedElementSelectionInterface>& InElementSelectionHandle, const FTypedElementSelectionOptions& InSelectionOptions)
{
	const FSMInstanceManager SMInstance = SMInstanceElementDataUtil::GetSMInstanceFromHandleChecked(InElementSelectionHandle);
	if (!SMInstance)
	{
		return false;
	}

	// Bail if global selection is locked
	return !GEdSelectionLock;
}

bool FSMInstanceElementLevelEditorSelectionCustomization::SelectElement(const TTypedElement<ITypedElementSelectionInterface>& InElementSelectionHandle, FTypedElementListRef InSelectionSet, const FTypedElementSelectionOptions& InSelectionOptions)
{
	const FSMInstanceManager SMInstance = SMInstanceElementDataUtil::GetSMInstanceFromHandleChecked(InElementSelectionHandle);
	if (!SMInstance)
	{
		return false;
	}

	if (!InElementSelectionHandle.SelectElement(InSelectionSet, InSelectionOptions))
	{
		return false;
	}

	UE_LOG(LogSMInstanceLevelEditorSelection, Verbose, TEXT("Selected SMInstance: %s (%s), Index %d"), *SMInstance.GetISMComponent()->GetPathName(), *SMInstance.GetISMComponent()->GetClass()->GetName(), SMInstance.GetISMInstanceIndex());
	
	return true;
}

bool FSMInstanceElementLevelEditorSelectionCustomization::DeselectElement(const TTypedElement<ITypedElementSelectionInterface>& InElementSelectionHandle, FTypedElementListRef InSelectionSet, const FTypedElementSelectionOptions& InSelectionOptions)
{
	const FSMInstanceManager SMInstance = SMInstanceElementDataUtil::GetSMInstanceFromHandleChecked(InElementSelectionHandle);
	if (!SMInstance)
	{
		return false;
	}

	if (!InElementSelectionHandle.DeselectElement(InSelectionSet, InSelectionOptions))
	{
		return false;
	}

	UE_LOG(LogSMInstanceLevelEditorSelection, Verbose, TEXT("Deselected SMInstance: %s (%s), Index %d"), *SMInstance.GetISMComponent()->GetPathName(), *SMInstance.GetISMComponent()->GetClass()->GetName(), SMInstance.GetISMInstanceIndex());

	return true;
}
