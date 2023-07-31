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

#ifndef OVR_Plugin_Types_h
#define OVR_Plugin_Types_h

#if !defined(OVRP_STRINGIFY)
#define OVRP_STRINGIFYIMPL(x) #x
#define OVRP_STRINGIFY(x) OVRP_STRINGIFYIMPL(x)
#endif

#define OVRP_MAJOR_VERSION 1
#define OVRP_MINOR_VERSION 65
#define OVRP_PATCH_VERSION 0

#define OVRP_VERSION OVRP_MAJOR_VERSION, OVRP_MINOR_VERSION, OVRP_PATCH_VERSION
#define OVRP_VERSION_STR OVRP_STRINGIFY(OVRP_MAJOR_VERSION.OVRP_MINOR_VERSION.OVRP_PATCH_VERSION)




#define OVRP_VERSION_CHANNEL "Release"


#define OVRP_CURRENT_FRAMEINDEX -1

#ifndef OVRP_EXPORT
#ifdef _WIN32
#define OVRP_EXPORT __declspec(dllexport)
#elif __ANDROID__
#define OVRP_EXPORT __attribute__ ((visibility ("default")))
#else
#define OVRP_EXPORT
#endif
#endif

#if defined ANDROID || defined __linux__
#define __cdecl
#endif

#ifdef __cplusplus
#define OVRP_REF(Type) Type&
#define OVRP_CONSTREF(Type) const Type&
#define OVRP_DEFAULTVALUE(Value) = Value
#else
#define OVRP_REF(Type) Type*
#define OVRP_CONSTREF(Type) const Type*
#define OVRP_DEFAULTVALUE(Value)
#endif

#define OVRP_UNUSED(x) ((void)(x))

#define OVRP_FILE_AND_LINE __FILE__ ":" OVRP_STRINGIFY(__LINE__)

#ifndef OVRP_MIXED_REALITY_PRIVATE
#define OVRP_MIXED_REALITY_PRIVATE 0
#endif

/// True or false
enum {
  ovrpBool_False = 0,
  ovrpBool_True = 1,
};
typedef int ovrpBool;

/// Byte
typedef unsigned char ovrpByte;

/// UInt32
typedef unsigned int ovrpUInt32;

/// Int16
typedef short ovrpInt16;

/// UInt16
typedef unsigned short ovrpUInt16;

/// Int64
typedef long long ovrpInt64;

/// UInt64
typedef unsigned long long ovrpUInt64;

/// Epsilon for floating point comparison
#define OVRP_FLOAT_EPSILON    (1e-5f)

/// Success and failure
typedef enum {
  /// Success
  ovrpSuccess = 0,
  ovrpSuccess_EventUnavailable = 1,
  ovrpSuccess_Pending = 2,

  /// Failure
  ovrpFailure = -1000,
  ovrpFailure_InvalidParameter = -1001,
  ovrpFailure_NotInitialized = -1002,
  ovrpFailure_InvalidOperation = -1003,
  ovrpFailure_Unsupported = -1004,
  ovrpFailure_NotYetImplemented = -1005,
  ovrpFailure_OperationFailed = -1006,
  ovrpFailure_InsufficientSize = -1007,
  ovrpFailure_DataIsInvalid = -1008,
  ovrpFailure_DeprecatedOperation = -1009,
} ovrpResult;

#define OVRP_SUCCESS(result) ((result) >= 0)
#define OVRP_FAILURE(result) ((result) < 0)

/// XR API types
typedef enum {
  ovrpXrApi_Unknown = 0,
  ovrpXrApi_CAPI = 1,
  ovrpXrApi_VRAPI = 2,
  ovrpXrApi_OpenXR = 3,
  ovrpXrApi_EnumSize = 0x7fffffff
} ovrpXrApi;

/// Initialization flags
typedef enum {
  /// Start GearVR battery and volume receivers
  ovrpInitializeFlag_StartGearVRReceivers = (1 << 0),
  /// Supports 2D/3D switching
  ovrpInitializeFlag_SupportsVRToggle = (1 << 1),
  /// Supports Life Cycle Focus (Dash)
  ovrpInitializeFlag_FocusAware = (1 << 2),
  /// DEPRECATED - Turn off Legacy Core Affinity Patch
  /// Background: Some legacy unity versions set thread affinities wrong on newer hardware like Oculus Go
  /// We need patch it in the runtime for published legacy apps.
  /// This flag will be passed from fixed Unity versions explicitly, so we can skip the runtime patch mechanism since we already have proper fixes.
  /// Deprecated Background: Several Unity versions incorrectly indicated they handled applying thread affinity, so this flag has been deprecated
  /// in order to fallback to runtime thread affinity handling. In the future, a new flag will be introduced to allow engine opt-out of
  /// runtime affinity handling.
  ovrpInitializeFlag_NoLegacyCoreAffinityPatch = (1 << 3), // DEPRECATED

  /// Allow to use sRGB frame buffer, we use it as an initialization flag because we need make the window surface
  /// sRGB compilable, this can't be changed after window created.
  ovrpInitializeFlag_SupportSRGBFrameBuffer = (1 << 4),






  /// Enable Application SpaceWarp support
  ovrpInitializeFlag_SupportAppSpaceWarp = (1 << 6),

  ovrpInitializeFlag_EnumSize = 0x7fffffff

} ovrpInitializeFlags;


/// Thread Performance
typedef enum {
  ovrpThreadPef_DeadLine_Normal = 0,
  ovrpThreadPef_DeadLine_Hard = 1,
  ovrpThreadPef_DeadLine_Soft = 2,
  ovrpThreadPef_EnumSize = 0x7fffffff
} ovrpThreadPerf;

/// Identifies an eye in a stereo pair.
typedef enum {
  ovrpEye_Center = -2,
  ovrpEye_None = -1,
  ovrpEye_Left = 0,
  ovrpEye_Right = 1,
  ovrpEye_Count,
  ovrpEye_EnumSize = 0x7fffffff
} ovrpEye;

/// Identifies a hand.
typedef enum {
  ovrpHand_None = -1,
  ovrpHand_Left = 0,
  ovrpHand_Right = 1,
  ovrpHand_Count,
  ovrpHand_EnumSize = 0x7fffffff
} ovrpHand;

/// Identifies a tracked device object.
typedef enum {
  ovrpDeviceObject_None = -1,
  ovrpDeviceObject_Zero = 0,
  ovrpDeviceObject_Count,
  ovrpDeviceObject_EnumSize = 0x7fffffff
} ovrpDeviceObject;

/// Identifies a tracking sensor.
typedef enum {
  ovrpTracker_None = -1,
  ovrpTracker_Zero = 0,
  ovrpTracker_One = 1,
  ovrpTracker_Two = 2,
  ovrpTracker_Three = 3,
  ovrpTracker_Count,
  ovrpTracker_EnumSize = 0x7fffffff
} ovrpTracker;











/// Identifies a tracked VR Node.
typedef enum {
  ovrpNode_None = -1,
  ovrpNode_EyeLeft = 0,
  ovrpNode_EyeRight = 1,
  ovrpNode_EyeCenter = 2,
  ovrpNode_HandLeft = 3,
  ovrpNode_HandRight = 4,
  ovrpNode_TrackerZero = 5,
  ovrpNode_TrackerOne = 6,
  ovrpNode_TrackerTwo = 7,
  ovrpNode_TrackerThree = 8,
  ovrpNode_Head = 9,
  ovrpNode_DeviceObjectZero = 10,



  ovrpNode_Count,
  ovrpNode_EnumSize = 0x7fffffff
} ovrpNode;

/// Identifies a tracking origin
typedef enum {
  ovrpTrackingOrigin_EyeLevel = 0,
  ovrpTrackingOrigin_FloorLevel = 1,
  ovrpTrackingOrigin_Stage = 2,



  ovrpTrackingOrigin_View = 4,
  ovrpTrackingOrigin_Count,
  ovrpTrackingOrigin_EnumSize = 0x7fffffff
} ovrpTrackingOrigin;

