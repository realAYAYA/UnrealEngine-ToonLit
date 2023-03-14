// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZoneShapeComponentVisualizer.h"
#include "CoreMinimal.h"
#include "Algo/AnyOf.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "EditorViewportCommands.h"
#include "LevelEditorActions.h"
#include "ScopedTransaction.h"
#include "ActorEditorUtils.h"
#include "ZoneGraphSubsystem.h"
#include "ZoneGraphSettings.h"
#include "ZoneShapeComponent.h"
#include "ZoneShapeUtilities.h"
#include "ZoneGraphRenderingUtilities.h"
#include "BezierUtilities.h"

// Uncomment to draw additional rotation debug visualizations.
// #define ZONEGRAPH_DEBUG_ROTATIONS

IMPLEMENT_HIT_PROXY(HZoneShapeVisProxy, HComponentVisProxy);
IMPLEMENT_HIT_PROXY(HZoneShapePointProxy, HZoneShapeVisProxy);
IMPLEMENT_HIT_PROXY(HZoneShapeSegmentProxy, HZoneShapeVisProxy);
IMPLEMENT_HIT_PROXY(HZoneShapeControlPointProxy, HZoneShapeVisProxy);

#define LOCTEXT_NAMESPACE "ZoneShapeComponentVisualizer"

/** Define commands for the shape component visualizer */
class FZoneShapeComponentVisualizerCommands : public TCommands<FZoneShapeComponentVisualizerCommands>
{
public:
	FZoneShapeComponentVisualizerCommands() : TCommands <FZoneShapeComponentVisualizerCommands>
		(
			"ZoneShapeComponentVisualizer",	// Context name for fast lookup
			LOCTEXT("ZoneShapeComponentVisualizer", "Zone Shape Component Visualizer"),	// Localized context name for displaying
			FName(),	// Parent
			FAppStyle::GetAppStyleSetName()
			)
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(DeletePoint, "Delete Point(s)", "Delete the currently selected shape points.", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete));
		UI_COMMAND(DuplicatePoint, "Duplicate Point(s)", "Duplicate the currently selected shape points.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(AddPoint, "Add Point Here", "Add a new shape point at the cursor location.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(SelectAll, "Select All Points", "Select all shape points.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(SetPointToSharp, "Sharp", "Set point to Sharp type", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(SetPointToBezier, "Bezier", "Set point to Bezier type", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(SetPointToAutoBezier, "Auto Bezier", "Set point to Auto Bezier type", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(SetPointToLaneSegment, "Lane Segment", "Set point to Lane Segment type", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(FocusViewportToSelection, "Focus Selected", "Moves the camera in front of the selection", EUserInterfaceActionType::Button, FInputChord(EKeys::F));
	}

public:
	TSharedPtr<FUICommandInfo> DeletePoint;
	TSharedPtr<FUICommandInfo> DuplicatePoint;
	TSharedPtr<FUICommandInfo> AddPoint;
	TSharedPtr<FUICommandInfo> SelectAll;
	TSharedPtr<FUICommandInfo> SetPointToSharp;
	TSharedPtr<FUICommandInfo> SetPointToBezier;
	TSharedPtr<FUICommandInfo> SetPointToAutoBezier;
	TSharedPtr<FUICommandInfo> SetPointToLaneSegment;
	TSharedPtr<FUICommandInfo> FocusViewportToSelection;
};

FZoneShapeComponentVisualizer::FZoneShapeComponentVisualizer()
	: FComponentVisualizer()
	, bAllowDuplication(true)
	, DuplicateAccumulatedDrag(FVector::ZeroVector)
	, bControlPointPositionCaptured(false)
	, ControlPointPosition(FVector::ZeroVector)
{
	FZoneShapeComponentVisualizerCommands::Register();

	ShapeComponentVisualizerActions = MakeShareable(new FUICommandList);

	ShapePointsProperty = FindFProperty<FProperty>(UZoneShapeComponent::StaticClass(), TEXT("Points")); //Can't use GET_MEMBER_NAME_CHECKED(UZoneShapeComponent, Points)) on private members :(

	SelectionState = NewObject<UZoneShapeComponentVisualizerSelectionState>(GetTransientPackage(), TEXT("ZoneShapeSelectionState"), RF_Transactional);
}

void FZoneShapeComponentVisualizer::OnRegister()
{
	const auto& Commands = FZoneShapeComponentVisualizerCommands::Get();

	ShapeComponentVisualizerActions->MapAction(
		Commands.DeletePoint,
		FExecuteAction::CreateSP(this, &FZoneShapeComponentVisualizer::OnDeletePoint),
		FCanExecuteAction::CreateSP(this, &FZoneShapeComponentVisualizer::CanDeletePoint));

	ShapeComponentVisualizerActions->MapAction(
		Commands.DuplicatePoint,
		FExecuteAction::CreateSP(this, &FZoneShapeComponentVisualizer::OnDuplicatePoint),
		FCanExecuteAction::CreateSP(this, &FZoneShapeComponentVisualizer::IsPointSelectionValid));

	ShapeComponentVisualizerActions->MapAction(
		Commands.AddPoint,
		FExecuteAction::CreateSP(this, &FZoneShapeComponentVisualizer::OnAddPointToSegment),
		FCanExecuteAction::CreateSP(this, &FZoneShapeComponentVisualizer::CanAddPointToSegment));

	ShapeComponentVisualizerActions->MapAction(
		Commands.SelectAll,
		FExecuteAction::CreateSP(this, &FZoneShapeComponentVisualizer::OnSelectAllPoints),
		FCanExecuteAction::CreateSP(this, &FZoneShapeComponentVisualizer::CanSelectAllPoints));

	ShapeComponentVisualizerActions->MapAction(
		Commands.SetPointToSharp,
		FExecuteAction::CreateSP(this, &FZoneShapeComponentVisualizer::OnSetPointType, FZoneShapePointType::Sharp),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FZoneShapeComponentVisualizer::IsPointTypeSet, FZoneShapePointType::Sharp));

	ShapeComponentVisualizerActions->MapAction(
		Commands.SetPointToBezier,
		FExecuteAction::CreateSP(this, &FZoneShapeComponentVisualizer::OnSetPointType, FZoneShapePointType::Bezier),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FZoneShapeComponentVisualizer::IsPointTypeSet, FZoneShapePointType::Bezier));

	ShapeComponentVisualizerActions->MapAction(
		Commands.SetPointToAutoBezier,
		FExecuteAction::CreateSP(this, &FZoneShapeComponentVisualizer::OnSetPointType, FZoneShapePointType::AutoBezier),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FZoneShapeComponentVisualizer::IsPointTypeSet, FZoneShapePointType::AutoBezier));

	ShapeComponentVisualizerActions->MapAction(
		Commands.SetPointToLaneSegment,
		FExecuteAction::CreateSP(this, &FZoneShapeComponentVisualizer::OnSetPointType, FZoneShapePointType::LaneProfile),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FZoneShapeComponentVisualizer::IsPointTypeSet, FZoneShapePointType::LaneProfile));

	ShapeComponentVisualizerActions->MapAction(
		Commands.FocusViewportToSelection,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ExecuteExecCommand, FString(TEXT("CAMERA ALIGN ACTIVEVIEWPORTONLY")))
	);

	bool bAlign = false;
	bool bUseLineTrace = false;
	bool bUseBounds = false;
	bool bUsePivot = false;
	ShapeComponentVisualizerActions->MapAction(
		FLevelEditorCommands::Get().SnapToFloor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapToFloor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
	);

	bAlign = true;
	bUseLineTrace = false;
	bUseBounds = false;
	bUsePivot = false;
	ShapeComponentVisualizerActions->MapAction(
		FLevelEditorCommands::Get().AlignToFloor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapToFloor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
	);
}

FZoneShapeComponentVisualizer::~FZoneShapeComponentVisualizer()
{
	FZoneShapeComponentVisualizerCommands::Unregister();
}

