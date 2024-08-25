// Copyright Epic Games, Inc. All Rights Reserved.

#include "Linux/LinuxApplication.h"
#include "Null/NullApplication.h"
#include "HAL/PlatformTime.h"
#include "Misc/StringUtility.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "Features/IModularFeatures.h"
#include "Linux/LinuxPlatformApplicationMisc.h"
#include "Null/NullPlatformApplicationMisc.h"
#include "IInputDeviceModule.h"
#include "IHapticDevice.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"

//
// GameController thresholds
//
#define GAMECONTROLLER_LEFT_THUMB_DEADZONE  7849
#define GAMECONTROLLER_RIGHT_THUMB_DEADZONE 8689
#define GAMECONTROLLER_TRIGGER_THRESHOLD    30

namespace
{
	// How long we wait from a FocusOut event to deactivate the application (100ms default)
	double DeactivationThreadshold = 0.1;

	// Mask away the higher bits (but preserve negative flags), as SDL uses those as flags
	// See: SDL_WINDOWPOS_UNDEFINED_MASK & SDL_WINDOWPOS_CENTERED_MASK for context
	int MaskAwayHigherBits(int Input)
	{
		if (Input < 0)
		{
			return -(-Input & 0xFFFF);
		}

		return Input & 0xFFFF;
	}
}

float ShortToNormalFloat(short AxisVal)
{
	// normalize [-32768..32767] -> [-1..1]
	const float Norm = (AxisVal <= 0 ? 32768.f : 32767.f);
	return float(AxisVal) / Norm;
}

namespace UE::Input
{
	/** A helper method for converting an Input Device id to an int32 */
	static int32 ConvertInputDeviceToInt(FInputDeviceId DeviceId)
	{
		int32 UserStateControllerId = INDEX_NONE;
		IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();

		FPlatformUserId UserId = Mapper.GetUserForInputDevice(DeviceId);
		Mapper.RemapUserAndDeviceToControllerId(UserId, UserStateControllerId, DeviceId);

		return UserStateControllerId;
	}
}

FLinuxApplication* LinuxApplication = NULL;

FLinuxApplication* FLinuxApplication::CreateLinuxApplication()
{
	if (!FApp::CanEverRender())	// this assumes that we're running in "headless" mode, and we don't need any kind of multimedia
	{
		return new FLinuxApplication();
	}

	if (!FLinuxPlatformApplicationMisc::InitSDL()) //	will not initialize more than once
	{
		UE_LOG(LogInit, Error, TEXT("FLinuxApplication::CreateLinuxApplication() : InitSDL() failed, cannot create application instance."));
		FLinuxPlatformMisc::RequestExitWithStatus(true, 1);
		// unreachable
		return nullptr;
	}

#if DO_CHECK
	uint32 InitializedSubsystems = SDL_WasInit(SDL_INIT_EVERYTHING);
	check(InitializedSubsystems & SDL_INIT_EVENTS);
	check(InitializedSubsystems & SDL_INIT_JOYSTICK);
	check(InitializedSubsystems & SDL_INIT_GAMECONTROLLER);
 	check(InitializedSubsystems & SDL_INIT_HAPTIC);
#endif // DO_CHECK

	LinuxApplication = new FLinuxApplication();

	for (int i = 0; i < SDL_NumJoysticks(); ++i)
	{
		if (SDL_IsGameController(i))
		{
 			LinuxApplication->AddGameController(i);
		}
	}

	return LinuxApplication;
}


FLinuxApplication::FLinuxApplication() 
	:	GenericApplication( MakeShareable( new FLinuxCursor() ) )
	,	bIsMouseCursorLocked(false)
	,	bIsMouseCaptureEnabled(false)
	,	bHasLoadedInputPlugins(false)
	,	bInsideOwnWindow(false)
	,	bIsDragWindowButtonPressed(false)
	,	bActivateApp(false)
	,	FocusOutDeactivationTime(0.0)
	,	LastTimeCachedDisplays(-1.0)
{
	bUsingHighPrecisionMouseInput = false;
	bAllowedToDeferMessageProcessing = true;
	MouseCaptureWindow = NULL;

	fMouseWheelScrollAccel = 1.0f;
	if (GConfig)
	{
		GConfig->GetFloat(TEXT("X11.Tweaks"), TEXT( "MouseWheelScrollAcceleration" ), fMouseWheelScrollAccel, GEngineIni);
	}
}

FLinuxApplication::~FLinuxApplication()
{
	if ( GConfig )
	{
		GConfig->GetFloat(TEXT("X11.Tweaks"), TEXT("MouseWheelScrollAcceleration"), fMouseWheelScrollAccel, GEngineIni);
		GConfig->Flush(false, GEngineIni);
	}
}

void FLinuxApplication::DestroyApplication()
{
	for(auto ControllerIt = ControllerStates.CreateConstIterator(); ControllerIt; ++ControllerIt)
	{
		if(ControllerIt.Value().Controller != nullptr)
		{
			SDL_GameControllerClose(ControllerIt.Value().Controller);
		}

		if(ControllerIt.Value().Haptic != nullptr)
		{
			SDL_HapticClose(ControllerIt.Value().Haptic);
		}
	}
	ControllerStates.Empty();
}

TSharedRef< FGenericWindow > FLinuxApplication::MakeWindow()
{
	return FLinuxWindow::Make();
}

void FLinuxApplication::InitializeWindow(	const TSharedRef< FGenericWindow >& InWindow,
											const TSharedRef< FGenericWindowDefinition >& InDefinition,
											const TSharedPtr< FGenericWindow >& InParent,
											const bool bShowImmediately )
{
	const TSharedRef< FLinuxWindow > Window = StaticCastSharedRef< FLinuxWindow >( InWindow );
	const TSharedPtr< FLinuxWindow > ParentWindow = StaticCastSharedPtr< FLinuxWindow >( InParent );

	Window->Initialize( this, InDefinition, ParentWindow, bShowImmediately );
	Windows.Add(Window);

	// Add the windows into the focus stack.
	if (Window->IsFocusWhenFirstShown())
	{
		RevertFocusStack.Add(Window);
	}

	// Add the window into the notification list if it is a notification window.
	if(Window->IsNotificationWindow())
	{
		NotificationWindows.Add(Window);
	}
}

void FLinuxApplication::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	GenericApplication::SetMessageHandler(InMessageHandler);
}

namespace
{
	TSharedPtr< FLinuxWindow > FindWindowBySDLWindow(const TArray< TSharedRef< FLinuxWindow > >& WindowsToSearch, SDL_HWindow const WindowHandle)
	{
		for (int32 WindowIndex=0; WindowIndex < WindowsToSearch.Num(); ++WindowIndex)
		{
			TSharedRef< FLinuxWindow > Window = WindowsToSearch[WindowIndex];
			if (Window->GetHWnd() == WindowHandle)
			{
				return Window;
			}
		}

		return TSharedPtr< FLinuxWindow >(nullptr);
	}
}

TSharedPtr< FLinuxWindow > FLinuxApplication::FindWindowBySDLWindow(SDL_Window *win)
{
	return ::FindWindowBySDLWindow(Windows, win);
}

void FLinuxApplication::PumpMessages( const float TimeDelta )
{
	FPlatformApplicationMisc::PumpMessages( true );
}

bool FLinuxApplication::IsCursorDirectlyOverSlateWindow() const
{
	return bInsideOwnWindow;
}

TSharedPtr<FGenericWindow> FLinuxApplication::GetWindowUnderCursor()
{
	// If we are drag and dropping the current window under the cursor will always be the DnD window
	// fallback to the SlateApplication finding the top most in DnD situations
	if (CurrentUnderCursorWindow.IsValid() && CurrentUnderCursorWindow->IsDragAndDropWindow())
	{
		return nullptr;
	}

	return CurrentUnderCursorWindow;
}

void FLinuxApplication::AddPendingEvent( SDL_Event SDLEvent )
{
	if( GPumpingMessagesOutsideOfMainLoop && bAllowedToDeferMessageProcessing )
	{
		PendingEvents.Add( SDLEvent );
	}
	else
	{
		// When not deferring messages, process them immediately
		ProcessDeferredMessage( SDLEvent );
	}
}

bool FLinuxApplication::GeneratesKeyCharMessage(const SDL_KeyboardEvent & KeyDownEvent)
{
	bool bCmdKeyPressed = (KeyDownEvent.keysym.mod & (KMOD_LCTRL | KMOD_RCTRL)) != 0;
	const SDL_Keycode Sym = KeyDownEvent.keysym.sym;

	// filter out command keys, non-ASCI and arrow keycodes that don't generate WM_CHAR under Windows (TODO: find a table?)
	return !bCmdKeyPressed && Sym < 128 &&
		(Sym != SDLK_DOWN && Sym != SDLK_LEFT && Sym != SDLK_RIGHT && Sym != SDLK_UP && Sym != SDLK_DELETE);
}

// Windows handles translating numpad numbers to arrow keys, but we have to do it manually
static SDL_Keycode TranslateNumLockKeySyms(const SDL_Keycode KeySym)
{
	if ((SDL_GetModState() & KMOD_NUM) == 0)
	{
		switch (KeySym)
		{
		case SDLK_KP_2:
			return SDLK_DOWN;
		case SDLK_KP_4:
			return SDLK_LEFT;
		case SDLK_KP_6:
			return SDLK_RIGHT;
		case SDLK_KP_8:
			return SDLK_UP;
		default:
			break;
		}
	}
	return KeySym;
}

static inline uint32 CharCodeFromSDLKeySym(const SDL_Keycode KeySym)
{
	// Mirrors windows returning nonzero char codes for numpad keys
	if (KeySym == SDLK_KP_2 || KeySym == SDLK_KP_4 || KeySym == SDLK_KP_6 || KeySym == SDLK_KP_8)
	{
		return (uint32) KeySym;
	}
	else if ((KeySym & SDLK_SCANCODE_MASK) != 0)
	{
		return 0;
	}
	else if (KeySym == SDLK_DELETE)  // this doesn't use the scancode mask for some reason.
	{
		return 0;
	}
	return (uint32) KeySym;
}