/// The charge status of a battery.
typedef enum {
  ovrpBatteryStatus_Charging,
  ovrpBatteryStatus_Discharging,
  ovrpBatteryStatus_Full,
  ovrpBatteryStatus_NotCharging,
  ovrpBatteryStatus_Unknown,
  ovrpBatteryStatus_EnumSize = 0x7fffffff
} ovrpBatteryStatus;

//Handedness of user as specified in the mobile device
typedef enum {
    ovrpHandedness_Unsupported = 0,
    ovrpHandedness_LeftHanded = 1,
    ovrpHandedness_RightHanded = 2
} ovrpHandedness;

/// An oculus platform UI.
typedef enum {
  ovrpUI_None = -1,
  ovrpUI_GlobalMenu = 0,
  ovrpUI_ConfirmQuit,
  ovrpUI_GlobalMenuTutorial, // Deprecated
  ovrpUI_EnumSize = 0x7fffffff
} ovrpUI;

/// A geographical region associated with the current system device.
typedef enum {
  ovrpSystemRegion_Unspecified,
  ovrpSystemRegion_Japan,
  ovrpSystemRegion_China,
  ovrpSystemRegion_EnumSize = 0x7fffffff
} ovrpSystemRegion;

typedef enum {
  ovrpSystemHeadset_None = 0,

  // Mobile & Standalone headsets
  ovrpSystemHeadset_GearVR_R320,    // Note4 Innovator
  ovrpSystemHeadset_GearVR_R321,    // S6 Innovator
  ovrpSystemHeadset_GearVR_R322,    // GearVR Commercial 1
  ovrpSystemHeadset_GearVR_R323,    // GearVR Commercial 2 (USB Type C)
  ovrpSystemHeadset_GearVR_R324,    // GearVR Commercial 3 (USB Type C)
  ovrpSystemHeadset_GearVR_R325,    // GearVR Commercial 4 (USB Type C)
  ovrpSystemHeadset_Oculus_Go,      // Oculus Go Commercial 1
  ovrpSystemHeadset_Oculus_Quest,   // Oculus Quest
  ovrpSystemHeadset_Oculus_Quest_2,  // Oculus Quest 2
  ovrpSystemHeadset_Placeholder_10,
  ovrpSystemHeadset_Placeholder_11,
  ovrpSystemHeadset_Placeholder_12,
  ovrpSystemHeadset_Placeholder_13,
  ovrpSystemHeadset_Placeholder_14,

  // PC headsets
  ovrpSystemHeadset_Rift_DK1 = 0x1000,
  ovrpSystemHeadset_Rift_DK2,
  ovrpSystemHeadset_Rift_CV1,
  ovrpSystemHeadset_Rift_CB,
  ovrpSystemHeadset_Rift_S,
  ovrpSystemHeadset_Oculus_Link_Quest,    // Quest connected through Oculus Link
  ovrpSystemHeadset_Oculus_Link_Quest_2,
  ovrpSystemHeadset_PC_Placeholder_4103,
  ovrpSystemHeadset_PC_Placeholder_4104,
  ovrpSystemHeadset_PC_Placeholder_4105,
  ovrpSystemHeadset_PC_Placeholder_4106,
  ovrpSystemHeadset_PC_Placeholder_4107,
  ovrpSystemHeadset_EnumSize = 0x7fffffff
} ovrpSystemHeadset;

/// These types are used to hide platform-specific details when passing
/// render device, OS, and texture data to the API.
///
/// The benefit of having these wrappers versus platform-specific API functions is
/// that they allow game glue code to be portable. A typical example is an
/// engine that has multiple back ends, say GL and D3D. Portable code that calls
/// these back ends may also use LibOVR. To do this, back ends can be modified
/// to return portable types such as ovrTexture and ovrRenderAPIConfig.
typedef enum {
  ovrpRenderAPI_None,
  ovrpRenderAPI_OpenGL,
  ovrpRenderAPI_Android_GLES, // Deprecated, use ovrpRenderAPI_OpenGL instead
  ovrpRenderAPI_D3D9, // Deprecated, unsupported
  ovrpRenderAPI_D3D10, // Deprecated, unsupported
  ovrpRenderAPI_D3D11,
  ovrpRenderAPI_D3D12,
  ovrpRenderAPI_Vulkan,
  ovrpRenderAPI_Count,
  ovrpRenderAPI_EnumSize = 0x7fffffff
} ovrpRenderAPIType;

/// Identifies a controller button.
typedef enum {
  ovrpButton_None = 0,
  ovrpButton_A = 0x00000001,
  ovrpButton_B = 0x00000002,
  ovrpButton_X = 0x00000100,
  ovrpButton_Y = 0x00000200,
  ovrpButton_Up = 0x00010000,
  ovrpButton_Down = 0x00020000,
  ovrpButton_Left = 0x00040000,
  ovrpButton_Right = 0x00080000,
  ovrpButton_Start = 0x00100000,
  ovrpButton_Back = 0x00200000,
  ovrpButton_LShoulder = 0x00000800,
  ovrpButton_LThumb = 0x00000400,
  ovrpButton_LTouchpad = 0x40000000,
  ovrpButton_RShoulder = 0x00000008,
  ovrpButton_RThumb = 0x00000004,
  ovrpButton_RTouchpad = 0x80000000,
  ovrpButton_VolUp = 0x00400000,
  ovrpButton_VolDown = 0x00800000,
  ovrpButton_Home = 0x01000000,
  ovrpButton_EnumSize = 0x7fffffff
} ovrpButton;

/// Identifies a controller touch.
typedef enum {
  ovrpTouch_None = 0,
  ovrpTouch_A = ovrpButton_A,
  ovrpTouch_B = ovrpButton_B,
  ovrpTouch_X = ovrpButton_X,
  ovrpTouch_Y = ovrpButton_Y,
  ovrpTouch_LIndexTrigger = 0x00001000,
  ovrpTouch_LThumb = ovrpButton_LThumb,
  ovrpTouch_LThumbRest = 0x00000800,
  ovrpTouch_LTouchpad = ovrpButton_LTouchpad,
  ovrpTouch_RIndexTrigger = 0x00000010,
  ovrpTouch_RThumb = ovrpButton_RThumb,
  ovrpTouch_RThumbRest = 0x00000008,
  ovrpTouch_RTouchpad = ovrpButton_RTouchpad,
  ovrpTouch_EnumSize = 0x7fffffff
} ovrpTouch;

/// Identifies a controller near touch.
typedef enum {
  ovrpNearTouch_None = 0,
  ovrpNearTouch_LIndexTrigger = 0x00000001,
  ovrpNearTouch_LThumbButtons = 0x00000002,
  ovrpNearTouch_RIndexTrigger = 0x00000004,
  ovrpNearTouch_RThumbButtons = 0x00000008,
  ovrpNearTouch_EnumSize = 0x7fffffff
} ovrpNearTouch;

/// Identifies a controller.
typedef enum {
  ovrpController_None = 0,
  ovrpController_LTouch = 0x01,
  ovrpController_RTouch = 0x02,
  ovrpController_Touch = ovrpController_LTouch | ovrpController_RTouch,
  ovrpController_Remote = 0x04,
  ovrpController_Gamepad = 0x10,
  ovrpController_LHand = 0x20,
  ovrpController_RHand = 0x40,
  ovrpController_Hands = ovrpController_LHand | ovrpController_RHand,
  ovrpController_Touchpad_DEPRECATED = 0x08000000,
  ovrpController_LTrackedRemote = 0x01000000,
  ovrpController_RTrackedRemote = 0x02000000,
  ovrpController_Active = 0x80000000,
  ovrpController_EnumSize = 0x7fffffff
} ovrpController;

/// Used to specify recentering behavior.
typedef enum {
  /// Recenter all default axes as defined by the current tracking origin type.
  ovrpRecenterFlag_Default = 0x00000000,
  /// Recenter only controllers that require drift correction.
  ovrpRecenterFlag_Controllers = 0x40000000,
  /// Clear the ShouldRecenter flag and leave all axes unchanged. Useful for apps that perform
  /// custom recentering logic.
  ovrpRecenterFlag_IgnoreAll = 0x80000000,
  ovrpRecenterFlag_EnumSize = 0x7fffffff
} ovrpRecenterFlag;