void FZoneShapeComponentVisualizer::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (SelectionState)
	{
		Collector.AddReferencedObject(SelectionState);
	}
}
void FZoneShapeComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	const UZoneShapeComponent* ShapeComp = Cast<const UZoneShapeComponent>(Component);
	if (!ShapeComp)
	{
		return;
	}
	const FMatrix LocalToWorld = ShapeComp->GetComponentTransform().ToMatrixWithScale();

	// Distance culling.
	float ShapeMaxDrawDistance = MAX_flt;
	if (const UZoneGraphSettings* ZoneGraphSettings = GetDefault<UZoneGraphSettings>())
	{
		ShapeMaxDrawDistance = ZoneGraphSettings->GetShapeMaxDrawDistance();
	}
	const float MaxDrawDistanceSqr = FMath::Square(ShapeMaxDrawDistance);

	// Taking into account the min and maximum drawing distance
	const FBoxSphereBounds ShapeBounds = ShapeComp->CalcBounds(ShapeComp->GetComponentTransform());
	const float DistanceSqr = FVector::DistSquared(ShapeBounds.Origin, View->ViewMatrices.GetViewOrigin());
	if (DistanceSqr > MaxDrawDistanceSqr)
	{
		return;
	}

	const UZoneShapeComponent* EditedShapeComp = GetEditedShapeComponent();
	const bool bIsActiveComponent = Component == EditedShapeComp;

	constexpr FColor NormalColor = FColor(255, 255, 255, 255);
	constexpr FColor SelectedColor = FColor(211, 93, 0, 255);
	constexpr FColor TangentColor = SelectedColor;
	
	const float GrabHandleSize = GetDefault<ULevelEditorViewportSettings>()->SelectedSplinePointSizeAdjustment + (bIsActiveComponent ? 10.0f : 0.0f);

	static constexpr float DepthBias = 0.0001f; // Little bias helps to make the lines visible when directly on top of geometry.
	static constexpr float HandlesDepthBias = 0.0002f; // A bit more than in the shape drawing, so that we get drawn on top
	static constexpr float LaneLineThickness = 2.0f;
	static constexpr float BoundaryLineThickness = 0.0f;

	TConstArrayView<FZoneShapePoint> ShapePoints = ShapeComp->GetPoints();
	check(SelectionState);

	// Lanes
	FZoneGraphStorage Zone;
	if (UZoneGraphSubsystem* ZoneGraph = UWorld::GetSubsystem<UZoneGraphSubsystem>(ShapeComp->GetWorld()))
	{
		ZoneGraph->GetBuilder().BuildSingleShape(*ShapeComp, FMatrix::Identity, Zone);
		Zone.DataHandle = FZoneGraphDataHandle(0xffff, 0xffff); // Give a valid handle so that the drawing happens correctly.
	}

	TConstArrayView<FZoneShapeConnector> Connectors = ShapeComp->GetShapeConnectors();
	TConstArrayView<FZoneShapeConnection> Connections = ShapeComp->GetConnectedShapes();

	PDI->SetHitProxy(nullptr);

	constexpr int32 ZoneIndex = 0; // We have only one zone in the storage, created above.
	constexpr bool bDrawDetails = true;
	const float ShapeAlpha = bIsActiveComponent ? 1.0f : 0.5f;
	UE::ZoneGraph::RenderingUtilities::FLaneHighlight LaneHighlight;

	// Highlight lanes that emanate from the selected point.
	if (bIsActiveComponent && ShapePoints.Num() > 0 && SelectionState->GetSelectedPoints().Num() > 0)
	{
		const int32 LastPointIndex = SelectionState->GetLastPointIndexSelected();
		if (ShapePoints.IsValidIndex(LastPointIndex))
		{
			const FZoneShapePoint& Point = ShapePoints[LastPointIndex];
			if (Point.Type == FZoneShapePointType::LaneProfile)
			{
				LaneHighlight.Position = LocalToWorld.TransformPosition(Point.Position);
				LaneHighlight.Rotation = LocalToWorld.ToQuat() * Point.Rotation.Quaternion();
				LaneHighlight.Width = Point.TangentLength;
			}
		}
	}

	// Draw boundary
	UE::ZoneGraph::RenderingUtilities::DrawZoneBoundary(Zone, ZoneIndex, PDI, LocalToWorld, BoundaryLineThickness, DepthBias, ShapeAlpha);

	// Draw Lanes
	PDI->SetHitProxy(new HZoneShapeVisProxy(Component));
	UE::ZoneGraph::RenderingUtilities::DrawZoneLanes(Zone, ZoneIndex, PDI, LocalToWorld, LaneLineThickness, DepthBias, ShapeAlpha, bDrawDetails, LaneHighlight);

	// Draw connectors
	for (int32 i = 0; i < Connectors.Num(); i++)
	{
		const FZoneShapeConnector& Connector = Connectors[i];
		const FZoneShapeConnection* Connection = i < Connections.Num() ? &Connections[i] : nullptr;
		PDI->SetHitProxy(new HZoneShapePointProxy(Component, Connector.PointIndex));
		UE::ZoneGraph::RenderingUtilities::DrawZoneShapeConnector(Connector, Connection, PDI, LocalToWorld, DepthBias);
	}

	// Segments
	if (ShapePoints.Num() > 1)
	{
		const int32 NumPoints = ShapePoints.Num();
		int StartIdx = ShapeComp->IsShapeClosed() ? (NumPoints - 1) : 0;
		int Idx = ShapeComp->IsShapeClosed() ? 0 : 1;

		TArray<FVector> CurvePoints;

		while (Idx < NumPoints)
		{
			const FZoneShapePoint& StartPoint = ShapePoints[StartIdx];
			const FZoneShapePoint& EndPoint = ShapePoints[Idx];

			FVector StartPosition(ForceInitToZero), StartControlPoint(ForceInitToZero), EndControlPoint(ForceInitToZero), EndPosition(ForceInitToZero);
			UE::ZoneShape::Utilities::GetCubicBezierPointsFromShapeSegment(StartPoint, EndPoint, LocalToWorld, StartPosition, StartControlPoint, EndControlPoint, EndPosition);

			PDI->SetHitProxy(new HZoneShapeSegmentProxy(Component, StartIdx));
			const FColor Color = (ShapeComp == EditedShapeComp && StartIdx == SelectionState->GetSelectedSegmentIndex()) ? SelectedColor : NormalColor;

			// TODO: Make this a setting or property on shape
			static constexpr float TessTolerance = 5.0f;
			CurvePoints.Reset();

			if (StartPoint.Type == FZoneShapePointType::LaneProfile)
			{
				CurvePoints.Add(LocalToWorld.TransformPosition(StartPoint.Position));
			}

			CurvePoints.Add(StartPosition);
			UE::CubicBezier::Tessellate(CurvePoints, StartPosition, StartControlPoint, EndControlPoint, EndPosition, TessTolerance);

			if (EndPoint.Type == FZoneShapePointType::LaneProfile)
			{
				CurvePoints.Add(LocalToWorld.TransformPosition(EndPoint.Position));
			}

			for (int32 i = 0; i < CurvePoints.Num() - 1; i++)
			{
				PDI->DrawLine(CurvePoints[i], CurvePoints[i + 1], Color, SDPG_Foreground, BoundaryLineThickness, HandlesDepthBias, true);
			}

			StartIdx = Idx;
			Idx++;
		}
	}

	// Draw handles on selected shapes
	if (bIsActiveComponent)
	{
		const int32 NumPoints = ShapePoints.Num();

		if (NumPoints == 0 && SelectionState->GetSelectedPoints().Num() > 0)
		{
			ChangeSelectionState(INDEX_NONE, false);
		}
		else
		{
			const TSet<int32> SelectedPointsCopy = SelectionState->GetSelectedPoints();
			for (int32 SelectedPoint : SelectedPointsCopy)
			{
				check(SelectedPoint >= 0);
				if (SelectedPoint >= NumPoints)
				{
					// Catch any keys that might not exist anymore due to the underlying component changing.
					ChangeSelectionState(SelectedPoint, true);
					continue;
				}

				const FZoneShapePoint& Point = ShapePoints[SelectedPoint];

				if (Point.Type == FZoneShapePointType::Bezier || Point.Type == FZoneShapePointType::LaneProfile)
				{
					const float TangentHandleSize = 8.0f + GetDefault<ULevelEditorViewportSettings>()->SplineTangentHandleSizeAdjustment;

					const FVector Position = LocalToWorld.TransformPosition(Point.Position);
					const FVector InControlPoint = LocalToWorld.TransformPosition(Point.GetInControlPoint());
					const FVector OutControlPoint = LocalToWorld.TransformPosition(Point.GetOutControlPoint());

					PDI->SetHitProxy(nullptr);

					PDI->DrawLine(Position, InControlPoint, TangentColor, SDPG_Foreground, 0.0, HandlesDepthBias);
					PDI->DrawLine(Position, OutControlPoint, TangentColor, SDPG_Foreground, 0.0, HandlesDepthBias);

					PDI->SetHitProxy(new HZoneShapeControlPointProxy(Component, SelectedPoint, true));
					PDI->DrawPoint(InControlPoint, TangentColor, TangentHandleSize, SDPG_Foreground);

					PDI->SetHitProxy(new HZoneShapeControlPointProxy(Component, SelectedPoint, false));
					PDI->DrawPoint(OutControlPoint, TangentColor, TangentHandleSize, SDPG_Foreground);

					PDI->SetHitProxy(nullptr);
				}
			}
		}
	}

	// Points
	for (int32 i = 0; i < ShapePoints.Num(); i++)
	{
		const FVector Point = LocalToWorld.TransformPosition(ShapePoints[i].Position);
		const FColor Color = (ShapeComp == EditedShapeComp && SelectionState->GetSelectedPoints().Contains(i)) ? SelectedColor : NormalColor;
		PDI->SetHitProxy(new HZoneShapePointProxy(Component, i));
		PDI->DrawPoint(Point, Color, GrabHandleSize, SDPG_Foreground);

#ifdef ZONEGRAPH_DEBUG_ROTATIONS
		const FRotator& Rot = ShapePoints[i].Rotation;
		const FVector Forward = LocalToWorld.TransformVector(Rot.RotateVector(FVector::ForwardVector));
		const FVector Side = LocalToWorld.TransformVector(Rot.RotateVector(FVector::RightVector));
		const FVector Up = LocalToWorld.TransformVector(Rot.RotateVector(FVector::UpVector));
		PDI->DrawLine(Point, Point + Forward * 40.0f, FColor::Red, SDPG_Foreground, 4.0f, HandlesDepthBias, true);
		PDI->DrawLine(Point, Point + Side * 40.0f, FColor::Green, SDPG_Foreground, 4.0f, HandlesDepthBias, true);
		PDI->DrawLine(Point, Point + Up * 40.0f, FColor::Blue, SDPG_Foreground, 4.0f, HandlesDepthBias, true);
#endif
	}

	PDI->SetHitProxy(nullptr);
}

