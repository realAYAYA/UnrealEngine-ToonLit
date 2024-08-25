// Copyright Epic Games, Inc. All Rights Reserved.

#include "SplineComponentVisualizer.h"
#include "CoreMinimal.h"
#include "Algo/AnyOf.h"
#include "Components/BrushComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/HitResult.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/Commands.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Volume.h"
#include "Styling/AppStyle.h"
#include "UnrealWidgetFwd.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "EditorViewportCommands.h"
#include "LevelEditorActions.h"
#include "Components/SplineComponent.h"
#include "SceneView.h"
#include "ScopedTransaction.h"
#include "ActorEditorUtils.h"
#include "UObject/UObjectIterator.h"
#include "WorldCollision.h"
#include "Widgets/Docking/SDockTab.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "SplineGeneratorPanel.h"
#include "EngineUtils.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "Math/UnrealMathUtility.h"

IMPLEMENT_HIT_PROXY(HSplineVisProxy, HComponentVisProxy);
IMPLEMENT_HIT_PROXY(HSplineKeyProxy, HSplineVisProxy);
IMPLEMENT_HIT_PROXY(HSplineSegmentProxy, HSplineVisProxy);
IMPLEMENT_HIT_PROXY(HSplineTangentHandleProxy, HSplineVisProxy);

#define LOCTEXT_NAMESPACE "SplineComponentVisualizer"
DEFINE_LOG_CATEGORY_STATIC(LogSplineComponentVisualizer, Log, All)

#define VISUALIZE_SPLINE_UPVECTORS 0

namespace SplineComponentVisualizerLocals
{
	// This is mostly modeled on FindNearestVisibleObjectHit_Internal in ModelingSceneSnappingManager.cpp,
	// which we probably can't access because it lives in a plugin.
	// TODO: Perhaps this code should live in some common utility place? Certainly if we do this again.
	bool RaycastWorld(const UWorld* World, FEditorViewportClient* ViewportClient, FViewport* Viewport, FHitResult& HitResultOut)
	{
		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
			ViewportClient->Viewport,
			ViewportClient->GetScene(),
			ViewportClient->EngineShowFlags));
		// this View is deleted by the FSceneViewFamilyContext destructor
		FSceneView* View = ViewportClient->CalcSceneView(&ViewFamily);

		FViewportCursorLocation MouseViewportRay(View, ViewportClient, Viewport->GetMouseX(), Viewport->GetMouseY());
		FRay Ray(MouseViewportRay.GetOrigin(), MouseViewportRay.GetDirection());

		FCollisionObjectQueryParams ObjectQueryParams(FCollisionObjectQueryParams::AllObjects);
		FCollisionQueryParams QueryParams = FCollisionQueryParams::DefaultQueryParam;
		QueryParams.bTraceComplex = true;

		TArray<FHitResult> OutHits;
		FVector RayEnd = (FVector)(Ray.PointAt(HALF_WORLD_MAX));
		if (World->LineTraceMultiByObjectType(OutHits, (FVector)Ray.Origin, RayEnd, ObjectQueryParams, QueryParams) == false)
		{
			return false;
		}

		double NearestVisible = TNumericLimits<double>::Max();
		for (const FHitResult& CurResult : OutHits)
		{
			UPrimitiveComponent* Component = CurResult.Component.Get();
			AActor* Actor = CurResult.GetActor();

			// Don't use volumes
			if (Cast<UBrushComponent>(Component) != nullptr &&
				Cast<AVolume>(Actor) != nullptr)
			{
				continue;
			}

			// Ignore invisible things
			if ((Actor && Actor->IsHidden())
				|| (Component && !Component->IsVisibleInEditor()))
			{
				continue;
			}

			if (CurResult.Distance < NearestVisible)
			{
				HitResultOut = CurResult;
				NearestVisible = CurResult.Distance;
			}
		}

		return NearestVisible < TNumericLimits<double>::Max();
	}
}

int32 USplineComponentVisualizerSelectionState::GetVerifiedLastKeyIndexSelected(const int32 InNumSplinePoints) const
{
	check(LastKeyIndexSelected != INDEX_NONE);
	check(LastKeyIndexSelected >= 0);
	check(LastKeyIndexSelected < InNumSplinePoints);
	return LastKeyIndexSelected;
}

void USplineComponentVisualizerSelectionState::GetVerifiedSelectedTangentHandle(const int32 InNumSplinePoints, int32& OutSelectedTangentHandle, ESelectedTangentHandle& OutSelectedTangentHandleType) const
{
	check(SelectedTangentHandle != INDEX_NONE);
	check(SelectedTangentHandle >= 0);
	check(SelectedTangentHandle < InNumSplinePoints);
	check(SelectedTangentHandleType != ESelectedTangentHandle::None);
	OutSelectedTangentHandle = SelectedTangentHandle;
	OutSelectedTangentHandleType = SelectedTangentHandleType;
}
void USplineComponentVisualizerSelectionState::Reset()
{
	SplinePropertyPath = FComponentPropertyPath();
	SelectedKeys.Reset();
	LastKeyIndexSelected = INDEX_NONE;
	CachedRotation = FQuat();
	ClearSelectedSegmentIndex();
	ClearSelectedTangentHandle();
}

void USplineComponentVisualizerSelectionState::ClearSelectedSegmentIndex()
{
	SelectedSegmentIndex = INDEX_NONE;
}

void USplineComponentVisualizerSelectionState::ClearSelectedTangentHandle()
{
	SelectedTangentHandle = INDEX_NONE;
	SelectedTangentHandleType = ESelectedTangentHandle::None;
}

bool USplineComponentVisualizerSelectionState::IsSplinePointSelected(const int32 InIndex) const
{
	return SelectedKeys.Contains(InIndex);
}

/** Define commands for the spline component visualizer */
class FSplineComponentVisualizerCommands : public TCommands<FSplineComponentVisualizerCommands>
{
public:
	FSplineComponentVisualizerCommands() : TCommands <FSplineComponentVisualizerCommands>
	(
		"SplineComponentVisualizer",	// Context name for fast lookup
		LOCTEXT("SplineComponentVisualizer", "Spline Component Visualizer"),	// Localized context name for displaying
		NAME_None,	// Parent
		FAppStyle::GetAppStyleSetName()
	)
	{
	}

	virtual void RegisterCommands() override
	{
		UI_COMMAND(DeleteKey, "Delete Spline Point", "Delete the currently selected spline point.", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete));
		UI_COMMAND(DuplicateKey, "Duplicate Spline Point", "Duplicate the currently selected spline point.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(AddKey, "Add Spline Point Here", "Add a new spline point at the cursor location.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(SelectAll, "Select All Spline Points", "Select all spline points.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(SelectNextSplinePoint, "Select Next Spline Point", "Select next spline point.", EUserInterfaceActionType::Button, FInputChord(EKeys::Period));
		UI_COMMAND(SelectPrevSplinePoint, "Select Prev Spline Point", "Select prev spline point.", EUserInterfaceActionType::Button, FInputChord(EKeys::Comma));
		UI_COMMAND(AddNextSplinePoint, "Add Next Spline Point", "Add next spline point.", EUserInterfaceActionType::Button, FInputChord(EKeys::Period, EModifierKey::Shift));
		UI_COMMAND(AddPrevSplinePoint, "Add Prev Spline Point", "Add prev spline point.", EUserInterfaceActionType::Button, FInputChord(EKeys::Comma, EModifierKey::Shift));
		UI_COMMAND(ResetToUnclampedTangent, "Unclamped Tangent", "Reset the tangent for this spline point to its default unclamped value.", EUserInterfaceActionType::Button, FInputChord(EKeys::T));
		UI_COMMAND(ResetToClampedTangent, "Clamped Tangent", "Reset the tangent for this spline point to its default clamped value.", EUserInterfaceActionType::Button, FInputChord(EKeys::T, EModifierKey::Shift));
		UI_COMMAND(SetKeyToCurve, "Curve", "Set spline point to Curve type", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(SetKeyToLinear, "Linear", "Set spline point to Linear type", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(SetKeyToConstant, "Constant", "Set spline point to Constant type", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(FocusViewportToSelection, "Focus Selected", "Moves the camera in front of the selection", EUserInterfaceActionType::Button, FInputChord(EKeys::F));
		UI_COMMAND(SnapKeyToNearestSplinePoint, "Snap to Nearest Spline Point", "Snap selected spline point to nearest non-adjacent spline point on current or nearby spline.", EUserInterfaceActionType::Button, FInputChord(EKeys::P, EModifierKey::Shift));
		UI_COMMAND(AlignKeyToNearestSplinePoint, "Align to Nearest Spline Point", "Align selected spline point to nearest non-adjacent spline point on current or nearby spline.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(AlignKeyPerpendicularToNearestSplinePoint, "Align Perpendicular to Nearest Spline Point", "Align perpendicular selected spline point to nearest non-adjacent spline point on current or nearby spline.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(SnapKeyToActor, "Snap to Actor", "Snap selected spline point to actor, Ctrl-LMB to select the actor after choosing this option.", EUserInterfaceActionType::Button, FInputChord(EKeys::P, (EModifierKey::Alt | EModifierKey::Shift)));
		UI_COMMAND(AlignKeyToActor, "Align to Actor", "Align selected spline point to actor, Ctrl-LMB to select the actor after choosing this option.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(AlignKeyPerpendicularToActor, "Align Perpendicular to Actor", "Align perpendicular  selected spline point to actor, Ctrl-LMB to select the actor after choosing this option.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(ToggleSnapTangentAdjustments, "Allow Tangents Updates On Snap", "Allow tangents to update when performing snap operations on points.", EUserInterfaceActionType::ToggleButton, FInputChord());
		UI_COMMAND(SnapAllToSelectedX, "Snap All To Selected X", "Snap all spline points to selected spline point world X position.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(SnapAllToSelectedY, "Snap All To Selected Y", "Snap all spline points to selected spline point world Y position.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(SnapAllToSelectedZ, "Snap All To Selected Z", "Snap all spline points to selected spline point world Z position.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(SnapToLastSelectedX, "Snap To Last Selected X", "Snap selected spline points to world X position of last selected spline point.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(SnapToLastSelectedY, "Snap To Last Selected Y", "Snap selected spline points to world Y position of last selected spline point.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(SnapToLastSelectedZ, "Snap To Last Selected Z", "Snap selected spline points to world Z position of last selected spline point.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(StraightenToNext, "Straighten To Next Point", "Straighten selected points toward next sequential point", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(StraightenToPrevious, "Straighten To Previous Point", "Straighten selected points toward previous sequential point", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(SetLockedAxisNone, "None", "New spline point axis is not fixed.", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(SetLockedAxisX, "X", "Fix X axis when adding new spline points.", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(SetLockedAxisY, "Y", "Fix Y axis when adding new spline points.", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(SetLockedAxisZ, "Z", "Fix Z axis when adding new spline points.", EUserInterfaceActionType::RadioButton, FInputChord());
		UI_COMMAND(VisualizeRollAndScale, "Visualize Roll and Scale", "Whether the visualization should show roll and scale on this spline.", EUserInterfaceActionType::ToggleButton, FInputChord());
		UI_COMMAND(DiscontinuousSpline, "Allow Discontinuous Splines", "Whether the visualization allows Arrive and Leave tangents to be set separately.", EUserInterfaceActionType::ToggleButton, FInputChord());
		UI_COMMAND(ToggleClosedLoop, "Closed Loop", "Toggle the Closed Loop setting of the spline", EUserInterfaceActionType::ToggleButton, FInputChord());
		UI_COMMAND(ResetToDefault, "Reset to Default", "Reset this spline to its archetype default.", EUserInterfaceActionType::Button, FInputChord());
	}

public:
	/** Delete key */
	TSharedPtr<FUICommandInfo> DeleteKey;

	/** Duplicate key */
	TSharedPtr<FUICommandInfo> DuplicateKey;

	/** Add key */
	TSharedPtr<FUICommandInfo> AddKey;

	/** Select all */
	TSharedPtr<FUICommandInfo> SelectAll;

	/** Select next spline point */
	TSharedPtr<FUICommandInfo> SelectNextSplinePoint;

	/** Select prev spline point */
	TSharedPtr<FUICommandInfo> SelectPrevSplinePoint;

	/** Add next spline point */
	TSharedPtr<FUICommandInfo> AddNextSplinePoint;

	/** Add prev spline point */
	TSharedPtr<FUICommandInfo> AddPrevSplinePoint;

	/** Reset to unclamped tangent */
	TSharedPtr<FUICommandInfo> ResetToUnclampedTangent;

	/** Reset to clamped tangent */
	TSharedPtr<FUICommandInfo> ResetToClampedTangent;

	/** Set spline key to Curve type */
	TSharedPtr<FUICommandInfo> SetKeyToCurve;

	/** Set spline key to Linear type */
	TSharedPtr<FUICommandInfo> SetKeyToLinear;

	/** Set spline key to Constant type */
	TSharedPtr<FUICommandInfo> SetKeyToConstant;

	/** Focus on selection */
	TSharedPtr<FUICommandInfo> FocusViewportToSelection;

	/** Snap key to nearest spline point on another spline component */
	TSharedPtr<FUICommandInfo> SnapKeyToNearestSplinePoint;

	/** Align key to nearest spline point on another spline component */
	TSharedPtr<FUICommandInfo> AlignKeyToNearestSplinePoint;

	/** Align key perpendicular to nearest spline point on another spline component */
	TSharedPtr<FUICommandInfo> AlignKeyPerpendicularToNearestSplinePoint;

	/** Snap key to nearest actor */
	TSharedPtr<FUICommandInfo> SnapKeyToActor;

	/** Align key to nearest actor */
	TSharedPtr<FUICommandInfo> AlignKeyToActor;

	/** Align key perpendicular to nearest actor */
	TSharedPtr<FUICommandInfo> AlignKeyPerpendicularToActor;

	/** Turn On / Off Tangent updates when snapping points*/
	TSharedPtr<FUICommandInfo> ToggleSnapTangentAdjustments;

	/** Snap all spline points to selected point world X position*/
	TSharedPtr<FUICommandInfo> SnapAllToSelectedX;

	/** Snap all spline points to selected point world Y position */
	TSharedPtr<FUICommandInfo> SnapAllToSelectedY;

	/** Snap all spline points to selected point world Z position */
	TSharedPtr<FUICommandInfo> SnapAllToSelectedZ;

	/** Snap selected spline points to last selected point world X position */
	TSharedPtr<FUICommandInfo> SnapToLastSelectedX;

	/** Snap selected spline points to last selected point world Y position */
	TSharedPtr<FUICommandInfo> SnapToLastSelectedY;

	/** Snap selected spline points to last selected point world Z position */
	TSharedPtr<FUICommandInfo> SnapToLastSelectedZ;

	/** Straighten tangents to align directly toward Next spline points */
	TSharedPtr<FUICommandInfo> StraightenToNext;

	/** Straighten tangents to align directly toward Previous spline points */
	TSharedPtr<FUICommandInfo> StraightenToPrevious;

	/** No axis is locked when adding new spline points */
	TSharedPtr<FUICommandInfo> SetLockedAxisNone;

	/** Lock X axis when adding new spline points */
	TSharedPtr<FUICommandInfo> SetLockedAxisX;

	/** Lock Y axis when adding new spline points */
	TSharedPtr<FUICommandInfo> SetLockedAxisY;

	/** Lock Z axis when adding new spline points */
	TSharedPtr<FUICommandInfo> SetLockedAxisZ;

	/** Whether the visualization should show roll and scale */
	TSharedPtr<FUICommandInfo> VisualizeRollAndScale;

	/** Whether we allow separate Arrive / Leave tangents, resulting in a discontinuous spline */
	TSharedPtr<FUICommandInfo> DiscontinuousSpline;

	/** Toggle the Closed Loop setting of the spline */
	TSharedPtr<FUICommandInfo> ToggleClosedLoop;

	/** Reset this spline to its default */
	TSharedPtr<FUICommandInfo> ResetToDefault;
};

TWeakPtr<SWindow> FSplineComponentVisualizer::WeakExistingWindow;

FSplineComponentVisualizer::FSplineComponentVisualizer()
	: FComponentVisualizer()
	, bAllowDuplication(true)
	, bDuplicatingSplineKey(false)
	, bUpdatingAddSegment(false)
	, DuplicateDelay(0)
	, DuplicateDelayAccumulatedDrag(FVector::ZeroVector)
	, DuplicateCacheSplitSegmentParam(0.0f)
	, AddKeyLockedAxis(EAxis::None)
	, bIsSnappingToActor(false)
	, SnapToActorMode(ESplineComponentSnapMode::Snap)
{
	FSplineComponentVisualizerCommands::Register();

	SplineComponentVisualizerActions = MakeShareable(new FUICommandList);

	SplineCurvesProperty = FindFProperty<FProperty>(USplineComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(USplineComponent, SplineCurves));

	SelectionState = NewObject<USplineComponentVisualizerSelectionState>(GetTransientPackage(), TEXT("SelectionState"), RF_Transactional);
}

void FSplineComponentVisualizer::OnRegister()
{
	const auto& Commands = FSplineComponentVisualizerCommands::Get();

	SplineComponentVisualizerActions->MapAction(
		Commands.DeleteKey,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnDeleteKey),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::CanDeleteKey));

	SplineComponentVisualizerActions->MapAction(
		Commands.DuplicateKey,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnDuplicateKey),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::IsKeySelectionValid));

	SplineComponentVisualizerActions->MapAction(
		Commands.AddKey,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnAddKeyToSegment),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::CanAddKeyToSegment));

	SplineComponentVisualizerActions->MapAction(
		Commands.SelectAll,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSelectAllSplinePoints),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::CanSelectSplinePoints));

	SplineComponentVisualizerActions->MapAction(
		Commands.SelectNextSplinePoint,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSelectPrevNextSplinePoint, true, false),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::CanSelectSplinePoints));

	SplineComponentVisualizerActions->MapAction(
		Commands.SelectPrevSplinePoint,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSelectPrevNextSplinePoint, false, false),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::CanSelectSplinePoints));

	SplineComponentVisualizerActions->MapAction(
		Commands.AddNextSplinePoint,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSelectPrevNextSplinePoint, true, true),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::CanSelectSplinePoints));

