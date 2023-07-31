// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ArrayView.h"
#include "Elements/Framework/TypedElementAssetEditorToolkitHostMixin.h"
#include "Elements/Framework/TypedElementInterfaceCustomization.h"
#include "Elements/Framework/TypedElementListFwd.h"
#include "Elements/Interfaces/TypedElementWorldInterface.h"
#include "InputState.h"
#include "Math/Axis.h"
#include "Math/Transform.h"
#include "Math/UnrealMathSSE.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UnrealWidgetFwd.h"

#include "TypedElementViewportInteraction.generated.h"

class UTypedElementSelectionSet;
struct FInputDeviceState;
struct FTypedElementHandle;

enum class ETypedElementViewportInteractionGizmoManipulationType : uint8
{
	/** The gizmo manipulation resulted in a drag operation */
	Drag,
	/** The gizmo manipulation resulted in a click operation */
	Click,
};

/**
 * Customization used to allow asset editors (such as the level editor) to override the base behavior of viewport interaction.
 */
class EDITORFRAMEWORK_API FTypedElementViewportInteractionCustomization : public FTypedElementAssetEditorToolkitHostMixin
{
public:
	virtual ~FTypedElementViewportInteractionCustomization() = default;

	//~ See UTypedElementViewportInteraction for API docs
	virtual bool GetGizmoPivotLocation(const TTypedElement<ITypedElementWorldInterface>& InElementWorldHandle, const UE::Widget::EWidgetMode InWidgetMode, FVector& OutPivotLocation);
	virtual void PreGizmoManipulationStarted(TArrayView<const FTypedElementHandle> InElementHandles, const UE::Widget::EWidgetMode InWidgetMode);
	virtual void GizmoManipulationStarted(const TTypedElement<ITypedElementWorldInterface>& InElementWorldHandle, const UE::Widget::EWidgetMode InWidgetMode);
	virtual void GizmoManipulationDeltaUpdate(const TTypedElement<ITypedElementWorldInterface>& InElementWorldHandle, const UE::Widget::EWidgetMode InWidgetMode, const EAxisList::Type InDragAxis, const FInputDeviceState& InInputState, const FTransform& InDeltaTransform, const FVector& InPivotLocation);
	virtual void GizmoManipulationStopped(const TTypedElement<ITypedElementWorldInterface>& InElementWorldHandle, const UE::Widget::EWidgetMode InWidgetMode, const ETypedElementViewportInteractionGizmoManipulationType InManipulationType);
	virtual void PostGizmoManipulationStopped(TArrayView<const FTypedElementHandle> InElementHandles, const UE::Widget::EWidgetMode InWidgetMode);
	virtual void MirrorElement(const TTypedElement<ITypedElementWorldInterface>& InElementWorldHandle, const FVector& InMirrorScale, const FVector& InPivotLocation);
	virtual bool GetFocusBounds(const TTypedElement<ITypedElementWorldInterface>& InElementWorldHandle, FBoxSphereBounds& OutBounds);
};

/**
 * Utility to hold a typed element handle and its associated world interface and  viewport interaction customization.
 */
struct EDITORFRAMEWORK_API FTypedElementViewportInteractionElement
{
public:
	FTypedElementViewportInteractionElement() = default;

	FTypedElementViewportInteractionElement(TTypedElement<ITypedElementWorldInterface> InElementWorldHandle, FTypedElementViewportInteractionCustomization* InViewportInteractionCustomization)
		: ElementWorldHandle(MoveTemp(InElementWorldHandle))
		, ViewportInteractionCustomization(InViewportInteractionCustomization)
	{
	}

	FTypedElementViewportInteractionElement(const FTypedElementViewportInteractionElement&) = default;
	FTypedElementViewportInteractionElement& operator=(const FTypedElementViewportInteractionElement&) = default;

	FTypedElementViewportInteractionElement(FTypedElementViewportInteractionElement&&) = default;
	FTypedElementViewportInteractionElement& operator=(FTypedElementViewportInteractionElement&&) = default;

	FORCEINLINE explicit operator bool() const
	{
		return IsSet();
	}

	FORCEINLINE bool IsSet() const
	{
		return ElementWorldHandle.IsSet()
			&& ViewportInteractionCustomization;
	}