void FLinuxApplication::ProcessDeferredMessage( SDL_Event Event )
{
	// This function can be reentered when entering a modal tick loop.
	// We need to make a copy of the events that need to be processed or we may end up processing the same messages twice
	SDL_HWindow NativeWindow = NULL;

	// get pointer to window that received this event
	bool bWindowlessEvent = false;
	TSharedPtr< FLinuxWindow > CurrentEventWindow = FindEventWindow(&Event, bWindowlessEvent);

	if (CurrentEventWindow.IsValid())
	{
		NativeWindow = CurrentEventWindow->GetHWnd();
	}
	if (!NativeWindow && !bWindowlessEvent)
	{
		return;
	}

	switch(Event.type)
	{
	case SDL_KEYDOWN:
		{
			const SDL_KeyboardEvent &KeyEvent = Event.key;
			SDL_Keycode KeySym = KeyEvent.keysym.sym;
			const uint32 CharCode = CharCodeFromSDLKeySym(KeySym);
			KeySym = TranslateNumLockKeySyms(KeySym);
			const bool bIsRepeated = KeyEvent.repeat != 0;

			// Text input is now handled in SDL_TEXTINPUT: see below
			MessageHandler->OnKeyDown(KeySym, CharCode, bIsRepeated);

			// Backspace input in only caught here.
			if (KeySym == SDLK_BACKSPACE)
			{
				MessageHandler->OnKeyChar('\b', bIsRepeated);
			}
			else if (KeySym == SDLK_RETURN)
			{
				MessageHandler->OnKeyChar('\r', bIsRepeated);
			}
		}
		break;
	case SDL_KEYUP:
		{
			const SDL_KeyboardEvent &KeyEvent = Event.key;
			const SDL_Keycode KeySym = KeyEvent.keysym.sym;
			const uint32 CharCode = CharCodeFromSDLKeySym(KeySym);
			const bool IsRepeat = KeyEvent.repeat != 0;

			MessageHandler->OnKeyUp( KeySym, CharCode, IsRepeat );
		}
		break;
	case SDL_TEXTINPUT:
		{
			// Slate now gets all its text from here, I hope.
			const bool bIsRepeated = false;  //Event.key.repeat != 0;
			const FString TextStr(UTF8_TO_TCHAR(Event.text.text));
			for (auto TextIter = TextStr.CreateConstIterator(); TextIter; ++TextIter)
			{
				MessageHandler->OnKeyChar(*TextIter, bIsRepeated);
			}
		}
		break;
	case SDL_MOUSEMOTION:
		{
			SDL_MouseMotionEvent motionEvent = Event.motion;
			FLinuxCursor *LinuxCursor = (FLinuxCursor*)Cursor.Get();
			LinuxCursor->InvalidateCaches();

			if (!LinuxCursor->IsHidden())
			{
				FWindowProperties Props;
				GetWindowPropertiesInEventLoop(NativeWindow, Props);

				// When bUsingHighPrecisionMouseInput=1, changing the position cache causes the cursor (inside top/left/right etc. ViewPort)
				// to not move correct with the selection tool. The next part should be only run when not in Editor mode.
				if(!GIsEditor)
				{
					int32 BorderSizeX, BorderSizeY;
					CurrentEventWindow->GetNativeBordersSize(BorderSizeX, BorderSizeY);

					LinuxCursor->SetCachedPosition((int32)(motionEvent.x + Props.Location.X + BorderSizeX), (int32)(motionEvent.y + Props.Location.Y + BorderSizeY));
				}

				if( !CurrentEventWindow->GetDefinition().HasOSWindowBorder )
				{
					if ( CurrentEventWindow->IsRegularWindow() )
					{
						FVector2D CurrentPosition = LinuxCursor->GetPosition();
						MessageHandler->GetWindowZoneForPoint(CurrentEventWindow.ToSharedRef(), (int32)(CurrentPosition.X - Props.Location.X), (int32)(CurrentPosition.Y - Props.Location.Y));
						MessageHandler->OnCursorSet();
					}
				}
			}

			if(bUsingHighPrecisionMouseInput)
			{
				MessageHandler->OnRawMouseMove(motionEvent.xrel, motionEvent.yrel);
			}
			else
			{
				MessageHandler->OnMouseMove();
			}
		}
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		{
			SDL_MouseButtonEvent buttonEvent = Event.button;

			EMouseButtons::Type button;
			switch(buttonEvent.button)
			{
			case SDL_BUTTON_LEFT:
				button = EMouseButtons::Left;
				break;
			case SDL_BUTTON_MIDDLE:
				button = EMouseButtons::Middle;
				break;
			case SDL_BUTTON_RIGHT:
				button = EMouseButtons::Right;
				break;
			case SDL_BUTTON_X1:
				button = EMouseButtons::Thumb01;
				break;
			case SDL_BUTTON_X2:
				button = EMouseButtons::Thumb02;
				break;
			default:
				button = EMouseButtons::Invalid;
				break;
			}
			
			if (buttonEvent.type == SDL_MOUSEBUTTONUP)
			{
				MessageHandler->OnMouseUp(button);
				
				if (buttonEvent.button == SDL_BUTTON_LEFT)
				{
					bIsDragWindowButtonPressed = false;
				}
			}
			else
			{
				UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("MOUSE_DOWN                                 : %d"), CurrentEventWindow->GetID());
				// User clicked any button. Is the application active? If not activate it.
				if (!bActivateApp)
				{
					ActivateApplication();
				}

				if (buttonEvent.button == SDL_BUTTON_LEFT)
				{
					bIsDragWindowButtonPressed = true;
				}

				if (buttonEvent.clicks == 2)
				{
					MessageHandler->OnMouseDoubleClick(CurrentEventWindow, button);
				}
				else
				{
					// Check if we have to activate the window.
					if (CurrentlyActiveWindow != CurrentEventWindow)
					{
						ActivateWindow(CurrentEventWindow);
					}

					// Check if we have to set the focus.
					if(CurrentFocusWindow != CurrentEventWindow)
					{
						SDL_SetWindowInputFocus(CurrentEventWindow->GetHWnd());
					}

					MessageHandler->OnMouseDown(CurrentEventWindow, button);

					if (NotificationWindows.Num() > 0)
					{
						RaiseNotificationWindows(CurrentEventWindow);
					}
				}
			}
		}
		break;
	case SDL_MOUSEWHEEL:
		{
			SDL_MouseWheelEvent *WheelEvent = &Event.wheel;
			float Amount = (float)WheelEvent->y * fMouseWheelScrollAccel;

			MessageHandler->OnMouseWheel(Amount);
		}
		break;
	case SDL_CONTROLLERDEVICEADDED:
		{
			SDL_ControllerDeviceEvent& ControllerEvent = Event.cdevice;
			AddGameController(ControllerEvent.which);
		}
		break;
	case SDL_CONTROLLERDEVICEREMOVED:
		{
			SDL_ControllerDeviceEvent& ControllerEvent = Event.cdevice;
			RemoveGameController(ControllerEvent.which);
		}
		break;
	case SDL_CONTROLLERAXISMOTION:
		{
			SDL_ControllerAxisEvent caxisEvent = Event.caxis;
			FGamepadKeyNames::Type Axis = FGamepadKeyNames::Invalid;
			float AxisValue = ShortToNormalFloat(caxisEvent.value);

			if (!ControllerStates.Contains(caxisEvent.which))
			{
				break;
			}

			SDLControllerState &ControllerState = ControllerStates[caxisEvent.which];
			FPlatformUserId UserId = IPlatformInputDeviceMapper::Get().GetUserForInputDevice(ControllerState.DeviceId);

			switch (caxisEvent.axis)
			{
			case SDL_CONTROLLER_AXIS_LEFTX:
				Axis = FGamepadKeyNames::LeftAnalogX;
				if(caxisEvent.value > GAMECONTROLLER_LEFT_THUMB_DEADZONE)
				{
					if(!ControllerState.AnalogOverThreshold[0])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::LeftStickRight, UserId, ControllerState.DeviceId, false);
						ControllerState.AnalogOverThreshold[0] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[0])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::LeftStickRight, UserId, ControllerState.DeviceId, false);
					ControllerState.AnalogOverThreshold[0] = false;
				}
				if(caxisEvent.value < -GAMECONTROLLER_LEFT_THUMB_DEADZONE)
				{
					if(!ControllerState.AnalogOverThreshold[1])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::LeftStickLeft, UserId, ControllerState.DeviceId, false);
						ControllerState.AnalogOverThreshold[1] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[1])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::LeftStickLeft, UserId, ControllerState.DeviceId, false);
					ControllerState.AnalogOverThreshold[1] = false;
				}
				break;
			case SDL_CONTROLLER_AXIS_LEFTY:
				Axis = FGamepadKeyNames::LeftAnalogY;
				AxisValue *= -1;
				if(caxisEvent.value > GAMECONTROLLER_LEFT_THUMB_DEADZONE)
				{
					if(!ControllerState.AnalogOverThreshold[2])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::LeftStickDown, UserId, ControllerState.DeviceId, false);
						ControllerState.AnalogOverThreshold[2] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[2])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::LeftStickDown, UserId, ControllerState.DeviceId, false);
					ControllerState.AnalogOverThreshold[2] = false;
				}
				if(caxisEvent.value < -GAMECONTROLLER_LEFT_THUMB_DEADZONE)
				{
					if(!ControllerState.AnalogOverThreshold[3])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::LeftStickUp, UserId, ControllerState.DeviceId, false);
						ControllerState.AnalogOverThreshold[3] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[3])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::LeftStickUp, UserId, ControllerState.DeviceId, false);
					ControllerState.AnalogOverThreshold[3] = false;
				}
				break;
			case SDL_CONTROLLER_AXIS_RIGHTX:
				Axis = FGamepadKeyNames::RightAnalogX;
				if(caxisEvent.value > GAMECONTROLLER_RIGHT_THUMB_DEADZONE)
				{
					if(!ControllerState.AnalogOverThreshold[4])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::RightStickRight, UserId, ControllerState.DeviceId, false);
						ControllerState.AnalogOverThreshold[4] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[4])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::RightStickRight, UserId, ControllerState.DeviceId, false);
					ControllerState.AnalogOverThreshold[4] = false;
				}
				if(caxisEvent.value < -GAMECONTROLLER_RIGHT_THUMB_DEADZONE)
				{
					if(!ControllerState.AnalogOverThreshold[5])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::RightStickLeft, UserId, ControllerState.DeviceId, false);
						ControllerState.AnalogOverThreshold[5] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[5])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::RightStickLeft, UserId, ControllerState.DeviceId, false);
					ControllerState.AnalogOverThreshold[5] = false;
				}
				break;
			case SDL_CONTROLLER_AXIS_RIGHTY:
				Axis = FGamepadKeyNames::RightAnalogY;
				AxisValue *= -1;
				if(caxisEvent.value > GAMECONTROLLER_RIGHT_THUMB_DEADZONE)
				{
					if(!ControllerState.AnalogOverThreshold[6])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::RightStickDown, UserId, ControllerState.DeviceId, false);
						ControllerState.AnalogOverThreshold[6] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[6])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::RightStickDown, UserId, ControllerState.DeviceId, false);
					ControllerState.AnalogOverThreshold[6] = false;
				}
				if(caxisEvent.value < -GAMECONTROLLER_RIGHT_THUMB_DEADZONE)
				{
					if(!ControllerState.AnalogOverThreshold[7])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::RightStickUp, UserId, ControllerState.DeviceId, false);
						ControllerState.AnalogOverThreshold[7] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[7])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::RightStickUp, UserId, ControllerState.DeviceId, false);
					ControllerState.AnalogOverThreshold[7] = false;
				}
				break;
			case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
				Axis = FGamepadKeyNames::LeftTriggerAnalog;
				if(caxisEvent.value > GAMECONTROLLER_TRIGGER_THRESHOLD)
				{
					if(!ControllerState.AnalogOverThreshold[8])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::LeftTriggerThreshold, UserId, ControllerState.DeviceId, false);
						ControllerState.AnalogOverThreshold[8] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[8])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::LeftTriggerThreshold, UserId, ControllerState.DeviceId, false);
					ControllerState.AnalogOverThreshold[8] = false;
				}
				break;
			case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
				Axis = FGamepadKeyNames::RightTriggerAnalog;
				if(caxisEvent.value > GAMECONTROLLER_TRIGGER_THRESHOLD)
				{
					if(!ControllerState.AnalogOverThreshold[9])
					{
						MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::RightTriggerThreshold, UserId, ControllerState.DeviceId, false);
						ControllerState.AnalogOverThreshold[9] = true;
					}
				}
				else if(ControllerState.AnalogOverThreshold[9])
				{
					MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::RightTriggerThreshold, UserId, ControllerState.DeviceId, false);
					ControllerState.AnalogOverThreshold[9] = false;
				}
				break;
			default:
				break;
			}

			if (Axis != FGamepadKeyNames::Invalid)
			{
				float & ExistingAxisEventValue = ControllerState.AxisEvents.FindOrAdd(Axis);
				ExistingAxisEventValue = AxisValue;
			}
		}
		break;
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
		{
			SDL_ControllerButtonEvent cbuttonEvent = Event.cbutton;
			FGamepadKeyNames::Type Button = FGamepadKeyNames::Invalid;

			if (!ControllerStates.Contains(cbuttonEvent.which))
			{
				break;
			}

			switch (cbuttonEvent.button)
			{
			case SDL_CONTROLLER_BUTTON_A:
				Button = FGamepadKeyNames::FaceButtonBottom;
				break;
			case SDL_CONTROLLER_BUTTON_B:
				Button = FGamepadKeyNames::FaceButtonRight;
				break;
			case SDL_CONTROLLER_BUTTON_X:
				Button = FGamepadKeyNames::FaceButtonLeft;
				break;
			case SDL_CONTROLLER_BUTTON_Y:
				Button = FGamepadKeyNames::FaceButtonTop;
				break;
			case SDL_CONTROLLER_BUTTON_BACK:
				Button = FGamepadKeyNames::SpecialLeft;
				break;
			case SDL_CONTROLLER_BUTTON_START:
				Button = FGamepadKeyNames::SpecialRight;
				break;
			case SDL_CONTROLLER_BUTTON_LEFTSTICK:
				Button = FGamepadKeyNames::LeftThumb;
				break;
			case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
				Button = FGamepadKeyNames::RightThumb;
				break;
			case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
				Button = FGamepadKeyNames::LeftShoulder;
				break;
			case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
				Button = FGamepadKeyNames::RightShoulder;
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_UP:
				Button = FGamepadKeyNames::DPadUp;
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
				Button = FGamepadKeyNames::DPadDown;
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
				Button = FGamepadKeyNames::DPadLeft;
				break;
			case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
				Button = FGamepadKeyNames::DPadRight;
				break;
			default:
				break;
			}

			if (Button != FGamepadKeyNames::Invalid)
			{
				FPlatformUserId UserId = IPlatformInputDeviceMapper::Get().GetUserForInputDevice(ControllerStates[cbuttonEvent.which].DeviceId);

				if(cbuttonEvent.type == SDL_CONTROLLERBUTTONDOWN)
				{
					MessageHandler->OnControllerButtonPressed(Button, UserId, ControllerStates[cbuttonEvent.which].DeviceId, false);
				}
				else
				{
					MessageHandler->OnControllerButtonReleased(Button, UserId, ControllerStates[cbuttonEvent.which].DeviceId, false);
				}
			}
		}
		break;

	case SDL_WINDOWEVENT:
		{
			SDL_WindowEvent windowEvent = Event.window;

			switch (windowEvent.event)
			{
				case SDL_WINDOWEVENT_SIZE_CHANGED:
					{
						int NewWidth  = windowEvent.data1;
						int NewHeight = windowEvent.data2;

						MessageHandler->OnSizeChanged(
							CurrentEventWindow.ToSharedRef(),
							NewWidth,
							NewHeight,
							//	bWasMinimized
							false
						);
					}
					break;

				case SDL_WINDOWEVENT_RESIZED:
					{
						MessageHandler->OnResizingWindow( CurrentEventWindow.ToSharedRef() );
					}
					break;

				case SDL_WINDOWEVENT_CLOSE:
					{
						MessageHandler->OnWindowClose( CurrentEventWindow.ToSharedRef() );
					}
					break;

				case SDL_WINDOWEVENT_SHOWN:
					{
						// (re)cache native properties
						CurrentEventWindow->CacheNativeProperties();

						// A window did show up. Is the whole Application active? If not first activate it (ignore tooltips).
						if (!bActivateApp && !CurrentEventWindow->IsTooltipWindow())
						{
							ActivateApplication();
						}

						// Check if this window is different then the currently active one. If it is another one
						// activate that window and if necessary deactivate the one which was active.
						if (CurrentlyActiveWindow != CurrentEventWindow)
						{
							ActivateWindow(CurrentEventWindow);
						}

						// Set focus if the window wants to have a focus when first shown.
						if (CurrentEventWindow->IsFocusWhenFirstShown())
						{
							SDL_SetWindowInputFocus(CurrentEventWindow->GetHWnd());
						}
					}
					break;

				case SDL_WINDOWEVENT_MOVED:
					{
						// Mask away the higher bits (but preserve negative flags), as SDL uses those as flags
						// See: SDL_WINDOWPOS_UNDEFINED_MASK & SDL_WINDOWPOS_CENTERED_MASK for context
						int32 ClientScreenX = MaskAwayHigherBits(windowEvent.data1);
						int32 ClientScreenY = MaskAwayHigherBits(windowEvent.data2);

						int32 BorderSizeX, BorderSizeY;
						CurrentEventWindow->GetNativeBordersSize(BorderSizeX, BorderSizeY);
						ClientScreenX += BorderSizeX;
						ClientScreenY += BorderSizeY;

						MessageHandler->OnMovedWindow(CurrentEventWindow.ToSharedRef(), ClientScreenX, ClientScreenY);

						if (bFirstFrameOfWindowMove)
						{
							bFirstFrameOfWindowMove = false;

							if (NotificationWindows.Num() > 0)
							{
								RaiseNotificationWindows(CurrentEventWindow);
							}
						}

						// Mouse dragging is the likely cause of this window move, so invalidate the cached cursor position
						FLinuxCursor *LinuxCursor = static_cast<FLinuxCursor*>(Cursor.Get());
						LinuxCursor->InvalidateCaches();
					}
					break;

				case SDL_WINDOWEVENT_MAXIMIZED:
					{
						MessageHandler->OnWindowAction(CurrentEventWindow.ToSharedRef(), EWindowAction::Maximize);
						UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("Window: '%d' got maximized"), CurrentEventWindow->GetID());

						if (NotificationWindows.Num() > 0)
						{
							RaiseNotificationWindows(CurrentEventWindow);
						}
					}
					break;

				case SDL_WINDOWEVENT_RESTORED:
					{
						MessageHandler->OnWindowAction(CurrentEventWindow.ToSharedRef(), EWindowAction::Restore);
						UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("Window: '%d' got restored"), CurrentEventWindow->GetID());

						if (NotificationWindows.Num() > 0)
						{
							RaiseNotificationWindows(CurrentEventWindow);
						}
					}
					break;

				case SDL_WINDOWEVENT_ENTER:
					{
						if (CurrentEventWindow.IsValid())
						{
							MessageHandler->OnCursorSet();

							// Currently Tooltip windows will also get enter/leave events. depending on if this causes issues
							// should avoid setting the window under cursor for tooltips and use the window under
							CurrentUnderCursorWindow = CurrentEventWindow;

							bInsideOwnWindow = true;

							UE_LOG(LogLinuxWindow, Verbose, TEXT("Entered one of application windows. Cursor under Window: '%d'"), CurrentEventWindow->GetID());
						}
					}
					break;

				case SDL_WINDOWEVENT_LEAVE:
					{
						if (CurrentEventWindow.IsValid())
						{
							if (GetCapture() != nullptr)
							{
								UpdateMouseCaptureWindow((SDL_HWindow)GetCapture());
							}

 							CurrentUnderCursorWindow = nullptr;
							bInsideOwnWindow = false;
							UE_LOG(LogLinuxWindow, Verbose, TEXT("Left an application window we were hovering above."));
						}
					}
					break;

				case SDL_WINDOWEVENT_HIT_TEST:
					{
						// The user clicked into the hit test area (Titlebar for example). Is the whole Application active?
						// If not, first activate (ignore tooltips).
						if (!bActivateApp && !CurrentEventWindow->IsTooltipWindow())
						{
							ActivateApplication();
						}

						// Check if this window is different then the currently active one. If it is another one activate this 
						// window and deactivate the other one.
						if (CurrentlyActiveWindow != CurrentEventWindow)
						{
							ActivateWindow(CurrentEventWindow);
						}

						// Set the input focus.
						if (CurrentEventWindow.IsValid())
						{
							SDL_SetWindowInputFocus(CurrentEventWindow->GetHWnd());
						}

						if (NotificationWindows.Num() > 0)
						{
							RaiseNotificationWindows(CurrentEventWindow);
						}

						bFirstFrameOfWindowMove = true;
					}
					break;

				case SDL_WINDOWEVENT_TAKE_FOCUS:
					{
						if (!bActivateApp)
						{
							ActivateApplication();
						}

						// Some windows like notification windows may popup without needing the focus. That is handled in the SDL_WINDOWEVENT_SHOWN case.
						// The problem would be that the WM will send the Take Focus event and wants to set the focus. We don't want it to set it
						// for notifications because they are already handled in the above mentioned event.
						if ((CurrentFocusWindow != CurrentEventWindow) && !CurrentEventWindow->IsNotificationWindow())
						{
							SDL_SetWindowInputFocus(CurrentEventWindow->GetHWnd());
						}
					}
					break;

				case SDL_WINDOWEVENT_FOCUS_GAINED:
					{
						UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("WM_SETFOCUS                                 : %d"), CurrentEventWindow->GetID());

						CurrentFocusWindow = CurrentEventWindow;

						if (NotificationWindows.Num() > 0)
						{
							RaiseNotificationWindows(CurrentEventWindow);
						}

						// We have gained focus, we can stop trying to check if we need to deactivate
						FocusOutDeactivationTime = 0.0;
					}
					break;

				case SDL_WINDOWEVENT_FOCUS_LOST:
					{
						UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("WM_KILLFOCUS                                : %d"), CurrentEventWindow->GetID());

						// OK, the active window lost focus. This could mean the app went completely out of
						// focus. That means the app must be deactivated. To make sure that the user did
						// not click to another window we delay the deactivation.
						// TODO Figure out if the delay time may cause problems.
						if(CurrentFocusWindow == CurrentEventWindow)
						{
							// Only do if the application is active.
							if(bActivateApp)
							{
								FocusOutDeactivationTime = FPlatformTime::Seconds() + DeactivationThreadshold;
							}
						}
						CurrentFocusWindow = nullptr;
					}
					break;
				case SDL_WINDOWEVENT_HIDDEN:	// intended fall-through
				case SDL_WINDOWEVENT_EXPOSED:	// intended fall-through
				case SDL_WINDOWEVENT_MINIMIZED:	// intended fall-through
				default:
					break;
			}
		}
		break;

	case SDL_DROPBEGIN:
		{
			check(DragAndDropQueue.Num() == 0);  // did we get confused?
			check(DragAndDropTextQueue.Num() == 0);  // did we get confused?
		}
		break;

	case SDL_DROPFILE:
		{
			FString tmp = StringUtility::UnescapeURI(UTF8_TO_TCHAR(Event.drop.file));
			DragAndDropQueue.Add(tmp);
			SDL_free(Event.drop.file);
			UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("File dropped: %s"), *tmp);
		}
		break;

	case SDL_DROPTEXT:
		{
			FString tmp = UTF8_TO_TCHAR(Event.drop.file);
			DragAndDropTextQueue.Add(tmp);
			SDL_free(Event.drop.file);
			UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("Text dropped: %s"), *tmp);
		}
		break;

	case SDL_DROPCOMPLETE:
		{
			if (DragAndDropQueue.Num() > 0)
			{
				MessageHandler->OnDragEnterFiles(CurrentEventWindow.ToSharedRef(), DragAndDropQueue);
				MessageHandler->OnDragDrop(CurrentEventWindow.ToSharedRef());
				DragAndDropQueue.Empty();
			}

			if (DragAndDropTextQueue.Num() > 0)
			{
				for (const auto & Text : DragAndDropTextQueue)
				{
					MessageHandler->OnDragEnterText(CurrentEventWindow.ToSharedRef(), Text);
					MessageHandler->OnDragDrop(CurrentEventWindow.ToSharedRef());
				}
				DragAndDropTextQueue.Empty();
			}
			UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("DragAndDrop finished for Window              : %d"), CurrentEventWindow->GetID());
		}
		break;

	case SDL_FINGERDOWN:
		{
			UE_LOG(LogLinuxWindow, Verbose, TEXT("Finger %llu is down at (%f, %f)"), Event.tfinger.fingerId, Event.tfinger.x, Event.tfinger.y);

			// touch events can have no window associated with them, in that case ignore (with a warning)
			if (LIKELY(!bWindowlessEvent))
			{
				// remove touch context even if it existed
				uint64 FingerId = static_cast<uint64>(Event.tfinger.fingerId);

				FTouchContext* TouchContext = Touches.Find(FingerId);
				if (UNLIKELY(Touches.Find(FingerId) != nullptr))
				{
					TouchIds[TouchContext->TouchIndex] = TOptional<uint64>();
					Touches.Remove(FingerId);
					UE_LOG(LogLinuxWindow, Warning, TEXT("Received another SDL_FINGERDOWN for finger %llu which was already down."), FingerId);
				}

				FTouchContext NewTouch;
				NewTouch.TouchIndex = GetFirstFreeTouchId();
				NewTouch.Location = GetTouchEventLocation(NativeWindow, Event);
				NewTouch.DeviceId = Event.tfinger.touchId;
				Touches.Add(FingerId, NewTouch);
				TouchIds[NewTouch.TouchIndex] = TOptional<uint64>(FingerId);

				UE_LOG(LogLinuxWindow, Verbose, TEXT("OnTouchStarted at (%f, %f), finger %d (system touch id %llu)"), NewTouch.Location.X, NewTouch.Location.Y, FingerId, NewTouch.TouchIndex);
				MessageHandler->OnTouchStarted(CurrentEventWindow, NewTouch.Location, 1.0f, NewTouch.TouchIndex, 0);// NewTouch.DeviceId);
			}
			else
			{
				UE_LOG(LogLinuxWindow, Warning, TEXT("Ignoring touch event SDL_FINGERDOWN (finger: %llu, x=%f, y=%f) that doesn't have a window associated with it"),
					Event.tfinger.fingerId, Event.tfinger.x, Event.tfinger.y);
			}
		}
		break;
	case SDL_FINGERUP:
		{
			UE_LOG(LogLinuxWindow, Verbose, TEXT("Finger %llu is up at (%f, %f)"), Event.tfinger.fingerId, Event.tfinger.x, Event.tfinger.y);

			// touch events can have no window associated with them, in that case ignore (with a warning)
			if (LIKELY(!bWindowlessEvent))
			{
				uint64 FingerId = static_cast<uint64>(Event.tfinger.fingerId);
				FTouchContext* TouchContext = Touches.Find(FingerId);
				if (UNLIKELY(TouchContext == nullptr))
				{
					UE_LOG(LogLinuxWindow, Warning, TEXT("Received SDL_FINGERUP for finger %llu which was already up."), FingerId);
					// do not send a duplicate up
				}
				else
				{
					TouchContext->Location = GetTouchEventLocation(NativeWindow, Event);
					// check touch device?

					UE_LOG(LogLinuxWindow, Verbose, TEXT("OnTouchEnded at (%f, %f), finger %d (system touch id %llu)"), TouchContext->Location.X, TouchContext->Location.Y, FingerId, TouchContext->TouchIndex);
					MessageHandler->OnTouchEnded(TouchContext->Location, TouchContext->TouchIndex, 0);// TouchContext->DeviceId);

					// remove the touch
					TouchIds[TouchContext->TouchIndex] = TOptional<uint64>();
					Touches.Remove(FingerId);
				}
			}
			else
			{
				UE_LOG(LogLinuxWindow, Warning, TEXT("Ignoring touch event SDL_FINGERUP (finger: %llu, x=%f, y=%f) that doesn't have a window associated with it"),
					Event.tfinger.fingerId, Event.tfinger.x, Event.tfinger.y);
			}
		}
		break;
	case SDL_FINGERMOTION:
		{
			// touch events can have no window associated with them, in that case ignore (with a warning)
			if (LIKELY(!bWindowlessEvent))
			{
				uint64 FingerId = static_cast<uint64>(Event.tfinger.fingerId);
				FTouchContext* TouchContext = Touches.Find(FingerId);
				if (UNLIKELY(TouchContext == nullptr))
				{
					UE_LOG(LogLinuxWindow, Warning, TEXT("Received SDL_FINGERMOTION for finger %llu which was not down."), FingerId);
					// ignore the event
				}
				else
				{
					// do not send moved event if position has not changed
					FVector2D Location = GetTouchEventLocation(NativeWindow, Event);
					if (LIKELY((Location - TouchContext->Location).IsNearlyZero() == false))
					{
						TouchContext->Location = Location;
						UE_LOG(LogLinuxWindow, Verbose, TEXT("OnTouchMoved at (%f, %f), finger %d (system touch id %llu)"), TouchContext->Location.X, TouchContext->Location.Y, TouchContext->TouchIndex, FingerId);
						MessageHandler->OnTouchMoved(TouchContext->Location, 1.0f, TouchContext->TouchIndex, 0);// TouchContext->DeviceId);
					}
				}
			}
			else
			{
				UE_LOG(LogLinuxWindow, Warning, TEXT("Ignoring touch event SDL_FINGERMOTION (finger: %llu, x=%f, y=%f) that doesn't have a window associated with it"),
					Event.tfinger.fingerId, Event.tfinger.x, Event.tfinger.y);
			}
		}
		break;

	default:
		UE_LOG(LogLinuxWindow, Verbose, TEXT("Received unknown SDL event type=%d"), Event.type);
		break;
	}
}

