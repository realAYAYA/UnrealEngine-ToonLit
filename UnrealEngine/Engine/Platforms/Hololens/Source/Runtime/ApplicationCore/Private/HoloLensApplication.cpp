// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoloLensApplication.h"
#include "HoloLensWindow.h"
#include "HoloLensCursor.h"
#include "HoloLensInputInterface.h"
#include "HoloLensPlatformMisc.h"
#include "GenericPlatform/GenericApplication.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "IInputDeviceModule.h"
#include "IInputDevice.h"

DEFINE_LOG_CATEGORY(LogHoloLens);

static FHoloLensApplication* HoloLensApplication = NULL;
FVector2D FHoloLensApplication::DesktopSize;

#if PLATFORM_HOLOLENS
Windows::Graphics::Holographic::HolographicSpace^ FHoloLensApplication::HoloSpace;
#endif

bool FHoloLensApplication::buildForRetailWindowsStore = false;

FHoloLensApplication* FHoloLensApplication::CreateHoloLensApplication()
{
	GConfig->GetBool(TEXT("/Script/HoloLensPlatformEditor.HoloLensTargetSettings"), TEXT("bBuildForRetailWindowsStore"), buildForRetailWindowsStore, GEngineIni);
	
	check( HoloLensApplication == NULL );
	HoloLensApplication = new FHoloLensApplication();

	HoloLensApplication->InitLicensing();

	return HoloLensApplication;
}

FHoloLensApplication* FHoloLensApplication::GetHoloLensApplication()
{
	return HoloLensApplication;
}

FHoloLensApplication::FHoloLensApplication()
	: GenericApplication( MakeShareable( new FHoloLensCursor() ) )
	, InputInterface( FHoloLensInputInterface::Create( MessageHandler ) )
	// @MIXEDREALITY_CHANGE : BEGIN
	, bHasLoadedInputPlugins(false)
	// @MIXEDREALITY_CHANGE : END
	, ApplicationWindow(FHoloLensWindow::Make())
{
}

TSharedRef< FGenericWindow > FHoloLensApplication::MakeWindow()
{
	return ApplicationWindow;
}

void FHoloLensApplication::InitializeWindow(const TSharedRef< FGenericWindow >& InWindow, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately)
{
	const TSharedRef< FHoloLensWindow > Window = StaticCastSharedRef< FHoloLensWindow >(InWindow);

	Window->Initialize(this, InDefinition);
}

IInputInterface* FHoloLensApplication::GetInputInterface()
{
	// NOTE: This does not increase the reference count, so don't cache the result
	return InputInterface.Get();
}

void FHoloLensApplication::PollGameDeviceState( const float TimeDelta )
{
	// @MIXEDREALITY_CHANGE : BEGIN Mixed Reality spatial input support
	// Initialize any externally-implemented input devices (we delay load initialize the array so any plugins have had time to load)
	if (!bHasLoadedInputPlugins && GIsRunning)
	{
		TArray< IInputDeviceModule* > PluginImplementations = IModularFeatures::Get().GetModularFeatureImplementations< IInputDeviceModule >(IInputDeviceModule::GetModularFeatureName());
		for (auto InputPluginIt = PluginImplementations.CreateIterator(); InputPluginIt; ++InputPluginIt)
		{
			TSharedPtr< IInputDevice > Device = (*InputPluginIt)->CreateInputDevice(MessageHandler);
			AddExternalInputDevice(Device);
		}

		bHasLoadedInputPlugins = true;
	}

	//TODO:
	if (FApp::UseVRFocus() && !FApp::HasVRFocus())
	{
		return; // do not proceed if the app uses VR focus but doesn't have it
	}

	// Poll game device state and send new events
	InputInterface->Tick(TimeDelta);
	InputInterface->SendControllerEvents();

	// Poll externally-implemented devices
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->Tick(TimeDelta);
		(*DeviceIt)->SendControllerEvents();
	}
	// @MIXEDREALITY_CHANGE : END Mixed Reality spatial input support
}

FVector2D FHoloLensApplication::GetDesktopSize()
{
	return DesktopSize;
}

bool FHoloLensApplication::IsMouseAttached() const
{
	return (ref new Windows::Devices::Input::MouseCapabilities())->MousePresent > 0;
}