/// Logging levels
typedef enum {
  ovrpLogLevel_Debug = 0,
  ovrpLogLevel_Info = 1,
  ovrpLogLevel_Error = 2,
  ovrpLogLevel_EnumSize = 0x7fffffff
} ovrpLogLevel;

/// Foveation levels
///
/// Levels should be consecutive integer enums, otherwise change GetTiledMultiResLevel
/// and SetTiledMultiResLevel to work without that assumption
typedef enum {
  ovrpTiledMultiResLevel_Off = 0,
  ovrpTiledMultiResLevel_LMSLow = 1,
  ovrpTiledMultiResLevel_LMSMedium = 2,
  ovrpTiledMultiResLevel_LMSHigh = 3,
  ovrpTiledMultiResLevel_LMSHighTop = 4,
  ovrpTiledMultiResLevel_EnumSize = 0x7fffffff
} ovrpTiledMultiResLevel;

/// Control the activation of mixed reality capture
typedef enum {
  ovrpMediaMrcActivationMode_Automatic = 0,
  ovrpMediaMrcActivationMode_Disabled = 1,
  ovrpMediaMrcActivationMode_EnumSize = 0x7fffffff
} ovrpMediaMrcActivationMode;

/// Control the platform camera status
typedef enum {
  ovrpPlatformCameraMode_Disabled = -1,
  ovrpPlatformCameraMode_Initialized = 0,
  ovrpPlatformCameraMode_UserControlled = 1,        //Ex: Quest user grab and
  ovrpPlatformCameraMode_SmartNavigated = 2,        //Ex: Program to follow/zoom, avoid obstacle ...etc
  ovrpPlatformCameraMode_StabilizedPoV = 3,         //Ex: Stabilized 1st person view
  ovrpPlatformCameraMode_RemoteDroneControlled = 4, //Ex: Control by remote clients
  ovrpPlatformCameraMode_RemoteSpatialMapped = 5,   //Ex: Control by anchor pose and SLAM tracking
  ovrpPlatformCameraMode_EnumSize = 0x7fffffff
} ovrpPlatformCameraMode;

/// Media encoder input buffer types
typedef enum {
  /// raw memory. pixel format in RGBA
  ovrpMediaInputVideoBufferType_Memory = 0,
  /// texture handle (e.g. texId if GLES)
  ovrpMediaInputVideoBufferType_TextureHandle = 1,
  ovrpMediaInputVideoBufferType_EnumSize = 0x7fffffff
} ovrpMediaInputVideoBufferType;

#if defined(__arm__)
typedef void(* ovrpLogCallback)(ovrpLogLevel, const char*);
#else
typedef void(__cdecl* ovrpLogCallback)(ovrpLogLevel, const char*);
#endif

typedef struct {
  int MajorVersion;
  int MinorVersion;
  int PatchVersion;
} ovrpVersion;

typedef struct {
  float LatencyRender;
  float LatencyTimewarp;
  float LatencyPostPresent;
  float ErrorRender;
  float ErrorTimewarp;
} ovrpAppLatencyTimings;

enum { ovrpAppPerfFrameStatsMaxCount = 5 };

/// App Perf Frame Stats
typedef struct {
  int HmdVsyncIndex;

  int AppFrameIndex;
  int AppDroppedFrameCount;
  float AppMotionToPhotonLatency;
  float AppQueueAheadTime;
  float AppCpuElapsedTime;
  float AppGpuElapsedTime;

  int CompositorFrameIndex;
  int CompositorDroppedFrameCount;
  float CompositorLatency;
  float CompositorCpuElapsedTime;
  float CompositorGpuElapsedTime;
  float CompositorCpuStartToGpuEndElapsedTime;
  float CompositorGpuEndToVsyncElapsedTime;
} ovrpAppPerfFrameStats;

/// App Perf Stats
typedef struct {
  ovrpAppPerfFrameStats FrameStats[ovrpAppPerfFrameStatsMaxCount];
  int FrameStatsCount;
  ovrpBool AnyFrameStatsDropped;
  float AdaptiveGpuPerformanceScale;
} ovrpAppPerfStats;

/// Cross-platform perf metrics
typedef enum {
  ovrpPerfMetrics_App_CpuTime_Float = 0,
  ovrpPerfMetrics_App_GpuTime_Float = 1,
  ovrpPerfMetrics_App_MotionToPhotonLatencyTime_Float_DEPRECATED = 2,

  ovrpPerfMetrics_Compositor_CpuTime_Float = 3,
  ovrpPerfMetrics_Compositor_GpuTime_Float = 4,
  ovrpPerfMetrics_Compositor_DroppedFrameCount_Int = 5,
  ovrpPerfMetrics_Compositor_LatencyTime_Float_DEPRECATED = 6,

  ovrpPerfMetrics_System_GpuUtilPercentage_Float = 7,
  ovrpPerfMetrics_System_CpuUtilAveragePercentage_Float = 8,
  ovrpPerfMetrics_System_CpuUtilWorstPercentage_Float = 9,

  // 1.32.0
  ovrpPerfMetrics_Device_CpuClockFrequencyInMHz_Float = 10,
  ovrpPerfMetrics_Device_GpuClockFrequencyInMHz_Float = 11,
  ovrpPerfMetrics_Device_CpuClockLevel_Int = 12,
  ovrpPerfMetrics_Device_GpuClockLevel_Int = 13,

  ovrpPerfMetrics_Count,
  ovrpPerfMetrics_Max = 0x7fffffff,
} ovrpPerfMetrics;

/// A 2D size with integer components.
typedef struct { int w, h; } ovrpSizei;

/// A 2D size with float components.
typedef struct { float w, h; } ovrpSizef;

/// A 2D vector with integer components.
typedef struct { int x, y; } ovrpVector2i;

/// A 2D vector with float components.
typedef struct { float x, y; } ovrpVector2f;

/// A 3D vector with float components.
typedef struct { float x, y, z; } ovrpVector3f;

/// A 4D vector with float components.
typedef struct { float x, y, z, w; } ovrpVector4f;

/// A 4D vector with Int16 components.
typedef struct { ovrpInt16 x, y, z, w; } ovrpVector4s;

/// A quaternion rotation.
typedef struct { float x, y, z, w; } ovrpQuatf;

/// Row-major 4x4 matrix.
typedef struct { float M[4][4]; } ovrpMatrix4f;

/// Position and orientation together.
typedef struct {
  ovrpQuatf Orientation;
  ovrpVector3f Position;
} ovrpPosef;

/// Position and orientation together.
typedef struct {
  ovrpPosef Pose;
  ovrpVector3f Velocity;
  ovrpVector3f Acceleration;
  ovrpVector3f AngularVelocity;
  ovrpVector3f AngularAcceleration;
  double Time;
} ovrpPoseStatef;

/// Asymmetric fov port
typedef struct {
  float UpTan;
  float DownTan;
  float LeftTan;
  float RightTan;
} ovrpFovf;

/// Asymmetric frustum for a camera.
typedef struct {
  /// Near clip plane.
  float zNear;
  /// Far clip plane.
  float zFar;
  ovrpFovf Fov;
} ovrpFrustum2f;

/// A 2D rectangle with a position and size as integers.
typedef struct {
  ovrpVector2i Pos;
  ovrpSizei Size;
} ovrpRecti;

/// A 2D rectangle with a position and size as floats.
typedef struct {
  ovrpVector2f Pos;
  ovrpSizef Size;
} ovrpRectf;

typedef struct {
  ovrpRectf LeftRect;
  ovrpRectf RightRect;
  ovrpVector4f LeftScaleBias;
  ovrpVector4f RightScaleBias;
} ovrpTextureRectMatrixf;

typedef struct {
  float WarpLeft;
  float WarpRight;
  float WarpUp;
  float WarpDown;
  float SizeLeft;
  float SizeRight;
  float SizeUp;
  float SizeDown;
} ovrpOctilinearLayout;