void FZoneShapeComponentVisualizer::ChangeSelectionState(int32 Index, bool bIsCtrlHeld) const
{
	check(SelectionState);
	SelectionState->Modify();

	TSet<int32>& SelectedPoints = SelectionState->ModifySelectedPoints();
	if (Index == INDEX_NONE)
	{
		SelectedPoints.Empty();
		SelectionState->SetLastPointIndexSelected(INDEX_NONE);
	}
	else if (!bIsCtrlHeld)
	{
		SelectedPoints.Empty();
		SelectedPoints.Add(Index);
		SelectionState->SetLastPointIndexSelected(Index);
	}
	else
	{
		// Add or remove from selection if Ctrl is held
		if (SelectedPoints.Contains(Index))
		{
			// If already in selection, toggle it off
			SelectedPoints.Remove(Index);

			if (SelectionState->GetLastPointIndexSelected() == Index)
			{
				if (SelectedPoints.Num() == 0)
				{
					// Last key selected: clear last key index selected
					SelectionState->SetLastPointIndexSelected(INDEX_NONE);
				}
				else
				{
					// Arbitrarily set last key index selected to first member of the set (so that it is valid)
					SelectionState->SetLastPointIndexSelected(*SelectedPoints.CreateConstIterator());
				}
			}
		}
		else
		{
			// Add to selection
			SelectedPoints.Add(Index);
			SelectionState->SetLastPointIndexSelected(Index);
		}
	}
}

const UZoneShapeComponent* FZoneShapeComponentVisualizer::UpdateSelectedShapeComponent(const HComponentVisProxy* VisProxy)
{
	check(SelectionState);
	const UZoneShapeComponent* NewShapeComp = CastChecked<const UZoneShapeComponent>(VisProxy->Component.Get());
	check(NewShapeComp);

	AActor* OldShapeOwningActor = SelectionState->GetShapePropertyPath().GetParentOwningActor();
	UZoneShapeComponent* OldShapeComp = GetEditedShapeComponent();

	const FComponentPropertyPath NewShapePropertyPath(NewShapeComp);
	SelectionState->SetShapePropertyPath(NewShapePropertyPath);

	AActor* NewShapeOwningActor = NewShapePropertyPath.GetParentOwningActor();

	if (NewShapePropertyPath.IsValid())
	{
		if (OldShapeOwningActor != NewShapeOwningActor ||  OldShapeComp != NewShapeComp)
		{
			// Reset selection state if we are selecting a different actor to the one previously selected
			ChangeSelectionState(INDEX_NONE, false);
			SelectionState->SetSelectedSegmentIndex(INDEX_NONE);
			SelectionState->SetSelectedControlPoint(INDEX_NONE);
			SelectionState->SetSelectedControlPointType(FZoneShapeControlPointType::None);
		}

		if (OldShapeComp != NewShapeComp)
		{
			bIsSelectingComponent = true; // Prevent the selection from clearing our own selection state.
        	GEditor->SelectNone(/*bNoteSelectionChange*/true, /*bDeselectBSPSurfs*/true);
        	GEditor->SelectActor(NewShapeOwningActor, /*bInSelected*/false, /*bNotify*/true);
			GEditor->SelectComponent(const_cast<UZoneShapeComponent*>(NewShapeComp), /*bInSelected*/true, /*bNotify*/true);
			bIsSelectingComponent = false;
		}
		
		return NewShapeComp;
	}
	SelectionState->SetShapePropertyPath(FComponentPropertyPath());

	return nullptr;
}


bool FZoneShapeComponentVisualizer::GetLastSelectedPointRotation(FQuat& OutRotation) const
{
	bool bResult = false;
	if (const UZoneShapeComponent* ShapeComp = GetEditedShapeComponent())
	{
		check(SelectionState);
		const TConstArrayView<FZoneShapePoint> ShapePoints = ShapeComp->GetPoints();
		const int32 LastPointIndexSelected = SelectionState->GetLastPointIndexSelected();
		if (ShapePoints.IsValidIndex(LastPointIndexSelected))
		{
			check(SelectionState->GetSelectedPoints().Contains(LastPointIndexSelected));
			OutRotation = ShapeComp->GetComponentTransform().GetRotation() * ShapePoints[LastPointIndexSelected].Rotation.Quaternion();
			bResult = true;
		}
	}
	return bResult;
}

