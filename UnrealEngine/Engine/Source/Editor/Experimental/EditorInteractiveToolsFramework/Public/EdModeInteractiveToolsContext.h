// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/World.h"
#include "HAL/Platform.h"
#include "InputCoreTypes.h"
#include "InputState.h"
#include "InteractiveTool.h"
#include "InteractiveToolManager.h"
#include "InteractiveToolsContext.h"
#include "Math/Ray.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Optional.h"
#include "Templates/SharedPointer.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "EdModeInteractiveToolsContext.generated.h"

class FCanvas;
class FEdMode;
class FEditorModeTools;
class FEditorViewportClient;
class FPrimitiveDrawInterface;
class FSceneView;
class FViewport;
class FViewportClient;
class ILevelEditor;
class IToolsContextQueriesAPI;
class IToolsContextRenderAPI;
class IToolsContextTransactionsAPI;
class UEdModeInteractiveToolsContext;
class UGizmoViewContext;
class UInputRouter;
class UInteractiveToolBuilder;
class UMaterialInterface;
class UObject;
class USelection;
class UTypedElementSelectionSet;
struct FToolBuilderState;

/**
 * UEditorInteractiveToolsContext is an extension/adapter of an InteractiveToolsContext designed 
 * for use in the UE Editor. Currently this implementation assumes that it is created by a
 * Mode Manager (FEditorModeTools), and that the Mode Manager will call various API functions
 * like Render() and Tick() when necessary. 
 * 
 * 
 * allows it to be easily embedded inside an FEdMode. A set of functions are provided which can be
 * called from the FEdMode functions of the same name. These will handle the data type
 * conversions and forwarding calls necessary to operate the ToolsContext
 */
UCLASS(Transient)
class EDITORINTERACTIVETOOLSFRAMEWORK_API UEditorInteractiveToolsContext : public UInteractiveToolsContext
{
	GENERATED_BODY()

public:
	UEditorInteractiveToolsContext();

	/**
	 * Initialize a new ToolsContext for an EdMode owned by the given InEditorModeManager
	 * @param 
	 */
	void InitializeContextWithEditorModeManager(FEditorModeTools* InEditorModeManager, UInputRouter* UseInputRouter = nullptr);

	/** Shutdown ToolsContext and clean up any connections/etc */
	virtual void ShutdownContext();

	// default behavior is to accept active tool
	virtual void TerminateActiveToolsOnPIEStart();

	// default behavior is to accept active tool
	virtual void TerminateActiveToolsOnSaveWorld();

	// default behavior is to cancel active tool
	virtual void TerminateActiveToolsOnWorldTearDown();

	// default behavior is to cancel active tool
	virtual void TerminateActiveToolsOnLevelChange();

	FEditorModeTools* GetParentEditorModeManager() const { return EditorModeManager; }

	IToolsContextQueriesAPI* GetQueriesAPI() const { return QueriesAPI; }
	IToolsContextTransactionsAPI* GetTransactionAPI() const { return TransactionAPI; }

	/** Call this to notify the Editor that the viewports this ToolsContext is related to may need a repaint, ie during interactive tool usage */
	virtual void PostInvalidation();

	// UObject Interface
	virtual UWorld* GetWorld() const override;

