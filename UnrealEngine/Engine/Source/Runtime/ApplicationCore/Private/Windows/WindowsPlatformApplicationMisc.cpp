// Copyright Epic Games, Inc. All Rights Reserved.

#include "Windows/WindowsPlatformApplicationMisc.h"
#include "Windows/WindowsApplication.h"
#include "Windows/WindowsApplicationErrorOutputDevice.h"
#include "Windows/WindowsConsoleOutputDevice.h"
#include "Windows/WindowsConsoleOutputDevice2.h"
#include "Windows/WindowsFeedbackContext.h"
#include "HAL/FeedbackContextAnsi.h"
#include "Misc/App.h"
#include "Math/Color.h"
#include "Misc/ScopeExit.h"
#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "Windows/WindowsPlatformOutputDevices.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "Templates/RefCounting.h"
#include "Null/NullPlatformApplicationMisc.h"

// Resource includes.
#include "Runtime/Launch/Resources/Windows/Resource.h"

THIRD_PARTY_INCLUDES_START
#include "Windows/AllowWindowsPlatformTypes.h"
#include "dxgi1_3.h"
#include "dxgi1_4.h"
#include "dxgi1_6.h"
#include "Windows/HideWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_END

typedef HRESULT(STDAPICALLTYPE *GetDpiForMonitorProc)(HMONITOR Monitor, int32 DPIType, uint32 *DPIX, uint32 *DPIY);
APPLICATIONCORE_API GetDpiForMonitorProc GetDpiForMonitor;

void FWindowsPlatformApplicationMisc::PreInit()
{
	FApp::SetHasFocusFunction(&FWindowsPlatformApplicationMisc::IsThisApplicationForeground);
}

void FWindowsPlatformApplicationMisc::LoadStartupModules()
{
#if !UE_SERVER
	FModuleManager::Get().LoadModule(TEXT("HeadMountedDisplay"));
#endif // !UE_SERVER

#if WITH_EDITOR
	FModuleManager::Get().LoadModule(TEXT("SourceCodeAccess"));
#endif	//WITH_EDITOR
}

class FOutputDeviceConsole* FWindowsPlatformApplicationMisc::CreateConsoleOutputDevice()
{
	// this is a slightly different kind of singleton that gives ownership to the caller and should not be called more than once
	if (FParse::Param(FCommandLine::Get(), TEXT("NewConsole")))
		return new FWindowsConsoleOutputDevice2();
	else
		return new FWindowsConsoleOutputDevice();
}

class FOutputDeviceError* FWindowsPlatformApplicationMisc::GetErrorOutputDevice()
{
	static FWindowsApplicationErrorOutputDevice Singleton;
	return &Singleton;
}

class FFeedbackContext* FWindowsPlatformApplicationMisc::GetFeedbackContext()
{
#if WITH_EDITOR
	static FWindowsFeedbackContext Singleton;
	return &Singleton;
#else
	return FPlatformOutputDevices::GetFeedbackContext();
#endif
}

GenericApplication* FWindowsPlatformApplicationMisc::CreateApplication()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("RenderOffScreen")))
	{
		return FNullPlatformApplicationMisc::CreateApplication();
	}

	HICON AppIconHandle = LoadIcon( hInstance, MAKEINTRESOURCE( GetAppIcon() ) );
	if( AppIconHandle == NULL )
	{
		AppIconHandle = LoadIcon( (HINSTANCE)NULL, IDI_APPLICATION ); 
	}

	return FWindowsApplication::CreateWindowsApplication( hInstance, AppIconHandle );
}

void FWindowsPlatformApplicationMisc::RequestMinimize()
{
	::ShowWindow(::GetActiveWindow(), SW_MINIMIZE);
}

bool FWindowsPlatformApplicationMisc::IsThisApplicationForeground()
{
	uint32 ForegroundProcess;
	::GetWindowThreadProcessId(GetForegroundWindow(), (::DWORD *)&ForegroundProcess);
	return (ForegroundProcess == GetCurrentProcessId());
}

int32 FWindowsPlatformApplicationMisc::GetAppIcon()
{
	return IDICON_UEGame;
}

static void WinPumpMessages()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(WinPumpMessages);
	{
		MSG Msg;
		while( PeekMessage(&Msg,NULL,0,0,PM_REMOVE) )
		{
			TranslateMessage( &Msg );
			DispatchMessage( &Msg );
		}
	}
}


