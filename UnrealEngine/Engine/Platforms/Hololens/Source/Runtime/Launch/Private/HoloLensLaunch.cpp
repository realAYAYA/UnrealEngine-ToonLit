// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformAffinity.h"
#include "Misc/LocalTimestampDirectoryVisitor.h"

#include "Misc/App.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/EngineVersion.h"

#include "LaunchEngineLoop.h"
//#include "CoreTypes.h"
#include "HAL/ExceptionHandling.h"
#include "RHI.h"
#include "RenderingThread.h"

#include "Interfaces/IPluginManager.h"

#include "HoloLensApplication.h"
#include "HoloLensCursor.h"
#include "HoloLensWindow.h"
#include <stdio.h>

//#include "Microsoft/AllowMicrosoftPlatformAtomics.h"
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#include <collection.h>
#include <ppltasks.h>
#include <concurrent_queue.h>
#include "Microsoft/HideMicrosoftPlatformTypes.h"
//#include "Microsoft/HideMicrosoftPlatformAtomics.h"

// @MIXEDREALITY_CHANGE : BEGIN - Enable Stereo rendering for suspended apps
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "Modules/ModuleManager.h"
// @MIXEDREALITY_CHANGE : END

// http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
// The following line is to favor the high performance NVIDIA GPU if there are multiple GPUs
// Has to be .exe module to be correctly detected.
extern "C" { _declspec(dllexport) uint32 NvOptimusEnablement = 0x00000001; }

// Similar for AMD GPU
extern "C" {__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1; }

#pragma warning(disable : 4946)	// reinterpret_cast used between related classes: 'Platform::Object' and ...

using namespace Windows::ApplicationModel::Activation;
using namespace Windows::ApplicationModel::Core;
using namespace Windows::ApplicationModel::ExtendedExecution;
using namespace Windows::Foundation;
using namespace Windows::System;
using namespace Windows::UI::Core;
using namespace Windows::Graphics::Holographic;

static bool GSeparateCoreWindowVisible = false;

DEFINE_LOG_CATEGORY_STATIC(LogLaunchHoloLens, Log, All);

void appHoloLensEarlyInit();
int32 GuardedMain( const TCHAR* CmdLine, HINSTANCE hInInstance, HINSTANCE hPrevInstance, int32 nCmdShow );

ref class ViewProvider sealed : public Windows::ApplicationModel::Core::IFrameworkView
{
public:
	ViewProvider();

	// IFrameworkView impl.
	virtual void Initialize(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView);
	virtual void SetWindow(Windows::UI::Core::CoreWindow^ window);
	virtual void Load(Platform::String^ entryPoint);
	virtual void Run();
	virtual void Uninitialize();

	// Process events on the core window dispatcher.
	void ProcessEvents();

	// Keyboard event handlers.
	void OnKeyDown(_In_ CoreWindow^ sender, _In_ KeyEventArgs^ args);
	void OnKeyUp(_In_ CoreWindow^ sender, _In_ KeyEventArgs^ args);
	void OnCharacterReceived(_In_ CoreWindow^ sender, _In_ CharacterReceivedEventArgs^ args);

	//! Dispatcher event handlers.
	void OnAcceleratorKeyActivated(CoreDispatcher ^sender, AcceleratorKeyEventArgs ^args);
	// Window event handlers.
	void OnVisibilityChanged(_In_ CoreWindow^ sender, _In_ VisibilityChangedEventArgs^ args);
	void OnWindowSizeChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::WindowSizeChangedEventArgs^ args);
	void OnWindowClosed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CoreWindowEventArgs^ args);

	void OnPointerPressed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);
	void OnPointerReleased(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);
	void OnPointerMoved(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);
	void OnPointerWheelChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args);

	bool IsVisible()
	{
		return bVisible;
	}

	bool WasWindowClosed()
	{
		return bWindowClosed;
	}

	void InitOptionalPackages();

private:
	Windows::Foundation::Rect existingSize;

	bool ActivationComplete;

	void OnActivated( _In_ Windows::ApplicationModel::Core::CoreApplicationView^ applicationView, _In_ Windows::ApplicationModel::Activation::IActivatedEventArgs^ args );
	void OnResuming( _In_ Platform::Object^ sender, _In_ Platform::Object^ args );
	void OnSuspending( _In_ Platform::Object^ sender, _In_ Windows::ApplicationModel::SuspendingEventArgs^ args );
	void OnEnteredBackground(_In_ Platform::Object^ sender, _In_ Windows::ApplicationModel::EnteredBackgroundEventArgs^ args);
	void OnLeavingBackground(_In_ Platform::Object^ sender, _In_ Windows::ApplicationModel::LeavingBackgroundEventArgs^ args);
	void OnExiting(_In_ Platform::Object^ sender, _In_ Platform::Object^ args);

	const TCHAR* GetPointerUpdateKindString(Windows::UI::Input::PointerUpdateKind InKind);
	EMouseButtons::Type PointerUpdateKindToUEKey(Windows::UI::Input::PointerUpdateKind InKind, bool& bWasPressed);
	bool ProcessMouseEvent(Windows::UI::Core::PointerEventArgs^ args);

	bool bVisible;
	bool bWindowClosed;

	struct QueuedPointerEvent
	{
		QueuedPointerEvent(Windows::UI::Core::PointerEventArgs^ Args) :
			RawPosition(Args->CurrentPoint->Position.X, Args->CurrentPoint->Position.Y),
			WheelDelta(Args->CurrentPoint->Properties->MouseWheelDelta),
			Kind(Args->CurrentPoint->Properties->PointerUpdateKind)
		{
		}

		FVector2D RawPosition;
		int32 WheelDelta;
		Windows::UI::Input::PointerUpdateKind Kind;
	};
	bool ProcessMouseEvent(const QueuedPointerEvent& Event);

	static void RegisterDlcPluginsForOptionalPackage(Windows::ApplicationModel::Package ^DependencyPackage);
	static void EnsureDlcPluginsAreMounted();

	TArray<QueuedPointerEvent> PointerEventQueue;
};

ViewProvider^ GViewProvider = nullptr;

ref class ViewProviderFactory sealed : Windows::ApplicationModel::Core::IFrameworkViewSource 
{
public:
	ViewProviderFactory() {}
	virtual Windows::ApplicationModel::Core::IFrameworkView^ CreateView()
	{
		GViewProvider = ref new ViewProvider();
		return GViewProvider;
	}
};

ViewProvider::ViewProvider() :
	ActivationComplete(false)
{
}

uint32 TranslateWinRTKey(Windows::UI::Core::KeyEventArgs^ keyEventArgs)
{
	Windows::System::VirtualKey WinRTKeyCode = keyEventArgs->VirtualKey;

	uint32 TranslatedKeyCode = (uint32)WinRTKeyCode;
	switch( WinRTKeyCode )
	{
	case Windows::System::VirtualKey::LeftButton:	TranslatedKeyCode = VK_LBUTTON;		break;
	case Windows::System::VirtualKey::RightButton:	TranslatedKeyCode = VK_RBUTTON;		break;
	case Windows::System::VirtualKey::MiddleButton:	TranslatedKeyCode = VK_MBUTTON;		break;

	case Windows::System::VirtualKey::XButton1:		TranslatedKeyCode = VK_XBUTTON1;	break;
	case Windows::System::VirtualKey::XButton2:		TranslatedKeyCode = VK_XBUTTON2;	break;

	case Windows::System::VirtualKey::Back:			TranslatedKeyCode = VK_BACK;		break;
	case Windows::System::VirtualKey::Tab:			TranslatedKeyCode = VK_TAB;			break;
	case Windows::System::VirtualKey::Enter:		TranslatedKeyCode = VK_RETURN;		break;
	case Windows::System::VirtualKey::Pause:		TranslatedKeyCode = VK_PAUSE;		break;

	case Windows::System::VirtualKey::CapitalLock:	TranslatedKeyCode = VK_CAPITAL;		break;
	case Windows::System::VirtualKey::Escape:		TranslatedKeyCode = VK_ESCAPE;		break;
	case Windows::System::VirtualKey::Space:		TranslatedKeyCode = VK_SPACE;		break;

	case Windows::System::VirtualKey::PageUp:		TranslatedKeyCode = VK_PRIOR;		break;
	case Windows::System::VirtualKey::PageDown:		TranslatedKeyCode = VK_NEXT;		break;
	case Windows::System::VirtualKey::End:			TranslatedKeyCode = VK_END;			break;
	case Windows::System::VirtualKey::Home:			TranslatedKeyCode = VK_HOME;		break;

	case Windows::System::VirtualKey::Left:			TranslatedKeyCode = VK_LEFT;		break;
	case Windows::System::VirtualKey::Up:			TranslatedKeyCode = VK_UP;			break;
	case Windows::System::VirtualKey::Right:		TranslatedKeyCode = VK_RIGHT;		break;
	case Windows::System::VirtualKey::Down:			TranslatedKeyCode = VK_DOWN;		break;

	case Windows::System::VirtualKey::Insert:		TranslatedKeyCode = VK_INSERT;		break;
	case Windows::System::VirtualKey::Delete:		TranslatedKeyCode = VK_DELETE;		break;

	// VK_0 - VK_9 are the same as ASCII '0' - '9' (0x30 - 0x39)
	// 0x40 : unassigned
	// VK_A - VK_Z are the same as ASCII 'A' - 'Z' (0x41 - 0x5A)
	case Windows::System::VirtualKey::Number0:		TranslatedKeyCode = 0x30;			break;
	case Windows::System::VirtualKey::Number1:		TranslatedKeyCode = 0x31;			break;
	case Windows::System::VirtualKey::Number2:		TranslatedKeyCode = 0x32;			break;
	case Windows::System::VirtualKey::Number3:		TranslatedKeyCode = 0x33;			break;
	case Windows::System::VirtualKey::Number4:		TranslatedKeyCode = 0x34;			break;
	case Windows::System::VirtualKey::Number5:		TranslatedKeyCode = 0x35;			break;
	case Windows::System::VirtualKey::Number6:		TranslatedKeyCode = 0x36;			break;
	case Windows::System::VirtualKey::Number7:		TranslatedKeyCode = 0x37;			break;
	case Windows::System::VirtualKey::Number8:		TranslatedKeyCode = 0x38;			break;
	case Windows::System::VirtualKey::Number9:		TranslatedKeyCode = 0x39;			break;
	case Windows::System::VirtualKey::A:			TranslatedKeyCode = 0x41;			break;
	case Windows::System::VirtualKey::B:			TranslatedKeyCode = 0x42;			break;
	case Windows::System::VirtualKey::C:			TranslatedKeyCode = 0x43;			break;
	case Windows::System::VirtualKey::D:			TranslatedKeyCode = 0x44;			break;
	case Windows::System::VirtualKey::E:			TranslatedKeyCode = 0x45;			break;
	case Windows::System::VirtualKey::F:			TranslatedKeyCode = 0x46;			break;
	case Windows::System::VirtualKey::G:			TranslatedKeyCode = 0x47;			break;
	case Windows::System::VirtualKey::H:			TranslatedKeyCode = 0x48;			break;
	case Windows::System::VirtualKey::I:			TranslatedKeyCode = 0x49;			break;
	case Windows::System::VirtualKey::J:			TranslatedKeyCode = 0x4A;			break;
	case Windows::System::VirtualKey::K:			TranslatedKeyCode = 0x4B;			break;
	case Windows::System::VirtualKey::L:			TranslatedKeyCode = 0x4C;			break;
	case Windows::System::VirtualKey::M:			TranslatedKeyCode = 0x4D;			break;
	case Windows::System::VirtualKey::N:			TranslatedKeyCode = 0x4E;			break;
	case Windows::System::VirtualKey::O:			TranslatedKeyCode = 0x4F;			break;
	case Windows::System::VirtualKey::P:			TranslatedKeyCode = 0x50;			break;
	case Windows::System::VirtualKey::Q:			TranslatedKeyCode = 0x51;			break;
	case Windows::System::VirtualKey::R:			TranslatedKeyCode = 0x52;			break;
	case Windows::System::VirtualKey::S:			TranslatedKeyCode = 0x53;			break;
	case Windows::System::VirtualKey::T:			TranslatedKeyCode = 0x54;			break;
	case Windows::System::VirtualKey::U:			TranslatedKeyCode = 0x55;			break;
	case Windows::System::VirtualKey::V:			TranslatedKeyCode = 0x56;			break;
	case Windows::System::VirtualKey::W:			TranslatedKeyCode = 0x57;			break;
	case Windows::System::VirtualKey::X:			TranslatedKeyCode = 0x58;			break;
	case Windows::System::VirtualKey::Y:			TranslatedKeyCode = 0x59;			break;
	case Windows::System::VirtualKey::Z:			TranslatedKeyCode = 0x5A;			break;

	case Windows::System::VirtualKey::NumberPad0:	TranslatedKeyCode = VK_NUMPAD0;		break;
	case Windows::System::VirtualKey::NumberPad1:	TranslatedKeyCode = VK_NUMPAD1;		break;
	case Windows::System::VirtualKey::NumberPad2:	TranslatedKeyCode = VK_NUMPAD2;		break;
	case Windows::System::VirtualKey::NumberPad3:	TranslatedKeyCode = VK_NUMPAD3;		break;
	case Windows::System::VirtualKey::NumberPad4:	TranslatedKeyCode = VK_NUMPAD4;		break;
	case Windows::System::VirtualKey::NumberPad5:	TranslatedKeyCode = VK_NUMPAD5;		break;
	case Windows::System::VirtualKey::NumberPad6:	TranslatedKeyCode = VK_NUMPAD6;		break;
	case Windows::System::VirtualKey::NumberPad7:	TranslatedKeyCode = VK_NUMPAD7;		break;
	case Windows::System::VirtualKey::NumberPad8:	TranslatedKeyCode = VK_NUMPAD8;		break;
	case Windows::System::VirtualKey::NumberPad9:	TranslatedKeyCode = VK_NUMPAD9;		break;

	case Windows::System::VirtualKey::Multiply:		TranslatedKeyCode = VK_MULTIPLY;	break;
	case Windows::System::VirtualKey::Add:			TranslatedKeyCode = VK_ADD;			break;
	case Windows::System::VirtualKey::Subtract:		TranslatedKeyCode = VK_SUBTRACT;	break;
	case Windows::System::VirtualKey::Decimal:		TranslatedKeyCode = VK_DECIMAL;		break;
	case Windows::System::VirtualKey::Divide:		TranslatedKeyCode = VK_DIVIDE;		break;

	case Windows::System::VirtualKey::F1:			TranslatedKeyCode = VK_F1;			break;
	case Windows::System::VirtualKey::F2:			TranslatedKeyCode = VK_F2;			break;
	case Windows::System::VirtualKey::F3:			TranslatedKeyCode = VK_F3;			break;
	case Windows::System::VirtualKey::F4:			TranslatedKeyCode = VK_F4;			break;
	case Windows::System::VirtualKey::F5:			TranslatedKeyCode = VK_F5;			break;
	case Windows::System::VirtualKey::F6:			TranslatedKeyCode = VK_F6;			break;
	case Windows::System::VirtualKey::F7:			TranslatedKeyCode = VK_F7;			break;
	case Windows::System::VirtualKey::F8:			TranslatedKeyCode = VK_F8;			break;
	case Windows::System::VirtualKey::F9:			TranslatedKeyCode = VK_F9;			break;
	case Windows::System::VirtualKey::F10:			TranslatedKeyCode = VK_F10;			break;
	case Windows::System::VirtualKey::F11:			TranslatedKeyCode = VK_F11;			break;
	case Windows::System::VirtualKey::F12:			TranslatedKeyCode = VK_F12;			break;

	case Windows::System::VirtualKey::NumberKeyLock:TranslatedKeyCode = VK_NUMLOCK;		break;

	case Windows::System::VirtualKey::Scroll:		TranslatedKeyCode = VK_SCROLL;		break;

	case Windows::System::VirtualKey::LeftShift:	TranslatedKeyCode = VK_LSHIFT;		break;
	case Windows::System::VirtualKey::RightShift:	TranslatedKeyCode = VK_RSHIFT;		break;
	case Windows::System::VirtualKey::LeftControl:	TranslatedKeyCode = VK_LCONTROL;	break;
	case Windows::System::VirtualKey::RightControl:	TranslatedKeyCode = VK_RCONTROL;	break;
	case Windows::System::VirtualKey::LeftMenu:		TranslatedKeyCode = VK_LMENU;		break;
	case Windows::System::VirtualKey::RightMenu:	TranslatedKeyCode = VK_RMENU;		break;

	// For the US standard keyboard, only.
#pragma warning(push)
#pragma warning(disable:4063)
	case 0xBA:										TranslatedKeyCode = VK_OEM_1;		break;
	case 0xBB:										TranslatedKeyCode = VK_OEM_PLUS;	break;
	case 0xBC:										TranslatedKeyCode = VK_OEM_COMMA;	break;
	case 0xBD:										TranslatedKeyCode = VK_OEM_MINUS;	break;
	case 0xBE:										TranslatedKeyCode = VK_OEM_PERIOD;	break;
	case 0xBF:										TranslatedKeyCode = VK_OEM_2;		break;
	case 0xC0:										TranslatedKeyCode = VK_OEM_3;		break;
	case 0xDB:										TranslatedKeyCode = VK_OEM_4;		break;
	case 0xDC:										TranslatedKeyCode = VK_OEM_5;		break;
	case 0xDD:										TranslatedKeyCode = VK_OEM_6;		break;
	case 0xDE:										TranslatedKeyCode = VK_OEM_7;		break;
#pragma warning(pop)

	default:
		UE_LOG(LogLaunchHoloLens, Verbose, TEXT("Unrecognized keystroke VirtualKeyCode:%d  ScanCode:%d  Extended:%s  [%s]\n"),
			TranslatedKeyCode,
			keyEventArgs->KeyStatus.ScanCode,
			keyEventArgs->KeyStatus.IsExtendedKey ? TEXT("yes") : TEXT("no"),
			keyEventArgs->KeyStatus.WasKeyDown ? TEXT("up") : TEXT("down"));
	}
	return TranslatedKeyCode;
}