void FLinuxApplication::CheckIfApplicatioNeedsDeactivation()
{
	// If FocusOutDeactivationTime is set we have had a focus out event and are waiting to see if we need to Deactivate the Application
	if (!FMath::IsNearlyZero(FocusOutDeactivationTime))
	{
		// We still havent hit our timeout limit, keep waiting
		if (FocusOutDeactivationTime > FPlatformTime::Seconds())
		{
			return;
		}
		// If we don't use bIsDragWindowButtonPressed the draged window will be destroyed because we
		// deactivate the whole appliacton. TODO Is that a bug? Do we have to do something?
		else if (!CurrentFocusWindow.IsValid() && !bIsDragWindowButtonPressed)
		{
			DeactivateApplication();

			FocusOutDeactivationTime = 0.0;
		}
		else
		{
			FocusOutDeactivationTime = 0.0;
		}
	}
}

int FLinuxApplication::GetFirstFreeTouchId()
{
	for (int i = 0; i < TouchIds.Num(); i++)
	{
		if (TouchIds[i].IsSet() == false)
		{
			return i;
		}
	}

	return TouchIds.Add(TOptional<uint64>()); 
}

FVector2D FLinuxApplication::GetTouchEventLocation(SDL_HWindow NativeWindow, SDL_Event TouchEvent)
{
	checkf(TouchEvent.type == SDL_FINGERDOWN || TouchEvent.type == SDL_FINGERUP || TouchEvent.type == SDL_FINGERMOTION, TEXT("Wrong touch event."));

	FWindowProperties Props;
	GetWindowPropertiesInEventLoop(NativeWindow, Props);
	// coordinates aren't necessarily normalized: e.g. if the input is grabbed and we're sliding outside of the window we can get x > 1
	return FVector2D(Props.Location.X + Props.Size.X * TouchEvent.tfinger.x, Props.Location.Y + Props.Size.Y * TouchEvent.tfinger.y);
}

