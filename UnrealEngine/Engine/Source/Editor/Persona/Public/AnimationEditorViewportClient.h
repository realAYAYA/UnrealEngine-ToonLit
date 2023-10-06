// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/IDelegateInstance.h"
#include "Misc/Guid.h"
#include "InputCoreTypes.h"
#include "HitProxies.h"
#include "UnrealWidgetFwd.h"
#include "EditorViewportClient.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "IPersonaPreviewScene.h"
#include "Preferences/PersonaOptions.h"
#include "SkeletalDebugRendering.h"

class FCanvas;
class UPersonaOptions;
class USkeletalMeshSocket;
struct FCompactHeapPose;
struct FSkelMeshRenderSection;

DECLARE_DELEGATE_OneParam(FOnBoneSizeSet, float);
DECLARE_DELEGATE_RetVal(float, FOnGetBoneSize)

//////////////////////////////////////////////////////////////////////////
// ELocalAxesMode

namespace ELocalAxesMode
{
	enum Type
	{
		None,
		Selected,
		All,
		NumAxesModes
	};
};

//////////////////////////////////////////////////////////////////////////
// ELocalAxesMode

namespace EDisplayInfoMode
{
	enum Type
	{
		None,
		Basic,	
		Detailed,
		SkeletalControls,
		NumInfoModes
	};
};

namespace EAnimationPlaybackSpeeds
{
	enum Type
	{
		OneTenth = 0,
		Quarter,
		Half,
		ThreeQuarters,
		Normal,
		Double,
		FiveTimes,
		TenTimes,
		Custom,
		NumPlaybackSpeeds
	};

	extern float Values[NumPlaybackSpeeds];
};

/////////////////////////////////////////////////////////////////////////
// FAnimationViewportClient

class PERSONA_API FAnimationViewportClient : public FEditorViewportClient, public TSharedFromThis<FAnimationViewportClient>
{
protected:

	/** Function to display bone names*/
	void ShowBoneNames(FCanvas* Canvas, FSceneView* View, UDebugSkelMeshComponent* MeshComponent);

	/** Function to display transform attribute names*/
	void ShowAttributeNames(FCanvas* Canvas, FSceneView* View, UDebugSkelMeshComponent* MeshComponent) const;

	/** Function to display debug lines generated from skeletal controls in animBP mode */
	void DrawNodeDebugLines(TArray<FText>& Lines, FCanvas* Canvas, FSceneView* View);

public:
	FAnimationViewportClient(const TSharedRef<class IPersonaPreviewScene>& InPreviewScene, const TSharedRef<class SAnimationEditorViewport>& InAnimationEditorViewport, const TSharedRef<class FAssetEditorToolkit>& InAssetEditorToolkit, int32 InViewportIndex, bool bInShowStats);
	virtual ~FAnimationViewportClient();

	void Initialize();

	// FEditorViewportClient interface
	virtual void Tick(float DeltaSeconds) override;
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI) override;
	virtual void DrawCanvas( FViewport& InViewport, FSceneView& View, FCanvas& Canvas ) override;
	
	UE_DEPRECATED(5.1, "This version of InputKey is deprecated. Please use the version that takes EventArgs instead.")
	virtual bool InputKey(FViewport* Viewport, int32 ControllerId, FKey Key, EInputEvent Event, float AmountDepressed = 1.f, bool bGamepad=false) override;
	virtual bool InputKey(const FInputKeyEventArgs& EventArgs) override;
	
	UE_DEPRECATED(5.1, "This version of InputAxis is deprecated. Please use the version that takes DeviceId instead.")
	virtual bool InputAxis(FViewport* InViewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime, int32 NumSamples = 1, bool bGamepad = false) override;
	virtual bool InputAxis(FViewport* InViewport, FInputDeviceId DeviceId, FKey Key, float Delta, float DeltaTime, int32 NumSamples = 1, bool bGamepad = false) override;
	
//	virtual void ProcessClick(class FSceneView& View, class HHitProxy* HitProxy, FKey Key, EInputEvent Event, uint32 HitX, uint32 HitY) override;
//	virtual bool InputWidgetDelta( FViewport* Viewport, EAxisList::Type CurrentAxis, FVector& Drag, FRotator& Rot, FVector& Scale ) override;
	virtual void TrackingStarted( const struct FInputEventState& InInputState, bool bIsDragging, bool bNudge ) override;
	virtual void TrackingStopped() override;