extern void appHoloLensKeyEvent(uint32 InCode, bool bInWasPressed);
extern void appHoloLensCharEvent(uint32 InCode, bool bInWasPressed);

void ViewProvider::OnKeyDown(
	_In_ CoreWindow^ sender,
	_In_ KeyEventArgs^ args 
	)
{
	uint32 Code = TranslateWinRTKey(args);
	if ((Code & 0xFFFFFF00) == 0)
	{
		appHoloLensKeyEvent(Code, true);
	}
	args->Handled = true;
}
void ViewProvider::OnKeyUp(
	_In_ CoreWindow^ sender,
	_In_ KeyEventArgs^ args 
	)
{
	uint32 Code = TranslateWinRTKey(args);
	if ((Code & 0xFFFFFF00) == 0)
	{
		appHoloLensKeyEvent(Code, false);
	}
	args->Handled = true;
}

void ViewProvider::OnAcceleratorKeyActivated(CoreDispatcher ^sender, AcceleratorKeyEventArgs ^args)
{
	if (args->VirtualKey == VirtualKey::Menu)
	{
		if (args->EventType == CoreAcceleratorKeyEventType::SystemKeyDown && args->KeyStatus.WasKeyDown == false)
		{
			appHoloLensKeyEvent(args->KeyStatus.IsExtendedKey ? VK_RMENU : VK_LMENU, true);
		}
		else if (args->EventType == CoreAcceleratorKeyEventType::SystemKeyUp && args->KeyStatus.WasKeyDown == true)
		{
			appHoloLensKeyEvent(args->KeyStatus.IsExtendedKey ? VK_RMENU : VK_LMENU, false);
		}
		args->Handled = true;
	}

	// Special case keycode for fullscreen
	else if (args->VirtualKey == VirtualKey::Enter
		&& args->EventType == CoreAcceleratorKeyEventType::SystemKeyDown
		&& !args->KeyStatus.WasKeyDown)
	{
		FHoloLensApplication* HoloLensApplication = FHoloLensApplication::GetHoloLensApplication();
		if (HoloLensApplication)
		{
			TSharedPtr< FHoloLensWindow > NativeWindow = HoloLensApplication->GetHoloLensWindow();
			if (NativeWindow.IsValid())
			{
				if (NativeWindow->GetWindowMode() == EWindowMode::Windowed)
				{
					NativeWindow->SetWindowMode(EWindowMode::WindowedFullscreen);
				}
				else
				{
					NativeWindow->SetWindowMode(EWindowMode::Windowed);
				}
				args->Handled = true;
			}
		}
	}
	else if (args->VirtualKey == VirtualKey::Control)
	{
		if ((args->EventType == CoreAcceleratorKeyEventType::SystemKeyDown || args->EventType == CoreAcceleratorKeyEventType::KeyDown) && args->KeyStatus.WasKeyDown == false)
		{
			appHoloLensKeyEvent(args->KeyStatus.IsExtendedKey ? VK_RCONTROL : VK_LCONTROL, true);
		}
		else if ((args->EventType == CoreAcceleratorKeyEventType::SystemKeyUp || args->EventType == CoreAcceleratorKeyEventType::KeyUp) && args->KeyStatus.WasKeyDown == true)
		{
			appHoloLensKeyEvent(args->KeyStatus.IsExtendedKey ? VK_RCONTROL : VK_LCONTROL, false);
		}
		args->Handled = true;
	}
	else if (args->VirtualKey == VirtualKey::Shift)
	{
		if ((args->EventType == CoreAcceleratorKeyEventType::SystemKeyDown || args->EventType == CoreAcceleratorKeyEventType::KeyDown) && args->KeyStatus.WasKeyDown == false)
		{
			appHoloLensKeyEvent(args->KeyStatus.ScanCode == 0x36 ? VK_RSHIFT : VK_LSHIFT, true);
		}
		else if ((args->EventType == CoreAcceleratorKeyEventType::SystemKeyUp || args->EventType == CoreAcceleratorKeyEventType::KeyUp) && args->KeyStatus.WasKeyDown == true)
		{
			appHoloLensKeyEvent(args->KeyStatus.ScanCode == 0x36 ? VK_RSHIFT : VK_LSHIFT, false);
		}
		args->Handled = true;
	}
}

