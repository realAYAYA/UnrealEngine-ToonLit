// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseGizmos/GizmoElementRenderState.h"
#include "BaseGizmos/GizmoElementShared.h"
#include "BaseGizmos/GizmoInterfaces.h"
#include "BaseGizmos/GizmoRenderingUtil.h"
#include "BaseGizmos/GizmoViewContext.h"
#include "InputState.h"
#include "SceneView.h"
#include "ToolContextInterfaces.h"
#include "UObject/GCObject.h"
#include "GizmoElementBase.generated.h"

class UMaterialInterface;

/**
 * Base class for 2d and 3d primitive objects intended to be used as part of 3D Gizmos.
 * Contains common properties and utility functions.
 * This class does nothing by itself, use subclasses like UGizmoElementCylinder
 */
UCLASS(Transient, Abstract, MinimalAPI)
class UGizmoElementBase : public UObject
{
	GENERATED_BODY()
public:

	static constexpr float DefaultViewDependentAngleTol = 0.052f;			// ~3 degrees
	static constexpr float DefaultViewDependentAxialMaxCosAngleTol = 0.998f;	// Cos(DefaultViewDependentAngleTol)
	static constexpr float DefaultViewDependentPlanarMinCosAngleTol = 0.052f;	// Cos(HALF_PI - DefaultViewDependentAngleTol)

	static constexpr float DefaultViewAlignAngleTol = 0.052f;				// ~3 degrees
	static constexpr float DefaultViewAlignMaxCosAngleTol = 0.998f;			// Cos(DefaultViewAlignAngleTol)

	static constexpr uint32 DefaultPartIdentifier = 0;						// Default part ID, used for elements that are not associated with any gizmo part

	// 
	// Render traversal state structure used to maintain the current render state while rendering.
	// As the gizmo element hierarchy is traversed, current state is maintained and updated. 
	// Element state attribute inheritance works as follows:
	//
	// - Child element state that is not set inherits from parent state.
	// - Child element state that is set replaces the parent state, except in the case of overrides.
	// - Overrides: parent element state with override set to true replaces all child state regardless of whether the child state has been set.
	//
	struct FRenderTraversalState
	{
		// LocalToWorld transform 
		// Note: non-uniform scale is not supported and the X scale element will be used for uniform scaling.
		FTransform LocalToWorldTransform;

		// Pixel to world scale
		double PixelToWorldScale = 1.0;

		// Interact state, if not equal to none, overrides the element's interact state 
		EGizmoElementInteractionState InteractionState = EGizmoElementInteractionState::None;

		// Current state used for rendering meshes
		FGizmoElementMeshRenderStateAttributes MeshRenderState;

		// Current state used for rendering lines
		FGizmoElementLineRenderStateAttributes LineRenderState;

		// Initialize state 
		void Initialize(const FSceneView* InSceneView, FTransform InTransform)
		{
			LocalToWorldTransform = InTransform;
			PixelToWorldScale = GizmoRenderingUtil::CalculateLocalPixelToWorldScale(InSceneView, InTransform.GetLocation());
		}

		// Returns the mesh material based on the current interaction state.
		const UMaterialInterface* GetCurrentMaterial()
		{
			return MeshRenderState.GetMaterial(InteractionState);
		}

		// Returns the mesh vertex color based on the current interaction state.
		FLinearColor GetCurrentVertexColor()
		{
			return MeshRenderState.GetVertexColor(InteractionState);
		}

		// Returns the line color based on the current interaction state.
		FLinearColor GetCurrentLineColor()
		{
			return LineRenderState.GetLineColor(InteractionState);
		}
	};

	struct FLineTraceTraversalState
	{
		// LocalToWorld transform 
		// Note: non-uniform scale is not supported and the X scale element will be used for uniform scaling.
		FTransform LocalToWorldTransform;

		// Pixel to world scale
		double PixelToWorldScale = 1.0;

		// View context is perspective projecion
		bool bIsPerspectiveProjection = true;

