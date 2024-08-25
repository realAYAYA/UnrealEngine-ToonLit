// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualCameraBeaconReceiver.h"

#include "Common/UdpSocketBuilder.h"
#include "IPixelStreamingEditorModule.h"
#include "MessageEndpoint.h"
#include "MessageEndpointBuilder.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "PixelStreamingVCamLog.h"
#include "PixelStreamingVCamModule.h"
#include "Serialization/ArrayReader.h"
#include "Serialization/ArrayWriter.h"
#include "VCamCoreUserSettings.h"

namespace UE::PixelStreamingVCam::Private
{
	namespace VirtualCameraBeaconReceiverConstants
	{
	constexpr uint8 ProtocolVersion = 0;
	const TArray<uint8> ProtocolIdentifier = { 'U', 'E', 'V', 'C', 'a', 'm' };
	}

	FVirtualCameraBeaconReceiver::FVirtualCameraBeaconReceiver()
		: FDiscoveryBeaconReceiver(
			TEXT("VCAMBeaconResponder"),
			VirtualCameraBeaconReceiverConstants::ProtocolIdentifier,
			VirtualCameraBeaconReceiverConstants::ProtocolVersion
		)
	{
	}

	void FVirtualCameraBeaconReceiver::SetIsStreamingReady(bool bNewValue)
	{
		bIsStreamingReady = bNewValue;
	}

	void FVirtualCameraBeaconReceiver::Startup()
	{
		PixelStreamingPort = IPixelStreamingEditorModule::Get().GetViewerPort();

		FDiscoveryBeaconReceiver::Startup();
	}

	bool FVirtualCameraBeaconReceiver::GetDiscoveryAddress(FIPv4Address& OutAddress) const
	{
		const UVirtualCameraCoreUserSettings& Settings = *GetDefault<UVirtualCameraCoreUserSettings>();
		FIPv4Address DiscoveryAddress;
		if (!FIPv4Address::Parse(Settings.DiscoveryEndpoint, DiscoveryAddress))
		{
			UE_LOG(LogPixelStreamingVCam, Error, TEXT("Failed to parse VCAM discovery endpoint address \"%s\""), *Settings.DiscoveryEndpoint);
			return false;
		}

		OutAddress = DiscoveryAddress;
		return true;
	}

	int32 FVirtualCameraBeaconReceiver::GetDiscoveryPort() const
	{
		const UVirtualCameraCoreUserSettings& Settings = *GetDefault<UVirtualCameraCoreUserSettings>();
		return Settings.DiscoveryPort;
	}

	bool FVirtualCameraBeaconReceiver::MakeBeaconResponse(uint8 BeaconProtocolVersion, FArrayReader& InMessageData, FArrayWriter& OutResponseData) const
	{
		const FString FriendlyName = GetFriendlyName();

		{
			OutResponseData << (uint32&)PixelStreamingPort;
			OutResponseData << (uint8&)bIsStreamingReady;
			OutResponseData << (FString&)FriendlyName;
		}

		return true;
	}

	FString FVirtualCameraBeaconReceiver::GetFriendlyName() const
	{
		FString FriendlyName;

		if (FParse::Value(FCommandLine::Get(), TEXT("-StageFriendlyName="), FriendlyName))
		{
			return FriendlyName;
		}

		if (FParse::Value(FCommandLine::Get(), TEXT("-CONCERTDISPLAYNAME="), FriendlyName))
		{
			return FriendlyName;
		}

		return FApp::GetSessionOwner();
	}
}