void ViewProvider::OnCharacterReceived(
	_In_ CoreWindow^ sender,
	_In_ CharacterReceivedEventArgs^ args)
{
	if (args->KeyStatus.IsExtendedKey == false)
	{
		appHoloLensCharEvent(args->KeyCode, true);
	}
	args->Handled = true;
}

// @MIXEDREALITY_CHANGE : BEGIN Windows Mixed Reality support
//bool bIsActivationContextHolographic = false;
//
//bool appHoloLensIsActivationHolographic()
//{
//	return bIsActivationContextHolographic;
//}

void appHoloLensViewProviderKeyEvent(
	_In_ CoreWindow^ sender,
	_In_ KeyEventArgs^ args,
	bool down)
{
	uint32 Code = TranslateWinRTKey(args);
	if ((Code & 0xFFFFFF00) == 0)
	{
		appHoloLensKeyEvent(Code, down);
	}
	args->Handled = true;
}

void appHoloLensViewProviderCharacterReceived(
	_In_ CoreWindow^ sender,
	_In_ CharacterReceivedEventArgs^ args)
{
	if (args->KeyStatus.IsExtendedKey == false)
	{
		appHoloLensCharEvent(args->KeyCode, true);
	}
	args->Handled = true;
}

void appHoloLensViewProviderAcceleratorKeyActivated(CoreDispatcher ^sender, AcceleratorKeyEventArgs ^args)
{
	if (args->VirtualKey == VirtualKey::Menu)
	{
		if (args->EventType == CoreAcceleratorKeyEventType::SystemKeyDown && args->KeyStatus.WasKeyDown == false)
		{
			appHoloLensKeyEvent(args->KeyStatus.IsExtendedKey ? VK_RMENU : VK_LMENU, true);
		}
		else if (args->EventType == CoreAcceleratorKeyEventType::SystemKeyUp && args->KeyStatus.WasKeyDown == true)
		{
			appHoloLensKeyEvent(args->KeyStatus.IsExtendedKey ? VK_RMENU : VK_LMENU, false);
		}
		args->Handled = true;
	}

	// Special case keycode for fullscreen
	else if (args->VirtualKey == VirtualKey::Enter
		&& args->EventType == CoreAcceleratorKeyEventType::SystemKeyDown
		&& !args->KeyStatus.WasKeyDown)
	{
		FHoloLensApplication* HoloLensApplication = FHoloLensApplication::GetHoloLensApplication();
		if (HoloLensApplication)
		{
			TSharedPtr< FHoloLensWindow > NativeWindow = HoloLensApplication->GetHoloLensWindow();
			if (NativeWindow.IsValid())
			{
				if (NativeWindow->GetWindowMode() == EWindowMode::Windowed)
				{
					NativeWindow->SetWindowMode(EWindowMode::WindowedFullscreen);
				}
				else
				{
					NativeWindow->SetWindowMode(EWindowMode::Windowed);
				}
				args->Handled = true;
			}
		}
	}
	else if (args->VirtualKey == VirtualKey::Control)
	{
		if ((args->EventType == CoreAcceleratorKeyEventType::SystemKeyDown || args->EventType == CoreAcceleratorKeyEventType::KeyDown) && args->KeyStatus.WasKeyDown == false)
		{
			appHoloLensKeyEvent(args->KeyStatus.IsExtendedKey ? VK_RCONTROL : VK_LCONTROL, true);
		}
		else if ((args->EventType == CoreAcceleratorKeyEventType::SystemKeyUp || args->EventType == CoreAcceleratorKeyEventType::KeyUp) && args->KeyStatus.WasKeyDown == true)
		{
			appHoloLensKeyEvent(args->KeyStatus.IsExtendedKey ? VK_RCONTROL : VK_LCONTROL, false);
		}
		args->Handled = true;
	}
	else if (args->VirtualKey == VirtualKey::Shift)
	{
		if ((args->EventType == CoreAcceleratorKeyEventType::SystemKeyDown || args->EventType == CoreAcceleratorKeyEventType::KeyDown) && args->KeyStatus.WasKeyDown == false)
		{
			appHoloLensKeyEvent(args->KeyStatus.ScanCode == 0x36 ? VK_RSHIFT : VK_LSHIFT, true);
		}
		else if ((args->EventType == CoreAcceleratorKeyEventType::SystemKeyUp || args->EventType == CoreAcceleratorKeyEventType::KeyUp) && args->KeyStatus.WasKeyDown == true)
		{
			appHoloLensKeyEvent(args->KeyStatus.ScanCode == 0x36 ? VK_RSHIFT : VK_LSHIFT, false);
		}
		args->Handled = true;
	}
}
// @MIXEDREALITY_CHANGE : END Windows Mixed Reality support

void ViewProvider::OnVisibilityChanged(_In_ CoreWindow^ sender, _In_ VisibilityChangedEventArgs^ args)
{
	bVisible = args->Visible;
}

void ViewProvider::OnWindowSizeChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::WindowSizeChangedEventArgs^ args)
{
	float Dpi = static_cast<uint32_t>(Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->LogicalDpi);
	int32 Width = FHoloLensWindow::ConvertDipsToPixels(args->Size.Width, Dpi);
	int32 Height = FHoloLensWindow::ConvertDipsToPixels(args->Size.Height, Dpi);

	UE_LOG(LogLaunchHoloLens, Log, TEXT("Window Size Changed to [%d, %d]"), Width, Height);

	FHoloLensApplication* pApplication = FHoloLensApplication::GetHoloLensApplication();
	if (pApplication)
	{
		// Since we use the window size to define the virtual desktop size, we need to report display metrics changed here.
		FDisplayMetrics DisplayMetrics;
		FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
		pApplication->OnDisplayMetricsChanged().Broadcast(DisplayMetrics);

		TSharedPtr< FHoloLensWindow > pNativeWindow = pApplication->GetHoloLensWindow();
		if (pNativeWindow.IsValid())
		{
			pApplication->GetMessageHandler()->OnResizingWindow(pNativeWindow.ToSharedRef());
			pApplication->GetMessageHandler()->OnMovedWindow(pNativeWindow.ToSharedRef(), 0, 0);
			pApplication->GetMessageHandler()->OnSizeChanged(pNativeWindow.ToSharedRef(), Width, Height);
			pApplication->GetMessageHandler()->OnResizingWindow(pNativeWindow.ToSharedRef());
		}
	}
	IConsoleManager::Get().CallAllConsoleVariableSinks();
}

