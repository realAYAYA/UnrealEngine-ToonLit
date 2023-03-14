// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Containers/Ticker.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "LensFile.h"
#include "UObject/StrongObjectPtr.h"


class ACameraActor;
class ACompositingElement;
class FCameraCalibrationToolkit;
class SWidget;
class UMediaPlayer;
class UMediaTexture;
class UCameraCalibrationStep;
class UCompositingElementMaterialPass;
class ULensComponent;
class ULensDistortionModelHandlerBase;
class ULensFile;

struct FGeometry;
struct FPointerEvent;

/** Enumeration of overlay passes used to indicate which overlay pass to interact with */
enum class EOverlayPassType : uint8
{
	ToolOverlay = 0,
	UserOverlay = 1
};

/**
 * Controller for SCameraCalibrationSteps, where the calibration steps are hosted in.
 */
class FCameraCalibrationStepsController : public TSharedFromThis<FCameraCalibrationStepsController>
{
public:

	FCameraCalibrationStepsController(TWeakPtr<FCameraCalibrationToolkit> InCameraCalibrationToolkit, ULensFile* InLensFile);
	~FCameraCalibrationStepsController();

	/** Initialize resources. */
	void Initialize();

	/** Returns the UI that this object controls */
	TSharedPtr<SWidget> BuildUI();

	/** Creates composite with CG and selected media source */
	void CreateComp();

	/** Returns the render target of the Comp */
	UTextureRenderTarget2D* GetRenderTarget() const;

	/** Returns the render target of the Media Plate */
	UTextureRenderTarget2D* GetMediaPlateRenderTarget() const;

	/** Returns the size of the render target used by the Comp */
	FIntPoint GetCompRenderTargetSize() const;

	/** Creates a way to read the media plate pixels for processing by any calibration step */
	void CreateMediaPlateOutput();

	/** Returns the CG weight that is composited on top of the media */
	float GetWiperWeight() const;

	/** Sets the weight/alpha for that CG that is composited on top of the media. 0 means invisible. */
	void SetWiperWeight(float InWeight);

	/** Sets the camera used for the CG */
	void SetCamera(ACameraActor* InCamera);

	/** Returns the camera used for the CG */
	ACameraActor* GetCamera() const;

	/** Toggles the play/stop state of the media player */
	void TogglePlay();

	/** Returns true if the media player is paused */
	bool IsPaused() const;

	/** Set play on the media player */
	void Play();

	/** Set pause on the media player */
	void Pause();

	/** Returns the latest data used when evaluating the lens */
	FLensFileEvaluationInputs GetLensFileEvaluationInputs() const;

	/** Returns the LensFile that this tool is using */
	ULensFile* GetLensFile() const;

	/** Returns the first LensComponent attached to the CG camera whose LensFile matches the open asset */
	ULensComponent* FindLensComponent() const;

	/** Returns the distortion handler used to distort the CG being displayed in the simulcam viewport */
	const ULensDistortionModelHandlerBase* GetDistortionHandler() const;

	/** Sets the media source url to be played. Returns true if the url is a valid media source */
	bool SetMediaSourceUrl(const FString& InMediaSourceUrl);

	/** Finds available media sources and adds their urls to the given array */
	void FindMediaSourceUrls(TArray<TSharedPtr<FString>>& OutMediaSourceUrls) const;

	/** Gets the current media source url being played. Empty if None */
	FString GetMediaSourceUrl() const;

	/** Returns the calibration steps */
	const TConstArrayView<TStrongObjectPtr<UCameraCalibrationStep>> GetCalibrationSteps() const;

	/** Returns the calibration steps */
	void SelectStep(const FName& Name);

	/** Calculates the normalized (0~1) coordinates in the simulcam viewport of the given mouse click */
	bool CalculateNormalizedMouseClickPosition(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, FVector2D& OutPosition) const;

	/** Finds the world being used by the tool for finding and spawning objects */
	UWorld* GetWorld() const;

	/** Reads the pixels in the media plate */
	bool ReadMediaPixels(TArray<FColor>& Pixels, FIntPoint& Size, ETextureRenderTargetFormat& PixelFormat, FText& OutErrorMessage) const;

	/** Returns true if the overlay transform pass is currently enabled */
	bool IsOverlayEnabled(EOverlayPassType OverlayPass = EOverlayPassType::ToolOverlay) const;

	/** Sets the enabled state of the overlay transform pass */
	void SetOverlayEnabled(const bool bEnabled = true, EOverlayPassType OverlayPass = EOverlayPassType::ToolOverlay);

	/** Sets the overlay material to be used by the overlay transform pass */
	void SetOverlayMaterial(UMaterialInterface* OverlayMaterial, bool bShowOverlay = true, EOverlayPassType OverlayPass = EOverlayPassType::ToolOverlay);

	/** Redraw the overlay material used by the input overlay pass */
	void RefreshOverlay(EOverlayPassType OverlayPass = EOverlayPassType::ToolOverlay);

public:

	/** Called by the UI when the Simulcam Viewport is clicked */
	void OnSimulcamViewportClicked(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	/** Called by the UI when the Simulcam Viewport receives keyboard input */
	bool OnSimulcamViewportInputKey(const FKey& InKey, const EInputEvent& InEvent);

	/** Called by the UI when the rewind button is clicked */
	FReply OnRewindButtonClicked();

	/** Called by the UI when the reverse button is clicked */
	FReply OnReverseButtonClicked();

	/** Called by the UI when the step back button is clicked */
	FReply OnStepBackButtonClicked();

	/** Called by the UI when the play button is clicked */
	FReply OnPlayButtonClicked();

	/** Called by the UI when the pause button is clicked */
	FReply OnPauseButtonClicked();

	/** Called by the UI when the step forward button is clicked */
	FReply OnStepForwardButtonClicked();

	/** Called by the UI when the forward button is clicked */
	FReply OnForwardButtonClicked();

	/** Returns true if the media player supports seeking */
	bool DoesMediaSupportSeeking() const;

	/** Returns true if the media player supports a reverse rate that is faster than its current rate */
	bool DoesMediaSupportNextReverseRate() const;

	/** Returns true if the media player supports a forward rate that is faster than its current rate */
	bool DoesMediaSupportNextForwardRate() const;

	/** Computes the reverse playback rate */
	float GetFasterReverseRate() const;

	/** Computes the fast forward playback rate */
	float GetFasterForwardRate() const;

	/** Toggle the setting that controls whether the media playback buttons will be visible in the UI */
	void ToggleShowMediaPlaybackControls();

	/** Returns true if the media playback buttons are visible in the UI  */
	bool AreMediaPlaybackControlsVisible() const;

private:

	/** Returns the first lens component with a matching LensFile found on the input camera, or nullptr if none exists */
	ULensComponent* FindLensComponentOnCamera(ACameraActor* CineCamera) const;

	/** Returns a namespaced version of the given name. Useful to generate names unique to this lens file */
	FString NamespacedName(const FString&& Name) const;

	/** Finds an existing comp element based on its name. */
	ACompositingElement* FindElement(const FString& Name) const;

	/** Spawns a new compositing element of the given class and parent element */
	TWeakObjectPtr<ACompositingElement> AddElement(ACompositingElement* Parent, FString& ClassPath, FString& ElementName) const;

	/** Releases resources used by the tool */
	void Cleanup();

	/** Convenience function that returns the first camera it finds that is using the lens associated with this object. */
	ACameraActor* FindFirstCameraWithCurrentLens() const;

	/** Enables distortion in the CG comp */
	void EnableDistortionInCG();

	/** Called by the core ticker */
	bool OnTick(float DeltaTime);

	/** Finds and creates the available calibration steps */
	void CreateSteps();

	/** Create a new material transform pass to represent an overlay and add it to the comp */
	void CreateOverlayPass(FName PassName, TWeakObjectPtr<UCompositingElementMaterialPass>& OverlayPass, TWeakObjectPtr<UTextureRenderTarget2D>& OverlayRenderTarget);

	/** Returns the overlay material pass associated with the input overlay pass type */
	UCompositingElementMaterialPass* GetOverlayMaterialPass(EOverlayPassType OverlayPassType) const;

	/** Returns the overlay render target used by the input overlay pass type */
	UTextureRenderTarget2D* GetOverlayRenderTarget(EOverlayPassType OverlayPassType) const;

	/** Returns the overlay material used by the input overlay pass type */
	UMaterialInterface* GetOverlayMaterial(EOverlayPassType OverlayPass) const;

private:

	/** Pointer to the camera calibration toolkit */
	TWeakPtr<FCameraCalibrationToolkit> CameraCalibrationToolkit;

	/** Size to use when creating the render targets for the comp and media output */
	FIntPoint RenderTargetSize;

	/** Array of the calibration steps that this controller is managing */
	TArray<TStrongObjectPtr<UCameraCalibrationStep>> CalibrationSteps;

	/** The lens asset */
	TWeakObjectPtr<class ULensFile> LensFile;

	/** The parent comp element */
	TWeakObjectPtr<ACompositingElement> Comp;

	/** The CG layer comp element */
	TWeakObjectPtr<ACompositingElement> CGLayer;

	/** The MediaPlate comp element*/
	TWeakObjectPtr<ACompositingElement> MediaPlate;

	/** The output render target of the comp */
	TWeakObjectPtr<UTextureRenderTarget2D> RenderTarget;

	/** The output render target of the media plate */
	TWeakObjectPtr<UTextureRenderTarget2D> MediaPlateRenderTarget;

	/** The media texture used by the media plate */
	TWeakObjectPtr<UMediaTexture> MediaTexture;

	/** The media player that is playing the selected media source */
	TStrongObjectPtr<UMediaPlayer> MediaPlayer;

	/** The material pass the does the CG + MediaPlate composite with a wiper weight */
	TWeakObjectPtr<UCompositingElementMaterialPass> MaterialPass;

	/** A material pass set by one of the calibration steps that renders an overlay on top of the composite */
	TWeakObjectPtr<UCompositingElementMaterialPass> ToolOverlayPass;

	/** A material pass set by the user (via the editor UI) that renders an overlay on top of the composite */
	TWeakObjectPtr<UCompositingElementMaterialPass> UserOverlayPass;

	/** The material used to render the overlay for the tool overlay pass */
	TWeakObjectPtr<UMaterialInterface> ToolOverlayMaterial;

	/** The material used to render the overlay for the user overlay pass */
	TWeakObjectPtr<UMaterialInterface> UserOverlayMaterial;

	/** The render target used as input to the tool overlay material */
	TWeakObjectPtr<UTextureRenderTarget2D> ToolOverlayRenderTarget;

	/** The render target used as input to the user overlay material */
	TWeakObjectPtr<UTextureRenderTarget2D> UserOverlayRenderTarget;

	/** The currently selected camera */
	TWeakObjectPtr<ACameraActor> Camera;

	/** The delegate for the core ticker callback */
	FTSTicker::FDelegateHandle TickerHandle;

	/** Evaluation Data supplied by the Lens Component for the current frame. Only valid during the current frame. */
	FLensFileEvaluationInputs LensFileEvaluationInputs;

	/** Setting to control whether the media playback buttons are visible */
	bool bShowMediaPlaybackButtons = true;
};