	SplineComponentVisualizerActions->MapAction(
		Commands.AddPrevSplinePoint,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSelectPrevNextSplinePoint, false, true),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::CanSelectSplinePoints));

	SplineComponentVisualizerActions->MapAction(
		Commands.ResetToUnclampedTangent,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnResetToAutomaticTangent, CIM_CurveAuto),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::CanResetToAutomaticTangent, CIM_CurveAuto));

	SplineComponentVisualizerActions->MapAction(
		Commands.ResetToClampedTangent,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnResetToAutomaticTangent, CIM_CurveAutoClamped),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::CanResetToAutomaticTangent, CIM_CurveAutoClamped));

	SplineComponentVisualizerActions->MapAction(
		Commands.SetKeyToCurve,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSetKeyType, CIM_CurveAuto),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FSplineComponentVisualizer::IsKeyTypeSet, CIM_CurveAuto));

	SplineComponentVisualizerActions->MapAction(
		Commands.SetKeyToLinear,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSetKeyType, CIM_Linear),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FSplineComponentVisualizer::IsKeyTypeSet, CIM_Linear));

	SplineComponentVisualizerActions->MapAction(
		Commands.SetKeyToConstant,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSetKeyType, CIM_Constant),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FSplineComponentVisualizer::IsKeyTypeSet, CIM_Constant));

	SplineComponentVisualizerActions->MapAction(
		Commands.FocusViewportToSelection,
		FExecuteAction::CreateStatic( &FLevelEditorActionCallbacks::ExecuteExecCommand, FString( TEXT("CAMERA ALIGN ACTIVEVIEWPORTONLY") ) )
	);

	SplineComponentVisualizerActions->MapAction(
		Commands.SnapKeyToNearestSplinePoint,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSnapKeyToNearestSplinePoint, ESplineComponentSnapMode::Snap),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::IsSingleKeySelected));

	SplineComponentVisualizerActions->MapAction(
		Commands.AlignKeyToNearestSplinePoint,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSnapKeyToNearestSplinePoint, ESplineComponentSnapMode::AlignToTangent),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::IsSingleKeySelected));

	SplineComponentVisualizerActions->MapAction(
		Commands.AlignKeyPerpendicularToNearestSplinePoint,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSnapKeyToNearestSplinePoint, ESplineComponentSnapMode::AlignPerpendicularToTangent),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::IsSingleKeySelected));

	SplineComponentVisualizerActions->MapAction(
		Commands.SnapKeyToActor,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSnapKeyToActor, ESplineComponentSnapMode::Snap),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::IsSingleKeySelected));

	SplineComponentVisualizerActions->MapAction(
		Commands.AlignKeyToActor,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSnapKeyToActor, ESplineComponentSnapMode::AlignToTangent),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::IsSingleKeySelected));

	SplineComponentVisualizerActions->MapAction(
		Commands.AlignKeyPerpendicularToActor,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSnapKeyToActor, ESplineComponentSnapMode::AlignPerpendicularToTangent),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::IsSingleKeySelected));
	
	SplineComponentVisualizerActions->MapAction(
		Commands.ToggleSnapTangentAdjustments,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnToggleSnapTangentAdjustment),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FSplineComponentVisualizer::IsSnapTangentAdjustment));

	SplineComponentVisualizerActions->MapAction(
		Commands.SnapAllToSelectedX,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSnapAllToAxis, EAxis::X),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::IsSingleKeySelected));

	SplineComponentVisualizerActions->MapAction(
		Commands.SnapAllToSelectedY,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSnapAllToAxis, EAxis::Y),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::IsSingleKeySelected));

	SplineComponentVisualizerActions->MapAction(
		Commands.SnapAllToSelectedZ,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSnapAllToAxis, EAxis::Z),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::IsSingleKeySelected));

	SplineComponentVisualizerActions->MapAction(
		Commands.SnapToLastSelectedX,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSnapSelectedToAxis, EAxis::X),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::AreMultipleKeysSelected));

	SplineComponentVisualizerActions->MapAction(
		Commands.SnapToLastSelectedY,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSnapSelectedToAxis, EAxis::Y),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::AreMultipleKeysSelected));

	SplineComponentVisualizerActions->MapAction(
		Commands.SnapToLastSelectedZ,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSnapSelectedToAxis, EAxis::Z),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::AreMultipleKeysSelected));

	SplineComponentVisualizerActions->MapAction(
		Commands.StraightenToNext,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnStraightenKey, 1),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::IsKeySelectionValid));

	SplineComponentVisualizerActions->MapAction(
		Commands.StraightenToPrevious,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnStraightenKey, -1),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::IsKeySelectionValid));

	SplineComponentVisualizerActions->MapAction(
		Commands.SetLockedAxisNone,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnLockAxis, EAxis::None),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FSplineComponentVisualizer::IsLockAxisSet, EAxis::None));

	SplineComponentVisualizerActions->MapAction(
		Commands.SetLockedAxisX,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnLockAxis, EAxis::X),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FSplineComponentVisualizer::IsLockAxisSet, EAxis::X));

	SplineComponentVisualizerActions->MapAction(
		Commands.SetLockedAxisY,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnLockAxis, EAxis::Y),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FSplineComponentVisualizer::IsLockAxisSet, EAxis::Y));

	SplineComponentVisualizerActions->MapAction(
		Commands.SetLockedAxisZ,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnLockAxis, EAxis::Z),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FSplineComponentVisualizer::IsLockAxisSet, EAxis::Z));

	SplineComponentVisualizerActions->MapAction(
		Commands.VisualizeRollAndScale,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSetVisualizeRollAndScale),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FSplineComponentVisualizer::IsVisualizingRollAndScale));

	SplineComponentVisualizerActions->MapAction(
		Commands.DiscontinuousSpline,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnSetDiscontinuousSpline),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FSplineComponentVisualizer::IsDiscontinuousSpline));

	SplineComponentVisualizerActions->MapAction(
		Commands.ToggleClosedLoop,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnToggleClosedLoop),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FSplineComponentVisualizer::IsClosedLoop));

	SplineComponentVisualizerActions->MapAction(
		Commands.ResetToDefault,
		FExecuteAction::CreateSP(this, &FSplineComponentVisualizer::OnResetToDefault),
		FCanExecuteAction::CreateSP(this, &FSplineComponentVisualizer::CanResetToDefault));

	bool bAlign = false;
	bool bUseLineTrace = false;
	bool bUseBounds = false;
	bool bUsePivot = false;
	SplineComponentVisualizerActions->MapAction(
		FLevelEditorCommands::Get().SnapToFloor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapToFloor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);

	bAlign = true;
	bUseLineTrace = false;
	bUseBounds = false;
	bUsePivot = false;
	SplineComponentVisualizerActions->MapAction(
		FLevelEditorCommands::Get().AlignToFloor,
		FExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::SnapToFloor_Clicked, bAlign, bUseLineTrace, bUseBounds, bUsePivot),
		FCanExecuteAction::CreateStatic(&FLevelEditorActionCallbacks::ActorSelected_CanExecute)
		);
}

bool FSplineComponentVisualizer::ShouldShowForSelectedSubcomponents(const UActorComponent* Component)
{
	if (const USplineComponent* SplineComp = Cast<const USplineComponent>(Component))
	{
		return SplineComp->bDrawDebug;
	}

	return false;
}

FSplineComponentVisualizer::~FSplineComponentVisualizer()
{
	FSplineComponentVisualizerCommands::Unregister();
}

void FSplineComponentVisualizer::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (SelectionState)
	{
		Collector.AddReferencedObject(SelectionState);
	}
}

static double GetDashSize(const FSceneView* View, const FVector& Start, const FVector& End, float Scale)
{
	const double StartW = View->WorldToScreen(Start).W;
	const double EndW = View->WorldToScreen(End).W;

	const double WLimit = 10.0f;
	if (StartW > WLimit || EndW > WLimit)
	{
		return FMath::Max(StartW, EndW) * Scale;
	}

	return 0.0f;
}

void FSplineComponentVisualizer::DrawVisualization(const UActorComponent* Component, const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	if (const USplineComponent* SplineComp = Cast<const USplineComponent>(Component))
	{
		const FInterpCurveVector& SplineInfo = SplineComp->GetSplinePointsPosition();
		const USplineComponent* EditedSplineComp = GetEditedSplineComponent();

		const USplineComponent* Archetype = CastChecked<USplineComponent>(SplineComp->GetArchetype());
		const bool bIsSplineEditable = !SplineComp->bModifiedByConstructionScript; // bSplineHasBeenEdited || SplineInfo == Archetype->SplineCurves.Position || SplineComp->bInputSplinePointsToConstructionScript;

		const FColor ReadOnlyColor = FColor(255, 0, 255, 255);
		const FColor NormalColor = bIsSplineEditable ? FColor(SplineComp->EditorUnselectedSplineSegmentColor.ToFColor(true)) : ReadOnlyColor;
		const FColor SelectedColor = bIsSplineEditable ? FColor(SplineComp->EditorSelectedSplineSegmentColor.ToFColor(true)) : ReadOnlyColor;
		const FColor TangentColor = bIsSplineEditable ? FColor(SplineComp->EditorTangentColor.ToFColor(true)) : ReadOnlyColor;
		const float GrabHandleSize = 10.0f + (bIsSplineEditable ? GetDefault<ULevelEditorViewportSettings>()->SelectedSplinePointSizeAdjustment : 0.0f);

		// Draw the tangent handles before anything else so they will not overdraw the rest of the spline
		if (SplineComp == EditedSplineComp)
		{
			check(SelectionState);

			if (SplineComp->GetNumberOfSplinePoints() == 0 && SelectionState->GetSelectedKeys().Num() > 0)
			{
				ChangeSelectionState(INDEX_NONE, false);
			}
			else
			{
				const TSet<int32> SelectedKeysCopy = SelectionState->GetSelectedKeys();
				for (int32 SelectedKey : SelectedKeysCopy)
				{
					check(SelectedKey >= 0);
					if (SelectedKey >= SplineComp->GetNumberOfSplinePoints())
					{
						// Catch any keys that might not exist anymore due to the underlying component changing.
						ChangeSelectionState(SelectedKey, true);
						continue;
					}

					if (SplineInfo.Points[SelectedKey].IsCurveKey())
					{
						const float TangentHandleSize = 8.0f + (bIsSplineEditable ? GetDefault<ULevelEditorViewportSettings>()->SplineTangentHandleSizeAdjustment : 0.0f);
						const float TangentScale = GetDefault<ULevelEditorViewportSettings>()->SplineTangentScale;

						const FVector Location = SplineComp->GetLocationAtSplinePoint(SelectedKey, ESplineCoordinateSpace::World);
						const FVector LeaveTangent = SplineComp->GetLeaveTangentAtSplinePoint(SelectedKey, ESplineCoordinateSpace::World) * TangentScale;
						const FVector ArriveTangent = SplineComp->bAllowDiscontinuousSpline ?
							SplineComp->GetArriveTangentAtSplinePoint(SelectedKey, ESplineCoordinateSpace::World) * TangentScale : LeaveTangent;

						PDI->SetHitProxy(NULL);

						// determine tangent coloration
						const bool bTangentSelected = (SelectedKey == SelectionState->GetSelectedTangentHandle());
						const ESelectedTangentHandle SelectedTangentHandleType = SelectionState->GetSelectedTangentHandleType();
						const bool bArriveSelected = bTangentSelected && (SelectedTangentHandleType == ESelectedTangentHandle::Arrive);
						const bool bLeaveSelected = bTangentSelected && (SelectedTangentHandleType == ESelectedTangentHandle::Leave);
						FColor ArriveColor = bArriveSelected ? SelectedColor : TangentColor;
						FColor LeaveColor = bLeaveSelected ? SelectedColor : TangentColor;
						
						PDI->DrawLine(Location, Location - ArriveTangent, ArriveColor, SDPG_Foreground);
						PDI->DrawLine(Location, Location + LeaveTangent, LeaveColor, SDPG_Foreground);

						if (bIsSplineEditable)
						{
							PDI->SetHitProxy(new HSplineTangentHandleProxy(Component, SelectedKey, false));
						}
						PDI->DrawPoint(Location + LeaveTangent, LeaveColor, TangentHandleSize, SDPG_Foreground);

						if (bIsSplineEditable)
						{
							PDI->SetHitProxy(new HSplineTangentHandleProxy(Component, SelectedKey, true));
						}
						PDI->DrawPoint(Location - ArriveTangent, ArriveColor, TangentHandleSize, SDPG_Foreground);

						PDI->SetHitProxy(NULL);
					}
				}
			}
		}

		const bool bShouldVisualizeScale = SplineComp->bShouldVisualizeScale;
		const float DefaultScale = SplineComp->ScaleVisualizationWidth;

		FVector OldKeyPos(0);
		FVector OldKeyRightVector(0);
		FVector OldKeyScale(0);

		const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();

		const int32 NumPoints = SplineInfo.Points.Num();
		const int32 NumSegments = SplineInfo.bIsLooped ? NumPoints : NumPoints - 1;
		for (int32 KeyIdx = 0; KeyIdx < NumSegments + 1; KeyIdx++)
		{
			const FVector NewKeyPos = SplineComp->GetLocationAtSplinePoint(KeyIdx, ESplineCoordinateSpace::World);
			const FVector NewKeyRightVector = SplineComp->GetRightVectorAtSplinePoint(KeyIdx, ESplineCoordinateSpace::World);
			const FVector NewKeyUpVector = SplineComp->GetUpVectorAtSplinePoint(KeyIdx, ESplineCoordinateSpace::World);
			const FVector NewKeyScale = SplineComp->GetScaleAtSplinePoint(KeyIdx) * DefaultScale;

			const FColor KeyColor = (SplineComp == EditedSplineComp && SelectedKeys.Contains(KeyIdx)) ? SelectedColor : NormalColor;

			// Draw the keypoint and up/right vectors
			if (KeyIdx < NumPoints)
			{
				if (bShouldVisualizeScale)
				{
					PDI->SetHitProxy(NULL);

					PDI->DrawLine(NewKeyPos, NewKeyPos - NewKeyRightVector * NewKeyScale.Y, KeyColor, SDPG_Foreground);
					PDI->DrawLine(NewKeyPos, NewKeyPos + NewKeyRightVector * NewKeyScale.Y, KeyColor, SDPG_Foreground);
					PDI->DrawLine(NewKeyPos, NewKeyPos + NewKeyUpVector * NewKeyScale.Z, KeyColor, SDPG_Foreground);

					const int32 ArcPoints = 20;
					FVector OldArcPos = NewKeyPos + NewKeyRightVector * NewKeyScale.Y;
					for (int32 ArcIndex = 1; ArcIndex <= ArcPoints; ArcIndex++)
					{
						float Sin;
						float Cos;
						FMath::SinCos(&Sin, &Cos, ArcIndex * PI / ArcPoints);
						const FVector NewArcPos = NewKeyPos + Cos * NewKeyRightVector * NewKeyScale.Y + Sin * NewKeyUpVector * NewKeyScale.Z;
						PDI->DrawLine(OldArcPos, NewArcPos, KeyColor, SDPG_Foreground);
						OldArcPos = NewArcPos;
					}
				}

				if (bIsSplineEditable)
				{
					PDI->SetHitProxy(new HSplineKeyProxy(Component, KeyIdx));
				}
				PDI->DrawPoint(NewKeyPos, KeyColor, GrabHandleSize, SDPG_Foreground);
				PDI->SetHitProxy(NULL);
			}

			// If not the first keypoint, draw a line to the previous keypoint.
			if (KeyIdx > 0)
			{
				const FColor LineColor = NormalColor;
				if (bIsSplineEditable)
				{
					PDI->SetHitProxy(new HSplineSegmentProxy(Component, KeyIdx - 1));
				}

				// For constant interpolation - don't draw ticks - just draw dotted line.
				if (SplineInfo.Points[KeyIdx - 1].InterpMode == CIM_Constant)
				{
					const double DashSize = GetDashSize(View, OldKeyPos, NewKeyPos, 0.03f);
					if (DashSize > 0.0f)
					{
						DrawDashedLine(PDI, OldKeyPos, NewKeyPos, LineColor, DashSize, SDPG_World);
					}
				}
				else
				{
					// Determine the colors to use
					const bool bIsEdited = (SplineComp == EditedSplineComp);
					const bool bKeyIdxLooped = (SplineInfo.bIsLooped && KeyIdx == NumPoints);
					const int32 BeginIdx = bKeyIdxLooped ? 0 : KeyIdx;
					const int32 EndIdx = KeyIdx - 1;
					const bool bBeginSelected = SelectedKeys.Contains(BeginIdx);
					const bool bEndSelected = SelectedKeys.Contains(EndIdx);
					const FColor BeginColor = (bIsEdited && bBeginSelected) ? SelectedColor : NormalColor;
					const FColor EndColor = (bIsEdited && bEndSelected) ? SelectedColor : NormalColor;
					
					// Find position on first keyframe.
					FVector OldPos = OldKeyPos;
					FVector OldRightVector = OldKeyRightVector;
					FVector OldScale = OldKeyScale;

					// Then draw a line for each substep.
					constexpr int32 NumSteps = 20;
					constexpr float PartialGradientProportion = 0.75f;
					constexpr int32 PartialNumSteps = (int32)(NumSteps * PartialGradientProportion);
					const float SegmentLineThickness = GetDefault<ULevelEditorViewportSettings>()->SplineLineThicknessAdjustment;

					for (int32 StepIdx = 1; StepIdx <= NumSteps; StepIdx++)
					{
						const float StepRatio = StepIdx / static_cast<float>(NumSteps);
						const float Key = EndIdx + StepRatio;
						const FVector NewPos = SplineComp->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);
						const FVector NewRightVector = SplineComp->GetRightVectorAtSplineInputKey(Key, ESplineCoordinateSpace::World);
						const FVector NewScale = SplineComp->GetScaleAtSplineInputKey(Key) * DefaultScale;

						// creates a gradient that starts partway through the selection
						FColor StepColor;
						if (bBeginSelected == bEndSelected)
						{
							StepColor = BeginColor;
						}
						else if (bBeginSelected && StepIdx > (NumSteps - PartialNumSteps))
						{
							const float LerpRatio = (1.0f - StepRatio) / PartialGradientProportion;
							StepColor = FMath::Lerp(BeginColor.ReinterpretAsLinear(), EndColor.ReinterpretAsLinear(), LerpRatio).ToFColor(false);
						}
						else if (bEndSelected && StepIdx <= PartialNumSteps)
						{
							const float LerpRatio = 1.0f - (StepRatio / PartialGradientProportion);
							StepColor = FMath::Lerp(BeginColor.ReinterpretAsLinear(), EndColor.ReinterpretAsLinear(), LerpRatio).ToFColor(false);
						}
						else
						{
							StepColor = NormalColor; // unselected
						}
						
						PDI->DrawLine(OldPos, NewPos, StepColor, SDPG_Foreground, SegmentLineThickness);
						if (bShouldVisualizeScale)
						{
							PDI->DrawLine(OldPos - OldRightVector * OldScale.Y, NewPos - NewRightVector * NewScale.Y, LineColor, SDPG_Foreground);
							PDI->DrawLine(OldPos + OldRightVector * OldScale.Y, NewPos + NewRightVector * NewScale.Y, LineColor, SDPG_Foreground);

							#if VISUALIZE_SPLINE_UPVECTORS
							const FVector NewUpVector = SplineComp->GetUpVectorAtSplineInputKey(Key, ESplineCoordinateSpace::World);
							PDI->DrawLine(NewPos, NewPos + NewUpVector * SplineComp->ScaleVisualizationWidth * 0.5f, LineColor, SDPG_Foreground);
							PDI->DrawLine(NewPos, NewPos + NewRightVector * SplineComp->ScaleVisualizationWidth * 0.5f, LineColor, SDPG_Foreground);
							#endif
						}

						OldPos = NewPos;
						OldRightVector = NewRightVector;
						OldScale = NewScale;
					}
				}

				PDI->SetHitProxy(NULL);
			}

			OldKeyPos = NewKeyPos;
			OldKeyRightVector = NewKeyRightVector;
			OldKeyScale = NewKeyScale;
		}
	}
}