void ViewProvider::OnWindowClosed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::CoreWindowEventArgs^ args)
{
	bWindowClosed = true;
}

void ViewProvider::OnPointerPressed(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{
	ProcessMouseEvent(args);
}

void ViewProvider::OnPointerReleased(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{
	ProcessMouseEvent(args); 
}

// ATG - This event occurs for any pointer updates, not just for movement, so must be certain to translate appropraitely
void ViewProvider::OnPointerMoved(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{
	ProcessMouseEvent(args);
}

void ViewProvider::OnPointerWheelChanged(Windows::UI::Core::CoreWindow^ sender, Windows::UI::Core::PointerEventArgs^ args)
{
	ProcessMouseEvent(args);
}

void ViewProvider::ProcessEvents()
{
	static std::atomic_flag Dispatching = ATOMIC_FLAG_INIT;

	if (Dispatching.test_and_set( std::memory_order_acquire ))
	{
		return;		// prevent nesting
	}

	CoreWindow^ Window = CoreWindow::GetForCurrentThread();
	if (nullptr != Window)
	{
		CoreDispatcher^ Dispatcher = Window->Dispatcher;

		if (nullptr != Dispatcher)
		{
			// OpenXR uses a separate CoreWindow which will have visibility instead of the CoreWindow created by HoloLensLaunch and in this
			// case ProcessOneAndAllPending cannot be used because it will block indefinitely once there are no more events to process.
			if (bVisible || GSeparateCoreWindowVisible)
			{
				Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
			}
			else
			{
				Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessOneAndAllPending);
			}
		}
	}

	TArray<QueuedPointerEvent> LocalPointerEvents(MoveTemp(PointerEventQueue));
	PointerEventQueue.Empty();
	Dispatching.clear(std::memory_order_release);

	if (!IsEngineExitRequested())
	{
		for (const QueuedPointerEvent& Args : LocalPointerEvents)
		{
			ProcessMouseEvent(Args);
		}
	}
}

void appWinPumpMessages()
{
	GViewProvider->ProcessEvents();
}

void ViewProvider::Initialize(Windows::ApplicationModel::Core::CoreApplicationView^ applicationView)
{
	applicationView->Activated += ref new Windows::Foundation::TypedEventHandler< CoreApplicationView^, IActivatedEventArgs^ >( this, &ViewProvider::OnActivated );
	CoreApplication::Suspending += ref new Windows::Foundation::EventHandler< Windows::ApplicationModel::SuspendingEventArgs^ >( this, &ViewProvider::OnSuspending );
	CoreApplication::Resuming += ref new Windows::Foundation::EventHandler< Platform::Object^>( this, & ViewProvider::OnResuming );
	CoreApplication::EnteredBackground += ref new Windows::Foundation::EventHandler< Windows::ApplicationModel::EnteredBackgroundEventArgs^ >(this, &ViewProvider::OnEnteredBackground);
	CoreApplication::LeavingBackground += ref new Windows::Foundation::EventHandler< Windows::ApplicationModel::LeavingBackgroundEventArgs^ >(this, &ViewProvider::OnLeavingBackground);
	CoreApplication::Exiting += ref new Windows::Foundation::EventHandler< Platform::Object^>(this, &ViewProvider::OnExiting);

}

static void InitCommandLine(Platform::String^ args)
{
	// check for a uecommandline.txt.
	FCommandLine::Set(L"");

	// FPaths should be good at this point (FPaths::EngineDir() is used during static init for instance)
	// so be specific about the location we want, otherwise working directory changes can mess this up.
	// Also this means the regular file interface is in place, so let's use that.
	FString FileName = FString(FPaths::RootDir()) / TEXT("UECommandLine.txt");

	IFileHandle* CmdLineFileHandle = IPlatformFile::GetPlatformPhysical().OpenRead(*FileName);
	if (CmdLineFileHandle != nullptr)
	{
		const int64 MaxCmdLineSize = 65536;

		int64 CmdLineFileSize = CmdLineFileHandle->Size();
		if (CmdLineFileSize < MaxCmdLineSize - 1)
		{
			char AnsiCmdLine[MaxCmdLineSize] = {};
			if (CmdLineFileHandle->Read(reinterpret_cast<uint8*>(AnsiCmdLine), CmdLineFileSize))
			{
				FString CmdLine = StringCast<TCHAR>(AnsiCmdLine).Get();
				CmdLine.TrimStartAndEndInline();
				
				if (args != nullptr)
				{
					CmdLine += FString(" ");
					CmdLine += FString(args->Data());

					CmdLine.TrimStartAndEndInline();
				}

				UE_LOG(LogLaunchHoloLens, Log, TEXT("%s"), *CmdLine);
				FCommandLine::Set(*CmdLine);
			}
			else
			{
				UE_LOG(LogLaunchHoloLens, Warning, TEXT("Failed to read commandline from %s!"), *FileName);
			}
		}
		else
		{
			UE_LOG(LogLaunchHoloLens, Warning, TEXT("Commandline file %s too large (%d / %d bytes).  Ignoring."), *FileName, CmdLineFileSize, MaxCmdLineSize);
		}
		delete CmdLineFileHandle;
	}
}

void ViewProvider::OnActivated(_In_ Windows::ApplicationModel::Core::CoreApplicationView^ applicationView, _In_ Windows::ApplicationModel::Activation::IActivatedEventArgs^ args)
{
	// Check for launch activation
	Platform::String^ arg = nullptr;
	if (args->Kind == ActivationKind::Launch)
	{
		auto launchArgs = static_cast<LaunchActivatedEventArgs^>(args);
		arg = launchArgs->Arguments;
	}

	// Check for protocol activation
	if (args->Kind == ActivationKind::Protocol)
	{
		auto protocolArgs = static_cast<ProtocolActivatedEventArgs^>(args);
		arg = protocolArgs->Uri->ToString();
	}

	InitCommandLine(arg);
	
	// Check commandline for start in VR.
	if (FParse::Param(FCommandLine::Get(), TEXT("vr")))
	{
		auto HoloSpace = HolographicSpace::CreateForCoreWindow(CoreWindow::GetForCurrentThread());
		FHoloLensApplication::SetHolographicSpace(HoloSpace);
	}
	else
	{
		UE_LOG(LogHoloLens, Warning, TEXT("Application will launch as a 2D slate.  If this is unexpected, add a -vr command line flag or enable the start in VR flag."));
	}

	// Take advantage of this opportunity to measure the desktop (based on view bounds before window activation).
	FHoloLensApplication::CacheDesktopSize();

	// Activate the window
	CoreWindow^ window = CoreWindow::GetForCurrentThread();
	window->Activate();
    ActivationComplete = true;

	// Query this to find out if the application was shut down gracefully last time
	if (args != nullptr)
	{
		if(args->Kind == ActivationKind::Launch)
		{
			LaunchActivatedEventArgs ^LaunchArgs = (LaunchActivatedEventArgs^)args;
			if (!LaunchArgs->Arguments->IsEmpty())
			{
				FCommandLine::Set(LaunchArgs->Arguments->Data());
			}
		}
		else if(args->Kind == Windows::ApplicationModel::Activation::ActivationKind::Protocol)
		{
			// If the game is launched from an invitation, the ActivationKind will be Protocol
			// so we need to initialize the command line on this path also. Since it's possible
			// to get a protocol activation after the game is already running as well, only set
			// the command line if it's not initialized yet.
			if ( !FCommandLine::IsInitialized() )
			{
				FCommandLine::Set( TEXT("") );
			}

			// Also save the actual protocol activation URI, for other parts of the engine to
			// access if necessary once they're initialized.
			// This may overwrite an existing URI if the game is activated multiple times,
			// but we assume only the most recent activation is relevant.
			ProtocolActivatedEventArgs^ ProtocolArgs = static_cast<ProtocolActivatedEventArgs^>(args);
			FPlatformMisc::SetProtocolActivationUri(ProtocolArgs->Uri->RawUri->Data());
		}

		existingSize = window->Bounds;
		ApplicationExecutionState lastState = args->PreviousExecutionState;
		UE_LOG(LogLaunchHoloLens, Log, TEXT("OnActivated (%5.0fx%5.0f): Last application state: %d"), existingSize.Width, existingSize.Height, static_cast<uint32>(lastState));
	}
}

void ViewProvider::OnResuming(_In_ Platform::Object^ Sender, _In_ Platform::Object^ Args)
{
	// Make the call down to the RHI to Resume the GPU state
	RHIResumeRendering();

	// Notify application of resume
	FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
}

void ViewProvider::OnSuspending(_In_ Platform::Object^ Sender, _In_ Windows::ApplicationModel::SuspendingEventArgs^ Args)
{
	// Get the Suspending Event
	Windows::ApplicationModel::SuspendingDeferral^ SuspendingEvent = Args->SuspendingOperation->GetDeferral();

	// Notify application of suspend. Application should kick off an async save at this point.
	FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();

	// Flush the RenderingThread
	FlushRenderingCommands();

	// Make the call down to the RHI to Suspend the GPU state
	if (GDynamicRHI != nullptr)
	{
		RHISuspendRendering();
	}

	// @TODO Wait for async save to complete
	// Flush the log so it's all written to disk
	GLog->FlushThreadedLogs();
	GLog->Flush();

	// Tell the callback that we are done
	SuspendingEvent->Complete();
}

void ViewProvider::OnEnteredBackground(_In_ Platform::Object^ sender, _In_ Windows::ApplicationModel::EnteredBackgroundEventArgs^ args)
{
	if (!GIsRunning)
	{
		return;
	}

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
}

void ViewProvider::OnLeavingBackground(_In_ Platform::Object^ sender, _In_ Windows::ApplicationModel::LeavingBackgroundEventArgs^ args)
{
	if (!GIsRunning)
	{
		return;
	}

	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
}

void ViewProvider::OnExiting(_In_ Platform::Object^ sender, _In_ Platform::Object^ args)
{
	FCoreDelegates::ApplicationWillTerminateDelegate.Broadcast();
}


const TCHAR* ViewProvider::GetPointerUpdateKindString(Windows::UI::Input::PointerUpdateKind InKind)
{
	switch (InKind)
	{
	case Windows::UI::Input::PointerUpdateKind::Other:
		return TEXT("Other");
	case Windows::UI::Input::PointerUpdateKind::LeftButtonPressed:
		return TEXT("LeftButtonPressed");
	case Windows::UI::Input::PointerUpdateKind::LeftButtonReleased:
		return TEXT("LeftButtonReleased");
	case Windows::UI::Input::PointerUpdateKind::RightButtonPressed:
		return TEXT("RightButtonPressed");
	case Windows::UI::Input::PointerUpdateKind::RightButtonReleased:
		return TEXT("RightButtonReleased");
	case Windows::UI::Input::PointerUpdateKind::MiddleButtonPressed:
		return TEXT("MiddleButtonPressed");
	case Windows::UI::Input::PointerUpdateKind::MiddleButtonReleased:
		return TEXT("MiddleButtonReleased");
	case Windows::UI::Input::PointerUpdateKind::XButton1Pressed:
		return TEXT("XButton1Pressed");
	case Windows::UI::Input::PointerUpdateKind::XButton1Released:
		return TEXT("XButton1Released");
	case Windows::UI::Input::PointerUpdateKind::XButton2Pressed:
		return TEXT("XButton2Pressed");
	case Windows::UI::Input::PointerUpdateKind::XButton2Released:
		return TEXT("XButton2Released");
	}

	return TEXT("*** UNKNOWN ***");
}


EMouseButtons::Type ViewProvider::PointerUpdateKindToUEKey(Windows::UI::Input::PointerUpdateKind InKind, bool& bWasPressed)
{
	switch (InKind)
	{
	case Windows::UI::Input::PointerUpdateKind::Other:
		bWasPressed = false; ////
		return EMouseButtons::Invalid; ////
	case Windows::UI::Input::PointerUpdateKind::LeftButtonPressed:
		bWasPressed = true;
		return EMouseButtons::Left;
	case Windows::UI::Input::PointerUpdateKind::LeftButtonReleased:
		bWasPressed = false;
		return EMouseButtons::Left;
	case Windows::UI::Input::PointerUpdateKind::RightButtonPressed:
		bWasPressed = true;
		return EMouseButtons::Right;
	case Windows::UI::Input::PointerUpdateKind::RightButtonReleased:
		bWasPressed = false;
		return EMouseButtons::Right;
	case Windows::UI::Input::PointerUpdateKind::MiddleButtonPressed:
		bWasPressed = true;
		return EMouseButtons::Middle;
	case Windows::UI::Input::PointerUpdateKind::MiddleButtonReleased:
		bWasPressed = false;
		return EMouseButtons::Middle;
	case Windows::UI::Input::PointerUpdateKind::XButton1Pressed:
		bWasPressed = true;
		return EMouseButtons::Thumb01;
	case Windows::UI::Input::PointerUpdateKind::XButton1Released:
		bWasPressed = false;
		return EMouseButtons::Thumb01;
	case Windows::UI::Input::PointerUpdateKind::XButton2Pressed:
		bWasPressed = true;
		return EMouseButtons::Thumb02;
	case Windows::UI::Input::PointerUpdateKind::XButton2Released:
		bWasPressed = false;
		return EMouseButtons::Thumb02;
	}

	return EMouseButtons::Invalid; ////
}

bool ViewProvider::ProcessMouseEvent(Windows::UI::Core::PointerEventArgs^ args)
{
	Windows::UI::Input::PointerPoint^ Point = args->CurrentPoint;
	Windows::UI::Input::PointerUpdateKind Kind = Point->Properties->PointerUpdateKind;
	Windows::System::VirtualKeyModifiers KeyModifiers = args->KeyModifiers;
	Windows::Foundation::Collections::IVector<Windows::UI::Input::PointerPoint^>^ intermediatePoints = args->GetIntermediatePoints();

	UE_LOG(LogLaunchHoloLens, Verbose, TEXT("ProcessMouseEvent %d = %5.2f, %5.2f - %s"),
		intermediatePoints->Size, Point->Position.X, Point->Position.Y, GetPointerUpdateKindString(Kind));

	PointerEventQueue.Add(QueuedPointerEvent(args));

	return true;
}

bool ViewProvider::ProcessMouseEvent(const QueuedPointerEvent& Event)
{
	FHoloLensApplication* const Application = FHoloLensApplication::GetHoloLensApplication();

	if (Application != NULL)
	{
		// process a cursor move unless we're using raw mouse deltas and a virtual cursor
		if (!Application->GetCursor()->IsUsingRawMouseNoCursor())
		{
			FVector2D CurrentCursorPosition = Event.RawPosition;
			float Dpi = static_cast<uint32_t>(Windows::Graphics::Display::DisplayInformation::GetForCurrentView()->LogicalDpi);
			CurrentCursorPosition.X = FHoloLensWindow::ConvertDipsToPixels(CurrentCursorPosition.X, Dpi);
			CurrentCursorPosition.Y = FHoloLensWindow::ConvertDipsToPixels(CurrentCursorPosition.Y, Dpi);

			Application->GetCursor()->UpdatePosition(CurrentCursorPosition);
			Application->GetMessageHandler()->OnMouseMove();
			Application->GetMessageHandler()->OnCursorSet();
		}
		// process a button event if we know this update isn't just for movement
		if (Event.Kind != Windows::UI::Input::PointerUpdateKind::Other)
		{
			bool bPressed = false;
			EMouseButtons::Type MouseButton = PointerUpdateKindToUEKey(Event.Kind, bPressed);
			if (MouseButton != EMouseButtons::Type::Invalid)
			{
				if (bPressed == true)
				{
					Application->GetMessageHandler()->OnMouseDown(NULL, MouseButton);
				}
				else
				{
					Application->GetMessageHandler()->OnMouseUp(MouseButton);
				}
			}
		}

		if (Event.WheelDelta != 0)
		{
			Application->GetMessageHandler()->OnMouseWheel(Event.WheelDelta / static_cast<float>(WHEEL_DELTA));
		}
	}

	return true;
}



void ViewProvider::SetWindow(Windows::UI::Core::CoreWindow^ window)
{
	window->KeyDown += ref new Windows::Foundation::TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &ViewProvider::OnKeyDown);
	window->KeyUp += ref new Windows::Foundation::TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &ViewProvider::OnKeyUp);
	window->CharacterReceived += ref new Windows::Foundation::TypedEventHandler<CoreWindow^, CharacterReceivedEventArgs^>(this, &ViewProvider::OnCharacterReceived);
	window->VisibilityChanged += ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &ViewProvider::OnVisibilityChanged);
	window->SizeChanged += ref new TypedEventHandler<CoreWindow^, WindowSizeChangedEventArgs^>(this, &ViewProvider::OnWindowSizeChanged);
	window->Closed += ref new TypedEventHandler<CoreWindow^, CoreWindowEventArgs^>(this, &ViewProvider::OnWindowClosed);
	window->PointerCursor = ref new CoreCursor(CoreCursorType::Arrow, 0);
	window->PointerPressed += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &ViewProvider::OnPointerPressed);
	window->PointerReleased += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &ViewProvider::OnPointerReleased);
	window->PointerMoved += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &ViewProvider::OnPointerMoved);
	window->PointerWheelChanged += ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &ViewProvider::OnPointerWheelChanged);
	window->Dispatcher->AcceleratorKeyActivated += ref new TypedEventHandler<CoreDispatcher ^, AcceleratorKeyEventArgs ^>(this, &ViewProvider::OnAcceleratorKeyActivated);
}