void FWindowsPlatformApplicationMisc::PumpMessages(bool bFromMainLoop)
{
	const bool bSetPumpingMessages = !GPumpingMessages;
	if (bSetPumpingMessages)
	{
		GPumpingMessages = true;
	}

	ON_SCOPE_EXIT
	{
		if (bSetPumpingMessages)
		{
			GPumpingMessages = false;
		}
	};

	if (!bFromMainLoop)
	{
		FPlatformMisc::PumpMessagesOutsideMainLoop();
		return;
	}

	GPumpingMessagesOutsideOfMainLoop = false;
	WinPumpMessages();

	// Determine if application has focus
	bool bHasFocus = FApp::HasFocus();
	static bool bHadFocus = false;

#if !UE_SERVER
	// For non-editor clients, record if the active window is in focus
	if( bHadFocus != bHasFocus )
	{
		FGenericCrashContext::SetEngineData(TEXT("Platform.AppHasFocus"), bHasFocus ? TEXT("true") : TEXT("false"));
	}
#endif

	bHadFocus = bHasFocus;

	// if its our window, allow sound, otherwise apply multiplier
	FApp::SetVolumeMultiplier( bHasFocus ? 1.0f : FApp::GetUnfocusedVolumeMultiplier() );
}

void FWindowsPlatformApplicationMisc::PreventScreenSaver()
{
	INPUT Input = { 0 };
	Input.type = INPUT_MOUSE;
	Input.mi.dx = 0;
	Input.mi.dy = 0;	
	Input.mi.mouseData = 0;
	Input.mi.dwFlags = MOUSEEVENTF_MOVE;
	Input.mi.time = 0;
	Input.mi.dwExtraInfo = 0; 	
	SendInput(1,&Input,sizeof(INPUT));
}

FLinearColor FWindowsPlatformApplicationMisc::GetScreenPixelColor(const FVector2D& InScreenPos, float /*InGamma*/)
{
	HDC TempDC = GetDC(HWND_DESKTOP);
	COLORREF PixelColorRef = GetPixel(TempDC, (int)InScreenPos.X, (int)InScreenPos.Y);

	ReleaseDC(HWND_DESKTOP, TempDC);

	FColor sRGBScreenColor(
		(uint8)(PixelColorRef & 0xFF),
		(uint8)((PixelColorRef & 0xFF00) >> 8),
		(uint8)((PixelColorRef & 0xFF0000) >> 16),
		255);

	// Assume the screen color is coming in as sRGB space
	return FLinearColor(sRGBScreenColor);
}

void FWindowsPlatformApplicationMisc::SetHighDPIMode()
{
	if (IsHighDPIAwarenessEnabled())
	{
		if (void* ShCoreDll = FPlatformProcess::GetDllHandle(TEXT("shcore.dll")))
		{
			typedef enum _PROCESS_DPI_AWARENESS {
				PROCESS_DPI_UNAWARE = 0,
				PROCESS_SYSTEM_DPI_AWARE = 1,
				PROCESS_PER_MONITOR_DPI_AWARE = 2
			} PROCESS_DPI_AWARENESS;

			typedef HRESULT(STDAPICALLTYPE *SetProcessDpiAwarenessProc)(PROCESS_DPI_AWARENESS Value);
			SetProcessDpiAwarenessProc SetProcessDpiAwareness = (SetProcessDpiAwarenessProc)FPlatformProcess::GetDllExport(ShCoreDll, TEXT("SetProcessDpiAwareness"));
			GetDpiForMonitor = (GetDpiForMonitorProc)FPlatformProcess::GetDllExport(ShCoreDll, TEXT("GetDpiForMonitor"));

			typedef HRESULT(STDAPICALLTYPE *GetProcessDpiAwarenessProc)(HANDLE hProcess, PROCESS_DPI_AWARENESS* Value);
			GetProcessDpiAwarenessProc GetProcessDpiAwareness = (GetProcessDpiAwarenessProc)FPlatformProcess::GetDllExport(ShCoreDll, TEXT("GetProcessDpiAwareness"));

			if (SetProcessDpiAwareness && GetProcessDpiAwareness && !IsRunningCommandlet() && !FApp::IsUnattended())
			{
				PROCESS_DPI_AWARENESS CurrentAwareness = PROCESS_DPI_UNAWARE;

				GetProcessDpiAwareness(nullptr, &CurrentAwareness);

				if (CurrentAwareness != PROCESS_PER_MONITOR_DPI_AWARE)
				{
					UE_LOG(LogInit, Log, TEXT("Setting process to per monitor DPI aware"));
					HRESULT Hr = SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE); // PROCESS_PER_MONITOR_DPI_AWARE_VALUE
																						// We dont care about this warning if we are in any kind of headless mode
					if (Hr != S_OK)
					{
						UE_LOG(LogInit, Warning, TEXT("SetProcessDpiAwareness failed.  Error code %x"), Hr);
					}
				}
			}

			FPlatformProcess::FreeDllHandle(ShCoreDll);
		}
		else if (void* User32Dll = GetModuleHandle(L"user32.dll"))
		{
			typedef BOOL(WINAPI *SetProcessDpiAwareProc)(void);
			SetProcessDpiAwareProc SetProcessDpiAware = (SetProcessDpiAwareProc)FPlatformProcess::GetDllExport(User32Dll, TEXT("SetProcessDPIAware"));

			if (SetProcessDpiAware && !IsRunningCommandlet() && !FApp::IsUnattended())
			{
				UE_LOG(LogInit, Log, TEXT("Setting process to DPI aware"));

				BOOL Result = SetProcessDpiAware();
				if (Result == 0)
				{
					UE_LOG(LogInit, Warning, TEXT("SetProcessDpiAware failed"));
				}
			}
		}
	}
}