		// Initialize state 
		void Initialize(const UGizmoViewContext* InGizmoViewContext, FTransform InTransform)
		{
			check(InGizmoViewContext);
			LocalToWorldTransform = InTransform;
			PixelToWorldScale = GizmoRenderingUtil::CalculateLocalPixelToWorldScale(InGizmoViewContext, InTransform.GetLocation());
			bIsPerspectiveProjection = InGizmoViewContext->IsPerspectiveProjection();
		}
	};

public:

	// Render enabled visible element.
	INTERACTIVETOOLSFRAMEWORK_API virtual void Render(IToolsContextRenderAPI* RenderAPI, const FRenderTraversalState& RenderState) PURE_VIRTUAL(UGizmoElementBase::Render);

	// Line trace enabled hittable element.
	INTERACTIVETOOLSFRAMEWORK_API virtual FInputRayHit LineTrace(const UGizmoViewContext* ViewContext, const FLineTraceTraversalState& LineTraceState, const FVector& RayOrigin, const FVector& RayDirection) PURE_VIRTUAL(UGizmoElementBase::LineTrace, return FInputRayHit(););

	// Set/get the visible bit in element state.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetVisibleState(bool bVisible);
	INTERACTIVETOOLSFRAMEWORK_API virtual bool GetVisibleState() const;

	// Set/get the hittable bit in element state.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetHittableState(bool bHittable);
	INTERACTIVETOOLSFRAMEWORK_API virtual bool GetHittableState() const;

	// Element enabled flag. Render and LineTrace only occur when bEnabled is true.
	// This flag is useful for turning on and off an element globally while retaining its specific settings.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetEnabled(bool bInEnabled);
	INTERACTIVETOOLSFRAMEWORK_API virtual bool GetEnabled() const;

	// Whether element is enabled for perspective projections.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetEnabledForPerspectiveProjection(bool bInEnabledForPerspectiveProjection);
	INTERACTIVETOOLSFRAMEWORK_API virtual bool GetEnabledForPerspectiveProjection();

	// Whether element is enabled for orthographic projections.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetEnabledInOrthographicProjection(bool bInEnabledForOrthographicProjection);
	INTERACTIVETOOLSFRAMEWORK_API virtual bool GetEnabledInOrthographicProjection();

	// Whether element is enabled when element state is default.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetEnabledForDefaultState(bool bInEnabledForDefaultState);
	INTERACTIVETOOLSFRAMEWORK_API virtual bool GetEnabledForDefaultState();

	// Whether element is enabled when element state is hovering.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetEnabledForHoveringState(bool bInEnabledForHoveringState);
	INTERACTIVETOOLSFRAMEWORK_API virtual bool GetEnabledForHoveringState();

	// Whether element is enabled when element state is interacting.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetEnabledForInteractingState(bool bInEnabledForInteractingState);
	INTERACTIVETOOLSFRAMEWORK_API virtual bool GetEnabledForInteractingState();

	// For an element hierarchy representing multiple parts of a single gizmo, the part identifier establishes 
	// a correspondence between a gizmo part and the elements that represent that part. The recognized
	// part identifier values should be defined in the gizmo. Gizmo part identifiers must be greater than or 
	// equal to one. Identifier 0 is reserved for the default ID which should be assigned to elements
	// that do not correspond to any gizmo part, such as non-hittable decorative elements.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetPartIdentifier(uint32 InPartId);
	INTERACTIVETOOLSFRAMEWORK_API virtual uint32 GetPartIdentifier();

	// Object type bitmask indicating whether this object is visible or hittable or both
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetElementState(EGizmoElementState InElementState);
	INTERACTIVETOOLSFRAMEWORK_API virtual EGizmoElementState GetElementState() const;

	// Object interaction state - None, Hovering or Interacting
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetElementInteractionState(EGizmoElementInteractionState InInteractionState);
	INTERACTIVETOOLSFRAMEWORK_API virtual EGizmoElementInteractionState GetElementInteractionState() const;

