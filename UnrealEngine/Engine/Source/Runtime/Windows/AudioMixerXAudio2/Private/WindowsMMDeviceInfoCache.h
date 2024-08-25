// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#if PLATFORM_WINDOWS

#include "AudioMixer.h"
#include "Misc/ScopeRWLock.h"
#include "ScopedCom.h"
#include <atomic>

#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"

#include "Windows/COMPointer.h"

THIRD_PARTY_INCLUDES_START
#include <mmreg.h>				// WAVEFORMATEX
#include <mmdeviceapi.h>		// IMMDeviceEnumerator
THIRD_PARTY_INCLUDES_END

#include "Windows/HideWindowsPlatformTypes.h"
#include "Windows/HideWindowsPlatformAtomics.h"

namespace Audio
{		
struct FWindowsMMDeviceCache : IAudioMixerDeviceChangedListener, IAudioPlatformDeviceInfoCache
{
	struct FCacheEntry
	{
		enum class EEndpointType { Unknown, Render, Capture };

		FName DeviceId;							// Key
		FString FriendlyName;
		FString DeviceFriendlyName;
		EAudioDeviceState State;
		int32 NumChannels = 0;
		int32 SampleRate = 0;
		EEndpointType Type = EEndpointType::Unknown;
		uint32 ChannelBitmask = 0;				// Bitfield used to build output channels, for easy comparison.

		TArray<EAudioMixerChannel::Type> OutputChannels;	// TODO. Generate this from the ChannelNum and bitmask when we are asked for it.
		mutable FRWLock MutationLock;

		FCacheEntry& operator=(const FCacheEntry& InOther);

		FCacheEntry& operator=(FCacheEntry&& InOther);

		FCacheEntry(const FCacheEntry& InOther);

		FCacheEntry(FCacheEntry&& InOther);

		FCacheEntry(const FString& InDeviceId);
	};

	TComPtr<IMMDeviceEnumerator> DeviceEnumerator;

	mutable FRWLock CacheMutationLock;							// R/W lock protects map and default arrays.
	TMap<FName, FCacheEntry> Cache;								// DeviceID GUID -> Info.
	FName DefaultCaptureId[(int32)EAudioDeviceRole::COUNT];		// Role -> DeviceID GUID
	FName DefaultRenderId[(int32)EAudioDeviceRole::COUNT];		// Role -> DeviceID GUID

	FWindowsMMDeviceCache();
	virtual ~FWindowsMMDeviceCache() = default;

	bool EnumerateChannelMask(uint32 InMask, FCacheEntry& OutInfo);

	bool EnumerateChannelFormat(const WAVEFORMATEX* InFormat, FCacheEntry& OutInfo);

	FCacheEntry::EEndpointType QueryDeviceDataFlow(const TComPtr<IMMDevice>& InDevice) const;

	bool EnumerateDeviceProps(const TComPtr<IMMDevice>& InDevice, FCacheEntry& OutInfo);

	void EnumerateEndpoints();

	void EnumerateDefaults();

	void OnDefaultCaptureDeviceChanged(const EAudioDeviceRole InAudioDeviceRole, const FString& DeviceId) override;

	void OnDefaultRenderDeviceChanged(const EAudioDeviceRole InAudioDeviceRole, const FString& DeviceId) override;

	void OnDeviceAdded(const FString& DeviceId, bool bIsRender) override;

	void OnDeviceRemoved(const FString& DeviceId, bool) override;

	TOptional<FCacheEntry> BuildCacheEntry(const FString& DeviceId);

	FString GetFriendlyName(FName InDeviceId) const;

	void OnDeviceStateChanged(const FString& DeviceId, const EAudioDeviceState InState, bool) override;

	void OnFormatChanged(const FString& InDeviceId, const FFormatChangedData& InFormat) override;

	void MakeDeviceInfo(const FCacheEntry& InEntry, FName InDefaultDevice, FAudioPlatformDeviceInfo& OutInfo) const;

	virtual TArray<FAudioPlatformDeviceInfo> GetAllActiveOutputDevices() const override;

	FName GetDefaultOutputDevice_NoLock() const;

	TOptional<FAudioPlatformDeviceInfo> FindDefaultOutputDevice() const override;

	TOptional<FAudioPlatformDeviceInfo> FindActiveOutputDevice(FName InDeviceID) const override;
};

}// namespace Audio

#endif //PLATFORM_WINDOWS
