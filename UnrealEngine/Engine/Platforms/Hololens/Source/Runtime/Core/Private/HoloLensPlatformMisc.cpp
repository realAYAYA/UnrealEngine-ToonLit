// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HoloLensMisc.cpp: HoloLens implementations of misc functions
=============================================================================*/

#include "HoloLensPlatformMisc.h"
#include "Internationalization/Internationalization.h"
#include "Misc/MessageDialog.h"
#include "Misc/App.h"
#include "HAL/ExceptionHandling.h"
#include "Misc/SecureHash.h"
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#include <time.h>
#include <agile.h>
#include "Microsoft/HideMicrosoftPlatformTypes.h"
#include "Modules/ModuleManager.h"

/** 
 * Whether support for integrating into the firewall is there
 */
#define WITH_FIREWALL_SUPPORT	0

/** Original C- Runtime pure virtual call handler that is being called in the (highly likely) case of a double fault */
_purecall_handler DefaultPureCallHandler;

/**
 * Our own pure virtual function call handler, set by appPlatformPreInit. Falls back
 * to using the default C- Runtime handler in case of double faulting.
 */ 
static void PureCallHandler()
{
	static bool bHasAlreadyBeenCalled = false;
	UE_DEBUG_BREAK();
	if (bHasAlreadyBeenCalled)
	{
		// Call system handler if we're double faulting.
		if (DefaultPureCallHandler)
		{
			DefaultPureCallHandler();
		}
	}
	else
	{
		bHasAlreadyBeenCalled = true;
		if (GIsRunning)
		{
			FMessageDialog::Open(EAppMsgType::Ok, NSLOCTEXT("Core", "PureVirtualFunctionCalledWhileRunningApp", "Pure virtual function being called while application was running (GIsRunning == 1)."));
		}
		UE_LOG(LogTemp, Fatal, TEXT("Pure virtual function being called"));
	}
}

/*-----------------------------------------------------------------------------
	SHA-1 functions.
-----------------------------------------------------------------------------*/

/**
 * Get the hash values out of the executable hash section
 *
 * NOTE: hash keys are stored in the executable, you will need to put a line like the
 *		 following into your PCLaunch.rc settings:
 *			ID_HASHFILE RCDATA "../../../../GameName/Build/Hashes.sha"
 *
 *		 Then, use the -sha option to the cooker (must be from commandline, not
 *       frontend) to generate the hashes for .ini, loc, startup packages, and .usf shader files
 *
 *		 You probably will want to make and checkin an empty file called Hashses.sha
 *		 into your source control to avoid linker warnings. Then for testing or final
 *		 final build ONLY, use the -sha command and relink your executable to put the
 *		 hashes for the current files into the executable.
 */
static void InitSHAHashes()
{
	//@todo.HoloLens: Implement this using GetModuleSection?
}

void FHoloLensMisc::PlatformPreInit()
{
	FGenericPlatformMisc::PlatformPreInit();

	// Use our own handler for pure virtuals being called.
	DefaultPureCallHandler = _set_purecall_handler(PureCallHandler);

	// initialize the file SHA hash mapping
	InitSHAHashes();

	PumpMessages(true);
}

void FHoloLensMisc::PlatformInit()
{
	// Identity.
	UE_LOG(LogInit, Log, TEXT("Computer: %s"), FPlatformProcess::ComputerName());
	UE_LOG(LogInit, Log, TEXT("User: %s"), FPlatformProcess::UserName());

	// Timer resolution.
	UE_LOG(LogInit, Log, TEXT("High frequency timer resolution =%f MHz"), 0.000001 / FPlatformTime::GetSecondsPerCycle());

	PumpMessages(true);
}

void FHoloLensMisc::PlatformPostInit(bool ShowSplashScreen)
{
	PumpMessages(true);
}

bool FHoloLensMisc::CoInitialize()
{
	HRESULT hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	return hr == S_OK || hr == S_FALSE;
}

void FHoloLensMisc::CoUninitialize()
{
	::CoUninitialize();
}

FString FHoloLensMisc::GetEnvironmentVariable(const TCHAR* VariableName)
{
	//@todo.HoloLens:
	return FString();
}