	// Update element's visibility state if element is associated with the specified gizmo part, return true if part was found.
	INTERACTIVETOOLSFRAMEWORK_API virtual bool UpdatePartVisibleState(bool bVisible, uint32 InPartIdentifier);

	// Get element's visible state for element associated with the specified gizmo part, if part id was found.
	INTERACTIVETOOLSFRAMEWORK_API virtual TOptional<bool> GetPartVisibleState(uint32 InPartIdentifier) const;

	// Update element's hittable state if element is associated with the specified gizmo part, return true if part id was found.
	INTERACTIVETOOLSFRAMEWORK_API virtual bool UpdatePartHittableState(bool bHittable, uint32 InPartIdentifier);

	// Get element's hittable state for element associated with the specified gizmo part, if part id was found.
	INTERACTIVETOOLSFRAMEWORK_API virtual TOptional<bool> GetPartHittableState(uint32 InPartIdentifier) const;

	// Update element's interaction state if element is associated with the specified gizmo part, return true if part id was found.
	INTERACTIVETOOLSFRAMEWORK_API virtual bool UpdatePartInteractionState(EGizmoElementInteractionState InInteractionState, uint32 InPartIdentifier);

	// Get element's interaction state for element associated with the specified gizmo part, if part id was found.
	INTERACTIVETOOLSFRAMEWORK_API virtual TOptional<EGizmoElementInteractionState> GetPartInteractionState(uint32 InPartIdentifier) const;

	// View-dependent type - None, Axis or Plane. 
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetViewDependentType(EGizmoElementViewDependentType ViewDependentType);
	INTERACTIVETOOLSFRAMEWORK_API virtual EGizmoElementViewDependentType GetViewDependentType() const;

	// View-dependent angle tolerance in radians 
	//   For Axis, object is culled when angle between view dependent axis and view direction is less than tolerance angle.
	//   For Planar, cos of angle between view dependent axis (plane normal) and view direction. 
	// When the view direction is within this tolerance from the plane or axis, this object will be culled.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetViewDependentAngleTol(float InMaxAngleTol);
	INTERACTIVETOOLSFRAMEWORK_API virtual float GetViewDependentAngleTol() const;

	// View-dependent axis or plane normal, based on the view-dependent type.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetViewDependentAxis(FVector InAxis);
	INTERACTIVETOOLSFRAMEWORK_API virtual FVector GetViewDependentAxis() const;

	// View align type: None, PointEye, PointOnly, or Axial.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetViewAlignType(EGizmoElementViewAlignType InViewAlignType);
	INTERACTIVETOOLSFRAMEWORK_API virtual EGizmoElementViewAlignType GetViewAlignType() const;

	// View align axis. 
	// PointEye, PointScreen and PointOnly rotate this axis to align with view up.
	// Axial rotates about this axis.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetViewAlignAxis(FVector InAxis);
	INTERACTIVETOOLSFRAMEWORK_API virtual FVector GetViewAlignAxis() const;

	// View align normal.
	// PointEye rotates the normal to align with camera view direction.
	// PointScreen rotates the normal to align with screen forward direction.
	// Axial rotates the normal around the axis to align as closely as possible with the view direction.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetViewAlignNormal(FVector InAxis);
	INTERACTIVETOOLSFRAMEWORK_API virtual FVector GetViewAlignNormal() const;

	// View-align angle tolerance in radians.
	// Viewer alignment will not occur when the viewing angle is within this angle of view align axis.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetViewAlignAxialAngleTol(float InMaxAngleTol);
	INTERACTIVETOOLSFRAMEWORK_API virtual float GetViewAlignAxialAngleTol() const;

	// Pixel hit distance threshold, element will be scaled enough to add this threshold when line-tracing. */
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetPixelHitDistanceThreshold(float InPixelHitDistanceThreshold);
	INTERACTIVETOOLSFRAMEWORK_API virtual float GetPixelHitDistanceThreshold() const;