bool FZoneShapeComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	if (VisProxy && VisProxy->Component.IsValid())
	{
		if (VisProxy->IsA(HZoneShapePointProxy::StaticGetType()))
		{
			// Control point clicked
			const FScopedTransaction Transaction(LOCTEXT("SelectShapePoint", "Select Shape Point"));

			SelectionState->Modify();

			if (UpdateSelectedShapeComponent(VisProxy))
			{
				const HZoneShapePointProxy* PointProxy = static_cast<HZoneShapePointProxy*>(VisProxy);
				// Modify the selection state, unless right-clicking on an already selected key
				const TSet<int32>& SelectedPoints = SelectionState->GetSelectedPoints();
				if (Click.GetKey() != EKeys::RightMouseButton || !SelectedPoints.Contains(PointProxy->PointIndex))
				{
					ChangeSelectionState(PointProxy->PointIndex, InViewportClient->IsCtrlPressed());
				}
				SelectionState->SetSelectedSegmentIndex(INDEX_NONE);
				SelectionState->SetSelectedControlPoint(INDEX_NONE);
				SelectionState->SetSelectedControlPointType(FZoneShapeControlPointType::None);

				if (SelectionState->GetLastPointIndexSelected() == INDEX_NONE)
				{
					SelectionState->SetShapePropertyPath(FComponentPropertyPath());
					return false;
				}

				return true;
			}
		}
		else if (VisProxy->IsA(HZoneShapeSegmentProxy::StaticGetType()))
		{
			// Shape segment clicked
			const FScopedTransaction Transaction(LOCTEXT("SelectShapeSegment", "Select Shape Segment"));
			SelectionState->Modify();

			if (const UZoneShapeComponent* ShapeComp = UpdateSelectedShapeComponent(VisProxy))
			{
				const FTransform& LocalToWorld = ShapeComp->GetComponentTransform();
				const HZoneShapeSegmentProxy* SegmentProxy = static_cast<HZoneShapeSegmentProxy*>(VisProxy);

				// Find nearest point on shape.
				ChangeSelectionState(INDEX_NONE, false);
				SelectionState->SetSelectedSegmentIndex(SegmentProxy->SegmentIndex);
				SelectionState->SetSelectedControlPoint(INDEX_NONE);
				SelectionState->SetSelectedControlPointType(FZoneShapeControlPointType::None);

				const int32 NumPoints = ShapeComp->GetNumPoints();
				const int32 StartIndex = SegmentProxy->SegmentIndex;
				const int32 EndIndex = (SegmentProxy->SegmentIndex + 1) % NumPoints;

				const TConstArrayView<FZoneShapePoint> ShapePoints = ShapeComp->GetPoints();

				FVector StartPosition(0), StartControlPoint(0), EndControlPoint(0), EndPosition(0);
				UE::ZoneShape::Utilities::GetCubicBezierPointsFromShapeSegment(ShapePoints[StartIndex], ShapePoints[EndIndex], LocalToWorld.ToMatrixWithScale(), StartPosition, StartControlPoint, EndControlPoint, EndPosition);

				const FVector RaySegStart = Click.GetOrigin();
				const FVector RaySegEnd = Click.GetOrigin() + Click.GetDirection() * 50000.0f;

				FVector ClosestPoint;
				float ClosestT = 0.0f;

				UE::CubicBezier::SegmentClosestPointApproximate(RaySegStart, RaySegEnd, StartPosition, StartControlPoint, EndControlPoint, EndPosition, ClosestPoint, ClosestT);

				SelectionState->SetSelectedSegmentPoint(ClosestPoint);
				SelectionState->SetSelectedSegmentT(ClosestT);

				return true;
			}
		}
		else if (VisProxy->IsA(HZoneShapeControlPointProxy::StaticGetType()))
		{
			// Shape segment clicked
			const FScopedTransaction Transaction(LOCTEXT("SelectShapeSegment", "Select Shape Segment"));
			SelectionState->Modify();

			if (UpdateSelectedShapeComponent(VisProxy))
			{
				// Tangent handle clicked
				const HZoneShapeControlPointProxy* ControlPointProxy = static_cast<HZoneShapeControlPointProxy*>(VisProxy);

				// Note: don't change key selection when a tangent handle is clicked
				SelectionState->SetSelectedSegmentIndex(INDEX_NONE);
				SelectionState->SetSelectedControlPoint(ControlPointProxy->PointIndex);
				SelectionState->SetSelectedControlPointType(ControlPointProxy->bInControlPoint ? FZoneShapeControlPointType::In : FZoneShapeControlPointType::Out);

				return true;
			}
		}
		else if (VisProxy->IsA(HZoneShapeVisProxy::StaticGetType()))
		{
			// Control point clicked
			const FScopedTransaction Transaction(LOCTEXT("SelectShape", "Select Shape"));

			SelectionState->Modify();

			if (UpdateSelectedShapeComponent(VisProxy))
			{
				ChangeSelectionState(INDEX_NONE, false);
				SelectionState->SetSelectedSegmentIndex(INDEX_NONE);
				SelectionState->SetSelectedControlPoint(INDEX_NONE);
				SelectionState->SetSelectedControlPointType(FZoneShapeControlPointType::None);

				return true;
			}
		}
	}

	return false;
}


UZoneShapeComponent* FZoneShapeComponentVisualizer::GetEditedShapeComponent() const
{
	check(SelectionState);
	return Cast<UZoneShapeComponent>(SelectionState->GetShapePropertyPath().GetComponent());
}

UActorComponent* FZoneShapeComponentVisualizer::GetEditedComponent() const
{
	return Cast<UActorComponent>(GetEditedShapeComponent());
}

bool FZoneShapeComponentVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	if (const UZoneShapeComponent* ShapeComp = GetEditedShapeComponent())
	{
		check(SelectionState);
		const TConstArrayView<FZoneShapePoint> ShapePoints = ShapeComp->GetPoints();

		if (SelectionState->GetSelectedControlPoint() != INDEX_NONE)
		{
			// If control point index is set, use that
			if (bControlPointPositionCaptured)
			{
				OutLocation = ShapeComp->GetComponentTransform().TransformPosition(ControlPointPosition);
			}
			else
			{
				check(SelectionState->GetSelectedControlPoint() < ShapePoints.Num());
				const FZoneShapePoint& Point = ShapePoints[SelectionState->GetSelectedControlPoint()];
				if (SelectionState->GetSelectedControlPointType() == FZoneShapeControlPointType::Out)
				{
					OutLocation = ShapeComp->GetComponentTransform().TransformPosition(Point.GetOutControlPoint());
				}
				else
				{
					OutLocation = ShapeComp->GetComponentTransform().TransformPosition(Point.GetInControlPoint());
				}
			}

			return true;
		}
		else if (SelectionState->GetSelectedSegmentIndex() != INDEX_NONE)
		{
			return false;
		}
		else if (SelectionState->GetLastPointIndexSelected() != INDEX_NONE)
		{
			// Otherwise use the last key index set
			const int32 LastPointIndexSelected = SelectionState->GetLastPointIndexSelected();
			check(LastPointIndexSelected >= 0);
			if (LastPointIndexSelected < ShapePoints.Num())
			{
				check(SelectionState->GetSelectedPoints().Contains(LastPointIndexSelected));
				const FZoneShapePoint& Point = ShapePoints[LastPointIndexSelected];
				OutLocation = ShapeComp->GetComponentTransform().TransformPosition(Point.Position);
				OutLocation += DuplicateAccumulatedDrag;
				return true;
			}
		}
	}

	return false;
}

bool FZoneShapeComponentVisualizer::GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const
{
	bool bResult = false;
	if (bHasCachedRotation)
	{
		OutMatrix = FRotationMatrix::Make(CachedRotation);
		bResult = true;
	}
	else
	{
		if (ViewportClient->GetWidgetCoordSystemSpace() == COORD_Local || ViewportClient->GetWidgetMode() == UE::Widget::WM_Rotate)
		{
			FQuat Rotation = FQuat::Identity;
			if (GetLastSelectedPointRotation(Rotation))
			{
				OutMatrix = FRotationMatrix::Make(Rotation);
				bResult = true;
			}
		}
	}

	return bResult;
}

bool FZoneShapeComponentVisualizer::IsVisualizingArchetype() const
{
	UZoneShapeComponent* ShapeComp = GetEditedShapeComponent();
	return (ShapeComp && ShapeComp->GetOwner() && FActorEditorUtils::IsAPreviewOrInactiveActor(ShapeComp->GetOwner()));
}


bool FZoneShapeComponentVisualizer::IsAnySelectedPointIndexOutOfRange(const UZoneShapeComponent& Comp) const
{
	check(SelectionState);
	const TSet<int32>& SelectedPoints = SelectionState->GetSelectedPoints();
	const int32 NumPoints = Comp.GetNumPoints();
	return Algo::AnyOf(SelectedPoints, [NumPoints](int32 Index) { return Index >= NumPoints; });
}

bool FZoneShapeComponentVisualizer::IsSinglePointSelected() const
{
	UZoneShapeComponent* ShapeComp = GetEditedShapeComponent();
	check(SelectionState);
	const TSet<int32>& SelectedPoints = SelectionState->GetSelectedPoints();
	return (ShapeComp != nullptr &&
		SelectedPoints.Num() == 1 &&
		SelectionState->GetLastPointIndexSelected() != INDEX_NONE);
}

bool FZoneShapeComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslate, FRotator& DeltaRotate, FVector& DeltaScale)
{
	if (const UZoneShapeComponent* ShapeComp = GetEditedShapeComponent())
	{
		check(SelectionState);

		if (IsAnySelectedPointIndexOutOfRange(*ShapeComp))
		{
			// Something external has changed the number of shape points, meaning that the cached selected keys are no longer valid
			EndEditing();
			return false;
		}

		if (SelectionState->GetSelectedControlPoint() != INDEX_NONE)
		{
			return TransformSelectedControlPoint(DeltaTranslate);
		}
		else if (SelectionState->GetSelectedPoints().Num() > 0)
		{
			if (ViewportClient->IsAltPressed())
			{
				if (ViewportClient->GetWidgetMode() == UE::Widget::WM_Translate && ViewportClient->GetCurrentWidgetAxis() != EAxisList::None)
				{
					if (bAllowDuplication)
					{
						static const float DuplicationDeadZoneSqr = FMath::Square(10.0f);

						DuplicateAccumulatedDrag += DeltaTranslate;
						if (DuplicateAccumulatedDrag.SizeSquared() >= DuplicationDeadZoneSqr)
						{
							DuplicatePointForAltDrag(DuplicateAccumulatedDrag);
							DuplicateAccumulatedDrag = FVector::ZeroVector;
							bAllowDuplication = false;
						}

						return true;
					}
					else
					{
						return TransformSelectedPoints(ViewportClient, DeltaTranslate, DeltaRotate, DeltaScale);
					}
				}
			}
			else
			{
				return TransformSelectedPoints(ViewportClient, DeltaTranslate, DeltaRotate, DeltaScale);
			}
		}
	}

	return false;
}