// this method is called after Initialize
void ViewProvider::Load(Platform::String^ /*entryPoint*/)
{
}

void ViewProvider::EnsureDlcPluginsAreMounted()
{
	IPluginManager::Get().RefreshPluginsList();
	TArray<TSharedRef<IPlugin>> DiscoveredPlugins = IPluginManager::Get().GetDiscoveredPlugins();
	for (TSharedRef<IPlugin>& Plugin : DiscoveredPlugins)
	{
		if (Plugin->GetType() == EPluginType::External && !Plugin->IsEnabled())
		{
			IPluginManager::Get().MountNewlyCreatedPlugin(Plugin->GetName());
		}
	}
}

void ViewProvider::RegisterDlcPluginsForOptionalPackage(Windows::ApplicationModel::Package^ DependencyPackage)
{
#if WIN10_SDK_VERSION >= 14393
	check(Windows::Foundation::Metadata::ApiInformation::IsPropertyPresent("Windows.ApplicationModel.Package", "IsOptional"));
	// Optional dependencies are the mechanism for DLC on HoloLens
	if (DependencyPackage->IsOptional)
	{
		bool ShouldLoad = true;
		if (DependencyPackage->Status->NeedsRemediation)
		{
			UE_LOG(LogLaunchHoloLens, Warning, TEXT("Optional package %s has NeedsRemediation set and will not be loaded."), DependencyPackage->Id->FullName->Data());
			ShouldLoad = false;
		}
		else if (DependencyPackage->Status->NotAvailable)
		{
			UE_LOG(LogLaunchHoloLens, Warning, TEXT("Optional package %s has NotAvailable set and will not be loaded."), DependencyPackage->Id->FullName->Data());
			ShouldLoad = false;
		}

		if (ShouldLoad)
		{
			IPluginManager::Get().AddPluginSearchPath(FString(DependencyPackage->InstalledLocation->Path->Data()), false);
		}
	}
#endif
}