//	virtual UE::Widget::EWidgetMode GetWidgetMode() const override;
//	virtual void SetWidgetMode(UE::Widget::EWidgetMode InWidgetMode) override;
//	virtual bool CanSetWidgetMode(UE::Widget::EWidgetMode NewMode) const override;
	virtual FVector GetWidgetLocation() const override;
	virtual FMatrix GetWidgetCoordSystem() const override;
	virtual ECoordSystem GetWidgetCoordSystemSpace() const override;
	virtual void SetWidgetCoordSystemSpace(ECoordSystem NewCoordSystem) override;
	virtual void SetViewMode(EViewModeIndex InViewModeIndex) override;
	virtual void SetViewportType(ELevelViewportType InViewportType) override;
	virtual void RotateViewportType() override;
	virtual bool CanCycleWidgetMode() const override;
	virtual void SetupViewForRendering( FSceneViewFamily& ViewFamily, FSceneView& View ) override;
	virtual void HandleToggleShowFlag(FEngineShowFlags::EShowFlag EngineShowFlagIndex) override;
	virtual FMatrix CalcViewRotationMatrix(const FRotator& InViewRotation) const override;
	// End of FEditorViewportClient interface

	/** Draw call to render UV overlay */
	void DrawUVsForMesh(FViewport* InViewport, FCanvas* InCanvas, int32 InTextYPos, UDebugSkelMeshComponent* MeshComponent);

	/** Set the camera follow mode */
	void SetCameraFollowMode(EAnimationViewportCameraFollowMode Mode, FName InBoneName = NAME_None);

	/** Called when viewport focuses on a selection */
	void OnFocusViewportToSelection();

	/** Get the camera follow mode */
	EAnimationViewportCameraFollowMode GetCameraFollowMode() const;

	/** Get the bone name to use when CameraFollowMode is EAnimationViewportCameraFollowMode::Bone */
	FName GetCameraFollowBoneName() const;

	/** Jump to the meshes default camera */
	void JumpToDefaultCamera();

	/** Save current camera as default for mesh */
	void SaveCameraAsDefault();

	/** Check whether we can save this camera as default */
	bool CanSaveCameraAsDefault() const;

	/** Clear any default camera for mesh */
	void ClearDefaultCamera();

	/** Returns whether we have a default camera set for this mesh */
	bool HasDefaultCameraSet() const;

	/** Handle the skeletal mesh mesh component being used for preview changing */
	void HandleSkeletalMeshChanged(class USkeletalMesh* OldSkeletalMesh, class USkeletalMesh* NewSkeletalMesh);

	/** Handle a change in the skeletal mesh mesh component being used for preview changing */
	void HandleOnMeshChanged();

	/** Handle a change in the skeletal mesh phusics component being used for preview changing */
	void HandleOnSkelMeshPhysicsCreated();

	/** Function to display bone names*/
	void ShowBoneNames(FViewport* Viewport, FCanvas* Canvas, UDebugSkelMeshComponent* MeshComponent);

	/** Function to enable/disable floor auto align */
	void OnToggleAutoAlignFloor();

	/** Function to check whether floor is auto align or not */
	bool IsAutoAlignFloor() const;

	/** Function to mute/unmute audio in the viewport */
	void OnToggleMuteAudio();

	/** Function to check whether audio is muted or not */
	bool IsAudioMuted() const;

	/** Set whether to use audio attenuation */
	void OnToggleUseAudioAttenuation();

	/** Check whether we are using audio attenuation */
	bool IsUsingAudioAttenuation() const;

	/** Function to set background color */
	void SetBackgroundColor(FLinearColor InColor);

	/** Function to get current brightness value */ 
	float GetBrightnessValue() const;

	/** Function to set brightness value */
	void SetBrightnessValue(float Value);

	/** Function to set Local axes mode for the ELocalAxesType */
	void SetLocalAxesMode(ELocalAxesMode::Type AxesMode);

	/** Local axes mode checking function for the ELocalAxesType */
	bool IsLocalAxesModeSet(ELocalAxesMode::Type AxesMode) const;

	/** Get the Bone local axis mode */
	ELocalAxesMode::Type GetLocalAxesMode() const;

	/** Access Bone Draw size config option*/
	void SetBoneDrawSize(const float InBoneDrawSize);
	float GetBoneDrawSize() const;

	/** Access CustomAnimationSpeed config option*/
	void SetCustomAnimationSpeed(const float InCustomAnimationSpeed);
	float GetCustomAnimationSpeed() const;

	/** Function to set Bone Draw  mode for the EBoneDrawType */
	void SetBoneDrawMode(EBoneDrawMode::Type AxesMode);

	/** Bone Draw  mode checking function for the EBoneDrawType */
	bool IsBoneDrawModeSet(EBoneDrawMode::Type AxesMode) const;

	/** Get the Bone local axis mode */
	EBoneDrawMode::Type GetBoneDrawMode() const;
	
	/** Returns the desired target of the camera */
	FSphere GetCameraTarget();

	/** Sets up the viewports camera (look-at etc) based on the current preview target*/
	void UpdateCameraSetup();

	/* Places the viewport camera at a good location to view the supplied sphere */
	void FocusViewportOnSphere( FSphere& Sphere, bool bInstant = true );

	/* Places the viewport camera at a good location to view the preview target */
	void FocusViewportOnPreviewMesh(bool bUseCustomCamera);

	/** Callback for toggling the normals show flag. */
	void ToggleCPUSkinning();

	/** Callback for checking the normals show flag. */
	bool IsSetCPUSkinningChecked() const;

	/** Toggles whether to lock the camera's rotation to a specified bone's orientation */
	void ToggleRotateCameraToFollowBone();

	/** Whether or not to lock the camera's rotation to a specified bone's orientation */
	bool GetShouldRotateCameraToFollowBone() const;

	/** Callback for toggling the normals show flag. */
	void ToggleShowNormals();

	/** Callback for checking the normals show flag. */
	bool IsSetShowNormalsChecked() const;

	/** Callback for toggling the tangents show flag. */
	void ToggleShowTangents();

	/** Callback for checking the tangents show flag. */
	bool IsSetShowTangentsChecked() const;

	/** Callback for toggling the binormals show flag. */
	void ToggleShowBinormals();

	/** Callback for checking the binormals show flag. */
	bool IsSetShowBinormalsChecked() const;

	/** Callback for toggling UV drawing in the viewport */
	void SetDrawUVOverlay(bool bInDrawUVs);

	/** Callback for checking whether the UV drawing is switched on. */
	bool IsSetDrawUVOverlayChecked() const;

	/** Returns the UV Channel that will be drawn when Draw UV Overlay is turned on */
	int32 GetUVChannelToDraw() const { return UVChannelToDraw; }

	/** Sets the UV Channel that will be drawn when Draw UV Overlay is turned on */
	void SetUVChannelToDraw(int32 UVChannel) { UVChannelToDraw = UVChannel; }

	/* Returns the floor height offset */	
	float GetFloorOffset() const;

	/* Sets the floor height offset, saves it to config and invalidates the viewport so it shows up immediately */
	void SetFloorOffset(float NewValue);

	/** Function to set mesh stat drawing state */
	void OnSetShowMeshStats(int32 ShowMode);
	/** Whether or not mesh stats are being displayed */
	bool IsShowingMeshStats() const;
	/** Whether or not selected node stats are being displayed */
	bool IsShowingSelectedNodeStats() const;
	/** Whether detailed mesh stats are being displayed or basic mesh stats */
	bool IsDetailedMeshStats() const;

	int32 GetShowMeshStats() const;

	/** Set the playback speed mode */
	void SetPlaybackSpeedMode(EAnimationPlaybackSpeeds::Type InMode);

	/** Get the playback speed mode */
	EAnimationPlaybackSpeeds::Type GetPlaybackSpeedMode() const;

	/** Get the preview scene we are viewing */
	TSharedRef<class IPersonaPreviewScene> GetPreviewScene() const { return PreviewScenePtr.ToSharedRef(); }

	/** Get the asset editor we are embedded in */
	TSharedRef<class FAssetEditorToolkit> GetAssetEditorToolkit() const { return AssetEditorToolkitPtr.Pin().ToSharedRef(); }

	/* Handle error checking for additive base pose */
	bool ShouldDisplayAdditiveScaleErrorMessage() const;

	/** Draws Mesh Sockets in foreground - bUseSkeletonSocketColor = true for grey (skeleton), false for red (mesh) **/
	static void DrawSockets(const UDebugSkelMeshComponent* InPreviewMeshComponent, TArray<USkeletalMeshSocket*>& InSockets, FSelectedSocketInfo InSelectedSocket, FPrimitiveDrawInterface* PDI, bool bUseSkeletonSocketColor);

	/** Draws Gizmo for the Transform in foreground **/
	static void RenderGizmo(const FTransform& Transform, FPrimitiveDrawInterface* PDI);

	/** Function to display warning and info text on the viewport when outside of animBP mode */
	FText GetDisplayInfo( bool bDisplayAllInfo ) const;

	/** Get the viewport index (0-3) for this client */
	int32 GetViewportIndex() const { return ViewportIndex; }

	/** Get the persona mode manager */
	UE_DEPRECATED(5.1, "Use the UPersonaEditorModeManagerContext object stored in the editor mode tools' context store instead.")
	class IPersonaEditorModeManager* GetPersonaModeManager() const;