typedef struct { float r, g, b, a; } ovrpColorf;

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
  unsigned char BatteryPercentRemaining[2];
  unsigned char RecenterCount[2];
  unsigned char Reserved[28];
} ovrpControllerState4;

/// Describes Haptics Buffer for use with Oculus Controllers.
typedef struct {
  const void* Samples;
  int SamplesCount;
} ovrpHapticsBuffer;

typedef struct {
  int SamplesAvailable;
  int SamplesQueued;
} ovrpHapticsState;

typedef struct {
  int SampleRateHz;
  int SampleSizeInBytes;
  int MinimumSafeSamplesQueued;
  int MinimumBufferSamplesCount;
  int OptimalBufferSamplesCount;
  int MaximumBufferSamplesCount;
} ovrpHapticsDesc;

/// Boundary types that specify a surface in the boundary system
typedef enum {
  /// Outer boundary - closely represents user setup walls, floor and ceiling
  ovrpBoundary_Outer = 0x0001,
  /// Play area - smaller convex area inside outer boundary where gameplay happens
  ovrpBoundary_PlayArea = 0x0100,
} ovrpBoundaryType;

/// Contains boundary test information
typedef struct {
  /// Indicates if the boundary system is being triggered and visible
  ovrpBool IsTriggering;
  /// Distance to the closest play area or outer boundary surface
  float ClosestDistance;
  /// Closest point in the surface
  ovrpVector3f ClosestPoint;
  /// Normal of the closest point
  ovrpVector3f ClosestPointNormal;
} ovrpBoundaryTestResult;

/// Boundary system look and feel
typedef struct {
  // Modulate color and alpha (color, brightness and opacity)
  ovrpColorf Color;
} ovrpBoundaryLookAndFeel;

/// Boundary system geometry
typedef struct {
  /// The boundary type that the geometry represents.
  ovrpBoundaryType BoundaryType;
  /// A pointer to a clock-wise ordered array of points. Max count of 256.
  ovrpVector3f Points[256];
  /// The number of points. Max count of 256.
  int PointsCount;
} ovrpBoundaryGeometry;

typedef struct {
  /// Distance between eyes.
  float InterpupillaryDistance;
  /// Eye height relative to the ground.
  float EyeHeight;
  /// Eye offset forward from the head center at EyeHeight.
  float HeadModelDepth;
  /// Neck joint offset down from the head center at EyeHeight.
  float HeadModelHeight;
} ovrpHeadModelParms;

typedef enum { ovrpFunctionEndFrame = 0, ovrpFunctionCreateTexture } ovrpFunctionType;

/// Camera status
typedef enum {
  ovrpCameraStatus_None,
  ovrpCameraStatus_Connected,
  ovrpCameraStatus_Calibrating,
  ovrpCameraStatus_CalibrationFailed,
  ovrpCameraStatus_Calibrated,
  ovrpCameraStatus_ThirdPerson,
  ovrpCameraStatus_EnumSize = 0x7fffffff
} ovrpCameraStatus;

// Camera anchor types
typedef enum {
	ovrpCameraAnchorType_PreDefined = 0,
	ovrpCameraAnchorType_Custom = 1,
  ovrpCameraAnchorType_Count,
	ovrpCameraAnchorType_EnumSize = 0x7fffffff
} ovrpCameraAnchorType;

/// Camera intrinsics
typedef struct {
  ovrpBool IsValid;
  double LastChangedTimeSeconds;
  ovrpFovf FOVPort;
  float VirtualNearPlaneDistanceMeters;
  float VirtualFarPlaneDistanceMeters;
  ovrpSizei ImageSensorPixelResolution;
} ovrpCameraIntrinsics;

/// Camera extrinsics
typedef struct {
  ovrpBool IsValid;
  double LastChangedTimeSeconds;
  ovrpCameraStatus CameraStatus;
  ovrpNode AttachedToNode;
  ovrpPosef RelativePose;
} ovrpCameraExtrinsics;

typedef void* ovrpCameraAnchorHandle;
#define OVRP_ANCHOR_INVALID_HANDLE    nullptr

#define OVRP_EXTERNAL_CAMERA_NAME_SIZE 32
#define OVRP_ANCHOR_NAME_SIZE 32

#if !OVRP_MIXED_REALITY_PRIVATE
/// Unified camera device types
typedef enum {
  ovrpCameraDevice_None = 0,
  ovrpCameraDevice_WebCamera_First = 100,
  ovrpCameraDevice_WebCamera0 = ovrpCameraDevice_WebCamera_First + 0,
  ovrpCameraDevice_WebCamera1 = ovrpCameraDevice_WebCamera_First + 1,
  ovrpCameraDevice_WebCamera_Last = ovrpCameraDevice_WebCamera1,
  ovrpCameraDevice_DEPRECATED_DEVICE = 300, // ovrpCameraDevice_ZEDStereoCamera before deprecation
  ovrpCameraDevice_EnumSize = 0x7fffffff
} ovrpCameraDevice;
#endif

typedef enum {
  ovrpCameraDeviceDepthSensingMode_Standard = 0,
  ovrpCameraDeviceDepthSensingMode_Fill,
  ovrpCameraDeviceDepthSensingMode_EnumSize = 0x7fffffff
} ovrpCameraDeviceDepthSensingMode;

typedef enum {
  ovrpCameraDeviceDepthQuality_Low = 0,
  ovrpCameraDeviceDepthQuality_Medium,
  ovrpCameraDeviceDepthQuality_High,
  ovrpCameraDeviceDepthQuality_EnumSize = 0x7fffffff
} ovrpCameraDeviceDepthQuality;

typedef struct {
  float fx; /* Focal length in pixels along x axis. */
  float fy; /* Focal length in pixels along y axis. */
  float cx; /* Optical center along x axis, defined in pixels (usually close to width/2). */
  float cy; /* Optical center along y axis, defined in pixels (usually close to height/2). */
  double disto[5]; /* Distortion factor : [ k1, k2, p1, p2, k3 ]. Radial (k1,k2,k3) and Tangential (p1,p2) distortion.*/
  float v_fov; /* Vertical field of view after stereo rectification, in degrees. */
  float h_fov; /* Horizontal field of view after stereo rectification, in degrees.*/
  float d_fov; /* Diagonal field of view after stereo rectification, in degrees.*/
  int w; /* Resolution width */
  int h; /* Resolution height */
} ovrpCameraDeviceIntrinsicsParameters;

const static ovrpPosef s_identityPose = {{0, 0, 0, 1}, {0, 0, 0}};
const static ovrpPoseStatef s_identityPoseState =
    {{{0, 0, 0, 1}, {0, 0, 0}}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, 0};
const static ovrpFrustum2f s_identityFrustum2 = {0, 0, {0, 0, 0, 0}};
const static ovrpVector4f s_vec4Zero = {0, 0, 0, 0};
const static ovrpVector3f s_vec3Zero = {0, 0, 0};
const static ovrpVector2f s_vec2Zero = {0, 0};
const static ovrpVector3f s_vec3One = {1, 1, 1};
const static ovrpQuatf s_identityQuat = {0, 0, 0, 1};
const static ovrpFovf s_identityFov = {1.0f, 1.0f, 1.0f, 1.0f};
const static ovrpCameraIntrinsics s_invalidCameraIntrinsics = {ovrpBool_False, -1, {0, 0, 0, 0}, 0, 0, {0, 0}};
const static ovrpCameraExtrinsics s_invalidCameraExtrinsics = {ovrpBool_False,
                                                               -1,
                                                               ovrpCameraStatus_None,
                                                               ovrpNode_None,
                                                               {{0, 0, 0, 1}, {0, 0, 0}}};

/// Texture handle which can be cast to GLuint, VkImage, ID3D11Texture2D*, or ID3D12Resource*
typedef unsigned long long ovrpTextureHandle;

/// Flags passed to ovrp_SetupDistortionWindow.
typedef enum {
  ovrpDistortionWindowFlag_None = 0x00000000,
  /// If true, the distortion window and eye buffers are set up to handle DRM-protected content.
  ovrpDistortionWindowFlag_Protected = 0x00000001,
  /// If true, the compositor's graphics device skips error checking to improve performance.
  ovrpDistortionWindowFlag_NoErrorContext = 0x00000002,
  /// Reserved 0x00000004 in ovrp_SetupDistortionWindow3

  ovrpDistortionWindowFlag_PhaseSync = 0x00000008,

  ovrpDistortionWindowFlag_EnumSize = 0x7fffffff
} ovrpDistortionWindowFlag;