	//
	// Methods for managing render state attributes: Material, HoverMaterial, InteractMaterial, VertexColor 
	// 
	// State inheritance works as follows: 
	// - Gizmo element state that is not set inherits from the corresponding state in the current render traversal.
	// - Gizmo element state that is set replaces the corresponding state in the current render traversal, except in the case of overrides.
	// - Gizmo element state that is set to override, will override any corresponding state in children.
	//

	// Set mesh render state material attribute. 
	//  @param InMaterial - material to be set
	//  @param InOverridesChildState - when true, this material will override the material of all child elements.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetMaterial(TWeakObjectPtr<UMaterialInterface> InMaterial, bool InOverridesChildState = false);

	// Get mesh render state material attribute's value. 
	INTERACTIVETOOLSFRAMEWORK_API virtual const UMaterialInterface* GetMaterial() const;

	// Get mesh render state material attribute's override setting. 
	INTERACTIVETOOLSFRAMEWORK_API virtual bool DoesMaterialOverrideChildState() const;

	// Clear mesh render state material attribute. 
	INTERACTIVETOOLSFRAMEWORK_API virtual void ClearMaterial();

	// Set mesh render state hover material attribute. 
	//  @param InHoverMaterial - hover material to be set
	//  @param InOverridesChildState - when true, this hover material will override the material of all child elements.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetHoverMaterial(TWeakObjectPtr<UMaterialInterface> InHoverMaterial, bool InOverridesChildState = false);

	// Get mesh render state hover material attribute's value.
	INTERACTIVETOOLSFRAMEWORK_API virtual const UMaterialInterface* GetHoverMaterial() const;

	// Get mesh render state hover material attribute's override setting.
	INTERACTIVETOOLSFRAMEWORK_API virtual bool DoesHoverMaterialOverrideChildState() const;

	// Clear mesh render state hover material attribute. 
	INTERACTIVETOOLSFRAMEWORK_API virtual void ClearHoverMaterial();

	// Set mesh render state interact material attribute. 
	//  @param InHoverMaterial - interact material to be set
	//  @param InOverridesChildState - when true, this interact material will override the material of all child elements.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetInteractMaterial(TWeakObjectPtr<UMaterialInterface> InInteractMaterial, bool InOverridesChildState = false);

	// Get mesh render state interact material attribute's value. 
	INTERACTIVETOOLSFRAMEWORK_API virtual const UMaterialInterface* GetInteractMaterial() const;

	// Get mesh render state interact material attribute's override setting. 
	INTERACTIVETOOLSFRAMEWORK_API virtual bool DoesInteractMaterialOverrideChildState() const;

	// Clear mesh render interact state material attribute. 
	INTERACTIVETOOLSFRAMEWORK_API virtual void ClearInteractMaterial();

	// Set mesh render state vertex color attribute. 
	//  @param InVertexColor - vertex color to be set
	//  @param InOverridesChildState - when true, this vertex color will override the material of all child elements.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetVertexColor(FLinearColor InVertexColor, bool InOverridesChildState = false);

	// Get mesh render state vertex color attribute's value. 
	INTERACTIVETOOLSFRAMEWORK_API virtual FLinearColor GetVertexColor() const;

	// Returns true, if mesh render state vertex color attribute has been set. 
	INTERACTIVETOOLSFRAMEWORK_API virtual bool HasVertexColor() const;

	// Get mesh render state vertex color attribute's override setting. 
	INTERACTIVETOOLSFRAMEWORK_API virtual bool DoesVertexColorOverrideChildState() const;

	// Clear mesh render state vertex color attribute.
	INTERACTIVETOOLSFRAMEWORK_API virtual void ClearVertexColor();

	// Set mesh render state vertex color attribute. 
	//  @param InVertexColor - vertex color to be set
	//  @param InOverridesChildState - when true, this vertex color will override the material of all child elements.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetHoverVertexColor(FLinearColor InVertexColor, bool InOverridesChildState = false);

	// Get mesh render state vertex color attribute's value. 
	INTERACTIVETOOLSFRAMEWORK_API virtual FLinearColor GetHoverVertexColor() const;

	// Returns true, if mesh render state vertex color attribute has been set. 
	INTERACTIVETOOLSFRAMEWORK_API virtual bool HasHoverVertexColor() const;

	// Get mesh render state vertex color attribute's override setting. 
	INTERACTIVETOOLSFRAMEWORK_API virtual bool DoesHoverVertexColorOverrideChildState() const;

	// Clear mesh render state vertex color attribute.
	INTERACTIVETOOLSFRAMEWORK_API virtual void ClearHoverVertexColor();

	// Set mesh render state vertex color attribute. 
	//  @param InVertexColor - vertex color to be set
	//  @param InOverridesChildState - when true, this vertex color will override the material of all child elements.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetInteractVertexColor(FLinearColor InVertexColor, bool InOverridesChildState = false);

	// Get mesh render state vertex color attribute's value. 
	INTERACTIVETOOLSFRAMEWORK_API virtual FLinearColor GetInteractVertexColor() const;

	// Returns true, if mesh render state vertex color attribute has been set. 
	INTERACTIVETOOLSFRAMEWORK_API virtual bool HasInteractVertexColor() const;

	// Get mesh render state vertex color attribute's override setting. 
	INTERACTIVETOOLSFRAMEWORK_API virtual bool DoesInteractVertexColorOverrideChildState() const;

	// Clear mesh render state vertex color attribute.
	INTERACTIVETOOLSFRAMEWORK_API virtual void ClearInteractVertexColor();

protected:

	// Whether element is enabled. Render and LineTrace only occur when bEnabled is true.
	UPROPERTY()
	bool bEnabled = true;

	// Whether element is enabled for perspective projection
	UPROPERTY()
	bool bEnabledForPerspectiveProjection = true;

	// Whether element is enabled for orthographic projection
	UPROPERTY()
	bool bEnabledForOrthographicProjection = true;

	// Whether element is enabled when element state is default
	UPROPERTY()
	bool bEnabledForDefaultState = true;

	// Whether element is enabled when element state is hovering
	UPROPERTY()
	bool bEnabledForHoveringState = true;

	// Whether element is enabled when element state is interacting
	UPROPERTY()
	bool bEnabledForInteractingState = true;

	// Part identifier
	UPROPERTY()
	uint32 PartIdentifier = DefaultPartIdentifier;

	// Mesh render state attributes for this element
	UPROPERTY()
	FGizmoElementMeshRenderStateAttributes MeshRenderAttributes;

	// Element state - indicates whether object is visible or hittable
	UPROPERTY()
	EGizmoElementState ElementState = EGizmoElementState::VisibleAndHittable;

	// Current element interaction state - None, Hovering or Interacting
	UPROPERTY()
	EGizmoElementInteractionState ElementInteractionState = EGizmoElementInteractionState::None;

	// View-dependent type - None, Axis or Plane. 
	UPROPERTY()
	EGizmoElementViewDependentType ViewDependentType = EGizmoElementViewDependentType::None;

	// View-dependent axis or plane normal, based on the view-dependent type.
	UPROPERTY()
	FVector ViewDependentAxis = FVector::UpVector;

	// View-dependent angle tolerance based on :
	//   For Axis, minimum radians between view dependent axis and view direction.
	//   For Planar, minimum radians between view dependent axis and the plane where axis is its normal.
	// When the angle between the view direction and the axis/plane is less than this tolerance, this object should be culled.
	UPROPERTY()
	float ViewDependentAngleTol = DefaultViewDependentAngleTol;

	// Axial view alignment minimum cos angle tolerance, computed based on ViewDependentAngleTol. 
	// When the cos of the angle between the view direction and the axis is less than this value, this object should not be culled.
	UPROPERTY()
	float ViewDependentAxialMaxCosAngleTol = DefaultViewDependentAxialMaxCosAngleTol;

	// Planar view alignment minimum cos angle tolerance, computed based on ViewDependentAngleTol. 
	// When the cos of the angle between the view direction and the axis is greater than this value, this object should not be culled.
	UPROPERTY()
	float ViewDependentPlanarMinCosAngleTol = DefaultViewDependentPlanarMinCosAngleTol;

	// View align type: None, PointEye, or PointWorld.
	// PointEye rotates this axis to align with the view up axis.
	// PointWorld rotates this axis to align with the world up axis.
	// Axial rotates around this axis to align the normal as closely as possible to the view direction.
	UPROPERTY()
	EGizmoElementViewAlignType ViewAlignType = EGizmoElementViewAlignType::None;

	// View align axis. 
	UPROPERTY()
	FVector ViewAlignAxis = FVector::UpVector;

	// View align normal.
	// PointEye and PointWorld both rotate the normal to align with the view direction.
	// Axial rotates the normal to align as closely as possible with view direction.
	UPROPERTY()
	FVector ViewAlignNormal = -FVector::ForwardVector;

	// Axial view alignment angle tolerance in radians, based on angle between align normal and view direction. 
	// When angle between the view align normal and the view direction is greater than this angle, the align rotation will be computed.
	UPROPERTY()
	float ViewAlignAxialAngleTol = DefaultViewAlignAngleTol;

	// Axial view alignment minimum cos angle tolerance, computed based on ViewAlignAxialAngleTol. 
	// When the cos of the angle between the view direction and the align normal is less than this value, the align rotation will be computed.
	UPROPERTY()
	float ViewAlignAxialMaxCosAngleTol = DefaultViewAlignMaxCosAngleTol;

	// Pixel hit distance threshold, element will be scaled enough to add this threshold when line-tracing.
	UPROPERTY()
	float PixelHitDistanceThreshold = 7.0;

protected:

	// Return whether element is currently visible.
	INTERACTIVETOOLSFRAMEWORK_API virtual bool IsVisible(const FSceneView* View, EGizmoElementInteractionState InCurrentInteractionState, 
		const FTransform& InLocalToWorldTransform, const FVector& InLocalCenter) const;

	// Return whether element is currently hittable.
	INTERACTIVETOOLSFRAMEWORK_API virtual bool IsHittable(const UGizmoViewContext* ViewContext, const FTransform& InLocalToWorldTransform, const FVector& InLocalCenter) const;

	// Returns whether object is visible in input FSceneView based on view-dependent visibility settings.
	INTERACTIVETOOLSFRAMEWORK_API virtual bool GetViewDependentVisibility(const FSceneView* View, const FTransform& InLocalToWorldTransform, const FVector& InLocalCenter) const;

	// Returns whether object is visible in input gizmo view context based on view-dependent visibility settings
	INTERACTIVETOOLSFRAMEWORK_API virtual bool GetViewDependentVisibility(const UGizmoViewContext* View, const FTransform& InLocalToWorldTransform, const FVector& InLocalCenter) const;

	// Returns whether object is visible based on view-dependent visibility settings.
	INTERACTIVETOOLSFRAMEWORK_API virtual bool GetViewDependentVisibility(const FVector& InViewLocation, const FVector& InViewDirection, bool bInPerspectiveView, const FTransform& InLocalToWorldTransform, const FVector& InLocalCenter) const;

	// Returns whether element is enabled for given interaction state.
	INTERACTIVETOOLSFRAMEWORK_API virtual bool GetEnabledForInteractionState(EGizmoElementInteractionState InInteractionState) const;

	// Returns whether element is enabled for given view projection type.
	INTERACTIVETOOLSFRAMEWORK_API virtual bool GetEnabledForViewProjection(bool bIsPerspectiveProjection) const;

	// Return whether this element has a view alignment rotation based on input FSceneView and view-dependent alignment settings.
	// @param OutAlignRot the rotation to align this element in local space, should be prepended to the local-to-world transform.
	INTERACTIVETOOLSFRAMEWORK_API virtual bool GetViewAlignRot(const FSceneView* View, const FTransform& InLocalToWorldTransform, const FVector& InLocalCenter, FQuat& OutAlignRot) const;

