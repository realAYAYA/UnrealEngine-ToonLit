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

#ifndef OVR_Plugin_Types_Deprecated_h
#define OVR_Plugin_Types_Deprecated_h

#include "OVR_Plugin_Types.h"

#if defined ANDROID || defined __linux__
#define __cdecl
#endif

/// Symmetric frustum for a camera.
typedef struct {
  /// Near clip plane.
  float zNear;
  /// Far clip plane.
  float zFar;
  /// Horizontal field of view.
  float fovX;
  /// Vertical field of view.
  float fovY;
} ovrpFrustumf;

const static ovrpFrustumf s_identityFrustum = {0, 0, 0, 0};

/// Describes Input State for use with Gamepads and Oculus Controllers.
typedef struct {
  unsigned int ConnectedControllerTypes;
  unsigned int Buttons;
  unsigned int Touches;
  unsigned int NearTouches;
  float IndexTrigger[2];
  float HandTrigger[2];
  ovrpVector2f Thumbstick[2];
  ovrpVector2f Touchpad[2];
} ovrpControllerState2;

/// Describes Input State for use with Gamepads and Oculus Controllers.
typedef struct {
  unsigned int ConnectedControllerTypes;
  unsigned int Buttons;
  unsigned int Touches;
  unsigned int NearTouches;
  float IndexTrigger[2];
  float HandTrigger[2];
  ovrpVector2f Thumbstick[2];
} ovrpControllerState;

typedef ovrpControllerState ovrpInputState;

typedef struct ovrpSkeleton_ {
  ovrpSkeletonType SkeletonType;
  unsigned int NumBones;
  unsigned int NumBoneCapsules;
  ovrpBone Bones[ovrpSkeletonConstants_MaxHandBones];
  ovrpBoneCapsule BoneCapsules[ovrpSkeletonConstants_MaxBoneCapsules];
} ovrpSkeleton;

/// Capability bits that control the plugin's configuration.
/// Each value corresponds to a left-shift offset in the bitfield.
typedef enum {
  /// If true, sRGB read-write occurs, reducing eye texture aliasing.
  ovrpCap_SRGB = 0,
  /// If true, the image will be corrected for chromatic aberration.
  ovrpCap_Chromatic,
  /// If true, eye textures are flipped on the Y axis before display.
  ovrpCap_FlipInput,
  /// If true, head tracking affects the rotation reported by ovrp_GetEyePose.
  ovrpCap_Rotation,
  /// (Deprecated) If true, head rotation affects the position reported by ovrp_GetEyePose.
  ovrpCap_HeadModel,
  /// If true, head position tracking affects the poses returned by ovrp_GetEyePose.
  ovrpCap_Position,
  /// If true, the runtime collects performance statistics for debugging.
  ovrpCap_CollectPerf,
  /// If true, a debugging heads-up display appears in the scene.
  ovrpCap_DebugDisplay,
  /// If true, the left eye image is shown to both eyes. Right is ignored.
  ovrpCap_Monoscopic,
  /// If true, both eyes share texture 0, with the left eye on the left side.
  ovrpCap_ShareTexture,
  /// If true, a clip mesh will be provided for both eyes
  ovrpCap_OcclusionMesh,
  ovrpCap_EnumSize = 0x7fffffff
} ovrpCaps;

/// Read-only bits that reflect the plugins' current status.
/// Each value corresponds to a left-shift offset in the bitfield.
typedef enum {
  /// If true, the VR display is virtual and no physical device is attached.
  ovrpStatus_Debug = 0,
  /// (Deprecated) If true, the health & safety warning is currently visible.
  ovrpStatus_HSWVisible,
  /// If true, the HMD supports position tracking (e.g. a camera is attached).
  ovrpStatus_PositionSupported,
  /// If true, position tracking is active and not obstructed.
  ovrpStatus_PositionTracked,
  /// If true, the system has reduced performance to save power.
  ovrpStatus_PowerSaving,
  /// If true, the plugin is initialized and ready for use.
  ovrpStatus_Initialized,
  /// If true, a working VR display is present, but it may be a "debug" display.
  ovrpStatus_HMDPresent,
  /// If true, the user is currently wearing the VR display and it is not idle.
  ovrpStatus_UserPresent,
  /// If true, the app has VR focus.
  ovrpStatus_HasVrFocus,
  /// If true, the app should quit as soon as possible.
  ovrpStatus_ShouldQuit,
  /// If true, the app should call ovrp_RecenterPose as soon as possible.
  ovrpStatus_ShouldRecenter,
  /// If true, we need to recreate the session
  ovrpStatus_ShouldRecreateDistortionWindow,
  ovrpStatus_EnumSize = 0x7fffffff
} ovrpStatus;

