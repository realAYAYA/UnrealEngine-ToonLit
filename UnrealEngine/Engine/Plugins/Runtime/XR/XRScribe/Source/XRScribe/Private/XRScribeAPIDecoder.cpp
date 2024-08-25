// Copyright Epic Games, Inc. All Rights Reserved.

#include "XRScribeAPIDecoder.h"

namespace UE::XRScribe
{

// helpers

template <typename XrType>
int32 ReadXrTypeList(const FOpenXRAPIPacketBase& Packet, int32 OffsetToListData, uint32 TypeCount, XrType* DstTypeList)
{
	const uint8* const PacketBaseAddress = reinterpret_cast<const uint8*>(&Packet);
	const uint8* const PropertiesBaseAddress = PacketBaseAddress + OffsetToListData;

	const int32 ListBytes = static_cast<int32>(TypeCount * sizeof(XrType));

	FMemory::Memcpy(static_cast<void*>(DstTypeList), PropertiesBaseAddress, ListBytes);

	return ListBytes;
}

FOpenXRCaptureDecoder::FOpenXRCaptureDecoder()
{
	for (ApiDecodeFn& Fn : DecodeFnTable)
	{
		Fn = nullptr;
	}

	DecodeFnTable[(uint32)EOpenXRAPIPacketId::EnumerateApiLayerProperties] = &FOpenXRCaptureDecoder::DecodeEnumerateApiLayerProperties;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::EnumerateInstanceExtensionProperties] = &FOpenXRCaptureDecoder::DecodeEnumerateInstanceExtensionProperties;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::CreateInstance] = &FOpenXRCaptureDecoder::DecodeCreateInstance;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::DestroyInstance] = &FOpenXRCaptureDecoder::DecodeDestroyInstance;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::GetInstanceProperties] = &FOpenXRCaptureDecoder::DecodeGetInstanceProperties;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::PollEvent] = nullptr; // TODO: pending encode support
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::ResultToString] = nullptr; // TODO: pending encode support
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::StructureTypeToString] = nullptr; // TODO: pending encode support
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::GetSystem] = &FOpenXRCaptureDecoder::DecodeGetSystem;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::GetSystemProperties] = &FOpenXRCaptureDecoder::DecodeGetSystemProperties;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::EnumerateEnvironmentBlendModes] = &FOpenXRCaptureDecoder::DecodeEnumerateEnvironmentBlendModes;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::CreateSession] = &FOpenXRCaptureDecoder::DecodeCreateSession;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::DestroySession] = &FOpenXRCaptureDecoder::DecodeDestroySession;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::EnumerateReferenceSpaces] = &FOpenXRCaptureDecoder::DecodeEnumerateReferenceSpaces;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::CreateReferenceSpace] = &FOpenXRCaptureDecoder::DecodeCreateReferenceSpace;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::GetReferenceSpaceBoundsRect] = &FOpenXRCaptureDecoder::DecodeGetReferenceSpaceBoundsRect;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::CreateActionSpace] = &FOpenXRCaptureDecoder::DecodeCreateActionSpace;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::LocateSpace] = &FOpenXRCaptureDecoder::DecodeLocateSpace;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::DestroySpace] = &FOpenXRCaptureDecoder::DecodeDestroySpace;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::EnumerateViewConfigurations] = &FOpenXRCaptureDecoder::DecodeEnumerateViewConfigurations;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::GetViewConfigurationProperties] = &FOpenXRCaptureDecoder::DecodeGetViewConfigurationProperties;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::EnumerateViewConfigurationViews] = &FOpenXRCaptureDecoder::DecodeEnumerateViewConfigurationViews;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::EnumerateSwapchainFormats] = &FOpenXRCaptureDecoder::DecodeEnumerateSwapchainFormats;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::CreateSwapchain] = &FOpenXRCaptureDecoder::DecodeCreateSwapchain;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::DestroySwapchain] = &FOpenXRCaptureDecoder::DecodeDestroySwapchain;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::EnumerateSwapchainImages] = &FOpenXRCaptureDecoder::DecodeEnumerateSwapchainImages;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::AcquireSwapchainImage] = &FOpenXRCaptureDecoder::DecodeAcquireSwapchainImage;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::WaitSwapchainImage] = &FOpenXRCaptureDecoder::DecodeWaitSwapchainImage;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::ReleaseSwapchainImage] = &FOpenXRCaptureDecoder::DecodeReleaseSwapchainImage;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::BeginSession] = &FOpenXRCaptureDecoder::DecodeBeginSession;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::EndSession] = &FOpenXRCaptureDecoder::DecodeEndSession;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::RequestExitSession] = &FOpenXRCaptureDecoder::DecodeRequestExitSession;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::WaitFrame] = &FOpenXRCaptureDecoder::DecodeWaitFrame;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::BeginFrame] = &FOpenXRCaptureDecoder::DecodeBeginFrame;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::EndFrame] = &FOpenXRCaptureDecoder::DecodeEndFrame;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::LocateViews] = &FOpenXRCaptureDecoder::DecodeLocateViews;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::StringToPath] = &FOpenXRCaptureDecoder::DecodeStringToPath;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::PathToString] = &FOpenXRCaptureDecoder::DecodePathToString;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::CreateActionSet] = &FOpenXRCaptureDecoder::DecodeCreateActionSet;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::DestroyActionSet] = &FOpenXRCaptureDecoder::DecodeDestroyActionSet;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::CreateAction] = &FOpenXRCaptureDecoder::DecodeCreateAction;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::DestroyAction] = &FOpenXRCaptureDecoder::DecodeDestroyAction;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::SuggestInteractionProfileBindings] = &FOpenXRCaptureDecoder::DecodeSuggestInteractionProfileBindings;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::AttachSessionActionSets] = &FOpenXRCaptureDecoder::DecodeAttachSessionActionSets;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::GetCurrentInteractionProfile] = &FOpenXRCaptureDecoder::DecodeGetCurrentInteractionProfile;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::GetActionStateBoolean] = &FOpenXRCaptureDecoder::DecodeGetActionStateBoolean;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::GetActionStateFloat] = &FOpenXRCaptureDecoder::DecodeGetActionStateFloat;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::GetActionStateVector2F] = &FOpenXRCaptureDecoder::DecodeGetActionStateVector2f;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::GetActionStatePose] = &FOpenXRCaptureDecoder::DecodeGetActionStatePose;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::SyncActions] = &FOpenXRCaptureDecoder::DecodeSyncActions;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::EnumerateBoundSourcesForAction] = nullptr; // TODO: pending encode support
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::GetInputSourceLocalizedName] = nullptr; // TODO: pending encode support
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::ApplyHapticFeedback] = &FOpenXRCaptureDecoder::DecodeApplyHapticFeedback;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::StopHapticFeedback] = &FOpenXRCaptureDecoder::DecodeStopHapticFeedback;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::InitializeLoaderKHR] = &FOpenXRCaptureDecoder::DecodeInitializeLoaderKHR;
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::GetVisibilityMaskKHR] = &FOpenXRCaptureDecoder::DecodeGetVisibilityMaskKHR;
#if defined(XR_USE_GRAPHICS_API_D3D11)
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::GetD3D11GraphicsRequirementsKHR] = &FOpenXRCaptureDecoder::DecodeGetD3D11GraphicsRequirementsKHR;
#endif
#if defined(XR_USE_GRAPHICS_API_D3D12)
	DecodeFnTable[(uint32)EOpenXRAPIPacketId::GetD3D12GraphicsRequirementsKHR] = &FOpenXRCaptureDecoder::DecodeGetD3D12GraphicsRequirementsKHR;