void ViewProvider::InitOptionalPackages()
{
#if WIN10_SDK_VERSION >= 14393
	if (Windows::Foundation::Metadata::ApiInformation::IsTypePresent("Windows.ApplicationModel.PackageCatalog"))
	{
		using namespace Windows::Foundation;
		using namespace Windows::Foundation::Collections;
		using namespace Windows::ApplicationModel;

		IVectorView<Package^>^ DependencyPackages = Package::Current->Dependencies;
		for (Package^ DependencyPackage : DependencyPackages)
		{
			RegisterDlcPluginsForOptionalPackage(DependencyPackage);
		}
		EnsureDlcPluginsAreMounted();

		static PackageCatalog^ Catalog = PackageCatalog::OpenForCurrentPackage();
		Catalog->PackageInstalling += ref new TypedEventHandler<PackageCatalog^, PackageInstallingEventArgs^>(
			[](PackageCatalog^, PackageInstallingEventArgs^ Args)
		{
			if (Args->IsComplete)
			{
				RegisterDlcPluginsForOptionalPackage(Args->Package);
				EnsureDlcPluginsAreMounted();
			}
		});

		Catalog->PackageStatusChanged += ref new TypedEventHandler<PackageCatalog^, PackageStatusChangedEventArgs^>(
			[](PackageCatalog^, PackageStatusChangedEventArgs^ Args)
		{
			RegisterDlcPluginsForOptionalPackage(Args->Package);
			EnsureDlcPluginsAreMounted();
		});
	}
	else
	{
		UE_LOG(LogLaunchHoloLens, Warning, TEXT("Optional packages (DLC) require a runtime Windows version of 14393 or higher."));
	}
#else
	UE_LOG(LogLaunchHoloLens, Warning, TEXT("Optional packages (DLC) are not available with this Windows SDK version (%d).  Use the 14393 SDK or later."), WIN10_SDK_VERSION);
#endif
}

#include "Microsoft/AllowMicrosoftPlatformAtomics.h"
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#include "NetworkMessage.h"

