// Copyright Epic Games, Inc. All Rights Reserved.

#include "WindowsStylusInputInterface.h"
#include "WindowsRealTimeStylusPlugin.h"
#include "Interfaces/IMainFrameModule.h"

#include "Framework/Application/SlateApplication.h" 

#if PLATFORM_WINDOWS

#include <ShlObj.h>

class FWindowsStylusInputInterfaceImpl
{
public:
	~FWindowsStylusInputInterfaceImpl();

	TMap<void*, TSharedPtr<FWindowsRealTimeStylusPlugin>> StylusPlugins;
	void* DLLHandle { nullptr };
	TArray<FTabletContextInfo> TabletContexts;
};

// We desire to receive everything, but what we actually will receive is determined in AddTabletContext
static const TArray<GUID> DesiredPackets = {
	GUID_PACKETPROPERTY_GUID_X,
	GUID_PACKETPROPERTY_GUID_Y,
	GUID_PACKETPROPERTY_GUID_Z,
	GUID_PACKETPROPERTY_GUID_PACKET_STATUS,
	GUID_PACKETPROPERTY_GUID_NORMAL_PRESSURE,
	GUID_PACKETPROPERTY_GUID_TANGENT_PRESSURE,
	GUID_PACKETPROPERTY_GUID_X_TILT_ORIENTATION,
	GUID_PACKETPROPERTY_GUID_Y_TILT_ORIENTATION,
	GUID_PACKETPROPERTY_GUID_TWIST_ORIENTATION,
	GUID_PACKETPROPERTY_GUID_WIDTH,
	GUID_PACKETPROPERTY_GUID_HEIGHT,
	// Currently not needed.
	//GUID_PACKETPROPERTY_GUID_BUTTON_PRESSURE,
	//GUID_PACKETPROPERTY_GUID_AZIMUTH_ORIENTATION,
	//GUID_PACKETPROPERTY_GUID_ALTITUDE_ORIENTATION,
};

FWindowsStylusInputInterface::FWindowsStylusInputInterface(TUniquePtr<FWindowsStylusInputInterfaceImpl> InImpl)
{
	check(InImpl.IsValid());

	Impl = MoveTemp(InImpl);
}

FWindowsStylusInputInterface::~FWindowsStylusInputInterface() = default;

void FWindowsStylusInputInterface::CreateStylusPluginForHWND(void* HwndPtr)
{
	if (Impl->StylusPlugins.Contains(HwndPtr))
	{
		return;
	}

	HWND Hwnd = reinterpret_cast<HWND>(HwndPtr);

	// Create RealTimeStylus interface
	void* OutInstance = nullptr;
	HRESULT hr = ::CoCreateInstance(__uuidof(RealTimeStylus), nullptr, CLSCTX_INPROC, __uuidof(IRealTimeStylus), &OutInstance);
	if (FAILED(hr))
	{
		UE_LOG(LogStylusInput, Warning, TEXT("Could not create RealTimeStylus!"));
		return;
	}

	TSharedPtr<FWindowsRealTimeStylusPlugin> NewPlugin = Impl->StylusPlugins.Add(HwndPtr, MakeShareable(new FWindowsRealTimeStylusPlugin()));
	NewPlugin->RealTimeStylus = static_cast<IRealTimeStylus*>(OutInstance);

	// Create free-threaded marshaller for the plugin
	hr = ::CoCreateFreeThreadedMarshaler(NewPlugin.Get(), &NewPlugin->FreeThreadedMarshaler);
	if (FAILED(hr))
	{
		UE_LOG(LogStylusInput, Warning, TEXT("Could not create FreeThreadedMarshaler!"));
		return;
	}

	NewPlugin->TabletContexts = &Impl->TabletContexts;

	NewPlugin->RealTimeStylus->SetDesiredPacketDescription(DesiredPackets.Num(), DesiredPackets.GetData());

	// Add stylus plugin to the interface
	hr = NewPlugin->RealTimeStylus->AddStylusSyncPlugin(0, NewPlugin.Get());
	if (FAILED(hr))
	{
		UE_LOG(LogStylusInput, Warning, TEXT("Could not add stylus plugin to API!"));
		return;
	}
	
	NewPlugin->RealTimeStylus->put_HWND(reinterpret_cast<uint64>(Hwnd));
	NewPlugin->RealTimeStylus->put_Enabled(Windows::TRUE);
}