	// Return whether this element has a view alignment rotation based on input gizmo view context and view-dependent alignment settings.
	// @param OutAlignRot the rotation to align this element in local space, should be prepended to the local-to-world transform.
	INTERACTIVETOOLSFRAMEWORK_API virtual bool GetViewAlignRot(const UGizmoViewContext* View, const FTransform& InLocalToWorldTransform, const FVector& InLocalCenter, FQuat& OutAlignRot) const;

	// Return whether this element has a view alignment rotation based on input view parameters and view-dependent alignment settings.
	// @param OutAlignRot the rotation to align this element in local space, should be prepended to the local-to-world transform.
	INTERACTIVETOOLSFRAMEWORK_API virtual bool GetViewAlignRot(const FVector& InViewLocation, const FVector& InViewDirection, const FVector& InViewUp, bool bInPerspectiveView, const FTransform& InLocalToWorldTransform, const FVector& InLocalCenter, FQuat& OutAlignRot) const;

	// Update render state during render traversal, determines the current render state for this element 
	// @param RenderAPI - tools render context
	// @param InLocalCenter - local element center position
	// @param InOutRenderState - render state's local to world transform will be updated with translation to center and view-dependent alignment rotation, if applicable.
	// @return view dependent visibility, true if this element is visible in the current view. 
	INTERACTIVETOOLSFRAMEWORK_API virtual bool UpdateRenderState(IToolsContextRenderAPI* RenderAPI, const FVector& InLocalCenter, FRenderTraversalState& InOutRenderState);

	// Update render state during render traversal, determines the current render state for this element
	// Same parameters as UpdateRenderState above plus two output parameters:
	// @param bOutHasAlignRot - whether alignment rotation was applied to output state
	// @param OutAlignRot - alignment rotation applied to output state, if applicable.
	INTERACTIVETOOLSFRAMEWORK_API virtual bool UpdateRenderState(IToolsContextRenderAPI* RenderAPI, const FVector& InLocalCenter, FRenderTraversalState& InOutRenderState, bool& bOutHasAlignRot, FQuat& OutAlignRot);

	// Update line trace state during line trace traversal, determines the current state for this element 
	// @param ViewContext - current gizmo view context
	// @param InLocalCenter - local element center position, this will update InOutLineTraceState's LocalToWorldTransform with a translation to LocalCenter
	// @param InOutLineTraceState - line trace state's local to world transform will be updated with translation to center and view-dependent alignment rotation, if applicable.
	// @return view dependent visibility, true if this element is visible in the current view. 
	INTERACTIVETOOLSFRAMEWORK_API virtual bool UpdateLineTraceState(const UGizmoViewContext* ViewContext, const FVector& InLocalCenter, FLineTraceTraversalState& InOutLineTraceState);

	// Update line trace state during line trace traversal, determines the current state for this element 
	// Same parameters as UpdateLineTraceState above plus two output parameters:
	// @param bOutHasAlignRot - whether alignment rotation was applied to output state
	// @param OutAlignRot - alignment rotation applied to output state, if applicable.
	INTERACTIVETOOLSFRAMEWORK_API virtual bool UpdateLineTraceState(const UGizmoViewContext* ViewContext, const FVector& InLocalCenter, FLineTraceTraversalState& InOutRenderState, bool& bOutHasAlignRot, FQuat& OutAlignRot);

protected:

	// Helper method to verify scale is uniform. If it is non-uniform, a one-time warning log is issued.
	// Returns true if scale is uniform.
	INTERACTIVETOOLSFRAMEWORK_API bool VerifyUniformScale(const FVector& Scale) const;

	// Helper method for view alignment.
	// Returns rotation between source and target input coordinate spaces.
	INTERACTIVETOOLSFRAMEWORK_API FQuat GetAlignRotBetweenCoordSpaces(FVector SourceForward, FVector SourceRight, FVector SourceUp, FVector TargetForward, FVector TargetRight, FVector TargetUp) const;
};