EWindowZone::Type FLinuxApplication::WindowHitTest(const TSharedPtr< FLinuxWindow > &Window, int x, int y)
{
	return MessageHandler->GetWindowZoneForPoint(Window.ToSharedRef(), x, y);
}

void FLinuxApplication::ProcessDeferredEvents( const float TimeDelta )
{
	// This function can be reentered when entering a modal tick loop.
	// We need to make a copy of the events that need to be processed or we may end up processing the same messages twice
	SDL_HWindow NativeWindow = NULL;

	TArray< SDL_Event > Events( PendingEvents );
	PendingEvents.Empty();

	for( int32 Index = 0; Index < Events.Num(); ++Index )
	{
		ProcessDeferredMessage( Events[Index] );
	}
}

void FLinuxApplication::PollGameDeviceState( const float TimeDelta )
{
	IPlatformInputDeviceMapper& Mapper = IPlatformInputDeviceMapper::Get();
	for(auto ControllerIt = ControllerStates.CreateIterator(); ControllerIt; ++ControllerIt)
	{
		for(auto Event = ControllerIt.Value().AxisEvents.CreateConstIterator(); Event; ++Event)
		{
			FPlatformUserId UserId = Mapper.GetUserForInputDevice(ControllerIt.Value().DeviceId);
			MessageHandler->OnControllerAnalog(Event.Key(), UserId, ControllerIt.Value().DeviceId, Event.Value());
		}
		ControllerIt.Value().AxisEvents.Empty();

		if (ControllerIt.Value().Haptic != nullptr)
		{
			ControllerIt.Value().UpdateHapticEffect();
		}
	}

	// initialize any externally-implemented input devices (we delay load initialize the array so any plugins have had time to load)
	if (!bHasLoadedInputPlugins && GIsRunning)
	{
		TArray<IInputDeviceModule*> PluginImplementations = IModularFeatures::Get().GetModularFeatureImplementations<IInputDeviceModule>(IInputDeviceModule::GetModularFeatureName());
		for (auto InputPluginIt = PluginImplementations.CreateIterator(); InputPluginIt; ++InputPluginIt)
		{
			TSharedPtr<IInputDevice> Device = (*InputPluginIt)->CreateInputDevice(MessageHandler);
			if (Device.IsValid())
			{
				UE_LOG(LogInit, Log, TEXT("Adding external input plugin."));
				ExternalInputDevices.Add(Device);
			}
		}

		bHasLoadedInputPlugins = true;
	}
	
	// Poll externally-implemented devices
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->Tick(TimeDelta);
		(*DeviceIt)->SendControllerEvents();
	}
}

TCHAR FLinuxApplication::ConvertChar( SDL_Keysym Keysym )
{
	if (SDL_GetKeyFromScancode(Keysym.scancode) >= 128)
	{
		return 0;
	}

	TCHAR Char = (TCHAR)SDL_GetKeyFromScancode(Keysym.scancode);

    if (Keysym.mod & (KMOD_LSHIFT | KMOD_RSHIFT))
    {
        // Convert to uppercase (FIXME: what about CAPS?)
		if( SDL_GetKeyFromScancode(Keysym.scancode)  >= 97 && SDL_GetKeyFromScancode(Keysym.scancode)  <= 122)
        {
            return (TCHAR)(Keysym.sym - 32);
        }
		else if( SDL_GetKeyFromScancode(Keysym.scancode) >= 91 && SDL_GetKeyFromScancode(Keysym.scancode)  <= 93)
        {
			return (TCHAR)(SDL_GetKeyFromScancode(Keysym.scancode) + 32); // [ \ ] -> { | }
        }
        else
        {
			switch(SDL_GetKeyFromScancode(Keysym.scancode) )
            {
                case '`': // ` -> ~
                    Char = TEXT('`');
                    break;

                case '-': // - -> _
                    Char = TEXT('_');
                    break;

                case '=': // - -> _
                    Char = TEXT('+');
                    break;

                case ',':
                    Char = TEXT('<');
                    break;

                case '.':
                    Char = TEXT('>');
                    break;

                case ';':
                    Char = TEXT(':');
                    break;

                case '\'':
                    Char = TEXT('\"');
                    break;

                case '/':
                    Char = TEXT('?');
                    break;

                case '0':
                    Char = TEXT(')');
                    break;

                case '9':
                    Char = TEXT('(');
                    break;

                case '8':
                    Char = TEXT('*');
                    break;

                case '7':
                    Char = TEXT('&');
                    break;

                case '6':
                    Char = TEXT('^');
                    break;

                case '5':
                    Char = TEXT('%');
                    break;

                case '4':
                    Char = TEXT('$');
                    break;

                case '3':
                    Char = TEXT('#');
                    break;

                case '2':
                    Char = TEXT('@');
                    break;

                case '1':
                    Char = TEXT('!');
                    break;

                default:
                    break;
            }
        }
    }

    return Char;
}

TSharedPtr< FLinuxWindow > FLinuxApplication::FindEventWindow(SDL_Event* Event, bool& bOutWindowlessEvent)
{
	uint32 WindowID = 0;
	bOutWindowlessEvent = false;

	switch (Event->type)
	{
		case SDL_TEXTINPUT:
			WindowID = Event->text.windowID;
			break;
		case SDL_TEXTEDITING:
			WindowID = Event->edit.windowID;
			break;
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			WindowID = Event->key.windowID;
			break;
		case SDL_MOUSEMOTION:
			WindowID = Event->motion.windowID;
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			WindowID = Event->button.windowID;
			break;
		case SDL_MOUSEWHEEL:
			WindowID = Event->wheel.windowID;
			break;
		case SDL_WINDOWEVENT:
			WindowID = Event->window.windowID;
			break;
		case SDL_DROPBEGIN:
		case SDL_DROPFILE:
		case SDL_DROPTEXT:
		case SDL_DROPCOMPLETE:
			WindowID = Event->drop.windowID;
			break;
		case SDL_FINGERDOWN:
		case SDL_FINGERUP:
		case SDL_FINGERMOTION:
			// SDL touch events are windowless, but Slate needs to associate touch down with a particular window.
			// Assume that the current focus window is the one relevant for the touch and if there's none, assume the event windowless
			if (CurrentFocusWindow.IsValid())
			{
				return CurrentFocusWindow;
			}
			else
			{
				bOutWindowlessEvent = true;
				return TSharedPtr<FLinuxWindow>(nullptr);
			}
			break;
		default:
			bOutWindowlessEvent = true;
			return TSharedPtr< FLinuxWindow >(nullptr);
	}

	for (int32 WindowIndex=0; WindowIndex < Windows.Num(); ++WindowIndex)
	{
		TSharedRef< FLinuxWindow > Window = Windows[WindowIndex];
		
		if (SDL_GetWindowID(Window->GetHWnd()) == WindowID)
		{
			return Window;
		}
	}

	return TSharedPtr< FLinuxWindow >(nullptr);
}

void FLinuxApplication::RemoveEventWindow(SDL_HWindow HWnd)
{
	for (int32 WindowIndex=0; WindowIndex < Windows.Num(); ++WindowIndex)
	{
		TSharedRef< FLinuxWindow > Window = Windows[ WindowIndex ];
		
		if ( Window->GetHWnd() == HWnd )
		{
			Windows.RemoveAt(WindowIndex);
			return;
		}
	}
}

FModifierKeysState FLinuxApplication::GetModifierKeys() const
{
	SDL_Keymod modifiers = SDL_GetModState();

	const bool bIsLeftShiftDown		= (modifiers & KMOD_LSHIFT) != 0;
	const bool bIsRightShiftDown	= (modifiers & KMOD_RSHIFT) != 0;
	const bool bIsLeftControlDown	= (modifiers & KMOD_LCTRL) != 0;
	const bool bIsRightControlDown	= (modifiers & KMOD_RCTRL) != 0;
	const bool bIsLeftAltDown		= (modifiers & KMOD_LALT) != 0;
	const bool bIsRightAltDown		= (modifiers & KMOD_RALT) != 0;
	const bool bAreCapsLocked		= (modifiers & KMOD_CAPS) != 0;

	return FModifierKeysState( bIsLeftShiftDown, bIsRightShiftDown, bIsLeftControlDown, bIsRightControlDown, bIsLeftAltDown, bIsRightAltDown, false, false, bAreCapsLocked );
}

void FLinuxApplication::SetCapture( const TSharedPtr< FGenericWindow >& InWindow )
{
	bIsMouseCaptureEnabled = InWindow.IsValid();
	UpdateMouseCaptureWindow( bIsMouseCaptureEnabled ? ((FLinuxWindow*)InWindow.Get())->GetHWnd() : NULL );
}

void* FLinuxApplication::GetCapture( void ) const
{
	return ( bIsMouseCaptureEnabled && MouseCaptureWindow ) ? MouseCaptureWindow : NULL;
}