bool FWindowsPlatformApplicationMisc::GetWindowTitleMatchingText(const TCHAR* TitleStartsWith, FString& OutTitle)
{
	bool bWasFound = false;
	WCHAR Buffer[8192];
	// Get the first window so we can start walking the window chain
	HWND hWnd = FindWindowW(NULL,NULL);
	if (hWnd != NULL)
	{
		size_t TitleStartsWithLen = _tcslen(TitleStartsWith);
		do
		{
			GetWindowText(hWnd,Buffer,8192);
			// If this matches, then grab the full text
			if (_tcsnccmp(TitleStartsWith, Buffer, TitleStartsWithLen) == 0)
			{
				OutTitle = Buffer;
				hWnd = NULL;
				bWasFound = true;
			}
			else
			{
				// Get the next window to interrogate
				hWnd = GetWindow(hWnd, GW_HWNDNEXT);
			}
		}
		while (hWnd != NULL);
	}
	return bWasFound;
}

int32 FWindowsPlatformApplicationMisc::GetMonitorDPI(const FMonitorInfo& MonitorInfo)
{
	int32 DisplayDPI = 96;

	if (IsHighDPIAwarenessEnabled())
	{
		if (GetDpiForMonitor)
		{
			RECT MonitorDim;
			MonitorDim.left = MonitorInfo.DisplayRect.Left;
			MonitorDim.top = MonitorInfo.DisplayRect.Top;
			MonitorDim.right = MonitorInfo.DisplayRect.Right;
			MonitorDim.bottom = MonitorInfo.DisplayRect.Bottom;

			HMONITOR Monitor = MonitorFromRect(&MonitorDim, MONITOR_DEFAULTTONEAREST);
			if (Monitor)
			{
				uint32 DPIX = 0;
				uint32 DPIY = 0;
				if (SUCCEEDED(GetDpiForMonitor(Monitor, 0 /*MDT_EFFECTIVE_DPI*/, &DPIX, &DPIY)))
				{
					DisplayDPI = DPIX;
				}
			}
		}
		else
		{
			HDC Context = GetDC(nullptr);
			DisplayDPI = GetDeviceCaps(Context, LOGPIXELSX);
			ReleaseDC(nullptr, Context);
		}
	}

	return DisplayDPI;
}

// Looks for an adapter with the most dedicated video memory
FWindowsPlatformApplicationMisc::FGPUInfo FWindowsPlatformApplicationMisc::GetBestGPUInfo()
{ 
	TRefCountPtr<IDXGIFactory1> DXGIFactory1;
	if (CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&DXGIFactory1) != S_OK || !DXGIFactory1)
	{
		return {};
	}

	DXGI_ADAPTER_DESC BestDesc = {};
	TRefCountPtr<IDXGIAdapter> TempAdapter;
	for (uint32 AdapterIndex = 0; DXGIFactory1->EnumAdapters(AdapterIndex, TempAdapter.GetInitReference()) != DXGI_ERROR_NOT_FOUND; ++AdapterIndex)
	{
		if (TempAdapter)
		{
			DXGI_ADAPTER_DESC Desc;
			TempAdapter->GetDesc(&Desc);

			if (Desc.DedicatedVideoMemory > BestDesc.DedicatedVideoMemory || AdapterIndex == 0)
			{
				BestDesc = Desc;
			}
		}
	}

	return FGPUInfo{ BestDesc.VendorId, BestDesc.DeviceId, BestDesc.DedicatedVideoMemory };
}

