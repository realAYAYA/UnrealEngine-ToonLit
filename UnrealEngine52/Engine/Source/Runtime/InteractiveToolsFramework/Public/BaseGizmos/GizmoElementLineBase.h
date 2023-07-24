// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseGizmos/GizmoElementBase.h"
#include "BaseGizmos/GizmoElementRenderState.h"
#include "BaseGizmos/GizmoElementShared.h"
#include "ToolContextInterfaces.h"
#include "GizmoElementLineBase.generated.h"

/**
 * Base class for 2d and 3d primitive objects which support line drawing,
 * intended to be used as part of 3D Gizmos.
 */
UCLASS(Transient, Abstract)
class INTERACTIVETOOLSFRAMEWORK_API UGizmoElementLineBase : public UGizmoElementBase
{
	GENERATED_BODY()
public:

	// Get line thickness for based on current element interaction state and view
	virtual float GetCurrentLineThickness(bool bPerspectiveView, float InViewFOV) const;

	// Line thickness when rendering lines, 0.0 is valid and will render thinnest line 
	virtual void SetLineThickness(float InLineThickness);
	virtual float GetLineThickness() const;

	// Multiplier applied to line thickness when hovering
	virtual void SetHoverLineThicknessMultiplier(float InHoverLineThicknessMultiplier);
	virtual float GetHoverLineThicknessMultiplier() const;

	// Multiplier applied to line thickness when interacting
	virtual void SetInteractLineThicknessMultiplier(float InInteractLineThicknessMultiplier);
	virtual float GetInteractLineThicknessMultiplier() const;

	// Whether line thickness is in screen space 
	virtual void SetScreenSpaceLine(bool bInScreenSpaceLine);
	virtual bool GetScreenSpaceLine() const;

	//
	// Methods for managing line state attributes: LineColor, HoverLineColor, InteractLineColor
	//
	// State inheritance works as follows:
	// - Gizmo element state that is not set inherits from the corresponding state in the current render traversal.
	// - Gizmo element state that is set replaces the corresponding state in the current render traversal, except in the case of overrides.
	// - Gizmo element state that is set to override, will override any corresponding state in children.
	//

	// Set line render state line color attribute. 
	//  @param InMaterial - line color to be set
	//  @param InOverridesChildState - when true, this line color will override the line color of all child elements.
	virtual void SetLineColor(FLinearColor InColor, bool InOverridesChildState = false);

	// Get line render state line color attribute's value.
	virtual FLinearColor GetLineColor() const;

	// Returns true if line render state line color attribute has been set.
	virtual bool HasLineColor() const;

	// Get line render state line color attribute's override setting. 
	virtual bool DoesLineColorOverrideChildState() const;

	// Clear line render state line color attribute.
	virtual void ClearLineColor();

	// Set line render state hover line color attribute. 
	//  @param InMaterial - hover line color to be set
	//  @param InOverridesChildState - when true, this hover line color will override the line color of all child elements.
	virtual void SetHoverLineColor(FLinearColor InColor, bool InOverridesChildState = false);

	// Get line render state hover line color attribute's value.
	virtual FLinearColor GetHoverLineColor() const;

	// Returns true if line render state hover line color attribute has been set. 
	virtual bool HasHoverLineColor() const;

	// Get line render state hover line color attribute's override setting.
	virtual bool DoesHoverLineColorOverrideChildState() const;

	// Clear line render state hover line color attribute. 
	virtual void ClearHoverLineColor();

	// Set line render state interact line color attribute. 
	//  @param InMaterial - interact line color to be set
	//  @param InOverridesChildState - when true, this interact line color will override the line color of all child elements.
	virtual void SetInteractLineColor(FLinearColor InColor, bool InOverridesChildState = false);

	// Get line render state interact line color attribute's value. 
	virtual FLinearColor GetInteractLineColor() const;

	// Returns true if line render state interact line color attribute has been set. 
	virtual bool HasInteractLineColor() const;

	// Get line render state interact line color attribute's override setting. 
	virtual bool DoesInteractLineColorOverrideChildState() const;

	// Clear line render state interact line color attribute.
	virtual void ClearInteractLineColor();

protected:

	// Line render state attributes for this element
	UPROPERTY()
	FGizmoElementLineRenderStateAttributes LineRenderAttributes;

	// Line thickness when rendering lines, must be >= 0.0, value of 0.0 will render thinnest line 
	UPROPERTY()
	float LineThickness = 0.0;

	// Whether line thickness is in screen space
	UPROPERTY()
	bool bScreenSpaceLine = true;

	// Multiplier applied to line thickness when hovering
	UPROPERTY(EditAnywhere, Category = Options)
	float HoverLineThicknessMultiplier = 2.0f;

	// Multiplier applied to line thickness when interacting
	UPROPERTY(EditAnywhere, Category = Options)
	float InteractLineThicknessMultiplier = 2.0f;

	// Update render state during render traversal, determines the current render state for this element
	// @return view dependent visibility, true if this element is visible in the current view.
	virtual bool UpdateRenderState(IToolsContextRenderAPI* RenderAPI, const FVector& InLocalOrigin, FRenderTraversalState& InOutRenderState) override;
};