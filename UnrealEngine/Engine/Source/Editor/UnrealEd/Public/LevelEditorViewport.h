// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraComponent.h"
#include "UnrealWidgetFwd.h"
#include "EditorViewportClient.h"
#include "UObject/ObjectKey.h"
#include "UnrealEdMisc.h"
#include "Elements/Framework/TypedElementListFwd.h"

struct FAssetData;
struct FMinimalViewInfo;
class FCanvas;
class FDragTool;
class HModel;
class ILevelEditor;
class SLevelViewport;
class UActorFactory;
class UModel;
class UTypedElementSelectionSet;
struct FWorldContext;
struct FTypedElementHandle;

/** Describes an object that's currently hovered over in the level viewport */
struct FViewportHoverTarget
{
	/** The actor we're drawing the hover effect for, or NULL */
	AActor* HoveredActor;

	/** The BSP model we're drawing the hover effect for, or NULL */
	UModel* HoveredModel;

	/** Surface index on the BSP model that currently has a hover effect */
	uint32 ModelSurfaceIndex;


	/** Construct from an actor */
	FViewportHoverTarget( AActor* InActor )
		: HoveredActor( InActor ),
			HoveredModel( NULL ),
			ModelSurfaceIndex( INDEX_NONE )
	{
	}

	/** Construct from an BSP model and surface index */
	FViewportHoverTarget( UModel* InModel, int32 InSurfaceIndex )
		: HoveredActor( NULL ),
			HoveredModel( InModel ),
			ModelSurfaceIndex( InSurfaceIndex )
	{
	}

	/** Equality operator */
	bool operator==( const FViewportHoverTarget& RHS ) const
	{
		return RHS.HoveredActor == HoveredActor &&
				RHS.HoveredModel == HoveredModel &&
				RHS.ModelSurfaceIndex == ModelSurfaceIndex;
	}

	friend uint32 GetTypeHash( const FViewportHoverTarget& Key )
	{
		return Key.HoveredActor ? GetTypeHash(Key.HoveredActor) : GetTypeHash(Key.HoveredModel)+Key.ModelSurfaceIndex;
	}
};

struct UNREALED_API FTrackingTransaction
{
	/** State of this transaction */
	struct ETransactionState
	{
		enum Enum
		{
			Inactive,
			Active,
			Pending,
		};
	};

	FTrackingTransaction();
	~FTrackingTransaction();

	/**
	 * Initiates a transaction.
	 */
	void Begin(const FText& Description, AActor* AdditionalActor = nullptr);

	void End();

	void Cancel();

	/** Begin a pending transaction, which won't become a real transaction until PromotePendingToActive is called */
	void BeginPending(const FText& Description);

	/** Promote a pending transaction (if any) to an active transaction */
	void PromotePendingToActive();

	bool IsActive() const { return TrackingTransactionState == ETransactionState::Active; }

	bool IsPending() const { return TrackingTransactionState == ETransactionState::Pending; }
	
	int32 TransCount = 0;

private:

	const UTypedElementSelectionSet* GetSelectionSet() const;
	UTypedElementSelectionSet* GetMutableSelectionSet() const;

	/** Editor selection changed delegate handler */	
	void OnEditorSelectionChanged(const UTypedElementSelectionSet* InSelectionSet);

	/** The current transaction. */
	class FScopedTransaction* ScopedTransaction = nullptr;

	/** This is set to Active if TrackingStarted() has initiated a transaction, Pending if a transaction will begin before the next delta change */
	ETransactionState::Enum TrackingTransactionState = ETransactionState::Inactive;

	/** The description to use if a pending transaction turns into a real transaction */
	FText PendingDescription;

	/** Initial package dirty states for the Actors within the transaction */
	TMap<UPackage*, bool> InitialPackageDirtyStates;
	
};

/** Interface for objects who want to lock the viewport to an actor. */
struct UNREALED_API FLevelViewportActorLock
{
	/** Represents no lock. */
	static const FLevelViewportActorLock None;

	/** Creates a new instance of FLevelViewportActorLock. */
	FLevelViewportActorLock() 
		: LockedActor(nullptr) {}

	/** Creates a new instance of FLevelViewportActorLock. */
	FLevelViewportActorLock(AActor* InActor) 
		: LockedActor(InActor) {}

	/** Creates a new instance of FLevelViewportActorLock. */
	FLevelViewportActorLock(AActor* InActor, TOptional<EAspectRatioAxisConstraint> InAspectRatioAxisConstraint) 
		: LockedActor(InActor), AspectRatioAxisConstraint(InAspectRatioAxisConstraint) {}

	/** Returns whether the locked actor is valid. */
	bool HasValidLockedActor() const { return LockedActor.IsValid(); }

	/** Gets the locked actor. */
	AActor* GetLockedActor() const { return LockedActor.Get(); }

