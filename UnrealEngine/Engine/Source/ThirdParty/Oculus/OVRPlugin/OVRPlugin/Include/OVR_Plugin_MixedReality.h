/************************************************************************************

Copyright (c) Facebook Technologies, LLC and its affiliates.  All rights reserved.

Your use of this SDK or tool is subject to the Oculus SDK License Agreement, available at
https://developer.oculus.com/licenses/oculussdk/

Unless required by applicable law or agreed to in writing, the Oculus SDK
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

************************************************************************************/

#ifndef OVR_Plugin_MixedReality_h
#define OVR_Plugin_MixedReality_h

#include "OVR_Plugin_Types.h"

#if OVRP_MIXED_REALITY_PRIVATE
#include "OVR_Plugin_MixedReality_Private.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

//////////////////// Tracked Camera //////////////////////////

/// Initialize Mixed Reality functionalities
OVRP_EXPORT ovrpResult ovrp_InitializeMixedReality();

/// Shutdown Mixed Reality functionalities
OVRP_EXPORT ovrpResult ovrp_ShutdownMixedReality();

/// Check whether Mixed Reality functionalities has been initialized
OVRP_EXPORT ovrpBool ovrp_GetMixedRealityInitialized();

/// Update external camera. Need to be called before accessing the camera count or individual camera information
OVRP_EXPORT ovrpResult ovrp_UpdateExternalCamera();

/// Get the number of external cameras
OVRP_EXPORT ovrpResult ovrp_GetExternalCameraCount(int* cameraCount);

/// Get the name of an external camera
OVRP_EXPORT ovrpResult ovrp_GetExternalCameraName(int cameraId, char cameraName[OVRP_EXTERNAL_CAMERA_NAME_SIZE]);

/// Get intrinsics of an external camera
OVRP_EXPORT ovrpResult ovrp_GetExternalCameraIntrinsics(int cameraId, ovrpCameraIntrinsics* cameraIntrinsics);

/// Get extrinsics of an external camera
OVRP_EXPORT ovrpResult ovrp_GetExternalCameraExtrinsics(int cameraId, ovrpCameraExtrinsics* cameraExtrinsics);

/// Get the raw transform pose when the external camera was calibrated
OVRP_EXPORT ovrpResult ovrp_GetExternalCameraCalibrationRawPose(int cameraId, ovrpPosef* rawPose);

/// Override the FOV of the external camera
OVRP_EXPORT ovrpResult ovrp_OverrideExternalCameraFov(int cameraId, ovrpBool useOverriddenFov, const ovrpFovf* fov);

/// Get if the FOV of the external camera is overridden
OVRP_EXPORT ovrpResult ovrp_GetUseOverriddenExternalCameraFov(int cameraId, ovrpBool* useOverriddenFov);

/// Override the Pose of the external camera.
OVRP_EXPORT ovrpResult ovrp_OverrideExternalCameraStaticPose(int cameraId, ovrpBool useOverriddenPose, const ovrpPosef* pose);

/// Get if the Pose of the external camera is overridden
OVRP_EXPORT ovrpResult ovrp_GetUseOverriddenExternalCameraStaticPose(int cameraId, ovrpBool* useOverriddenStaticPose);

/// Helper function to get the camera pose in the tracking space
OVRP_EXPORT ovrpResult ovrp_GetExternalCameraPose(int cameraId, ovrpPosef* cameraPose);

/// Helper function to get convert a pose in tracking space to camera space
OVRP_EXPORT ovrpResult
ovrp_ConvertPoseToCameraSpace(int cameraId, ovrpPosef* trackingSpacePose, ovrpPosef* cameraSpacePose);

/// Reset the manual external camera
/// On Quest, it would stop listenting to the MRC port if needed
OVRP_EXPORT ovrpResult ovrp_ResetDefaultExternalCamera();


/// Set a manual external camera to the system. The manual external camera is valid when there is no camera configuration can be loaded
/// On Quest, it would start listenting to the MRC port if needed
OVRP_EXPORT ovrpResult ovrp_SetDefaultExternalCamera(const char* cameraName, const ovrpCameraIntrinsics* cameraIntrinsics, const ovrpCameraExtrinsics* cameraExtrinsics);

/// (PC only) set external camera intrinsics and extrinsics
OVRP_EXPORT ovrpResult ovrp_SetExternalCameraProperties(const char* cameraName, const ovrpCameraIntrinsics* cameraIntrinsics, const ovrpCameraExtrinsics* cameraExtrinsics);


//////////////////// Camera Devices //////////////////////////

/// Retrieve all supported camera devices
OVRP_EXPORT ovrpResult
ovrp_EnumerateAllCameraDevices(ovrpCameraDevice* deviceArray, int deviceArraySize, int* deviceCount);