typedef enum {
  /// (String) Identifies the version of OVRPlugin you are using. Format: "major.minor.release"
  ovrpKey_Version,
  /// (String) Identifies the type of VR display device in use, if any.
  ovrpKey_ProductName,
  /// (String) The latest measured latency.
  ovrpKey_Latency,
  /// (Float) The physical distance from the front of the player's eye to the back of their neck
  /// in meters.
  ovrpKey_EyeDepth,
  /// (Float) The physical height of the player's eyes from the ground in meters.
  ovrpKey_EyeHeight,
  /// (Float, read-only) The current available battery charge, ranging from 0 (empty) to 1 (full).
  ovrpKey_BatteryLevel,
  /// (Float, read-only) The current battery temperature in degrees Celsius.
  ovrpKey_BatteryTemperature,
  /// (Float) The current CPU performance level, rounded down to nearest integer in the range 0-2.
  ovrpKey_CpuLevel,
  /// (Float) The current GPU performance level, rounded down to nearest integer in the range 0-2.
  ovrpKey_GpuLevel,
  /// (Float, read-only) The current system volume level.
  ovrpKey_SystemVolume,
  /// (Float) The fraction of a frame ahead to predict poses and allow GPU-CPU parallelism.
  /// Trades latency for performance.
  ovrpKey_QueueAheadFraction,
  /// (Float) The physical inter-pupillary distance (IPD) separating the user's eyes in meters.
  ovrpKey_IPD,
  /// (Float) The number of allocated eye texture texels per screen pixel in each direction
  /// (horizontal and vertical).
  ovrpKey_NativeTextureScale,
  /// (Float) The number of rendered eye texture texels per screen pixel based on viewport scaling.
  ovrpKey_VirtualTextureScale,
  /// (Float) The native refresh rate of the HMD.
  ovrpKey_Frequency,
  /// (String) The version of the underlying SDK in use.
  ovrpKey_SDKVersion,
  ovrpKey_EnumSize = 0x7fffffff
} ovrpKey;

typedef ovrpShape ovrpOverlayShape;

typedef enum {
  ovrpOverlayFlag_None = 0x00000000,
  /// If true, the overlay appears on top of all lower-indexed layers and the eye buffers.
  ovrpOverlayFlag_OnTop = 0x00000001,
  /// If true, the overlay bypasses TimeWarp and directly follows head motion.
  ovrpOverlayFlag_HeadLocked = 0x00000002,
  /// If true, the overlay will not allow depth compositing on Rift.
  ovrpOverlayFlag_NoDepth  = 0x00000004,
  // If true, the overlay will use the VrApi supersample flag, which can be helpful but is extremely expensive.
  ovrpOverlayFlag_ExpensiveSuperSample = 0x00000008,







  // Use left 5 - 8 bits for shape flags
  ovrpOverlayFlag_ShapeShift = 4,
  ovrpOverlayFlag_Quad = (ovrpShape_Quad << ovrpOverlayFlag_ShapeShift),
  ovrpOverlayFlag_Cylinder = (ovrpShape_Cylinder << ovrpOverlayFlag_ShapeShift),
  ovrpOverlayFlag_Cubemap = (ovrpShape_Cubemap << ovrpOverlayFlag_ShapeShift),
  ovrpOverlayFlag_offCenterCubemap = (ovrpShape_OffcenterCubemap << ovrpOverlayFlag_ShapeShift),
  ovrpOverlayFlag_ShapeMask = (0xF << ovrpOverlayFlag_ShapeShift),

  ovrpOverlayFlag_Hidden = 0x00000200,

  // Internal flags
  /// If true, the overlay is a loading screen.
  ovrpOverlayFlag_LoadingScreen = 0x40000000,
  /// If true, the overlay bypasses distortion and is copied directly to the display
  /// (possibly with scaling).
  ovrpOverlayFlag_Undistorted = 0x80000000,
  ovrpOverlayFlag_EnumSize = 0x7fffffff
} ovrpOverlayFlag;

#endif
