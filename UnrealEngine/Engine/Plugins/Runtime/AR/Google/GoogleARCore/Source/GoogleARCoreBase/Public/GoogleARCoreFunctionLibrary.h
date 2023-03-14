// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/LatentActionManager.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "GoogleARCoreTypes.h"
#include "GoogleARCoreAugmentedFace.h"
#include "GoogleARCoreAugmentedImage.h"
#include "GoogleARCoreSessionConfig.h"
#include "GoogleARCoreFunctionLibrary.generated.h"

/** A function library that provides static/Blueprint functions associated with GoogleARCore session.*/
UCLASS()
class GOOGLEARCOREBASE_API UGoogleARCoreSessionFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	//-----------------Lifecycle---------------------

	/**
	 * A Latent Action to check the availability of ARCore on this device.
	 * This may initiate a query with a remote service to determine if the device is supported by ARCore. The Latent Action will complete when the check is finished.
	 *
	 * @param WorldContextObject	The world context.
	 * @param LatentInfo			Unreal internal type required for all latent actions.
	 * @param OutAvailability		The availability result as a EGoogleARCoreAvailability.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use CheckARServiceAvailability from UARDependencyHandler.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|Availability", meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject", Keywords = "googlear arcore availability", DeprecatedFunction, DeprecationMessage="This function is deprecated, use CheckARServiceAvailability from UARDependencyHandler."))
	static void CheckARCoreAvailability(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, EGoogleARCoreAvailability& OutAvailability);

	/**
	 * A Latent Action to initiates installation of ARCore if required.
	 * This function may cause your application be paused if installing ARCore is required.
	 *
	 * @param WorldContextObject	The world context.
	 * @param LatentInfo			Unreal internal type required for all latent actions.
	 * @param OutInstallResult		The install request result as a EGoogleARCoreInstallRequestResult.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use InstallARService from UARDependencyHandler.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|Availability", meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject", Keywords = "googlear arcore availability", DeprecatedFunction, DeprecationMessage="This function is deprecated, use InstallARService from UARDependencyHandler."))
	static void InstallARCoreService(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, EGoogleARCoreInstallRequestResult& OutInstallResult);

	/**
	 * A polling function to check the ARCore availability in C++.
	 * This may initiate a query with a remote service to determine if the device is supported by ARCore, so this function will EGoogleARCoreAvailability::UnknownChecking.
	 *
	 * @return	The availability result as a EGoogleARCoreAvailability.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use CheckARServiceAvailability from UARDependencyHandler.")
	static EGoogleARCoreAvailability CheckARCoreAvailableStatus();

	/**
	 * Initiates installation of ARCore if required.
	 * This function will return immediately and may pause your application if installing ARCore is required.
	 *
	 * @return EGoogleARCoreInstallStatus::Requrested if it started a install request.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use InstallARService from UARDependencyHandler.")
	static EGoogleARCoreInstallStatus RequestInstallARCoreAPK();

	/**
	 * A polling function to check the ARCore install request result in C++.
	 * After you call RequestInstallARCoreAPK() and it returns EGoogleARCoreInstallStatus::Requrested. You can call this function to check the install requst result.
	 *
	 * @return The install request result as a EGoogleARCoreInstallRequestResult.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use CheckARServiceAvailability from UARDependencyHandler.")
	static EGoogleARCoreInstallRequestResult GetARCoreAPKInstallResult();

	/**
	 * Get the UGoogleARCoreEventManager to bind BP events or c++ delegate in GoogleARCore plugins.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use GetSupportedVideoFormats.")
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|Session", meta = (Keywords = "googlear arcore event manager", DeprecatedFunction, DeprecationMessage="This function is deprecated, use GetSupportedVideoFormats."))
	static UGoogleARCoreEventManager* GetARCoreEventManager();

	/**
	 * Starts a new ARCore tracking session GoogleARCore specific configuration.
	 * If the session already started and the config isn't the same, it will stop the previous session and start a new session with the new config.
	 * Note that this is a latent action, you can query the session start result by querying GetARCoreSessionStatus() after the latent action finished.
	 *
	 * @param WorldContextObject	The world context.
	 * @param LatentInfo			Unreal internal type required for all latent actions.
	 * @param Configuration			The ARCoreSession configuration to start the session.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use StartARSession.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|Session", meta = (Latent, LatentInfo = "LatentInfo", WorldContext = "WorldContextObject", Keywords = "googlear arcore session start config", DeprecatedFunction, DeprecationMessage="This function is deprecated, use StartARSession."))
	static void StartARCoreSession(UObject* WorldContextObject, struct FLatentActionInfo LatentInfo, UGoogleARCoreSessionConfig* Configuration);

	/**
	 * Configure the ARCoreSession with the desired camera configuration. The TargetCameraConfig must be
	 * from a list returned by UGoogleARCoreEventManager::OnCameraConfig delegate.
	 *
	 * This function should be called when UGoogleARCoreEventManager::OnCameraConfig delegate got triggered.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use UARSessionConfig::SetDesiredVideoFormat.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|Session", meta = (Keywords = "googlear arcore camera config", DeprecatedFunction, DeprecationMessage="This function is deprecated, use UARSessionConfig::SetDesiredVideoFormat."))
	static bool SetARCoreCameraConfig(FGoogleARCoreCameraConfig TargetCameraConfig);

	/**
	 * Get the FGoogleARCoreCameraConfig that the current ARCore session is using.
	 *
	 * @param OutCurrentCameraConfig   The FGoogleARCoreCameraConfig that the current ARCore session is using.
	 * @return  True if there is a valid ARCore session and the current camera config is returned.
	 *          False if ARCore session hasn't been started or it is already stopped.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use GetSupportedVideoFormats.")
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|Session", meta = (Keywords = "googlear arcore camera config", DeprecatedFunction, DeprecationMessage="This function is deprecated, use GetSupportedVideoFormats."))
	static bool GetARCoreCameraConfig(FGoogleARCoreCameraConfig& OutCurrentCameraConfig);

	//-----------------PassthroughCamera---------------------
	/**
	 * Returns the state of the passthrough camera rendering in GoogleARCore ARSystem.
	 *
	 * @return	True if the passthrough camera rendering is enabled.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use UARSessionConfig::ShouldRenderCameraOverlay.")
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|PassthroughCamera", meta = (Keywords = "googlear arcore passthrough camera", DeprecatedFunction, DeprecationMessage="This function is deprecated, use UARSessionConfig::ShouldRenderCameraOverlay."))
	static bool IsPassthroughCameraRenderingEnabled();

	/**
	 * Enables/Disables the passthrough camera rendering in GoogleARCore ARSystem.
	 * Note that when passthrough camera rendering is enabled, the camera FOV will be forced
	 * to match FOV of the physical camera on the device.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use UARSessionConfig::bEnableAutomaticCameraOverlay.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|PassthroughCamera", meta = (Keywords = "googlear arcore passthrough camera", DeprecatedFunction, DeprecationMessage="This function is deprecated, use UARSessionConfig::bEnableAutomaticCameraOverlay."))
	static void SetPassthroughCameraRenderingEnabled(bool bEnable);

	/**
	 * Gets the texture coordinate information about the passthrough camera texture.
	 *
	 * @param InUV		The original UVs of on the quad. Should be an array with 8 floats.
	 * @param OutUV		The orientated UVs that can be used to sample the passthrough camera texture and make sure it is displayed correctly.
	 */
	UE_DEPRECATED(4.21, "Use UGoogleARCoreFrameFunctionLibrary::TransformARCoordinates2D(EGoogleARCoreCoordinates2DType::Viewport, InUV, EGoogleARCoreCoordinates2DType::Texture, OutUV) instead.")
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|PassthroughCamera", meta = (Keywords = "googlear arcore passthrough camera uv"))
	static void GetPassthroughCameraImageUV(const TArray<float>& InUV, TArray<float>& OutUV);

	//-------------------Trackables-------------------------
	/**
	 * Gets a list of all valid UARPlaneGeometry objects that ARCore is currently tracking.
	 * Planes that have entered the EARTrackingState::StoppedTracking state or for which
	 * UARPlaneGeometry::GetSubsumedBy returns non-null will not be included.
	 *
	 * @param OutPlaneList		An array that contains all the valid planes detected by ARCore.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated. Use \"GetAllGeometriesByClass\" instead.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|TrackablePlane", meta = (DeprecatedFunction, DeprecationMessage = "GetAllPlanes is deprecated. Use GetAllGeometriesByClass.", Keywords = "googlear arcore all plane"))
	static void GetAllPlanes(TArray<UARPlaneGeometry*>& OutPlaneList);

	/**
	 * Gets a list of all valid UARTrackedPoint objects that ARCore is currently tracking.
	 * TrackablePoint that have entered the EARTrackingState::StoppedTracking state will not be included.
	 *
	 * @param OutTrackablePointList		An array that contains all the valid trackable points detected by ARCore.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated. Use \"GetAllGeometriesByClass\" instead.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|TrackablePoint", meta = (DeprecatedFunction, DeprecationMessage = "GetAllTrackablePoints is deprecated. Use GetAllGeometriesByClass.", Keywords = "googlear arcore pose transform"))
	static void GetAllTrackablePoints(TArray<UARTrackedPoint*>& OutTrackablePointList);

	/**
	 * Gets a list of all valid UGoogleARCoreAugmentedImage objects that ARCore is currently tracking.
	 * UGoogleARCoreAugmentedImage that have entered the EARTrackingState::StoppedTracking state will not be included.
	 *
	 * @param OutAugmentedImageList		An array that contains all the valid augmented images detected by ARCore.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated. Use \"GetAllGeometriesByClass\" instead.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|AugmentedImage", meta = (DeprecatedFunction, DeprecationMessage = "GetAllAugmentedImages is deprecated. Use GetAllGeometriesByClass.", Keywords = "googlear arcore all augmented image"))
	static void GetAllAugmentedImages(TArray<UGoogleARCoreAugmentedImage*>& OutAugmentedImageList);

	/**
	 * Gets a list of all valid UGoogleARCoreAugmentedFace objects that ARCore is currently tracking.
	 * UGoogleARCoreAugmentedFace that have entered the EARTrackingState::StoppedTracking state will not be included.
	 *
	 * @param OutAugmentedFaceList		An array that contains all the valid augmented faces detected by ARCore.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use GetAllGeometriesByClass.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|AugmentedFace", meta = (Keywords = "googlear arcore all augmented face", DeprecatedFunction, DeprecationMessage = "This function is deprecated, use GetAllGeometriesByClass."))
	static void GetAllAugmentedFaces(TArray<UGoogleARCoreAugmentedFace*>& OutAugmentedFaceList);

	/** Template function to get all trackables from a given type. */
	template< class T > static void GetAllTrackable(TArray<T*>& OutTrackableList);

	/**
	 * Create an ARCandidateImage object from the raw pixel data and add it to the ARCandidateImageList of the given \c UARSessionConfig object.
	 *
	 * Note that you need to restart the AR session with the \c UARSessionConfig you are adding to to make the change take effect.
	 *
	 * On ARCore platform, you can leave the PhysicalWidth and PhysicalHeight to 0 if you don't know the physical size of the image or
	 * the physical size is dynamic. And this function takes time to perform non-trivial image processing (20ms - 30ms),
	 * and should be run on a background thread.
	 *
	 * @return A \c UARCandidateImage Object pointer if the underlying ARPlatform added the candidate image at runtime successfully.
	 *		  Return nullptr otherwise.
	 */
	UFUNCTION(BlueprintCallable, Category = "AR AugmentedReality|Session", meta = (Keywords = "ar augmentedreality augmented reality candidate image"))
	static UARCandidateImage* AddRuntimeCandidateImageFromRawbytes(UARSessionConfig* SessionConfig, const TArray<uint8>& ImageGrayscalePixels, int ImageWidth, int ImageHeight,
			FString FriendlyName, float PhysicalWidth, UTexture2D* CandidateTexture = nullptr);
};