/// Retrive all supported camera devices which is also available
OVRP_EXPORT ovrpResult
ovrp_EnumerateAvailableCameraDevices(ovrpCameraDevice* deviceArray, int deviceArraySize, int* deviceCount);

/// Update all the opened cameras. Should be called on each frame from the main thread
OVRP_EXPORT ovrpResult ovrp_UpdateCameraDevices();

/// Check the camera device availablity
OVRP_EXPORT ovrpResult ovrp_IsCameraDeviceAvailable2(ovrpCameraDevice camera, ovrpBool* available);

/// The PreferredColorFrameSize is only a hint. The final ColorFrameSize could be different
OVRP_EXPORT ovrpResult
ovrp_SetCameraDevicePreferredColorFrameSize(ovrpCameraDevice camera, ovrpSizei preferredColorFrameSize);

/// Open the camera device
OVRP_EXPORT ovrpResult ovrp_OpenCameraDevice(ovrpCameraDevice camera);

/// Close the camera device
OVRP_EXPORT ovrpResult ovrp_CloseCameraDevice(ovrpCameraDevice camera);

/// Check if the camera device has been opened
OVRP_EXPORT ovrpResult ovrp_HasCameraDeviceOpened2(ovrpCameraDevice camera, ovrpBool* opened);

/// Try to retrieve the camera intrinsics parameters if available
OVRP_EXPORT ovrpResult ovrp_GetCameraDeviceIntrinsicsParameters(
    ovrpCameraDevice camera,
    ovrpBool* supportIntrinsics,
    ovrpCameraDeviceIntrinsicsParameters* intrinsicsParameters);

/// Check if there the color frame is available for the camera device
OVRP_EXPORT ovrpResult ovrp_IsCameraDeviceColorFrameAvailable2(ovrpCameraDevice camera, ovrpBool* available);

/// Retrieve the dimension of the current color frame
OVRP_EXPORT ovrpResult ovrp_GetCameraDeviceColorFrameSize(ovrpCameraDevice camera, ovrpSizei* colorFrameSize);

/// Retrieve the raw data of the currect color frame (in BGRA arrangement)
OVRP_EXPORT ovrpResult ovrp_GetCameraDeviceColorFrameBgraPixels(
    ovrpCameraDevice camera,
    const ovrpByte** colorFrameBgraPixels,
    int* colorFrameRowPitch);

/// Check if the camera device support returning depth frames
OVRP_EXPORT ovrpResult ovrp_DoesCameraDeviceSupportDepth(ovrpCameraDevice camera, ovrpBool* supportDepth);

/// Get the current depth sensing mode
OVRP_EXPORT ovrpResult
ovrp_GetCameraDeviceDepthSensingMode(ovrpCameraDevice camera, ovrpCameraDeviceDepthSensingMode* depthSensingMode);

/// Set the current depth sensing mode
OVRP_EXPORT ovrpResult
ovrp_SetCameraDeviceDepthSensingMode(ovrpCameraDevice camera, ovrpCameraDeviceDepthSensingMode depthSensingMode);

/// Get the current preferred depth quality
OVRP_EXPORT ovrpResult
ovrp_GetCameraDevicePreferredDepthQuality(ovrpCameraDevice camera, ovrpCameraDeviceDepthQuality* depthQuality);

/// Set the preferred depth quality. It should be set before opening the camera
OVRP_EXPORT ovrpResult
ovrp_SetCameraDevicePreferredDepthQuality(ovrpCameraDevice camera, ovrpCameraDeviceDepthQuality depthQuality);


/// Check if the depth frame is available
OVRP_EXPORT ovrpResult ovrp_IsCameraDeviceDepthFrameAvailable(ovrpCameraDevice camera, ovrpBool* available);

/// Get the depth frame resolution
OVRP_EXPORT ovrpResult ovrp_GetCameraDeviceDepthFrameSize(ovrpCameraDevice camera, ovrpSizei* depthFrameSize);

/// Depth data is in centimeters
OVRP_EXPORT ovrpResult
ovrp_GetCameraDeviceDepthFramePixels(ovrpCameraDevice camera, const float** depthFramePixels, int* depthFrameRowPitch);

/// The confidence value is mapped between 0 (high confidence threshold, sparse data) and 100 (low confidence threshold,
/// dense data)
OVRP_EXPORT ovrpResult ovrp_GetCameraDeviceDepthConfidencePixels(
    ovrpCameraDevice camera,
    const float** depthConfidencePixels,
    int* depthConfidenceRowPitch);


#ifdef __cplusplus
}
#endif

#endif