/// A timestep type corresponding to a use case for tracking data.
typedef enum {
  /// Updated from game thread at end of frame, to hand-off state to Render thread.
  ovrpStep_Render = -1,
  /// Updated from physics thread, once per simulation step.
  ovrpStep_Physics = 0,
  ovrpStep_EnumSize = 0x7fffffff
} ovrpStep;

typedef enum {
  ovrpShape_Quad = 0,
  ovrpShape_Cylinder = 1,
  ovrpShape_Cubemap = 2,
  ovrpShape_EyeFov = 3,
  ovrpShape_OffcenterCubemap = 4,
  ovrpShape_Equirect = 5,
  ovrpShape_ReconstructionPassthrough = 7,
  ovrpShape_SurfaceProjectedPassthrough = 8,
  ovrpShape_Fisheye = 9,



  ovrpShape_EnumSize = 0xF
} ovrpShape;

typedef enum {
  ovrpLayout_Stereo = 0,
  ovrpLayout_Mono = 1,
  ovrpLayout_DoubleWide = 2,
  ovrpLayout_Array = 3,
  ovrpLayout_EnumSize = 0xF
} ovrpLayout;

/// A texture format.
typedef enum {
  ovrpTextureFormat_R8G8B8A8_sRGB = 0,
  ovrpTextureFormat_R8G8B8A8 = 1,
  ovrpTextureFormat_R16G16B16A16_FP = 2,
  ovrpTextureFormat_R11G11B10_FP = 3,
  ovrpTextureFormat_B8G8R8A8_sRGB = 4,
  ovrpTextureFormat_B8G8R8A8 = 5,
  ovrpTextureFormat_R5G6B5 = 11,
  ovrpTextureFormat_R16G16_FP = 12,
  ovrpTextureFormat_A2B10G10R10 = 13,

  //depth texture formats
  ovrpTextureFormat_D16 = 6,
  ovrpTextureFormat_D24_S8 = 7,
  ovrpTextureFormat_D32_FP = 8,
  ovrpTextureFormat_D32_S824_FP = 9,

  ovrpTextureFormat_None = 10,

  ovrpTextureFormat_EnumSize = 0x7fffffff
} ovrpTextureFormat;

/// Flags used by ovrpLayerDesc
typedef enum {
  /// Only create a single stage
  ovrpLayerFlag_Static = (1 << 0),
  /// Boost CPU priority while visible
  ovrpLayerFlag_LoadingScreen = (1 << 1),
  /// Force fov to be symmetric
  ovrpLayerFlag_SymmetricFov = (1 << 2),
  /// Texture origin is in bottom-left
  ovrpLayerFlag_TextureOriginAtBottomLeft = (1 << 3),
  /// Correct for chromatic aberration, deprecated
  ovrpLayerFlag_ChromaticAberrationCorrection = (1 << 4),
  /// Does not allocate texture space within the swapchain
  ovrpLayerFlag_NoAllocation = (1 << 5),
  /// Enable protected content, added in 1.23
  ovrpLayerFlag_ProtectedContent = (1 << 6),
  /// Allocate AndroidSurfaceSwapChain, instead of regular TextureSwapChain
  ovrpLayerFlag_AndroidSurfaceSwapChain = (1 << 7),
  /// Allocate SpaceWarp data with m_textureSwapChain together
  ovrpLayerFlag_SpaceWarpDataAllocation = (1 << 8),
  ovrpLayerFlag_SpaceWarpDedicatedDepth = (1 << 9),
  /// VrApi flag: VRAPI_ANDROID_SURFACE_SWAP_CHAIN_FLAG_SYNCHRONOUS
  ovrpLayerFlag_Synchronous = (1 << 10),
  /// VrApi flag:VRAPI_ANDROID_SURFACE_SWAP_CHAIN_FLAG_USE_TIMESTAMPS
  ovrpLayerFlag_UseTimestamps = (1 << 11),
  /// Allocate layer using subsampled layout
  ovrpLayerFlag_Subsampled = (1 << 12),
  /// if non-static, allocate 4 elements in a swapchain
  ovrpLayerFlag_4DeepSwapchain = (1 << 13),




  /// The layer is created through ovrp_EnqueueSetupLayer()
  ovrpLayerFlag_LegacyOverlay = (1 << 15),
} ovrpLayerFlags;

/// Layer description used by ovrp_SetupLayer to create the layer
#define OVRP_LAYER_DESC       \
  struct {                    \
    ovrpShape Shape;          \
    ovrpLayout Layout;        \
    ovrpSizei TextureSize;    \
    int MipLevels;            \
    int SampleCount;          \
    ovrpTextureFormat Format; \
    int LayerFlags;           \
  }

typedef OVRP_LAYER_DESC ovrpLayerDesc;

#define OVRP_LAYER_DESC_TYPE \
  union {                    \
    ovrpLayerDesc Base;      \
    OVRP_LAYER_DESC;         \
  }

typedef OVRP_LAYER_DESC_TYPE ovrpLayerDesc_Quad;
typedef OVRP_LAYER_DESC_TYPE ovrpLayerDesc_Cylinder;
typedef OVRP_LAYER_DESC_TYPE ovrpLayerDesc_Cubemap;



typedef OVRP_LAYER_DESC_TYPE ovrpLayerDesc_InsightPassthrough;

typedef struct {
  OVRP_LAYER_DESC_TYPE;
  ovrpFovf Fov[ovrpEye_Count];
  ovrpRectf VisibleRect[ovrpEye_Count];
  ovrpSizei MaxViewportSize;
  //added for 1.17
  ovrpTextureFormat DepthFormat;

  //added for 1.49
  ovrpTextureFormat MotionVectorFormat;
  ovrpTextureFormat MotionVectorDepthFormat;
  ovrpSizei MotionVectorTextureSize;
} ovrpLayerDesc_EyeFov;

typedef OVRP_LAYER_DESC_TYPE ovrpLayerDesc_OffcenterCubemap;
typedef OVRP_LAYER_DESC_TYPE ovrpLayerDesc_Equirect;
typedef OVRP_LAYER_DESC_TYPE ovrpLayerDesc_Fisheye;

typedef union {
  OVRP_LAYER_DESC_TYPE;
  ovrpLayerDesc_Quad Quad;
  ovrpLayerDesc_Cylinder Cylinder;
  ovrpLayerDesc_Cubemap Cubemap;
  ovrpLayerDesc_EyeFov EyeFov;
  ovrpLayerDesc_OffcenterCubemap OffcenterCubemap;
  ovrpLayerDesc_Equirect Equirect;
  ovrpLayerDesc_Fisheye Fisheye;



  ovrpLayerDesc_InsightPassthrough InsightPassthrough;
} ovrpLayerDescUnion;

#undef OVRP_LAYER_DESC
#undef OVRP_LAYER_DESC_TYPE

/// Flags used by ovrpLayerSubmit
typedef enum {
  /// Pose relative to head
  ovrpLayerSubmitFlag_HeadLocked = (1 << 0),
  /// Layer is octilinear (LMS)
  ovrpLayerSubmitFlag_Octilinear = (1 << 1),
  /// Use reverse Z
  ovrpLayerSubmitFlag_ReverseZ = (1 << 2),
  /// Disable layer depth compositing on Rift
  ovrpLayerSubmitFlag_NoDepth = (1 << 3),
  /// Use inverse alpha for timewarp blending
  ovrpLayerSubmitFlag_InverseAlpha = (1 << 4),
  /// Combine the submitted layer with the layers generated from OVROverlay commands
  ovrpLayerSubmitFlag_CombineLayerSubmits = (1 << 5),
  /// Enable VrApi "Expensive" SuperSample Flag.
  ovrpLayerSubmitFlag_ExpensiveSuperSample = (1 << 8),
  /// Enable per-overlay show/hide functionality.
  ovrpLayerSubmitFlag_Hidden = (1 << 9),
  /// Force the texture's alpha to 1.0 on Rift
  ovrpLayerSubmitFlag_IgnoreSourceAlpha = (1 << 10),
  /// Enable Space warp on Fov layer
  ovrpLayerSubmitFlag_SpaceWarp = (1 << 11),






} ovrpLayerSubmitFlags;