bool FZoneShapeComponentVisualizer::TransformSelectedControlPoint(const FVector& DeltaTranslate)
{
	if (UZoneShapeComponent* ShapeComp = GetEditedShapeComponent())
	{
		check(SelectionState);
		check(SelectionState->GetSelectedControlPoint() != INDEX_NONE);

		TArray<FZoneShapePoint>& ShapePoints = ShapeComp->GetMutablePoints();
		const int32 NumPoints = ShapePoints.Num();
		check(SelectionState->GetSelectedControlPoint() < NumPoints);

		if (!DeltaTranslate.IsZero())
		{
			ShapeComp->Modify();

			if (!bControlPointPositionCaptured)
			{
				// We capture the control point position on first update and use that as the gizmo position.
				// That allows us to constrain the handle locations as needed, and have the gizmo follow the user input.
				bControlPointPositionCaptured = true;

				const FZoneShapePoint& EditedPoint = ShapePoints[SelectionState->GetSelectedControlPoint()];
				if (EditedPoint.Type == FZoneShapePointType::Bezier || EditedPoint.Type == FZoneShapePointType::LaneProfile)
				{
					if (SelectionState->GetSelectedControlPointType() == FZoneShapeControlPointType::Out)
					{
						ControlPointPosition = EditedPoint.GetOutControlPoint();
					}
					else
					{
						ControlPointPosition = EditedPoint.GetInControlPoint();
					}
				}
			}

			ControlPointPosition += ShapeComp->GetComponentTransform().InverseTransformVector(DeltaTranslate);

			FZoneShapePoint& EditedPoint = ShapePoints[SelectionState->GetSelectedControlPoint()];

			if (EditedPoint.Type == FZoneShapePointType::Bezier || EditedPoint.Type == FZoneShapePointType::LaneProfile)
			{
				// Note: Lane control points will get adjusted to fit the lane profile in UpdateShape() below.
				if (SelectionState->GetSelectedControlPointType() == FZoneShapeControlPointType::Out)
				{
					EditedPoint.SetOutControlPoint(ControlPointPosition);
				}
				else
				{
					EditedPoint.SetInControlPoint(ControlPointPosition);
				}
			}
		}

		ShapeComp->UpdateShape();
		NotifyPropertyModified(ShapeComp, ShapePointsProperty);

		return true;
	}

	return false;
}

bool FZoneShapeComponentVisualizer::TransformSelectedPoints(const FEditorViewportClient* ViewportClient, const FVector& DeltaTranslate, const FRotator& DeltaRotate, const FVector& DeltaScale) const
{
	if (UZoneShapeComponent* ShapeComp = GetEditedShapeComponent())
	{
		check(SelectionState);
		TArray<FZoneShapePoint>& ShapePoints = ShapeComp->GetMutablePoints();
		const int32 NumPoints = ShapePoints.Num();
		check(SelectionState->GetLastPointIndexSelected() != INDEX_NONE);
		check(SelectionState->GetLastPointIndexSelected() >= 0);
		check(SelectionState->GetLastPointIndexSelected() < NumPoints);
		const TSet<int32>& SelectedPoints = SelectionState->GetSelectedPoints();
		const int32 LastPointIndexSelected = SelectionState->GetLastPointIndexSelected();
		check(SelectedPoints.Num() > 0);
		check(SelectedPoints.Contains(LastPointIndexSelected));

		ShapeComp->Modify();

		for (const int32 SelectedIndex : SelectedPoints)
		{
			check(SelectedIndex >= 0);
			check(SelectedIndex < NumPoints);

			FZoneShapePoint& EditedPoint = ShapePoints[SelectedIndex];

			if (!DeltaTranslate.IsZero())
			{
				const FVector LocalDelta = ShapeComp->GetComponentTransform().InverseTransformVector(DeltaTranslate);
				EditedPoint.Position += LocalDelta;
			}

			if (!DeltaRotate.IsZero())
			{
				FQuat NewRot = ShapeComp->GetComponentTransform().GetRotation() * EditedPoint.Rotation.Quaternion(); // convert local-space rotation to world-space
				NewRot = DeltaRotate.Quaternion() * NewRot; // apply world-space rotation
				NewRot = ShapeComp->GetComponentTransform().GetRotation().Inverse() * NewRot; // convert world-space rotation to local-space
				EditedPoint.Rotation = NewRot.Rotator();
			}

			if (DeltaScale.X != 0.0f)
			{
				if (EditedPoint.Type == FZoneShapePointType::Bezier)
				{
					EditedPoint.TangentLength *= (1.0f + DeltaScale.X);
				}
			}
		}

		ShapeComp->UpdateShape();
		NotifyPropertyModified(ShapeComp, ShapePointsProperty);
		GEditor->RedrawLevelEditingViewports(true);

		return true;
	}

	return false;
}

bool FZoneShapeComponentVisualizer::HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	bool bHandled = false;

	UZoneShapeComponent* ShapeComp = GetEditedShapeComponent();
	if (ShapeComp != nullptr && IsAnySelectedPointIndexOutOfRange(*ShapeComp))
	{
		// Something external has changed the number of shape points, meaning that the cached selected keys are no longer valid
		EndEditing();
		return false;
	}

	if (Key == EKeys::LeftMouseButton && Event == IE_Released)
	{
		// Reset duplication on LMB release
		bAllowDuplication = true;
		DuplicateAccumulatedDrag = FVector::ZeroVector;

		bControlPointPositionCaptured = false;
		ControlPointPosition = FVector::ZeroVector;

		bHasCachedRotation = false;
		CachedRotation = FQuat::Identity;
	}

	if (Key == EKeys::LeftMouseButton && Event == IE_Pressed)
	{
		bHasCachedRotation = false;
		CachedRotation = FQuat::Identity;

		// Cache the widget rotation when mouse is pressed down to avoid feedback effects during gizmo interaction.
		if (ViewportClient->GetWidgetCoordSystemSpace() == COORD_Local || ViewportClient->GetWidgetMode() == UE::Widget::WM_Rotate)
		{
			bHasCachedRotation = GetLastSelectedPointRotation(CachedRotation);
		}
	}

	if (Event == IE_Pressed)
	{
		bHandled = ShapeComponentVisualizerActions->ProcessCommandBindings(Key, FSlateApplication::Get().GetModifierKeys(), false);
	}

	return bHandled;
}

bool FZoneShapeComponentVisualizer::HandleBoxSelect(const FBox& InBox, FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	const FScopedTransaction Transaction(LOCTEXT("HandleBoxSelect", "Box Select Shape Points"));
	check(SelectionState);
	SelectionState->Modify();

	if (const UZoneShapeComponent* ShapeComp = GetEditedShapeComponent())
	{
		bool bSelectionChanged = false;

		const TConstArrayView<FZoneShapePoint> ShapePoints = ShapeComp->GetPoints();
		const int32 NumPoints = ShapePoints.Num();
		const FTransform& LocalToWorld = ShapeComp->GetComponentTransform();

		// Shape control point selection always uses transparent box selection.
		for (int32 Idx = 0; Idx < NumPoints; Idx++)
		{
			const FVector WorldPos = LocalToWorld.TransformPosition(ShapePoints[Idx].Position);
			if (InBox.IsInside(WorldPos))
			{
				ChangeSelectionState(Idx, true);
				bSelectionChanged = true;
			}
		}

		if (bSelectionChanged)
		{
			SelectionState->SetSelectedSegmentIndex(INDEX_NONE);
			SelectionState->SetSelectedControlPoint(INDEX_NONE);
			SelectionState->SetSelectedControlPointType(FZoneShapeControlPointType::None);
		}
	}

	return true;
}

