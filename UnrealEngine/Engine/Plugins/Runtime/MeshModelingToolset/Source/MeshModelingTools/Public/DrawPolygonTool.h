// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "InteractiveGizmo.h"
#include "InteractiveToolBuilder.h"
#include "BaseTools/SingleClickTool.h"
#include "PreviewMesh.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Snapping/PointPlanarSnapSolver.h"
#include "ToolSceneQueriesUtil.h"
#include "Properties/MeshMaterialProperties.h"
#include "PropertySets/CreateMeshObjectTypeProperties.h"
#include "Mechanics/PlaneDistanceFromHitMechanic.h"
#include "DrawPolygonTool.generated.h"


class UCombinedTransformGizmo;
class UDragAlignmentMechanic;
class UTransformProxy;
PREDECLARE_USE_GEOMETRY_CLASS(FDynamicMesh3);

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLS_API UDrawPolygonToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};



/** Polygon tool draw type */
UENUM()
enum class EDrawPolygonDrawMode : uint8
{
	/** Draw a freehand polygon */
	Freehand,

	/** Draw a circle */
	Circle,

	/** Draw a square */
	Square,

	/** Draw a rectangle */
	Rectangle,

	/** Draw a rounded rectangle */
	RoundedRectangle,

	/** Draw a circle with a hole in the center */
	Ring
};


/** How the drawn polygon gets extruded */
UENUM()
enum class EDrawPolygonExtrudeMode : uint8
{
	/** Flat polygon without extrusion */
	Flat,

	/** Extrude drawn polygon to fixed height determined by the Extrude Height property */
	Fixed,

	/** Extrude drawn polygon to height set via additional mouse input after closing the polygon */
	Interactive,
};





UCLASS()
class MESHMODELINGTOOLS_API UDrawPolygonToolStandardProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UDrawPolygonToolStandardProperties();

	/** Type of polygon to draw in the viewport */
	UPROPERTY(EditAnywhere, NonTransactional, Category = Polygon, meta = (DisplayName = "Draw Mode"))
	EDrawPolygonDrawMode PolygonDrawMode = EDrawPolygonDrawMode::Freehand;

	/** Allow freehand drawn polygons to self-intersect */
	UPROPERTY(EditAnywhere, NonTransactional, Category = Polygon,
		meta = (DisplayName ="Self-Intersections", EditCondition = "PolygonDrawMode == EDrawPolygonDrawMode::Freehand"))
	bool bAllowSelfIntersections = false;

	/** Size of secondary features, e.g. the rounded corners of a rounded rectangle, as fraction of the overall shape size */
	UPROPERTY(EditAnywhere, NonTransactional, Category = Polygon, meta = (UIMin = "0.01", UIMax = "0.99", ClampMin = "0.01", ClampMax = "0.99",
		EditCondition = "PolygonDrawMode == EDrawPolygonDrawMode::RoundedRectangle || PolygonDrawMode == EDrawPolygonDrawMode::Ring"))
	float FeatureSizeRatio = .25;

	/** Number of radial subdivisions in round features, e.g. circles or rounded corners */
	UPROPERTY(EditAnywhere, NonTransactional, Category = Polygon, meta = (UIMin = "3", UIMax = "100", ClampMin = "3", ClampMax = "10000",
		EditCondition =	"PolygonDrawMode == EDrawPolygonDrawMode::Circle || PolygonDrawMode == EDrawPolygonDrawMode::RoundedRectangle || PolygonDrawMode == EDrawPolygonDrawMode::Ring"))
	int RadialSlices = 16;

	/** Distance between the last clicked point and the current point  */
	UPROPERTY(VisibleAnywhere, NonTransactional, Category = Polygon, meta = (TransientToolProperty))
	float Distance = 0.0f;

	/** If true, shows a gizmo to manipulate the additional grid used to draw the polygon on */
	UPROPERTY(EditAnywhere, NonTransactional, Category = Polygon)
	bool bShowGridGizmo = true;

	/** If and how the drawn polygon gets extruded */
	UPROPERTY(EditAnywhere, NonTransactional, Category = Extrude)
	EDrawPolygonExtrudeMode ExtrudeMode = EDrawPolygonExtrudeMode::Interactive;

	/** Extrude distance when using the Fixed extrude mode */
	UPROPERTY(EditAnywhere, NonTransactional, Category = Extrude, meta = (UIMin = "-1000", UIMax = "1000", ClampMin = "-10000", ClampMax = "10000",
		EditCondition = "ExtrudeMode == EDrawPolygonExtrudeMode::Fixed"))
	float ExtrudeHeight = 100.0f;
};