const TCHAR* FHoloLensMisc::GetPlatformFeaturesModuleName()
{
	// Expectation is that if we hand back a non-null name then it's guaranteed to load.
	// Since the platform features supported on HoloLens are strictly optional and enabled via
	// plug-in we'll do a quick pre-check here.
	static const TCHAR* PlatformFeaturesModuleName = TEXT("HoloLensPlatformFeatures");
	IModuleInterface* CustomModule = FModuleManager::LoadModulePtr<IModuleInterface>(PlatformFeaturesModuleName);
	return CustomModule ? PlatformFeaturesModuleName : nullptr;
}

// Defined in HoloLensLaunch.cpp
extern void appWinPumpMessages();

void FHoloLensMisc::PumpMessages(bool bFromMainLoop)
{
	if (!bFromMainLoop)
	{
		// Process pending windows messages, which is necessary to the rendering thread in some rare cases where DX10
		// sends window messages (from IDXGISwapChain::Present) to the main thread owned viewport window.
		// Only process sent messages to minimize the opportunity for re-entry in the editor, since wx messages are not deferred.
		return;
	}

	// Handle all incoming messages if we're not using wxWindows in which case this is done by their
	// message loop.
	appWinPumpMessages();

	// check to see if the window in the foreground was made by this process (ie, does this app
	// have focus)
	//@todo.HoloLens: Will this always be true?
	bool HasFocus = true;
	// if its our window, allow sound, otherwise apply multiplier
	FApp::SetVolumeMultiplier(HasFocus ? 1.0f : FApp::GetUnfocusedVolumeMultiplier());
}

void FHoloLensMisc::LowLevelOutputDebugString(const TCHAR *Message)
{
	OutputDebugString(Message);
}

void FHoloLensMisc::RequestExit(bool Force)
{
	UE_LOG(LogTemp, Log, TEXT("FHoloLensMisc::RequestExit(%i)"), Force);
	if (Force)
	{
		// Force immediate exit.
		// Dangerous because config code isn't flushed, global destructors aren't called, etc.
		//::TerminateProcess(GetCurrentProcess(), 0); 
		RequestEngineExit(TEXT("FHoloLensMisc::RequestExit"));
	}
	else
	{
		RequestEngineExit(TEXT("FHoloLensMisc::RequestExit"));
	}
}

const TCHAR* FHoloLensMisc::GetSystemErrorMessage(TCHAR* OutBuffer, int32 BufferCount, int32 Error)
{
	check(OutBuffer && BufferCount);
	*OutBuffer = TEXT('\0');
	if (Error == 0)
	{
		Error = GetLastError();
	}
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, Error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), OutBuffer, BufferCount, NULL);
	TCHAR* Found = FCString::Strchr(OutBuffer,TEXT('\r'));
	if (Found)
	{
		*Found = TEXT('\0');
	}
	Found = FCString::Strchr(OutBuffer,TEXT('\n'));
	if (Found)
	{
		*Found = TEXT('\0');
	}
	return OutBuffer;
}

void FHoloLensMisc::CreateGuid(FGuid& Result)
{
	verify(CoCreateGuid((GUID*)&Result)==S_OK);
}

int32 FHoloLensMisc::NumberOfCores()
{
	static int32 cpus = 0;
	if (cpus == 0)
	{
		ULONG CpuInfoBytes = 0;
		GetSystemCpuSetInformation(nullptr, 0, &CpuInfoBytes, GetCurrentProcess(), 0);

		TArray<byte> cpuInfoBuffer;
		cpuInfoBuffer.Reserve(CpuInfoBytes);
		GetSystemCpuSetInformation(reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(cpuInfoBuffer.GetData()),
			CpuInfoBytes, &CpuInfoBytes, GetCurrentProcess(), 0);

		byte* firstCpu = cpuInfoBuffer.GetData();
		int local_cpus = 0;
		for (byte* currentCpu = firstCpu; currentCpu < firstCpu + CpuInfoBytes; currentCpu += reinterpret_cast<PSYSTEM_CPU_SET_INFORMATION>(currentCpu)->Size)
		{
			local_cpus++;
		}
		cpus = local_cpus;
	}
	return cpus;
}

#if !UE_BUILD_SHIPPING
bool FHoloLensMisc::IsDebuggerPresent()
{
	return !!::IsDebuggerPresent(); 
}
#endif // UE_BUILD_SHIPPING