	// call functions of the same name on the ToolManager and GizmoManager
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime);
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI);
	virtual void DrawHUD(FViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View, FCanvas* Canvas);

	// These delegates can be used to hook into the Render() / DrawHUD() / Tick() calls above. In particular, non-legacy UEdMode's
	// don't normally receive Render() and DrawHUD() calls from the mode manager, but can attach to these.
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRender, IToolsContextRenderAPI* RenderAPI);
	FOnRender OnRender;
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDrawHUD, FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI);
	FOnDrawHUD OnDrawHUD;
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnTick, float DeltaTime);
	FOnTick OnTick;

	/** @return true if selected actors/components can be deleted */
	virtual bool ProcessEditDelete();

	//
	// Utility functions useful for hooking up to UICommand/etc
	//

	virtual bool CanStartTool(const FString ToolTypeIdentifier) const;
	virtual bool HasActiveTool() const;
	virtual FString GetActiveToolName() const;
	virtual bool ActiveToolHasAccept() const;
	virtual bool CanAcceptActiveTool() const;
	virtual bool CanCancelActiveTool() const;
	virtual bool CanCompleteActiveTool() const;
	virtual void StartTool(const FString ToolTypeIdentifier);
	virtual void EndTool(EToolShutdownType ShutdownType);
	void Activate();
	void Deactivate();


	/** @return Ray into 3D scene at last mouse event */
	virtual FRay GetLastWorldRay() const
	{
		check(false);
		return FRay();
	}

	//
	// Configuration functions
	//

	/*
	 * Configure whether ::Render() should early-out for HitProxy rendering passes.
	 * If the Mode does not use HitProxy, and the Tools/Gizmos have expensive Render() calls, this can help with interactive performance.
	 */
	void SetEnableRenderingDuringHitProxyPass(bool bEnabled);

	/** @return true if HitProxy rendering will be allowed in ::Render() */
	bool GetEnableRenderingDuringHitProxyPass() const { return bEnableRenderingDuringHitProxyPass; }


	/**
	 * Configure whether Transform Gizmos created by the ITF (eg CombinedTransformGizmo) should prefer to show in 'Combined' mode.
	 * If this is disabled, the Gizmo should respect the active Editor Gizmo setting (eg in the Level Viewport)
	 */
	void SetForceCombinedGizmoMode(bool bEnabled);

	/** @return true if Force Combined Gizmo mode is Enabled */
	bool GetForceCombinedGizmoModeEnabled() const { return bForceCombinedGizmoMode; }


	/**
	 * Configure whether Transform Gizmos created by the ITF (eg CombinedTransformGizmo) should, when in World coordinate system,
	 * snap to an Absolute world-aligned grid, or snap Relative to the initial position of any particular gizmo transform.
	 * Relative is the default and is also the behavior of the standard UE Gizmo.
	 */
	void SetAbsoluteWorldSnappingEnabled(bool bEnabled);

	/** @return true if Absolute World Snapping mode is Enabled */
	bool GetAbsoluteWorldSnappingEnabled() const { return bEnableAbsoluteWorldSnapping; }

protected:

	// we hide these 
	virtual void Initialize(IToolsContextQueriesAPI* QueriesAPI, IToolsContextTransactionsAPI* TransactionsAPI) override;
	virtual void Shutdown() override;

	virtual void DeactivateActiveTool(EToolSide WhichSide, EToolShutdownType ShutdownType);
	virtual void DeactivateAllActiveTools(EToolShutdownType ShutdownType);

public:
	UPROPERTY()
	TObjectPtr<UMaterialInterface> StandardVertexColorMaterial;

protected:
	// EdMode implementation of InteractiveToolFramework APIs - see ToolContextInterfaces.h
	IToolsContextQueriesAPI* QueriesAPI;
	IToolsContextTransactionsAPI* TransactionAPI;

	// Tools need to be able to Invalidate the view, in case it is not Realtime.
	// Currently we do this very aggressively, and also force Realtime to be on, but in general we should be able to rely on Invalidation.
	// However there are multiple Views and we do not want to Invalidate immediately, so we store a timestamp for each
	// ViewportClient, and invalidate it when we see it if it's timestamp is out-of-date.
	// (In theory this map will continually grow as new Viewports are created...)
	TMap<FViewportClient*, int32> InvalidationMap;
	// current invalidation timestamp, incremented by invalidation calls
	int32 InvalidationTimestamp = 0;

	// An object in which we save the current scene view information that gizmos can use on the game thread
	// to figure out how big the gizmo is for hit testing. Lives in the context store, but we keep a pointer here
	// to avoid having to look for it.
	UGizmoViewContext* GizmoViewContext = nullptr;

	// Utility function to convert viewport x/y from mouse events (and others?) into scene ray.
	// Copy-pasted from other Editor code, seems kind of expensive?
	static FRay GetRayFromMousePos(FEditorViewportClient* ViewportClient, FViewport* Viewport, int MouseX, int MouseY);

	// editor UI state that we set before starting tool and when exiting tool
	// Currently disabling anti-aliasing during active Tools because it causes PDI flickering
	void SetEditorStateForTool();
	void RestoreEditorState();

	void OnToolEnded(UInteractiveToolManager* InToolManager, UInteractiveTool* InEndedTool);
	void OnToolPostBuild(UInteractiveToolManager* InToolManager, EToolSide InSide, UInteractiveTool* InBuiltTool, UInteractiveToolBuilder* InToolBuilder, const FToolBuilderState& ToolState);

	TOptional<FString> PendingToolToStart = {};
	TOptional<EToolShutdownType> PendingToolShutdownType = {};

private:
	FEditorModeTools* EditorModeManager = nullptr;

	// currently defaulting to enabled as FEdModes generally assume this, and in most cases hitproxy pass is not expensive.
	bool bEnableRenderingDuringHitProxyPass = true;

	bool bForceCombinedGizmoMode = false;
	bool bEnableAbsoluteWorldSnapping = false;

	bool bIsActive = false;
};


/**
 * UModeManagerInteractiveToolsContext extends UEditorInteractiveToolsContext with various functions for handling 
 * device (mouse) input. These functions are currently called by the EdMode Manager (FEditorModeTools).
 */
