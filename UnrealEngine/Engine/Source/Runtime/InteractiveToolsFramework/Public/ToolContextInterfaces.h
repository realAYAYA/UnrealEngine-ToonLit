// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComponentSourceInterfaces.h"
#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "Elements/Framework/TypedElementSelectionSet.h" // For deprecated FToolBuilderState::TypedElementSelectionSet member
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector.h"
#include "Misc/EnumClassFlags.h"
#include "Templates/UniquePtr.h"
#include "UObject/Interface.h"
#include "UObject/ObjectMacros.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

// predeclarations so we don't have to include these in all tools
class AActor;
class FPrimitiveDrawInterface;
class FSceneView;
class FText;
class FToolCommandChange;
class FViewport;
class UActorComponent;
class UInteractiveGizmoManager;
class UInteractiveToolManager;
class UMaterialInterface;
class UObject;
class UToolTargetManager;
class UTypedElementSelectionSet;

class UWorld;
struct FMeshDescription;
struct FTypedElementHandle;

/**
 * FToolBuilderState is a bucket of state information that a ToolBuilder might need
 * to construct a Tool. This information comes from a level above the Tools framework,
 * and depends on the context we are in (Editor vs Runtime, for example).
 */
struct INTERACTIVETOOLSFRAMEWORK_API FToolBuilderState
{
	/** The current UWorld */
	UWorld* World = nullptr;
	/** The current ToolManager */
	UInteractiveToolManager* ToolManager = nullptr;
	/** The current TargetManager */
	UToolTargetManager* TargetManager = nullptr;
	/** The current GizmoManager */
	UInteractiveGizmoManager* GizmoManager = nullptr;

	/** Current selected Actors. May be empty or nullptr. */
	TArray<AActor*> SelectedActors;
	/** Current selected Components. May be empty or nullptr. */
	TArray<UActorComponent*> SelectedComponents;

	UE_DEPRECATED(5.1, "This has moved to a context object. See IAssetEditorContextInterface")
	TWeakObjectPtr<UTypedElementSelectionSet> TypedElementSelectionSet;
};


/**
 * FViewCameraState is a bucket of state information that a Tool might
 * need to implement interactions that depend on the current scene view.
 */
struct INTERACTIVETOOLSFRAMEWORK_API FViewCameraState
{
	/** Current camera/head position */
	FVector Position;
	/** Current camera/head orientation */
	FQuat Orientation;
	/** Current Horizontal Field of View Angle in Degrees. Only relevant if bIsOrthographic is false. */
	float HorizontalFOVDegrees;
	/** Current width of viewport in world space coordinates. Only valid if bIsOrthographic is true. */
	float OrthoWorldCoordinateWidth;
	/** Current Aspect Ratio */
	float AspectRatio;
	/** Is current view an orthographic view */
	bool bIsOrthographic;
	/** Is current view a VR view */
	bool bIsVR;

	/** @return "right"/horizontal direction in camera plane */
	FVector Right() const { return Orientation.GetAxisY(); }
	/** @return "up"/vertical direction in camera plane */
	FVector Up() const { return Orientation.GetAxisZ(); }
	/** @return forward camera direction */
	FVector Forward() const { return Orientation.GetAxisX(); }

	/** @return scaling factor that should be applied to PDI thickness/size */
	float GetPDIScalingFactor() const { return HorizontalFOVDegrees / 90.0f; }

	/** @return FOV normalization factor that should be applied when comparing angles */
	float GetFOVAngleNormalizationFactor() const { return HorizontalFOVDegrees / 90.0f; }

};



/** Types of standard materials that Tools may request from Context */
UENUM()
enum class EStandardToolContextMaterials
{
	/** White material that displays vertex colors set on mesh */
	VertexColorMaterial = 1
};


/** Types of coordinate systems that a Tool/Gizmo might use */
UENUM()
enum class EToolContextCoordinateSystem
{
	/** World space coordinate system */
	World = 0,
	/** Local coordinate system */
	Local = 1
};

/**
 * Snapping configuration settings/state
 */
struct INTERACTIVETOOLSFRAMEWORK_API FToolContextSnappingConfiguration
{
	/** Specify whether position snapping should be applied */
	bool bEnablePositionGridSnapping = false;
	/** Specify position grid spacing or cell dimensions */
	FVector PositionGridDimensions = FVector::Zero();

	/** Specify whether rotation snapping should be applied */
	bool bEnableRotationGridSnapping = false;
	/** Specify rotation snapping step size */
	FRotator RotationGridAngles = FRotator::ZeroRotator;
};


/**
 * Users of the Tools Framework need to implement IToolsContextQueriesAPI to provide
 * access to scene state information like the current UWorld, active USelections, etc.
 */
class IToolsContextQueriesAPI
{
public:
	virtual ~IToolsContextQueriesAPI() {}

	/**
	 * @return the UWorld currently being targetted by the ToolsContext, default location for new Actors/etc
	 */
	virtual UWorld* GetCurrentEditingWorld() const = 0;

	/**
	 * Collect up current-selection information for the current scene state (ie what is selected in Editor, etc)
	 * @param StateOut this structure is populated with available state information
	 */
	virtual void GetCurrentSelectionState(FToolBuilderState& StateOut) const = 0;

