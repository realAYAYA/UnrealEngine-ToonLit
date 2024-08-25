// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "OpenXRPlatformRHI.h"
#include "Containers/Map.h"
#include "UObject/NameTypes.h"
#include "Serialization/ArrayReader.h"
#include "XRScribeFileFormat.h"

namespace UE::XRScribe
{

class FOpenXRCaptureDecoder
{
public:
	explicit FOpenXRCaptureDecoder();
	~FOpenXRCaptureDecoder();

	void DecodeDataFromMemory();

	// state accessors
	[[nodiscard]] const TArray<XrExtensionProperties>& GetInstanceExtensionProperties() { return InstanceExtensionProperties; }
	[[nodiscard]] const TArray<XrApiLayerProperties>& GetApiLayerProperties() {	return ApiLayerProperties; }
	[[nodiscard]] XrInstanceCreateFlags GetInstanceCreateFlags() { return ValidInstanceCreateFlags; }
	[[nodiscard]] const TArray<TStaticArray<ANSICHAR, XR_MAX_API_LAYER_NAME_SIZE>>& GetRequestedApiLayerNames() { return RequestedLayerNames; }
	[[nodiscard]] const TArray<TStaticArray<ANSICHAR, XR_MAX_EXTENSION_NAME_SIZE>>& GetRequestedExtensionNames() { return RequestedExtensionNames; }
	[[nodiscard]] const XrInstanceProperties& GetInstanceProperties() { return InstanceProperties; }
	[[nodiscard]] const XrSystemGetInfo& GetSystemInfo() { return SystemGetInfo; }
	[[nodiscard]] const XrSystemProperties& GetSystemProperties() { return SystemProperties; }
	[[nodiscard]] const TArray<XrEnvironmentBlendMode>& GetEnvironmentBlendModes() { return EnvironmentBlendModes; }
	[[nodiscard]] const TArray<XrViewConfigurationType>& GetViewConfigurationTypes() { return ViewConfigurationTypes; }
	[[nodiscard]] const TMap<XrViewConfigurationType, XrViewConfigurationProperties>& GetViewConfigurationProperties() { return ViewConfigurationProperties; }
	[[nodiscard]] const TMap<XrViewConfigurationType, TArray<XrViewConfigurationView>>& GetViewConfigurationViews() { return ViewConfigurationViews; }
	[[nodiscard]] const TMap<XrViewConfigurationType, TArray<FOpenXRLocateViewsPacket>>& GetViewLocations() { return ViewLocations; }
	[[nodiscard]] const TArray<FOpenXRCreateReferenceSpacePacket>& GetCreatedReferenceSpaces() { return CreatedReferenceSpaces; }
	[[nodiscard]] const TArray<FOpenXRCreateActionSpacePacket>& GetCreatedActionSpaces() { return CreatedActionSpaces; }
	[[nodiscard]] const TMap<XrSpace, TArray<FOpenXRLocateSpacePacket>>& GetSpaceLocations() { return SpaceLocations; }
	[[nodiscard]] const TArray<XrReferenceSpaceType>& GetReferenceSpaceTypes() { return ReferenceSpaceTypes; }
	[[nodiscard]] const TMap<XrReferenceSpaceType, XrExtent2Df>& GetReferenceSpaceBounds() { return ReferenceSpaceBounds; }
	[[nodiscard]] const TArray<int64>& GetSwapchainFormats() { return SwapchainFormats; }
	[[nodiscard]] const TArray<FOpenXRCreateActionPacket>& GetCreatedActions() { return CreatedActions; }
	[[nodiscard]] const TArray<FOpenXRWaitFramePacket>& GetWaitFrames() { return WaitFrames; }
	[[nodiscard]] const TArray<FOpenXRSyncActionsPacket>& GetSyncActions() { return SyncActions; }
	[[nodiscard]] const TMap<XrAction, TArray<FOpenXRGetActionStateBooleanPacket>>& GetBooleanActionStates() { return BooleanActionStates; }
	[[nodiscard]] const TMap<XrAction, TArray<FOpenXRGetActionStateFloatPacket>>& GetFloatActionStates() { return FloatActionStates; }
	[[nodiscard]] const TMap<XrAction, TArray<FOpenXRGetActionStateVector2fPacket>>& GetVectorActionStates() { return VectorActionStates; }
	[[nodiscard]] const TMap<XrAction, TArray<FOpenXRGetActionStatePosePacket>>& GetPoseActionStates() { return PoseActionStates; }
	[[nodiscard]] const TMap<XrPath, FName>& GetPathToStringMap() { return PathToStringMap; }

	TArray<uint8>& GetEncodedData()
	{
		return EncodedData;
	}

protected:

	// packet decoders
	void DecodeEnumerateApiLayerProperties(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeEnumerateInstanceExtensionProperties(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeCreateInstance(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeDestroyInstance(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeGetInstanceProperties(const FOpenXRAPIPacketBase& BasePacket);
	//void DecodePollEvent(const FOpenXRAPIPacketBase& BasePacket);
	//void DecodeResultToString(const FOpenXRAPIPacketBase& BasePacket);
	//void DecodeStructureTypeToString(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeGetSystem(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeGetSystemProperties(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeEnumerateEnvironmentBlendModes(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeCreateSession(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeDestroySession(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeEnumerateReferenceSpaces(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeCreateReferenceSpace(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeGetReferenceSpaceBoundsRect(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeCreateActionSpace(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeLocateSpace(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeDestroySpace(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeEnumerateViewConfigurations(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeGetViewConfigurationProperties(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeEnumerateViewConfigurationViews(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeEnumerateSwapchainFormats(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeCreateSwapchain(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeDestroySwapchain(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeEnumerateSwapchainImages(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeAcquireSwapchainImage(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeWaitSwapchainImage(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeReleaseSwapchainImage(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeBeginSession(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeEndSession(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeRequestExitSession(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeWaitFrame(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeBeginFrame(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeEndFrame(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeLocateViews(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeStringToPath(const FOpenXRAPIPacketBase& BasePacket);
	void DecodePathToString(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeCreateActionSet(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeDestroyActionSet(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeCreateAction(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeDestroyAction(const FOpenXRAPIPacketBase& BasePacket);

	void DecodeSuggestInteractionProfileBindings(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeAttachSessionActionSets(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeGetCurrentInteractionProfile(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeGetActionStateBoolean(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeGetActionStateFloat(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeGetActionStateVector2f(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeGetActionStatePose(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeSyncActions(const FOpenXRAPIPacketBase& BasePacket);
	//void DecodeEnumerateBoundSourcesForAction(const FOpenXRAPIPacketBase& BasePacket);
	//void DecodeGetInputSourceLocalizedName(const FOpenXRAPIPacketBase& BasePacket);

	void DecodeApplyHapticFeedback(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeStopHapticFeedback(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeInitializeLoaderKHR(const FOpenXRAPIPacketBase& BasePacket);
	void DecodeGetVisibilityMaskKHR(const FOpenXRAPIPacketBase& BasePacket);
#if defined(XR_USE_GRAPHICS_API_D3D11)
	void DecodeGetD3D11GraphicsRequirementsKHR(const FOpenXRAPIPacketBase& BasePacket);
#endif
#if defined(XR_USE_GRAPHICS_API_D3D12)
	void DecodeGetD3D12GraphicsRequirementsKHR(const FOpenXRAPIPacketBase& BasePacket);
#endif

	FArrayReader EncodedData;

	typedef void(FOpenXRCaptureDecoder::* ApiDecodeFn)(const FOpenXRAPIPacketBase& BasePacket);
	TStaticArray<ApiDecodeFn, (uint32)EOpenXRAPIPacketId::NumValidAPIPacketIds> DecodeFnTable;

	// derived state from capture

	TArray<XrExtensionProperties> InstanceExtensionProperties;
	TArray<XrApiLayerProperties> ApiLayerProperties;
	// TODO: Per-layer extension properties

	XrInstanceCreateFlags ValidInstanceCreateFlags = 0;
	TArray<TStaticArray<ANSICHAR, XR_MAX_API_LAYER_NAME_SIZE>> RequestedLayerNames;
	TArray<TStaticArray<ANSICHAR, XR_MAX_EXTENSION_NAME_SIZE>> RequestedExtensionNames;

	XrInstanceProperties InstanceProperties = {};

	XrSystemGetInfo SystemGetInfo = {};
	XrSystemProperties SystemProperties = {};
	TArray<XrEnvironmentBlendMode> EnvironmentBlendModes;

	XrSessionCreateInfo SessionCreateInfo = {};

	TArray<XrReferenceSpaceType> ReferenceSpaceTypes;
	TMap<XrReferenceSpaceType, XrExtent2Df> ReferenceSpaceBounds;
	TMap<XrSpace, XrReferenceSpaceType> ReferenceSpaceMap;
	TArray<FOpenXRCreateReferenceSpacePacket> CreatedReferenceSpaces;

	TArray<FOpenXRCreateActionSpacePacket> CreatedActionSpaces;
	TMap<XrSpace, XrAction> ActionSpaceMap;

	TMap<XrSpace, TArray<FOpenXRLocateSpacePacket>> SpaceLocations;

	TArray<XrViewConfigurationType> ViewConfigurationTypes;
	TMap<XrViewConfigurationType, XrViewConfigurationProperties> ViewConfigurationProperties;
	TMap<XrViewConfigurationType, TArray<XrViewConfigurationView>> ViewConfigurationViews;

	TArray<int64> SwapchainFormats;

	TMap<XrViewConfigurationType, TArray<FOpenXRLocateViewsPacket>> ViewLocations;

	TMap<XrPath, FName> PathToStringMap;
	TMap<FName, TArray<XrActionSuggestedBinding>> StringToSuggestedBindingsMap;

	TArray<FOpenXRCreateActionPacket> CreatedActions;

	TArray<FOpenXRWaitFramePacket> WaitFrames;

	TArray<FOpenXRSyncActionsPacket> SyncActions;

	TMap<XrAction, TArray<FOpenXRGetActionStateBooleanPacket>> BooleanActionStates;
	TMap<XrAction, TArray<FOpenXRGetActionStateFloatPacket>> FloatActionStates;
	TMap<XrAction, TArray<FOpenXRGetActionStateVector2fPacket>> VectorActionStates;
	TMap<XrAction, TArray<FOpenXRGetActionStatePosePacket>> PoseActionStates;

	// TODO: Would I ever want to bin properties into per-instance collections?
	// When we are repaying, we're just going to create our own set of 'valid' parameters
	// that our replay runtime supports, do we really care about how the original application run
	// managed their instances?
};

}