bool FHoloLensApplication::IsGamepadAttached() const
{
	//@MIXEDREALITY_CHANGE : BEGIN
	bool gamepadAttached = false;
	
	gamepadAttached = Windows::Gaming::Input::Gamepad::Gamepads->Size > 0;
	if (gamepadAttached)
	{
		return true;
	}

	for( auto DeviceIt = ExternalInputDevices.CreateConstIterator(); DeviceIt; ++DeviceIt )
	{
		if ((*DeviceIt)->IsGamepadAttached())
		{
			return true;
		}
	}
	
	return false;
	//@MIXEDREALITY_CHANGE : END
}

FPlatformRect FHoloLensApplication::GetWorkArea(const FPlatformRect& CurrentWindow) const
{
	// No good way of accounting for desktop task bar here (if present) since
	// it's not a Universal Windows construct.  Best we can do is return the
	// full desktop size.

	FPlatformRect WorkArea;
	WorkArea.Left = 0;
	WorkArea.Top = 0;
	WorkArea.Right = DesktopSize.X;
	WorkArea.Bottom = DesktopSize.Y;

	return WorkArea;
}

void FHoloLensApplication::GetInitialDisplayMetrics(FDisplayMetrics& OutDisplayMetrics) const
{
	FDisplayMetrics::RebuildDisplayMetrics(OutDisplayMetrics);

	// The initial virtual display rect is used to constrain maximum window size, and
	// so should report the full desktop resolution.  Later calls will report window
	// size in order to apply a tightly fitting hit test structure.
	OutDisplayMetrics.VirtualDisplayRect = OutDisplayMetrics.PrimaryDisplayWorkAreaRect;
}

void FHoloLensApplication::CacheDesktopSize()
{
#if WIN10_SDK_VERSION >= 14393
	if (Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent("Windows.Graphics.Display.DisplayInformation", "ScreenHeightInRawPixels"))
	{
		Windows::Graphics::Display::DisplayInformation^ DisplayInfo = Windows::Graphics::Display::DisplayInformation::GetForCurrentView();
		DesktopSize.X = DisplayInfo->ScreenWidthInRawPixels;
		DesktopSize.Y = DisplayInfo->ScreenHeightInRawPixels;
	}
	else
#endif
	{
		// Note this only works *before* the CoreWindow has been activated and received its first resize event.
		Windows::UI::ViewManagement::ApplicationView^ ViewManagementView = Windows::UI::ViewManagement::ApplicationView::GetForCurrentView();
		float Dpi = static_cast<uint32_t>(Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->LogicalDpi);
		FVector2D Size;
		Size.X = FHoloLensWindow::ConvertDipsToPixels(ViewManagementView->VisibleBounds.Width, Dpi);
		Size.Y = FHoloLensWindow::ConvertDipsToPixels(ViewManagementView->VisibleBounds.Height, Dpi);
		DesktopSize = Size;
	}
}

void FDisplayMetrics::RebuildDisplayMetrics(FDisplayMetrics& OutDisplayMetrics)
{
	FVector2D DesktopSize = FHoloLensApplication::GetDesktopSize();

	OutDisplayMetrics.PrimaryDisplayWidth = DesktopSize.X;
	OutDisplayMetrics.PrimaryDisplayHeight = DesktopSize.Y;

	// As with FHoloLensApplication::GetWorkArea we can't really measure the difference between
	// desktop size and work area on HoloLens.
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Left = 0;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Top = 0;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Right = DesktopSize.X;
	OutDisplayMetrics.PrimaryDisplayWorkAreaRect.Bottom = DesktopSize.Y;

	// We can't get a proper VirtualDisplayRect that's the equivalent of Windows Desktop.
	// We can, however, take advantage of the fact that one of the primary purposes of this
	// property is to define the coordinate space for mouse events.  Reporting the window
	// size aligns this with the coordinate space naturally used for HoloLens pointer events,
	// and allows us to correctly handle cases where a windowed app spans multiple screens.
	OutDisplayMetrics.VirtualDisplayRect = FHoloLensWindow::GetOSWindowBounds();

	// Translate so that the top left of the Window is the origin to exactly match
	// pointer event coordinate space.
	OutDisplayMetrics.VirtualDisplayRect.Bottom -= OutDisplayMetrics.VirtualDisplayRect.Top;
	OutDisplayMetrics.VirtualDisplayRect.Right -= OutDisplayMetrics.VirtualDisplayRect.Left;
	OutDisplayMetrics.VirtualDisplayRect.Left = 0;
	OutDisplayMetrics.VirtualDisplayRect.Top = 0;

	// Apply the debug safe zones
	OutDisplayMetrics.ApplyDefaultSafeZones();
}