void FSplineComponentVisualizer::DrawVisualizationHUD(const UActorComponent* Component, const FViewport* Viewport, const FSceneView* View, FCanvas* Canvas) 
{
	if (const USplineComponent* SplineComp = Cast<const USplineComponent>(Component))
	{
		const bool bIsSplineEditable = !SplineComp->bModifiedByConstructionScript; // bSplineHasBeenEdited || SplineInfo == Archetype->SplineCurves.Position || SplineComp->bInputSplinePointsToConstructionScript;
		const USplineComponent* EditedSplineComp = GetEditedSplineComponent();

		if (SplineComp == EditedSplineComp)
		{
			if (bIsSnappingToActor)
			{
				const FIntRect CanvasRect = Canvas->GetViewRect();

				static const FText SnapToActorHelp = LOCTEXT("SplinePointSnapToActorMessage", "Snap to Actor: Use Ctrl-LMB to select actor to use as target.");
				static const FText AlignToActorHelp = LOCTEXT("SplinePointAlignToActorMessage", "Snap Align to Actor: Use Ctrl-LMB to select actor to use as target.");
				static const FText AlignPerpToActorHelp = LOCTEXT("SplinePointAlignPerpToActorMessage", "Snap Align Perpendicular to Actor: Use Ctrl-LMB to select actor to use as target.");

				auto DisplaySnapToActorHelpText = [&](const FText& SnapHelpText)
				{
					int32 XL;
					int32 YL;
					StringSize(GEngine->GetLargeFont(), XL, YL, *SnapHelpText.ToString());
					const float DrawPositionX = FMath::FloorToFloat(CanvasRect.Min.X + (CanvasRect.Width() - XL) * 0.5f);
					const float DrawPositionY = CanvasRect.Min.Y + 50.0f;
					Canvas->DrawShadowedString(DrawPositionX, DrawPositionY, *SnapHelpText.ToString(), GEngine->GetLargeFont(), FLinearColor::Yellow);
				};

				if (SnapToActorMode == ESplineComponentSnapMode::Snap)
				{
					DisplaySnapToActorHelpText(SnapToActorHelp);
				}
				else if (SnapToActorMode == ESplineComponentSnapMode::AlignToTangent)
				{
					DisplaySnapToActorHelpText(AlignToActorHelp);
				}
				else
				{
					DisplaySnapToActorHelpText(AlignPerpToActorHelp);
				}
			}
		}
		else
		{
			ResetTempModes();
		}
	}
}

void FSplineComponentVisualizer::ChangeSelectionState(int32 Index, bool bIsCtrlHeld)
{
	check(SelectionState);
	SelectionState->Modify();

	TSet<int32>& SelectedKeys = SelectionState->ModifySelectedKeys();
	if (Index == INDEX_NONE)
	{
		SelectedKeys.Empty();
		SelectionState->SetLastKeyIndexSelected(INDEX_NONE);
	}
	else if (!bIsCtrlHeld)
	{
		SelectedKeys.Empty();
		SelectedKeys.Add(Index);
		SelectionState->SetLastKeyIndexSelected(Index);
	}
	else
	{
		// Add or remove from selection if Ctrl is held
		if (SelectedKeys.Contains(Index))
		{
			// If already in selection, toggle it off
			SelectedKeys.Remove(Index);

			if (SelectionState->GetLastKeyIndexSelected() == Index)
			{
				if (SelectedKeys.Num() == 0)
				{
					// Last key selected: clear last key index selected
					SelectionState->SetLastKeyIndexSelected(INDEX_NONE);
				}
				else
				{
					// Arbitarily set last key index selected to first member of the set (so that it is valid)
					SelectionState->SetLastKeyIndexSelected(*SelectedKeys.CreateConstIterator());
				}
			}
		}
		else
		{
			// Add to selection
			SelectedKeys.Add(Index);
			SelectionState->SetLastKeyIndexSelected(Index);
		}
	}

	if (SplineGeneratorPanel.IsValid())
	{
		SplineGeneratorPanel->OnSelectionUpdated();
	}

	if (Index != INDEX_NONE)
	{
		if (!DeselectedInEditorDelegateHandle.IsValid())
		{
			DeselectedInEditorDelegateHandle = GetEditedSplineComponent()->OnDeselectedInEditor.AddRaw(this, &FSplineComponentVisualizer::OnDeselectedInEditor);
		}
	}
}


const USplineComponent* FSplineComponentVisualizer::UpdateSelectedSplineComponent(HComponentVisProxy* VisProxy)
{
	check(SelectionState);

	const USplineComponent* SplineComp = CastChecked<const USplineComponent>(VisProxy->Component.Get());

	AActor* OldSplineOwningActor = SelectionState->GetSplinePropertyPath().GetParentOwningActor();
	FComponentPropertyPath NewSplinePropertyPath(SplineComp);
	SelectionState->SetSplinePropertyPath(NewSplinePropertyPath);
	AActor* NewSplineOwningActor = NewSplinePropertyPath.GetParentOwningActor();

	if (NewSplinePropertyPath.IsValid())
	{
		if (OldSplineOwningActor != NewSplineOwningActor)
		{
			// Reset selection state if we are selecting a different actor to the one previously selected
			ChangeSelectionState(INDEX_NONE, false);
			SelectionState->ClearSelectedSegmentIndex();
			SelectionState->ClearSelectedTangentHandle();
		}

		return SplineComp;
	}

	SelectionState->SetSplinePropertyPath(FComponentPropertyPath());
	return nullptr;
}

bool FSplineComponentVisualizer::VisProxyHandleClick(FEditorViewportClient* InViewportClient, HComponentVisProxy* VisProxy, const FViewportClick& Click)
{
	ResetTempModes();

	bool bVisProxyClickHandled = false;

	if(VisProxy && VisProxy->Component.IsValid())
	{
		check(SelectionState);

		if (VisProxy->IsA(HSplineKeyProxy::StaticGetType()))
		{
			// Control point clicked
			const FScopedTransaction Transaction(LOCTEXT("SelectSplinePoint", "Select Spline Point"));
				 
			SelectionState->Modify();

			ResetTempModes();

			if (const USplineComponent* SplineComp = UpdateSelectedSplineComponent(VisProxy))
			{
				HSplineKeyProxy* KeyProxy = (HSplineKeyProxy*)VisProxy;

				// Modify the selection state, unless right-clicking on an already selected key
				const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
				if (Click.GetKey() != EKeys::RightMouseButton || !SelectedKeys.Contains(KeyProxy->KeyIndex))
				{
					ChangeSelectionState(KeyProxy->KeyIndex, InViewportClient->IsCtrlPressed());
				}
				SelectionState->ClearSelectedSegmentIndex();
				SelectionState->ClearSelectedTangentHandle();

				if (SelectionState->GetLastKeyIndexSelected() == INDEX_NONE)
				{
					SelectionState->SetSplinePropertyPath(FComponentPropertyPath());
					return false;
				}

				SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(SelectionState->GetLastKeyIndexSelected(), ESplineCoordinateSpace::World));

				bVisProxyClickHandled = true;
			}
		}
		else if (VisProxy->IsA(HSplineSegmentProxy::StaticGetType()))
		{
			// Spline segment clicked
			const FScopedTransaction Transaction(LOCTEXT("SelectSplineSegment", "Select Spline Segment"));

			SelectionState->Modify();

			ResetTempModes();

			if (const USplineComponent* SplineComp = UpdateSelectedSplineComponent(VisProxy))
			{
				// Divide segment into subsegments and test each subsegment against ray representing click position and camera direction.
				// Closest encounter with the spline determines the spline position.
				const int32 NumSubdivisions = 16;

				HSplineSegmentProxy* SegmentProxy = (HSplineSegmentProxy*)VisProxy;

				// Ignore Ctrl key, segments should only be selected one at time
				ChangeSelectionState(SegmentProxy->SegmentIndex, false);
				SelectionState->SetSelectedSegmentIndex(SegmentProxy->SegmentIndex);
				SelectionState->ClearSelectedTangentHandle();

				if (SelectionState->GetLastKeyIndexSelected() == INDEX_NONE)
				{
					SelectionState->SetSplinePropertyPath(FComponentPropertyPath());
					return false;
				}

				SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(SelectionState->GetLastKeyIndexSelected(), ESplineCoordinateSpace::World));

				int32 SelectedSegmentIndex = SelectionState->GetSelectedSegmentIndex();
				float SubsegmentStartKey = static_cast<float>(SelectedSegmentIndex);
				FVector SubsegmentStart = SplineComp->GetLocationAtSplineInputKey(SubsegmentStartKey, ESplineCoordinateSpace::World);

				double ClosestDistance = TNumericLimits<double>::Max();
				FVector BestLocation = SubsegmentStart;

				for (int32 Step = 1; Step < NumSubdivisions; Step++)
				{
					const float SubsegmentEndKey = SelectedSegmentIndex + Step / static_cast<float>(NumSubdivisions);
					const FVector SubsegmentEnd = SplineComp->GetLocationAtSplineInputKey(SubsegmentEndKey, ESplineCoordinateSpace::World);

					FVector SplineClosest;
					FVector RayClosest;
					FMath::SegmentDistToSegmentSafe(SubsegmentStart, SubsegmentEnd, Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection() * 50000.0f, SplineClosest, RayClosest);

					const double Distance = FVector::DistSquared(SplineClosest, RayClosest);
					if (Distance < ClosestDistance)
					{
						ClosestDistance = Distance;
						BestLocation = SplineClosest;
					}

					SubsegmentStartKey = SubsegmentEndKey;
					SubsegmentStart = SubsegmentEnd;
				}

				SelectionState->SetSelectedSplinePosition(BestLocation);

				bVisProxyClickHandled = true;
			}
		}
		else if (VisProxy->IsA(HSplineTangentHandleProxy::StaticGetType()))
		{
			// Spline segment clicked
			const FScopedTransaction Transaction(LOCTEXT("SelectSplineSegment", "Select Spline Segment"));

			SelectionState->Modify();

			ResetTempModes();

			if (const USplineComponent* SplineComp = UpdateSelectedSplineComponent(VisProxy))
			{
				// Tangent handle clicked

				HSplineTangentHandleProxy* KeyProxy = (HSplineTangentHandleProxy*)VisProxy;

				// Note: don't change key selection when a tangent handle is clicked.
				// Ignore Ctrl-modifier, cannot select multiple tangent handles at once.
				// To do: replace the following section with new method ClearMetadataSelectionState()
				// since this is the only reason ChangeSelectionState is being called here.
				TSet<int32> SelectedKeysCopy(SelectionState->GetSelectedKeys());
				ChangeSelectionState(KeyProxy->KeyIndex, false);
				TSet<int32>& SelectedKeys = SelectionState->ModifySelectedKeys();
				for (int32 KeyIndex : SelectedKeysCopy)
				{
					if (KeyIndex != KeyProxy->KeyIndex)
					{
						SelectedKeys.Add(KeyIndex);
					}
				}

				SelectionState->ClearSelectedSegmentIndex();
				SelectionState->SetSelectedTangentHandle(KeyProxy->KeyIndex);
				SelectionState->SetSelectedTangentHandleType(KeyProxy->bArriveTangent ? ESelectedTangentHandle::Arrive : ESelectedTangentHandle::Leave);
				SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(SelectionState->GetSelectedTangentHandle(), ESplineCoordinateSpace::World));

				bVisProxyClickHandled = true;
			}
		}
	}

	if (bVisProxyClickHandled)
	{
		GEditor->RedrawLevelEditingViewports(true);
	}

	return bVisProxyClickHandled;
}

void FSplineComponentVisualizer::SetEditedSplineComponent(const USplineComponent* InSplineComponent) 
{
	check(SelectionState);
	SelectionState->Modify();
	SelectionState->Reset();

	FComponentPropertyPath SplinePropertyPath(InSplineComponent);
	SelectionState->SetSplinePropertyPath(SplinePropertyPath);
}

USplineComponent* FSplineComponentVisualizer::GetEditedSplineComponent() const
{
	check(SelectionState);
	return Cast<USplineComponent>(SelectionState->GetSplinePropertyPath().GetComponent());
}

UActorComponent* FSplineComponentVisualizer::GetEditedComponent() const
{
	return Cast<UActorComponent>(GetEditedSplineComponent());
}

bool FSplineComponentVisualizer::GetWidgetLocation(const FEditorViewportClient* ViewportClient, FVector& OutLocation) const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr)
	{
		check(SelectionState);

		const FInterpCurveVector& Position = SplineComp->GetSplinePointsPosition();
		const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
		int32 LastKeyIndexSelected = SelectionState->GetLastKeyIndexSelected();

		int32 SelectedTangentHandle = SelectionState->GetSelectedTangentHandle();
		ESelectedTangentHandle SelectedTangentHandleType = SelectionState->GetSelectedTangentHandleType();
		if (SelectedTangentHandle != INDEX_NONE)
		{
			// If tangent handle index is set, use that
			check(SelectedTangentHandle < Position.Points.Num());
			const auto& Point = Position.Points[SelectedTangentHandle];

			check(SelectedTangentHandleType != ESelectedTangentHandle::None);
			const float TangentScale = GetDefault<ULevelEditorViewportSettings>()->SplineTangentScale;

			if (SelectedTangentHandleType == ESelectedTangentHandle::Leave)
			{
				OutLocation = SplineComp->GetComponentTransform().TransformPosition(Point.OutVal + Point.LeaveTangent * TangentScale);
			}
			else if (SelectedTangentHandleType == ESelectedTangentHandle::Arrive)
			{
				OutLocation = SplineComp->GetComponentTransform().TransformPosition(Point.OutVal - Point.ArriveTangent * TangentScale);
			}

			return true;
		}
		else if (LastKeyIndexSelected != INDEX_NONE)
		{
			// Otherwise use the last key index set
			check(LastKeyIndexSelected >= 0);
			if (LastKeyIndexSelected < Position.Points.Num())
			{
				check(SelectedKeys.Contains(LastKeyIndexSelected));
				const FInterpCurvePointVector& Point = Position.Points[LastKeyIndexSelected];
				OutLocation = SplineComp->GetComponentTransform().TransformPosition(Point.OutVal);
				if (!DuplicateDelayAccumulatedDrag.IsZero())
				{
					OutLocation += DuplicateDelayAccumulatedDrag;
				}
				return true;
			}
		}
	}

	return false;
}


bool FSplineComponentVisualizer::GetCustomInputCoordinateSystem(const FEditorViewportClient* ViewportClient, FMatrix& OutMatrix) const
{
	if (ViewportClient->GetWidgetCoordSystemSpace() == COORD_Local || ViewportClient->GetWidgetMode() == UE::Widget::WM_Rotate)
	{
		USplineComponent* SplineComp = GetEditedSplineComponent();
		if (SplineComp != nullptr)
		{
			check(SelectionState);
			OutMatrix = FRotationMatrix::Make(SelectionState->GetCachedRotation());
			return true;
		}
	}

	return false;
}


bool FSplineComponentVisualizer::IsVisualizingArchetype() const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	return (SplineComp && SplineComp->GetOwner() && FActorEditorUtils::IsAPreviewOrInactiveActor(SplineComp->GetOwner()));
}

bool FSplineComponentVisualizer::IsAnySelectedKeyIndexOutOfRange(const USplineComponent* Comp) const
{
	const int32 NumPoints = Comp->GetSplinePointsPosition().Points.Num();
	check(SelectionState);
	const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
	return Algo::AnyOf(SelectedKeys, [NumPoints](int32 Index) { return Index >= NumPoints; });
}

bool FSplineComponentVisualizer::IsSingleKeySelected() const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SelectionState);
	const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
	int32 LastKeyIndexSelected = SelectionState->GetLastKeyIndexSelected();
	return (SplineComp != nullptr &&
		SelectedKeys.Num() == 1 &&
		LastKeyIndexSelected != INDEX_NONE);
}

bool FSplineComponentVisualizer::AreMultipleKeysSelected() const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SelectionState);
	const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
	int32 LastKeyIndexSelected = SelectionState->GetLastKeyIndexSelected();
	return (SplineComp != nullptr &&
		SelectedKeys.Num() > 1 &&
		LastKeyIndexSelected != INDEX_NONE);
}

bool FSplineComponentVisualizer::AreKeysSelected() const
{
	return (IsSingleKeySelected() || AreMultipleKeysSelected());
}