private:
	/**
	 * Updates the audio listener for this viewport 
	 *
	 * @param View	The scene view to use when calculate the listener position
	 */
	void UpdateAudioListener(const FSceneView& View);

public:

	/** persona config options **/
	UPersonaOptions* ConfigOption;

	/** allow client code to store/serialize bone size if desired */
	FOnBoneSizeSet OnSetBoneSize;
	FOnGetBoneSize OnGetBoneSize;

private:
	/** Shared pointer back to the preview scene we are viewing 
	* Workaround fix for FORT-495476, UE-159733, UE-160424, UE-145060
	* We hold a shared because if the PreviewScene gets destroyed before we reach
	* this class destructor, we can not unregister the callbacks from this class
	* and we get crashes when any of the callbacks is triggered afterwards
	*/
	TSharedPtr<class IPersonaPreviewScene> PreviewScenePtr;

	/** Weak pointer back to asset editor we are embedded in */
	TWeakPtr<class FAssetEditorToolkit> AssetEditorToolkitPtr;

	// Current widget mode
	UE::Widget::EWidgetMode WidgetMode;

	/** The current camera follow mode */
	EAnimationViewportCameraFollowMode CameraFollowMode;

	/** The bone we will follow when in EAnimationViewportCameraFollowMode::Bone */
	FName CameraFollowBoneName;

	/** Should we auto align floor to mesh bounds */
	bool bAutoAlignFloor;

	/** Whether to lock the camera's rotation to a specified bone's orientation */
	bool bRotateCameraToFollowBone;

	/** User selected color using color picker */
	FLinearColor SelectedHSVColor;

	/** Selected playback speed mode, used for deciding scale */
	EAnimationPlaybackSpeeds::Type AnimationPlaybackSpeedMode;

	/** Flag for displaying the UV data in the viewport */
	bool bDrawUVs;

	/** Which UV channel to draw */
	int32 UVChannelToDraw;

	enum GridParam
	{
		MinCellCount = 64,
		MinGridSize = 2,
		MaxGridSize	= 50,
	};

	/** Focus on the preview component the next time we draw the viewport */
	bool bFocusOnDraw;
	bool bFocusUsingCustomCamera;

	/** Handle additive anim scale validation */
	mutable bool bDoesAdditiveRefPoseHaveZeroScale;
	mutable FGuid RefPoseGuid;

	/** Screen size cached when we draw */
	float CachedScreenSize;

	/* Member use to unregister OnPhysicsCreatedDelegate */
	FDelegateHandle OnPhysicsCreatedDelegateHandle;

	/* Member use to unregister OnMeshChanged for our preview skeletal mesh */
	FDelegateHandle OnMeshChangedDelegateHandle;