void FWindowsStylusInputInterface::RemovePluginForWindow(const TSharedRef<SWindow>& Window)
{
	void* Hwnd = Window->GetNativeWindow()->GetOSWindowHandle();
	TSharedPtr<FWindowsRealTimeStylusPlugin>* Plugin = Impl->StylusPlugins.Find(Hwnd);
	if (Plugin != nullptr)
	{
		(*Plugin)->RealTimeStylus->put_Enabled(Windows::FALSE);
		(*Plugin)->RealTimeStylus->RemoveAllStylusSyncPlugins();

		Impl->StylusPlugins.Remove(Hwnd);
	}
}

void FWindowsStylusInputInterface::Tick()
{
	for (const FTabletContextInfo& Context : Impl->TabletContexts)
	{
		// don't change focus if any stylus is down
		if (Context.GetCurrentState().IsStylusDown())
		{
			return;
		}
	}

	if (!FSlateApplication::IsInitialized())
	{
		return;
	}

	FSlateApplication& Application = FSlateApplication::Get();

	FWidgetPath WidgetPath = Application.LocateWindowUnderMouse(Application.GetCursorPos(), Application.GetInteractiveTopLevelWindows());
	if (WidgetPath.IsValid())
	{
		TSharedPtr<SWindow> Window = WidgetPath.GetWindow();
		if (Window.IsValid() && Window->IsRegularWindow())
		{
			TSharedPtr<FGenericWindow> NativeWindow = Window->GetNativeWindow();
			void* Hwnd = NativeWindow->GetOSWindowHandle();

			TSharedPtr<FWindowsRealTimeStylusPlugin>* CurrentPlugin = Impl->StylusPlugins.Find(Hwnd);
			if (CurrentPlugin == nullptr)
			{
				CreateStylusPluginForHWND(Hwnd);
				Window->GetOnWindowClosedEvent().AddRaw(this, &FWindowsStylusInputInterface::RemovePluginForWindow);
			}
		}
	}
}

int32 FWindowsStylusInputInterface::NumInputDevices() const
{
	return Impl->TabletContexts.Num();
}

IStylusInputDevice* FWindowsStylusInputInterface::GetInputDevice(int32 Index) const
{
	IStylusInputDevice* Result = nullptr;
	if (Index >= 0 && Index < Impl->TabletContexts.Num())
	{
		Result = &Impl->TabletContexts[Index];
	}
	return Result;
}

FWindowsStylusInputInterfaceImpl::~FWindowsStylusInputInterfaceImpl()
{
	for (const auto& Plugin : StylusPlugins)
	{
		Plugin.Value->RealTimeStylus->RemoveAllStylusSyncPlugins();
		Plugin.Value->RealTimeStylus.Reset();
	}

	if (DLLHandle != nullptr)
	{
		FPlatformProcess::FreeDllHandle(DLLHandle);
		DLLHandle = nullptr;
	}
}

TSharedPtr<IStylusInputInterfaceInternal> CreateStylusInputInterface()
{
	if (!FWindowsPlatformMisc::CoInitialize()) 
	{
		UE_LOG(LogStylusInput, Display, TEXT("Could not initialize COM library!"));
		return nullptr;
	}

	TUniquePtr<FWindowsStylusInputInterfaceImpl> WindowsImpl = MakeUnique<FWindowsStylusInputInterfaceImpl>();

	// Load RealTimeStylus DLL 
	TCHAR CommonFilesPath[MAX_PATH];
	::SHGetFolderPath(0, CSIDL_PROGRAM_FILES_COMMON, NULL, 0, CommonFilesPath);
	const FString InkDLLDirectory = FString(CommonFilesPath) + TEXT("\\microsoft shared\\ink");
	FPlatformProcess::PushDllDirectory(*InkDLLDirectory);

	const FString RTSComDLL = TEXT("RTSCom.dll");
	WindowsImpl->DLLHandle = FPlatformProcess::GetDllHandle(*(InkDLLDirectory / RTSComDLL));
	if (WindowsImpl->DLLHandle == nullptr)
	{
		FWindowsPlatformMisc::CoUninitialize();
		UE_LOG(LogStylusInput, Display, TEXT("Could not load RTSCom.dll. Expected folder: %s"), *InkDLLDirectory);
		return nullptr;
	}

	FPlatformProcess::PopDllDirectory(*InkDLLDirectory);

	return MakeShared<FWindowsStylusInputInterface>(MoveTemp(WindowsImpl));
}

#endif // PLATFORM_WINDOWS