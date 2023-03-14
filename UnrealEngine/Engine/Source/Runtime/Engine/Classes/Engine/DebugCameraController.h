// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "FinalPostProcessSettings.h"
#include "SceneTypes.h"
#include "GameFramework/PlayerController.h"
#include "DebugCameraController.generated.h"

class ASpectatorPawn;

/**
* Camera controller that allows you to fly around a level mostly unrestricted by normal movement rules.
*
* To turn it on, please press Alt+C or both (left and right) analogs on XBox pad,
* or use the "ToggleDebugCamera" console command. Check the debug camera bindings
* in DefaultPawn.cpp for the camera controls.
*/
UCLASS(config=Game)
class ENGINE_API ADebugCameraController
	: public APlayerController
{
	GENERATED_UCLASS_BODY()

	/** Whether to show information about the selected actor on the debug camera HUD. */
	UPROPERTY(globalconfig)
	uint32 bShowSelectedInfo:1;

	/** Saves whether the FreezeRendering console command is active */
	UPROPERTY()
	uint32 bIsFrozenRendering:1;

	/** Whether to orbit selected actor. */
	UPROPERTY()
	uint32 bIsOrbitingSelectedActor : 1;

	/** When orbiting, true if using actor center as pivot, false if using last selected hitpoint */
	UPROPERTY()
	uint32 bOrbitPivotUseCenter:1;

	/** Whether set view mode to display GBuffer visualization overview */
	UPROPERTY()
	uint32 bEnableBufferVisualization : 1;

	/** Whether set view mode to display GBuffer visualization full */
	UPROPERTY()
	uint32 bEnableBufferVisualizationFullMode : 1;

	/** Whether GBuffer visualization overview inputs are set up  */
	UPROPERTY()
	uint32 bIsBufferVisualizationInputSetup : 1;

	/** Last display enabled setting before toggling buffer visualization overview */
	UPROPERTY()
	uint32 bLastDisplayEnabled : 1;

	/** Visualizes the frustum of the camera */
	UPROPERTY()
	TObjectPtr<class UDrawFrustumComponent> DrawFrustum;
	
	/** Sets whether to show information about the selected actor on the debug camera HUD.t */
	UFUNCTION(exec)
	virtual void ShowDebugSelectedInfo();

	/** Selects the object the camera is aiming at. */
	void SelectTargetedObject();

	/** Called when the user pressed the deselect key, just before the selected actor is cleared. */
	void Unselect();

	/** Speeds up camera movement */
	void IncreaseCameraSpeed();

	/** Slows down camera movement */
	void DecreaseCameraSpeed();

	/** Increases camera field of vision */
	void IncreaseFOV();

	/** Decreases camera field of vision */
	void DecreaseFOV();

	/** Toggles the display of debug info and input commands for the Debug Camera. */
	UFUNCTION(BlueprintCallable, Category="Debug Camera")
	void ToggleDisplay();

	/** Sets display of debug info and input commands. */
	void SetDisplay(bool bEnabled);

	/** Returns whether debug info display is enabled */
	bool IsDisplayEnabled();

	/**
	 * function called from key bindings command to save information about
	 * turning on/off FreezeRendering command.
	 */
	virtual void ToggleFreezeRendering();

	/** Method called prior to processing input */
	virtual void PreProcessInput(const float DeltaTime, const bool bGamePaused);

	/**
	 * Updates the rotation of player, based on ControlRotation after RotationInput has been applied.
	 * This may then be modified by the PlayerCamera, and is passed to Pawn->FaceRotation().
	 */
	virtual void UpdateRotation(float DeltaTime) override;

	/** Pre process input when orbiting */
	void PreProcessInputForOrbit(const float DeltaTime, const bool bGamePaused);

	/** Updates the rotation and location of player when orbiting */
	void UpdateRotationForOrbit(float DeltaTime);

	/** Gets pivot to use when orbiting */
	bool GetPivotForOrbit(FVector& PivotLocation) const;

	/** Toggles camera orbit */
	void ToggleOrbit(bool bOrbitCenter);

	/** Toggles camera orbit center */
	void ToggleOrbitCenter();

	/** Toggles camera orbit hitpoint */
	void ToggleOrbitHitPoint();

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	/** Whether buffer visualization option should be enabled */
	static bool EnableDebugBuffers();

	/** Whether cycle viewmode option should be enabled */
	static bool EnableDebugViewmodes();

	/** Cycle view mode */
	void CycleViewMode();

	/** Toggle buffer visualization overview */
	void ToggleBufferVisualizationOverviewMode();

	/** Buffer overview move up */
	void BufferVisualizationMoveUp();

	/** Buffer overview move down */
	void BufferVisualizationMoveDown();

	/** Buffer overview move right */
	void BufferVisualizationMoveRight();

	/** Buffer overview move left */
	void BufferVisualizationMoveLeft();

	/** Ignores axis motion */
	void ConsumeAxisMotion(float Val);

	/** Toggle buffer visualization full display */
	void ToggleBufferVisualizationFullMode();

	/** Set buffer visualization full mode */
	void SetBufferVisualizationFullMode(bool bFullMode);

	/** Update visualize buffer post processing settings */
	void UpdateVisualizeBufferPostProcessing(FFinalPostProcessSettings& InOutPostProcessingSettings);

	/** Get visualization buffer's material name used by post processing settings */
	FString GetBufferMaterialName(const FString& InBuffer);

	/** Get current visualization buffer's material name */
	FString GetSelectedBufferMaterialName();


#endif

public:

	/** Currently selected actor, may be invalid */
	UPROPERTY()
	TWeakObjectPtr<class AActor> SelectedActor;

	/** Returns the currently selected actor, or null if it is invalid or not set */
	UFUNCTION(BlueprintCallable, Category="Debug Camera")
	AActor* GetSelectedActor() const;
	
	/** Currently selected component, may be invalid */
	UPROPERTY()
	TWeakObjectPtr<class UPrimitiveComponent> SelectedComponent;

	/** Selected hit point */
	UPROPERTY()
	FHitResult SelectedHitPoint;

	/** Controller that was active before this was spawned */
	UPROPERTY()	
	TObjectPtr<class APlayerController> OriginalControllerRef;

	/** Player object that was active before this was spawned */
	UPROPERTY()	
	TObjectPtr<class UPlayer> OriginalPlayer;

	/** Allows control over the speed of the spectator pawn. This scales the speed based on the InitialMaxSpeed. Use Set Pawn Movement Speed Scale during runtime */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Debug Camera")
	float SpeedScale;
	
	/** Sets the pawn movement speed scale. */
	UFUNCTION(BlueprintCallable, Category="Debug Camera")
	void SetPawnMovementSpeedScale(float NewSpeedScale);
	
	/** Initial max speed of the spectator pawn when we start possession. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Debug Camera")
	float InitialMaxSpeed;

	/** Initial acceleration of the spectator pawn when we start possession. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Debug Camera")
	float InitialAccel;

	/** Initial deceleration of the spectator pawn when we start possession. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Debug Camera")
	float InitialDecel;

protected:

	// Adjusts movement speed limits based on SpeedScale.
	virtual void ApplySpeedScale();
	virtual void SetupInputComponent() override;

	/** Sets up or clears input for buffer visualization overview */
	void SetupBufferVisualizationOverviewInput();

public:

	/** 
	* Function called on activation of debug camera controller.
	* @param OriginalPC The active player controller before this debug camera controller was possessed by the player.
	*/
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "OnActivate"))
	void ReceiveOnActivate(class APlayerController* OriginalPC);

	/** Function called on activation debug camera controller */
	virtual void OnActivate(class APlayerController* OriginalPC);
	
	/** 
	* Function called on deactivation of debug camera controller.
	* @param RestoredPC The Player Controller that the player input is being returned to.
	*/
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "OnDeactivate"))
	void ReceiveOnDeactivate(class APlayerController* RestoredPC);

	/** Function called on deactivation debug camera controller */
	virtual void OnDeactivate(class APlayerController* RestoredPC);

	/**
	 * Builds a list of components that are hidden based upon gameplay
	 *
	 * @param ViewLocation the view point to hide/unhide from
	 * @param HiddenComponents the list to add to/remove from
	 */
	virtual void UpdateHiddenComponents(const FVector& ViewLocation,TSet<FPrimitiveComponentId>& HiddenComponents) override;