bool FLinuxApplication::IsGamepadAttached() const
{
	for (const TPair<SDL_JoystickID, SDLControllerState>& ControllerState : ControllerStates)
	{
		if (SDL_GameControllerGetAttached(ControllerState.Value.Controller))
		{
			return true;
		}
	}
	
	return false;
}

void FLinuxApplication::UpdateMouseCaptureWindow(SDL_HWindow TargetWindow)
{
	const bool bEnable = bIsMouseCaptureEnabled || bIsMouseCursorLocked;
	FLinuxCursor *LinuxCursor = static_cast<FLinuxCursor*>(Cursor.Get());

	// this is a hacky heuristic which makes QA-ClickHUD work while not ruining SlateViewer...
	bool bShouldGrab = (IS_PROGRAM != 0 || WITH_ENGINE != 0 || GIsEditor) && !LinuxCursor->IsHidden();
	if (bEnable)
	{
		if (TargetWindow)
		{
			MouseCaptureWindow = TargetWindow;
		}
		if (bShouldGrab && MouseCaptureWindow)
		{
			SDL_CaptureMouse(SDL_TRUE);
		}
	}
	else
	{
		SDL_CaptureMouse(SDL_FALSE);
		MouseCaptureWindow = nullptr;
	}
}

void FLinuxApplication::SetHighPrecisionMouseMode( const bool Enable, const TSharedPtr< FGenericWindow >& InWindow )
{
	MessageHandler->OnCursorSet();
	bUsingHighPrecisionMouseInput = Enable;
	if (!FParse::Param(FCommandLine::Get(), TEXT("norelativemousemode")))
	{
		SDL_SetRelativeMouseMode(Enable ? SDL_TRUE : SDL_FALSE);
	}
}

void FLinuxApplication::RefreshDisplayCache()
{
	const double kCacheLifetime = 5.0;	// ask once in 5 seconds

	double CurrentTime = FPlatformTime::Seconds();
	if (CurrentTime - LastTimeCachedDisplays > kCacheLifetime)
	{
		CachedDisplays.Empty();

		int NumDisplays = SDL_GetNumVideoDisplays();

		for (int DisplayIdx = 0; DisplayIdx < NumDisplays; ++DisplayIdx)
		{
			SDL_Rect UsableBounds;
			SDL_GetDisplayUsableBounds(DisplayIdx, &UsableBounds);

			CachedDisplays.Add(UsableBounds);
		}

		LastTimeCachedDisplays = CurrentTime;
	}
}

FPlatformRect FLinuxApplication::GetWorkArea( const FPlatformRect& CurrentWindow ) const
{
	(const_cast<FLinuxApplication *>(this))->RefreshDisplayCache();

	// loop over all monitors to determine which one is the best
	int NumDisplays = CachedDisplays.Num();
	if (NumDisplays <= 0)
	{
		// fake something
		return CurrentWindow;
	}

	SDL_Rect BestDisplayBounds = CachedDisplays[0];

	// see if any other are better (i.e. cover top left)
	for (int DisplayIdx = 1; DisplayIdx < NumDisplays; ++DisplayIdx)
	{
		const SDL_Rect & DisplayBounds = CachedDisplays[DisplayIdx];

		// only check top left corner for "bestness"
		if (DisplayBounds.x <= CurrentWindow.Left && DisplayBounds.x + DisplayBounds.w > CurrentWindow.Left &&
			DisplayBounds.y <= CurrentWindow.Top && DisplayBounds.y + DisplayBounds.h > CurrentWindow.Bottom)
		{
			BestDisplayBounds = DisplayBounds;
			// there can be only one, as we don't expect overlapping displays
			break;
		}
	}

	FPlatformRect WorkArea;
	WorkArea.Left	= BestDisplayBounds.x;
	WorkArea.Top	= BestDisplayBounds.y;
	WorkArea.Right	= BestDisplayBounds.x + BestDisplayBounds.w;
	WorkArea.Bottom	= BestDisplayBounds.y + BestDisplayBounds.h;

	return WorkArea;
}

void FDisplayMetrics::RebuildDisplayMetrics(FDisplayMetrics& OutDisplayMetrics)
{
	if (FNullPlatformApplicationMisc::IsUsingNullApplication())
	{
		FNullPlatformDisplayMetrics::RebuildDisplayMetrics(OutDisplayMetrics);
	}
	else
	{
		int NumDisplays = 0;

		if (LIKELY(FApp::CanEverRender()))
		{
			if (FLinuxPlatformApplicationMisc::InitSDL()) //	will not initialize more than once
			{
				NumDisplays = SDL_GetNumVideoDisplays();
			}
			else
			{
				UE_LOG(LogInit, Warning, TEXT("FDisplayMetrics::GetDisplayMetrics: InitSDL() failed, cannot get display metrics"));
			}
		}

		OutDisplayMetrics.MonitorInfo.Empty();

		if (NumDisplays <= 0)
		{
			if (IsRunningDedicatedServer())
			{
				// dedicated servers has always been exiting early
				OutDisplayMetrics.PrimaryDisplayWorkAreaRect = FPlatformRect(0, 0, 0, 0);
				OutDisplayMetrics.VirtualDisplayRect = OutDisplayMetrics.PrimaryDisplayWorkAreaRect;
				OutDisplayMetrics.PrimaryDisplayWidth = 0;
				OutDisplayMetrics.PrimaryDisplayHeight = 0;

				return;
			}
			else
			{
				// headless clients need some plausible values because high level logic depends on viewport sizes not being 0 (see e.g. UnrealClient.cpp)
				int32 Width = 1920;
				int32 Height = 1080;

				FMonitorInfo Display;
				Display.bIsPrimary = true;
				if (FPlatformApplicationMisc::IsHighDPIAwarenessEnabled())
				{
					Display.DPI = 96;
				}
				Display.ID = TEXT("fakedisplay");
				Display.NativeWidth = Width;
				Display.NativeHeight = Height;
				Display.MaxResolution = FIntPoint(Width, Height);
				Display.DisplayRect = FPlatformRect(0, 0, Width, Height);
				Display.WorkArea = FPlatformRect(0, 0, Width, Height);

				OutDisplayMetrics.PrimaryDisplayWorkAreaRect = Display.WorkArea;
				OutDisplayMetrics.VirtualDisplayRect = OutDisplayMetrics.PrimaryDisplayWorkAreaRect;
				OutDisplayMetrics.PrimaryDisplayWidth = Display.NativeWidth;
				OutDisplayMetrics.PrimaryDisplayHeight = Display.NativeHeight;
				OutDisplayMetrics.MonitorInfo.Add(Display);
			}
		}

		for (int32 DisplayIdx = 0; DisplayIdx < NumDisplays; ++DisplayIdx)
		{
			SDL_Rect DisplayBounds, UsableBounds;
			FMonitorInfo Display;
			SDL_GetDisplayBounds(DisplayIdx, &DisplayBounds);
			SDL_GetDisplayUsableBounds(DisplayIdx, &UsableBounds);

			Display.Name = UTF8_TO_TCHAR(SDL_GetDisplayName(DisplayIdx));
			Display.ID = FString::Printf(TEXT("display%d"), DisplayIdx);
			Display.NativeWidth = DisplayBounds.w;
			Display.NativeHeight = DisplayBounds.h;
			Display.MaxResolution = FIntPoint(DisplayBounds.w, DisplayBounds.h);
			Display.DisplayRect = FPlatformRect(DisplayBounds.x, DisplayBounds.y, DisplayBounds.x + DisplayBounds.w, DisplayBounds.y + DisplayBounds.h);
			Display.WorkArea = FPlatformRect(UsableBounds.x, UsableBounds.y, UsableBounds.x + UsableBounds.w, UsableBounds.y + UsableBounds.h);
			Display.bIsPrimary = DisplayIdx == 0;

			if (FPlatformApplicationMisc::IsHighDPIAwarenessEnabled())
			{
				float HorzDPI = 0.0f, VertDPI = 0.0f;
				if (SDL_GetDisplayDPI(DisplayIdx, nullptr, &HorzDPI, &VertDPI) == 0)
				{
					Display.DPI = FMath::FloorToInt((HorzDPI + VertDPI) / 2.0f);
				}
			}

			OutDisplayMetrics.MonitorInfo.Add(Display);

			if (Display.bIsPrimary)
			{
				OutDisplayMetrics.PrimaryDisplayWorkAreaRect = FPlatformRect(UsableBounds.x, UsableBounds.y, UsableBounds.x + UsableBounds.w, UsableBounds.y + UsableBounds.h);

				OutDisplayMetrics.PrimaryDisplayWidth = DisplayBounds.w;
				OutDisplayMetrics.PrimaryDisplayHeight = DisplayBounds.h;

				OutDisplayMetrics.VirtualDisplayRect = OutDisplayMetrics.PrimaryDisplayWorkAreaRect;
			}
			else
			{
				// accumulate the total bound rect
				OutDisplayMetrics.VirtualDisplayRect.Left = FMath::Min(DisplayBounds.x, OutDisplayMetrics.VirtualDisplayRect.Left);
				OutDisplayMetrics.VirtualDisplayRect.Right = FMath::Max(OutDisplayMetrics.VirtualDisplayRect.Right, DisplayBounds.x + DisplayBounds.w);
				OutDisplayMetrics.VirtualDisplayRect.Top = FMath::Min(DisplayBounds.y, OutDisplayMetrics.VirtualDisplayRect.Top);
				OutDisplayMetrics.VirtualDisplayRect.Bottom = FMath::Max(OutDisplayMetrics.VirtualDisplayRect.Bottom, DisplayBounds.y + DisplayBounds.h);
			}
		}
	}

	// Apply the debug safe zones
	OutDisplayMetrics.ApplyDefaultSafeZones();
}

void FLinuxApplication::RemoveNotificationWindow(SDL_HWindow HWnd)
{
	for (int32 WindowIndex=0; WindowIndex < NotificationWindows.Num(); ++WindowIndex)
	{
		TSharedRef< FLinuxWindow > Window = NotificationWindows[ WindowIndex ];

		if ( Window->GetHWnd() == HWnd )
		{
			NotificationWindows.RemoveAt(WindowIndex);
			return;
		}
	}
}