private:

	void SetCameraTargetLocation(const FSphere &BoundSphere, float DeltaSeconds);

	/** Draws Mesh Bones in foreground **/
	void DrawMeshBones(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const;
	/** Draws the given array of transforms as bones */
	void DrawBonesFromTransforms(TArray<FTransform>& Transforms, UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI, FLinearColor BoneColour, FLinearColor RootBoneColour) const;
	/** Draws Bones for a compact pose */
	void DrawBonesFromCompactPose(const FCompactHeapPose& Pose, UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI, const FLinearColor& DrawColor) const;
	/** Draws Bones for uncompressed animation **/
	void DrawMeshBonesUncompressedAnimation(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const;
	/** Draw Bones for non retargeted animation. */
	void DrawMeshBonesNonRetargetedAnimation(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const;
	/** Draws Bones for Additive Base Pose */
	void DrawMeshBonesAdditiveBasePose(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const;
	/** Draw Bones for non retargeted animation. */
	void DrawMeshBonesSourceRawAnimation(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const;
	/** Draw Bones for non retargeted animation. */
	void DrawMeshBonesBakedAnimation(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI) const;
	/** Draw Bones from skeleton reference pose. */
	void DrawBonesFromSkeleton(const USkeleton* Skeleton, const TArray<int32>& InSelectedBones, FPrimitiveDrawInterface* PDI) const;
	/** Draws Bones for RequiredBones with WorldTransform **/
	void DrawBones(
		const FVector& ComponentOrigin,
		const TArray<FBoneIndexType>& RequiredBones,
		const FReferenceSkeleton& RefSkeleton,
		const TArray<FTransform>& WorldTransforms,
		const TArray<int32>& InSelectedBones,
		const TArray<FLinearColor>& BoneColors,
		FPrimitiveDrawInterface* PDI,
		bool bForceDraw,
		bool bAddHitProxy) const;
	/** Draw Sub set of Bones **/
	void DrawMeshSubsetBones(const UDebugSkelMeshComponent* MeshComponent, const TArray<int32>& BonesOfInterest, FPrimitiveDrawInterface* PDI) const;

	/** Draws active transform attributes */
	void DrawAttributes(UDebugSkelMeshComponent* MeshComponent, FPrimitiveDrawInterface* PDI) const;

	/** Draws bones from watched poses*/
	void DrawWatchedPoses(UDebugSkelMeshComponent * MeshComponent, FPrimitiveDrawInterface* PDI);

	/** Get the typed anim preview scene shared ptr*/
	TSharedPtr<class FAnimationEditorPreviewScene> GetAnimPreviewScenePtr() const;

	/** Get the typed anim preview scene */
	TSharedRef<class FAnimationEditorPreviewScene> GetAnimPreviewScene() const;

	/** Invalidate this view in response to a preview scene change */
	void HandleInvalidateViews();

	/** Handle the view being focused from the preview scene */
	void HandleFocusViews();

	/** Delegate for preview profile is changed (used for updating show flags) */
	void OnAssetViewerSettingsChanged(const FName& InPropertyName);

	/** Sets up the ShowFlag according to the current preview scene profile */
	void SetAdvancedShowFlagsForScene(const bool bAdvancedShowFlags);
	
	/** Computes a bounding box for the selected section of the preview mesh component.
	    If there is no selected section, returns an empty box. */
	FBox ComputeBoundingBoxForSelectedEditorSection() const;

	/** Converts all local vertex positions to world positions. */
	void TransformVertexPositionsToWorld(TArray<FFinalSkinVertex>& LocalVertices) const;

	/** Gets all vertex indices that the given section references. */
	void GetAllVertexIndicesUsedInSection(const FRawStaticIndexBuffer16or32Interface& IndexBuffer,
										  const FSkelMeshRenderSection& SkelMeshSection,
										  TArray<int32>& OutIndices) const;

	/** Used for camera tracking - store data about the scene pre-tick */
	void HandlePreviewScenePreTick();

	/** Used for camera tracking - use stored data about the scene post-tick */
	void HandlePreviewScenePostTick();

private:
	/** Custom Animation speed in the viewport. Transient setting. */
	float CustomAnimationSpeed = 1.0f;

	/** Size to draw bones in the viewport. Transient setting. */
	float BoneDrawSize=1.0f;
	
	/** Allow mesh stats to be disabled for specific viewport instances */
	bool bShowMeshStats;

	/** Whether we have initially focused on the preview mesh */
	bool bInitiallyFocused;

	/** When orbiting, the base rotation of the camera, allowing orbiting around different axes to Z */
	FQuat OrbitRotation;

	// We allow for replacing this in the underlying client so we cache it here
	FEditorCameraController* CachedDefaultCameraController;

	/** Index (0-3) of this viewport */
	int32 ViewportIndex;

	/** The last location the camera was told to look at */
	FVector LastLookAtLocation;

	// Delegate Handler to allow changing of camera controller
	void OnCameraControllerChanged();

	/** True when the preview animation should resume playing upon finishing tracking */
	bool bResumeAfterTracking;
};