/// Factors used for source and dest alpha to make up the blend function.
typedef enum {
  ovrpBlendFactorZero = 0,
  ovrpBlendFactorOne = 1,
  ovrpBlendFactorSrcAlpha = 2,
  ovrpBlendFactorOneMinusSrcAlpha = 3,
  ovrpBlendFactorDstAlpha = 4,
  ovrpBlendFactorOneMinusDstAlpha = 5
} ovrpBlendFactor;

/// Layer state to submit to ovrp_EndFrame
#define OVRP_LAYER_SUBMIT                         \
  struct {                                        \
    int LayerId;                                  \
    int TextureStage;                             \
    ovrpRecti ViewportRect[ovrpEye_Count];        \
    ovrpPosef Pose;                               \
    int LayerSubmitFlags;                         \
    /* Added in 1.31 */                           \
    ovrpVector4f ColorScale;                      \
    ovrpVector4f ColorOffset;                     \
    /* Added in 1.34 */                           \
    ovrpBool OverrideTextureRectMatrix;           \
    ovrpTextureRectMatrixf TextureRectMatrix;     \
    ovrpBool OverridePerLayerColorScaleAndOffset; \
    /* Added in 1.60 */                           \
    /* If blend factors are present (signaled by `HasBlendFactors == true`),*/\
    /* they override the default blend function and all other influences    */\
    /* like the layer submit flags `ovrpLayerSubmitFlag_InverseAlpha` and   */\
    /* `ovrpLayerSubmitFlag_IgnoreSourceAlpha`.                             */\
    /* Blend factors are not supported by CAPI and are ignored in the CAPI  */\
    /* implementation.                                                      */\
    ovrpBool HasBlendFactors;                     \
    ovrpBlendFactor SrcBlendFactor;               \
    ovrpBlendFactor DstBlendFactor;               \
  }

typedef OVRP_LAYER_SUBMIT ovrpLayerSubmit;

#define OVRP_LAYER_SUBMIT_TYPE \
  union {                      \
    ovrpLayerSubmit Base;      \
    OVRP_LAYER_SUBMIT;         \
  }

typedef struct {
  OVRP_LAYER_SUBMIT_TYPE;
  ovrpSizef Size;
} ovrpLayerSubmit_Quad;

typedef struct {
  OVRP_LAYER_SUBMIT_TYPE;
  float ArcWidth;
  float Height;
  float Radius;
} ovrpLayerSubmit_Cylinder;









typedef OVRP_LAYER_SUBMIT_TYPE ovrpLayerSubmit_Cubemap;

typedef struct {
  OVRP_LAYER_SUBMIT_TYPE;
  // added in 1.18
  ovrpOctilinearLayout OctilinearLayout[ovrpEye_Count];
  float DepthNear;
  float DepthFar;
  // added in 1.44
  ovrpFovf Fov[ovrpEye_Count];
  // added in 1.49
  float MotionVectorDepthNear;
  float MotionVectorDepthFar;
  ovrpVector4f MotionVectorScale;
  ovrpVector4f MotionVectorOffset;
  ovrpPosef AppSpaceDeltaPose;
} ovrpLayerSubmit_EyeFov;

typedef OVRP_LAYER_SUBMIT_TYPE ovrpLayerSubmit_OffcenterCubemap;
typedef OVRP_LAYER_SUBMIT_TYPE ovrpLayerSubmit_Equirect;

typedef struct {
  OVRP_LAYER_SUBMIT_TYPE;
  float FovX;
  float FovY;
  float Horizon;
  float Meridian;
} ovrpLayerSubmit_Fisheye;

typedef union {
  OVRP_LAYER_SUBMIT_TYPE;
  ovrpLayerSubmit_Quad Quad;
  ovrpLayerSubmit_Cylinder Cylinder;
  ovrpLayerSubmit_Cubemap Cubemap;
  ovrpLayerSubmit_EyeFov EyeFov;
  ovrpLayerSubmit_OffcenterCubemap OffcenterCubemap;
  ovrpLayerSubmit_Equirect Equirect;
  ovrpLayerSubmit_Fisheye Fisheye;



} ovrpLayerSubmitUnion;

typedef enum {
  ovrpViewportStencilType_HiddenArea = 0,
  ovrpViewportStencilType_VisibleArea = 1,
  ovrpViewportStencilType_BorderLine = 2,
  ovrpViewportStencilType_VisibleRectangle = 3,
} ovrpViewportStencilType;

#undef OVRP_LAYER_SUBMIT
#undef OVRP_LAYER_SUBMIT_TYPE

//-----------------------------------------------------------------
// Hand tracking
//-----------------------------------------------------------------

typedef enum ovrpHandStatus_ {
  ovrpHandStatus_HandTracked = (1 << 0), // hand is currently tracked by hand tracking
  ovrpHandStatus_InputValid = (1 << 1), // if this is set the pointer pose and pinch data is usable
  ovrpHandStatus_SystemGestureInProgress = (1 << 6), // if this is set the user is performing the system gesture
  ovrpHandStatus_DominantHand = (1 << 7), // if this is set the hand is considered the dominant hand
  ovrpHandStatus_MenuPressed = (1 << 8), // if this is set the hand performed the system gesture as the non-dominant hand
  ovrpHandStatus_EnumSize = 0x7fffffff
} ovrpHandStatus;

typedef enum ovrpHandFinger_ {
  ovrpHandFinger_Thumb = 0,
  ovrpHandFinger_Index = 1,
  ovrpHandFinger_Middle = 2,
  ovrpHandFinger_Ring = 3,
  ovrpHandFinger_Pinky = 4,
  ovrpHandFinger_Max,
  ovrpHandFinger_EnumSize = 0x7fffffff
} ovrpHandFinger;

typedef enum ovrpHandFingerPinch_ {
  ovrpHandFingerPinch_Thumb    = (1 << ovrpHandFinger_Thumb),
  ovrpHandFingerPinch_Index    = (1 << ovrpHandFinger_Index),
  ovrpHandFingerPinch_Middle   = (1 << ovrpHandFinger_Middle),
  ovrpHandFingerPinch_Ring     = (1 << ovrpHandFinger_Ring),
  ovrpHandFingerPinch_Pinky    = (1 << ovrpHandFinger_Pinky),
  ovrpHandFingerPinch_Max,
  ovrpHandFingerPinch_EnumSize = 0x7fffffff
} ovrpHandFingerPinch;

