// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDPrivate.h"
#include "IHeadMountedDisplay.h"
#include "OculusFunctionLibrary.h"
#include "OculusHMD_VulkanExtensions.h"
#include "OculusPluginWrapper.h"

//-------------------------------------------------------------------------------------------------
// FOculusHMDModule
//-------------------------------------------------------------------------------------------------

PRAGMA_DISABLE_DEPRECATION_WARNINGS

class FOculusHMDModule : public IOculusHMDModule
{
public:
	FOculusHMDModule();

	static inline FOculusHMDModule& Get()
	{
		return FModuleManager::LoadModuleChecked< FOculusHMDModule >("OculusHMD");
	}

	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;


	// IHeadMountedDisplayModule
	virtual FString GetModuleKeyName() const override;
	virtual void GetModuleAliases(TArray<FString>& AliasesOut) const override;
	virtual bool PreInit() override;
	virtual bool IsHMDConnected() override;
	virtual uint64 GetGraphicsAdapterLuid() override;
	virtual FString GetAudioInputDevice() override;
	virtual FString GetAudioOutputDevice() override;
	virtual FString GetDeviceSystemName() override;
	virtual TSharedPtr< class IXRTrackingSystem, ESPMode::ThreadSafe > CreateTrackingSystem() override;
	virtual TSharedPtr< IHeadMountedDisplayVulkanExtensions, ESPMode::ThreadSafe > GetVulkanExtensions() override;
	virtual bool IsStandaloneStereoOnlyDevice() override;

	// IOculusHMDModule
	virtual void GetPose(FRotator& DeviceRotation, FVector& DevicePosition, FVector& NeckPosition, bool bUseOrienationForPlayerCamera = false, bool bUsePositionForPlayerCamera = false, const FVector PositionScale = FVector::ZeroVector) override
	{
		UDEPRECATED_UOculusFunctionLibrary::GetPose(DeviceRotation, DevicePosition, NeckPosition, bUseOrienationForPlayerCamera, bUsePositionForPlayerCamera, PositionScale);
	}

	virtual void GetRawSensorData(FVector& AngularAcceleration, FVector& LinearAcceleration, FVector& AngularVelocity, FVector& LinearVelocity, float& TimeInSeconds) override
	{
		UDEPRECATED_UOculusFunctionLibrary::GetRawSensorData(AngularAcceleration, LinearAcceleration, AngularVelocity, LinearVelocity, TimeInSeconds, ETrackedDeviceType::HMD);
	}

	virtual bool GetUserProfile(struct FHmdUserProfile& Profile) override
	{
		return UDEPRECATED_UOculusFunctionLibrary::GetUserProfile(Profile);
	}

	virtual void SetBaseRotationAndBaseOffsetInMeters(FRotator Rotation, FVector BaseOffsetInMeters, EOrientPositionSelector::Type Options) override
	{
		UDEPRECATED_UOculusFunctionLibrary::SetBaseRotationAndBaseOffsetInMeters(Rotation, BaseOffsetInMeters, Options);
	}

	virtual void GetBaseRotationAndBaseOffsetInMeters(FRotator& OutRotation, FVector& OutBaseOffsetInMeters) override
	{
		UDEPRECATED_UOculusFunctionLibrary::GetBaseRotationAndBaseOffsetInMeters(OutRotation, OutBaseOffsetInMeters);
	}

	virtual void SetBaseRotationAndPositionOffset(FRotator BaseRot, FVector PosOffset, EOrientPositionSelector::Type Options) override
	{
		UDEPRECATED_UOculusFunctionLibrary::SetBaseRotationAndPositionOffset(BaseRot, PosOffset, Options);
	}

	virtual void GetBaseRotationAndPositionOffset(FRotator& OutRot, FVector& OutPosOffset) override
	{
		UDEPRECATED_UOculusFunctionLibrary::GetBaseRotationAndPositionOffset(OutRot, OutPosOffset);
	}

	virtual class IStereoLayers* GetStereoLayers() override
	{
		return UDEPRECATED_UOculusFunctionLibrary::GetStereoLayers();
	}

	bool IsOVRPluginAvailable() const
	{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
		return OVRPluginHandle != nullptr;
#else
		return false;
#endif
	}

#if OCULUS_HMD_SUPPORTED_PLATFORMS
	OCULUSHMD_API static void* GetOVRPluginHandle();
	OCULUSHMD_API static inline OculusPluginWrapper& GetPluginWrapper() { return PluginWrapper; }
	virtual bool PoseToOrientationAndPosition(const FQuat& InOrientation, const FVector& InPosition, FQuat& OutOrientation, FVector& OutPosition) const override;

protected:
	void SetGraphicsAdapterLuid(uint64 InLuid);

	static OculusPluginWrapper PluginWrapper;

	bool bPreInit;
	bool bPreInitCalled;
	void *OVRPluginHandle;
	uint64 GraphicsAdapterLuid;
	TWeakPtr< IHeadMountedDisplay, ESPMode::ThreadSafe > HeadMountedDisplay;
	TSharedPtr< IHeadMountedDisplayVulkanExtensions, ESPMode::ThreadSafe > VulkanExtensions;

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS
