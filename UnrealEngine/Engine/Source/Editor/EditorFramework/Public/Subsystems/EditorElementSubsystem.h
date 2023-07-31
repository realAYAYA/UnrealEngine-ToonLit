// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "Elements/Framework/TypedElementHandle.h"
#include "Elements/Framework/TypedElementListFwd.h"
#include "Elements/Framework/TypedElementSelectionSet.h"
#include "Elements/Interfaces/TypedElementWorldInterface.h"
#include "Math/Transform.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "EditorElementSubsystem.generated.h"

class UObject;
struct FTypedElementHandle;

UCLASS(Transient)
class EDITORFRAMEWORK_API UEditorElementSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	/**
	 * Sets the world transform of the given element handle, if possible.
	 * @returns false if the world transform could not be set.
	 */
	static bool SetElementTransform(FTypedElementHandle InElementHandle, const FTransform& InWorldTransform);

	/**
	 * Return a normalized selection set like the editor would use for the gizmo manipulations
	 */
	static FTypedElementListRef GetEditorNormalizedSelectionSet(const UTypedElementSelectionSet& SelectionSet);

	/**
	 * Return only the manipulatable elements from a selection list.
	 * Require a editor normalized selection list to behave like the editor selection
	 * Note: A manipulable element is a element that would be move when manipulating the gizmo in the editor
	 */
	static FTypedElementListRef GetEditorManipulableElements(const FTypedElementListRef& NormalizedSelection);

	/**
	 * Return the most recently selected element that is manipulable.
	 * Require a editor normalized selection list to behave like the editor selection
	 * Note: A manipulable element is a element that would be move when manipulating the gizmo in the editor.
	 */
	static TTypedElement<ITypedElementWorldInterface> GetLastSelectedEditorManipulableElement(const FTypedElementListRef& NormalizedSelection);

	static bool IsElementEditorManipulable(const TTypedElement<ITypedElementWorldInterface>& WorldElement);
};