float FWindowsPlatformApplicationMisc::GetDPIScaleFactorAtPoint(float X, float Y)
{
	float Scale = 1.0f;

	if (IsHighDPIAwarenessEnabled())
	{
		if (GetDpiForMonitor)
		{
			POINT Position = { static_cast<LONG>(X), static_cast<LONG>(Y) };
			HMONITOR Monitor = MonitorFromPoint(Position, MONITOR_DEFAULTTONEAREST);
			if (Monitor)
			{
				uint32 DPIX = 0;
				uint32 DPIY = 0;
				if (SUCCEEDED(GetDpiForMonitor(Monitor, 0 /*MDT_EFFECTIVE_DPI*/, &DPIX, &DPIY)))
				{
					Scale = (float)DPIX / 96.0f;
				}
			}
		}
		else
		{
			HDC Context = GetDC(nullptr);
			int32 DPI = GetDeviceCaps(Context, LOGPIXELSX);
			Scale = (float)DPI / 96.0f;
			ReleaseDC(nullptr, Context);
		}
	}

	return Scale;
}

// Disabling optimizations helps to reduce the frequency of OpenClipboard failing with error code 0. It still happens
// though only with really large text buffers and we worked around this by changing the editor to use an intermediate
// text buffer for internal operations.
UE_DISABLE_OPTIMIZATION_SHIP

void FWindowsPlatformApplicationMisc::ClipboardCopy(const TCHAR* Str)
{
	if( OpenClipboard(GetActiveWindow()) )
	{
		verify(EmptyClipboard());
		HGLOBAL GlobalMem;
		int32 StrLen = FCString::Strlen(Str);
		GlobalMem = GlobalAlloc( GMEM_MOVEABLE, sizeof(TCHAR)*(StrLen+1) );
		check(GlobalMem);
		TCHAR* Data = (TCHAR*) GlobalLock( GlobalMem );
		FCString::Strcpy( Data, (StrLen+1), Str );
		GlobalUnlock( GlobalMem );
		if( SetClipboardData( CF_UNICODETEXT, GlobalMem ) == NULL )
			UE_LOG(LogWindows, Fatal,TEXT("SetClipboardData failed with error code %i"), (uint32)GetLastError() );
		verify(CloseClipboard());
	}
	else
	{
		UE_LOG(LogWindows, Warning, TEXT("OpenClipboard failed with error code %i"), (uint32)GetLastError());
	}
}

void FWindowsPlatformApplicationMisc::ClipboardPaste(class FString& Result)
{
	if( OpenClipboard(GetActiveWindow()) )
	{
		HGLOBAL GlobalMem = NULL;
		bool Unicode = 0;
		GlobalMem = GetClipboardData( CF_UNICODETEXT );
		Unicode = 1;
		if( !GlobalMem )
		{
			GlobalMem = GetClipboardData( CF_TEXT );
			Unicode = 0;
		}
		if( !GlobalMem )
		{
			Result = TEXT("");
		}
		else
		{
			void* Data = GlobalLock( GlobalMem );
			check( Data );	
			if( Unicode )
				Result = (TCHAR*) Data;
			else
			{
				ANSICHAR* ACh = (ANSICHAR*) Data;
				int32 i;
				for( i=0; ACh[i]; i++ );
				TArray<TCHAR> Ch;
				Ch.AddUninitialized(i+1);
				for( i=0; i<Ch.Num(); i++ )
					Ch[i]=CharCast<TCHAR>(ACh[i]);
				Result.GetCharArray() = MoveTemp(Ch);
			}
			GlobalUnlock( GlobalMem );
		}
		verify(CloseClipboard());
	}
	else 
	{
		Result=TEXT("");
		UE_LOG(LogWindows, Warning, TEXT("OpenClipboard failed with error code %i"), (uint32)GetLastError());
	}
}

UE_ENABLE_OPTIMIZATION_SHIP