bool FSplineComponentVisualizer::HandleInputDelta(FEditorViewportClient* ViewportClient, FViewport* Viewport, FVector& DeltaTranslateIn, FRotator& DeltaRotate, FVector& DeltaScale)
{
	using namespace SplineComponentVisualizerLocals;

	ResetTempModes();

	USplineComponent* SplineComp = GetEditedSplineComponent();
	bool bInputHandled = false;
	if (SplineComp != nullptr)
	{
		if (IsAnySelectedKeyIndexOutOfRange(SplineComp))
		{
			// Something external has changed the number of spline points, meaning that the cached selected keys are no longer valid
			EndEditing();
			return false;
		}

		check(SelectionState);

		// Use a local value for DeltaTranslate so that we can modify it based on the SnapToSurface setting without
		// changing it for the caller (that parameter should probably be const, but it's a base class function)
		FVector DeltaTranslate = DeltaTranslateIn;

		// It's tough to port the actor "surface snapping" toggle behavior here because the original code only works on actors
		// and involves things like keeping track of last transform, snapping to surface even if moving in a different plane, etc
		// (see FLevelEditorViewportClient::ProjectActorsIntoWorld). However we can at least support the limited but useful case
		// of trying to drag points around by the ball of the widget.
		const ULevelEditorViewportSettings* ViewportSettings = GetDefault<ULevelEditorViewportSettings>();
		const float SnapOffsetExtent = (ViewportSettings->SnapToSurface.bEnabled) ? (ViewportSettings->SnapToSurface.SnapOffsetExtent) : (0.0f);
		if (ViewportSettings->SnapToSurface.bEnabled
			&& ViewportClient->GetWidgetMode() == UE::Widget::EWidgetMode::WM_Translate
			&& (ViewportClient->GetCurrentWidgetAxis() == EAxisList::Screen || ViewportClient->GetCurrentWidgetAxis() == EAxisList::XYZ)
			&& !ViewportClient->IsOrtho())
		{
			FHitResult HitResult;
			if (RaycastWorld(SplineComp->GetWorld(), ViewportClient, Viewport, HitResult))
			{
				FVector3d NewGizmoLocation = HitResult.ImpactPoint + ViewportSettings->SnapToSurface.SnapOffsetExtent * HitResult.ImpactNormal;
				DeltaTranslate = NewGizmoLocation - ViewportClient->GetWidgetLocation();
			}
		}

		if (SelectionState->GetSelectedTangentHandle() != INDEX_NONE)
		{
			// Transform the tangent using an EPropertyChangeType::Interactive change. Later on, at the end of mouse tracking, a non-interactive change will be notified via void TrackingStopped :
			bInputHandled = TransformSelectedTangent(EPropertyChangeType::Interactive, DeltaTranslate);
		}
		else if (ViewportClient->IsAltPressed())
		{
			if (ViewportClient->GetWidgetMode() == UE::Widget::WM_Translate && ViewportClient->GetCurrentWidgetAxis() != EAxisList::None && SelectionState->GetSelectedKeys().Num() == 1)
			{
				static const int MaxDuplicationDelay = 3;

				FVector Drag = DeltaTranslate;

				if (bAllowDuplication)
				{
					float SmallestGridSize = 1.0f;
					const TArray<float>& PosGridSizes = GEditor->GetCurrentPositionGridArray();
					if (PosGridSizes.IsValidIndex(0))
					{
						SmallestGridSize = PosGridSizes[0];
					}

					// When grid size is set to a value other than the smallest grid size, do not delay duplication
					if (DuplicateDelay >= MaxDuplicationDelay || GEditor->GetGridSize() > SmallestGridSize)
					{
						Drag += DuplicateDelayAccumulatedDrag;
						DuplicateDelayAccumulatedDrag = FVector::ZeroVector;

						bAllowDuplication = false;
						bDuplicatingSplineKey = true;

						DuplicateKeyForAltDrag(Drag);
					}
					else
					{ 
						DuplicateDelay++;
						DuplicateDelayAccumulatedDrag += DeltaTranslate;
					}
				}
				else
				{
					UpdateDuplicateKeyForAltDrag(Drag);
				}

				bInputHandled = true;
			}
		}
		else
		{
			// Transform the spline keys using an EPropertyChangeType::Interactive change. Later on, at the end of mouse tracking, a non-interactive change will be notified via void TrackingStopped :
			bInputHandled = TransformSelectedKeys(EPropertyChangeType::Interactive, DeltaTranslate, DeltaRotate, DeltaScale);
		}
	}
	if (bInputHandled)
	{
		if (AActor* OwnerActor = SplineComp->GetOwner())
		{
			OwnerActor->PostEditMove(false);
		}
	}

	return bInputHandled;
}

bool FSplineComponentVisualizer::TransformSelectedTangent(EPropertyChangeType::Type InPropertyChangeType, const FVector& InDeltaTranslate)
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr)
	{
		FInterpCurveVector& SplinePosition = SplineComp->GetSplinePointsPosition();

		const int32 NumPoints = SplinePosition.Points.Num();

		check(SelectionState);
		int32 SelectedTangentHandle;
		ESelectedTangentHandle SelectedTangentHandleType;
		SelectionState->GetVerifiedSelectedTangentHandle(NumPoints, SelectedTangentHandle, SelectedTangentHandleType);

		if (!InDeltaTranslate.IsZero())
		{
			SplineComp->Modify();

			const float TangentScale = GetDefault<ULevelEditorViewportSettings>()->SplineTangentScale;

			FInterpCurvePoint<FVector>& EditedPoint = SplinePosition.Points[SelectedTangentHandle];
			if (SplineComp->bAllowDiscontinuousSpline)
			{
				if (SelectedTangentHandleType == ESelectedTangentHandle::Leave)
				{
					EditedPoint.LeaveTangent += SplineComp->GetComponentTransform().InverseTransformVector(InDeltaTranslate) / TangentScale;
				}
				else
				{
					EditedPoint.ArriveTangent += SplineComp->GetComponentTransform().InverseTransformVector(-InDeltaTranslate) / TangentScale;
				}
			}
			else
			{
				const FVector Delta = (SelectedTangentHandleType == ESelectedTangentHandle::Leave) ? InDeltaTranslate : -InDeltaTranslate;
				const FVector Tangent = EditedPoint.LeaveTangent + SplineComp->GetComponentTransform().InverseTransformVector(Delta) / TangentScale;

				EditedPoint.LeaveTangent = Tangent;
				EditedPoint.ArriveTangent = Tangent;
			}

			EditedPoint.InterpMode = CIM_CurveUser;
		}

		SplineComp->UpdateSpline();
		SplineComp->bSplineHasBeenEdited = true;

		NotifyPropertyModified(SplineComp, SplineCurvesProperty, InPropertyChangeType);

		return true;
	}

	return false;
}

bool FSplineComponentVisualizer::TransformSelectedKeys(EPropertyChangeType::Type InPropertyChangeType, const FVector& InDeltaTranslate, const FRotator& InDeltaRotate, const FVector& InDeltaScale)
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr)
	{
		FInterpCurveVector& SplinePosition = SplineComp->GetSplinePointsPosition();
		FInterpCurveQuat& SplineRotation = SplineComp->GetSplinePointsRotation();
		FInterpCurveVector& SplineScale = SplineComp->GetSplinePointsScale();

		const int32 NumPoints = SplinePosition.Points.Num();

		check(SelectionState);
		const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
		int32 LastKeyIndexSelected = SelectionState->GetVerifiedLastKeyIndexSelected(NumPoints);
		check(SelectedKeys.Num() > 0);
		check(SelectedKeys.Contains(LastKeyIndexSelected));

		SplineComp->Modify();

		for (int32 SelectedKeyIndex : SelectedKeys)
		{
			check(SelectedKeyIndex >= 0); 
			check(SelectedKeyIndex < NumPoints);

			FInterpCurvePoint<FVector>& EditedPoint = SplinePosition.Points[SelectedKeyIndex];
			FInterpCurvePoint<FQuat>& EditedRotPoint = SplineRotation.Points[SelectedKeyIndex];
			FInterpCurvePoint<FVector>& EditedScalePoint = SplineScale.Points[SelectedKeyIndex];


			if (!InDeltaTranslate.IsZero())
			{
				// Find key position in world space
				const FVector CurrentWorldPos = SplineComp->GetComponentTransform().TransformPosition(EditedPoint.OutVal);
				// Move in world space
				const FVector NewWorldPos = CurrentWorldPos + InDeltaTranslate;

				// Convert back to local space
				EditedPoint.OutVal = SplineComp->GetComponentTransform().InverseTransformPosition(NewWorldPos);
			}

			if (!InDeltaRotate.IsZero())
			{
				// Set point tangent as user controlled
				EditedPoint.InterpMode = CIM_CurveUser;

				// Rotate tangent according to delta rotation
				FVector NewTangent = SplineComp->GetComponentTransform().GetRotation().RotateVector(EditedPoint.LeaveTangent); // convert local-space tangent vector to world-space
				NewTangent = InDeltaRotate.RotateVector(NewTangent); // apply world-space delta rotation to world-space tangent
				NewTangent = SplineComp->GetComponentTransform().GetRotation().Inverse().RotateVector(NewTangent); // convert world-space tangent vector back into local-space
				EditedPoint.LeaveTangent = NewTangent;
				EditedPoint.ArriveTangent = NewTangent;

				// Rotate spline rotation according to delta rotation
				FQuat NewRot = SplineComp->GetComponentTransform().GetRotation() * EditedRotPoint.OutVal; // convert local-space rotation to world-space
				NewRot = InDeltaRotate.Quaternion() * NewRot; // apply world-space rotation
				NewRot = SplineComp->GetComponentTransform().GetRotation().Inverse() * NewRot; // convert world-space rotation to local-space
				EditedRotPoint.OutVal = NewRot;
			}

			if (InDeltaScale.X != 0.0f)
			{
				// Set point tangent as user controlled
				EditedPoint.InterpMode = CIM_CurveUser;

				const FVector NewTangent = EditedPoint.LeaveTangent * (1.0f + InDeltaScale.X);
				EditedPoint.LeaveTangent = NewTangent;
				EditedPoint.ArriveTangent = NewTangent;
			}

			if (InDeltaScale.Y != 0.0f)
			{
				// Scale in Y adjusts the scale spline
				EditedScalePoint.OutVal.Y *= (1.0f + InDeltaScale.Y);
			}

			if (InDeltaScale.Z != 0.0f)
			{
				// Scale in Z adjusts the scale spline
				EditedScalePoint.OutVal.Z *= (1.0f + InDeltaScale.Z);
			}
		}

		SplineComp->UpdateSpline();
		SplineComp->bSplineHasBeenEdited = true;

		NotifyPropertyModified(SplineComp, SplineCurvesProperty, InPropertyChangeType);

		if (!InDeltaRotate.IsZero())
		{
			SelectionState->Modify();
			SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(LastKeyIndexSelected, ESplineCoordinateSpace::World));
		}

		GEditor->RedrawLevelEditingViewports(true);

		return true;
	}

	return false;
}

bool FSplineComponentVisualizer::HandleInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	bool bHandled = false;

	USplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr && IsAnySelectedKeyIndexOutOfRange(SplineComp))
	{
		// Something external has changed the number of spline points, meaning that the cached selected keys are no longer valid
		EndEditing();
		return false;
	}

	if (Key == EKeys::LeftMouseButton && Event == IE_Released)
	{
		if (SplineComp != nullptr)
		{
			check(SelectionState);

			// Recache widget rotation
			int32 Index = SelectionState->GetSelectedTangentHandle();
			if (Index == INDEX_NONE)
			{
				// If not set, fall back to last key index selected
				Index = SelectionState->GetLastKeyIndexSelected();
			}

			SelectionState->Modify();
			SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(Index, ESplineCoordinateSpace::World));
		}

		// Reset duplication on LMB release
		ResetAllowDuplication();
	}

	if (Event == IE_Pressed)
	{
		bHandled = SplineComponentVisualizerActions->ProcessCommandBindings(Key, FSlateApplication::Get().GetModifierKeys(), false);
	}

	return bHandled;
}

bool FSplineComponentVisualizer::HandleModifiedClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	if (Click.IsControlDown())
	{
		ESplineComponentSnapMode SnapMode;

		if (GetSnapToActorMode(SnapMode))
		{
			ResetTempModes();

			if (HitProxy && HitProxy->IsA(HActor::StaticGetType()))
			{
				HActor* ActorProxy = static_cast<HActor*>(HitProxy);
				SnapKeyToActor(ActorProxy->Actor, SnapMode);
			}

			return true;
		}

	}

	ResetTempModes();

	/*
	if (Click.IsControlDown())
	{
		// Add points on Ctrl-Click if the last spline point is selected.

		USplineComponent* SplineComp = GetEditedSplineComponent();
		if (SplineComp != nullptr)
		{
			FInterpCurveVector& SplinePosition = SplineComp->GetSplinePointsPosition();
			int32 NumPoints = SplinePosition.Points.Num();

			// to do add end point
			check(SelectionState);
			const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
			if (SelectedKeys.Num() == 1 && !SplineComp->IsClosedLoop())
			{
				int32 LastKeyIndexSelected = SelectionState->GetLastKeyIndexSelected();
				check(LastKeyIndexSelected != INDEX_NONE);
				check(SelectedKeys.Contains(LastKeyIndexSelected));

				if (LastKeyIndexSelected == 0)
				{
					int32 KeyIdx = LastKeyIndexSelected;

					FInterpCurvePoint<FVector>& EditedPoint = SplinePosition.Points[LastKeyIndexSelected];

					FHitResult Hit(1.0f);
					FCollisionQueryParams Params(SCENE_QUERY_STAT(MoveSplineKeyToTrace), true);

					// Find key position in world space
					const FVector CurrentWorldPos = SplineComp->GetComponentTransform().TransformPosition(EditedPoint.OutVal);

					FVector DeltaTranslate = FVector::ZeroVector;

					if (SplineComp->GetWorld()->LineTraceSingleByChannel(Hit, Click.GetOrigin(), Click.GetOrigin() + Click.GetDirection() * WORLD_MAX, ECC_WorldStatic, Params))
					{
						DeltaTranslate = Hit.Location - CurrentWorldPos;
					}
					else
					{
						FVector ArriveTangent = SplineComp->GetComponentTransform().GetRotation().RotateVector(EditedPoint.ArriveTangent); // convert local-space tangent vector to world-space
						DeltaTranslate = ArriveTangent.GetSafeNormal() * ArriveTangent.Size() * 0.5;
						DeltaTranslate = ArriveTangent.GetSafeNormal() * ArriveTangent.Size() * 0.5;
					}

					OnAddKey();
					TransformSelectedKeys(DeltaTranslate);

					return true;
				}
			}
		}
	}
	*/
	return false;
}


bool FSplineComponentVisualizer::HandleBoxSelect(const FBox& InBox, FEditorViewportClient* InViewportClient, FViewport* InViewport) 
{
	const FScopedTransaction Transaction(LOCTEXT("HandleBoxSelect", "Box Select Spline Points"));

	check(SelectionState);
	SelectionState->Modify();

	ResetTempModes();

	USplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr)
	{
		bool bSelectionChanged = false;
		bool bAppendToSelection = InViewportClient->IsShiftPressed();

		const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
		const FInterpCurveVector& SplineInfo = SplineComp->GetSplinePointsPosition();
		int32 NumPoints = SplineInfo.Points.Num();

		// Spline control point selection always uses transparent box selection.
		for (int32 KeyIdx = 0; KeyIdx < NumPoints; KeyIdx++)
		{
			const FVector Pos = SplineComp->GetLocationAtSplinePoint(KeyIdx, ESplineCoordinateSpace::World);

			if (InBox.IsInside(Pos))
			{
				if (!bAppendToSelection || !SelectedKeys.Contains(KeyIdx))
				{
					ChangeSelectionState(KeyIdx, bAppendToSelection);
					bAppendToSelection = true;
					bSelectionChanged = true;
				}
			}
		}

		if (bSelectionChanged)
		{
			SelectionState->ClearSelectedSegmentIndex();
			SelectionState->ClearSelectedTangentHandle();
		}
	}

	return true;
}

bool FSplineComponentVisualizer::HandleFrustumSelect(const FConvexVolume& InFrustum, FEditorViewportClient* InViewportClient, FViewport* InViewport) 
{
	const FScopedTransaction Transaction(LOCTEXT("HandleFrustumSelect", "Frustum Select Spline Points"));

	check(SelectionState);
	SelectionState->Modify();

	ResetTempModes();

	USplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr)
	{
		bool bSelectionChanged = false;
		bool bAppendToSelection = InViewportClient->IsShiftPressed();

		const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
		const FInterpCurveVector& SplineInfo = SplineComp->GetSplinePointsPosition();
		int32 NumPoints = SplineInfo.Points.Num();

		// Spline control point selection always uses transparent box selection.
		for (int32 KeyIdx = 0; KeyIdx < NumPoints; KeyIdx++)
		{
			const FVector Pos = SplineComp->GetLocationAtSplinePoint(KeyIdx, ESplineCoordinateSpace::World);

			if (InFrustum.IntersectPoint(Pos))
			{
				if (!bAppendToSelection || !SelectedKeys.Contains(KeyIdx))
				{
					ChangeSelectionState(KeyIdx, bAppendToSelection);
					bAppendToSelection = true;
					bSelectionChanged = true;
				}
			}
		}

		if (bSelectionChanged)
		{
			SelectionState->ClearSelectedSegmentIndex();
			SelectionState->ClearSelectedTangentHandle();
		}

		return true;
	}

	return false;
}

bool FSplineComponentVisualizer::HasFocusOnSelectionBoundingBox(FBox& OutBoundingBox)
{
	OutBoundingBox.Init();

	if (USplineComponent* SplineComp = GetEditedSplineComponent())
	{
		check(SelectionState);
		const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
		if (SelectedKeys.Num() > 0)
		{
			// Spline control point selection always uses transparent box selection.
			for (int32 KeyIdx : SelectedKeys)
			{
				check(KeyIdx >= 0); 
				check(KeyIdx < SplineComp->GetNumberOfSplinePoints());

				const FVector Pos = SplineComp->GetLocationAtSplinePoint(KeyIdx, ESplineCoordinateSpace::World);

				OutBoundingBox += Pos;
			}

			OutBoundingBox = OutBoundingBox.ExpandBy(50.f);
			return true;
		}
	}

	return false;
}