/** A function library that provides static/Blueprint functions associated with most recent GoogleARCore tracking frame.*/
UCLASS()
class GOOGLEARCOREBASE_API UGoogleARCoreFrameFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Returns the current ARCore session status.
	 *
	 * @return	A EARSessionStatus enum that describes the session status.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated. Use \"GetTrackingQuality\" instead.")
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|MotionTracking", meta = (DeprecatedFunction, DeprecationMessage = "GetTrackingState is deprecated. Use GetTrackingQuality.", Keywords = "googlear arcore session"))
	static EGoogleARCoreTrackingState GetTrackingState();

	/**
	 * Returns the reason when UARBlueprintLibrary::GetTrackingQuality() returns NotTracking, or UGoogleARCoreFrameFunctionLibrary::GetTrackingState
	 * returns Paused.
	 *
	 * In scenarios when multiple causes result in tracking failures, this reports the most actionable failure reason.
	 *
	 * @return	A EGoogleARCoreTrackingFailureReason enum that describes the tracking failure reason.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated. Use \"GetTrackingQualityReason\" instead.")
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|MotionTracking", meta = (DeprecatedFunction, DeprecationMessage = "GetTrackingFailureReason is deprecated. Use GetTrackingQualityReason.", Keywords = "googlear arcore session"))
	static EGoogleARCoreTrackingFailureReason GetTrackingFailureReason();

	/**
	 * Gets the latest tracking pose of the ARCore device in Unreal AR Tracking Space
	 *
	 * Note that ARCore motion tracking has already integrated with HMD and the motion controller interface.
	 * Use this function only if you need to implement your own tracking component.
	 *
	 * @param OutPose		The latest device pose.
	 * @return				True if the pose is updated successfully for this frame.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use GetOrientationAndPosition.")
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|MotionTracking", meta = (Keywords = "googlear arcore pose transform", DeprecatedFunction, DeprecationMessage = "This function is deprecated, use GetOrientationAndPosition."))
	static void GetPose(FTransform& OutPose);

	/**
	 * Traces a ray from the user's device in the direction of the given location in the camera
	 * view. Intersections with detected scene geometry are returned, sorted by distance from the
	 * device; the nearest intersection is returned first.
	 *
	 * @param WorldContextObject	The world context.
	 * @param ScreenPosition		The position on the screen to cast the ray from.
	 * @param TraceChannels			A set of EGoogleARCoreLineTraceChannel indicate which type of line trace it should perform.
	 * @param OutHitResults			The list of hit results sorted by distance.
	 * @return						True if there is a hit detected.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use LineTraceTrackedObjects.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|LineTrace", meta = (WorldContext = "WorldContextObject", Keywords = "googlear arcore raycast hit", DeprecatedFunction, DeprecationMessage = "This function is deprecated, use LineTraceTrackedObjects."))
	static bool ARCoreLineTrace(UObject* WorldContextObject, const FVector2D& ScreenPosition, TSet<EGoogleARCoreLineTraceChannel> TraceChannels, TArray<FARTraceResult>& OutHitResults);

	/**
	* Traces a ray along the given line. Intersections with detected scene geometry are returned,
	* sorted by distance from the start of the line; the nearest intersection is returned first.
	*
	* @param WorldContextObject	The world context.
	* @param Start		The start of line segment.
	* @param End		The end of line segment.
	* @param TraceChannels			A set of EGoogleARCoreLineTraceChannel indicate which type of line trace it should perform.
	* @param OutHitResults			The list of hit results sorted by distance.
	* @return						True if there is a hit detected.
	*/
	UE_DEPRECATED(4.26, "This function is deprecated, use LineTraceTrackedObjects3D.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|LineTrace", meta = (WorldContext = "WorldContextObject", Keywords = "googlear arcore raycast hit", DeprecatedFunction, DeprecationMessage = "This function is deprecated, use LineTraceTrackedObjects3D."))
	static bool ARCoreLineTraceRay(UObject* WorldContextObject, const FVector& Start, const FVector& End, TSet<EGoogleARCoreLineTraceChannel> TraceChannels, TArray<FARTraceResult>& OutHitResults);

	/**
	 * Gets a list of UARPin objects that were changed in this frame.
	 *
	 * @param OutAnchorList		An array that contains the updated UARPin.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use GetAllPins.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|ARAnchor", meta = (Keywords = "googlear arcore Anchor", DeprecatedFunction, DeprecationMessage="This function is deprecated, use GetAllPins."))
	static void GetUpdatedARPins(TArray<UARPin*>& OutAnchorList);

	/**
	 * Gets a list of UARPlaneGeometry objects that were changed in this frame.
	 *
	 * @param OutPlaneList	An array that contains the updated UARPlaneGeometry.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use GetAllGeometriesByClass.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|TrackablePlane", meta = (Keywords = "googlear arcore pose transform", DeprecatedFunction, DeprecationMessage="This function is deprecated, use GetAllGeometriesByClass."))
	static void GetUpdatedPlanes(TArray<UARPlaneGeometry*>& OutPlaneList);

	/**
	 * Gets a list of UARTrackedPoint objects that were changed in this frame.
	 *
	 * @param OutTrackablePointList	An array that contains the updated UARTrackedPoint.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use GetAllGeometriesByClass.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|TrackablePoint", meta = (Keywords = "googlear arcore pose transform", DeprecatedFunction, DeprecationMessage="This function is deprecated, use GetAllGeometriesByClass."))
	static void GetUpdatedTrackablePoints(TArray<UARTrackedPoint*>& OutTrackablePointList);

	/**
	 * Gets a list of UGoogleARCoreAugmentedImage objects that were changed in this frame.
	 *
	 * @param OutImageList	An array that contains the updated UGoogleARCoreAugmentedImage.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use GetAllGeometriesByClass.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|AugmentedImage", meta = (Keywords = "googlear arcore augmented image updated", DeprecatedFunction, DeprecationMessage="This function is deprecated, use GetAllGeometriesByClass."))
	static void GetUpdatedAugmentedImages(TArray<UGoogleARCoreAugmentedImage*>& OutImageList);

	/**
	* Gets a list of UGoogleARCoreAugmentedFace objects that were changed in this frame.
	*
	* @param OutFaceList	An array that contains the updated UGoogleARCoreAugmentedFace.
	*/
	UE_DEPRECATED(4.26, "This function is deprecated, use GetAllGeometriesByClass.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|AugmentedFace", meta = (Keywords = "googlear arcore augmented face updated", DeprecatedFunction, DeprecationMessage="This function is deprecated, use GetAllGeometriesByClass."))
	static void GetUpdatedAugmentedFaces(TArray<UGoogleARCoreAugmentedFace*>& OutFaceList);

	/** Template function to get the updated trackables in this frame a given trackable type. */
	template< class T > static void GetUpdatedTrackable(TArray<T*>& OutTrackableList);

	/**
	 * Gets the latest light estimation.
	 *
	 * @param OutLightEstimate		The struct that describes the latest light estimation.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use GetCurrentLightEstimate.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|LightEstimation", meta = (Keywords = "googlear arcore light ambient", DeprecatedFunction, DeprecationMessage="This function is deprecated, use GetCurrentLightEstimate."))
	static void GetLightEstimation(FGoogleARCoreLightEstimate& OutLightEstimate);

	/**
	 * Gets the latest point cloud that will be only available for this frame.
	 * If you want to keep the point cloud data, you can either copy it to your own struct
	 * or call AcquireLatestPointCloud() to avoid the copy.
	 *
	 * @param OutLatestPointCloud		A pointer point to the latest point cloud.
	 * @return  An EGoogleARCoreFunctionStatus. Possible value: Success, SessionPaused, ResourceExhausted.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use GetPointCloud.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|PointCloud", meta = (Keywords = "googlear arcore pointcloud", DeprecatedFunction, DeprecationMessage="This function is deprecated, use GetPointCloud."))
	static EGoogleARCoreFunctionStatus GetPointCloud(UGoogleARCorePointCloud*& OutLatestPointCloud);

	/**
	 * Acquires latest point cloud. This will make the point cloud remain valid unless you call UGoogleARCrePointCloud::ReleasePointCloud().
	 * Be aware that this function could fail if the maximal number of point cloud has been acquired.
	 *
	 * @param OutLatestPointCloud		A pointer point to the latest point cloud.
	 * @return  An EGoogleARCoreFunctionStatus. Possible value: Success, SessionPaused, ResourceExhausted.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use GetPointCloud.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|PointCloud", meta = (Keywords = "googlear arcore pointcloud", DeprecatedFunction, DeprecationMessage="This function is deprecated, use GetPointCloud."))
	static EGoogleARCoreFunctionStatus AcquirePointCloud(UGoogleARCorePointCloud*& OutLatestPointCloud);

#if PLATFORM_ANDROID
	/**
	 * Gets the camera metadata for the latest camera image.
	 * Note that ACameraMetadata is a Ndk type. Include the Ndk header <camera/NdkCameraMetadata.h> to use query value from ACameraMetadata.
	 *
	 * @param OutCameraMetadata		A pointer to a ACameraMetadata struct which is only valid in one frame.
	 * @return An EGoogleARCoreFunctionStatus. Possible value: Success, SessionPaused, NotAvailable.
	 */
	static EGoogleARCoreFunctionStatus GetCameraMetadata(const ACameraMetadata*& OutCameraMetadata);
