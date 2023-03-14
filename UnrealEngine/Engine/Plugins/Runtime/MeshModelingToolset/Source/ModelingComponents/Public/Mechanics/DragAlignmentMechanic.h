// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BaseBehaviors/BehaviorTargetInterfaces.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "ToolContextInterfaces.h" // FViewCameraState
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_1
#include "Engine/EngineTypes.h" //FHitResult
#endif
#include "Engine/HitResult.h"
#include "InteractionMechanic.h"
#include "TransformTypes.h"
#include "VectorTypes.h"

#include "DragAlignmentMechanic.generated.h"

PREDECLARE_USE_GEOMETRY_CLASS(FGroupTopologyDeformer);
class UPrimitiveComponent;
class UIntervalGizmo;
class UCombinedTransformGizmo;

/**
 * Mechanic that can be added to (potentially multiple) UCombinedTransformGizmo object to allow them to snap to
 * scene geometry in rotation and translation when the Ctrl key is pressed.
 */
UCLASS()
class MODELINGCOMPONENTS_API UDragAlignmentMechanic : public UInteractionMechanic, public IModifierToggleBehaviorTarget
{
	GENERATED_BODY()

public:

	/**
	 * Adds this mechanic to the given gizmo. Can be called on multiple gizmos, and works both for UCombinedTransformGizmo
	 * and URepositionableTransformGizmo.
	 * For repositioning, the gizmo will not ignore anything in its alignment (so that the gizmo can be aligned to the
	 * target object(s)). For movement, the gizmo will ignore ComponentsToIgnore, as well as modified triangles if used
	 * for deformation.
	 */
	void AddToGizmo(UCombinedTransformGizmo* TransformGizmo, const TArray<const UPrimitiveComponent*>* ComponentsToIgnore = nullptr,
		const TArray<const UPrimitiveComponent*>* InvisibleComponentsToInclude = nullptr);

	/**
	 * Like the other AddToGizmo, but acts on interval gizmos.
	 */
	void AddToGizmo(UIntervalGizmo* IntervalGizmo, const TArray<const UPrimitiveComponent*>* ComponentsToIgnoreInAlignment = nullptr,
		const TArray<const UPrimitiveComponent*>* InvisibleComponentsToInclude = nullptr);

	/**
	 * Optional- allows the use of the mechanic in mesh deformation. Specifically, alignment will attempt to align
	 * to the dynamic mesh underlying the given FDynamicMeshAABBTree3, and ignore deformed triangles when the gizmo
	 * is moved.
	 * This works with only one deformed mesh, so it probably wouldn't be used with multiple gizmos.
	 */
	void InitializeDeformedMeshRayCast(
		TFunction<UE::Geometry::FDynamicMeshAABBTree3* ()> GetSpatialIn,
		const FTransform3d& TargetTransform, const FGroupTopologyDeformer* LinearDeformer);

	virtual void Setup(UInteractiveTool* ParentToolIn) override;
	virtual void Shutdown() override;

	/**
	 * This will draw a line from the last input position to the last output position. This
	 * gets cleared when the transform is ended. 
	 */
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	// IModifierToggleBehaviorTarget implementation
	virtual void OnUpdateModifierState(int ModifierID, bool bIsOn) override;

protected:
	// These are modifiable in case we someday want to make it easier to customize them further.
	TUniqueFunction<bool(const FRay& InputRay, FHitResult& OutputHit, 
		const TArray<const UPrimitiveComponent*>* ComponentsToIgnore,
		const TArray<const UPrimitiveComponent*>* InvisibleComponentsToInclude)> WorldRayCast = nullptr;
	TUniqueFunction<bool(const FRay&, FHitResult&, bool bUseFilter)> MeshRayCast = nullptr;
	TUniqueFunction<bool(int32 Tid)> MeshRayCastTriangleFilter = [](int32 Tid) { return true; };

	// Used to identify the type of object we hit, so we can choose different renderings.
	static const int32 TRIANGLE_HIT_TYPE_ID = 1;
	static const int32 VERT_HIT_TYPE_ID = 2;
	static const int32 EDGE_HIT_TYPE_ID = 3;

	// Used for snapping to verts
	FViewCameraState CachedCameraState;
	float VisualAngleSnapThreshold = 1.0;

	// We keep track of the last position we gave the user and the last position that
	// they moved their gizmo. This lets us render lines connecting the two.
	bool bPreviewEndpointsValid = false;
	bool bWaitingOnProjectedResult = false;
	FHitResult LastHitResult;
	FVector3d LastProjectedResult;

	bool bAlignmentToggle = false;
	int32 AlignmentModifierID = 1;

	virtual bool CastRay(const FRay& WorldRay, FVector& OutputPoint,
		const TArray<const UPrimitiveComponent*>* ComponentsToIgnore, 
		const TArray<const UPrimitiveComponent*>* InvisibleComponentsToInclude,
		bool bUseFilter);
	void OnGizmoTransformChanged(FTransform NewTransform);
};