bool FZoneShapeComponentVisualizer::HandleFrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, FViewport* InViewport)
{
	const FScopedTransaction Transaction(LOCTEXT("HandleFrustumSelect", "Frustum Select Shape Points"));
	check(SelectionState);
	SelectionState->Modify();

	if (const UZoneShapeComponent* ShapeComp = GetEditedShapeComponent())
	{
		bool bSelectionChanged = false;

		const TConstArrayView<FZoneShapePoint> ShapePoints = ShapeComp->GetPoints();
		const int32 NumPoints = ShapePoints.Num();
		const FTransform& LocalToWorld = ShapeComp->GetComponentTransform();

		// Shape control point selection always uses transparent box selection.
		for (int32 Idx = 0; Idx < NumPoints; Idx++)
		{
			const FVector WorldPos = LocalToWorld.TransformPosition(ShapePoints[Idx].Position);
			if (InFrustum.IntersectPoint(WorldPos))
			{
				ChangeSelectionState(Idx, true);
				bSelectionChanged = true;
			}
		}

		if (bSelectionChanged)
		{
			SelectionState->SetSelectedSegmentIndex(INDEX_NONE);
			SelectionState->SetSelectedControlPoint(INDEX_NONE);
			SelectionState->SetSelectedControlPointType(FZoneShapeControlPointType::None);
		}
	}

	return true;
}

bool FZoneShapeComponentVisualizer::HasFocusOnSelectionBoundingBox(FBox& OutBoundingBox)
{
	OutBoundingBox.Init();

	if (const UZoneShapeComponent* ShapeComp = GetEditedShapeComponent())
	{
		check(SelectionState);
		const TSet<int32>& SelectedPoints = SelectionState->GetSelectedPoints();

		if (SelectedPoints.Num() > 0)
		{
			const TConstArrayView<FZoneShapePoint> ShapePoints = ShapeComp->GetPoints();
			const int32 NumPoints = ShapePoints.Num();
			const FTransform& LocalToWorld = ShapeComp->GetComponentTransform();

			// Shape control point selection always uses transparent box selection.
			for (const int32 Idx : SelectedPoints)
			{
				check(Idx >= 0);
				check(Idx < NumPoints);
				const FVector WorldPos = LocalToWorld.TransformPosition(ShapePoints[Idx].Position);
				OutBoundingBox += WorldPos;
			}

			OutBoundingBox = OutBoundingBox.ExpandBy(50.f);
			return true;
		}
	}

	return false;
}

bool FZoneShapeComponentVisualizer::HandleSnapTo(const bool bInAlign, const bool bInUseLineTrace, const bool bInUseBounds, const bool bInUsePivot, AActor* InDestination)
{
	// Does not handle Snap/Align Pivot, Snap/Align Bottom Control Points or Snap/Align to Actor.
	if (bInUsePivot || bInUseBounds || InDestination)
	{
		return false;
	}

	// Note: value of bInUseLineTrace is ignored as we always line trace from control points.
	if (UZoneShapeComponent* ShapeComp = GetEditedShapeComponent())
	{
		check(SelectionState);
		const TSet<int32>& SelectedPoints = SelectionState->GetSelectedPoints();
		if (SelectedPoints.Num() > 0)
		{
			TArray<FZoneShapePoint>& ShapePoints = ShapeComp->GetMutablePoints();
			const int32 NumPoints = ShapePoints.Num();

			check(SelectionState->GetLastPointIndexSelected() != INDEX_NONE);
			check(SelectionState->GetLastPointIndexSelected() >= 0);
			check(SelectionState->GetLastPointIndexSelected() < NumPoints);
			check(SelectedPoints.Contains(SelectionState->GetLastPointIndexSelected()));

			ShapeComp->Modify();

			bool bMovedKey = false;

			// Shape control point selection always uses transparent box selection.
			for (int32 Idx : SelectedPoints)
			{
				check(Idx >= 0);
				check(Idx < NumPoints);

				FVector Direction = FVector(0.f, 0.f, -1.f);

				FZoneShapePoint& EditedPoint = ShapePoints[Idx];

				FHitResult Hit(1.0f);
				FCollisionQueryParams Params(SCENE_QUERY_STAT(MoveShapePointToTrace), true);

				// Find key position in world space
				const FVector CurrentWorldPos = ShapeComp->GetComponentTransform().TransformPosition(EditedPoint.Position);

				if (ShapeComp->GetWorld()->LineTraceSingleByChannel(Hit, CurrentWorldPos, CurrentWorldPos + Direction * WORLD_MAX, ECC_WorldStatic, Params))
				{
					// Convert back to local space
					EditedPoint.Position = ShapeComp->GetComponentTransform().InverseTransformPosition(Hit.Location);

					if (bInAlign && EditedPoint.Type == FZoneShapePointType::Bezier)
					{
						// Get delta rotation between up vector and hit normal
						FQuat DeltaRotate = FQuat::FindBetweenNormals(FVector::UpVector, Hit.Normal);

						// Rotate tangent according to delta rotation
						const FVector WorldPosition = ShapeComp->GetComponentTransform().TransformPosition(EditedPoint.Position);
						const FVector WorldInControlPoint = ShapeComp->GetComponentTransform().TransformPosition(EditedPoint.GetInControlPoint());
						const FVector WorldTangent = WorldInControlPoint - WorldPosition;
						FVector NewTangent = DeltaRotate.RotateVector(WorldTangent);
						NewTangent = ShapeComp->GetComponentTransform().InverseTransformVector(NewTangent);
						EditedPoint.SetInControlPoint(EditedPoint.Position + NewTangent);
					}

					bMovedKey = true;
				}
			}

			if (bMovedKey)
			{
				ShapeComp->UpdateShape();
				NotifyPropertyModified(ShapeComp, ShapePointsProperty);
				GEditor->RedrawLevelEditingViewports(true);
			}

			return true;
		}
	}

	return false;
}

void FZoneShapeComponentVisualizer::EndEditing()
{
	// Ignore if there is an undo/redo operation in progress
	if (!GIsTransacting)
	{
		return;
	}

	// Ignore if this happens during selection.
	if (bIsSelectingComponent)
	{
		return;
	}
	
	check(SelectionState);
	SelectionState->Modify();
	if (GetEditedShapeComponent())
	{
		ChangeSelectionState(INDEX_NONE, false);
		SelectionState->SetSelectedSegmentIndex(INDEX_NONE);
		SelectionState->SetSelectedControlPoint(INDEX_NONE);
		SelectionState->SetSelectedControlPointType(FZoneShapeControlPointType::None);
	}
	SelectionState->SetShapePropertyPath(FComponentPropertyPath());
}

void FZoneShapeComponentVisualizer::OnDuplicatePoint() const
{
	DuplicateSelectedPoints();
}

bool FZoneShapeComponentVisualizer::CanAddPointToSegment() const
{
	if (const UZoneShapeComponent* ShapeComp = GetEditedShapeComponent())
	{
		check(SelectionState);
		const int32 SelectedSegmentIndex = SelectionState->GetSelectedSegmentIndex();
		return (SelectedSegmentIndex != INDEX_NONE && SelectedSegmentIndex >= 0 && SelectedSegmentIndex < ShapeComp->GetNumPoints());
	}
	return false;
}

void FZoneShapeComponentVisualizer::OnAddPointToSegment() const
{
	const FScopedTransaction Transaction(LOCTEXT("AddShapePoint", "Add Shape Point"));
	UZoneShapeComponent* ShapeComp = GetEditedShapeComponent();
	check(ShapeComp != nullptr);
	const int32 SelectedSegmentIndex = SelectionState->GetSelectedSegmentIndex();
	check(SelectionState);
	check(SelectedSegmentIndex != INDEX_NONE);
	check(SelectedSegmentIndex >= 0);
	check(SelectedSegmentIndex < ShapeComp->GetNumSegments());

	SelectionState->Modify();

	SplitSegment(SelectionState->GetSelectedSegmentIndex(), SelectionState->GetSelectedSegmentT());

	SelectionState->SetSelectedSegmentPoint(FVector::ZeroVector);
	SelectionState->SetSelectedSegmentIndex(INDEX_NONE);
}