#endif

	/**
	 * Get the pass-through camera texture that GoogleARCore plugin will use to render the passthrough camera background.
	 * Note that UTexture object this function returns may change every frame. If you want to use the camera texture, you should
	 * call the function every frame and update the texture parameter in your material.
	 *
	 * @return A pointer to the UTexture that will be used to render the passthrough camera background.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated. Use \"GoogleARCore Passthrough Camera\" expression in your material instead.")
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|PassthroughCamera", meta = (DeprecatedFunction, DeprecationMessage = "GetCameraTexture is deprecated. Currently, use GoogleARCore Passthrough Camera expression.", Keywords = "googlear arcore passthroughcamera"))
	static UTexture* GetCameraTexture();

	/**
	 * Acquire a CPU-accessible camera image.
	 *
	 * @param OutLatestCameraImage    A place to store the pointer to a new UGoogleARCoreCameraImage instance.
	 * @return An EGoogleARCoreFunctionStatus. Possible value: Success, ResourceExhausted, NotAvailable.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use GetARTexture.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|PassthroughCamera", meta = (Keywords = "googlear arcore passthroughcamera", DeprecatedFunction, DeprecationMessage = "This function is deprecated, use GetARTexture."))
	static EGoogleARCoreFunctionStatus AcquireCameraImage(UGoogleARCoreCameraImage *&OutLatestCameraImage);

	/**
	 * Transforms an array of 2D coordinates into a different 2D coordinate system.  This will account for the display rotation,
	 * and any additional required adjustment.
	 *
	 * Some examples of useful conversions:
	 *   To transform screen space UVs for texture space UVs to rendering pass-through camera texture: Viewport -> Texture;
	 *   To transform a point found by a computer vision algorithm in the pass-through camera image into a point on the viewport: Image -> Viewport;
	 *
	 * @param InputCoordinatesType		The coordinate system used by InputCoordinates.
	 * @param InputCoordinates			The input 2d coordinates.
	 * @param OutputCoordinatesType		The coordinate system to transform to.
	 * @param OutputCoordinates			The output 2d coordinates.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use the standard materials from ARUtilities plugin for passthrough rendering.")
	UFUNCTION(BlueprintPure, Category = "GoogleARCore|PassthroughCamera", meta = (Keywords = "googlear arcore passthrough camera uv", DeprecatedFunction, DeprecationMessage = "This function is deprecated, use the standard materials from ARUtilities plugin for passthrough rendering."))
	static void TransformARCoordinates2D(EGoogleARCoreCoordinates2DType InputCoordinatesType, const TArray<FVector2D>& InputCoordinates,
		EGoogleARCoreCoordinates2DType OutputCoordinatesType, TArray<FVector2D>& OutputCoordinates);

	/**
	 * Get the camera intrinsics for the camera image (CPU image).
	 *
	 * @param OutCameraIntrinsics  The output intrinsics object.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use GetCameraIntrinsics.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|CameraIntrinsics", meta = (Keywords = "googlear arcore camera", DeprecatedFunction, DeprecationMessage = "This function is deprecated, use GetCameraIntrinsics."))
	static EGoogleARCoreFunctionStatus GetCameraImageIntrinsics(UGoogleARCoreCameraIntrinsics *&OutCameraIntrinsics);

	/**
	 * Get the camera intrinsics for the camera texture (GPU image).
	 *
	 * @param OutCameraIntrinsics  The output intrinsics object.
	 */
	UE_DEPRECATED(4.26, "This function is deprecated, use GetCameraIntrinsics.")
	UFUNCTION(BlueprintCallable, Category = "GoogleARCore|CameraIntrinsics", meta = (Keywords = "googlear arcore camera", DeprecatedFunction, DeprecationMessage = "This function is deprecated, use GetCameraIntrinsics."))
	static EGoogleARCoreFunctionStatus GetCameraTextureIntrinsics(UGoogleARCoreCameraIntrinsics *&OutCameraIntrinsics);
};