void FLinuxApplication::RaiseNotificationWindows(const TSharedPtr< FLinuxWindow >& ParentWindow)
{
	// Raise notification windows above everything except for modal windows
	for (int32 WindowIndex=0; WindowIndex < NotificationWindows.Num(); ++WindowIndex)
	{
		TSharedRef< FLinuxWindow > NotificationWindow = NotificationWindows[WindowIndex];

		if(!ParentWindow->IsModalWindow())
		{
			SDL_RaiseWindow(NotificationWindow->GetHWnd());
		}
	}
}

void FLinuxApplication::RemoveRevertFocusWindow(SDL_HWindow HWnd)
{
	for (int32 WindowIndex=0; WindowIndex < RevertFocusStack.Num(); ++WindowIndex)
	{
		TSharedRef< FLinuxWindow > Window = RevertFocusStack[ WindowIndex ];

		if (Window->GetHWnd() == HWnd)
		{
			UE_LOG(LogLinuxWindow, Verbose, TEXT("Found Window that is going to be destroyed. Going to revert focus ..."), Window->GetID());
			RevertFocusStack.RemoveAt(WindowIndex);

			if(Window->IsUtilityWindow() || Window->IsDialogWindow())
			{
				ActivateWindow(Window->GetParent());
			}
			// Was the deleted window a Blueprint, Cascade etc. window?
			else if (Window->IsNotificationWindow())
			{
				// Do not revert focus if the root window of the destroyed window is another one.
				TSharedPtr<FLinuxWindow> RevertFocusToWindow = Window->GetParent(); 
				TSharedPtr<FLinuxWindow> RootWindow = GetRootWindow(Window);
				UE_LOG(LogLinuxWindow, Verbose, TEXT("CurrentlyActiveWindow: %d, RootParentWindow: %d "), 	CurrentlyActiveWindow.IsValid() ? CurrentlyActiveWindow->GetID() : -1,
																											RootWindow.IsValid() ? RootWindow->GetID() : -1);

				// Only do this if the destroyed window had a root and the currently active is neither itself nor the root window.
				// If the currently active window is not the root another window got active before we could destroy it. So we give the focus to the
				// currently active one and the currently active window shouldn't be the destructed one, if yes that means that no other window got active
				// so we can process normally.
				if(CurrentlyActiveWindow.IsValid() && RootWindow.IsValid() && (CurrentlyActiveWindow != RootWindow) && (CurrentlyActiveWindow != Window) )
				{
					UE_LOG(LogLinuxWindow, Verbose, TEXT("Root Parent is different, going to set focus to CurrentlyActiveWindow: %d"), CurrentlyActiveWindow.IsValid() ? CurrentlyActiveWindow->GetID() : -1);
					RevertFocusToWindow = CurrentlyActiveWindow;
				}

				ActivateWindow(RevertFocusToWindow);
			}
			// Was the deleted window a top level window and we have still at least one other window in the stack?
			else if (Window->IsTopLevelWindow() && (RevertFocusStack.Num() > 0))
			{
				// OK, give focus to the one on top of the stack.
				TSharedPtr< FLinuxWindow > TopmostWindow = RevertFocusStack.Top();
				if (TopmostWindow.IsValid())
				{
					ActivateWindow(TopmostWindow);
				}
			}
			// Was it a popup menu?
			else if (Window->IsPopupMenuWindow() && bActivateApp)
			{
				ActivateWindow(Window->GetParent());

				UE_LOG(LogLinuxWindowType, Verbose, TEXT("FLinuxWindow::Destroy: Going to revert focus to %d"), Window->GetParent()->GetID());
			}
			break;
		}
	}
}

void FLinuxApplication::ActivateApplication()
{
	MessageHandler->OnApplicationActivationChanged( true );
	bActivateApp = true;
	UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("WM_ACTIVATEAPP, wParam = 1"));
}

void FLinuxApplication::DeactivateApplication()
{
	MessageHandler->OnApplicationActivationChanged( false );
	CurrentlyActiveWindow = nullptr;
	CurrentFocusWindow = nullptr;
	bActivateApp = false;
	UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("WM_ACTIVATEAPP, wParam = 0"));
}

void FLinuxApplication::ActivateWindow(const TSharedPtr< FLinuxWindow >& Window)
{
	if (Window->GetActivationPolicy() == EWindowActivationPolicy::Never)
	{
		return;
	}

	PreviousActiveWindow = CurrentlyActiveWindow;
	CurrentlyActiveWindow = Window;
	if(PreviousActiveWindow.IsValid())
	{
		MessageHandler->OnWindowActivationChanged(PreviousActiveWindow.ToSharedRef(), EWindowActivation::Deactivate);
		UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("WM_ACTIVATE,    wParam = WA_INACTIVE     : %d"), PreviousActiveWindow->GetID());
	}
	MessageHandler->OnWindowActivationChanged(CurrentlyActiveWindow.ToSharedRef(), EWindowActivation::Activate);
	UE_LOG(LogLinuxWindowEvent, Verbose, TEXT("WM_ACTIVATE,    wParam = WA_ACTIVE       : %d"), CurrentlyActiveWindow->GetID());
}

void FLinuxApplication::ActivateRootWindow(const TSharedPtr< FLinuxWindow >& Window)
{
	TSharedPtr< FLinuxWindow > ParentWindow = Window;
	while(ParentWindow.IsValid() && ParentWindow->GetParent().IsValid())
	{
		ParentWindow = ParentWindow->GetParent();
	}
	ActivateWindow(ParentWindow);
}

TSharedPtr< FLinuxWindow > FLinuxApplication::GetRootWindow(const TSharedPtr< FLinuxWindow >& Window)
{
	TSharedPtr< FLinuxWindow > ParentWindow = Window;
	while(ParentWindow->GetParent().IsValid())
	{
		ParentWindow = ParentWindow->GetParent();
	}
	return ParentWindow;
}

bool FLinuxApplication::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Ignore any execs that doesn't start with LinuxApp
	if (!FParse::Command(&Cmd, TEXT("LinuxApp")))
	{
		return false;
	}

	if (FParse::Command(&Cmd, TEXT("Cursor")))
	{
		return HandleCursorCommand(Cmd, Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("Window")))
	{
		return HandleWindowCommand(Cmd, Ar);
	}

	return false;
}

bool FLinuxApplication::HandleCursorCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("Status")))
	{
		FLinuxCursor *LinuxCursor = static_cast<FLinuxCursor*>(Cursor.Get());
		FVector2D CurrentPosition = LinuxCursor->GetPosition();

		Ar.Logf(TEXT("Cursor status:"));
		Ar.Logf(TEXT("Position: (%f, %f)"), CurrentPosition.X, CurrentPosition.Y);
		Ar.Logf(TEXT("IsHidden: %s"), LinuxCursor->IsHidden() ? TEXT("true") : TEXT("false"));
		Ar.Logf(TEXT("bIsMouseCaptureEnabled: %s"), bIsMouseCaptureEnabled ? TEXT("true") : TEXT("false"));
		Ar.Logf(TEXT("bUsingHighPrecisionMouseInput: %s"), bUsingHighPrecisionMouseInput ? TEXT("true") : TEXT("false"));
		Ar.Logf(TEXT("bIsMouseCaptureEnabled: %s"), bIsMouseCaptureEnabled ? TEXT("true") : TEXT("false"));

		return true;
	}

	return false;
}

bool FLinuxApplication::HandleWindowCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("List")))
	{
		Ar.Logf(TEXT("Window list:"));
		for (int WindowIdx = 0, NumWindows = Windows.Num(); WindowIdx < NumWindows; ++WindowIdx)
		{
			Ar.Logf(TEXT("%d: native handle: %p, debugging ID: %d"), WindowIdx, Windows[WindowIdx]->GetHWnd(), Windows[WindowIdx]->GetID());
		}

		return true;
	}

	return false;
}

void FLinuxApplication::SaveWindowPropertiesForEventLoop(void)
{
	for (int32 WindowIndex = 0; WindowIndex < Windows.Num(); ++WindowIndex)
	{
		TSharedRef< FLinuxWindow > Window = Windows[WindowIndex];
		int X = 0;
		int Y = 0;
		int Width = 0;
		int Height = 0;
		SDL_HWindow NativeWindow = Window->GetHWnd();
		SDL_GetWindowPosition(NativeWindow, &X, &Y);
		X = MaskAwayHigherBits(X);
		Y = MaskAwayHigherBits(Y);
		SDL_GetWindowSize(NativeWindow, &Width, &Height);

		FWindowProperties Props;
		Props.Location = FVector2D(static_cast<float>(X), static_cast<float>(Y));
		Props.Size = FVector2D(static_cast<float>(Width), static_cast<float>(Height));
		SavedWindowPropertiesForEventLoop.FindOrAdd(NativeWindow) = Props;
	}
}

void FLinuxApplication::ClearWindowPropertiesAfterEventLoop(void)
{
	SavedWindowPropertiesForEventLoop.Empty();

	// This is a hack for 4.22.1 to avoid changing a header. We will be calling in from here at
	// LinuxPlatformApplicationMisc.cpp in FLinuxPlatformApplicationMisc::PumpMessages
	//
	// In 4.23 this will be in a function: void FLinuxApplication::CheckIfApplicatioNeedsDeactivation()

	// If FocusOutDeactivationTime is set we have had a focus out event and are waiting to see if we need to Deactivate the Application
	if (!FMath::IsNearlyZero(FocusOutDeactivationTime))
	{
		// We still havent hit our timeout limit, keep waiting
		if (FocusOutDeactivationTime > FPlatformTime::Seconds())
		{
			return;
		}
		// If we don't use bIsDragWindowButtonPressed the draged window will be destroyed because we
		// deactivate the whole appliacton. TODO Is that a bug? Do we have to do something?
		else if (!CurrentFocusWindow.IsValid() && !bIsDragWindowButtonPressed)
		{
			DeactivateApplication();

			FocusOutDeactivationTime = 0.0;
		}
		else
		{
			FocusOutDeactivationTime = 0.0;
		}
	}
}