// this method is called after Load
void ViewProvider::Run()
{
	FTaskTagScope Scope(ETaskTag::EGameThread);

	// Wait for activation to complete
	while (!ActivationComplete)
	{
		ProcessEvents();
	}

	appHoloLensEarlyInit();

	// command line fetch moved to earlier in startup, in a background task

	// Parse the basedir and check for the 'clearbasedir' option
	const TCHAR* BaseDirectory = FPlatformProcess::BaseDir();
	if (FParse::Param(FCommandLine::Get(), TEXT("clearbasedir")))
	{
		// An actual path was passed in... check it
		WIN32_FILE_ATTRIBUTE_DATA AttribData;
		if (GetFileAttributesEx(BaseDirectory, GetFileExInfoStandard, (void*)&AttribData) == TRUE)
		{
			if ((AttribData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
			{
				// Deleting the directory does not appear to work.
				// So delete the files one by one
				UE_LOG(LogLaunchHoloLens, Warning, TEXT("WARNING: Requested to clear base directory: %s"), BaseDirectory);
				IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
				TArray<FString> DirectoriesToSkip;
				TArray<FString> DirectoriesToNotRecurse;
				FLocalTimestampDirectoryVisitor Visitor(PlatformFile, DirectoriesToSkip, DirectoriesToNotRecurse, false);
				PlatformFile.IterateDirectory(BaseDirectory, Visitor);
				if (Visitor.FileTimes.Num() > 0)
				{
					for (TMap<FString,FDateTime>::TIterator FileIt(Visitor.FileTimes); FileIt; ++FileIt)
					{
						DeleteFileW(*(FileIt.Key()));
					}
				}
			}
		}
	}

	GuardedMain(FCommandLine::Get(), NULL, NULL, 0 );

	Windows::ApplicationModel::Core::CoreApplication::Exit();
}
#include "Microsoft/HideMicrosoftPlatformTypes.h"
#include "Microsoft/HideMicrosoftPlatformAtomics.h"

void ViewProvider::Uninitialize()
{
}

[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
	uint64 GameThreadAffinity = FPlatformAffinity::GetMainGameMask();
	FPlatformProcess::SetThreadAffinityMask((DWORD_PTR)GameThreadAffinity);

	auto viewProviderFactory = ref new ViewProviderFactory();
	Windows::ApplicationModel::Core::CoreApplication::Run(viewProviderFactory);
}

// The following functions are also located in Launch.cpp in a #if _WINDOWS block...
/**
 * Handler for CRT parameter validation. Triggers error
 *
 * @param Expression - the expression that failed crt validation
 * @param Function - function which failed crt validation
 * @param File - file where failure occured
 * @param Line - line number of failure
 * @param Reserved - not used
 */
void InvalidParameterHandler(const TCHAR* Expression,
							 const TCHAR* Function, 
							 const TCHAR* File, 
							 uint32 Line, 
							 uintptr_t Reserved)
{
	UE_LOG(LogLaunchHoloLens, Fatal,
		TEXT("SECURE CRT: Invalid parameter detected.\nExpression: %s Function: %s. File: %s Line: %d\n"), 
		Expression ? Expression : TEXT("Unknown"), 
		Function ? Function : TEXT("Unknown"), 
		File ? File : TEXT("Unknown"), 
		Line );
}

/**
 * Setup the common debug settings 
 */
void SetupWindowsEnvironment( void )
{
	// all crt validation should trigger the callback
	_set_invalid_parameter_handler(InvalidParameterHandler);
}

void appHoloLensEarlyInit()
{
	// Setup common Windows settings
	SetupWindowsEnvironment();

	int32 ErrorLevel			= 0;
}

/** The global EngineLoop instance */
FEngineLoop	GEngineLoop;
bool GIsConsoleExecutable = false;

/** 
 * PreInits the engine loop 
 */
int32 EnginePreInit( const TCHAR* CmdLine )
{
	int32 ErrorLevel = GEngineLoop.PreInit(CmdLine);

	return(ErrorLevel);
}

/** 
 * Inits the engine loop 
 */
int32 EngineInit( void )
{
	GViewProvider->InitOptionalPackages();
	return GEngineLoop.Init();
}

/** 
 * Ticks the engine loop 
 */
void EngineTick( void )
{
	GEngineLoop.Tick();
}

/**
 * Shuts down the engine
 */
void EngineExit( void )
{
	// Make sure this is set
	RequestEngineExit(TEXT("EngineExit() was called"));
	GEngineLoop.Exit();
}

/**
 * Static guarded main function. Rolled into own function so we can have error handling for debug/ release builds depending
 * on whether a debugger is attached or not.
 */
int32 GuardedMain( const TCHAR* CmdLine, HINSTANCE hInInstance, HINSTANCE hPrevInstance, int32 nCmdShow )
{
#if UE_BUILD_DEBUG
	UE_LOG(LogLaunchHoloLens, Log, TEXT("--- Running a DEBUG build ---"));
#elif UE_BUILD_DEVELOPMENT
	UE_LOG(LogLaunchHoloLens, Log, TEXT("--- Running a DEVELOPMENT build ---"));
#elif UE_BUILD_TEST
	UE_LOG(LogLaunchHoloLens, Log, TEXT("--- Running a TEST build ---"));
#elif UE_BUILD_SHIPPING
	UE_LOG(LogLaunchHoloLens, Log, TEXT("--- Running a SHIPPING build ---"));
#else
	UE_LOG(LogLaunchHoloLens, Log, TEXT("--- Running a ***UNKNOWN*** build ---"));
#endif

	// make sure GEngineLoop::Exit() is always called.
	struct EngineLoopCleanupGuard 
	{ 
		~EngineLoopCleanupGuard()
		{
			EngineExit();
		}
	} CleanupGuard;

	// Set up minidump filename. We cannot do this directly inside main as we use an FString that requires 
	// destruction and main uses SEH.
	// These names will be updated as soon as the Filemanager is set up so we can write to the log file.
	// That will also use the user folder for installed builds so we don't write into program files or whatever.
	FCString::Strcpy(MiniDumpFilenameW, *FString::Printf( TEXT("unreal-v%i-%s.dmp"), FEngineVersion::Current().GetChangelist(), *FDateTime::Now().ToString()));

	GViewProvider->ProcessEvents();

	// Call PreInit and exit if failed.
	int32 ErrorLevel = EnginePreInit(CmdLine);
	if ((ErrorLevel != 0) || IsEngineExitRequested())
	{
		return ErrorLevel;
	}

	GViewProvider->ProcessEvents();

	// Game without wxWindows.
	ErrorLevel = EngineInit();

	static const FName OpenXRSystemName(TEXT("OpenXR"));
	GSeparateCoreWindowVisible = GEngine->XRSystem.IsValid() && (GEngine->XRSystem->GetSystemName() == OpenXRSystemName);

	uint64 GameThreadAffinity = FPlatformAffinity::GetMainGameMask();
	FPlatformProcess::SetThreadAffinityMask((DWORD_PTR)GameThreadAffinity);

	while (!IsEngineExitRequested())
	{
		// ProcessEvents will throw if the process is exiting, allowing us to
		// break out of the loop.  This will be cleaned up when we get proper
		// process lifetime management online in a future release.
		GViewProvider->ProcessEvents();
        FHoloLensApplication::GetHoloLensApplication()->GetCursor()->ProcessDeferredActions();

		EngineTick();
	}
	return ErrorLevel;
}