#endif
}

FOpenXRCaptureDecoder::~FOpenXRCaptureDecoder() {}

void FOpenXRCaptureDecoder::DecodeDataFromMemory()
{
	const int64 NumBytes = EncodedData.Num();
	int64 CurByteIndex = 0;

	while (CurByteIndex < NumBytes)
	{
		const FOpenXRAPIPacketBase* NextPacket = reinterpret_cast<const FOpenXRAPIPacketBase*>(&EncodedData[CurByteIndex]);

		check(NextPacket->Padding0 == FOpenXRAPIPacketBase::MagicPacketByte);
		check(NextPacket->ApiId >= EOpenXRAPIPacketId::EnumerateApiLayerProperties && NextPacket->ApiId < EOpenXRAPIPacketId::NumValidAPIPacketIds);
		check(DecodeFnTable[(uint32)NextPacket->ApiId] != nullptr);

		// We explicitly need `this` to make it clear which instance is being passed to function pointer
		(this->*DecodeFnTable[(uint32)NextPacket->ApiId])(*NextPacket); 

		CurByteIndex = EncodedData.Tell();
	}

	check(CurByteIndex == NumBytes);
}

///////////////
/// packet decoders

void FOpenXRCaptureDecoder::DecodeEnumerateApiLayerProperties(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::EnumerateApiLayerProperties);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXREnumerateApiLayerPropertiesPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	if (!Data.LayerProperties.IsEmpty())
	{
		ApiLayerProperties = MoveTemp(Data.LayerProperties);
	}
}