UCLASS()
class MESHMODELINGTOOLS_API UDrawPolygonToolSnapProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	/** Enables additional snapping controls. If false, all snapping is disabled. */
	UPROPERTY(EditAnywhere, NonTransactional, Category = Snapping)
	bool bEnableSnapping = true;

	//~ Not user visible. Mirrors the snapping settings in the viewport and is used in EditConditions
	UPROPERTY(meta = (TransientToolProperty))
	bool bSnapToWorldGrid = false;

	/** Snap to vertices in other meshes. Requires Enable Snapping to be true. */
	UPROPERTY(EditAnywhere, NonTransactional, Category = Snapping, meta = (EditCondition = "bEnableSnapping"))
	bool bSnapToVertices = true;

	/** Snap to edges in other meshes. Requires Enable Snapping to be true. */
	UPROPERTY(EditAnywhere, NonTransactional, Category = Snapping, meta = (EditCondition = "bEnableSnapping"))
	bool bSnapToEdges = false;

	/** Snap to axes of the drawing grid and axes relative to the last segment. Requires grid snapping to be disabled in viewport, and Enable Snapping to be true. */
	UPROPERTY(EditAnywhere, NonTransactional, Category = Snapping, meta = (EditCondition = "bEnableSnapping && !bSnapToWorldGrid"))
	bool bSnapToAxes = true;

	/** When snapping to axes, also try to snap to the length of an existing segment in the polygon. Requires grid snapping to be disabled in viewport, and Snap to Axes and Enable Snapping to be true. */
	UPROPERTY(EditAnywhere, NonTransactional, Category = Snapping, meta = (EditCondition = "bEnableSnapping && !bSnapToWorldGrid && bSnapToAxes"))
	bool bSnapToLengths = true;

	/** Snap to surfaces of existing objects. Requires grid snapping to be disabled in viewport, and Enable Snapping to be true.  */
	UPROPERTY(EditAnywhere, NonTransactional, Category = Snapping, meta = (EditCondition = "bEnableSnapping && !bSnapToWorldGrid"))
	bool bSnapToSurfaces = false;

	/** Offset for snap point on the surface of an existing object in the direction of the surface normal. Requires grid snapping to be disabled in viewport, and Snap to Surfaces and Enable Snapping to be true. */
	UPROPERTY(EditAnywhere, NonTransactional, Category = Snapping, meta = (DisplayName = "Surface Offset", 
		EditCondition = "bEnableSnapping && !bSnapToWorldGrid && bSnapToSurfaces"))
	float SnapToSurfacesOffset = 0.0f;
};





/**
 * This tool allows the user to draw and extrude 2D polygons
 */
UCLASS()
class MESHMODELINGTOOLS_API UDrawPolygonTool : public UInteractiveTool, public IClickSequenceBehaviorTarget
{
	GENERATED_BODY()
public:
	UDrawPolygonTool();

	virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;

	virtual void SetWorld(UWorld* World);

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual bool HasCancel() const override { return false; }
	virtual bool HasAccept() const override { return false; }
	virtual bool CanAccept() const override { return false; }

	// IClickSequenceBehaviorTarget implementation

	virtual void OnBeginSequencePreview(const FInputDeviceRay& ClickPos) override;
	virtual bool CanBeginClickSequence(const FInputDeviceRay& ClickPos) override;
	virtual void OnBeginClickSequence(const FInputDeviceRay& ClickPos) override;
	virtual void OnNextSequencePreview(const FInputDeviceRay& ClickPos) override;
	virtual bool OnNextSequenceClick(const FInputDeviceRay& ClickPos) override;
	virtual void OnTerminateClickSequence() override;
	virtual bool RequestAbortClickSequence() override;
	virtual void OnUpdateModifierState(int ModifierID, bool bIsOn) override;

	// polygon drawing functions
	virtual void ResetPolygon();
	virtual void UpdatePreviewVertex(const FVector3d& PreviewVertex);
	virtual void AppendVertex(const FVector3d& Vertex);
	virtual bool FindDrawPlaneHitPoint(const FInputDeviceRay& ClickPos, FVector3d& HitPosOut);
	virtual void EmitCurrentPolygon();

	virtual void BeginInteractiveExtrude();
	virtual void EndInteractiveExtrude();

	virtual void ApplyUndoPoints(const TArray<FVector3d>& ClickPointsIn, const TArray<FVector3d>& PolygonVerticesIn);


protected:
	// flags used to identify modifier keys/buttons
	static constexpr int IgnoreSnappingModifier = 1;
	static constexpr int AngleSnapModifier = 2;

	/** Property set for type of output object (StaticMesh, Volume, etc) */
	UPROPERTY()
	TObjectPtr<UCreateMeshObjectTypeProperties> OutputTypeProperties;

	/** Properties that control polygon generation exposed to user via details view */
	UPROPERTY()
	TObjectPtr<UDrawPolygonToolStandardProperties> PolygonProperties;

	UPROPERTY()
	TObjectPtr<UDrawPolygonToolSnapProperties> SnapProperties;

	UPROPERTY()
	TObjectPtr<UNewMeshMaterialProperties> MaterialProperties;
	

	/** Origin of plane we will draw polygon on */
	FVector3d DrawPlaneOrigin;

	/** Orientation of plane we will draw polygon on */
	UE::Geometry::FQuaterniond DrawPlaneOrientation;
	
