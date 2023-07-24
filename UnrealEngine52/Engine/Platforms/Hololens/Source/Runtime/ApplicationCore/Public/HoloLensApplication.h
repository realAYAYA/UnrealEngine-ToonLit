// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericApplication.h"

DECLARE_LOG_CATEGORY_EXTERN(LogHoloLens, Log, All);

class FHoloLensApplication : public GenericApplication
{
public:

	static FHoloLensApplication* CreateHoloLensApplication();

	static FHoloLensApplication* GetHoloLensApplication();

	static void CacheDesktopSize();

	static FVector2D GetDesktopSize();

public:	

	virtual ~FHoloLensApplication() {}

	virtual void PollGameDeviceState( const float TimeDelta ) override;

	virtual bool IsMouseAttached() const override;

	virtual bool IsGamepadAttached() const override;

	virtual FPlatformRect GetWorkArea( const FPlatformRect& CurrentWindow ) const override;
	virtual TSharedRef< FGenericWindow > MakeWindow();
	virtual void InitializeWindow(const TSharedRef< FGenericWindow >& Window, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately);

	virtual IInputInterface* GetInputInterface() override;

	TSharedRef< class FGenericApplicationMessageHandler > GetMessageHandler() const;

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler ) override;
	virtual void PumpMessages(const float TimeDelta) override;

	virtual void GetInitialDisplayMetrics(FDisplayMetrics& OutDisplayMetrics) const override;

	TSharedRef< class FHoloLensCursor > GetCursor() const;
	TSharedPtr< class FHoloLensInputInterface > GetHoloLensInputInterface() const { return InputInterface; }

	TSharedPtr< class FHoloLensWindow > GetHoloLensWindow() const { return ApplicationWindow; }

	virtual bool ApplicationLicenseValid(FPlatformUserId PlatformUser = PLATFORMUSERID_NONE);

	// @MIXEDREALITY_CHANGE : BEGIN
	virtual void AddExternalInputDevice(TSharedPtr< class IInputDevice > InputDevice);
	// @MIXEDREALITY_CHANGE : END
	
#if PLATFORM_HOLOLENS
public:
	static void SetHolographicSpace(Windows::Graphics::Holographic::HolographicSpace^ holoSpace) { HoloSpace = holoSpace; }
	static Windows::Graphics::Holographic::HolographicSpace^ GetHolographicSpace() { return HoloSpace; }
private:
	static Windows::Graphics::Holographic::HolographicSpace^ HoloSpace;
#endif

private:

	FHoloLensApplication();

	static void InitLicensing();
	static void LicenseChangedHandler();
	static void BroadcastUELicenseChangeEvent();
private:

	// @MIXEDREALITY_CHANGE : BEGIN
	/** List of input devices implemented in external modules. **/
	TArray< TSharedPtr< class IInputDevice > > ExternalInputDevices;
	bool bHasLoadedInputPlugins;
	// @MIXEDREALITY_CHANGE : END

	TSharedPtr< class FHoloLensInputInterface > InputInterface;

	TSharedRef< class FHoloLensWindow > ApplicationWindow;

	static FVector2D DesktopSize;
	
	static bool buildForRetailWindowsStore;
};