/** Get the application root directory. */
const TCHAR* FHoloLensMisc::RootDir()
{
	static FString Path;
	if (Path.Len() == 0)
	{
		Path = Windows::ApplicationModel::Package::Current->InstalledLocation->Path->Data();

		// Add trailing \\ - this is important for correct operation of FPaths::MakePathRelativeTo
		Path += TEXT("\\");

		// Switch to / so that FPaths::MakeStandardFilename will work
		Path.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
	}
	return *Path;
}

FString FHoloLensMisc::ProtocolActivationUri = TEXT("");

void FHoloLensMisc::SetProtocolActivationUri(const FString& NewUriString)
{
	ProtocolActivationUri = NewUriString;
}

const FString& FHoloLensMisc::GetProtocolActivationUri()
{
	return ProtocolActivationUri;
}

EAppReturnType::Type FHoloLensMisc::MessageBoxExt(EAppMsgType::Type MsgType, const TCHAR* Text, const TCHAR* Caption)
{
	using namespace Windows::UI::Core;
	using namespace Windows::UI::Popups;
	using namespace Windows::Foundation;

	Platform::Agile<MessageDialog> HoloLensMessageBox = Platform::Agile<MessageDialog>(ref new MessageDialog(ref new Platform::String(Text), ref new Platform::String(Caption)));

	Platform::String^ CancelString;
	Platform::String^ NoToAllString;
	Platform::String^ NoString;
	Platform::String^ YesToAllString;
	Platform::String^ YesString;
	Platform::String^ OkString;
	Platform::String^ RetryString;
	Platform::String^ ContinueString;

	// The Localize* functions will return the Key if a dialog is presented before the config system is initialized.
	// Instead, we use hard-coded strings if config is not yet initialized.
	if (!GConfig)
	{
		CancelString = TEXT("Cancel");
		NoToAllString = TEXT("No to All");
		NoString = TEXT("No");
		YesToAllString = TEXT("Yes to All");
		YesString = TEXT("Yes");
		OkString = TEXT("OK");
		RetryString = TEXT("Retry");
		ContinueString = TEXT("Continue");
	}
	else
	{
		CancelString = ref new Platform::String(*NSLOCTEXT("Core", "Cancel", "Cancel").ToString());
		NoToAllString = ref new Platform::String(*NSLOCTEXT("Core", "NoToAll", "No to All").ToString());
		NoString = ref new Platform::String(*NSLOCTEXT("Core", "No", "No").ToString());
		YesToAllString = ref new Platform::String(*NSLOCTEXT("Core", "YesToAll", "Yes to All").ToString());
		YesString = ref new Platform::String(*NSLOCTEXT("Core", "Yes", "Yes").ToString());
		OkString = ref new Platform::String(*NSLOCTEXT("Core", "OK", "OK").ToString());
		RetryString = ref new Platform::String(*NSLOCTEXT("Core", "Retry", "Retry").ToString());
		ContinueString = ref new Platform::String(*NSLOCTEXT("Core", "Continue", "Continue").ToString());
	}

	switch (MsgType)
	{
	case EAppMsgType::YesNo:
	{
		HoloLensMessageBox->Commands->Append(ref new UICommand(YesString, nullptr, (Platform::Object^)(int)EAppReturnType::Yes));
		HoloLensMessageBox->Commands->Append(ref new UICommand(NoString, nullptr, (Platform::Object^)(int)EAppReturnType::No));
		HoloLensMessageBox->CancelCommandIndex = 1;
		break;
	}
	case EAppMsgType::OkCancel:
	{
		HoloLensMessageBox->Commands->Append(ref new UICommand(OkString, nullptr, (Platform::Object^)(int)EAppReturnType::Ok));
		HoloLensMessageBox->Commands->Append(ref new UICommand(CancelString, nullptr, (Platform::Object^)(int)EAppReturnType::Cancel));
		HoloLensMessageBox->CancelCommandIndex = 1;
		break;
	}
	case EAppMsgType::YesNoYesAllNoAllCancel:
		UE_LOG(LogCore, Warning, TEXT("MessageBox type requires more buttons than can be displayed on this platform.  Using fallback."));
	case EAppMsgType::YesNoCancel:
	{
		HoloLensMessageBox->Commands->Append(ref new UICommand(YesString, nullptr, (Platform::Object^)(int)EAppReturnType::Yes));
		HoloLensMessageBox->Commands->Append(ref new UICommand(NoString, nullptr, (Platform::Object^)(int)EAppReturnType::No));
		HoloLensMessageBox->Commands->Append(ref new UICommand(CancelString, nullptr, (Platform::Object^)(int)EAppReturnType::Cancel));
		HoloLensMessageBox->CancelCommandIndex = 2;
		break;
	}
	case EAppMsgType::CancelRetryContinue:
	{
		HoloLensMessageBox->Commands->Append(ref new UICommand(CancelString, nullptr, (Platform::Object^)(int)EAppReturnType::Cancel));
		HoloLensMessageBox->Commands->Append(ref new UICommand(RetryString, nullptr, (Platform::Object^)(int)EAppReturnType::Retry));
		HoloLensMessageBox->Commands->Append(ref new UICommand(ContinueString, nullptr, (Platform::Object^)(int)EAppReturnType::Continue));
		HoloLensMessageBox->CancelCommandIndex = 0;
		break;
	}
	case EAppMsgType::YesNoYesAllNoAll:
		UE_LOG(LogCore, Warning, TEXT("MessageBox type requires more buttons than can be displayed on this platform.  Using fallback."));
	case EAppMsgType::YesNoYesAll:
	{
		HoloLensMessageBox->Commands->Append(ref new UICommand(YesString, nullptr, (Platform::Object^)(int)EAppReturnType::Yes));
		HoloLensMessageBox->Commands->Append(ref new UICommand(YesToAllString, nullptr, (Platform::Object^)(int)EAppReturnType::YesAll));
		HoloLensMessageBox->Commands->Append(ref new UICommand(NoString, nullptr, (Platform::Object^)(int)EAppReturnType::No));
		HoloLensMessageBox->CancelCommandIndex = 2;
		break;
	}
	case EAppMsgType::Ok:
	default:
	{
		HoloLensMessageBox->Commands->Append(ref new UICommand(OkString, nullptr, (Platform::Object^)(int)EAppReturnType::Ok));
		break;
	}
	}

	IAsyncOperation<IUICommand^>^ UIOperation;
	bool IsUIThread = CoreWindow::GetForCurrentThread() != nullptr;
	if (IsUIThread)
	{
		UIOperation = HoloLensMessageBox->ShowAsync();
	}
	else
	{
		// Must invoke the box from the UI thread, so dispatch to that
		IAsyncAction^ RunAction = Windows::ApplicationModel::Core::CoreApplication::MainView->Dispatcher->RunAsync(
			Windows::UI::Core::CoreDispatcherPriority::Normal,
			ref new Windows::UI::Core::DispatchedHandler([&UIOperation, HoloLensMessageBox]()
		{
			UIOperation = HoloLensMessageBox->ShowAsync();
		}));

		// Wait for the UI thread to invoke the box.  Note this completes when the
		// box is shown, not when it is closed.
		while (RunAction->Status == AsyncStatus::Started)
		{
			PumpMessages(false);

			// Yield to make sure UI thread can kick in and pop the UI.
			FHoloLensProcess::Sleep(0.f);
		}

		if (!UIOperation)
		{
			// Error in the dispatched call.  Default to cancel.
			return EAppReturnType::Cancel;
		}
	}

	// Pump the core window messages until the dialog has been dismissed.
	while (UIOperation->Status == AsyncStatus::Started)
	{
		PumpMessages(IsUIThread);

		// Yield - possibly needed if we're not the UI thread, and won't hurt if we are.
		FHoloLensProcess::Sleep(0.f);
	}

	// Return the command type pressed (or cancel for error).
	EAppReturnType::Type ReturnValue = EAppReturnType::Cancel;
	if (UIOperation->Status == AsyncStatus::Completed)
	{
		ReturnValue = static_cast<EAppReturnType::Type>((int)UIOperation->GetResults()->Id);
	}
	return ReturnValue;
}

void FHoloLensMisc::GetValidTargetPlatforms(class TArray<class FString>& TargetPlatformNames)
{
	TargetPlatformNames.Add(FPlatformProperties::PlatformName());
}

bool FHoloLensMisc::VerifyWindowsVersion(uint32 MajorVersion, uint32 MinorVersion, uint32 BuildNumber)
{
	// HoloLens is a Windows 10 OS.  
	// UWP does not have an equivalent of Win32's VerifyVersionInfo function which is used in WindowsPlatformMisc.
	// The closest alternative is parsing Windows.System.Profile.AnalyticsInfo.VersionInfo.DeviceFamilyVersion,
	// But, this string is not intended for runtime version checking and its format could change.
	return MajorVersion <= 10;
}