typedef enum ovrpBoneId_ {
  ovrpBoneId_Invalid                 = -1,

  // hand bones
  ovrpBoneId_Hand_Start              = 0,
  ovrpBoneId_Hand_WristRoot          = ovrpBoneId_Hand_Start + 0, // root frame of the hand, where the wrist is located
  ovrpBoneId_Hand_ForearmStub        = ovrpBoneId_Hand_Start + 1, // frame for user's forearm
  ovrpBoneId_Hand_Thumb0             = ovrpBoneId_Hand_Start + 2, // thumb trapezium bone
  ovrpBoneId_Hand_Thumb1             = ovrpBoneId_Hand_Start + 3, // thumb metacarpal bone
  ovrpBoneId_Hand_Thumb2             = ovrpBoneId_Hand_Start + 4, // thumb proximal phalange bone
  ovrpBoneId_Hand_Thumb3             = ovrpBoneId_Hand_Start + 5, // thumb distal phalange bone
  ovrpBoneId_Hand_Index1             = ovrpBoneId_Hand_Start + 6, // index proximal phalange bone
  ovrpBoneId_Hand_Index2             = ovrpBoneId_Hand_Start + 7, // index intermediate phalange bone
  ovrpBoneId_Hand_Index3             = ovrpBoneId_Hand_Start + 8, // index distal phalange bone
  ovrpBoneId_Hand_Middle1            = ovrpBoneId_Hand_Start + 9, // middle proximal phalange bone
  ovrpBoneId_Hand_Middle2            = ovrpBoneId_Hand_Start + 10, // middle intermediate phalange bone
  ovrpBoneId_Hand_Middle3            = ovrpBoneId_Hand_Start + 11, // middle distal phalange bone
  ovrpBoneId_Hand_Ring1              = ovrpBoneId_Hand_Start + 12, // ring proximal phalange bone
  ovrpBoneId_Hand_Ring2              = ovrpBoneId_Hand_Start + 13, // ring intermediate phalange bone
  ovrpBoneId_Hand_Ring3              = ovrpBoneId_Hand_Start + 14, // ring distal phalange bone
  ovrpBoneId_Hand_Pinky0             = ovrpBoneId_Hand_Start + 15, // pinky metacarpal bone
  ovrpBoneId_Hand_Pinky1             = ovrpBoneId_Hand_Start + 16, // pinky proximal phalange bone
  ovrpBoneId_Hand_Pinky2             = ovrpBoneId_Hand_Start + 17, // pinky intermediate phalange bone
  ovrpBoneId_Hand_Pinky3             = ovrpBoneId_Hand_Start + 18, // pinky distal phalange bone
  ovrpBoneId_Hand_MaxSkinnable       = ovrpBoneId_Hand_Start + 19,
  // Bone tips are position only. They are not used for skinning but useful for hit-testing.
  // NOTE: ovrBoneId_Hand_ThumbTip == ovrBoneId_Hand_MaxSkinnable since the extended tips need to be contiguous
  ovrpBoneId_Hand_ThumbTip           = ovrpBoneId_Hand_MaxSkinnable + 0, // tip of the thumb
  ovrpBoneId_Hand_IndexTip           = ovrpBoneId_Hand_MaxSkinnable + 1, // tip of the index finger
  ovrpBoneId_Hand_MiddleTip          = ovrpBoneId_Hand_MaxSkinnable + 2, // tip of the middle finger
  ovrpBoneId_Hand_RingTip            = ovrpBoneId_Hand_MaxSkinnable + 3, // tip of the ring finger
  ovrpBoneId_Hand_PinkyTip           = ovrpBoneId_Hand_MaxSkinnable + 4, // tip of the pinky
  ovrpBoneId_Hand_End                = ovrpBoneId_Hand_MaxSkinnable + 5,

























































  // add other skeleton bone definitions here...




  ovrpBoneId_Max                     = (ovrpBoneId_Hand_End > 50) ? ovrpBoneId_Hand_End : 50,

  ovrpBoneId_EnumSize = 0x7fff
} ovrpBoneId;

//-----------------------------------------------------------------
// Hand skeleton

// ovrBoneCapsule
//    _---_
//  -"     "-
// /         \
// |----A----|
// |    |    |
// |    |    |
// |    |-r->|
// |    |    |
// |    |    |
// |----B----|
// \         /
//  -.     .-
//    '---'
typedef struct ovrpBoneCapsule_ {
  short BoneIndex;
  // Points at either end of the cylinder inscribed in the capsule. Also the center points for
  // spheres at either end of the capsule. Points A and B in the diagram above.
  ovrpVector3f Points[2];
  // The radius of the capsule cylinder and of the half-sphere caps on the ends of the capsule.
  float Radius;
} ovrpBoneCapsule;

typedef struct ovrpBone_ {
  ovrpBoneId BoneId;
  // index of this bone's parent bone (-1 if no parent)
  short ParentBoneIndex;
  ovrpPosef Pose;
} ovrpBone;

typedef enum ovrpSkeletonConstants_ {
  ovrpSkeletonConstants_MaxHandBones = ovrpBoneId_Hand_End,



  ovrpSkeletonConstants_MaxBones = ovrpBoneId_Max,
  ovrpSkeletonConstants_MaxBoneCapsules = 19,
  ovrpSkeletonConstants_EnumSize = 0x7fffffff
} ovrpSkeletonConstants;

/// Identifies a skeleton type.
typedef enum ovrpSkeletonType_ {
  ovrpSkeletonType_None = -1,
  ovrpSkeletonType_HandLeft = 0,
  ovrpSkeletonType_HandRight = 1,



  ovrpSkeletonType_Count,
  ovrpSkeletonType_EnumSize = 0x7fffffff
} ovrpSkeletonType;

typedef struct ovrpSkeleton2_ {
  ovrpSkeletonType SkeletonType;
  unsigned int NumBones;
  unsigned int NumBoneCapsules;
  ovrpBone Bones[ovrpSkeletonConstants_MaxBones];
  ovrpBoneCapsule BoneCapsules[ovrpSkeletonConstants_MaxBoneCapsules];
} ovrpSkeleton2;

//-----------------------------------------------------------------
// Hand mesh

typedef enum ovrpMeshConstants_ {
  ovrpMesh_MaxVertices = 3000,
  ovrpMesh_MaxIndices = ovrpMesh_MaxVertices * 6,
  ovrpMesh_EnumSize = 0x7fffffff
} ovrpMeshConstants;

/// Identifies a mesh type.
typedef enum ovrpMeshType_ {
  ovrpMeshType_None = -1,
  ovrpMeshType_HandLeft = 0,
  ovrpMeshType_HandRight = 1,
  ovrpMeshType_Count,
  ovrpMeshType_EnumSize = 0x7fffffff
} ovrpMeshType;


typedef struct ovrpMesh_ {
  // Type of mesh this data describes.
  ovrpMeshType MeshType;
  // Number of unique vertices in the mesh.
  unsigned int NumVertices;
  // Number of unique indices in the mesh.
  unsigned int NumIndices;
  // An array of count NumVertices positions for each vertex. Always valid.
  ovrpVector3f VertexPositions[ovrpMesh_MaxVertices];
  // An array of count NumIndices of vertex indices specifying triangles that make up the mesh. Always valid.
  ovrpInt16 Indices[ovrpMesh_MaxIndices];
  // An array of count NumVertices of normals for each vertex.
  // If null, this attribute is not used.
  ovrpVector3f VertexNormals[ovrpMesh_MaxVertices];
  // An array of count NumVertices of texture coordinates for each vertex.
  // If null, this attribute is not used.
  ovrpVector2f VertexUV0[ovrpMesh_MaxVertices];
  // An array of count NumVertices of blend indices for each of the bones that each vertex is weighted to.
  // Always valid. An index of < 0 means no blend weight.
  ovrpVector4s BlendIndices[ovrpMesh_MaxVertices];
  // An array of count NumVertices of weights for each of the bones affecting each vertex. Always valid.
  ovrpVector4f BlendWeights[ovrpMesh_MaxVertices];
} ovrpMesh;

//-----------------------------------------------------------------
// Hand pose
typedef enum ovrpTrackingConfidence_ {
  ovrpTrackingConfidence_Low = 0,
  ovrpTrackingConfidence_High = 0x3f800000,
  ovrpTrackingConfidence_EnumSize = 0x7fffffff
} ovrpTrackingConfidence;

typedef struct ovrpHandState_ {
  // Hand Status bitfield described by ovrpHandStatus flags.
  unsigned int Status;

  // Root pose of the hand in world space. Not to be confused with the root bone's transform.
  // The root bone can still be offset from this by the skeleton's rest pose.
  ovrpPosef RootPose;

  // Current rotation of each bone.
  ovrpQuatf BoneRotations[ovrpSkeletonConstants_MaxHandBones];

  // Provides a bitmask indicating if each finger is "pinched" or not. Indexable via bitshifting with the ovrpHandFinger enum
  // i.e. (1 << ovrpHandFinger_Index)
  unsigned int Pinches;

  // Provides a 0.0f to 1.0f value of how "pinched" each finger is. Indexable via the ovrpHandFinger enum.
  float PinchStrength[ovrpHandFinger_Max];

  // World space position and translation of the pointer attached to the hand.
  ovrpPosef PointerPose;

  float HandScale;

  // Tracking confidence. Range [0,1], 0.0 = lowest confidence, 1.0 = highest confidence.
  // This is useful for smoothly de-emphasizing hands as confidence decreases.
  // This is the amount of confidence that the system has that the entire hand pose is correct.
  ovrpTrackingConfidence HandConfidence;

  // Per-finger tracking confidence. Range [0,1], 0.0 = lowest confidence, 1.0 = highest confidence.
  // This is the amount of confidence the system has that the individual finger poses are correct.
  ovrpTrackingConfidence FingerConfidences[ovrpHandFinger_Max];

  // Time stamp for the pose that was requested in global system time.
  double RequestedTimeStamp;

  // Time stamp of the captured sample that the pose was extrapolated from.
  double SampleTimeStamp;
} ovrpHandState;









































