void FZoneShapeComponentVisualizer::DuplicateSelectedPoints(const FVector& WorldOffset, bool bInsertAfter) const
{
	const FScopedTransaction Transaction(LOCTEXT("DuplicatePoint", "Duplicate Point"));

	UZoneShapeComponent* ShapeComp = GetEditedShapeComponent();
	check(ShapeComp != nullptr);
	check(SelectionState);
	TSet<int32>& SelectedPoints = SelectionState->ModifySelectedPoints();
	const int32 LastPointIndexSelected = SelectionState->GetLastPointIndexSelected();
	check(LastPointIndexSelected != INDEX_NONE);
	check(LastPointIndexSelected >= 0);
	check(LastPointIndexSelected < ShapeComp->GetNumPoints());
	check(SelectedPoints.Num() > 0);
	check(SelectedPoints.Contains(LastPointIndexSelected));

	SelectionState->Modify();

	ShapeComp->Modify();
	if (AActor* Owner = ShapeComp->GetOwner())
	{
		Owner->Modify();
	}

	TArray<int32> SelectedPointsSorted;
	for (int32 SelectedIndex : SelectedPoints)
	{
		SelectedPointsSorted.Add(SelectedIndex);
	}
	SelectedPointsSorted.Sort([](int32 A, int32 B) { return A < B; });

	TArray<FZoneShapePoint>& ShapePoints = ShapeComp->GetMutablePoints();

	// Make copies of the points and adjust them based on the requested offset.
	const FVector LocalOffset = ShapeComp->GetComponentTransform().InverseTransformVector(WorldOffset);
	TArray<FZoneShapePoint> SelectedPointsCopy;
	for (const int32 SelectedIndex : SelectedPointsSorted)
	{
		FZoneShapePoint& Point = SelectedPointsCopy.Add_GetRef(ShapePoints[SelectedIndex]);
		Point.Position += LocalOffset;
	}

	SelectedPoints.Empty();

	// The offset is incremented each time a point to make sure that the following points are inserted at after their copies too.
	int32 Offset = bInsertAfter ? 1 : 0;
	for (int32 i = 0; i < SelectedPointsSorted.Num(); i++)
	{
		// Add new point
		const int32 SelectedIndex = SelectedPointsSorted[i];
		const FZoneShapePoint& Point = SelectedPointsCopy[i];
		const int32 InsertIndex = SelectedIndex + Offset;
		check(InsertIndex <= ShapePoints.Num());
		ShapePoints.Insert(Point, InsertIndex);

		// Adjust selection
		if (LastPointIndexSelected == SelectedIndex)
		{
			SelectionState->SetLastPointIndexSelected(InsertIndex);
		}
		SelectedPoints.Add(InsertIndex);

		Offset++;
	}

	ShapeComp->UpdateShape();

	// Unset tangent handle selection
	SelectionState->SetSelectedControlPoint(INDEX_NONE);
	SelectionState->SetSelectedControlPointType(FZoneShapeControlPointType::None);

	NotifyPropertyModified(ShapeComp, ShapePointsProperty);

	GEditor->RedrawLevelEditingViewports(true);
}

bool FZoneShapeComponentVisualizer::DuplicatePointForAltDrag(const FVector& InDrag) const
{
	UZoneShapeComponent* ShapeComp = GetEditedShapeComponent();
	check(ShapeComp != nullptr);
	check(SelectionState);
	const TSet<int32>& SelectedPoints = SelectionState->GetSelectedPoints();
	const int32 LastPointIndexSelected = SelectionState->GetLastPointIndexSelected();
	const int32 NumPoints = ShapeComp->GetNumPoints();
	check(LastPointIndexSelected != INDEX_NONE);
	check(LastPointIndexSelected >= 0);
	check(LastPointIndexSelected < NumPoints);
	check(SelectedPoints.Contains(LastPointIndexSelected));

	// Calculate approximate tangent around the current point.
	int32 PrevIndex = 0;
	int32 NextIndex = 0;
	if (ShapeComp->IsShapeClosed())
	{
		PrevIndex = (LastPointIndexSelected + NumPoints - 1) % NumPoints;
		NextIndex = (LastPointIndexSelected + 1) % NumPoints;
	}
	else
	{
		PrevIndex = FMath::Max(0, LastPointIndexSelected - 1);
		NextIndex = FMath::Min(LastPointIndexSelected + 1, NumPoints - 1);
	}

	const TConstArrayView<FZoneShapePoint> ShapePoints = ShapeComp->GetPoints();
	const FVector PrevPoint = ShapePoints[PrevIndex].Position;
	const FVector NextPoint = ShapePoints[NextIndex].Position;
	const FVector TangentDir = (NextPoint - PrevPoint).GetSafeNormal();

	// Detect where to insert the point based on if we're dragging towards the next point or previous point.
	const bool bInsertAfter = FVector::DotProduct(TangentDir, InDrag) > 0.0f;

	DuplicateSelectedPoints(InDrag, bInsertAfter);

	return true;
}

void FZoneShapeComponentVisualizer::SplitSegment(const int32 InSegmentIndex, const float SegmentSplitT) const
{
	UZoneShapeComponent* ShapeComp = GetEditedShapeComponent();

	check(ShapeComp != nullptr);
	check(InSegmentIndex != INDEX_NONE);
	check(InSegmentIndex >= 0);
	check(InSegmentIndex < ShapeComp->GetNumSegments());

	ShapeComp->Modify();
	if (AActor* Owner = ShapeComp->GetOwner())
	{
		Owner->Modify();
	}

	TArray<FZoneShapePoint>& ShapePoints = ShapeComp->GetMutablePoints();
	const int32 NumPoints = ShapePoints.Num();
	const int32 StartPointIdx = InSegmentIndex;
	const int32 EndPointIdx = (InSegmentIndex + 1) % NumPoints;
	const FZoneShapePoint& StartPoint = ShapePoints[StartPointIdx];
	const FZoneShapePoint& EndPoint = ShapePoints[EndPointIdx];

	FVector StartPosition(ForceInitToZero), StartControlPoint(ForceInitToZero), EndControlPoint(ForceInitToZero), EndPosition(ForceInitToZero);
	UE::ZoneShape::Utilities::GetCubicBezierPointsFromShapeSegment(StartPoint, EndPoint, FMatrix::Identity, StartPosition, StartControlPoint, EndControlPoint, EndPosition);

	FZoneShapePoint NewPoint;
	NewPoint.Position = UE::CubicBezier::Eval(StartPosition, StartControlPoint, EndControlPoint, EndPosition, SegmentSplitT);


	// Set new point type based on neighbors
	if (StartPoint.Type == FZoneShapePointType::AutoBezier || EndPoint.Type == FZoneShapePointType::AutoBezier)
	{
		// Auto bezier handles will be updated in UpdateShape()
		NewPoint.Type = FZoneShapePointType::AutoBezier;
	}
	else if (StartPoint.Type == FZoneShapePointType::Bezier || EndPoint.Type == FZoneShapePointType::Bezier)
	{
		// Initial Bezier handles are created below, after insert.
		NewPoint.Type = FZoneShapePointType::Bezier;
	}
	else
	{
		NewPoint.Type = FZoneShapePointType::Sharp;
		NewPoint.TangentLength = 0.0f;
	}

	const int NewPointIndex = InSegmentIndex + 1;

	ShapePoints.Insert(NewPoint, NewPointIndex);

	// Create sane default tangent for Bezier points.
	if (NewPoint.Type == FZoneShapePointType::Bezier)
	{
		ShapeComp->UpdatePointRotationAndTangent(NewPointIndex);
	}

	// Set selection to new point
	ChangeSelectionState(NewPointIndex, false);

	ShapeComp->UpdateShape();
	NotifyPropertyModified(ShapeComp, ShapePointsProperty);

	GEditor->RedrawLevelEditingViewports(true);
}

void FZoneShapeComponentVisualizer::OnDeletePoint() const
{
	const FScopedTransaction Transaction(LOCTEXT("DeletePoint", "Delete Points"));
	UZoneShapeComponent* ShapeComp = GetEditedShapeComponent();
	check(ShapeComp != nullptr);
	check(SelectionState);
	const TSet<int32>& SelectedPoints = SelectionState->GetSelectedPoints();
	const int32 LastPointIndexSelected = SelectionState->GetLastPointIndexSelected();
	check(LastPointIndexSelected != INDEX_NONE);
	check(LastPointIndexSelected >= 0);
	check(LastPointIndexSelected < ShapeComp->GetNumPoints());
	check(SelectedPoints.Num() > 0);
	check(SelectedPoints.Contains(LastPointIndexSelected));

	ShapeComp->Modify();
	if (AActor* Owner = ShapeComp->GetOwner())
	{
		Owner->Modify();
	}

	// Get a sorted list of all the selected indices, highest to lowest
	TArray<int32> SelectedPointsSorted;
	for (int32 SelectedIndex : SelectedPoints)
	{
		SelectedPointsSorted.Add(SelectedIndex);
	}
	SelectedPointsSorted.Sort([](int32 A, int32 B) { return A > B; });

	// Delete selected keys from list, highest index first
	TArray<FZoneShapePoint>& ShapePoints = ShapeComp->GetMutablePoints();
	for (const int32 SelectedIndex : SelectedPointsSorted)
	{
		if (ShapePoints.Num() <= 2)
		{
			// Keep at least 2 points
			break;
		}

		ShapePoints.RemoveAt(SelectedIndex);
	}

	// Clear selection
	ChangeSelectionState(INDEX_NONE, false);
	SelectionState->SetSelectedSegmentIndex(INDEX_NONE);
	SelectionState->SetSelectedControlPoint(INDEX_NONE);
	SelectionState->SetSelectedControlPointType(FZoneShapeControlPointType::None);

	ShapeComp->UpdateShape();
	NotifyPropertyModified(ShapeComp, ShapePointsProperty);

	GEditor->RedrawLevelEditingViewports(true);
}