	/** Vertices of current preview polygon */
	TArray<FVector3d> PolygonVertices;

	/** Vertices of holes in current preview polygon */
	TArray<TArray<FVector3d>> PolygonHolesVertices;

	/** last vertex of polygon that is actively being updated as input device is moved */
	FVector3d PreviewVertex;

	UWorld* TargetWorld;

	FViewCameraState CameraState;

	UPROPERTY()
	TObjectPtr<UPreviewMesh> PreviewMesh;

	
	// drawing plane gizmo

	UPROPERTY()
	TObjectPtr<UCombinedTransformGizmo> PlaneTransformGizmo;

	UPROPERTY()
	TObjectPtr<UTransformProxy> PlaneTransformProxy;

	// called on PlaneTransformProxy.OnTransformChanged
	void PlaneTransformChanged(UTransformProxy* Proxy, FTransform Transform);

	// calls SetDrawPlaneFromWorldPos when user ctrl+clicks on scene
	IClickBehaviorTarget* SetPointInWorldConnector = nullptr;

	// updates plane and gizmo position
	virtual void SetDrawPlaneFromWorldPos(const FVector3d& Position, const FVector3d& Normal);

	void UpdateShowGizmoState(bool bNewVisibility);

	// whether to allow the draw plane to be updated in the UI -- returns false if there is an in-progress shape relying on the current draw plane
	bool AllowDrawPlaneUpdates();

	// polygon drawing

	bool bAbortActivePolygonDraw;

	bool bInFixedPolygonMode = false;
	TArray<FVector3d> FixedPolygonClickPoints;

	// can close poly if current segment intersects existing segment
	bool UpdateSelfIntersection();
	bool bHaveSelfIntersection;
	int SelfIntersectSegmentIdx;
	FVector3d SelfIntersectionPoint;

	// only used when SnapSettings.bSnapToSurfaces = true
	bool bHaveSurfaceHit;
	FVector3d SurfaceHitPoint;
	FVector3d SurfaceOffsetPoint;

	bool bIgnoreSnappingToggle = false;		// toggled by hotkey (shift)
	UE::Geometry::FPointPlanarSnapSolver SnapEngine;
	ToolSceneQueriesUtil::FSnapGeometry LastSnapGeometry;
	FVector3d LastGridSnapPoint;

	void GetPolygonParametersFromFixedPoints(const TArray<FVector3d>& FixedPoints, FVector2d& FirstReferencePt, FVector2d& BoxSize, double& YSign, double& AngleRad);
	void GenerateFixedPolygon(const TArray<FVector3d>& FixedPoints, TArray<FVector3d>& VerticesOut, TArray<TArray<FVector3d>>& HolesVerticesOut);


	// extrusion control

	bool bInInteractiveExtrude = false;
	bool bHasSavedExtrudeHeight = false;
	float SavedExtrudeHeight;

	void UpdateLivePreview();
	bool bPreviewUpdatePending;

	UPROPERTY()
	TObjectPtr<UPlaneDistanceFromHitMechanic> HeightMechanic;

	UPROPERTY()
	TObjectPtr<UDragAlignmentMechanic> DragAlignmentMechanic = nullptr;

	/** Generate extruded meshes.  Returns true on success. */
	bool GeneratePolygonMesh(const TArray<FVector3d>& Polygon, const TArray<TArray<FVector3d>>& PolygonHoles, FDynamicMesh3* ResultMeshOut, UE::Geometry::FFrame3d& WorldFrameOut, bool bIncludePreviewVtx, double ExtrudeDistance, bool bExtrudeSymmetric);


	// user feedback messages
	void ShowStartupMessage();
	void ShowExtrudeMessage();


	friend class FDrawPolygonStateChange;
	int32 CurrentCurveTimestamp = 1;
	void UndoCurrentOperation(const TArray<FVector3d>& ClickPointsIn, const TArray<FVector3d>& PolygonVerticesIn);
	bool CheckInCurve(int32 Timestamp) const { return CurrentCurveTimestamp == Timestamp; }
};



// Change event used by DrawPolygonTool to undo draw state.
// Currently does not redo.
class MESHMODELINGTOOLS_API FDrawPolygonStateChange : public FToolCommandChange
{
public:
	bool bHaveDoneUndo = false;
	int32 CurveTimestamp = 0;
	const TArray<FVector3d> FixedVertexPoints;
	const TArray<FVector3d> PolyPoints;

	FDrawPolygonStateChange(int32 CurveTimestampIn,
							const TArray<FVector3d>& FixedVertexPointsIn,
							const TArray<FVector3d>& PolyPointsIn)
		: CurveTimestamp(CurveTimestampIn),
		FixedVertexPoints(FixedVertexPointsIn),
		PolyPoints(PolyPointsIn)
	{
	}
	virtual void Apply(UObject* Object) override {}
	virtual void Revert(UObject* Object) override;
	virtual bool HasExpired(UObject* Object) const override;
	virtual FString ToString() const override;
};