//-----------------------------------------------------------------
// Color Space Management
//-----------------------------------------------------------------
/// Color space types for HMDs
///
/// Until vrapi_SetClientColorDesc is called, the client will default to Rec2020 for Quest and
/// Rec709 for Go HMDs.
///
/// This API only handles color-space remapping. Unless specified, all color spaces use D65 white
/// point. It will not affect brightness, contrast or gamma curves. Some of these aspects such as
/// gamma, is handled by the texture format being used. From the GPU samplers' point-of-view, each
/// texture will continue to be treated as linear luminance including sRGB which is converted to
/// linear by the texture sampler.
///
/// 'VRAPI_COLORSPACE_UNMANAGED' will force the runtime to skip color correction for the provided
/// content. This is *not* recommended unless the app developer is sure about what they're doing.
/// 'VRAPI_COLORSPACE_UNMANAGED' is mostly useful for research & experimentation, but not for
/// software distribution. This is because unless the client is applying the necessary corrections
/// for each HMD type, the results seen in the HMD will be uncalibrated. This is especially true for
/// future HMDs where the color space is not yet known or defined, which could lead to colors that
/// look too dull, too saturated, or hue shifted.
///
/// Although native Quest and Rift CV1 color spaces are provided as options, they are not
/// standardized color spaces. While we provide the exact color space primary coordinates, for
/// better standardized visualized of authored content, it's recommended that the developers master
/// using a well-defined color space in the provided in the options such as Rec.2020.
///
/// It is also recommended that content be authored for the wider color spaces instead of Rec.709 to
/// prevent visuals from looking "washed out", "dull" or "desaturated" on wider gamut devices like
/// the Quest.
///
/// Unique Color Space Details with Chromaticity Primaries in CIE 1931 xy:
///
/// Color Space: P3, similar to DCI-P3, but using D65 white point instead.
/// Red  : (0.680, 0.320)
/// Green: (0.265, 0.690)
/// Blue : (0.150, 0.060)
/// White: (0.313, 0.329)
///
/// Color Space: Rift CV1 between P3 & Adobe RGB using D75 white point
/// Red  : (0.666, 0.334)
/// Green: (0.238, 0.714)
/// Blue : (0.139, 0.053)
/// White: (0.298, 0.318)
///
/// Color Space: Quest similar to Rift CV1 using D75 white point
/// Red  : (0.661, 0.338)
/// Green: (0.228, 0.718)
/// Blue : (0.142, 0.042)
/// White: (0.298, 0.318)
///
/// Color Space: Rift S similar to Rec 709 using D75
/// Red  : (0.640, 0.330)
/// Green: (0.292, 0.586)
/// Blue : (0.156, 0.058)
/// White: (0.298, 0.318)
///
/// Note: Due to LCD limitations, the Go display will not be able to meaningfully differentiate
/// brightness levels below 13 out of 255 for 8-bit sRGB or 0.0015 out of 1.0 max for linear-RGB
/// shader output values. To that end, it is recommended that reliance on a dark and narrow gamut is
/// avoided, and the content is instead spread across a larger brightness range when possible.
///
typedef enum ovrpColorSpace_ {
    /// Default value until client sets calls SetClientColorDesc
    ovrpColorSpace_Unknown = 0,
    /// No color correction, not recommended for production use. See notes above for more info
    ovrpColorSpace_Unmanaged = 1,
    /// Preferred color space for standardized color across all Oculus HMDs with D65 white point
    ovrpColorSpace_Rec_2020 = 2,
    /// Rec. 709 is used on Oculus Go and shares the same primary color coordinates as sRGB
    ovrpColorSpace_Rec_709 = 3,
    /// Oculus Rift CV1 uses a unique color space, see enum description for more info
    ovrpColorSpace_Rift_CV1 = 4,
    /// Oculus Rift S uses a unique color space, see enum description for more info
    ovrpColorSpace_Rift_S = 5,
    /// Oculus Quest's native color space is slightly different than Rift CV1
    ovrpColorSpace_Quest = 6,
    /// Similar to DCI-P3. See notes above for more details on P3
    ovrpColorSpace_P3 = 7,
    /// Similar to sRGB but with deeper greens using D65 white point
    ovrpColorSpace_Adobe_RGB = 8,
    ovrpColorSpace_Count
} ovrpColorSpace;

//-----------------------------------------------------------------
// Event Management
//-----------------------------------------------------------------
// Enum defining the type of the underlying event, required first element in every event struct
typedef enum ovrpEventType_ {
  ovrpEventType_None = 0,
  /// Refresh rate changed event
  ovrpEventType_DisplayRefreshRateChange = 1,











} ovrpEventType;

// biggest event that OVRPlugin can use
typedef struct ovrpEventDataBuffer_ {
  ovrpEventType EventType;
  unsigned char EventData[4000];
} ovrpEventDataBuffer;

typedef struct ovrpEventDisplayRefreshRateChange_ {
  ovrpEventType EventType;
  float FromRefreshRate;
  float ToRefreshRate;
} ovrpEventDisplayRefreshRateChange;








































































//-----------------------------------------------------------------
// Insight Passthrough
//-----------------------------------------------------------------
typedef enum {
  ovrpInsightPassthroughColorMapType_None = 0,
  ovrpInsightPassthroughColorMapType_MonoToRgba = 1,
  ovrpInsightPassthroughColorMapType_MonoToMono = 2,
  ovrpInsightPassthroughColorMapType_HandsContrast = 3,
  ovrpInsightPassthroughColorMapType_EnumSize = 0x7fffffff
} ovrpInsightPassthroughColorMapType;

typedef enum {
  ovrpInsightPassthroughStyleFlags_HasTextureOpacityFactor = 1 << 0,
  ovrpInsightPassthroughStyleFlags_HasEdgeColor = 1 << 1,
  ovrpInsightPassthroughStyleFlags_HasTextureColorMap = 1 << 2,
  ovrpInsightPassthroughStyleFlags_EnumSize = 0x7fffffff
} ovrpInsightPassthroughStyleFlags;

typedef struct {
  /// The flags determine which fields of the struct have been initialize and
  /// should be read. The values of fields which are not indicated to be
  /// present by the flags should not be accessed. This is used to establish
  /// backward: When new fields are added to the struct, callers of an older
  /// version will only initialize the memory of previously known fields and
  //  indicate which ones those are in the flags.
  ovrpInsightPassthroughStyleFlags Flags;

  /// Opacity of the (main) passthrough texture.
  float TextureOpacityFactor;

  /// Color of the edge rendering effect. The effect is disabled if the alpha
  /// value is set to 0.
  ovrpColorf EdgeColor;

  /// The texture color map assigns a new color for each input (image) color.
  /// The contents of `TextureColorMapData` is determined by
  /// `TextureColorMapType`:
  /// - For `MonoToRgba`, it is an array of 256 MrColorf values, i.e. one
  ///   float color tuple for each 8 bit grayscale input value.
  /// - For `MonoToMono`, it is an array of 256 uint8 values, i.e. one
  ///   8 bit grayscale output value for each input value.
  /// - For `HandsContrast`, it is an array of 4 float values.
  ovrpInsightPassthroughColorMapType TextureColorMapType;
  unsigned int TextureColorMapDataSize;
  unsigned char* TextureColorMapData;
} ovrpInsightPassthroughStyle;




















































































































































#endif