	//~ See UTypedElementViewportInteraction for API docs
	bool GetGizmoPivotLocation(const UE::Widget::EWidgetMode InWidgetMode, FVector& OutPivotLocation) const { return ViewportInteractionCustomization->GetGizmoPivotLocation(ElementWorldHandle, InWidgetMode, OutPivotLocation); }
	void GizmoManipulationStarted(const UE::Widget::EWidgetMode InWidgetMode) const { ViewportInteractionCustomization->GizmoManipulationStarted(ElementWorldHandle, InWidgetMode); }
	void GizmoManipulationDeltaUpdate(const UE::Widget::EWidgetMode InWidgetMode, const EAxisList::Type InDragAxis, const FInputDeviceState& InInputState, const FTransform& InDeltaTransform, const FVector& InPivotLocation) const { ViewportInteractionCustomization->GizmoManipulationDeltaUpdate(ElementWorldHandle, InWidgetMode, InDragAxis, InInputState, InDeltaTransform, InPivotLocation); }
	void GizmoManipulationStopped(const UE::Widget::EWidgetMode InWidgetMode, const ETypedElementViewportInteractionGizmoManipulationType InManipulationType) const { ViewportInteractionCustomization->GizmoManipulationStopped(ElementWorldHandle, InWidgetMode, InManipulationType); }
	void MirrorElement(const FVector& InMirrorScale, const FVector& InPivotLocation) const { ViewportInteractionCustomization->MirrorElement(ElementWorldHandle, InMirrorScale, InPivotLocation); }
	bool GetFocusBounds(FBoxSphereBounds& OutBounds) const { return ViewportInteractionCustomization->GetFocusBounds(ElementWorldHandle, OutBounds); }
	
private:
	TTypedElement<ITypedElementWorldInterface> ElementWorldHandle;
	FTypedElementViewportInteractionCustomization* ViewportInteractionCustomization = nullptr;
};

/**
 * A utility to handle higher-level viewport interactions, but default via UTypedElementWorldInterface,
 * but asset editors can customize this behavior via FTypedElementViewportInteractionCustomization.
 */
UCLASS(Transient)
class EDITORFRAMEWORK_API UTypedElementViewportInteraction : public UObject, public TTypedElementInterfaceCustomizationRegistry<FTypedElementViewportInteractionCustomization>
{
	GENERATED_BODY()

public:
	/**
	 * Notify that the gizmo is potentially about to start manipulating the transform of the given set of elements.
	 * @note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList, or FLevelEditorViewportClient::GetElementsToManipulate.
	 */
	void BeginGizmoManipulation(FTypedElementListConstRef InElementsToMove, const UE::Widget::EWidgetMode InWidgetMode);

	/**
	 * Notify that the gizmo has manipulated the transform of the given set of elements.
	 * @note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList, or FLevelEditorViewportClient::GetElementsToManipulate.
	 */
	void UpdateGizmoManipulation(FTypedElementListConstRef InElementsToMove, const UE::Widget::EWidgetMode InWidgetMode, const EAxisList::Type InDragAxis, const FInputDeviceState& InInputState, const FTransform& InDeltaTransform);
	
	/**
	 * Notify that the gizmo has finished manipulating the transform of the given set of elements.
	 * @note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList, or FLevelEditorViewportClient::GetElementsToManipulate.
	 */
	void EndGizmoManipulation(FTypedElementListConstRef InElementsToMove, const UE::Widget::EWidgetMode InWidgetMode, const ETypedElementViewportInteractionGizmoManipulationType InManipulationType);

	/**
	 * Apply the given delta to the specified element.
	 * This performs a similar operation to an in-flight gizmo manipulation, but without any pre/post-change notification.
	 */
	void ApplyDeltaToElement(const FTypedElementHandle& InElementHandle, const UE::Widget::EWidgetMode InWidgetMode, const EAxisList::Type InDragAxis, const FInputDeviceState& InInputState, const FTransform& InDeltaTransform);

	/**
	 * Apply the given mirror scale to the specified element.
	 */
	void MirrorElement(const FTypedElementHandle& InElementHandle, const FVector& InMirrorScale);

	/**
	 *	Calculate the bounds of all elements in the list, to be focused on in the viewport
	 *	@note This list should have been pre-normalized via UTypedElementSelectionSet::GetNormalizedSelection or UTypedElementSelectionSet::GetNormalizedElementList, or FLevelEditorViewportClient::GetElementsToManipulate.
	 */
	bool GetFocusBounds(FTypedElementListConstRef InElements, FBoxSphereBounds& OutBox);
private:
	/**
	 * Attempt to resolve the selection interface and viewport interaction customization for the given element, if any.
	 */
	FTypedElementViewportInteractionElement ResolveViewportInteractionElement(const FTypedElementHandle& InElementHandle) const;
};