	/** The actor the viewport should be locked to. */
	TWeakObjectPtr<AActor> LockedActor;

	/** An optional aspect ratio axis constraint to use when resizing the viewport. */
	TOptional<EAspectRatioAxisConstraint> AspectRatioAxisConstraint;
};

/** */
class UNREALED_API FLevelEditorViewportClient : public FEditorViewportClient
{
	friend class FActorElementLevelEditorViewportInteractionCustomization;
	friend class FComponentElementLevelEditorViewportInteractionCustomization;

public:

	/** @return Returns the current global drop preview actor, or a NULL pointer if we don't currently have one */
	static const TArray< TWeakObjectPtr<AActor> >& GetDropPreviewActors()
	{
		return DropPreviewActors;
	}

	FVector2D GetDropPreviewLocation() const { return FVector2D( DropPreviewMouseX, DropPreviewMouseY ); }

	/**
	 * Constructor
	 */
	FLevelEditorViewportClient(const TSharedPtr<class SLevelViewport>& InLevelViewport);

	/**
	 * Destructor
	 */
	virtual ~FLevelEditorViewportClient();

	////////////////////////////
	// FViewElementDrawer interface
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI) override;
	// End of FViewElementDrawer interface
	
	virtual FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily, const int32 StereoViewIndex = INDEX_NONE) override;

	////////////////////////////
	// FEditorViewportClient interface
	virtual void DrawCanvas( FViewport& InViewport, FSceneView& View, FCanvas& Canvas ) override;
	virtual bool InputKey(const FInputKeyEventArgs& InEventArgs) override;
	virtual bool InputAxis(FViewport* Viewport, FInputDeviceId DeviceId, FKey Key, float Delta, float DeltaTime, int32 NumSamples=1, bool bGamepad=false) override;
	virtual EMouseCursor::Type GetCursor(FViewport* Viewport,int32 X,int32 Y) override;
	virtual void CapturedMouseMove(FViewport* InViewport, int32 InMouseX, int32 InMouseY) override;
	virtual void MouseMove(FViewport* InViewport, int32 x, int32 y) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool InputWidgetDelta( FViewport* Viewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale ) override;
	virtual TSharedPtr<FDragTool> MakeDragTool( EDragTool::Type DragToolType ) override;
	virtual bool IsLevelEditorClient() const override { return ParentLevelEditor.IsValid(); }
	virtual void TrackingStarted( const struct FInputEventState& InInputState, bool bIsDraggingWidget, bool bNudge ) override;
	virtual void TrackingStopped() override;
	virtual void AbortTracking() override;
	virtual UE::Widget::EWidgetMode GetWidgetMode() const override;
	virtual FVector GetWidgetLocation() const override;
	virtual FMatrix GetWidgetCoordSystem() const override;
	virtual void SetupViewForRendering( FSceneViewFamily& ViewFamily, FSceneView& View ) override;
	virtual FLinearColor GetBackgroundColor() const override;
	virtual int32 GetCameraSpeedSetting() const override;
	virtual void SetCameraSpeedSetting(int32 SpeedSetting) override;
	virtual float GetCameraSpeedScalar() const override;
	virtual void SetCameraSpeedScalar(float SpeedScalar) override;
	virtual void ReceivedFocus(FViewport* InViewport) override;
	virtual void LostFocus(FViewport* InViewport) override;
	virtual void ProcessClick(FSceneView& View, HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
	virtual UWorld* GetWorld() const override;
	virtual void BeginCameraMovement(bool bHasMovement) override;
	virtual void EndCameraMovement() override;
	virtual void SetVREditView(bool bGameViewEnable) override;
	virtual bool GetPivotForOrbit(FVector& Pivot) const override;
	virtual bool ShouldScaleCameraSpeedByDistance() const override;

	virtual bool OverrideHighResScreenshotCaptureRegion(FIntRect& OutCaptureRegion) override;

	/** Sets a flag for this frame indicating that the camera has been cut, and temporal effects (such as motion blur) should be reset */
	void SetIsCameraCut()
	{
		bEditorCameraCut = true;
		bWasEditorCameraCut = false;
	}
	bool GetIsCameraCut() const
	{
		return bEditorCameraCut;
	}

	/** 
	 * Initialize visibility flags
	 */
	void InitializeVisibilityFlags();

	/**
	 * Initialize viewport interaction
	 */
	void InitializeViewportInteraction();

	/**
	 * Reset the camera position and rotation.  Used when creating a new level.
	 */
	void ResetCamera();

	/**
	 * Reset the view for a new map 
	 */
	void ResetViewForNewMap();

	/**
	 * Stores camera settings that may be adversely affected by PIE, so that they may be restored later
	 */
	void PrepareCameraForPIE();

	/**
	 * Restores camera settings that may be adversely affected by PIE
	 */
	void RestoreCameraFromPIE();

	/**
	 * Updates the audio listener for this viewport 
	 *
	 * @param View	The scene view to use when calculate the listener position
	 */
	void UpdateAudioListener( const FSceneView& View );

	/** Determines if the new MoveCanvas movement should be used */
	bool ShouldUseMoveCanvasMovement (void);

	/** 
	 * Returns true if the passed in volume is visible in the viewport (due to volume actor visibility flags)
	 *
	 * @param VolumeActor	The volume to check
	 */
	bool IsVolumeVisibleInViewport( const AActor& VolumeActor ) const;

	/**
	 * Updates or resets view properties such as aspect ratio, FOV, location etc to match that of any actor we are locked to
	 */
	void UpdateViewForLockedActor(float DeltaTime=0.f);

	/**
	 * Returns the horizontal axis for this viewport.
	 */
	EAxisList::Type GetHorizAxis() const;

	/**
	 * Returns the vertical axis for this viewport.
	 */
	EAxisList::Type GetVertAxis() const;

	virtual void NudgeSelectedObjects( const struct FInputEventState& InputState ) override;

	/**
	 * Moves the viewport camera according to the locked actors location and rotation
	 */
	void MoveCameraToLockedActor();

	/**
	 * Check to see if this actor is locked by the viewport
	 */
	bool IsActorLocked(const TWeakObjectPtr<const AActor> InActor) const;

	/**
	 * Check to see if any actor is locked by the viewport
	 */
	bool IsAnyActorLocked() const;

	void ApplyDeltaToActors( const FVector& InDrag, const FRotator& InRot, const FVector& InScale );
	void ApplyDeltaToActor( AActor* InActor, const FVector& InDeltaDrag, const FRotator& InDeltaRot, const FVector& InDeltaScale );
	void ApplyDeltaToComponent(USceneComponent* InComponent, const FVector& InDeltaDrag, const FRotator& InDeltaRot, const FVector& InDeltaScale);
	
	void ApplyDeltaToSelectedElements(const FTransform& InDeltaTransform);
	void ApplyDeltaToElement(const FTypedElementHandle& InElementHandle, const FTransform& InDeltaTransform);

	void MirrorSelectedActors(const FVector& InMirrorScale);
	void MirrorSelectedElements(const FVector& InMirrorScale);

	bool GetFocusBounds(FTypedElementListConstRef InElements, FBoxSphereBounds& OutBounds);

	/**
	 * Get the elements (from the current selection set) that this viewport can manipulate (eg, via the transform gizmo).
	 */
	FTypedElementListConstRef GetElementsToManipulate(const bool bForceRefresh = false);

	virtual void SetIsSimulateInEditorViewport( bool bInIsSimulateInEditorViewport ) override;

	/**
	 *	Draw the texture streaming bounds.
	 */
	void DrawTextureStreamingBounds(const FSceneView* View,FPrimitiveDrawInterface* PDI);

	/** GC references. */
	void AddReferencedObjects( FReferenceCollector& Collector ) override;
	
	/**
	 * Copies layout and camera settings from the specified viewport
	 *
	 * @param InViewport The viewport to copy settings from
	 */
	void CopyLayoutFromViewport( const FLevelEditorViewportClient& InViewport );

	/**
	 * Returns whether the provided unlocalized sprite category is visible in the viewport or not
	 *
	 * @param	InSpriteCategory	Sprite category to get the index of
	 *
	 * @return	true if the specified category is visible in the viewport; false if it is not
	 */
	bool GetSpriteCategoryVisibility( const FName& InSpriteCategory ) const;

	/**
	 * Returns whether the sprite category specified by the provided index is visible in the viewport or not
	 *
	 * @param	Index	Index of the sprite category to check
	 *
	 * @return	true if the category specified by the index is visible in the viewport; false if it is not
	 */
	bool GetSpriteCategoryVisibility( int32 Index ) const;

	/**
	 * Sets the visibility of the provided unlocalized category to the provided value
	 *
	 * @param	InSpriteCategory	Sprite category to get the index of
	 * @param	bVisible			true if the category should be made visible, false if it should be hidden
	 */
	void SetSpriteCategoryVisibility( const FName& InSpriteCategory, bool bVisible );

	/**
	 * Sets the visibility of the category specified by the provided index to the provided value
	 *
	 * @param	Index		Index of the sprite category to set the visibility of
	 * @param	bVisible	true if the category should be made visible, false if it should be hidden
	 */
	void SetSpriteCategoryVisibility( int32 Index, bool bVisible );

	/**
	 * Sets the visibility of all sprite categories to the provided value
	 *
	 * @param	bVisible	true if all the categories should be made visible, false if they should be hidden
	 */
	void SetAllSpriteCategoryVisibility( bool bVisible );

	void SetReferenceToWorldContext(FWorldContext& WorldContext);

	void RemoveReferenceToWorldContext(FWorldContext& WorldContext);

	/** Returns true if a placement dragging actor exists */
	virtual bool HasDropPreviewActors() const override;

	/**
	 * If dragging an actor for placement, this function updates its position.
	 *
	 * @param MouseX						The position of the mouse's X coordinate
	 * @param MouseY						The position of the mouse's Y coordinate
	 * @param DroppedObjects				The Objects that were used to create preview objects
	 * @param out_bDroppedObjectsVisible	Output, returns if preview objects are visible or not
	 *
	 * Returns true if preview actors were updated
	 */
	virtual bool UpdateDropPreviewActors(int32 MouseX, int32 MouseY, const TArray<UObject*>& DroppedObjects, bool& out_bDroppedObjectsVisible, class UActorFactory* FactoryToUse = NULL) override;

	/**
	 * If dragging an actor for placement, this function destroys the actor.
	 */
	virtual void DestroyDropPreviewActors() override;

	/**
	 * Checks the viewport to see if the given object can be dropped using the given mouse coordinates local to this viewport
	 *
	 * @param MouseX			The position of the mouse's X coordinate
	 * @param MouseY			The position of the mouse's Y coordinate
	 * @param AssetInfo			Asset in question to be dropped
	 */
	virtual FDropQuery CanDropObjectsAtCoordinates(int32 MouseX, int32 MouseY, const FAssetData& AssetInfo) override;

	/**
	 * Attempts to intelligently drop the given objects in the viewport, using the given mouse coordinates local to this viewport
	 *
	 * @param MouseX			 The position of the mouse's X coordinate
	 * @param MouseY			 The position of the mouse's Y coordinate
	 * @param DroppedObjects	 The Objects to be placed into the editor via this viewport
	 * @param OutNewActors		 The new actor objects that were created
	 * @param bOnlyDropOnTarget  Flag that when True, will only attempt a drop on the actor targeted by the Mouse position. Defaults to false.
	 * @param bCreateDropPreview If true, a drop preview actor will be spawned instead of a normal actor.
	 * @param bSelectActors		 If true, select the newly dropped actors (defaults: true)
	 * @param FactoryToUse		 The preferred actor factory to use (optional)
	 */
	virtual bool DropObjectsAtCoordinates(int32 MouseX, int32 MouseY, const TArray<UObject*>& DroppedObjects, TArray<AActor*>& OutNewActors, bool bOnlyDropOnTarget = false, bool bCreateDropPreview = false, bool bSelectActors = true, UActorFactory* FactoryToUse = NULL ) override;

	/**
	 * Sets GWorld to the appropriate world for this client
	 * 
	 * @return the previous GWorld
	 */
	virtual UWorld* ConditionalSetWorld() override;

	/**
	 * Restores GWorld to InWorld
	 *
	 * @param InWorld	The world to restore
	 */
	virtual void ConditionalRestoreWorld( UWorld* InWorld  ) override;

	/**
	 *	Called to check if a material can be applied to an object, given the hit proxy
	 */
	bool CanApplyMaterialToHitProxy( const HHitProxy* HitProxy ) const;

	/**
	 * Static: Adds a hover effect to the specified object
	 *
	 * @param	InHoverTarget	The hoverable object to add the effect to
	 */
	static void AddHoverEffect( const struct FViewportHoverTarget& InHoverTarget );

	/**
	 * Static: Removes a hover effect to the specified object
	 *
	 * @param	InHoverTarget	The hoverable object to remove the effect from
	 */
	static void RemoveHoverEffect( const struct FViewportHoverTarget& InHoverTarget );

	/**
	 * Static: Clears viewport hover effects from any objects that currently have that
	 */
	static void ClearHoverFromObjects();

	/** Set the global ptr to the current viewport */
	void SetCurrentViewport();

	/** Set the global ptr to the last viewport to receive a key press */
	void SetLastKeyViewport();

	/** 
	 * Access the 'active' actor lock.
	 *
	 * This returns the actor lock (as per GetActorLock) if that is the currently active lock. It is *not* the currently
	 * active lock if there's a valid cinematic lock actor (as per GetCinematicActorLock), since cinematics take
	 * precedence.
	 * 
	 * @return  The actor currently locked to the viewport and actively linked to the camera movements.
	 */
	TWeakObjectPtr<AActor> GetActiveActorLock() const
	{
		if (ActorLocks.CinematicActorLock.HasValidLockedActor())
		{
			return TWeakObjectPtr<AActor>();
		}
		return ActorLocks.ActorLock.LockedActor;
	}
	
	/**
	 * Find a view component to use for the specified actor. Prioritizes selected 
	 * components first, followed by camera components (then falls through to the first component that implements GetEditorPreviewInfo)
	 */
	static UActorComponent* FindViewComponentForActor(AActor const* Actor);

	/** 
	 * Find the camera component that is driving this viewport, in the following order of preference:
	 *		1. Cinematic locked actor
	 *		2. User actor lock (if (bLockedCameraView is true)
	 * 
	 * @return  Pointer to a camera component to use for this viewport's view
	 */
	UCameraComponent* GetCameraComponentForView() const
	{
		const FLevelViewportActorLock& ActorLock = ActorLocks.GetLock(bLockedCameraView);
		return Cast<UCameraComponent>(FindViewComponentForActor(ActorLock.GetLockedActor()));
	}

	/**
	 * Gets the actor lock. This is the actor locked to the viewport via the viewport menus.
	 */
	const FLevelViewportActorLock& GetActorLock() const
	{
		return ActorLocks.ActorLock;
	}

	/**
	 * Gets the actor lock. This is the actor locked to the viewport via the viewport menus.
	 */
	FLevelViewportActorLock& GetActorLock()
	{
		return ActorLocks.ActorLock;
	}

	/** 
	 * Set the actor lock. This is the actor locked to the viewport via the viewport menus.
	 */
	void SetActorLock(AActor* Actor);

	/** 
	 * Set the actor lock. This is the actor locked to the viewport via the viewport menus.
	 */
	void SetActorLock(const FLevelViewportActorLock& InActorLock);

	/**
	 * Get the actor locked to the viewport by cinematic tools like Sequencer.
	 */
	const FLevelViewportActorLock& GetCinematicActorLock() const
	{
		return ActorLocks.CinematicActorLock;
	}

	/**
	 * Get the actor locked to the viewport by cinematic tools like Sequencer.
	 */
	FLevelViewportActorLock& GetCinematicActorLock()
	{
		return ActorLocks.CinematicActorLock;
	}

	/**
	 * Set the actor locked to the viewport by cinematic tools like Sequencer.
	 */
	void SetCinematicActorLock(AActor* Actor);

	/**
	 * Set the actor locked to the viewport by cinematic tools like Sequencer.
	 */
	void SetCinematicActorLock(const FLevelViewportActorLock& InActorLock);

	/**
	 * Gets the previous actor lock. This is the actor locked to the viewport via the viewport menus.
	 */
	const FLevelViewportActorLock& GetPreviousActorLock() const
	{
		return PreviousActorLocks.ActorLock;
	}

	/**
	 * Get the previous actor locked to the viewport by cinematic tools like Sequencer.
	 */
	const FLevelViewportActorLock& GetPreviousCinematicActorLock() const
	{
		return PreviousActorLocks.CinematicActorLock;
	}

	/** 
	 * Check whether this viewport is locked to the specified actor
	 */
	bool IsLockedToActor(AActor* Actor) const
	{
		return ActorLocks.HasActorLocked(Actor);
	}

	/**
	 * Check whether this viewport is locked to display a cinematic camera, like a Sequencer camera.
	 */
	bool IsLockedToCinematic() const
	{
		return ActorLocks.CinematicActorLock.HasValidLockedActor();
	}

	void UpdateHoveredObjects( const TSet<FViewportHoverTarget>& NewHoveredObjects );

	/**
	 * Calling SetViewportType from Dragtool_ViewportChange
	 */
	void SetViewportTypeFromTool(ELevelViewportType InViewportType);

	/**
	 * Static: Attempts to place the specified object in the level, returning one or more newly-created actors if successful.
	 * IMPORTANT: The placed actor's location must be first set using GEditor->ClickLocation and GEditor->ClickPlane.
	 *
	 * @param	InLevel			Level in which to drop actor
	 * @param	ObjToUse		Asset to attempt to use for an actor to place
	 * @param	CursorLocation	Location of the cursor while dropping
	 * @param	bSelectActors	If true, select the newly dropped actors (defaults: true)
	 * @param	ObjectFlags		The flags to place on the actor when it is spawned
	 * @param	FactoryToUse	The preferred actor factory to use (optional)
	 * @param	Cursor			Optional pre-calculated cursor location
	 *
	 * @return	true if the object was successfully used to place an actor; false otherwise
	 */
	static TArray<AActor*> TryPlacingActorFromObject( ULevel* InLevel, UObject* ObjToUse, bool bSelectActors, EObjectFlags ObjectFlags, UActorFactory* FactoryToUse, const FName Name = NAME_None, const FViewportCursorLocation* Cursor = nullptr);

	/** 
	 * Returns true if creating a preview actor in the viewport. 
	 */
	static bool IsDroppingPreviewActor()
	{
		return bIsDroppingPreviewActor;
	}

	/**
	 * Static: Given a texture, returns a material for that texture, creating a new asset if necessary.  This is used
	 * for dragging and dropping assets into the scene
	 *
	 * @param	UnrealTexture	Texture that we need a material for
	 *
	 * @return	The material that uses this texture, or null if we couldn't find or create one
	 */
	static UObject* GetOrCreateMaterialFromTexture( UTexture* UnrealTexture );

	virtual bool UseAppTime() const override { return false; }

	/**
	 * Informs the renderer that the view is being interactively edited. (ex. rotation/translation gizmo).
	 * This state is reset on tick.
	 */
	void SetEditingThroughMovementWidget();

protected:
	/**
	* Sets the state of creating a preview actor in the viewport.
	*/
	static void SetIsDroppingPreviewActor(bool bNewIsDroppingPreviewActor)
	{
		bIsDroppingPreviewActor = bNewIsDroppingPreviewActor;
	}


	/** 
	 * Checks the viewport to see if the given blueprint asset can be dropped on the viewport.
	 * @param AssetInfo		The blueprint Asset in question to be dropped
	 *
	 * @return true if asset can be dropped, false otherwise
	 */
	bool CanDropBlueprintAsset ( const struct FSelectedAssetInfo& );

	/** Called when editor cleanse event is triggered */
	void OnEditorCleanse();

	/** Called before the editor tries to begin PIE */
	void OnPreBeginPIE(const bool bIsSimulating);

	/** Callback for when an editor user setting has changed */
	void HandleViewportSettingChanged(FName PropertyName);

	/** Callback for when a map is created or destroyed */
	void OnMapChanged(UWorld* InWorld, EMapChangeType MapChangeType);

	/** Delegate handler for ActorMoved events */
	void OnActorMoved(AActor* InActor);

public:
	/** FEditorViewportClient Interface*/
	virtual void UpdateLinkedOrthoViewports(bool bInvalidate = false) override;
	virtual ELevelViewportType GetViewportType() const override;
	virtual void SetViewportType(ELevelViewportType InViewportType) override;
	virtual void RotateViewportType() override;
	virtual void OverridePostProcessSettings(FSceneView& View) override;
	virtual bool ShouldLockPitch() const override;
	virtual void CheckHoveredHitProxy(HHitProxy* HoveredHitProxy) override;

protected:

	virtual void PerspectiveCameraMoved() override;
	virtual bool GetActiveSafeFrame(float& OutAspectRatio) const override;
	virtual void RedrawAllViewportsIntoThisScene() override;

private:
	FTransform CachePreDragActorTransform(const AActor* InActor);

	/**
	 * Checks to see the viewports locked actor need updating
	 */
	void UpdateLockedActorViewports(const AActor* InActor, const bool bCheckRealtime);
	void UpdateLockedActorViewport(const AActor* InActor, const bool bCheckRealtime);

	/**
	 * Moves the locked actor according to the viewport cameras location and rotation
	 */
	void MoveLockedActorToCamera();
	
	/** @return	Returns true if the delta tracker was used to modify any selected actors or BSP.  Must be called before EndTracking(). */
	bool HaveSelectedObjectsBeenChanged() const;

	/** Cache the list of elements to manipulate based on the current selection set. */
	void CacheElementsToManipulate(const bool bForceRefresh = false);

	/** Reset the list of elements to manipulate */
	void ResetElementsToManipulate(const bool bClearList = true);

	/** Reset the list of elements to manipulate, because the selection set they were cached from has changed */
	void ResetElementsToManipulateFromSelectionChange(const UTypedElementSelectionSet* InSelectionSet);

	/** Reset the list of elements to manipulate, because the typed element registry is about to process deferred deletion */
	void ResetElementsToManipulateFromProcessingDeferredElementsToDestroy();

	/** Get the selection set that associated with our level editor. */
	const UTypedElementSelectionSet* GetSelectionSet() const;
	UTypedElementSelectionSet* GetMutableSelectionSet() const;

	/**
	 * Called when to attempt to apply an object to a BSP surface
	 *
	 * @param	ObjToUse			The object to attempt to apply
	 * @param	ModelHitProxy		The hitproxy of the BSP model whose surface the user is clicking on
	 * @param	Cursor				Mouse cursor location
	 *
	 * @return	true if the object was applied to the object
	 */
	bool AttemptApplyObjAsMaterialToSurface( UObject* ObjToUse, class HModel* ModelHitProxy, FViewportCursorLocation& Cursor );

	/**
	 * Called when an asset is dropped onto the blank area of a viewport.
	 *
	 * @param	Cursor				Mouse cursor location
	 * @param	DroppedObjects		Array of objects dropped into the viewport
	 * @param	ObjectFlags			The object flags to place on the actors that this function spawns.
	 * @param	OutNewActors		The list of actors created while dropping
	 * @param	bCreateDropPreview	If true, the actor being dropped is a preview actor (defaults: false)
	 * @param	bSelectActors		If true, select the newly dropped actors (defaults: true)
	 * @param	FactoryToUse		The preferred actor factory to use (optional)
	 *
	 * @return	true if the drop operation was successfully handled; false otherwise
	 */
	bool DropObjectsOnBackground( struct FViewportCursorLocation& Cursor, const TArray<UObject*>& DroppedObjects, EObjectFlags ObjectFlags, TArray<AActor*>& OutNewActors, bool bCreateDropPreview = false, bool bSelectActors = true, class UActorFactory* FactoryToUse = NULL );

	/**
	* Called when an asset is dropped upon an existing actor.
	*
	* @param	Cursor				Mouse cursor location
	* @param	DroppedObjects		Array of objects dropped into the viewport
	* @param	DroppedUponActor	The actor that we are dropping upon
	* @param    DroppedUponSlot     The material slot/submesh that was identified as the drop location.  If unknown use -1.
	* @param	ObjectFlags			The object flags to place on the actors that this function spawns.
	* @param	OutNewActors		The list of actors created while dropping
	* @param	bCreateDropPreview	If true, the actor being dropped is a preview actor (defaults: false)
	* @param	bSelectActors		If true, select the newly dropped actors (defaults: true)
	* @param	FactoryToUse		The preferred actor factory to use (optional)
	*
	* @return	true if the drop operation was successfully handled; false otherwise
	*/
	bool DropObjectsOnActor(struct FViewportCursorLocation& Cursor, const TArray<UObject*>& DroppedObjects, AActor* DroppedUponActor, int32 DroppedUponSlot, EObjectFlags ObjectFlags, TArray<AActor*>& OutNewActors, bool bCreateDropPreview = false, bool bSelectActors = true, class UActorFactory* FactoryToUse = NULL);

	/**
	 * Called when an asset is dropped upon a BSP surface.
	 *
	 * @param	View				The SceneView for the dropped-in viewport
	 * @param	Cursor				Mouse cursor location
	 * @param	DroppedObjects		Array of objects dropped into the viewport
	 * @param	TargetProxy			Hit proxy representing the dropped upon model
	 * @param	ObjectFlags			The object flags to place on the actors that this function spawns.
	 * @param	OutNewActors		The list of actors created while dropping
	 * @param	bCreateDropPreview	If true, the actor being dropped is a preview actor (defaults: false)
	 * @param	bSelectActors		If true, select the newly dropped actors (defaults: true)
	 * @param	FactoryToUse		The preferred actor factory to use (optional)
	 *
	 * @return	true if the drop operation was successfully handled; false otherwise
	 */
	bool DropObjectsOnBSPSurface(FSceneView* View, struct FViewportCursorLocation& Cursor, const TArray<UObject*>& DroppedObjects, HModel* TargetProxy, EObjectFlags ObjectFlags, TArray<AActor*>& OutNewActors, bool bCreateDropPreview = false, bool bSelectActors = true, UActorFactory* FactoryToUse = NULL);

	/**
	 * Called when an asset is dropped upon a manipulation widget.
	 *
	 * @param	View				The SceneView for the dropped-in viewport
	 * @param	Cursor				Mouse cursor location
	 * @param	DroppedObjects		Array of objects dropped into the viewport
	 * @param	bCreateDropPreview	If true, the actor being dropped is a preview actor (defaults: false)
	 *
	 * @return	true if the drop operation was successfully handled; false otherwise
	 */
	bool DropObjectsOnWidget(FSceneView* View, struct FViewportCursorLocation& Cursor, const TArray<UObject*>& DroppedObjects, bool bCreateDropPreview = false);

	/** Project the specified actors into the world according to the current drag parameters */
	void ProjectActorsIntoWorld(const TArray<AActor*>& Actors, FViewport* Viewport, const FVector& Drag, const FRotator& Rot);

	/** Draw additional details for brushes in the world */
	void DrawBrushDetails(const FSceneView* View, FPrimitiveDrawInterface* PDI);

	/** Internal function for public FindViewComponentForActor, which finds a view component to use for the specified actor. */
	static UActorComponent* FindViewComponentForActor(AActor const* Actor, TSet<AActor const*>& CheckedActors);

public:
	/** Static: List of objects we're hovering over */
	static TSet< FViewportHoverTarget > HoveredObjects;
	
	/** Parent level editor that owns this viewport.  Currently, this may be null if the parent doesn't happen to be a level editor. */
	TWeakPtr< class ILevelEditor > ParentLevelEditor;

	/** List of layers that are hidden in this view */
	TArray<FName>			ViewHiddenLayers;

	/** Special volume actor visibility settings. Each bit represents a visibility state for a specific volume class. 1 = visible, 0 = hidden */
	TBitArray<>				VolumeActorVisibility;

	/** The viewport location that is restored when exiting PIE */
	FVector					LastEditorViewLocation;
	/** The viewport orientation that is restored when exiting PIE */
	FRotator				LastEditorViewRotation;

	FVector					ColorScale;

	FColor					FadeColor;

	float					FadeAmount;

	bool					bEnableFading;

	bool					bEnableColorScaling;

	/** Indicates whether, of not, the base attachment volume should be drawn for this viewport. */
	bool bDrawBaseInfo;

	/**
	 * Used for drag duplication. Set to true on Alt+LMB so that the selected
	 * objects (components or actors) will be duplicated as soon as the widget is displaced.
	 */
	bool					bDuplicateOnNextDrag;

	/**
	* bDuplicateActorsOnNextDrag will not be set again while bDuplicateActorsInProgress is true.
	* The user needs to release ALT and all mouse buttons to clear bDuplicateActorsInProgress.
	*/
	bool					bDuplicateActorsInProgress;

	/**
	 * true when a brush is being transformed by its Widget
	 */
	bool					bIsTrackingBrushModification;

	/**
	 * true if only the pivot position has been moved
	 */
	bool					bOnlyMovedPivot;

	/** True if this viewport is to change its view (aspect ratio, post processing, FOV etc) to match that of the currently locked camera, if applicable */
	bool					bLockedCameraView;

	/** true if the viewport needs to restore the flag when tracking ends */
	bool					bNeedToRestoreComponentBeingMovedFlag;

	/** true if gizmo manipulation was started from a tracking event */
	bool					bHasBegunGizmoManipulation;

	/** Whether this viewport recently received focus. Used to determine whether component selection is permissible. */
	bool bReceivedFocusRecently;

	/** When enabled, the Unreal transform widget will become visible after an actor is selected, even if it was turned off via a show flag */
	bool bAlwaysShowModeWidgetAfterSelectionChanges;

private:
	/** The actors that are currently being placed in the viewport via dragging */
	static TArray< TWeakObjectPtr< AActor > > DropPreviewActors;

	/** If currently creating a preview actor. */
	static bool bIsDroppingPreviewActor;

	/** A map of actor locations before a drag operation */
	mutable TMap<TWeakObjectPtr<const AActor>, FTransform> PreDragActorTransforms;

	/** The elements (from the current selection set) that this viewport can manipulate (eg, via the transform gizmo) */
	bool bHasCachedElementsToManipulate = false;
	FTypedElementListRef CachedElementsToManipulate;

	/** Bit array representing the visibility of every sprite category in the current viewport */
	TBitArray<>	SpriteCategoryVisibility;

	UWorld* World;

	FTrackingTransaction TrackingTransaction;

	/** Represents the last known drop preview mouse position. */
	int32 DropPreviewMouseX;
	int32 DropPreviewMouseY;

	/** If this view was controlled by another view this/last frame, don't update itself */
	bool bWasControlledByOtherViewport;

	/** Whether the user is currently using the rotation / translation widget */
	bool bCurrentlyEditingThroughMovementWidget;

	/**
	 * When locked to an actor this view will be positioned in the same location and rotation as the actor.
	 * If the actor has a camera component the view will also inherit camera settings such as aspect ratio, 
	 * FOV, post processing settings, and the like.
	 *
	 * This structure allows us to keep track of two actor locks: a normal actor lock, and a lock specifically
	 * for cinematic tools like Sequencer. A viewport locked to an actor by cinematics will always take 
	 * precedent over any other.
	 */
	struct FActorLockStack
	{
		/** Get the active lock info. Cinematics take precedence. */
		const FLevelViewportActorLock& GetLock(bool bAllowActorLock = true) const
		{
			if (CinematicActorLock.LockedActor.IsValid())
			{
				return CinematicActorLock;
			}
			return bAllowActorLock ? ActorLock : FLevelViewportActorLock::None;
		}

		/** Returns whether the given actor is used as one of our locks. */
		bool HasActorLocked(const AActor* InActor) const
		{
			return CinematicActorLock.LockedActor.Get() == InActor || ActorLock.LockedActor.Get() == InActor;
		}

		FLevelViewportActorLock CinematicActorLock;
		FLevelViewportActorLock ActorLock;
	};
	FActorLockStack ActorLocks;
	FActorLockStack PreviousActorLocks;

	/** Caching for expensive FindViewComponentForActor. Invalidated once per Tick. */
	static TMap<TObjectKey<AActor>, TWeakObjectPtr<UActorComponent>> ViewComponentForActorCache;

	/** If true, we switched between two different cameras. Set by cinematics, used by the motion blur to invalidate this frames motion vectors */
	bool					bEditorCameraCut;

	/** Stores the previous frame's value of bEditorCameraCut in order to reset it back to false on the next frame */
	bool					bWasEditorCameraCut;

	bool					bApplyCameraSpeedScaleByDistance;

	/** Handle to a timer event raised in ::ReceivedFocus*/
	FTimerHandle			FocusTimerHandle;
};