public:

	// APlayerController Interface

	virtual void PostInitializeComponents() override;
	virtual FString ConsoleCommand(const FString& Command, bool bWriteToLog = true) override;
	virtual void AddCheats(bool bForce) override;
	virtual void EndSpectatingState() override;
	/** Custom spawn to spawn a default SpectatorPawn, to use as a spectator and initialize it. By default it is spawned at the PC's current location and rotation. */
	virtual ASpectatorPawn* SpawnSpectatorPawn() override;

protected:

	/**
	 * Called when an actor has been selected with the primary key (e.g. left mouse button).
	 *
	 * The selection trace starts from the center of the debug camera's view.
	 *
	 * @param SelectHitLocation The exact world-space location where the selection trace hit the New Selected Actor.
	 * @param SelectHitNormal The world-space surface normal of the New Selected Actor at the hit location.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category="Debug Camera", meta=(DisplayName="OnActorSelected"))
	void ReceiveOnActorSelected(AActor* NewSelectedActor, const FVector& SelectHitLocation, const FVector& SelectHitNormal, const FHitResult& Hit);
	
	/**
	 * Called when an actor has been selected with the primary key (e.g. left mouse button).
	 * @param Hit	Info struct for the selection point.
	 */
	virtual void Select( FHitResult const& Hit );

	virtual void SetSpectatorPawn(class ASpectatorPawn* NewSpectatorPawn) override;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)

	/** Get buffer visualization overview targets based on console var */
	TArray<FString> GetBufferVisualizationOverviewTargets();

	/** Get next buffer */
	void GetNextBuffer(const TArray<FString>& OverviewBuffers, int32 Step = 1);

	/** Get next buffer */
	void GetNextBuffer(int32 Step = 1);

#endif

private:

	/** The normalized screen location when a drag starts */
	FVector2D LastTouchDragLocation;

	/** Last position for orbit */
	FVector LastOrbitPawnLocation;

	/** Current orbit pivot, if orbit is enabled */
	FVector OrbitPivot;

	/** Current orbit radius, if orbit is enabled */
	float OrbitRadius;

	/** Last index in settings array for cycle view modes */
	int32 LastViewModeSettingsIndex;

	/** Buffer selected in buffer visualization overview or full screen view */
	FString CurrSelectedBuffer;

	void OnTouchBegin(ETouchIndex::Type FingerIndex, FVector Location);
	void OnTouchEnd(ETouchIndex::Type FingerIndex, FVector Location);
	void OnFingerMove(ETouchIndex::Type FingerIndex, FVector Location);
};