void FLinuxApplication::GetWindowPropertiesInEventLoop(SDL_HWindow NativeWindow, FWindowProperties& Properties)
{
	FWindowProperties *SavedProps = SavedWindowPropertiesForEventLoop.Find(NativeWindow);
	if(SavedProps)
	{
		Properties = *SavedProps;
	}
	else if(NativeWindow)
	{
		int X, Y, Width, Height;
		SDL_GetWindowPosition(NativeWindow, &X, &Y);
		X = MaskAwayHigherBits(X);
		Y = MaskAwayHigherBits(Y);
		SDL_GetWindowSize(NativeWindow, &Width, &Height);
		Properties.Location = FVector2D(static_cast<float>(X), static_cast<float>(Y));
		Properties.Size = FVector2D(static_cast<float>(Width), static_cast<float>(Height));

		// If we've hit this case, then we're either not in the event
		// loop, or suddenly have a new window to keep track of.
		// Record the initial window position.
		SavedWindowPropertiesForEventLoop.FindOrAdd(NativeWindow) = Properties;
	}
	else
	{
		UE_LOG(LogLinuxWindowEvent, Error, TEXT("Tried to get the location of a non-existent window\n"));
		Properties.Location = FVector2D(0, 0);
		Properties.Size = FVector2D(0, 0);
	}
}

bool FLinuxApplication::IsMouseAttached() const
{
	int rc;
	char Mouse[64] = "/sys/class/input/mouse0";
	int MouseIdx = strlen(Mouse) - 1;
	FCStringAnsi::Strncat(Mouse, "/device/name", sizeof(Mouse) - 1);

	for (int i=0; i<9; i++)
	{
		Mouse[MouseIdx] = (char)('0' + i);
		if (access(Mouse, F_OK) == 0)
		{
			return true;
		}
	}

	return false;
}

void FLinuxApplication::SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
	if (FApp::UseVRFocus() && !FApp::HasVRFocus())
	{
		return; // do not proceed if the app uses VR focus but doesn't have it
	}

	for (auto ControllerIt = ControllerStates.CreateIterator(); ControllerIt; ++ControllerIt)
	{
		auto& ControllerState = ControllerIt.Value();
		int32 ControllerIndex = UE::Input::ConvertInputDeviceToInt(ControllerState.DeviceId);
		if (ControllerIndex == ControllerId)
		{
			if (ControllerState.Haptic != nullptr)
			{
				switch (ChannelType)
				{
				case FForceFeedbackChannelType::LEFT_LARGE:
					ControllerState.ForceFeedbackValues.LeftLarge = Value;
					break;
				case FForceFeedbackChannelType::LEFT_SMALL:
					ControllerState.ForceFeedbackValues.LeftSmall = Value;
					break;
				case FForceFeedbackChannelType::RIGHT_LARGE:
					ControllerState.ForceFeedbackValues.RightLarge = Value;
					break;
				case FForceFeedbackChannelType::RIGHT_SMALL:
					ControllerState.ForceFeedbackValues.RightSmall = Value;
					break;
				}
			}
			break;
		}
	}

	// send vibration to externally-implemented devices
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		(*DeviceIt)->SetChannelValue(ControllerId, ChannelType, Value);
	}
}
void FLinuxApplication::SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &Values)
{
	if (FApp::UseVRFocus() && !FApp::HasVRFocus())
	{
		return; // do not proceed if the app uses VR focus but doesn't have it
	}

	for (auto ControllerIt = ControllerStates.CreateIterator(); ControllerIt; ++ControllerIt)
	{
		auto& ControllerState = ControllerIt.Value();
		int32 ControllerIndex = UE::Input::ConvertInputDeviceToInt(ControllerState.DeviceId);
		if (ControllerIndex == ControllerId)
		{
			if (ControllerState.Haptic != nullptr)
			{
				ControllerState.ForceFeedbackValues = Values;
			}
			break;
		}
	}

	// send vibration to externally-implemented devices
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		if ((*DeviceIt)->IsGamepadAttached())
		{
			(*DeviceIt)->SetChannelValues(ControllerId, Values);
		}
	}
}

void FLinuxApplication::SetHapticFeedbackValues(int32 ControllerId, int32 Hand, const FHapticFeedbackValues& Values)
{
	if (FApp::UseVRFocus() && !FApp::HasVRFocus())
	{
		return; // do not proceed if the app uses VR focus but doesn't have it
	}
	for (auto DeviceIt = ExternalInputDevices.CreateIterator(); DeviceIt; ++DeviceIt)
	{
		IHapticDevice* HapticDevice = (*DeviceIt)->GetHapticDevice();
		if (HapticDevice)
		{
			HapticDevice->SetHapticFeedbackValues(ControllerId, Hand, Values);
		}
	}
}

void FLinuxApplication::SDLControllerState::UpdateHapticEffect()
{
	if (Haptic == nullptr)
	{
		return;
	}

	float LargeValue = FMath::Max(ForceFeedbackValues.LeftLarge, ForceFeedbackValues.RightLarge);
	float SmallValue = FMath::Max(ForceFeedbackValues.LeftSmall, ForceFeedbackValues.RightSmall);

	if (FMath::IsNearlyEqual(SmallValue, 0.0f) && FMath::IsNearlyEqual(LargeValue, 0.0f))
	{
		if (EffectId >= 0 && bEffectRunning)
		{
			SDL_HapticStopEffect(Haptic, EffectId);
			bEffectRunning = false;
		}
		return;
	}

	SDL_HapticEffect Effect;
	FMemory::Memzero(Effect);

	if (SDL_HapticQuery(Haptic) & SDL_HAPTIC_LEFTRIGHT)
	{
		Effect.type = SDL_HAPTIC_LEFTRIGHT;
		Effect.leftright.length = 1000;
		Effect.leftright.large_magnitude = (uint16)(32767.0f * LargeValue);
		Effect.leftright.small_magnitude = (uint16)(32767.0f * SmallValue);
	}
	else if (SDL_HapticQuery(Haptic) & SDL_HAPTIC_SINE)
	{
		Effect.type = SDL_HAPTIC_SINE;
		Effect.periodic.length = 1000;
		Effect.periodic.period = 1000;
		Effect.periodic.magnitude = (uint16)(32767.0f * FMath::Max(SmallValue, LargeValue));
	}
	else
	{
		UE_LOG(LogLinux, Warning, TEXT("No available haptic effects"));
		SDL_HapticClose(Haptic);
		Haptic = nullptr;
		return;
	}

	if (EffectId < 0)
	{
		EffectId = SDL_HapticNewEffect(Haptic, &Effect);
		if (EffectId < 0)
		{
			UE_LOG(LogLinux, Warning, TEXT("Failed to create haptic effect: %s"), UTF8_TO_TCHAR(SDL_GetError()) );
			SDL_HapticClose(Haptic);
			Haptic = nullptr;
			return;
		}
	}

	if (SDL_HapticUpdateEffect(Haptic, EffectId, &Effect) < 0)
	{
		SDL_HapticDestroyEffect(Haptic, EffectId);
		EffectId = SDL_HapticNewEffect(Haptic, &Effect);
		if (EffectId < 0)
		{
			UE_LOG(LogLinux, Warning, TEXT("Failed to update and recreate haptic effect: %s"), UTF8_TO_TCHAR(SDL_GetError()) );
			SDL_HapticClose(Haptic);
			Haptic = nullptr;
			return;
		}
	}

	SDL_HapticRunEffect(Haptic, EffectId, 1);
	bEffectRunning = true;
}

void FLinuxApplication::AddGameController(int Index)
{
	SDL_JoystickID Id = SDL_JoystickGetDeviceInstanceID(Index);
	if(ControllerStates.Contains(Id))
	{
		return;
	}

	auto Controller = SDL_GameControllerOpen(Index);
	if (Controller == nullptr)
	{
		UE_LOG(LogLinux, Warning, TEXT("Could not open gamecontroller %i: %s"), Index, UTF8_TO_TCHAR(SDL_GetError()) );
		return;
	}

	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();

	uint32 UsedBits = 0;
	for (auto ControllerIt = ControllerStates.CreateIterator(); ControllerIt; ++ControllerIt)
	{
		FPlatformUserId UserId = DeviceMapper.GetUserForInputDevice(ControllerIt.Value().DeviceId);
		UsedBits |= (1 << UserId.GetInternalId());
	}

	int32 FirstUnusedIndex = FMath::CountTrailingZeros(~UsedBits);

	UE_LOG(LogLinux, Verbose, TEXT("Adding controller %i '%s'"), FirstUnusedIndex, UTF8_TO_TCHAR(SDL_GameControllerName(Controller)));
	auto& ControllerState = ControllerStates.Add(Id);
	ControllerState.Controller = Controller;

	FPlatformUserId UserId = FPlatformUserId::CreateFromInternalId(FirstUnusedIndex);
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(FirstUnusedIndex, UserId, ControllerState.DeviceId);

	// Check for haptic support.
	ControllerState.Haptic = SDL_HapticOpenFromJoystick(SDL_GameControllerGetJoystick(Controller));

	if (ControllerState.Haptic != nullptr)
	{
		if ((SDL_HapticQuery(ControllerState.Haptic) & (SDL_HAPTIC_SINE | SDL_HAPTIC_LEFTRIGHT)) == 0)
		{
			UE_LOG(LogLinux, Warning, TEXT("No supported haptic effects for controller %i"), ControllerState.DeviceId.GetId());
			SDL_HapticClose(ControllerState.Haptic);
			ControllerState.Haptic = nullptr;
		}
	}

	DeviceMapper.Internal_MapInputDeviceToUser(ControllerState.DeviceId, UserId, EInputDeviceConnectionState::Connected);
}

void FLinuxApplication::RemoveGameController(SDL_JoystickID Id)
{
	if (!ControllerStates.Contains(Id))
	{
		return;
	}

	SDLControllerState& ControllerState = ControllerStates[Id];
	UE_LOG(LogLinux, Verbose, TEXT("Removing controller %i '%s'"), ControllerState.DeviceId.GetId(), UTF8_TO_TCHAR(SDL_GameControllerName(ControllerState.Controller)));

	if (ControllerState.Haptic != nullptr)
	{
		SDL_HapticClose(ControllerState.Haptic);
	}

	SDL_GameControllerClose(ControllerState.Controller);
	ControllerStates.Remove(Id);

	FPlatformUserId PlatformUserId = PLATFORMUSERID_NONE;
	FInputDeviceId DeviceId = INPUTDEVICEID_NONE;

	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	DeviceMapper.RemapControllerIdToPlatformUserAndDevice(Id, OUT PlatformUserId, OUT DeviceId);

	DeviceMapper.Internal_MapInputDeviceToUser(DeviceId, PlatformUserId, EInputDeviceConnectionState::Disconnected);
}
