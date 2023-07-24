// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoloLensPlatformApplicationMisc.h"
#include "HoloLensApplication.h"
#include "HoloLensErrorOutputDevice.h"
#include "HAL/PlatformProcess.h"
#include "Misc/App.h"

FOutputDeviceError* FHoloLensPlatformApplicationMisc::GetErrorOutputDevice()
{
	static FHoloLensErrorOutputDevice Singleton;
	return &Singleton;
}

GenericApplication* FHoloLensPlatformApplicationMisc::CreateApplication()
{
	return FHoloLensApplication::CreateHoloLensApplication();
}

// Defined in HoloLensLaunch.cpp
extern void appWinPumpMessages();

void FHoloLensPlatformApplicationMisc::PumpMessages(bool bFromMainLoop)
{
	if (!bFromMainLoop)
	{
		return;
	}

	GPumpingMessagesOutsideOfMainLoop = false;

	appWinPumpMessages();

	bool HasFocus = true;
	// if its our window, allow sound, otherwise apply multiplier
	FApp::SetVolumeMultiplier(HasFocus ? 1.0f : FApp::GetUnfocusedVolumeMultiplier());
}

float FHoloLensPlatformApplicationMisc::GetDPIScaleFactorAtPoint(float X, float Y)
{
	float Dpi = static_cast<uint32_t>(Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->LogicalDpi);
	return Dpi / 96.0f;
}

void FHoloLensPlatformApplicationMisc::ClipboardCopy(const TCHAR* Str)
{
	using namespace Windows::ApplicationModel::DataTransfer;

	DataPackage ^Package = ref new DataPackage();
	Package->RequestedOperation = DataPackageOperation::Copy;
	Package->SetText(ref new Platform::String(Str));
	Clipboard::SetContent(Package);
}

void FHoloLensPlatformApplicationMisc::ClipboardPaste(class FString& Dest)
{
	using namespace Windows::Foundation;
	using namespace Windows::ApplicationModel::DataTransfer;

	DataPackageView^ ClipboardContent = Clipboard::GetContent();
	if (ClipboardContent != nullptr && ClipboardContent->Contains(StandardDataFormats::Text))
	{
		IAsyncOperation<Platform::String^>^ AsyncOp = ClipboardContent->GetTextAsync();
		while (AsyncOp->Status == AsyncStatus::Started)
		{
			PumpMessages(false);

			// Yield
			FPlatformProcess::Sleep(0.f);
		}

		if (AsyncOp->Status == AsyncStatus::Completed)
		{
			Dest = AsyncOp->GetResults()->Data();
		}
	}
}