	/**
	 * Request information about current view state
	 * @param StateOut this structure is populated with available state information
	 */
	virtual void GetCurrentViewState(FViewCameraState& StateOut) const = 0;

	/**
	 * Request current external coordinate-system setting
	 */
	virtual EToolContextCoordinateSystem GetCurrentCoordinateSystem() const = 0;	

	/**
	 * Request current external snapping settings
	 */
	virtual FToolContextSnappingConfiguration GetCurrentSnappingSettings() const = 0;

	/**
	 * Many tools need standard types of materials that the user should provide (eg a vertex-color material, etc)
	 * @param MaterialType the type of material being requested
	 * @return Instance of material to use for this purpose
	 */
	virtual UMaterialInterface* GetStandardMaterial(EStandardToolContextMaterials MaterialType) const = 0;

	/**
	 * @returns the last valid viewport that was hovered, or nullptr.
	 */
	virtual FViewport* GetHoveredViewport() const = 0;

	/**
	 * @returns the last valid viewport that received some input, or nullptr.
	 */
	virtual FViewport* GetFocusedViewport() const = 0;
};




/** Level of severity of messages emitted by Tool framework */
UENUM()
enum class EToolMessageLevel
{
	/** Development message goes into development log */
	Internal = 0,
	/** User message should appear in user-facing log */
	UserMessage = 1,
	/** Notification message should be shown in a non-modal notification window */
	UserNotification = 2,
	/** Warning message should be shown in a non-modal notification window with panache */
	UserWarning = 3,
	/** Error message should be shown in a modal notification window */
	UserError = 4
};


/** Type of change we want to apply to a selection */
UENUM()
enum class ESelectedObjectsModificationType
{
	Replace = 0,
	Add = 1,
	Remove = 2,
	Clear = 3
};


/** Represents a change to a set of selected Actors and Components */
struct FSelectedOjectsChangeList
{
	/** How should this list be interpreted in the context of a larger selection set */
	ESelectedObjectsModificationType ModificationType;
	/** List of Actors */
	TArray<AActor*> Actors;
	/** List of Componets */
	TArray<UActorComponent*> Components;
};


/**
 * Users of the Tools Framework need to implement IToolsContextTransactionsAPI so that
 * the Tools have the ability to create Transactions and emit Changes. Note that this is
 * technically optional, but that undo/redo won't be supported without it.
 */
class IToolsContextTransactionsAPI
{
public:
	virtual ~IToolsContextTransactionsAPI() {}

	/**
	 * Request that context display message information.
	 * @param Message text of message
	 * @param Level severity level of message
	 */
	virtual void DisplayMessage(const FText& Message, EToolMessageLevel Level) = 0;

	/** 
	 * Forward an invalidation request from Tools framework, to cause repaint/etc. 
	 * This is not always necessary but in some situations (eg in Non-Realtime mode in Editor)
	 * a redraw will not happen every frame. 
	 * See UInputRouter for options to enable auto-invalidation.
	 */
	virtual void PostInvalidation() = 0;
	
	/**
	 * Begin a Transaction, whatever this means in the current Context. For example in the
	 * Editor it means open a GEditor Transaction. You must call EndUndoTransaction() after calling this.
	 * @param Description text description of the transaction that could be shown to user
	 */
	virtual void BeginUndoTransaction(const FText& Description) = 0;

	/**
	 * Complete the Transaction. Assumption is that Begin/End are called in pairs.
	 */
	virtual void EndUndoTransaction() = 0;

	/**
	 * Insert an FChange into the transaction history in the current Context. 
	 * This cannot be called between Begin/EndUndoTransaction, the FChange should be 
	 * automatically inserted into a Transaction.
	 * @param TargetObject The UObject this Change is applied to
	 * @param Change The Change implementation
	 * @param Description text description of the transaction that could be shown to user
	 */
	virtual void AppendChange(UObject* TargetObject, TUniquePtr<FToolCommandChange> Change, const FText& Description) = 0;



	/**
	 * Request a modification to the current selected objects
	 * @param SelectionChange desired modification to current selection
	 * @return true if the selection change could be applied
	 */
	virtual bool RequestSelectionChange(const FSelectedOjectsChangeList& SelectionChange) = 0;

};

UENUM()
enum class EViewInteractionState {
	None = 0,
	Hovered = 1,
	Focused = 2
};
ENUM_CLASS_FLAGS(EViewInteractionState);

/**
 * Users of the Tools Framework need to implement IToolsContextRenderAPI to allow
 * Tools, Indicators, and Gizmos to make low-level rendering calls for things like line drawing.
 * This API will be passed to eg UInteractiveTool::Render(), so access is only provided when it
 * makes sense to call the functions
 */
class IToolsContextRenderAPI
{
public:
	virtual ~IToolsContextRenderAPI() {}

	/** @return Current PDI */
	virtual FPrimitiveDrawInterface* GetPrimitiveDrawInterface() = 0;

	/** @return Current FSceneView */
	virtual const FSceneView* GetSceneView() = 0;

	/** @return Current Camera State for this Render API */
	virtual FViewCameraState GetCameraState() = 0;

	/** @return Current interaction state of the view to render */
	virtual EViewInteractionState GetViewInteractionState() = 0;
};