bool FSplineComponentVisualizer::HandleSnapTo(const bool bInAlign, const bool bInUseLineTrace, const bool bInUseBounds, const bool bInUsePivot, AActor* InDestination)
{
	ResetTempModes();

	// Does not handle Snap/Align Pivot, Snap/Align Bottom Control Points or Snap/Align to Actor.
	if (bInUsePivot || bInUseBounds || InDestination)
	{
		return false;
	}

	// Note: value of bInUseLineTrace is ignored as we always line trace from control points.

	USplineComponent* SplineComp = GetEditedSplineComponent();

	if (SplineComp != nullptr)
	{
		check(SelectionState);
		const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
		if (SelectedKeys.Num() > 0)
		{
			int32 LastKeyIndexSelected = SelectionState->GetVerifiedLastKeyIndexSelected(SplineComp->GetNumberOfSplinePoints());
			check(SelectedKeys.Contains(LastKeyIndexSelected));

			SplineComp->Modify();

			FInterpCurveVector& SplinePosition = SplineComp->GetSplinePointsPosition();
			FInterpCurveQuat& SplineRotation = SplineComp->GetSplinePointsRotation();
			int32 NumPoints = SplinePosition.Points.Num();

			bool bMovedKey = false;

			// Spline control point selection always uses transparent box selection.
			for (int32 KeyIdx : SelectedKeys)
			{
				check(KeyIdx >= 0);
				check(KeyIdx < NumPoints);

				FVector Direction = FVector(0.f, 0.f, -1.f);

				FInterpCurvePoint<FVector>& EditedPoint = SplinePosition.Points[KeyIdx];
				FInterpCurvePoint<FQuat>& EditedRotPoint = SplineRotation.Points[KeyIdx];

				FHitResult Hit(1.0f);
				FCollisionQueryParams Params(SCENE_QUERY_STAT(MoveSplineKeyToTrace), true);

				// Find key position in world space
				const FVector CurrentWorldPos = SplineComp->GetComponentTransform().TransformPosition(EditedPoint.OutVal);

				if (SplineComp->GetWorld()->LineTraceSingleByChannel(Hit, CurrentWorldPos, CurrentWorldPos + Direction * WORLD_MAX, ECC_WorldStatic, Params))
				{
					// Convert back to local space
					EditedPoint.OutVal = SplineComp->GetComponentTransform().InverseTransformPosition(Hit.Location);

					if (bInAlign)
					{		
						// Set point tangent as user controlled
						EditedPoint.InterpMode = CIM_CurveUser;

						// Get delta rotation between up vector and hit normal
						FVector WorldUpVector = SplineComp->GetUpVectorAtSplineInputKey((float)KeyIdx, ESplineCoordinateSpace::World);
						FQuat DeltaRotate = FQuat::FindBetweenNormals(WorldUpVector, Hit.Normal);

						// Rotate tangent according to delta rotation
						FVector NewTangent = SplineComp->GetComponentTransform().GetRotation().RotateVector(EditedPoint.LeaveTangent); // convert local-space tangent vector to world-space
						NewTangent = DeltaRotate.RotateVector(NewTangent); // apply world-space delta rotation to world-space tangent
						NewTangent = SplineComp->GetComponentTransform().GetRotation().Inverse().RotateVector(NewTangent); // convert world-space tangent vector back into local-space
						EditedPoint.LeaveTangent = NewTangent;
						EditedPoint.ArriveTangent = NewTangent;

						// Rotate spline rotation according to delta rotation
						FQuat NewRot = SplineComp->GetComponentTransform().GetRotation() * EditedRotPoint.OutVal; // convert local-space rotation to world-space
						NewRot = DeltaRotate * NewRot; // apply world-space rotation
						NewRot = SplineComp->GetComponentTransform().GetRotation().Inverse() * NewRot; // convert world-space rotation to local-space
						EditedRotPoint.OutVal = NewRot;
					}

					bMovedKey = true;
				}
			}

			if (bMovedKey)
			{
				SplineComp->UpdateSpline();
				SplineComp->bSplineHasBeenEdited = true;

				NotifyPropertyModified(SplineComp, SplineCurvesProperty);
				if (AActor* Owner = SplineComp->GetOwner())
				{
					Owner->PostEditMove(true);
				}
				
				if (bInAlign)
				{
					SelectionState->Modify();
					SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(LastKeyIndexSelected, ESplineCoordinateSpace::World));
				}

				GEditor->RedrawLevelEditingViewports(true);
			}

			return true;
		}
	}

	return false;
}

void FSplineComponentVisualizer::TrackingStopped(FEditorViewportClient* InViewportClient, bool bInDidMove)
{
	if (bInDidMove)
	{
		// After dragging, notify that the spline curves property has changed one last time, this time as a EPropertyChangeType::ValueSet :
		USplineComponent* SplineComp = GetEditedSplineComponent();
		NotifyPropertyModified(SplineComp, SplineCurvesProperty, EPropertyChangeType::ValueSet);
		if (AActor* Owner = SplineComp->GetOwner())
		{
			Owner->PostEditMove(true);
		}
	}
}

void FSplineComponentVisualizer::OnSnapKeyToNearestSplinePoint(ESplineComponentSnapMode InSnapMode)
{
	const FScopedTransaction Transaction(LOCTEXT("SnapToNearestSplinePoint", "Snap To Nearest Spline Point"));

	ResetTempModes();

	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);
	check(SelectionState);
	const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
	int32 LastKeyIndexSelected = SelectionState->GetLastKeyIndexSelected();
	check(LastKeyIndexSelected != INDEX_NONE);
	check(LastKeyIndexSelected >= 0);
	check(LastKeyIndexSelected < SplineComp->GetNumberOfSplinePoints());
	check(SelectedKeys.Num() == 1);
	check(SelectedKeys.Contains(LastKeyIndexSelected));

	FInterpCurvePoint<FVector>& EditedPosition = SplineComp->GetSplinePointsPosition().Points[LastKeyIndexSelected];
	const FVector WorldPos = SplineComp->GetComponentTransform().TransformPosition(EditedPosition.OutVal); // convert local-space position to world-space

	double NearestDistanceSquared = 0.0f;
	USplineComponent* NearestSplineComp = nullptr;
	int32 NearestKeyIndex = INDEX_NONE;

	static const double SnapTol = 5000.0f;
	double SnapTolSquared = SnapTol * SnapTol;

	auto UpdateNearestKey = [WorldPos, SnapTolSquared, &NearestDistanceSquared, &NearestSplineComp, &NearestKeyIndex](USplineComponent* InSplineComp, int InKeyIdx)
	{
		const FVector TestKeyWorldPos = InSplineComp->GetLocationAtSplinePoint(InKeyIdx, ESplineCoordinateSpace::World);
		double TestDistanceSquared = FVector::DistSquared(TestKeyWorldPos, WorldPos);

		if (TestDistanceSquared < SnapTolSquared && (NearestKeyIndex == INDEX_NONE || TestDistanceSquared < NearestDistanceSquared))
		{
			NearestDistanceSquared = TestDistanceSquared;
			NearestSplineComp = InSplineComp;
			NearestKeyIndex = InKeyIdx;
		}
	};

	{
		// Test non-adjacent points on current spline.
		const FInterpCurveVector& SplineInfo = SplineComp->GetSplinePointsPosition();
		const int32 NumPoints = SplineInfo.Points.Num();

		// Don't test against current or adjacent points
		TSet<int32> IgnoreIndices;
		IgnoreIndices.Add(LastKeyIndexSelected);
		int32 PrevIndex = LastKeyIndexSelected - 1;
		int32 NextIndex = LastKeyIndexSelected + 1;

		if (PrevIndex >= 0)
		{
			IgnoreIndices.Add(PrevIndex);
		}
		else if (SplineComp->IsClosedLoop())
		{
			IgnoreIndices.Add(NumPoints - 1);
		}

		if (NextIndex < NumPoints)
		{
			IgnoreIndices.Add(NextIndex);
		}
		else if (SplineComp->IsClosedLoop())
		{
			IgnoreIndices.Add(0);
		}

		for (int32 KeyIdx = 0; KeyIdx < NumPoints; KeyIdx++)
		{
			if (!IgnoreIndices.Contains(KeyIdx))
			{
				UpdateNearestKey(SplineComp, KeyIdx);
			}
		}
	}

	// Test whether component and its owning actor are valid and visible
	auto IsValidAndVisible = [](const USplineComponent* Comp)
	{
		return (Comp && !Comp->IsBeingDestroyed() && Comp->IsVisibleInEditor() &&
				Comp->GetOwner() && IsValid(Comp->GetOwner()) && !Comp->GetOwner()->IsHiddenEd());
	};

	// Next search all spline components for nearest point on splines, excluding current spline
	// Only test points in splines whose bounding box contains this point.
	for (TObjectIterator<USplineComponent> SplineIt; SplineIt; ++SplineIt)
	{
		USplineComponent* TestComponent = *SplineIt;

		// Ignore current spline and those which are not valid 
		if (TestComponent && TestComponent != SplineComp && IsValidAndVisible(TestComponent) &&
			!FMath::IsNearlyZero(TestComponent->Bounds.SphereRadius))
		{
			FBox TestComponentBoundingBox = TestComponent->Bounds.GetBox().ExpandBy(FVector(SnapTol, SnapTol, SnapTol));

			if (TestComponentBoundingBox.IsInsideOrOn(WorldPos))
			{
				const FInterpCurveVector& SplineInfo = TestComponent->GetSplinePointsPosition();
				const int32 NumPoints = SplineInfo.Points.Num();
				for (int32 KeyIdx = 0; KeyIdx < NumPoints; KeyIdx++)
				{
					UpdateNearestKey(TestComponent, KeyIdx);
				}
			}
		}
	}

	if (!NearestSplineComp || NearestKeyIndex == INDEX_NONE)
	{
		UE_LOG(LogSplineComponentVisualizer, Warning, TEXT("No nearest spline point found."));
		return;
	}

	const FInterpCurvePoint<FVector>& NearestPosition = NearestSplineComp->GetSplinePointsPosition().Points[NearestKeyIndex];

	// Copy position
	const FVector NearestWorldPos = NearestSplineComp->GetComponentTransform().TransformPosition(NearestPosition.OutVal); // convert local-space position to world-space
	FVector NearestWorldUpVector(0.0f, 0.0f, 1.0f);
	FVector NearestWorldTangent(0.0f, 1.0f, 0.0f);
	FVector NearestWorldScale(1.0f, 1.0f, 1.0f);
	USplineMetadata* NearestSplineMetadata = nullptr;

	if (InSnapMode == ESplineComponentSnapMode::AlignToTangent || InSnapMode == ESplineComponentSnapMode::AlignPerpendicularToTangent)
	{
		// Get tangent
		NearestWorldTangent = NearestSplineComp->GetComponentTransform().GetRotation().RotateVector(NearestPosition.ArriveTangent); // convert local-space tangent vectors to world-space

		// Get up vector
		NearestWorldUpVector = NearestSplineComp->GetUpVectorAtSplinePoint(NearestKeyIndex, ESplineCoordinateSpace::World);

		// Get scale, only when aligning parallel
		if (InSnapMode == ESplineComponentSnapMode::AlignToTangent)
		{
			const FInterpCurvePoint<FVector>& NearestScale = NearestSplineComp->GetSplinePointsScale().Points[NearestKeyIndex];
			NearestWorldScale = SplineComp->GetComponentTransform().GetScale3D() * NearestScale.OutVal; // convert local-space rotation to world-space
		}

		// Get metadata (only when aligning)
		USplineMetadata* SplineMetadata = SplineComp->GetSplinePointsMetadata();
		NearestSplineMetadata = SplineMetadata ? NearestSplineComp->GetSplinePointsMetadata() : nullptr;
	}

	SnapKeyToTransform(InSnapMode, NearestWorldPos, NearestWorldUpVector, NearestWorldTangent, NearestWorldScale, NearestSplineMetadata, NearestKeyIndex);
}

void FSplineComponentVisualizer::OnSnapKeyToActor(const ESplineComponentSnapMode InSnapMode)
{
	ResetTempModes();
	SetSnapToActorMode(true, InSnapMode);
}

void FSplineComponentVisualizer::SnapKeyToActor(const AActor* InActor, const ESplineComponentSnapMode InSnapMode)
{
	const FScopedTransaction Transaction(LOCTEXT("SnapToActor", "Snap To Actor"));

	if (InActor && IsSingleKeySelected())
	{
		const FVector ActorLocation = InActor->GetActorLocation();
		const FVector ActorUpVector = InActor->GetActorUpVector();
		const FVector ActorForwardVector = InActor->GetActorForwardVector();
		const FVector UniformScale(1.0f, 1.0f, 1.0f);

		SnapKeyToTransform(InSnapMode, ActorLocation, ActorUpVector, ActorForwardVector, UniformScale);
	}
}

void FSplineComponentVisualizer::SnapKeyToTransform(const ESplineComponentSnapMode InSnapMode,
	const FVector& InWorldPos,
	const FVector& InWorldUpVector,
	const FVector& InWorldForwardVector,
	const FVector& InScale,
	const USplineMetadata* InCopySplineMetadata,
	const int32 InCopySplineMetadataKey)
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);
	check(SelectionState);
	const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
	int32 LastKeyIndexSelected = SelectionState->GetVerifiedLastKeyIndexSelected(SplineComp->GetNumberOfSplinePoints());
	check(SelectedKeys.Num() == 1);
	check(SelectedKeys.Contains(LastKeyIndexSelected));

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	FInterpCurvePoint<FVector>& EditedPosition = SplineComp->GetSplinePointsPosition().Points[LastKeyIndexSelected];
	FInterpCurvePoint<FQuat>& EditedRotation = SplineComp->GetSplinePointsRotation().Points[LastKeyIndexSelected];
	FInterpCurvePoint<FVector>& EditedScale = SplineComp->GetSplinePointsScale().Points[LastKeyIndexSelected];

	// Copy position
	EditedPosition.OutVal = SplineComp->GetComponentTransform().InverseTransformPosition(InWorldPos); // convert world-space position to local-space

	if (InSnapMode == ESplineComponentSnapMode::AlignToTangent || InSnapMode == ESplineComponentSnapMode::AlignPerpendicularToTangent)
	{
		// Copy tangents
		FVector AlignTangent;
		FQuat AlignRot;
		const FVector WorldUpVector = InWorldUpVector.GetSafeNormal();
		const FVector WorldForwardVector = InWorldForwardVector.GetSafeNormal();

		// Copy tangents
		FVector NewTangent = WorldForwardVector;

		if (InSnapMode == ESplineComponentSnapMode::AlignPerpendicularToTangent)
		{
			// Rotate tangent by 90 degrees
			const FQuat DeltaRotate(WorldUpVector, UE_HALF_PI);
			NewTangent = DeltaRotate.RotateVector(NewTangent);
		}

		const FVector Tangent = SplineComp->GetComponentTransform().GetRotation().RotateVector(EditedPosition.ArriveTangent); // convert local-space tangent vectors to world-space

		// Swap the tangents if they are not pointing in the same general direction
		double CurrentAngle = FMath::Acos(FVector::DotProduct(Tangent, NewTangent) / Tangent.Size());
		if (CurrentAngle > UE_HALF_PI)
		{
			NewTangent = SplineComp->GetComponentTransform().GetRotation().Inverse().RotateVector(NewTangent * -1.0f) * Tangent.Size(); // convert world-space tangent vectors into local-space
		}
		else
		{
			NewTangent = SplineComp->GetComponentTransform().GetRotation().Inverse().RotateVector(NewTangent) * Tangent.Size(); // convert world-space tangent vectors into local-space
		}

		// Update tangent
		EditedPosition.ArriveTangent = NewTangent;
		EditedPosition.LeaveTangent = NewTangent;
		EditedPosition.InterpMode = CIM_CurveUser;

		// Copy rotation, it is only used to determine up vector so no need to adjust it 
		const FQuat Rot = FQuat::FindBetweenNormals(FVector(0.0f, 0.0f, 1.0f), WorldUpVector);
		EditedRotation.OutVal = SplineComp->GetComponentTransform().GetRotation().Inverse() * Rot; // convert world-space rotation to local-space

		// Copy scale, only when aligning parallel
		if (InSnapMode == ESplineComponentSnapMode::AlignToTangent)
		{
			const FVector SplineCompScale = SplineComp->GetComponentTransform().GetScale3D();
			EditedScale.OutVal.X = FMath::IsNearlyZero(SplineCompScale.X) ? InScale.X : InScale.X / SplineCompScale.X; // convert world-space scale to local-space
			EditedScale.OutVal.Y = FMath::IsNearlyZero(SplineCompScale.Y) ? InScale.Y : InScale.Y / SplineCompScale.Y;
			EditedScale.OutVal.Z = FMath::IsNearlyZero(SplineCompScale.Z) ? InScale.Z : InScale.Z / SplineCompScale.Z;
		}

	}

	// Copy metadata
	if (InCopySplineMetadata)
	{
		if (USplineMetadata* SplineMetadata = SplineComp->GetSplinePointsMetadata())
		{
			SplineMetadata->CopyPoint(InCopySplineMetadata, InCopySplineMetadataKey, LastKeyIndexSelected);
		}
	}

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;

	NotifyPropertyModified(SplineComp, SplineCurvesProperty);
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->PostEditMove(true);
	}

	if (InSnapMode == ESplineComponentSnapMode::AlignToTangent || InSnapMode == ESplineComponentSnapMode::AlignPerpendicularToTangent)
	{
		SelectionState->Modify();
		SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(LastKeyIndexSelected, ESplineCoordinateSpace::World));
	}

	GEditor->RedrawLevelEditingViewports(true);
}

void FSplineComponentVisualizer::OnSnapAllToAxis(EAxis::Type InAxis)
{
	const FScopedTransaction Transaction(LOCTEXT("SnapAllToSelectedAxis", "Snap All To Selected Axis"));

	ResetTempModes();

	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);
	check(SelectionState);
	const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
	int32 LastKeyIndexSelected = SelectionState->GetVerifiedLastKeyIndexSelected(SplineComp->GetNumberOfSplinePoints());
	check(SelectedKeys.Num() == 1);
	check(SelectedKeys.Contains(LastKeyIndexSelected));
	check(InAxis == EAxis::X || InAxis == EAxis::Y || InAxis == EAxis::Z);

	TArray<int32> SnapKeys;
	for (int32 KeyIdx = 0; KeyIdx < SplineComp->GetNumberOfSplinePoints(); KeyIdx++)
	{
		if (KeyIdx != LastKeyIndexSelected)
		{
			SnapKeys.Add(KeyIdx);
		}
	}

	SnapKeysToLastSelectedAxisPosition(InAxis, SnapKeys);
}

void FSplineComponentVisualizer::OnSnapSelectedToAxis(EAxis::Type InAxis)
{
	const FScopedTransaction Transaction(LOCTEXT("SnapSelectedToLastAxis", "Snap Selected To Axis"));

	ResetTempModes();

	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SelectionState);
	const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
	int32 LastKeyIndexSelected = SelectionState->GetVerifiedLastKeyIndexSelected(SplineComp->GetNumberOfSplinePoints());
	check(SplineComp != nullptr);	
	check(SelectedKeys.Num() > 1);

	TArray<int32> SnapKeys;
	for (int32 KeyIdx : SelectedKeys)
	{
		if (KeyIdx != LastKeyIndexSelected)
		{
			SnapKeys.Add(KeyIdx);
		}
	}

	SnapKeysToLastSelectedAxisPosition(InAxis, SnapKeys);
}

void FSplineComponentVisualizer::OnStraightenKey(int32 Direction)
{
	const FScopedTransaction Transaction(LOCTEXT("Straighten To Previous", "Straighten Points Toward Previous"));

	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);
	check(SelectionState);
	const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
	int32 LastKeyIndexSelected = SelectionState->GetVerifiedLastKeyIndexSelected(SplineComp->GetNumberOfSplinePoints());

	for (int32 CurrentKey : SelectedKeys)
	{
		int32 ToKey = CurrentKey + Direction;
		if (ToKey != INDEX_NONE && ToKey < SplineComp->GetNumberOfSplinePoints())
		{
			StraightenKey(CurrentKey, ToKey);
		}
	}

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;

	NotifyPropertyModified(SplineComp, SplineCurvesProperty);
	if (AActor* OwnerActor = SplineComp->GetOwner())
	{
		OwnerActor->PostEditMove(true);
	}

	SelectionState->Modify();
	SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(LastKeyIndexSelected, ESplineCoordinateSpace::World));

	GEditor->RedrawLevelEditingViewports(true);
}

void FSplineComponentVisualizer::StraightenKey(int32 KeyToStraighten, int32 KeyToStraightenToward)
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);

	const float TangentLength = SplineComp->GetTangentAtSplinePoint(KeyToStraighten, ESplineCoordinateSpace::Local).Length();
	FVector StraightenLocation = SplineComp->GetLocationAtSplinePoint(KeyToStraighten, ESplineCoordinateSpace::Local);
	FVector TowardLocation = SplineComp->GetLocationAtSplinePoint(KeyToStraightenToward, ESplineCoordinateSpace::Local);
	FVector Direction = TowardLocation - StraightenLocation;
	Direction.Normalize();

	FVector NewTangent = Direction * TangentLength * (KeyToStraighten > KeyToStraightenToward ? 1 : -1);
	SplineComp->SetTangentAtSplinePoint(KeyToStraighten, -NewTangent, ESplineCoordinateSpace::Local);
}

void FSplineComponentVisualizer::OnToggleSnapTangentAdjustment()
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	SplineComp->bAdjustTangentsOnSnap = !SplineComp->bAdjustTangentsOnSnap;

	TArray<FProperty*> Properties;
	Properties.Add(SplineCurvesProperty);
	Properties.Add(FindFProperty<FProperty>(USplineComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(USplineComponent, bAdjustTangentsOnSnap)));
	NotifyPropertiesModified(SplineComp, Properties);

	GEditor->RedrawLevelEditingViewports(true);
}

bool FSplineComponentVisualizer::IsSnapTangentAdjustment() const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	return SplineComp ? SplineComp->bAdjustTangentsOnSnap : false;
}

void FSplineComponentVisualizer::SnapKeysToLastSelectedAxisPosition(const EAxis::Type InAxis, TArray<int32> InSnapKeys)
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);
	check(SelectionState);
	check(InAxis == EAxis::X || InAxis == EAxis::Y || InAxis == EAxis::Z);
	int32 LastKeyIndexSelected = SelectionState->GetVerifiedLastKeyIndexSelected(SplineComp->GetNumberOfSplinePoints());

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	FInterpCurveVector& SplinePositions = SplineComp->GetSplinePointsPosition();
	FInterpCurveQuat& SplineRotations = SplineComp->GetSplinePointsRotation();

	const FVector WorldPos = SplineComp->GetComponentTransform().TransformPosition(SplinePositions.Points[LastKeyIndexSelected].OutVal); 

	int32 NumPoints = SplinePositions.Points.Num();

	for (int32 KeyIdx : InSnapKeys)
	{
		if (KeyIdx >= 0 && KeyIdx < SplineComp->GetNumberOfSplinePoints())
		{
			FInterpCurvePoint<FVector>& EditedPosition = SplinePositions.Points[KeyIdx];
			FInterpCurvePoint<FQuat>& EditedRotation = SplineRotations.Points[KeyIdx];

			// Copy position
			FVector NewWorldPos = SplineComp->GetComponentTransform().TransformPosition(EditedPosition.OutVal); // convert local-space position to world-space
			if (InAxis == EAxis::X)
			{
				NewWorldPos.X = WorldPos.X;
			}
			else if (InAxis == EAxis::Y)
			{
				NewWorldPos.Y = WorldPos.Y;
			}
			else
			{
				NewWorldPos.Z = WorldPos.Z;
			}

			EditedPosition.OutVal = SplineComp->GetComponentTransform().InverseTransformPosition(NewWorldPos); // convert world-space position to local-space

			// Set point to auto so its tangents will be auto-adjusted after snapping
			if (SplineComp->bAdjustTangentsOnSnap)
			{
				EditedPosition.InterpMode = CIM_CurveAuto;
			}
		}
	}

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;

	NotifyPropertyModified(SplineComp, SplineCurvesProperty);
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->PostEditMove(true);
	}

	SelectionState->Modify();
	SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(LastKeyIndexSelected, ESplineCoordinateSpace::World));

	GEditor->RedrawLevelEditingViewports(true);
}

void FSplineComponentVisualizer::EndEditing()
{
	// Ignore if there is an undo/redo operation in progress
	if (!GIsTransacting)
	{
		check(SelectionState);
		SelectionState->Modify();

		if (USplineComponent* SplineComp = GetEditedSplineComponent())
		{
			ChangeSelectionState(INDEX_NONE, false);
			SelectionState->ClearSelectedSegmentIndex();
			SelectionState->ClearSelectedTangentHandle();
		}
		SelectionState->SetSplinePropertyPath(FComponentPropertyPath());

		ResetTempModes();
	}
}

void FSplineComponentVisualizer::ResetTempModes()
{
	SetSnapToActorMode(false);
}

void FSplineComponentVisualizer::SetSnapToActorMode(const bool bInIsSnappingToActor, const ESplineComponentSnapMode InSnapMode)
{
	bIsSnappingToActor = bInIsSnappingToActor;
	SnapToActorMode = InSnapMode;
}

bool FSplineComponentVisualizer::GetSnapToActorMode(ESplineComponentSnapMode& OutSnapMode) const 
{
	OutSnapMode = SnapToActorMode;
	return bIsSnappingToActor;
}

void FSplineComponentVisualizer::OnDuplicateKey()
{
	const FScopedTransaction Transaction(LOCTEXT("DuplicateSplinePoint", "Duplicate Spline Point"));

	ResetTempModes();
	
	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);
	check(SelectionState);
	const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
	int32 LastKeyIndexSelected = SelectionState->GetVerifiedLastKeyIndexSelected(SplineComp->GetNumberOfSplinePoints());
	check(SelectionState->GetSelectedKeys().Num() > 0);
	check(SelectionState->GetSelectedKeys().Contains(LastKeyIndexSelected));

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	// Get a sorted list of all the selected indices, highest to lowest
	TArray<int32> SelectedKeysSorted;
	for (int32 SelectedKeyIndex : SelectedKeys)
	{
		SelectedKeysSorted.Add(SelectedKeyIndex);
	}
	SelectedKeysSorted.Sort([](int32 A, int32 B) { return A > B; });

	// Insert duplicates into the list, highest index first, so that the lower indices remain the same
	FInterpCurveVector& SplinePosition = SplineComp->GetSplinePointsPosition();
	FInterpCurveQuat& SplineRotation = SplineComp->GetSplinePointsRotation();
	FInterpCurveVector& SplineScale = SplineComp->GetSplinePointsScale();
	USplineMetadata* SplineMetadata = SplineComp->GetSplinePointsMetadata();

	for (int32 SelectedKeyIndex : SelectedKeysSorted)
	{
		check(SelectedKeyIndex >= 0);
		check(SelectedKeyIndex < SplineComp->GetNumberOfSplinePoints());

		// Insert duplicates into arrays.
		// It's necessary to take a copy because copying existing array items by reference isn't allowed (the array may reallocate)
		SplinePosition.Points.Insert(FInterpCurvePoint<FVector>(SplinePosition.Points[SelectedKeyIndex]), SelectedKeyIndex);
		SplineRotation.Points.Insert(FInterpCurvePoint<FQuat>(SplineRotation.Points[SelectedKeyIndex]), SelectedKeyIndex);
		SplineScale.Points.Insert(FInterpCurvePoint<FVector>(SplineScale.Points[SelectedKeyIndex]), SelectedKeyIndex);

		if (SplineMetadata)
		{
			SplineMetadata->DuplicatePoint(SelectedKeyIndex);
		}

		// Adjust input keys of subsequent points
		for (int Index = SelectedKeyIndex + 1; Index < SplinePosition.Points.Num(); Index++)
		{
			SplinePosition.Points[Index].InVal += 1.0f;
			SplineRotation.Points[Index].InVal += 1.0f;
			SplineScale.Points[Index].InVal += 1.0f;
		}
	}

	SelectionState->Modify();

	// Repopulate the selected keys
	TSet<int32>& NewSelectedKeys = SelectionState->ModifySelectedKeys();
	NewSelectedKeys.Empty();
	int32 Offset = SelectedKeysSorted.Num();
	for (int32 SelectedKeyIndex : SelectedKeysSorted)
	{
		NewSelectedKeys.Add(SelectedKeyIndex + Offset);

		if (SelectionState->GetLastKeyIndexSelected() == SelectedKeyIndex)
		{
			SelectionState->SetLastKeyIndexSelected(SelectionState->GetLastKeyIndexSelected() + Offset);
		}

		Offset--;
	}

	// Unset tangent handle selection
	SelectionState->ClearSelectedTangentHandle();

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;

	NotifyPropertyModified(SplineComp, SplineCurvesProperty);
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->PostEditMove(true);
	}

	if (NewSelectedKeys.Num() == 1)
	{
		SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(SelectionState->GetLastKeyIndexSelected(), ESplineCoordinateSpace::World));
	}

	GEditor->RedrawLevelEditingViewports(true);
}

bool FSplineComponentVisualizer::CanAddKeyToSegment() const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp == nullptr)
	{
		return false;
	}

	check(SelectionState);
	int32 SelectedSegmentIndex = SelectionState->GetSelectedSegmentIndex();
	return (SelectedSegmentIndex != INDEX_NONE && SelectedSegmentIndex >=0 && SelectedSegmentIndex < SplineComp->GetNumberOfSplineSegments());
}

void FSplineComponentVisualizer::OnAddKeyToSegment()
{
	const FScopedTransaction Transaction(LOCTEXT("AddSplinePoint", "Add Spline Point"));

	ResetTempModes();

	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);
	check(SelectionState);
	check(SelectionState->GetSelectedTangentHandle() == INDEX_NONE);
	check(SelectionState->GetSelectedTangentHandleType() == ESelectedTangentHandle::None);

	SelectionState->Modify();

	SplitSegment(SelectionState->GetSelectedSplinePosition(), SelectionState->GetSelectedSegmentIndex());

	SelectionState->SetSelectedSegmentIndex(INDEX_NONE);
	SelectionState->SetSelectedSplinePosition(FVector::ZeroVector);
	SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(SelectionState->GetLastKeyIndexSelected(), ESplineCoordinateSpace::World));
}

bool FSplineComponentVisualizer::DuplicateKeyForAltDrag(const FVector& InDrag)
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);
	check(SelectionState);
	const int32 NumPoints = SplineComp->GetNumberOfSplinePoints();
	const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
	int32 LastKeyIndexSelected = SelectionState->GetVerifiedLastKeyIndexSelected(NumPoints);
	check(SelectedKeys.Num() == 1);
	check(SelectedKeys.Contains(LastKeyIndexSelected));

	// When dragging from end point, maximum angle is 60 degrees from attached segment
	// to determine whether to split existing segment or create a new point
	static const double Angle60 = 1.0472;

	// Insert duplicates into the list, highest index first, so that the lower indices remain the same
	FInterpCurveVector& SplinePosition = SplineComp->GetSplinePointsPosition();

	// Find key position in world space
	int32 CurrentIndex = LastKeyIndexSelected;
	const FVector CurrentKeyWorldPos = SplineComp->GetComponentTransform().TransformPosition(SplinePosition.Points[CurrentIndex].OutVal);

	// Determine direction to insert new point				
	bool bHasPrevKey = SplineComp->IsClosedLoop() || CurrentIndex > 0;
	double PrevAngle = 0.0f;
	if (bHasPrevKey)
	{
		// Wrap index around for closed-looped splines
		int32 PrevKeyIndex = (CurrentIndex > 0 ? CurrentIndex - 1 : NumPoints - 1);
		FVector PrevKeyWorldPos = SplineComp->GetComponentTransform().TransformPosition(SplinePosition.Points[PrevKeyIndex].OutVal);
		FVector SegmentDirection = PrevKeyWorldPos - CurrentKeyWorldPos;
		if (!SegmentDirection.IsZero())
		{
			PrevAngle = FMath::Acos(FVector::DotProduct(InDrag, SegmentDirection) / (InDrag.Size() * SegmentDirection.Size()));
		}
		else
		{
			PrevAngle = Angle60;
		}
	}

	bool bHasNextKey = SplineComp->IsClosedLoop() || CurrentIndex + 1 < NumPoints;
	double NextAngle = 0.0f;
	if (bHasNextKey)
	{
		// Wrap index around for closed-looped splines
		int32 NextKeyIndex = (CurrentIndex + 1 < NumPoints ? CurrentIndex + 1 : 0);
		FVector NextKeyWorldPos = SplineComp->GetComponentTransform().TransformPosition(SplinePosition.Points[NextKeyIndex].OutVal);
		FVector SegmentDirection = NextKeyWorldPos - CurrentKeyWorldPos;
		if (!SegmentDirection.IsZero())
		{
			NextAngle = FMath::Acos(FVector::DotProduct(InDrag, SegmentDirection) / (InDrag.Size() * SegmentDirection.Size()));
		}
		else
		{
			NextAngle = Angle60;
		}
	}

	// Set key index to which the drag will be applied after duplication
	int32 SegmentIndex = CurrentIndex;

	if ((bHasPrevKey && bHasNextKey && PrevAngle < NextAngle) ||
		(bHasPrevKey && !bHasNextKey && PrevAngle < Angle60) ||
		(!bHasPrevKey && bHasNextKey && NextAngle >= Angle60))
	{
		SegmentIndex--;
	}

	// Wrap index around for closed-looped splines
	const int32 NumSegments = SplineComp->GetNumberOfSplineSegments();
	if (SplineComp->IsClosedLoop() && SegmentIndex < 0)
	{
		SegmentIndex = NumSegments - 1;
	}

	FVector WorldPos = CurrentKeyWorldPos + InDrag;

	// Split existing segment or add new segment
	if (SegmentIndex >= 0 && SegmentIndex < NumSegments)
	{
		bool bCopyFromSegmentBeginIndex = (LastKeyIndexSelected == SegmentIndex);
		SplitSegment(WorldPos, SegmentIndex, bCopyFromSegmentBeginIndex);

	}
	else
	{
		AddSegment(WorldPos, (SegmentIndex > 0));
		bUpdatingAddSegment = true;
	}

	// Unset tangent handle selection
	SelectionState->Modify();
	SelectionState->ClearSelectedTangentHandle();
	
	return true;
}

bool FSplineComponentVisualizer::UpdateDuplicateKeyForAltDrag(const FVector& InDrag)
{
	if (bUpdatingAddSegment)
	{
		UpdateAddSegment(InDrag);
	}
	else
	{
		UpdateSplitSegment(InDrag);
	}

	return true;
}

float FSplineComponentVisualizer::FindNearest(const FVector& InLocalPos, int32 InSegmentIndex, FVector& OutSplinePos, FVector& OutSplineTangent) const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);
	check(InSegmentIndex != INDEX_NONE);
	check(InSegmentIndex >= 0);
	check(InSegmentIndex < SplineComp->GetNumberOfSplineSegments());

	FInterpCurveVector& SplinePosition = SplineComp->GetSplinePointsPosition();
	float OutSquaredDistance = 0.0f;
	float t = SplinePosition.FindNearestOnSegment(InLocalPos, InSegmentIndex, OutSquaredDistance);
	OutSplinePos = SplinePosition.Eval(t, FVector::ZeroVector);
	OutSplineTangent = SplinePosition.EvalDerivative(t, FVector::ZeroVector);

	return t;
}

void FSplineComponentVisualizer::SplitSegment(const FVector& InWorldPos, int32 InSegmentIndex, bool bCopyFromSegmentBeginIndex /* = true */)
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);
	check(SelectionState);
	const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
	int32 LastKeyIndexSelected = SelectionState->GetVerifiedLastKeyIndexSelected(SplineComp->GetNumberOfSplinePoints());
	check(SelectedKeys.Num() == 1);
	check(SelectedKeys.Contains(LastKeyIndexSelected));
	check(InSegmentIndex != INDEX_NONE);
	check(InSegmentIndex >= 0);
	check(InSegmentIndex < SplineComp->GetNumberOfSplineSegments());

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	// Compute local pos
	FVector LocalPos = SplineComp->GetComponentTransform().InverseTransformPosition(InWorldPos);

	FVector SplinePos, SplineTangent;
	float SplineParam = FindNearest(LocalPos, InSegmentIndex, SplinePos, SplineTangent);
	float t = SplineParam - static_cast<float>(InSegmentIndex);

	if (bDuplicatingSplineKey)
	{
		DuplicateCacheSplitSegmentParam = t;
	}

	int32 SegmentBeginIndex = InSegmentIndex;
	int32 SegmentSplitIndex = InSegmentIndex + 1;
	int32 SegmentEndIndex = SegmentSplitIndex;
	if (SplineComp->IsClosedLoop() && SegmentEndIndex >= SplineComp->GetNumberOfSplinePoints())
	{
		SegmentEndIndex = 0;
	}

	FInterpCurveVector& SplinePosition = SplineComp->GetSplinePointsPosition();
	FInterpCurveQuat& SplineRotation = SplineComp->GetSplinePointsRotation();
	FInterpCurveVector& SplineScale = SplineComp->GetSplinePointsScale();
	USplineMetadata* SplineMetadata = SplineComp->GetSplinePointsMetadata();
	
	// Set adjacent points to CurveAuto so their tangents adjust automatically as new point moves.
	if (SplinePosition.Points[SegmentBeginIndex].InterpMode == CIM_CurveUser)
	{
		SplinePosition.Points[SegmentBeginIndex].InterpMode = CIM_CurveAuto;
	}
	if (SplinePosition.Points[SegmentEndIndex].InterpMode == CIM_CurveUser)
	{
		SplinePosition.Points[SegmentEndIndex].InterpMode = CIM_CurveAuto;
	}

	// Compute interpolated scale
	FVector NewScale;
	FInterpCurvePoint<FVector>& PrevScale = SplineScale.Points[SegmentBeginIndex];
	FInterpCurvePoint<FVector>& NextScale = SplineScale.Points[SegmentEndIndex];
	NewScale = FMath::LerpStable(PrevScale.OutVal, NextScale.OutVal, t);

	// Compute interpolated rot
	FQuat NewRot;
	FInterpCurvePoint<FQuat>& PrevRot = SplineRotation.Points[SegmentBeginIndex];
	FInterpCurvePoint<FQuat>& NextRot = SplineRotation.Points[SegmentEndIndex];
	NewRot = FMath::Lerp(PrevRot.OutVal, NextRot.OutVal, t);

	// Determine which index to use when copying interp mode
	int32 SourceIndex = bCopyFromSegmentBeginIndex ? SegmentBeginIndex : SegmentEndIndex;

	FInterpCurvePoint<FVector> NewPoint(
		(float)SegmentSplitIndex,
		SplinePos,
		FVector::ZeroVector,
		FVector::ZeroVector,
		SplinePosition.Points[SourceIndex].InterpMode);

	FInterpCurvePoint<FQuat> NewRotPoint(
		(float)SegmentSplitIndex,
		NewRot,
		FQuat::Identity,
		FQuat::Identity,
		CIM_CurveAuto);

	FInterpCurvePoint<FVector> NewScalePoint(
		(float)SegmentSplitIndex,
		NewScale,
		FVector::ZeroVector,
		FVector::ZeroVector,
		CIM_CurveAuto);

	if (SegmentEndIndex == 0)
	{
		// Splitting last segment of a closed-looped spline
		SplinePosition.Points.Emplace(NewPoint);
		SplineRotation.Points.Emplace(NewRotPoint);
		SplineScale.Points.Emplace(NewScalePoint);
	}
	else
	{
		SplinePosition.Points.Insert(NewPoint, SegmentEndIndex);
		SplineRotation.Points.Insert(NewRotPoint, SegmentEndIndex);
		SplineScale.Points.Insert(NewScalePoint, SegmentEndIndex);
	}

	if (SplineMetadata)
	{
		SplineMetadata->InsertPoint(SegmentEndIndex, t, SplineComp->IsClosedLoop());
	}

	// Adjust input keys of subsequent points
	for (int Index = SegmentSplitIndex + 1; Index < SplineComp->GetNumberOfSplinePoints(); Index++)
	{
		SplinePosition.Points[Index].InVal += 1.0f;
		SplineRotation.Points[Index].InVal += 1.0f;
		SplineScale.Points[Index].InVal += 1.0f;
	}

	// Set selection to new key
	ChangeSelectionState(SegmentSplitIndex, false);

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;

	NotifyPropertyModified(SplineComp, SplineCurvesProperty);
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->PostEditMove(true);
	}


	GEditor->RedrawLevelEditingViewports(true);
}

