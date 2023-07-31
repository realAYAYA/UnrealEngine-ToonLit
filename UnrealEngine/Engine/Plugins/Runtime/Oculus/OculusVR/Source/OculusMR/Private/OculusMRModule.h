// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "IOculusMRModule.h"
#include "Engine/EngineBaseTypes.h"

#define LOCTEXT_NAMESPACE "OculusMR"

enum class EOculusMR_CompositionMethod : uint8;
enum class EOculusMR_CameraDeviceEnum : uint8;
enum class EOculusMR_DepthQuality : uint8;

class UDEPRECATED_UOculusMR_Settings;
class AOculusMR_CastingCameraActor;
class UOculusMR_State;

//-------------------------------------------------------------------------------------------------
// FOculusInputModule
//-------------------------------------------------------------------------------------------------

PRAGMA_DISABLE_DEPRECATION_WARNINGS

class FOculusMRModule : public IOculusMRModule
{
public:
	FOculusMRModule();
	~FOculusMRModule();

	static inline FOculusMRModule& Get()
	{
		return FModuleManager::GetModuleChecked< FOculusMRModule >("OculusMR");
	}

	// IOculusMRModule
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	bool IsInitialized() { return bInitialized; }

	bool IsActive();
	UDEPRECATED_UOculusMR_Settings* GetMRSettings();
	UOculusMR_State* GetMRState();

private:
	bool bInitialized;
	UDEPRECATED_UOculusMR_Settings* MRSettings_DEPRECATED;
	UOculusMR_State* MRState;
	AOculusMR_CastingCameraActor* MRActor;
	UWorld* CurrentWorld;

	FDelegateHandle WorldAddedEventBinding;
	FDelegateHandle WorldDestroyedEventBinding;
	FDelegateHandle WorldLoadEventBinding;

	void InitMixedRealityCapture();

	/** Initialize the tracked physical camera */
	void SetupExternalCamera();
	/** Close the tracked physical camera */
	void CloseExternalCamera();
	/** Set up the needed settings and actors for MRC in-game */
	void SetupInGameCapture();
	/** Destroy actors for MRC in-game */
	void CloseInGameCapture();
	/** Reset all the MRC settings and state to the config and default */
	void ResetSettingsAndState();

	/** Handle changes on specific settings */
	void OnCompositionMethodChanged(EOculusMR_CompositionMethod OldVal, EOculusMR_CompositionMethod NewVal);
	void OnCapturingCameraChanged(EOculusMR_CameraDeviceEnum OldVal, EOculusMR_CameraDeviceEnum NewVal);
	void OnIsCastingChanged(bool OldVal, bool NewVal);
	void OnUseDynamicLightingChanged(bool OldVal, bool NewVal);
	void OnDepthQualityChanged(EOculusMR_DepthQuality OldVal, EOculusMR_DepthQuality NewVal);
	void OnTrackedCameraIndexChanged(int OldVal, int NewVal);

	void OnWorldCreated(UWorld* NewWorld);
	void OnWorldDestroyed(UWorld* NewWorld);

#if PLATFORM_ANDROID
	bool bActivated;

	FDelegateHandle InitialWorldAddedEventBinding;
	FDelegateHandle InitialWorldLoadEventBinding;
	FDelegateHandle PreWorldTickEventBinding;

	void ChangeCaptureState();
	void OnWorldTick(UWorld* World, ELevelTick Tick, float Delta);
	void OnInitialWorldCreated(UWorld* NewWorld);
#endif

#if WITH_EDITOR
	FDelegateHandle PieBeginEventBinding;
	FDelegateHandle PieStartedEventBinding;
	FDelegateHandle PieEndedEventBinding;

	void OnPieBegin(bool bIsSimulating);
	void OnPieStarted(bool bIsSimulating);
	void OnPieEnded(bool bIsSimulating);
#endif
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

#undef LOCTEXT_NAMESPACE