void FOpenXRCaptureDecoder::DecodeEnumerateInstanceExtensionProperties(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::EnumerateInstanceExtensionProperties);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXREnumerateInstanceExtensionPropertiesPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;
	
	check(Data.LayerName[0] == 0); // TODO: save off extension properties queried from layers

	if (!Data.ExtensionProperties.IsEmpty())
	{
		InstanceExtensionProperties = MoveTemp(Data.ExtensionProperties);
	}
}

void FOpenXRCaptureDecoder::DecodeCreateInstance(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::CreateInstance);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRCreateInstancePacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	ValidInstanceCreateFlags = Data.CreateFlags;
	RequestedLayerNames = MoveTemp(Data.EnabledLayerNames);
	RequestedExtensionNames = MoveTemp(Data.EnabledExtensionNames);
}

void FOpenXRCaptureDecoder::DecodeDestroyInstance(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::DestroyInstance);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRDestroyInstancePacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	// nothing for us to do with this action currently
}

void FOpenXRCaptureDecoder::DecodeGetInstanceProperties(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::GetInstanceProperties);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRGetInstancePropertiesPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	InstanceProperties = MoveTemp(Data.InstanceProperties);
}

void FOpenXRCaptureDecoder::DecodeGetSystem(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::GetSystem);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRGetSystemPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	SystemGetInfo = MoveTemp(Data.SystemGetInfo);
}

void FOpenXRCaptureDecoder::DecodeGetSystemProperties(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::GetSystemProperties);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRGetSystemPropertiesPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	SystemProperties = MoveTemp(Data.SystemProperties);
}

void FOpenXRCaptureDecoder::DecodeEnumerateEnvironmentBlendModes(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::EnumerateEnvironmentBlendModes);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXREnumerateEnvironmentBlendModesPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	if ((Data.ViewConfigurationType == XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO) && 
		!Data.EnvironmentBlendModes.IsEmpty())
	{
		EnvironmentBlendModes = MoveTemp(Data.EnvironmentBlendModes);
	}
	else
	{
		// TODO: Log error, unsupported view config type
	}
}

void FOpenXRCaptureDecoder::DecodeCreateSession(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::CreateSession);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRCreateSessionPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	SessionCreateInfo = Data.SessionCreateInfo;
}

void FOpenXRCaptureDecoder::DecodeDestroySession(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::DestroySession);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRDestroySessionPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	// Nothing to do
}

void FOpenXRCaptureDecoder::DecodeEnumerateReferenceSpaces(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::EnumerateReferenceSpaces);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXREnumerateReferenceSpacesPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	if (!Data.Spaces.IsEmpty())
	{
		ReferenceSpaceTypes = MoveTemp(Data.Spaces);
	}
}

void FOpenXRCaptureDecoder::DecodeCreateReferenceSpace(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::CreateReferenceSpace);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRCreateReferenceSpacePacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	ReferenceSpaceMap.Add(Data.Space, Data.ReferenceSpaceCreateInfo.referenceSpaceType);
	CreatedReferenceSpaces.Add(Data);
}

void FOpenXRCaptureDecoder::DecodeGetReferenceSpaceBoundsRect(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::GetReferenceSpaceBoundsRect);

	FOpenXRGetReferenceSpaceBoundsRectPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	if (Data.Result == XR_SUCCESS)
	{
		ReferenceSpaceBounds.Add(Data.ReferenceSpaceType, Data.Bounds);
		// TODO: Check for existing bounds associated with reference space?
	}
}

void FOpenXRCaptureDecoder::DecodeCreateActionSpace(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::CreateActionSpace);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRCreateActionSpacePacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	ActionSpaceMap.Add(Data.Space, Data.ActionSpaceCreateInfo.action);
	CreatedActionSpaces.Add(Data);
}

void FOpenXRCaptureDecoder::DecodeLocateSpace(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::LocateSpace);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRLocateSpacePacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	if (!SpaceLocations.Contains(Data.Space))
	{
		SpaceLocations.Add(Data.Space);
	}

	//FLocateSpaceRecord Record{};
	//Record.BaseSpace = Data.BaseSpace;
	//Record.Time = Data.Time;
	//Record.Location = Data.Location;

	SpaceLocations[Data.Space].Add(Data);
}

void FOpenXRCaptureDecoder::DecodeDestroySpace(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::DestroySpace);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRDestroySpacePacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	// nothing to do yet
}

void FOpenXRCaptureDecoder::DecodeEnumerateViewConfigurations(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::EnumerateViewConfigurations);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXREnumerateViewConfigurationsPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	if (!Data.ViewConfigurationTypes.IsEmpty())
	{
		ViewConfigurationTypes = MoveTemp(Data.ViewConfigurationTypes);
	}
}

void FOpenXRCaptureDecoder::DecodeGetViewConfigurationProperties(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::GetViewConfigurationProperties);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRGetViewConfigurationPropertiesPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	ViewConfigurationProperties.Add(Data.ViewConfigurationType, Data.ConfigurationProperties);
}

void FOpenXRCaptureDecoder::DecodeEnumerateViewConfigurationViews(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::EnumerateViewConfigurationViews);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXREnumerateViewConfigurationViewsPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	if (!Data.Views.IsEmpty())
	{
		ViewConfigurationViews.Add(Data.ViewConfigurationType, Data.Views);
	}
}

void FOpenXRCaptureDecoder::DecodeEnumerateSwapchainFormats(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::EnumerateSwapchainFormats);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXREnumerateSwapchainFormatsPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	if (!Data.Formats.IsEmpty())
	{
		SwapchainFormats = MoveTemp(Data.Formats);
	}
}

void FOpenXRCaptureDecoder::DecodeCreateSwapchain(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::CreateSwapchain);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRCreateSwapchainPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	// TODO: Actual swapchain creation information not needed...yet
}

void FOpenXRCaptureDecoder::DecodeDestroySwapchain(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::DestroySwapchain);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRDestroySwapchainPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	// nothing to do yet
}

void FOpenXRCaptureDecoder::DecodeEnumerateSwapchainImages(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::EnumerateSwapchainImages);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXREnumerateSwapchainImagesPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	// nothing to do yet
}

void FOpenXRCaptureDecoder::DecodeAcquireSwapchainImage(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::AcquireSwapchainImage);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRAcquireSwapchainImagePacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	// nothing to do yet
}

void FOpenXRCaptureDecoder::DecodeWaitSwapchainImage(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::WaitSwapchainImage);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRWaitSwapchainImagePacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	// nothing to do yet
}
void FOpenXRCaptureDecoder::DecodeReleaseSwapchainImage(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::ReleaseSwapchainImage);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRReleaseSwapchainImagePacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	// nothing to do yet
}
void FOpenXRCaptureDecoder::DecodeBeginSession(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::BeginSession);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRBeginSessionPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;
	// nothing to do yet
}

void FOpenXRCaptureDecoder::DecodeEndSession(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::EndSession);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXREndSessionPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;
	// nothing to do yet
}

void FOpenXRCaptureDecoder::DecodeRequestExitSession(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::RequestExitSession);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRRequestExitSessionPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;
	// nothing to do yet
}

void FOpenXRCaptureDecoder::DecodeWaitFrame(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::WaitFrame);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRWaitFramePacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	WaitFrames.Add(Data);
}

void FOpenXRCaptureDecoder::DecodeBeginFrame(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::BeginFrame);
	check(BasePacket.Result == XR_SUCCESS || BasePacket.Result == XR_FRAME_DISCARDED);

	FOpenXRBeginFramePacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;
	// nothing to do yet
}

void FOpenXRCaptureDecoder::DecodeEndFrame(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::EndFrame);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXREndFramePacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	// Not really sure what to do with this wealth of info here!
}

void FOpenXRCaptureDecoder::DecodeLocateViews(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::LocateViews);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRLocateViewsPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	if (!ViewLocations.Contains(Data.ViewLocateInfo.viewConfigurationType))
	{
		ViewLocations.Add(Data.ViewLocateInfo.viewConfigurationType);
	}

	if (Data.Views.Num() > 0)
	{
		ViewLocations[Data.ViewLocateInfo.viewConfigurationType].Add(Data);
	}
}

void FOpenXRCaptureDecoder::DecodeStringToPath(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::StringToPath);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRStringToPathPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	PathToStringMap.Add(Data.GeneratedPath, FName(ANSI_TO_TCHAR(Data.PathStringToWrite.GetData())));

	// TODO: Do we need a bi-directional map at any point?
}

void FOpenXRCaptureDecoder::DecodePathToString(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::PathToString);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRPathToStringPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;
	// nothing to do yet
}

void FOpenXRCaptureDecoder::DecodeCreateActionSet(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::CreateActionSet);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRCreateActionSetPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;
	// nothing to do yet
}

void FOpenXRCaptureDecoder::DecodeDestroyActionSet(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::DestroyActionSet);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRDestroyActionSetPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;
	// nothing to do yet
}

void FOpenXRCaptureDecoder::DecodeCreateAction(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::CreateAction);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRCreateActionPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	CreatedActions.Add(Data);
}

void FOpenXRCaptureDecoder::DecodeDestroyAction(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::DestroyAction);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRDestroyActionPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;
	// nothing to do yet
}