UCLASS(Transient)
class EDITORINTERACTIVETOOLSFRAMEWORK_API UModeManagerInteractiveToolsContext : public UEditorInteractiveToolsContext
{
	GENERATED_BODY()

	//
	// UEditorInteractiveToolsContext API implementations that also forward calls to any child EdMode ToolsContexts
	//
public:
	virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime) override;
	virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI) override;
	virtual void DrawHUD(FViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View, FCanvas* Canvas) override;

	virtual bool ProcessEditDelete() override;

	//
	// Input handling, these functions forward ViewportClient events to the UInputRouter
	//
public:
	bool InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event);
	/**
	 * This updates internal state like InputKey, but doesn't route the results to the input router. 
	 * Use this if the input is captured by some higher system, to avoid this class from having an
	 * incorrect view of e.g. the mouse state because it did not receive a mouse release event.
	 */
	void UpdateStateWithoutRoutingInputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event);

	bool MouseEnter(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y);
	bool MouseLeave(FEditorViewportClient* ViewportClient, FViewport* Viewport);
	bool MouseMove(FEditorViewportClient* ViewportClient, FViewport* Viewport, int32 x, int32 y);

	bool StartTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport);
	bool CapturedMouseMove(FEditorViewportClient* InViewportClient, FViewport* InViewport, int32 InMouseX, int32 InMouseY);
	bool EndTracking(FEditorViewportClient* InViewportClient, FViewport* InViewport);

	/** @return Ray into 3D scene at last mouse event */
	virtual FRay GetLastWorldRay() const override;

protected:
	virtual void DeactivateAllActiveTools(EToolShutdownType ShutdownType) override;
	virtual void Initialize(IToolsContextQueriesAPI* QueriesAPIIn, IToolsContextTransactionsAPI* TransactionsAPIIn) override;
	virtual void Shutdown() override;

	/** Input event instance used to keep track of various button states, etc, that we cannot directly query on-demand */
	FInputDeviceState CurrentMouseState;
	// called when PIE is about to start, shuts down active tools
	FDelegateHandle BeginPIEDelegateHandle;
	// called before a Save starts. This currently shuts down active tools.
	FDelegateHandle PreSaveWorldDelegateHandle;
	// called when a map is changed
	FDelegateHandle WorldTearDownDelegateHandle;
	// called when viewport clients change
	FDelegateHandle ViewportClientListChangedHandle;

private:
	bool bIsTrackingMouse;




protected:
	UPROPERTY()
	TArray<TObjectPtr<UEdModeInteractiveToolsContext>> EdModeToolsContexts;

public:
	/** 
	* Create and initialize a new EdMode-level ToolsContext derived from the ModeManager ToolsContext.
	* The EdMode ToolsContext does not have it's own InputRouter, it shares the InputRouter with the ModeManager ToolsContext.
	* The ModeManager ToolsContext keeps track of these derived ToolsContext's and automatically Tick()'s them/etc.
	* When the child ToolsContext is shut down, OnChildEdModeToolsContextShutdown() must be called to clean up
	* @return new ToolsContext
	*/
	UEdModeInteractiveToolsContext* CreateNewChildEdModeToolsContext();

	/**
	 * Call to add a child EdMode ToolsContext created using the above function
	 * @return true if child was added
	 */
	bool OnChildEdModeActivated(UEdModeInteractiveToolsContext* ChildToolsContext);

	/**
	 * Call to release a child EdMode ToolsContext created using the above function
	 * @return true if child was found and removed
	 */
	bool OnChildEdModeDeactivated(UEdModeInteractiveToolsContext* ChildToolsContext);

};


/**
 * UEdModeInteractiveToolsContext is an UEditorInteractiveToolsContext intended for use/lifetime in the context of a UEdMode.
 * This ITC subclass is dependent on a UModeManagerInteractiveToolsContext to provide an InputRouter.
 */
UCLASS(Transient)
class EDITORINTERACTIVETOOLSFRAMEWORK_API UEdModeInteractiveToolsContext : public UEditorInteractiveToolsContext
{
	friend class UModeManagerInteractiveToolsContext;

	GENERATED_BODY()
public:
	/**
	 * Initialize a new EdModeToolsContext that is derived from a ModeManagerToolsContext.
	 * This new ToolsContext will not have it's own InputRouter, it will share the InputRouter with the ModeManagerToolsContext
	 */
	void InitializeContextFromModeManagerContext(UModeManagerInteractiveToolsContext* ModeManagerToolsContext);

	/** @return Ray into 3D scene at last mouse event */
	virtual FRay GetLastWorldRay() const override;

protected:
	UPROPERTY()
	TObjectPtr<UModeManagerInteractiveToolsContext> ParentModeManagerToolsContext = nullptr;
};