bool FZoneShapeComponentVisualizer::CanDeletePoint() const
{
	check(SelectionState);
	const TSet<int32>& SelectedPoints = SelectionState->GetSelectedPoints();
	const int32 LastPointIndexSelected = SelectionState->GetLastPointIndexSelected();
	UZoneShapeComponent* ShapeComp = GetEditedShapeComponent();
	return (ShapeComp != nullptr &&
		SelectedPoints.Num() > 0 &&
		SelectedPoints.Num() != ShapeComp->GetNumPoints() &&
		LastPointIndexSelected != INDEX_NONE);
}


bool FZoneShapeComponentVisualizer::IsPointSelectionValid() const
{
	check(SelectionState);
	const TSet<int32>& SelectedPoints = SelectionState->GetSelectedPoints();
	const int32 LastPointIndexSelected = SelectionState->GetLastPointIndexSelected();
	UZoneShapeComponent* ShapeComp = GetEditedShapeComponent();
	return (ShapeComp != nullptr &&
		SelectedPoints.Num() > 0 &&
		LastPointIndexSelected != INDEX_NONE);
}

void FZoneShapeComponentVisualizer::OnSetPointType(FZoneShapePointType NewType) const
{
	UZoneShapeComponent* ShapeComp = GetEditedShapeComponent();
	check(ShapeComp != nullptr);
	check(SelectionState);
	const TSet<int32>& SelectedPoints = SelectionState->GetSelectedPoints();

	const FScopedTransaction Transaction(LOCTEXT("SetPointType", "Set Point Type"));

	ShapeComp->Modify();
	if (AActor* Owner = ShapeComp->GetOwner())
	{
		Owner->Modify();
	}

	TArray<FZoneShapePoint>& ShapePoints = ShapeComp->GetMutablePoints();

	for (const int32 SelectedIndex : SelectedPoints)
	{
		check(SelectedIndex >= 0);
		check(SelectedIndex < ShapePoints.Num());

		FZoneShapePoint& Point = ShapePoints[SelectedIndex];
		if (Point.Type != NewType)
		{
			const FZoneShapePointType OldType = Point.Type;
			Point.Type = NewType;
			if (Point.Type == FZoneShapePointType::Sharp)
			{
				Point.TangentLength = 0.0f;
			}
			else if (OldType == FZoneShapePointType::Sharp)
			{
				if (Point.Type == FZoneShapePointType::Bezier || Point.Type == FZoneShapePointType::LaneProfile)
				{
					// Initialize bezier points with auto tangents.
					ShapeComp->UpdatePointRotationAndTangent(SelectedIndex);
				}
			}
			else if (OldType == FZoneShapePointType::LaneProfile && Point.Type != FZoneShapePointType::LaneProfile)
			{
				// Change forward to point along tangent.
				Point.Rotation.Yaw -= 90.0f;
			}
			else if (OldType != FZoneShapePointType::LaneProfile && Point.Type == FZoneShapePointType::LaneProfile)
			{
				// Change forward to point inside the shape.
				Point.Rotation.Yaw += 90.0f;
			}
		}
	}

	ShapeComp->UpdateShape();
	NotifyPropertyModified(ShapeComp, ShapePointsProperty);

	GEditor->RedrawLevelEditingViewports(true);
}


bool FZoneShapeComponentVisualizer::IsPointTypeSet(FZoneShapePointType Type) const
{
	if (IsPointSelectionValid())
	{
		UZoneShapeComponent* ShapeComp = GetEditedShapeComponent();
		check(ShapeComp != nullptr);
		check(SelectionState);
		const TSet<int32>& SelectedPoints = SelectionState->GetSelectedPoints();

		const TConstArrayView<FZoneShapePoint> ShapePoints = ShapeComp->GetPoints();

		for (const int32 SelectedIndex : SelectedPoints)
		{
			check(SelectedIndex >= 0);
			check(SelectedIndex < ShapePoints.Num());
			if (ShapePoints[SelectedIndex].Type == Type)
			{
				return true;
			}
		}
	}

	return false;
}

void FZoneShapeComponentVisualizer::OnSelectAllPoints() const
{
	if (const UZoneShapeComponent* ShapeComp = GetEditedShapeComponent())
	{
		check(SelectionState);
		TSet<int32>& SelectedPoints = SelectionState->ModifySelectedPoints();

		const FScopedTransaction Transaction(LOCTEXT("SelectAllPoints", "Select All Points"));

		SelectionState->Modify();
		SelectedPoints.Empty();

		// Shape control point selection always uses transparent box selection.
		const int32 NumPoints = ShapeComp->GetNumPoints();
		for (int32 Idx = 0; Idx < NumPoints; Idx++)
		{
			SelectedPoints.Add(Idx);
		}

		SelectionState->SetLastPointIndexSelected(NumPoints - 1);
		SelectionState->SetSelectedSegmentIndex(INDEX_NONE);
		SelectionState->SetSelectedControlPoint(INDEX_NONE);
		SelectionState->SetSelectedControlPointType(FZoneShapeControlPointType::None);
	}
}

bool FZoneShapeComponentVisualizer::CanSelectAllPoints() const
{
	UZoneShapeComponent* ShapeComp = GetEditedShapeComponent();
	return (ShapeComp != nullptr);
}

TSharedPtr<SWidget> FZoneShapeComponentVisualizer::GenerateContextMenu() const
{
	check(SelectionState);

	FMenuBuilder MenuBuilder(true, ShapeComponentVisualizerActions);

	MenuBuilder.BeginSection("ShapePointEdit", LOCTEXT("ShapePoint", "Shape Point"));
	{
		if (SelectionState->GetSelectedSegmentIndex() != INDEX_NONE)
		{
			MenuBuilder.AddMenuEntry(FZoneShapeComponentVisualizerCommands::Get().AddPoint);
		}
		else if (SelectionState->GetLastPointIndexSelected() != INDEX_NONE)
		{
			MenuBuilder.AddMenuEntry(FZoneShapeComponentVisualizerCommands::Get().DeletePoint);
			MenuBuilder.AddMenuEntry(FZoneShapeComponentVisualizerCommands::Get().DuplicatePoint);
			MenuBuilder.AddMenuEntry(FZoneShapeComponentVisualizerCommands::Get().SelectAll);

			MenuBuilder.AddSubMenu(
				LOCTEXT("ShapePointType", "Point Type"),
				LOCTEXT("ShapePointTypeTooltip", "Define the type of the point."),
				FNewMenuDelegate::CreateSP(this, &FZoneShapeComponentVisualizer::GenerateShapePointTypeSubMenu));
		}
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("Transform");
	{
		MenuBuilder.AddMenuEntry(FZoneShapeComponentVisualizerCommands::Get().FocusViewportToSelection);
	}
	MenuBuilder.EndSection();

	TSharedPtr<SWidget> MenuWidget = MenuBuilder.MakeWidget();
	return MenuWidget;
}

void FZoneShapeComponentVisualizer::GenerateShapePointTypeSubMenu(FMenuBuilder& MenuBuilder) const
{
	UZoneShapeComponent* ShapeComp = GetEditedShapeComponent();

	MenuBuilder.AddMenuEntry(FZoneShapeComponentVisualizerCommands::Get().SetPointToSharp);
	MenuBuilder.AddMenuEntry(FZoneShapeComponentVisualizerCommands::Get().SetPointToBezier);
	MenuBuilder.AddMenuEntry(FZoneShapeComponentVisualizerCommands::Get().SetPointToAutoBezier);
	if (ShapeComp && ShapeComp->GetShapeType() == FZoneShapeType::Polygon)
	{
		MenuBuilder.AddMenuEntry(FZoneShapeComponentVisualizerCommands::Get().SetPointToLaneSegment);
	}
}

#undef LOCTEXT_NAMESPACE