void FOpenXRCaptureDecoder::DecodeSuggestInteractionProfileBindings(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::SuggestInteractionProfileBindings);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRSuggestInteractionProfileBindingsPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	check(PathToStringMap.Contains(Data.InteractionProfile));
	StringToSuggestedBindingsMap.Add(PathToStringMap[Data.InteractionProfile], Data.SuggestedBindings);
}

void FOpenXRCaptureDecoder::DecodeAttachSessionActionSets(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::AttachSessionActionSets);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRAttachSessionActionSetsPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;
	// nothing to do yet
}

void FOpenXRCaptureDecoder::DecodeGetCurrentInteractionProfile(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::GetCurrentInteractionProfile);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRGetCurrentInteractionProfilePacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;
	// nothing to do yet
}

void FOpenXRCaptureDecoder::DecodeGetActionStateBoolean(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::GetActionStateBoolean);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRGetActionStateBooleanPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	if (!BooleanActionStates.Contains(Data.GetInfoBoolean.action))
	{
		BooleanActionStates.Add(Data.GetInfoBoolean.action);
	}
	BooleanActionStates[Data.GetInfoBoolean.action].Add(Data);
}

void FOpenXRCaptureDecoder::DecodeGetActionStateFloat(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::GetActionStateFloat);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRGetActionStateFloatPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	if (!FloatActionStates.Contains(Data.GetInfoFloat.action))
	{
		FloatActionStates.Add(Data.GetInfoFloat.action);
	}
	FloatActionStates[Data.GetInfoFloat.action].Add(Data);
}

void FOpenXRCaptureDecoder::DecodeGetActionStateVector2f(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::GetActionStateVector2F);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRGetActionStateVector2fPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	if (!VectorActionStates.Contains(Data.GetInfoVector2f.action))
	{
		VectorActionStates.Add(Data.GetInfoVector2f.action);
	}
	VectorActionStates[Data.GetInfoVector2f.action].Add(Data);
}

void FOpenXRCaptureDecoder::DecodeGetActionStatePose(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::GetActionStatePose);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRGetActionStatePosePacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	if (!PoseActionStates.Contains(Data.GetInfoPose.action))
	{
		PoseActionStates.Add(Data.GetInfoPose.action);
	}
	PoseActionStates[Data.GetInfoPose.action].Add(Data);
}

void FOpenXRCaptureDecoder::DecodeSyncActions(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::SyncActions);

	// We can succeed here with NOT_FOCUSED
	check((BasePacket.Result == XR_SUCCESS) || (BasePacket.Result == XR_SESSION_NOT_FOCUSED));

	FOpenXRSyncActionsPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;

	SyncActions.Add(Data);
}

//void FOpenXRCaptureDecoder::DecodeEnumerateBoundSourcesForAction(const FOpenXRAPIPacketBase& BasePacket)
//{
//
//}
//void FOpenXRCaptureDecoder::DecodeGetInputSourceLocalizedName(const FOpenXRAPIPacketBase& BasePacket)
//{
//
//}

void FOpenXRCaptureDecoder::DecodeApplyHapticFeedback(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::ApplyHapticFeedback);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRApplyHapticFeedbackPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;
}

void FOpenXRCaptureDecoder::DecodeStopHapticFeedback(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::StopHapticFeedback);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRStopHapticFeedbackPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;
}

void FOpenXRCaptureDecoder::DecodeInitializeLoaderKHR(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::InitializeLoaderKHR);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRInitializeLoaderKHRPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;
}

void FOpenXRCaptureDecoder::DecodeGetVisibilityMaskKHR(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::GetVisibilityMaskKHR);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRGetVisibilityMaskKHRPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;
}

#if defined(XR_USE_GRAPHICS_API_D3D11)
void FOpenXRCaptureDecoder::DecodeGetD3D11GraphicsRequirementsKHR(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::GetD3D11GraphicsRequirementsKHR);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRGetD3D11GraphicsRequirementsKHRPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;
}
#endif

#if defined(XR_USE_GRAPHICS_API_D3D12)
void FOpenXRCaptureDecoder::DecodeGetD3D12GraphicsRequirementsKHR(const FOpenXRAPIPacketBase& BasePacket)
{
	check(BasePacket.ApiId == EOpenXRAPIPacketId::GetD3D12GraphicsRequirementsKHR);
	check(BasePacket.Result == XR_SUCCESS);

	FOpenXRGetD3D12GraphicsRequirementsKHRPacket Data(XrResult::XR_ERROR_RUNTIME_FAILURE);

	EncodedData << Data;
}
#endif

}