TSharedRef< class FGenericApplicationMessageHandler > FHoloLensApplication::GetMessageHandler() const
{
	return MessageHandler;
}

void FHoloLensApplication::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
	InputInterface->SetMessageHandler( InMessageHandler );
	
	//@MIXEDREALITY_CHANGE : BEGIN
	TArray<IInputDeviceModule*> PluginImplementations = IModularFeatures::Get().GetModularFeatureImplementations<IInputDeviceModule>( IInputDeviceModule::GetModularFeatureName() );
	for( auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt )
	{
		(*DeviceIt)->SetMessageHandler(InMessageHandler);
	}
	//@MIXEDREALITY_CHANGE : END
}

void FHoloLensApplication::PumpMessages(const float TimeDelta)
{
	//TODO: message pump here?  Keep track of active dispatcher?
	FHoloLensMisc::PumpMessages(true);
}

TSharedRef< class FHoloLensCursor > FHoloLensApplication::GetCursor() const
{
	return StaticCastSharedPtr<FHoloLensCursor>( Cursor ).ToSharedRef();
}

bool FHoloLensApplication::ApplicationLicenseValid(FPlatformUserId PlatformUser)
{
	try
	{
		if (buildForRetailWindowsStore)
		{
			return Windows::ApplicationModel::Store::CurrentApp::LicenseInformation->IsActive;
		}
		else
		{
			return Windows::ApplicationModel::Store::CurrentAppSimulator::LicenseInformation->IsActive;
		}
	}
	catch (...)
	{
		UE_LOG(LogCore, Warning, TEXT("Error retrieving license information.  This typically indicates that the incorrect version of CurrentStoreApp is being used.  The currently selected version is Windows::ApplicationModel::Store::%s.  Check USING_RETAIL_WINDOWS_STORE."), USING_RETAIL_WINDOWS_STORE != 0 ? TEXT("CurrentApp") : TEXT("CurrentAppSimulator"));
		return false;
	}
}

void FHoloLensApplication::InitLicensing()
{
	try
	{
		if (buildForRetailWindowsStore)
		{
			Windows::ApplicationModel::Store::CurrentApp::LicenseInformation->LicenseChanged += ref new Windows::ApplicationModel::Store::LicenseChangedEventHandler(LicenseChangedHandler);
		}
		else
		{
			Windows::ApplicationModel::Store::CurrentAppSimulator::LicenseInformation->LicenseChanged += ref new Windows::ApplicationModel::Store::LicenseChangedEventHandler(LicenseChangedHandler);
		}
	}
	catch (...)
	{
		UE_LOG(LogCore, Warning, TEXT("Error registering for LicenseChanged event.  This typically indicates that the incorrect version of CurrentStoreApp is being used.  The currently selected version is Windows::ApplicationModel::Store::%s.  Check USING_RETAIL_WINDOWS_STORE."), USING_RETAIL_WINDOWS_STORE != 0 ? TEXT("CurrentApp") : TEXT("CurrentAppSimulator"));
#if UE_BUILD_SHIPPING
		// In shipping builds we treat this as fatal to reduce the chance of a build with misconfigured licensing being accidentally released.
		throw;
#endif
	}
}

void FHoloLensApplication::LicenseChangedHandler()
{
	Windows::ApplicationModel::Core::CoreApplication::MainView->Dispatcher->RunAsync(
		Windows::UI::Core::CoreDispatcherPriority::Normal,
		ref new Windows::UI::Core::DispatchedHandler(BroadcastUELicenseChangeEvent));
}

void FHoloLensApplication::BroadcastUELicenseChangeEvent()
{
	FCoreDelegates::ApplicationLicenseChange.Broadcast();
}

// @MIXEDREALITY_CHANGE : BEGIN Mixed Reality spatial input support
void FHoloLensApplication::AddExternalInputDevice(TSharedPtr<IInputDevice> InputDevice)
{
	if (InputDevice.IsValid())
	{
		ExternalInputDevices.Add(InputDevice);
	}
}
// @MIXEDREALITY_CHANGE : END Mixed Reality spatial input support