void FSplineComponentVisualizer::UpdateSplitSegment(const FVector& InDrag)
{
	const FScopedTransaction Transaction(LOCTEXT("UpdateSplitSegment", "Update Split Segment"));

	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);
	check(SelectionState);
	const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
	int32 LastKeyIndexSelected = SelectionState->GetLastKeyIndexSelected();
	check(LastKeyIndexSelected != INDEX_NONE);
	check(SelectedKeys.Num() == 1);
	check(SelectedKeys.Contains(LastKeyIndexSelected));
	// LastKeyIndexSelected is the newly created point when splitting a segment with alt-drag. 
	// Check that it is an internal point, not an end point.
	check(LastKeyIndexSelected > 0);
	check(LastKeyIndexSelected < SplineComp->GetNumberOfSplineSegments());

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	int32 SegmentStartIndex = LastKeyIndexSelected - 1;
	int32 SegmentSplitIndex = LastKeyIndexSelected;
	int32 SegmentEndIndex = LastKeyIndexSelected + 1;

	// Wrap end point if on last segment of closed-looped spline
	if (SplineComp->IsClosedLoop() && SegmentEndIndex >= SplineComp->GetNumberOfSplineSegments())
	{
		SegmentEndIndex = 0;
	}

	FInterpCurveVector& SplinePosition = SplineComp->GetSplinePointsPosition();
	FInterpCurveVector& SplineScale = SplineComp->GetSplinePointsScale();
	FInterpCurveQuat& SplineRotation = SplineComp->GetSplinePointsRotation();
	USplineMetadata* SplineMetadata = SplineComp->GetSplinePointsMetadata();

	// Find key position in world space
	FInterpCurvePoint<FVector>& EditedPoint = SplinePosition.Points[SegmentSplitIndex];
	const FVector CurrentWorldPos = SplineComp->GetComponentTransform().TransformPosition(EditedPoint.OutVal);

	// Move in world space
	const FVector NewWorldPos = CurrentWorldPos + InDrag;

	// Convert back to local space
	FVector LocalPos = SplineComp->GetComponentTransform().InverseTransformPosition(NewWorldPos);

	FVector SplinePos0, SplinePos1;
	FVector SplineTangent0, SplineTangent1;
	float t = 0.0f;
	float SplineParam0 = FindNearest(LocalPos, SegmentStartIndex, SplinePos0, SplineTangent0);
	float t0 = SplineParam0 - static_cast<float>(SegmentStartIndex);
	float SplineParam1 = FindNearest(LocalPos, SegmentSplitIndex, SplinePos1, SplineTangent1);
	float t1 = SplineParam1 - static_cast<float>(SegmentSplitIndex);

	// Calculate params
	if (FVector::Distance(LocalPos, SplinePos0) < FVector::Distance(LocalPos, SplinePos1))
	{
		t = DuplicateCacheSplitSegmentParam * t0;
	}
	else
	{
		t = DuplicateCacheSplitSegmentParam + (1 - DuplicateCacheSplitSegmentParam) * t1;
	}
	DuplicateCacheSplitSegmentParam = t;

	// Update location
	EditedPoint.OutVal = LocalPos;

	// Update scale
	FInterpCurvePoint<FVector>& EditedScale = SplineScale.Points[SegmentSplitIndex];
	FInterpCurvePoint<FVector>& PrevScale = SplineScale.Points[SegmentStartIndex];
	FInterpCurvePoint<FVector>& NextScale = SplineScale.Points[SegmentEndIndex];
	EditedScale.OutVal = FMath::LerpStable(PrevScale.OutVal, NextScale.OutVal, t);

	// Update rot
	FInterpCurvePoint<FQuat>& EditedRot = SplineRotation.Points[SegmentSplitIndex];
	FInterpCurvePoint<FQuat>& PrevRot = SplineRotation.Points[SegmentStartIndex];
	FInterpCurvePoint<FQuat>& NextRot = SplineRotation.Points[SegmentEndIndex];
	EditedRot.OutVal = FMath::Lerp(PrevRot.OutVal, NextRot.OutVal, t);

	// Update metadata
	if (SplineMetadata)
	{
		SplineMetadata->UpdatePoint(SegmentSplitIndex, t, SplineComp->IsClosedLoop());
	}

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;

	// Transform the spline keys using an EPropertyChangeType::Interactive change. Later on, at the end of mouse tracking, a non-interactive change will be notified via void TrackingStopped :
	NotifyPropertyModified(SplineComp, SplineCurvesProperty, EPropertyChangeType::Interactive);

	GEditor->RedrawLevelEditingViewports(true);
}

void FSplineComponentVisualizer::AddSegment(const FVector& InWorldPos, bool bAppend)
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	int32 KeyIdx = 0;
	int32 NewKeyIdx = 0;

	FInterpCurveVector& SplinePosition = SplineComp->GetSplinePointsPosition();

	if (bAppend)
	{
		NewKeyIdx = SplinePosition.Points.Num();
		KeyIdx = NewKeyIdx - 1;
	}

	FInterpCurveQuat& SplineRotation = SplineComp->GetSplinePointsRotation();
	FInterpCurveVector& SplineScale = SplineComp->GetSplinePointsScale();
	USplineMetadata* SplineMetadata = SplineComp->GetSplinePointsMetadata();
	
	// Set adjacent point to CurveAuto so its tangent adjusts automatically as new point moves.
	if (SplinePosition.Points[KeyIdx].InterpMode == CIM_CurveUser)
	{
		SplinePosition.Points[KeyIdx].InterpMode = CIM_CurveAuto;
	}

	// Compute local pos
	FVector LocalPos = SplineComp->GetComponentTransform().InverseTransformPosition(InWorldPos);

	FInterpCurvePoint<FVector> NewPoint(
		(float)NewKeyIdx,
		LocalPos,
		FVector::ZeroVector,
		FVector::ZeroVector,
		SplinePosition.Points[KeyIdx].InterpMode);

	FInterpCurvePoint<FQuat> NewRotPoint(
		(float)NewKeyIdx,
		SplineRotation.Points[KeyIdx].OutVal,
		FQuat::Identity,
		FQuat::Identity,
		CIM_CurveAuto);

	FInterpCurvePoint<FVector> NewScalePoint(
		(float)NewKeyIdx,
		SplineScale.Points[KeyIdx].OutVal,
		FVector::ZeroVector,
		FVector::ZeroVector,
		CIM_CurveAuto);

	if (KeyIdx == 0)
	{
		SplinePosition.Points.Insert(NewPoint, KeyIdx);
		SplineRotation.Points.Insert(NewRotPoint, KeyIdx);
		SplineScale.Points.Insert(NewScalePoint, KeyIdx);
	}
	else
	{
		SplinePosition.Points.Emplace(NewPoint);
		SplineRotation.Points.Emplace(NewRotPoint);
		SplineScale.Points.Emplace(NewScalePoint);
	}

	// Adjust input keys of subsequent points
	if (!bAppend)
	{
		for (int Index = 1; Index < SplinePosition.Points.Num(); Index++)
		{
			SplinePosition.Points[Index].InVal += 1.0f;
			SplineRotation.Points[Index].InVal += 1.0f;
			SplineScale.Points[Index].InVal += 1.0f;
		}
	}

	if (SplineMetadata)
	{
		SplineMetadata->DuplicatePoint(KeyIdx);
	}

	// Set selection to key
	ChangeSelectionState(NewKeyIdx, false);

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;

	NotifyPropertyModified(SplineComp, SplineCurvesProperty);

	GEditor->RedrawLevelEditingViewports(true);
}

void FSplineComponentVisualizer::UpdateAddSegment(const FVector& InDrag)
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);
	check(SelectionState);
	const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
	int32 LastKeyIndexSelected = SelectionState->GetVerifiedLastKeyIndexSelected(SplineComp->GetNumberOfSplinePoints());
	check(SelectedKeys.Num() == 1);
	check(SelectedKeys.Contains(LastKeyIndexSelected));
	// Only work on keys at either end of a non-closed-looped spline 
	check(!SplineComp->IsClosedLoop());
	check(LastKeyIndexSelected == 0 || LastKeyIndexSelected == SplineComp->GetSplinePointsPosition().Points.Num() - 1);

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	// Move added point to new position
	FInterpCurvePoint<FVector>& AddedPoint = SplineComp->GetSplinePointsPosition().Points[LastKeyIndexSelected];
	const FVector CurrentWorldPos = SplineComp->GetComponentTransform().TransformPosition(AddedPoint.OutVal);
	const FVector NewWorldPos = CurrentWorldPos + InDrag;
	AddedPoint.OutVal = SplineComp->GetComponentTransform().InverseTransformPosition(NewWorldPos);

	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;

	// Transform the spline keys using an EPropertyChangeType::Interactive change. Later on, at the end of mouse tracking, a non-interactive change will be notified via void TrackingStopped :
	NotifyPropertyModified(SplineComp, SplineCurvesProperty, EPropertyChangeType::Interactive);

	GEditor->RedrawLevelEditingViewports(true);
}

void FSplineComponentVisualizer::ResetAllowDuplication()
{
	bAllowDuplication = true;
	bDuplicatingSplineKey = false;
	bUpdatingAddSegment = false;
	DuplicateDelay = 0;
	DuplicateDelayAccumulatedDrag = FVector::ZeroVector;
	DuplicateCacheSplitSegmentParam = 0.0f;
}

void FSplineComponentVisualizer::OnDeleteKey()
{
	const FScopedTransaction Transaction(LOCTEXT("DeleteSplinePoint", "Delete Spline Point"));

	ResetTempModes();

	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);
	check(SelectionState);
	const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
	int32 LastKeyIndexSelected = SelectionState->GetVerifiedLastKeyIndexSelected(SplineComp->GetNumberOfSplinePoints());
	check(SelectedKeys.Num() > 0);
	check(SelectedKeys.Contains(LastKeyIndexSelected));

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	// Get a sorted list of all the selected indices, highest to lowest
	TArray<int32> SelectedKeysSorted;
	for (int32 SelectedKeyIndex : SelectedKeys)
	{
		SelectedKeysSorted.Add(SelectedKeyIndex);
	}
	SelectedKeysSorted.Sort([](int32 A, int32 B) { return A > B; });

	// Delete selected keys from list, highest index first
	FInterpCurveVector& SplinePosition = SplineComp->GetSplinePointsPosition();
	FInterpCurveQuat& SplineRotation = SplineComp->GetSplinePointsRotation();
	FInterpCurveVector& SplineScale = SplineComp->GetSplinePointsScale();
	USplineMetadata* SplineMetadata = SplineComp->GetSplinePointsMetadata();
		
	for (int32 SelectedKeyIndex : SelectedKeysSorted)
	{
		if (SplineMetadata)
		{
			SplineMetadata->RemovePoint(SelectedKeyIndex);
		}
		
		SplinePosition.Points.RemoveAt(SelectedKeyIndex);
		SplineRotation.Points.RemoveAt(SelectedKeyIndex);
		SplineScale.Points.RemoveAt(SelectedKeyIndex);

		for (int Index = SelectedKeyIndex; Index < SplinePosition.Points.Num(); Index++)
		{
			SplinePosition.Points[Index].InVal -= 1.0f;
			SplineRotation.Points[Index].InVal -= 1.0f;
			SplineScale.Points[Index].InVal -= 1.0f;
		}
	}

	// Select first key
	ChangeSelectionState(0, false);
	SelectionState->Modify();
	SelectionState->ClearSelectedSegmentIndex();
	SelectionState->ClearSelectedTangentHandle();


	SplineComp->UpdateSpline();
	SplineComp->bSplineHasBeenEdited = true;

	NotifyPropertyModified(SplineComp, SplineCurvesProperty);
	if (AActor* OwnerActor = SplineComp->GetOwner())
	{
		OwnerActor->PostEditMove(true);
	}

	SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(SelectionState->GetLastKeyIndexSelected(), ESplineCoordinateSpace::World));

	GEditor->RedrawLevelEditingViewports(true);
}


bool FSplineComponentVisualizer::CanDeleteKey() const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
	int32 LastKeyIndexSelected = SelectionState->GetLastKeyIndexSelected();
	return (SplineComp != nullptr &&
			SelectedKeys.Num() > 0 &&
			SelectedKeys.Num() != SplineComp->SplineCurves.Position.Points.Num() &&
			LastKeyIndexSelected != INDEX_NONE);
}


bool FSplineComponentVisualizer::IsKeySelectionValid() const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
	int32 LastKeyIndexSelected = SelectionState->GetLastKeyIndexSelected();
	return (SplineComp != nullptr &&
			SelectedKeys.Num() > 0 &&
			LastKeyIndexSelected != INDEX_NONE);
}

void FSplineComponentVisualizer::OnLockAxis(EAxis::Type InAxis)
{
	const FScopedTransaction Transaction(LOCTEXT("LockAxis", "Lock Axis"));

	ResetTempModes();

	AddKeyLockedAxis = InAxis;
}

bool FSplineComponentVisualizer::IsLockAxisSet(EAxis::Type Index) const
{
	return (Index == AddKeyLockedAxis);
}

void FSplineComponentVisualizer::OnResetToAutomaticTangent(EInterpCurveMode Mode)
{
	const FScopedTransaction Transaction(LOCTEXT("ResetToAutomaticTangent", "Reset to Automatic Tangent"));

	ResetTempModes();

	USplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr)
	{
		check(SelectionState);

		SplineComp->Modify();
		if (AActor* Owner = SplineComp->GetOwner())
		{
			Owner->Modify();
		}

		const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
		for (int32 SelectedKeyIndex : SelectedKeys)
		{
			auto& Point = SplineComp->SplineCurves.Position.Points[SelectedKeyIndex];
			if (Point.IsCurveKey())
			{
				Point.InterpMode = Mode;
			}
		}

		SplineComp->UpdateSpline();
		SplineComp->bSplineHasBeenEdited = true;

		NotifyPropertyModified(SplineComp, SplineCurvesProperty);
		if (AActor* OwnerActor = SplineComp->GetOwner())
		{
			OwnerActor->PostEditMove(true);
		}

		SelectionState->Modify();
		SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(SelectionState->GetLastKeyIndexSelected(), ESplineCoordinateSpace::World));
	}
}


bool FSplineComponentVisualizer::CanResetToAutomaticTangent(EInterpCurveMode Mode) const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr && SelectionState != nullptr && SelectionState->GetLastKeyIndexSelected() != INDEX_NONE)
	{
		const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
		for (int32 SelectedKeyIndex : SelectedKeys)
		{
			const auto& Point = SplineComp->SplineCurves.Position.Points[SelectedKeyIndex];
			if (Point.IsCurveKey() && Point.InterpMode != Mode)
			{
				return true;
			}
		}
	}

	return false;
}


void FSplineComponentVisualizer::OnSetKeyType(EInterpCurveMode Mode)
{
	const FScopedTransaction Transaction(LOCTEXT("SetSplinePointType", "Set Spline Point Type"));

	ResetTempModes();

	USplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr)
	{
		check(SelectionState);

		SplineComp->Modify();
		if (AActor* Owner = SplineComp->GetOwner())
		{
			Owner->Modify();
		}

		const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
		for (int32 SelectedKeyIndex : SelectedKeys)
		{
			check(SelectedKeyIndex >= 0);
			check(SelectedKeyIndex < SplineComp->GetNumberOfSplinePoints());
			SplineComp->SplineCurves.Position.Points[SelectedKeyIndex].InterpMode = Mode;
		}

		SplineComp->UpdateSpline();
		SplineComp->bSplineHasBeenEdited = true;

		NotifyPropertyModified(SplineComp, SplineCurvesProperty);
		if (AActor* OwnerActor = SplineComp->GetOwner())
		{
			OwnerActor->PostEditMove(true);
		}

		SelectionState->Modify();
		SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(SelectionState->GetLastKeyIndexSelected(), ESplineCoordinateSpace::World));
	}
}


bool FSplineComponentVisualizer::IsKeyTypeSet(EInterpCurveMode Mode) const
{
	if (IsKeySelectionValid())
	{
		USplineComponent* SplineComp = GetEditedSplineComponent();
		check(SplineComp != nullptr);
		check(SelectionState);

		const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
		for (int32 SelectedKeyIndex : SelectedKeys)
		{
			check(SelectedKeyIndex >= 0);
			check(SelectedKeyIndex < SplineComp->GetNumberOfSplinePoints());
			const auto& SelectedPoint = SplineComp->SplineCurves.Position.Points[SelectedKeyIndex];
			if ((Mode == CIM_CurveAuto && SelectedPoint.IsCurveKey()) || SelectedPoint.InterpMode == Mode)
			{
				return true;
			}
		}
	}

	return false;
}


void FSplineComponentVisualizer::OnSetVisualizeRollAndScale()
{
	ResetTempModes();

	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	SplineComp->bShouldVisualizeScale = !SplineComp->bShouldVisualizeScale;

	NotifyPropertyModified(SplineComp, FindFProperty<FProperty>(USplineComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(USplineComponent, bShouldVisualizeScale)));

	GEditor->RedrawLevelEditingViewports(true);
}


bool FSplineComponentVisualizer::IsVisualizingRollAndScale() const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	
	return SplineComp ? SplineComp->bShouldVisualizeScale : false;
}


void FSplineComponentVisualizer::OnSetDiscontinuousSpline()
{
	ResetTempModes();

	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	SplineComp->bAllowDiscontinuousSpline = !SplineComp->bAllowDiscontinuousSpline;

	// If not allowed discontinuous splines, set all ArriveTangents to match LeaveTangents
	if (!SplineComp->bAllowDiscontinuousSpline)
	{
		for (int Index = 0; Index < SplineComp->SplineCurves.Position.Points.Num(); Index++)
		{
			SplineComp->SplineCurves.Position.Points[Index].ArriveTangent = SplineComp->SplineCurves.Position.Points[Index].LeaveTangent;
		}
	}

	TArray<FProperty*> Properties;
	Properties.Add(SplineCurvesProperty);
	Properties.Add(FindFProperty<FProperty>(USplineComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(USplineComponent, bAllowDiscontinuousSpline)));
	NotifyPropertiesModified(SplineComp, Properties);

	GEditor->RedrawLevelEditingViewports(true);
}


bool FSplineComponentVisualizer::IsDiscontinuousSpline() const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();

	return SplineComp ? SplineComp->bAllowDiscontinuousSpline : false;
}


void FSplineComponentVisualizer::OnToggleClosedLoop()
{
	const FScopedTransaction Transaction(LOCTEXT("ToggleClosedLoop", "Toggle Closed Loop"));

	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);

	SplineComp->Modify();
	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	SplineComp->SetClosedLoop(!SplineComp->IsClosedLoop());

	TArray<FProperty*> Properties;
	Properties.Add(SplineCurvesProperty);
	Properties.Add(FindFProperty<FProperty>(USplineComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(USplineComponent, bClosedLoop)));
	NotifyPropertiesModified(SplineComp, Properties);

	GEditor->RedrawLevelEditingViewports(true);
}

bool FSplineComponentVisualizer::IsClosedLoop() const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	return SplineComp ? SplineComp->IsClosedLoop() : false;
}

void FSplineComponentVisualizer::OnResetToDefault()
{
	const FScopedTransaction Transaction(LOCTEXT("ResetToDefault", "Reset to Default"));

	ResetTempModes();

	USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp != nullptr);
	check(SelectionState);

	SplineComp->Modify();
	SplineComp->ResetToDefault();

	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->Modify();
	}

	SplineComp->bSplineHasBeenEdited = false;

	// Select first key
	ChangeSelectionState(0, false);
	SelectionState->Modify();
	SelectionState->ClearSelectedSegmentIndex();
	SelectionState->ClearSelectedTangentHandle();

	if (AActor* Owner = SplineComp->GetOwner())
	{
		Owner->PostEditMove(true);
	}

	GEditor->RedrawLevelEditingViewports(true);
}


bool FSplineComponentVisualizer::CanResetToDefault() const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	if(SplineComp != nullptr)
    {
        return SplineComp->SplineCurves != CastChecked<USplineComponent>(SplineComp->GetArchetype())->SplineCurves;
    }
    else
    {
        return false;
    }
}

bool FSplineComponentVisualizer::HandleSelectFirstLastSplinePoint(USplineComponent* InSplineComponent, bool bFirstPoint)
{
	const FScopedTransaction Transaction(LOCTEXT("SelectFirstSplinePoint", "Select First Spline Point"));

	check(InSplineComponent);
	check(SelectionState);

	bool bResetEditedSplineComponent = false;
	if (GetEditedSplineComponent() != InSplineComponent)
	{
		SetEditedSplineComponent(InSplineComponent);
		bResetEditedSplineComponent = true;
	}

	OnSelectFirstLastSplinePoint(bFirstPoint);

	return bResetEditedSplineComponent;
}

bool FSplineComponentVisualizer::HandleSelectAllSplinePoints(USplineComponent* InSplineComponent)
{
	const FScopedTransaction Transaction(LOCTEXT("SelectAllSplinePoints", "Select All Spline Points"));

	check(InSplineComponent);
	check(SelectionState);

	bool bResetEditedSplineComponent = false;
	if (GetEditedSplineComponent() != InSplineComponent)
	{
		SetEditedSplineComponent(InSplineComponent);
		bResetEditedSplineComponent = true;
	}

	OnSelectAllSplinePoints();

	return bResetEditedSplineComponent;
}

void FSplineComponentVisualizer::OnSelectFirstLastSplinePoint(bool bFirstPoint)
{
	const FScopedTransaction Transaction(LOCTEXT("SelectFirstSplinePoint", "Select First Spline Point"));

	ResetTempModes();

	if (USplineComponent* SplineComp = GetEditedSplineComponent())
	{
		const int32 NumSplinePoints = SplineComp->GetNumberOfSplinePoints();
		if (NumSplinePoints > 0)
		{
			SelectSplinePoint(bFirstPoint ? 0 : NumSplinePoints - 1, false);
		}
	}
}

void FSplineComponentVisualizer::OnSelectPrevNextSplinePoint(bool bNextPoint, bool bAddToSelection)
{
	const FScopedTransaction Transaction(LOCTEXT("SelectSplinePoint", "Select Spline Point"));

	ResetTempModes();

	if (USplineComponent* SplineComp = GetEditedSplineComponent())
	{
		if (AreKeysSelected())
		{
			const int32 NumSplinePoints = SplineComp->GetNumberOfSplinePoints();
			check(SelectionState);
			const int32 LastKeyIndexSelected = SelectionState->GetVerifiedLastKeyIndexSelected(NumSplinePoints);

			int32 SelectIndex = INDEX_NONE;
			const int32 Step = bNextPoint ? 1 : -1;
			auto WrapKeys = [&NumSplinePoints](int32 Key) { return (Key >= NumSplinePoints ? 0 : (Key < 0 ? NumSplinePoints - 1 : Key)); };

			for (int32 Index = WrapKeys(LastKeyIndexSelected + Step); Index != LastKeyIndexSelected; Index = WrapKeys(Index + Step))
			{
				if (!bAddToSelection || !SelectionState->IsSplinePointSelected(Index))
				{
					SelectIndex = Index;
					break;
				}
			}

			if (SelectIndex != INDEX_NONE)
			{
				if (!bAddToSelection)
				{	
					SelectSplinePoint(SelectIndex, false);
				}
				else
				{
					// To do: change the following to use SelectSplinePoint(), with a parameter bClearMetadataSelectionState set to false.
					check(SelectionState);
					SelectionState->Modify();

					TSet<int32>& SelectedKeys = SelectionState->ModifySelectedKeys();
					SelectedKeys.Add(SelectIndex);

					SelectionState->SetLastKeyIndexSelected(SelectIndex);
					SelectionState->ClearSelectedSegmentIndex();
					SelectionState->ClearSelectedTangentHandle();
					SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(SelectionState->GetLastKeyIndexSelected(), ESplineCoordinateSpace::World));

					GEditor->RedrawLevelEditingViewports(true);
				}
			}
		}
	}
}

void FSplineComponentVisualizer::SetCachedRotation(const FQuat& NewRotation)
{
	check(SelectionState);
	SelectionState->Modify();
	SelectionState->SetCachedRotation(NewRotation);
}

void FSplineComponentVisualizer::SelectSplinePoint(int32 SelectIndex, bool bAddToSelection)
{
	const FScopedTransaction Transaction(LOCTEXT("SelectSplinePoint", "Select Spline Point"));

	ResetTempModes();

	check(SelectionState);

	if (USplineComponent* SplineComp = GetEditedSplineComponent())
	{
		if (SelectIndex != INDEX_NONE)
		{
			SelectionState->Modify();

			ChangeSelectionState(SelectIndex, bAddToSelection);

			SelectionState->ClearSelectedSegmentIndex();
			SelectionState->ClearSelectedTangentHandle();
			SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(SelectionState->GetLastKeyIndexSelected(), ESplineCoordinateSpace::World));

			GEditor->RedrawLevelEditingViewports(true);
		}
	}
}

void FSplineComponentVisualizer::OnSelectAllSplinePoints()
{
	const FScopedTransaction Transaction(LOCTEXT("SelectAllSplinePoints", "Select All Spline Points"));

	ResetTempModes();

	if (USplineComponent* SplineComp = GetEditedSplineComponent())
	{
		const FInterpCurveVector& SplineInfo = SplineComp->GetSplinePointsPosition();
		int32 NumPoints = SplineInfo.Points.Num();

		check(SelectionState);
		SelectionState->Modify();

		TSet<int32>& SelectedKeys = SelectionState->ModifySelectedKeys();
		SelectedKeys.Empty();

		// Spline control point selection always uses transparent box selection.
		for (int32 KeyIdx = 0; KeyIdx < NumPoints; KeyIdx++)
		{
			SelectedKeys.Add(KeyIdx);
		}

		SelectionState->SetLastKeyIndexSelected(NumPoints - 1);
		SelectionState->ClearSelectedSegmentIndex();
		SelectionState->ClearSelectedTangentHandle();
		SelectionState->SetCachedRotation(SplineComp->GetQuaternionAtSplinePoint(SelectionState->GetLastKeyIndexSelected(), ESplineCoordinateSpace::World));

		GEditor->RedrawLevelEditingViewports(true);
	}
}

bool FSplineComponentVisualizer::CanSelectSplinePoints() const
{
	USplineComponent* SplineComp = GetEditedSplineComponent();
	return (SplineComp != nullptr);
}

TSharedPtr<SWidget> FSplineComponentVisualizer::GenerateContextMenu() const
{
	FMenuBuilder MenuBuilder(true, SplineComponentVisualizerActions);
	
	GenerateContextMenuSections(MenuBuilder);

	TSharedPtr<SWidget> MenuWidget = MenuBuilder.MakeWidget();
	return MenuWidget;
}

void FSplineComponentVisualizer::GenerateContextMenuSections(FMenuBuilder& InMenuBuilder) const
{
	InMenuBuilder.BeginSection("SplinePointEdit", LOCTEXT("SplinePoint", "Spline Point"));

	USplineComponent* SplineComp = GetEditedSplineComponent();
	if (SplineComp != nullptr)
	{
		check(SelectionState);

		if (SelectionState->GetSelectedSegmentIndex() != INDEX_NONE)
		{
			InMenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().AddKey);
		}
		else if (SelectionState->GetLastKeyIndexSelected() != INDEX_NONE)
		{
			InMenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().DeleteKey);
			InMenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().DuplicateKey);

			InMenuBuilder.AddSubMenu(
				LOCTEXT("SelectSplinePoints", "Select Spline Points"),
				LOCTEXT("SelectSplinePointsTooltip", "Select spline point."),
				FNewMenuDelegate::CreateSP(this, &FSplineComponentVisualizer::GenerateSelectSplinePointsSubMenu));

			InMenuBuilder.AddSubMenu(
				LOCTEXT("SplinePointType", "Spline Point Type"),
				LOCTEXT("SplinePointTypeTooltip", "Define the type of the spline point."),
				FNewMenuDelegate::CreateSP(this, &FSplineComponentVisualizer::GenerateSplinePointTypeSubMenu));

			// Only add the Automatic Tangents submenu if any of the keys is a curve type
			const TSet<int32>& SelectedKeys = SelectionState->GetSelectedKeys();
			for (int32 SelectedKeyIndex : SelectedKeys)
			{
				check(SelectedKeyIndex >= 0);
				check(SelectedKeyIndex < SplineComp->GetNumberOfSplinePoints());
				const auto& Point = SplineComp->SplineCurves.Position.Points[SelectedKeyIndex];
				if (Point.IsCurveKey())
				{
					InMenuBuilder.AddSubMenu(
						LOCTEXT("ResetToAutomaticTangent", "Reset to Automatic Tangent"),
						LOCTEXT("ResetToAutomaticTangentTooltip", "Reset the spline point tangent to an automatically generated value."),
						FNewMenuDelegate::CreateSP(this, &FSplineComponentVisualizer::GenerateTangentTypeSubMenu));
					break;
				}
			}

			InMenuBuilder.AddMenuEntry(
				LOCTEXT("SplineGenerate", "Spline Generation Panel"),
				LOCTEXT("SplineGenerateTooltip", "Opens up a spline generation panel to easily create basic shapes with splines"),
				FSlateIcon(),
				FUIAction( 
					FExecuteAction::CreateSP(const_cast<FSplineComponentVisualizer*>(this), &FSplineComponentVisualizer::CreateSplineGeneratorPanel),
					FCanExecuteAction::CreateLambda([] { return true; })
				)
			);
		}
	}
	InMenuBuilder.EndSection();

	InMenuBuilder.BeginSection("Transform");
	{
		InMenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().FocusViewportToSelection);

		InMenuBuilder.AddSubMenu(
			LOCTEXT("SplineSnapAlign", "Snap/Align"),
			LOCTEXT("SplineSnapAlignTooltip", "Snap align options."),
			FNewMenuDelegate::CreateSP(this, &FSplineComponentVisualizer::GenerateSnapAlignSubMenu));

		/* temporarily disabled
		InMenuBuilder.AddSubMenu(
			LOCTEXT("LockAxis", "Lock Axis"),
			LOCTEXT("LockAxisTooltip", "Axis to lock when adding new spline points."),
			FNewMenuDelegate::CreateSP(this, &FSplineComponentVisualizer::GenerateLockAxisSubMenu));
			*/
	}
	InMenuBuilder.EndSection();

	InMenuBuilder.BeginSection("Spline", LOCTEXT("Spline", "Spline"));
	{
		InMenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().ToggleClosedLoop);
		InMenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().ResetToDefault);
	}
	InMenuBuilder.EndSection();

	InMenuBuilder.BeginSection("Visualization", LOCTEXT("Visualization", "Visualization"));
	{
		InMenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().VisualizeRollAndScale);
		InMenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().DiscontinuousSpline);
	}
	InMenuBuilder.EndSection();
}

void FSplineComponentVisualizer::GenerateSelectSplinePointsSubMenu(FMenuBuilder& MenuBuilder) const
{
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SelectAll);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SelectPrevSplinePoint);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SelectNextSplinePoint);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().AddPrevSplinePoint);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().AddNextSplinePoint);
}
void FSplineComponentVisualizer::GenerateSplinePointTypeSubMenu(FMenuBuilder& MenuBuilder) const
{
	const USplineComponent* SplineComp = GetEditedSplineComponent();
	check(SplineComp);
	const TArray<ESplinePointType::Type> EnabledSplinePointTypes = SplineComp->GetEnabledSplinePointTypes();
	if (EnabledSplinePointTypes.Contains(ESplinePointType::Curve))
	{
		MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SetKeyToCurve);
	}
	if (EnabledSplinePointTypes.Contains(ESplinePointType::Linear))
	{
		MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SetKeyToLinear);
	}
	if (EnabledSplinePointTypes.Contains(ESplinePointType::Constant))
	{
		MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SetKeyToConstant);
	}
}

void FSplineComponentVisualizer::GenerateTangentTypeSubMenu(FMenuBuilder& MenuBuilder) const
{
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().ResetToUnclampedTangent);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().ResetToClampedTangent);
}

void FSplineComponentVisualizer::GenerateSnapAlignSubMenu(FMenuBuilder& MenuBuilder) const
{
	MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().SnapToFloor);
	MenuBuilder.AddMenuEntry(FLevelEditorCommands::Get().AlignToFloor);
	MenuBuilder.AddSeparator();
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SnapKeyToNearestSplinePoint);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().AlignKeyToNearestSplinePoint);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().AlignKeyPerpendicularToNearestSplinePoint);
	MenuBuilder.AddSeparator();
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SnapKeyToActor);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().AlignKeyToActor);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().AlignKeyPerpendicularToActor);
	MenuBuilder.AddSeparator();
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SnapAllToSelectedX);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SnapAllToSelectedY);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SnapAllToSelectedZ);
	MenuBuilder.AddSeparator();
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SnapToLastSelectedX);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SnapToLastSelectedY);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SnapToLastSelectedZ);
	MenuBuilder.AddSeparator();
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().StraightenToNext);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().StraightenToPrevious);
	MenuBuilder.AddSeparator();
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().ToggleSnapTangentAdjustments);
}

void FSplineComponentVisualizer::GenerateLockAxisSubMenu(FMenuBuilder& MenuBuilder) const
{
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SetLockedAxisNone);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SetLockedAxisX);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SetLockedAxisY);
	MenuBuilder.AddMenuEntry(FSplineComponentVisualizerCommands::Get().SetLockedAxisZ);
}

void FSplineComponentVisualizer::CreateSplineGeneratorPanel()
{
	SAssignNew(SplineGeneratorPanel, SSplineGeneratorPanel, SharedThis(this));

	TSharedPtr<SWindow> ExistingWindow = WeakExistingWindow.Pin();
	if (!ExistingWindow.IsValid())
	{
		ExistingWindow = SNew(SWindow)
			.ScreenPosition(FSlateApplication::Get().GetCursorPos())
			.Title(LOCTEXT("SplineGenerationPanelTitle", "Spline Generation"))
			.SizingRule(ESizingRule::Autosized)
			.AutoCenter(EAutoCenter::None)
			.SupportsMaximize(false)
			.SupportsMinimize(false);

		ExistingWindow->SetOnWindowClosed(FOnWindowClosed::CreateSP(SplineGeneratorPanel.ToSharedRef(), &SSplineGeneratorPanel::OnWindowClosed));

		TSharedPtr<SWindow> RootWindow = FSlateApplication::Get().GetActiveTopLevelWindow();

		if (RootWindow.IsValid())
		{
			FSlateApplication::Get().AddWindowAsNativeChild(ExistingWindow.ToSharedRef(), RootWindow.ToSharedRef());
		}
		else
		{
			FSlateApplication::Get().AddWindow(ExistingWindow.ToSharedRef());
		}

		ExistingWindow->BringToFront();
		WeakExistingWindow = ExistingWindow;
	}
	else
	{
		ExistingWindow->BringToFront();
	}
	ExistingWindow->SetContent(SplineGeneratorPanel.ToSharedRef());
}

void FSplineComponentVisualizer::OnDeselectedInEditor(TObjectPtr<USplineComponent> SplineComponent)
{
	if (DeselectedInEditorDelegateHandle.IsValid() && SplineComponent)
	{
		SplineComponent->OnDeselectedInEditor.Remove(DeselectedInEditorDelegateHandle);
	}
	DeselectedInEditorDelegateHandle.Reset();
	EndEditing();
}


#undef LOCTEXT_NAMESPACE
